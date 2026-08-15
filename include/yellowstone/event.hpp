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

#include "command.hpp"
#include "common.hpp"

#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <type_traits>
#include <utility>

/**
 * @file event.hpp
 * @brief Defines executable event abstractions and tracked completion events.
 */

namespace event
{
  /**
   * @typedef EventCallback
   * @brief Function pointer type for executable event callbacks.
   */
  using EventCallback = void (*)(void);

  /**
   * @typedef EventPriority
   * @brief Integral type representing event priority.
   */
  using EventPriority = std::uint32_t;

  /**
   * @typedef EventTimestamp
   * @brief Alias for timestamp type used in scheduled events.
   */
  using EventTimestamp = Timestamp;

  /**
   * @enum EventScheduleType
   * @brief Defines how an executable event should be scheduled.
   */
  enum class EventScheduleType
  {
    IMMEDIATE,
    TIMESTAMP,
    UNSUPPORTED
  };

  /**
   * @class AEvent
   * @brief Abstract base class providing common scheduling metadata for events.
   */
  struct AEvent
  {
    /**
     * @brief Scheduling type of the event.
     */
    const EventScheduleType type;

    /**
     * @brief Callback associated with this event.
     */
    const EventCallback callback;

    /**
     * @brief Event priority value.
     */
    EventPriority priority;

    /**
     * @brief Execution timestamp.
     */
    EventTimestamp timestamp;

    /**
     * @brief Performs the ~AEvent operation for the event component.
     */
    virtual ~AEvent() noexcept = default;

    /**
     * @brief Returns true when this event carries graph envelope metadata.
     */
    [[nodiscard]] virtual bool has_graph(void) const noexcept { return false; }

    /**
     * @brief Returns the graph superstep when @ref has_graph is true.
     */
    [[nodiscard]] virtual std::uint64_t graph_superstep(void) const noexcept { return 0ULL; }

    /**
     * @brief Returns the graph queue identity when @ref has_graph is true.
     */
    [[nodiscard]] virtual std::uint64_t graph_queue_id(void) const noexcept { return 0ULL; }

    /**
     * @brief Coalesces graph metadata from @p other into this event when compatible.
     *
     * @return True when any merge occurred; false otherwise.
     */
    virtual bool graph_merge_from(const AEvent& /*other*/) noexcept { return false; }

    /**
     * @brief Produces a heap-allocated clone of this event envelope.
     *
     * @details
     * This is used by the EventBus to copy a published envelope into each
     * subscriber partition without sharing mutable graph metadata across
     * partitions.
     */
    [[nodiscard]] virtual std::shared_ptr<AEvent> clone(void) const noexcept { return nullptr; }

  protected:
    /**
     * @brief Default-constructs an event with "unsupported" schedule metadata.
     *
     * @details
     * This exists so derived utility types (e.g., acknowledgement-tracking `Event<T,N>`)
     * can inherit common metadata without requiring a concrete scheduling decision.
     */
    AEvent() noexcept
      : type(EventScheduleType::UNSUPPORTED), callback(nullptr), priority(0U), timestamp{}
    {}

    /**
     * @brief Constructs an immediate-schedule event.
     *
     * @param priority_ Event priority.
     * @param type_ Scheduling type.
     * @param callback_ Callback to invoke.
     */
    AEvent(EventPriority priority_, const EventScheduleType type_,
           const EventCallback callback_) noexcept;

    /**
     * @brief Constructs a timestamp-based event.
     *
     * @param priority_ Event priority.
     * @param type_ Scheduling type.
     * @param callback_ Callback to invoke.
     * @param timestamp_ Event timestamp.
     */
    AEvent(EventPriority priority_, const EventScheduleType type_,
           const EventCallback callback_, EventTimestamp timestamp_) noexcept;
  };

  /**
   * @class ImmediateEvent
   * @brief Concrete executable event scheduled for immediate execution.
   */
  struct ImmediateEvent final : public AEvent
  {
    ImmediateEvent(const EventPriority priority_,
                   const EventCallback callback_) noexcept
      : AEvent(priority_, EventScheduleType::IMMEDIATE, callback_)
    {}
  };

  /**
   * @class TimestampEvent
   * @brief Concrete executable event scheduled by timestamp.
   */
  struct TimestampEvent final : public AEvent
  {
    TimestampEvent(const EventPriority priority_,
                   const EventCallback callback_,
                   const EventTimestamp timestamp_) noexcept
      : AEvent(priority_, EventScheduleType::TIMESTAMP, callback_, timestamp_)
    {}
  };

  /**
   * @class Event
   * @brief Tracks acknowledgement state for a payload across @p N viewers.
   *
   * @tparam T Payload type.
   * @tparam N Number of possible viewers.
   */
  template <typename T, std::size_t N>
  /**
   * @struct Event
   * @brief Represents the Event data structure used by the event component.
   */
  struct Event final : public AEvent
  {
    /**
     * @brief Performs the Static Assert operation for the event component.
     * @param UL Value forwarded to the static_assert operation.
     * @param zero Value forwarded to the static_assert operation.
     */
    static_assert(N > 0UL, "Event<N>: N must be greater than zero.");

    std::size_t    pending{0UL};
    std::bitset<N> viewed{};
    T              payload{};

    /**
     * @brief Graph envelope metadata (opt-in; does not allocate heap memory).
     */
    struct GraphEdge final
    {
      std::uint64_t to_id{0ULL};
      float         weight{0.0F};
    };

    static constexpr std::size_t kMaxGraphEdgesInline = 32UL;

    enum class GraphEdgeAddResult : std::uint8_t
    {
      Added,
      Duplicate,
      Full
    };

