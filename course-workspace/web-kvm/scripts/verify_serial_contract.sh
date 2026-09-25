#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
record serial-contract hil NOT_VERIFIED "No bound T527 UART is attached"
