# Commands

Quick reference for `p101-wrapper-audit`. This tool is Python/Clang based, so
`./build.sh` byte-compiles the script and tests instead of compiling C.

| Command | What it does |
| --- | --- |
| `./build.sh` | Byte-compile the tool and tests |
| `./test.sh` | Run the Python unit tests |
| `./check.sh` | Run the local gate |
| `./p101-wrapper-audit src include` | Find calls that bypass available p101 wrappers, with replacement hints |
| `./p101-wrapper-audit --compile-db compile_commands.json src include` | Audit with exact compiler flags from a compile database |
| `./p101-wrapper-audit -j src include` | Emit JSON |
| `./p101-wrapper-audit -e src include` | Treat unmapped external calls as findings too |
| `./p101-wrapper-audit -a TEST_ASSERT_EQUAL_INT test src` | Allow a specific external callee |
| `./p101-wrapper-audit --show-inventory` | Print the generated `original -> p101_wrapper` inventory |

Exit status: `0` means no missed wrappers, `1` means missed wrappers or strict
external findings, and `2` means setup/parser trouble.
