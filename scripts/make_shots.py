"""
Turns raw captures (simulator window snips, board photos) into Chrome Web Store
screenshots: scaled to fit, centered on a dark canvas, RGB (no alpha), saved PNG.

Usage:
    python make_shots.py            # 640x400 (store screenshot size)
    python make_shots.py 1280 800   # custom width height

Drop .png/.jpg/.bmp files into scripts/shots_in/ and run; results land in
scripts/shots_out/.
"""
import os
import sys
from PIL import Image

W, H = 640, 400
BG = (15, 15, 15)

HERE = os.path.dirname(os.path.abspath(__file__))
IN_DIR = os.path.join(HERE, "shots_in")
OUT_DIR = os.path.join(HERE, "shots_out")

# Scales an image to fit W x H and centers it on a dark canvas
def process(path, out):
    img = Image.open(path).convert("RGB")
    scale = min(W / img.width, H / img.height)
    size = (max(1, round(img.width * scale)), max(1, round(img.height * scale)))
    img = img.resize(size, Image.LANCZOS)
    canvas = Image.new("RGB", (W, H), BG)
    canvas.paste(img, ((W - img.width) // 2, (H - img.height) // 2))
    canvas.save(out, "PNG")

def main():
    global W, H
    if len(sys.argv) == 3:
        W, H = int(sys.argv[1]), int(sys.argv[2])
    os.makedirs(IN_DIR, exist_ok=True)
    os.makedirs(OUT_DIR, exist_ok=True)
    count = 0
    for name in sorted(os.listdir(IN_DIR)):
        if name.lower().endswith((".png", ".jpg", ".jpeg", ".bmp")):
            dst = os.path.join(OUT_DIR, os.path.splitext(name)[0] + ".png")
            process(os.path.join(IN_DIR, name), dst)
            print("wrote", dst)
            count += 1
    if count == 0:
        print("no images found in", IN_DIR)

if __name__ == "__main__":
    main()
