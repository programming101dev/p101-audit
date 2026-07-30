# Wrapper-form contract

`p101-wrapper-audit --wrapper-form-only` checks the structural form of wrapper
functions in any C or C++ project that Clang can parse. The project supplies
the policy; `p101_` is not required.

## Contract

```json
{
  "schema": "p101-wrapper-form-contract-v1",
  "selector": {
    "include": "^wrap_",
    "exclude": ["_internal$"],
    "public_only": true,
    "minimum_matches": 1
  },
  "mapping": {
    "strip_prefix": "wrap_"
  },
  "context_parameter": {
    "index": 0,
    "type_contains": "app_context",
    "mode": "required"
  },
  "error_parameter": {
    "index": 1,
    "type_contains": "app_error",
    "mode": "optional"
  },
  "requirements": {
    "balanced_trace": true,
    "fault": "when-error",
    "target_required": true,
    "target_call_count": 1,
    "compare_target_signature": true
  },
  "instrumentation_calls": {
    "trace_entry": ["app_trace_enter"],
    "trace_exit": ["app_trace_exit"],
    "fault": ["app_fault_point"],
    "fd": ["app_track_fd"],
    "allocation": ["app_track_allocation"],
    "resource": ["app_track_resource"]
  },
  "capabilities": {
    "wrap_open": ["fd"]
  },
  "overrides": [
    {
      "match": "^wrap_open$",
      "target": "native_open",
      "compare_target_signature": false
    }
  ]
}
```

`selector.include` and each exclusion are regular expressions. Public-only is
the default. A selector that matches fewer than `minimum_matches` functions is
tool trouble, so a stale or misplaced contract cannot silently pass.

`mapping.strip_prefix` maps `wrap_open` to `open`. An override may set an exact
`target` or use `target_replacement` as a regular-expression replacement.
Every override must match at least one selected function; stale overrides are
tool trouble. A human-readable `reason` is accepted and recommended for every
intentional signature or target deviation.

Context and error parameter specifications are optional. When present, they
check both type text and position. An optional error parameter is permitted to
be absent, but must occupy the declared position when present.

`fault` is `optional`, `required`, `when-error`, or `forbidden`.
`target_call_count` is the number of direct syntactic calls expected in the
definition. Branching variadic adapters can override it with their actual
number of native call sites. Signature comparison removes injected
context/error parameters and compares canonical Clang parameter types, return
type, and variadic status.

Capabilities are explicit per-function lifecycle obligations. Supported
capabilities are `fd`, `allocation`, and `resource`. A capability contract for
a function not selected by the contract is stale tool configuration.

If `instrumentation_calls` is omitted, p101 tracing, fault, descriptor,
allocation, and generic-resource calls are recognized. A non-p101 project
lists its own call names. Calls through a same-file helper are propagated, so a
small shared tracing helper remains visible.

## Findings

| ID | Meaning |
| --- | --- |
| `P101-WFORM-002` | context parameter missing or misplaced |
| `P101-WFORM-003` | error parameter missing or misplaced |
| `P101-WFORM-004` | entry/exit tracing is not balanced |
| `P101-WFORM-005` | fault-injection form violates policy |
| `P101-WFORM-006` | mapped target call is missing or has the wrong count |
| `P101-WFORM-007` | wrapper and mapped target signatures differ |
| `P101-WFORM-008` | required lifecycle instrumentation is missing |
| `P101-WFORM-900` | invalid or stale contract |

Exit `0` means the admitted definitions satisfy the structural contract, exit
`1` means findings, and exit `2` means parser, input, or contract trouble.
Text and JSON output include the contract SHA-256 so a saved result identifies
the policy that produced it.

## Evidence and limits

The check admits active translation units from the selected compile database
and reports source paths and lines from Clang's AST. It sees macro expansion,
compiler-lowered atomics, fortified libc calls, and same-file helper calls.

It proves neither semantic equivalence nor runtime cleanup. Presence of a
lifecycle hook does not prove it executes on every control-flow path. A
conditional native call may be structurally present but guarded by the wrong
condition. Third-party code and inactive platform translation units remain
outside the receipt. Pair this gate with unit tests, sanitizers, mutation
testing, fault walking, and resource tracking.
