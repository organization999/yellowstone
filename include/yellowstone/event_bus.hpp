/**
 * @file event_bus.hpp
 * @brief Declares the generic fan-out event bus backed by Phelps workers.
 */
#pragma once

#include "phelps/manager.hpp"
#include "yellowstone/event.hpp"
#include "yellowstone/partition.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace yellowstone
{
  struct PublishResult final
  {
    std::size_t delivered{0UL};
    std::size_t coalesced{0UL};
  };

  /**
   * @brief Standalone generic event bus preserving the old Uni fan-out model.
   *
   * @tparam Payload Application-selected event payload type.
   *
   * Every subscriber owns an independent bounded partition. publish() fans a
   * copy of the event into every current subscriber partition. One Phelps worker
   * is registered for every active subscriber when start() is called, so each
   * partition has exactly one consumer and per-subscriber ordering is stable.
   *
   * Events are consumed by lowest superstep first and FIFO within a superstep.
   * Duplicate (event-id, superstep) entries are coalesced in place using the
   * configured payload merger.
   */
  template <EventPayload Payload>
  class EventBus final
  {
  public:
    using EventType = Event<Payload>;
    using Handler = std::function<void(const EventType&)>;
    using Merger = std::function<void(Payload&, const Payload&)>;
    using ErrorHandler =
      std::function<void(SubscriberID, const EventType&, std::exception_ptr)>;
    using PartitionType = Partition<Payload, Merger>;

    static constexpr std::size_t k_max_subscribers =
      phelps::WorkerManager::k_max_workers;

    explicit EventBus(
      std::size_t partition_capacity = 1024UL,
      Merger merger = {},
      ErrorHandler error_handler = {}
    );

    EventBus(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    void operator=(const EventBus&) = delete;
    void operator=(EventBus&&) = delete;

    ~EventBus() noexcept;

    [[nodiscard]] SubscriberID subscribe(Handler handler);
    void unsubscribe(SubscriberID subscriber_id);

    [[nodiscard]] PublishResult publish(
      EventID event_id,
      Payload payload,
      Superstep superstep = 0UL
    );

    [[nodiscard]] PublishResult publish_one(
      SubscriberID subscriber_id,
      EventID event_id,
      Payload payload,
      Superstep superstep = 0UL
    );

    void start(void);
    void stop(void) noexcept;

    [[nodiscard]] bool running(void) const noexcept;
    [[nodiscard]] std::size_t subscriber_count(void) const noexcept;
    [[nodiscard]] std::size_t pending(SubscriberID subscriber_id) const;
    [[nodiscard]] std::size_t partition_capacity(void) const noexcept;

  private:
    struct Subscriber final
    {
      Subscriber(
        SubscriberID subscriber_id,
        Handler subscriber_handler,
        std::size_t capacity,
        const Merger& merger
      );

      SubscriberID id;
      Handler handler;
      PartitionType partition;
    };

    class ConsumerWorker final : public phelps::WorkerManager::Worker
    {
    public:
      ConsumerWorker(EventBus* bus, SubscriberID subscriber_id) noexcept;
      void work(void) override;

    private:
      EventBus* m_bus;
      SubscriberID m_subscriber_id;
    };

    using SubscriberPtr = std::shared_ptr<Subscriber>;
    using Subscribers = std::array<SubscriberPtr, k_max_subscribers>;
    using WorkerPtr = std::shared_ptr<ConsumerWorker>;

    [[nodiscard]] SubscriberPtr m_subscriber(SubscriberID subscriber_id) const;
    [[nodiscard]] std::vector<SubscriberPtr> m_snapshot_subscribers(void) const;
    void m_consume_one(SubscriberID subscriber_id) noexcept;
    void m_report_error(
      SubscriberID subscriber_id,
      const EventType& event,
      std::exception_ptr error
    ) noexcept;

    const std::size_t m_partition_capacity;
    Merger m_merger;
    ErrorHandler m_error_handler;

    mutable std::mutex m_lifecycle_mutex{};
    mutable std::mutex m_publish_mutex{};
    mutable std::mutex m_subscriber_mutex{};

    Subscribers m_subscribers{};
    std::size_t m_subscriber_count{0UL};
    std::atomic<Sequence> m_sequence{0UL};
    std::atomic<bool> m_running{false};

    std::unique_ptr<phelps::WorkerManager> m_manager{};
    std::vector<WorkerPtr> m_workers{};
  };

} // namespace yellowstone

#include "event_bus.inl"
