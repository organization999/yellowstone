# Concurrency

## EventBus

- `publish()` is serialized so fan-out can preflight every subscriber capacity before inserting. A capacity failure therefore occurs before partial delivery caused by a full partition.
- Each subscriber partition is independently synchronized and has exactly one Phelps consumer.
- `stop()` first clears Yellowstone's running flag and wakes blocked partitions, then asks Phelps to stop/join the workers.
- Python callbacks acquire the GIL only while dispatching into Python. `start()` and `stop()` release the GIL while native lifecycle work runs.
- Python payload references use a shared `PyObject*` handle whose deleter reacquires the GIL, preventing refcount destruction on arbitrary native worker threads without the GIL.

## Scheduler

- Schedule insertion and pairing-heap access are protected by the scheduler mutex.
- The timing thread releases that mutex before publishing into EventBus, so subscriber callbacks may safely re-enter the scheduler or bus.
- Every insertion notifies the scheduler condition variable because the new request may become the earliest deadline.
- The timed wait uses a root-change predicate. If a new earlier `(deadline, insertion_order)` root appears, the thread recomputes its wait immediately.
- A due request is extracted before EventBus publication, preserving re-entrant pending-state semantics.
- EventBus `overflow_error` is treated as transient backpressure; the same scheduled item is reinserted and retried after a bounded delay.
- `stop()` disables acceptance before requesting stop, wakes the condition variable, and joins the timing thread. Pending items stay in the heap for restart.
- Python `Scheduler.start()` and `Scheduler.stop()` release the GIL. Native scheduler errors are converted to unraisable Python errors from the timing thread.
