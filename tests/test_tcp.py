#!/usr/bin/env python3
"""Integration checks for the native C protocol. Build car, race, viewer and control first."""

import ctypes as c
import os
from pathlib import Path
import pty
import re
import select
import signal
import socket
import subprocess
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get("RACING_BIN_DIR", ROOT / "bin")).resolve()
RACE_START, RACE_STOP, RACE_REGISTRATION, RACE_INIT, RACE_UPDATE, RACE_DISCONNECT = range(6)


class Point(c.Structure):
    _fields_ = [("x", c.c_int), ("y", c.c_int)]


class Road(c.Structure):
    _fields_ = [("lane", c.c_int), ("distance", c.c_int),
                ("forward", Point), ("back", Point), ("left", Point), ("right", Point)]


class Entity(c.Structure):
    _fields_ = [("type", c.c_int), ("id", c.c_int)]


class Cell(c.Structure):
    _fields_ = [("x", c.c_int), ("y", c.c_int), ("color", c.c_int),
                ("type", c.c_int), ("entity", Entity), ("road", Road)]


class Team(c.Structure):
    _fields_ = [("name", c.c_char * 13), ("color", c.c_int)]


class Car(c.Structure):
    _fields_ = [("id", c.c_int), ("name", c.c_char * 13), ("team", Team),
                ("pos", Point), ("laps", c.c_int), ("prev", Point),
                ("passed", c.c_bool), ("out", c.c_bool), ("paused", c.c_bool)]


class Registration(c.Structure):
    _fields_ = [("name", c.c_char * 13), ("team", Team), ("resume_id", c.c_int)]


class RegistrationResponse(c.Structure):
    _fields_ = [("accepted", c.c_bool), ("id", c.c_int)]


class Settings(c.Structure):
    _fields_ = [("minimum", c.c_int), ("maximum", c.c_int), ("laps", c.c_int)]


class Init(c.Structure):
    _fields_ = [("width", c.c_int), ("height", c.c_int),
                ("map", (Cell * 50) * 40), ("settings", Settings), ("car", Car)]

    @property
    def pos(self):
        return self.car.pos


class ControlRequest(c.Structure):
    _fields_ = [("action", c.c_int), ("car_id", c.c_int)]


class ControlResponse(c.Structure):
    _fields_ = [("accepted", c.c_bool), ("message", c.c_char * 96)]


class Response(c.Structure):
    _fields_ = [("allowed", c.c_bool), ("car", Car),
                ("over", c.c_bool), ("winner", c.c_int)]


class Standing(c.Structure):
    _fields_ = [("car", Car), ("lane", c.c_int), ("distance", c.c_int)]


class ViewerInit(c.Structure):
    _fields_ = [("width", c.c_int), ("height", c.c_int),
                ("map", (Cell * 50) * 40), ("expected", c.c_int), ("laps", c.c_int)]


class Snapshot(c.Structure):
    _fields_ = [("state", c.c_int), ("count", c.c_int),
                ("standings", Standing * 26), ("winner", c.c_int)]


def receive(peer, kind):
    data = bytearray()
    while len(data) < c.sizeof(kind):
        chunk = peer.recv(c.sizeof(kind) - len(data))
        if not chunk:
            raise EOFError(f"Received {len(data)}/{c.sizeof(kind)} bytes")
        data.extend(chunk)
    return kind.from_buffer_copy(data)


def position(point):
    return point.x, point.y


def receive_car_message(peer, expected, kind=None):
    command = receive(peer, c.c_int).value
    if command != expected:
        raise AssertionError(f"Expected car message {expected}, received {command}")
    return receive(peer, kind) if kind else None


class TcpRaceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="auto-racing-tcp-")
        self.path = Path(self.directory.name)
        self.processes = []
        self.files = []
        self.peers = []
        self.assigned_ids = {}
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            self.port = probe.getsockname()[1]

    def tearDown(self):
        for peer in self.peers:
            peer.close()
        for process in self.processes:
            if process.poll() is None:
                process.send_signal(signal.SIGINT)
        for process in self.processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
        for stream in self.files:
            stream.close()
        self.directory.cleanup()

    def spawn(self, name, executable, args):
        stdout = open(self.path / f"{name}.out", "wb")
        stderr = open(self.path / f"{name}.err", "wb")
        self.files.extend([stdout, stderr])
        process = subprocess.Popen([str(BIN / executable), *args], cwd=BIN,
                                   stdout=stdout, stderr=stderr)
        self.processes.append(process)
        return process

    def start_server(self, cars=1, laps=1, delay="0:1", name="server", draw_delay=50, map_only=False):
        self.report = self.path / f"{name}.report"
        self.server = self.spawn(name, "race", ["--cars", str(cars), "--laps", str(laps),
                                  "--ip", "127.0.0.1", "--port", str(self.port),
                                  "--map", str(ROOT / "data/circle"), "--car-delay", delay,
                                  "--output", str(self.report), "--draw-delay", str(draw_delay),
                                  *(["--map-only"] if map_only else [])])
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if "listening" in (self.path / f"{name}.out").read_text():
                return
            if self.server.poll() is not None:
                self.fail((self.path / f"{name}.err").read_text())
            time.sleep(0.01)
        self.fail("Server did not start listening")

    def start_car(self, label, name=None):
        name = name or f"Driver{label}"
        return self.spawn(f"car-{label}", "car", ["--driver", name,
                           "--team", "Test", "--color", "33", "--port", str(self.port)])

    def register(self, name, accepted=True, fragmented=False, color=33, resume_id=-1):
        peer = socket.create_connection(("127.0.0.1", self.port), timeout=5)
        peer.settimeout(5)
        self.peers.append(peer)
        message = Registration(name.encode(), Team(b"Test", color), resume_id)
        payload = bytes(c.c_int(0)) + bytes(message)  # CLIENT_CAR, затем регистрация
        if fragmented:
            for offset in range(0, len(payload), 3):
                peer.sendall(payload[offset:offset + 3])
        else:
            peer.sendall(payload)
        response = receive_car_message(peer, RACE_REGISTRATION, RegistrationResponse)
        self.assertEqual(response.accepted, accepted)

        if accepted:
            self.assertGreaterEqual(response.id, 0)
            if resume_id == -1:
                self.assertNotIn(response.id, self.assigned_ids.values())
            else:
                self.assertEqual(response.id, resume_id)
            self.assigned_ids[peer] = response.id
        else:
            self.assertEqual(response.id, -1)

        return peer

    def initialize(self, peer):
        initial = receive_car_message(peer, RACE_INIT, Init)
        self.assertEqual((initial.width, initial.height), (17, 11))
        receive_car_message(peer, RACE_START)
        return initial

    def move(self, peer, point):
        peer.sendall(bytes(Point(*point)))
        return receive_car_message(peer, RACE_UPDATE, Response)

    def control(self, action, car_id, accepted=True):
        with socket.create_connection(("127.0.0.1", self.port), timeout=5) as peer:
            peer.sendall(bytes(c.c_int(2)))  # CLIENT_CONTROL
            peer.sendall(bytes(ControlRequest(action, car_id)))
            response = receive(peer, ControlResponse)
        self.assertEqual(response.accepted, accepted, response.message.decode())
        return response.message.decode()

    def wait_for_output(self, name, text):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            output = (self.path / f"{name}.out").read_text()
            if text in output:
                return output
            time.sleep(0.01)
        self.fail(f"Missing {text!r} in {name}: {output}")

    def register_viewer(self, accepted=True):
        peer = socket.create_connection(("127.0.0.1", self.port), timeout=5)
        self.peers.append(peer)
        peer.sendall(bytes(c.c_int(1)))  # CLIENT_VIEWER
        self.assertEqual(receive(peer, c.c_bool).value, accepted)
        return peer

    def snapshot_until(self, peer, predicate):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            snapshot = receive(peer, Snapshot)
            if predicate(snapshot):
                return snapshot
        self.fail("Viewer did not receive the expected snapshot")

    def finish_server(self, status="Completed successfully"):
        self.assertEqual(self.server.wait(timeout=10), 0,
                         (self.path / "server.err").read_text() if (self.path / "server.err").exists() else "")
        report = self.report.read_text()
        self.assertIn(status, report)
        return report

    def test_two_real_cars(self):
        self.start_server(cars=2, laps=3)
        second = self.start_car(1)
        first = self.start_car(0)
        self.assertEqual(first.wait(timeout=10), 0, (self.path / "car-0.err").read_text())
        self.assertEqual(second.wait(timeout=10), 0, (self.path / "car-1.err").read_text())
        report = self.finish_server()
        rows = [line.split("|") for line in report.splitlines() if line.count("|") == 6]
        self.assertTrue(any(row[3].strip() == "3" for row in rows))
        for car_id in [0, 1]:
            output = (self.path / f"car-{car_id}.out").read_text()
            self.assertIn("race finished, winner ID", output)
            self.assertEqual((self.path / f"car-{car_id}.err").read_text(), "")

            initial = re.search(r"initialized at \((\d+), (\d+)\)", output)
            self.assertIsNotNone(initial)
            moves = re.findall(r"move to \((\d+), (\d+)\) accepted", output)
            positions = [tuple(map(int, initial.groups())), *(tuple(map(int, point)) for point in moves)]
            for index in range(1, len(positions)):
                before, after = positions[index - 1:index + 1]
                self.assertEqual(abs(before[0] - after[0]) + abs(before[1] - after[1]), 1)
                if index >= 2:
                    self.assertNotEqual(after, positions[index - 2])
                if index >= 4:
                    self.assertNotEqual(after, positions[index - 4], positions[index - 4:index + 1])

    def test_invalid_moves_and_authoritative_position(self):
        self.start_server()
        peer = self.register("First", fragmented=True)
        initial = self.initialize(peer)
        start = position(initial.pos)
        for target in [(-1, 0), (17, 1), (0, 0), (15, 9), start]:
            response = self.move(peer, target)
            self.assertFalse(response.allowed)
            self.assertEqual(position(response.car.pos), start)
            self.assertEqual(response.car.laps, 0)
        target = position(initial.map[initial.pos.y][initial.pos.x].road.forward)
        response = self.move(peer, target)
        self.assertTrue(response.allowed)
        self.assertEqual(position(response.car.pos), target)
        self.assertEqual(position(response.car.prev), start)
        self.assertFalse(self.move(peer, start).allowed)
        peer.close()
        self.finish_server("All cars are out")

    def test_car_can_pass_paused_car_after_turn(self):
        self.start_server(cars=2, laps=10)
        first, second = self.register("First"), self.register("Second")
        a, b = self.initialize(first), self.initialize(second)

        def follow_inner_lane(peer, initial, destination):
            state = initial.car
            for _ in range(2):
                target = initial.map[state.pos.y][state.pos.x].road.right
                response = self.move(peer, position(target))
                self.assertTrue(response.allowed)
                state = response.car
            for _ in range(200):
                if position(state.pos) == destination:
                    return state
                target = initial.map[state.pos.y][state.pos.x].road.forward
                response = self.move(peer, position(target))
                self.assertTrue(response.allowed)
                state = response.car
            self.fail(f"Car did not reach {destination}")

        saved = follow_inner_lane(first, a, (4, 3))
        state = follow_inner_lane(second, b, (3, 3))
        road = b.map[state.pos.y][state.pos.x].road
        self.assertEqual(position(road.forward), (4, 3))
        self.assertEqual(position(road.left), (3, 2))
        self.assertFalse(self.move(second, position(road.forward)).allowed)

        self.control(0, 0)
        receive_car_message(first, RACE_DISCONNECT)
        first.close()
        resumed = self.register("First", resume_id=0)
        self.assertFalse(self.move(second, position(road.forward)).allowed)
        self.assertTrue(self.move(second, position(road.left)).allowed)
        self.assertFalse(self.move(second, (3, 3)).allowed)
        self.assertTrue(self.move(second, (4, 2)).allowed)
        self.assertFalse(self.move(second, (4, 3)).allowed)
        self.assertTrue(self.move(second, (5, 2)).allowed)
        self.assertTrue(self.move(second, (5, 3)).allowed)

        self.control(1, 0)
        restored = self.initialize(resumed)
        self.assertEqual(position(restored.car.pos), position(saved.pos))
        self.assertEqual(restored.car.laps, saved.laps)
        resumed.close()
        second.close()
        self.finish_server("All cars are out")

    def test_car_cannot_enter_neighbor_against_traffic(self):
        self.start_server(laps=10)
        peer = self.register("First")
        initial = self.initialize(peer)
        target = initial.map[initial.pos.y][initial.pos.x].road.right
        response = self.move(peer, position(target))
        self.assertTrue(response.allowed)
        state = response.car

        for _ in range(200):
            if position(state.pos) == (2, 3):
                break
            target = initial.map[state.pos.y][state.pos.x].road.forward
            response = self.move(peer, position(target))
            self.assertTrue(response.allowed)
            state = response.car
        self.assertEqual(position(state.pos), (2, 3))

        self.assertTrue(self.move(peer, (3, 3)).allowed)
        self.assertEqual(position(initial.map[4][3].road.forward), (3, 3))
        rejected = self.move(peer, (3, 4))
        self.assertFalse(rejected.allowed)
        self.assertEqual(position(rejected.car.pos), (3, 3))
        self.assertEqual(position(rejected.car.prev), (2, 3))
        self.assertTrue(self.move(peer, (4, 3)).allowed)
        peer.close()
        self.finish_server("All cars are out")

    def test_occupied_cell_and_simultaneous_moves(self):
        self.start_server(cars=2, laps=10)
        first, second = self.register("First"), self.register("Second")
        a, b = self.initialize(first), self.initialize(second)
        self.assertFalse(self.move(second, position(a.pos)).allowed)
        inner = position(a.map[a.pos.y][a.pos.x].road.right)
        self.assertTrue(self.move(first, inner).allowed)
        self.assertTrue(self.move(second, position(a.pos)).allowed)
        ahead = position(a.map[a.pos.y][a.pos.x].road.forward)
        self.assertTrue(self.move(second, ahead).allowed)
        target = position(a.map[inner[1]][inner[0]].road.forward)
        self.assertEqual(target, position(a.map[ahead[1]][ahead[0]].road.right))
        barrier = threading.Barrier(2)
        responses, errors = [], []
        def compete(peer):
            try:
                barrier.wait(timeout=3)
                responses.append(self.move(peer, target))
            except BaseException as error:
                errors.append(error)
        threads = [threading.Thread(target=compete, args=(peer,)) for peer in [first, second]]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(timeout=6)
            self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(sum(response.allowed for response in responses), 1)
        self.assertNotEqual(position(responses[0].car.pos), position(responses[1].car.pos))
        first.close()
        second.close()
        self.finish_server("All cars are out")

    def test_server_assigns_unique_ids_even_for_same_names(self):
        self.start_server(cars=2)
        first = self.register("SameName")
        second = self.register("SameName")

        self.assertEqual(self.assigned_ids[first], 0)
        self.assertEqual(self.assigned_ids[second], 1)

        initial = self.initialize(first)
        self.initialize(second)
        occupants = [initial.map[y][x].entity.id for y in range(initial.height)
                     for x in range(initial.width) if initial.map[y][x].entity.type == 0]
        self.assertEqual(sorted(occupants), [0, 1])

        first.close()
        second.close()
        self.finish_server("All cars are out")

    def test_invalid_registration_does_not_consume_id(self):
        self.start_server()
        invalid_name = self.register("", accepted=False)
        invalid_color = self.register("BadColor", accepted=False, color=99)

        self.assertEqual(invalid_name.recv(1), b"")
        self.assertEqual(invalid_color.recv(1), b"")

        valid = self.register("Valid")
        self.assertEqual(self.assigned_ids[valid], 0)
        self.initialize(valid)

        valid.close()
        self.finish_server("All cars are out")

    def test_finish_sent_to_idle_car(self):
        self.start_server(cars=2)
        idle = self.register("First")
        driver = self.start_car(1)
        self.initialize(idle)
        response = receive_car_message(idle, RACE_UPDATE, Response)
        self.assertTrue(response.over)
        self.assertEqual(response.winner, 1)
        self.assertEqual(driver.wait(timeout=10), 0, (self.path / "car-1.err").read_text())
        self.finish_server()

    def test_partial_request_disconnect(self):
        self.start_server()
        peer = self.register("First")
        self.initialize(peer)
        peer.sendall(bytes(Point(1, 1))[:3])
        peer.close()
        self.finish_server("All cars are out")

    def test_server_interrupt_while_waiting(self):
        self.start_server(cars=2)
        self.server.send_signal(signal.SIGINT)
        self.finish_server("Interrupted by user")

    def test_server_interrupt_with_blocked_handler(self):
        self.start_server()
        peer = self.register("First")
        self.initialize(peer)
        peer.sendall(bytes(Point(1, 1))[:3])
        self.server.send_signal(signal.SIGINT)
        receive_car_message(peer, RACE_STOP)
        self.assertEqual(peer.recv(1), b"")
        peer.close()
        self.finish_server("Interrupted by user")

    def test_stop_all_clients_before_track_initialization(self):
        self.start_server(cars=3)
        drivers = [self.start_car(i) for i in range(2)]
        for i in range(2):
            self.wait_for_output(f"car-{i}", "waiting for the track")

        viewers = []
        for i in range(2):
            name = f"viewer-{i}"
            viewers.append(self.spawn(name, "viewer", ["--port", str(self.port)]))
            self.wait_for_output(name, "LEADER BOARD")

        self.server.send_signal(signal.SIGINT)
        for i, driver in enumerate(drivers):
            self.assertEqual(driver.wait(timeout=3), 0, (self.path / f"car-{i}.err").read_text())
            self.assertIn("server stopped the race", (self.path / f"car-{i}.out").read_text())
            self.assertEqual((self.path / f"car-{i}.err").read_text(), "")
        for viewer in viewers:
            self.assertEqual(viewer.wait(timeout=3), 0)
        self.finish_server("Interrupted by user")

    def test_stop_car_while_waiting_for_start(self):
        self.start_server(map_only=True)
        driver = self.start_car(0)
        self.wait_for_output("car-0", "initialized at")
        self.server.send_signal(signal.SIGINT)
        self.assertEqual(driver.wait(timeout=3), 0, (self.path / "car-0.err").read_text())
        self.assertIn("server stopped the race", (self.path / "car-0.out").read_text())
        self.assertEqual((self.path / "car-0.err").read_text(), "")
        self.finish_server("Interrupted by user")

    def test_stop_before_registration_response(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", self.port))
            listener.listen(1)
            listener.settimeout(5)
            driver = self.start_car(0)
            peer, _ = listener.accept()
            self.peers.append(peer)
            peer.settimeout(5)
            self.assertEqual(receive(peer, c.c_int).value, 0)  # CLIENT_CAR
            receive(peer, Registration)
            peer.sendall(bytes(c.c_int(RACE_STOP)))
            peer.shutdown(socket.SHUT_WR)
            self.assertEqual(driver.wait(timeout=3), 0, (self.path / "car-0.err").read_text())
            self.assertIn("server stopped the race", (self.path / "car-0.out").read_text())
            self.assertEqual((self.path / "car-0.err").read_text(), "")

    def test_stop_all_clients_during_long_delay(self):
        for stop_signal in [signal.SIGINT, signal.SIGTERM]:
            with self.subTest(signal=stop_signal):
                self.start_server(cars=2, laps=100000, delay="300000:300000")
                drivers = [self.start_car(i) for i in range(2)]
                for i in range(2):
                    self.wait_for_output(f"car-{i}", "accepted")

                viewer = self.spawn("viewer", "viewer", ["--port", str(self.port)])
                self.wait_for_output("viewer", "LEADER BOARD")
                self.server.send_signal(stop_signal)

                for i, driver in enumerate(drivers):
                    self.assertEqual(driver.wait(timeout=3), 0, (self.path / f"car-{i}.err").read_text())
                    self.assertIn("server stopped the race", (self.path / f"car-{i}.out").read_text())
                    self.assertEqual((self.path / f"car-{i}.err").read_text(), "")
                self.assertEqual(viewer.wait(timeout=3), 0, (self.path / "viewer.err").read_text())
                self.finish_server("Interrupted by user")

    def test_stop_busy_cars_without_connection_errors(self):
        for round_number in range(5):
            with self.subTest(round=round_number):
                self.start_server(cars=3, laps=100000, delay="0:0")
                drivers = [self.start_car(i) for i in range(3)]
                for i in range(3):
                    self.wait_for_output(f"car-{i}", "accepted")
                self.server.send_signal(signal.SIGINT)
                for i, driver in enumerate(drivers):
                    self.assertEqual(driver.wait(timeout=3), 0, (self.path / f"car-{i}.err").read_text())
                    self.assertEqual((self.path / f"car-{i}.err").read_text(), "")
                    self.assertIn("server stopped the race", (self.path / f"car-{i}.out").read_text())
                self.finish_server("Interrupted by user")

    def test_startup_failure_stops_registered_car(self):
        self.start_server(cars=2)
        first = self.register("First")
        first.close()
        driver = self.start_car(1)
        self.assertEqual(driver.wait(timeout=5), 0, (self.path / "car-1.err").read_text())
        self.assertEqual((self.path / "car-1.err").read_text(), "")
        self.assertIn("server stopped the race", (self.path / "car-1.out").read_text())
        self.assertEqual(self.server.wait(timeout=5), 1)
        self.assertIn("Failed to start", self.report.read_text())

    def test_car_unexpected_server_disconnect_is_error(self):
        self.start_server(delay="300000:300000")
        driver = self.start_car(0)
        self.wait_for_output("car-0", "accepted")
        self.server.kill()
        self.server.wait(timeout=3)
        self.assertEqual(driver.wait(timeout=3), 1)
        self.assertNotEqual((self.path / "car-0.err").read_text(), "")

    def test_fragmented_car_move(self):
        self.start_server()
        peer = self.register("First")
        initial = self.initialize(peer)
        target = initial.map[initial.pos.y][initial.pos.x].road.forward
        payload = bytes(target)
        for offset in range(0, len(payload), 2):
            peer.sendall(payload[offset:offset + 2])
            time.sleep(0.01)
        response = receive_car_message(peer, RACE_UPDATE, Response)
        self.assertTrue(response.allowed)
        self.assertEqual(position(response.car.pos), position(target))
        peer.close()
        self.finish_server("All cars are out")

    def test_client_interrupt_during_long_delay(self):
        self.start_server(delay="300000:300000")
        driver = self.start_car(0)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if "accepted" in (self.path / "car-0.out").read_text():
                break
            self.assertIsNone(driver.poll())
            time.sleep(0.01)
        else:
            self.fail("Client did not make its first move")
        driver.send_signal(signal.SIGINT)
        self.assertEqual(driver.wait(timeout=3), 0)
        self.finish_server("All cars are out")

    def test_quick_restart_and_repeated_races(self):
        for round_number in range(5):
            self.start_server(cars=3, delay="0:0", name=f"round-{round_number}")
            drivers = [self.start_car(car_id) for car_id in [2, 0, 1]]
            for car_id, driver in zip([2, 0, 1], drivers):
                self.assertEqual(driver.wait(timeout=10), 0, (self.path / f"car-{car_id}.err").read_text())
            self.finish_server()

    def test_viewer_before_start_and_winner(self):
        self.start_server(cars=2)
        viewer = self.spawn("viewer", "viewer", ["--port", str(self.port)])
        self.wait_for_output("viewer", "LEADER BOARD")

        drivers = [self.start_car(i) for i in range(2)]
        for driver in drivers:
            self.assertEqual(driver.wait(timeout=10), 0)

        self.assertEqual(viewer.wait(timeout=5), 0, (self.path / "viewer.err").read_text())
        output = (self.path / "viewer.out").read_text()
        self.assertIn("LEADER BOARD", output)
        self.assertNotIn("RACE |", output)
        self.assertIn("Winner: Driver", output)
        self.assertIn("Driver0", output)
        self.assertIn("Driver1", output)
        self.assertNotIn("\x1b", output)  # В файл выводится обычный текст.
        self.finish_server()

    def test_viewer_snapshots_and_car_disconnect(self):
        self.start_server(cars=2, laps=10)
        viewer = self.register_viewer()
        initial_view = receive(viewer, ViewerInit)
        self.assertEqual((initial_view.width, initial_view.height, initial_view.expected, initial_view.laps),
                         (17, 11, 2, 10))
        self.assertEqual(receive(viewer, Snapshot).state, 0)  # WAITING

        first, second = self.register("First"), self.register("Second")
        initial = self.initialize(first)
        self.initialize(second)
        running = self.snapshot_until(viewer, lambda s: s.state == 1)
        self.assertEqual(running.count, 2)
        self.assertEqual(position(running.standings[0].car.pos), position(initial.pos))

        target = position(initial.map[initial.pos.y][initial.pos.x].road.forward)
        self.assertTrue(self.move(first, target).allowed)
        moved = self.snapshot_until(viewer, lambda s: position(s.standings[0].car.pos) == target)
        road = initial.map[target[1]][target[0]].road
        self.assertEqual((moved.standings[0].lane, moved.standings[0].distance), (road.lane, road.distance))
        self.assertNotEqual(position(moved.standings[0].car.pos), position(moved.standings[1].car.pos))

        first.close()
        self.snapshot_until(viewer, lambda s: s.standings[0].car.out)
        second.close()
        final = self.snapshot_until(viewer, lambda s: s.state == 2)
        self.assertEqual(final.winner, -1)
        self.assertTrue(all(final.standings[i].car.out for i in range(2)))
        self.finish_server("All cars are out")

    def test_viewer_connects_during_race(self):
        self.start_server()
        peer = self.register("First")
        self.initialize(peer)

        viewer = self.spawn("viewer", "viewer", ["--ip", "127.0.0.1", "--port", str(self.port)])
        self.wait_for_output("viewer", "LEADER BOARD")
        self.server.send_signal(signal.SIGINT)
        self.assertEqual(viewer.wait(timeout=5), 0, (self.path / "viewer.err").read_text())
        self.assertIn("Race ended without a winner", (self.path / "viewer.out").read_text())
        self.finish_server("Interrupted by user")

    def test_viewer_interrupt_does_not_stop_race(self):
        self.start_server()
        peer = self.register("First")
        initial = self.initialize(peer)
        viewer = self.spawn("viewer", "viewer", ["--port", str(self.port)])
        self.wait_for_output("viewer", "LEADER BOARD")

        viewer.send_signal(signal.SIGINT)
        self.assertEqual(viewer.wait(timeout=3), 0)
        self.assertIsNone(self.server.poll())
        target = position(initial.map[initial.pos.y][initial.pos.x].road.forward)
        self.assertTrue(self.move(peer, target).allowed)

        replacement = self.spawn("replacement", "viewer", ["--port", str(self.port)])
        self.wait_for_output("replacement", "LEADER BOARD")
        peer.close()
        self.assertEqual(replacement.wait(timeout=5), 0, (self.path / "replacement.err").read_text())
        self.finish_server("All cars are out")

    def test_multiple_viewers_receive_final_snapshot(self):
        self.start_server(draw_delay=60000)
        first = self.register_viewer()
        receive(first, ViewerInit)
        receive(first, Snapshot)

        second = self.register_viewer()
        receive(second, ViewerInit)
        receive(second, Snapshot)
        self.assertIsNone(self.server.poll())
        self.server.send_signal(signal.SIGINT)
        for viewer in [first, second]:
            self.snapshot_until(viewer, lambda s: s.state == 3)
        self.finish_server("Interrupted by user")

    def test_parallel_viewers_and_reconnect_during_race(self):
        self.start_server()
        car = self.register("First")
        initial = self.initialize(car)
        clients, errors = [], []
        barrier = threading.Barrier(8)

        def connect_viewer():
            try:
                barrier.wait(timeout=3)
                viewer = self.register_viewer()
                receive(viewer, ViewerInit)
                self.assertEqual(receive(viewer, Snapshot).state, 1)
                clients.append(viewer)
            except BaseException as error:
                errors.append(error)

        threads = [threading.Thread(target=connect_viewer) for _ in range(8)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(timeout=6)
            self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        self.assertEqual(len(clients), 8)

        clients.pop().close()
        target = position(initial.map[initial.pos.y][initial.pos.x].road.forward)
        self.assertTrue(self.move(car, target).allowed)
        for viewer in clients:
            self.snapshot_until(viewer, lambda s: position(s.standings[0].car.pos) == target)

        replacement = self.register_viewer()
        receive(replacement, ViewerInit)
        snapshot = receive(replacement, Snapshot)
        self.assertEqual(snapshot.state, 1)
        self.assertEqual(position(snapshot.standings[0].car.pos), target)
        clients.append(replacement)

        car.close()
        for viewer in clients:
            final = self.snapshot_until(viewer, lambda s: s.state == 2)
            self.assertTrue(final.standings[0].car.out)
        self.finish_server("All cars are out")

    def test_multiple_viewer_processes_receive_same_winner(self):
        self.start_server()
        viewers = []
        for index in range(3):
            name = f"viewer-{index}"
            viewers.append(self.spawn(name, "viewer", ["--port", str(self.port)]))
            self.wait_for_output(name, "LEADER BOARD")

        driver = self.start_car(0)
        self.assertEqual(driver.wait(timeout=5), 0)
        for index, viewer in enumerate(viewers):
            self.assertEqual(viewer.wait(timeout=5), 0, (self.path / f"viewer-{index}.err").read_text())
            self.assertIn("Winner: Driver0, team Test (car 0)", (self.path / f"viewer-{index}.out").read_text())
        self.finish_server()

    def test_disconnected_viewer_resources_are_reclaimed(self):
        self.start_server(draw_delay=60000)
        process_directory = Path(f"/proc/{self.server.pid}")
        initial_fds = len(list((process_directory / "fd").iterdir()))
        initial_threads = len(list((process_directory / "task").iterdir()))

        for _ in range(20):
            viewer = self.register_viewer()
            receive(viewer, ViewerInit)
            receive(viewer, Snapshot)
            viewer.close()

        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            fds = len(list((process_directory / "fd").iterdir()))
            threads = len(list((process_directory / "task").iterdir()))
            if fds <= initial_fds and threads <= initial_threads:
                break
            time.sleep(0.01)

        self.assertLessEqual(fds, initial_fds)
        self.assertLessEqual(threads, initial_threads)
        self.server.send_signal(signal.SIGINT)
        self.finish_server("Interrupted by user")

    def test_nonreading_viewer_does_not_block_race(self):
        self.start_server()
        peer = socket.socket()
        peer.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
        peer.settimeout(5)
        peer.connect(("127.0.0.1", self.port))
        self.peers.append(peer)
        peer.sendall(bytes(c.c_int(1)))
        self.assertTrue(receive(peer, c.c_bool).value)

        active = self.register_viewer()
        receive(active, ViewerInit)
        receive(active, Snapshot)

        driver = self.start_car(0)
        final = self.snapshot_until(active, lambda s: s.state == 2)
        self.assertEqual(final.winner, 0)
        self.assertEqual(driver.wait(timeout=5), 0, (self.path / "car-0.err").read_text())
        self.finish_server()

    def test_extra_car_is_rejected_during_race(self):
        self.start_server()
        first = self.register("First")
        self.initialize(first)
        extra = self.register("Extra", accepted=False)
        self.assertEqual(extra.recv(1), b"")
        first.close()
        self.finish_server("All cars are out")

    def test_control_preserves_full_car_state(self):
        self.start_server(laps=10)
        peer = self.register("First")
        initial = self.initialize(peer)
        state = initial.car
        for _ in range(200):
            if state.laps >= 1:
                break
            target = initial.map[state.pos.y][state.pos.x].road.forward
            response = self.move(peer, position(target))
            self.assertTrue(response.allowed)
            state = response.car
        self.assertEqual(state.laps, 1)

        self.control(0, 0)
        receive_car_message(peer, RACE_DISCONNECT)
        self.assertEqual(peer.recv(1), b"")
        peer.close()
        self.assertIsNone(self.server.poll())

        observer = self.register_viewer()
        receive(observer, ViewerInit)
        paused = self.snapshot_until(observer, lambda s: s.standings[0].car.paused)
        self.assertEqual(position(paused.standings[0].car.pos), position(state.pos))
        self.assertEqual(paused.standings[0].car.laps, 1)
        self.assertFalse(paused.standings[0].car.out)

        resumed = self.register("First", resume_id=0)
        self.assertEqual(select.select([resumed], [], [], 0.1)[0], [])
        self.control(1, 0)
        restored = self.initialize(resumed)
        self.assertEqual(position(restored.car.pos), position(state.pos))
        self.assertEqual(position(restored.car.prev), position(state.prev))
        self.assertEqual(restored.car.laps, state.laps)
        self.assertEqual(restored.car.passed, state.passed)
        self.assertFalse(restored.car.paused)
        self.assertFalse(self.move(resumed, position(state.prev)).allowed)

        target = restored.map[state.pos.y][state.pos.x].road.forward
        self.assertTrue(self.move(resumed, position(target)).allowed)
        resumed.close()
        self.finish_server("All cars are out")

    def test_control_utility_and_automatic_car_reconnect(self):
        self.start_server(laps=100000, delay="300000:300000")
        driver = self.start_car(0)
        self.wait_for_output("car-0", "accepted")

        disconnected = self.spawn("disconnect", "control", ["--port", str(self.port), "--disconnect", "0"])
        self.assertEqual(disconnected.wait(timeout=3), 0, (self.path / "disconnect.err").read_text())
        self.wait_for_output("server", "Car 0 reconnected; waiting for restore")
        self.assertIsNone(driver.poll())

        restored = self.spawn("restore", "control", ["--port", str(self.port), "--restore", "0"])
        self.assertEqual(restored.wait(timeout=3), 0, (self.path / "restore.err").read_text())
        self.wait_for_output("server", "Car 0 restored at")
        self.assertIsNone(driver.poll())
        self.server.send_signal(signal.SIGINT)
        self.assertEqual(driver.wait(timeout=3), 0, (self.path / "car-0.err").read_text())
        self.assertEqual((self.path / "car-0.err").read_text(), "")
        self.finish_server("Interrupted by user")

    def test_control_repeated_disconnect_restore(self):
        self.start_server(laps=100000)
        peer = self.register("First")
        initial = self.initialize(peer)
        baseline_threads = len(list(Path(f"/proc/{self.server.pid}/task").iterdir()))
        for _ in range(8):
            self.control(0, 0)
            receive_car_message(peer, RACE_DISCONNECT)
            peer.close()
            peer = self.register("First", resume_id=0)
            self.control(1, 0)
            resumed = self.initialize(peer)
            self.assertEqual(position(resumed.pos), position(initial.pos))
            self.assertEqual(resumed.car.laps, 0)
        self.assertEqual(len(list(Path(f"/proc/{self.server.pid}/task").iterdir())), baseline_threads)
        peer.close()
        self.finish_server("All cars are out")

    def test_control_restore_before_car_reconnects(self):
        self.start_server(laps=100000)
        peer = self.register("First")
        initial = self.initialize(peer)
        for _ in range(4):
            self.control(0, 0)
            self.control(1, 0)
            receive_car_message(peer, RACE_DISCONNECT)
            peer.close()
            peer = self.register("First", resume_id=0)
            restored = self.initialize(peer)
            self.assertEqual(position(restored.pos), position(initial.pos))
        peer.close()
        self.finish_server("All cars are out")

    def test_server_stop_during_car_reconnect(self):
        self.start_server()
        peer = self.register("First")
        self.initialize(peer)
        self.control(0, 0)
        receive_car_message(peer, RACE_DISCONNECT)
        peer.close()
        self.wait_for_output("server", "Car 0 disconnected; state saved")
        self.server.send_signal(signal.SIGINT)
        time.sleep(0.05)
        with socket.create_connection(("127.0.0.1", self.port), timeout=3) as reconnected:
            reconnected.sendall(bytes(c.c_int(0)))
            reconnected.sendall(bytes(Registration(b"First", Team(b"Test", 33), 0)))
            receive_car_message(reconnected, RACE_STOP)
        self.finish_server("Interrupted by user")

    def test_control_rejects_invalid_commands(self):
        self.start_server()
        self.control(0, 0, accepted=False)
        peer = self.register("First")
        self.initialize(peer)
        self.control(0, -1, accepted=False)
        self.control(0, 1, accepted=False)
        self.control(99, 0, accepted=False)
        self.control(1, 0, accepted=False)
        self.control(0, 0)
        self.control(0, 0, accepted=False)
        receive_car_message(peer, RACE_DISCONNECT)
        peer.close()
        self.register("WrongName", resume_id=0, accepted=False).close()
        resumed = self.register("First", resume_id=0)
        self.register("First", resume_id=0, accepted=False).close()
        self.control(1, 0)
        self.initialize(resumed)
        resumed.close()
        self.finish_server("All cars are out")

    def test_all_paused_cars_stop_with_server(self):
        self.start_server(cars=2, laps=100000, delay="300000:300000")
        drivers = [self.start_car(i) for i in range(2)]
        for i in range(2):
            self.wait_for_output(f"car-{i}", "accepted")
        for car_id in range(2):
            self.control(0, car_id)
            self.wait_for_output("server", f"Car {car_id} reconnected; waiting for restore")
        self.assertIsNone(self.server.poll())
        self.server.send_signal(signal.SIGTERM)
        for i, driver in enumerate(drivers):
            self.assertEqual(driver.wait(timeout=3), 0, (self.path / f"car-{i}.err").read_text())
            self.assertEqual((self.path / f"car-{i}.err").read_text(), "")
        self.finish_server("Interrupted by user")

    def test_winner_finishes_while_other_car_is_paused(self):
        self.start_server(cars=2)
        idle = self.register("Idle")
        racer = self.register("Racer")
        idle_initial = self.initialize(idle)
        initial = self.initialize(racer)
        inner = idle_initial.map[idle_initial.pos.y][idle_initial.pos.x].road.right
        self.assertTrue(self.move(idle, position(inner)).allowed)
        self.control(0, 0)
        receive_car_message(idle, RACE_DISCONNECT)
        idle.close()
        pending = self.register("Idle", resume_id=0)

        state = initial.car
        for _ in range(200):
            target = initial.map[state.pos.y][state.pos.x].road.forward
            response = self.move(racer, position(target))
            state = response.car
            if response.over:
                break
        self.assertTrue(response.over)
        receive_car_message(pending, RACE_STOP)
        pending.close()
        racer.close()
        self.finish_server()

    def test_viewer_fragmented_messages_and_invalid_snapshot(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", self.port))
            listener.listen(1)
            listener.settimeout(5)
            viewer = self.spawn("viewer", "viewer", ["--port", str(self.port)])
            peer, _ = listener.accept()
            self.peers.append(peer)
            peer.settimeout(5)
            self.assertEqual(receive(peer, c.c_int).value, 1)
            peer.sendall(bytes(c.c_bool(True)))

            initial = ViewerInit()
            initial.width = initial.height = initial.expected = initial.laps = 1
            payload = bytes(initial)
            for offset in range(0, len(payload), 777):
                peer.sendall(payload[offset:offset + 777])

            snapshot = Snapshot()
            snapshot.state, snapshot.count, snapshot.winner = 1, 1, -1
            snapshot.standings[0].car.name = b"Test"
            snapshot.standings[0].car.team = Team(b"Test", 33)
            payload = bytes(snapshot)
            for offset in range(0, len(payload), 3):
                peer.sendall(payload[offset:offset + 3])
            self.wait_for_output("viewer", "LEADER BOARD")

            snapshot.standings[0].car.pos.x = 1  # За пределами карты 1x1.
            peer.sendall(bytes(snapshot))
            self.assertEqual(viewer.wait(timeout=5), 1)
            self.assertIn("Invalid race snapshot", (self.path / "viewer.err").read_text())

    def test_viewer_unexpected_server_disconnect(self):
        self.start_server()
        viewer = self.spawn("viewer", "viewer", ["--port", str(self.port)])
        self.wait_for_output("viewer", "LEADER BOARD")
        self.server.kill()
        self.server.wait(timeout=3)
        self.assertEqual(viewer.wait(timeout=5), 1)
        self.assertIn("before the final race snapshot", (self.path / "viewer.err").read_text())

    def test_final_snapshot_wakes_long_draw_delay(self):
        self.start_server(draw_delay=60000)
        viewer = self.spawn("viewer", "viewer", ["--port", str(self.port)])
        self.wait_for_output("viewer", "LEADER BOARD")
        self.server.send_signal(signal.SIGINT)
        self.assertEqual(viewer.wait(timeout=3), 0)
        self.assertIn("Race ended without a winner", (self.path / "viewer.out").read_text())
        self.finish_server("Interrupted by user")

    def test_viewer_terminal_restores_cursor(self):
        self.start_server()
        master, slave = pty.openpty()
        self.addCleanup(os.close, master)
        errors = open(self.path / "viewer.err", "wb")
        self.files.append(errors)
        viewer = subprocess.Popen([str(BIN / "viewer"), "--port", str(self.port)],
                                  stdout=slave, stderr=errors)
        self.processes.append(viewer)
        os.close(slave)

        output = bytearray()
        deadline = time.monotonic() + 5
        interrupted = False
        while time.monotonic() < deadline:
            if not select.select([master], [], [], 0.1)[0]:
                continue
            try:
                output.extend(os.read(master, 65536))
            except OSError:  # Последний дескриптор slave закрыт.
                break

            if b"LEADER BOARD" in output and not interrupted:
                viewer.send_signal(signal.SIGINT)
                interrupted = True

        self.assertTrue(interrupted)
        self.assertEqual(viewer.wait(timeout=3), 0)
        self.assertIn(b"\x1b[?25l", output)
        self.assertIn(b"\x1b[?25h", output)
        self.assertEqual((self.path / "viewer.err").read_text(), "")
        self.server.send_signal(signal.SIGINT)
        self.finish_server("Interrupted by user")

    def test_viewer_rejects_invalid_map_dimensions(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", self.port))
            listener.listen(1)
            listener.settimeout(5)
            viewer = self.spawn("viewer", "viewer", ["--port", str(self.port)])
            peer, _ = listener.accept()
            self.peers.append(peer)
            self.assertEqual(receive(peer, c.c_int).value, 1)
            peer.sendall(bytes(c.c_bool(True)))
            initial = ViewerInit()
            initial.width = 51
            initial.height = initial.expected = initial.laps = 1
            peer.sendall(bytes(initial))
            self.assertEqual(viewer.wait(timeout=5), 1)
            self.assertIn("Invalid viewer initialization", (self.path / "viewer.err").read_text())


if __name__ == "__main__":
    unittest.main(verbosity=2)
