#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
[[ ${1:-} == --software ]] || { echo 'only --software is allowed without explicit HIL authorization' >&2; exit 2; }
"$COURSE_ROOT/scripts/build_all.sh" --arm64
"$COURSE_ROOT/scripts/verify_dts.sh"
"$COURSE_ROOT/scripts/verify_driver.sh"
"$COURSE_ROOT/scripts/verify_architecture.sh"
"$COURSE_ROOT/scripts/verify_startup.sh"
for mode in video-format first-frame capture-loop encoder-memory performance gadget keyboard mouse usb-recovery video-socket capture-control troubleshooting; do
  "$COURSE_ROOT/scripts/run_simulation.sh" "$mode" >/dev/null
done
"$COURSE_ROOT/scripts/verify_wiring_contract.sh"
"$COURSE_ROOT/scripts/verify_serial_contract.sh"
"$COURSE_ROOT/scripts/verify_network_contract.sh"
"$COURSE_ROOT/scripts/verify_hdmi_contract.sh"
node "$COURSE_ROOT/scripts/capture_provenance.mjs"
record software-acceptance software PASS "cross-build, static contracts and simulations passed; HIL remains NOT_VERIFIED"
printf 'SOFTWARE_ACCEPTANCE_PASS HIL=NOT_VERIFIED\n'
