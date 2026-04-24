# Architecture

Sthymuli firmware is organized in three layers. Code at each layer should
not reach across more than one boundary.

```
+---------------------------------------------+
|  main.py  (entry point, timing, input edges) |
+---------------------------------------------+
|  sthymuli.fsm    sthymuli.states             |   behavior layer
|  (generic FSM)   (Sthymuli vocabulary)       |   -- pure Python
+---------------------------------------------+
|  sthymuli.hal    (abstract interface)        |
|  sthymuli.hal_devkit, hal_breadboard, ...    |   hardware layer
+---------------------------------------------+
|  MicroPython machine, neopixel, i2c, ...     |
+---------------------------------------------+
```

## Behavior layer

`sthymuli/fsm.py` is a minimal finite state machine with entry/exit hooks
and transition listeners. It has no knowledge of Sthymuli or of any
hardware and works identically under CPython.

`sthymuli/states.py` names the Sthymuli-specific states and events, and
defines the transition table. The whole pedagogical model of the robot
lives in this one file. Edit it to change behavior.

### States

| State | Color | Meaning |
|---|---|---|
| `SLEEPING` | near-black violet | dormant, barely-alive pulse |
| `IDLE` | calm cyan | awake, waiting |
| `LISTENING` | vivid teal | at least one user engaged |
| `THINKING` | amber | robot is processing |
| `ACKNOWLEDGING` | green | robot is responding |

### Events

`wake`, `sleep`, `engage`, `leave`, `pause`, `done`, `finish`.

`pause` is the only teacher-driven transition: everything else is reactive.

## Hardware abstraction layer

`sthymuli/hal.py` defines an abstract `HAL` class. Every piece of code that
needs to talk to hardware goes through this interface: LED output, input
polling, time. A `FakeHAL` is included for laptop testing.

Concrete implementations:

- `sthymuli/hal_devkit.py` — ESP32-S3-DevKitC-1 standalone (dev / demo)
- *(planned)* `sthymuli/hal_breadboard.py` — DevKit + breadboard with 4 buttons + LED strip
- *(planned)* `sthymuli/hal_motherboard.py` — real Sthymuli motherboard

Swapping HAL implementations should not require any change to the behavior
layer.

## Top-level wiring

`main.py` is the only file that:

- imports a concrete HAL (`hal_devkit`)
- reads wall-clock time
- detects input edges (rising/falling, triple-press windows)
- posts events to the FSM
- ticks the main loop at ~50 Hz

Keeping these concerns out of the FSM and HAL is deliberate: it's what lets
us unit-test behavior on CPython without needing a clock or a mock for input
polling.

## Per-user state (planned)

The five global states describe the robot's collective mood. Per-user state
(each of NORTH, EAST, SOUTH, WEST has its own mini-state: `inactive`,
`active`, `highlighted`, `rewarded`) modulates just that user's LED
segment. Global state = overall mood color; per-user state = local overlay.
This is why the HAL has both `set_mood_color` and `set_user_color`.

See issue `#TBD` for the proposal.
