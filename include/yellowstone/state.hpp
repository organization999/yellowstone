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
 * @file state.hpp
 * @brief Declares the State interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#include <ostream>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @enum StateType
   * @brief Represents the StateType set of named constants used by the event component.
   */
  enum class StateType { A, B };

  /**
   * @brief Performs the To String operation for the event component.
   * @param s Value forwarded to the to_string operation.
   * @return Result produced by this operation.
   */
  inline const char *to_string(StateType s) noexcept;

  /**
   * @brief Performs the Operator<< operation for the event component.
   * @param os Value forwarded to the operator<< operation.
   * @param s Value forwarded to the operator<< operation.
   * @return Result produced by this operation.
   */
  inline std::ostream &operator<<(std::ostream &os, StateType s);

} // namespace event
