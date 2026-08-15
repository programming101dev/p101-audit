# p101-audit

`p101-audit` is the workspace's semantic source-policy category. It combines
the former wrapper audit, error-contract, module-map, and doctor repositories
without combining their judgments into one opaque pass.

## Internal engines

- `audit-facts`: acquire a reusable P101FACT snapshot through `lib_c_facts`.
- `audit-wrappers`: check wrapper boundaries and wrapper form.
- `audit-errors`: check error/environment ownership and control flow.
- `audit-modules`: check module shape, public surface, and dependency direction.
- `audit-doctor`: compose the source engines with an executable preflight.
- `audit-workspace`: enforce cross-repository boundaries, public-enum quality,
  wrapper test/instrumentation coverage, ownership, parity, and fault-phase
  contracts without a Python policy process.

Run an engine from the configured build directory, or use the checked-in
`audit-facts`, `audit-wrappers`, `audit-errors`, `audit-modules`, and
`audit-doctor` launchers. `audit-workspace` is built for the governed CMake
acceptance graph. Detailed engine contracts live under
`components/*/README.md`.

## Contract

Admitted inputs are explicit source paths, compile databases, fact snapshots,
and boundary ledgers. Outputs are facts, findings, reports, and exit status.
The engines cannot see omitted source, unsupported language constructs, or
runtime behavior. `lib_c_facts` owns parsing; these engines own policy. Finding
engines use the shared `p101-tool-report-v1` lifecycle and accept
`-d:human`, `-d:json`, or `-d:human,json`; there is no separate JSON alias.

`audit-workspace` admits `repos.txt`, the named workspace contracts, API and
unit-test manifests, and AST facts produced by `lib_c_facts` from the source
paths named by each policy. Policies that accept `--facts` can reuse the
governed `P101SEMANTIC` bundle instead of reparsing those translation units;
the engine rejects an unknown bundle version or malformed record. It emits
compiler-shaped human diagnostics and/or `p101-tool-diagnostic-v1` JSON lines,
plus a nonzero status when findings exist. It cannot prove behavior that is
absent from those contracts or source inputs, and it does not replace runtime
wrapper tests.

## Native-policy boundary

The workspace engine replaces nine former Python policy processes:
functional-library layout, native-wrapper parity, wrapper fault semantics,
governed test inventory, source-responsibility boundaries, architecture
boundaries, public-enum quality discovery, public wrapper unit-test coverage,
and instrumentation coverage. They share one native JSON/TSV reader, one
`lib_c_facts` acquisition path, one loader for the shared semantic bundle, and
the common `lib_tool_event` diagnostic writer. Platform instrumentation
receipts retain their byte-exact contract SHA-256 so the check-graph's
policy-free cross-platform merger can compare them safely.
Their negative controls live in
`scripts/tests/test-audit-workspace.sh` and are split into architecture and
fault groups so the governed graph does not repeat the same scans.

Python remains appropriate for the governed check-graph scheduler, repository
lock/candidate transactions, source and test generators, external HTML
harvesting, and mechanical cross-platform receipt aggregation. Those jobs
primarily manipulate dynamic JSON graphs, create source, or orchestrate child
processes; translating them to C would enlarge the trusted implementation
without removing their dominant filesystem, compiler, or network cost. Shell
remains limited to portable process composition. This is the conversion
boundary, not a claim that C is inherently preferable for every check.

## Evidence

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DP101_BUILD_LEVEL=1
cmake --build build
cmake -S . -B build -DP101_BUILD_LEVEL=2 && cmake --build build
```
