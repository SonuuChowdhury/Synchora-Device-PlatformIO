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
  1. Run:  python circuit_generator.py
     -> writes circuit_diagram.svg in the script directory
  2. Open circuit.html and replace the contents between
     <div class="diagram-shell"> ... </div> with the freshly generated SVG.

DESIGN & ROUTING PRINCIPLES
  - Clean GPIO lines without pin endpoint dots.
  - Main heading updated to "SYNCHORA — FULL CIRCUIT SCHEMATIC".
  - Clear, spacious Parallel Speaker stage (Speaker 1 & Speaker 2 boxes + OUT+/OUT- bus rails).
  - Power Supply Chain vertically distributed across Y=150..990 to fill left vacant space.
  - Includes MAX98357A I2S Class-D Audio Amplifier Module + Dual 16Ω Parallel Speakers (8Ω total).
  - ESP32 DevKit-V1 chip title centered right in the middle of the MCU box.
  - Compact 100px horizontal signal wire gap between ESP32 and Sensor Modules (X=1480).
  - Normal compact module card sizes with clear vertical gaps between cards.
  - ALL signal wires are 100% PURE HORIZONTAL LINES with ZERO TURNS.
  - DISTINCT COLOR ENCODING for EVERY SINGLE signal wire & power rail.
  - Power supply chain (18650 Pack → TP4056 → Slide Switch → MT3608) on far left.
  - Parallel Power bus rails at top (5V, 3.3V) and bottom (GND).
  - Dedicated routing corridors so vertical wires never cross pin labels.
  - Pin labels placed cleanly ABOVE wire lines.
  - Top header accent bars on cards to prevent vertical wire confusion.

