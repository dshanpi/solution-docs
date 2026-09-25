#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
target="$SDK_ROOT/device/config/chips/t527/configs/avaota_a1/linux-5.15/board.dts"
grep -q 'sensor0_mname = "tp2815_mipi"' "$target"
patch --dry-run --silent -d "$SDK_ROOT" -p1 < "$COURSE_ROOT/patches/avaota_a1-lt6911c.patch"
grep -q 'SENSOR_NAME "lt6911c_mipi"' "$SDK_ROOT/bsp/drivers/vin/modules/sensor/lt6911c_mipi.c"
record dts-contract software PASS "LT6911C patch applies cleanly to preserved avaota_a1 baseline"
