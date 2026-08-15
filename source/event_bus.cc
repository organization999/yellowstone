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

/**
 * @file event_bus.cc
 * @brief Implements the Event Bus logic for the event component.
 *
 * @details
 * This translation unit contains the out-of-line implementation for the surrounding feature. It complements the corresponding headers by providing concrete behavior, coordinating helper types, and keeping module-specific logic localized to one compilation unit.
 */

#include "policy/event/event_bus.hpp"

#include <cassert>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @brief Performs the Notify operation for the event component.
   * @param l Value forwarded to the EventBus::Subscriber::notify operation.
   * @param partition Value forwarded to the EventBus::Subscriber::notify operation.
   */
  void EventBus::Subscriber::notify(const Lease l, const Partition* partition) noexcept
  {
    assert(m_observer != nullptr);
    m_observer->notify(m_id, l.pid, l.gen, partition);
  }

  /**
   * @brief Performs the Set Bus operation for the event component.
   * @param bus Value forwarded to the EventBus::Subscriber::set_bus operation.
   */
  void EventBus::Subscriber::set_bus(EventBus* bus) noexcept
  {
    m_bus = bus;
  }

  /**
   * @brief Performs the Set Exchange operation for the event component.
   * @param exchange Value forwarded to the EventBus::Subscriber::set_exchange operation.
   */
  void EventBus::Subscriber::set_exchange(Exchange* exchange) noexcept
  {
    m_exchange = exchange;
  }

  /**
   * @brief Performs the Set Id operation for the event component.
   * @param id Value forwarded to the EventBus::Subscriber::set_id operation.
   */
  void EventBus::Subscriber::set_id(const SubscriberID id) noexcept
  {
    m_id = id;
  }

  /**
   * @brief Performs the Set Observer operation for the event component.
   * @param observer Value forwarded to the EventBus::Subscriber::set_observer operation.
   */
  void EventBus::Subscriber::set_observer(AObserver* observer) noexcept
  {
    m_observer = observer;
  }

  /**
   * @brief Performs the EventBus operation for the event component.
   * @param observer Value forwarded to the EventBus::EventBus operation.
   */
  EventBus::EventBus(Exchange* exchange) noexcept : m_exchange(exchange) {}

  void EventBus::subscribe(AObserver* observer) noexcept
  {
    observer->set_exchange(m_exchange);

    Subscriber& subscriber = m_subscribers[m_num_subscribers];

    /**
     * @brief Performs the Set Bus operation for the event component.
     * @param this Value forwarded to the set_bus operation.
     */
    subscriber.set_bus(this);

    /**
     * @brief Performs the Set Exchange operation for the event component.
     * @param m_exchange Value forwarded to the set_exchange operation.
     */
    subscriber.set_exchange(m_exchange);

    /**
     * @brief Performs the Set Id operation for the event component.
     * @param m_num_subscribers Value forwarded to the set_id operation.
     */
    subscriber.set_id(m_num_subscribers);

    /**
     * @brief Performs the Set Observer operation for the event component.
     * @param observer Value forwarded to the set_observer operation.
     */
    subscriber.set_observer(observer);

    ++m_num_subscribers;
  }

} // namespace event
