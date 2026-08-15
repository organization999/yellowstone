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
#pragma once

#include "event_bus.hpp"
#include "exchange.hpp"
#include "worker.hpp"
#include "execution_request.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

namespace execution { class AbstractExpression; }
namespace evm { struct FrontEndOptions; struct TieredExecutionOptions; namespace mir { struct InterpreterState; } }

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @file context.hpp
   * @brief Defines the top-level runtime context for the event package.
   *
   * @details
   * This integrated version keeps the newer exchange/event-bus/worker design and
   * removes the obsolete `IContext`, `AnyEvent`, and old context-local observer
   * dispatch model.
   */
  class Context final
  {
    /**
     * @enum StateType
     * @brief Represents the StateType set of named constants used by the event component.
     */
    enum class StateType
    {
      RUNNING,
      TERMINATED
    };

    /**
     * @class IState
     * @brief Represents the IState type used by the event component.
     */
    class IState
    {
    protected:
      /**
       * @brief Performs the IState operation for the event component.
       */
      IState() noexcept = default;

    public:
      /**
       * @brief Performs the ~IState operation for the event component.
       */
      virtual ~IState() noexcept = default;

      /**
       * @brief Performs the IState operation for the event component.
       * @param IState Value forwarded to the IState operation.
       */
      IState(const IState&)         = delete;
      /**
       * @brief Performs the Operator= operation for the event component.
       * @param IState Value forwarded to the operator= operation.
       */
      void operator=(const IState&) = delete;
      /**
       * @brief Performs the IState operation for the event component.
       * @param IState Value forwarded to the IState operation.
       */
      IState(IState&&)              = delete;
      /**
       * @brief Performs the Operator= operation for the event component.
       * @param IState Value forwarded to the operator= operation.
       */
      void operator=(IState&&)      = delete;

      /**
       * @brief Performs the Execute operation for the event component.
       */
      virtual void execute(void) noexcept = 0;
    };

    /**
     * @class RunningState
     * @brief Represents the RunningState type used by the event component.
     */
    class RunningState final : public IState
    {
      /**
       * @brief Performs the RunningState operation for the event component.
       */
      RunningState() noexcept = default;

    public:
      void execute(void) noexcept override {}

      static IState& create(void) noexcept
      {
        static RunningState instance;
        return instance;
      }
    };

    /**
     * @class TerminatedState
     * @brief Represents the TerminatedState type used by the event component.
     */
    class TerminatedState final : public IState
    {
      /**
       * @brief Performs the TerminatedState operation for the event component.
       */
      TerminatedState() noexcept = default;

    public:
      void execute(void) noexcept override {}

      static IState& create(void) noexcept
      {
        static TerminatedState instance;
        return instance;
      }
    };

    static constexpr std::size_t kMaxWorkers = 16UL;

    std::size_t                     m_num_workers{0UL};
    /**
     * @brief Performs the Create operation for the event component.
     * @return Result produced by this operation.
     */
    IState*                         m_state{&RunningState::create()};
    Exchange                        m_exchange{};
    std::array<Worker, kMaxWorkers> m_workers{};
    EventBus                        m_event_bus{&m_exchange};

    bool                            m_inline_execution{false};
    std::atomic<std::uint64_t>      m_rr_expression_worker{0ULL};

    /**
     * @brief Performs the M Change State operation for the event component.
     * @param next_state Value forwarded to the m_change_state operation.
     */
    void m_change_state(const StateType next_state) noexcept;

  public:
    /**
     * @brief Performs the Context operation for the event component.
     * @param num_workers Value forwarded to the Context operation.
     */
    explicit Context(const std::size_t num_workers, const bool inline_execution = false) noexcept;

    /**
     * @brief Performs the Context operation for the event component.
     * @param Context Value forwarded to the Context operation.
     */
    Context(const Context&)            = delete;
    /**
     * @brief Performs the Operator= operation for the event component.
     * @param Context Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    Context& operator=(const Context&) = delete;
    /**
     * @brief Performs the Context operation for the event component.
     * @param Context Value forwarded to the Context operation.
     */
    Context(Context&&)                 = delete;
    /**
     * @brief Performs the Operator= operation for the event component.
     * @param Context Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    Context& operator=(Context&&)      = delete;

    /**
     * @brief Performs the ~Context operation for the event component.
     */
    ~Context() noexcept = default;

    /**
     * @brief Performs the Run operation for the event component.
     */
    // void run(void) noexcept;

    template <typename T>
    /**
     * @brief Performs the Publish operation for the event component.
     * @param payload Value forwarded to the publish operation.
     */
    void publish(T&& payload) noexcept
    {
      /**
       * @brief Performs the Publish<T> operation for the event component.
       * @param payload Value forwarded to the publish<T> operation.
       */
      m_event_bus.publish<T>(static_cast<T&&>(payload));
    }

    void publish_event(const std::shared_ptr<AEvent>& event) noexcept
    {
      m_event_bus.publish_event(event);
    }

    void publish_one_event(const std::uint64_t subscriber_id,
                           const std::shared_ptr<AEvent>& event) noexcept
    {
      m_event_bus.publish_one_event(subscriber_id, event);
    }

    [[nodiscard]] float publish_expression(const std::shared_ptr<const execution::AbstractExpression>& expression, const bool memssa = false) noexcept;
    [[nodiscard]] std::uint32_t publish_word_expression(const std::shared_ptr<const execution::AbstractExpression>& expression,
                                                        evm::mir::InterpreterState& state,
                                                        evm::FrontEndOptions& frontend_opts,
                                                        evm::TieredExecutionOptions& tier_opts,
                                                        const bool prefer_cuda_backend = false) noexcept;
    [[nodiscard]] std::shared_ptr<WordEvalTicket> publish_word_expression_async(
      const std::shared_ptr<const execution::AbstractExpression>& expression,
      evm::mir::InterpreterState& state,
      evm::FrontEndOptions& frontend_opts,
      evm::TieredExecutionOptions& tier_opts,
      const bool prefer_cuda_backend = false) noexcept;

    template <typename E>
    requires std::derived_from<E, execution::AbstractExpression>
    [[nodiscard]] float publish(const std::shared_ptr<E>& expression) noexcept
    {
      return publish_expression(std::static_pointer_cast<const execution::AbstractExpression>(expression));
    }

    [[nodiscard]] float publish(const std::shared_ptr<const execution::AbstractExpression>& expression) noexcept
    {
      return publish_expression(expression, false);
    }

    /**
     * @brief Performs the Start operation for the event component.
     */
    void start(void) noexcept;

    /**
     * @brief Performs the Stop operation for the event component.
     */
    void stop(void) noexcept;

    /**
     * @brief Performs the Acquire operation for the event component.
     * @param worker_id Value forwarded to the acquire operation.
     * @param partition Value forwarded to the acquire operation.
     * @return Result produced by this operation.
     */
    [[nodiscard]] Lease acquire(const WorkerID worker_id,
                                Partition*&    partition) noexcept;

    /**
     * @brief Performs the Release operation for the event component.
     * @param worker_id Value forwarded to the release operation.
     * @param partition_id Value forwarded to the release operation.
     * @param gen Value forwarded to the release operation.
     * @return Result produced by this operation.
     */
    [[nodiscard]] int release(const WorkerID      worker_id,
                              const PartitionID   partition_id,
                              const std::uint32_t gen) noexcept;

  }; // class Context final

  class ContextGuard final
  {
  public:
    ContextGuard(const std::size_t num_workers, const bool inline_execution = false) noexcept;

    ~ContextGuard();

    /**
     * @brief Performs the Publish operation for the event component.
     * @param payload Value forwarded to the publish operation.
     */
    template <typename T>
    void publish(T&& payload) noexcept
    {
      m_context.publish<T>(std::forward<T>(payload));
    }

    void publish_event(const std::shared_ptr<AEvent>& event) noexcept
    {
      m_context.publish_event(event);
    }

    [[nodiscard]] float publish(const std::shared_ptr<const execution::AbstractExpression>& expression) noexcept
    {
      return m_context.publish_expression(expression, false);
    }

    [[nodiscard]] float publish_memssa(const std::shared_ptr<const execution::AbstractExpression>& expression) noexcept
    {
      return m_context.publish_expression(expression, true);
    }

    [[nodiscard]] std::uint32_t publish_word(const std::shared_ptr<const execution::AbstractExpression>& expression,
                                             evm::mir::InterpreterState& state,
                                             evm::FrontEndOptions& frontend_opts,
                                             evm::TieredExecutionOptions& tier_opts,
                                             const bool prefer_cuda_backend = false) noexcept
    {
      return m_context.publish_word_expression(expression, state, frontend_opts, tier_opts, prefer_cuda_backend);
    }

    [[nodiscard]] std::shared_ptr<WordEvalTicket> publish_word_async(
      const std::shared_ptr<const execution::AbstractExpression>& expression,
      evm::mir::InterpreterState& state,
      evm::FrontEndOptions& frontend_opts,
      evm::TieredExecutionOptions& tier_opts,
      const bool prefer_cuda_backend = false) noexcept
    {
      return m_context.publish_word_expression_async(expression, state, frontend_opts, tier_opts, prefer_cuda_backend);
    }

    template <typename E>
    requires std::derived_from<E, execution::AbstractExpression>
    [[nodiscard]] float publish(const std::shared_ptr<E>& expression) noexcept
    {
      return m_context.publish(expression);
    }

  private:
    Context m_context;

  }; // class ContextGuard final

  [[nodiscard]] inline std::size_t default_worker_count(void) noexcept
  {
    const unsigned int hc = std::thread::hardware_concurrency();
    const std::size_t v = hc == 0U ? 1UL : static_cast<std::size_t>(hc);
    return v <= 16UL ? v : 16UL;
  }

} // namespace event
