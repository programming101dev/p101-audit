#!/usr/bin/env bash
set -euo pipefail
CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-${TMPDIR:-/tmp}/p101-wrapper-audit-pycache}"
coverage_dir=$(mktemp -d "${TMPDIR:-/tmp}/p101-wrapper-audit-coverage.XXXXXX")
export COVERAGE_FILE="$coverage_dir/.coverage"
trap 'rm -rf "$coverage_dir"' EXIT
COVERAGE_RCFILE="$PWD/coverage.ini" P101_COVERAGE=1 python3 test/test_wrapper_audit.py
COVERAGE_RCFILE="$PWD/coverage.ini" python3 -m coverage run --parallel-mode test/test_wrapper_audit_unit.py
COVERAGE_RCFILE="$PWD/coverage.ini" python3 -m coverage combine "$coverage_dir"
COVERAGE_RCFILE="$PWD/coverage.ini" python3 -m coverage report --include="$PWD/p101_wrapper_audit.py"
