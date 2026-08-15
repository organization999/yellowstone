#pragma once

#include <functional>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace yellowstone
{
  template <EventPayload Payload, typename Merge>
  Partition<Payload, Merge>::Partition(
    const std::size_t capacity,
    Merger merger
  )
    : m_capacity(capacity),
      m_merger(std::move(merger))
  {
    if (0UL == m_capacity)
    {
      throw std::invalid_argument("partition capacity must be greater than zero");
    }

    if constexpr (requires(const Merger& value) { static_cast<bool>(value); })
    {
      if (!m_merger)
      {
        throw std::invalid_argument("partition merger must be callable");
      }
    }
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] std::size_t
  Partition<Payload, Merge>::KeyHash::operator()(const Key& key) const noexcept
  {
    const auto lhs = std::hash<EventID>{}(key.id);
    const auto rhs = std::hash<Superstep>{}(key.superstep);
    return lhs ^ (rhs + 0x9e3779b97f4a7c15ULL + (lhs << 6U) + (lhs >> 2U));
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] typename Partition<Payload, Merge>::Key
  Partition<Payload, Merge>::m_key(const EventType& event) noexcept
  {
    return Key{event.id(), event.superstep()};
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] bool
  Partition<Payload, Merge>::can_accept(const EventType& event) const
  {
    std::lock_guard lock{m_mutex};
    return m_index.contains(m_key(event)) || m_size < m_capacity;
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] PushResult
  Partition<Payload, Merge>::push(EventType event)
  {
    std::unique_lock lock{m_mutex};
    const Key key = m_key(event);

    if (const auto found = m_index.find(key); found != m_index.end())
    {
      m_merger(found->second->mutable_payload(), event.payload());
      return PushResult::coalesced;
    }

    if (m_size >= m_capacity)
    {
      return PushResult::full;
    }

    auto& level = m_levels[event.superstep()];
    level.push_back(std::move(event));
    auto position = std::prev(level.end());
    m_index.emplace(key, position);
    ++m_size;

    lock.unlock();
    m_available.notify_one();
    return PushResult::inserted;
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] std::optional<typename Partition<Payload, Merge>::EventType>
  Partition<Payload, Merge>::m_pop_locked(void)
  {
    if (0UL == m_size)
    {
      return std::nullopt;
    }

    auto level = m_levels.begin();
    auto position = level->second.begin();
    const Key key = m_key(*position);
    EventType event = std::move(*position);

    level->second.erase(position);
    m_index.erase(key);
    --m_size;

    if (level->second.empty())
    {
      m_levels.erase(level);
    }

    return event;
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] std::optional<typename Partition<Payload, Merge>::EventType>
  Partition<Payload, Merge>::pop(void)
  {
    std::lock_guard lock{m_mutex};
    return m_pop_locked();
  }

  template <EventPayload Payload, typename Merge>
  template <typename Rep, typename Period>
  [[nodiscard]] std::optional<typename Partition<Payload, Merge>::EventType>
  Partition<Payload, Merge>::wait_pop_for(
    const std::chrono::duration<Rep, Period>& timeout,
    const std::atomic<bool>& running
  )
  {
    std::unique_lock lock{m_mutex};

    m_available.wait_for(
      lock,
      timeout,
      [this, &running]()
      {
        return 0UL != m_size || !running.load(std::memory_order_acquire);
      }
    );

    return m_pop_locked();
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] bool Partition<Payload, Merge>::empty(void) const
  {
    std::lock_guard lock{m_mutex};
    return 0UL == m_size;
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] std::size_t Partition<Payload, Merge>::size(void) const
  {
    std::lock_guard lock{m_mutex};
    return m_size;
  }

  template <EventPayload Payload, typename Merge>
  [[nodiscard]] std::size_t Partition<Payload, Merge>::capacity(void) const noexcept
  {
    return m_capacity;
  }

  template <EventPayload Payload, typename Merge>
  void Partition<Payload, Merge>::wake_all(void) noexcept
  {
    m_available.notify_all();
  }

} // namespace yellowstone
