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
 * @file exchange.cc
 * @brief Implements the Exchange logic for the event component.
 *
 * @details
 * This translation unit contains the out-of-line implementation for the surrounding feature. It complements the corresponding headers by providing concrete behavior, coordinating helper types, and keeping module-specific logic localized to one compilation unit.
 */

#include "policy/event/exchange.hpp"

#include <condition_variable>
#include <mutex>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @brief Performs the Try Get operation for the event component.
   * @param worker_id Value forwarded to the Exchange::Dispatcher::try_get operation.
   * @param out Value forwarded to the Exchange::Dispatcher::try_get operation.
   * @return Result produced by this operation.
   */
  bool Exchange::Dispatcher::try_get(const WorkerID worker_id, Entry& out) noexcept
  {
    /**
     * @brief Performs the Lock operation for the event component.
     * @param m_mutex Value forwarded to the lock operation.
     * @return Result produced by this operation.
     */
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_registry.find(worker_id);
    if (it == m_registry.end())
    {
      return false;
    }

    out = it->val;
    return true;
  }

  /**
   * @brief Performs the Set Open operation for the event component.
   * @param worker_id Value forwarded to the Exchange::Dispatcher::set_open operation.
   * @param l Value forwarded to the Exchange::Dispatcher::set_open operation.
   * @return Result produced by this operation.
   */
  int Exchange::Dispatcher::set_open(const WorkerID worker_id, const Lease l) noexcept
  {
    /**
     * @brief Performs the Lock operation for the event component.
     * @param m_mutex Value forwarded to the lock operation.
     * @return Result produced by this operation.
     */
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_registry.find(worker_id) != m_registry.end())
    {
      return (-1);
    }

    /**
     * @brief Performs the Emplace operation for the event component.
     * @param worker_id Value forwarded to the emplace operation.
     * @param l Value forwarded to the emplace operation.
     * @return Result produced by this operation.
     */
    const bool ok = m_registry.emplace(worker_id, Entry{l});
    return ok ? 0 : (-4);
  }

  /**
   * @brief Performs the Erase Open operation for the event component.
   * @param worker_id Value forwarded to the Exchange::Dispatcher::erase_open operation.
   * @param out Value forwarded to the Exchange::Dispatcher::erase_open operation.
   * @return Result produced by this operation.
   */
  int Exchange::Dispatcher::erase_open(const WorkerID worker_id, Lease& out) noexcept
  {
    /**
     * @brief Performs the Lock operation for the event component.
     * @param m_mutex Value forwarded to the lock operation.
     * @return Result produced by this operation.
     */
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_registry.find(worker_id);
    if (it == m_registry.end())
    {
      return (-1);
    }

    out = it->val.lease;

    /**
     * @brief Performs the Erase operation for the event component.
     * @param it Value forwarded to the erase operation.
     * @return Result produced by this operation.
     */
    const bool ok = m_registry.erase(it);
    return ok ? 0 : (-4);
  }

  /**
   * @brief Performs the Exchange operation for the event component.
   */
  Exchange::Exchange() noexcept
  {
    for (PartitionID partition_id = 0L;
        static_cast<std::uint64_t>(partition_id) < kMaxNumPartitions;
        partition_id++)
    {
      assert(m_ready.push(partition_id) == 0);
    }
  }

  bool Exchange::is_closed(const PartitionID pid,
                          const std::uint32_t gen) const noexcept
  {
    return m_ctrl[static_cast<std::size_t>(pid)]
            .closed_gen.load(std::memory_order_acquire) == gen;
  }

  void Exchange::close_lease(const WorkerID /*worker_id*/,
                            const PartitionID pid,
                            const std::uint32_t gen) noexcept
  {
    /**
     * @brief Performs the Lock operation for the event component.
     * @param m_mutex Value forwarded to the lock operation.
     * @return Result produced by this operation.
     */
    std::lock_guard<std::mutex> lock(m_mutex);

    m_ctrl[static_cast<std::size_t>(pid)]
      .closed_gen.store(gen, std::memory_order_release);
  }

  /**
   * @brief Performs the Partition Mutex operation for the event component.
   * @param id Value forwarded to the Exchange::partition_mutex operation.
   * @return Result produced by this operation.
   */
  std::mutex& Exchange::partition_mutex(PartitionID id) noexcept
  {
    return m_part_mutex[static_cast<std::size_t>(id)];
  }

  Lease Exchange::acquire(const WorkerID worker_id,
                          Partition*& partition) noexcept
  {
    /**
     * @brief Performs the Lock operation for the event component.
     * @param m_mutex Value forwarded to the lock operation.
     * @return Result produced by this operation.
     */
    std::lock_guard<std::mutex> lock(m_mutex);

    Dispatcher::Entry e{};

    if (m_dispatcher.try_get(worker_id, e))
    {
      partition = nullptr;
      return Lease{-1, 0};
    }

    PartitionID pid{};

    if (m_ready.pop(pid) ==
        collections::Queue<PartitionID, (2UL * kMaxNumPartitions)>::kEmpty)
    {
      partition = nullptr;
      return Lease{-1, 0};
    }

    /**
     * @brief Performs the Size T> operation for the event component.
     * @param pid Value forwarded to the static_cast<std::size_t> operation.
     * @return Result produced by this operation.
     */
    auto& c = m_ctrl[static_cast<std::size_t>(pid)];

    const std::uint32_t gen =
      /**
       * @brief Performs the Fetch Add operation for the event component.
       * @param memory_order_relaxed Value forwarded to the fetch_add operation.
       */
      c.gen.fetch_add(1, std::memory_order_relaxed) + 1;

    /**
     * @brief Performs the Store operation for the event component.
     * @param memory_order_relaxed Value forwarded to the store operation.
     */
    c.closed_gen.store(0, std::memory_order_relaxed);

    const Lease l{pid, gen};

    assert(m_dispatcher.set_open(worker_id, l) == 0);

    partition = &m_partitions[static_cast<std::size_t>(pid)];

    return l;
  }

  int Exchange::release(const WorkerID worker_id,
                        const PartitionID pid,
                        const std::uint32_t gen) noexcept
  {
    /**
     * @brief Performs the Lock operation for the event component.
     * @param m_mutex Value forwarded to the lock operation.
     * @return Result produced by this operation.
     */
    std::lock_guard<std::mutex> lock(m_mutex);

    Dispatcher::Entry e{};

    if (!m_dispatcher.try_get(worker_id, e))
    {
      return (-1);
    }

    if (e.lease.pid != pid || e.lease.gen != gen)
    {
      // If the caller's view is stale but the currently-registered lease for this
      // worker is already closed, prefer releasing the registered lease rather
      // than failing hard. This makes shutdown paths more robust in the face of
      // delayed notices.
      if (!is_closed(e.lease.pid, e.lease.gen))
      {
        return (-2);
      }

      // Fall through and release the registered lease.
    }

    const PartitionID   effective_pid = e.lease.pid;
    const std::uint32_t effective_gen = e.lease.gen;

    if (!is_closed(effective_pid, effective_gen))
    {
      return (-3);
    }

    Lease erased{};

    /**
     * @brief Performs the Erase Open operation for the event component.
     * @param worker_id Value forwarded to the erase_open operation.
     * @param erased Value forwarded to the erase_open operation.
     * @return Result produced by this operation.
     */
    const int rc = m_dispatcher.erase_open(worker_id, erased);
    if (rc != 0)
    {
      return rc;
    }

    assert(erased.pid == effective_pid && erased.gen == effective_gen);

    assert(m_ready.push(effective_pid) == 0);

    return 0;
  }

  void Exchange::graph_arrive_and_wait(const std::uint64_t observed_superstep) noexcept
  {
    if (!graph_mode_enabled())
    {
      return;
    }

    const std::uint32_t parties = m_graph_parties.load(std::memory_order_acquire);
    if (parties == 0U)
    {
      return;
    }

    std::unique_lock<std::mutex> lock(m_graph_mutex);

    // If the superstep already advanced, return immediately.
    if (m_graph_superstep.load(std::memory_order_acquire) != observed_superstep)
    {
      return;
    }

    const std::uint32_t arrived = m_graph_arrived.fetch_add(1U, std::memory_order_acq_rel) + 1U;

    if (arrived >= parties)
    {
      // Last arrival advances the superstep and releases everyone.
      m_graph_arrived.store(0U, std::memory_order_release);
      m_graph_superstep.store(observed_superstep + 1ULL, std::memory_order_release);
      lock.unlock();
      m_graph_cv.notify_all();
      return;
    }

    m_graph_cv.wait(lock, [&]
    {
      return m_graph_superstep.load(std::memory_order_acquire) != observed_superstep;
    });
  }

} // namespace event
