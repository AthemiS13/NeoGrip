import argparse
import collections
import json
import logging
import math
import select
import socket
import threading
import time
from urllib.parse import urlsplit, urlunsplit

import requests
import websocket

esp_to_pc_port = 9999
pc_to_esp_port = 8888
broadcast_ip = "255.255.255.255"
headers = {"X-ALVR": "true", "Content-Type": "application/json"}
HEARTBEAT_SECONDS = 1.0
DISCONNECT_SECONDS = 3.0
MAX_STATE_AGE = 0.10
MAX_PENDING_PER_HAND = 4
ALVR_SEND_INTERVAL = 0.02  # At most 50 HTTP requests/sec across both hands.
HAPTIC_SEGMENT_MS = 999  # Existing firmware wire format; not an effect duration cap.
HAPTIC_REFRESH_SECONDS = 0.25
STATE_REFRESH_SECONDS = 0.25
log = logging.getLogger("NeoGrip")


def decode_packet(packet, deadzone=0.0):
    """Validate before registering a controller; preserve the existing axis mapping."""
    if len(packet) != 15 or packet[0] not in ("L", "R"):
        raise ValueError("invalid controller ID or packet length")
    if any(packet[i] not in "01" for i in (1, 2, 3, 4, 5, 14)):
        raise ValueError("invalid button state")
    if any(c not in "0123456789" for c in packet[6:14]):
        raise ValueError("invalid joystick digits")
    raw_x, raw_y = int(packet[6:10]), int(packet[10:14])
    if raw_x > 4095 or raw_y > 4095:
        raise ValueError("joystick out of range")
    side = packet[0]
    hand = "/user/hand/left" if side == "L" else "/user/hand/right"
    btn1, btn2 = ("x", "y") if side == "L" else ("a", "b")
    x, y = (raw_x, raw_y) if side == "L" else (raw_y, raw_x)

    def axis(value):
        normalized = max(-1.0, min(1.0, (value - 2047) / 2047.0))
        if abs(normalized) <= deadzone:
            return 0.0
        return math.copysign((abs(normalized) - deadzone) / (1.0 - deadzone), normalized)

    data = [
        {"path": f"{hand}/input/trigger/value", "value": {"Scalar": float(packet[4])}},
        {"path": f"{hand}/input/squeeze/value", "value": {"Scalar": float(packet[5])}},
        {"path": f"{hand}/input/{btn1}/click", "value": {"Binary": packet[1] == "1"}},
        {"path": f"{hand}/input/{btn2}/click", "value": {"Binary": packet[2] == "1"}},
        {"path": f"{hand}/input/system/click", "value": {"Binary": packet[3] == "1"}},
        {"path": f"{hand}/input/thumbstick/x", "value": {"Scalar": axis(x)}},
        {"path": f"{hand}/input/thumbstick/y", "value": {"Scalar": axis(y)}},
        {"path": f"{hand}/input/thumbstick/click", "value": {"Binary": packet[14] == "1"}},
    ]
    buttons = packet[1:6] + packet[14]
    return side, buttons, data


