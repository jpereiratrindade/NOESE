#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sha256sum --check third_party/SHA256SUMS
./scripts/build.sh
python3 - <<'CHECK'
import json
from pathlib import Path
commands=json.loads(Path("build/compile_commands.json").read_text())
assert commands and all("-std=c++26" in entry["command"].split() for entry in commands), "C++26 flag missing"
CHECK
ctest --test-dir build --output-on-failure
