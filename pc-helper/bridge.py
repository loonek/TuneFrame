import argparse
import asyncio
import io
import json
import queue
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

_latest_np = None
_np_lock = threading.Lock()
_cmd_out = queue.Queue()
_stop = threading.Event()


def set_latest_np(np):
    global _latest_np
    with _np_lock:
        _latest_np = np


def get_latest_np():
    with _np_lock:
        return _latest_np


def strip_cover(np):
    return {k: v for k, v in np.items() if k != "cover_url"}


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


def serial_worker(port, baud):
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
    finally:
        _ws_clients.discard(ws)
        print("[bridge] extension disconnected", file=sys.stderr)


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
    args = parser.parse_args()

    threading.Thread(target=serial_worker, args=(args.port, args.baud), daemon=True).start()

    async with websockets.serve(ws_handler, "localhost", 8765):
        print(f"[bridge] ws://localhost:8765 <-> serial {args.port}", file=sys.stderr)
        await pump_commands()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        _stop.set()
