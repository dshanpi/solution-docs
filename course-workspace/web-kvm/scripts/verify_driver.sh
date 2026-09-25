#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
driver="$SDK_ROOT/bsp/drivers/vin/modules/sensor/lt6911c_mipi.c"
module="$SDK_ROOT/out/t527/avaota_a1/buildroot/buildroot/target/lib/modules/5.15.147/lt6911c_mipi.ko"
grep -q 'V4L2_IDENT_SENSOR  0x1605' "$driver"
grep -q '3840' "$driver"
test -s "$module"
sha256sum "$driver" "$module" > "$EVIDENCE_DIR/lt6911c-sha256.txt"
record driver-contract software PASS "driver source and previously built arm64 module present"
