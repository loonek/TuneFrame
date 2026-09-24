import argparse
import asyncio
import collections
import io
import json
import os
import queue
import socket
import sys
import threading
import time
import urllib.request

try:
    import winreg  # Windows only, for the "start on launch" toggle
except ImportError:
    winreg = None

import serial as pyserial
from serial.tools import list_ports
import websockets
import pystray
from PIL import Image, ImageDraw

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
_playlist_out = queue.Queue()
_log = collections.deque(maxlen=200)   # recent events for the diagnostics window
_board_connected = False
_loop = None                            # asyncio loop, for cross-thread shutdown
_reconnect = threading.Event()          # signals serial_worker to re-open the board link
_port = "auto"                          # "auto" = detect the ESP by USB VID, else a COM name

ESP_VID = 0x303A                        # Espressif USB vendor id (native USB CDC)

# Appends a timestamped line to the diagnostics log and, if present, stderr
def log(msg):
    _log.append(time.strftime("%H:%M:%S") + " " + msg)
    if sys.stderr:                       # None in a windowed (no-console) exe
        try:
            print("[bridge] " + msg, file=sys.stderr)
        except Exception:
            pass

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

# Steams a playlist's tracks to serial as pb/pi/pe
def send_playlist(ser, pl):
    try:
        head = {"t": "pb", "title": pl.get("title", "")}
        ser.write((json.dumps(head, ensure_ascii=False) + "\n").encode("utf-8"))
        for it in pl.get("items", []):
            row = {"t": "pi", "title": it.get("title", ""), "sub": it.get("sub", "")}
            ser.write((json.dumps(row, ensure_ascii=False) + "\n").encode("utf-8"))
        ser.write(b'{"t":"pe"}\n')
    except pyserial.SerialTimeoutException:
        return

# Sends the cover: header + RGB565 pixels in chunks, waiting for ack
def send_cover(ser, url):
    try:
        out = url_to_rgb565(url)
    except Exception as e:
        log(f"cover fetch/convert failed: {e}")
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

# Finds the ESP's serial port by USB vendor id, or None
def detect_port():
    for p in list_ports.comports():
        if getattr(p, "vid", None) == ESP_VID:
            return p.device
    return None

# Names of all serial ports currently present
def list_serial_ports():
    return [p.device for p in list_ports.comports()]

# Opens the board link (auto-detecting the ESP port), or returns None so the caller retries
def open_link(baud, tcp):
    if tcp:
        try:
            s = TcpSerial("127.0.0.1", 8766)
            log("board connected (sim tcp 127.0.0.1:8766)")
            return s
        except Exception:
            return None
    port = _port
    if port == "auto":
        port = detect_port()
        if not port:
            return None
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
        log(f"cannot open {port}: {e}")
        return None
    time.sleep(2)
    log(f"board connected on {port}")
    return ser

# Serial thread: (re)opens the board link and relays np/feed/cover/commands
def serial_worker(baud, tcp):
    global _board_connected
    while not _stop.is_set():
        ser = open_link(baud, tcp)
        if ser is None:
            time.sleep(2)          # board not ready; keep retrying
            continue

        _board_connected = True
        _reconnect.clear()
        rx = bytearray()
        last_push = 0.0
        last_art_title = None
        while not _stop.is_set() and not _reconnect.is_set():
            try:
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
                                log("cmd from board: " + str(obj.get("a", "")))
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

                try:
                    pl = _playlist_out.get_nowait()
                except queue.Empty:
                    pl = None
                if pl:
                    send_playlist(ser, pl)

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
            except (pyserial.SerialException, OSError) as e:
                log(f"serial error: {e}")
                break

            time.sleep(0.02)

        _board_connected = False
        try:
            ser.close()
        except Exception:
            pass
        log("reconnecting board link" if _reconnect.is_set() else "board disconnected")


_ws_clients = set()

# Connects with the browser extension
async def ws_handler(ws):
    try:
        origin = ws.request.headers.get("Origin", "")      # websockets >= 13
    except AttributeError:
        origin = ws.request_headers.get("Origin", "")       # older API
    if origin.startswith("http://") or origin.startswith("https://"):
        await ws.close()   # reject real web pages; the extension has no http(s) origin
        return

    _ws_clients.add(ws)
    log("extension connected")
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
            elif msg.get("t") == "playlist":
                _playlist_out.put(msg)
    finally:
        _ws_clients.discard(ws)
        log("extension disconnected")

# Sends esp commands to the browser extension
async def pump_commands():
    while not _stop.is_set():
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


# Serves the WebSocket and pumps commands until shutdown
async def async_main(args):
    global _loop
    _loop = asyncio.get_running_loop()
    async with websockets.serve(ws_handler, "localhost", 8765):
        log("ws://localhost:8765 <-> " + ("sim (tcp)" if args.tcp else "serial " + args.port))
        await pump_commands()

# Runs the asyncio server on its own thread so the tray owns the main thread
def run_async(args):
    try:
        asyncio.run(async_main(args))
    except (KeyboardInterrupt, RuntimeError):
        pass
    except OSError as e:
        log(f"server error (is another bridge running?): {e}")

# Resolves a bundled resource path (works in dev and in the PyInstaller exe)
def resource_path(name):
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, name)

# Loads the tray icon (the app logo), falling back to a plain dot
def make_icon_image():
    try:
        return Image.open(resource_path("tuneframe.png")).convert("RGBA")
    except Exception:
        img = Image.new("RGB", (64, 64), (15, 15, 15))
        d = ImageDraw.Draw(img)
        d.ellipse((16, 16, 48, 48), fill=(255, 0, 0))
        return img

_RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
_APP_NAME = "TuneFrame"

