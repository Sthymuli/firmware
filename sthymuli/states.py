# Copyright 2026 Sthymuli Contributors
# SPDX-License-Identifier: Apache-2.0
"""Concrete state and event names for Sthymuli, plus the transition table.

Keeping this file plain-data (no imports, no classes) means the entire
pedagogical model of the robot is readable on one screen. A collaborator
or student can modify behavior by editing TRANSITIONS without needing to
understand any of the driver code.
"""

# --- States ---
SLEEPING = "SLEEPING"
IDLE = "IDLE"
LISTENING = "LISTENING"
THINKING = "THINKING"
ACKNOWLEDGING = "ACKNOWLEDGING"

ALL_STATES = (SLEEPING, IDLE, LISTENING, THINKING, ACKNOWLEDGING)

# --- Events ---
EV_WAKE = "wake"  # any user interaction while sleeping
EV_SLEEP = "sleep"  # inactivity timer expired in IDLE
EV_ENGAGE = "engage"  # at least one user started interacting
EV_LEAVE = "leave"  # every user stopped interacting
EV_TEACHER_PAUSE = "pause"  # teacher explicitly moves to THINKING
EV_THINK_DONE = "done"  # thinking timer expired
EV_ACK_DONE = "finish"  # acknowledging display finished

# --- Transition table: the core pedagogy lives here ---
TRANSITIONS = {
    SLEEPING: {
        EV_WAKE: IDLE,
    },
    IDLE: {
        EV_SLEEP: SLEEPING,
        EV_ENGAGE: LISTENING,
    },
    LISTENING: {
        EV_LEAVE: IDLE,
        EV_TEACHER_PAUSE: THINKING,
    },
    THINKING: {
        EV_THINK_DONE: ACKNOWLEDGING,
    },
    ACKNOWLEDGING: {
        EV_ACK_DONE: IDLE,
    },
}

# --- Mood color per state (RGB, low-brightness) ---
# Values kept small so the LED is comfortable to look at in a classroom.
STATE_COLORS = {
    SLEEPING: (2, 0, 4),  # near-black, barely-violet "alive" pulse
    IDLE: (0, 6, 10),  # calm cyan
    LISTENING: (0, 20, 10),  # vivid teal
    THINKING: (20, 10, 0),  # amber
    ACKNOWLEDGING: (0, 25, 0),  # green (positive by default)
}

# --- Default timings (ms) ---
DEFAULT_TIMINGS = {
    "inactivity_timeout": 15_000,  # IDLE -> SLEEPING
    "think_duration": 2_000,  # THINKING -> ACKNOWLEDGING
    "ack_duration": 1_500,  # ACKNOWLEDGING -> IDLE
}
