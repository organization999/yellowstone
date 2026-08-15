/**
 * Copyright (C) 2026 Da'Jour J. Christophe. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef YELLOWSTONE_COLLECTIONS_PAIRING_HEAP_HPP
#define YELLOWSTONE_COLLECTIONS_PAIRING_HEAP_HPP

#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace yellowstone::collections
{
  /**
   * A meldable pairing heap used by Yellowstone's deadline scheduler.
   *
   * Compare follows std::priority_queue semantics. With std::less<T>, top()
   * returns the greatest value. With std::greater<T>, top() returns the least
   * value.
   *
   * Handles remain valid while their node remains in the heap, including
   * across heap moves and meld operations. A handle becomes invalid after its
   * node is removed or the owning heap is cleared/destroyed.
   */
  template <class T, class Compare = std::less<T>>
  class PairingHeap final
  {
  private:
    struct Node;

  public:
    using value_type      = T;
    using compare_type    = Compare;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = value_type&;
    using const_reference = const value_type&;

    class Handle final
    {
      friend class PairingHeap;

    public:
      constexpr Handle() noexcept = default;

      [[nodiscard]] constexpr explicit operator bool() const noexcept
      {
        return m_node != nullptr;
      }

      [[nodiscard]] const_reference value() const;

    private:
      explicit constexpr Handle(Node* node) noexcept : m_node(node) {}

      Node* m_node{nullptr};

    }; // class Handle final

    constexpr PairingHeap() noexcept(
        std::is_nothrow_default_constructible_v<compare_type>) = default;

    explicit constexpr PairingHeap(compare_type compare) noexcept(
        std::is_nothrow_move_constructible_v<compare_type>);

    template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
    PairingHeap(
        Iterator first,
        Sentinel last,
        compare_type compare = compare_type{});

    PairingHeap(
        std::initializer_list<value_type> values,
        compare_type compare = compare_type{});

    PairingHeap(const PairingHeap&)            = delete;
    PairingHeap& operator=(const PairingHeap&) = delete;

    PairingHeap(PairingHeap&& other) noexcept(
        std::is_nothrow_move_constructible_v<compare_type>);

    PairingHeap& operator=(PairingHeap&& other) noexcept(
        std::is_nothrow_move_assignable_v<compare_type>);

    ~PairingHeap() noexcept;

    [[nodiscard]] constexpr bool empty() const noexcept
    {
      return m_root == nullptr;
    }

    [[nodiscard]] constexpr size_type size() const noexcept
    {
      return m_size;
    }

    [[nodiscard]] const_reference top() const;

    Handle push(const value_type& value);
    Handle push(value_type&& value);

    template <class... Arguments>
      requires std::constructible_from<T, Arguments...>
    Handle emplace(Arguments&&... arguments);

    void pop();

    [[nodiscard]] value_type extract_top();

    /**
     * Meld another heap into this heap in amortized O(1).
     *
     * Both heaps must use equivalent comparator state. Handles referring to
     * nodes in either heap remain valid after the meld.
     */
    void meld(PairingHeap&& other);

    /**
     * Increase a node's priority according to Compare.
     *
     * For the default std::less<T> max heap, new_value must be greater than or
     * equal to the current value. For std::greater<T>, it must be less than or
     * equal to the current value.
     *
     * The handle must refer to a live node owned by this heap.
     */
    template <class U>
      requires std::assignable_from<T&, U&&> &&
               std::convertible_to<U&&, T>
    void increase_priority(Handle handle, U&& new_value);

    void clear() noexcept;

    void swap(PairingHeap& other) noexcept(
        std::is_nothrow_swappable_v<compare_type>);

    [[nodiscard]] constexpr const compare_type& comparator() const noexcept
    {
      return m_compare;
    }

  private:
    struct Node final
    {
      template <class... Arguments>
        requires std::constructible_from<value_type, Arguments...>
      explicit Node(Arguments&&... arguments)
        : value(std::forward<Arguments>(arguments)...)
      {
      }

      value_type value;
      Node*      parent{nullptr};
      Node*      previous_sibling{nullptr};
      Node*      first_child{nullptr};
      Node*      next_sibling{nullptr};

    }; // struct Node final

    [[nodiscard]] Node* meld_nodes(Node* lhs, Node* rhs);
    [[nodiscard]] Node* combine_children(Node* first_child);

    static void detach_from_parent(Node* node) noexcept;
    static void destroy_tree(Node* root) noexcept;

    Node*       m_root{nullptr};
    size_type   m_size{0UL};
    compare_type m_compare{};

  }; // class PairingHeap final

  template <class T, class Compare>
  void swap(PairingHeap<T, Compare>& lhs, PairingHeap<T, Compare>& rhs) noexcept(
      noexcept(lhs.swap(rhs)));

} // namespace yellowstone::collections

#include "pairing_heap.inl"

#endif // YELLOWSTONE_COLLECTIONS_PAIRING_HEAP_HPP
