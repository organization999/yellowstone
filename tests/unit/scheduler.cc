#include "yellowstone/yellowstone.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

int main(void)
{
  using namespace std::chrono_literals;

  yellowstone::EventBus<int> bus{};

  std::mutex mutex{};
  std::condition_variable condition{};
  std::vector<std::uint64_t> order{};
  std::vector<std::chrono::system_clock::time_point> observed{};

  static_cast<void>(bus.subscribe(
    [&](const yellowstone::Event<int>& event)
    {
      {
        const std::lock_guard lock{mutex};
        order.push_back(event.id());
        observed.push_back(std::chrono::system_clock::now());
      }
      condition.notify_all();
    }
  ));

  bus.start();
  yellowstone::Scheduler<int> scheduler{bus};

  const auto now = std::chrono::system_clock::now();
  const auto first_deadline = now + 120ms;
  const auto second_deadline = now + 40ms;
  const auto third_deadline = now + 80ms;

  static_cast<void>(scheduler.schedule(1UL, 10, first_deadline));
  static_cast<void>(scheduler.schedule(2UL, 20, second_deadline));
  static_cast<void>(scheduler.schedule(3UL, 30, third_deadline));

  {
    std::unique_lock lock{mutex};
    const bool complete = condition.wait_for(
      lock,
      2s,
      [&order]
      {
        return order.size() == 3UL;
      }
    );
    assert(complete);
  }

  scheduler.stop();
  bus.stop();

  assert((order == std::vector<std::uint64_t>{2UL, 3UL, 1UL}));
  assert(observed.size() == 3UL);
  assert(observed[0UL] >= second_deadline);
  assert(observed[1UL] >= third_deadline);
  assert(observed[2UL] >= first_deadline);
  assert(scheduler.scheduled_events() == 3UL);
  assert(scheduler.forwarded_events() == 3UL);
  assert(scheduler.failed_events() == 0UL);
  assert(scheduler.pending_events() == 0UL);

  // Equal deadlines remain FIFO by insertion order.
  order.clear();
  observed.clear();
  bus.start();
  scheduler.start();

  const auto equal_deadline = std::chrono::system_clock::now() + 40ms;
  static_cast<void>(scheduler.schedule(4UL, 40, equal_deadline));
  static_cast<void>(scheduler.schedule(5UL, 50, equal_deadline));

  {
    std::unique_lock lock{mutex};
    const bool complete = condition.wait_for(
      lock,
      2s,
      [&order]
      {
        return order.size() == 2UL;
      }
    );
    assert(complete);
  }

  scheduler.stop();
  bus.stop();
  assert((order == std::vector<std::uint64_t>{4UL, 5UL}));

  return 0;
}
