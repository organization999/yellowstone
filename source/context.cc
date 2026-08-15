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
 * @file context.cc
 * @brief Implements the Context logic for the event component.
 *
 * @details
 * This translation unit contains the out-of-line implementation for the surrounding feature. It complements the corresponding headers by providing concrete behavior, coordinating helper types, and keeping module-specific logic localized to one compilation unit.
 */

#include "policy/event/command.hpp"
#include "policy/event/context.hpp"

#include "expression/operators/execution_target_hint.hpp"
#include "policy/event/execution_request.hpp"

#include "adaptation/bookkeeper.hpp"

#include "execution/float_mmio_pipeline.hpp"
#include "execution/tensor_runtime.hpp"
#include "execution/tiered_execution.hpp"
#include "execution/host_runtime.hpp"

#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  namespace
  {
    [[nodiscard]] std::uint32_t fnv1a32(const char* s) noexcept
    {
      std::uint32_t h = 2166136261u;
      if (s == nullptr) return h;
      for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p != 0; ++p)
      {
        h ^= static_cast<std::uint32_t>(*p);
        h *= 16777619u;
      }
      return h;
    }
    [[maybe_unused]] [[nodiscard]] execution::ExecutionTargetHint extract_target_hint(
      const std::shared_ptr<const execution::AbstractExpression>& expr) noexcept
    {
      if (!expr) return execution::ExecutionTargetHint::CPU;
      if (const auto* hint = dynamic_cast<const execution::ExecutionTargetHintExpression*>(expr.get()))
      {
        return hint->target();
      }
      return execution::ExecutionTargetHint::CPU;
    }

    [[nodiscard]] std::uint32_t estimate_load(
      const std::shared_ptr<const execution::AbstractExpression>& expr) noexcept
    {
      if (!expr) return 1U;
      const std::size_t n = expr->get_nodes().size();
      const std::size_t load = (n == 0U ? 1U : n);
      return static_cast<std::uint32_t>((load > 0xFFFFFFFFull) ? 0xFFFFFFFFull : load);
    }
  }

  /**
   * @brief Performs the M Change State operation for the event component.
   * @param next_state Value forwarded to the Context::m_change_state operation.
   */
  void Context::m_change_state(const StateType next_state) noexcept
  {
    switch (next_state)
    {
      case StateType::RUNNING:
        m_state = &RunningState::create();
        break;

      case StateType::TERMINATED:
        m_state = &TerminatedState::create();
        break;

      default:
        break;
    }
  }

  /**
   * @brief Performs the Context operation for the event component.
   * @param num_workers Value forwarded to the Context::Context operation.
   */
  Context::Context(const std::size_t num_workers, const bool inline_execution) noexcept
    : m_num_workers(num_workers <= kMaxWorkers ? num_workers : kMaxWorkers),
      m_inline_execution(inline_execution)
  {
    m_exchange.set_graph_barrier_parties(m_num_workers);

    for (std::uint64_t worker_id = 0UL; worker_id < m_num_workers; ++worker_id)
    {
      /**
       * @brief Performs the Subscribe operation for the event component.
       * @param worker_id Value forwarded to the subscribe operation.
       */
      m_event_bus.subscribe(&m_workers[worker_id]);
    }
  }

  /**
   * @brief Performs the Start operation for the event component.
   */
  void Context::start(void) noexcept
  {
    for (std::uint64_t worker_id = 0UL; worker_id < m_num_workers; ++worker_id)
    {
      Worker& worker = m_workers[worker_id];

      worker.handle(EventScheduleType::IMMEDIATE, [](const std::shared_ptr<AEvent>& event) -> std::shared_ptr<AEvent>
      {
        if (event == nullptr)
        {
          return nullptr;
        }

        if (auto expr_event = std::dynamic_pointer_cast<Event<F32EvalRequest, 16>>(event))
        {
          F32EvalRequest& req = expr_event->payload;

          F32EvalTicket* ticket = req.ticket_raw ? req.ticket_raw : req.ticket.get();
          if (ticket && req.expression)
          {
            // Scalar f32 is CPU-only today; intent is carried for uniformity.
            (void)req.prefer_cuda_backend;
            const float v = req.memssa
              ? evm::eval_f32_expression_memssa_cached_direct(req.expression)
              : evm::eval_f32_expression_cached_direct(req.expression);

            // Inline execution never waits; still set the ticket fields for correctness.
            {
              std::lock_guard<std::mutex> lk(ticket->m);
              ticket->result = v;
              ticket->done = true;
            }
            ticket->cv.notify_all();
          }

          if (req.adaptation_load != 0U)
          {
            (void)adaptation::global_bookkeeper().pop_cpu(req.adaptation_load);
          }

          // Consumed.
          return nullptr;
        }

        if (auto expr_event = std::dynamic_pointer_cast<Event<WordEvalRequest, 16>>(event))
        {
          WordEvalRequest& req = expr_event->payload;
          WordEvalTicket* ticket = req.ticket_raw ? req.ticket_raw : req.ticket.get();
          if (ticket && req.expression && req.state != nullptr)
          {
            evm::tensor::table(*req.state).backend = req.prefer_cuda_backend ? evm::tensor::Backend::CUDA : evm::tensor::Backend::CPU;

            evm::FrontEndOptions fe{};
            evm::TieredExecutionOptions te{};
            if (req.frontend_opts != nullptr) fe = *req.frontend_opts;
            if (req.tier_opts != nullptr) te = *req.tier_opts;
            fe.execution_target = req.prefer_cuda_backend ? evm::ExecutionTarget::CUDA : evm::ExecutionTarget::CPU;

            const std::uint32_t v = evm::eval_expression_tiered_cached_direct(req.expression, *req.state, fe, te);

            // Pipeline feedback: if the host runtime has a pending exception, surface it
            // via the per-state trace buffer so higher layers can treat it as a reward
            // signal without adding new synchronous APIs.
            //
            // Convention: push 0 on success, otherwise push exception id.
            const std::uint32_t limit = req.state->trace_u32_limit;
            if (limit != 0U)
            {
              const int pending = evm::host_runtime::exception_pending();
              const std::uint32_t code = pending != 0 ? static_cast<std::uint32_t>(evm::host_runtime::consume_exception_id()) : 0U;
              if (req.state->trace_u32.size() >= static_cast<std::size_t>(limit))
              {
                const std::size_t overflow =
                  (req.state->trace_u32.size() + 1U) - static_cast<std::size_t>(limit);
                req.state->trace_u32.erase(req.state->trace_u32.begin(),
                                          req.state->trace_u32.begin() + static_cast<std::ptrdiff_t>(overflow));
              }
              req.state->trace_u32.push_back(code);

              // Native feedback handler (v1): apply a small online update to the byte-head bias
              // based on execution success/failure. This enables "agent does not wait for
              // execution completion" pipelines where the worker can update the model from
              // teardown+pipeline signals without round-tripping to Python.
              //
              // Convention from TransformerBytecodePolicy:
              // - Teardown pushes `chosen_byte` (u32, 0..255) before execution completes.
              // - The worker appends `code` (0=success else exception id).
              if (req.state->trace_u32.size() >= 2U)
              {
                const std::size_t n = req.state->trace_u32.size();
                const std::uint32_t chosen = req.state->trace_u32[n - 2U] & 0xFFu;
                const std::uint32_t status = req.state->trace_u32[n - 1U];
                static const std::uint32_t slot_b = fnv1a32("agent.byte_head_b");
                std::vector<std::uint32_t> dims{256u};
                // Fetch current bias, update one entry, and write back. (Bounded, deterministic.)
                std::vector<float> data{};
                std::vector<std::uint32_t> got_dims{};
                const std::uint32_t h = evm::tensor::var_get_or_create_f32(*req.state, slot_b, dims);
                const bool ok = evm::tensor::to_host_f32(*req.state, h, data, got_dims);
                if (ok && got_dims.size() == 1U && got_dims[0] == 256U && data.size() == 256U)
                {
                  // Only penalize failures here. Rewarding "success" collapses the
                  // policy toward whatever byte happened to execute recently, which
                  // harms exploration/learning. The RL signal should come from the
                  // environment reward; this hook is purely a validity guardrail.
                  if (status != 0U)
                  {
                    const float lr = 0.05f;
                    data[chosen] -= lr;
                  }
                  (void)evm::tensor::var_set_f32(*req.state, slot_b, dims, data);
                }
              }
            }

            {
              std::lock_guard<std::mutex> lk(ticket->m);
              ticket->result = v;
              ticket->done = true;
            }
            ticket->cv.notify_all();
          }

          if (req.adaptation_load != 0U)
          {
            if (req.prefer_cuda_backend)
              (void)adaptation::global_bookkeeper().pop_gpu(req.adaptation_load);
            else
              (void)adaptation::global_bookkeeper().pop_cpu(req.adaptation_load);
          }
          return nullptr;
        }

        // NOTE: DO NOT use a constant here!
        auto concrete = std::dynamic_pointer_cast<Event<CommandType, 16>>(event);
        if (!concrete)
        {
          return event;
        }

        switch (concrete->payload)
        {
          case CommandType::EXECUTION:
          {
            CommandExecution command;
            return command.execute(concrete);
          }

          default: break;
        }

        return event;
      });

      worker.handle(EventScheduleType::TIMESTAMP, [](const std::shared_ptr<AEvent>& event) -> std::shared_ptr<AEvent>
      {
        (void)event;
        ts::println("EventType is timed");
        return nullptr;
      });

      worker.handle(EventScheduleType::UNSUPPORTED, [](const std::shared_ptr<AEvent>& event) -> std::shared_ptr<AEvent>
      {
        (void)event;
        ts::println("EventType is not supported");
        return nullptr;
      });

      /**
       * @brief Performs the Start operation for the event component.
       */
      if (!m_inline_execution)
      {
        worker.start();
      }
    }
  }

  float Context::publish_expression(const std::shared_ptr<const execution::AbstractExpression>& expression, const bool memssa) noexcept
  {
    const std::uint32_t load = estimate_load(expression);
    const int device = adaptation::global_bookkeeper().push(load);
    const bool prefer_cuda_backend = (device == 1);

    if (m_inline_execution)
    {
      F32EvalRequest req{};
      req.expression = expression;
      req.memssa = memssa;
      req.prefer_cuda_backend = prefer_cuda_backend;
      req.adaptation_load = load;

      // Ultra-hot synchronous path: still uses the event envelope/request shape, but avoids
      // shared_ptr churn + RTTI dispatch overhead in Worker::Router. This is required for
      // per-scalar eval loops (benchmarks/training).
      const float v = req.memssa
        ? evm::eval_f32_expression_memssa_cached_direct(req.expression)
        : evm::eval_f32_expression_cached_direct(req.expression);

      (void)adaptation::global_bookkeeper().pop_cpu(load);
      return v;
    }

    auto ticket = std::make_shared<F32EvalTicket>();
    F32EvalRequest req{};
    req.expression = expression;
    req.memssa = memssa;
    req.prefer_cuda_backend = prefer_cuda_backend;
    req.adaptation_load = load;
    req.ticket = ticket;

    // Route to one worker for request/response semantics (EventBus publish() is broadcast).
    const std::uint64_t id = m_num_workers == 0UL
      ? 0UL
      : (m_rr_expression_worker.fetch_add(1ULL, std::memory_order_relaxed) % m_num_workers);

    m_event_bus.publish_one(id, std::move(req));

    std::unique_lock<std::mutex> lk(ticket->m);
    ticket->cv.wait(lk, [&] { return ticket->done; });
    return ticket->result;
  }

  std::uint32_t Context::publish_word_expression(const std::shared_ptr<const execution::AbstractExpression>& expression,
                                                 evm::mir::InterpreterState& state,
                                                 evm::FrontEndOptions& frontend_opts,
                                                 evm::TieredExecutionOptions& tier_opts,
                                                 const bool prefer_cuda_backend) noexcept
  {
    (void)prefer_cuda_backend;
    const std::uint32_t load = estimate_load(expression);
    const int device = adaptation::global_bookkeeper().push(load);
    const bool prefer_cuda_backend_derived = (device == 1);

    if (m_inline_execution)
    {
      WordEvalRequest req{};
      req.expression = expression;
      req.state = &state;
      req.frontend_opts = &frontend_opts;
      req.tier_opts = &tier_opts;
      req.prefer_cuda_backend = prefer_cuda_backend_derived;
      req.adaptation_load = load;

      // Ultra-hot synchronous path: avoid Worker router RTTI/alloc overhead; execute the request directly.
      // Keep semantics identical to the worker handler.
      evm::tensor::table(state).backend = req.prefer_cuda_backend ? evm::tensor::Backend::CUDA : evm::tensor::Backend::CPU;

      evm::FrontEndOptions fe = frontend_opts;
      fe.execution_target = req.prefer_cuda_backend ? evm::ExecutionTarget::CUDA : evm::ExecutionTarget::CPU;

      evm::TieredExecutionOptions te = tier_opts;
      const std::uint32_t out = evm::eval_expression_tiered_cached_direct(req.expression, state, fe, te);
      if (req.prefer_cuda_backend)
        (void)adaptation::global_bookkeeper().pop_gpu(load);
      else
        (void)adaptation::global_bookkeeper().pop_cpu(load);
      return out;
    }

    auto ticket = std::make_shared<WordEvalTicket>();
    WordEvalRequest req{};
    req.expression = expression;
    req.state = &state;
    req.frontend_opts = &frontend_opts;
    req.tier_opts = &tier_opts;
    req.prefer_cuda_backend = prefer_cuda_backend_derived;
    req.adaptation_load = load;
    req.ticket = ticket;

    const std::uint64_t id = m_num_workers == 0UL
      ? 0UL
      : (m_rr_expression_worker.fetch_add(1ULL, std::memory_order_relaxed) % m_num_workers);
    m_event_bus.publish_one(id, std::move(req));

    std::unique_lock<std::mutex> lk(ticket->m);
    ticket->cv.wait(lk, [&] { return ticket->done; });
    return ticket->result;
  }

  std::shared_ptr<WordEvalTicket> Context::publish_word_expression_async(
    const std::shared_ptr<const execution::AbstractExpression>& expression,
    evm::mir::InterpreterState& state,
    evm::FrontEndOptions& frontend_opts,
    evm::TieredExecutionOptions& tier_opts,
    const bool prefer_cuda_backend) noexcept
  {
    (void)prefer_cuda_backend;
    const std::uint32_t load = estimate_load(expression);
    const int device = adaptation::global_bookkeeper().push(load);
    const bool prefer_cuda_backend_derived = (device == 1);

    auto ticket = std::make_shared<WordEvalTicket>();

    if (m_inline_execution)
    {
      WordEvalRequest req{};
      req.expression = expression;
      req.state = &state;
      req.frontend_opts = &frontend_opts;
      req.tier_opts = &tier_opts;
      req.prefer_cuda_backend = prefer_cuda_backend_derived;
      req.adaptation_load = load;
      req.ticket = ticket;

      // Execute immediately (still writes ticket + trace feedback).
      evm::tensor::table(state).backend = req.prefer_cuda_backend ? evm::tensor::Backend::CUDA : evm::tensor::Backend::CPU;

      evm::FrontEndOptions fe = frontend_opts;
      fe.execution_target = req.prefer_cuda_backend ? evm::ExecutionTarget::CUDA : evm::ExecutionTarget::CPU;
      evm::TieredExecutionOptions te = tier_opts;
      const std::uint32_t out = evm::eval_expression_tiered_cached_direct(req.expression, state, fe, te);

      // Mirror worker handler pipeline feedback.
      const std::uint32_t limit = state.trace_u32_limit;
      if (limit != 0U)
      {
        const int pending = evm::host_runtime::exception_pending();
        const std::uint32_t code = pending != 0 ? static_cast<std::uint32_t>(evm::host_runtime::consume_exception_id()) : 0U;
        if (state.trace_u32.size() >= static_cast<std::size_t>(limit))
        {
          const std::size_t overflow = (state.trace_u32.size() + 1U) - static_cast<std::size_t>(limit);
          state.trace_u32.erase(state.trace_u32.begin(),
                                state.trace_u32.begin() + static_cast<std::ptrdiff_t>(overflow));
        }
        state.trace_u32.push_back(code);
      }

      {
        std::lock_guard<std::mutex> lk(ticket->m);
        ticket->result = out;
        ticket->done = true;
      }
      ticket->cv.notify_all();

      if (req.prefer_cuda_backend)
        (void)adaptation::global_bookkeeper().pop_gpu(load);
      else
        (void)adaptation::global_bookkeeper().pop_cpu(load);
      return ticket;
    }

    WordEvalRequest req{};
    req.expression = expression;
    req.state = &state;
    req.frontend_opts = &frontend_opts;
    req.tier_opts = &tier_opts;
    req.prefer_cuda_backend = prefer_cuda_backend_derived;
    req.adaptation_load = load;
    req.ticket = ticket;

    const std::uint64_t id = m_num_workers == 0UL
      ? 0UL
      : (m_rr_expression_worker.fetch_add(1ULL, std::memory_order_relaxed) % m_num_workers);
    m_event_bus.publish_one(id, std::move(req));
    return ticket;
  }

  /**
   * @brief Performs the Stop operation for the event component.
   */
  void Context::stop(void) noexcept
  {
    /**
     * @brief Performs the M Change State operation for the event component.
     * @param TERMINATED Value forwarded to the m_change_state operation.
     */
    m_change_state(StateType::TERMINATED);

    for (std::uint64_t worker_id = 0UL; worker_id < m_num_workers; ++worker_id)
    {
      Worker& worker = m_workers[worker_id];
      /**
       * @brief Performs the Stop operation for the event component.
       */
      worker.stop();
    }
  }

  /**
   * @brief Performs the Acquire operation for the event component.
   * @param worker_id Value forwarded to the Context::acquire operation.
   * @param partition Value forwarded to the Context::acquire operation.
   * @return Result produced by this operation.
   */
  Lease Context::acquire(const WorkerID worker_id, Partition*& partition) noexcept
  {
    return m_exchange.acquire(worker_id, partition);
  }

  int Context::release(const WorkerID      worker_id,
                       const PartitionID   partition_id,
                       const std::uint32_t gen) noexcept
  {
    return m_exchange.release(worker_id, partition_id, gen);
  }

  ContextGuard::ContextGuard(const std::size_t num_workers, const bool inline_execution) noexcept
    : m_context(num_workers, inline_execution)
  {
    m_context.start();
  }

  ContextGuard::~ContextGuard()
  {
    m_context.stop();
  }

} // namespace event
