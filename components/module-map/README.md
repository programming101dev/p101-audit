# audit-modules

`audit-modules` helps students see the shape of a C project.

Its policy is intentionally structural: module size and naming, header/source
pairing, public API surface, include edges, cycles, and configured layering.
Error/environment ownership belongs to `audit-errors`; direct native
calls that bypass available wrappers belong to `audit-wrappers`.

It uses the shared native `lib_c_facts` Clang analysis to parse `.c` and `.h`
files, groups files into modules by basename, and writes a Markdown report
showing:

- source/header pairs;
- public, private, and header-declared functions;
- local include relationships;
- optional layer-rule violations;
- modules that may be doing too much;
- non-static functions that look like private helpers;
- direct include cycles.

This is a teaching tool, not a proof engine. The C report generator no longer
tries to parse C itself. `lib_c_facts` owns the Clang AST pass. A saved
P101FACT stream remains available as an explicit replay input, so the module
map keeps a real parser while its policy and report logic stay readable.
The basename grouping is intentionally lexical because C has no module
declaration for the AST to identify. Function, type, and call relationships
within those presentation groups use declaration USRs; renaming a local
variable cannot change a finding.

Rules `P101-MOD-014` through `P101-MOD-020` are explicitly naming-convention
checks. They compare public spellings such as `_create`/`_destroy`,
`_count`/`_at`, and include guards. They do not infer allocation, ownership,
destruction, or boolean purpose from those spellings. All other function and
type relationships are resolved by AST declaration identity and type. A
naming-convention finding is evidence of vocabulary asymmetry only.

Unreadable files, dangling symlinks, and missing optional layer files are skipped
or treated as non-fatal. The tool should report the project shape it can see
rather than abort because a build artifact or external include path wandered into
the scan.

## Usage

```sh
audit-modules [-h] [-d:FORMAT] [-L] [-v] [-o <report>] [-l <layers.txt>] [-m <max-functions>] [-p <max-public>] [-i <facts.tsv> | -C <compile_commands.json>] [path...]
```

Examples:

```sh
audit-modules src include
audit-modules -o module-map.md src include
audit-modules -l layers.txt src include
audit-modules -m 8 -p 4 programs/p101-port-forwarder/src
audit-modules -C build-clang/compile_commands.json src include
audit-modules -L -C build-clang/compile_commands.json src include
audit-modules -i source-facts.tsv -o module-map.md src include
audit-modules -d:json -i source-facts.tsv -o module-map.json src include
```

With no paths, `audit-modules` scans the current directory.

`-L` selects library mode. A library repo cannot prove that its public
functions, types, or macros are unused without scanning external consumers, and
its implementation must call the underlying APIs at wrapper boundaries.
Library mode therefore retains local structural checks while omitting those
closed-world findings. When a compile database is supplied, declarations for a
module with no active source translation unit are also excluded; this supports
source-controlled, non-installed platform placeholders. Installed-header/link
validation remains the build system's responsibility.

Function declarations and definitions are paired by C symbol across the
scanned project, not by matching filenames. This admits intentional split
implementations and umbrella headers. A source-only basename is reported only
when it exposes a non-static function with no scanned header declaration.

The tool automatically uses the current project's Clang compilation database
from `.last-build-dir` or `build-clang/compile_commands.json`. This preserves
sibling-library include roots, feature-test macros, and other project flags.
Use `-C` to select a different database explicitly.
Use `-i` to consume an existing P101FACT v7 snapshot without starting another
Clang AST pass. `-d:json` writes normalized findings with `id`, `severity`,
`location`, `message`, and a playground lesson route. `-d:human,json` writes
JSON to stdout and the Markdown overview plus compiler-style human diagnostics
to stderr. JSON uses the shared `p101-tool-report-v1` envelope, which also
records admitted inputs, blind spots, counters, outcome, and exit status.

Layer files contain allowed local include edges, one per line:

```text
cli -> runner
runner -> model
report -> model
```

When `-l` is provided, any scanned local include edge not listed in the file is
reported as a teaching note.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | Report was written with no findings |
| `1` | Report was written and contains one or more findings |
| `2` | Usage, file, parser, or other tool trouble |

## Build and check

Configure a compiler once, then run the gate:

```sh
./change-compiler.sh -c clang
./check.sh
```
