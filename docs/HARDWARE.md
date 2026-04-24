# Hardware

The firmware targets two hardware configurations:

1. **ESP32-S3-DevKitC-1** (standalone) — used for development and
   pre-hardware demos.
2. **Sthymuli motherboard** with 0-3 **cardinal boards** — the real robot.

## Related hardware repositories

- [Sthymuli-motherboard](https://github.com/nathmo/Sthymuli-motherboard) — main PCB
- [Sthymuli-cardinalboard](https://github.com/nathmo/Sthymuli-cardinalboard) — daughter board for EAST/SOUTH/WEST users

Both are published under CC BY-SA 3.0. The motherboard is built around an
ESP32-S3-WROOM-1-N16 module (16 MB flash) and integrates a 4S LiFePO4 BMS,
an ES8374 audio codec, a WS2812 LED chain, IR proximity sensors, and an
IRM-H638T remote receiver.

## DevKit pin map

| Resource | GPIO |
|---|---|
| Onboard WS2812 RGB LED | 38 (v1.1 boards) or 48 (v1.0) |
| BOOT button | 0 |

Confirm the variant by checking the sticker on the module. If you flash a
v1.0 build on a v1.1 board (or vice-versa) the LED will not respond — that
is the quickest diagnostic.

## Motherboard pin map (preliminary)

These are read from `exports/Sthymuli-motherboard_schematic.pdf` in the
motherboard repository, and will be finalized once the first PCBs are in
hand. See `sthymuli/config.py` (planned) for the single source of truth.

| Bus / signal | GPIO(s) |
|---|---|
| I²C (SDA / SCL) | 48 / 47 |
| WS2812 chain DIN | 9 |
| I²S to ES8374 (SCLK / LCLK / DSIN / DOUT / MCLK) | 12 / 13 / 14 / 15 / 16 |
| IR modulation (38 kHz drive) | 10 |
| IR remote receiver | 11 |
| IR proximity analog in | 2 |
| MCP23017 INTA / INTB | 17 / 18 |
| IMU INT1 (motherboard A / B) | 39 / 40 |
| BMS ALERT | 1 |
| Power latch | 6 |

I²C addresses (preliminary):

| Device | Address |
|---|---|
| MCP23017 (cardinal button expander) | 0x20 |
| ES8374 codec | 0x10 |
| LSM6DS3 (motherboard A) | 0x6A |
| LSM6DS3 (motherboard B) | 0x6B |
| LSM6DS3 (cardinal, optional) | 0x5B |
| BQ76905 fuel gauge | 0x08 |

## Cardinal board connector

Each cardinal board is a `Conn_02x08_Counter_Clockwise` header carrying:

+3V3, +5V, GND, SDA, SCL, BAT+, BAT-, btn_A, btn_B,
IR_TRA_VERT, IR_REC_VERT, IR_TRA_HORI, IR_REC_HORI, DIN, DOUT,
ACC_INT (SOUTH only).

The SOUTH position is special: its connector exposes `ACC_B_INT1`, which
means the optional IMU-equipped cardinal plugs into the south slot.
