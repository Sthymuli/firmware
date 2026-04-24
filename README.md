# Sthymuli firmware

MicroPython firmware for **Sthymuli**, an educational desktop robot designed
around the teacher rather than the student. Sthymuli sits at the center of a
small group of children, supports up to four simultaneous users (NORTH, EAST,
SOUTH, WEST around a donut-shaped body), and provides the teacher with
attention, turn-taking, and feedback cues during classroom activities.

This repository contains the software that runs on the Sthymuli main board
(ESP32‑S3‑WROOM‑1, 16 MB flash) and on a stand-alone ESP32‑S3‑DevKitC‑1 used
for development and simulation.

---

## Related repositories

| Repository | Purpose |
|---|---|
| [Sthymuli-motherboard](https://github.com/nathmo/Sthymuli-motherboard) | Main PCB: ESP32‑S3, BMS, codec, LEDs, sensors |
| [Sthymuli-cardinalboard](https://github.com/nathmo/Sthymuli-cardinalboard) | Daughter board for EAST / SOUTH / WEST user positions |

## Status

Pre-alpha. The firmware is being developed ahead of a validation session at
CERN with young children. The current focus is the core behavior state
machine and the hardware abstraction layer; peripheral drivers (audio, IR,
IMU) are in progress.

## Quick start

### Requirements

- macOS, Linux, or Windows with a working Python 3.10+ installation
- An [ESP32‑S3‑DevKitC‑1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html) for development, **or** a real Sthymuli motherboard
- VS Code with the [MicroPico](https://marketplace.visualstudio.com/items?itemName=paulober.pico-w-go) extension (recommended)

### Flash MicroPython

See [`docs/GETTING_STARTED.md`](docs/GETTING_STARTED.md) for the full
walkthrough, including the flash-mode flags needed for N32R16V modules.

### Run the firmware on a DevKit

```bash
# Install development tools
pip install -e ".[dev]"

# Upload firmware to the board and reset
make sync

# Open a REPL and watch it boot
make repl
```

Press the **BOOT** button on the DevKit to simulate user NORTH engaging with
the robot. See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full
behavior model.

### Run the tests on your laptop

The behavior logic has no hardware dependencies and runs under CPython:

```bash
make test
```

## Project structure

```
sthymuli-firmware/
├── main.py              # Firmware entry point
├── sthymuli/            # Core package
│   ├── fsm.py           #   Generic finite state machine (CPython-safe)
│   ├── states.py        #   Sthymuli state vocabulary + transition table
│   ├── hal.py           #   Hardware abstraction layer interface
│   └── hal_devkit.py    #   HAL implementation for ESP32-S3-DevKitC-1
├── tests/               # Laptop-side unit tests (pytest)
├── docs/                # Architecture, hardware, contribution docs
└── .github/workflows/   # CI configuration
```

## Contributing

Contributions are welcome. Please read
[`CONTRIBUTING.md`](CONTRIBUTING.md) before opening a pull request.

## License

Apache License 2.0 — see [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
