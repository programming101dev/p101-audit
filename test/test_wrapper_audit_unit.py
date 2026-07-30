#!/usr/bin/env python3
from __future__ import annotations

import contextlib
import importlib.machinery
import importlib.util
import io
import json
import runpy
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "p101-wrapper-audit"


def load_tool():
    loader = importlib.machinery.SourceFileLoader("p101_wrapper_audit", str(TOOL))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    assert spec is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[loader.name] = module
    loader.exec_module(module)
    return module


tool = load_tool()


def capture(function, *args):
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        result = function(*args)
    return result, stdout.getvalue(), stderr.getvalue()


def test_discovery_helpers() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        nested = root / "a" / "b"
        nested.mkdir(parents=True)
        database = root / "compile_commands.json"
        database.write_text("[]", encoding="utf-8")
        assert tool.find_compile_db([nested]) == database
        (nested / ".git").mkdir()
        local_database = nested / "build" / "compile_commands.json"
        local_database.parent.mkdir()
        local_database.write_text("[]", encoding="utf-8")
        assert tool.find_compile_db([nested / "missing.c"]) == local_database
        assert tool.find_compile_db([root / "nowhere" / "deep" / "file.c"]) == database

        source = root / "relative.c"
        absolute = root / "absolute.c"
        database.write_text(
            json.dumps(
                [
                    {},
                    {"directory": str(root), "file": "relative.c"},
                    {"directory": "/ignored", "file": str(absolute)},
                ]
            ),
            encoding="utf-8",
        )
        entries = tool.load_compile_db(database)
        assert source in entries and absolute in entries
        assert tool.load_compile_db(None) == {}
        assert tool.load_compile_db(root / "missing") == {}

        header = root / "single.h"
        header.write_text("", encoding="utf-8")
        assert tool.discover_headers([header]) == [header]
        assert tool.entry_to_command({"arguments": ["cc", 1]}) == ["cc", "1"]
        assert tool.entry_to_command({"command": "cc -c a.c"}) == ["cc", "-c", "a.c"]
        assert tool.entry_to_command({}) is None
        compile_units = tool.discover_sources(
            [root],
            {
                root / "not-source.txt": {"arguments": ["cc"]},
                root.parent / "outside.c": {"arguments": ["cc"]},
                source: {"arguments": ["cc"], "directory": str(root)},
            },
            False,
        )
        assert [unit.source for unit in compile_units] == [source]
        assert tool.discover_sources([root / "does-not-exist"], {}, False) == []


def test_header_and_inventory_helpers() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        installed = root / "installed"
        (installed / "p101_demo").mkdir(parents=True)
        sibling = root / "lib_demo" / "include"
        sibling.mkdir(parents=True)
        discovered = tool.discover_header_include_dirs([root / "missing", installed, root])
        assert installed in discovered and sibling in discovered

        source_dir = root / "src"
        source_dir.mkdir()
        flags = tool.header_compile_flags(
            {
                root / "empty.c": {},
                root / "a.c": {
                    "arguments": [
                        "clang",
                        "-D",
                        "A=1",
                        "-I",
                        "inc",
                        "-UOLD",
                        "-std=c17",
                        "--sysroot=/sdk",
                        "-include",
                        "prefix.h",
                    ]
                }
            },
            [source_dir, root / "missing"],
            ["-DEXTRA"],
            [installed],
        )
        assert flags[:3] == ["-DEXTRA", "-D", "A=1"]
        assert "-Iinc" not in flags
        assert "-I" in flags and "inc" in flags
        assert f"-I{source_dir}" in flags
        duplicate_flag = f"-I{installed}"
        duplicate_flags = tool.header_compile_flags({}, [], [duplicate_flag], [installed])
        assert duplicate_flags.count(duplicate_flag) == 1

        header = installed / "p101_demo" / "p101_api.h"
        header.write_text("void p101_open(void); void p101_atomic_uint_load(void);", encoding="utf-8")
        inventory = tool.wrapper_inventory([installed])
        assert inventory["open"] == "p101_open"
        assert inventory["atomic_load"] == "p101_atomic_uint_load"
        empty_atomic_inventory = {}
        tool.add_atomic_aliases(empty_atomic_inventory)
        assert empty_atomic_inventory == {}
        assert tool.wrapper_inventory([root / "missing"])
        with mock.patch.object(Path, "read_text", side_effect=OSError("denied")):
            try:
                tool.wrapper_inventory([installed])
                raise AssertionError("header read failure accepted")
            except RuntimeError as exc:
                assert "denied" in str(exc)


