# PmneziaVPN brand assets

The PmneziaVPN mark is the **Crema Shield**: a shield cut from a single coffee
bean, its seam running through it like a tunnel. It belongs to the same family
as coffeeblack-vpn's bean mark, and shares that project's palette.

| Role | Colour |
|---|---|
| Crema (mark, accent; also coffeeblack-vpn's accent) | `#FBB26A` |
| Roast (mark and accent on light grounds) | `#B8651E` |
| Espresso (ground) | `#0B0B13` |
| Tile (icon ground) | `#11111C` |
| Text | `#FAFAFA` |
| Tray idle / error | `#9A9AA5` / `#EB5757` |

Wordmark: "Pmnezia" in the text colour, "VPN" in crema, set in
[Sora](https://github.com/sora-xor/sora-font) Bold (`fonts/`, SIL OFL 1.1,
licence in `fonts/OFL.txt`). Outlines are baked into the SVGs, so nothing at
build time needs the font.

## Layout

| Path | What |
|---|---|
| `generate.py` | Builds everything below from one geometry definition. |
| `src/` | SVG masters: mark, light-ground mark, app icon, stacked and inline lockups. |
| `overlay/` | Generated replacements for upstream artwork, at their repository paths. |
| `overlay.json` | The list of overlay files, written by `generate.py`. |

`deploy/rebrand.sh` copies `overlay/` over the tree at build time, before it
renames brand-carrying files, and fails if an overlay file no longer replaces
an upstream one. The upstream images stay in git untouched, so merges from
amnezia-vpn/amnezia-client never conflict on artwork.

The overlay covers the Windows `.ico`, the macOS `.icns` and both macOS icon
sets, the iOS icon set, Android launcher, round, adaptive foreground,
monochrome and TV banner icons, the in-app logos, the tray states (idle grey,
connected crema, error red), the Linux icon and the F-Droid listing icon.

## Regenerating

Rendering only; nothing is compiled.

```sh
python3 -m venv /tmp/brand-venv
/tmp/brand-venv/bin/pip install cairosvg pillow fonttools
/tmp/brand-venv/bin/python deploy/brand/generate.py
```

ImageMagick's `convert` must be on `PATH` (it assembles the `.ico`). Commit the
regenerated `src/`, `overlay/` and `overlay.json` together.

When an upstream merge adds or resizes a branded image, add it to
`generate.py`. `rebrand.sh` refuses an overlay file whose upstream counterpart
has gone.
