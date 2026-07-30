#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "p101-wrapper-audit"


def run_tool(*args: str) -> subprocess.CompletedProcess[str]:
    command = [sys.executable, str(TOOL), *args]
    if os.environ.get("P101_COVERAGE") == "1":
        command = [sys.executable, "-m", "coverage", "run", "--parallel-mode", str(TOOL), *args]
    return subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


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


def test_simple_local_function_pointer_target_is_resolved() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            #include <stdlib.h>
            int main(void) {
                void *(*allocate)(size_t) = malloc;
                void *value = allocate(4);
                free(value);
                return 0;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool(str(source))
        assert result.returncode == 1, result.stderr + result.stdout
        assert "missed-wrapper: malloc" in result.stdout
        assert "indirect-call: allocate" not in result.stdout


def test_unresolved_function_pointer_parameter_remains_boundary() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            static int invoke(int (*operation)(void)) {
                return operation();
            }
            int main(void) {
                return invoke(0);
            }
            """,
            encoding="utf-8",
        )
        result = run_tool(str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "indirect-call: operation" in result.stdout


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


def test_cross_translation_unit_definition_is_local() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "helper.c").write_text("int helper(void) { return 7; }\n", encoding="utf-8")
        (root / "main.c").write_text("int helper(void); int main(void) { return helper(); }\n", encoding="utf-8")

        result = run_tool("--strict-external", str(root))

        assert result.returncode == 0, result.stderr + result.stdout
        assert "external-call: helper" not in result.stdout


def test_declaration_without_definition_remains_external() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text("int provided_elsewhere(void); int main(void) { return provided_elsewhere(); }\n", encoding="utf-8")

        result = run_tool("--strict-external", str(source))

        assert result.returncode == 1, result.stderr + result.stdout
        assert "external-call: provided_elsewhere" in result.stdout


def test_module_facts_exclude_compiler_pseudo_files() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text("#define OWN_MACRO 1\nint main(void) { return OWN_MACRO - 1; }\n", encoding="utf-8")

        result = run_tool("--emit-module-facts", str(source))

        assert result.returncode == 0, result.stderr + result.stdout
        assert "<built-in>" not in result.stdout
        assert "<command line>" not in result.stdout
        assert "\tOWN_MACRO" in result.stdout


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


def test_fortified_builtin_matches_source_level_boundary_rule() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "main.c"
        allow = root / "allow.txt"
        source.write_text(
            """
            #include <stddef.h>
            static void *copy_bytes(void *dst, const void *src, size_t size) {
                return __builtin___memcpy_chk(
                    dst, src, size, __builtin_object_size(dst, 0));
            }
            int main(void) {
                char dst[4] = {0};
                const char src[4] = {1, 2, 3, 4};
                return copy_bytes(dst, src, sizeof(src)) == dst ? 0 : 1;
            }
            """,
            encoding="utf-8",
        )
        allow.write_text("main.c:copy_bytes:memcpy\n", encoding="utf-8")
        result = run_tool("--strict-external", "--allow-file", str(allow), str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        assert "stale boundary rule" not in result.stderr
        assert "__builtin___memcpy_chk" not in result.stdout


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


def test_compile_database_keeps_flags_per_translation_unit() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        first = root / "first.c"
        second = root / "second.c"
        database = root / "compile_commands.json"
        first.write_text(
            "#ifndef FIRST\n#error FIRST is required\n#endif\n#ifdef SECOND\n#error SECOND leaked\n#endif\nint first(void) { return 1; }\n",
            encoding="utf-8",
        )
        second.write_text(
            "#ifndef SECOND\n#error SECOND is required\n#endif\n#ifdef FIRST\n#error FIRST leaked\n#endif\nint second(void) { return 2; }\n",
            encoding="utf-8",
        )
        database.write_text(
            json.dumps(
                [
                    {"directory": str(root), "file": str(first), "arguments": ["clang", "-DFIRST", "-c", str(first)]},
                    {"directory": str(root), "file": str(second), "arguments": ["clang", "-DSECOND", "-c", str(second)]},
                ]
            ),
            encoding="utf-8",
        )

        result = run_tool("--compile-db", str(database), "--compile-db-only", str(root))

        assert result.returncode == 0, result.stderr + result.stdout


def test_module_facts_ignore_inactive_preprocessor_branches() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            #if 0
            #define INACTIVE_LIMIT 99
            #include "inactive.h"
            #endif
            #define ACTIVE_LIMIT 7
            int main(void) { return ACTIVE_LIMIT; }
            """,
            encoding="utf-8",
        )

        result = run_tool("--emit-module-facts", str(source))

        assert result.returncode == 0, result.stderr + result.stdout
        assert "ACTIVE_LIMIT" in result.stdout
        assert "INACTIVE_LIMIT" not in result.stdout
        assert "inactive.h" not in result.stdout


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


