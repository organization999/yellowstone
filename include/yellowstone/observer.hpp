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

#include "collections/event_queue.hpp"

#include "common.hpp"
#include "event.hpp"
#include "exchange.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

/**
 * @file observer.hpp
 * @brief Defines the observer interface and the default exchange-driven observer.
 */

namespace event
{
  /**
   * @class IObserver
   * @brief Abstract observer interface for receiving partition notices.
   */
  class IObserver
  {
  protected:
    /**
     * @brief Performs the IObserver operation for the event component.
     */
    IObserver() noexcept = default;

  public:
    /**
     * @brief Performs the ~IObserver operation for the event component.
     */
    virtual ~IObserver() noexcept = default;

    /**
     * @brief Performs the IObserver operation for the event component.
     * @param IObserver Value forwarded to the IObserver operation.
     */
    IObserver(const IObserver&)            = delete;
    /**
     * @brief Performs the Operator= operation for the event component.
     * @param IObserver Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    IObserver& operator=(const IObserver&) = delete;
    /**
     * @brief Performs the IObserver operation for the event component.
     * @param IObserver Value forwarded to the IObserver operation.
     */
    IObserver(IObserver&&)                 = delete;
    /**
     * @brief Performs the Operator= operation for the event component.
     * @param IObserver Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    IObserver& operator=(IObserver&&)      = delete;

    virtual void notify(const WorkerID      worker_id,
                        const PartitionID   partition_id,
                        const std::uint32_t gen,
                        const Partition*    partition) noexcept = 0;

    /**
     * @brief Performs the Set Exchange operation for the event component.
     * @param exchange Value forwarded to the set_exchange operation.
     */
    virtual void set_exchange(Exchange* exchange) noexcept = 0;
  };

  /**
   * @class AObserver
   * @brief Default exchange-backed observer implementation.
   */
  class AObserver : public IObserver
  {
  protected:
    template <typename T>
    /**
     * @class Specification
     * @brief Represents the Specification type used by the event component.
     */
    class Specification
    {
    public:
      /**
       * @brief Performs the Specification operation for the event component.
       */
      Specification() noexcept = default;
      /**
       * @brief Performs the ~Specification operation for the event component.
       */
      virtual ~Specification() noexcept = default;

      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param T Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      virtual bool isSatisfiedBy(T&) const noexcept = 0;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param T Value forwarded to the isSatisfiedBy operation.
       * @param IEvent Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      virtual bool isSatisfiedBy(T&, std::shared_ptr<AEvent>&) const noexcept = 0;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param T Value forwarded to the isSatisfiedBy operation.
       * @param Notice Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      virtual bool isSatisfiedBy(T&, Notice&) const noexcept = 0;
    };

