#pragma once

#include <chrono>
#include <stdexcept>

namespace yellowstone
{
  template <EventPayload Payload>
  Scheduler<Payload>::Scheduler(
    Bus& event_bus,
    const RetryDelay retry_delay,
    ErrorHandler error_handler
  )
    : m_event_bus(event_bus),
      m_retry_delay(retry_delay),
      m_error_handler(std::move(error_handler))
  {
    if (m_retry_delay <= RetryDelay::zero())
    {
      throw std::invalid_argument(
        "scheduler retry delay must be greater than zero"
      );
    }

    start();
  }

  template <EventPayload Payload>
  Scheduler<Payload>::~Scheduler() noexcept
  {
    stop();
  }

  template <EventPayload Payload>
  [[nodiscard]] bool Scheduler<Payload>::EarlierDeadline::operator()(
    const ScheduledItem& lhs,
    const ScheduledItem& rhs
  ) const noexcept
  {
    if (lhs.forward_at != rhs.forward_at)
    {
      return lhs.forward_at > rhs.forward_at;
    }

    return lhs.insertion_order > rhs.insertion_order;
  }

  template <EventPayload Payload>
  [[nodiscard]] typename Scheduler<Payload>::TimePoint
  Scheduler<Payload>::m_to_time_point(
    const std::int64_t unix_nanoseconds
  ) noexcept
  {
    const auto since_epoch =
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::nanoseconds{unix_nanoseconds}
      );

