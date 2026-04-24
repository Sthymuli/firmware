# Copyright 2026 Sthymuli Contributors
# SPDX-License-Identifier: Apache-2.0
"""HAL for the ESP32-S3-DevKitC-1 alone.

Onboard resources used:

  * one WS2812 RGB LED on GPIO38 (v1.1 board; GPIO48 on older v1.0 boards)
  * one BOOT button on GPIO0

Mapping for demo / simulation
-----------------------------
  * Mood color         -> onboard LED
  * User NORTH segment -> onboard LED (same pixel; mood wins when both set)
  * User NORTH button  -> BOOT button
  * Users E/S/W        -> inactive until we add breadboard inputs

This is deliberately crude: the point is that all five states of the FSM
are visible and testable with just the DevKit in your hand.
"""

from sthymuli.hal import HAL

_ONBOARD_RGB_PIN = 38  # DevKitC-1 v1.1. Change to 48 if you have v1.0.
_BOOT_BUTTON_PIN = 0


class HALDevKit(HAL):
    def __init__(self):
        # Imports deferred so that ``import sthymuli.hal_devkit`` doesn't
        # fail on CPython (where ``machine`` and ``neopixel`` don't exist).
        # On the DevKit, this import runs once at instantiation.
        from machine import Pin
        from neopixel import NeoPixel

        self._time = self._import_time()
        self._rgb = NeoPixel(Pin(_ONBOARD_RGB_PIN), 1)
        self._btn = Pin(_BOOT_BUTTON_PIN, Pin.IN, Pin.PULL_UP)

    @staticmethod
    def _import_time():
        import time

        return time

    # ----- LED output -----
    def set_mood_color(self, r, g, b):
        self._rgb[0] = (r, g, b)
        self._rgb.write()

    def set_user_color(self, position, r, g, b):
        # On the bare DevKit only NORTH maps to anything visible.
        if position == "NORTH":
            self.set_mood_color(r, g, b)

    # ----- Input polling -----
    def engaged_positions(self):
        # BOOT button is active-low with internal pull-up.
        return {"NORTH"} if self._btn.value() == 0 else set()

    # ----- Time -----
    def ticks_ms(self):
        return self._time.ticks_ms()

    def ticks_diff(self, a, b):
        return self._time.ticks_diff(a, b)

    def sleep_ms(self, ms):
        self._time.sleep_ms(ms)
