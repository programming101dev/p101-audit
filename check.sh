#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
./build.sh
./test.sh
echo "ALL CHECKS PASSED"

