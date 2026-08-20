# audit-errors

`audit-errors` checks explicit, inspectable function-level control and
error contracts. When a function uses p101 wrappers, tracing, or p101 error
macros, it must make its `env` and `err` contracts visible. Every function-call
result must also be isolated before another expression consumes it.

The normal path consumes native libclang records directly from `lib_c_facts`,
including exact function extents and caller identity, so it does not own a
private C parser or reconstruct function boundaries from line order. A
`P101FACT` snapshot remains available as an explicit offline input.

## Usage

```sh
audit-errors [-h] [-d:FORMAT] [-q] [-v] [-i <facts.tsv> | -C <compile_commands.json>] [path ...]
```

Examples:

```sh
audit-errors
audit-errors src include
audit-errors -C build-clang/compile_commands.json src
audit-errors -i source-facts.tsv src include
audit-errors -d:json src > error-contract.json
```

If no path is supplied, `src` is scanned.

## Findings

| ID | Meaning |
| --- | --- |
| `P101-ERR-001` | A p101 wrapper call or `P101_TRACE` appears before a visible `p101_env` / `env` contract in the current function. |
| `P101-ERR-002` | A fallible p101 wrapper call or p101 error macro appears before a visible `p101_error` / `err` contract in the current function. |
| `P101-ERR-003` | A fallible p101 wrapper passes `NULL` in the standard error-object position without documenting an intentional best-effort boundary. |
| `P101-ERR-004` | With `-S`, a second fallible p101 call is reachable on the same Clang statement path before the prior error state is checked or returned. |
| `P101-ERR-005` | A source file creates more `p101_error` objects than it destroys. |
| `P101-ERR-006` | A source file creates more `p101_env` objects than it destroys. |
| `P101-ERR-007` | A function other than `main` terminates the process instead of returning a status or raising through `p101_error`. |
| `P101-ERR-008` | A return is not the function's final top-level statement, or the function contains more than one explicit exit point. |
| `P101-ERR-009` | A call is embedded in an argument, condition, return, cast, arithmetic operation, or another larger expression instead of being isolated in its own statement or named variable. |
| `P101-MEM-001` | A statically provable zero-size allocation is requested. Runtime observation also reports a zero-size allocation that was not constant in source. |
| `P101-MEM-002` | A restricted copy has provably identical source and destination objects. |
| `P101-THREAD-001` | A thread-creation argument refers to automatic storage that may expire before the thread finishes. |
| `P101-SIGNAL-001` | Code reachable from a registered signal handler accesses mutable shared storage outside the portable signal-safe forms. |
| `P101-SIGNAL-002` | Code reachable from a registered signal handler calls an external operation outside the portable async-signal-safe set. |
| `P101-ENV-001` | A borrowed environment, locale, or static-library result is used after a semantic invalidation operation. |
| `P101-FILE-001` | The same pathname object is checked and later used by a separate operation, creating a TOCTOU window. |
| `P101-MOD-028` | Direct recursive control flow has no explicit bounded-depth semantic contract. |

Process termination is an application-boundary decision. Helpers, libraries,
and CLI parsers must return a status or raise an error so `main` retains
control of cleanup, reporting, and the final exit status. The sole mechanism
exception is `p101_tool_run_child_main`: it is the entry point of a freshly
forked child and must call `_exit` if `exec` fails, because returning would
resume the parent's workflow in the child.

Each function also converges on one exit point. Returning functions use one
final `return`; a `void` function may use its closing brace as the single
implicit return. `main` follows the same structure and makes one final process
status decision for the shell. This keeps cleanup, error reporting, and state
commit decisions in one visible place.

Function calls are isolated for the same reason. A call whose result is
intentionally ignored is a complete statement:

```c
notify();
```

When another operation consumes a call's result, the call must be the entire
initializer or right-hand side of a simple assignment:

```c
ready  = is_ready();
result = transform(input);
```

The named value is then used by conditions, arguments, returns, casts, and
larger expressions. Calls such as `if(is_ready())`, `transform(read_value())`,
`return transform(input)`, and `(void)transform(input)` are findings. Calls
generated inside a macro definition are excluded because the invocation site
cannot materialize an implementation detail it did not spell.

This rule primarily improves debugger stops, watch expressions, logging, and
source-level inspection. A conforming compiler commonly optimizes the temporary
away, so the contract does not claim an automatic runtime performance
improvement. It can avoid real work only when the refactor also prevents
duplicate evaluation.

`P101-ERR-004` is intentionally strict. Enable it with `-S` when code must
preserve the first failure and stop before any later fallible side effect. The
default contract accepts the common p101 boundary style where an error object
remains set across a short sequence and is checked or returned at the boundary.

For a deliberately fallible boolean probe where failure *is* the result rather
than an error to report, place
`/* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: reason */` immediately before the
probe. The exception applies only to a call on that line or the immediately
following line, and also admits an intentional `NULL` error argument. It is
visible to reviewers; do not use it to silence ordinary error propagation.

The checker accepts either a signature-level contract discovered from Clang AST
facts:

```c
static int load_file(const struct p101_env *env, struct p101_error *err, const char *path);
```

or a local contract that is created before the first relevant p101 call:

```c
struct p101_error *err = p101_error_create(false);
struct p101_env   *env = p101_env_create(err, NULL);
```

## Admitted inputs

