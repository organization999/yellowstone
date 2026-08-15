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
 * @file observer.cc
 * @brief Implements the Observer logic for the event component.
 *
 * @details
 * This translation unit contains the out-of-line implementation for the surrounding feature. It complements the corresponding headers by providing concrete behavior, coordinating helper types, and keeping module-specific logic localized to one compilation unit.
 */

#include "policy/event/observer.hpp"

#include <array>
#include <cassert>
#include <immintrin.h>
#include <thread>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  const std::array<AObserver::Factory, static_cast<std::size_t>(AObserver::StateType::COUNT)>
    AObserver::kFactories{
      &AObserver::WaitState::create,
      &AObserver::AcquireState::create,
      &AObserver::DrainState::create,
      &AObserver::ReleaseState::create,
      &AObserver::TeardownState::create,
      &AObserver::TerminatedState::create,
    };

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param observer Value forwarded to the AObserver::IsExchangeClosed::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsExchangeClosed::isSatisfiedBy(AObserver& observer) const noexcept
  {
    assert(observer.m_exchange != nullptr);
    return observer.m_exchange->is_closed(observer.m_partition_id, observer.m_generation);
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param AObserver Value forwarded to the AObserver::IsExchangeClosed::isSatisfiedBy operation.
   * @param IEvent Value forwarded to the AObserver::IsExchangeClosed::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsExchangeClosed::isSatisfiedBy(AObserver&, std::shared_ptr<AEvent>&) const noexcept
  {
    return false;
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param AObserver Value forwarded to the AObserver::IsExchangeClosed::isSatisfiedBy operation.
   * @param Notice Value forwarded to the AObserver::IsExchangeClosed::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsExchangeClosed::isSatisfiedBy(AObserver&, Notice&) const noexcept
  {
    return false;
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param AObserver Value forwarded to the AObserver::IsSuccessfulMailboxPop::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsSuccessfulMailboxPop::isSatisfiedBy(AObserver&) const noexcept
  {
    return false;
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param AObserver Value forwarded to the AObserver::IsSuccessfulMailboxPop::isSatisfiedBy operation.
   * @param IEvent Value forwarded to the AObserver::IsSuccessfulMailboxPop::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsSuccessfulMailboxPop::isSatisfiedBy(AObserver&, std::shared_ptr<AEvent>&) const noexcept
  {
    return false;
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param observer Value forwarded to the AObserver::IsSuccessfulMailboxPop::isSatisfiedBy operation.
   * @param notice Value forwarded to the AObserver::IsSuccessfulMailboxPop::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsSuccessfulMailboxPop::isSatisfiedBy(AObserver& observer, Notice& notice) const noexcept
  {
    return (observer.m_mailbox.pop(notice) == 0);
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param AObserver Value forwarded to the AObserver::IsPopPartition::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsPopPartition::isSatisfiedBy(AObserver&) const noexcept
  {
    return false;
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param observer Value forwarded to the AObserver::IsPopPartition::isSatisfiedBy operation.
   * @param ievent Value forwarded to the AObserver::IsPopPartition::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsPopPartition::isSatisfiedBy(AObserver& observer, std::shared_ptr<AEvent>& ievent) const noexcept
  {
    assert(observer.m_partition != nullptr);
    return (observer.m_partition->pop(ievent) == 0);
  }

  /**
   * @brief Performs the IsSatisfiedBy operation for the event component.
   * @param AObserver Value forwarded to the AObserver::IsPopPartition::isSatisfiedBy operation.
   * @param Notice Value forwarded to the AObserver::IsPopPartition::isSatisfiedBy operation.
   * @return Result produced by this operation.
   */
  bool AObserver::IsPopPartition::isSatisfiedBy(AObserver&, Notice&) const noexcept
  {
    return false;
  }

  /**
   * @brief Performs the Execute operation for the event component.
   * @param observer Value forwarded to the AObserver::WaitState::execute operation.
   */
  void AObserver::WaitState::execute(AObserver& observer) noexcept
  {
    for (;;)
    {
      /**
       * @brief Performs the Load operation for the event component.
       * @param memory_order_acquire Value forwarded to the load operation.
       * @return Result produced by this operation.
       */
      const std::uint32_t count = observer.m_mailbox_count.load(std::memory_order_acquire);

      if (observer.m_stop.load(std::memory_order_acquire) && count == 0U)
      {
        /**
         * @brief Performs the Change State operation for the event component.
         * @param TERMINATED Value forwarded to the change_state operation.
         */
        observer.change_state(StateType::TERMINATED);
        return;
      }

      if (count != 0U)
      {
        /**
         * @brief Performs the Change State operation for the event component.
         * @param ACQUIRE Value forwarded to the change_state operation.
         */
        observer.change_state(StateType::ACQUIRE);
        return;
      }

      while (observer.m_wake.load(std::memory_order_acquire) == 0U)
      {
        /**
         * @brief Performs the Wait operation for the event component.
         * @param U Value forwarded to the wait operation.
         * @param memory_order_acquire Value forwarded to the wait operation.
         */
        observer.m_wake.wait(0U, std::memory_order_acquire);
      }

      /**
       * @brief Performs the Store operation for the event component.
       * @param U Value forwarded to the store operation.
       * @param memory_order_release Value forwarded to the store operation.
       */
      observer.m_wake.store(0U, std::memory_order_release);
    }
  }

  /**
   * @brief Performs the Create operation for the event component.
   * @return Result produced by this operation.
   */
  AObserver::IState& AObserver::WaitState::create(void) noexcept
  {
    static WaitState instance;
    return instance;
  }

  /**
   * @brief Performs the Execute operation for the event component.
   * @param observer Value forwarded to the AObserver::AcquireState::execute operation.
   */
  void AObserver::AcquireState::execute(AObserver& observer) noexcept
  {
    Notice notice{};

    /**
     * @brief Performs the Pop operation for the event component.
     * @param notice Value forwarded to the pop operation.
     * @return Result produced by this operation.
     */
    const int rc = observer.m_mailbox.pop(notice);

    if (rc != 0)
    {
      /**
       * @brief Performs the Change State operation for the event component.
       * @param WAITING Value forwarded to the change_state operation.
       */
      observer.change_state(StateType::WAITING);
      return;
    }

    /**
     * @brief Performs the Fetch Sub operation for the event component.
     * @param U Value forwarded to the fetch_sub operation.
     * @param memory_order_acq_rel Value forwarded to the fetch_sub operation.
     */
    observer.m_mailbox_count.fetch_sub(1U, std::memory_order_acq_rel);

    observer.m_generation   = notice.gen;
    observer.m_worker_id    = notice.worker_id;
    observer.m_partition_id = notice.partition_id;
    observer.m_partition    = notice.partition;

    /**
     * @brief Performs the Change State operation for the event component.
     * @param DRAINING Value forwarded to the change_state operation.
     */
    observer.change_state(StateType::DRAINING);
  }

  /**
   * @brief Performs the Create operation for the event component.
   * @return Result produced by this operation.
   */
  AObserver::IState& AObserver::AcquireState::create(void) noexcept
  {
    static AcquireState instance;
    return instance;
  }

  /**
   * @brief Performs the Execute operation for the event component.
   * @param observer Value forwarded to the AObserver::DrainState::execute operation.
   */
  void AObserver::DrainState::execute(AObserver& observer) noexcept
  {
    IsPopPartition is_pop_partition;

    for (;;)
    {
      std::shared_ptr<AEvent> ievent{};

      // Graph-mode drain: enforce global superstep barrier across workers.
      if (observer.m_exchange != nullptr &&
          observer.m_exchange->graph_mode_enabled() &&
          observer.m_partition != nullptr &&
          observer.m_partition->has_graph_pending())
      {
        const std::uint64_t current = observer.m_exchange->graph_current_superstep();
        const int grc = observer.m_partition->pop_graph_if_superstep(current, ievent);

        if (grc == Partition::kOk)
        {
          if (ievent != nullptr)
          {
            (void)observer.dispatch(ievent);
          }
          continue;
        }

        if (grc == Partition::kFuture)
        {
          // No work for the current superstep in this partition, but future
          // superstep work exists. Close + release the lease so publishers can
          // enqueue more work, then participate in the global barrier.
          observer.m_exchange->close_lease(
            observer.m_worker_id,
            observer.m_partition_id,
            observer.m_generation
          );

          (void)observer.m_exchange->release(observer.m_worker_id,
                                             observer.m_partition_id,
                                             observer.m_generation);

          observer.m_partition    = nullptr;
          observer.m_partition_id = 0L;
          observer.m_generation   = 0U;
          observer.m_worker_id    = 0L;

          observer.m_exchange->graph_arrive_and_wait(current);

          // After barrier advance, return to WAITING so the worker can acquire
          // new notices.
          observer.change_state(StateType::WAITING);
          return;
        }
      }

      if (is_pop_partition.isSatisfiedBy(observer, ievent))
      {
        if (ievent != nullptr)
        {
          (void)observer.dispatch(ievent);
        }

        continue;
      }

      // No more events in the current partition. Close the lease so the exchange
      // can validate and reclaim it, then proceed to RELEASE.

      assert(observer.m_exchange != nullptr);

      observer.m_exchange->close_lease(
        observer.m_worker_id,
        observer.m_partition_id,
        observer.m_generation
      );

      // Immediately proceed to RELEASE in this same turn to minimize the window
      // where the lease is closed but not yet released.
      // Move directly to release to avoid any other queued states observing a
      // closed-but-not-released lease.
      // AObserver::ReleaseState::create().execute(observer);

      observer.change_state(StateType::RELEASE);
      return;
    }

  }

  /**
   * @brief Performs the Create operation for the event component.
   * @return Result produced by this operation.
   */
  AObserver::IState& AObserver::DrainState::create(void) noexcept
  {
    static DrainState instance;
    return instance;
  }

  /**
   * @brief Performs the Execute operation for the event component.
   * @param observer Value forwarded to the AObserver::ReleaseState::execute operation.
   */
  void AObserver::ReleaseState::execute(AObserver& observer) noexcept
  {
    assert(observer.m_exchange != nullptr);

    const int rc = observer.m_exchange->release(observer.m_worker_id,
                                                observer.m_partition_id,
                                                observer.m_generation);

    if (rc != 0 && rc != (-2))
    {
      // rc == -1 is common when a stale notice is processed after a lease has already
      // been released; this should not spam logs in normal operation.
      if (rc != (-1))
      {
        ts::println("release failed rc=", rc,
                    " wid=", observer.m_worker_id,
                    " pid=", observer.m_partition_id,
                    " gen=", observer.m_generation);
      }

      // In production, a release failure should not hard-crash the process.
      // It can occur if the lease was already reclaimed or the dispatcher entry
      // was cleared during shutdown/teardown. Fall through and reset local state.
    }

    observer.m_partition    = nullptr;
    observer.m_partition_id = 0L;
    observer.m_generation   = 0U;
    observer.m_worker_id    = 0L;

    /**
     * @brief Performs the Load operation for the event component.
     * @param memory_order_acquire Value forwarded to the load operation.
     * @return Result produced by this operation.
     */
    const std::uint32_t count = observer.m_mailbox_count.load(std::memory_order_acquire);

    if (count != 0U)
    {
      /**
       * @brief Performs the Change State operation for the event component.
       * @param ACQUIRE Value forwarded to the change_state operation.
       */
      observer.change_state(StateType::ACQUIRE);
      return;
    }

    if (observer.m_stop.load(std::memory_order_acquire))
    {
      /**
       * @brief Performs the Change State operation for the event component.
       * @param TERMINATED Value forwarded to the change_state operation.
       */
      observer.change_state(StateType::TERMINATED);
    }
    else
    {
      /**
       * @brief Performs the Change State operation for the event component.
       * @param WAITING Value forwarded to the change_state operation.
       */
      observer.change_state(StateType::WAITING);
    }
  }

  /**
   * @brief Performs the Create operation for the event component.
   * @return Result produced by this operation.
   */
  AObserver::IState& AObserver::ReleaseState::create(void) noexcept
  {
    static ReleaseState instance;
    return instance;
  }

  /**
   * @brief Performs the Execute operation for the event component.
   * @param observer Value forwarded to the AObserver::TeardownState::execute operation.
   */
  void AObserver::TeardownState::execute(AObserver& observer) noexcept
  {
    /**
     * @brief Performs the Change State operation for the event component.
     * @param WAITING Value forwarded to the change_state operation.
     */
    observer.change_state(StateType::WAITING);
  }

  /**
   * @brief Performs the Create operation for the event component.
   * @return Result produced by this operation.
   */
  AObserver::IState& AObserver::TeardownState::create(void) noexcept
  {
    static TeardownState instance;
    return instance;
  }

  /**
   * @brief Performs the Execute operation for the event component.
   * @param observer Value forwarded to the AObserver::TerminatedState::execute operation.
   */
  void AObserver::TerminatedState::execute(AObserver& observer) noexcept
  {
    /**
     * @brief Performs the Clear operation for the event component.
     */
    observer.m_states.clear();
  }

  /**
   * @brief Performs the Create operation for the event component.
   * @return Result produced by this operation.
   */
  AObserver::IState& AObserver::TerminatedState::create(void) noexcept
  {
    static TerminatedState instance;
    return instance;
  }

  /**
   * @brief Performs the M Run operation for the event component.
   */
  void AObserver::m_run(void) noexcept
  {
    while (m_states.empty() == false)
    {
      IState* state{nullptr};

      if (m_states.pop(state) != 0 || state == nullptr)
      {
        // In release builds `assert` is compiled out; keep the loop robust and avoid
        // undefined behavior if the state stack is concurrently mutated or corrupted.
        continue;
      }

      state->execute(*this);
    }
  }

  /**
   * @brief Performs the Force Terminate operation for the event component.
   */
  void AObserver::force_terminate(void) noexcept
  {
    /**
     * @brief Performs the Clear operation for the event component.
     */
    m_states.clear();

    /**
     * @brief Performs the Change State operation for the event component.
     * @param TERMINATED Value forwarded to the change_state operation.
     */
    change_state(StateType::TERMINATED);
  }

  /**
   * @brief Performs the AObserver operation for the event component.
   */
  AObserver::AObserver() noexcept
  {
    /**
     * @brief Performs the Change State operation for the event component.
     * @param WAITING Value forwarded to the change_state operation.
     */
    change_state(StateType::WAITING);
  }

  /**
   * @brief Performs the ~AObserver operation for the event component.
   */
  AObserver::~AObserver() noexcept
  {
    /**
     * @brief Performs the Stop operation for the event component.
     */
    stop();
  }

  /**
   * @brief Performs the Start operation for the event component.
   */
  void AObserver::start(void) noexcept
  {
    assert(false == m_thread.joinable());
    /**
     * @brief Performs the Store operation for the event component.
     * @param false Value forwarded to the store operation.
     * @param memory_order_release Value forwarded to the store operation.
     */
    m_stop.store(false, std::memory_order_release);
    m_thread = std::thread(&AObserver::m_run, this);
  }

  /**
   * @brief Performs the Stop operation for the event component.
   */
  void AObserver::stop(void) noexcept
  {
    /**
     * @brief Performs the Store operation for the event component.
     * @param true Value forwarded to the store operation.
     * @param memory_order_release Value forwarded to the store operation.
     */
    m_stop.store(true, std::memory_order_release);

    /**
     * @brief Performs the Store operation for the event component.
     * @param U Value forwarded to the store operation.
     * @param memory_order_release Value forwarded to the store operation.
     */
    m_wake.store(1U, std::memory_order_release);

    /**
     * @brief Performs the Notify One operation for the event component.
     */
    m_wake.notify_one();

    if (m_thread.joinable())
    {
      /**
       * @brief Performs the Join operation for the event component.
       */
      m_thread.join();
    }
  }

  void AObserver::notify(const WorkerID      worker_id,
                         const PartitionID   partition_id,
                         const std::uint32_t gen,
                         const Partition*    partition) noexcept
  {
    Notice n{
      worker_id,
      partition_id,
      gen,
      const_cast<Partition*>(partition)
    };

    while (m_mailbox.push(n) != 0)
    {
      /**
       * @brief Performs the Mm Pause operation for the event component.
       */
      _mm_pause();
    }

    /**
     * @brief Performs the Fetch Add operation for the event component.
     * @param U Value forwarded to the fetch_add operation.
     * @param memory_order_release Value forwarded to the fetch_add operation.
     */
    m_mailbox_count.fetch_add(1U, std::memory_order_release);

    /**
     * @brief Performs the Store operation for the event component.
     * @param U Value forwarded to the store operation.
     * @param memory_order_release Value forwarded to the store operation.
     */
    m_wake.store(1U, std::memory_order_release);

    /**
     * @brief Performs the Notify One operation for the event component.
     */
    m_wake.notify_one();
  }

  /**
   * @brief Performs the Change State operation for the event component.
   * @param next_state Value forwarded to the AObserver::change_state operation.
   */
  void AObserver::change_state(const StateType next_state) noexcept
  {
    /**
     * @brief Performs the To Index operation for the event component.
     * @param void Value forwarded to the to_index operation.
     * @return Result produced by this operation.
     */
    const std::size_t idx = to_index(next_state); (void)idx;
    assert(idx < kFactories.size());
    assert(m_states.push(&kFactories[idx]()) == 0);
  }

  /**
   * @brief Performs the Set Exchange operation for the event component.
   * @param exchange Value forwarded to the AObserver::set_exchange operation.
   */
  void AObserver::set_exchange(Exchange* exchange) noexcept
  {
    m_exchange = exchange;
  }

} // namespace event
