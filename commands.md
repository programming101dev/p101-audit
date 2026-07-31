# Commands

Quick reference for the native C `p101-wrapper-audit`, backed by
`lib_c_facts` and libclang.

| Command | What it does |
| --- | --- |
| `./build.sh` | Build the native commands and strict analysis targets |
| `./test.sh` | Run the native CLI regression tests |
| `./check.sh` | Run the local gate |
| `./p101-wrapper-audit src include` | Find calls that bypass available p101 wrappers, with replacement hints |
| `./p101-wrapper-audit --compile-db compile_commands.json src include` | Audit with exact compiler flags from a compile database |
| `./p101-wrapper-audit -j src include` | Emit JSON |
| `./p101-wrapper-audit -e src include` | Treat unmapped external calls as findings too |
| `./p101-wrapper-audit -a TEST_ASSERT_EQUAL_INT test src` | Allow a specific external callee |
| `./p101-wrapper-audit --show-inventory` | Print the generated `original -> p101_wrapper` inventory |
| `./p101-wrapper-audit --show-inventory-json` | Print the generated wrapper inventory as JSON |

Exit status: `0` means no missed wrappers, `1` means missed wrappers or strict
external findings, and `2` means setup/parser trouble.
