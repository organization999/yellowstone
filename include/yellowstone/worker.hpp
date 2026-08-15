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
 * @file worker.hpp
 * @brief Declares the Worker interfaces used by the event component.
 *
 * @details
 * The declarations in this header define the public or internal contract exposed by this slice of the codebase. The types, aliases, constants, and function signatures here are intended to be the authoritative interface that nearby translation units include and implement.
 */

#pragma once

#ifndef EVENT_ENABLE_PERSISTENCE
#define EVENT_ENABLE_PERSISTENCE 0
#endif

#if EVENT_ENABLE_PERSISTENCE
#include "persistence/persistence.hpp"
#endif

#include "event.hpp"
#include "observer.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

/**
 * @namespace event
 * @brief Contains event dispatch, routing, observation, and worker-side
 * persistence facilities.
 */
namespace event
{
  /**
   * @class Worker
   * @brief Event observer responsible for routing events and batching them into
   * worker-local persistent storage.
   *
   * @details
   * A Worker subscribes to the event subsystem through the AObserver interface.
   * It owns a local Router for dispatching events by EventScheduleType and a
   * worker-local Persistence instance for storing selected event envelopes.
   *
   * Each Worker receives a unique worker identifier and therefore a unique
   * persistence path:
   *
   * @code
   * tmp/workers/<worker-id>/events
   * @endcode
   *
   * Events are not written to disk immediately. Instead, persistable events are
   * accumulated in an in-memory batch and flushed once the batch reaches
   * kPersistenceBatchSize or when the Worker is destroyed.
   *
   * This design favors high throughput by reducing disk trips and isolating each
   * worker into its own shard tree.
   */
  class Worker final : public AObserver
  {
  private:
    /**
     * @class Router
     * @brief Maps event schedule types to controller callbacks.
     *
     * @details
     * Router is the Worker-local dispatch table. It associates an
     * EventScheduleType with a Controller. When an event is dispatched, the
     * router checks the event type and invokes the matching controller if one
     * exists.
     *
     * If no route exists, or if the controller is empty, the original event is
     * returned unchanged.
     */
    class Router final
    {
    public:
      /**
       * @typedef EventPtr
       * @brief Shared ownership pointer to an abstract event.
       */
      using EventPtr      = std::shared_ptr<AEvent>;

      /**
       * @typedef EventRef
       * @brief Const reference to a shared event pointer.
       */
      using EventRef      = const std::shared_ptr<AEvent>&;

      /**
       * @typedef Controller
       * @brief Callback used to transform, consume, or forward an event.
       */
      using Controller    = std::function<EventPtr(EventRef)>;

      /**
       * @typedef ControllerRef
       * @brief Const reference to a controller callback.
       */
      using ControllerRef = const Controller&;

      /**
       * @typedef Routes
       * @brief Hash table mapping schedule types to controllers.
       */
      using Routes        = std::unordered_map<EventScheduleType, Controller>;

      /**
       * @brief Dispatches an event to the controller registered for its schedule
       * type.
       *
       * @param event Shared event pointer to dispatch.
       *
       * @return The controller result when a route exists; the original event
       * when no route exists; nullptr when @p event is null.
       *
       * @note This function is noexcept. Controller exceptions should be handled
       * by the controller implementation.
       */
      EventPtr dispatch(EventRef) noexcept;

      /**
       * @brief Registers or replaces a controller for a schedule type.
       *
       * @param type Event schedule type used as the route key.
       * @param controller Controller callback to store.
       */
      void handle(const EventScheduleType type, ControllerRef controller) noexcept;

    private:
      /**
       * @brief Looks up the controller registered for a schedule type.
       *
       * @param type Event schedule type to search for.
       *
       * @return Reference to the registered controller, or a static empty
       * controller when no route exists.
       */
      ControllerRef m_get(const EventScheduleType type) const noexcept;

      /**
       * @brief Worker-local route table.
       */
      Routes m_routes{};

    }; // class Router final

  public:
    /**
     * @typedef PersistedEvent
     * @brief Concrete event envelope type persisted by Worker.
     */
    using PersistedEvent = event::Event<event::EventScheduleType, 16>;

    /**
     * @typedef EventID
     * @brief Persistent event identifier type.
     */
  #if EVENT_ENABLE_PERSISTENCE
    using EventID        = persistence::Persistence::EventID;
  #else
    using EventID        = std::uint64_t;
  #endif

