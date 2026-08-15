#include "yellowstone/collections/pairing_heap.hpp"

#include <cassert>
#include <cstddef>
#include <functional>

int main(void)
{
  yellowstone::collections::PairingHeap<int, std::greater<int>> heap{};

  assert(heap.empty());
  assert(heap.size() == 0UL);

  static_cast<void>(heap.push(30));
  static_cast<void>(heap.push(10));
  static_cast<void>(heap.push(20));
  static_cast<void>(heap.emplace(5));

  assert(heap.size() == 4UL);
  assert(heap.top() == 5);
  assert(heap.extract_top() == 5);
  assert(heap.extract_top() == 10);
  assert(heap.extract_top() == 20);
  assert(heap.extract_top() == 30);
  assert(heap.empty());

  yellowstone::collections::PairingHeap<int> lhs{};
  yellowstone::collections::PairingHeap<int> rhs{};

  static_cast<void>(lhs.push(1));
  static_cast<void>(lhs.push(5));
  static_cast<void>(rhs.push(3));
  static_cast<void>(rhs.push(9));

  lhs.meld(std::move(rhs));

  assert(rhs.empty());
  assert(lhs.size() == 4UL);
  assert(lhs.top() == 9);

  return 0;
}
