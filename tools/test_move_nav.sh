#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/test-logs
make build/test_profiles build/test_host build/test_le build/test_crypto build/test_usb build/test_web build/test_diagnostic_lock build/test_usb_nav_seed build/test_usb_move_seed build/test_move_bridge build/test_nav_bridge >build/test-logs/build.log 2>&1
for test in test_profiles test_host test_le test_crypto test_usb test_web test_diagnostic_lock test_usb_nav_seed test_usb_move_seed test_move_bridge test_nav_bridge; do
 if ./build/$test >build/test-logs/$test.log 2>&1; then echo "$test passed"; else tail -40 build/test-logs/$test.log; exit 1; fi
done
bash tools/fuzz_move_nav.sh
