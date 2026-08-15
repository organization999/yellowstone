from __future__ import annotations

from dataclasses import dataclass
from threading import Event as ThreadEvent
import unittest

import yellowstone


@dataclass(slots=True)
class Payload:
    value: int


class YellowstoneTests(unittest.TestCase):
    def test_payload_type_is_enforced(self) -> None:
        bus = yellowstone.EventBus(Payload)
        bus.subscribe(lambda event: None)
        with self.assertRaises(TypeError):
            bus.publish(1, "wrong")

    def test_fanout_and_worker_consumption(self) -> None:
        bus = yellowstone.EventBus(Payload)
        done = ThreadEvent()
        received: list[tuple[int, int]] = []

        def handler(event: yellowstone.Event[Payload]) -> None:
            received.append((event.id, event.payload.value))
            if len(received) == 2:
                done.set()

        bus.subscribe(handler)
        bus.subscribe(handler)
        bus.publish(10, Payload(42))
        bus.start()
        try:
            self.assertTrue(done.wait(2.0))
        finally:
            bus.stop()
        self.assertEqual([(10, 42), (10, 42)], sorted(received))

    def test_merge_coalesces_same_id_and_superstep(self) -> None:
        bus = yellowstone.EventBus(
            Payload,
            merge=lambda current, incoming: Payload(current.value + incoming.value),
        )
        done = ThreadEvent()
        values: list[int] = []
        bus.subscribe(lambda event: (values.append(event.payload.value), done.set()))

        first = bus.publish(1, Payload(2), superstep=3)
        second = bus.publish(1, Payload(5), superstep=3)
        self.assertEqual(0, first.coalesced)
        self.assertEqual(1, second.coalesced)

        bus.start()
        try:
            self.assertTrue(done.wait(2.0))
        finally:
            bus.stop()
        self.assertEqual([7], values)

    def test_superstep_ordering(self) -> None:
        bus = yellowstone.EventBus(Payload)
        done = ThreadEvent()
        order: list[int] = []

        def handler(event: yellowstone.Event[Payload]) -> None:
            order.append(event.superstep)
            if len(order) == 3:
                done.set()

        bus.subscribe(handler)
        bus.publish(1, Payload(1), superstep=3)
        bus.publish(2, Payload(2), superstep=1)
        bus.publish(3, Payload(3), superstep=2)
        bus.start()
        try:
            self.assertTrue(done.wait(2.0))
        finally:
            bus.stop()
        self.assertEqual([1, 2, 3], order)

    def test_subscriptions_are_frozen_while_running(self) -> None:
        bus = yellowstone.EventBus(Payload)
        subscriber = bus.subscribe(lambda event: None)
        bus.start()
        try:
            with self.assertRaises(RuntimeError):
                bus.subscribe(lambda event: None)
            with self.assertRaises(RuntimeError):
                bus.unsubscribe(subscriber)
        finally:
            bus.stop()

    def test_publish_one_targets_only_one_subscriber(self) -> None:
        bus = yellowstone.EventBus(Payload)
        first: list[int] = []
        second: list[int] = []
        done = ThreadEvent()
        first_id = bus.subscribe(lambda event: (first.append(event.payload.value), done.set()))
        bus.subscribe(lambda event: second.append(event.payload.value))
        result = bus.publish_one(first_id, 3, Payload(9))
        self.assertEqual(1, result.delivered)
        bus.start()
        try:
            self.assertTrue(done.wait(2.0))
        finally:
            bus.stop()
        self.assertEqual([9], first)
        self.assertEqual([], second)

    def test_full_partition_prevents_partial_fanout(self) -> None:
        bus = yellowstone.EventBus(Payload, capacity=1)
        first = bus.subscribe(lambda event: None)
        second = bus.subscribe(lambda event: None)
        bus.publish_one(first, 1, Payload(1))
        with self.assertRaises(OverflowError):
            bus.publish(2, Payload(2))
        self.assertEqual(1, bus.pending(first))
        self.assertEqual(0, bus.pending(second))

    def test_bus_can_restart_with_existing_subscribers(self) -> None:
        bus = yellowstone.EventBus(Payload)
        values: list[int] = []
        done = ThreadEvent()

        def handler(event: yellowstone.Event[Payload]) -> None:
            values.append(event.payload.value)
            done.set()

        bus.subscribe(handler)
        bus.publish(1, Payload(1))
        bus.start()
        self.assertTrue(done.wait(2.0))
        bus.stop()

        done.clear()
        bus.publish(2, Payload(2))
        bus.start()
        try:
            self.assertTrue(done.wait(2.0))
        finally:
            bus.stop()
        self.assertEqual([1, 2], values)


if __name__ == "__main__":
    unittest.main(verbosity=2)
