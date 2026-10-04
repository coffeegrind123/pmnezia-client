#!/usr/bin/env python3
"""Generate every PmneziaVPN brand asset from one geometry definition.

Writes SVG masters to deploy/brand/src/ and the files that replace upstream
artwork to deploy/brand/overlay/, laid out at their repository paths.
deploy/rebrand.sh copies the overlay over the tree at build time, so git keeps
upstream's files and merges from amnezia-vpn/amnezia-client stay clean.

Rendering only, nothing is compiled. Needs cairosvg, Pillow and fontTools:

    python3 -m venv /tmp/brand-venv
    /tmp/brand-venv/bin/pip install cairosvg pillow fonttools
    /tmp/brand-venv/bin/python deploy/brand/generate.py

The mark is a shield cut from a single bean; its seam is a real hole (an
even-odd sub-path), not a stroke in the ground colour, so every output works
on any background and the same path data feeds SVG, Cairo and Android vector
drawables.
"""

import io
import json
import math
import os
import struct
import subprocess
import sys

import cairosvg
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, "src")
OVERLAY = os.path.join(HERE, "overlay")
FONT = os.path.join(HERE, "fonts", "Sora[wght].ttf")

# Palette. CREMA is also coffeeblack-vpn's accent, which keeps the client and
# server one family.
CREMA = "#FBB26A"
ROAST = "#B8651E"      # crema darkened for light grounds
ESPRESSO = "#0B0B13"   # page ground
TILE = "#11111C"       # icon tile, one step lighter than the ground
TEXT = "#FAFAFA"
TRAY_IDLE = "#9A9AA5"
TRAY_ERROR = "#EB5757"
MONO_GREY = "#CCCAC8"  # the settings-list icon colour upstream uses

# Geometry, in a 400x400 box.
SHIELD = "M200 36 C262 64 318 74 352 78 V196 C352 290 288 346 200 372 C112 346 48 290 48 196 V78 C82 74 138 64 200 36 Z"
SEAM = ((200, 92), (150, 158), (250, 236), (200, 318))


def seam_width(px):
    """Seam thickness for an icon rendered at `px` pixels: thicker as it shrinks."""
    if px >= 96:
        return 30
    if px >= 48:
        return 34
    if px >= 24:
        return 44
    return 56


def _bezier(t):
    (x0, y0), (x1, y1), (x2, y2), (x3, y3) = SEAM
    u = 1 - t
    x = u**3 * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t**3 * x3
    y = u**3 * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t**3 * y3
    dx = 3 * u * u * (x1 - x0) + 6 * u * t * (x2 - x1) + 3 * t * t * (x3 - x2)
    dy = 3 * u * u * (y1 - y0) + 6 * u * t * (y2 - y1) + 3 * t * t * (y3 - y2)
    n = math.hypot(dx, dy)
    return x, y, dx / n, dy / n


def seam_outline(width, steps=48):
    """Closed outline of the seam stroked at `width`, with round caps."""
    r = width / 2
    left, right = [], []
    for i in range(steps + 1):
        x, y, tx, ty = _bezier(i / steps)
        nx, ny = -ty, tx
        left.append((x + nx * r, y + ny * r))
        right.append((x - nx * r, y - ny * r))
    f = lambda p: f"{p[0]:.2f} {p[1]:.2f}"
    d = "M" + f(left[0])
    d += "".join(" L" + f(p) for p in left[1:])
    d += f" A{r:.2f} {r:.2f} 0 0 0 {f(right[-1])}"
    d += "".join(" L" + f(p) for p in reversed(right[:-1]))
    d += f" A{r:.2f} {r:.2f} 0 0 0 {f(left[0])} Z"
    return d


def mark_path(px):
    """Shield with the seam cut out, as one even-odd path."""
    return SHIELD + " " + seam_outline(seam_width(px))


def mark_svg(px, fill, seam=True, box=400):
    d = mark_path(px) if seam else SHIELD
    return f'<path d="{d}" fill="{fill}" fill-rule="evenodd"/>'


def wrap(body, w, h, vb=None):
    vb = vb or f"0 0 {w} {h}"
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
            f'viewBox="{vb}">{body}</svg>')


