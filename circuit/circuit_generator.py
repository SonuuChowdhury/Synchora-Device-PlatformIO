#!/usr/bin/env python3
"""
Generate the ESP32 Synchora device circuit schematic as SVG.

================================================================================
 READ THIS FIRST — instructions for future AI agents / developers
================================================================================

WHAT THIS IS
  This script is the SOURCE OF TRUTH that generates the wiring diagram used
  inside `circuit.html`. Do NOT hand-edit the <svg>...</svg> block inside
  circuit.html directly — it is generated output and will be overwritten.
  Instead, edit this script and re-run it, then paste the new SVG into
  circuit.html (or run the accompanying build step, see "HOW TO REGENERATE").

SOURCE OF TRUTH FOR PIN NUMBERS
  All GPIO numbers below must match the firmware's `config/pins.h`. If pins.h
  changes, update `pins_left` / `pins_right` here first, then everything else
  (wire routing, tables in circuit.html) should be updated to match.

HOW TO REGENERATE circuit.html AFTER EDITING THIS FILE
  1. Run:  python3 circuit_generator.py
     -> writes circuit_diagram.svg (or circuit_preview.svg, see bottom of file)
  2. Open circuit.html and replace the contents between
     <div class="diagram-shell"> ... </div> with the freshly generated SVG.
  3. If you added/removed a peripheral or pin, also update the two
     "GPIO Pin Map" tables and the "Peripheral Power & Ground" table further
     down in circuit.html so the written reference stays in sync with the
     picture.

COORDINATE SYSTEM (viewBox 0 0 1800 1120)
  - BUS_TOP (y=108)   : horizontal 3V3 power rail, spans full width
  - BUS_BOT (y=908)   : horizontal GND rail, spans full width
  - ESP32 box         : x=780..1020, y=200..800 (BOX_X, BOX_Y, BOX_W, BOX_H)
  - LEFT side (x < 700)   is reserved for "Human Interface" — LEDs + buttons.
    Pin list: `pins_left`, six rows spaced 100px apart (y = 260..760).
  - RIGHT side (x > 1100) is reserved for "Sensors" — DHT22 / GPS / I2S mic.
    Pin list: `pins_right`, same y-spacing as the left side.
  - Everything is wired with orthogonal (Manhattan-style) lines via the
    `line()` / `polyline()` helpers — no diagonal wires except the short
    "fan out" lines connecting a module's internal pin row to the matching
    ESP32 pin on the box edge (kept deliberately short & shallow-angled so
    they stay readable).

HOW TO ADD A NEW GPIO-DRIVEN COMPONENT (e.g. another LED or button)
  1. Add a tuple to `pins_left` or `pins_right`, e.g.
       ("GPIO27", "Buzzer", 860, C_GPIO)
     — pick a free y slot (each side currently has 6 rows @ 100px pitch;
     if you add a 7th row you'll need to grow BOX_H / shift BUS_BOT down).
  2. Call the matching helper for it: `left_led(y, ...)`, `left_button(y, ...)`,
     or write a similar helper if it's a new component type.
  3. Re-run the script, verify no wires overlap (render to PNG with cairosvg
     to eyeball it — see the bottom of this file / conversation history for
     the render-and-crop workflow used during development).

HOW TO ADD A NEW MULTI-PIN MODULE (e.g. another I2C sensor)
  1. Pick free vertical space on the right (or left) OUTSIDE the ESP32 box's
     x-range (x < 700 or x > 1100) so bus stub lines never cross the box.
  2. Use `module_box(x, y, w, h, title, subtitle)` to draw the housing.
  3. List its pins with `dot()` + `text()` calls (see the DHT22 / GPS / mic
     sections for the pattern), then wire VCC/GND up/down to BUS_TOP/BUS_BOT
     with straight vertical `line()` calls at a dedicated x offset so the
     stub doesn't collide with neighboring modules' stubs.
  4. Wire signal pins across to their ESP32 GPIO with a two-segment
     `line()` (diagonal fan-in, then horizontal run into the box) exactly
     like the existing WS/SCK/SD or TX/RX examples.

COLOR CODING (must stay consistent with the legend in circuit.html)
  C_3V3  = 3.3V power      C_GND  = ground
  C_5V   = VIN/5V (unused, informational only)
  C_GPIO = generic digital GPIO (LEDs, buttons, single-wire sensors)
  C_UART = UART serial (GPS)      C_I2S = I2S bus (microphone)
  If you add a new bus type (e.g. I2C), add a new C_* constant, use it
  consistently, and add a matching row to the legend in circuit.html.

RENDERING / PREVIEWING WHILE EDITING
  This script only writes an .svg file — it does not render a PNG itself.
  To visually check your changes before committing them:
      pip install cairosvg --break-system-packages
      python3 -c "import cairosvg; cairosvg.svg2png(url='circuit_diagram.svg', write_to='preview.png', output_width=1800)"
  Then view preview.png (crop with PIL into quadrants for a closer look —
  the full 1800x1120 canvas is dense and easy to misjudge at thumbnail size).

FILES IN THIS DELIVERY
  circuit.html          - the final page a human opens in a browser
                           (embeds the SVG below + reference tables + legend)
  circuit_generator.py   - this script (source of truth, safe to re-run)
  circuit_diagram.svg    - the last SVG this script produced, standalone
================================================================================
"""

