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
 * @file state.cc
 * @brief Implements the State logic for the event component.
 *
 * @details
 * This translation unit contains the out-of-line implementation for the surrounding feature. It complements the corresponding headers by providing concrete behavior, coordinating helper types, and keeping module-specific logic localized to one compilation unit.
 */

#include "policy/event/state.hpp"

#include <ostream>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @brief Performs the To String operation for the event component.
   * @param s Value forwarded to the to_string operation.
   * @return Result produced by this operation.
   */
  const char *to_string(StateType s) noexcept;

  /**
   * @brief Performs the Operator<< operation for the event component.
   * @param os Value forwarded to the operator<< operation.
   * @param s Value forwarded to the operator<< operation.
   * @return Result produced by this operation.
   */
  std::ostream &operator<<(std::ostream &os, StateType s);

} // namespace event
