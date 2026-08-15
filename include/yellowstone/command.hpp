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
 * @file command.hpp
 * @brief Declares the Command interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#include <memory>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  struct AEvent;

  enum class CommandType { EXECUTION }; // enum class CommandType

  /**
   * @struct ICommand
   * @brief Represents the ICommand data structure used by the event component.
   */
  struct ICommand
  {
    /**
     * @brief Performs the ~ICommand operation for the event component.
     */
    virtual ~ICommand() noexcept = default;

    /**
     * @brief Performs the Execute operation for the event component.
     */
    virtual std::shared_ptr<AEvent> execute(const std::shared_ptr<AEvent>&) = 0;

  }; // struct ICommand

  struct CommandExecution final : public ICommand
  {
    std::shared_ptr<AEvent> execute(const std::shared_ptr<AEvent>& event) noexcept override;

  }; // struct CommandExecution final

} // namespace event
