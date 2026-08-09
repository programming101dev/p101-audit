# Commands

Quick reference for the native C `audit-wrappers`, backed by
`lib_c_facts` and libclang.

| Command | What it does |
| --- | --- |
| `./build.sh` | Build the native commands and strict analysis targets |
| `./test.sh` | Run the native CLI regression tests |
| `./check.sh` | Run the local gate |
| `./audit-wrappers src include` | Find calls that bypass available wrappers, with replacement hints |
| `./audit-wrappers --compile-db compile_commands.json src include` | Audit with exact compiler flags from a compile database |
| `./audit-wrappers -d:json src include` | Emit JSON |
| `./audit-wrappers -d:human,json src include` | Emit JSON on stdout and human diagnostics on stderr |
| `./audit-wrappers -e src include` | Treat unmapped external calls as findings too |
| `./audit-wrappers --allow-usr 'c:@F@TEST_ASSERT_EQUAL_INT' test src` | Allow one exact external declaration identity |
| `./audit-wrappers --show-inventory` | Print the explicit native/wrapper identity inventory |
| `./audit-wrappers --show-inventory-json` | Print the generated wrapper inventory as JSON |

Exit status: `0` means no missed wrappers, `1` means missed wrappers or strict
external findings, and `2` means setup/parser trouble.