# ---------------------------------------------------------------- palette
BG          = "#0b1220"
BG_GRID     = "#141d31"
PANEL       = "#111a2e"
PANEL_EDGE  = "#2a3a5c"
BOX_FILL    = "#16213a"
BOX_EDGE    = "#4f7cff"
TEXT_MAIN   = "#e8edf9"
TEXT_DIM    = "#7f8cab"
TEXT_LABEL  = "#a9b6d6"

C_3V3   = "#ff5d73"   # power (3.3V)
C_5V    = "#ff9f43"   # VIN / 5V
C_GND   = "#8ba3c7"   # ground
C_GPIO  = "#39c5ff"   # generic digital gpio
C_UART  = "#ffd166"   # uart (gps)
C_I2S   = "#c792ea"   # i2s (mic)
C_LED   = "#5eead4"

FONT = "'JetBrains Mono','Fira Code',monospace"

W, H = 1800, 1120
BUS_TOP = 108
BUS_BOT = 908
BOX_X, BOX_Y, BOX_W, BOX_H = 780, 200, 240, 600

pins_left = [
    ("GPIO2",  "WiFi Status LED",      260, C_GPIO),
    ("GPIO4",  "Recording LED",        360, C_GPIO),
    ("GPIO14", "Socket Connected LED", 460, C_GPIO),
    ("GPIO22", "Emergency LED",        560, C_GPIO),
    ("GPIO13", "Recording Button",     660, C_GPIO),
    ("GPIO23", "Emergency Button",     760, C_GPIO),
]
pins_right = [
    ("GPIO5",  "DHT22 Data",              260, C_GPIO),
    ("GPIO16", "UART2 RX  (\u2190 GPS TX)",     360, C_UART),
    ("GPIO17", "UART2 TX  (\u2192 GPS RX)",     460, C_UART),
    ("GPIO15", "I2S WS  (Mic Word Select)",560, C_I2S),
    ("GPIO26", "I2S SCK (Mic Bit Clock)", 660, C_I2S),
    ("GPIO32", "I2S SD  (Mic Data Out)",  760, C_I2S),
]

svg = []
def add(s): svg.append(s)

def text(x, y, s, size=15, color=TEXT_MAIN, anchor="start", weight="400", family=FONT, style=""):
    add(f'<text x="{x}" y="{y}" font-family="{family}" font-size="{size}" '
        f'fill="{color}" text-anchor="{anchor}" font-weight="{weight}" style="{style}">{s}</text>')

