#include "yellowstone/event.hpp"
#include <cassert>
#include <string>

int main()
{
  yellowstone::Event<std::string> event{7UL, 3UL, 11UL, "payload"};
  assert(7UL == event.id());
  assert(3UL == event.superstep());
  assert(11UL == event.sequence());
  assert("payload" == event.payload());
  return 0;
}
