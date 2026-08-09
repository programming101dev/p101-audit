#!/usr/bin/env bash
set -euo pipefail

tool=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/audit-modules-test.XXXXXX")
if [ -z "${P101_KEEP_TEST:-}" ]; then
  trap 'rm -rf "$work"' EXIT
else
  printf 'keeping module-map test evidence: %s\n' "$work" >&2
fi

expect() {
  wanted=$1
  shift
  set +e
  "$tool" "$@" >"$work/stdout" 2>"$work/stderr"
  got=$?
  set -e
  [ "$got" -eq "$wanted" ] || {
    cat "$work/stderr" >&2
    return 1
  }
}

cat >"$work/facts.tsv" <<'FACTS'
ordinary text
P101FACT	7	FILE	src/alpha.c	alpha	0	1
P101FACT	7	FILE	src/alpha-extra.c	alpha	0	1
P101FACT	7	FILE	include/alpha.h	alpha	1	1
P101FACT	7	FILE	include/alpha-extra.h	alpha	1	1
P101FACT	7	FILE	src/beta.c	beta	0	1
P101FACT	7	FILE	src/main.c	main	0	1
P101FACT	7	FILE	src/util.c	util	0	1
P101FACT	7	FILE	include/gamma.h	gamma	1	1
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	2	alpha.h	1	/work/include/alpha.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	3	stdio.h	0	/usr/include/stdio.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	4	beta.h	1	/work/include/beta.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	5	missing.h	1	
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	6	linux/input.h	0	/usr/include/linux/input.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	7	mach/mach.h	0	/usr/include/mach/mach.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	8	windows.h	0	/usr/include/windows.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	9	sys/event.h	0	/usr/include/sys/event.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	10	sys/kqueue.h	0	/usr/include/sys/kqueue.h
P101FACT	7	INCLUDE	src/alpha.c	alpha	0	11	sys/sysctl.h	0	/usr/include/sys/sysctl.h
P101FACT	7	INCLUDE	src/beta.c	beta	0	2	alpha.h	1	/work/include/alpha.h
P101FACT	7	FUNCTION	src/alpha.c	alpha	0	10	alpha_public	0	0	c:@F@alpha_public	0	0
P101FACT	7	FUNCTION	include/alpha.h	alpha	1	5	alpha_public	0	1	c:@F@alpha_public	0	0
P101FACT	7	FUNCTION	src/alpha.c	alpha	0	20	alpha_private	0	0	c:@F@alpha_private	0	0
P101FACT	7	FUNCTION	src/alpha.c	alpha	0	30	alpha_static	1	0	c:@F@alpha_static	0	0
P101FACT	7	FUNCTION	src/beta.c	beta	0	10	beta_large	0	0	c:@F@beta_large	0	0
P101FACT	7	FUNCTION	src/main.c	main	0	2	main	0	0	c:@F@main	0	0
P101FACT	7	FUNCTION	src/main.c	main	0	3	main_a	1	0	c:@F@main_a	0	0
P101FACT	7	FUNCTION	src/main.c	main	0	4	main_b	1	0	c:@F@main_b	0	0
P101FACT	7	FUNCTION	src/main.c	main	0	5	main_c	1	0	c:@F@main_c	0	0
P101FACT	7	FUNCTION	src/util.c	util	0	2	util_a	1	0	c:@F@util_a	0	0
P101FACT	7	FUNCTION	src/util.c	util	0	3	util_b	1	0	c:@F@util_b	0	0
P101FACT	7	FUNCTION	src/util.c	util	0	4	util_c	1	0	c:@F@util_c	0	0
P101FACT	7	FUNCTION	src/util.c	util	0	5	util_d	1	0	c:@F@util_d	0	0
P101FACT	7	FUNCTION	src/util.c	util	0	6	util_e	1	0	c:@F@util_e	0	0
P101FACT	7	FUNCTION	src/util.c	util	0	7	util_forward	1	1	c:@F@util_forward	0	0
P101FACT	7	TYPE	src/util.c	util	0	8	util_private_type	c:@S@util_private_type
P101FACT	7	MACRO	src/util.c	util	0	9	UTIL_PRIVATE_LIMIT	1		0	0
P101FACT	7	CALL	src/beta.c	beta	0	11	alpha_public	0	0	0	beta_large	c:@F@alpha_public	c:@F@beta_large	0	0
P101FACT	7	CALL	src/alpha.c	alpha	0	31	p101_error_create	0	0	0	alpha_static	c:@F@p101_error_create	c:@F@alpha_static	0	0
P101FACT	7	CALL	src/alpha.c	alpha	0	32	p101_error_has_error	0	0	0	alpha_static	c:@F@p101_error_has_error	c:@F@alpha_static	0	0
P101FACT	7	CALL	src/alpha.c	alpha	0	32	p101_error_is_error	0	0	0	alpha_static	c:@F@p101_error_is_error	c:@F@alpha_static	0	0
P101FACT	7	CALL	src/alpha.c	alpha	0	32	p101_error_reset	0	0	0	alpha_static	c:@F@p101_error_reset	c:@F@alpha_static	0	0
P101FACT	7	CALL	src/alpha.c	alpha	0	33	p101_error_destroy	0	0	0	alpha_static	c:@F@p101_error_destroy	c:@F@alpha_static	0	0
P101FACT	7	CALL	src/alpha.c	alpha	0	34	p101_env_create	0	0	0	alpha_static	c:@F@p101_env_create	c:@F@alpha_static	0	0
P101FACT	7	CALL	src/alpha.c	alpha	0	35	p101_env_destroy	0	0	0	alpha_static	c:@F@p101_env_destroy	c:@F@alpha_static	0	0
P101FACT	7	CALL	src/beta.c	beta	0	14	p101_error_create	0	0	0	beta_large	c:@F@p101_error_create	c:@F@beta_large	0	0
P101FACT	7	CALL	src/beta.c	beta	0	15	p101_env_create	0	0	0	beta_large	c:@F@p101_env_create	c:@F@beta_large	0	0
P101FACT	7	CALL	src/beta.c	beta	0	16	malloc	0	0	0	beta_large	c:@F@malloc	c:@F@beta_large	0	0
P101FACT	7	FUNCTION	include/alpha.h	alpha	1	8	missing_definition	0	1	c:@F@missing_definition	0	0
P101FACT	7	FUNCTION	include/gamma.h	gamma	1	2	gamma_api	0	1	c:@F@gamma_api	0	0
P101FACT	7	MACRO	include/gamma.h	gamma	1	3	GAMMA_LIMIT	1		0	0
P101FACT	7	TYPE	include/gamma.h	gamma	1	4	gamma_type	c:@S@gamma_type
P101FACT	7	TYPE	include/alpha.h	alpha	1	6	alpha_type	c:@S@alpha_type
P101FACT	7	MACRO	include/alpha.h	alpha	1	7	ALPHA_LIMIT	1		0	0
P101FACT	7	NOTE	src/beta.c	beta	0	12	ERROR_USE	helper	3	c:@F@helper	0	0
P101FACT	7	NOTE	src/beta.c	beta	0	13	ERROR_CHECK	helper	3	c:@F@helper	0	0
P101FACT	7	FUNCTION	src/alpha.c	alpha	0	40	alpha_thing_create	0	0	c:@F@alpha_thing_create	0	0
P101FACT	7	FUNCTION	include/alpha.h	alpha	1	9	alpha_thing_create	0	1	c:@F@alpha_thing_create	0	0
P101FACT	7	FUNCTION	src/alpha.c	alpha	0	50	alpha_item_count	0	0	c:@F@alpha_item_count	0	0
P101FACT	7	FUNCTION	include/alpha.h	alpha	1	10	alpha_item_count	0	1	c:@F@alpha_item_count	0	0
P101FACT	7	FUNCTION	src/beta.c	beta	0	20	omega_run	0	0	c:@F@omega_run	0	0
P101FACT	7	FILE	src/delta.c	delta	0	1
P101FACT	7	FILE	include/delta.h	delta	1	1
P101FACT	7	INCLUDE	src/delta.c	delta	0	2	alpha.h	1	/work/include/alpha.h
P101FACT	7	INCLUDE	src/delta.c	delta	0	3	delta.h	1	/work/include/delta.h
P101FACT	7	FUNCTION	src/alpha.c	alpha	0	60	alpha_ctx_init	0	0	c:@F@alpha_ctx_init	0	0
P101FACT	7	FUNCTION	src/alpha.c	alpha	0	70	alpha_kind_from_name	0	0	c:@F@alpha_kind_from_name	0	0
P101FACT	7	TYPE	include/alpha.h	alpha	1	12	alpha_handle_t	c:@T@alpha_handle_t
P101FACT	7	NOTE	src/alpha.c	alpha	0	80	SIGNATURE_ENV_ORDER	alpha_reorder	1	c:@F@alpha_reorder	0	0
P101FACT	7	FUNCTION	src/beta.c	beta	0	30	beta_widget_create	0	0	c:@F@beta_widget_create	0	0
P101FACT	7	FUNCTION	src/beta.c	beta	0	40	beta_widget_destroy	0	0	c:@F@beta_widget_destroy	0	0
P101FACT	7	NOTE	src/alpha.c	alpha	0	95	ALLOC_SIZEOF_TYPE	malloc	1	c:@F@malloc	0	0
FACTS

