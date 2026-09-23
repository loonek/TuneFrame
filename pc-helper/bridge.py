import argparse
import asyncio
import io
import json
import queue
import socket
import sys
import threading
import time
import urllib.request

import serial as pyserial
import websockets
from PIL import Image

ART = 200
ART_CHUNK = 4096
NP_PERIOD = 0.3
THUMB = 96
FEED_MAX = 64
QUEUE_MAX = 50

_latest_np = None
_np_lock = threading.Lock()
_cmd_out = queue.Queue()
_feed_out = queue.Queue()
_stop = threading.Event()
_queue_out = queue.Queue()

# Saves latest "now playing" under lock
def set_latest_np(np):
    global _latest_np
    with _np_lock:
        _latest_np = np

# Loads latest -||-
def get_latest_np():
    with _np_lock:
        return _latest_np

# Removes "cover_url" from the np dict
def strip_cover(np):
    return {k: v for k, v in np.items() if k != "cover_url"}

# Converts image from the url to RGB565 format
def url_to_rgb565(url, size=ART):
    with urllib.request.urlopen(url, timeout=5) as r:
        data = r.read()
    img = Image.open(io.BytesIO(data)).convert("RGB").resize((size, size), Image.LANCZOS)
    px = img.tobytes()
    out = bytearray(size * size * 2)
    for i in range(size * size):
        r_, g_, b_ = px[3 * i], px[3 * i + 1], px[3 * i + 2]
        v = ((r_ & 0xF8) << 8) | ((g_ & 0xFC) << 3) | (b_ >> 3)
        out[2 * i] = v & 0xFF
        out[2 * i + 1] = (v >> 8) & 0xFF
    return bytes(out)

# Waits for cover data confirmation from the board
def wait_ok(ser, timeout=1.0):
    deadline = time.monotonic() + timeout
    buf = bytearray()
    while time.monotonic() < deadline:
        n = ser.in_waiting
        if n:
            buf.extend(ser.read(n))
            while b"\n" in buf:
                line, _, rest = buf.partition(b"\n")
                buf[:] = rest
                try:
                    if json.loads(line.decode("utf-8", "ignore")).get("t") == "ok":
                        return True
                except json.JSONDecodeError:
                    pass
        else:
            time.sleep(0.002)
    return False

# Sends one feed thumbnail addressed to a card index (RGB565 chunks + ack)
def send_feed_thumb(ser, idx, url):
    try:
        out = url_to_rgb565(url, THUMB)
    except Exception:
        return
    try:
        ser.write((json.dumps({"t": "ft", "i": idx, "w": THUMB, "h": THUMB}) + "\n").encode("utf-8"))
        for off in range(0, len(out), ART_CHUNK):
            ser.write(out[off:off + ART_CHUNK])
            if not wait_ok(ser):
                return
    except pyserial.SerialTimeoutException:
        return

# Streams feed to serial as fb/fs/fi/fe, then the thumbnails
def send_feed(ser, feed):
    try:
        ser.write(b'{"t":"fb"}\n')
        for sec in feed.get("sections", []):
            head = {"t": "fs", "title": sec.get("title", ""), "k": sec.get("kind", "")}
            ser.write((json.dumps(head, ensure_ascii=False) + "\n").encode("utf-8"))
            for it in sec.get("items", []):
                row = {
                    "t": "fi",
                    "title": it.get("title", ""),
                    "sub": it.get("sub", ""),
                    "id": it.get("id", ""),
                    "k": it.get("kind", ""),
                }
                ser.write((json.dumps(row, ensure_ascii=False) + "\n").encode("utf-8"))
        ser.write(b'{"t":"fe"}\n')
    except pyserial.SerialTimeoutException:
        return

    idx = 0
    for sec in feed.get("sections", []):
        for it in sec.get("items", []):
            if idx >= FEED_MAX:
                return
            url = it.get("thumb")
            if url:
                send_feed_thumb(ser, idx, url)
            idx += 1

# Streams the play queue to serial as qb/qi/qe
def send_queue(ser, q):
    try:
        ser.write(b'{"t":"qb"}\n')
        for i, it in enumerate(q.get("items", [])):
            if i >= QUEUE_MAX:
                break
            row = {
                "t": "qi",
                "title": it.get("title", ""),
                "sub": it.get("sub", ""),
                "cur": bool(it.get("cur", False)),
            }
            ser.write((json.dumps(row, ensure_ascii=False) + "\n").encode("utf-8"))
        ser.write(b'{"t":"qe"}\n')
    except pyserial.SerialTimeoutException:
        return

