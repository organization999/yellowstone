#include "yellowstone/partition.hpp"
#include <cassert>
#include <functional>
#include <string>

int main()
{
  using Merge = std::function<void(std::string&, const std::string&)>;
  yellowstone::Partition<std::string, Merge> partition{
    3UL,
    [](std::string& current, const std::string& incoming) { current += incoming; }
  };

  using Event = yellowstone::Event<std::string>;
  assert(yellowstone::PushResult::inserted == partition.push(Event{1, 2, 0, "a"}));
  assert(yellowstone::PushResult::inserted == partition.push(Event{2, 1, 1, "b"}));
  assert(yellowstone::PushResult::inserted == partition.push(Event{3, 1, 2, "c"}));
  assert(yellowstone::PushResult::coalesced == partition.push(Event{2, 1, 3, "+"}));
  assert(3UL == partition.size());

  auto out = partition.pop();
  assert(out);
  assert(2UL == out->id());
  assert("b+" == out->payload());
  out = partition.pop();
  assert(out);
  assert(3UL == out->id());
  out = partition.pop();
  assert(out);
  assert(1UL == out->id());
  assert(partition.empty());
  return 0;
}
