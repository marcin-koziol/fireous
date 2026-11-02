#!/usr/bin/env python3
"""Static Cairo mockup of the Fireous hardware-DSP UI direction.

Not wired into the build - a design aid, same spirit as the siblings'
offline_render.cpp.  Run:

    python3 tools/mockup_hardware.py docs/ui-hardware-mockup.png
"""

import math
import sys

import cairo

W, H = 1280, 900
OUT = sys.argv[1] if len(sys.argv) > 1 else "docs/ui-hardware-mockup.png"

# ---------------------------------------------------------------------------
# palette

BG_TOP      = (0.075, 0.075, 0.078)
BG_BOT      = (0.025, 0.025, 0.028)
PANEL_TOP   = (0.135, 0.135, 0.145)
PANEL_BOT   = (0.070, 0.070, 0.078)
PANEL_EDGE  = (0.34, 0.34, 0.37)
PANEL_HI    = (0.50, 0.50, 0.54)
METAL       = (0.36, 0.36, 0.39)
SILVER      = (0.78, 0.78, 0.80)
CREAM       = (0.93, 0.88, 0.76)
COPPER      = (0.74, 0.46, 0.22)
AMBER       = (1.00, 0.69, 0.00)
AMBER_DIM   = (0.55, 0.38, 0.00)
GREEN       = (0.22, 1.00, 0.45)
RED         = (1.00, 0.23, 0.18)
BLUE        = (0.29, 0.64, 1.00)
TEXT        = (0.86, 0.84, 0.78)
DIM         = (0.46, 0.44, 0.40)
BLACK       = (0.02, 0.02, 0.02)


def rgb(c, a=1.0):
    return (c[0], c[1], c[2], a)


def set_color(cr, c, a=1.0):
    cr.set_source_rgba(c[0], c[1], c[2], a)


def rounded_rect(cr, x, y, w, h, r):
    cr.new_sub_path()
    cr.arc(x + w - r, y + r, r, -math.pi / 2, 0)
    cr.arc(x + w - r, y + h - r, r, 0, math.pi / 2)
    cr.arc(x + r, y + h - r, r, math.pi / 2, math.pi)
    cr.arc(x + r, y + r, r, math.pi, 3 * math.pi / 2)
    cr.close_path()


def text(cr, s, x, y, size=11, color=TEXT, bold=True, mono=False, align="left"):
    family = "DejaVu Sans Mono" if mono else "DejaVu Sans"
    cr.select_font_face(family, cairo.FONT_SLANT_NORMAL,
                        cairo.FONT_WEIGHT_BOLD if bold else cairo.FONT_WEIGHT_NORMAL)
    cr.set_font_size(size)
    ext = cr.text_extents(s)
    if align == "center":
        x -= ext.width / 2 + ext.x_bearing
    elif align == "right":
        x -= ext.width + ext.x_bearing
    cr.move_to(x, y)
    set_color(cr, color)
    cr.show_text(s)
    cr.new_path()


def panel_bg(cr, x, y, w, h, r=8, top=PANEL_TOP, bot=PANEL_BOT):
    g = cairo.LinearGradient(x, y, x, y + h)
    g.add_color_stop_rgb(0, *top)
    g.add_color_stop_rgb(1, *bot)
    rounded_rect(cr, x, y, w, h, r)
    cr.set_source(g)
    cr.fill()
    # top highlight
    rounded_rect(cr, x + 0.5, y + 0.5, w - 1, h - 1, r)
    set_color(cr, PANEL_HI, 0.35)
    cr.set_line_width(1)
    cr.stroke()


def screw(cr, x, y, r=6):
    set_color(cr, (0.12, 0.12, 0.13))
    cr.arc(x, y, r, 0, 2 * math.pi)
    cr.fill()
    set_color(cr, METAL)
    cr.arc(x, y, r - 1, 0, 2 * math.pi)
    cr.set_line_width(1)
    cr.stroke()
    set_color(cr, (0.10, 0.10, 0.11))
    cr.set_line_width(1.4)
    cr.move_to(x - r + 2, y)
    cr.line_to(x + r - 2, y)
    cr.stroke()
    cr.move_to(x, y - r + 2)
    cr.line_to(x, y + r - 2)
    cr.stroke()


