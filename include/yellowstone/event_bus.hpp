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
 * @file event_bus.hpp
 * @brief Declares the Event Bus interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#include "exchange.hpp"
#include "observer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @brief Facilitates distribution of events to subscribers.
   *
   * @details
   * Responsibility (Single) - An Event Bus facilitates the distribution
   * of Events to Subscribers.
   */
  class EventBus final
  {
    /**
     * @typedef SubscriberID
     * @brief Alias used by the event component to name the SubscriberID concept.
     */
    using SubscriberID = WorkerID;

    /**
     * @class Subscriber
     * @brief Represents the Subscriber type used by the event component.
     */
    class Subscriber final
    {
      SubscriberID m_id{0UL};
      EventBus*    m_bus{nullptr};
      Exchange*    m_exchange{nullptr};
      AObserver*   m_observer{nullptr};

    public:
      /**
       * @brief Performs the Subscriber operation for the event component.
       */
      Subscriber() noexcept = default;

      /**
       * @brief Performs the Notify operation for the event component.
       * @param l Value forwarded to the notify operation.
       * @param partition Value forwarded to the notify operation.
       */
      void notify(const Lease l, const Partition* partition) noexcept;

      /**
       * @brief Performs the Set Bus operation for the event component.
       * @param bus Value forwarded to the set_bus operation.
       */
      void set_bus(EventBus* bus) noexcept;

      /**
       * @brief Performs the Set Exchange operation for the event component.
       * @param exchange Value forwarded to the set_exchange operation.
       */
      void set_exchange(Exchange* exchange) noexcept;

      /**
       * @brief Performs the Set Id operation for the event component.
       * @param id Value forwarded to the set_id operation.
       */
      void set_id(const SubscriberID id) noexcept;

      /**
       * @brief Performs the Set Observer operation for the event component.
       * @param observer Value forwarded to the set_observer operation.
       */
      void set_observer(AObserver* observer) noexcept;

    }; // class Subscriber final

    static constexpr std::size_t kMaxChannelSize = 1024UL;

    Exchange*                                  m_exchange{nullptr};
    std::size_t                                m_num_subscribers{0UL};
    std::array<Subscriber, kMaxNumSubscribers> m_subscribers{};

  public:
    /**
     * @brief Performs the EventBus operation for the event component.
     * @param exchange Value forwarded to the EventBus operation.
     */
    explicit EventBus(Exchange* exchange) noexcept;

    template <class T>
    /**
     * @brief Performs the Publish operation for the event component.
     * @param payload Value forwarded to the publish operation.
     */
    void publish(T&& payload) noexcept;

    template <class T>
    void publish_one(const std::uint64_t subscriber_id, T&& payload) noexcept;

    /**
     * @brief Publishes a pre-constructed event envelope to all subscribers.
     */
    void publish_event(const std::shared_ptr<AEvent>& event) noexcept;

    /**
     * @brief Publishes a pre-constructed event envelope to a single subscriber.
     */
    void publish_one_event(const std::uint64_t subscriber_id,
                           const std::shared_ptr<AEvent>& event) noexcept;

    /**
     * @brief Performs the Subscribe operation for the event component.
     * @param observer Value forwarded to the subscribe operation.
     */
    void subscribe(AObserver* observer) noexcept;

  }; // class EventBus final

} // namespace event

#include "event_bus.inl"