def test_clang_command_and_location_helpers() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "unit.m"
        source.write_text("line1\nline2\n", encoding="utf-8")
        unit = tool.TranslationUnit(source, None, root)
        timeout = subprocess.TimeoutExpired(["clang"], 1)
        with mock.patch.object(tool.subprocess, "run", side_effect=timeout):
            for function, arguments in (
                (tool.clang_ast, ("clang", unit, [], 1.0)),
                (tool.clang_preprocessor_facts, ("clang", unit, [], [root], 1.0)),
            ):
                try:
                    function(*arguments)
                    raise AssertionError("timeout accepted")
                except RuntimeError as exc:
                    assert "timed out" in str(exc)

        failed = subprocess.CompletedProcess([], 1, stdout="", stderr="bad")
        with mock.patch.object(tool.subprocess, "run", return_value=failed):
            try:
                tool.clang_preprocessor_facts("clang", unit, [], [root], 1.0)
                raise AssertionError("preprocessor failure accepted")
            except RuntimeError as exc:
                assert "bad" in str(exc)

        empty = tool.TranslationUnit(source, [], root)
        try:
            tool.ast_command_from_compile_command("clang", empty, [])
            raise AssertionError("empty compile command accepted")
        except ValueError:
            pass
        command_unit = tool.TranslationUnit(
            source,
            ["cc", "-c", str(source), "-o", "out", "-MF", "dep", "-MT", "target", "-MQ", "queue", "-output", "-ObjC"],
            root,
        )
        command = tool.ast_command_from_compile_command("clang", command_unit, ["-DEXTRA"])
        assert command[0] == "clang" and "-ObjC" in command and "out" not in command
        tool.ast_command_from_compile_command("clang", tool.TranslationUnit(source, ["cc", "-o"], root), [])

        assert tool.language_for(Path("a.m")) == "objective-c"
        assert tool.language_for(Path("a.mm")) == "objective-c++"
        assert tool.language_for(Path("a.unknown")) == "c"
        data = source.read_bytes()
        node = {"loc": {}, "range": {"begin": {"spellingLoc": {"offset": 6}}}}
        assert tool.spelling_location(node, source, data) == (source, 2, 1)
        assert tool.spelling_location({"loc": {"spellingLoc": {"file": str(source), "line": 1, "col": 2}}}) == (source, 1, 2)
        assert tool.spelling_location({"range": {"begin": {"expansionLoc": {"file": str(source), "line": 2}}}})[0] == source
        assert tool.node_offset({"loc": {"spellingLoc": {"offset": 2}}}) == 2
        assert tool.node_offset({"range": {"begin": {"spellingLoc": {"offset": 3}}}}) == 3
        assert tool.node_offset({"range": {"begin": {"expansionLoc": {"offset": 4}}}}) == 4
        assert tool.range_location({"expansionLoc": {"offset": 1}}) == {"offset": 1}
        assert tool.range_location({"spellingLoc": {"offset": 2}}) == {"offset": 2}
        assert tool.node_range_offsets({"range": {"begin": {}, "end": {}}}) is None
        annotated_call = {
            "range": {
                "begin": {"offset": 0},
                "end": {"offset": 7, "tokLen": 1},
            }
        }
        assert tool.call_has_optional_error_annotation(
            annotated_call,
            b"p101_x(); /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: expected. */",
        )

        ancestor = {"kind": "FunctionDecl", "loc": {"file": str(root / "missing.c")}, "inner": [{"kind": "CompoundStmt"}]}
        unresolved = {"loc": {"offset": 1, "includedFrom": {"file": "header.h"}}}
        assert tool.spelling_location_with_ancestors(unresolved, (ancestor,), source, data)[0] == root / "missing.c"
        ancestor_without_file = {"kind": "FunctionDecl", "inner": [{"kind": "CompoundStmt"}]}
        assert tool.spelling_location_with_ancestors(unresolved, (ancestor_without_file,), source, data)[0] is None
        no_offset = {"loc": {"includedFrom": {"file": "header.h"}}}
        assert tool.spelling_location_with_ancestors(no_offset, (ancestor,), source, data)[0] == root / "missing.c"
        assert tool.spelling_location_with_ancestors({}, ({"kind": "Other"},), source, data)[0] is None
        assert list(tool.iter_nodes({"inner": ["bad"]}))
        assert list(tool.iter_nodes_with_ancestors({"inner": ["bad"]}))


def function_node(name: str, offset: int, calls=(), parameters=()):
    return {
        "kind": "FunctionDecl",
        "name": name,
        "loc": {"offset": offset},
        "inner": [
            *[
                {"kind": "ParmVarDecl", "type": {"qualType": parameter}}
                for parameter in parameters
            ],
            {
                "kind": "CompoundStmt",
                "inner": [
                    {
                        "kind": "CallExpr",
                        "referencedDecl": {"kind": "FunctionDecl", "name": call},
                    }
                    for call in calls
                ],
            },
        ],
    }