cat >"$work/layers.txt" <<'LAYERS'
# allowed dependencies

not an edge
beta -> alpha
LAYERS

expect 0 --help
expect 0 -h
expect 2 -Z
expect 2 -j
expect 2 -d:xml
expect 2 -i
P101_MODULE_MAP_TEST_OPTION=: expect 2
P101_MODULE_MAP_TEST_OPTION='?' expect 2
P101_MODULE_MAP_TEST_OPTION=@ expect 2
expect 2 -m nope -i "$work/facts.tsv"
expect 2 -p nope -i "$work/facts.tsv"
expect 2 -i "$work/facts.tsv" -C compile_commands.json
expect 1 -i "$work/facts.tsv" -l "$work/layers.txt" -m 1 -p 1 src include
for diagnostic_id in \
  P101-MOD-002 \
  P101-MOD-003 \
  P101-MOD-004 \
  P101-MOD-006 \
  P101-MOD-007 \
  P101-MOD-008 \
  P101-MOD-009 \
  P101-MOD-010 \
  P101-MOD-011 \
  P101-MOD-012 \
  P101-MOD-013 \
  P101-MOD-014 \
  P101-MOD-015 \
  P101-MOD-016 \
  P101-MOD-017 \
  P101-MOD-018 \
  P101-MOD-019 \
  P101-MOD-020 \
  P101-MOD-021 \
  P101-MOD-022 \
  P101-MOD-027
