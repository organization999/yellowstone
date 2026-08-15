# Yellowstone runtime generic binding fix

The typing stubs advertise the following classes as generic:

- `Event[PayloadT]`
- `EventBus[PayloadT]`
- `Scheduler[PayloadT]`

The pybind11 runtime classes did not implement `__class_getitem__`, so Python
evaluating an annotation such as `Event[Message]` raised:

```text
TypeError: type 'yellowstone._yellowstone.Event' is not subscriptable
```

The binding now exposes `__class_getitem__` for all three classes and returns a
real `types.GenericAlias`.

Examples:

```python
Event[Message]
EventBus[Message]
Scheduler[Message]
```

are now valid runtime expressions and expose normal `__origin__` and `__args__`
metadata.

Validation:
- native extension compiled under strict C++23 flags
- `python -m examples.basic` passed
- 14 Python unit tests passed