class NeoGripProxy:
    def __init__(self, alvr_base="http://127.0.0.1:8082", bind_ip="", broadcast=broadcast_ip, deadzone=0.0):
        address = urlsplit(alvr_base)
        if address.scheme not in ("http", "https") or not address.hostname or address.path not in ("", "/") or address.query or address.fragment:
            raise ValueError("ALVR URL must be a base URL such as http://127.0.0.1:8082")
        if not 0 <= deadzone < 0.5:
            raise ValueError("deadzone must be at least 0 and less than 0.5")
        self.alvr_url = urlunsplit((address.scheme, address.netloc, "/api/set-buttons", "", ""))
        self.ws_url = urlunsplit(("wss" if address.scheme == "https" else "ws", address.netloc, "/api/events", "", ""))
        self.bind_ip = bind_ip
        self.broadcast_ip = broadcast
        self.deadzone = deadzone
        # Importing this module does not bind ports or start background threads.
        self.stop_event = threading.Event()
        self.state_changed = threading.Condition()
        self.send_lock = threading.Lock()
        self.haptic_effects = {}  # side -> (strength, end time, next renewal); send_lock protects it.
        self.controllers = {}  # side -> (IP, monotonic last-seen)
        self.known_ips = {}  # Retain recovery addresses when input briefly stops.
        self.latest = {}
        self.forwarded = {}
        self.pending = {side: collections.deque(maxlen=MAX_PENDING_PER_HAND) for side in ("L", "R")}
        self.sock_receive = None
        self.sock_send = None
        self.ws = None
        self.threads = []
        self.started = False

    def process_packet(self, packet, addr, now=None):
        side, buttons, data = decode_packet(packet, self.deadzone)
        now = time.monotonic() if now is None else now
        with self.state_changed:
            previous = self.controllers.get(side)
            self.controllers[side] = (addr[0], now)
            self.known_ips[side] = addr[0]
            if previous is None or previous[0] != addr[0]:
                log.info("Controller %s connected at %s", side, addr[0])
            self._queue_state(side, now, buttons, data)

    def _queue_state(self, side, now, buttons, data):
        # Caller holds state_changed. Coalesce analog-only updates, preserving
        # press/release edges unless an actual outage exhausts the bounded queue.
        pending = self.pending[side]
        self.latest[side] = (now, buttons, data)
        if pending and pending[-1][1] == buttons:
            pending[-1] = (now, buttons, data)
        else:
            pending.append((now, buttons, data))
        self.state_changed.notify()

    def _take_batch(self, now):
        # One state per hand per HTTP request; never combine opposite button edges.
        data = []
        for side, pending in self.pending.items():
            while pending and now - pending[0][0] > MAX_STATE_AGE:
                pending.popleft()
            state = pending.popleft() if pending else self.latest.get(side)
            if state is None:
                continue
            if side in self.controllers and now - state[0] > MAX_STATE_AGE:
                continue
            last = self.forwarded.get(side)
            if last and last[0] == state[2] and now - last[1] < STATE_REFRESH_SECONDS:
                continue
            data.extend(state[2])
        return data

    def expire_controllers(self, now):
        with self.state_changed:
            for side, (ip, last_seen) in list(self.controllers.items()):
                if now - last_seen < DISCONNECT_SECONDS:
                    continue
                del self.controllers[side]
                self.pending[side].clear()
                # Release controls in ALVR instead of leaving a lost trigger held.
                _, buttons, data = decode_packet(side + "00000204720470")
                self._queue_state(side, now, buttons, data)
                log.warning("Controller %s input timed out at %s (%.1fs without packets); recovery continues", side, ip, now - last_seen)

    def _mark_forwarded(self, data, now):
        with self.state_changed:
            for offset in range(0, len(data), 8):
                hand_data = data[offset:offset + 8]
                side = "L" if "/hand/left/" in hand_data[0]["path"] else "R"
                self.forwarded[side] = (hand_data, now)

    def send_datagram(self, payload, ip):
        with self.send_lock:
            if self.stop_event.is_set():
                return
            try:
                self.sock_send.sendto(payload, (ip, pc_to_esp_port))
            except OSError as error:
                log.warning("UDP send to %s failed: %s", ip, error)

    def heartbeat_once(self, now=None):
        now = time.monotonic() if now is None else now
        self.expire_controllers(now)
        with self.state_changed:
            ips = set(self.known_ips.values())
            # Acknowledge only genuinely recent input, not a remembered IP.
            # This lets a controller detect a broken controller -> PC path
            # even while START packets still arrive in the opposite direction.
            acknowledgements = [
                (side, ip) for side, (ip, last_seen) in self.controllers.items()
                if now - last_seen < HEARTBEAT_SECONDS * 1.5
            ]
        # Never stop discovery after two controllers connect: either may reboot.
        # Unicast heartbeats also avoid reliance on Wi-Fi broadcast delivery.
        self.send_datagram(b"START", self.broadcast_ip)
        self.send_datagram(b"ACK?", self.broadcast_ip)
        for ip in ips:
            self.send_datagram(b"START", ip)
            self.send_datagram(b"ACK?", ip)
        for side, ip in acknowledgements:
            self.send_datagram(b"ACK" + side.encode("ascii"), ip)

    def send_startup_signal(self):
        while not self.stop_event.is_set():
            self.heartbeat_once()
            self.stop_event.wait(HEARTBEAT_SECONDS)

    def send_to_alvr(self):
        with requests.Session() as session:
            session.trust_env = False
            last_error_at = float("-inf")
            last_request_at = float("-inf")
            while not self.stop_event.is_set():
                if self.stop_event.wait(max(0, last_request_at + ALVR_SEND_INTERVAL - time.monotonic())):
                    break
                with self.state_changed:
                    if not any(self.pending.values()):
                        self.state_changed.wait(timeout=ALVR_SEND_INTERVAL)
                    if self.stop_event.is_set():
                        break
                    data = self._take_batch(time.monotonic())
                if not data:
                    continue
                try:
                    last_request_at = time.monotonic()
                    response = session.post(self.alvr_url, headers=headers, json=data, timeout=(0.1, 0.1))
                    response.raise_for_status()
                    self._mark_forwarded(data, time.monotonic())
                except requests.RequestException as error:
                    # Bound both memory and data age if ALVR slows/stops. Subsequent
                    # controller packets carry fresh state rather than a stale FIFO.
                    now = time.monotonic()
                    if now - last_error_at >= 2:
                        log.warning("ALVR request failed: %s", error)
                        last_error_at = now

    def _haptic_ips(self):
        now = time.monotonic()
        with self.state_changed:
            return {
                side: info[0] if (info := self.controllers.get(side)) and now - info[1] < DISCONNECT_SECONDS else self.broadcast_ip
                for side in ("L", "R")
            }

    def _send_haptic_locked(self, side, strength, duration_ms, ip):
        # Caller holds send_lock. Every packet has its own firmware-enforced end.
        payload = f"{side}{strength:03d}{duration_ms:03d}".encode("ascii")
        try:
            self.sock_send.sendto(payload, (ip, pc_to_esp_port))
        except OSError as error:
            log.warning("UDP send to %s failed: %s", ip, error)
        log.debug("Haptic %s -> %s", payload, ip)

    def send_to_esp32(self, controller_id, strength, duration_ms):
        if controller_id not in ("L", "R"):
            return
        strength = max(0, min(255, int(strength)))
        duration_ms = max(0, int(math.ceil(duration_ms)))
        # A zero-strength or zero-duration event means stop; do not suppress it.
        if strength == 0 or duration_ms == 0:
            strength = duration_ms = 0
        ips = self._haptic_ips()
        with self.send_lock:
            if self.stop_event.is_set():
                return
            now = time.monotonic()
            # New events replace the previous effect immediately, including its
            # renewal schedule. Never queue old effects behind a long vibration.
            self.haptic_effects.pop(controller_id, None)
            if duration_ms > HAPTIC_SEGMENT_MS:
                self.haptic_effects[controller_id] = (
                    strength, now + duration_ms / 1000.0, now + HAPTIC_REFRESH_SECONDS
                )
            self._send_haptic_locked(controller_id, strength, min(duration_ms, HAPTIC_SEGMENT_MS), ips[controller_id])

    def renew_haptics(self):
        ips = self._haptic_ips()
        with self.send_lock:
            if self.stop_event.is_set():
                return
            now = time.monotonic()
            for side, (strength, end_at, next_at) in list(self.haptic_effects.items()):
                if now < next_at:
                    continue
                remaining_ms = max(0, math.ceil((end_at - now) * 1000))
                if remaining_ms == 0:
                    del self.haptic_effects[side]
                    self._send_haptic_locked(side, 0, 0, ips[side])
                else:
                    self._send_haptic_locked(side, strength, min(remaining_ms, HAPTIC_SEGMENT_MS), ips[side])
                    self.haptic_effects[side] = (strength, end_at, now + HAPTIC_REFRESH_SECONDS)

    def maintain_haptics(self):
        # Initial events are sent immediately by the WebSocket callback. This
        # worker only renews effects that outlast a single transport segment.
        while not self.stop_event.wait(HAPTIC_REFRESH_SECONDS):
            self.renew_haptics()

    def stop_haptics(self):
        ips = self._haptic_ips()
        with self.send_lock:
            self.haptic_effects.clear()
            if not self.stop_event.is_set():
                for side in ("L", "R"):
                    self._send_haptic_locked(side, 0, 0, ips[side])

    def on_message(self, ws, message):
        try:
            data = json.loads(message)
            event_data = data.get("event_type", {}).get("data", {})
            path = event_data.get("path", "")
            side = "R" if path.startswith("/user/hand/right/") else "L" if path.startswith("/user/hand/left/") else None
            duration = event_data.get("duration")
            amplitude = event_data.get("amplitude")
            if side is None or not isinstance(duration, dict) or amplitude is None:
                return
            duration_ms = float(duration.get("secs", 0)) * 1000 + float(duration.get("nanos", 0)) / 1e6
            amplitude = float(amplitude)
            if not math.isfinite(amplitude) or not math.isfinite(duration_ms):
                return
            # Positive sub-millisecond/sub-step events must not round down to stop.
            strength = math.ceil(max(0.0, min(1.0, amplitude)) * 255)
            self.send_to_esp32(side, strength, duration_ms)
        except (ValueError, TypeError, AttributeError) as error:
            log.debug("Ignored malformed WebSocket event: %s", error)

    def start_websocket(self):
        while not self.stop_event.is_set():
            try:
                self.ws = websocket.WebSocketApp(self.ws_url, on_message=self.on_message)
                self.ws.run_forever(ping_interval=0)
            except Exception as error:
                log.warning("WebSocket failed: %s", error)
            # ALVR went away: discard renewals before reconnecting. Old effects
            # cannot survive a lost WebSocket or restart after reconnection.
            self.stop_haptics()
            self.stop_event.wait(2)

    def run(self):
        self.sock_receive = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock_send = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            self.sock_receive.settimeout(0.5)
            self.sock_receive.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16384)
            self.sock_receive.bind((self.bind_ip, esp_to_pc_port))
            self.sock_send.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self.sock_send.setblocking(False)
            self.started = True
            for target in (self.send_to_alvr, self.start_websocket, self.send_startup_signal, self.maintain_haptics):
                thread = threading.Thread(target=target, daemon=True)
                self.threads.append(thread)
                thread.start()
            log.info("Press system on each controller. Menu + trigger for 7 seconds restarts that controller.")
            while not self.stop_event.is_set():
                try:
                    data, addr = self.sock_receive.recvfrom(1024)
                    # If the receiver was paused, discard buffered intermediate
                    # snapshots and catch up to the newest packet from each hand.
                    newest = {}
                    for _ in range(256):
                        try:
                            packet = data.decode("ascii")
                            decode_packet(packet)
                            newest[packet[0]] = (packet, addr)
                        except (ValueError, UnicodeError):
                            pass
                        if not select.select([self.sock_receive], [], [], 0)[0]:
                            break
                        data, addr = self.sock_receive.recvfrom(1024)
                    else:
                        # Include the datagram fetched at the end of the batch.
                        try:
                            packet = data.decode("ascii")
                            decode_packet(packet)
                            newest[packet[0]] = (packet, addr)
                        except (ValueError, UnicodeError):
                            pass
                    for packet, address in newest.values():
                        self.process_packet(packet, address)
                except socket.timeout:
                    pass
                except (ValueError, UnicodeError) as error:
                    log.debug("Ignored invalid controller packet: %s", error)
        except KeyboardInterrupt:
            log.info("Putting NeoGrip to sleep...")
        finally:
            self.shutdown()

    def shutdown(self):
        self.stop_event.set()
        with self.state_changed:
            ips = set(self.known_ips.values())
            self.state_changed.notify_all()
        # Serialize STOP against an in-flight START: nothing can wake a board
        # between STOP repeats during shutdown.
        try:
            with self.send_lock:
                self.haptic_effects.clear()
                if self.started and self.sock_send is not None:
                    for repeat in range(5):
                        for ip in ips | {self.broadcast_ip}:
                            payloads = (b"L000000", b"R000000", b"STOP") if repeat == 0 else (b"STOP",)
                            for payload in payloads:
                                try:
                                    self.sock_send.sendto(payload, (ip, pc_to_esp_port))
                                except OSError:
                                    pass
                        time.sleep(0.05)
        except KeyboardInterrupt:
            pass
        finally:
            self.started = False
            if self.ws is not None:
                self.ws.close()
            for thread in self.threads:
                thread.join(timeout=1)
            for sock in (self.sock_receive, self.sock_send):
                if sock is not None:
                    sock.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NeoGrip controller bridge with automatic reconnection.")
    parser.add_argument("--alvr-url", default="http://127.0.0.1:8082", help="ALVR base URL")
    parser.add_argument("--bind", default="", help="PC interface address to receive on (default: all)")
    parser.add_argument("--broadcast", default=broadcast_ip, help="Discovery broadcast address for your LAN")
    parser.add_argument("--deadzone", type=float, default=0.0, help="Optional joystick deadzone, e.g. 0.08; default 0")
    parser.add_argument("--debug", action="store_true", help="Log haptic packets and invalid input")
    args = parser.parse_args()
    logging.basicConfig(level=logging.DEBUG if args.debug else logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s", datefmt="%H:%M:%S")
    try:
        app = NeoGripProxy(args.alvr_url, args.bind, args.broadcast, args.deadzone)
    except ValueError as error:
        parser.error(str(error))
    app.run()
