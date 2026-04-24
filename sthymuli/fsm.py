# Copyright 2026 Sthymuli Contributors
# SPDX-License-Identifier: Apache-2.0
"""Minimal finite state machine, MicroPython-compatible.

Design philosophy
-----------------
- States and events are plain strings. No enum module: saves memory on
  MicroPython, and strings compare cheaply.
- Transitions are a dict-of-dicts: ``transitions[state][event] -> next_state``.
- Events are queued (``post``) and processed (``process_all``) separately,
  so the main tick loop stays in charge of scheduling.
- Entry hooks fire when a state becomes current. Exit hooks fire when it
  stops being current. Listeners fire after every transition.

This module has zero hardware dependencies and runs identically on CPython,
which is the single biggest win: you can unit-test all behavior logic on
your laptop with no ESP32 attached.
"""


class FSM:
    def __init__(self, initial_state, transitions, on_enter=None, on_exit=None):
        self.state = initial_state
        self.transitions = transitions
        self.on_enter = on_enter or {}
        self.on_exit = on_exit or {}
        self._queue = []
        self._listeners = []
        # Fire the initial entry hook so LED/color setup happens on boot.
        self._fire(self.on_enter, initial_state)

    def post(self, event):
        """Queue an event for later processing."""
        self._queue.append(event)

    def process_all(self):
        """Drain the queue, applying every event in order.

        Events with no matching transition from the current state are
        silently dropped. This is a design choice: "a button press in
        SLEEPING does nothing" should NOT be an error.
        """
        while self._queue:
            event = self._queue.pop(0)
            row = self.transitions.get(self.state)
            if row is None:
                continue
            next_state = row.get(event)
            if next_state is None:
                continue
            prev = self.state
            self._fire(self.on_exit, prev)
            self.state = next_state
            self._fire(self.on_enter, next_state)
            for fn in self._listeners:
                fn(prev, next_state, event)

    def add_listener(self, fn):
        """Register fn(old_state, new_state, event), called after every transition."""
        self._listeners.append(fn)

    @staticmethod
    def _fire(hook_dict, state):
        hook = hook_dict.get(state)
        if hook:
            hook()
