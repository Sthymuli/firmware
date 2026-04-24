# Copyright 2026 Sthymuli Contributors
# SPDX-License-Identifier: Apache-2.0
"""Sthymuli firmware entry point (DevKit edition).

Run this on the ESP32-S3-DevKitC-1:

  * Press and hold BOOT    -> user NORTH engages (IDLE -> LISTENING)
  * Release BOOT           -> user NORTH leaves  (LISTENING -> IDLE)
  * Triple-press BOOT fast -> teacher pause      (LISTENING -> THINKING
                                                   -> ACKNOWLEDGING -> IDLE)
  * Leave it alone 15 s    -> inactivity         (IDLE -> SLEEPING)
"""

from sthymuli.fsm import FSM
from sthymuli.hal_devkit import HALDevKit
from sthymuli.states import (
    ACKNOWLEDGING,
    DEFAULT_TIMINGS,
    EV_ACK_DONE,
    EV_ENGAGE,
    EV_LEAVE,
    EV_SLEEP,
    EV_TEACHER_PAUSE,
    EV_THINK_DONE,
    EV_WAKE,
    IDLE,
    LISTENING,
    SLEEPING,
    STATE_COLORS,
    THINKING,
    TRANSITIONS,
)

# Three BOOT presses within this window counts as a "teacher pause" gesture.
TRIPLE_PRESS_WINDOW_MS = 800


def run():
    hal = HALDevKit()

    def paint(state):
        """Factory returning a zero-arg function that paints the mood color."""
        return lambda: hal.set_mood_color(*STATE_COLORS[state])

    fsm = FSM(
        initial_state=IDLE,
        transitions=TRANSITIONS,
        on_enter={s: paint(s) for s in (SLEEPING, IDLE, LISTENING, THINKING, ACKNOWLEDGING)},
    )

    # Timer bookkeeping lives here (in main.py), not in the FSM. The FSM
    # only knows about state transitions; time is a concern of the caller.
    state_entered_at = hal.ticks_ms()

    def on_transition(prev, new, event):
        nonlocal state_entered_at
        state_entered_at = hal.ticks_ms()
        print("[fsm]", prev, "--", event, "-->", new)

    fsm.add_listener(on_transition)

    press_edges = []  # timestamps of recent press edges
    last_engaged = False
    print("[sthymuli] ready. BOOT = NORTH. triple-press = teacher pause.")

    while True:
        now = hal.ticks_ms()
        age = hal.ticks_diff(now, state_entered_at)
        engaged_now = len(hal.engaged_positions()) > 0

        # ---- Edge detection on user engagement ----
        if engaged_now and not last_engaged:
            # Rising edge: record for triple-press detection, drop old ones.
            press_edges.append(now)
            press_edges = [
                t for t in press_edges if hal.ticks_diff(now, t) < TRIPLE_PRESS_WINDOW_MS
            ]
            # Triple-press only fires while LISTENING.
            if len(press_edges) >= 3 and fsm.state == LISTENING:
                fsm.post(EV_TEACHER_PAUSE)
                press_edges = []
            else:
                # Wake up if asleep, then engage. Both are no-ops in states
                # that don't accept them, so posting both is cheap and safe.
                fsm.post(EV_WAKE)
                fsm.post(EV_ENGAGE)
        elif (not engaged_now) and last_engaged:
            fsm.post(EV_LEAVE)

        last_engaged = engaged_now

        # ---- State-driven timers ----
        if fsm.state == IDLE and age > DEFAULT_TIMINGS["inactivity_timeout"]:
            fsm.post(EV_SLEEP)
        elif fsm.state == THINKING and age > DEFAULT_TIMINGS["think_duration"]:
            fsm.post(EV_THINK_DONE)
        elif fsm.state == ACKNOWLEDGING and age > DEFAULT_TIMINGS["ack_duration"]:
            fsm.post(EV_ACK_DONE)

        fsm.process_all()
        hal.sleep_ms(20)  # ~50 Hz tick


run()
