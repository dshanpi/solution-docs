#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
mode=${1:---check}
if [[ $mode == --check ]]; then
  test -x "$SDK_ROOT/out/toolchain/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc"
  test -d "$SDK_ROOT/out/t527/avaota_a1/buildroot/buildroot/target/usr/lib"
  make -C "$COURSE_ROOT" -n arm64 >/dev/null
  record sdk-build-check software PASS "avaota_a1 toolchain and target libraries resolved"
else
  make -C "$COURSE_ROOT" host arm64
  "$COURSE_ROOT/build/host/protocol_test"
  "$COURSE_ROOT/build/host/hid_reports"
  file "$COURSE_ROOT"/build/arm64/* | tee "$EVIDENCE_DIR/arm64-file.txt"
  sha256sum "$COURSE_ROOT"/build/arm64/* > "$EVIDENCE_DIR/arm64-sha256.txt"
  record sdk-build-arm64 software PASS "host tests and arm64 cross build completed"
fi
