#!/usr/bin/env python3
"""
UZZ panel generator.

Emits res/UZZ.svg with geometry only (backgrounds, accent bars, row separators,
knob rings, section frames). All text labels are drawn in C++ via TextLabel
widgets (because NanoSVG, VCV Rack's SVG renderer, does NOT support <text>).

IMPORTANT: Layout constants below must stay in sync with the UI:: namespace in
src/UZZ.cpp. When changing either, update both.

Usage:
    python3 tools/gen_panel.py
"""
from __future__ import annotations
import os
from pathlib import Path
from textwrap import dedent

# ---------------------------------------------------------------------------
# Canvas
# ---------------------------------------------------------------------------
HP = 58                          # 58 HP
WIDTH = HP * 15                  # = 870 px
HEIGHT = 380                     # standard rack module height

# ---------------------------------------------------------------------------
# Layout constants — MUST MATCH src/UZZ.cpp UI:: namespace
# ---------------------------------------------------------------------------
COLS = 16
LEFT = 48.0
RIGHT = 1.0
BOTTOM_MARGIN = 28.0

Y_STEP_LED = 10.0
Y_STEP_MODE = 10.0
Y_NOTE = 30.0

ROW_START = 72.0
ROW_SPACE = 48.0
Y_PITCH = ROW_START
Y_OCT   = ROW_START + ROW_SPACE
Y_DUR   = ROW_START + ROW_SPACE * 2
Y_C1    = ROW_START + ROW_SPACE * 3
Y_C2    = ROW_START + ROW_SPACE * 4

RAND_X = LEFT - 10.0

SHIFT_X_CENTER = (RAND_X - 18.0) + 15.0   # rowShiftX() in cpp

# Column center helper (matches UI::colCenter)
def col_center(i: int) -> float:
    usable = WIDTH - LEFT - RIGHT
    col_w = usable / float(COLS)
    return LEFT + (i + 0.5) * col_w

# Bottom section — X positions aligned to step-column centers (1-indexed).
BOTTOM_BASE = HEIGHT - BOTTOM_MARGIN
Y_BOT_TOP = BOTTOM_BASE - 54.0
Y_BOT_MID = BOTTOM_BASE - 24.0
Y_BOT_BOT = BOTTOM_BASE +  6.0

X_IN     = col_center(1)    # step 2
X_CTRL1  = col_center(3)    # step 4
X_CTRL2  = col_center(6)    # step 7
X_SWITCH = col_center(10)   # step 11
X_OUT1   = col_center(12)   # step 13
X_OUT2   = col_center(14)   # step 15

# Separator between step area and bottom section
# Midpoint between last knob row (Y_C2=264) and bottom top row (298)
Y_BOT_SEPARATOR = (Y_C2 + Y_BOT_TOP) / 2.0   # = 281.0

# ---------------------------------------------------------------------------
# Style (per-theme palettes)
# ---------------------------------------------------------------------------
ACCENT_COLOR     = "#2C7FFF"
ACCENT_H         = 2.4             # top/bottom accent bar height
RING_WIDTH       = 0.8
RING_RADIUS      = 13.0
SEPARATOR_WIDTH  = 0.6

THEMES = {
    "dark": {
        "bg":               "#1A1A1A",
        "ring":             "#3A4150",
        "separator":        "#33384A",
        "separator_bright": "#5A6278",
    },
    "light": {
        "bg":               "#E5E5E5",
        "ring":             "#B8BECA",
        "separator":        "#9AA2B5",
        "separator_bright": "#707890",
    },
}

# ---------------------------------------------------------------------------
# SVG helpers
# ---------------------------------------------------------------------------
def rect(x, y, w, h, fill, **kw) -> str:
    extra = " ".join(f'{k}="{v}"' for k, v in kw.items())
    return f'    <rect x="{x:g}" y="{y:g}" width="{w:g}" height="{h:g}" fill="{fill}" {extra} />'

