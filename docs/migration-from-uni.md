# Migration from Uni

The following concepts were retained:

- fan-out to an independent partition per subscriber;
- bounded queues;
- superstep-first ordering;
- FIFO ordering within a superstep;
- duplicate work coalescing.

The following concepts were intentionally removed from Yellowstone:

- expression and expression-runtime payload assumptions;
- graph edges and graph-specific merge hooks;
- `Context` / `ExecutionRequest` coupling;
- persistence shards;
- command routing and execution policy;
- observer/exchange state machinery that existed to support those legacy consumers.

Applications now encode those concerns in their own payload type and subscriber handler.