def led(cr, x, y, r=4.0, color=GREEN, on=True):
    if on:
        g = cairo.RadialGradient(x, y, 0, x, y, r * 3.2)
        g.add_color_stop_rgba(0, *rgb(color, 0.55))
        g.add_color_stop_rgba(1, *rgb(color, 0.0))
        cr.arc(x, y, r * 3.2, 0, 2 * math.pi)
        cr.set_source(g)
        cr.fill()
        set_color(cr, color, 1.0)
    else:
        set_color(cr, (0.16, 0.16, 0.16), 1.0)
    cr.arc(x, y, r, 0, 2 * math.pi)
    cr.fill()
    set_color(cr, (0.05, 0.05, 0.05))
    cr.arc(x, y, r * 1.45, 0, 2 * math.pi)
    cr.set_line_width(1)
    cr.stroke()


def button(cr, x, y, w, h, label, active=False, accent=AMBER, size=10):
    rounded_rect(cr, x, y, w, h, 4)
    g = cairo.LinearGradient(x, y, x, y + h)
    g.add_color_stop_rgb(0, 0.19, 0.19, 0.20)
    g.add_color_stop_rgb(1, 0.08, 0.08, 0.09)
    cr.set_source(g)
    cr.fill_preserve()
    set_color(cr, accent if active else PANEL_EDGE, 0.9 if active else 0.6)
    cr.set_line_width(1.2)
    cr.stroke()
    text(cr, label, x + w / 2, y + h / 2 + size * 0.35, size, accent if active else TEXT,
         align="center", mono=True)


def knob(cr, cx, cy, r, value, label, accent=AMBER, size=9):
    start = math.pi * 0.75
    sweep = math.pi * 1.5
    ang = start + sweep * max(0.0, min(1.0, value))

    # tick ring
    for i in range(11):
        a = start + sweep * i / 10
        x0 = cx + math.cos(a) * (r + 7)
        y0 = cy + math.sin(a) * (r + 7)
        x1 = cx + math.cos(a) * (r + 11)
        y1 = cy + math.sin(a) * (r + 11)
        set_color(cr, DIM, 0.8)
        cr.set_line_width(1.2)
        cr.move_to(x0, y0)
        cr.line_to(x1, y1)
        cr.stroke()

    # value arc
    set_color(cr, (0.20, 0.20, 0.21))
    cr.set_line_width(4.5)
    cr.arc(cx, cy, r, start, start + sweep)
    cr.stroke()
    set_color(cr, accent, 0.95)
    cr.set_line_width(4.5)
    cr.arc(cx, cy, r, start, ang)
    cr.stroke()

    # body
    g = cairo.RadialGradient(cx - r * 0.35, cy - r * 0.4, r * 0.2, cx, cy, r)
    g.add_color_stop_rgb(0, 0.24, 0.24, 0.26)
    g.add_color_stop_rgb(1, 0.06, 0.06, 0.07)
    cr.arc(cx, cy, r - 3, 0, 2 * math.pi)
    cr.set_source(g)
    cr.fill()
    set_color(cr, (0.42, 0.42, 0.45), 0.7)
    cr.set_line_width(1)
    cr.arc(cx, cy, r - 3, 0, 2 * math.pi)
    cr.stroke()

    # cap
    g = cairo.LinearGradient(cx, cy - r * 0.55, cx, cy + r * 0.55)
    g.add_color_stop_rgb(0, 0.72, 0.72, 0.75)
    g.add_color_stop_rgb(1, 0.30, 0.30, 0.33)
    cr.arc(cx, cy, r * 0.55, 0, 2 * math.pi)
    cr.set_source(g)
    cr.fill()
    set_color(cr, (0.12, 0.12, 0.13), 0.8)
    cr.set_line_width(1)
    cr.arc(cx, cy, r * 0.55, 0, 2 * math.pi)
    cr.stroke()

    # pointer
    set_color(cr, (0.98, 0.98, 0.98))
    cr.set_line_width(2.2)
    cr.move_to(cx + math.cos(ang) * r * 0.18, cy + math.sin(ang) * r * 0.18)
    cr.line_to(cx + math.cos(ang) * r * 0.48, cy + math.sin(ang) * r * 0.48)
    cr.stroke()

    text(cr, label, cx, cy + r + 20, size, DIM, align="center")
    text(cr, f"{value * 100:.0f}", cx, cy - r - 10, size - 1, accent, align="center", mono=True)