def test_instrumentation_and_ast_helpers() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "unit.c"
        source.write_text(" " * 200, encoding="utf-8")
        helper = function_node("helper", 1, ["p101_env_trace", "p101_env_check_fault"])
        wrapper = function_node("p101_work", 10, ["helper"], ["const struct p101_env *", "struct p101_error *"])
        duplicate = function_node("p101_work", 10)
        unnamed = function_node("", 20)
        outside = function_node("p101_outside", 30)
        outside["loc"] = {"file": str(root.parent / "outside.c"), "line": 1}
        ast = {"kind": "TranslationUnitDecl", "inner": [helper, wrapper, duplicate, unnamed, outside]}
        unit = tool.TranslationUnit(source, None, root)
        records = tool.collect_instrumentation([(unit, ast)], [root])
        record = next(item for item in records if item["function"] == "p101_work")
        assert record["trace_entry"] and record["fault"] and record["has_env"] and record["has_error"]
        output = root / "instrumentation.json"
        tool.write_instrumentation(output, records)
        assert json.loads(output.read_text(encoding="utf-8"))["functions"] == records
        no_body = {"kind": "FunctionDecl", "name": "p101_no_body"}
        assert tool.collect_instrumentation([(unit, {"inner": [no_body]})], [root]) == []

        no_name = {"kind": "FunctionDecl", "loc": {"offset": 1}, "inner": [{"kind": "CompoundStmt"}]}
        assert tool.collect_local_functions([(unit, {"inner": [no_name]})], [root]) == set()

        assert tool.call_target({"kind": "AtomicExpr", "name": "atomic_load"}) == ("atomic_load", "FunctionDecl")
        assert tool.call_target({"referencedDecl": {"name": "direct"}}) == ("direct", "")
        assert tool.call_target({}) == (None, None)
        assert tool.call_contract({"inner": [{"referencedDecl": {"type": "bad"}}, {"referencedDecl": {"type": {"qualType": ""}}}]}) == (False, False)
        assert tool.node_is_null_pointer({"kind": "GNUNullExpr"})
        assert tool.declref_name({}) is None
        assert tool.declref_info({"inner": [{"kind": "Other"}]}) == (None, None)
        assert tool.declref_info({"inner": ["bad"]}) == (None, None)
        assert tool.call_argument_object({"inner": []}, 1) == "?"

        malformed_assignment = {
            "kind": "FunctionDecl",
            "name": "caller",
            "inner": [
                {"kind": "CompoundStmt"},
                {"kind": "BinaryOperator", "opcode": "=", "inner": [{"kind": "DeclRefExpr"}]},
            ],
        }
        malformed_assignment["inner"].append({"kind": "VarDecl"})
        malformed_assignment["inner"].append("bad")
        assert tool.local_function_pointer_targets(malformed_assignment) == {}


def test_mutation_and_finding_edge_paths() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "unit.c"
        source.write_text("a < b;", encoding="utf-8")
        unit = tool.TranslationUnit(source, None, root)
        bad_nodes = [
            {"kind": "BinaryOperator", "opcode": "<", "loc": {"file": str(root.parent / "outside.c"), "includedFrom": {}}},
            {"kind": "BinaryOperator", "opcode": "<", "loc": {"offset": 0}, "range": {}},
            {
                "kind": "BinaryOperator",
                "opcode": "<",
                "loc": {"offset": 0},
                "range": {"begin": {"offset": -1}, "end": {"offset": 99}},
            },
        ]
        ast = {"inner": bad_nodes}
        assert tool.collect_mutation_candidates([(unit, ast)], [root]) == []
        with mock.patch.object(tool, "spelling_location_with_ancestors", return_value=(root.parent / "outside.c", 1, 1)):
            assert tool.collect_mutation_candidates([(unit, {"kind": "CallExpr"})], [root]) == []
        with (
            mock.patch.object(tool, "spelling_location_with_ancestors", return_value=(source, 1, 1)),
            mock.patch.object(tool, "node_range_offsets", return_value=(-1, 100)),
        ):
            assert tool.collect_mutation_candidates([(unit, {"kind": "CallExpr"})], [root]) == []
        edge_nodes = [
            {
                "kind": "BinaryOperator",
                "opcode": "<",
                "loc": {"offset": 0},
                "range": {"begin": {"offset": 0}, "end": {"offset": 4, "tokLen": 1}},
                "inner": [],
            },
            {
                "kind": "BinaryOperator",
                "opcode": "<",
                "loc": {"offset": 0},
                "range": {"begin": {"offset": 0}, "end": {"offset": 4, "tokLen": 1}},
                "inner": [{"range": {}}, {"range": {}}],
            },
            {
                "kind": "BinaryOperator",
                "opcode": ">",
                "loc": {"offset": 0},
                "range": {"begin": {"offset": 0}, "end": {"offset": 4, "tokLen": 1}},
                "inner": [
                    {"range": {"begin": {"offset": 0}, "end": {"offset": 0}}},
                    {"range": {"begin": {"offset": 4}, "end": {"offset": 4}}},
                ],
            },
            {
                "kind": "CallExpr",
                "loc": {"offset": 0},
                "range": {"begin": {"offset": 0}, "end": {"offset": 4, "tokLen": 1}},
                "referencedDecl": {"name": "p101_error_has_error", "kind": "FunctionDecl"},
            },
            {
                "kind": "CallExpr",
                "loc": {"offset": 0},
                "range": {"begin": {"offset": 0}, "end": {"offset": 4, "tokLen": 1}},
                "referencedDecl": {"name": "p101_free", "kind": "FunctionDecl"},
            },
        ]
        tool.collect_mutation_candidates([(unit, {"inner": edge_nodes})], [root])
        valid = {
            "kind": "BinaryOperator",
            "opcode": "<",
            "loc": {"offset": 0},
            "range": {"begin": {"offset": 0}, "end": {"offset": 4, "tokLen": 1}},
            "inner": [
                {"range": {"begin": {"offset": 0}, "end": {"offset": 0}}},
                {"range": {"begin": {"offset": 4}, "end": {"offset": 4}}},
            ],
        }
        assert len(tool.collect_mutation_candidates([(unit, {"inner": [valid, valid]})], [root])) == 1

        nameless = {"kind": "CallExpr", "loc": {"offset": 0}}
        self_call = {
            "kind": "FunctionDecl",
            "name": "p101_open",
            "loc": {"offset": 0},
            "inner": [
                {"kind": "CompoundStmt"},
                {
                    "kind": "CallExpr",
                    "loc": {"offset": 0},
                    "referencedDecl": {"name": "open", "kind": "FunctionDecl"},
                },
            ],
        }
        duplicate_call = {
            "kind": "CallExpr",
            "loc": {"offset": 0},
            "referencedDecl": {"name": "external", "kind": "FunctionDecl"},
        }
        finding_ast = {"inner": [nameless, self_call, duplicate_call, duplicate_call]}
        findings = tool.collect_findings([(unit, finding_ast)], [root], set(), {"open": "p101_open"}, set(), [])
        assert len(findings) == 1 and findings[0].name == "external"
        allowed_call = {
            "kind": "CallExpr",
            "loc": {"offset": 0},
            "referencedDecl": {"name": "allowed", "kind": "FunctionDecl"},
        }
        outside_call = {
            "kind": "CallExpr",
            "loc": {"file": str(root.parent / "outside.c"), "includedFrom": {}},
            "referencedDecl": {"name": "outside", "kind": "FunctionDecl"},
        }
        assert tool.collect_findings([(unit, {"inner": [allowed_call, outside_call]})], [root], set(), {}, {"allowed"}, []) == []


