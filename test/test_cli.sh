#!/usr/bin/env bash
set -euo pipefail

audit=$1
facts=$2
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-wrapper-audit-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

"$audit" --help >/dev/null 2>&1
"$facts" --help >/dev/null 2>&1

mkdir -p "$work/src/alpha"
cat >"$work/src/alpha/local.h" <<'HEADER'
#ifndef LOCAL_H
#define LOCAL_H
#define P101_TRACE_SCOPE(value) ((void)(value))
typedef enum
{
    P101_SAMPLE_OK,
    P101_SAMPLE_REFUSED
} p101_sample_result;
#endif
HEADER
cat >"$work/src/alpha/main.c" <<'SOURCE'
#include <stdlib.h>
#include "local.h"
struct p101_env;
struct p101_error;
int p101_first(const struct p101_env *, struct p101_error *);
int p101_second(const struct p101_env *, struct p101_error *);
int p101_error_has_error(const struct p101_error *);
int external_boundary(void);

static int p101_traced(const struct p101_env *env)
{
    P101_TRACE_SCOPE(env);
    return 0;
}

static int local(void)
{
    P101_TRACE_SCOPE(0);
    return p101_traced(0) + external_boundary();
}

int checked(const struct p101_env *env, struct p101_error *err)
{
    p101_first(env, err);
    if(p101_error_has_error(err))
    {
        return -1;
    }
    p101_second(env, err);
    return 0;
}

int chained(const struct p101_env *env, struct p101_error *err)
{
    p101_first(env, err);
    p101_second(env, err);
    return 0;
}

int optional(const struct p101_env *env)
{
    /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: absence is the result. */
    return p101_first(env, 0);
}

int main(void)
{
    void *memory = malloc(8);
    free(memory);
    return local();
}
SOURCE

set +e
"$audit" "$work" >"$work/audit.txt" 2>"$work/audit.err"
status=$?
set -e
[ "$status" -eq 1 ]
grep -q 'malloc -> p101_malloc' "$work/audit.txt"
grep -q 'free -> p101_free' "$work/audit.txt"
! grep -q 'local ->' "$work/audit.txt"

set +e
"$audit" -j "$work" >"$work/audit.json" 2>"$work/audit-json.err"
status=$?
set -e
[ "$status" -eq 1 ]
grep -q '"schema":"p101-wrapper-audit-findings-v2"' "$work/audit.json"

set +e
"$audit" --allow malloc --allow free "$work" >"$work/external.txt" 2>"$work/external.err"
status=$?
set -e
[ "$status" -eq 0 ]
grep -q 'external_calls: 1' "$work/external.txt"

set +e
"$audit" --strict-external --allow malloc --allow free "$work" >"$work/strict.txt" 2>"$work/strict.err"
status=$?
set -e
[ "$status" -eq 1 ]
grep -q 'external-call: external_boundary' "$work/strict.txt"
! grep -q 'external-call: P101_TRACE_SCOPE' "$work/strict.txt"

printf '*:local:external_boundary\n' >"$work/allow.rules"
"$audit" --strict-external --allow malloc --allow free \
    --allow-file "$work/allow.rules" "$work" >/dev/null

cat >"$work/builtin-format.c" <<'SOURCE'
#include <stdarg.h>
#include <stdio.h>

static int format_text(char *buffer, size_t size, const char *format, ...)
{
    int result;
    va_list arguments;
    va_list copy;

    va_start(arguments, format);
    va_copy(copy, arguments);
    result = vsnprintf(buffer, size, format, copy);
    va_end(copy);
    va_end(arguments);
    return result;
}
SOURCE
cat >"$work/builtin-format.rules" <<'RULES'
*:format_text:va_start
*:format_text:va_copy
*:format_text:vsnprintf
*:format_text:va_end
RULES
"$audit" --strict-external --allow-file "$work/builtin-format.rules" \
    "$work/builtin-format.c" >/dev/null
rm -f "$work/builtin-format.c" "$work/builtin-format.rules"

