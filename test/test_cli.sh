#!/usr/bin/env bash
set -euo pipefail

audit=$1
facts=$2
work=$(mktemp -d "${TMPDIR:-/tmp}/audit-wrappers-test.XXXXXX")
semantic_allows=(
    --allow-usr 'c:@F@malloc'
    --allow-usr 'c:@F@free'
    --allow-usr 'c:@F@p101_first'
    --allow-usr 'c:@F@p101_second'
    --allow-usr 'c:@F@p101_error_has_error'
    --allow-usr 'c:@F@p101_error_optional'
)
trap 'rm -rf "$work"' EXIT

"$audit" --help >/dev/null 2>&1
"$facts" --help >/dev/null 2>&1
set +e
"$audit" -j >/dev/null 2>&1
status=$?
set -e
[ "$status" -eq 2 ]
set +e
"$audit" -d:xml >/dev/null 2>&1
status=$?
set -e
[ "$status" -eq 2 ]

mkdir -p "$work/src/alpha"
cat >"$work/src/alpha/local.h" <<'HEADER'
#ifndef LOCAL_H
#define LOCAL_H
struct p101_env;
typedef struct p101_trace_scope
{
    const struct p101_env *env;
} p101_trace_scope __attribute__((annotate("p101:trace-scope")));
#define P101_TRACE_SCOPE(value)              \
    p101_trace_scope trace_scope = {(value)}; \
    (void)trace_scope
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
int p101_error_has_error(const struct p101_error *) __attribute__((annotate("p101:error-state-query")));
struct p101_error *p101_error_optional(void) __attribute__((annotate("p101:optional-error")));
#define P101_ERROR_OPTIONAL p101_error_optional()
int external_boundary(void);

static int p101_traced(const struct p101_env *env)
{
    P101_TRACE_SCOPE(env);
    return 0;
}

void *p101_malloc(const struct p101_env *env, struct p101_error *err, size_t size)
{
    void *memory;

    P101_TRACE_SCOPE(env);
    (void)err;
    memory = malloc(size);
    return memory;
}

static int local(void)
{
    P101_TRACE_SCOPE(0);
    return p101_traced(0) + external_boundary();
}

int checked(const struct p101_env *env, struct p101_error *err)
{
    int result = 0;
    int has_error;

    p101_first(env, err);
    has_error = p101_error_has_error(err);
    if(has_error)
    {
        result = -1;
        goto done;
    }
    p101_second(env, err);
    has_error = p101_error_has_error(err);
    if(has_error)
    {
        result = -1;
    }

done:
    return result;
}

int chained(const struct p101_env *env, struct p101_error *err)
{
    p101_first(env, err);
    p101_second(env, err);
    return 0;
}

