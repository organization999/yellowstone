/**
 * @file event.hpp
 * @brief Declares the generic Yellowstone event value type.
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <utility>

namespace yellowstone
{
  using EventID = std::uint64_t;
  using Superstep = std::uint64_t;
  using Sequence = std::uint64_t;
  using SubscriberID = std::uint64_t;

  /**
   * @brief Payload requirement for events that may be fanned out to subscribers.
   *
   * Fan-out gives each subscriber partition its own Event value. Payloads must
   * therefore be copy constructible and assignable. Applications that need to
   * transport expensive or non-copyable objects should make the payload a
   * copyable ownership handle such as std::shared_ptr<T>.
   */
  template <typename T>
  concept EventPayload =
    std::copy_constructible<T>
    && std::assignable_from<T&, const T&>;

  template <EventPayload Payload, typename Merge>
  class Partition;

  /**
   * @brief Generic event transported by EventBus.
   *
   * @tparam Payload Application-defined payload type.
   *
   * Yellowstone deliberately imposes no expression, graph, persistence, or
   * execution-request semantics on Payload. The application chooses the type.
   * Event identity and ordering metadata belong to Yellowstone; the payload
   * belongs to the application.
   */
  template <EventPayload Payload>
  class Event final
  {
  public:
    using PayloadType = Payload;

    Event(
      EventID id,
      Superstep superstep,
      Sequence sequence,
      Payload payload
    );

    [[nodiscard]] EventID id(void) const noexcept;
    [[nodiscard]] Superstep superstep(void) const noexcept;
    [[nodiscard]] Sequence sequence(void) const noexcept;
    [[nodiscard]] const Payload& payload(void) const noexcept;

  private:
    template <EventPayload P, typename M>
    friend class Partition;

    [[nodiscard]] Payload& mutable_payload(void) noexcept;

    EventID m_id;
    Superstep m_superstep;
    Sequence m_sequence;
    Payload m_payload;
  };

} // namespace yellowstone

#include "event.inl"
