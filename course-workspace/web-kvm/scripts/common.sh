#!/usr/bin/env bash
set -euo pipefail
COURSE_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SDK_ROOT=${SDK_ROOT:-$(cd "$COURSE_ROOT/../.." && pwd)}
EVIDENCE_DIR=${EVIDENCE_DIR:-$COURSE_ROOT/evidence}
mkdir -p "$EVIDENCE_DIR"
record() {
  local name=$1 source=$2 status=$3 detail=$4
  printf '{"name":"%s","source":"%s","status":"%s","detail":"%s","timestamp":%s}\n' \
    "$name" "$source" "$status" "${detail//\"/\\\"}" "$(date +%s)" > "$EVIDENCE_DIR/$name.json"
}
