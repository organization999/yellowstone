#pragma once

#include <iostream>
#include <syncstream>
#include <thread>
#include <utility>

namespace yellowstone
{
  template <EventPayload Payload>
  EventBus<Payload>::Subscriber::Subscriber(
    const SubscriberID subscriber_id,
    Handler subscriber_handler,
    const std::size_t capacity,
    const Merger& merger
  )
    : id(subscriber_id),
      handler(std::move(subscriber_handler)),
      partition(capacity, merger)
  {
  }

  template <EventPayload Payload>
  EventBus<Payload>::ConsumerWorker::ConsumerWorker(
    EventBus* bus,
    const SubscriberID subscriber_id
  ) noexcept
    : m_bus(bus),
      m_subscriber_id(subscriber_id)
  {
  }

  template <EventPayload Payload>
  void EventBus<Payload>::ConsumerWorker::work(void)
  {
    m_bus->m_consume_one(m_subscriber_id);
  }

  template <EventPayload Payload>
  EventBus<Payload>::EventBus(
    const std::size_t partition_capacity,
    Merger merger,
    ErrorHandler error_handler
  )
    : m_partition_capacity(partition_capacity),
      m_merger(std::move(merger)),
      m_error_handler(std::move(error_handler))
  {
    if (0UL == m_partition_capacity)
    {
      throw std::invalid_argument("partition capacity must be greater than zero");
    }

    if (!m_merger)
    {
      m_merger = [](Payload& current, const Payload& incoming)
      {
        current = incoming;
      };
    }
  }

  template <EventPayload Payload>
  EventBus<Payload>::~EventBus() noexcept
  {
    stop();
  }

  template <EventPayload Payload>
  [[nodiscard]] SubscriberID EventBus<Payload>::subscribe(Handler handler)
  {
    if (!handler)
    {
      throw std::invalid_argument("subscriber handler must be callable");
    }

    std::lock_guard lifecycle_lock{m_lifecycle_mutex};
    std::scoped_lock publish_lock{m_publish_mutex};
    std::lock_guard subscriber_lock{m_subscriber_mutex};

    if (m_running.load(std::memory_order_acquire))
    {
      throw std::logic_error("subscribers cannot be added while the bus is running");
    }

    if (m_subscriber_count >= k_max_subscribers)
    {
      throw std::length_error("maximum Yellowstone subscriber count reached");
    }

    for (SubscriberID id = 0UL; id < k_max_subscribers; ++id)
    {
      if (!m_subscribers[id])
      {
        m_subscribers[id] = std::make_shared<Subscriber>(
          id,
          std::move(handler),
          m_partition_capacity,
          m_merger
        );
        ++m_subscriber_count;
        return id;
      }
    }

    throw std::length_error("maximum Yellowstone subscriber count reached");
  }

  template <EventPayload Payload>
  void EventBus<Payload>::unsubscribe(const SubscriberID subscriber_id)
  {
    std::lock_guard lifecycle_lock{m_lifecycle_mutex};
    std::scoped_lock publish_lock{m_publish_mutex};
    std::lock_guard subscriber_lock{m_subscriber_mutex};

    if (m_running.load(std::memory_order_acquire))
    {
      throw std::logic_error("subscribers cannot be removed while the bus is running");
    }

    if (subscriber_id >= k_max_subscribers || !m_subscribers[subscriber_id])
    {
      throw std::out_of_range("subscriber does not exist");
    }

    m_subscribers[subscriber_id].reset();
    --m_subscriber_count;
  }

  template <EventPayload Payload>
  [[nodiscard]] typename EventBus<Payload>::SubscriberPtr
  EventBus<Payload>::m_subscriber(const SubscriberID subscriber_id) const
  {
    std::lock_guard lock{m_subscriber_mutex};

    if (subscriber_id >= k_max_subscribers || !m_subscribers[subscriber_id])
    {
      throw std::out_of_range("subscriber does not exist");
    }

    return m_subscribers[subscriber_id];
  }

  template <EventPayload Payload>
  [[nodiscard]] std::vector<typename EventBus<Payload>::SubscriberPtr>
  EventBus<Payload>::m_snapshot_subscribers(void) const
  {
    std::lock_guard lock{m_subscriber_mutex};
    std::vector<SubscriberPtr> snapshot{};
    snapshot.reserve(m_subscriber_count);

    for (const auto& subscriber : m_subscribers)
    {
      if (subscriber)
      {
        snapshot.push_back(subscriber);
      }
    }

    return snapshot;
  }

  template <EventPayload Payload>
  [[nodiscard]] PublishResult EventBus<Payload>::publish(
    const EventID event_id,
    Payload payload,
    const Superstep superstep
  )
  {
    std::lock_guard publish_lock{m_publish_mutex};
    const auto subscribers = m_snapshot_subscribers();
    const Sequence sequence = m_sequence.fetch_add(1UL, std::memory_order_relaxed);
    const EventType prototype{event_id, superstep, sequence, std::move(payload)};

    for (const auto& subscriber : subscribers)
    {
      if (!subscriber->partition.can_accept(prototype))
      {
        throw std::overflow_error("one or more subscriber partitions are full");
      }
    }

    PublishResult result{};

    for (const auto& subscriber : subscribers)
    {
      switch (subscriber->partition.push(prototype))
      {
        case PushResult::inserted:
          ++result.delivered;
          break;
        case PushResult::coalesced:
          ++result.delivered;
          ++result.coalesced;
          break;
        case PushResult::full:
          throw std::logic_error("partition capacity changed during serialized publish");
      }
    }

    return result;
  }

