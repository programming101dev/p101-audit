#ifndef P101_ERROR_CONTRACT_TYPES_H
#define P101_ERROR_CONTRACT_TYPES_H

#include "constants.h"
#include <stdbool.h>
#include <stddef.h>

enum contract_event_kind
{
    CONTRACT_EVENT_CALL = 0,
    CONTRACT_EVENT_ENV_USE,
    CONTRACT_EVENT_ERROR_USE,
    CONTRACT_EVENT_TRACE_USE,
    CONTRACT_EVENT_ERROR_CHECK,
    CONTRACT_EVENT_ERROR_OPTIONAL,
    CONTRACT_EVENT_ERROR_DISCARD,
    CONTRACT_EVENT_ERROR_PROPAGATED,
    CONTRACT_EVENT_ERROR_UNCHECKED_CHAIN,
    CONTRACT_EVENT_ERROR_OUTPUT_UNCHECKED,
    CONTRACT_EVENT_PROCESS_TERMINATION,
    CONTRACT_EVENT_FUNCTION_RETURN,
    CONTRACT_EVENT_FUNCTION_EARLY_RETURN,
    CONTRACT_EVENT_CALL_NOT_ISOLATED,
    CONTRACT_EVENT_MUST_CHECK_RESULT_DISCARDED,
    CONTRACT_EVENT_ERROR_CLEANUP_SHADOW,
    CONTRACT_EVENT_PARTIAL_RESULT_DISCARDED,
    CONTRACT_EVENT_UNCERTAIN_PROGRESS_RETRIED,
    CONTRACT_EVENT_CONDITION_WAIT_OUTSIDE_LOOP,
    CONTRACT_EVENT_POST_FORK_UNSAFE_CALL,
    CONTRACT_EVENT_ZERO_SIZE_ALLOCATION,
    CONTRACT_EVENT_OVERLAPPING_RESTRICTED_COPY,
    CONTRACT_EVENT_THREAD_AUTOMATIC_STORAGE_ESCAPE,
    CONTRACT_EVENT_ENV_BORROWED_POINTER_INVALIDATED,
    CONTRACT_EVENT_PATH_TOCTOU,
    CONTRACT_EVENT_SIGNAL_HANDLER_REGISTERED,
    CONTRACT_EVENT_SIGNAL_SHARED_OBJECT_ACCESS,
    CONTRACT_EVENT_RECURSIVE_CALL
};

enum contract_ownership_kind
{
    CONTRACT_OWNERSHIP_ERROR_ACQUIRE = 0,
    CONTRACT_OWNERSHIP_ERROR_RELEASE,
    CONTRACT_OWNERSHIP_ENV_ACQUIRE,
    CONTRACT_OWNERSHIP_ENV_RELEASE
};

struct contract_function
{
    char   path[CONTRACT_PATH_LEN];
    char   name[FUNCTION_NAME_LEN];
    char   usr[FUNCTION_NAME_LEN];
    size_t line;
    size_t start;
    size_t end;
    bool   has_env_contract;
    bool   has_error_contract;
    bool   is_termination_adapter;
    bool   env_reported;
    bool   error_reported;
};

struct contract_event
{
    enum contract_event_kind kind;
    char                     path[CONTRACT_PATH_LEN];
    char                     name[FUNCTION_NAME_LEN];
    char                     caller[FUNCTION_NAME_LEN];
    char                     usr[FUNCTION_NAME_LEN];
    char                     caller_usr[FUNCTION_NAME_LEN];
    size_t                   line;
    size_t                   start;
    size_t                   end;
    bool                     needs_env;
    bool                     needs_error;
};

struct contract_ownership_file
{
    char   path[CONTRACT_PATH_LEN];
    size_t error_create_count;
    size_t error_destroy_count;
    size_t env_create_count;
    size_t env_destroy_count;
    size_t first_error_create_line;
    size_t first_env_create_line;
};

struct contract_model
{
    struct contract_function       functions[MAX_FACT_FUNCTIONS];
    struct contract_event          events[MAX_FACT_EVENTS];
    size_t                         function_count;
    size_t                         event_count;
    size_t                         files_scanned;
    struct contract_ownership_file ownership_files[MAX_FACT_FUNCTIONS];
    size_t                         ownership_file_count;
};

#endif