do
  if ! grep -q "$diagnostic_id" "$work/stdout"; then
    printf 'missing expected diagnostic %s\n' "$diagnostic_id" >&2
    cat "$work/stdout" >&2
    exit 1
  fi
done
grep -q ': warning:' "$work/stdout"
grep -q 'playgrounds/blob/main/lessons/module-boundaries.md' "$work/stdout"

# A forward declaration of a static helper inside a .c file is a
# compilation-order artefact, not a module interface. Both loaders drop it, so
# it must not reach the report at all — recording it made every static helper
# read as a public declaration with no definition (P101-MOD-007/008).
! grep -q 'util_forward' "$work/stdout"

# Module `util` is included by nobody, so its exposed surface is reported. A
# type or macro declared inside util.c is not exposed: it is already the
# private declaration P101-MOD-009 and P101-MOD-010 ask for.
! grep -q 'util_private_type' "$work/stdout"
! grep -q 'UTIL_PRIVATE_LIMIT' "$work/stdout"

expect 1 -d:json -L -v -i "$work/facts.tsv" -l "$work/layers.txt" -m 1 -p 1 -o "$work/report.json"
test -s "$work/report.json"
grep -q '"schema":"p101-tool-report-v1"' "$work/report.json"
grep -q '"tool":"audit-modules"' "$work/report.json"
grep -q '"schema":"p101-tool-diagnostic-v1"' "$work/report.json"
grep -q '"message":' "$work/report.json"
grep -q '"path":"lessons/module-boundaries.md"' "$work/report.json"
grep -q '"outcome":"findings","exit_status":1' "$work/report.json"
set +e
"$tool" -d:human,json -L -i "$work/facts.tsv" -l "$work/layers.txt" -m 1 -p 1 >"$work/both.json" 2>"$work/both.txt"
got=$?
set -e
[ "$got" -eq 1 ]
grep -q '"message":' "$work/both.json"
grep -q ': warning:' "$work/both.txt"
expect 1 -L -i "$work/facts.tsv"

printf 'ordinary text\n' >"$work/empty.tsv"
expect 2 -i "$work/empty.tsv"
printf 'P101FACT\t7\tFILE\tbroken\n' >"$work/bad.tsv"
expect 2 -i "$work/bad.tsv"
expect 2 -i "$work/missing.tsv"
expect 2 -i "$work/facts.tsv" -o /tmp
expect 2 -i "$work/facts.tsv" -l "$work/missing-layers.txt"

{
  printf 'P101FACT\t7\tFILE\t'
  head -c 5000 /dev/zero | tr '\0' x
  printf '\nP101FACT\t7\tFILE\tsrc/main.c\tmain\t0\t1\n'
} >"$work/long.tsv"
expect 0 -i "$work/long.tsv"
