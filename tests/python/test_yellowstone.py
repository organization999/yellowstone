from __future__ import annotations

from dataclasses import dataclass
from threading import Event as ThreadEvent
import time
import types
import unittest

import yellowstone


@dataclass(slots=True)
class Payload:
    value: int


class YellowstoneTests(unittest.TestCase):
    def test_runtime_generic_aliases_match_typing_contract(self) -> None:
        for generic in (
            yellowstone.Event,
            yellowstone.EventBus,
            yellowstone.Scheduler,
        ):
            alias = generic[Payload]

            self.assertIsInstance(alias, types.GenericAlias)
            self.assertIs(alias.__origin__, generic)
            self.assertEqual((Payload,), alias.__args__)

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


    def test_scheduler_orders_deadlines_and_never_forwards_early(self) -> None:
        bus = yellowstone.EventBus(Payload)
        received: list[tuple[int, int, int]] = []
        done = ThreadEvent()

        def handler(event: yellowstone.Event[Payload]) -> None:
            received.append((event.id, event.payload.value, time.time_ns()))
            if len(received) == 3:
                done.set()

        bus.subscribe(handler)
        bus.start()
        scheduler = yellowstone.Scheduler(bus)

        now = time.time_ns()
        deadlines = {
            1: now + 120_000_000,
            2: now + 40_000_000,
            3: now + 80_000_000,
        }

        try:
            scheduler.schedule(1, Payload(10), deadlines[1])
            scheduler.schedule(2, Payload(20), deadlines[2])
            scheduler.schedule(3, Payload(30), deadlines[3])

            self.assertTrue(done.wait(2.0))
        finally:
            scheduler.stop()
            bus.stop()

        self.assertEqual([2, 3, 1], [event_id for event_id, _, _ in received])

        for event_id, _, observed_at in received:
            self.assertGreaterEqual(observed_at, deadlines[event_id])

        self.assertEqual(3, scheduler.scheduled_events)
        self.assertEqual(3, scheduler.forwarded_events)
        self.assertEqual(0, scheduler.failed_events)
        self.assertEqual(0, scheduler.pending_events)

    def test_scheduler_schedule_one_targets_one_subscriber(self) -> None:
        bus = yellowstone.EventBus(Payload)
        first: list[int] = []
        second: list[int] = []
        done = ThreadEvent()

        first_id = bus.subscribe(
            lambda event: (first.append(event.payload.value), done.set())
        )
        bus.subscribe(lambda event: second.append(event.payload.value))
        bus.start()
        scheduler = yellowstone.Scheduler(bus)

        try:
            schedule_id = scheduler.schedule_one(
                first_id,
                10,
                Payload(99),
                time.time_ns() + 30_000_000,
            )
            self.assertGreater(schedule_id, 0)
            self.assertTrue(done.wait(2.0))
        finally:
            scheduler.stop()
            bus.stop()

        self.assertEqual([99], first)
        self.assertEqual([], second)

    def test_scheduler_retries_when_partition_is_full(self) -> None:
        bus = yellowstone.EventBus(Payload, capacity=1)
        received: list[int] = []
        done = ThreadEvent()

        bus.subscribe(
            lambda event: (
                received.append(event.id),
                done.set() if event.id == 2 else None,
            )
        )

        # Fill the only partition before the EventBus worker is started.
        bus.publish(1, Payload(1))

        scheduler = yellowstone.Scheduler(
            bus,
            retry_delay_ns=1_000_000,
        )

        try:
            scheduler.schedule(
                2,
                Payload(2),
                time.time_ns() + 20_000_000,
            )

            time.sleep(0.05)

            # The due event could not be published but must remain scheduled.
            self.assertEqual(1, scheduler.pending_events)
            self.assertEqual(0, scheduler.forwarded_events)

            bus.start()

            self.assertTrue(done.wait(2.0))
        finally:
            scheduler.stop()
            bus.stop()

        self.assertEqual([1, 2], received)
        self.assertEqual(1, scheduler.forwarded_events)
        self.assertEqual(0, scheduler.failed_events)

    def test_scheduler_stop_preserves_pending_work_for_restart(self) -> None:
        bus = yellowstone.EventBus(Payload)
        done = ThreadEvent()
        received: list[int] = []

        bus.subscribe(
            lambda event: (received.append(event.id), done.set())
        )
        bus.start()
        scheduler = yellowstone.Scheduler(bus)

        try:
            scheduler.schedule(
                7,
                Payload(7),
                time.time_ns() + 150_000_000,
            )

            scheduler.stop()

            self.assertFalse(scheduler.running)
            self.assertEqual(1, scheduler.pending_events)

            with self.assertRaises(RuntimeError):
                scheduler.schedule(
                    8,
                    Payload(8),
                    time.time_ns() + 20_000_000,
                )

            scheduler.start()

            self.assertTrue(done.wait(2.0))
        finally:
            scheduler.stop()
            bus.stop()

        self.assertEqual([7], received)
        self.assertEqual(0, scheduler.pending_events)

    def test_scheduler_enforces_event_bus_payload_type(self) -> None:
        bus = yellowstone.EventBus(Payload)
        scheduler = yellowstone.Scheduler(bus)

        try:
            with self.assertRaises(TypeError):
                scheduler.schedule(
                    1,
                    "wrong",
                    time.time_ns() + 10_000_000,
                )
        finally:
            scheduler.stop()


if __name__ == "__main__":
    unittest.main(verbosity=2)
