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
 * @file sequence.cc
 * @brief Implements the Sequence logic for the event component.
 *
 * @details
 * This translation unit contains the out-of-line implementation for the surrounding feature. It complements the corresponding headers by providing concrete behavior, coordinating helper types, and keeping module-specific logic localized to one compilation unit.
 */

#include "policy/event/sequence.hpp"

#include <cstdint>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  Sequence::Sequence(const std::int64_t initial) noexcept : value(initial) {}

  std::int64_t Sequence::get(void) const noexcept
  {
    return value;
  }

  /**
   * @brief Performs the Set operation for the event component.
   * @param v Value forwarded to the Sequence::set operation.
   */
  void Sequence::set(const std::int64_t v) noexcept
  {
    value = v;
  }
} // namespace event