def fader(cr, x, y, w, h, value, label):
    # slot
    rounded_rect(cr, x + w / 2 - 4, y, 8, h, 4)
    set_color(cr, (0.02, 0.02, 0.02))
    cr.fill()
    set_color(cr, (0.30, 0.30, 0.32), 0.7)
    cr.set_line_width(1)
    cr.stroke()

    # ticks
    for i in range(9):
        ty = y + h * i / 8
        set_color(cr, DIM, 0.55)
        cr.set_line_width(1)
        cr.move_to(x + w / 2 + 8, ty)
        cr.line_to(x + w / 2 + 15, ty)
        cr.stroke()

    # cap
    cy = y + (1.0 - max(0.0, min(1.0, value))) * (h - 26) + 13
    g = cairo.LinearGradient(x, cy - 12, x, cy + 12)
    g.add_color_stop_rgb(0, 0.76, 0.76, 0.78)
    g.add_color_stop_rgb(1, 0.28, 0.28, 0.30)
    rounded_rect(cr, x + w / 2 - 16, cy - 11, 32, 22, 4)
    cr.set_source(g)
    cr.fill_preserve()
    set_color(cr, (0.10, 0.10, 0.11), 0.9)
    cr.set_line_width(1)
    cr.stroke()
    set_color(cr, (0.95, 0.95, 0.95))
    cr.set_line_width(1.5)
    cr.move_to(x + w / 2 - 10, cy)
    cr.line_to(x + w / 2 + 10, cy)
    cr.stroke()

    text(cr, label, x + w / 2, y + h + 15, 8, DIM, align="center", mono=True)


def meter(cr, x, y, w, h, value, segments=14):
    gap = 2
    seg_h = (h - gap * (segments - 1)) / segments
    for i in range(segments):
        frac = (i + 1) / segments
        if i >= segments * 0.80:
            color = RED
        elif i >= segments * 0.60:
            color = AMBER
        else:
            color = GREEN
        on = value >= frac
        yy = y + h - (i + 1) * seg_h - i * gap
        rounded_rect(cr, x, yy, w, seg_h, 1.5)
        set_color(cr, color, 0.9 if on else 0.10)
        cr.fill()
    text(cr, "GR", x + w / 2, y + h + 14, 8, DIM, align="center", mono=True)


def module_card(cr, x, y, w, h, title, slot, accent, bypass=False, wide=False):
    panel_bg(cr, x, y, w, h)
    # header strip
    rounded_rect(cr, x + 1, y + 1, w - 2, 30, 7)
    g = cairo.LinearGradient(x, y, x, y + 30)
    g.add_color_stop_rgb(0, 0.20, 0.20, 0.21)
    g.add_color_stop_rgb(1, 0.10, 0.10, 0.11)
    cr.set_source(g)
    cr.fill()
    # accent stripe
    set_color(cr, accent, 0.9)
    cr.rectangle(x + 1, y + 1, 4, 30)
    cr.fill()
    # title / slot
    text(cr, title, x + 14, y + 20, 12, CREAM)
    text(cr, f"{slot:02d}", x + w - 74, y + 20, 10, DIM, mono=True)
    # bypass LED + label
    led(cr, x + w - 50, y + 16, 3.6, RED if bypass else GREEN, True)
    text(cr, "BYP", x + w - 42, y + 20, 8, DIM, mono=True)
    # drag dots
    set_color(cr, DIM, 0.7)
    for i in range(3):
        cr.arc(x + w - 16, y + 9 + i * 5, 1.5, 0, 2 * math.pi)
        cr.fill()
    # screws
    screw(cr, x + 10, y + h - 10, 4.5)
    screw(cr, x + w - 10, y + h - 10, 4.5)