cat >"$work/macro-wrapper.c" <<'SOURCE'
#include <stddef.h>
void *native_allocate(size_t size);
#define malloc(size) native_allocate(size)

void *allocate_one(void)
{
    return malloc(1);
}
SOURCE
set +e
"$audit" "$work/macro-wrapper.c" >"$work/macro-wrapper.out" 2>"$work/macro-wrapper.err"
status=$?
set -e
[ "$status" -eq 1 ]
grep -q 'malloc -> p101_malloc' "$work/macro-wrapper.out"
printf '*:*:malloc\n' >"$work/macro-wrapper.rules"
"$audit" --allow-file "$work/macro-wrapper.rules" "$work/macro-wrapper.c" >/dev/null
rm -f "$work/macro-wrapper.c" "$work/macro-wrapper.rules"

cat >"$work/macro-wrapper-implementation.c" <<'SOURCE'
#define atomic_load(object) (*(object))

unsigned int p101_atomic_uint_load(const unsigned int *object)
{
    return atomic_load(object);
}
SOURCE
"$audit" "$work/macro-wrapper-implementation.c" >/dev/null
rm -f "$work/macro-wrapper-implementation.c"

printf '*:missing:external_boundary\n' >"$work/stale.rules"
set +e
"$audit" --allow malloc --allow free --allow-file "$work/stale.rules" \
    "$work" >"$work/stale.out" 2>"$work/stale.err"
status=$?
set -e
[ "$status" -eq 2 ]
grep -q 'did not match' "$work/stale.err"

"$facts" "$work" >"$work/facts.tsv"
grep -q $'\talpha/main\t' "$work/facts.tsv"
grep -q $'\tENUM\t.*\tp101_sample_result$' "$work/facts.tsv"
grep -q $'\tENUMERATOR\t.*\tP101_SAMPLE_REFUSED\tp101_sample_result$' "$work/facts.tsv"
grep -q $'\tERROR_OPTIONAL' "$work/facts.tsv"
[ "$(grep -c $'\tERROR_UNCHECKED_CHAIN' "$work/facts.tsv")" -eq 1 ]
awk -F '\t' '$3 == "INCLUDE" { found = 1; if(NF != 9) exit 1 } END { exit !found }' "$work/facts.tsv"
! grep -q $'\tFUNCTION\t.*\texternal_boundary\t' "$work/facts.tsv"

"$audit" --facts-output "$work/snapshot.tsv" \
    --input-manifest "$work/manifest.json" \
    --instrumentation-output "$work/instrumentation.json" \
    --mutation-candidates-output "$work/mutations.json" \
    --allow malloc --allow free "$work" >/dev/null
grep -q '^P101FACT' "$work/snapshot.tsv"
grep -q '"schema":"p101-wrapper-input-manifest-v3"' "$work/manifest.json"
grep -Fq '"translation_units":[' "$work/manifest.json"
grep -q '"inventory_entries":' "$work/manifest.json"
grep -q '"schema":"p101-instrumentation-coverage-v1"' "$work/instrumentation.json"
grep -q '"function":"p101_traced".*"public":false.*"has_env":true.*"trace_entry":true.*"trace_exit":true' "$work/instrumentation.json"
grep -q '"schema":"p101-mutation-candidates-v2"' "$work/mutations.json"

set +e
"$audit" --keep-going --json --cflag=-x --cflag=not-a-language "$work/src/alpha/main.c" \
    >"$work/parse-failure.json" 2>"$work/parse-failure.err"
status=$?
set -e
[ "$status" -eq 2 ]
grep -q '"id":"P101-WRAP-900"' "$work/parse-failure.json"

set +e
"$facts" --keep-going --cflag=-x --cflag=not-a-language \
    "$work/src/alpha/main.c" >"$work/facts-parse-failure.tsv" \
    2>"$work/facts-parse-failure.err"
status=$?
set -e
[ "$status" -eq 2 ]
grep -q 'Clang could not parse the translation unit' "$work/facts-parse-failure.err"