    /**
     * @brief Constructs a Worker with a unique worker-local persistence shard.
     *
     * @param root Root directory used for worker-local storage.
     *
     * @details
     * The constructor generates a unique worker id, derives a worker-specific
     * path from @p root, and initializes the Persistence layer using that path.
     *
     * The resulting directory layout is:
     *
     * @code
     * <root>/workers/<worker-id>/events
     * @endcode
     */
    Worker(const std::filesystem::path& root = std::filesystem::path("tmp"));

    /**
     * @brief Flushes pending events and cleans up empty shard directories.
     *
     * @details
     * The destructor attempts to persist all buffered events before shutdown.
     * After flushing, it removes empty shard directories under the worker-local
     * persistence path.
     *
     * @note Destruction should not allow persistence cleanup failures to escape.
     */
    ~Worker() override;

    /**
     * @brief Persists eligible events and dispatches the event through the
     * worker router.
     *
     * @param event Event pointer to dispatch.
     *
     * @return Routed event result, original event, or nullptr when @p event is
     * null.
     *
     * @details
     * If @p event is dynamically identified as PersistedEvent, the Worker first
     * appends it to the write-behind persistence batch. The event is then passed
     * through the Router.
     */
    Router::EventPtr dispatch(Router::EventRef) noexcept override;

    /**
     * @brief Registers a route handler for a schedule type.
     *
     * @param type Schedule type that should be handled by @p controller.
     * @param controller Controller callback to invoke for matching events.
     */
    void handle(const EventScheduleType type, Router::ControllerRef controller) noexcept override;

    /**
     * @brief Adds an event to the in-memory persistence batch.
     *
     * @param event Concrete event envelope to persist later.
     *
     * @details
     * The event is buffered in memory. When the number of buffered events reaches
     * kPersistenceBatchSize, flush() is invoked automatically.
     *
     * @note This method is noexcept so persistence failures do not interrupt
     * dispatch.
     */
    void persist(const PersistedEvent& event) noexcept;

    /**
     * @brief Writes all currently buffered events to the Persistence layer.
     *
     * @details
     * Each buffered event is assigned the next monotonically increasing worker
     * local EventID. The buffer is cleared only after all events are successfully
     * stored.
     *
     * @note This method is noexcept. If persistence fails, pending events remain
     * in memory.
     */
    void flush(void) noexcept;

    /**
     * @brief Gets the unique id assigned to this worker.
     *
     * @return Const reference to the worker id string.
     */
    [[nodiscard]] const std::string& get_id(void) const noexcept;

  private:
    /**
     * @brief Maximum number of events buffered before automatic flush.
     */
    static constexpr std::size_t kPersistenceBatchSize = 1024;

    /**
     * @brief Removes empty persistence shard directories owned by this worker.
     *
     * @details
     * Cleanup walks the worker-local event path bottom-up and removes empty
     * directories. It also removes the worker directory and workers directory
     * when they become empty.
     *
     * @note This operation is noexcept and intended for shutdown cleanup.
     */
    void cleanup_empty_shards(void) noexcept;

    /**
     * @brief Generates a unique worker identifier.
     *
     * @return Worker id string composed from a high-resolution counter,
     * random-device output, and an atomic process-local sequence number.
     */
    [[nodiscard]] static std::string generate_worker_id(void);

    /**
     * @brief Builds the worker-specific event persistence path.
     *
     * @param root Root persistence directory.
     * @param id Unique worker id.
     *
     * @return Filesystem path for this worker's event shard.
     */
    [[nodiscard]] static std::filesystem::path build_worker_path(
      const std::filesystem::path& root,
      const std::string& id);

    /**
     * @brief In-memory write-behind queue for events awaiting persistence.
     */
    std::vector<PersistedEvent> m_pending_events{};

    /**
     * @brief Next worker-local event id assigned during flush.
     */
  #if EVENT_ENABLE_PERSISTENCE
    EventID                     m_next_event_id{0};
  #endif

    /**
     * @brief Unique worker identifier.
     */
    std::string                 m_worker_id{};

    /**
     * @brief Worker-specific persistence path.
     */
    std::filesystem::path       m_worker_path{};

    /**
     * @brief Worker-local persistence backend.
     */
  #if EVENT_ENABLE_PERSISTENCE
    persistence::Persistence    m_persistence;
  #endif

    /**
     * @brief Worker-local event router.
     */
    Router                      m_registry{};

  }; // class Worker final

} // namespace event
