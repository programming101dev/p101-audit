# audit-doctor

`audit-doctor` is the source preflight conductor for the Programming 101 quality
tools. It runs a project through:

1. `audit-wrappers`, which checks whether source code bypasses available
   p101 wrappers;
2. `audit-errors`, which checks whether p101 wrapper/error use has a
   visible `env`/`err` contract and rejects silently discarded wrapper errors;
3. `audit-modules`, which checks module shape, public API surface, include
   relationships, and likely split/static-scope opportunities.

The lower-level tools remain the source of truth. `audit-doctor` gives students
and reviewers one command that leaves behind a readable source-quality index.
Runtime capture, fault campaigns, and replay analysis are separate
`p101-inspect run`, `test-faults`, and `p101-inspect analyze` operations.
Use `-x` when you want the module check without the static p101 source-contract
checks.

## Usage

```sh
audit-doctor [-h] [-v] [-x] [-o <doctor-dir>] [-s <source-path>]... [-C <compile_commands.json>] \
    [-A <audit-wrappers>] [-E <audit-errors>] [-M <audit-modules>] \
    -- <command> [args...]
```

Examples:

```sh
audit-doctor -- ./my-program
audit-doctor -x -s src -s include -- ./my-program
audit-doctor -o doctor -s src -s include -- ./my-program
audit-doctor -C build-clang/compile_commands.json -s src -s include -- ./my-program
audit-doctor \
    -A ../audit-wrappers/audit-wrappers \
    -E ../audit-errors/build-clang/audit-errors \
    -M ../audit-modules/build-clang/audit-modules \
    -- ./my-program
```

With no `-o`, the doctor directory is `audit-doctor-<pid>` in the current
directory. The directory must not already exist.

## Doctor contents

`audit-doctor` writes:

```text
command.txt
manifest.txt
summary.md
doctor.json
receipt.txt
tool-receipt.json
wrapper-audit.stdout.txt
wrapper-audit.stderr.txt
source-facts.tsv
source-inputs.json
error-contract.stdout.txt
error-contract.stderr.txt
module-map.md
module-map.json
module-map.stdout.txt
module-map.stderr.txt
```

The source-contract files are produced by `audit-wrappers` and
`audit-errors`; they are omitted when `-x` is used. `module-map.md` is
produced by `audit-modules`; it is always run because module/API shape is
useful even when static p101 source-contract checks are skipped.

`summary.md` starts with a quick grade for wrapper usage, error contracts,
and module shape. `manifest.txt` records the exact tool paths, source paths, and
target command used for the run.

## Boundaries

`audit-doctor` is a conductor, not a separate proof engine. Its findings are only
as complete as the delegated tools and the admitted inputs they receive. Direct
non-p101 calls, third-party code outside the wrapper/event stream, skipped
source-contract checks with `-x`, and source paths that do not cover the real
project can all hide issues from the final summary.

For source checks, the doctor runs one Clang AST pass through
`audit-wrappers`, writes `source-facts.tsv` plus `source-inputs.json`, and
feeds that immutable P101FACT v8 snapshot to both `audit-errors` and
`audit-modules`. The wrapper pass also enables its portability-header rule
pack, so known platform-only headers are reported at the wrapper boundary
rather than as module-structure findings. Use `-C` to pin the compilation
database; otherwise the
doctor uses `lib_c_facts` to discover the current project database. The input
manifest records inactive and unparsed files so a green policy result cannot
hide an incomplete admitted-input set.
If the project root contains `.audit-wrappers-allow`, doctor passes that
scoped boundary ledger to wrapper-audit; the same input manifest records its
path and hash.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | All delegated checks completed cleanly with no findings |
| `1` | At least one delegated tool found a wrapper, error-contract, or module issue |
| `2` | `audit-doctor` could not create/run the doctor workflow |

## Build and check

Configure a compiler once, then run the gate:

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DP101_BUILD_LEVEL=1
cmake -S . -B build -DP101_BUILD_LEVEL=3 && cmake --build build
```
