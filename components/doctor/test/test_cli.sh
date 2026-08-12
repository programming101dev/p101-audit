#!/usr/bin/env bash
set -euo pipefail

doctor=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/audit-doctor-test.XXXXXX")
allow_file="$PWD/.audit-wrappers-allow"
trap 'rm -rf "$work"; rm -f "$allow_file"' EXIT
fake="$work/fake-tool"

cat >"$fake" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
name=$(basename "$0")
case "$name" in
  audit-wrappers) status=${P101_DOCTOR_WRAPPER_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  audit-errors) status=${P101_DOCTOR_CONTRACT_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  audit-modules) status=${P101_DOCTOR_MODULE_STATUS:-${P101_DOCTOR_FAKE_STATUS:-0}} ;;
  *) status=${P101_DOCTOR_FAKE_STATUS:-0} ;;
esac
if [ "$name" = audit-modules ]; then
  for argument in "$@"; do
    if [ "$argument" = -F ]; then
      echo "audit-modules received unsupported -F" >&2
      exit 64
    fi
  done
fi
exit "$status"
SCRIPT
chmod +x "$fake"
for name in audit-wrappers audit-errors audit-modules; do
  ln -s "$fake" "$work/$name"
done

run_expect() {
  expected=$1
  shift
  set +e
  "$doctor" "$@" >/dev/null 2>&1
  actual=$?
  set -e
  [ "$actual" -eq "$expected" ]
}

common=(-A "$work/audit-wrappers" -E "$work/audit-errors"
  -M "$work/audit-modules")
fault_jobs=${P101_DOCTOR_FAULT_JOBS:-4}
case "$fault_jobs" in
  ''|*[!0-9]*|0) echo "P101_DOCTOR_FAULT_JOBS must be positive" >&2; exit 2 ;;
esac

run_fault_campaign() {
  prefix=$1
  count=$2
  shift 2
  pids=()
  for index in $(seq 1 "$count"); do
    (
      P101_FAULT_CALL=$index P101_FAULT_ERRNO=5 \
        "$doctor" "$@" -o "$work/$prefix-$index" "${common[@]}" \
        -- /usr/bin/true >/dev/null 2>&1 || :
    ) &
    pids+=("$!")
    if [ "${#pids[@]}" -eq "$fault_jobs" ]; then
      for pid in "${pids[@]}"; do
        wait "$pid" || :
      done
      pids=()
    fi
  done
  for pid in "${pids[@]}"; do
    wait "$pid" || :
  done
}

run_expect 0 --help
run_expect 0 -h
touch "$allow_file"
run_expect 0 -v -o "$work/clean" -s src -s include -C "$work/compile_commands.json" "${common[@]}" -- /usr/bin/true
grep -q '"schema":"p101-tool-run-receipt-v4"' "$work/clean/tool-receipt.json"
grep -q '"outcome":"clean"' "$work/clean/tool-receipt.json"
grep -q '^schema=audit-doctor-evidence-receipt-v1$' "$work/clean/receipt.txt"
grep -Eq '^artifact\.doctor-json=present[[:space:]]' "$work/clean/receipt.txt"
rm -f "$allow_file"
P101_DOCTOR_FAKE_STATUS=1 run_expect 1 -o "$work/findings" "${common[@]}" -- /usr/bin/true
P101_DOCTOR_FAKE_STATUS=2 run_expect 2 -o "$work/trouble" "${common[@]}" -- /usr/bin/true
run_expect 0 -x -o "$work/skip" "${common[@]}" -- /usr/bin/true
run_expect 0 -x -C "$work/compile_commands.json" -o "$work/skip-explicit-db" "${common[@]}" -- /usr/bin/true
P101_DOCTOR_TEST_OPTION=@ run_expect 2 -x -o "$work/forced-option" "${common[@]}" -- /usr/bin/true
P101_DOCTOR_TEST_OPTION=$'\001' run_expect 2 -x -o "$work/forced-control-option" "${common[@]}" -- /usr/bin/true
for variable in P101_DOCTOR_WRAPPER_STATUS P101_DOCTOR_CONTRACT_STATUS P101_DOCTOR_MODULE_STATUS; do
  env "$variable=1" "$doctor" -o "$work/$variable-findings" "${common[@]}" -- /usr/bin/true >/dev/null 2>&1 || :
  env "$variable=2" "$doctor" -o "$work/$variable-trouble" "${common[@]}" -- /usr/bin/true >/dev/null 2>&1 || :
done

run_expect 2
run_expect 2 -- /usr/bin/true
run_expect 2 -o '' -- /usr/bin/true
run_expect 2 -s '' -- /usr/bin/true
run_expect 2 -C '' -- /usr/bin/true
for option in A E M; do
  run_expect 2 "-$option" '' -- /usr/bin/true
done
run_expect 2 -Z -- /usr/bin/true
run_expect 2 "-"$'\001' -- /usr/bin/true
run_expect 2 -o

many_sources=()
for index in $(seq 1 33); do
  many_sources+=(-s "source-$index")
done
run_expect 2 "${many_sources[@]}" -- /usr/bin/true

# Walk every wrapper failure point in the doctor itself. This exercises the
# error paths that ordinary successful fake tools cannot reach.
run_fault_campaign fault 180

# Exercise source-contract-free auto-discovery failures before a child tool
# starts. The ordinary failure walk takes the source-contract path instead.
run_fault_campaign skip-fault 80 -x