def tile_svg(px, shape="rounded", margin=0.0, mark_scale=0.74, bg=TILE):
    """App icon: the mark on a tile. `shape` is rounded, square or circle;
    `margin` is the transparent border as a fraction of the canvas."""
    s = 400
    inset = s * margin
    side = s - 2 * inset
    if shape == "circle":
        ground = f'<circle cx="200" cy="200" r="{side / 2:.2f}" fill="{bg}"/>'
    elif shape == "square":
        ground = f'<rect x="{inset}" y="{inset}" width="{side}" height="{side}" fill="{bg}"/>'
    else:
        ground = (f'<rect x="{inset}" y="{inset}" width="{side}" height="{side}" '
                  f'rx="{side * 0.225:.2f}" fill="{bg}"/>')
    k = side * mark_scale / s
    off = 200 - 200 * k
    mark = f'<g transform="translate({off:.2f} {off:.2f}) scale({k:.4f})">{mark_svg(px * k, CREMA)}</g>'
    return wrap(ground + mark, s, s)


class Wordmark:
    """"PmneziaVPN" in Sora Bold, as outlines."""

    def __init__(self):
        font = TTFont(FONT)
        self.font = instancer.instantiateVariableFont(font, {"wght": 700})
        self.gs = self.font.getGlyphSet()
        self.cmap = self.font.getBestCmap()
        self.upm = self.font["head"].unitsPerEm
        self.cap = self.font["OS/2"].sCapHeight

    def paths(self, text, x0=0.0):
        """Path data for `text`, baseline at y=0, y down, in font units."""
        out, x = [], x0
        for ch in text:
            name = self.cmap[ord(ch)]
            pen = SVGPathPen(self.gs)
            self.gs[name].draw(TransformPen(pen, (1, 0, 0, -1, x, 0)))
            out.append(pen.getCommands())
            x += self.gs[name].width
        return " ".join(out), x

    def svg_group(self, height, light=False):
        """Wordmark scaled to cap `height`; returns (svg group, width)."""
        first, x = self.paths("Pmnezia")
        second, end = self.paths("VPN", x)
        k = height / self.cap
        fg = "#1D1712" if light else TEXT
        acc = ROAST if light else CREMA
        g = (f'<g transform="scale({k:.5f})"><path d="{first}" fill="{fg}"/>'
             f'<path d="{second}" fill="{acc}"/></g>')
        return g, end * k


def lockup_svg(w, h, wm, layout="stacked", mark_frac=0.5, light=False):
    """Mark plus wordmark centred on a transparent w x h canvas."""
    if layout == "stacked":
        mark_h = h * mark_frac
        cap = mark_h * 0.22
        group, ww = wm.svg_group(cap, light)
        gap = mark_h * 0.18
        total = mark_h + gap + cap
        top = (h - total) / 2
        k = mark_h / 400
        mark = (f'<g transform="translate({(w - mark_h) / 2:.2f} {top:.2f}) scale({k:.4f})">'
                f'{mark_svg(mark_h, ROAST if light else CREMA)}</g>')
        word = f'<g transform="translate({(w - ww) / 2:.2f} {top + mark_h + gap + cap:.2f})">{group}</g>'
        return wrap(mark + word, w, h)
    # inline: mark on the left, wordmark to its right, both vertically centred,
    # shrunk to fit when the canvas is narrower than the lockup at full height
    fit = 1.0
    _, ww_full = wm.svg_group(h * 0.56, light)
    natural = h * 0.92 + h * 0.32 + ww_full
    if natural > w * 0.96:
        fit = w * 0.96 / natural
    mark_h = h * 0.92 * fit
    cap = h * 0.56 * fit
    group, ww = wm.svg_group(cap, light)
    gap = h * 0.32 * fit
    total = mark_h + gap + ww
    left = (w - total) / 2
    k = mark_h / 400
    mark = (f'<g transform="translate({left:.2f} {(h - mark_h) / 2:.2f}) scale({k:.4f})">'
            f'{mark_svg(max(mark_h, 16), ROAST if light else CREMA)}</g>')
    word = f'<g transform="translate({left + mark_h + gap:.2f} {(h + cap) / 2:.2f})">{group}</g>'
    return wrap(mark + word, w, h)


