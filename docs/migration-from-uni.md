# Migration from Uni

## EventBus extraction

The following event-bus concepts were retained:

- fan-out to an independent partition per subscriber;
- bounded queues;
- superstep-first ordering;
- FIFO ordering within a superstep;
- duplicate work coalescing.

The following application-specific concepts were intentionally removed:

- expression and expression-runtime payload assumptions;
- graph edges and graph-specific merge hooks;
- `Context` / `ExecutionRequest` coupling;
- persistence shards;
- command routing and execution policy;
- observer/exchange state machinery that existed to support those legacy consumers.

Applications now encode those concerns in their own payload type and subscriber handler.

## Scheduler extraction

The legacy scheduler behavior was retained without its analytics/event-envelope coupling:

- a pairing heap selects the earliest requested wall-clock deadline;
- equal deadlines preserve insertion order;
- inserting a new earlier root wakes the timed wait;
- due work is removed from the heap before publication;
- the scheduler publishes outside its own mutex;
- scheduled work is never intentionally forwarded early;
- stop wakes and joins the timing thread.

Yellowstone no longer creates `schedule_requested` and `scheduled_forward` application messages. Scheduling metadata is internal to `Scheduler<Payload>`. At the deadline, the original application payload is published as an ordinary Yellowstone event.

Yellowstone additionally handles bounded EventBus backpressure: a due request that encounters a full subscriber partition is retained and retried instead of being lost.