def test_small_helpers_and_flow() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        allow = root / "allow.txt"
        allow.write_text("bad\n", encoding="utf-8")
        try:
            tool.load_allow_files([allow])
            raise AssertionError("bad allow rule accepted")
        except ValueError:
            pass

        assert tool.node_has_body({"inner": ["bad", {"kind": "Other", "inner": [{"kind": "CompoundStmt"}]}]})
        assert tool.node_type_text({"type": "bad"}) == ""
        assert not tool.function_has_parameter_type({"inner": ["bad"]}, "p101_env")
        loop = {"kind": "WhileStmt", "inner": [{"kind": "IntegerLiteral"}]}
        source = root / "x.c"
        source.write_text("", encoding="utf-8")
        unit = tool.TranslationUnit(source, None, root)
        pending, findings = tool.statement_error_flow(loop, {"err"}, unit, b"x")
        assert pending == {"err"} and findings == []
        assert tool.statement_error_flow({"kind": "ReturnStmt"}, {"err"}, unit, b"x") == (set(), [])
        assert tool.module_name_for(Path("src")) == "src"
        assert tool.collect_module_facts(
            [(unit, {"inner": [{"kind": "FunctionDecl", "loc": {"offset": 0}, "inner": [{"kind": "CompoundStmt"}]}]})],
            [root],
        ) == []
        declaration = {
            "kind": "FunctionDecl",
            "name": "declared",
            "loc": {"file": str(source), "line": 1},
        }
        nameless_call = {"kind": "CallExpr", "loc": {"offset": 0}}
        tool.collect_module_facts([(unit, {"inner": [declaration, nameless_call]})], [root])

        header = root / "x.h"
        header.write_text("", encoding="utf-8")
        header_unit = tool.TranslationUnit(header, None, root)
        type_ast = {
            "inner": [
                {"kind": "TypedefDecl", "loc": {"offset": 0}},
                {"kind": "RecordDecl", "name": "hidden", "isImplicit": True, "loc": {"offset": 0}},
            ]
        }
        assert tool.collect_module_facts([(header_unit, type_ast)], [root]) == []

        _, output, _ = capture(tool.print_inventory, {"b": "p101_b", "a": "p101_a"})
        assert output.splitlines()[0].startswith("a\t")


def base_args(root: Path):
    args = tool.parse_args([str(root)])
    args.header_root = []
    return args


def run_main_with(args, **patches):
    defaults = {
        "parse_args": mock.patch.object(tool, "parse_args", return_value=args),
    }
    defaults.update(patches)
    with contextlib.ExitStack() as stack:
        for patcher in defaults.values():
            stack.enter_context(patcher)
        return capture(tool.main, [])


def test_manifest_modes() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "a.c"
        source.write_text("", encoding="utf-8")
        output = root / "manifest.json"
        unit = tool.TranslationUnit(source, None, root)
        for compile_only, compile_path, expected in (
            (True, root / "db", "compile-database-only"),
            (False, root / "db", "compile-database-plus-requested-paths"),
            (False, None, "requested-paths"),
        ):
            if compile_path is not None:
                compile_path.write_text("[]", encoding="utf-8")
            tool.write_input_manifest(output, [root], compile_path, compile_only, "missing-clang", None, [], set(), [unit], [unit], [])
            assert json.loads(output.read_text(encoding="utf-8"))["selection_mode"] == expected


