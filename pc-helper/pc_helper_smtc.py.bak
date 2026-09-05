"""
PC-side helper (the "brain"). Talks to the ESP32 over serial, both ways:
  - PUSH: now-playing (from Windows SMTC) as JSON lines, and album art as a
          header line + raw RGB565 bytes whenever the track changes.
  - RECV: {"t":"cmd","a":...} lines from the board -> SMTC transport controls.

Usage:
  python pc_helper.py                  # dry run: print now-playing to stdout
  python pc_helper.py --port COM4      # bidirectional link to the board
"""

import argparse
import asyncio
import io
import json
import sys
import time

from winsdk.windows.media.control import (
    GlobalSystemMediaTransportControlsSessionManager as MediaManager,
    GlobalSystemMediaTransportControlsSessionPlaybackStatus as PlaybackStatus,
)
from winsdk.windows.storage.streams import DataReader
from pycaw.pycaw import AudioUtilities
from PIL import Image

STATUS_MAP = {
    PlaybackStatus.PLAYING: "playing",
    PlaybackStatus.PAUSED: "paused",
    PlaybackStatus.STOPPED: "stopped",
}

ART = 200        # cover size sent to the board (must be <= ART_MAX on the ESP32)
SETTLE_S = 1.0   # after a skip, pause updates so we don't catch transitional data
VOL_STEP = 0.10  # per-app volume change per button press (0.10 = 10%)
ART_CHUNK = 4096  # album-art chunk size; must match ART_CHUNK on the ESP32


def secs(td):
    return int(td.total_seconds()) if td is not None else 0


async def read_now_playing(mgr):
    session = mgr.get_current_session()
    if session is None:
        return {"t": "np", "status": "none"}
    info = await session.try_get_media_properties_async()
    playback = session.get_playback_info()
    timeline = session.get_timeline_properties()
    return {
        "t": "np",
        "title": info.title or "",
        "artist": info.artist or "",
        "status": STATUS_MAP.get(playback.playback_status, "stopped"),
        "pos": secs(timeline.position),
        "dur": secs(timeline.end_time),
        "vol": get_app_volume(session.source_app_user_model_id),  # 0-100 or null
    }