# Sends the cover: header + RGB565 pixels in chunks, waiting for ack
def send_cover(ser, url):
    try:
        out = url_to_rgb565(url)
    except Exception as e:
        print(f"[cover] fetch/convert failed: {e}", file=sys.stderr)
        return False
    try:
        ser.write((json.dumps({"t": "art", "w": ART, "h": ART}) + "\n").encode("utf-8"))
        for off in range(0, len(out), ART_CHUNK):
            ser.write(out[off:off + ART_CHUNK])
            if not wait_ok(ser):
                return False
        return True
    except pyserial.SerialTimeoutException:
        return False

# Serial-like adapter over TCP so the bridge can drive the PC simulator instead of a board
class TcpSerial:
    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.setblocking(False)
        self.buf = bytearray()

    def _pump(self):
        try:
            while True:
                data = self.sock.recv(65536)
                if not data:
                    break
                self.buf.extend(data)
        except BlockingIOError:
            pass
        except Exception:
            pass

    @property
    def in_waiting(self):
        self._pump()
        return len(self.buf)

    def read(self, n):
        self._pump()
        out = bytes(self.buf[:n])
        del self.buf[:n]
        return out

    def write(self, b):
        try:
            self.sock.sendall(b)
        except Exception:
            pass

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass

# Serial thread: queues incoming cmds, pushes np, streams feed, sends cover on track change
def serial_worker(port, baud, tcp):
    if tcp:
        ser = None
        for _ in range(60):
            try:
                ser = TcpSerial("127.0.0.1", 8766)
                break
            except Exception:
                time.sleep(1)
        if ser is None:
            print("[serial] cannot connect to sim on 127.0.0.1:8766", file=sys.stderr)
            return
        print("[serial] connected to sim on 127.0.0.1:8766", file=sys.stderr)
    else:
        ser = pyserial.Serial()
        ser.port = port
        ser.baudrate = baud
        ser.timeout = 0
        ser.write_timeout = 2
        ser.dtr = False
        ser.rts = False
        try:
            ser.open()
        except Exception as e:
            print(f"[serial] cannot open {port}: {e}", file=sys.stderr)
            return
        time.sleep(2)
        print(f"[serial] open on {port}", file=sys.stderr)

    rx = bytearray()
    last_push = 0.0
    last_art_title = None
    while not _stop.is_set():
        n = ser.in_waiting
        if n:
            rx.extend(ser.read(n))
            while b"\n" in rx:
                line, _, rest = rx.partition(b"\n")
                rx[:] = rest
                try:
                    obj = json.loads(line.decode("utf-8", "ignore"))
                    if obj.get("t") == "cmd":
                        _cmd_out.put(obj)
                except json.JSONDecodeError:
                    pass

        try:
            feed = _feed_out.get_nowait()
        except queue.Empty:
            feed = None
        if feed:
            send_feed(ser, feed)

        try:
            q = _queue_out.get_nowait()
        except queue.Empty:
            q = None
        if q:
            send_queue(ser, q)

        np = get_latest_np()
        now = time.monotonic()
        if np and now - last_push >= NP_PERIOD:
            try:
                ser.write((json.dumps(strip_cover(np), ensure_ascii=False) + "\n").encode("utf-8"))
            except pyserial.SerialTimeoutException:
                pass
            last_push = now

            title = np.get("title")
            url = np.get("cover_url")
            if np.get("status") != "none" and title and title != last_art_title and url:
                if send_cover(ser, url):
                    last_art_title = title

        time.sleep(0.02)

    ser.close()


_ws_clients = set()

# Connects with the browser extension
async def ws_handler(ws):
    _ws_clients.add(ws)
    print("[bridge] extension connected", file=sys.stderr)
    try:
        async for message in ws:
            try:
                msg = json.loads(message)
            except json.JSONDecodeError:
                continue
            if msg.get("t") == "np":
                set_latest_np(msg)
            elif msg.get("t") == "feed":
                _feed_out.put(msg)
            elif msg.get("t") == "queue":
                _queue_out.put(msg)
    finally:
        _ws_clients.discard(ws)
        print("[bridge] extension disconnected", file=sys.stderr)

# Sends esp commands to the browser extension
async def pump_commands():
    while True:
        try:
            cmd = _cmd_out.get_nowait()
        except queue.Empty:
            await asyncio.sleep(0.02)
            continue
        text = json.dumps(cmd)
        for ws in list(_ws_clients):
            try:
                await ws.send(text)
            except Exception:
                pass


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--tcp", action="store_true", help="drive the PC simulator over TCP instead of a serial port")
    args = parser.parse_args()

    threading.Thread(target=serial_worker, args=(args.port, args.baud, args.tcp), daemon=True).start()

    async with websockets.serve(ws_handler, "localhost", 8765):
        print(f"[bridge] ws://localhost:8765 <-> serial {args.port}", file=sys.stderr)
        await pump_commands()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        _stop.set()