def test_module_fact_edges() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "edge.c"
        source.write_text("", encoding="utf-8")
        unit = tool.TranslationUnit(source, None, root)

        declaration = {"kind": "FunctionDecl", "name": "declared", "loc": {"file": str(source), "line": 1}}
        assert tool.collect_module_facts([(unit, declaration)], [root]) == []

        nameless_call = {"kind": "CallExpr", "loc": {"file": str(source), "line": 1}}
        assert tool.collect_module_facts([(unit, nameless_call)], [root]) == []

        named_call = {
            "kind": "CallExpr",
            "loc": {"file": str(source), "line": 1},
            "referencedDecl": {"name": "plain", "kind": "FunctionDecl"},
        }
        call_facts = tool.collect_module_facts([(unit, named_call)], [root])
        assert any(fact[0] == "CALL" for fact in call_facts)

        header = root / "edge.h"
        header.write_text("", encoding="utf-8")
        header_unit = tool.TranslationUnit(header, None, root)
        for node in (
            {"kind": "TypedefDecl", "loc": {"file": str(header), "line": 1}},
            {"kind": "RecordDecl", "name": "implicit", "isImplicit": True, "loc": {"file": str(header), "line": 1}},
        ):
            assert tool.collect_module_facts([(header_unit, node)], [root]) == []

        duplicate = ("MACRO", source, 1, ("NAME",))
        assert tool.collect_module_facts([], [root], [duplicate, duplicate]) == [duplicate]


def test_main_error_boundaries() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "a.c"
        source.write_text("int main(void){return 0;}", encoding="utf-8")

        args = base_args(root)
        args.emit_module_facts = True
        args.facts_output = root / "facts"
        assert run_main_with(args)[0] == 2

        args = base_args(root)
        args.allow_file = [root / "missing"]
        assert run_main_with(args)[0] == 2

        args = base_args(root)
        args.show_inventory = True
        assert run_main_with(args, inventory=mock.patch.object(tool, "wrapper_inventory", return_value={"x": "p101_x"}))[0] == 0

        args = base_args(root)
        assert run_main_with(args, inventory=mock.patch.object(tool, "wrapper_inventory", return_value={}))[0] == 2

        args = base_args(root)
        assert run_main_with(args, inventory=mock.patch.object(tool, "wrapper_inventory", side_effect=RuntimeError("bad header")))[0] == 2

        bad_db = root / "bad.json"
        bad_db.write_text("{", encoding="utf-8")
        args = base_args(root)
        args.compile_db = bad_db
        assert run_main_with(args, inventory=mock.patch.object(tool, "wrapper_inventory", return_value={"x": "p101_x"}))[0] == 2

        args = base_args(root)
        assert run_main_with(
            args,
            inventory=mock.patch.object(tool, "wrapper_inventory", return_value={"x": "p101_x"}),
            sources=mock.patch.object(tool, "discover_sources", return_value=[]),
        )[0] == 2

        unit = tool.TranslationUnit(source, ["clang", "-c", str(source)], root)
        args = base_args(root)
        args.compile_db = root / "db.json"
        args.compile_db.write_text("[]", encoding="utf-8")
        assert run_main_with(
            args,
            inventory=mock.patch.object(tool, "wrapper_inventory", return_value={"x": "p101_x"}),
            sources=mock.patch.object(tool, "discover_sources", return_value=[unit]),
            ast=mock.patch.object(tool, "clang_ast", side_effect=RuntimeError("parse")),
        )[0] == 2

        args = base_args(root)
        args.keep_going = True
        assert run_main_with(
            args,
            inventory=mock.patch.object(tool, "wrapper_inventory", return_value={"x": "p101_x"}),
            sources=mock.patch.object(tool, "discover_sources", return_value=[unit]),
            ast=mock.patch.object(tool, "clang_ast", side_effect=RuntimeError("parse")),
        )[0] == 2

        for attribute, writer in (
            ("facts_output", "write_module_facts"),
            ("input_manifest", "write_input_manifest"),
            ("instrumentation_output", "write_instrumentation"),
            ("mutation_candidates_output", "write_mutation_candidates"),
        ):
            args = base_args(root)
            setattr(args, attribute, root / attribute)
            assert run_main_with(
                args,
                inventory=mock.patch.object(tool, "wrapper_inventory", return_value={"x": "p101_x"}),
                sources=mock.patch.object(tool, "discover_sources", return_value=[unit]),
                ast=mock.patch.object(tool, "clang_ast", return_value={}),
                writer=mock.patch.object(tool, writer, side_effect=OSError("full")),
            )[0] == 2


def wrapper_form_record(**updates):
    record = {
        "path": "/project/wrap.c",
        "line": 7,
        "function": "wrap_open",
        "public": True,
        "return_type": "int",
        "parameter_types": ["const struct app_context *", "struct app_error *", "const char *", "int"],
        "variadic": False,
        "calls": [
            {
                "name": "open",
                "return_type": "int",
                "parameter_types": ["const char *", "int"],
                "variadic": False,
            }
        ],
        "call_names": ["app_fault", "app_trace_enter", "app_trace_exit", "open"],
        "trace_entry": False,
        "trace_exit": False,
        "fault": False,
        "fd": False,
        "allocation": False,
        "resource": False,
    }
    record.update(updates)
    return record


def wrapper_form_contract():
    return {
        "schema": "p101-wrapper-form-contract-v1",
        "selector": {"include": "^wrap_", "exclude": ["_private$"], "minimum_matches": 1},
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
            "trace_entry": ["app_trace_enter"],
            "trace_exit": ["app_trace_exit"],
            "fault": ["app_fault"],
            "fd": ["app_fd"],
            "allocation": ["app_alloc"],
            "resource": ["app_resource"],
        },
        "capabilities": {"wrap_open": ["fd"]},
        "overrides": [],
    }