    return TimePoint{since_epoch};
  }

  template <EventPayload Payload>
  [[nodiscard]] ScheduleID Scheduler<Payload>::schedule(
    const EventID event_id,
    Payload payload,
    const TimePoint forward_at,
    const Superstep superstep
  )
  {
    return m_schedule(
      std::nullopt,
      event_id,
      std::move(payload),
      forward_at,
      superstep
    );
  }

  template <EventPayload Payload>
  [[nodiscard]] ScheduleID
  Scheduler<Payload>::schedule_at_unix_nanoseconds(
    const EventID event_id,
    Payload payload,
    const std::int64_t forward_at_unix_nanoseconds,
    const Superstep superstep
  )
  {
    if (forward_at_unix_nanoseconds <= 0)
    {
      throw std::invalid_argument(
        "scheduled events require a positive Unix nanosecond timestamp"
      );
    }

    return schedule(
      event_id,
      std::move(payload),
      m_to_time_point(forward_at_unix_nanoseconds),
      superstep
    );
  }

  template <EventPayload Payload>
  [[nodiscard]] ScheduleID Scheduler<Payload>::schedule_one(
    const SubscriberID subscriber_id,
    const EventID event_id,
    Payload payload,
    const TimePoint forward_at,
    const Superstep superstep
  )
  {
    return m_schedule(
      subscriber_id,
      event_id,
      std::move(payload),
      forward_at,
      superstep
    );
  }

  template <EventPayload Payload>
  [[nodiscard]] ScheduleID
  Scheduler<Payload>::schedule_one_at_unix_nanoseconds(
    const SubscriberID subscriber_id,
    const EventID event_id,
    Payload payload,
    const std::int64_t forward_at_unix_nanoseconds,
    const Superstep superstep
  )
  {
    if (forward_at_unix_nanoseconds <= 0)
    {
      throw std::invalid_argument(
        "scheduled events require a positive Unix nanosecond timestamp"
      );
    }

    return schedule_one(
      subscriber_id,
      event_id,
      std::move(payload),
      m_to_time_point(forward_at_unix_nanoseconds),
      superstep
    );
  }

  template <EventPayload Payload>
  [[nodiscard]] ScheduleID Scheduler<Payload>::m_schedule(
    std::optional<SubscriberID> subscriber_id,
    const EventID event_id,
    Payload payload,
    const TimePoint forward_at,
    const Superstep superstep
  )
  {
    ScheduleID schedule_id{0UL};

    {
      const std::lock_guard lock{m_mutex};

      if (!m_accepting)
      {
        throw std::logic_error(
          "scheduler is stopped and is not accepting new events"
        );
      }

      schedule_id = m_next_schedule_id++;

      m_schedule_heap.emplace(
        ScheduledItem{
          forward_at,
          m_next_insertion_order++,
          schedule_id,
          subscriber_id,
          event_id,
          superstep,
          std::move(payload),
        }
      );

      m_scheduled_events.fetch_add(
        1UL,
        std::memory_order_relaxed
      );
    }

    // The new request may be earlier than the deadline currently observed by
    // the worker, so every insertion wakes the timed wait.
    m_condition.notify_one();

    return schedule_id;
  }

  template <EventPayload Payload>
  void Scheduler<Payload>::start(void)
  {
    const std::lock_guard lock{m_mutex};

    if (m_running.load(std::memory_order_acquire))
    {
      return;
    }

    m_accepting = true;
    m_running.store(true, std::memory_order_release);

    try
    {
      m_worker = std::jthread(
        [this](const std::stop_token stop_token) noexcept
        {
          m_run(stop_token);
        }
      );
    }
    catch (...)
    {
      m_accepting = false;
      m_running.store(false, std::memory_order_release);
      throw;
    }
  }

  template <EventPayload Payload>
  void Scheduler<Payload>::stop(void) noexcept
  {
    {
      const std::lock_guard lock{m_mutex};
      m_accepting = false;
    }

    if (m_worker.joinable())
    {
      m_worker.request_stop();
      m_condition.notify_all();
      m_worker.join();
    }

    m_running.store(false, std::memory_order_release);
  }

  template <EventPayload Payload>
  [[nodiscard]] bool Scheduler<Payload>::running(void) const noexcept
  {
    return m_running.load(std::memory_order_acquire);
  }

  template <EventPayload Payload>
  [[nodiscard]] std::size_t
  Scheduler<Payload>::pending_events(void) const noexcept
  {
    const std::lock_guard lock{m_mutex};
    return m_schedule_heap.size();
  }

  template <EventPayload Payload>
  [[nodiscard]] std::size_t
  Scheduler<Payload>::scheduled_events(void) const noexcept
  {
    return m_scheduled_events.load(std::memory_order_relaxed);
  }

  template <EventPayload Payload>
  [[nodiscard]] std::size_t
  Scheduler<Payload>::forwarded_events(void) const noexcept
  {
    return m_forwarded_events.load(std::memory_order_relaxed);
  }

  template <EventPayload Payload>
  [[nodiscard]] std::size_t
  Scheduler<Payload>::failed_events(void) const noexcept
  {
    return m_failed_events.load(std::memory_order_relaxed);
  }

  template <EventPayload Payload>
  [[nodiscard]] typename Scheduler<Payload>::RetryDelay
  Scheduler<Payload>::retry_delay(void) const noexcept
  {
    return m_retry_delay;
  }

  template <EventPayload Payload>
  [[nodiscard]] bool Scheduler<Payload>::m_root_changed(
    const TimePoint observed_deadline,
    const std::uint64_t observed_order
  ) const noexcept
  {
    if (m_schedule_heap.empty())
    {
      return true;
    }

    const ScheduledItem& current = m_schedule_heap.top();

    return
      current.forward_at < observed_deadline
      || (
        current.forward_at == observed_deadline
        && current.insertion_order < observed_order
      );
  }

  template <EventPayload Payload>
  void Scheduler<Payload>::m_run(
    const std::stop_token stop_token
  ) noexcept
  {
    std::unique_lock lock{m_mutex};

    while (!stop_token.stop_requested())
    {
      if (m_schedule_heap.empty())
      {
        m_condition.wait(
          lock,
          [this, &stop_token]
          {
            return
              stop_token.stop_requested()
              || !m_schedule_heap.empty();
          }
        );

        continue;
      }

      const ScheduledItem& root = m_schedule_heap.top();
      const TimePoint observed_deadline = root.forward_at;
      const std::uint64_t observed_order = root.insertion_order;

      const bool interrupted = m_condition.wait_until(
        lock,
        observed_deadline,
        [this, &stop_token, observed_deadline, observed_order]
        {
          return
            stop_token.stop_requested()
            || m_root_changed(
              observed_deadline,
              observed_order
            );
        }
      );

      if (stop_token.stop_requested())
      {
        break;
      }

      if (interrupted)
      {
        continue;
      }

      // Remove before publishing so re-entrant EventBus callbacks cannot
      // observe the request as still pending.
      ScheduledItem due = m_schedule_heap.extract_top();
      lock.unlock();

      try
      {
        if (due.subscriber_id.has_value())
        {
          static_cast<void>(
            m_event_bus.publish_one(
              *due.subscriber_id,
              due.event_id,
              due.payload,
              due.superstep
            )
          );
        }
        else
        {
          static_cast<void>(
            m_event_bus.publish(
              due.event_id,
              due.payload,
              due.superstep
            )
          );
        }

        m_forwarded_events.fetch_add(
          1UL,
          std::memory_order_relaxed
        );

        lock.lock();
      }
      catch (const std::overflow_error&)
      {
        // Bounded subscriber partitions may be temporarily full. Preserve the
        // request at the same deadline/insertion order and retry after a short
        // backoff instead of silently dropping scheduled work.
        lock.lock();
        m_schedule_heap.push(std::move(due));

        m_condition.wait_for(
          lock,
          m_retry_delay,
          [&stop_token]
          {
            return stop_token.stop_requested();
          }
        );
      }
      catch (...)
      {
        const auto error = std::current_exception();

        m_failed_events.fetch_add(
          1UL,
          std::memory_order_relaxed
        );

        m_report_error(due.schedule_id, error);
        lock.lock();
      }
    }
  }

  template <EventPayload Payload>
  void Scheduler<Payload>::m_report_error(
    const ScheduleID schedule_id,
    std::exception_ptr error
  ) noexcept
  {
    if (!m_error_handler)
    {
      return;
    }

    try
    {
      m_error_handler(schedule_id, std::move(error));
    }
    catch (...)
    {
      // Asynchronous diagnostic callbacks must never terminate the scheduler.
    }
  }

} // namespace yellowstone
