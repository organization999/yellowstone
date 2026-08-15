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
 * @file command.cc
 * @brief Implements the Command logic for the event component.
 *
 * @details
 * This translation unit contains the out-of-line implementation for the surrounding feature. It complements the corresponding headers by providing concrete behavior, coordinating helper types, and keeping module-specific logic localized to one compilation unit.
 */

#include "policy/event/command.hpp"
#include "policy/event/common.hpp"
#include "policy/event/event.hpp"

#include <iostream>
#include <thread>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  std::shared_ptr<AEvent> CommandExecution::execute(const std::shared_ptr<AEvent>& event) noexcept
  {
    (void)event;
    ts::println("[", std::this_thread::get_id(), "] CommandExecution");
    return nullptr;
  }

} // namespace event
