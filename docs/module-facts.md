# p101 module facts

`p101-wrapper-audit --emit-module-facts` emits a plain tab-separated stream for
tools that need C project facts without owning a C parser.

The canonical parser and record constants live in `lib_c_facts`. Keep this
document in sync with `libraries/lib_c_facts/README.md` when the schema changes.

Each record has this prefix:

```text
P101FACT<TAB>5<TAB>kind<TAB>path<TAB>module<TAB>is_header<TAB>line...
```

The version field is currently `5`. Fields after `line` depend on `kind`.
Backslash, tab, newline, and carriage-return characters inside variable fields
are escaped as `\\`, `\t`, `\n`, and `\r`.

## Fact kinds

| Kind | Extra fields | Meaning |
| --- | --- | --- |
| `FILE` | none | A source or header file that was part of the scan. |
| `INCLUDE` | `target`, `is_local` | A preprocessor include, where `is_local` is `1` for quoted includes. |
| `FUNCTION` | `name`, flags, `usr`, extent | A function definition or declaration identified by Clang. |
| `CALL` | `name`, env/error flags, indirect flag, display caller, callee/caller USRs, extent | A resolved or explicitly indirect call expression. |
| `TYPE` | `name`, `usr` | A typedef or record declaration exposed by a header. |
| `ENUM` | `name`, `usr` | A named enum declaration, including a typedef name for an anonymous enum. |
| `ENUMERATOR` | `name`, enum display name, enumerator/enum USRs | An enumerator and its resolved parent declaration. |
| `MACRO` | `name`, definition flag, caller USR, extent | A macro definition or source expansion. |
| `NOTE` | `name`, display caller, column, caller USR, extent | A typed semantic observation. |

## Ownership rule

`p101-wrapper-audit` owns the C/C++ parsing pass. `lib_c_facts` owns the stable
record contract and parser. Consumers such as `p101-module-map` should consume
this fact stream through `lib_c_facts` instead of scanning C syntax themselves.
If another tool needs more C facts, extend this format at the producer and keep
the consumer simple.

All source facts come from libclang cursors. Macro spelling is retained because
macros have no runtime function identity, but the fact also records that it is
a macro cursor, its expansion extent, and its enclosing function identity.

`ENV_CONTRACT` and `ERROR_CONTRACT` are emitted on a function declaration line
when Clang resolves a parameter to the canonical `p101_env` or `p101_error`
record identity. `ENV_USE` and `ERROR_USE` mark declarations of those types.
`TYPE_SEMANTIC_ROLE:p101:trace-scope`, `ERROR_CHECK`, and `ERROR_OPTIONAL`
mark typed or helper uses that may not appear as ordinary calls in the AST.
`FUNCTION_RETURN` and
`FUNCTION_EARLY_RETURN` preserve statement-structure evidence for the
single-exit rule. `SEMANTIC_ROLE:<role>` carries an explicit Clang annotation
on a function declaration; `CALLEE_SEMANTIC_ROLE:<role>` carries that
annotation at a call site after libclang resolves the referenced declaration.
Indirect calls carry the function-pointer type USR when libclang can resolve
one, and `CALL_RESULT_DISCARDED` records a call whose result is discarded.

Older versions are no longer accepted. The call flags and caller identity let
`p101-error-contract` analyze every p101 function consistently and enforce
caller-sensitive rules without a hard-coded name list.