def line(x1, y1, x2, y2, color, width=2.4, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    add(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
        f'stroke-width="{width}" stroke-linecap="round"{d}/>')

def polyline(pts, color, width=2.4):
    p = " ".join(f"{x},{y}" for x, y in pts)
    add(f'<polyline points="{p}" fill="none" stroke="{color}" stroke-width="{width}" '
        f'stroke-linecap="round" stroke-linejoin="round"/>')

def dot(x, y, color, r=4.5):
    add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')

def resistor(cx, cy, w=64, h=18, color=TEXT_MAIN, label="", horizontal=True):
    """Zig-zag resistor symbol centered at (cx,cy)."""
    if horizontal:
        x0 = cx - w/2
        seg = w/6
        pts = [(x0, cy)]
        for i in range(6):
            pts.append((x0 + seg*(i+1), cy + (h/2 if i % 2 == 0 else -h/2)))
        pts.append((x0 + w, cy))
        # fix endpoints to be flat
        pts[0] = (x0, cy)
        pts[-1] = (x0 + w, cy)
        polyline(pts, color, 2.2)
        if label:
            text(cx, cy - h/2 - 8, label, 13, TEXT_LABEL, "middle")
        return x0, x0 + w
    else:
        y0 = cy - w/2
        seg = w/6
        pts = [(cx, y0)]
        for i in range(6):
            pts.append((cx + (h/2 if i % 2 == 0 else -h/2), y0 + seg*(i+1)))
        pts.append((cx, y0 + w))
        pts[0] = (cx, y0)
        pts[-1] = (cx, y0 + w)
        polyline(pts, color, 2.2)
        if label:
            text(cx + h/2 + 34, cy, label, 13, TEXT_LABEL, "middle")
        return y0, y0 + w

def led(cx, cy, color, flip=False):
    """LED diode symbol (triangle + bar), horizontal, current flows left->right unless flip."""
    r = 15
    if not flip:
        p1 = (cx - r, cy - r); p2 = (cx - r, cy + r); p3 = (cx + r, cy)
        bx = cx + r
    else:
        p1 = (cx + r, cy - r); p2 = (cx + r, cy + r); p3 = (cx - r, cy)
        bx = cx - r
    add(f'<polygon points="{p1[0]},{p1[1]} {p2[0]},{p2[1]} {p3[0]},{p3[1]}" '
        f'fill="{color}" fill-opacity="0.85" stroke="{color}" stroke-width="1.5"/>')
    line(bx, cy - r, bx, cy + r, color, 3)
    # little emitted-light arrows
    ax = bx + (10 if not flip else -10)
    for off in (-8, 2):
        add(f'<line x1="{ax+off-4}" y1="{cy-r-4+off*0.2}" x2="{ax+off+6}" y2="{cy-r-14+off*0.2}" '
            f'stroke="{color}" stroke-width="1.4"/>')

def switch(cx, cy):
    """Push-button symbol, horizontal."""
    line(cx-22, cy, cx-8, cy, TEXT_MAIN, 2.2)
    line(cx+8, cy, cx+22, cy, TEXT_MAIN, 2.2)
    line(cx-8, cy+9, cx+10, cy-11, TEXT_MAIN, 2.6)
    dot(cx-8, cy, TEXT_MAIN, 3)
    dot(cx+8, cy, TEXT_MAIN, 3)
    add(f'<line x1="{cx-4}" y1="{cy-16}" x2="{cx-4}" y2="{cy-24}" stroke="{TEXT_MAIN}" stroke-width="2"/>')
    add(f'<line x1="{cx+12}" y1="{cy-16}" x2="{cx+12}" y2="{cy-24}" stroke="{TEXT_MAIN}" stroke-width="2"/>')

def module_box(x, y, w, h, title, subtitle):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="10" fill="{BOX_FILL}" '
        f'stroke="{BOX_EDGE}" stroke-width="1.6"/>')
    text(x + w/2, y + 26, title, 16, TEXT_MAIN, "middle", "700")
    text(x + w/2, y + 45, subtitle, 12, TEXT_DIM, "middle")

