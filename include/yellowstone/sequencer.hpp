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

/**
 * @file sequencer.hpp
 * @brief Declares the Sequencer interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#include "sequence.hpp"

#include <cstddef>
#include <cstdint>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  template <std::size_t N>
  /**
   * @class SingleProducerSequencer
   * @brief Represents the SingleProducerSequencer type used by the event component.
   */
  class SingleProducerSequencer final
  {
    /**
     * @brief Performs the Static Assert operation for the event component.
     * @param UL Value forwarded to the static_assert operation.
     * @param two Value forwarded to the static_assert operation.
     */
    static_assert((N & (N - 1UL)) == 0UL, "Ring size N must be power-of-two");

    static constexpr std::int64_t kInitialCursor = (-1);

    std::int64_t m_next_value{kInitialCursor};
    std::int64_t m_cursor{kInitialCursor};

    // Cached gating sequence = "prediction" of consumer progress.
    std::int64_t m_cached_gating{kInitialCursor};

    // SPSC here: one gating sequence (consumer).
    Sequence* m_gating{nullptr};

    /**
     * @brief Performs the Min Gating operation for the event component.
     * @param g Value forwarded to the min_gating operation.
     * @return Result produced by this operation.
     */
    static std::int64_t min_gating(const Sequence* g) noexcept;

  public:
    static constexpr int kOk   =  0;
    static constexpr int kFull = -2;

    /**
     * @brief Performs the SingleProducerSequencer operation for the event component.
     */
    explicit SingleProducerSequencer() noexcept = default;

    /**
     * @brief Performs the Add Gating Sequence operation for the event component.
     * @param gating Value forwarded to the add_gating_sequence operation.
     */
    void add_gating_sequence(Sequence& gating) noexcept;

    /**
     * @brief Performs the Cursor operation for the event component.
     * @return Result produced by this operation.
     */
    [[nodiscard]] std::int64_t cursor(void) const noexcept;

    /**
     * @brief Performs the Remaining Capacity operation for the event component.
     * @return Result produced by this operation.
     */
    [[nodiscard]] std::int64_t remaining_capacity(void) const noexcept;

    // Try-claim without waiting/spinning: returns kFull if no capacity.
    /**
     * @brief Performs the Try Next operation for the event component.
     * @param out_seq Value forwarded to the try_next operation.
     * @return Result produced by this operation.
     */
    [[nodiscard]] int try_next(std::int64_t& out_seq) noexcept;

    // Claim next sequence (Disruptor-style).
    // In a real Disruptor, slow path would spin/park; here we just loop.
    /**
     * @brief Performs the Next operation for the event component.
     * @return Result produced by this operation.
     */
    [[nodiscard]] std::int64_t next(void) noexcept;

    // Publish makes claimed sequence visible to consumers.
    /**
     * @brief Performs the Publish operation for the event component.
     * @param seq Value forwarded to the publish operation.
     */
    void publish(const std::int64_t seq) noexcept;

  }; // class SingleProducerSequencer final

} // namespace event

#include "sequencer.inl"
