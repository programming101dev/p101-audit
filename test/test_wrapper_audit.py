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


def main() -> int:
    tests = [
        test_missed_wrapper_and_local_function,
        test_external_inventory_does_not_fail_by_default,
        test_json_output,
        test_missing_compile_db_is_clear,
        test_compile_db_without_source_command_is_clear,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
