# Contributing to Sthymuli firmware

Thank you for your interest in Sthymuli. This document describes how to
propose changes.

## Before you start

- The project is licensed under **Apache-2.0**. By submitting a
  contribution, you agree that your work is licensed under the same terms
  (Apache-2.0 Section 5).
- Please read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) first. Most
  changes only touch one layer (FSM, HAL, or a driver), and the
  architecture document tells you which.

## Development workflow

1. Fork the repository and clone your fork locally.
2. Create a feature branch from `main`:
   ```bash
   git checkout -b feat/short-description
   ```
3. Install development dependencies:
   ```bash
   pip install -e ".[dev]"
   ```
4. Make your changes. Run the tests locally:
   ```bash
   make test
   ```
5. Commit in small, focused increments. Use [Conventional
   Commits](https://www.conventionalcommits.org/) style prefixes (`feat:`,
   `fix:`, `docs:`, `refactor:`, `test:`, `chore:`).
6. Push to your fork and open a pull request against `main`.

## What makes a good pull request

- **Keep it small.** One behavioral change per PR. Mechanical refactors
  can share a PR if they're all the same kind of refactor.
- **Write a test.** Any change to `sthymuli/fsm.py` or `sthymuli/states.py`
  should come with a test that would fail on `main` and pass on your branch.
- **Update the changelog.** Add a bullet to the `[Unreleased]` section of
  `CHANGELOG.md`. The PR template will remind you.
- **Explain the why.** The PR description should answer "why this change",
  not just "what it does" — the diff already shows what.

## Code style

- Python is formatted with [Ruff](https://docs.astral.sh/ruff/). Run
  `make fmt` before committing.
- Docstrings are expected on public classes and non-trivial functions.
- Keep module-level imports sorted: stdlib first, then third-party, then
  local.

## Hardware contributions

This repository is for firmware only. PCB and mechanical changes live in
[Sthymuli-motherboard](https://github.com/nathmo/Sthymuli-motherboard) and
[Sthymuli-cardinalboard](https://github.com/nathmo/Sthymuli-cardinalboard)
respectively. Firmware PRs that depend on a hardware change should link
to the corresponding hardware PR.

## Reporting bugs or asking questions

Please use GitHub Issues. Use the issue templates — they prompt you for
the information we need to reproduce the problem.