def test_wrapper_form_contract_edges() -> None:
    assert tool.node_canonical_type_text({"type": {"qualType": "size_t", "desugaredQualType": "unsigned long"}}) == "unsigned long"
    assert tool.node_canonical_type_text({"type": "bad"}) == ""
    assert tool.function_return_type({"type": {"qualType": "int (int)"}}) == "int"
    assert tool.function_return_type({"type": {"qualType": "int(int)"}}) == "int"
    assert tool.function_return_type({"type": {"qualType": "size_t(void)"}}, {"size_t": "unsigned long"}) == "unsigned long"
    assert tool.expand_type_aliases("outer", {"outer": "inner", "inner": "int"}) == "int"
    assert tool.expand_type_aliases("int", {}) == "int"
    assert tool.expand_type_aliases("a", {"a": "a a"}) == "a a a a"
    assert tool.normalized_type("char * restrict _Nonnull") == "char*"
    assert tool.referenced_function_decl({"inner": [{"referencedDecl": {"kind": "VarDecl"}}]}, {}) is None
    referenced = {"kind": "FunctionDecl", "id": "target", "name": "native"}
    full = {"kind": "FunctionDecl", "id": "target", "name": "native", "type": {"qualType": "int (void)"}}
    assert tool.referenced_function_decl({"referencedDecl": referenced}, {"target": full}) == full
    assert tool.referenced_function_decl({"referencedDecl": {"kind": "FunctionDecl", "name": "fallback"}}, {})["name"] == "fallback"

    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        path = root / "contract.json"
        valid = wrapper_form_contract()
        path.write_text(json.dumps(valid), encoding="utf-8")
        assert tool.load_wrapper_form_contract(path) == valid
        without_parameters = dict(valid)
        without_parameters.pop("context_parameter")
        without_parameters.pop("error_parameter")
        path.write_text(json.dumps(without_parameters), encoding="utf-8")
        assert tool.load_wrapper_form_contract(path) == without_parameters
        invalid_contracts = [
            [],
            {},
            {**valid, "selector": {}},
            {**valid, "selector": {"include": "["}},
            {**valid, "selector": {"include": "^wrap_", "exclude": "bad"}},
            {**valid, "selector": {"include": "^wrap_", "minimum_matches": True}},
            {**valid, "selector": {"include": "^wrap_", "minimum_matches": -1}},
            {**valid, "mapping": {}},
            {**valid, "requirements": []},
            {**valid, "overrides": [{}]},
            {**valid, "overrides": [{"match": "["}]},
            {**valid, "context_parameter": []},
            {**valid, "context_parameter": {"type_contains": "ctx", "index": True}},
            {**valid, "context_parameter": {"type_contains": "ctx", "index": -1}},
            {**valid, "error_parameter": {"type_contains": "err", "mode": "sometimes"}},
            {**valid, "capabilities": []},
            {**valid, "capabilities": {"wrap_open": [1]}},
            {**valid, "instrumentation_calls": {"fault": "app_fault"}},
            {**valid, "instrumentation_calls": {"fault": [1]}},
            {**valid, "requirements": {**valid["requirements"], "fault": "sometimes"}},
            {**valid, "requirements": {**valid["requirements"], "target_call_count": True}},
            {**valid, "requirements": {**valid["requirements"], "target_call_count": -1}},
            {**valid, "requirements": {**valid["requirements"], "balanced_trace": "yes"}},
            {**valid, "overrides": [{"match": "^wrap_", "capabilities": "fd"}]},
        ]
        for invalid in invalid_contracts:
            path.write_text(json.dumps(invalid), encoding="utf-8")
            try:
                tool.load_wrapper_form_contract(path)
                raise AssertionError(f"invalid contract accepted: {invalid}")
            except ValueError:
                pass

    contract = wrapper_form_contract()
    clean = wrapper_form_record(call_names=["app_fault", "app_trace_enter", "app_trace_exit", "app_fd", "open"])
    findings, checked, trouble = tool.check_wrapper_forms([clean], contract)
    assert checked == 1 and not findings and not trouble

    bad = wrapper_form_record(
        function="wrap_bad",
        return_type="long",
        parameter_types=["struct app_error *", "const struct app_context *", "int"],
        variadic=True,
        calls=[{"name": "open", "return_type": "int", "parameter_types": [], "variadic": False}],
        call_names=["app_fault", "app_resource", "open"],
    )
    bad_contract = wrapper_form_contract()
    bad_contract["selector"]["minimum_matches"] = 3
    bad_contract["requirements"]["fault"] = "forbidden"
    bad_contract["requirements"]["capabilities"] = ["allocation", "resource", "mystery"]
    bad_contract["capabilities"] = {"wrap_missing": ["fd"]}
    bad_contract["overrides"] = [
        {"match": "^wrap_bad$", "target_replacement": "open", "capabilities": "bad"},
        {"match": "^never_matches$", "target": "none"},
    ]
    findings, checked, trouble = tool.check_wrapper_forms(
        [bad, wrapper_form_record(function="wrap_private", public=False)],
        bad_contract,
    )
    ids = {finding.diagnostic_id for finding in findings}
    assert {"P101-WFORM-002", "P101-WFORM-003", "P101-WFORM-004", "P101-WFORM-005", "P101-WFORM-007", "P101-WFORM-900"} <= ids
    assert checked == 1
    assert any("selector matched" in value for value in trouble)
    assert any("stale override" in value for value in trouble)
    assert any("stale capability" in value for value in trouble)

    missing_target_contract = wrapper_form_contract()
    missing_target_contract["instrumentation_calls"] = {"trace_entry": "bad"}
    assert tool.check_wrapper_forms([clean], missing_target_contract)[2] == ["instrumentation_calls must be an object"]
    missing_target_contract["instrumentation_calls"] = {}
    missing_target_contract["capabilities"] = []
    assert tool.check_wrapper_forms([clean], missing_target_contract)[2] == ["capabilities must be an object"]

    missing = wrapper_form_record(
        function="native",
        parameter_types=["const struct app_context *"],
        calls=[],
        call_names=[],
    )
    missing_contract = wrapper_form_contract()
    missing_contract["selector"] = {"include": "^native$"}
    missing_contract["mapping"] = {"strip_prefix": "wrap_"}
    missing_contract["error_parameter"]["mode"] = "optional"
    missing_contract["requirements"]["fault"] = "required"
    missing_contract["requirements"]["capabilities"] = ["allocation", "resource"]
    missing_contract["capabilities"] = {}
    findings, _, _ = tool.check_wrapper_forms([missing], missing_contract)
    ids = {finding.diagnostic_id for finding in findings}
    assert {"P101-WFORM-004", "P101-WFORM-005", "P101-WFORM-006", "P101-WFORM-008"} <= ids

    built_in_contract = wrapper_form_contract()
    built_in_contract.pop("context_parameter")
    built_in_contract.pop("error_parameter")
    built_in_contract["instrumentation_calls"] = {}
    built_in_contract["requirements"]["fault"] = "optional"
    built_in_contract["requirements"]["capabilities"] = ["mystery"]
    built_in_contract["capabilities"] = {}
    built_in = wrapper_form_record(
        parameter_types=["const char *"],
        trace_entry=True,
        trace_exit=True,
        calls=[{"name": "open", "return_type": "long", "parameter_types": ["int"], "variadic": True}],
    )
    findings, _, _ = tool.check_wrapper_forms([built_in], built_in_contract)
    ids = [finding.diagnostic_id for finding in findings]
    assert ids.count("P101-WFORM-007") == 2
    assert "P101-WFORM-900" in ids

    output_finding = tool.form_finding(clean, "P101-WFORM-001", "example", detail="evidence")
    _, text_output, _ = capture(tool.print_wrapper_form_results, [output_finding], 1, ["trouble"], False)
    assert "P101-WFORM-900" in text_output and "P101-WFORM-001" in text_output
    _, json_output, _ = capture(tool.print_wrapper_form_results, [output_finding], 1, [], True)
    json_result = json.loads(json_output)
    assert json_result["findings"][0]["evidence"]["parser"] == "clang-ast"
    assert json_result["contract"]["sha256"] is None
    with tempfile.TemporaryDirectory() as temp:
        receipt_contract = Path(temp) / "contract.json"
        receipt_contract.write_text("{}", encoding="utf-8")
        _, text_output, _ = capture(tool.print_wrapper_form_results, [], 0, [], False, receipt_contract)
        assert "contract_sha256:" in text_output


