#include "model.h"
#include <dirent.h>
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/filesystem.h>
#include <sys/stat.h>

enum
{
    INITIAL_CAPACITY       = 256,
    MANIFEST_LINE_SIZE     = 1024,
    WORKSPACE_PARENT_LIMIT = 8
};

static bool add_inventory_mapping(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *original, const char *wrapper);

static bool inventory_has_original(const struct p101_env *env, const struct p101_wrapper_model *model, const char *name)
{
    for(size_t index = 0U; index < model->inventory_count; index++)
    {
        if(p101_strcmp(env, model->inventory[index].original, name) == 0)
        {
            return true;
        }
    }
    return false;
}

static void copy_field(const struct p101_env *env, char *destination, size_t size, const char *source)
{
    P101_TRACE_SCOPE(env);
    destination[0] = '\0';
    if(source != NULL)
    {
        size_t length;

        length = p101_strlen(env, source);
        if(length >= size)
        {
            length = size - 1U;
        }
        p101_memcpy(env, destination, source, length);
        destination[length] = '\0';
    }
}

static bool grow_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model)
{
    size_t                         capacity;
    struct p101_wrapper_inventory *inventory;

    P101_TRACE_SCOPE(env);
    capacity  = model->inventory_capacity == 0U ? INITIAL_CAPACITY : model->inventory_capacity * 2U;
    inventory = (struct p101_wrapper_inventory *)p101_realloc(env, err, model->inventory, capacity * sizeof(*inventory));
    if(inventory == NULL)
    {
        return false;
    }
    model->inventory          = inventory;
    model->inventory_capacity = capacity;
    return true;
}

static bool add_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *wrapper)
{
    const char *original;

    P101_TRACE_SCOPE(env);
    if(wrapper == NULL || p101_strncmp(env, wrapper, "p101_", sizeof("p101_") - 1U) != 0)
    {
        return true;
    }
    original = wrapper + sizeof("p101_") - 1U;
    return add_inventory_mapping(env, err, model, original, wrapper);
}

static bool add_inventory_mapping(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *original, const char *wrapper)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->inventory_count; index++)
    {
        if(p101_strcmp(env, model->inventory[index].original, original) == 0)
        {
            return true;
        }
    }
    if(model->inventory_count == model->inventory_capacity && !grow_inventory(env, err, model))
    {
        return false;
    }
    copy_field(env, model->inventory[model->inventory_count].original, sizeof(model->inventory[model->inventory_count].original), original);
    copy_field(env, model->inventory[model->inventory_count].wrapper, sizeof(model->inventory[model->inventory_count].wrapper), wrapper);
    model->inventory_count++;
    return true;
}

static bool add_atomic_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model)
{
    static const struct
    {
        const char *original;
        const char *wrapper;
    } mappings[] = {
        {"atomic_compare_exchange_strong",          "p101_atomic_uint_compare_exchange_strong"         },
        {"atomic_compare_exchange_strong_explicit", "p101_atomic_uint_compare_exchange_strong_explicit"},
        {"atomic_compare_exchange_weak",            "p101_atomic_uint_compare_exchange_weak"           },
        {"atomic_compare_exchange_weak_explicit",   "p101_atomic_uint_compare_exchange_weak_explicit"  },
        {"atomic_exchange",                         "p101_atomic_uint_exchange"                        },
        {"atomic_exchange_explicit",                "p101_atomic_uint_exchange_explicit"               },
        {"atomic_fetch_add",                        "p101_atomic_uint_fetch_add"                       },
        {"atomic_fetch_add_explicit",               "p101_atomic_uint_fetch_add_explicit"              },
        {"atomic_fetch_and",                        "p101_atomic_uint_fetch_and"                       },
        {"atomic_fetch_and_explicit",               "p101_atomic_uint_fetch_and_explicit"              },
        {"atomic_fetch_or",                         "p101_atomic_uint_fetch_or"                        },
        {"atomic_fetch_or_explicit",                "p101_atomic_uint_fetch_or_explicit"               },
        {"atomic_fetch_sub",                        "p101_atomic_uint_fetch_sub"                       },
        {"atomic_fetch_sub_explicit",               "p101_atomic_uint_fetch_sub_explicit"              },
        {"atomic_fetch_xor",                        "p101_atomic_uint_fetch_xor"                       },
        {"atomic_fetch_xor_explicit",               "p101_atomic_uint_fetch_xor_explicit"              },
        {"atomic_load",                             "p101_atomic_uint_load"                            },
        {"atomic_load_explicit",                    "p101_atomic_uint_load_explicit"                   },
        {"atomic_store",                            "p101_atomic_uint_store"                           },
        {"atomic_store_explicit",                   "p101_atomic_uint_store_explicit"                  },
    };

    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); index++)
    {
        if(!inventory_has_original(env, model, mappings[index].original) && !add_inventory_mapping(env, err, model, mappings[index].original, mappings[index].wrapper))
        {
            return false;
        }
    }
    return true;
}

