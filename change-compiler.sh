#!/usr/bin/env bash
set -euo pipefail
CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
while [ "$#" -gt 0 ]; do
  case "$1" in
    -h|--help)
      echo "Usage: ./change-compiler.sh [ignored p101 build options]"
      exit 0
      ;;
    -c|-x|-f|-t|-k|-s)
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done
echo "p101-wrapper-audit is a Python/Clang tool; no compiler configuration needed."

