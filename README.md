# p101-wrapper-audit

`p101-wrapper-audit` finds calls that cross outside the local/p101 wrapper
boundary.

It uses Clang's real C/C++ parser, not a token scanner:

1. inventory wrapped functions from installed/local `p101_*` headers;
2. parse source translation units with `clang -Xclang -ast-dump=json`;
3. collect functions defined inside the audited roots;
4. report calls to functions that are neither local nor `p101_*`, plus
   indirect function-pointer calls where the static target cannot be proven.

Known wrapped functions are reported as `missed-wrapper` findings and make the
tool exit `1`. Each finding includes a replacement hint such as
`open -> p101_open`. Other external calls are reported as inventory and only
fail with `--strict-external`. Indirect calls are reported separately because
they are audit boundaries rather than proof of a specific missed wrapper.

## Usage

```sh
p101-wrapper-audit [options] [path...]
```

Options:

- `-j`, `--json` emits JSON.
- `-e`, `--strict-external` makes unmapped external and indirect calls fail too.
- `-a NAME`, `--allow NAME` allows an external callee name.
- `--compile-db compile_commands.json` uses a specific compile database.
- `--compile-db-only` audits only active translation units in that database.
- `--active-headers-only` derives header facts from those active translation
  units instead of parsing every header as an independent translation unit.
  Workspace-wide consumer audits use this mode so a C++ header is interpreted
  under the C++ command that actually includes it.
  This is useful for portable projects that retain platform-specific source
  files which are intentionally not built on the current host.
- `--allow-file FILE` reads intentional boundary rules in
  `path:function:callee` form. `*` may stand for the path or function, blank
  lines and `#` comments are ignored, and an unused rule is tool trouble. Keep
  this file in the audited repo so every exception remains scoped and stale
  exceptions are removed.
- `--cflag FLAG` adds a compiler flag for files not present in a compile
  database; may be repeated.
- `--clang clang-22` uses a specific Clang.
- `--header-root DIR` adds a p101 header inventory root.
- `--timeout SECONDS` sets the per-translation-unit Clang AST timeout.
- `--keep-going` continues after translation-unit parse failures, reports the
  skipped files, and still exits non-clean so incomplete audits cannot pass
  silently.
- `--show-inventory` prints the generated `original -> p101_wrapper` inventory.
- `--show-inventory-json` prints the same inventory as machine-readable JSON.
- `--emit-module-facts` emits the Clang-derived TSV fact stream parsed by
  `lib_c_facts` and consumed by `p101-module-map`; see
  [docs/module-facts.md](docs/module-facts.md).
- `--facts-output FILE` writes the same P101FACT v2 snapshot while the wrapper
  audit runs, so later policy tools reuse the exact AST evidence.
- `--input-manifest FILE` writes a JSON receipt containing the compiler,
  compile-database and fact hashes, discovered/active/parsed files, inactive
  sources, parse failures, allowed callees, and hashes of scoped boundary-rule
  files.

Examples:

```sh
./p101-wrapper-audit ../simple-port-forwarder/src
./p101-wrapper-audit -j --compile-db ../simple-port-forwarder/build-clang/compile_commands.json ../simple-port-forwarder
./p101-wrapper-audit --cflag=-Iinclude src
./p101-wrapper-audit --keep-going --timeout 60 --cflag=-Iinclude src
./p101-wrapper-audit --emit-module-facts --cflag=-Iinclude src include
./p101-wrapper-audit --facts-output facts.tsv --input-manifest inputs.json src include
./p101-wrapper-audit -e -a TEST_ASSERT_EQUAL_INT test src
```

The wrapper inventory is part of the trust boundary. A normal audit fails as a
setup error if no p101 wrappers can be inventoried, because an empty inventory
would otherwise make direct calls look harmless.

## Trust boundary and blind spots

This tool is a gate for the code it successfully parses. It is not a whole
process instrumentation proof.

It can see:

- direct calls in translation units parsed by Clang;
- calls hidden behind macros after preprocessing;
- p101 wrapper functions inventoried from the configured p101 headers;
- indirect calls as an explicit audit boundary.

It cannot see:

- source files that are not part of the compile database or path list;
- translation units skipped after parser/tool errors;
- behavior inside third-party libraries;
- libc calls made by dependencies after your code calls them;
- the runtime effect of function pointers whose target is only known at run
  time.

For teaching, that is the intended ceiling: the audit tells students whether
their code follows the wrapper contract. Pair it with `p101-observe`,
`p101-resource-tracker`, and sanitizers for runtime behavior.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | no missed wrappers; external inventory may still be present |
| `1` | missed wrappers, or external calls with `--strict-external` |
| `2` | parser/tool/setup trouble |

With `--keep-going`, parser trouble is reported alongside any findings found in
the translation units that did parse. The exit status remains `2` when any unit
was skipped, because the report is intentionally partial.

JSON findings use the common envelope keys `id`, `severity`, `location`,
`message`, and `evidence`. Wrapper IDs are `P101-WRAP-001` (available wrapper
bypassed), `P101-WRAP-002` (external call), and `P101-WRAP-003` (indirect
call). Parser trouble is `P101-WRAP-900`.
