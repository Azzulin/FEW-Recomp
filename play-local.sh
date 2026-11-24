#!/usr/bin/env bash
# Kept for HWDER habits: builds (or updates) the Linux version and starts it. See build-linux.sh.
set -eu
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec bash "$here/build-linux.sh" --launch "$@"
