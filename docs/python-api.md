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

The callback runs only when both events have the same `event_id` and `superstep` in the same
subscriber partition.

### Lifecycle

Subscribers may be added/removed only while stopped. Events may be published before `start()`;
this is useful for deterministic initial batches. `start()` and `stop()` are idempotent.