def lcd(cr, x, y, w, h):
    rounded_rect(cr, x, y, w, h, 6)
    set_color(cr, (0.02, 0.035, 0.025))
    cr.fill_preserve()
    set_color(cr, (0.22, 0.30, 0.22), 0.8)
    cr.set_line_width(1.2)
    cr.stroke()

    # scanlines
    for i in range(0, int(h), 3):
        set_color(cr, (0.20, 0.45, 0.25), 0.035)
        cr.rectangle(x + 4, y + i, w - 8, 1)
        cr.fill()

    # grid
    for i in range(1, 8):
        xx = x + 8 + (w - 16) * i / 8
        set_color(cr, (0.20, 0.50, 0.25), 0.10)
        cr.set_line_width(1)
        cr.move_to(xx, y + 6)
        cr.line_to(xx, y + h - 6)
        cr.stroke()
    for i in range(1, 4):
        yy = y + 6 + (h - 12) * i / 4
        set_color(cr, (0.20, 0.50, 0.25), 0.10)
        cr.set_line_width(1)
        cr.move_to(x + 6, yy)
        cr.line_to(x + w - 6, yy)
        cr.stroke()

    # response curve (illustrative)
    pts = []
    for i in range(240):
        t = i / 239.0
        f = 20.0 * (1000.0 ** t)  # 20 Hz .. 20 kHz
        db = (2.5 * math.exp(-((math.log10(f / 70.0)) ** 2) / 0.04)
              - 4.0 * math.exp(-((math.log10(f / 420.0)) ** 2) / 0.05)
              + 1.5 * math.exp(-((math.log10(f / 2600.0)) ** 2) / 0.10)
              + 3.5 * math.exp(-((math.log10(f / 10500.0)) ** 2) / 0.06))
        px = x + 8 + (w - 16) * t
        py = y + h / 2 - db * (h - 24) / 24.0
        pts.append((px, py))

    set_color(cr, (0.25, 0.75, 0.35), 0.30)
    cr.set_line_width(5)
    for i, (px, py) in enumerate(pts):
        if i == 0:
            cr.move_to(px, py)
        else:
            cr.line_to(px, py)
    cr.stroke()
    set_color(cr, (0.55, 1.00, 0.55), 0.95)
    cr.set_line_width(1.8)
    for i, (px, py) in enumerate(pts):
        if i == 0:
            cr.move_to(px, py)
        else:
            cr.line_to(px, py)
    cr.stroke()

    # labels
    text(cr, "MAGNITUDE", x + 12, y + 18, 9, (0.45, 0.85, 0.45), mono=True)
    text(cr, "+12", x + 8, y + 30, 7, (0.30, 0.65, 0.30), mono=True)
    text(cr, "-12", x + 8, y + h - 10, 7, (0.30, 0.65, 0.30), mono=True)
    for label, t in (("20", 0.0), ("100", 0.233), ("1k", 0.566), ("10k", 0.899), ("20k", 1.0)):
        px = x + 8 + (w - 16) * t
        text(cr, label, px, y + h - 3, 7, (0.30, 0.65, 0.30), mono=True, align="center")

    # right-side readouts
    text(cr, "LATENCY", x + w - 150, y + 18, 8, (0.35, 0.75, 0.40), mono=True)
    text(cr, "0.0 ms", x + w - 150, y + 36, 16, (0.60, 1.00, 0.60), mono=True)
    text(cr, "48.0 kHz", x + w - 150, y + 54, 8, (0.35, 0.75, 0.40), mono=True)
    text(cr, "0 samples · MIN-PHASE", x + w - 150, y + 68, 8, (0.35, 0.75, 0.40), mono=True)



