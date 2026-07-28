#!/usr/bin/env bash
set -euo pipefail
CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
./build.sh
./test.sh
echo "ALL CHECKS PASSED"

