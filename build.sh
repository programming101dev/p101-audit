#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-${TMPDIR:-/tmp}/p101-wrapper-audit-pycache}"
python3 -m py_compile p101-wrapper-audit test/test_wrapper_audit.py
echo "PASS"
