# Architecture

Yellowstone preserves the useful transport and scheduling behavior that was embedded in the legacy system while removing application-specific dependencies.

```text
                       immediate publish
publisher ----------------------------------------+
                                                   |
producer -> Scheduler<Payload> -- deadline due ----+--> EventBus<Payload>
                 |                                      |
                 | pairing heap                         +-- subscriber 0 partition -- Phelps worker -- handler 0
                 | dedicated timed jthread              |
                 +-------------------------------------- +-- subscriber 1 partition -- Phelps worker -- handler 1
                                                        |
                                                        +-- subscriber N partition -- Phelps worker -- handler N
```

## EventBus

The subscriber registry is frozen while the bus is running. At `start()`, Yellowstone creates one `phelps::WorkerManager` sized to the active subscriber count and registers one derived consumer Worker per subscriber. This preserves per-subscriber single-consumer ordering.

Each partition orders by `superstep`, then FIFO insertion order. Duplicate `(event_id, superstep)` events keep their original queue position and merge only the payload.

## Scheduler

The scheduler is a producer of normal EventBus events, not a second transport. It stores future publication requests in a pairing heap ordered by deadline and insertion order. Its dedicated `std::jthread` blocks on a condition variable until either the root is due, a new earlier root is inserted, or the scheduler is stopped.

Once a deadline is reached, the request is removed from the heap and published through `EventBus.publish()` or `publish_one()`. From that point forward, subscriber delivery uses the same partition and Phelps-worker path as an immediate publication.

The timed wait deliberately does not occupy a Phelps worker. Phelps is the EventBus execution pool; the scheduler thread is a single-purpose timer coordinator.

Yellowstone intentionally does **not** contain expression evaluation, graph-edge routing, legacy state/context objects, persistence shards, or command execution. Those are consumers of the transport, not requirements of an event bus or scheduler.

## Capacity boundary

The current Phelps `WorkerManager` uses its default `ThreadPool<>` specialization with 16 worker slots. Yellowstone therefore exposes `MAX_SUBSCRIBERS == 16` and maps one active subscriber to one Phelps worker. This is an ordering/execution boundary, not a scheduler limit.