def banner_svg(w, h, wm):
    ground = f'<rect width="{w}" height="{h}" fill="{TILE}"/>'
    inner = lockup_svg(w * 0.84, h * 0.5, wm, layout="inline")
    inner = inner[inner.index(">") + 1:inner.rindex("</svg>")]
    return wrap(ground + f'<g transform="translate({w * 0.08:.2f} {h * 0.25:.2f})">{inner}</g>', w, h)


def render(svg, w, h=None, opaque=False):
    h = h or w
    png = cairosvg.svg2png(bytestring=svg.encode(), output_width=w, output_height=h)
    im = Image.open(io.BytesIO(png)).convert("RGBA")
    if opaque:
        bg = Image.new("RGBA", im.size, TILE)
        bg.alpha_composite(im)
        im = bg.convert("RGB")
    return im


def out(rel):
    path = os.path.join(OVERLAY, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return path


def save_png(im, rel):
    im.save(out(rel), optimize=True)


def write_icns(images, rel):
    """ICNS from PNG payloads. Types per Apple's icns format: icp4 16, icp5 32,
    ic07 128, ic08 256, ic09 512, ic10 1024 (512@2x), ic11 32 (16@2x), ic12 64
    (32@2x), ic13 256 (128@2x), ic14 512 (256@2x)."""
    types = [("icp4", 16), ("icp5", 32), ("ic11", 32), ("ic12", 64), ("ic07", 128),
             ("ic13", 256), ("ic08", 256), ("ic14", 512), ("ic09", 512), ("ic10", 1024)]
    chunks = b""
    for code, px in types:
        buf = io.BytesIO()
        images[px].save(buf, format="PNG", optimize=True)
        data = buf.getvalue()
        chunks += code.encode() + struct.pack(">I", len(data) + 8) + data
    with open(out(rel), "wb") as f:
        f.write(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)


def write_ico(images, rel):
    """Multi-size .ico from per-size renders (Pillow would resample one image)."""
    tmp = []
    for px, im in sorted(images.items()):
        p = out(f".ico-tmp/{px}.png")
        im.save(p)
        tmp.append(p)
    subprocess.run(["convert", *tmp, out(rel)], check=True)
    for p in tmp:
        os.remove(p)
    os.rmdir(os.path.dirname(tmp[0]))


def android_vector(rel, viewport=400):
    """The mark as an Android vector drawable (used for the monochrome icon)."""
    xml = f'''<?xml version="1.0" encoding="utf-8"?>
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="256dp"
    android:height="256dp"
    android:viewportWidth="{viewport}"
    android:viewportHeight="{viewport}">
  <path
      android:fillColor="#FFFFFFFF"
      android:fillType="evenOdd"
      android:pathData="{mark_path(96)}" />
</vector>
'''
    with open(out(rel), "w") as f:
        f.write(xml)


def write_text(rel, text):
    with open(out(rel), "w") as f:
        f.write(text)


def main():
    if not os.path.exists(FONT):
        sys.exit(f"missing {FONT}")
    wm = Wordmark()
    os.makedirs(SRC, exist_ok=True)

    # SVG masters.
    masters = {
        "mark.svg": wrap(mark_svg(400, CREMA), 400, 400),
        "mark-light.svg": wrap(mark_svg(400, ROAST), 400, 400),
        "app-icon.svg": tile_svg(400),
        "logo-stacked.svg": lockup_svg(742, 655, wm),
        "logo-inline.svg": lockup_svg(600, 88, wm, layout="inline"),
        "logo-inline-light.svg": lockup_svg(600, 88, wm, layout="inline", light=True),
    }
    for name, svg in masters.items():
        with open(os.path.join(SRC, name), "w") as f:
            f.write(svg + "\n")

    # Desktop and shared images.
    tiles = {px: render(tile_svg(px), px) for px in (16, 24, 32, 48, 64, 128, 256, 512, 1024)}
    save_png(tiles[256], "client/images/icon.png")
    save_png(tiles[512], "deploy/data/linux/AmneziaVPN.png")
    write_ico({px: tiles[px] for px in (16, 24, 32, 48, 64, 128, 256)}, "client/images/app.ico")
    save_png(render(lockup_svg(1440, 1200, wm, mark_frac=0.46), 1440, 1200), "client/images/amneziaBigLogo.png")
    save_png(render(lockup_svg(150, 22, wm, layout="inline"), 150, 22), "client/images/AmneziaVPN.png")
    write_text("client/images/AmneziaVPN_Full_logo.svg", masters["logo-stacked.svg"] + "\n")
    write_text("client/images/controls/amnezia.svg",
               wrap(mark_svg(24, MONO_GREY), 23, 22, vb="0 0 400 400") + "\n")
    for state, colour in (("default", TRAY_IDLE), ("active", CREMA), ("error", TRAY_ERROR)):
        save_png(render(wrap(mark_svg(32, colour), 400, 400), 200), f"client/images/tray/{state}.png")

    # macOS: Apple's grid puts an 824 px body in a 1024 canvas.
    mac = {px: render(tile_svg(px, margin=100 / 1024), px) for px in (16, 32, 64, 128, 256, 512, 1024)}
    write_icns(mac, "client/images/app.icns")
    for base in ("client/macos/app/Images.xcassets/AppIcon.appiconset",
                 "client/macos/app/Images-beta.xcassets/AppIcon.appiconset"):
        for px in (16, 32, 128, 256, 512):
            save_png(mac[px], f"{base}/{px}.png")
            save_png(mac[px * 2], f"{base}/{px}@2x.png")

    # iOS: full-bleed and opaque; the system applies the corner mask.
    ios = "client/ios/app/Media.xcassets/AppIcon.appiconset"
    for px in (20, 29, 40, 50, 57, 58, 60, 72, 76, 80, 87, 100, 114, 120, 144, 152, 167, 180, 1024):
        save_png(render(tile_svg(px, shape="square", mark_scale=0.66), px, opaque=True), f"{ios}/{px}.png")

    # Android.
    res = "client/android/res"
    for density, px in (("ldpi", 36), ("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        save_png(render(tile_svg(px), px), f"{res}/mipmap-{density}/icon.png")
        save_png(render(tile_svg(px, shape="circle", mark_scale=0.62), px), f"{res}/mipmap-{density}/icon_round.png")
        save_png(render(lockup_svg(150, 22, wm, layout="inline"), 150, 22), f"{res}/drawable-{density}/logo.png")
    for density, px in (("mdpi", 108), ("hdpi", 162), ("xhdpi", 216), ("xxhdpi", 324), ("xxxhdpi", 432)):
        # Adaptive foreground: keep the mark inside the central 66/108 safe zone.
        fg = wrap(f'<g transform="translate(100 100) scale(0.5)">{mark_svg(px / 2, CREMA)}</g>', 400, 400)
        save_png(render(fg, px), f"{res}/mipmap-{density}/ic_launcher_foreground.png")
    for density, (w, h) in (("mdpi", (160, 90)), ("hdpi", (240, 135)), ("xhdpi", (320, 180))):
        save_png(render(banner_svg(w, h, wm), w, h), f"{res}/mipmap-{density}/ic_banner.png")
    android_vector(f"{res}/drawable/ic_amnezia_round.xml")
    write_text(f"{res}/drawable/ic_launcher_background.xml", f'''<?xml version="1.0" encoding="utf-8"?>
<shape xmlns:android="http://schemas.android.com/apk/res/android"
    android:shape="rectangle">
    <solid android:color="{TILE}" />
</shape>
''')
    write_text(f"{res}/values/colors.xml", f'''<?xml version="1.0" encoding="utf-8"?>
<resources>
    <color name="ic_launcher_background">{TILE}</color>
</resources>
''')

    # F-Droid listing icon.
    save_png(render(tile_svg(480), 480), "metadata/en-US/images/icon.png")

    files = sorted(os.path.relpath(os.path.join(d, f), OVERLAY)
                   for d, _, fs in os.walk(OVERLAY) for f in fs)
    with open(os.path.join(HERE, "overlay.json"), "w") as f:
        json.dump(files, f, indent=1)
        f.write("\n")
    print(f"wrote {len(masters)} masters and {len(files)} overlay files")


if __name__ == "__main__":
    main()