def test_collect_wrapper_forms_edges() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "forms.c"
        source.write_text(" " * 300, encoding="utf-8")
        target_decl = {
            "kind": "FunctionDecl",
            "id": "native-id",
            "name": "native",
            "type": {"qualType": "int (int)"},
            "inner": [{"kind": "ParmVarDecl", "type": {"qualType": "int"}}],
        }
        fortified_decl = {
            "kind": "FunctionDecl",
            "id": "fortified-id",
            "name": "__builtin___stpncpy_chk",
            "type": {"qualType": "char *(char *, const char *, size_t, size_t)"},
        }
        stpncpy_decl = {
            "kind": "FunctionDecl",
            "id": "stpncpy-id",
            "name": "stpncpy",
            "type": {"qualType": "char *(char *, const char *, size_t)"},
            "inner": [
                {"kind": "ParmVarDecl", "type": {"qualType": "char *"}},
                {"kind": "ParmVarDecl", "type": {"qualType": "const char *"}},
                {"kind": "ParmVarDecl", "type": {"qualType": "size_t"}},
            ],
        }
        helper = function_node("helper", 1, ["trace_custom"])
        helper["type"] = {"qualType": "void (void)"}
        wrapper = function_node("wrap_native", 20, ["helper"])
        wrapper["type"] = {"qualType": "int (int)"}
        wrapper["inner"].insert(0, {"kind": "ParmVarDecl", "type": {"qualType": "int"}})
        wrapper["inner"][-1]["inner"].extend(
            [
                {
                    "kind": "CallExpr",
                    "referencedDecl": {"kind": "FunctionDecl", "id": "native-id", "name": "native"},
                },
                {"kind": "CallExpr"},
            ]
        )
        fortified_wrapper = function_node("wrap_stpncpy", 60)
        fortified_wrapper["type"] = {"qualType": "char *(char *, const char *, size_t)"}
        fortified_wrapper["inner"][-1]["inner"].append(
            {
                "kind": "CallExpr",
                "referencedDecl": {
                    "kind": "FunctionDecl",
                    "id": "fortified-id",
                    "name": "__builtin___stpncpy_chk",
                },
            }
        )
        duplicate = function_node("wrap_native", 20)
        unnamed = function_node("", 40)
        nameless_declaration = {"kind": "FunctionDecl", "id": "nameless-id"}
        outside = function_node("wrap_outside", 50)
        outside["loc"] = {"file": str(root.parent / "outside.c"), "line": 1}
        ast = {"inner": [target_decl, fortified_decl, stpncpy_decl, nameless_declaration, helper, wrapper, fortified_wrapper, duplicate, unnamed, outside]}
        unit = tool.TranslationUnit(source, None, root)
        records = tool.collect_function_forms([(unit, ast)], [root])
        record = next(item for item in records if item["function"] == "wrap_native")
        assert "trace_custom" in record["call_names"]
        native = next(call for call in record["calls"] if call["name"] == "native")
        assert native["parameter_types"] == ["int"]
        fortified_record = next(item for item in records if item["function"] == "wrap_stpncpy")
        fortified_call = next(call for call in fortified_record["calls"] if call["name"] == "stpncpy")
        assert len(fortified_call["parameter_types"]) == 3


