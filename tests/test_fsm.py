# Copyright 2026 Sthymuli Contributors
# SPDX-License-Identifier: Apache-2.0
"""Unit tests for the generic FSM and the Sthymuli transition table.

These run under CPython, not MicroPython. Every behavioral decision about
the robot should be expressible (and testable) at this layer. If a change
to the FSM or transition table does not need a new or updated test here,
it probably isn't a behavioral change.
"""

import pytest

from sthymuli.fsm import FSM
from sthymuli.states import (
    ACKNOWLEDGING,
    ALL_STATES,
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


def make_fsm(initial=IDLE):
    return FSM(initial_state=initial, transitions=TRANSITIONS)


# ---------------------------------------------------------------------------
# Behavioral transitions — each test documents one pedagogical rule.
# ---------------------------------------------------------------------------


def test_sleeping_wakes_on_wake_event():
    """A dormant robot becomes idle when any user interacts."""
    fsm = make_fsm(SLEEPING)
    fsm.post(EV_WAKE)
    fsm.process_all()
    assert fsm.state == IDLE


def test_idle_sleeps_after_inactivity():
    """Inactivity in IDLE puts the robot back to SLEEPING."""
    fsm = make_fsm(IDLE)
    fsm.post(EV_SLEEP)
    fsm.process_all()
    assert fsm.state == SLEEPING


def test_idle_engages_with_user():
    """User interaction in IDLE brings the robot to LISTENING."""
    fsm = make_fsm(IDLE)
    fsm.post(EV_ENGAGE)
    fsm.process_all()
    assert fsm.state == LISTENING


def test_listening_returns_to_idle_when_user_leaves():
    fsm = make_fsm(LISTENING)
    fsm.post(EV_LEAVE)
    fsm.process_all()
    assert fsm.state == IDLE


def test_teacher_pause_is_only_valid_from_listening():
    """The teacher_pause gesture is only meaningful during LISTENING.

    If we allowed it from IDLE or SLEEPING the teacher could accidentally
    skip the engagement phase; if we allowed it from THINKING or
    ACKNOWLEDGING it would truncate the robot's own feedback.
    """
    for s in (SLEEPING, IDLE, THINKING, ACKNOWLEDGING):
        fsm = make_fsm(s)
        fsm.post(EV_TEACHER_PAUSE)
        fsm.process_all()
        assert fsm.state == s, f"pause should be ignored from {s}"

    fsm = make_fsm(LISTENING)
    fsm.post(EV_TEACHER_PAUSE)
    fsm.process_all()
    assert fsm.state == THINKING


def test_full_engagement_cycle():
    """IDLE -> LISTENING -> THINKING -> ACKNOWLEDGING -> IDLE."""
    fsm = make_fsm(IDLE)
    for event, expected in (
        (EV_ENGAGE, LISTENING),
        (EV_TEACHER_PAUSE, THINKING),
        (EV_THINK_DONE, ACKNOWLEDGING),
        (EV_ACK_DONE, IDLE),
    ):
        fsm.post(event)
        fsm.process_all()
        assert fsm.state == expected


def test_events_without_matching_transition_are_dropped():
    """An event with no defined transition is a no-op, not an error."""
    fsm = make_fsm(SLEEPING)
    fsm.post(EV_ENGAGE)  # not valid from SLEEPING
    fsm.process_all()
    assert fsm.state == SLEEPING


# ---------------------------------------------------------------------------
# FSM plumbing — entry/exit hooks and listeners.
# ---------------------------------------------------------------------------


def test_on_enter_fires_for_initial_state():
    log = []
    FSM(
        initial_state=IDLE,
        transitions=TRANSITIONS,
        on_enter={IDLE: lambda: log.append("enter_idle")},
    )
    assert log == ["enter_idle"]


def test_on_exit_and_on_enter_fire_in_order():
    log = []
    fsm = FSM(
        initial_state=IDLE,
        transitions=TRANSITIONS,
        on_enter={LISTENING: lambda: log.append("enter_listening")},
        on_exit={IDLE: lambda: log.append("exit_idle")},
    )
    fsm.post(EV_ENGAGE)
    fsm.process_all()
    assert log == ["exit_idle", "enter_listening"]


def test_listeners_receive_transition_tuple():
    seen = []
    fsm = make_fsm(IDLE)
    fsm.add_listener(lambda prev, new, evt: seen.append((prev, new, evt)))
    fsm.post(EV_ENGAGE)
    fsm.process_all()
    assert seen == [(IDLE, LISTENING, EV_ENGAGE)]


# ---------------------------------------------------------------------------
# Invariants: these guard against accidental breakage when TRANSITIONS changes.
# ---------------------------------------------------------------------------


def test_every_state_has_a_mood_color():
    for s in ALL_STATES:
        assert s in STATE_COLORS, f"state {s} missing mood color"
        r, g, b = STATE_COLORS[s]
        assert 0 <= r <= 255 and 0 <= g <= 255 and 0 <= b <= 255


def test_every_transition_target_is_a_known_state():
    for source, row in TRANSITIONS.items():
        assert source in ALL_STATES
        for event, target in row.items():
            assert target in ALL_STATES, f"{source} --{event}--> {target}: unknown target state"


def test_every_state_appears_in_the_transition_table():
    """Catch typos: a state that's never a source or target is dead code."""
    reachable = set(TRANSITIONS.keys())
    for row in TRANSITIONS.values():
        reachable.update(row.values())
    missing = set(ALL_STATES) - reachable
    assert not missing, f"states never referenced in TRANSITIONS: {missing}"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
