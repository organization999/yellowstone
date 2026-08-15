"""Schedule typed Yellowstone events for future EventBus publication."""

from __future__ import annotations

from dataclasses import dataclass
from threading import Event as ThreadEvent
import time

from yellowstone import Event, EventBus, Scheduler


@dataclass(slots=True)
class Message:
    """Payload transported by the example EventBus."""

    text: str


def main() -> None:
    """Forward three events in deadline order rather than insertion order."""

    bus = EventBus(Message)
    completed = ThreadEvent()
    received: list[tuple[int, str]] = []

    def consume(event: Event[Message]) -> None:
        received.append((event.id, event.payload.text))

        if len(received) == 3:
            completed.set()

    bus.subscribe(consume)
    bus.start()

    scheduler = Scheduler(bus)
    now = time.time_ns()

    # Insert out of deadline order. The scheduler forwards 2, then 3, then 1.
    scheduler.schedule(1, Message("third"), now + 120_000_000)
    scheduler.schedule(2, Message("first"), now + 40_000_000)
    scheduler.schedule(3, Message("second"), now + 80_000_000)

    try:
        if not completed.wait(timeout=2.0):
            raise RuntimeError("scheduled events were not delivered before timeout")

        print(received)
    finally:
        scheduler.stop()
        bus.stop()


if __name__ == "__main__":
    main()