def module_lcd(cr, x, y, w, h, title, big=None, lines=(), curve=False, color=GREEN):
    rounded_rect(cr, x, y, w, h, 4)
    set_color(cr, (0.02, 0.035, 0.025))
    cr.fill_preserve()
    set_color(cr, (0.22, 0.30, 0.22), 0.8)
    cr.set_line_width(1)
    cr.stroke()
    for i in range(0, int(h), 3):
        set_color(cr, (0.20, 0.45, 0.25), 0.03)
        cr.rectangle(x + 3, y + i, w - 6, 1)
        cr.fill()
    for i in range(1, 6):
        xx = x + 6 + (w - 12) * i / 6
        set_color(cr, color, 0.08)
        cr.set_line_width(1)
        cr.move_to(xx, y + 4)
        cr.line_to(xx, y + h - 4)
        cr.stroke()
    if curve:
        pts = []
        for i in range(120):
            t = i / 119.0
            f = 20.0 * (1000.0 ** t)
            db = (2.0 * math.exp(-((math.log10(f / 80.0)) ** 2) / 0.05)
                  - 3.0 * math.exp(-((math.log10(f / 450.0)) ** 2) / 0.05)
                  + 2.0 * math.exp(-((math.log10(f / 3000.0)) ** 2) / 0.08)
                  + 2.5 * math.exp(-((math.log10(f / 11000.0)) ** 2) / 0.05))
            px = x + 6 + (w - 12) * t
            py = y + h / 2 + 6 - db * (h - 26) / 20.0
            pts.append((px, py))
        set_color(cr, color, 0.25)
        cr.set_line_width(4)
        for i, (px, py) in enumerate(pts):
            cr.move_to(px, py) if i == 0 else cr.line_to(px, py)
        cr.stroke()
        set_color(cr, color, 0.9)
        cr.set_line_width(1.4)
        for i, (px, py) in enumerate(pts):
            cr.move_to(px, py) if i == 0 else cr.line_to(px, py)
        cr.stroke()
    text(cr, title, x + 8, y + 13, 8, color, mono=True)
    if big is not None:
        text(cr, big, x + w - 10, y + 34, 17, color, mono=True, align="right")
    yy = y + h - 6
    for line in reversed(lines):
        text(cr, line, x + 8, yy, 7, color, mono=True)
        yy -= 9

