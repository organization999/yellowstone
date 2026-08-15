/**
 * @file scheduler.hpp
 * @brief Declares Yellowstone's generic deadline scheduler.
 */
#pragma once

#include "yellowstone/collections/pairing_heap.hpp"
#include "yellowstone/event_bus.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace yellowstone
{
  using ScheduleID = std::uint64_t;

  /**
   * @brief Schedule generic Yellowstone events for publication at deadlines.
   *
   * @tparam Payload Application-selected EventBus payload type.
   *
   * The scheduler preserves the core legacy semantics while removing the
   * analytics/expression-specific message envelope:
   *
   * - earliest wall-clock deadline is always the pairing-heap root;
   * - equal deadlines are forwarded in insertion order;
   * - inserting an earlier deadline wakes the timing thread;
   * - an event is removed from the heap before EventBus publication;
   * - publication never occurs before the requested deadline;
   * - EventBus capacity overflow is retried instead of dropping work;
   * - stop() joins the timing thread without discarding pending work.
   *
   * Scheduler owns one dedicated timed jthread. Phelps remains responsible for
   * EventBus consumer workers. EventBus must outlive Scheduler.
   */
  template <EventPayload Payload>
  class Scheduler final
  {
  public:
    using Bus = EventBus<Payload>;
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    using RetryDelay = std::chrono::nanoseconds;
    using ErrorHandler =
      std::function<void(ScheduleID, std::exception_ptr)>;

    explicit Scheduler(
      Bus& event_bus,
      RetryDelay retry_delay = std::chrono::milliseconds{1},
      ErrorHandler error_handler = {}
    );

    Scheduler(const Scheduler&) = delete;
    Scheduler(Scheduler&&) = delete;
    void operator=(const Scheduler&) = delete;
    void operator=(Scheduler&&) = delete;

    ~Scheduler() noexcept;

    [[nodiscard]] ScheduleID schedule(
      EventID event_id,
      Payload payload,
      TimePoint forward_at,
      Superstep superstep = 0UL
    );

    [[nodiscard]] ScheduleID schedule_at_unix_nanoseconds(
      EventID event_id,
      Payload payload,
      std::int64_t forward_at_unix_nanoseconds,
      Superstep superstep = 0UL
    );

    [[nodiscard]] ScheduleID schedule_one(
      SubscriberID subscriber_id,
      EventID event_id,
      Payload payload,
      TimePoint forward_at,
      Superstep superstep = 0UL
    );

    [[nodiscard]] ScheduleID schedule_one_at_unix_nanoseconds(
      SubscriberID subscriber_id,
      EventID event_id,
      Payload payload,
      std::int64_t forward_at_unix_nanoseconds,
      Superstep superstep = 0UL
    );

    /**
     * @brief Start or restart the scheduler timing thread.
     *
     * Pending requests survive stop() and resume after start().
     */
    void start(void);

    /**
     * @brief Stop accepting new requests and join the timing thread.
     */
    void stop(void) noexcept;

    [[nodiscard]] bool running(void) const noexcept;
    [[nodiscard]] std::size_t pending_events(void) const noexcept;
    [[nodiscard]] std::size_t scheduled_events(void) const noexcept;
    [[nodiscard]] std::size_t forwarded_events(void) const noexcept;
    [[nodiscard]] std::size_t failed_events(void) const noexcept;
    [[nodiscard]] RetryDelay retry_delay(void) const noexcept;

  private:
    struct ScheduledItem final
    {
      TimePoint forward_at{};
      std::uint64_t insertion_order{0UL};
      ScheduleID schedule_id{0UL};
      std::optional<SubscriberID> subscriber_id{};
      EventID event_id{0UL};
      Superstep superstep{0UL};
      Payload payload;
    };

    struct EarlierDeadline final
    {
      [[nodiscard]] bool operator()(
        const ScheduledItem& lhs,
        const ScheduledItem& rhs
      ) const noexcept;
    };

    using ScheduleHeap =
      collections::PairingHeap<ScheduledItem, EarlierDeadline>;

    [[nodiscard]] static TimePoint m_to_time_point(
      std::int64_t unix_nanoseconds
    ) noexcept;

    [[nodiscard]] ScheduleID m_schedule(
      std::optional<SubscriberID> subscriber_id,
      EventID event_id,
      Payload payload,
      TimePoint forward_at,
      Superstep superstep
    );

    [[nodiscard]] bool m_root_changed(
      TimePoint observed_deadline,
      std::uint64_t observed_order
    ) const noexcept;

    void m_run(std::stop_token stop_token) noexcept;

    void m_report_error(
      ScheduleID schedule_id,
      std::exception_ptr error
    ) noexcept;

    Bus& m_event_bus;
    const RetryDelay m_retry_delay;
    ErrorHandler m_error_handler;

    mutable std::mutex m_mutex{};
    std::condition_variable m_condition{};
    ScheduleHeap m_schedule_heap{};
    std::jthread m_worker{};

    bool m_accepting{false};
    std::uint64_t m_next_insertion_order{0UL};
    ScheduleID m_next_schedule_id{1UL};

    std::atomic<bool> m_running{false};
    std::atomic<std::size_t> m_scheduled_events{0UL};
    std::atomic<std::size_t> m_forwarded_events{0UL};
    std::atomic<std::size_t> m_failed_events{0UL};
  };

} // namespace yellowstone

#include "scheduler.inl"
