#include "yellowstone/event_bus.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

int main()
{
  using Bus = yellowstone::EventBus<std::string>;
  std::mutex mutex{};
  std::condition_variable cv{};
  std::vector<std::string> first{};
  std::vector<std::string> second{};

  Bus bus{8UL, [](std::string& current, const std::string& incoming) { current += incoming; }};

  const auto a = bus.subscribe([&](const Bus::EventType& event)
  {
    std::lock_guard lock{mutex};
    first.push_back(event.payload());
    cv.notify_all();
  });
  const auto b = bus.subscribe([&](const Bus::EventType& event)
  {
    std::lock_guard lock{mutex};
    second.push_back(event.payload());
    cv.notify_all();
  });

  assert(0UL == a);
  assert(1UL == b);
  assert(2UL == bus.subscriber_count());

  // Coalescing happens before workers start, guaranteeing deterministic merge.
  auto result = bus.publish(10UL, std::string{"x"}, 2UL);
  assert(2UL == result.delivered);
  result = bus.publish(10UL, std::string{"y"}, 2UL);
  assert(2UL == result.coalesced);

  // Lower superstep must be consumed before the earlier-published superstep 2.
  static_cast<void>(bus.publish(11UL, std::string{"early"}, 1UL));
  bus.start();

  {
    std::unique_lock lock{mutex};
    const bool done = cv.wait_for(lock, std::chrono::seconds{2}, [&]
    {
      return first.size() >= 2UL && second.size() >= 2UL;
    });
    assert(done);
  }

  bus.stop();
  assert(!bus.running());
  assert("early" == first[0]);
  assert("xy" == first[1]);
  assert("early" == second[0]);
  assert("xy" == second[1]);
  return 0;
}
