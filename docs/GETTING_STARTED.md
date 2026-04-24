# Getting started

This guide walks you from zero to a blinking ESP32-S3-DevKitC-1 running
Sthymuli firmware. Examples use macOS shell syntax; Linux is identical
and Windows users should adapt paths.

## 1. Install the host tools

```bash
# 1. Verify Python 3.10+
python3 --version

# 2. Install development dependencies (esptool, mpremote, pytest, ruff)
pip install -e ".[dev]"

# 3. Verify
esptool.py version
mpremote --version
```

If `esptool.py` or `mpremote` aren't found, your Python scripts directory
is missing from `PATH`. On macOS add this to `~/.zshrc`:

```bash
export PATH="$HOME/Library/Python/3.11/bin:$PATH"
```

Adjust the Python version to match yours.

## 2. Plug in the board

Use the **left-hand USB port** on the DevKitC-1 (labeled UART). That goes
through the CP210x bridge and is the reliable path for flashing and REPL.

Find the serial device:

```bash
ls /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART 2>/dev/null
```

Export it so you don't have to retype it:

```bash
export PORT=/dev/cu.usbserial-0001   # match your actual device
```

## 3. Flash MicroPython

For the **N32R16V** module variant (32 MB flash, 16 MB octal PSRAM,
1.8 V rails) you need the `spiram-oct` MicroPython build and the right
flash-mode flags. Using the wrong flags gives a reboot loop.

```bash
# Put the board into download mode: hold BOOT, tap RESET, release BOOT

# Download the matching firmware from:
#   https://micropython.org/download/ESP32_GENERIC_S3/
# Pick the build with "Support for SPIRAM (Octal)"

esptool.py --chip esp32s3 --port $PORT erase_flash
esptool.py --chip esp32s3 --port $PORT --baud 460800 \
    write_flash --flash_mode opi --flash_size 32MB --flash_freq 80m \
    0x0 ~/Downloads/ESP32_GENERIC_S3-SPIRAM_OCT-*.bin
```

Press RESET to boot MicroPython. Verify:

```bash
mpremote connect $PORT
# At the >>> prompt:
#   import sys; print(sys.implementation)
# Press Ctrl-] to exit
```

## 4. Sync the firmware

From the project root:

```bash
make sync PORT=$PORT
make flash PORT=$PORT   # reset the board after sync
make repl PORT=$PORT    # open the REPL to watch it boot
```

You should see `[boot] Sthymuli firmware starting` followed by
`[sthymuli] ready. BOOT = NORTH. triple-press = teacher pause.`

The onboard RGB LED glows calm cyan (IDLE). Hold BOOT to enter LISTENING
(brighter teal). Release to return to IDLE. Triple-press quickly to
trigger the THINKING → ACKNOWLEDGING cycle. Leave it alone for 15 s to
enter SLEEPING.

## 5. Run the tests

The FSM and state tables are CPython-testable:

```bash
make test
```

Thirteen tests should pass in about 0.05 seconds. If they don't, stop and
fix them before touching any hardware — the HAL will not save you from a
broken state table.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| LED doesn't light | You may have a v1.0 DevKitC-1; edit `_ONBOARD_RGB_PIN = 48` in `sthymuli/hal_devkit.py` |
| `esptool` can't find the device | Unplug, replug into the **left** USB port, check `ls /dev/cu.*` |
| Reboot loop with PSRAM errors | Wrong MicroPython build; verify you used the SPIRAM_OCT variant and the `--flash_mode opi` flag |
| `ImportError: no module named neopixel` | You're running on CPython, not MicroPython. Only `main.py` and `hal_devkit.py` should be invoked on the board |
| CI fails but tests pass locally | Run `make lint` — the formatter may disagree with your commits |