static bool load_manifest_file(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *path)
{
    FILE *stream;
    char  line[MANIFEST_LINE_SIZE];

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        return false;
    }
    while(p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        const char *tab;

        tab = p101_strchr(env, line, '\t');
        if(tab != NULL)
        {
            line[(size_t)(tab - line)] = '\0';
        }
        if(p101_strncmp(env, line, "p101_", sizeof("p101_") - 1U) == 0 && !add_inventory(env, err, model, line))
        {
            break;
        }
    }
    p101_fclose(env, err, stream);
    return p101_error_has_no_error(err);
}

static bool load_manifests(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *directory)    // NOLINT(misc-no-recursion)
{
    DIR           *stream;
    struct dirent *entry;

    P101_TRACE_SCOPE(env);
    stream = p101_opendir(env, err, directory);
    if(stream == NULL)
    {
        return false;
    }
    while((entry = p101_readdir(env, err, stream)) != NULL && p101_error_has_no_error(err))
    {
        char        path[P101_WRAPPER_PATH_SIZE];
        struct stat status;

        if(p101_strcmp(env, entry->d_name, ".") == 0 || p101_strcmp(env, entry->d_name, "..") == 0 || p101_strcmp(env, entry->d_name, ".git") == 0 || p101_strcmp(env, entry->d_name, "build") == 0 ||
           p101_strncmp(env, entry->d_name, "build-", sizeof("build-") - 1U) == 0)
        {
            continue;
        }
        p101_snprintf(env, err, path, sizeof(path), "%s/%s", directory, entry->d_name);
        if(p101_stat(env, err, path, &status) != 0)
        {
            break;
        }
        if(S_ISDIR(status.st_mode))
        {
            if(!load_manifests(env, err, model, path))
            {
                break;
            }
        }
        else if(S_ISREG(status.st_mode) && p101_strcmp(env, entry->d_name, "api-manifest.tsv") == 0)
        {
            if(!load_manifest_file(env, err, model, path))
            {
                break;
            }
        }
    }
    p101_closedir(env, err, stream);
    return p101_error_has_no_error(err);
}

static bool find_workspace_libraries(const struct p101_env *env, const char *program_path, char *path, size_t size)
{
    char   current[P101_WRAPPER_PATH_SIZE];
    size_t attempt;

    P101_TRACE_SCOPE(env);
    /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: discovery probes candidate roots. */
    if(program_path != NULL && p101_realpath(env, NULL, program_path, current) != NULL)
    {
        const char *slash;

        slash = p101_strrchr(env, current, '/');
        if(slash != NULL)
        {
            current[(size_t)(slash - current)] = '\0';
        }
    }
    /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: discovery failure is returned as false. */
    else if(p101_getcwd(env, NULL, current, sizeof(current)) == NULL)
    {
        return false;
    }
    for(attempt = 0U; attempt < WORKSPACE_PARENT_LIMIT; attempt++)
    {
        struct stat status;
        const char *slash;

        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: an empty path rejects this discovery candidate. */
        p101_snprintf(env, NULL, path, size, "%s/libraries", current);
        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: discovery probes candidate roots. */
        if(p101_stat(env, NULL, path, &status) == 0 && S_ISDIR(status.st_mode))
        {
            return true;
        }
        slash = p101_strrchr(env, current, '/');
        if(slash == NULL || slash == current)
        {
            break;
        }
        current[(size_t)(slash - current)] = '\0';
    }
    path[0] = '\0';
    return false;
}

bool p101_wrapper_model_load_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments, const char *program_path)
{
    char   libraries[P101_WRAPPER_PATH_SIZE];
    size_t index;

    P101_TRACE_SCOPE(env);
    if(find_workspace_libraries(env, program_path, libraries, sizeof(libraries)) && !load_manifests(env, err, model, libraries))
    {
        return false;
    }
    for(index = 0U; index < arguments->header_root_count; index++)
    {
        struct p101_c_analysis_options options;
        const char                    *path;
        size_t                         first_fact;
        size_t                         fact_index;

        path       = arguments->header_roots[index];
        first_fact = model->fact_count;
        p101_memset(env, &options, 0, sizeof(options));
        options.paths                                = &path;
        options.path_count                           = 1U;
        options.extra_arguments                      = arguments->extra_arguments;
        options.extra_argument_count                 = arguments->extra_argument_count;
        options.detailed_preprocessing               = true;
        options.include_headers_as_translation_units = true;
        options.keep_going                           = true;
        if(!p101_c_analysis_scan(env, err, &options, p101_wrapper_analysis_observer, model))
        {
            return false;
        }
        for(fact_index = first_fact; fact_index < model->fact_count; fact_index++)
        {
            if(model->facts[fact_index].kind == P101_C_ANALYSIS_FUNCTION && !add_inventory(env, err, model, model->facts[fact_index].name))
            {
                return false;
            }
        }
        model->fact_count = first_fact;
    }
    if(!add_atomic_inventory(env, err, model))
    {
        return false;
    }
    if(model->inventory_count == 0U)
    {
        P101_ERROR_RAISE_USER(err, "No p101 wrapper inventory could be discovered.", 1);
        return false;
    }
    return true;
}
