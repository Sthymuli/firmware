# Copyright 2026 Sthymuli Contributors
# SPDX-License-Identifier: Apache-2.0
"""Hardware Abstraction Layer (HAL) interface.

No module that imports ``machine``, ``neopixel``, ``esp32``, or any other
hardware module should be imported by the FSM or behavior code. Everything
goes through the HAL. That lets us:

  * Run behavior unit tests on the laptop with a fake HAL.
  * Swap DevKit -> Breadboard -> Motherboard implementations without
    touching a single line of behavior code.

Positions
---------
``"NORTH"``, ``"EAST"``, ``"SOUTH"``, ``"WEST"``. NORTH is always present
(it's on the motherboard itself). The others may or may not be populated
depending on how many cardinal boards are plugged in.
"""

POSITIONS = ("NORTH", "EAST", "SOUTH", "WEST")


class HAL:
    """Abstract interface. Subclass and override every method."""

    # ----- LED output -----
    def set_mood_color(self, r, g, b):
        """Paint the whole robot with the current mood color."""
        raise NotImplementedError

    def set_user_color(self, position, r, g, b):
        """Paint only one user's LED segment (may overlay the mood color)."""
        raise NotImplementedError

    # ----- Input polling -----
    def engaged_positions(self):
        """Return a set of positions whose users are currently engaged.

        "Engaged" today means button held. It will eventually also cover
        proximity detection, IMU tap, etc.
        """
        raise NotImplementedError

    # ----- Time -----
    def ticks_ms(self):
        """Monotonic millisecond counter.

        Wraps on both MicroPython and CPython fakes: always use
        :py:meth:`ticks_diff`, never subtract directly.
        """
        raise NotImplementedError

    def ticks_diff(self, a, b):
        """Return signed difference ``a - b`` accounting for wrap-around."""
        raise NotImplementedError

    def sleep_ms(self, ms):
        """Block for approximately ``ms`` milliseconds."""
        raise NotImplementedError


class FakeHAL(HAL):
    """In-memory HAL used by unit tests.

    Never import this from firmware code. It lives here (not in ``tests/``)
    so that anyone prototyping a behavior on their laptop can ``from
    sthymuli.hal import FakeHAL`` and drive the FSM without a real device.
    """

    def __init__(self):
        self.mood = (0, 0, 0)
        self.user_colors = {p: (0, 0, 0) for p in POSITIONS}
        self._engaged = set()
        self._now_ms = 0

    # ----- LED output -----
    def set_mood_color(self, r, g, b):
        self.mood = (r, g, b)

    def set_user_color(self, position, r, g, b):
        if position in POSITIONS:
            self.user_colors[position] = (r, g, b)

    # ----- Input polling -----
    def engaged_positions(self):
        return set(self._engaged)

    # Test-only helpers (not part of the HAL contract)
    def simulate_engage(self, position):
        self._engaged.add(position)

    def simulate_leave(self, position):
        self._engaged.discard(position)

    # ----- Time (virtual clock) -----
    def ticks_ms(self):
        return self._now_ms

    def ticks_diff(self, a, b):
        return a - b

    def sleep_ms(self, ms):
        self._now_ms += ms

    def advance(self, ms):
        """Advance the virtual clock without sleeping."""
        self._now_ms += ms
