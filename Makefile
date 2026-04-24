# Sthymuli firmware — developer shortcuts
# ----------------------------------------
# Override PORT from the shell if mpremote's autodetect fails:
#   make sync PORT=/dev/cu.usbserial-0001
PORT ?= auto

.PHONY: help test fmt lint sync flash repl clean

help:
	@echo "Sthymuli firmware — make targets"
	@echo "  make test    run laptop-side unit tests (pytest)"
	@echo "  make fmt     format all Python files with ruff"
	@echo "  make lint    check formatting and lint rules"
	@echo "  make sync    copy project files to the connected ESP32"
	@echo "  make flash   sync, then reset the board"
	@echo "  make repl    open a MicroPython REPL on the board"
	@echo "  make clean   remove pycache / test artifacts"

test:
	python3 -m pytest tests/ -v

fmt:
	python3 -m ruff format sthymuli tests main.py boot.py
	python3 -m ruff check --fix sthymuli tests main.py boot.py

lint:
	python3 -m ruff format --check sthymuli tests main.py boot.py
	python3 -m ruff check sthymuli tests main.py boot.py

# `mpremote cp -r .` pushes every non-hidden file in the project root.
# We explicitly exclude the laptop-only stuff.
sync:
	mpremote connect $(PORT) cp boot.py :boot.py
	mpremote connect $(PORT) cp main.py :main.py
	mpremote connect $(PORT) cp VERSION :VERSION
	mpremote connect $(PORT) cp -r sthymuli :
	@echo "Sync complete."

flash: sync
	mpremote connect $(PORT) reset

repl:
	mpremote connect $(PORT)

clean:
	find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	rm -rf .pytest_cache .ruff_cache .coverage htmlcov build dist *.egg-info
