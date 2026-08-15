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
 * @file sequence.hpp
 * @brief Declares the Sequence interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#include <cstdint>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @struct Sequence
   * @brief Represents the Sequence data structure used by the event component.
   */
  struct Sequence final
  {
    std::int64_t value;

    /**
     * @brief Performs the Sequence operation for the event component.
     * @param initial Value forwarded to the Sequence operation.
     */
    explicit Sequence(const std::int64_t initial = (-1)) noexcept;

    /**
     * @brief Performs the Get operation for the event component.
     * @return Result produced by this operation.
     */
    std::int64_t get(void) const noexcept;

    /**
     * @brief Performs the Set operation for the event component.
     * @param v Value forwarded to the set operation.
     */
    void set(const std::int64_t v) noexcept;

  }; // struct Sequence final

} // namespace event
