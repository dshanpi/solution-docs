#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
record network-contract hil NOT_VERIFIED "No running T527 network target is attached"
