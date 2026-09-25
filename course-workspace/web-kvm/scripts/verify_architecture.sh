#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
grep -q 'KVM2_HEADER_SIZE 20' "$COURSE_ROOT/include/kvm2_protocol.h"
grep -q 'CTRL_SOCKET.*kvm_ctrl.sock' "$COURSE_ROOT/src/kvm_video.cpp"
record architecture-contract software PASS "KVM2 video and NDJSON control boundaries are separated"
