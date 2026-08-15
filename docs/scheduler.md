# Scheduler

Yellowstone's `Scheduler` delays publication of an event until a requested wall-clock deadline. It is generic over the same payload type as the `EventBus` it targets.

```text
producer
   |
   | schedule(event_id, payload, deadline)
   v
Scheduler<Payload>
   |
   | pairing heap: earliest deadline first
   | condition-variable timed wait
   v
EventBus<Payload>
   |
   +---- subscriber partition ---- Phelps worker ---- handler
```

## Why the scheduler has its own thread

The EventBus uses Phelps workers to drain subscriber partitions. The scheduler does not use one of those workers for its deadline wait. A scheduled deadline may be seconds, hours, or days in the future; occupying a general worker with a blocking `wait_until()` would waste a worker slot, while polling would add latency and unnecessary CPU usage.

Instead, `Scheduler` owns one `std::jthread` whose only responsibility is deadline management. Once an event becomes due, the scheduler publishes it into the ordinary EventBus and the existing Phelps-backed consumers take over.

## Ordering

Each scheduled request stores:

- the target wall-clock deadline;
- a monotonically increasing insertion order;
- a unique `ScheduleID`;
- optional target subscriber ID;
- Yellowstone event ID and superstep;
- the application payload.

The pairing heap orders by deadline first and insertion order second. Therefore:

1. the earliest deadline is always selected first;
2. requests with the same deadline are forwarded FIFO;
3. inserting a request that becomes the new root wakes the timing thread immediately.

The scheduler removes a due item from the heap **before** publishing it. This allows re-entrant EventBus callbacks to inspect or schedule new work without seeing the currently forwarding request as still pending.

## Deadlines

Native C++ accepts `std::chrono::system_clock::time_point` or Unix nanoseconds. The Python API uses Unix nanoseconds:

```python
import time

forward_at = time.time_ns() + 500_000_000
scheduler.schedule(42, payload, forward_at)
```

A request whose deadline is already in the past is eligible for immediate publication. The scheduler does not intentionally publish before the requested deadline.

## Broadcast and targeted delivery

`schedule()` eventually calls `EventBus.publish()`, so the event fans out to every subscriber.

`schedule_one()` eventually calls `EventBus.publish_one()`, so only the selected subscriber partition receives the event.

Both methods preserve the supplied `event_id` and `superstep`. The EventBus assigns the final event sequence when the scheduled request is actually published.

## Backpressure

EventBus subscriber partitions are bounded. If a due scheduled event cannot be published because a partition is full, Yellowstone treats that condition as transient backpressure:

1. the scheduled item is reinserted with the same original deadline and insertion order;
2. the scheduler waits for `retry_delay_ns`;
3. publication is retried.

The scheduled request is not counted as failed or silently discarded merely because the bus is temporarily full.

Other EventBus publication failures are counted in `failed_events`. The native scheduler can receive an asynchronous error callback; the Python binding reports such failures through Python's unraisable-exception mechanism because they occur on the timing thread.

## Lifecycle

`Scheduler` starts when constructed.

`stop()`:

- stops accepting new schedule requests;
- requests stop on the timing thread;
- wakes its condition variable;
- joins the thread;
- leaves pending scheduled requests in the pairing heap.

`start()` can then restart the scheduler and resume those pending requests. `start()` and `stop()` are idempotent.

The native `EventBus` must outlive its `Scheduler`. The Python binding enforces this lifetime relationship by retaining the `EventBus` while its scheduler exists.

## Counters

The scheduler exposes:

- `pending_events`: requests currently stored in the heap;
- `scheduled_events`: accepted schedule requests since construction;
- `forwarded_events`: requests successfully published into the EventBus;
- `failed_events`: non-backpressure publication failures;
- `retry_delay_ns`: backpressure retry delay.

`forwarded_events` means the event has entered the EventBus. If the bus is stopped at that moment, the event may remain queued in subscriber partitions until the EventBus starts.