COLOR PALETTE (Distinct wire color coding for every signal)
  C_5V       = 5V Power Bus         (#f87171 - Red)
  C_33V      = 3.3V Power Bus       (#fb923c - Orange)
  C_GND      = Ground Rail          (#6b7280 - Gray)
  C_BATT     = Raw Battery 3.7V     (#facc15 - Amber Yellow)
  C_SW       = Switched Power 3.7V  (#eab308 - Deep Amber)

  C_LED1     = WiFi Status LED      (#38bdf8 - Sky Blue)
  C_LED2     = Recording LED        (#4ade80 - Emerald Green)
  C_LED3     = Socket Connected LED (#a855f7 - Purple)
  C_LED4     = Emergency LED        (#f43f5e - Rose Red)
  C_BTN1     = Recording Button     (#34d399 - Mint Green)
  C_BTN2     = Emergency Button     (#fbbf24 - Gold Yellow)

  C_DHT      = DHT22 Data           (#22d3ee - Bright Cyan)
  C_GPS_TX   = GPS TX (UART RX)     (#38bdf8 - Light Blue)
  C_GPS_RX   = GPS RX (UART TX)     (#f472b6 - Pink)
  C_I2S_WS   = I2S WS (Mic Word Sel)(#a78bfa - Violet)
  C_I2S_SCK  = I2S SCK (Mic Bit Clk)(#c084fc - Lavender)
  C_I2S_SD   = I2S SD (Mic Data Out)(#e879f9 - Magenta)

  C_SPK_LRC  = I2S Speaker LRC/WS   (#34d399 - Mint Green)
  C_SPK_BCLK = I2S Speaker BCLK     (#fbbf24 - Gold Yellow)
  C_SPK_DIN  = I2S Speaker DIN      (#f472b6 - Pink)
  C_SPK_SD   = Speaker Mute/Shutdown(#a78bfa - Violet)
================================================================================
"""

import os

# ---------------------------------------------------------------- palette
BG          = "#0b1220"
BG_GRID     = "#141d31"
PANEL       = "#111a2e"
PANEL_EDGE  = "#2a3a5c"
BOX_FILL    = "#0f172a"
BOX_EDGE    = "#3b82f6"
TEXT_MAIN   = "#e8edf9"
TEXT_DIM    = "#7f8cab"
TEXT_LABEL  = "#a9b6d6"

# Distinct Wire & Power Rail Colors
C_5V       = "#f87171"   # 5V Power Rail (Red)
C_33V      = "#fb923c"   # 3.3V Power Rail (Orange)
C_GND      = "#6b7280"   # Common Ground (Gray)
C_BATT     = "#facc15"   # Raw Battery 3.7V (Amber)
C_SW       = "#eab308"   # Switched Battery 3.7V (Deep Amber)

# Left Side Human Interface Colors
C_LED1     = "#38bdf8"   # WiFi Status LED (Sky Blue)
C_LED2     = "#4ade80"   # Recording LED (Emerald Green)
C_LED3     = "#a855f7"   # Socket Connected LED (Purple)
C_LED4     = "#f43f5e"   # Emergency LED (Rose Red)
C_BTN1     = "#34d399"   # Recording Button (Mint Green)
C_BTN2     = "#fbbf24"   # Emergency Button (Gold Yellow)

# Right Side Sensor & Audio Colors
C_DHT      = "#22d3ee"   # DHT22 Data (Cyan)
C_GPS_TX   = "#38bdf8"   # GPS TX -> UART RX (Light Blue)
C_GPS_RX   = "#f472b6"   # ESP32 TX -> GPS RX (Pink)
C_I2S_WS   = "#a78bfa"   # Mic I2S Word Select (Violet)
C_I2S_SCK  = "#c084fc"   # Mic I2S Bit Clock (Lavender)
C_I2S_SD   = "#e879f9"   # Mic I2S Data Out (Magenta)

C_SPK_LRC  = "#34d399"   # Speaker LRC / WS (Mint Green)
C_SPK_BCLK = "#fbbf24"   # Speaker BCLK (Gold Yellow)
C_SPK_DIN  = "#f472b6"   # Speaker DIN (Pink)
C_SPK_SD   = "#a78bfa"   # Speaker SD Shutdown (Violet)

CPIN       = "#94a3b8"
FONT       = "'JetBrains Mono','Fira Code','Courier New',monospace"

W, H = 2400, 1380
BUS_5V  = 70
BUS_3V3 = 115
BUS_BOT = 1140
BOX_X, BOX_Y, BOX_W, BOX_H = 1000, 170, 280, 810

pins_left = [
    ("GPIO2",  "WiFi Status LED",      220, C_LED1),
    ("GPIO4",  "Recording LED",        340, C_LED2),
    ("GPIO14", "Socket Connected LED", 460, C_LED3),
    ("GPIO22", "Emergency LED",        580, C_LED4),
    ("GPIO13", "Recording Button",     700, C_BTN1),
    ("GPIO23", "Emergency Button",     820, C_BTN2),
]
pins_right = [
    ("GPIO5",  "DHT22 Data",              220, C_DHT),
    ("GPIO16", "UART2 RX (\u2190 GPS TX)",      300, C_GPS_TX),
    ("GPIO17", "UART2 TX (\u2192 GPS RX)",      380, C_GPS_RX),
    ("GPIO15", "I2S WS   (Mic Word Sel)", 460, C_I2S_WS),
    ("GPIO26", "I2S SCK  (Mic Bit Clk)",  540, C_I2S_SCK),
    ("GPIO32", "I2S SD   (Mic Data Out)", 620, C_I2S_SD),
    ("GPIO25", "Speaker LRC (Word Sel)",  700, C_SPK_LRC),
    ("GPIO27", "Speaker BCLK (Bit Clk)",  780, C_SPK_BCLK),
    ("GPIO33", "Speaker DIN (Data In)",   860, C_SPK_DIN),
    ("GPIO18", "Speaker SD (Mute/Enable)",940, C_SPK_SD),
]

svg = []
def add(s): svg.append(s)

def text(x, y, s, size=15, color=TEXT_MAIN, anchor="start", weight="400", family=FONT, style=""):
    s_escaped = str(s).replace("&", "&amp;") if isinstance(s, str) and "&amp;" not in str(s) else str(s)
    add(f'<text x="{x}" y="{y}" font-family="{family}" font-size="{size}" '
        f'fill="{color}" text-anchor="{anchor}" font-weight="{weight}" style="{style}">{s_escaped}</text>')

def hline(x1, x2, y, color, width=2.4, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    add(f'<line x1="{min(x1,x2)}" y1="{y}" x2="{max(x1,x2)}" y2="{y}" stroke="{color}" '
        f'stroke-width="{width}" stroke-linecap="round"{d}/>')

def vline(x, y1, y2, color, width=2.4, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    add(f'<line x1="{x}" y1="{min(y1,y2)}" x2="{x}" y2="{max(y1,y2)}" stroke="{color}" '
        f'stroke-width="{width}" stroke-linecap="round"{d}/>')

def line(x1, y1, x2, y2, color, width=2.4, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    add(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
        f'stroke-width="{width}" stroke-linecap="round"{d}/>')

def dot(x, y, color, r=4.5):
    """Junction / solder dot at wire connections."""
    add(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')

def resistor(cx, cy, w=64, h=18, color=TEXT_MAIN, label="", horizontal=True):
    """Box schematic resistor symbol with label."""
    if horizontal:
        rx, ry = cx - w/2, cy - h/2
        add(f'<rect x="{rx}" y="{ry}" width="{w}" height="{h}" rx="3" fill="#1e293b" stroke="{color}" stroke-width="1.8"/>')
        if label:
            text(cx, ry - 7, label, 12, TEXT_LABEL, "middle", "700")
        return rx, rx + w

def led(cx, cy, color, flip=False):
    """LED diode symbol (triangle + bar), horizontal."""
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
    ax = bx + (10 if not flip else -10)
    for off in (-8, 2):
        add(f'<line x1="{ax+off-4}" y1="{cy-r-4+off*0.2}" x2="{ax+off+6}" y2="{cy-r-14+off*0.2}" '
            f'stroke="{color}" stroke-width="1.4"/>')

def switch(cx, cy, color=TEXT_MAIN):
    """Push-button symbol, horizontal."""
    line(cx-22, cy, cx-8, cy, color, 2.2)
    line(cx+8, cy, cx+22, cy, color, 2.2)
    line(cx-8, cy+9, cx+10, cy-11, color, 2.6)
    dot(cx-8, cy, color, 3)
    dot(cx+8, cy, color, 3)
    add(f'<line x1="{cx-4}" y1="{cy-16}" x2="{cx-4}" y2="{cy-24}" stroke="{color}" stroke-width="2"/>')
    add(f'<line x1="{cx+12}" y1="{cy-16}" x2="{cx+12}" y2="{cy-24}" stroke="{color}" stroke-width="2"/>')

def module_box(x, y, w, h, title, subtitle, accent=C_DHT, title_y_off=22):
    """Draw stylized card container with drop shadow, border, and top accent header bar."""
    add(f'<rect x="{x+3}" y="{y+3}" width="{w}" height="{h}" rx="6" fill="#000" opacity="0.45"/>')
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="{BOX_FILL}" stroke="{PANEL_EDGE}" stroke-width="1.6"/>')
    add(f'<rect x="{x}" y="{y}" width="{w}" height="4" rx="2" fill="{accent}"/>')
    text(x + w/2, y + title_y_off, title, 15, TEXT_MAIN, "middle", "700")
    if subtitle:
        text(x + w/2, y + title_y_off + 17, subtitle, 11.5, TEXT_DIM, "middle")

def pin_label_left(card_x, pin_y, label, col=CPIN):
    text(card_x + 14, pin_y + 4, label, 11.5, col)

def pin_label_right(card_x, card_w, pin_y, label, col=CPIN):
    text(card_x + card_w - 14, pin_y + 4, label, 11.5, col, anchor="end")

# =====================================================================
add(f'<svg viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg" font-family="{FONT}">')

# background + grid
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add('<defs>')
add(f'<pattern id="grid" width="32" height="32" patternUnits="userSpaceOnUse">'
    f'<path d="M 32 0 L 0 0 0 32" fill="none" stroke="{BG_GRID}" stroke-width="1"/></pattern>')
add('</defs>')
add(f'<rect width="{W}" height="{H}" fill="url(#grid)"/>')

# title
text(W/2, 34, "SYNCHORA — FULL CIRCUIT SCHEMATIC", 24, TEXT_MAIN, "middle", "700")
text(W/2, 52, "Auto-generated from firmware pin map (config/pins.h) • MCU: ESP32 DevKit • Distinct Wire Color Scheme • 100% Orthogonal", 12, TEXT_DIM, "middle")

# ---------------------------------------------------------------- buses
# 5V Power Rail
hline(50, W-50, BUS_5V, C_5V, 3.5)
text(60, BUS_5V-6, "5V POWER BUS", 13, C_5V, "start", "700")

# 3.3V Power Rail
hline(50, W-50, BUS_3V3, C_33V, 3.5)
text(60, BUS_3V3-6, "3.3V POWER BUS", 13, C_33V, "start", "700")

# GND Rail
hline(50, W-50, BUS_BOT, C_GND, 3.5)
text(60, BUS_BOT+24, "GND BUS (COMMON GROUND)", 13, C_GND, "start", "700")

# =====================================================================
# POWER SUPPLY CHAIN (Column 1: Vertically Distributed across Y = 150 .. 990)
# =====================================================================
PC_X, PC_W = 60, 320
BAT_Y = 150
TP_Y  = 380
SW_Y  = 610
MT_Y  = 810

# 18650 Battery Pack (H=170, Y=150..320)
module_box(PC_X, BAT_Y, PC_W, 170, "18650 x2 Pack", "Parallel 3.7V Li-ion Battery", C_BATT)
add(f'<rect x="{PC_X + 16}" y="{BAT_Y + 70}" width="105" height="24" rx="4" fill="#1e293b" stroke="{C_BATT}" stroke-width="1.2"/>')
text(PC_X + 68, BAT_Y + 86, "+ Cell 1 -", 11, C_BATT, "middle", "700")
add(f'<rect x="{PC_X + 16}" y="{BAT_Y + 110}" width="105" height="24" rx="4" fill="#1e293b" stroke="{C_BATT}" stroke-width="1.2"/>')
text(PC_X + 68, BAT_Y + 126, "+ Cell 2 -", 11, C_BATT, "middle", "700")
pin_label_right(PC_X, PC_W, BAT_Y+82, "BAT+ (3.7V)", C_BATT)
pin_label_right(PC_X, PC_W, BAT_Y+122, "BAT- (GND)",  C_GND)

# TP4056 Charger (H=170, Y=380..550)
module_box(PC_X, TP_Y, PC_W, 170, "TP4056 Module", "USB Charger & Protection", C_BATT)
pin_label_left(PC_X, TP_Y+90, "USB (5V in)", TEXT_DIM)
pin_label_right(PC_X, PC_W, TP_Y+40, "B+ (bat+)", C_BATT)
pin_label_right(PC_X, PC_W, TP_Y+80, "B- (bat-)", C_GND)
pin_label_right(PC_X, PC_W, TP_Y+120, "OUT+ 3.7V", C_BATT)
pin_label_right(PC_X, PC_W, TP_Y+150, "OUT- GND", C_GND)

# Slide Switch (H=140, Y=610..750)
module_box(PC_X, SW_Y, PC_W, 140, "Slide Switch", "Main Power ON/OFF Switch", C_SW)
pin_label_right(PC_X, PC_W, SW_Y+50, "IN (3.7V)", C_BATT)
pin_label_right(PC_X, PC_W, SW_Y+100, "OUT (Switched)", C_SW)

# MT3608 Boost Converter (H=180, Y=810..990)
module_box(PC_X, MT_Y, PC_W, 180, "MT3608 Boost", "3.7V → 5V Step-Up Converter", C_5V)
pin_label_right(PC_X, PC_W, MT_Y+50, "IN+ (3.7V)", C_SW)
pin_label_right(PC_X, PC_W, MT_Y+90, "IN- (GND)",  C_GND)
pin_label_right(PC_X, PC_W, MT_Y+130, "OUT+ 5V",   C_5V)
pin_label_right(PC_X, PC_W, MT_Y+160, "OUT- GND",  C_GND)

# Power chain vertical wiring channels (X = 405 .. 555)
pc_r = PC_X + PC_W   # 380
ch_bat_p  = pc_r + 25   # 405
ch_bat_n  = pc_r + 50   # 430
ch_sw_in  = pc_r + 75   # 455
ch_sw_out = pc_r + 100  # 480
ch_gnd    = pc_r + 125  # 505
ch_5v     = pc_r + 150  # 530
ch_mtgnd  = pc_r + 175  # 555

# 1. BAT+ (Y=232) -> TP4056 B+ (Y=420)
hline(pc_r, ch_bat_p, BAT_Y+82, C_BATT, 2)
vline(ch_bat_p, BAT_Y+82, TP_Y+40, C_BATT, 2)
hline(pc_r, ch_bat_p, TP_Y+40, C_BATT, 2)

# 2. BAT- (Y=272) -> TP4056 B- (Y=460)
hline(pc_r, ch_bat_n, BAT_Y+122, C_GND, 2)
vline(ch_bat_n, BAT_Y+122, TP_Y+80, C_GND, 2)
hline(pc_r, ch_bat_n, TP_Y+80, C_GND, 2)

# 3. TP4056 OUT+ (3.7V Y=500) -> Slide Switch IN (Y=660)
hline(pc_r, ch_sw_in, TP_Y+120, C_BATT, 2)
vline(ch_sw_in, TP_Y+120, SW_Y+50, C_BATT, 2)
hline(pc_r, ch_sw_in, SW_Y+50, C_BATT, 2)

# 4. Slide Switch OUT (Y=710) -> MT3608 IN+ (Y=860)
hline(pc_r, ch_sw_out, SW_Y+100, C_SW, 2.2)
vline(ch_sw_out, SW_Y+100, MT_Y+50, C_SW, 2.2)
hline(pc_r, ch_sw_out, MT_Y+50, C_SW, 2.2)

# 5. TP4056 OUT- (Y=530) & MT3608 IN- (Y=900) -> GND Rail
hline(pc_r, ch_gnd, TP_Y+150, C_GND, 2)
vline(ch_gnd, TP_Y+150, BUS_BOT, C_GND, 2)
hline(pc_r, ch_gnd, MT_Y+90, C_GND, 2)
dot(ch_gnd, MT_Y+90, C_GND)
dot(ch_gnd, BUS_BOT, C_GND)

# 6. MT3608 OUT+ (5V Y=940) -> 5V Bus Rail
hline(pc_r, ch_5v, MT_Y+130, C_5V, 2.4)
vline(ch_5v, BUS_5V, MT_Y+130, C_5V, 2.4)
dot(ch_5v, BUS_5V, C_5V)

# 7. MT3608 OUT- (GND Y=970) -> GND Bus Rail
hline(pc_r, ch_mtgnd, MT_Y+160, C_GND, 2)
vline(ch_mtgnd, MT_Y+160, BUS_BOT, C_GND, 2)
dot(ch_mtgnd, BUS_BOT, C_GND)

# =====================================================================
# ESP32 MICROCONTROLLER (Column 3: Standard Box X = 1000 .. 1280)
# =====================================================================
add(f'<rect x="{BOX_X+3}" y="{BOX_Y+3}" width="{BOX_W}" height="{BOX_H}" rx="6" fill="#000" opacity="0.45"/>')
add(f'<rect x="{BOX_X}" y="{BOX_Y}" width="{BOX_W}" height="{BOX_H}" rx="6" fill="{BOX_FILL}" stroke="{PANEL_EDGE}" stroke-width="1.6"/>')
add(f'<rect x="{BOX_X}" y="{BOX_Y}" width="{BOX_W}" height="4" rx="2" fill="{C_DHT}"/>')
add(f'<circle cx="{BOX_X+24}" cy="{BOX_Y+24}" r="5" fill="#243554"/>')

# Title centered in the middle of the ESP32 IC Box
text(BOX_X + BOX_W/2, BOX_Y + BOX_H/2 - 10, "ESP32 DevKit-V1", 18, TEXT_MAIN, "middle", "700")
text(BOX_X + BOX_W/2, BOX_Y + BOX_H/2 + 16, "(30-pin — pins not in use hidden)", 12, TEXT_DIM, "middle")

# ESP32 Power Stubs
esp_vin_x = BOX_X + 50
esp_gnd_x = BOX_X + 140
esp_33_x  = BOX_X + 230

# VIN powered from 5V Power Bus
vline(esp_vin_x, BUS_5V, BOX_Y, C_5V, 3)
dot(esp_vin_x, BUS_5V, C_5V)
text(esp_vin_x - 8, (BUS_5V + BOX_Y)/2 + 4, "VIN (5V in)", 11, C_5V, "end", "700")

# 3.3V Output feeding 3.3V Power Bus
vline(esp_33_x, BUS_3V3, BOX_Y, C_33V, 3)
dot(esp_33_x, BUS_3V3, C_33V)
text(esp_33_x + 8, (BUS_3V3 + BOX_Y)/2 + 4, "3.3V (out)", 11, C_33V, "start", "700")

# GND connected to GND Bus
vline(esp_gnd_x, BOX_Y+BOX_H, BUS_BOT, C_GND, 3)
dot(esp_gnd_x, BUS_BOT, C_GND)
text(esp_gnd_x + 8, BOX_Y+BOX_H+18, "GND", 12, C_GND, "start", "700")

# left pins (Pin text printed ABOVE the pin wire)
esp_l_pin = BOX_X - 100   # 900
for label, desc, y, color in pins_left:
    hline(esp_l_pin, BOX_X, y, color, 2.6)
    dot(BOX_X, y, color, 3.5)
    text(BOX_X - 12, y - 8, label, 13, color, "end", "700")

# right pins (Pin text printed ABOVE the pin wire)
esp_r_pin = BOX_X + BOX_W + 100  # 1380
for label, desc, y, color in pins_right:
    hline(BOX_X+BOX_W, esp_r_pin, y, color, 2.6)
    dot(BOX_X+BOX_W, y, color, 3.5)
    text(BOX_X + BOX_W + 12, y - 8, label, 13, color, "start", "700")

# =====================================================================
# LEFT SIDE PERIPHERALS (Column 2: Human-Interface LEDs + Buttons: X = 640 .. 900)
# =====================================================================

def left_led(y, gpio_color, name, res_label="220Ω"):
    led_x = 810
    res_x = 710
    gnd_x = 640
    hline(led_x+16, esp_l_pin, y, gpio_color, 2.6)
    led(led_x, y, gpio_color, flip=True)
    hline(res_x+32, led_x-16, y, gpio_color, 2.4)
    resistor(res_x, y, 64, 16, TEXT_MAIN, res_label, horizontal=True)
    hline(gnd_x, res_x-32, y, C_GND, 2.4)
    vline(gnd_x, y, BUS_BOT, C_GND, 2.4)
    dot(gnd_x, BUS_BOT, C_GND)
    text(led_x, y - 24, name, 13, TEXT_MAIN, "middle", "700")

left_led(220, C_LED1, "WiFi Status LED (D1)")
left_led(340, C_LED2, "Recording LED (D2)")
left_led(460, C_LED3, "Socket Connected LED (D3)")
left_led(580, C_LED4, "Emergency LED (D4)")

def left_button(y, gpio_color, name, note):
    btn_x = 810
    gnd_x = 640
    hline(btn_x+26, esp_l_pin, y, gpio_color, 2.6)
    switch(btn_x, y, gpio_color)
    hline(gnd_x, btn_x-26, y, C_GND, 2.4)
    vline(gnd_x, y, BUS_BOT, C_GND, 2.4)
    dot(gnd_x, BUS_BOT, C_GND)
    text(btn_x, y - 24, name, 13, TEXT_MAIN, "middle", "700")
    text(btn_x, y + 36, note, 11, TEXT_DIM, "middle")

left_button(700, C_BTN1, "Recording Button (SW1)", "INPUT_PULLUP — pressed = LOW → GND")
left_button(820, C_BTN2, "Emergency Button (SW2)", "Hold 5s = SOS armed, hold 2s = cancel")

# =====================================================================
# RIGHT SIDE PERIPHERALS (Column 4: Shifted Leftward to X=1480 for 100px gap)
# ZERO-TURN ALIGNMENT: 10 Sensor & Audio pins aligned EXACTLY to ESP32 pin Y coordinates!
#   GPIO5  (220) -> DHT22 DATA (220)   [Cyan #22d3ee]
#   GPIO16 (300) -> GPS TX     (300)   [Light Blue #38bdf8]
#   GPIO17 (380) -> GPS RX     (380)   [Pink #f472b6]
#   GPIO15 (460) -> Mic WS     (460)   [Violet #a78bfa]
#   GPIO26 (540) -> Mic SCK    (540)   [Lavender #c084fc]
#   GPIO32 (620) -> Mic SD     (620)   [Magenta #e879f9]
#   GPIO25 (700) -> Spk LRC    (700)   [Mint Green #34d399]
#   GPIO27 (780) -> Spk BCLK   (780)   [Gold Yellow #fbbf24]
#   GPIO33 (860) -> Spk DIN    (860)   [Pink #f472b6]
#   GPIO18 (940) -> Spk SD     (940)   [Violet #a78bfa]
# =====================================================================

# ---- DHT22 Module (dx=1480, dy=150..255) ----
dx, dy, dw, dh = 1480, 150, 280, 105
module_box(dx, dy, dw, dh, "DHT22 Module", "Temp & Humidity", C_DHT, title_y_off=20)
pin_defs_dht = [("VCC (3.3V)", C_33V, 200), ("DATA", C_DHT, 220), ("GND", C_GND, 240)]
for lbl, col, py in pin_defs_dht:
    dot(dx, py, col, 3.2)
    text(dx+14, py+4, lbl, 11.5, col, "start", "700")

# VCC up to 3.3V bus
hline(dx-50, dx, 200, C_33V, 2.4)
vline(dx-50, BUS_3V3, 200, C_33V, 2.4)
dot(dx-50, BUS_3V3, C_33V)

# GND down to bus
hline(dx-50, dx, 240, C_GND, 2.4)
vline(dx-50, 240, BUS_BOT, C_GND, 2.4)
dot(dx-50, BUS_BOT, C_GND)

# DATA -> ESP32 GPIO5 (0 turns)
hline(esp_r_pin, dx, 220, C_DHT, 2.6)


# ---- GPS NEO-6M (gx=1480, gy=275..415) ----
gx, gy, gw, gh = 1480, 275, 280, 140
module_box(gx, gy, gw, gh, "NEO-6M GPS", "UART → HardwareSerial2 @ 9600", C_GPS_TX, title_y_off=68)
gp = [("VCC (5V)", C_5V, 288), ("TX", C_GPS_TX, 300), ("RX", C_GPS_RX, 380), ("GND", C_GND, 396)]
for lbl, col, py in gp:
    dot(gx, py, col, 3.2)
    text(gx+14, py+4, lbl, 11.5, col, "start", "700")

# VCC up to 5V bus
hline(gx-65, gx, 288, C_5V, 2.4)
vline(gx-65, BUS_5V, 288, C_5V, 2.4)
dot(gx-65, BUS_5V, C_5V)

# GND down to bus
hline(gx-65, gx, 396, C_GND, 2.4)
vline(gx-65, 396, BUS_BOT, C_GND, 2.4)
dot(gx-65, BUS_BOT, C_GND)

# GPS TX -> ESP32 GPIO16 (0 turns)
hline(esp_r_pin, gx, 300, C_GPS_TX, 2.6)

# ESP32 GPIO17 -> GPS RX (0 turns)
hline(esp_r_pin, gx, 380, C_GPS_RX, 2.6)


# ---- INMP441 I2S mic (mx=1480, my=435..655) ----
mx, my, mw, mh = 1480, 435, 280, 220
module_box(mx, my, mw, mh, "INMP441 Module", "I2S MEMS Microphone", C_I2S_WS, title_y_off=150)
mp = [("VDD (3.3V)", C_33V, 448), ("WS",  C_I2S_WS, 460), ("SCK", C_I2S_SCK, 540),
      ("SD",  C_I2S_SD, 620), ("L/R", C_GND, 634), ("GND", C_GND, 646)]
for lbl, col, py in mp:
    dot(mx, py, col, 3.2)
    text(mx+14, py+4, lbl, 11.5, col, "start", "700")

# VDD up to 3.3V bus
hline(mx-80, mx, 448, C_33V, 2.4)
vline(mx-80, BUS_3V3, 448, C_33V, 2.4)
dot(mx-80, BUS_3V3, C_33V)

# GND down to bus
hline(mx-80, mx, 646, C_GND, 2.4)
vline(mx-80, 646, BUS_BOT, C_GND, 2.4)
dot(mx-80, BUS_BOT, C_GND)

# L/R tied to GND
hline(mx-40, mx, 634, C_GND, 2.2)
vline(mx-40, 634, BUS_BOT, C_GND, 2.2)
dot(mx-40, BUS_BOT, C_GND)

# Mic WS -> ESP32 GPIO15 (0 turns)
hline(esp_r_pin, mx, 460, C_I2S_WS, 2.6)

# Mic SCK -> ESP32 GPIO26 (0 turns)
hline(esp_r_pin, mx, 540, C_I2S_SCK, 2.6)

# Mic SD -> ESP32 GPIO32 (0 turns)
hline(esp_r_pin, mx, 620, C_I2S_SD, 2.6)


# ---- MAX98357A I2S Speaker Amplifier (sx=1480, sy=675..985) ----
sx, sy, sw, sh = 1480, 675, 280, 310
module_box(sx, sy, sw, sh, "MAX98357A Amplifier", "I2S Mono Class-D Audio Amp", C_SPK_LRC, title_y_off=220)
sp = [
    ("VIN (5V)",  C_5V,       690),
    ("LRC (WS)",  C_SPK_LRC,  700),
    ("BCLK",      C_SPK_BCLK, 780),
    ("DIN",       C_SPK_DIN,  860),
    ("SD (Mute)", C_SPK_SD,   940),
    ("GAIN (NC)", TEXT_DIM,   956),
    ("GND",       C_GND,      970),
]
for lbl, col, py in sp:
    dot(sx, py, col, 3.2)
    text(sx+14, py+4, lbl, 11.5, col, "start", "700")

# GAIN pin note (Floating for +9dB gain)
text(sx+92, 956+4, "(Floating +9dB)", 10, TEXT_DIM, "start")

# VIN up to 5V bus
hline(sx-95, sx, 690, C_5V, 2.4)
vline(sx-95, BUS_5V, 690, C_5V, 2.4)
dot(sx-95, BUS_5V, C_5V)

# GND down to bus
hline(sx-95, sx, 970, C_GND, 2.4)
vline(sx-95, 970, BUS_BOT, C_GND, 2.4)
dot(sx-95, BUS_BOT, C_GND)

# Speaker LRC -> ESP32 GPIO25 (0 turns)
hline(esp_r_pin, sx, 700, C_SPK_LRC, 2.6)

# Speaker BCLK -> ESP32 GPIO27 (0 turns)
hline(esp_r_pin, sx, 780, C_SPK_BCLK, 2.6)

# Speaker DIN -> ESP32 GPIO33 (0 turns)
hline(esp_r_pin, sx, 860, C_SPK_DIN, 2.6)

# Speaker SD -> ESP32 GPIO18 (0 turns)
hline(esp_r_pin, sx, 940, C_SPK_SD, 2.6)

# =====================================================================
# DUAL PARALLEL SPEAKERS STAGE (Clean, Spacious, Professional Layout)
# =====================================================================
spk_out_x = sx + sw   # 1760

# Output Pin Terminals on MAX98357A card right edge
pin_label_right(sx, sw, 810, "OUT+", C_SPK_DIN)
pin_label_right(sx, sw, 870, "OUT-", C_GND)

# Parallel Distribution Bus Rails
bus_out_p_x = 1840  # Vertical OUT+ Bus
bus_out_n_x = 1880  # Vertical OUT- Bus

# Horizontal Stubs from Amplifier Output Pins to Bus Rails
hline(spk_out_x, bus_out_p_x, 810, C_SPK_DIN, 2.6)
hline(spk_out_x, bus_out_n_x, 870, C_GND, 2.6)

# Vertical OUT+ Parallel Bus Rail (Y = 780 .. 880)
vline(bus_out_p_x, 780, 880, C_SPK_DIN, 2.6)
dot(bus_out_p_x, 810, C_SPK_DIN, 4)  # Main OUT+ solder junction

# Vertical OUT- Parallel Bus Rail (Y = 810 .. 910)
vline(bus_out_n_x, 810, 910, C_GND, 2.6)
dot(bus_out_n_x, 870, C_GND, 4)      # Main OUT- solder junction

# ---- SPEAKER 1 MODULE BOX (X = 1940, Y = 750..820) ----
spk1_x, spk1_y, spk1_w, spk1_h = 1940, 750, 260, 70
module_box(spk1_x, spk1_y, spk1_w, spk1_h, "Speaker 1", "16Ω • 0.25W (RoHS)", C_SPK_DIN, title_y_off=22)
pin_label_left(spk1_x, 780, "+", C_SPK_DIN)
pin_label_left(spk1_x, 810, "-", C_GND)

# Speaker 1 Wiring to Parallel Bus Rails
hline(bus_out_p_x, spk1_x, 780, C_SPK_DIN, 2.2)
hline(bus_out_n_x, spk1_x, 810, C_GND, 2.2)
dot(bus_out_p_x, 780, C_SPK_DIN, 3.5)
dot(bus_out_n_x, 810, C_GND, 3.5)


# ---- SPEAKER 2 MODULE BOX (X = 1940, Y = 850..920) ----
spk2_x, spk2_y, spk2_w, spk2_h = 1940, 850, 260, 70
module_box(spk2_x, spk2_y, spk2_w, spk2_h, "Speaker 2", "16Ω • 0.25W (RoHS)", C_SPK_DIN, title_y_off=22)
pin_label_left(spk2_x, 880, "+", C_SPK_DIN)
pin_label_left(spk2_x, 910, "-", C_GND)

# Speaker 2 Wiring to Parallel Bus Rails
hline(bus_out_p_x, spk2_x, 880, C_SPK_DIN, 2.2)
hline(bus_out_n_x, spk2_x, 910, C_GND, 2.2)
dot(bus_out_p_x, 880, C_SPK_DIN, 3.5)
dot(bus_out_n_x, 910, C_GND, 3.5)


# Parallel Combined Load Badge (X = 1940, Y = 945)
add(f'<rect x="{spk1_x}" y="945" width="{spk1_w}" height="28" rx="6" fill="#1e293b" stroke="{PANEL_EDGE}" stroke-width="1.2"/>')
text(spk1_x + spk1_w/2, 963, "Parallel Load: 8Ω (0.50W Total)", 11.5, C_SPK_BCLK, "middle", "700")

# =====================================================================
# LEGEND & METADATA PANEL
# =====================================================================
lx, ly, lw, lh = 60, 1170, 2280, 175
add(f'<rect x="{lx+3}" y="{ly+3}" width="{lw}" height="{lh}" rx="12" fill="#000" opacity="0.45"/>')
add(f'<rect x="{lx}" y="{ly}" width="{lw}" height="{lh}" rx="12" fill="{PANEL}" stroke="{PANEL_EDGE}" stroke-width="1.4"/>')
text(lx+24, ly+30, "COLOR CODED SCHEMATIC LEGEND & HARDWARE PIN MAP", 15, TEXT_MAIN, "start", "700")

legend_col1 = [
    (C_5V,       "5V Power Bus Rail (MT3608 Boost)"),
    (C_33V,      "3.3V Power Bus Rail (ESP32 LDO)"),
    (C_GND,      "Ground (GND) Common Rail"),
    (C_BATT,     "Raw Battery 3.7V (Li-ion Pack)"),
    (C_SW,       "Switched 3.7V (Slide Switch OUT)"),
]

legend_col2 = [
    (C_LED1,     "GPIO2 — WiFi Status LED (D1)"),
    (C_LED2,     "GPIO4 — Recording LED (D2)"),
    (C_LED3,     "GPIO14 — Socket Connected LED (D3)"),
    (C_LED4,     "GPIO22 — Emergency LED (D4)"),
    (C_BTN1,     "GPIO13 — Recording Button (SW1)"),
    (C_BTN2,     "GPIO23 — Emergency Button (SW2)"),
]

legend_col3 = [
    (C_DHT,      "GPIO5 — DHT22 Data Signal"),
    (C_GPS_TX,   "GPIO16 — UART2 RX (GPS TX)"),
    (C_GPS_RX,   "GPIO17 — UART2 TX (GPS RX)"),
    (C_I2S_WS,   "GPIO15 — I2S Mic WS (Word Select)"),
    (C_I2S_SCK,  "GPIO26 — I2S Mic SCK (Bit Clock)"),
    (C_I2S_SD,   "GPIO32 — I2S Mic SD (Data Out)"),
]

legend_col4 = [
    (C_SPK_LRC,  "GPIO25 — I2S Speaker LRC (Word Select)"),
    (C_SPK_BCLK, "GPIO27 — I2S Speaker BCLK (Bit Clock)"),
    (C_SPK_DIN,  "GPIO33 — I2S Speaker DIN (Data In)"),
    (C_SPK_SD,   "GPIO18 — Speaker SD (Shutdown / Mute Control)"),
    (C_SPK_DIN,  "GAIN Pin: Floating (+9dB Hardware Gain)"),
    (C_SPK_DIN,  "Speakers: 2x 16Ω 0.25W Parallel (8Ω Load)"),
]

columns = [
    (lx + 24, legend_col1),
    (lx + 580, legend_col2),
    (lx + 1140, legend_col3),
    (lx + 1720, legend_col4),
]

for col_x, items in columns:
    cyy = ly + 52
    for col, lbl in items:
        hline(col_x, col_x+30, cyy, col, 3.5)
        dot(col_x+30, cyy, col, 3)
        text(col_x+40, cyy+4, lbl, 11.5, TEXT_LABEL, "start")
        cyy += 19.5

add('</svg>')

svg_out = "\n".join(svg)
OUTPUT = os.path.join(os.path.dirname(__file__), "circuit_diagram.svg")
with open(OUTPUT, "w", encoding="utf-8") as f:
    f.write(svg_out)
print(f"[OK] Circuit SVG generated successfully: {OUTPUT}")
