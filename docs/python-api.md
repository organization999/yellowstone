# Python API

## `EventBus[PayloadT]`

Construct with the payload class:

```python
bus = EventBus(MyPayload, capacity=1024)
```

`publish()` validates `isinstance(payload, MyPayload)` before entering the native bus.

### Coalescing

The default duplicate policy is latest-wins. Supply a merge callable to customize it:

```python
bus = EventBus(
    Counter,
    merge=lambda current, incoming: Counter(current.value + incoming.value),
)
```

The callback runs only when both events have the same `event_id` and `superstep` in the same subscriber partition.

### Lifecycle

Subscribers may be added/removed only while stopped. Events may be published before `start()`; this is useful for deterministic initial batches. `start()` and `stop()` are idempotent.

## `Scheduler[PayloadT]`

Attach a scheduler to an EventBus with the same payload type:

```python
import time
from yellowstone import Scheduler

scheduler = Scheduler(bus)

schedule_id = scheduler.schedule(
    100,
    MyPayload(...),
    time.time_ns() + 500_000_000,
    superstep=0,
)
```

`Scheduler` starts automatically when constructed. The EventBus does not need to be running when a request is scheduled; successful deadline publication may queue work in the bus for later consumption.

### `schedule(event_id, payload, forward_at_unix_nanoseconds, *, superstep=0)`

Schedules a broadcast event. At the deadline, the request enters `EventBus.publish()` and is fanned out to every subscriber.

Returns a unique integer `schedule_id`.

### `schedule_one(subscriber_id, event_id, payload, forward_at_unix_nanoseconds, *, superstep=0)`

Schedules targeted publication through `EventBus.publish_one()`.

### Payload validation

The scheduler validates payloads using the EventBus's declared Python payload type. A mismatched payload raises `TypeError` synchronously when `schedule()` or `schedule_one()` is called.

### Backpressure

If a subscriber partition is full when a scheduled event becomes due, the request remains scheduled and Yellowstone retries. Configure the retry interval in nanoseconds:

```python
scheduler = Scheduler(bus, retry_delay_ns=1_000_000)
```

### Lifecycle and metrics

```python
scheduler.stop()
assert not scheduler.running

scheduler.start()
assert scheduler.running

print(scheduler.pending_events)
print(scheduler.scheduled_events)
print(scheduler.forwarded_events)
print(scheduler.failed_events)
```

Pending requests survive `stop()`/`start()`. New schedule requests are rejected while the scheduler is stopped.