def circle(cx, cy, r, stroke, sw=RING_WIDTH, fill="none") -> str:
    return (f'    <circle cx="{cx:g}" cy="{cy:g}" r="{r:g}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{sw:g}" />')

def line(x1, y1, x2, y2, stroke, sw=SEPARATOR_WIDTH) -> str:
    return (f'    <line x1="{x1:g}" y1="{y1:g}" x2="{x2:g}" y2="{y2:g}" '
            f'stroke="{stroke}" stroke-width="{sw:g}" stroke-linecap="round" />')

def group(ident: str, body: list[str]) -> list[str]:
    out = [f'  <g id="{ident}">']
    out.extend(body)
    out.append('  </g>')
    return out

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
def build_svg(theme: str = "dark") -> str:
    palette = THEMES[theme]
    BG_COLOR        = palette["bg"]
    RING_COLOR      = palette["ring"]
    SEPARATOR_COLOR = palette["separator"]

    lines: list[str] = []
    lines.append('<?xml version="1.0" encoding="UTF-8"?>')
    lines.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" '
        f'viewBox="0 0 {WIDTH} {HEIGHT}" width="{WIDTH}" height="{HEIGHT}">'
    )

    # Local wrappers that bake in the theme palette.
    def tcircle(cx, cy, r, stroke=RING_COLOR, sw=RING_WIDTH, fill="none"):
        return circle(cx, cy, r, stroke=stroke, sw=sw, fill=fill)
    def tline(x1, y1, x2, y2, stroke=SEPARATOR_COLOR, sw=SEPARATOR_WIDTH):
        return line(x1, y1, x2, y2, stroke=stroke, sw=sw)

    # --- Background -------------------------------------------------------
    bg: list[str] = []
    bg.append(rect(0, 0, WIDTH, HEIGHT, BG_COLOR))
    lines += group("background", bg)

    # --- Accent bars ------------------------------------------------------
    accents: list[str] = []
    accents.append(rect(0, 0, WIDTH, ACCENT_H, ACCENT_COLOR))
    accents.append(rect(0, HEIGHT - ACCENT_H, WIDTH, ACCENT_H, ACCENT_COLOR))
    lines += group("accent_bars", accents)

    # --- Row separators (horizontal) --------------------------------------
    seps: list[str] = []
    row_ys = [Y_PITCH, Y_OCT, Y_DUR, Y_C1, Y_C2]
    for y in row_ys[:-1]:
        ymid = y + ROW_SPACE / 2.0
        seps.append(tline(LEFT, ymid, WIDTH - 2, ymid))
    # Separator between top step area (notes) and first knob row (PITCH) —
    # exactly midway between their centers.
    y_top_sep = (Y_NOTE + Y_PITCH) / 2.0   # = 51
    seps.append(tline(LEFT, y_top_sep, WIDTH - 2, y_top_sep))
    # Separator above bottom section — same gray style as row separators
    seps.append(tline(LEFT, Y_BOT_SEPARATOR, WIDTH - 2, Y_BOT_SEPARATOR))
    lines += group("separators", seps)

    # --- Vertical column separators between steps 1-2 ... 15-16 ---------
    col_seps: list[str] = []
    col_w = (WIDTH - LEFT - RIGHT) / float(COLS)
    y_top    = ACCENT_H + 2.0                # just under the top accent bar
    y_bot    = Y_BOT_SEPARATOR               # stop above the bottom section
    y_bot_ex = HEIGHT - ACCENT_H - 2.0       # extend to bottom accent bar
    # Dividers that extend ALL the way to the bottom accent bar.
    # Index 0 = LEFT (x=48, before step 1); index i = LEFT + i*col_w.
    FULL_HEIGHT_DIVIDERS = {0, 2, 5, 8, 9, 13, 15}  # extend to bottom accent bar
    BRIGHT_DIVIDERS      = {4, 9, 13}               # slightly brighter colour

    # Pick stroke colour for a given divider index.
    def sep_color(i):
        return THEMES[theme]["separator_bright"] if i in BRIGHT_DIVIDERS \
               else SEPARATOR_COLOR

    # Line between the left controls (randoms/shifts/trigs) and step 1 (idx 0)
    col_seps.append(line(LEFT, y_top, LEFT,
                         y_bot_ex if 0 in FULL_HEIGHT_DIVIDERS else y_bot,
                         stroke=sep_color(0)))
    for i in range(1, COLS):          # 15 vertical lines between steps
        x   = LEFT + i * col_w
        end = y_bot_ex if i in FULL_HEIGHT_DIVIDERS else y_bot
        if i in BRIGHT_DIVIDERS and i in FULL_HEIGHT_DIVIDERS:
            # Bright only through the step area; gray for the bottom extension.
            col_seps.append(line(x, y_top, x, y_bot, stroke=sep_color(i)))
            col_seps.append(line(x, y_bot, x, end,   stroke=SEPARATOR_COLOR))
        else:
            col_seps.append(line(x, y_top, x, end, stroke=sep_color(i)))
    lines += group("col_separators", col_seps)

    # (Knob rings are drawn in C++ as value arcs around each UzzArcKnob.)

    # (Port frames removed — random input column outline was distracting.)

    # --- Animatek logo (same visual size as OxiCvExp, step 9 column) ----------
    # OxiCvExp: mm SVG, scale=0.0007 → 6.14 mm = 18.1 px (at 1HP=15px=5.08mm).
    # UZZ is a px SVG, so scale = 18.1 / 8773 ≈ 0.00206.
    # Y matches OxiCvExp: 5.5 mm from bottom = 16 px → y = HEIGHT - 16.
    logo_scale  = 0.00206
    logo_cx     = col_center(8)               # step 9 vertical
    logo_cy     = HEIGHT - 16.0              # same relative height as expander
    logo_path   = ("M3953,4248.31L3953,1873.04C3953,1769.02 4022.35,1699.67 4126.38,1699.67"
                   "L4663.84,1699.67C4767.87,1699.67 4837.22,1769.02 4837.22,1873.04"
                   "L4837.22,4248.31C4837.22,4352.34 4767.87,4421.69 4663.84,4421.69"
                   "L4126.38,4421.69C4022.35,4421.69 3953,4352.34 3953,4248.31Z"
                   "M3953,8565.4L3953,6536.89C3953,6432.86 4022.35,6363.51 4126.38,6363.51"
                   "L4663.84,6363.51C4767.87,6363.51 4837.22,6432.86 4837.22,6536.89"
                   "L4837.22,8565.4C4837.22,8669.43 4906.57,8756.11 5027.94,8738.78"
                   "C7143.14,8426.7 8772.88,6588.9 8772.88,4369.67"
                   "C8772.88,1959.73 6796.38,0.57 4386.44,0.57"
                   "C1959.16,0.57 0,1959.73 0,4369.67"
                   "C0,6606.24 1647.08,8426.7 3762.28,8738.78"
                   "C3883.65,8756.11 3953,8669.43 3953,8565.4Z")
    logo_tf = (f"translate({logo_cx:g},{logo_cy:g}) "
               f"scale({logo_scale}) "
               f"translate(-4386,-4369)")
    logo_color = "#2C7FFF"
    lines.append(f'  <g id="logo" transform="{logo_tf}">')
    lines.append(f'    <path d="{logo_path}" fill="{logo_color}" fill-rule="nonzero"/>')
    lines.append(f'  </g>')

    lines.append("</svg>")
    return "\n".join(lines) + "\n"


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    targets = [
        ("dark",  root / "res" / "UZZ.svg"),
        ("light", root / "res" / "UZZ-light.svg"),
    ]
    for theme, out in targets:
        svg = build_svg(theme)
        out.write_text(svg, encoding="utf-8")
        print(f"Wrote {out} ({len(svg):,} bytes, theme={theme}, "
              f"{HP} HP, {WIDTH}x{HEIGHT} px)")


if __name__ == "__main__":
    main()