def test_wrapper_form_main_modes() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        source = root / "a.c"
        source.write_text("", encoding="utf-8")
        unit = tool.TranslationUnit(source, None, root)
        contract_path = root / "contract.json"
        contract_path.write_text(json.dumps(wrapper_form_contract()), encoding="utf-8")

        args = base_args(root)
        args.wrapper_form_only = True
        assert run_main_with(args)[0] == 2

        args = base_args(root)
        args.wrapper_form_contract = contract_path
        args.json = True
        assert run_main_with(args)[0] == 2

        contract_path.write_text("{}", encoding="utf-8")
        args = base_args(root)
        args.wrapper_form_only = True
        args.wrapper_form_contract = contract_path
        assert run_main_with(args)[0] == 2
        contract_path.write_text(json.dumps(wrapper_form_contract()), encoding="utf-8")

        clean = wrapper_form_record(call_names=["app_fault", "app_trace_enter", "app_trace_exit", "app_fd", "open"])
        base_patches = {
            "inventory": mock.patch.object(tool, "wrapper_inventory", return_value={}),
            "sources": mock.patch.object(tool, "discover_sources", return_value=[unit]),
            "ast": mock.patch.object(tool, "clang_ast", return_value={}),
            "forms": mock.patch.object(tool, "collect_function_forms", return_value=[clean]),
        }
        args = base_args(root)
        args.wrapper_form_only = True
        args.wrapper_form_contract = contract_path
        assert run_main_with(args, **base_patches)[0] == 0

        args = base_args(root)
        args.wrapper_form_only = True
        args.wrapper_form_contract = contract_path
        finding = tool.form_finding(clean, "P101-WFORM-006", "bad")
        assert run_main_with(
            args,
            **base_patches,
            check_forms=mock.patch.object(tool, "check_wrapper_forms", return_value=([finding], 1, [])),
        )[0] == 1

        args = base_args(root)
        args.wrapper_form_only = True
        args.wrapper_form_contract = contract_path
        assert run_main_with(
            args,
            **base_patches,
            check_forms=mock.patch.object(tool, "check_wrapper_forms", return_value=([], 1, ["bad contract"])),
        )[0] == 2

        combined_patches = {
            "inventory": mock.patch.object(tool, "wrapper_inventory", return_value={"open": "wrap_open"}),
            "sources": mock.patch.object(tool, "discover_sources", return_value=[unit]),
            "ast": mock.patch.object(tool, "clang_ast", return_value={}),
            "forms": mock.patch.object(tool, "collect_function_forms", return_value=[clean]),
            "local": mock.patch.object(tool, "collect_local_functions", return_value=set()),
            "findings": mock.patch.object(tool, "collect_findings", return_value=[]),
        }
        args = base_args(root)
        args.wrapper_form_contract = contract_path
        assert run_main_with(
            args,
            **combined_patches,
            check_forms=mock.patch.object(tool, "check_wrapper_forms", return_value=([], 1, ["bad contract"])),
        )[0] == 2
        args = base_args(root)
        args.wrapper_form_contract = contract_path
        assert run_main_with(
            args,
            **combined_patches,
            check_forms=mock.patch.object(tool, "check_wrapper_forms", return_value=([finding], 1, [])),
        )[0] == 1


def main() -> int:
    test_discovery_helpers()
    test_header_and_inventory_helpers()
    test_clang_command_and_location_helpers()
    test_instrumentation_and_ast_helpers()
    test_mutation_and_finding_edge_paths()
    test_small_helpers_and_flow()
    test_manifest_modes()
    test_module_fact_edges()
    test_main_error_boundaries()
    test_wrapper_form_contract_edges()
    test_collect_wrapper_forms_edges()
    test_wrapper_form_main_modes()
    with mock.patch.object(sys, "argv", [str(TOOL), "--timeout", "0"]):
        try:
            runpy.run_path(str(TOOL), run_name="__main__")
        except SystemExit as exc:
            assert exc.code == 2
    print("PASS test_wrapper_audit_unit")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
