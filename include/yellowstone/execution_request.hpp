/*
Copyright (C) 2026 Da'Jour J. Christophe. All rights reserved.

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

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

namespace execution { class AbstractExpression; }
namespace evm { struct FrontEndOptions; struct TieredExecutionOptions; namespace mir { struct InterpreterState; } }

namespace event
{
  /**
   * @brief Synchronization token used by expression execution events.
   *
   * @details
   * The event worker writes the result and signals completion; the publisher waits.
   */
  struct F32EvalTicket final
  {
    std::mutex              m{};
    std::condition_variable cv{};
    bool                    done{false};
    float                   result{0.0f};
  };

  /**
   * @brief Event payload that transports a canonical expression for f32 evaluation.
   *
   * @details
   * The expression is treated as an immutable composition; lowering + MMIO execution is performed
   * by the event worker prior to fulfilling the ticket.
   */
  struct F32EvalRequest final
  {
    std::shared_ptr<const execution::AbstractExpression> expression{};
    bool                                                memssa{false};
    bool                                                prefer_cuda_backend{false};
    std::uint32_t                                       adaptation_load{0U};
    // Async path uses shared_ptr; inline path may use `ticket_raw` to avoid heap allocations.
    std::shared_ptr<F32EvalTicket>                       ticket{};
    F32EvalTicket*                                       ticket_raw{nullptr};
  };

  struct WordEvalTicket final
  {
    std::mutex              m{};
    std::condition_variable cv{};
    bool                    done{false};
    std::uint32_t           result{0U};
  };

  struct WordEvalRequest final
  {
    std::shared_ptr<const execution::AbstractExpression> expression{};
    evm::mir::InterpreterState*                          state{nullptr};
    evm::FrontEndOptions*                                frontend_opts{nullptr};
    evm::TieredExecutionOptions*                         tier_opts{nullptr};
    bool                                                 prefer_cuda_backend{false};
    std::uint32_t                                       adaptation_load{0U};
    std::shared_ptr<WordEvalTicket>                      ticket{};
    WordEvalTicket*                                      ticket_raw{nullptr};
  };
} // namespace event
