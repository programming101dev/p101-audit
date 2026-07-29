#!/usr/bin/env python3
from __future__ import annotations

import hashlib
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


def test_compiler_builtins_are_not_external_boundaries() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            int main(void) {
                return __builtin_popcount(7U) == 3 ? 0 : 1;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool("--strict-external", str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "__builtin_popcount" not in result.stdout


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
        assert data["schema"] == "p101-wrapper-audit-findings-v1"
        assert data["missed_wrappers"] >= 1
        assert any(item["evidence"]["callee"] == "printf" for item in data["findings"])
        assert any(item["id"] == "P101-WRAP-001" for item in data["findings"])
        assert all({"id", "severity", "location", "message", "evidence"} <= item.keys() for item in data["findings"])


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


def test_compile_db_only_ignores_inactive_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        active = root / "active.c"
        inactive = root / "inactive.c"
        db = root / "compile_commands.json"
        active.write_text("int main(void) { return 0; }\n", encoding="utf-8")
        inactive.write_text("this is intentionally not valid C\n", encoding="utf-8")
        db.write_text(
            json.dumps(
                [
                    {
                        "directory": str(root),
                        "file": str(active),
                        "arguments": ["clang", "-c", str(active)],
                    }
                ]
            ),
            encoding="utf-8",
        )
        result = run_tool("--compile-db", str(db), "--compile-db-only", str(root))
        assert result.returncode == 0, result.stderr + result.stdout
        assert str(inactive) not in result.stdout


def test_compile_db_only_requires_compile_database() -> None:
    result = run_tool("--compile-db-only", ".")
    assert result.returncode == 2
    assert "--compile-db-only requires --compile-db" in result.stderr


def test_fact_snapshot_reuses_compile_database_includes_for_headers() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        include = root / "include"
        src = root / "src"
        include.mkdir()
        src.mkdir()
        public = include / "public.h"
        detail = include / "detail.h"
        source = src / "main.c"
        database = root / "compile_commands.json"
        facts = root / "facts.tsv"

        detail.write_text("#define ANSWER 42\n", encoding="utf-8")
        public.write_text('#include <detail.h>\nint answer(void);\n', encoding="utf-8")
        source.write_text('#include <public.h>\nint answer(void) { return ANSWER; }\n', encoding="utf-8")
        database.write_text(
            json.dumps(
                [
                    {
                        "directory": str(root),
                        "file": str(source),
                        "arguments": ["clang", f"-I{include}", "-c", str(source)],
                    }
                ]
            ),
            encoding="utf-8",
        )

        result = run_tool("--compile-db", str(database), "--compile-db-only", "--facts-output", str(facts), str(src), str(include))
        assert result.returncode == 0, result.stderr + result.stdout
        assert str(public.resolve()) in facts.read_text(encoding="utf-8")


def test_active_headers_only_uses_translation_unit_language() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "main.cpp"
        used = root / "used.hpp"
        unrelated = root / "unrelated.h"
        database = root / "compile_commands.json"
        facts = root / "facts.tsv"

        used.write_text("inline int answer() { return 42; }\n", encoding="utf-8")
        unrelated.write_text("_Static_assert(sizeof(int) > 0, \"C-only header\");\n", encoding="utf-8")
        source.write_text('#include "used.hpp"\nint main() { return answer() == 42 ? 0 : 1; }\n', encoding="utf-8")
        database.write_text(
            json.dumps(
                [
                    {
                        "directory": str(root),
                        "file": str(source),
                        "arguments": ["clang++", "-std=c++20", "-c", str(source)],
                    }
                ]
            ),
            encoding="utf-8",
        )

        result = run_tool(
            "--compile-db",
            str(database),
            "--compile-db-only",
            "--active-headers-only",
            "--facts-output",
            str(facts),
            str(root),
        )
        assert result.returncode == 0, result.stderr + result.stdout
        facts_text = facts.read_text(encoding="utf-8")
        assert str(used.resolve()) in facts_text
        assert str(unrelated.resolve()) not in facts_text


def test_header_root_discovers_sibling_library_includes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        libraries = root / "libraries"
        first_include = libraries / "lib_first" / "include" / "p101_first"
        second_include = libraries / "lib_second" / "include" / "p101_second"
        source = root / "main.c"
        database = root / "compile_commands.json"

        first_include.mkdir(parents=True)
        second_include.mkdir(parents=True)
        (second_include / "second.h").write_text("int p101_second_value(void);\n", encoding="utf-8")
        (first_include / "first.h").write_text(
            "#include <p101_second/second.h>\nint p101_first_value(void);\n",
            encoding="utf-8",
        )
        source.write_text(
            "#include <p101_first/first.h>\nint main(void) { return p101_first_value(); }\n",
            encoding="utf-8",
        )
        database.write_text(
            json.dumps(
                [
                    {
                        "directory": str(root),
                        "file": str(source),
                        "arguments": ["clang", "-c", str(source)],
                    }
                ]
            ),
            encoding="utf-8",
        )

        result = run_tool("--compile-db", str(database), "--header-root", str(libraries), str(source))
        assert result.returncode == 0, result.stderr + result.stdout


def test_allow_file_suppresses_named_boundary() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "main.c"
        allow = root / "allow.txt"
        source.write_text(
            """
            #include <stdlib.h>
            int main(void) {
                void *p = malloc(4);
                free(p);
                return 0;
            }
            """,
            encoding="utf-8",
        )
        allow.write_text("# allocation boundary\nmain.c:main:malloc\nmain.c:main:free # owned here\n", encoding="utf-8")
        result = run_tool("--allow-file", str(allow), str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "missed_wrappers: 0" in result.stdout


def test_stale_allow_rule_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "main.c"
        allow = root / "allow.txt"
        source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
        allow.write_text("main.c:main:malloc\n", encoding="utf-8")
        result = run_tool("--allow-file", str(allow), str(source))
        assert result.returncode == 2
        assert "stale boundary rule" in result.stderr


def test_timeout_must_be_positive() -> None:
    result = run_tool("--timeout", "0", ".")
    assert result.returncode == 2
    assert "--timeout must be greater than zero" in result.stderr


def test_keep_going_reports_partial_results_and_parse_failures() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        bad = root / "bad.c"
        good = root / "good.c"
        bad.write_text("int broken(void) {\n", encoding="utf-8")
        good.write_text(
            """
            #include <stdlib.h>
            int main(void) {
                void *p = malloc(4);
                return p == 0;
            }
            """,
            encoding="utf-8",
        )

        stopped = run_tool(str(root))
        assert stopped.returncode == 2
        assert "clang failed" in stopped.stderr

        continued = run_tool("--keep-going", str(root))
        assert continued.returncode == 2
        assert "skipped" in continued.stderr
        assert "parse_failures: 1" in continued.stdout
        assert "missed-wrapper: malloc" in continued.stdout


def test_static_inline_header_calls_are_audited_at_header_location() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        header = root / "helper.h"
        source = root / "main.c"
        header.write_text(
            """
            #ifndef HELPER_H
            #define HELPER_H
            #include <stdlib.h>
            static inline void *helper_make(void) {
                return malloc(4);
            }
            #endif
            """,
            encoding="utf-8",
        )
        source.write_text(
            """
            #include "helper.h"
            int main(void) {
                return helper_make() == 0;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool(f"--cflag=-I{root}", str(root))
        assert result.returncode == 1, result.stderr + result.stdout
        assert f"{header.resolve()}:6:" in result.stdout
        assert "missed-wrapper: malloc" in result.stdout


def test_indirect_function_pointer_calls_are_reported_as_boundaries() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            int puts(const char *);
            int main(void) {
                int (*fp)(const char *) = puts;
                return fp("hello");
            }
            """,
            encoding="utf-8",
        )
        result = run_tool(str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "indirect_calls: 1" in result.stdout
        assert "indirect-call: fp" in result.stdout
        assert "external-call: fp" not in result.stdout


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
            #include <p101_env/env.h>
            #include <p101_error/error.h>
            #include <p101_posix/p101_unistd.h>
            static int helper(int value) { return value + 1; }
            static void traced(const struct p101_env *env, struct p101_error *err) {
                P101_TRACE(env);
                if(p101_error_has_error(err)) {
                    P101_ERROR_RAISE_USER(err, "bad", 1);
                }
            }
            static int optional_probe(const struct p101_env *env) {
                /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: failure means absent. */
                return p101_access(env, NULL, "missing", 0);
            }
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
        assert result.stdout.startswith("P101FACT\t2\t")
        assert "\tprintf" in result.stdout
        assert "\thelper" in result.stdout
        assert "\tENV_CONTRACT" in result.stdout
        assert "\tERROR_CONTRACT" in result.stdout
        assert "\tTRACE_USE" in result.stdout
        assert "\tERROR_CHECK" in result.stdout
        assert "\tERROR_OPTIONAL" in result.stdout
        assert "\tTYPE\t" in result.stdout
        assert "\tthing_callback" in result.stdout
        assert "\tthing_state" in result.stdout
        assert "\tMACRO\t" in result.stdout
        assert "\tTHING_LIMIT" in result.stdout


def test_reusable_fact_snapshot_and_input_manifest() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "main.c"
        facts = root / "facts.tsv"
        manifest = root / "inputs.json"
        allow = root / "allow.txt"
        source.write_text("extern int puts(const char *text);\nint f(void) { return puts(\"hello\"); }\n", encoding="utf-8")
        allow.write_text("main.c:f:puts\n", encoding="utf-8")
        result = run_tool("--allow-file", str(allow), "--allow", "vendor_call", "--facts-output", str(facts), "--input-manifest", str(manifest), str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "P101FACT\t2\t" in facts.read_text(encoding="utf-8")
        assert "\tputs\t0\t0" in facts.read_text(encoding="utf-8")
        data = json.loads(manifest.read_text(encoding="utf-8"))
        assert data["schema"] == "p101-audit-inputs-v2"
        assert str(source.resolve()) in data["active_translation_units"]
        assert str(source.resolve()) in data["parsed_translation_units"]
        assert data["fact_snapshot_sha256"]
        assert data["allowed_callees"] == ["vendor_call"]
        assert data["boundary_rule_files"] == [{"path": str(allow.resolve()), "sha256": hashlib.sha256(allow.read_bytes()).hexdigest()}]
        assert data["parse_failures"] == []


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


def test_module_facts_parse_cxx_headers_as_cxx() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        include = root / "include"
        src = root / "src"
        include.mkdir()
        src.mkdir()
        header = include / "display.hpp"
        source = src / "display.cpp"
        header.write_text(
            """
            #ifndef DISPLAY_HPP
            #define DISPLAY_HPP
            struct p101_env;
            struct p101_error;
            using callback_t = int (*)(int);
            void display(const p101_env *env, p101_error *err, const char *msg);
            #endif
            """,
            encoding="utf-8",
        )
        source.write_text(
            """
            #include "../include/display.hpp"
            void display(const p101_env *env, p101_error *err, const char *msg) {
                (void)env;
                (void)err;
                (void)msg;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool("--emit-module-facts", f"--cflag=-I{include}", str(src), str(include))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "\tFUNCTION\t" in result.stdout
        assert "\tdisplay\t" in result.stdout


def main() -> int:
    tests = [
        test_missed_wrapper_and_local_function,
        test_external_inventory_does_not_fail_by_default,
        test_bool_returning_local_function_is_not_external,
        test_compiler_builtins_are_not_external_boundaries,
        test_json_output,
        test_inventory_json_output,
        test_missing_compile_db_is_clear,
        test_compile_db_without_source_command_is_clear,
        test_compile_db_only_ignores_inactive_source,
        test_compile_db_only_requires_compile_database,
        test_fact_snapshot_reuses_compile_database_includes_for_headers,
        test_active_headers_only_uses_translation_unit_language,
        test_header_root_discovers_sibling_library_includes,
        test_allow_file_suppresses_named_boundary,
        test_stale_allow_rule_fails,
        test_timeout_must_be_positive,
        test_keep_going_reports_partial_results_and_parse_failures,
        test_static_inline_header_calls_are_audited_at_header_location,
        test_indirect_function_pointer_calls_are_reported_as_boundaries,
        test_module_fact_output_uses_clang_ast_for_c_facts,
        test_reusable_fact_snapshot_and_input_manifest,
        test_module_facts_include_bool_returning_definitions,
        test_module_facts_parse_cxx_headers_as_cxx,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