# =====================================================================
add(f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg" font-family="{FONT}">')

# background + subtle grid
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add('<defs>')
add(f'<pattern id="grid" width="32" height="32" patternUnits="userSpaceOnUse">'
    f'<path d="M 32 0 L 0 0 0 32" fill="none" stroke="{BG_GRID}" stroke-width="1"/></pattern>')
add('<marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
    f'<path d="M0,0 L10,5 L0,10 z" fill="{TEXT_LABEL}"/></marker>')
add('</defs>')
add(f'<rect width="{W}" height="{H}" fill="url(#grid)"/>')

# title
text(W/2, 42, "SYNCHORA \u2014 ESP32 WEARABLE DEVICE  \u2014  FULL CIRCUIT SCHEMATIC", 24, TEXT_MAIN, "middle", "700")
text(W/2, 66, "Auto-generated from firmware pin map (config/pins.h)  \u2022  MCU: ESP32 DevKit", 13, TEXT_DIM, "middle")

# ---------------------------------------------------------------- buses
line(60, BUS_TOP, W-60, BUS_TOP, C_3V3, 4)
line(60, BUS_BOT, W-60, BUS_BOT, C_GND, 4)
text(70, BUS_TOP-12, "3V3 POWER BUS", 14, C_3V3, "start", "700")
text(70, BUS_BOT+24, "GND BUS (COMMON GROUND)", 14, C_GND, "start", "700")

# ESP32 <-> bus stubs (near box, top-left / bottom-left corners)
esp_3v3_x = BOX_X + 40
esp_gnd_x = BOX_X + 90
line(esp_3v3_x, BUS_TOP, esp_3v3_x, BOX_Y, C_3V3, 3)
dot(esp_3v3_x, BUS_TOP, C_3V3)
text(esp_3v3_x, BOX_Y-10, "3V3", 12, C_3V3, "middle", "700")
line(esp_gnd_x, BUS_BOT, esp_gnd_x, BOX_Y+BOX_H, C_GND, 3)
dot(esp_gnd_x, BUS_BOT, C_GND)
text(esp_gnd_x, BOX_Y+BOX_H+18, "GND", 12, C_GND, "middle", "700")
line(esp_3v3_x+30, BUS_BOT, esp_3v3_x+30, BOX_Y+BOX_H, C_5V, 3, dash="5,4")
dot(esp_3v3_x+30, BUS_BOT, C_5V)
text(esp_3v3_x+30, BOX_Y+BOX_H+18, "VIN", 12, C_5V, "middle", "700")

# ---------------------------------------------------------------- ESP32 box
add(f'<rect x="{BOX_X}" y="{BOX_Y}" width="{BOX_W}" height="{BOX_H}" rx="14" '
    f'fill="#132038" stroke="{BOX_EDGE}" stroke-width="2.4"/>')
text(BOX_X+BOX_W/2, BOX_Y+34, "ESP32 DevKit-V1", 20, TEXT_MAIN, "middle", "700")
text(BOX_X+BOX_W/2, BOX_Y+56, "(30-pin \u2014 pins not in use are hidden)", 12, TEXT_DIM, "middle")
add(f'<circle cx="{BOX_X+24}" cy="{BOX_Y+24}" r="5" fill="#243554"/>')

# left pins
for label, desc, y, color in pins_left:
    line(BOX_X, y, BOX_X-80, y, color, 2.6)
    dot(BOX_X, y, color, 3.5)
    text(BOX_X-88, y+5, label, 14, color, "end", "700")

# right pins
for label, desc, y, color in pins_right:
    line(BOX_X+BOX_W, y, BOX_X+BOX_W+80, y, color, 2.6)
    dot(BOX_X+BOX_W, y, color, 3.5)
    text(BOX_X+BOX_W+88, y+5, label, 14, color, "start", "700")

# =====================================================================
# LEFT SIDE PERIPHERALS  (Human-interface: LEDs + Buttons)
# =====================================================================
text(360, 168, "HUMAN INTERFACE \u2014 STATUS LEDs &amp; BUTTONS", 14, TEXT_LABEL, "middle", "700")

def left_led(y, gpio_color, name, res_label="220\u03a9"):
    pin_x = BOX_X - 80          # 700
    led_x = 560
    res_x = 430
    gnd_x = 300
    line(pin_x, y, led_x+16, y, gpio_color, 2.6)
    led(led_x, y, C_LED, flip=True)     # anode faces right (toward MCU), cathode faces left
    line(led_x-16, y, res_x+32, y, C_LED, 2.4)
    resistor(res_x, y, 64, 16, TEXT_MAIN, res_label, horizontal=True)
    line(res_x-32, y, gnd_x, y, C_GND, 2.4)
    line(gnd_x, y, gnd_x, BUS_BOT, C_GND, 2.4)
    dot(gnd_x, BUS_BOT, C_GND)
    text(pin_x-8, y-20, name, 14, TEXT_MAIN, "end", "700")

left_led(260, C_GPIO, "WiFi Status LED  (D1, blinks while connecting)")
left_led(360, C_GPIO, "Recording LED  (D2, on while capturing audio)")
left_led(460, C_GPIO, "Socket Connected LED  (D3, solid = WS linked)")
left_led(560, C_GPIO, "Emergency LED  (D4, fast-blink = SOS armed)")

def left_button(y, name, note):
    pin_x = BOX_X - 80
    btn_x = 560
    gnd_x = 430
    line(pin_x, y, btn_x+26, y, C_GPIO, 2.6)
    switch(btn_x, y)
    line(btn_x-26, y, gnd_x, y, C_GND, 2.4)
    line(gnd_x, y, gnd_x, BUS_BOT, C_GND, 2.4)
    dot(gnd_x, BUS_BOT, C_GND)
    text(pin_x-8, y-20, name, 14, TEXT_MAIN, "end", "700")
    text(pin_x-8, y+34, note, 11.5, TEXT_DIM, "end")

left_button(660, "Recording Button  (SW1)", "INPUT_PULLUP \u2014 pressed = LOW, other leg \u2192 GND")
left_button(760, "Emergency Button  (SW2)", "Hold 5s = trigger SOS, hold 2s while active = cancel")

# =====================================================================
# RIGHT SIDE PERIPHERALS  (Sensors: DHT22 / GPS / I2S mic)
# =====================================================================
text(1440, 168, "SENSORS \u2014 ENVIRONMENT, LOCATION &amp; AUDIO", 14, TEXT_LABEL, "middle", "700")

# ---- DHT22 ----
dx, dy, dw, dh = 1230, 210, 240, 130
module_box(dx, dy, dw, dh, "DHT22", "Temperature &amp; Humidity Sensor")
pin_defs = [("VCC", C_3V3, dy+70), ("DATA", C_GPIO, dy+92), ("GND", C_GND, dy+114)]
for lbl, col, py in pin_defs:
    dot(dx, py, col, 3.2)
    text(dx+14, py+4, lbl, 12, col, "start", "700")
# VCC up to bus
line(dx, dy+70, dx-40, dy+70, C_3V3, 2.4)
line(dx-40, dy+70, dx-40, BUS_TOP, C_3V3, 2.4)
dot(dx-40, BUS_TOP, C_3V3)
# GND down to bus
line(dx, dy+114, dx-40, dy+114, C_GND, 2.4)
line(dx-40, dy+114, dx-40, BUS_BOT, C_GND, 2.4)
dot(dx-40, BUS_BOT, C_GND)
# pull-up resistor from VCC rail to DATA line (required for 1-wire DHT bus)
pu_x = dx - 90
res_top, res_bot = resistor(pu_x, dy+95, 50, 14, TEXT_MAIN, "", horizontal=False)
line(pu_x, dy+70, pu_x, res_top, C_3V3, 2.2)          # VCC tap -> resistor top
line(pu_x, dy+70, dx-40, dy+70, C_3V3, 2.2)           # jog to join the VCC vertical run
line(pu_x, res_bot, pu_x, dy+92, C_GPIO, 2.2)         # resistor bottom -> DATA line
line(pu_x, dy+92, dx, dy+92, C_GPIO, 2.2)
text(pu_x-14, dy+95, "10k\u03a9", 12, TEXT_LABEL, "end")
text(pu_x-14, dy+112, "pull-up", 11, TEXT_DIM, "end")
# DATA to GPIO5
line(dx+dw, dy+92, 1100, 260, C_GPIO, 2.6)
line(1100, 260, BOX_X+BOX_W, 260, C_GPIO, 2.6)

# ---- GPS NEO-6M ----
gx, gy, gw, gh = 1230, 400, 240, 140
module_box(gx, gy, gw, gh, "NEO-6M GPS", "UART \u2192 HardwareSerial2 @ 9600 baud")
gp = [("VCC", C_3V3, gy+68), ("TX", C_UART, gy+90), ("RX", C_UART, gy+112), ("GND", C_GND, gy+132)]
for lbl, col, py in gp:
    dot(gx, py, col, 3.2)
    text(gx+14, py+4, lbl, 12, col, "start", "700")
line(gx, gy+68, gx-56, gy+68, C_3V3, 2.4)
line(gx-56, gy+68, gx-56, BUS_TOP, C_3V3, 2.4)
dot(gx-56, BUS_TOP, C_3V3)
line(gx, gy+132, gx-56, gy+132, C_GND, 2.4)
line(gx-56, gy+132, gx-56, BUS_BOT, C_GND, 2.4)
dot(gx-56, BUS_BOT, C_GND)
# GPS TX -> ESP32 GPIO16 (RX2)
line(gx+gw, gy+90, 1140, 360, C_UART, 2.6)
line(1140, 360, BOX_X+BOX_W, 360, C_UART, 2.6)
# ESP32 GPIO17 (TX2) -> GPS RX
line(BOX_X+BOX_W, 460, 1170, 460, C_UART, 2.6)
line(1170, 460, gx+gw, gy+112, C_UART, 2.6)

# ---- INMP441 I2S mic ----
mx, my, mw, mh = 1230, 580, 240, 210
module_box(mx, my, mw, mh, "INMP441", "I2S MEMS Microphone")
mp = [("VDD", C_3V3, my+66), ("WS",  C_I2S, my+90), ("SCK", C_I2S, my+114),
      ("SD",  C_I2S, my+138), ("L/R", C_GND, my+162), ("GND", C_GND, my+184)]
for lbl, col, py in mp:
    dot(mx, py, col, 3.2)
    text(mx+14, py+4, lbl, 12, col, "start", "700")
line(mx, my+66, mx-70, my+66, C_3V3, 2.4)
line(mx-70, my+66, mx-70, BUS_TOP, C_3V3, 2.4)
dot(mx-70, BUS_TOP, C_3V3)
line(mx, my+184, mx-70, my+184, C_GND, 2.4)
line(mx-70, my+184, mx-70, BUS_BOT, C_GND, 2.4)
dot(mx-70, BUS_BOT, C_GND)
# L/R tied to GND too (selects channel read out)
line(mx, my+162, mx-40, my+162, C_GND, 2.2)
line(mx-40, my+162, mx-40, BUS_BOT, C_GND, 2.2)
dot(mx-40, BUS_BOT, C_GND)
text(mx-46, my+162-8, "channel select", 10.5, TEXT_DIM, "end")
# WS -> GPIO15, SCK -> GPIO26, SD -> GPIO32
line(mx+mw, my+90, 1130, 560, C_I2S, 2.6)
line(1130, 560, BOX_X+BOX_W, 560, C_I2S, 2.6)
line(mx+mw, my+114, 1160, 660, C_I2S, 2.6)
line(1160, 660, BOX_X+BOX_W, 660, C_I2S, 2.6)
line(mx+mw, my+138, 1190, 760, C_I2S, 2.6)
line(1190, 760, BOX_X+BOX_W, 760, C_I2S, 2.6)

# =====================================================================
# LEGEND
# =====================================================================
lx, ly, lw, lh = 60, 940, 1680, 150
add(f'<rect x="{lx}" y="{ly}" width="{lw}" height="{lh}" rx="12" fill="{PANEL}" stroke="{PANEL_EDGE}" stroke-width="1.4"/>')
text(lx+24, ly+30, "LEGEND", 15, TEXT_MAIN, "start", "700")

legend_items = [
    (C_3V3, "3.3V Power"),
    (C_5V,  "VIN / 5V (unused rail, brought out for reference)"),
    (C_GND, "Ground (GND)"),
    (C_GPIO,"Digital GPIO (LED / Button)"),
    (C_UART,"UART Serial (GPS NEO-6M)"),
    (C_I2S, "I2S Bus (INMP441 Mic)"),
]
lyy = ly + 55
lxx = lx + 24
for col, lbl in legend_items:
    line(lxx, lyy, lxx+34, lyy, col, 4)
    text(lxx+44, lyy+5, lbl, 13, TEXT_LABEL, "start")
    lyy += 26
    if lyy > ly+lh-20:
        lyy = ly+55
        lxx += 520

text(lx+960, ly+30, "DEVICE ID", 15, TEXT_MAIN, "start", "700")
text(lx+960, ly+55, "synchora84205@!&amp;100@!%device", 12.5, TEXT_DIM, "start")
text(lx+960, ly+80, "WI-FI", 15, TEXT_MAIN, "start", "700")
text(lx+960, ly+104, "SSID: Realme11x   (WPA2, credentials in wifiConfig.h)", 12.5, TEXT_DIM, "start")

text(lx+1360, ly+30, "SERVER LINK", 15, TEXT_MAIN, "start", "700")
text(lx+1360, ly+55, "WebSocket \u2192 ws://...ngrok-free.app", 12.5, TEXT_DIM, "start")
text(lx+1360, ly+80, "Sends TOKEN on connect, telemetry every 60s", 12.5, TEXT_DIM, "start")
text(lx+1360, ly+104, "Reconnect: WiFi 5s check / WS auto 3s retry", 12.5, TEXT_DIM, "start")

add('</svg>')

svg_out = "\n".join(svg)
with open("/home/claude/circuit_preview.svg", "w", encoding="utf-8") as f:
    f.write(svg_out)
print("wrote", len(svg_out), "bytes")