async def _read_cover_async(size):
    """Fetch the SMTC thumbnail, resize, and return RGB565 LE bytes.
    Creates its own session manager so it can run on a worker thread's loop."""
    mgr = await MediaManager.request_async()
    session = mgr.get_current_session()
    if session is None:
        return None
    info = await session.try_get_media_properties_async()
    ref = info.thumbnail
    if ref is None:
        return None

    stream = await ref.open_read_async()
    reader = DataReader(stream)
    await reader.load_async(stream.size)
    raw = bytearray(stream.size)
    reader.read_bytes(raw)

    img = Image.open(io.BytesIO(bytes(raw))).convert("RGB").resize((size, size), Image.LANCZOS)
    px = img.tobytes()
    out = bytearray(size * size * 2)
    for i in range(size * size):
        r = px[3 * i]; g = px[3 * i + 1]; b = px[3 * i + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[2 * i] = v & 0xFF
        out[2 * i + 1] = (v >> 8) & 0xFF
    return bytes(out)


def read_cover_threaded(size=ART):
    """Run the whole winsdk fetch on its OWN event loop in a worker thread.
    The main helper loop's serial I/O disrupts winrt await callbacks, so we
    isolate the fetch here; call via asyncio.to_thread(...) + wait_for."""
    return asyncio.run(_read_cover_async(size))


def find_music_session(aumid):
    """The per-process audio session of the app that owns the media session.

    Matches the SMTC app id to a session; falls back to whatever is actively
    playing. App-agnostic (no hard-coded process name), so only the music app's
    audio is ever touched.
    """
    aumid_l = (aumid or "").lower()
    active = None
    for sess in AudioUtilities.GetAllSessions():
        proc = sess.Process
        if proc is None:
            continue
        if sess.State == 1 and active is None:   # 1 = AudioSessionStateActive
            active = sess
        base = proc.name().lower().rsplit(".exe", 1)[0]   # "opera.exe" -> "opera"
        if base and base in aumid_l:
            return sess
    return active                                # fallback: whatever is playing


def get_app_volume(aumid):
    """Current volume of the music app as 0-100, or None if unknown."""
    sess = find_music_session(aumid)
    if sess is None:
        return None
    return int(round(sess.SimpleAudioVolume.GetMasterVolume() * 100))


def adjust_app_volume(aumid, delta):
    """Change ONLY the music app's volume by delta (0.10 = +10%)."""
    sess = find_music_session(aumid)
    if sess is None:
        return
    sv = sess.SimpleAudioVolume
    sv.SetMasterVolume(max(0.0, min(1.0, sv.GetMasterVolume() + delta)), None)


async def do_command(mgr, action):
    session = mgr.get_current_session()
    if session is None:
        return
    if action == "playpause":
        await session.try_toggle_play_pause_async()
    elif action == "next":
        await session.try_skip_next_async()
    elif action == "prev":
        await session.try_skip_previous_async()
    elif action in ("vol_up", "vol_down"):
        step = VOL_STEP if action == "vol_up" else -VOL_STEP
        adjust_app_volume(session.source_app_user_model_id, step)


def make_io(port, baud):
    """Return (send, send_bytes, read_cmds, close)."""
    if not port:
        def send(obj):
            sys.stdout.write(json.dumps(obj, ensure_ascii=False) + "\n")
            sys.stdout.flush()
        def send_art(w, h, data):
            print(f"[dry run] would send {len(data)} art bytes ({w}x{h})", file=sys.stderr)
            return True
        return send, send_art, (lambda: []), (lambda: None)

    import serial  # pyserial
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0
    ser.write_timeout = 2    # never block forever if the board stops draining
    ser.dtr = False
    ser.rts = False
    ser.open()
    time.sleep(2)

    def send(obj):
        ser.write((json.dumps(obj, ensure_ascii=False) + "\n").encode("utf-8"))

    def wait_ok(timeout=1.0):
        """Read serial until the board sends {"t":"ok"} (chunk received)."""
        deadline = time.monotonic() + timeout
        acc = bytearray()
        while time.monotonic() < deadline:
            n = ser.in_waiting
            if n:
                acc.extend(ser.read(n))
                while b"\n" in acc:
                    line, _, rest = acc.partition(b"\n")
                    acc[:] = rest
                    try:
                        if json.loads(line.decode("utf-8", "ignore")).get("t") == "ok":
                            return True
                    except json.JSONDecodeError:
                        pass
            else:
                time.sleep(0.001)
        return False

    def send_art(w, h, data):
        """Send the cover in ACK-paced chunks so the board's RX never overflows."""
        try:
            send({"t": "art", "w": w, "h": h})
            for off in range(0, len(data), ART_CHUNK):
                ser.write(data[off:off + ART_CHUNK])
                if not wait_ok():
                    return False
            return True
        except serial.SerialTimeoutException:
            return False

    rx = bytearray()
    def read_cmds():
        actions = []
        n = ser.in_waiting
        if n:
            rx.extend(ser.read(n))
            while b"\n" in rx:
                line, _, rest = rx.partition(b"\n")
                rx[:] = rest
                try:
                    obj = json.loads(line.decode("utf-8", "ignore"))
                    if obj.get("t") == "cmd":
                        actions.append(obj.get("a"))
                except json.JSONDecodeError:
                    pass
        return actions

    return send, send_art, read_cmds, ser.close


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", help="serial port, e.g. COM4 (omit for stdout dry run)")
    parser.add_argument("--baud", type=int, default=921600)  # ignored by native USB CDC
    parser.add_argument("--interval", type=float, default=1.0)
    args = parser.parse_args()

    send, send_art, read_cmds, close = make_io(args.port, args.baud)
    mgr = await MediaManager.request_async()

    print(f"link up -> {args.port or 'stdout'} (Ctrl+C to stop)", file=sys.stderr)
    last_push = 0.0
    last_art_title = None
    settle_until = 0.0        # while now < settle_until, pause updates (post-skip)
    try:
        while True:
            for action in read_cmds():
                print(f"[cmd] {action}", file=sys.stderr)          # DIAG
                await do_command(mgr, action)
                if action in ("next", "prev"):
                    settle_until = time.monotonic() + SETTLE_S

            now = time.monotonic()
            if now >= settle_until and now - last_push >= args.interval:
                np = await read_now_playing(mgr)
                send(np)
                print(f"[np] {np.get('status')} {np.get('title')!r}", file=sys.stderr)  # DIAG
                last_push = now

                # Album art: attempt once per track; a stuck thumbnail read must
                # never freeze the whole helper, so bound it with a timeout.
                title = np.get("title")
                if np.get("status") != "none" and title and title != last_art_title:
                    timed_out = False
                    try:
                        cover = await asyncio.wait_for(asyncio.to_thread(read_cover_threaded, ART), timeout=3.0)
                    except asyncio.TimeoutError:
                        cover, timed_out = None, True
                        print("[art] cover fetch TIMED OUT", file=sys.stderr)  # DIAG
                    if cover:
                        ok = send_art(ART, ART, cover)
                        print(f"[art] {'sent' if ok else 'FAILED'} ({len(cover)}B)", file=sys.stderr)  # DIAG
                        if ok:
                            last_art_title = title      # done for this track
                    elif timed_out:
                        last_art_title = title          # give up on a hanging fetch
                    # else: thumbnail not ready yet -> leave it, retry next tick

            await asyncio.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
