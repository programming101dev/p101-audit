# p101-wrapper-audit

`p101-wrapper-audit` finds calls that cross outside the local/p101 wrapper
boundary.

It uses Clang's real C/C++ parser, not a token scanner:

1. inventory wrapped functions from installed/local `p101_*` headers;
2. parse source translation units with `clang -Xclang -ast-dump=json`;
3. collect functions defined inside the audited roots;
4. report calls to functions that are neither local nor `p101_*`.

Known wrapped functions are reported as `missed-wrapper` findings and make the
tool exit `1`. Each finding includes a replacement hint such as
`open -> p101_open`. Other external calls are reported as inventory and only
fail with `--strict-external`.

## Usage

```sh
p101-wrapper-audit [options] [path...]
```

Options:

- `-j`, `--json` emits JSON.
- `-e`, `--strict-external` makes unmapped external calls fail too.
- `-a NAME`, `--allow NAME` allows an external callee name.
- `--compile-db compile_commands.json` uses a specific compile database.
- `--cflag FLAG` adds a compiler flag for files not present in a compile
  database; may be repeated.
- `--clang clang-22` uses a specific Clang.
- `--header-root DIR` adds a p101 header inventory root.
- `--show-inventory` prints the generated `original -> p101_wrapper` inventory.
- `--show-inventory-json` prints the same inventory as machine-readable JSON.
- `--emit-module-facts` emits the Clang-derived TSV fact stream consumed by
  `p101-module-map`; see [docs/module-facts.md](docs/module-facts.md).

Examples:

```sh
./p101-wrapper-audit ../simple-port-forwarder/src
./p101-wrapper-audit -j --compile-db ../simple-port-forwarder/build-clang/compile_commands.json ../simple-port-forwarder
./p101-wrapper-audit --cflag=-Iinclude src
./p101-wrapper-audit --emit-module-facts --cflag=-Iinclude src include
./p101-wrapper-audit -e -a TEST_ASSERT_EQUAL_INT test src
```

The wrapper inventory is part of the trust boundary. A normal audit fails as a
setup error if no p101 wrappers can be inventoried, because an empty inventory
would otherwise make direct calls look harmless.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | no missed wrappers; external inventory may still be present |
| `1` | missed wrappers, or external calls with `--strict-external` |
| `2` | parser/tool/setup trouble |
