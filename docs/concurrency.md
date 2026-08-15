# Concurrency

- `publish()` is serialized so fan-out can preflight every subscriber capacity before inserting.
  A capacity failure therefore occurs before partial delivery caused by a full partition.
- Each subscriber partition is independently synchronized and has exactly one Phelps consumer.
- `stop()` first clears Yellowstone's running flag and wakes blocked partitions, then asks Phelps
  to stop/join the workers.
- Python callbacks acquire the GIL only while dispatching into Python. `start()` and `stop()`
  release the GIL while native lifecycle work runs.
- Python payload references use a shared `PyObject*` handle whose deleter reacquires the GIL,
  preventing refcount destruction on arbitrary native worker threads without the GIL.
