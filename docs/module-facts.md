# p101 module facts

`audit-wrappers --emit-module-facts` emits a plain tab-separated stream for
tools that need C project facts without owning a C parser.

The canonical parser and record constants live in `lib_c_facts`. Keep this
document in sync with `libraries/lib_c_facts/README.md` when the schema changes.

Each record has this prefix:

```text
P101FACT<TAB>8<TAB>kind<TAB>path<TAB>module<TAB>is_header<TAB>line...
```

The version field is currently `8`, and must match `P101_C_FACT_VERSION` in
`libraries/lib_c_facts/include/p101_c_facts/facts.h`. Fields after `line` depend
on `kind`.
Backslash, tab, newline, and carriage-return characters inside variable fields
are escaped as `\\`, `\t`, `\n`, and `\r`.

## Fact kinds

| Kind | Extra fields | Meaning |
| --- | --- | --- |
| `FILE` | none | A source or header file that was part of the scan. |
| `INCLUDE` | `target`, `is_local` | A preprocessor include, where `is_local` is `1` for quoted includes. |
| `FUNCTION` | `name`, flags, `usr`, extent | A function definition or declaration identified by Clang. |
| `PARAMETER` | `name`, spelled/canonical types, parent function `usr`, index, extent | A parameter of a function declaration identified by type and position. |
| `CALL` | `name`, env/error flags, indirect flag, display caller, callee/caller USRs, extent | A resolved or explicitly indirect call expression. |
| `TYPE` | `name`, `usr` | A typedef or record declaration exposed by a header. |
| `ENUM` | `name`, `usr` | A named enum declaration, including a typedef name for an anonymous enum. |
| `ENUMERATOR` | `name`, enum display name, enumerator/enum USRs | An enumerator and its resolved parent declaration. |
| `MACRO` | `name`, definition flag, caller USR, extent | A macro definition or source expansion. |
| `NOTE` | `name`, display caller, column, caller USR, extent | A typed semantic observation. |

## Ownership rule

`audit-wrappers` owns the C/C++ parsing pass. `lib_c_facts` owns the stable
record contract and parser. Consumers such as `audit-modules` should consume
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

## The NOTE vocabulary is the extension point

New semantic evidence is added as a new `NOTE` name, not as a new fact kind and
not as a version bump. `enum p101_c_note_kind` in `facts.h` is the closed list;
names outside it decode as `P101_C_NOTE_OTHER` and must be interpreted from the
note text. Kinds are appended so an existing numeric value never changes, which
is why consumers keep working across additions and why the content-addressed
caches do not have to re-key.

The idiom notes consumed by `audit-modules` are:

| Note | Symbol field | Rule |
| --- | --- | --- |
| `SIGNATURE_ENV_ORDER` | function name | P101-MOD-022 |
| `ALLOC_SIZEOF_TYPE` | allocation site | P101-MOD-027 |

Four further idiom notes are emitted and deliberately **not** consumed by any
rule. Do not wire them up:

| Note | Why not |
| --- | --- |
| `MACRO_ARGUMENT_BARE` | owned by `bugprone-macro-parentheses` |
| `MACRO_STATEMENT_BARE` | owned by `bugprone-multiple-statement-macro` |
| `HANDLER_REGISTERED` | owned by `bugprone-signal-handler` (alias `cert-sig30-c`) |
| `FIELD_REACH` | no rule survives contact with the corpus — see below |

The first three are clang-tidy's: it already reports the same problems, and a
student should see one diagnostic per problem, not two. They stay in the stream
because the fact layer is general evidence, not a rule feed; another tool may
want them for something clang-tidy does not do.

`FIELD_REACH` is emitted per access whose owning record is declared in a
different admitted file. Deciding whether that access crosses a *module*
boundary is a consumer's job: join the symbol USR against the `TYPE` records to
recover the owning module. P101-MOD-026 did exactly that, exempting records with
no lifecycle, and was still wrong — it found 28 accesses in `lib_tool_event`
that are correct by design. Records here routinely carry both a lifecycle and a
shared representation, so the note is evidence about structure, not a defect
signal. Consume it accordingly.

Older versions are no longer accepted. The call flags and caller identity let
`audit-errors` analyze every p101 function consistently and enforce
caller-sensitive rules without a hard-coded name list.
