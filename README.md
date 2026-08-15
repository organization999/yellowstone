# Yellowstone

Yellowstone is a standalone generic event bus with a native C++23 core and a Python API.
It extracts the event transport architecture previously embedded in `uni` and removes the
legacy expression, graph, persistence, execution-request, and command-routing dependencies.

## Design

- **Generic payloads.** `EventBus(T)` requires the Python payload type up front and rejects
  incompatible values at publish time.
- **Subscriber fan-out.** Every subscriber receives its own event copy in an independent
  partition.
- **Superstep ordering.** Lower supersteps are consumed first; insertion order is preserved
  within one superstep.
- **Coalescing.** Duplicate `(event_id, superstep)` work is merged in place. The default
  policy is latest-wins; applications may supply `merge(current, incoming) -> payload`.
- **Phelps execution.** One registered Phelps worker consumes each subscriber partition.
  Yellowstone owns no ad-hoc thread-pool implementation. The default Phelps specialization
  caps Yellowstone at 16 simultaneous subscribers.

## Python quick start

```python
from dataclasses import dataclass
from yellowstone import Event, EventBus

@dataclass
class Message:
    text: str

bus = EventBus(Message)

bus.subscribe(lambda event: print(event.payload.text))
bus.publish(1, Message("hello"))
bus.start()
# ...
bus.stop()
```

## Build

```powershell
python setup.py build_ext --inplace
python setup.py test_native
python setup.py test_python
python setup.py test_all
python setup.py bdist_wheel
```

Initialize the Phelps submodule before building from a Git checkout:

```powershell
git submodule update --init --recursive
```
