#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x build/noese ]]; then ./scripts/build.sh >&2; fi
exec ./build/noese mcp "$@"
