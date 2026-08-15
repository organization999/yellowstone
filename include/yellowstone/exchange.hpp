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
 * @file exchange.hpp
 * @brief Declares the Exchange interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#include "collections/graph_partition_queue.hpp"

#include "event.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  static constexpr std::size_t kMaxNumSubscribers =   16UL;
  static constexpr std::size_t kMaxNumPartitions  =   16UL;
  static constexpr std::size_t kMaxChannelSize    = 1024UL;

  /**
   * @typedef PartitionID
   * @brief Alias used by the event component to name the PartitionID concept.
   */
  using PartitionID = std::int64_t;
  /**
   * @typedef WorkerID
   * @brief Alias used by the event component to name the WorkerID concept.
   */
  using WorkerID    = std::int64_t;

  /**
   * @typedef Partition
   * @brief Alias used by the event component to name the Partition concept.
   */
  using Partition   = collections::GraphPartitionQueue<kMaxChannelSize>;

  /**
   * @struct Notice
   * @brief Represents the Notice data structure used by the event component.
   */
  struct Notice
  {
    WorkerID      worker_id;
    PartitionID   partition_id;
    std::uint32_t gen;
    Partition*    partition;
  };

  /**
   * @struct Lease
   * @brief Represents the Lease data structure used by the event component.
   */
  struct Lease
  {
    PartitionID   pid{-1};
    std::uint32_t gen{0};
  };

  /**
   * @struct LeaseCtrl
   * @brief Represents the LeaseCtrl data structure used by the event component.
   */
  struct LeaseCtrl
  {
    std::atomic<std::uint32_t> gen{0};
    std::atomic<std::uint32_t> closed_gen{0};
  };

  /**
   * @brief Exchange distributes and reclaims partitions.
   */
  class Exchange final
  {
  public:

    /**
     * @brief Dispatcher tracks active partition leases per worker.
     */
    class Dispatcher final
    {
    public:

      /**
       * @struct Entry
       * @brief Represents the Entry data structure used by the event component.
       */
      struct Entry
      {
        Lease lease{};
      };

    private:

      collections::UnorderedMap<WorkerID, Entry, 32UL> m_registry{};
      mutable std::mutex                               m_mutex{};

    public:

      /**
       * @brief Performs the Try Get operation for the event component.
       * @param worker_id Value forwarded to the try_get operation.
       * @param out Value forwarded to the try_get operation.
       * @return Result produced by this operation.
       */
      [[nodiscard]] bool try_get(const WorkerID worker_id, Entry& out) noexcept;

      /**
       * @brief Performs the Set Open operation for the event component.
       * @param worker_id Value forwarded to the set_open operation.
       * @param l Value forwarded to the set_open operation.
       * @return Result produced by this operation.
       */
      [[nodiscard]] int set_open(const WorkerID worker_id, const Lease l) noexcept;

      /**
       * @brief Performs the Erase Open operation for the event component.
       * @param worker_id Value forwarded to the erase_open operation.
       * @param out Value forwarded to the erase_open operation.
       * @return Result produced by this operation.
       */
      [[nodiscard]] int erase_open(const WorkerID worker_id, Lease& out) noexcept;

    }; // class Dispatcher final

  private:

    std::array<Partition, kMaxNumPartitions>                   m_partitions{};
    std::array<LeaseCtrl, kMaxNumPartitions>                   m_ctrl{};
    collections::Queue<PartitionID, (2UL * kMaxNumPartitions)> m_ready{};
    Dispatcher                                                 m_dispatcher{};
    mutable std::mutex                                         m_mutex{};
    std::array<std::mutex, kMaxNumPartitions>                  m_part_mutex{};

    // Optional graph-mode coordination (superstep barrier). Disabled until the
    // first graph-enabled event is published.
    std::atomic<bool>                                          m_graph_mode{false};
    std::atomic<std::uint64_t>                                 m_graph_superstep{0ULL};
    std::atomic<std::uint32_t>                                 m_graph_parties{0U};
    std::atomic<std::uint32_t>                                 m_graph_arrived{0U};
    std::atomic<std::uint64_t>                                 m_graph_epoch{0ULL};
    std::mutex                                                 m_graph_mutex{};
    std::condition_variable                                    m_graph_cv{};

  public:

    /**
     * @brief Performs the Exchange operation for the event component.
     */
    Exchange() noexcept;

    /**
     * @brief Performs the Is Closed operation for the event component.
     * @param pid Value forwarded to the is_closed operation.
     * @param gen Value forwarded to the is_closed operation.
     * @return Result produced by this operation.
     */
    [[nodiscard]] bool is_closed(const PartitionID  pid,
                                const std::uint32_t gen) const noexcept;

    void close_lease(const WorkerID     worker_id,
                    const PartitionID   pid,
                    const std::uint32_t gen) noexcept;

    /**
     * @brief Performs the Partition Mutex operation for the event component.
     * @param id Value forwarded to the partition_mutex operation.
     * @return Result produced by this operation.
     */
    std::mutex& partition_mutex(PartitionID id) noexcept;

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
     * @param pid Value forwarded to the release operation.
     * @param gen Value forwarded to the release operation.
     * @return Result produced by this operation.
     */
    [[nodiscard]] int release(const WorkerID      worker_id,
                              const PartitionID   pid,
                              const std::uint32_t gen) noexcept;

    /**
     * @brief Configures the expected participant count for the graph superstep barrier.
     */
    void set_graph_barrier_parties(const std::size_t parties) noexcept
    {
      m_graph_parties.store(static_cast<std::uint32_t>(parties), std::memory_order_release);
    }

    /**
     * @brief Enables graph-mode on the exchange (sticky for process lifetime).
     */
    void enable_graph_mode(void) noexcept
    {
      m_graph_mode.store(true, std::memory_order_release);
    }

    void begin_graph_epoch(void) noexcept
    {
      const bool was_enabled = m_graph_mode.exchange(true, std::memory_order_acq_rel);
      if (!was_enabled)
      {
        m_graph_superstep.store(0ULL, std::memory_order_release);
        m_graph_arrived.store(0U, std::memory_order_release);
        m_graph_epoch.fetch_add(1ULL, std::memory_order_acq_rel);
      }
    }

    [[nodiscard]] bool graph_mode_enabled(void) const noexcept
    {
      return m_graph_mode.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t graph_current_superstep(void) const noexcept
    {
      return m_graph_superstep.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t graph_epoch(void) const noexcept
    {
      return m_graph_epoch.load(std::memory_order_acquire);
    }

    /**
     * @brief Participates in the global superstep barrier.
     *
     * Blocks the calling worker until the superstep advances. No-ops when graph
     * mode is disabled.
     */
    void graph_arrive_and_wait(const std::uint64_t observed_superstep) noexcept;

  }; // class Exchange final

} // namespace event