The user gives source/header paths. With `-i`, the tool consumes that exact
P101FACT v8 snapshot and does not invoke Clang again. Otherwise, it invokes
`lib_c_facts` directly over the admitted translation units. If the current
project has a Clang build named by
`.last-build-dir`, or a `build-clang/compile_commands.json`, that database is
used automatically so sibling p101 include directories and project defines
are preserved. Use `-C` to select another compile database explicitly.

## Outputs

Text output is line-oriented:

```text
path/to/file.c:42:0: error: fallible p101 call or error macro appears before a visible error contract [P101-ERR-002] (function function_name)
path/to/file.c:42:0: note: learn more: P101-LESSON-ERROR-CONTRACTS (https://github.com/programming101dev/playgrounds/blob/main/lessons/error-contracts.md) [P101-ERR-002]
```

With `-d:json`, the tool emits JSON. With `-d:human,json`, JSON goes to stdout
and human diagnostics go to stderr:

```json
{"schema":"p101-tool-report-v1","tool":"audit-errors","admitted_inputs":"Clang AST facts for the selected C translation units.","does_not_prove":"Unscanned code, third-party internals, and runtime-only behavior are outside this static contract report.","findings":[],"summary":{"findings":0,"files_scanned":0},"outcome":"clean","exit_status":0}
```

Each finding uses the common `id`, `severity`, `location`, `message`, and
`lesson` envelope. The outer report contract is shared with the other finding
tools, including its outcome and exit status.

## Blind spots

This checker is only as complete as the admitted translation units. If
libclang cannot parse one, or the wrong include flags or compile database are
used, analysis fails as tool trouble rather than silently reporting a partial
clean result. The contract judgment is still a teaching heuristic, not a proof
of all possible C control flow.

The flow check follows calls with the standard fallible wrapper contract
(`env, err, ...`), recognizes explicit error guards and returns, and keeps
branch-local state for conditionals, switches, and loops. A function return
propagates its existing `err` state; the tool does not guess whether a numeric
return value means success or failure. libclang's stable C API does not expose
Clang's complete compiler CFG, so goto edges, switch fallthrough, exceptional
C++ flow, and precise loop fixed points remain explicit blind spots.

The single-exit check uses AST statement structure: a returning function's sole
`return` must be the final top-level statement, while a `void` function may use
its closing brace as its sole exit. It also counts explicit terminating calls.
Clang's stable C API does not expose a complete CFG, so computed jumps,
exceptional C++ flow, and nonlocal control transfer remain blind spots.

The call-isolation check uses the call's immediate AST parent. Standalone calls,
direct variable initializers, and direct simple assignments are admitted; every
other parent shape is rejected. The stable libclang C API does not expose a
binary-operator opcode, so identifying the simple `=` assignment is the narrow
lexical exception: `lib_c_facts` inspects that parent expression's tokens for
the exact `=` operator. It does not use function or variable names to classify
the call.

Direct libc calls are outside this tool's job; use `audit-wrappers` for
that. Third-party code is only checked if you ask this tool to scan it, and it
may not follow p101 conventions.

The ownership checks are deliberately file-scoped balance checks. They cannot
prove object identity or infer an undocumented ownership transfer through a
return value or output parameter. An intentional transfer should therefore be
made visible in the API and documented when reviewing the finding.

The `needs_env` and `needs_error` decisions come from the resolved callee
signature recorded by libclang. The discard check relies on the p101 API
convention that fallible wrappers take `env, err` as their first two arguments.
C acquisition and parsing belong to `lib_c_facts`; this tool owns only the
error-contract policy.

The security checks use resolved declaration identities, storage duration,
canonical types, call edges, and semantic roles. They do not classify variables
or user-defined functions by spelling. The fixed function identities in the
[POSIX.1-2024 async-signal-safe table](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html)
are the standard API boundary, not a naming heuristic. A recursive function may carry the reviewed
`p101:recursion:bounded` role. An atomic object may carry
`p101:signal:lock-free-atomic` only when the target contract proves that its
operations are lock-free.

Signal reachability starts at handlers directly visible to a scanned
registration call and follows calls to functions defined in the admitted
translation units. It also recognizes the ordinary `sigaction` pattern when a
function is assigned to a function-pointer member and that same resolved object
is passed to the registration call. Function pointers selected dynamically,
handler addresses hidden behind more general data flow, unscanned callees, and
platform extensions to the portable async-signal-safe set remain blind spots. The rule deliberately
uses the portable baseline: a platform-specific function being safe does not
make it acceptable on every supported platform. Preservation of `errno` across
a handler is not yet proven by this analysis.

The lifetime and TOCTOU rules are intentionally narrow. They diagnose direct
automatic-storage escape, direct borrowed-result invalidation, identical
restricted-copy objects, constant zero sizes, and a check/use pair over the
same resolved pathname object. Aliasing through aggregates, pointer arithmetic,
interprocedural ownership transfer, runtime range overlap, and filesystem
changes by other actors require runtime evidence or review.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | No findings |
| `1` | Findings were reported |
| `2` | Usage or tool trouble |

## Build and check

Configure a compiler once, then run the gate:

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DP101_BUILD_LEVEL=1
cmake -S . -B build -DP101_BUILD_LEVEL=3 && cmake --build build --no-fuzz
```

Useful receipts while developing:

```sh
cmake --build build-clang
cmake -S . -B build -DP101_BUILD_LEVEL=2 && cmake --build build
./build-clang/audit-errors src
```
