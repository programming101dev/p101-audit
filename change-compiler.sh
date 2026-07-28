#!/usr/bin/env bash
set -euo pipefail
CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
while [ "$#" -gt 0 ]; do
  case "$1" in
    -h|--help)
      echo "Usage: ./change-compiler.sh [ignored p101 build options]"
      exit 0
      ;;
    -c|-x|-f|-t|-k|-s)
      [[ $# -ge 2 ]] || {
        printf 'Error: %s requires an argument.\n' "$1" >&2
        exit 2
      }
      shift 2
      ;;
    -N|--no-flags|-S|--standard|-I|--skip-install)
      shift
      ;;
    --)
      shift
      [[ $# -eq 0 ]] || {
        echo "Error: unexpected positional arguments." >&2
        exit 2
      }
      ;;
    *)
      printf 'Error: unknown option: %s\n' "$1" >&2
      exit 2
      ;;
  esac
done
echo "p101-wrapper-audit is a Python/Clang tool; no compiler configuration needed."
