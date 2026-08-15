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

#include <cstdint>
#include <cassert>
#include <memory>

namespace event
{
  inline void EventBus::publish_event(const std::shared_ptr<AEvent>& envelope) noexcept
  {
    if (envelope && envelope->has_graph())
    {
      assert(m_exchange != nullptr);
      m_exchange->begin_graph_epoch();
    }

    for (std::uint64_t subscriber_id = 0UL; subscriber_id < m_num_subscribers; ++subscriber_id)
    {
      Partition* partition{nullptr};
      Lease      l = m_exchange->acquire(subscriber_id, partition);

      while (l.pid < 0L)
      {
        l = m_exchange->acquire(subscriber_id, partition);
      }

      std::shared_ptr<AEvent> event_copy = envelope;
      if (envelope)
      {
        if (auto cloned = envelope->clone())
        {
          event_copy = std::move(cloned);
        }
      }

      assert(partition->push(event_copy) == 0);

      Subscriber& subscriber = m_subscribers[subscriber_id];
      subscriber.notify(l, partition);
    }
  }

  inline void EventBus::publish_one_event(const std::uint64_t subscriber_id,
                                         const std::shared_ptr<AEvent>& envelope) noexcept
  {
    if (subscriber_id >= m_num_subscribers)
    {
      return;
    }

    if (envelope && envelope->has_graph())
    {
      assert(m_exchange != nullptr);
      m_exchange->begin_graph_epoch();
    }

    Partition* partition{nullptr};
    Lease      l = m_exchange->acquire(subscriber_id, partition);

    while (l.pid < 0L)
    {
      l = m_exchange->acquire(subscriber_id, partition);
    }

    std::shared_ptr<AEvent> event_copy = envelope;
    if (envelope)
    {
      if (auto cloned = envelope->clone())
      {
        event_copy = std::move(cloned);
      }
    }

    assert(partition->push(event_copy) == 0);

    Subscriber& subscriber = m_subscribers[subscriber_id];
    subscriber.notify(l, partition);
  }

  template <class T>
  void EventBus::publish(T&& payload) noexcept
  {
    for (std::uint64_t subscriber_id = 0UL; subscriber_id < m_num_subscribers; ++subscriber_id)
    {
      Partition* partition{nullptr};
      Lease      l = m_exchange->acquire(subscriber_id, partition);

      while (l.pid < 0L)
      {
        l = m_exchange->acquire(subscriber_id, partition);
      }

      auto event = std::make_shared<Event<T, kMaxNumSubscribers>>(EventScheduleType::IMMEDIATE,
                                                                  m_num_subscribers,
                                                                  std::forward<T>(payload));
      assert(partition->push(event) == 0);

      Subscriber& subscriber = m_subscribers[subscriber_id];
      subscriber.notify(l, partition);
    }
  }

  template <class T>
  void EventBus::publish_one(const std::uint64_t subscriber_id, T&& payload) noexcept
  {
    if (subscriber_id >= m_num_subscribers)
    {
      return;
    }

    Partition* partition{nullptr};
    Lease      l = m_exchange->acquire(subscriber_id, partition);

    while (l.pid < 0L)
    {
      l = m_exchange->acquire(subscriber_id, partition);
    }

    auto event = std::make_shared<Event<T, kMaxNumSubscribers>>(EventScheduleType::IMMEDIATE,
                                                                /*pending=*/1UL,
                                                                std::forward<T>(payload));
    assert(partition->push(event) == 0);

    Subscriber& subscriber = m_subscribers[subscriber_id];
    subscriber.notify(l, partition);
  }

} // namespace event
