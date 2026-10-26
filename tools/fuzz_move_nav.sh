#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make fuzz >/tmp/anypad-move-nav-fuzz.log 2>&1
tail -3 /tmp/anypad-move-nav-fuzz.log >build/test-logs/fuzz.log
cat build/test-logs/fuzz.log