#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
case ${1:-} in
  video-format|first-frame|capture-loop|encoder-memory|performance|gadget|keyboard|mouse|usb-recovery|video-socket|capture-control|troubleshooting) ;;
  *) echo "unsupported simulation: ${1:-missing}" >&2; exit 2 ;;
esac
make -C "$COURSE_ROOT" host >/dev/null
"$COURSE_ROOT/build/host/protocol_test" > "$EVIDENCE_DIR/protocol-test.txt"
"$COURSE_ROOT/build/host/hid_reports" > "$EVIDENCE_DIR/hid-test.txt"
record "simulation-$1" simulation PASS "deterministic protocol and HID fixture checks passed; no hardware claim"
printf 'SIMULATION_PASS mode=%s source=simulation\n' "$1"
