#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
record wiring-contract hil NOT_VERIFIED "Avaota A1 and LT6911C are not attached"
