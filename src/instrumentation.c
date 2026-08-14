#include "instrumentation.h"
#include <errno.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <stdint.h>

struct instrumentation_slot
{
    const char *usr;
    size_t      fact_index_plus_one;
};

enum
{
    INSTRUMENTATION_INITIAL_CAPACITY = 16,
    INSTRUMENTATION_HASH_SEED        = 5381,
    INSTRUMENTATION_HASH_MULTIPLIER  = 33,
    INSTRUMENTATION_HASH_REDUCTION   = 104729
};

static size_t instrumentation_hash(const char *text);
static bool   instrumentation_insert(const struct p101_env *env, struct instrumentation_slot *slots, size_t capacity, const char *usr, size_t fact_index);
static size_t instrumentation_lookup(const struct p101_env *env, const struct instrumentation_slot *slots, size_t capacity, const char *usr, size_t missing);
static size_t instrumentation_fallback_function(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *fact);
static bool   instrumentation_same_repository(const struct p101_env *env, const char *left, const char *right);
static void   instrumentation_add_role(const struct p101_env *env, struct p101_instrumentation_capabilities *capabilities, const char *role);
static bool   instrumentation_merge(struct p101_instrumentation_capabilities *destination, const struct p101_instrumentation_capabilities *source);

bool p101_instrumentation_collect(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, struct p101_instrumentation_capabilities *capabilities)
{
    struct instrumentation_slot    *slots;
    const struct p101_wrapper_fact *fact;
    enum p101_c_note_kind           note_kind;
    size_t                          function_count;
    size_t                          capacity;
    size_t                          caller;
    size_t                          callee;
    bool                            inserted;
    bool                            changed;
    bool                            collected;
    void                           *allocation;

    P101_TRACE_SCOPE(env);
    slots          = NULL;
    function_count = 0U;
    capacity       = INSTRUMENTATION_INITIAL_CAPACITY;
    collected      = false;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        fact = &model->facts[index];
        if(fact->kind == P101_C_ANALYSIS_FUNCTION && fact->is_definition && fact->usr[0] != '\0')
        {
            function_count++;
        }
    }
    while(capacity < function_count * 2U)
    {
        if(capacity > SIZE_MAX / 2U)
        {
            P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
            goto done;
        }
        capacity *= 2U;
    }
    allocation = p101_calloc(env, err, capacity, sizeof(*slots));
    slots      = (struct instrumentation_slot *)allocation;
    if(slots == NULL)
    {
        goto done;
    }
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_FUNCTION || !fact->is_definition || fact->usr[0] == '\0')
        {
            continue;
        }
        inserted = instrumentation_insert(env, slots, capacity, fact->usr, index);
        if(!inserted)
        {
            P101_ERROR_RAISE_USER(err, "semantic function map is full", EOVERFLOW);
            goto done;
        }
    }
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_NOTE)
        {
            continue;
        }
        caller = instrumentation_lookup(env, slots, capacity, fact->caller_usr, model->fact_count);
        if(caller == model->fact_count)
        {
            caller = instrumentation_fallback_function(env, model, fact);
        }
        if(caller == model->fact_count)
        {
            continue;
        }
        note_kind = p101_c_note_kind_from_name(env, fact->name);
        if(note_kind == P101_C_NOTE_TRACE_USE)
        {
            capabilities[caller].trace_entry = true;
            capabilities[caller].trace_exit  = true;
        }
        instrumentation_add_role(env, &capabilities[caller], fact->name);
    }
    do
    {
        changed = false;
        for(size_t index = 0U; index < model->fact_count; index++)
        {
            fact = &model->facts[index];
            if(fact->kind != P101_C_ANALYSIS_CALL)
            {
                continue;
            }
            caller = instrumentation_lookup(env, slots, capacity, fact->caller_usr, model->fact_count);
            callee = instrumentation_lookup(env, slots, capacity, fact->usr, model->fact_count);
            if(caller == model->fact_count || callee == model->fact_count)
            {
                continue;
            }
            inserted = instrumentation_same_repository(env, model->facts[caller].path, model->facts[callee].path);
            if(!inserted)
            {
                continue;
            }
            inserted = instrumentation_merge(&capabilities[caller], &capabilities[callee]);
            changed  = ((changed || inserted) != 0);
        }
    } while(changed);
    collected = true;

done:
    p101_free(env, slots);
    return collected;
}

static size_t instrumentation_hash(const char *text)
{
    size_t        hash;
    unsigned char byte;

    hash = INSTRUMENTATION_HASH_SEED;
    while(*text != '\0')
    {
        byte = (unsigned char)*text;
        if(hash > (SIZE_MAX - byte) / INSTRUMENTATION_HASH_MULTIPLIER)
        {
            hash %= INSTRUMENTATION_HASH_REDUCTION;
        }
        hash = (hash * INSTRUMENTATION_HASH_MULTIPLIER) + byte;
        text++;
    }
    return hash;
}

