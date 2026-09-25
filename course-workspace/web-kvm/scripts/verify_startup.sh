#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
bash -n "$COURSE_ROOT"/scripts/*.sh
test ! -e "$COURSE_ROOT/build/arm64/S99kvm"
record startup-contract software PASS "scripts parse; course does not install or start board services during software phase"
