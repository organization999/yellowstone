"""Basic Yellowstone fan-out example."""
from dataclasses import dataclass
from time import sleep

from yellowstone import Event, EventBus


@dataclass(slots=True)
class Message:
    text: str


def main() -> None:
    bus = EventBus(Message)

    def first(event: Event[Message]) -> None:
        print("first:", event.id, event.payload.text)

    def second(event: Event[Message]) -> None:
        print("second:", event.id, event.payload.text)

    bus.subscribe(first)
    bus.subscribe(second)
    bus.publish(1, Message("hello from Yellowstone"))
    bus.start()

    try:
        sleep(0.05)
    finally:
        bus.stop()


if __name__ == "__main__":
    main()
