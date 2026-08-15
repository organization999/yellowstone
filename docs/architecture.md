# Architecture

Yellowstone preserves the event-bus behavior that was useful in `uni` while removing its
application-specific dependencies.

```text
publisher
   |
   v
EventBus<Payload>
   |
   +---- subscriber 0 partition ---- Phelps consumer worker ---- handler 0
   |
   +---- subscriber 1 partition ---- Phelps consumer worker ---- handler 1
   |
   +---- subscriber N partition ---- Phelps consumer worker ---- handler N
```

The subscriber registry is frozen while the bus is running. At `start()`, Yellowstone creates
one `phelps::WorkerManager` sized to the active subscriber count and registers one derived
consumer Worker per subscriber. This preserves per-subscriber single-consumer ordering.

Each partition orders by `superstep`, then FIFO insertion order. Duplicate `(event_id,
superstep)` events keep their original queue position and merge only the payload.

Yellowstone intentionally does **not** contain expression evaluation, graph-edge routing,
legacy state/context objects, persistence shards, or command execution. Those were consumers
of the old event transport, not requirements of an event bus.

## Capacity boundary

The current Phelps `WorkerManager` uses its default `ThreadPool<>` specialization with 16 worker
slots. Yellowstone therefore exposes `MAX_SUBSCRIBERS == 16` and maps one active subscriber to
one Phelps worker. This is an intentional ordering boundary, not an arbitrary Yellowstone queue
limit.
