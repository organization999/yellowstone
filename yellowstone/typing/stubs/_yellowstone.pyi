"""Static typing declarations for Yellowstone's native extension."""

from collections.abc import Callable
from typing import Final, Generic, TypeVar

PayloadT = TypeVar("PayloadT")

MAX_SUBSCRIBERS: Final[int]


class Event(Generic[PayloadT]):
    @property
    def id(self) -> int: ...

    @property
    def superstep(self) -> int: ...

    @property
    def sequence(self) -> int: ...

    @property
    def payload(self) -> PayloadT: ...


class PublishResult:
    @property
    def delivered(self) -> int: ...

    @property
    def coalesced(self) -> int: ...


class EventBus(Generic[PayloadT]):
    def __init__(
        self,
        payload_type: type[PayloadT],
        *,
        capacity: int = 1024,
        merge: Callable[[PayloadT, PayloadT], PayloadT] | None = None,
    ) -> None: ...

    def subscribe(self, handler: Callable[[Event[PayloadT]], None]) -> int: ...
    def unsubscribe(self, subscriber_id: int) -> None: ...

    def publish(
        self,
        event_id: int,
        payload: PayloadT,
        *,
        superstep: int = 0,
    ) -> PublishResult: ...

    def publish_one(
        self,
        subscriber_id: int,
        event_id: int,
        payload: PayloadT,
        *,
        superstep: int = 0,
    ) -> PublishResult: ...

    def start(self) -> None: ...
    def stop(self) -> None: ...

    @property
    def running(self) -> bool: ...

    @property
    def subscriber_count(self) -> int: ...

    @property
    def capacity(self) -> int: ...

    @property
    def payload_type(self) -> type[PayloadT]: ...

    def pending(self, subscriber_id: int) -> int: ...


class Scheduler(Generic[PayloadT]):
    """Schedule typed EventBus payloads for future wall-clock publication."""

    def __init__(
        self,
        event_bus: EventBus[PayloadT],
        *,
        retry_delay_ns: int = 1_000_000,
    ) -> None: ...

    def schedule(
        self,
        event_id: int,
        payload: PayloadT,
        forward_at_unix_nanoseconds: int,
        *,
        superstep: int = 0,
    ) -> int: ...

    def schedule_one(
        self,
        subscriber_id: int,
        event_id: int,
        payload: PayloadT,
        forward_at_unix_nanoseconds: int,
        *,
        superstep: int = 0,
    ) -> int: ...

    def start(self) -> None: ...
    def stop(self) -> None: ...

    @property
    def running(self) -> bool: ...

    @property
    def pending_events(self) -> int: ...

    @property
    def scheduled_events(self) -> int: ...

    @property
    def forwarded_events(self) -> int: ...

    @property
    def failed_events(self) -> int: ...

    @property
    def retry_delay_ns(self) -> int: ...
