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

#include <bit>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <utility>

/**
 * @file common.hpp
 * @brief Common concepts and time-related aliases shared across the runtime.
 *
 * @details
 * This header provides small, reusable building blocks used throughout the
 * codebase:
 *
 * - `PowerOfTwo<N>`:
 *     A compile-time constraint to ensure a non-zero integer `N` is a power of
 * two. Used for ring-buffer sizing and mask-based indexing.
 *
 * - `Comparable<T>`:
 *     A concept requiring that `T` supports `<` comparison producing a
 * boolean-like result. Used as a lightweight constraint for generic ordering.
 *
 * - `Clock` and `Timestamp`:
 *     A steady (monotonic) clock and its associated time_point type.
 *     Used for scheduling and timed data structures (e.g., timed heaps /
 * delayed events).
 *
 * @note Design intent
 * These utilities are kept small and dependency-light to allow inclusion in
 * core data structures without pulling in heavy headers.
 */

namespace event
{
  /* ============================================================================
   *  Concepts
   * ==========================================================================*/

  /**
   * @concept PowerOfTwo
   * @brief Checks at compile-time that `N` is a non-zero power of two.
   *
   * @tparam N Compile-time integer value.
   *
   * @details
   * This concept is satisfied if and only if:
   * - `N > 0`, and
   * - `std::has_single_bit(N)` is true.
   *
   * `std::has_single_bit()` (C++20) returns true when the value has exactly one
   * bit set in its binary representation, which is precisely the definition of
   * a power-of-two for positive integers.
   *
   * @par Why this matters
   * Many performance-oriented structures (ring buffers, bit masks) rely on a
   * power-of-two capacity to replace modulo operations with a cheap bitmask:
   * @code
   * index % N  ==  index & (N - 1)   // when N is power-of-two
   * @endcode
   *
   * @note
   * The type of `N` is `std::size_t` by template definition.
   */
  template <std::size_t N>
  concept PowerOfTwo = ((N > 0UL) && std::has_single_bit(N));

  /**
   * @concept Comparable
   * @brief Requires that `T` supports the less-than operator (`<`).
   *
   * @tparam T Type to test.
   *
   * @details
   * This concept is satisfied if the expression:
   * @code
   * a < b
   * @endcode
   * is valid for objects `a` and `b` of type `T`, and the result is convertible
   * to `bool`.
   *
   * @note
   * This is a minimal ordering requirement; it does not require:
   * - `>`, `<=`, `>=`
   * - a total order
   * - `std::three_way_comparable`
   *
   * It is useful for lightweight constraints in generic code where only `<` is
   * needed.
   */
  template <typename T>
  /**
   * @brief Performs the Requires operation for the event component.
   * @param a Value forwarded to the requires operation.
   * @param b Value forwarded to the requires operation.
   * @return Result produced by this operation.
   */
  concept Comparable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
  };

  /* ============================================================================
  *  Time aliases
  * ==========================================================================*/

  /**
   * @brief Monotonic clock used across the runtime.
   *
   * @details
   * `std::chrono::steady_clock` is guaranteed to be monotonic (never goes
   * backwards), making it suitable for measuring intervals and scheduling
   * time-based events.
   *
   * @note
   * A steady clock is preferred over `system_clock` for scheduling logic because
   * it is not affected by wall-clock adjustments (NTP, user changes, DST, etc.).
   */
  using Clock = std::chrono::steady_clock;

  /**
   * @brief Timestamp type used for scheduling and timed data structures.
   *
   * @details
   * A `Timestamp` represents a point in time according to `Clock`:
   * @code
   * Timestamp t = Clock::now();
   * @endcode
   *
   * Common usage includes:
   * - event scheduling (`event.timestamp`)
   * - timed heaps that release elements when `timestamp <= Clock::now()`
   */
  using Timestamp = std::chrono::time_point<Clock>;

  /**
   * @namespace ts
   * @brief Contains the event facilities for this part of the codebase.
   */
  namespace ts
  {
    /**
     * @brief Performs the Cout Mutex operation for the event component.
     * @return Result produced by this operation.
     */
    inline std::mutex& cout_mutex(void) noexcept
    {
      static std::mutex m;
      return m;
    }

    template <typename... Args>
    void println(Args&&... args)
    {
      /**
       * @brief Performs the G operation for the event component.
       * @param cout_mutex Value forwarded to the g operation.
       * @return Result produced by this operation.
       */
      std::lock_guard<std::mutex> g(cout_mutex());
      ((std::cout << std::forward<Args>(args)), ...);
      std::cout << '\n';
    }

    template <typename... Args>
    void print(Args&&... args)
    {
      /**
       * @brief Performs the G operation for the event component.
       * @param cout_mutex Value forwarded to the g operation.
       * @return Result produced by this operation.
       */
      std::lock_guard<std::mutex> g(cout_mutex());
      ((std::cout << std::forward<Args>(args)), ...);
    }
  } // namespace ts
} // namespace event
