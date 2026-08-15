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
 * @file event_extension.cc
 * @brief Implements Python bindings for the event component.
 *
 * @details
 * This source file bridges the native implementation into a Python-facing API surface. It centralizes binding definitions so ownership, lifetimes, and exported symbols stay aligned with the underlying C++ implementation.
 */

﻿#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "context.hpp"
#include "event.hpp"
#include "sequence.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>

/**
 * @namespace py
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace py = pybind11;

using namespace event;

namespace
{
  /**
   * @brief Performs the Callback From Address operation for the event component.
   * @param address Value forwarded to the callback_from_address operation.
   * @return Result produced by this operation.
   */
  [[nodiscard]] EventCallback callback_from_address(const std::uintptr_t address)
  {
    if (address == 0U)
    {
      /**
       * @brief Performs the Invalid Argument operation for the event component.
       * @param zero Value forwarded to the std::invalid_argument operation.
       * @return Result produced by this operation.
       */
      throw std::invalid_argument("callback address must be non-zero");
    }

    return reinterpret_cast<EventCallback>(address);
  }

} // namespace

namespace evm::pvm
{
  void init_event(py::module_& root)
  {
  /**
   * @brief Performs the Doc operation for the event component.
   */
    py::module_ m = root.def_submodule("event", "Python bindings for the event runtime");

  /*
   * Sequence
   */
  py::class_<Sequence>(m, "Sequence")
    .def(py::init<std::int64_t>(), py::arg("initial") = -1)
    .def("get", &Sequence::get)
    .def("set", &Sequence::set);

  /*
   * Base event interface
   */
  py::class_<IEvent, std::shared_ptr<IEvent>>(m, "IEvent")
    .def("execute", &IEvent::execute);

  /**
   * @brief Performs the Shared Ptr<AEvent>> operation for the event component.
   * @param m Value forwarded to the std::shared_ptr<AEvent>> operation.
   * @param AEvent Value forwarded to the std::shared_ptr<AEvent>> operation.
   * @return Result produced by this operation.
   */
  py::class_<AEvent, IEvent, std::shared_ptr<AEvent>>(m, "AEvent");

  /*
   * ImmediateEvent
   *
   * We intentionally bind callback addresses as uintptr_t instead of binding
   * EventCallback (a raw C function pointer type) directly. That avoids the
   * MSVC/pybind11 function-type caster warning.
   */
  py::class_<ImmediateEvent, AEvent, std::shared_ptr<ImmediateEvent>>(m, "ImmediateEvent")
    .def(
      py::init([](const EventPriority priority, const std::uintptr_t callback_address)
      {
        return std::make_shared<ImmediateEvent>(
          priority,
          callback_from_address(callback_address)
        );
      }),
      py::arg("priority"),
      py::arg("callback_address")
    );

  /*
   * TimestampEvent
   */
  py::class_<TimestampEvent, AEvent, std::shared_ptr<TimestampEvent>>(m, "TimestampEvent")
    .def(
      py::init([](const EventPriority  priority,
                  const std::uintptr_t callback_address,
                  const EventTimestamp timestamp)
      {
        return std::make_shared<TimestampEvent>(
          priority,
          callback_from_address(callback_address),
          timestamp
        );
      }),
      py::arg("priority"),
      py::arg("callback_address"),
      py::arg("timestamp")
    );

  /*
   * Context runtime
   */
  py::class_<Context>(m, "Context")
    .def(py::init<std::size_t>())
    .def("start", &Context::start)
    .def("stop", &Context::stop)
    .def("run", &Context::run)
    .def(
      "publish_int",
      [](Context& ctx, int value)
      {
        ctx.publish(std::move(value));
      }
    );

  /*
   * Worker
   */
  py::class_<Worker>(m, "Worker");
  }
}
