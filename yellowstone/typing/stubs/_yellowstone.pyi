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