    /**
     * @class IsExchangeClosed
     * @brief Represents the IsExchangeClosed type used by the event component.
     */
    class IsExchangeClosed final : public Specification<AObserver>
    {
    public:
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param observer Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver& observer) const noexcept override;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param AObserver Value forwarded to the isSatisfiedBy operation.
       * @param IEvent Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver&, std::shared_ptr<AEvent>&) const noexcept override;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param AObserver Value forwarded to the isSatisfiedBy operation.
       * @param Notice Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver&, Notice&) const noexcept override;
    };

    /**
     * @class IsSuccessfulMailboxPop
     * @brief Represents the IsSuccessfulMailboxPop type used by the event component.
     */
    class IsSuccessfulMailboxPop final : public Specification<AObserver>
    {
    public:
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param AObserver Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver&) const noexcept override;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param AObserver Value forwarded to the isSatisfiedBy operation.
       * @param IEvent Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver&, std::shared_ptr<AEvent>&) const noexcept override;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param observer Value forwarded to the isSatisfiedBy operation.
       * @param notice Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver& observer, Notice& notice) const noexcept override;
    };

    /**
     * @class IsPopPartition
     * @brief Represents the IsPopPartition type used by the event component.
     */
    class IsPopPartition final : public Specification<AObserver>
    {
    public:
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param AObserver Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver&) const noexcept override;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param observer Value forwarded to the isSatisfiedBy operation.
       * @param ievent Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver& observer, std::shared_ptr<AEvent>& ievent) const noexcept override;
      /**
       * @brief Performs the IsSatisfiedBy operation for the event component.
       * @param AObserver Value forwarded to the isSatisfiedBy operation.
       * @param Notice Value forwarded to the isSatisfiedBy operation.
       * @return Result produced by this operation.
       */
      bool isSatisfiedBy(AObserver&, Notice&) const noexcept override;
    };

    /**
     * @enum StateType
     * @brief Represents the StateType set of named constants used by the event component.
     */
    enum class StateType : std::size_t
    {
      WAITING    = 0UL,
      ACQUIRE    = 1UL,
      DRAINING   = 2UL,
      RELEASE    = 3UL,
      TEARDOWN   = 4UL,
      TERMINATED = 5UL,
      COUNT,
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
      IState(const IState&)            = delete;
      /**
       * @brief Performs the Operator= operation for the event component.
       * @param IState Value forwarded to the operator= operation.
       * @return Result produced by this operation.
       */
      IState& operator=(const IState&) = delete;
      /**
       * @brief Performs the IState operation for the event component.
       * @param IState Value forwarded to the IState operation.
       */
      IState(IState&&)                 = delete;
      /**
       * @brief Performs the Operator= operation for the event component.
       * @param IState Value forwarded to the operator= operation.
       * @return Result produced by this operation.
       */
      IState& operator=(IState&&)      = delete;

      /**
       * @brief Performs the Execute operation for the event component.
       * @param observer Value forwarded to the execute operation.
       */
      virtual void execute(AObserver& observer) noexcept = 0;
    };

    /**
     * @class WaitState
     * @brief Represents the WaitState type used by the event component.
     */
    class WaitState final : public IState
    {
    public:
      /**
       * @brief Performs the Execute operation for the event component.
       * @param observer Value forwarded to the execute operation.
       */
      void execute(AObserver& observer) noexcept override;
      /**
       * @brief Performs the Create operation for the event component.
       * @return Result produced by this operation.
       */
      static IState& create(void) noexcept;
    };

    /**
     * @class AcquireState
     * @brief Represents the AcquireState type used by the event component.
     */
    class AcquireState final : public IState
    {
    public:
      /**
       * @brief Performs the Execute operation for the event component.
       * @param observer Value forwarded to the execute operation.
       */
      void execute(AObserver& observer) noexcept override;
      /**
       * @brief Performs the Create operation for the event component.
       * @return Result produced by this operation.
       */
      static IState& create(void) noexcept;
    };

    /**
     * @class DrainState
     * @brief Represents the DrainState type used by the event component.
     */
    class DrainState final : public IState
    {
    public:
      /**
       * @brief Performs the Execute operation for the event component.
       * @param observer Value forwarded to the execute operation.
       */
      void execute(AObserver& observer) noexcept override;
      /**
       * @brief Performs the Create operation for the event component.
       * @return Result produced by this operation.
       */
      static IState& create(void) noexcept;
    };

    /**
     * @class ReleaseState
     * @brief Represents the ReleaseState type used by the event component.
     */
    class ReleaseState final : public IState
    {
    public:
      /**
       * @brief Performs the Execute operation for the event component.
       * @param observer Value forwarded to the execute operation.
       */
      void execute(AObserver& observer) noexcept override;
      /**
       * @brief Performs the Create operation for the event component.
       * @return Result produced by this operation.
       */
      static IState& create(void) noexcept;
    };

    /**
     * @class TeardownState
     * @brief Represents the TeardownState type used by the event component.
     */
    class TeardownState final : public IState
    {
    public:
      /**
       * @brief Performs the Execute operation for the event component.
       * @param observer Value forwarded to the execute operation.
       */
      void execute(AObserver& observer) noexcept override;
      /**
       * @brief Performs the Create operation for the event component.
       * @return Result produced by this operation.
       */
      static IState& create(void) noexcept;
    };

    /**
     * @class TerminatedState
     * @brief Represents the TerminatedState type used by the event component.
     */
    class TerminatedState final : public IState
    {
    public:
      /**
       * @brief Performs the Execute operation for the event component.
       * @param observer Value forwarded to the execute operation.
       */
      void execute(AObserver& observer) noexcept override;
      /**
       * @brief Performs the Create operation for the event component.
       * @return Result produced by this operation.
       */
      static IState& create(void) noexcept;
    };

    static constexpr std::size_t kMaxStateBufferSize = 4UL;
    /**
     * @typedef StateBuffer
     * @brief Alias used by the event component to name the StateBuffer concept.
     */
    using StateBuffer = collections::Queue<IState*, kMaxStateBufferSize>;

    /**
     * @typedef Factory
     * @brief Alias used by the event component to name the Factory concept.
     */
    using Factory = IState& (*)(void) noexcept;

    /**
     * @brief Performs the Size T> operation for the event component.
     * @param COUNT Value forwarded to the static_cast<std::size_t> operation.
     * @return Result produced by this operation.
     */
    static const std::array<Factory, static_cast<std::size_t>(StateType::COUNT)> kFactories;

    std::thread                m_thread{};
    WorkerID                   m_worker_id{0L};
    PartitionID                m_partition_id{0L};
    Partition*                 m_partition{nullptr};
    StateBuffer                m_states{};
    std::uint32_t              m_generation{0U};
    std::atomic<std::uint32_t> m_mailbox_count{0U};
    std::atomic<bool>          m_stop{false};
    std::atomic<std::uint32_t> m_wake{0U};

    static constexpr std::size_t kMailboxSize = 64UL;
    collections::Queue<Notice, kMailboxSize>  m_mailbox{};

    Exchange* m_exchange{nullptr};

    /**
     * @brief Performs the To Index operation for the event component.
     * @param state_type Value forwarded to the to_index operation.
     * @return Result produced by this operation.
     */
    static constexpr std::size_t to_index(const StateType state_type) noexcept
    {
      return static_cast<std::size_t>(state_type);
    }

    /**
     * @brief Performs the M Run operation for the event component.
     */
    void m_run(void) noexcept;

    /**
     * @brief Performs the Force Terminate operation for the event component.
     */
    void force_terminate(void) noexcept;

  public:
    using EventPtr      = std::shared_ptr<AEvent>;
    using EventRef      = const std::shared_ptr<AEvent>&;

    using Controller    = std::function<EventPtr(EventRef)>;
    using ControllerRef = const Controller&;

    /**
     * @brief Performs the AObserver operation for the event component.
     */
    AObserver() noexcept;

    /**
     * @brief Performs the ~AObserver operation for the event component.
     */
    ~AObserver() noexcept override;

    /**
     * @brief Performs the AObserver operation for the event component.
     * @param AObserver Value forwarded to the AObserver operation.
     */
    AObserver(const AObserver&)            = delete;

    /**
     * @brief Performs the Operator= operation for the event component.
     * @param AObserver Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    AObserver& operator=(const AObserver&) = delete;

    /**
     * @brief Performs the AObserver operation for the event component.
     * @param AObserver Value forwarded to the AObserver operation.
     */
    AObserver(AObserver&&)                 = delete;

    /**
     * @brief Performs the Operator= operation for the event component.
     * @param AObserver Value forwarded to the operator= operation.
     * @return Result produced by this operation.
     */
    AObserver& operator=(AObserver&&)      = delete;

    /**
     * @brief Performs the Start operation for the event component.
     */
    void start(void) noexcept;

    /**
     * @brief Performs the Stop operation for the event component.
     */
    void stop(void) noexcept;

    virtual EventPtr dispatch(EventRef) = 0;

    virtual void handle(const EventScheduleType type, ControllerRef controller) = 0;

    void notify(const WorkerID      worker_id,
                const PartitionID   partition_id,
                const std::uint32_t gen,
                const Partition*    partition) noexcept override;

    /**
     * @brief Performs the Change State operation for the event component.
     * @param next_state Value forwarded to the change_state operation.
     */
    void change_state(const StateType next_state) noexcept;

    /**
     * @brief Performs the Set Exchange operation for the event component.
     * @param exchange Value forwarded to the set_exchange operation.
     */
    void set_exchange(Exchange* exchange) noexcept override;

  }; // class AObserver

} // namespace event
