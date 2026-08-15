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
 * @file cache.hpp
 * @brief Declares the Cache interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#include <atomic>
#include <cstddef>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  template <typename T, std::size_t CacheLineSize = 64UL>
  /**
   * @struct alignas
   * @brief Represents the Alignas data structure used by the event component.
   */
  struct alignas(CacheLineSize) CacheLinePaddedAtomic final
  {
    std::atomic<T> value;
    unsigned char pad[CacheLineSize - (sizeof(std::atomic<T>) % CacheLineSize)];

    /**
     * @brief Performs the CacheLinePaddedAtomic operation for the event component.
     */
    CacheLinePaddedAtomic() noexcept = default;

    explicit CacheLinePaddedAtomic(const T v) noexcept : value(v), pad{} {}

    CacheLinePaddedAtomic(const CacheLinePaddedAtomic &) = delete;

    /**
     * @brief Performs the Operator= operation for the event component.
     * @param CacheLinePaddedAtomic Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    CacheLinePaddedAtomic &operator=(const CacheLinePaddedAtomic &) = delete;

    /**
     * @brief Performs the CacheLinePaddedAtomic operation for the event component.
     * @param CacheLinePaddedAtomic Value forwarded to the CacheLinePaddedAtomic operation.
     */
    CacheLinePaddedAtomic(CacheLinePaddedAtomic &&) = delete;

    /**
     * @brief Performs the Operator= operation for the event component.
     * @param CacheLinePaddedAtomic Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    CacheLinePaddedAtomic &operator=(CacheLinePaddedAtomic &&) = delete;
  };

} // namespace event
