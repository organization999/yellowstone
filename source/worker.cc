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
#include "policy/event/event.hpp"
#include "policy/event/observer.hpp"
#include "policy/event/worker.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @namespace event
 * @brief Contains the event facilities for this part of the codebase.
 */
namespace event
{
  /**
   * @brief Dispatches an event through the route registered for its schedule type.
   *
   * @param event Shared event pointer to route.
   *
   * @return nullptr when @p event is null, the original event when no matching
   * route exists, or the controller result when a route is registered.
   */
  Worker::Router::EventPtr Worker::Router::dispatch(EventRef event) noexcept
  {
    if (!event)
    {
      return nullptr;
    }

    const auto it = m_routes.find(event->type);
    if (it == m_routes.end())
    {
      return event;
    }

    const Controller& controller = it->second;
    if (!controller)
    {
      return event;
    }

    return controller(event);
  }

  /**
   * @brief Registers a controller for a schedule type.
   *
   * @param type Schedule type used as the route key.
   * @param controller Controller callback to invoke for matching events.
   */
  void Worker::Router::handle(const EventScheduleType type, ControllerRef controller) noexcept
  {
    m_routes[type] = controller;
  }

  /**
   * @brief Retrieves the controller associated with a schedule type.
   *
   * @param type Schedule type to search for.
   *
   * @return Reference to the registered controller, or a static empty
   * controller if no route exists.
   */
  Worker::Router::ControllerRef Worker::Router::m_get(const EventScheduleType type) const noexcept
  {
    static Controller kNoop{};
    const auto it = m_routes.find(type);
    return (it == m_routes.end()) ? kNoop : it->second;
  }

  /**
   * @brief Constructs a worker and initializes its private persistence shard.
   *
   * @param root Root directory used for worker-local event storage.
   *
   * @details
   * The constructor generates a unique worker id, derives the worker persistence
   * path, constructs the Persistence backend, and ensures the worker directory
   * exists on disk.
   */
  Worker::Worker(const std::filesystem::path& root)
    : m_worker_id(generate_worker_id()),
      m_worker_path(build_worker_path(root, m_worker_id))
#if EVENT_ENABLE_PERSISTENCE
    , m_persistence(m_worker_path)
#endif
  {
    std::filesystem::create_directories(m_worker_path);
  }

  /**
   * @brief Flushes pending events and removes empty shard directories.
   */
  Worker::~Worker()
  {
    flush();
    cleanup_empty_shards();
  }

  /**
   * @brief Buffers persistable events and dispatches the event through the router.
   *
   * @param event Shared event pointer to process.
   *
   * @return nullptr when @p event is null, otherwise the router dispatch result.
   */
  Worker::Router::EventPtr Worker::dispatch(Router::EventRef event) noexcept
  {
    if (!event)
    {
      return nullptr;
    }

    if (const auto* persisted = dynamic_cast<const PersistedEvent*>(event.get()))
    {
      persist(*persisted);
    }

    return m_registry.dispatch(event);
  }

  /**
   * @brief Registers a controller with the worker router.
   *
   * @param type Schedule type handled by the controller.
   * @param controller Controller callback to register.
   */
  void Worker::handle(const EventScheduleType type, Router::ControllerRef controller) noexcept
  {
    m_registry.handle(type, controller);
  }

  /**
   * @brief Adds an event to the write-behind persistence buffer.
   *
   * @param event Concrete event envelope to persist.
   *
   * @details
   * The event is stored in memory first. When the buffer reaches
   * kPersistenceBatchSize, the worker attempts to flush the batch to disk.
   */
  void Worker::persist(const PersistedEvent& event) noexcept
  {
    try
    {
      m_pending_events.push_back(event);

      if (m_pending_events.size() >= kPersistenceBatchSize)
      {
        flush();
      }
    }
    catch (...)
    {
      // Persistence must not break event dispatch.
    }
  }

  /**
   * @brief Writes all buffered events to the worker-local persistence backend.
   *
   * @details
   * Each event receives a monotonically increasing worker-local event id. The
   * buffer is cleared only after the write loop completes successfully.
   *
   * @note If a persistence exception occurs, pending events are kept in memory.
   */
  void Worker::flush(void) noexcept
  {
    try
    {
#if EVENT_ENABLE_PERSISTENCE
      for (const auto& event : m_pending_events)
      {
        m_persistence.store(m_next_event_id++, event);
      }

      m_pending_events.clear();
#else
      m_pending_events.clear();
#endif
    }
    catch (...)
    {
      // Keep pending events in memory if disk write fails.
    }
  }

  /**
   * @brief Gets this worker's unique id.
   *
   * @return Const reference to the worker id.
   */
  [[nodiscard]] const std::string& Worker::get_id(void) const noexcept
  {
    return m_worker_id;
  }

  /**
   * @brief Removes empty persistence shard directories owned by this worker.
   *
   * @details
   * The cleanup pass walks the worker persistence tree and removes empty
   * directories bottom-up. After the shard tree is pruned, the worker event
   * root, worker root, and shared workers directory are also removed when empty.
   *
   * @note This method is noexcept because it is called during shutdown.
   */
  void Worker::cleanup_empty_shards(void) noexcept
  {
    try
    {
      if (!std::filesystem::exists(m_worker_path))
      {
        return;
      }

      //
      // Remove empty directories bottom-up.
      //
      for (;;)
      {
        bool removed = false;

        for (auto it =
              std::filesystem::recursive_directory_iterator(
                m_worker_path,
                std::filesystem::directory_options::
                  skip_permission_denied);
            it != std::filesystem::recursive_directory_iterator{};
            ++it)
        {
          if (!it->is_directory())
          {
            continue;
          }

          const auto& path = it->path();

          if (std::filesystem::is_empty(path))
          {
            std::filesystem::remove(path);
            removed = true;
          }
        }

        if (!removed)
        {
          break;
        }
      }

      //
      // Remove worker root if now empty.
      //
      if (std::filesystem::exists(m_worker_path) &&
          std::filesystem::is_empty(m_worker_path))
      {
        std::filesystem::remove(m_worker_path);
      }

      //
      // Remove worker directory if empty.
      //
      const auto worker_root = m_worker_path.parent_path();

      if (std::filesystem::exists(worker_root) &&
          std::filesystem::is_empty(worker_root))
      {
        std::filesystem::remove(worker_root);
      }

      //
      // Remove workers/ if empty.
      //
      const auto workers_root = worker_root.parent_path();

      if (std::filesystem::exists(workers_root) &&
          std::filesystem::is_empty(workers_root))
      {
        std::filesystem::remove(workers_root);
      }
    }
    catch (...)
    {
      //
      // Cleanup must never throw during shutdown.
      //
    }
  }

  /**
   * @brief Generates a unique worker id.
   *
   * @return Unique worker id string.
   */
  [[nodiscard]] std::string Worker::generate_worker_id(void)
  {
    static std::atomic<std::uint64_t> counter{0};

    const auto ticks =
      static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now()
          .time_since_epoch()
          .count());

    std::random_device rd;

    std::stringstream ss;

    ss
      << std::hex
      << ticks
      << "-"
      << rd()
      << "-"
      << counter.fetch_add(1,
            std::memory_order_relaxed);

    return ss.str();
  }

  /**
   * @brief Builds this worker's persistence path.
   *
   * @param root Root storage directory.
   * @param id Worker id.
   * @return Worker-local event storage path.
   */
  [[nodiscard]] std::filesystem::path Worker::build_worker_path(
    const std::filesystem::path& root,
    const std::string& id)
  {
    return
      root
      / "workers"
      / id
      / "events";
  }

} // namespace event