static bool instrumentation_insert(const struct p101_env *env, struct instrumentation_slot *slots, size_t capacity, const char *usr, size_t fact_index)
{
    size_t slot;
    size_t hash;
    bool   inserted;
    int    comparison;

    hash     = instrumentation_hash(usr);
    slot     = hash & (capacity - 1U);
    inserted = false;
    for(size_t probe = 0U; probe < capacity; probe++)
    {
        if(slots[slot].usr == NULL)
        {
            slots[slot].usr                 = usr;
            slots[slot].fact_index_plus_one = fact_index + 1U;
            inserted                        = true;
            break;
        }
        comparison = p101_strcmp(env, slots[slot].usr, usr);
        if(comparison == 0)
        {
            inserted = true;
            break;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }
    return inserted;
}

static size_t instrumentation_lookup(const struct p101_env *env, const struct instrumentation_slot *slots, size_t capacity, const char *usr, size_t missing)
{
    size_t slot;
    size_t hash;
    size_t found;
    int    comparison;

    found = missing;
    if(usr[0] == '\0')
    {
        goto done;
    }
    hash = instrumentation_hash(usr);
    slot = hash & (capacity - 1U);
    for(size_t probe = 0U; probe < capacity; probe++)
    {
        if(slots[slot].usr == NULL)
        {
            break;
        }
        comparison = p101_strcmp(env, slots[slot].usr, usr);
        if(comparison == 0)
        {
            found = slots[slot].fact_index_plus_one - 1U;
            break;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }

done:
    return found;
}

static size_t instrumentation_fallback_function(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *fact)
{
    const struct p101_wrapper_fact *candidate;
    size_t                          found;
    int                             comparison;

    found = model->fact_count;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        candidate = &model->facts[index];
        if(candidate->kind != P101_C_ANALYSIS_FUNCTION || !candidate->is_definition)
        {
            continue;
        }
        comparison = p101_strcmp(env, candidate->path, fact->path);
        if(comparison == 0 && fact->start >= candidate->start && fact->start < candidate->end)
        {
            found = index;
            break;
        }
    }
    return found;
}

static bool instrumentation_same_repository(const struct p101_env *env, const char *left, const char *right)
{
    static const char *const roots[] = {"/libraries/", "/programs/", "/templates/"};
    const char              *left_root;
    const char              *right_root;
    const char              *left_end;
    const char              *right_end;
    size_t                   left_length;
    size_t                   right_length;
    size_t                   root_length;
    bool                     same;

    same       = false;
    left_root  = NULL;
    right_root = NULL;
    for(size_t index = 0U; index < sizeof(roots) / sizeof(roots[0]); index++)
    {
        left_root  = p101_strstr(env, left, roots[index]);
        right_root = p101_strstr(env, right, roots[index]);
        if(left_root != NULL && right_root != NULL)
        {
            root_length = p101_strlen(env, roots[index]);
            left_root += root_length;
            right_root += root_length;
            break;
        }
        left_root  = NULL;
        right_root = NULL;
    }
    if(left_root == NULL || right_root == NULL)
    {
        goto done;
    }
    left_end  = p101_strchr(env, left_root, '/');
    right_end = p101_strchr(env, right_root, '/');
    if(left_end == NULL)
    {
        left_length = p101_strlen(env, left_root);
    }
    else
    {
        left_length = (size_t)(left_end - left_root);
    }
    if(right_end == NULL)
    {
        right_length = p101_strlen(env, right_root);
    }
    else
    {
        right_length = (size_t)(right_end - right_root);
    }
    if(left_length == right_length)
    {
        int comparison;

        comparison = p101_strncmp(env, left_root, right_root, left_length);
        same       = comparison == 0;
    }

done:
    return same;
}

static void instrumentation_add_role(const struct p101_env *env, struct p101_instrumentation_capabilities *capabilities, const char *role)
{
    static const char *const names[] = {
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:trace-entry",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:trace-exit",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:fault",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:fd",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:allocation",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:resource",
    };
    bool *const flags[] = {
        &capabilities->trace_entry,
        &capabilities->trace_exit,
        &capabilities->fault,
        &capabilities->fd,
        &capabilities->allocation,
        &capabilities->resource,
    };
    int comparison;

    for(size_t index = 0U; index < sizeof(names) / sizeof(names[0]); index++)
    {
        comparison = p101_strcmp(env, role, names[index]);
        if(comparison == 0)
        {
            *flags[index] = true;
            break;
        }
    }
}

static bool instrumentation_merge(struct p101_instrumentation_capabilities *destination, const struct p101_instrumentation_capabilities *source)
{
    bool changed;

    changed = false;
    if(!destination->trace_entry && source->trace_entry)
    {
        destination->trace_entry = true;
        changed                  = true;
    }
    if(!destination->trace_exit && source->trace_exit)
    {
        destination->trace_exit = true;
        changed                 = true;
    }
    if(!destination->fault && source->fault)
    {
        destination->fault = true;
        changed            = true;
    }
    if(!destination->fd && source->fd)
    {
        destination->fd = true;
        changed         = true;
    }
    if(!destination->allocation && source->allocation)
    {
        destination->allocation = true;
        changed                 = true;
    }
    if(!destination->resource && source->resource)
    {
        destination->resource = true;
        changed               = true;
    }
    return changed;
}
