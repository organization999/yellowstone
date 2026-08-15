/**
 * @file partition.hpp
 * @brief Declares a bounded subscriber partition with superstep ordering.
 */
#pragma once

#include "yellowstone/event.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace yellowstone
{
  enum class PushResult
  {
    inserted,
    coalesced,
    full,
  };

  /**
   * @brief Per-subscriber queue preserving Uni's event transport semantics.
   *
   * Events are ordered by the lowest superstep first. Events within one
   * superstep preserve insertion order. A second event with the same
   * (event-id, superstep) key is coalesced into the existing queue position by
   * invoking the configured payload merger.
   *
   * @tparam Payload Application payload type.
   * @tparam Merge Callable equivalent to void(Payload&, const Payload&).
   */
  template <
    EventPayload Payload,
    typename Merge = std::function<void(Payload&, const Payload&)>
  >
  class Partition final
  {
  public:
    using EventType = Event<Payload>;
    using Merger = Merge;

    explicit Partition(
      std::size_t capacity,
      Merger merger
    );

    Partition(const Partition&) = delete;
    Partition(Partition&&) = delete;
    void operator=(const Partition&) = delete;
    void operator=(Partition&&) = delete;

    [[nodiscard]] bool can_accept(const EventType& event) const;
    [[nodiscard]] PushResult push(EventType event);
    [[nodiscard]] std::optional<EventType> pop(void);

    template <typename Rep, typename Period>
    [[nodiscard]] std::optional<EventType> wait_pop_for(
      const std::chrono::duration<Rep, Period>& timeout,
      const std::atomic<bool>& running
    );

    [[nodiscard]] bool empty(void) const;
    [[nodiscard]] std::size_t size(void) const;
    [[nodiscard]] std::size_t capacity(void) const noexcept;
    void wake_all(void) noexcept;

  private:
    struct Key final
    {
      EventID id;
      Superstep superstep;

      [[nodiscard]] bool operator==(const Key&) const noexcept = default;
    };

    struct KeyHash final
    {
      [[nodiscard]] std::size_t operator()(const Key& key) const noexcept;
    };

    using Level = std::list<EventType>;
    using Levels = std::map<Superstep, Level>;
    using Position = typename Level::iterator;
    using Index = std::unordered_map<Key, Position, KeyHash>;

    [[nodiscard]] static Key m_key(const EventType& event) noexcept;
    [[nodiscard]] std::optional<EventType> m_pop_locked(void);

    const std::size_t m_capacity;
    Merger m_merger;

    mutable std::mutex m_mutex{};
    std::condition_variable m_available{};
    Levels m_levels{};
    Index m_index{};
    std::size_t m_size{0UL};
  };

} // namespace yellowstone

#include "partition.inl"
