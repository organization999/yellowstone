/**
 * Copyright (C) 2026 Da'Jour J. Christophe. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
#ifndef YELLOWSTONE_COLLECTIONS_PAIRING_HEAP_INL
#define YELLOWSTONE_COLLECTIONS_PAIRING_HEAP_INL

namespace yellowstone::collections
{
  template <class T, class Compare>
  auto PairingHeap<T, Compare>::Handle::value() const -> const_reference
  {
    if (m_node == nullptr)
    {
      throw std::logic_error{"The pairing-heap handle is empty."};
    }

    return m_node->value;
  }

  template <class T, class Compare>
  constexpr PairingHeap<T, Compare>::PairingHeap(compare_type compare) noexcept(
      std::is_nothrow_move_constructible_v<compare_type>)
    : m_compare(std::move(compare))
  {
  }

  template <class T, class Compare>
  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel>
  PairingHeap<T, Compare>::PairingHeap(
      Iterator first,
      Sentinel last,
      compare_type compare)
    : m_compare(std::move(compare))
  {
    try
    {
      for (; first != last; ++first)
      {
        emplace(*first);
      }
    }
    catch (...)
    {
      clear();
      throw;
    }
  }

  template <class T, class Compare>
  PairingHeap<T, Compare>::PairingHeap(
      std::initializer_list<value_type> values,
      compare_type compare)
    : PairingHeap(values.begin(), values.end(), std::move(compare))
  {
  }

  template <class T, class Compare>
  PairingHeap<T, Compare>::PairingHeap(PairingHeap&& other) noexcept(
      std::is_nothrow_move_constructible_v<compare_type>)
    : m_root(std::exchange(other.m_root, nullptr)),
      m_size(std::exchange(other.m_size, 0UL)),
      m_compare(std::move(other.m_compare))
  {
  }

  template <class T, class Compare>
  auto PairingHeap<T, Compare>::operator=(PairingHeap&& other) noexcept(
      std::is_nothrow_move_assignable_v<compare_type>) -> PairingHeap&
  {
    if (this == &other)
    {
      return *this;
    }

    clear();

    m_compare = std::move(other.m_compare);
    m_root    = std::exchange(other.m_root, nullptr);
    m_size    = std::exchange(other.m_size, 0UL);

    return *this;
  }

  template <class T, class Compare>
  PairingHeap<T, Compare>::~PairingHeap() noexcept
  {
    clear();
  }

  template <class T, class Compare>
  auto PairingHeap<T, Compare>::top() const -> const_reference
  {
    if (m_root == nullptr)
    {
      throw std::out_of_range{"PairingHeap::top() called on an empty heap."};
    }

    return m_root->value;
  }

  template <class T, class Compare>
  auto PairingHeap<T, Compare>::push(const value_type& value) -> Handle
  {
    return emplace(value);
  }

  template <class T, class Compare>
  auto PairingHeap<T, Compare>::push(value_type&& value) -> Handle
  {
    return emplace(std::move(value));
  }

  template <class T, class Compare>
  template <class... Arguments>
    requires std::constructible_from<T, Arguments...>
  auto PairingHeap<T, Compare>::emplace(Arguments&&... arguments) -> Handle
  {
    Node* node = new Node(std::forward<Arguments>(arguments)...);

    try
    {
      m_root = meld_nodes(m_root, node);
    }
    catch (...)
    {
      delete node;
      throw;
    }

    ++m_size;
    return Handle{node};
  }

  template <class T, class Compare>
  void PairingHeap<T, Compare>::pop()
  {
    if (m_root == nullptr)
    {
      throw std::out_of_range{"PairingHeap::pop() called on an empty heap."};
    }

    Node* old_root = m_root;
    Node* children = old_root->first_child;

    old_root->first_child = nullptr;
    m_root               = combine_children(children);

    delete old_root;
    --m_size;
  }

  template <class T, class Compare>
  auto PairingHeap<T, Compare>::extract_top() -> value_type
  {
    if (m_root == nullptr)
    {
      throw std::out_of_range{
          "PairingHeap::extract_top() called on an empty heap."};
    }

    value_type result = std::move(m_root->value);
    pop();
    return result;
  }

  template <class T, class Compare>
  void PairingHeap<T, Compare>::meld(PairingHeap&& other)
  {
    if (this == &other || other.m_root == nullptr)
    {
      return;
    }

    m_root = meld_nodes(m_root, other.m_root);
    m_size += other.m_size;

    other.m_root = nullptr;
    other.m_size = 0UL;
  }

  template <class T, class Compare>
  template <class U>
    requires std::assignable_from<T&, U&&> && std::convertible_to<U&&, T>
  void PairingHeap<T, Compare>::increase_priority(
      Handle handle,
      U&& new_value)
  {
    Node* node = handle.m_node;

    if (node == nullptr)
    {
      throw std::invalid_argument{
          "PairingHeap::increase_priority() received an empty handle."};
    }

    if (m_compare(new_value, node->value))
    {
      throw std::invalid_argument{
          "The replacement value would lower the node's priority."};
    }

    node->value = std::forward<U>(new_value);

    if (node == m_root || node->parent == nullptr)
    {
      return;
    }

    if (!m_compare(node->parent->value, node->value))
    {
      return;
    }

    detach_from_parent(node);
    m_root = meld_nodes(m_root, node);
  }

  template <class T, class Compare>
  void PairingHeap<T, Compare>::clear() noexcept
  {
    destroy_tree(m_root);
    m_root = nullptr;
    m_size = 0UL;
  }

  template <class T, class Compare>
  void PairingHeap<T, Compare>::swap(PairingHeap& other) noexcept(
      std::is_nothrow_swappable_v<compare_type>)
  {
    using std::swap;

    swap(m_root, other.m_root);
    swap(m_size, other.m_size);
    swap(m_compare, other.m_compare);
  }

  template <class T, class Compare>
  auto PairingHeap<T, Compare>::meld_nodes(Node* lhs, Node* rhs) -> Node*
  {
    if (lhs == nullptr)
    {
      return rhs;
    }

    if (rhs == nullptr)
    {
      return lhs;
    }

    // Compare has std::priority_queue semantics. If lhs compares before rhs,
    // rhs has the higher priority and must become the root.
    if (m_compare(lhs->value, rhs->value))
    {
      std::swap(lhs, rhs);
    }

    lhs->parent           = nullptr;
    lhs->previous_sibling = nullptr;
    lhs->next_sibling     = nullptr;

    rhs->parent           = lhs;
    rhs->previous_sibling = nullptr;
    rhs->next_sibling     = lhs->first_child;

    if (lhs->first_child != nullptr)
    {
      lhs->first_child->previous_sibling = rhs;
    }

    lhs->first_child = rhs;
    return lhs;
  }

  template <class T, class Compare>
  auto PairingHeap<T, Compare>::combine_children(Node* first_child) -> Node*
  {
    if (first_child == nullptr)
    {
      return nullptr;
    }

    // First pass: pair adjacent roots from left to right. Store the paired
    // roots in reverse order by reusing the sibling pointers.
    Node* paired_roots{nullptr};
    Node* current{first_child};

    while (current != nullptr)
    {
      Node* first  = current;
      Node* second = first->next_sibling;
      Node* next   = second == nullptr ? nullptr : second->next_sibling;

      first->parent           = nullptr;
      first->previous_sibling = nullptr;
      first->next_sibling     = nullptr;

      Node* merged{first};

      if (second != nullptr)
      {
        second->parent           = nullptr;
        second->previous_sibling = nullptr;
        second->next_sibling     = nullptr;
        merged                   = meld_nodes(first, second);
      }

      merged->next_sibling = paired_roots;

      if (paired_roots != nullptr)
      {
        paired_roots->previous_sibling = merged;
      }

      paired_roots = merged;
      current      = next;
    }

    // Second pass: meld the paired roots from right to left.
    Node* result{nullptr};
    current = paired_roots;

    while (current != nullptr)
    {
      Node* next = current->next_sibling;

      current->parent           = nullptr;
      current->previous_sibling = nullptr;
      current->next_sibling     = nullptr;

      if (next != nullptr)
      {
        next->previous_sibling = nullptr;
      }

      result  = meld_nodes(current, result);
      current = next;
    }

    return result;
  }

  template <class T, class Compare>
  void PairingHeap<T, Compare>::detach_from_parent(Node* node) noexcept
  {
    Node* parent = node->parent;

    if (parent == nullptr)
    {
      return;
    }

    if (node->previous_sibling != nullptr)
    {
      node->previous_sibling->next_sibling = node->next_sibling;
    }
    else
    {
      parent->first_child = node->next_sibling;
    }

    if (node->next_sibling != nullptr)
    {
      node->next_sibling->previous_sibling = node->previous_sibling;
    }

    node->parent           = nullptr;
    node->previous_sibling = nullptr;
    node->next_sibling     = nullptr;
  }

  template <class T, class Compare>
  void PairingHeap<T, Compare>::destroy_tree(Node* root) noexcept
  {
    Node* current = root;

    // Iteratively flatten child lists into the traversal chain so destruction
    // does not recurse through an adversarially deep heap.
    while (current != nullptr)
    {
      if (current->first_child != nullptr)
      {
        Node* child = current->first_child;

        current->first_child = child->next_sibling;

        if (current->first_child != nullptr)
        {
          current->first_child->previous_sibling = nullptr;
        }

        child->parent           = nullptr;
        child->previous_sibling = nullptr;
        child->next_sibling     = current;
        current                 = child;
        continue;
      }

      Node* next = current->next_sibling;
      delete current;
      current = next;
    }
  }

  template <class T, class Compare>
  void swap(PairingHeap<T, Compare>& lhs, PairingHeap<T, Compare>& rhs) noexcept(
      noexcept(lhs.swap(rhs)))
  {
    lhs.swap(rhs);
  }

} // namespace yellowstone::collections

#endif // YELLOWSTONE_COLLECTIONS_PAIRING_HEAP_INL
