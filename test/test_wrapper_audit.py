#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "p101-wrapper-audit"


def run_tool(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(TOOL), *args], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def test_missed_wrapper_and_local_function() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            #include <stdlib.h>
            static int mine(void) { return 7; }
            int main(void) {
                void *p = malloc(4);
                mine();
                free(p);
                return 0;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool(str(source))
        assert result.returncode == 1, result.stderr + result.stdout
        assert "missed-wrapper: malloc" in result.stdout
        assert "missed-wrapper: free" in result.stdout
        assert "hint: use p101_malloc" in result.stdout
        assert "mine" not in result.stdout


def test_external_inventory_does_not_fail_by_default() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            extern int third_party(void);
            int main(void) {
                return third_party();
            }
            """,
            encoding="utf-8",
        )
        result = run_tool(str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "external-call: third_party" in result.stdout


def test_bool_returning_local_function_is_not_external() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            #include <stdbool.h>
            bool ready(void) {
                return true;
            }
            int main(void) {
                return ready() ? 0 : 1;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool(str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "external-call: ready" not in result.stdout


def test_json_output() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            #include <stdio.h>
            int main(void) {
                printf("hello\\n");
                return 0;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool("-j", str(source))
        assert result.returncode == 1, result.stderr + result.stdout
        data = json.loads(result.stdout)
        assert data["missed_wrappers"] >= 1
        assert any(item["name"] == "printf" for item in data["findings"])
        assert any("hint" in item for item in data["findings"])


def test_inventory_json_output() -> None:
    result = run_tool("--show-inventory-json")
    assert result.returncode == 0, result.stderr + result.stdout
    data = json.loads(result.stdout)
    assert data["schema"] == "p101-wrapper-inventory-v1"
    assert any(item["original"] == "malloc" and item["wrapper"] == "p101_malloc" for item in data["wrappers"])


def test_missing_compile_db_is_clear() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
        result = run_tool("--compile-db", str(Path(tmp) / "missing.json"), str(source))
        assert result.returncode == 2
        assert "compile database does not exist" in result.stderr


def test_compile_db_without_source_command_is_clear() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        db = Path(tmp) / "compile_commands.json"
        source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
        db.write_text("[]\n", encoding="utf-8")
        result = run_tool("--compile-db", str(db), str(source))
        assert result.returncode == 2
        assert "compile database has no command" in result.stderr


def test_module_fact_output_uses_clang_ast_for_c_facts() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        header = root / "thing.h"
        source = root / "thing.c"
        header.write_text(
            """
            #ifndef THING_H
            #define THING_H
            #define THING_LIMIT 8
            typedef int (*thing_callback)(int);
            struct thing_state;
            int thing_run(int value);
            #endif
            """,
            encoding="utf-8",
        )
        source.write_text(
            """
            #include "thing.h"
            #include <stdio.h>
            static int helper(int value) { return value + 1; }
            int thing_run(int value) {
                return printf("%d\\n", helper(value));
            }
            """,
            encoding="utf-8",
        )
        result = run_tool("--emit-module-facts", f"--cflag=-I{root}", str(root))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "\tFUNCTION\t" in result.stdout
        assert "\tthing_run\t0\t0" in result.stdout
        assert "\thelper\t1\t0" in result.stdout
        assert "\tCALL\t" in result.stdout
        assert "\tprintf" in result.stdout
        assert "\thelper" in result.stdout
        assert "\tTYPE\t" in result.stdout
        assert "\tthing_callback" in result.stdout
        assert "\tthing_state" in result.stdout
        assert "\tMACRO\t" in result.stdout
        assert "\tTHING_LIMIT" in result.stdout


def test_module_facts_include_bool_returning_definitions() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        header = root / "ready.h"
        source = root / "ready.c"
        header.write_text(
            """
            #ifndef READY_H
            #define READY_H
            #include <stdbool.h>
            bool ready(const char *path);
            #endif
            """,
            encoding="utf-8",
        )
        source.write_text(
            """
            #include "ready.h"
            bool ready(const char *path) {
                return path != 0;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool("--emit-module-facts", f"--cflag=-I{root}", str(root))
        assert result.returncode == 0, result.stderr + result.stdout
        header_fact = f"FUNCTION\t{header.resolve()}\tready\t1\t5\tready\t0\t1"
        source_fact = f"FUNCTION\t{source.resolve()}\tready\t0\t3\tready\t0\t0"
        assert header_fact in result.stdout
        assert source_fact in result.stdout


def main() -> int:
    tests = [
        test_missed_wrapper_and_local_function,
        test_external_inventory_does_not_fail_by_default,
        test_bool_returning_local_function_is_not_external,
        test_json_output,
        test_inventory_json_output,
        test_missing_compile_db_is_clear,
        test_compile_db_without_source_command_is_clear,
        test_module_fact_output_uses_clang_ast_for_c_facts,
        test_module_facts_include_bool_returning_definitions,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
