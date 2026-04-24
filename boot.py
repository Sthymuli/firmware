# Copyright 2026 Sthymuli Contributors
# SPDX-License-Identifier: Apache-2.0
"""boot.py -- runs once at power-on, before main.py.

Intentionally minimal today. Put low-level setup here if it ever becomes
necessary (WiFi credentials from a secrets module, GPIO power latch, etc.).
Keep it short: if boot.py raises, the REPL is the only way out.
"""

print("[boot] Sthymuli firmware starting")
