#pragma once

#include <utility>

namespace yellowstone
{
  template <EventPayload Payload>
  Event<Payload>::Event(
    const EventID id,
    const Superstep superstep,
    const Sequence sequence,
    Payload payload
  )
    : m_id(id),
      m_superstep(superstep),
      m_sequence(sequence),
      m_payload(std::move(payload))
  {
  }

  template <EventPayload Payload>
  [[nodiscard]] EventID Event<Payload>::id(void) const noexcept
  {
    return m_id;
  }

  template <EventPayload Payload>
  [[nodiscard]] Superstep Event<Payload>::superstep(void) const noexcept
  {
    return m_superstep;
  }

  template <EventPayload Payload>
  [[nodiscard]] Sequence Event<Payload>::sequence(void) const noexcept
  {
    return m_sequence;
  }

  template <EventPayload Payload>
  [[nodiscard]] const Payload& Event<Payload>::payload(void) const noexcept
  {
    return m_payload;
  }

  template <EventPayload Payload>
  [[nodiscard]] Payload& Event<Payload>::mutable_payload(void) noexcept
  {
    return m_payload;
  }

} // namespace yellowstone