def test_initialized_function_pointer_calls_are_resolved() -> None:
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
        assert result.returncode == 1, result.stderr + result.stdout
        assert "indirect_calls: 0" in result.stdout
        assert "missed-wrapper: puts" in result.stdout
        assert "indirect-call: fp" not in result.stdout


def test_reassigned_function_pointer_with_competing_targets_is_indirect() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            int puts(const char *);
            static int local_puts(const char *text) { (void)text; return 0; }
            int run(int external) {
                int (*operation)(const char *) = local_puts;
                if(external) {
                    operation = puts;
                }
                return operation("hello");
            }
            """,
            encoding="utf-8",
        )

        result = run_tool(str(source))

        assert result.returncode == 0, result.stderr + result.stdout
        assert "indirect-call: operation" in result.stdout
        assert "missed-wrapper: puts" not in result.stdout


def test_atomic_expression_is_audited() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "main.c"
        source.write_text(
            """
            #include <stdatomic.h>
            unsigned int increment(atomic_uint *value) {
                return atomic_fetch_add(value, 1U);
            }
            """,
            encoding="utf-8",
        )

        result = run_tool(str(source))

        assert result.returncode == 1, result.stderr + result.stdout
        assert "missed-wrapper: atomic_fetch_add" in result.stdout
        assert "p101_atomic_uint_fetch_add" in result.stdout


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
                struct p101_env *created = p101_env_create(err, NULL);
                (void)created;
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
        assert "\tERROR_DISCARD" in result.stdout
        create_fact = next(line for line in result.stdout.splitlines() if "\tCALL\t" in line and "\tp101_env_create\t" in line)
        assert create_fact.endswith("\tp101_env_create\t0\t1")
        assert "\tTYPE\t" in result.stdout
        assert "\tthing_callback" in result.stdout
        assert "\tthing_state" in result.stdout
        assert "\tMACRO\t" in result.stdout
        assert "\tTHING_LIMIT" in result.stdout


def test_module_fact_names_preserve_subdirectories() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        first = root / "src" / "alpha"
        second = root / "src" / "beta"
        public = root / "include" / "p101_demo" / "alpha"
        first.mkdir(parents=True)
        second.mkdir(parents=True)
        public.mkdir(parents=True)
        (first / "common.c").write_text("int alpha_common(void) { return 1; }\n", encoding="utf-8")
        (second / "common.c").write_text("int beta_common(void) { return 2; }\n", encoding="utf-8")
        (public / "common.h").write_text("int alpha_common(void);\n", encoding="utf-8")

        result = run_tool("--emit-module-facts", str(root))

        assert result.returncode == 0, result.stderr + result.stdout
        assert "\talpha/common\t" in result.stdout
        assert "\tbeta/common\t" in result.stdout


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


def test_mutation_candidates_use_clang_locations() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "main.c"
        output = root / "mutations.json"
        source.write_text(
            """
            #include <stdbool.h>
            struct p101_env;
            struct p101_error;
            bool p101_error_has_error(const struct p101_error *);
            void p101_free(const struct p101_env *, void *);
            int check(const struct p101_env *env, struct p101_error *err, int value, void *memory) {
                if(value < 7 && p101_error_has_error(err)) {
                    p101_free(env, memory);
                    return 1;
                }
                return 0;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool("--mutation-candidates-output", str(output), str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        data = json.loads(output.read_text(encoding="utf-8"))
        assert data["schema"] == "p101-mutation-candidates-v1"
        operators = {item["operator"] for item in data["candidates"]}
        assert {"comparison-boundary", "error-predicate", "skip-cleanup"} <= operators
        for candidate in data["candidates"]:
            source_bytes = source.read_bytes()
            assert source_bytes[candidate["start"] : candidate["end"]].decode() == candidate["original"]


def test_error_flow_facts_split_if_branches_and_find_reachable_chains() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "flow.c"
        source.write_text(
            """
            struct p101_env;
            struct p101_error;
            int p101_first(const struct p101_env *, struct p101_error *);
            int p101_second(const struct p101_env *, struct p101_error *);
            int p101_error_has_error(const struct p101_error *);
            int separate(const struct p101_env *env, struct p101_error *err, int choice) {
                if(choice) {
                    p101_first(env, err);
                } else {
                    p101_second(env, err);
                }
                return p101_error_has_error(err);
            }
            int chained(const struct p101_env *env, struct p101_error *err) {
                p101_first(env, err);
                p101_second(env, err);
                return 0;
            }
            """,
            encoding="utf-8",
        )
        result = run_tool("--emit-module-facts", str(source))
        assert result.returncode == 0, result.stderr + result.stdout
        chain_notes = [line for line in result.stdout.splitlines() if "\tERROR_UNCHECKED_CHAIN" in line]
        assert len(chain_notes) == 1
        assert "\t17\tERROR_UNCHECKED_CHAIN" in chain_notes[0]


def test_error_flow_checks_the_matching_error_object() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "flow.c"
        source.write_text(
            """
            struct p101_env;
            struct p101_error;
            int p101_first(const struct p101_env *, struct p101_error *);
            int p101_second(const struct p101_env *, struct p101_error *);
            int p101_error_has_error(const struct p101_error *);
            int mismatched(const struct p101_env *env, struct p101_error *first, struct p101_error *second) {
                p101_first(env, first);
                (void)p101_error_has_error(second);
                p101_second(env, first);
                return 0;
            }
            """,
            encoding="utf-8",
        )

        result = run_tool("--emit-module-facts", str(source))

        assert result.returncode == 0, result.stderr + result.stdout
        chain_notes = [line for line in result.stdout.splitlines() if "\tERROR_UNCHECKED_CHAIN" in line]
        assert len(chain_notes) == 1
        assert "\t10\tERROR_UNCHECKED_CHAIN" in chain_notes[0]


def test_multiline_optional_error_annotation_uses_call_start_line() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "optional.c"
        source.write_text(
            """
            struct p101_env;
            struct p101_error;
            int p101_format(const struct p101_env *, struct p101_error *, char *, int);
            void format(const struct p101_env *env, char *output) {
                p101_format(env,
                            0,
                            output,
                            16); /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: bounded diagnostic. */
            }
            """,
            encoding="utf-8",
        )

        result = run_tool("--emit-module-facts", str(source))

        assert result.returncode == 0, result.stderr + result.stdout
        optional_notes = [line for line in result.stdout.splitlines() if "\tERROR_OPTIONAL" in line]
        assert any("\t6\tERROR_OPTIONAL" in line for line in optional_notes)


def test_generic_wrapper_form_contract_checks_shape_and_native_signature() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = root / "wrapper.c"
        contract = root / "wrapper-contract.json"
        source.write_text(
            """
            struct app_context;
            struct app_error;
            int native_open(const char *path, int flags);
            void trace_enter(const struct app_context *context);
            void trace_exit(const struct app_context *context);
            int inject_fault(const struct app_context *context);

            int wrap_open(const struct app_context *context,
                          struct app_error *error,
                          const char *path,
                          int flags)
            {
                trace_enter(context);
                (void)error;
                (void)inject_fault(context);
                int result = native_open(path, flags);
                trace_exit(context);
                return result;
            }
            """,
            encoding="utf-8",
        )
        contract.write_text(
            json.dumps(
                {
                    "schema": "p101-wrapper-form-contract-v1",
                    "selector": {"include": "^wrap_", "public_only": True, "minimum_matches": 1},
                    "mapping": {"strip_prefix": "wrap_"},
                    "context_parameter": {"index": 0, "type_contains": "app_context", "mode": "required"},
                    "error_parameter": {"index": 1, "type_contains": "app_error", "mode": "optional"},
                    "requirements": {
                        "balanced_trace": True,
                        "fault": "when-error",
                        "target_required": True,
                        "target_call_count": 1,
                        "compare_target_signature": True,
                    },
                    "instrumentation_calls": {
                        "trace_entry": ["trace_enter"],
                        "trace_exit": ["trace_exit"],
                        "fault": ["inject_fault"],
                    },
                    "overrides": [{"match": "^wrap_open$", "target": "native_open"}],
                }
            ),
            encoding="utf-8",
        )

        clean = run_tool("--wrapper-form-contract", str(contract), "--wrapper-form-only", str(source))
        assert clean.returncode == 0, clean.stderr + clean.stdout
        assert "functions_checked: 1" in clean.stdout
        assert "findings: 0" in clean.stdout

        source.write_text(source.read_text(encoding="utf-8").replace("trace_exit(context);", ""), encoding="utf-8")
        broken = run_tool("-j", "--wrapper-form-contract", str(contract), "--wrapper-form-only", str(source))
        assert broken.returncode == 1, broken.stderr + broken.stdout
        data = json.loads(broken.stdout)
        assert data["schema"] == "p101-wrapper-form-findings-v1"
        assert any(finding["id"] == "P101-WFORM-004" for finding in data["findings"])


def main() -> int:
    tests = [
        test_missed_wrapper_and_local_function,
        test_external_inventory_does_not_fail_by_default,
        test_simple_local_function_pointer_target_is_resolved,
        test_unresolved_function_pointer_parameter_remains_boundary,
        test_bool_returning_local_function_is_not_external,
        test_cross_translation_unit_definition_is_local,
        test_declaration_without_definition_remains_external,
        test_module_facts_exclude_compiler_pseudo_files,
        test_compiler_builtins_are_not_external_boundaries,
        test_fortified_builtin_matches_source_level_boundary_rule,
        test_json_output,
        test_inventory_json_output,
        test_missing_compile_db_is_clear,
        test_compile_db_without_source_command_is_clear,
        test_compile_db_only_ignores_inactive_source,
        test_compile_db_only_requires_compile_database,
        test_compile_database_keeps_flags_per_translation_unit,
        test_module_facts_ignore_inactive_preprocessor_branches,
        test_fact_snapshot_reuses_compile_database_includes_for_headers,
        test_active_headers_only_uses_translation_unit_language,
        test_header_root_discovers_sibling_library_includes,
        test_allow_file_suppresses_named_boundary,
        test_stale_allow_rule_fails,
        test_timeout_must_be_positive,
        test_keep_going_reports_partial_results_and_parse_failures,
        test_static_inline_header_calls_are_audited_at_header_location,
        test_initialized_function_pointer_calls_are_resolved,
        test_reassigned_function_pointer_with_competing_targets_is_indirect,
        test_atomic_expression_is_audited,
        test_module_fact_output_uses_clang_ast_for_c_facts,
        test_module_fact_names_preserve_subdirectories,
        test_reusable_fact_snapshot_and_input_manifest,
        test_module_facts_include_bool_returning_definitions,
        test_module_facts_parse_cxx_headers_as_cxx,
        test_mutation_candidates_use_clang_locations,
        test_error_flow_facts_split_if_branches_and_find_reachable_chains,
        test_error_flow_checks_the_matching_error_object,
        test_multiline_optional_error_annotation_uses_call_start_line,
        test_generic_wrapper_form_contract_checks_shape_and_native_signature,
    ]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
