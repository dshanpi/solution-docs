#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
record hdmi-contract hil NOT_VERIFIED "No LT6911C HDMI source is attached"
