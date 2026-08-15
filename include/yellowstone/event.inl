/*
Copyright (C) 2025-2026 Da'Jour J. Christophe. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once

#include "event.hpp"

namespace event
{
  /**
   * @brief Constructs an immediate-schedule event.
   *
   * Initializes the event metadata without a timestamp. This constructor
   * is typically used for events whose scheduling type is
   * @ref EventScheduleType::IMMEDIATE.
   *
   * @param priority_  Priority value used by the scheduler.
   * @param type_      Scheduling type of the event.
   * @param callback_  Function pointer invoked during execution.
   *
   * @note The callback may be nullptr. In such cases, execution will fail.
   */
  inline AEvent::AEvent(EventPriority priority_, const EventScheduleType type_,
                        const EventCallback callback_) noexcept
    : type(type_), callback(callback_), priority(priority_), timestamp{}
  {}

  /**
   * @brief Constructs a timestamp-based event.
   *
   * Initializes the event metadata including the execution timestamp.
   * This constructor is typically used for events whose scheduling type is
   * @ref EventScheduleType::TIMESTAMP.
   *
   * @param priority_   Priority value used by the scheduler.
   * @param type_       Scheduling type of the event.
   * @param callback_   Function pointer invoked during execution.
   * @param timestamp_  Time at which the event becomes eligible for execution.
   *
   * @note The callback may be nullptr. In such cases, execution will fail.
   */
  inline AEvent::AEvent(EventPriority priority_, const EventScheduleType type_,
                        const EventCallback callback_,
                        EventTimestamp timestamp_) noexcept
    : type(type_), callback(callback_), priority(priority_), timestamp(timestamp_)
  {}

  template <typename T, std::size_t N>
  inline Event<T, N>::Event() noexcept(std::is_nothrow_default_constructible_v<T>)
    : AEvent(0U, EventScheduleType::IMMEDIATE, nullptr), pending(0UL), viewed{}, payload{}
  {}

  template <typename T, std::size_t N>
  inline Event<T, N>::Event(const std::size_t pending_) noexcept(std::is_nothrow_default_constructible_v<T>)
    : AEvent(0U, EventScheduleType::IMMEDIATE, nullptr), pending(pending_), viewed{}, payload{}
  {
    assert(pending_ <= N);
  }

  template <typename T, std::size_t N>
  inline Event<T, N>::Event(const EventScheduleType type,
                            const std::size_t pending_) noexcept(std::is_nothrow_default_constructible_v<T>)
    : AEvent(0U, type, nullptr), pending(pending_), viewed{}, payload{}
  {
    assert(pending_ <= N);
  }

  template <typename T, std::size_t N>
  inline Event<T, N>::Event(const std::size_t pending_, const T& payload_) noexcept(std::is_nothrow_copy_constructible_v<T>)
    : AEvent(0U, EventScheduleType::IMMEDIATE, nullptr), pending(pending_), viewed{}, payload(payload_)
  {
    assert(pending_ <= N);
  }

  template <typename T, std::size_t N>
  inline Event<T, N>::Event(const std::size_t pending_, T&& payload_) noexcept(std::is_nothrow_move_constructible_v<T>)
    : AEvent(0U, EventScheduleType::IMMEDIATE, nullptr), pending(pending_), viewed{}, payload(std::move(payload_))
  {
    assert(pending_ <= N);
  }

  template <typename T, std::size_t N>
  inline Event<T, N>::Event(const EventScheduleType type,
                            const std::size_t pending_,
                            const T& payload_) noexcept(std::is_nothrow_copy_constructible_v<T>)
    : AEvent(0U, type, nullptr), pending(pending_), viewed{}, payload(payload_)
  {
    assert(pending_ <= N);
  }

  template <typename T, std::size_t N>
  inline Event<T, N>::Event(const EventScheduleType type,
                            const std::size_t pending_,
                            T&& payload_) noexcept(std::is_nothrow_move_constructible_v<T>)
    : AEvent(0U, type, nullptr), pending(pending_), viewed{}, payload(std::move(payload_))
  {
    assert(pending_ <= N);
  }

  template <typename T, std::size_t N>
  inline void Event<T, N>::view(const std::size_t i) noexcept
  {
    assert(i < N);

    if (viewed.test(i))
    {
      return;
    }

    viewed.set(i);

    if (pending > 0UL)
    {
      --pending;
    }
  }

  template <typename T, std::size_t N>
  [[nodiscard]] inline bool Event<T, N>::complete(void) const noexcept
  {
    return (0UL == pending);
  }

  template <typename T, std::size_t N>
  [[nodiscard]] inline std::size_t Event<T, N>::get_pending(void) const noexcept
  {
    return pending;
  }

} // namespace event
