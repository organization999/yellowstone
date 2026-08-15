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

#include "sequence.hpp"

#include <cstddef>
#include <cstdint>

namespace event
{
  template <std::size_t N>
  inline std::int64_t SingleProducerSequencer<N>::min_gating(const Sequence* g) noexcept
  {
    return g ? g->get() : kInitialCursor;
  }

  template <std::size_t N>
  inline void SingleProducerSequencer<N>::add_gating_sequence(Sequence& gating) noexcept
  {
    m_gating = &gating;
    m_cached_gating = min_gating(m_gating);
  }

  template <std::size_t N>
  inline std::int64_t SingleProducerSequencer<N>::cursor(void) const noexcept
  {
    return m_cursor;
  }

  template <std::size_t N>
  inline std::int64_t SingleProducerSequencer<N>::remaining_capacity(void) const noexcept
  {
    const std::int64_t produced = m_next_value;
    const std::int64_t consumed = min_gating(m_gating);
    const std::int64_t used     = produced - consumed;
    return static_cast<std::int64_t>(N) - used;
  }

  template <std::size_t N>
  inline int SingleProducerSequencer<N>::try_next(std::int64_t& out_seq) noexcept
  {
    const std::int64_t next       = m_next_value + 1;
    const std::int64_t wrap_point = next - static_cast<std::int64_t>(N);

    if (wrap_point > m_cached_gating)
    {
      const std::int64_t g = min_gating(m_gating);
      m_cached_gating = g;

      if (wrap_point > g)
      {
        return kFull;
      }
    }

    m_next_value = next;
    out_seq = next;
    return kOk;
  }

  template <std::size_t N>
  inline std::int64_t SingleProducerSequencer<N>::next(void) noexcept
  {
    const std::int64_t next       = m_next_value + 1;
    const std::int64_t wrap_point = next - static_cast<std::int64_t>(N);

    if (wrap_point > m_cached_gating)
    {
      while (true)
      {
        const std::int64_t g = min_gating(m_gating);
        m_cached_gating = g;

        if (wrap_point <= g)
        {
          break;
        }
      }
    }

    m_next_value = next;
    return next;
  }

  template <std::size_t N>
  inline void SingleProducerSequencer<N>::publish(const std::int64_t seq) noexcept
  {
    m_cursor = seq;
  }
} // namespace event
