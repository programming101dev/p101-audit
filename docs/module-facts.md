# p101 module facts

`p101-wrapper-audit --emit-module-facts` emits a plain tab-separated stream for
tools that need C project facts without owning a C parser.

The canonical parser and record constants live in `lib_c_facts`. Keep this
document in sync with `libraries/lib_c_facts/README.md` when the schema changes.

Each record has this prefix:

```text
P101FACT<TAB>1<TAB>kind<TAB>path<TAB>module<TAB>is_header<TAB>line...
```

The version field is currently `1`. Fields after `line` depend on `kind`.
Backslash, tab, newline, and carriage-return characters inside variable fields
are escaped as `\\`, `\t`, `\n`, and `\r`.

## Fact kinds

| Kind | Extra fields | Meaning |
| --- | --- | --- |
| `FILE` | none | A source or header file that was part of the scan. |
| `INCLUDE` | `target`, `is_local` | A preprocessor include, where `is_local` is `1` for quoted includes. |
| `FUNCTION` | `name`, `is_static`, `is_header_declaration` | A function definition or a function declared in a header. |
| `CALL` | `name` | A call expression found by Clang's AST. |
| `TYPE` | `name` | A typedef or record declaration exposed by a header. |
| `MACRO` | `name` | A macro definition, excluding ordinary include guards. |
| `NOTE` | `name` | A teaching note such as `ERROR_USE` or `ERROR_CHECK`. |

## Ownership rule

`p101-wrapper-audit` owns the C/C++ parsing pass. `lib_c_facts` owns the stable
record contract and parser. Consumers such as `p101-module-map` should consume
this fact stream through `lib_c_facts` instead of scanning C syntax themselves.
If another tool needs more C facts, extend this format at the producer and keep
the consumer simple.

The only line-oriented extraction in the producer is for preprocessor facts such
as includes and macros, because the JSON AST dump does not expose those in a
portable, useful shape for this workflow. Function, call, type, and declaration
facts come from Clang.
