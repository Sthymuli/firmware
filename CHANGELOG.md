# Changelog

All notable changes to the Sthymuli firmware are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

While the firmware is at a pre-1.0 version, the MINOR version will be bumped
for any breaking change to the public behavior (FSM transitions, HAL
interface, pin mapping defaults).

## [Unreleased]

### Added
- Initial repository scaffolding
- Apache-2.0 license
- Hardware abstraction layer interface (`sthymuli.hal`)
- DevKit HAL implementation (`sthymuli.hal_devkit`)
- Generic finite state machine (`sthymuli.fsm`)
- Sthymuli state vocabulary and transition table (`sthymuli.states`)
- Unit tests for the FSM (run under CPython, no hardware required)
- CI workflow running tests on every push

[Unreleased]: https://github.com/Sthymuli/firmware/compare/HEAD...HEAD
