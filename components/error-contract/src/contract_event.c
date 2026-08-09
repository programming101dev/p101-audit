#include "contract_event.h"

bool p101_contract_event_needs_env(const struct contract_event *event)
{
    bool needs_env;

    needs_env = event->kind == CONTRACT_EVENT_TRACE_USE;
    if(!needs_env && event->kind == CONTRACT_EVENT_CALL && event->needs_env)
    {
        needs_env = true;
    }
    return needs_env;
}

bool p101_contract_event_needs_error(const struct contract_event *event)
{
    bool needs_error;

    needs_error = event->kind == CONTRACT_EVENT_ERROR_CHECK;
    if(!needs_error && event->kind == CONTRACT_EVENT_CALL && event->needs_error)
    {
        needs_error = true;
    }
    return needs_error;
}

bool p101_contract_event_is_not_after(const struct contract_event *candidate, const struct contract_event *event)
{
    bool ordered;

    if(candidate->start != 0U && event->start != 0U)
    {
        ordered = candidate->start <= event->start;
    }
    else
    {
        ordered = candidate->line <= event->line;
    }
    return ordered;
}

bool p101_contract_event_kind_from_note(enum p101_c_note_kind note, enum contract_event_kind *kind)
{
    bool found;

    found = true;
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(note)
    {
        case P101_C_NOTE_ENV_USE:
            *kind = CONTRACT_EVENT_ENV_USE;
            break;
        case P101_C_NOTE_ERROR_USE:
            *kind = CONTRACT_EVENT_ERROR_USE;
            break;
        case P101_C_NOTE_TRACE_USE:
            *kind = CONTRACT_EVENT_TRACE_USE;
            break;
        case P101_C_NOTE_ERROR_CHECK:
            *kind = CONTRACT_EVENT_ERROR_CHECK;
            break;
        case P101_C_NOTE_ERROR_OPTIONAL:
            *kind = CONTRACT_EVENT_ERROR_OPTIONAL;
            break;
        case P101_C_NOTE_ERROR_DISCARD:
            *kind = CONTRACT_EVENT_ERROR_DISCARD;
            break;
        case P101_C_NOTE_ERROR_PROPAGATED:
            *kind = CONTRACT_EVENT_ERROR_PROPAGATED;
            break;
        case P101_C_NOTE_ERROR_UNCHECKED_CHAIN:
            *kind = CONTRACT_EVENT_ERROR_UNCHECKED_CHAIN;
            break;
        case P101_C_NOTE_FUNCTION_RETURN:
            *kind = CONTRACT_EVENT_FUNCTION_RETURN;
            break;
        case P101_C_NOTE_FUNCTION_EARLY_RETURN:
            *kind = CONTRACT_EVENT_FUNCTION_EARLY_RETURN;
            break;
        case P101_C_NOTE_CALL_NOT_ISOLATED:
            *kind = CONTRACT_EVENT_CALL_NOT_ISOLATED;
            break;
        case P101_C_NOTE_SIGNATURE_ENV_ORDER:
        case P101_C_NOTE_FIELD_REACH:
        case P101_C_NOTE_ALLOC_SIZEOF_TYPE:
        case P101_C_NOTE_MACRO_ARGUMENT_BARE:
        case P101_C_NOTE_MACRO_STATEMENT_BARE:
        case P101_C_NOTE_HANDLER_REGISTERED:
        case P101_C_NOTE_OTHER:
        case P101_C_NOTE_ENV_CONTRACT:
        case P101_C_NOTE_ERROR_CONTRACT:
        case P101_C_NOTE_CALL_RESULT_DISCARDED:
        case P101_C_NOTE_TERMINATION_ADAPTER:
        case P101_C_NOTE_OWNERSHIP_ERROR_ACQUIRE:
        case P101_C_NOTE_OWNERSHIP_ERROR_RELEASE:
        case P101_C_NOTE_OWNERSHIP_ENV_ACQUIRE:
        case P101_C_NOTE_OWNERSHIP_ENV_RELEASE:
        default:
            found = false;
            break;
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    return found;
}