def draw():
    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
    cr = cairo.Context(surface)

    # chassis background
    g = cairo.LinearGradient(0, 0, 0, H)
    g.add_color_stop_rgb(0, *BG_TOP)
    g.add_color_stop_rgb(1, *BG_BOT)
    cr.set_source(g)
    cr.paint()

    # brushed metal texture
    for i in range(260):
        yy = i * H / 260.0
        a = 0.012 + 0.018 * (0.5 + 0.5 * math.sin(i * 12.9898))
        set_color(cr, (1, 1, 1), a)
        cr.rectangle(0, yy, W, 1)
        cr.fill()

    # outer chassis edge
    rounded_rect(cr, 8, 8, W - 16, H - 16, 10)
    set_color(cr, (0.24, 0.24, 0.26), 0.9)
    cr.set_line_width(2)
    cr.stroke()

    # rack ears
    for x in (16, W - 56):
        panel_bg(cr, x, 20, 40, 58, 6, top=(0.18, 0.18, 0.19), bot=(0.07, 0.07, 0.08))
        for i, yy in enumerate((32, 49, 66)):
            set_color(cr, (0.02, 0.02, 0.02))
            cr.arc(x + 20, yy, 5.5, 0, 2 * math.pi)
            cr.fill()
            set_color(cr, METAL, 0.8)
            cr.arc(x + 20, yy, 5.0, 0, 2 * math.pi)
            cr.set_line_width(1)
            cr.stroke()

    # header
    panel_bg(cr, 62, 20, W - 124, 58, 8, top=(0.16, 0.16, 0.17), bot=(0.07, 0.07, 0.08))
    text(cr, "FIReous", 82, 52, 30, CREAM)
    text(cr, "FIR-5", 236, 38, 11, COPPER, mono=True)
    text(cr, "IIR / LINEAR", 236, 54, 8, DIM, mono=True)
    led(cr, 330, 44, 4.2, GREEN, True)
    text(cr, "POWER", 340, 48, 8, DIM, mono=True)

    # header mini LCD
    rounded_rect(cr, 370, 28, 300, 42, 4)
    set_color(cr, (0.02, 0.035, 0.025))
    cr.fill_preserve()
    set_color(cr, (0.22, 0.30, 0.22), 0.8)
    cr.set_line_width(1)
    cr.stroke()
    text(cr, "LATENCY 0.1 ms", 384, 48, 14, (0.60, 1.00, 0.60), mono=True)
    text(cr, "IIR MODE  ·  48.0 kHz", 384, 63, 8, (0.35, 0.75, 0.40), mono=True)

    # preset bar
    px = 690
    button(cr, px, 30, 32, 38, "<", size=12)
    button(cr, px + 38, 30, 32, 38, ">", size=12)
    rounded_rect(cr, px + 80, 30, 200, 38, 4)
    set_color(cr, (0.06, 0.06, 0.07))
    cr.fill_preserve()
    set_color(cr, PANEL_EDGE, 0.8)
    cr.set_line_width(1)
    cr.stroke()
    text(cr, "PRESET", px + 92, 44, 7, DIM, mono=True)
    text(cr, "INIT", px + 92, 60, 13, TEXT)
    button(cr, px + 290, 30, 64, 38, "SAVE")
    button(cr, px + 362, 30, 74, 38, "DELETE", accent=COPPER)
    button(cr, px + 444, 30, 64, 38, "UI 100%", accent=BLUE)

    # signal flow strip
    sy = 92
    text(cr, "SIGNAL FLOW", 24, sy + 22, 8, DIM, mono=True)
    flow = ["IN", "EQ", "PEQ", "DELAY", "COMP", "LIMIT", "OUT"]
    x0 = 130
    bw = 118
    gap = 26
    for i, name in enumerate(flow):
        x = x0 + i * (bw + gap)
        active = name not in ("IN", "OUT")
        rounded_rect(cr, x, sy, bw, 30, 4)
        set_color(cr, (0.10, 0.10, 0.11))
        cr.fill_preserve()
        set_color(cr, COPPER if active else PANEL_EDGE, 0.8)
        cr.set_line_width(1.2)
        cr.stroke()
        led(cr, x + 12, sy + 15, 3.2, GREEN if active else DIM, active)
        text(cr, name, x + 24, sy + 19, 9, TEXT if active else DIM, mono=True)
        if i < len(flow) - 1:
            ax = x + bw + 5
            ay = sy + 15
            set_color(cr, DIM, 0.7)
            cr.set_line_width(1.5)
            cr.move_to(ax, ay)
            cr.line_to(ax + gap - 10, ay)
            cr.stroke()
            cr.move_to(ax + gap - 14, ay - 4)
            cr.line_to(ax + gap - 10, ay)
            cr.line_to(ax + gap - 14, ay + 4)
            cr.stroke()

    # module grid (small LCDs inside each module, no full-width graph)
    gx, gy = 24, 136
    gap = 12
    cw = (W - 48 - 2 * gap) / 3.0
    rh = (H - 24 - gy - gap) / 2.0

    # row 1: EQ spans 2 cols, PEQ 1 col
    eq_w = cw * 2 + gap
    module_card(cr, gx, gy, eq_w, rh, "GRAPHIC EQ", 1, AMBER)
    module_lcd(cr, gx + 18, gy + 42, eq_w - 36, 58, "10-BAND GRAPHIC",
               lines=("MAG", "±12 dB"), curve=True, color=GREEN)
    button(cr, gx + eq_w - 92, gy + 106, 72, 22, "FLAT", accent=COPPER, size=9)
    fader_y = gy + 142
    fader_h = rh - 186
    for i in range(10):
        fx = gx + 54 + i * ((eq_w - 108) / 9.0)
        val = (math.sin(i * 0.8) * 0.18 + 0.5)
        fader(cr, fx - 18, fader_y, 36, fader_h, val,
              ["31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"][i])

    module_card(cr, gx + eq_w + gap, gy, cw, rh, "PARAMETRIC EQ", 2, COPPER)
    module_lcd(cr, gx + eq_w + gap + 18, gy + 42, cw - 36, 58, "6-BAND PEQ",
               lines=("B2 BELL", "HP 20 Hz · LP 20 kHz"), curve=True, color=GREEN)
    bx = gx + eq_w + gap + 18
    for i in range(6):
        button(cr, bx + i * 58, gy + 112, 48, 22, f"B{i+1}", active=(i == 1), accent=COPPER, size=8)
    kx = gx + eq_w + gap + cw / 2
    knob(cr, kx - 80, gy + 190, 27, 0.62, "FREQ", COPPER)
    knob(cr, kx, gy + 190, 27, 0.55, "GAIN", COPPER)
    knob(cr, kx + 80, gy + 190, 27, 0.40, "Q", COPPER)
    button(cr, bx, gy + rh - 42, 72, 24, "HP 20", accent=COPPER, size=9)
    button(cr, bx + 80, gy + rh - 42, 72, 24, "LP 20k", accent=COPPER, size=9)
    text(cr, "TYPE: BELL", bx + 170, gy + rh - 26, 8, DIM, mono=True)

    # row 2: delay, compressor, limiter
    ry = gy + rh + gap
    module_card(cr, gx, ry, cw, rh, "DELAY", 3, BLUE)
    module_lcd(cr, gx + 18, ry + 42, cw - 36, 58, "STEREO DELAY",
               big="250 ms", lines=("SYNC 1/8", "ALIGN L 0.0  R 0.0"), color=AMBER)
    knob(cr, gx + 62, ry + 170, 25, 0.42, "TIME", BLUE)
    knob(cr, gx + 152, ry + 170, 25, 0.35, "FDBK", BLUE)
    knob(cr, gx + 242, ry + 170, 25, 0.20, "P-PONG", BLUE)
    knob(cr, gx + 332, ry + 170, 25, 0.28, "MIX", BLUE)
    knob(cr, gx + 92, ry + 268, 20, 0.15, "ALIGN L", BLUE)
    knob(cr, gx + 202, ry + 268, 20, 0.25, "ALIGN R", BLUE)
    button(cr, gx + 24, ry + rh - 42, 80, 24, "TAP", accent=BLUE, size=9)
    led(cr, gx + 116, ry + rh - 30, 3.5, BLUE, True)
    text(cr, "MODE: EFFECT+ALIGN", gx + 126, ry + rh - 26, 8, DIM, mono=True)

    module_card(cr, gx + cw + gap, ry, cw, rh, "COMPRESSOR", 4, GREEN)
    cig = gx + cw + gap
    module_lcd(cr, cig + 18, ry + 42, cw - 96, 58, "COMPRESSOR",
               big="-3.2 dB", lines=("GR", "RMS · LINK 100%"), color=AMBER)
    meter(cr, cig + cw - 46, ry + 42, 16, rh - 120, 0.35)
    knob(cr, cig + 44, ry + 170, 22, 0.55, "THRESH", GREEN)
    knob(cr, cig + 114, ry + 170, 22, 0.40, "RATIO", GREEN)
    knob(cr, cig + 184, ry + 170, 22, 0.30, "ATTACK", GREEN)
    knob(cr, cig + 254, ry + 170, 22, 0.55, "RELEASE", GREEN)
    knob(cr, cig + 324, ry + 170, 22, 0.20, "MAKEUP", GREEN)
    button(cr, cig + 18, ry + rh - 42, 80, 24, "PEAK/RMS", accent=GREEN, size=8)
    button(cr, cig + 106, ry + rh - 42, 80, 24, "AUTO MK", accent=GREEN, size=8)

    module_card(cr, gx + 2 * (cw + gap), ry, cw, rh, "LIMITER", 5, RED)
    lig = gx + 2 * (cw + gap)
    module_lcd(cr, lig + 18, ry + 42, cw - 96, 58, "LIMITER",
               big="-0.3 dB", lines=("CEILING", "SAMPLE · LA 0 ms"), color=AMBER)
    meter(cr, lig + cw - 46, ry + 42, 16, rh - 120, 0.20)
    knob(cr, lig + 90, ry + 170, 27, 0.85, "CEILING", RED)
    knob(cr, lig + 220, ry + 170, 27, 0.45, "RELEASE", RED)
    button(cr, lig + 18, ry + rh - 42, 90, 24, "TRUE PEAK", accent=RED, size=8)
    button(cr, lig + 116, ry + rh - 42, 80, 24, "LA 0 ms", accent=RED, size=8)
    led(cr, lig + 24, ry + rh - 70, 4.0, GREEN, True)
    text(cr, "SAFETY", lig + 34, ry + rh - 66, 8, DIM, mono=True)

    # chassis screws
    for sx, sy in ((20, 20), (W - 20, 20), (20, H - 20), (W - 20, H - 20)):
        screw(cr, sx, sy, 5)

    # footer
    text(cr, "FIReous  ·  IIR / LINEAR-PHASE ENGINE  ·  VST3 / LV2 / CLAP / JACK", W / 2, H - 8, 7, DIM, align="center", mono=True)

    surface.write_to_png(OUT)
    print("wrote", OUT)


if __name__ == "__main__":
    draw()