int optional(const struct p101_env *env)
{
    /* P101_ERROR_OPTIONAL rationale: absence is the result. */
    return p101_first(env, P101_ERROR_OPTIONAL);
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
grep -q ': error: missed-wrapper:' "$work/audit.txt"
grep -q 'playgrounds/blob/main/lessons/wrapper-boundaries.md' "$work/audit.txt"
! grep -q 'local ->' "$work/audit.txt"

set +e
"$audit" -d:json "$work" >"$work/audit.json" 2>"$work/audit-json.err"
status=$?
set -e
[ "$status" -eq 1 ]
grep -q '"schema":"p101-tool-report-v1"' "$work/audit.json"
grep -q '"tool":"audit-wrappers"' "$work/audit.json"
grep -q '"does_not_prove":' "$work/audit.json"
grep -q '"schema":"p101-tool-diagnostic-v1"' "$work/audit.json"
grep -q '"message":"missed-wrapper: malloc -> p101_malloc"' "$work/audit.json"
grep -q '"path":"lessons/wrapper-boundaries.md"' "$work/audit.json"
grep -q '"outcome":"findings","exit_status":1' "$work/audit.json"

set +e
"$audit" -d:human,json "$work" >"$work/audit-both.json" 2>"$work/audit-both.txt"
status=$?
set -e
[ "$status" -eq 1 ]
grep -q '"message":"missed-wrapper: malloc -> p101_malloc"' "$work/audit-both.json"
grep -q ': error: missed-wrapper: malloc -> p101_malloc' "$work/audit-both.txt"

set +e
"$audit" "${semantic_allows[@]}" "$work" >"$work/external.txt" 2>"$work/external.err"
status=$?
set -e
[ "$status" -eq 0 ]
grep -q 'external_calls=1' "$work/external.txt"

set +e
"$audit" --strict-external "${semantic_allows[@]}" "$work" >"$work/strict.txt" 2>"$work/strict.err"
status=$?
set -e
[ "$status" -eq 1 ]
grep -q 'external-call: external_boundary' "$work/strict.txt"
! grep -q 'external-call: P101_TRACE_SCOPE' "$work/strict.txt"

printf '*\t\tc:@F@external_boundary\n' >"$work/allow.rules"
"$audit" --strict-external "${semantic_allows[@]}" \
    --allow-file "$work/allow.rules" "$work" >/dev/null

cat >"$work/trusted-api.h" <<'HEADER'
int trusted_api(void);
HEADER
cat >"$work/trusted-api-use.c" <<'SOURCE'
#include "trusted-api.h"

int use_trusted_api(void)
{
    int result;

    result = trusted_api();
    return result;
}
SOURCE
set +e
"$audit" --strict-external --cflag="-I$work" \
    "$work/trusted-api-use.c" >"$work/trusted-api-before.out" 2>&1
status=$?
set -e
[ "$status" -eq 1 ]
grep -q 'external-call: trusted_api' "$work/trusted-api-before.out"
"$audit" --strict-external --cflag="-I$work" --header-root "$work/trusted-api.h" \
    "$work/trusted-api-use.c" >/dev/null
rm -f "$work/trusted-api.h" "$work/trusted-api-use.c"

cat >"$work/errno-macro.c" <<'SOURCE'
#include <errno.h>

int saved_errno(void)
{
    int result;

    result = errno;
    return result;
}
SOURCE
"$audit" --strict-external "$work/errno-macro.c" >/dev/null
rm -f "$work/errno-macro.c"

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
*		c:@F@va_start
*		c:@F@va_copy
*		c:@F@vsnprintf
*		c:@F@va_end
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
printf '*\t\tc:@F@malloc\n' >"$work/macro-wrapper.rules"
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

printf '*\tc:@F@missing\tc:@F@external_boundary\n' >"$work/stale.rules"
set +e
"$audit" "${semantic_allows[@]}" --allow-file "$work/stale.rules" \
    "$work" >"$work/stale.out" 2>"$work/stale.err"
status=$?
set -e
[ "$status" -eq 2 ]
grep -q 'did not match' "$work/stale.err"

"$facts" "$work" >"$work/facts.tsv"
grep -q $'\talpha/main\t' "$work/facts.tsv"
awk -F '\t' '$3 == "ENUM" && $8 == "p101_sample_result" { found = 1 } END { exit !found }' "$work/facts.tsv"
awk -F '\t' '$3 == "ENUMERATOR" && $8 == "P101_SAMPLE_REFUSED" && $9 == "p101_sample_result" { found = 1 } END { exit !found }' "$work/facts.tsv"
grep -q $'\tERROR_OPTIONAL' "$work/facts.tsv"
[ "$(grep -c $'\tERROR_UNCHECKED_CHAIN' "$work/facts.tsv")" -eq 1 ]
awk -F '\t' '$3 == "INCLUDE" { found = 1; if(NF != 9) exit 1 } END { exit !found }' "$work/facts.tsv"
! grep -q $'\tFUNCTION\t.*\texternal_boundary\t' "$work/facts.tsv"

"$audit" --facts-output "$work/snapshot.tsv" \
    --input-manifest "$work/manifest.json" \
    --instrumentation-output "$work/instrumentation.json" \
    --mutation-candidates-output "$work/mutations.json" \
    "${semantic_allows[@]}" "$work" >/dev/null
grep -q '^P101FACT' "$work/snapshot.tsv"
grep -q '"schema":"p101-wrapper-input-manifest-v3"' "$work/manifest.json"
grep -Fq '"translation_units":[' "$work/manifest.json"
grep -q '"inventory_entries":' "$work/manifest.json"
grep -q '"schema":"p101-instrumentation-coverage-v1"' "$work/instrumentation.json"
grep -q '"function":"p101_malloc".*"public":true.*"has_env":true.*"trace_entry":true.*"trace_exit":true' "$work/instrumentation.json"
grep -q '"schema":"p101-mutation-candidates-v2"' "$work/mutations.json"

set +e
"$audit" --keep-going -d:json --cflag=-x --cflag=not-a-language "$work/src/alpha/main.c" \
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