# The command Windows should run at login (the exe when frozen, else python + this script)
def _autostart_command():
    if getattr(sys, "frozen", False):
        return f'"{sys.executable}"'
    return f'"{sys.executable}" "{os.path.abspath(__file__)}"'

# Whether the login autostart entry is present
def is_autostart_enabled():
    if not winreg:
        return False
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, _RUN_KEY) as k:
            winreg.QueryValueEx(k, _APP_NAME)
        return True
    except OSError:
        return False

# Adds or removes the login autostart entry (the app opens straight to the tray)
def set_autostart(enable):
    if not winreg:
        return
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, _RUN_KEY, 0, winreg.KEY_SET_VALUE) as k:
            if enable:
                winreg.SetValueEx(k, _APP_NAME, 0, winreg.REG_SZ, _autostart_command())
            else:
                try:
                    winreg.DeleteValue(k, _APP_NAME)
                except OSError:
                    pass
        log("start on launch: " + ("on" if enable else "off"))
    except OSError as e:
        log(f"autostart error: {e}")

# Menu action: toggle start on launch
def on_toggle_autostart(icon=None, item=None):
    set_autostart(not is_autostart_enabled())

# Menu action: re-open the board link and drop WS clients so the extension reconnects
def on_reconnect(icon=None, item=None):
    log("reconnect requested")
    _reconnect.set()
    if _loop:
        _loop.call_soon_threadsafe(_close_ws_clients)

# Closes all extension WebSocket connections (runs on the asyncio loop thread)
def _close_ws_clients():
    for ws in list(_ws_clients):
        asyncio.create_task(ws.close())

# Builds the Port submenu: Auto plus each present serial port
def port_menu():
    def choose(value):
        def handler(icon=None, item=None):
            global _port
            _port = value
            log(f"port set to {value}")
            _reconnect.set()
        return handler
    items = [pystray.MenuItem("Auto", choose("auto"), checked=lambda i: _port == "auto", radio=True)]
    for p in list_serial_ports():
        items.append(pystray.MenuItem(p, choose(p), checked=lambda i, p=p: _port == p, radio=True))
    return pystray.Menu(*items)

# Shared with the tray-thread menu callbacks
_root = None
_diag = None
_tray_icon = None

# Menu action: show the diagnostics window (scheduled onto the Tk main thread)
def on_diagnostics(icon=None, item=None):
    if _root:
        _root.after(0, lambda: (_diag.deiconify(), _diag.lift()))

# Menu action: quit everything (scheduled onto the Tk main thread)
def on_quit(icon=None, item=None):
    if _root:
        _root.after(0, _do_quit)

# Tears down the loop, tray and Tk (main thread only)
def _do_quit():
    _stop.set()
    if _loop:
        _loop.call_soon_threadsafe(_loop.stop)
    if _tray_icon:
        _tray_icon.stop()
    if _root:
        _root.quit()

# Builds the hidden diagnostics window, refreshed every 500 ms while visible
def build_diag(root):
    import tkinter as tk
    win = tk.Toplevel(root)
    win.title("JC bridge diagnostics")
    win.configure(bg="#111111")
    win.protocol("WM_DELETE_WINDOW", win.withdraw)   # hide, keep it reusable
    win.withdraw()

    status = tk.Label(win, fg="white", bg="#111111", justify="left", font=("Consolas", 10))
    status.pack(anchor="w", padx=8, pady=6)
    txt = tk.Text(win, width=72, height=18, bg="#1a1a1a", fg="#dddddd",
                  insertbackground="white", font=("Consolas", 9))
    txt.pack(padx=8, pady=(0, 8))

    def tick():
        if win.state() == "normal":
            ext = "yes" if _ws_clients else "no"
            board = "yes" if _board_connected else "no"
            np = get_latest_np()
            now = ""
            if np and np.get("status") != "none":
                now = (np.get("title") or "") + " - " + (np.get("artist") or "")
            status.config(text=f"Extension: {ext}    Board: {board}\nNow playing: {now}")
            txt.delete("1.0", "end")
            txt.insert("end", "\n".join(_log))
            txt.see("end")
        root.after(500, tick)

    tick()
    return win


def main():
    global _root, _diag, _tray_icon, _port
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="auto", help="serial port, or 'auto' to detect the ESP by USB id")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--tcp", action="store_true", help="drive the PC simulator over TCP instead of a serial port")
    parser.add_argument("--no-tray", action="store_true", help="run headless, without the tray icon")
    args = parser.parse_args()
    _port = args.port

    threading.Thread(target=serial_worker, args=(args.baud, args.tcp), daemon=True).start()
    threading.Thread(target=run_async, args=(args,), daemon=True).start()

    if args.no_tray:
        try:
            while not _stop.is_set():
                time.sleep(0.5)
        except KeyboardInterrupt:
            _stop.set()
        return

    import tkinter as tk

    _root = tk.Tk()
    _root.withdraw()                 # Tk owns the main thread; only the Toplevel is shown
    _diag = build_diag(_root)

    menu = pystray.Menu(
        pystray.MenuItem("Reconnect", on_reconnect),
        pystray.MenuItem("Port", port_menu()),
        pystray.MenuItem("Diagnostics", on_diagnostics),
        pystray.MenuItem("Run on startup", on_toggle_autostart, checked=lambda i: is_autostart_enabled()),
        pystray.MenuItem("Quit", on_quit),
    )
    _tray_icon = pystray.Icon("tuneframe", make_icon_image(), "TuneFrame", menu)
    _tray_icon.run_detached()        # tray runs on its own thread
    try:
        _root.mainloop()             # blocks on the main thread until _do_quit
    except KeyboardInterrupt:
        _do_quit()


if __name__ == "__main__":
    main()