  template <EventPayload Payload>
  [[nodiscard]] PublishResult EventBus<Payload>::publish_one(
    const SubscriberID subscriber_id,
    const EventID event_id,
    Payload payload,
    const Superstep superstep
  )
  {
    std::lock_guard publish_lock{m_publish_mutex};
    const auto subscriber = m_subscriber(subscriber_id);
    const Sequence sequence = m_sequence.fetch_add(1UL, std::memory_order_relaxed);
    EventType event{event_id, superstep, sequence, std::move(payload)};

    if (!subscriber->partition.can_accept(event))
    {
      throw std::overflow_error("subscriber partition is full");
    }

    PublishResult result{};
    const auto status = subscriber->partition.push(std::move(event));

    if (PushResult::inserted == status || PushResult::coalesced == status)
    {
      result.delivered = 1UL;
    }
    if (PushResult::coalesced == status)
    {
      result.coalesced = 1UL;
    }

    return result;
  }

  template <EventPayload Payload>
  void EventBus<Payload>::start(void)
  {
    std::lock_guard lifecycle_lock{m_lifecycle_mutex};

    if (m_running.load(std::memory_order_acquire))
    {
      return;
    }

    std::vector<SubscriberPtr> subscribers{};

    {
      // Freeze subscriber membership before constructing the worker mapping.
      // subscribe()/unsubscribe() use the same lifecycle lock, while publish()
      // uses the publish lock and can safely continue after this snapshot.
      std::lock_guard subscriber_lock{m_subscriber_mutex};
      subscribers.reserve(m_subscriber_count);
      for (const auto& subscriber : m_subscribers)
      {
        if (subscriber)
        {
          subscribers.push_back(subscriber);
        }
      }
      m_running.store(true, std::memory_order_release);
    }

    try
    {
      auto manager = std::make_unique<phelps::WorkerManager>(subscribers.size());
      std::vector<WorkerPtr> workers{};
      workers.reserve(subscribers.size());

      for (const auto& subscriber : subscribers)
      {
        auto worker = std::make_shared<ConsumerWorker>(this, subscriber->id);
        static_cast<void>(manager->register_worker(worker));
        workers.push_back(std::move(worker));
      }

      manager->start();
      m_manager = std::move(manager);
      m_workers = std::move(workers);
    }
    catch (...)
    {
      m_running.store(false, std::memory_order_release);
      for (const auto& subscriber : subscribers)
      {
        subscriber->partition.wake_all();
      }
      throw;
    }
  }

  template <EventPayload Payload>
  void EventBus<Payload>::stop(void) noexcept
  {
    std::lock_guard lifecycle_lock{m_lifecycle_mutex};

    if (!m_running.exchange(false, std::memory_order_acq_rel))
    {
      return;
    }

    const auto subscribers = m_snapshot_subscribers();
    for (const auto& subscriber : subscribers)
    {
      subscriber->partition.wake_all();
    }

    if (m_manager)
    {
      m_manager->stop();
    }

    m_workers.clear();
    m_manager.reset();
  }

  template <EventPayload Payload>
  void EventBus<Payload>::m_consume_one(const SubscriberID subscriber_id) noexcept
  {
    SubscriberPtr subscriber{};

    try
    {
      subscriber = m_subscriber(subscriber_id);
    }
    catch (...)
    {
      std::this_thread::yield();
      return;
    }

    if (!m_running.load(std::memory_order_acquire))
    {
      std::this_thread::yield();
      return;
    }

    auto event = subscriber->partition.wait_pop_for(
      std::chrono::milliseconds{10},
      m_running
    );

    if (!event)
    {
      return;
    }

    try
    {
      subscriber->handler(*event);
    }
    catch (...)
    {
      m_report_error(subscriber_id, *event, std::current_exception());
    }
  }

  template <EventPayload Payload>
  void EventBus<Payload>::m_report_error(
    const SubscriberID subscriber_id,
    const EventType& event,
    std::exception_ptr error
  ) noexcept
  {
    if (m_error_handler)
    {
      try
      {
        m_error_handler(subscriber_id, event, error);
        return;
      }
      catch (...)
      {
      }
    }

    try
    {
      std::rethrow_exception(error);
    }
    catch (const std::exception& exception)
    {
      std::osyncstream(std::cerr)
        << "yellowstone subscriber " << subscriber_id
        << " failed while handling event " << event.id()
        << ": " << exception.what() << '\n';
    }
    catch (...)
    {
      std::osyncstream(std::cerr)
        << "yellowstone subscriber " << subscriber_id
        << " failed while handling event " << event.id()
        << " with a non-standard exception\n";
    }
  }

  template <EventPayload Payload>
  [[nodiscard]] bool EventBus<Payload>::running(void) const noexcept
  {
    return m_running.load(std::memory_order_acquire);
  }

  template <EventPayload Payload>
  [[nodiscard]] std::size_t EventBus<Payload>::subscriber_count(void) const noexcept
  {
    std::lock_guard lock{m_subscriber_mutex};
    return m_subscriber_count;
  }

  template <EventPayload Payload>
  [[nodiscard]] std::size_t EventBus<Payload>::pending(
    const SubscriberID subscriber_id
  ) const
  {
    return m_subscriber(subscriber_id)->partition.size();
  }

  template <EventPayload Payload>
  [[nodiscard]] std::size_t EventBus<Payload>::partition_capacity(void) const noexcept
  {
    return m_partition_capacity;
  }

} // namespace yellowstone