    bool                                     graph_enabled{false};
    std::uint64_t                            graph_node_id{0ULL};
    std::uint64_t                            graph_superstep_{0ULL};
    float                                    graph_node_value{0.0F};
    std::uint64_t                            graph_queue_id_{0ULL};
    std::size_t                              graph_edges_size{0UL};
    std::array<GraphEdge, kMaxGraphEdgesInline> graph_edges{};

    /**
     * @brief Performs the Event operation for the event component.
     * @param T Value forwarded to the Event operation.
     */
    Event() noexcept(std::is_nothrow_default_constructible_v<T>);

    /**
     * @brief Performs the Event operation for the event component.
     * @param T Value forwarded to the Event operation.
     */
    explicit Event(const std::size_t pending_) noexcept(std::is_nothrow_default_constructible_v<T>);

    /**
     * @brief Constructs a scheduled acknowledgement-tracking event envelope.
     *
     * @param type_ Scheduling type of the event.
     * @param pending_ Number of acknowledgements expected.
     */
    Event(const EventScheduleType type, const std::size_t pending_) noexcept(std::is_nothrow_default_constructible_v<T>);

    /**
     * @brief Performs the Event operation for the event component.
     * @param pending_ Value forwarded to the Event operation.
     * @param T Value forwarded to the Event operation.
     */
    Event(const std::size_t pending_, const T& payload_) noexcept(std::is_nothrow_copy_constructible_v<T>);

    /**
     * @brief Performs the Event operation for the event component.
     * @param pending_ Value forwarded to the Event operation.
     * @param T Value forwarded to the Event operation.
     */
    Event(const std::size_t pending_, T&& payload_) noexcept(std::is_nothrow_move_constructible_v<T>);

    /**
     * @brief Constructs a scheduled acknowledgement-tracking event envelope.
     *
     * @param type_ Scheduling type of the event.
     * @param pending_ Number of acknowledgements expected.
     * @param payload_ Payload to transport.
     */
    Event(const EventScheduleType type, const std::size_t pending_, const T& payload_) noexcept(std::is_nothrow_copy_constructible_v<T>);

    /**
     * @brief Constructs a scheduled acknowledgement-tracking event envelope.
     *
     * @param type_ Scheduling type of the event.
     * @param pending_ Number of acknowledgements expected.
     * @param payload_ Payload to transport.
     */
    Event(const EventScheduleType type, const std::size_t pending_, T&& payload_) noexcept(std::is_nothrow_move_constructible_v<T>);

    /**
     * @brief Performs the View operation for the event component.
     * @param i Value forwarded to the view operation.
     */
    void view(const std::size_t i) noexcept;

    /**
     * @brief Performs the Complete operation for the event component.
     * @return Result produced by this operation.
     */
    [[nodiscard]] bool complete(void) const noexcept;

    /**
     * @brief Performs the Get Pending operation for the event component.
     * @return Result produced by this operation.
     */
    [[nodiscard]] std::size_t get_pending(void) const noexcept;

    /**
     * @brief Enables graph metadata on this envelope and computes the queue id.
     */
    void set_graph(const std::uint64_t node_id,
                   const std::uint64_t superstep,
                   const float node_value = 0.0F) noexcept
    {
      graph_enabled = true;
      graph_node_id = node_id;
      graph_superstep_ = superstep;
      graph_node_value = node_value;
      graph_queue_id_ = (superstep << 32U) ^ node_id;
      graph_edges_size = 0UL;
    }

    /**
     * @brief Attempts to add an outgoing graph edge to the inline edge set.
     */
    [[nodiscard]] GraphEdgeAddResult try_add_edge(const std::uint64_t to_id,
                                                  const float weight = 1.0F) noexcept
    {
      for (std::size_t i = 0UL; i < graph_edges_size; ++i)
      {
        if (graph_edges[i].to_id == to_id)
        {
          return GraphEdgeAddResult::Duplicate;
        }
      }

      if (graph_edges_size >= kMaxGraphEdgesInline)
      {
        return GraphEdgeAddResult::Full;
      }

      graph_edges[graph_edges_size++] = GraphEdge{to_id, weight};
      return GraphEdgeAddResult::Added;
    }

    /**
     * @brief Merges inline edges from @p other into this envelope.
     */
    [[nodiscard]] bool merge_edges_from(const Event& other) noexcept
    {
      bool changed = false;

      for (std::size_t i = 0UL; i < other.graph_edges_size; ++i)
      {
        const GraphEdge& e = other.graph_edges[i];
        const auto r = try_add_edge(e.to_id, e.weight);
        if (r == GraphEdgeAddResult::Added)
        {
          changed = true;
        }
      }

      return changed;
    }

    [[nodiscard]] bool has_graph(void) const noexcept override { return graph_enabled; }
    [[nodiscard]] std::uint64_t graph_superstep(void) const noexcept override { return graph_superstep_; }
    [[nodiscard]] std::uint64_t graph_queue_id(void) const noexcept override { return graph_queue_id_; }

    bool graph_merge_from(const AEvent& other) noexcept override
    {
      if (!graph_enabled)
      {
        return false;
      }

      const auto* rhs = dynamic_cast<const Event*>(&other);
      if (rhs == nullptr)
      {
        return false;
      }

      if (!rhs->graph_enabled || rhs->graph_queue_id_ != graph_queue_id_)
      {
        return false;
      }

      return merge_edges_from(*rhs);
    }

    [[nodiscard]] std::shared_ptr<AEvent> clone(void) const noexcept override
    {
      return std::make_shared<Event>(*this);
    }

  }; // struct Event final

} // namespace event

#include "event.inl"
