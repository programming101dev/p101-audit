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

Run an engine from the configured build directory, or use the checked-in
`audit-facts`, `audit-wrappers`, `audit-errors`, `audit-modules`, and
`audit-doctor` launchers. Detailed engine contracts live under
`components/*/README.md`.

## Contract

Admitted inputs are explicit source paths, compile databases, fact snapshots,
and boundary ledgers. Outputs are facts, findings, reports, and exit status.
The engines cannot see omitted source, unsupported language constructs, or
runtime behavior. `lib_c_facts` owns parsing; these engines own policy. Finding
engines use the shared `p101-tool-report-v1` lifecycle and accept
`-d:human`, `-d:json`, or `-d:human,json`; there is no separate JSON alias.

## Evidence

```sh
./change-compiler.sh -c clang
./build.sh
./test.sh
```
