#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x build/noese ]]; then ./scripts/build.sh; fi
exec ./build/noese "$@"
