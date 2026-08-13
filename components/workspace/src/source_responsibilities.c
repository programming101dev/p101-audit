#include "model.h"
#include "workspace_analysis.h"
#include "workspace_audit.h"
#include "workspace_json.h"
#include <dirent.h>
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_record/record.h>

enum
{
    RESPONSIBILITY_ROOT_COUNT = 4,
    RESPONSIBILITY_NAME_SIZE  = 512
};

struct responsibility_scan_paths
{
    char (*values)[P101_WORKSPACE_AUDIT_PATH_SIZE];
    size_t count;
    size_t capacity;
};

static bool   require_text(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size, const char *contract_path, struct p101_workspace_audit_result *result);
static bool   source_suffix(const struct p101_env *env, const char *name);
static bool   excluded_directory(const struct p101_env *env, const char *name);
static bool   add_source_path(const struct p101_env *env, struct p101_error *err, struct responsibility_scan_paths *paths, const char *path);
static bool   collect_source_paths(const struct p101_env *env, struct p101_error *err, struct responsibility_scan_paths *paths, const char *directory);
static bool   path_beneath(const struct p101_env *env, const char *path, const char *root);
static bool   production_path(const struct p101_env *env, const char *path);
static bool   fact_identity_under(const struct p101_env *env, const struct p101_wrapper_model *model, enum p101_c_analysis_kind kind, const char *identity, const char *root);
static bool   validate_owner(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t owner, const char *contract_path, const char *workspace, const struct p101_wrapper_model *model,
                             struct p101_workspace_audit_result *result);
static bool   validate_facade(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t facade, const char *contract_path, const char *workspace, struct p101_workspace_audit_result *result);
static bool   validate_dependencies(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_wrapper_model *model, struct p101_workspace_audit_result *result);
static bool   config_has_target(const struct p101_env *env, const char *text, const char *target);
static bool   include_target(const struct p101_env *env, const char *include_name, char *target, size_t target_size);
static size_t count_lines(const char *text);
static bool   load_fact_bundle(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_wrapper_model *model);
static enum p101_c_analysis_kind analysis_kind(const struct p101_env *env, const char *text);
static bool                      copy_bundle_field(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input);

bool p101_workspace_audit_run_source_responsibilities(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    static const char *const         roots[RESPONSIBILITY_ROOT_COUNT] = {"libraries", "programs", "templates", "playgrounds"};
    struct p101_workspace_json       document;
    struct p101_wrapper_arguments    arguments;
    struct p101_wrapper_model        model;
    struct responsibility_scan_paths scan_paths;
    char                             contract_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                             root_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                             text[RESPONSIBILITY_NAME_SIZE];
    char                             analysis_arguments[P101_WORKSPACE_ANALYSIS_ARGUMENT_CAPACITY][P101_WORKSPACE_AUDIT_PATH_SIZE];
    size_t                           owners;
    size_t                           facades;
    size_t                           row;
    size_t                           index;
    bool                             joined;
    bool                             loaded;
    bool                             found;
    bool                             valid;
    bool                             collected;
    bool                             prepared;
    bool                             scanned;
    bool                             success;

    P101_TRACE_SCOPE(env);
    success = false;
    p101_workspace_json_init(&document);
    p101_wrapper_model_init(&model);
    p101_memset(env, &arguments, 0, sizeof(arguments));
    p101_memset(env, &scan_paths, 0, sizeof(scan_paths));
    scan_paths.capacity = P101_WRAPPER_MAX_PATHS;
    scan_paths.values   = (char (*)[P101_WORKSPACE_AUDIT_PATH_SIZE])p101_calloc(env, err, scan_paths.capacity, sizeof(*scan_paths.values));
    if(scan_paths.values == NULL)
    {
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, contract_path, sizeof(contract_path), options->scripts_root, "contracts/p101-source-responsibilities.json");
    if(!joined)
    {
        goto done;
    }
    loaded = p101_workspace_json_load(env, err, contract_path, &document);
    if(!loaded)
    {
        goto done;
    }
    found = p101_workspace_json_object_get(env, &document, 0U, "schema", &row);
    valid = found && p101_workspace_json_token_equals(env, &document, row, "p101-source-responsibilities-v2");
    if(!valid)
    {
        p101_workspace_audit_add(env, err, result, contract_path, "unexpected source-responsibility schema");
        success = p101_error_has_no_error(err);
        goto done;
    }
    valid = require_text(env, err, &document, 0U, "does_not_prove", text, sizeof(text), contract_path, result);
    found = p101_workspace_json_object_get(env, &document, 0U, "owners", &owners);
    valid = found && document.tokens[owners].kind == P101_WORKSPACE_JSON_ARRAY && document.tokens[owners].child_count > 0U && valid;
    found = p101_workspace_json_object_get(env, &document, 0U, "facades", &facades);
    valid = found && document.tokens[facades].kind == P101_WORKSPACE_JSON_ARRAY && document.tokens[facades].child_count > 0U && valid;
    if(!valid)
    {
        p101_workspace_audit_add(env, err, result, contract_path, "source-responsibility register has no owners or facade ratchets");
        success = p101_error_has_no_error(err);
        goto done;
    }
    collected = true;
    for(index = 0U; index < RESPONSIBILITY_ROOT_COUNT && collected && options->facts_path == NULL; index++)
    {
        joined = p101_workspace_audit_join(env, err, root_path, sizeof(root_path), options->workspace, roots[index]);
        if(!joined)
        {
            collected = false;
        }
        else
        {
            collected = collect_source_paths(env, err, &scan_paths, root_path);
        }
    }
    if(!collected)
    {
        goto done;
    }
    for(index = 0U; index < scan_paths.count; index++)
    {
        arguments.paths[arguments.path_count] = scan_paths.values[index];
        arguments.path_count++;
    }
    arguments.keep_going = true;
    if(options->facts_path != NULL)
    {
        scanned = load_fact_bundle(env, err, options->facts_path, &model);
    }
    else
    {
        prepared = p101_workspace_audit_prepare_analysis(env, err, options, &arguments, analysis_arguments, P101_WORKSPACE_ANALYSIS_ARGUMENT_CAPACITY);
        if(!prepared)
        {
            goto done;
        }
        scanned = p101_wrapper_model_scan(env, err, &model, &arguments);
    }
    if(!scanned)
    {
        goto done;
    }
    for(index = 0U; index < document.tokens[owners].child_count; index++)
    {
        found = p101_workspace_json_array_get(&document, owners, index, &row);
        if(found)
        {
            validate_owner(env, err, &document, row, contract_path, options->workspace, &model, result);
        }
    }
    for(index = 0U; index < document.tokens[facades].child_count; index++)
    {
        found = p101_workspace_json_array_get(&document, facades, index, &row);
        if(found)
        {
            validate_facade(env, err, &document, row, contract_path, options->workspace, result);
        }
    }
    validate_dependencies(env, err, options, &model, result);
    result->checks += scan_paths.count + model.fact_count + document.tokens[owners].child_count + document.tokens[facades].child_count;
    success = p101_error_has_no_error(err);

done:
    p101_free(env, scan_paths.values);
    p101_wrapper_model_destroy(env, &model);
    p101_workspace_json_destroy(env, &document);
    return success;
}

static bool require_text(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size, const char *contract_path, struct p101_workspace_audit_result *result)
{
    size_t value;
    bool   found;
    bool   copied;

    found  = p101_workspace_json_object_get(env, document, object, key, &value);
    copied = found && p101_workspace_json_token_copy(env, err, document, value, output, output_size);
    if(copied)
    {
        copied = output[0] != '\0';
    }
    if(!copied && p101_error_has_no_error(err))
    {
        p101_workspace_audit_add(env, err, result, contract_path, "required source-responsibility text is absent or empty");
    }
    return copied;
}

static bool source_suffix(const struct p101_env *env, const char *name)
{
    static const char *const suffixes[] = {".c", ".cc", ".cpp", ".cxx"};
    size_t                   name_length;
    size_t                   suffix_length;
    size_t                   index;
    int                      comparison;
    bool                     matches;

    name_length = p101_strlen(env, name);
    matches     = false;
    for(index = 0U; index < sizeof(suffixes) / sizeof(suffixes[0]); index++)
    {
        suffix_length = p101_strlen(env, suffixes[index]);
        if(name_length >= suffix_length)
        {
            comparison = p101_strcmp(env, name + name_length - suffix_length, suffixes[index]);
            if(comparison == 0)
            {
                matches = true;
                break;
            }
        }
    }
    return matches;
}

static bool excluded_directory(const struct p101_env *env, const char *name)
{
    int  comparison;
    bool excluded;

    comparison = p101_strcmp(env, name, ".git");
    excluded   = comparison == 0;
    if(!excluded)
    {
        comparison = p101_strcmp(env, name, "test");
        excluded   = comparison == 0;
    }
    if(!excluded)
    {
        comparison = p101_strcmp(env, name, "tests");
        excluded   = comparison == 0;
    }
    if(!excluded)
    {
        comparison = p101_strcmp(env, name, "fuzz");
        excluded   = comparison == 0;
    }
    if(!excluded)
    {
        comparison = p101_strncmp(env, name, "build", 5U);
        excluded   = comparison == 0;
    }
    return excluded;
}

static bool add_source_path(const struct p101_env *env, struct p101_error *err, struct responsibility_scan_paths *paths, const char *path)
{
    size_t index;
    size_t length;
    int    comparison;
    bool   added;

    added = false;
    for(index = 0U; index < paths->count; index++)
    {
        comparison = p101_strcmp(env, paths->values[index], path);
        if(comparison == 0)
        {
            added = true;
            goto done;
        }
    }
    length = p101_strlen(env, path);
    if(paths->count >= paths->capacity || length >= sizeof(paths->values[0]))
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    p101_memcpy(env, paths->values[paths->count], path, length + 1U);
    paths->count++;
    added = true;

done:
    return added;
}

static bool collect_source_paths(const struct p101_env *env, struct p101_error *err, struct responsibility_scan_paths *paths, const char *directory)
{
    char           path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    DIR           *stream;
    struct dirent *entry;
    struct stat    status_buffer;
    int            status;
    int            close_status;
    bool           joined;
    bool           collected;

    stream = p101_opendir(env, err, directory);
    if(stream == NULL)
    {
        collected = false;
        goto done;
    }
    collected = true;
    while(collected)
    {
        entry = p101_readdir(env, err, stream);
        if(entry == NULL)
        {
            break;
        }
        if(entry->d_name[0] == '.' || excluded_directory(env, entry->d_name))
        {
            continue;
        }
        joined = p101_workspace_audit_join(env, err, path, sizeof(path), directory, entry->d_name);
        if(!joined)
        {
            collected = false;
            break;
        }
        status = p101_stat(env, P101_ERROR_OPTIONAL, path, &status_buffer);
        if(status != 0)
        {
            continue;
        }
        if(S_ISDIR(status_buffer.st_mode) != 0)
        {
            if(p101_strcmp(env, entry->d_name, "src") == 0)
            {
                collected = add_source_path(env, err, paths, path);
            }
            else
            {
                collected = collect_source_paths(env, err, paths, path);
            }
        }
        else if(S_ISREG(status_buffer.st_mode) != 0 && source_suffix(env, entry->d_name))
        {
            collected = add_source_path(env, err, paths, path);
        }
    }
    close_status = p101_closedir(env, P101_ERROR_OPTIONAL, stream);
    if(close_status != 0)
    {
        collected = false;
    }

done:
    return collected;
}

static bool path_beneath(const struct p101_env *env, const char *path, const char *root)
{
    size_t root_length;
    int    comparison;
    bool   beneath;

    root_length = p101_strlen(env, root);
    comparison  = p101_strncmp(env, path, root, root_length);
    beneath     = comparison == 0;
    if(beneath && path[root_length] != '\0')
    {
        beneath = path[root_length] == '/';
    }
    return beneath;
}

static bool production_path(const struct p101_env *env, const char *path)
{
    static const char *const excluded[] = {"/test/", "/tests/", "/fuzz/", "/.git/", "/build-", "/build_", "/build."};
    size_t                   index;
    const char              *match;
    bool                     production;

    production = true;
    for(index = 0U; index < sizeof(excluded) / sizeof(excluded[0]); index++)
    {
        match = p101_strstr(env, path, excluded[index]);
        if(match != NULL)
        {
            production = false;
            break;
        }
    }
    return production;
}

static bool fact_identity_under(const struct p101_env *env, const struct p101_wrapper_model *model, enum p101_c_analysis_kind kind, const char *identity, const char *root)
{
    size_t index;
    int    comparison;
    bool   found;

    found = false;
    for(index = 0U; index < model->fact_count; index++)
    {
        comparison = p101_strcmp(env, model->facts[index].usr, identity);
        if(model->facts[index].kind == kind && comparison == 0 && path_beneath(env, model->facts[index].path, root))
        {
            found = true;
            break;
        }
    }
    return found;
}

static bool validate_owner(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t owner, const char *contract_path, const char *workspace, const struct p101_wrapper_model *model,
                           struct p101_workspace_audit_result *result)
{
    char   identifier[RESPONSIBILITY_NAME_SIZE];
    char   owner_name[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   owner_root[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   consumer_name[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   consumer_root[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   identity[RESPONSIBILITY_NAME_SIZE];
    char   message[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    size_t markers;
    size_t consumers;
    size_t forbidden_definitions;
    size_t forbidden_calls;
    size_t value;
    size_t index;
    size_t fact_index;
    bool   valid;
    bool   found;
    bool   copied;
    bool   joined;
    int    comparison;

    valid  = require_text(env, err, document, owner, "id", identifier, sizeof(identifier), contract_path, result);
    valid  = require_text(env, err, document, owner, "owner", owner_name, sizeof(owner_name), contract_path, result) && valid;
    joined = valid && p101_workspace_audit_join(env, err, owner_root, sizeof(owner_root), workspace, owner_name);
    if(!joined)
    {
        return false;
    }
    found = p101_workspace_json_object_get(env, document, owner, "marker_usrs", &markers);
    if(!found || document->tokens[markers].kind != P101_WORKSPACE_JSON_ARRAY || document->tokens[markers].child_count == 0U)
    {
        p101_workspace_audit_add(env, err, result, contract_path, "source-responsibility owner has no semantic markers");
    }
    else
    {
        for(index = 0U; index < document->tokens[markers].child_count; index++)
        {
            found  = p101_workspace_json_array_get(document, markers, index, &value);
            copied = found && p101_workspace_json_token_copy(env, err, document, value, identity, sizeof(identity));
            if(copied && !fact_identity_under(env, model, P101_C_ANALYSIS_FUNCTION, identity, owner_root))
            {
                p101_snprintf(env, err, message, sizeof(message), "owner %s lacks declaration identity %s", identifier, identity);
                p101_workspace_audit_add(env, err, result, owner_root, message);
            }
        }
    }
    found = p101_workspace_json_object_get(env, document, owner, "consumer_roots", &consumers);
    if(!found || document->tokens[consumers].kind != P101_WORKSPACE_JSON_ARRAY || document->tokens[consumers].child_count == 0U)
    {
        p101_workspace_audit_add(env, err, result, contract_path, "source-responsibility owner has no consumers");
        return p101_error_has_no_error(err);
    }
    forbidden_definitions = SIZE_MAX;
    forbidden_calls       = SIZE_MAX;
    p101_workspace_json_object_get(env, document, owner, "forbidden_definition_usrs", &forbidden_definitions);
    p101_workspace_json_object_get(env, document, owner, "forbidden_call_usrs", &forbidden_calls);
    for(index = 0U; index < document->tokens[consumers].child_count; index++)
    {
        found  = p101_workspace_json_array_get(document, consumers, index, &value);
        copied = found && p101_workspace_json_token_copy(env, err, document, value, consumer_name, sizeof(consumer_name));
        joined = copied && p101_workspace_audit_join(env, err, consumer_root, sizeof(consumer_root), workspace, consumer_name);
        if(!joined)
        {
            continue;
        }
        for(fact_index = 0U; fact_index < model->fact_count; fact_index++)
        {
            const struct p101_wrapper_fact *fact;
            size_t                          list_index;
            size_t                          identity_token;

            fact = &model->facts[fact_index];
            if(!production_path(env, fact->path) || !path_beneath(env, fact->path, consumer_root))
            {
                continue;
            }
            if(forbidden_definitions != SIZE_MAX && fact->kind == P101_C_ANALYSIS_FUNCTION)
            {
                for(list_index = 0U; list_index < document->tokens[forbidden_definitions].child_count; list_index++)
                {
                    found      = p101_workspace_json_array_get(document, forbidden_definitions, list_index, &identity_token);
                    copied     = found && p101_workspace_json_token_copy(env, err, document, identity_token, identity, sizeof(identity));
                    comparison = copied ? p101_strcmp(env, fact->usr, identity) : 1;
                    if(comparison == 0)
                    {
                        p101_snprintf(env, err, message, sizeof(message), "%s redefines owner declaration %s", fact->path, identity);
                        p101_workspace_audit_add(env, err, result, fact->path, message);
                    }
                }
            }
            if(forbidden_calls != SIZE_MAX && fact->kind == P101_C_ANALYSIS_CALL)
            {
                for(list_index = 0U; list_index < document->tokens[forbidden_calls].child_count; list_index++)
                {
                    found      = p101_workspace_json_array_get(document, forbidden_calls, list_index, &identity_token);
                    copied     = found && p101_workspace_json_token_copy(env, err, document, identity_token, identity, sizeof(identity));
                    comparison = copied ? p101_strcmp(env, fact->usr, identity) : 1;
                    if(comparison == 0)
                    {
                        p101_snprintf(env, err, message, sizeof(message), "%s bypasses %s with declaration %s", fact->path, identifier, identity);
                        p101_workspace_audit_add(env, err, result, fact->path, message);
                    }
                }
            }
        }
    }
    return p101_error_has_no_error(err);
}

static bool validate_facade(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t facade, const char *contract_path, const char *workspace, struct p101_workspace_audit_result *result)
{
    char   relative[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   reason[RESPONSIBILITY_NAME_SIZE];
    char   message[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    char  *text;
    size_t length;
    size_t maximum_token;
    size_t maximum;
    size_t lines;
    bool   valid;
    bool   found;
    bool   parsed;
    bool   joined;
    bool   read_ok;

    text    = NULL;
    length  = 0U;
    maximum = 0U;
    valid   = require_text(env, err, document, facade, "path", relative, sizeof(relative), contract_path, result);
    valid   = require_text(env, err, document, facade, "reason", reason, sizeof(reason), contract_path, result) && valid;
    found   = p101_workspace_json_object_get(env, document, facade, "maximum_lines", &maximum_token);
    parsed  = found && p101_workspace_json_token_size(env, document, maximum_token, &maximum) && maximum > 0U;
    if(!valid || !parsed)
    {
        p101_workspace_audit_add(env, err, result, contract_path, "invalid facade ratchet");
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, path, sizeof(path), workspace, relative);
    if(!joined)
    {
        goto done;
    }
    read_ok = p101_workspace_audit_read_file(env, err, path, &text, &length);
    if(!read_ok)
    {
        p101_workspace_audit_add(env, err, result, path, "missing facade");
        goto done;
    }
    lines = count_lines(text);
    if(lines > maximum)
    {
        p101_snprintf(env, err, message, sizeof(message), "facade responsibility grew: %s=%zu>%zu", relative, lines, maximum);
        p101_workspace_audit_add(env, err, result, path, message);
    }

done:
    p101_free(env, text);
    return p101_error_has_no_error(err);
}

static bool validate_dependencies(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_wrapper_model *model, struct p101_workspace_audit_result *result)
{
    char   checked_repositories[128][P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   repository[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   config_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   target[RESPONSIBILITY_NAME_SIZE];
    char   message[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    char  *config_text;
    size_t config_length;
    size_t root_index;
    size_t fact_index;
    size_t edges;
    size_t checked_count;
    size_t checked_index;
    bool   joined;
    bool   read_ok;
    bool   declares_event;
    bool   declares_record;
    bool   uses_event;
    bool   uses_record;
    bool   already_checked;

    (void)options;
    p101_memset(env, checked_repositories, 0, sizeof(checked_repositories));
    config_text   = NULL;
    edges         = 0U;
    checked_count = 0U;
    for(root_index = 0U; root_index < model->fact_count; root_index++)
    {
        const struct p101_wrapper_fact *config_fact;

        config_fact = &model->facts[root_index];
        if(config_fact->kind != P101_C_ANALYSIS_FILE || !production_path(env, config_fact->path))
        {
            continue;
        }
        p101_snprintf(env, err, repository, sizeof(repository), "%s", config_fact->path);
        while(repository[0] != '\0')
        {
            const char *separator;

            separator = p101_strrchr(env, repository, '/');
            if(separator == NULL)
            {
                repository[0] = '\0';
                break;
            }
            repository[(size_t)(separator - repository)] = '\0';
            joined                                       = p101_workspace_audit_join(env, err, config_path, sizeof(config_path), repository, "config.cmake");
            if(joined && p101_workspace_audit_file_exists(env, P101_ERROR_OPTIONAL, config_path))
            {
                break;
            }
        }
        if(repository[0] == '\0')
        {
            continue;
        }
        already_checked = false;
        for(checked_index = 0U; checked_index < checked_count; checked_index++)
        {
            if(p101_strcmp(env, checked_repositories[checked_index], repository) == 0)
            {
                already_checked = true;
                break;
            }
        }
        if(already_checked)
        {
            continue;
        }
        if(checked_count >= sizeof(checked_repositories) / sizeof(checked_repositories[0]))
        {
            P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
            goto done;
        }
        p101_snprintf(env, err, checked_repositories[checked_count], sizeof(checked_repositories[checked_count]), "%s", repository);
        checked_count++;
        read_ok = p101_workspace_audit_read_file(env, err, config_path, &config_text, &config_length);
        if(!read_ok)
        {
            goto done;
        }
        declares_event  = config_has_target(env, config_text, "p101_tool_event");
        declares_record = config_has_target(env, config_text, "p101_record");
        uses_event      = false;
        uses_record     = false;
        for(fact_index = 0U; fact_index < model->fact_count; fact_index++)
        {
            const struct p101_wrapper_fact *fact;

            fact = &model->facts[fact_index];
            if(fact->kind != P101_C_ANALYSIS_INCLUDE || !production_path(env, fact->path) || !path_beneath(env, fact->path, repository))
            {
                continue;
            }
            if(include_target(env, fact->name, target, sizeof(target)))
            {
                if(p101_strcmp(env, target, "p101_tool_event") == 0)
                {
                    uses_event = true;
                }
                if(p101_strcmp(env, target, "p101_record") == 0)
                {
                    uses_record = true;
                }
                if(!config_has_target(env, config_text, target))
                {
                    p101_snprintf(env, err, message, sizeof(message), "config has undeclared p101 include boundary: %s", target);
                    p101_workspace_audit_add(env, err, result, config_path, message);
                }
                edges++;
            }
        }
        if(declares_event && !uses_event)
        {
            p101_workspace_audit_add(env, err, result, config_path, "config declares p101_tool_event without using its API");
        }
        if(uses_record && !declares_record)
        {
            p101_workspace_audit_add(env, err, result, config_path, "config uses p101_record without declaring it");
        }
        p101_free(env, config_text);
        config_text = NULL;
        result->checks++;
    }
    result->checks += edges;

done:
    p101_free(env, config_text);
    return p101_error_has_no_error(err);
}

static bool config_has_target(const struct p101_env *env, const char *text, const char *target)
{
    const char *cursor;
    size_t      length;
    bool        found;

    cursor = text;
    length = p101_strlen(env, target);
    found  = false;
    while(cursor != NULL)
    {
        cursor = p101_strstr(env, cursor, target);
        if(cursor == NULL)
        {
            break;
        }
        if((cursor == text || !(cursor[-1] == '_' || (cursor[-1] >= '0' && cursor[-1] <= '9') || (cursor[-1] >= 'A' && cursor[-1] <= 'Z') || (cursor[-1] >= 'a' && cursor[-1] <= 'z'))) &&
           !(cursor[length] == '_' || (cursor[length] >= '0' && cursor[length] <= '9') || (cursor[length] >= 'A' && cursor[length] <= 'Z') || (cursor[length] >= 'a' && cursor[length] <= 'z')))
        {
            found = true;
            break;
        }
        cursor++;
    }
    return found;
}

static bool include_target(const struct p101_env *env, const char *include_name, char *target, size_t target_size)
{
    const char *start;
    const char *separator;
    size_t      length;
    bool        copied;

    start = include_name;
    if(start[0] == '<' || start[0] == '"')
    {
        start++;
    }
    separator = p101_strchr(env, start, '/');
    copied    = separator != NULL;
    if(copied)
    {
        length = (size_t)(separator - start);
        copied = length > 5U && length < target_size && p101_strncmp(env, start, "p101_", 5U) == 0;
        if(copied)
        {
            p101_memcpy(env, target, start, length);
            target[length] = '\0';
        }
    }
    return copied;
}

static size_t count_lines(const char *text)
{
    size_t lines;

    lines = text[0] == '\0' ? 0U : 1U;
    while(*text != '\0')
    {
        if(*text == '\n' && text[1] != '\0')
        {
            lines++;
        }
        text++;
    }
    return lines;
}

static bool load_fact_bundle(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_wrapper_model *model)
{
    enum
    {
        BUNDLE_FIELD_COUNT = 7
    };

    char                     *text;
    char                     *line;
    char                     *next_line;
    char                     *records;
    char                     *cursor;
    const char               *line_end;
    char                     *fields[BUNDLE_FIELD_COUNT];
    struct p101_wrapper_fact *fact;
    size_t                    length;
    size_t                    count;
    size_t                    field_index;
    int                       parse_status;
    bool                      loaded;
    bool                      copied;

    text   = NULL;
    length = 0U;
    loaded = p101_workspace_audit_read_file(env, err, path, &text, &length);
    if(!loaded)
    {
        goto done;
    }
    line     = text;
    line_end = p101_strchr(env, line, '\n');
    if(line_end == NULL)
    {
        P101_ERROR_RAISE_USER(err, "invalid semantic fact bundle", EINVAL);
        loaded = false;
        goto done;
    }
    next_line  = line + (size_t)(line_end - line);
    *next_line = '\0';
    if(p101_strcmp(env, line, "P101SEMANTIC\t1") != 0)
    {
        P101_ERROR_RAISE_USER(err, "invalid semantic fact bundle", EINVAL);
        loaded = false;
        goto done;
    }
    count   = 0U;
    records = next_line + 1;
    line    = records;
    while(*line != '\0')
    {
        count++;
        line_end = p101_strchr(env, line, '\n');
        if(line_end == NULL)
        {
            break;
        }
        next_line = line + (size_t)(line_end - line);
        line      = next_line + 1;
    }
    model->facts = (struct p101_wrapper_fact *)p101_calloc(env, err, count, sizeof(*model->facts));
    if(model->facts == NULL)
    {
        loaded = false;
        goto done;
    }
    model->fact_capacity = count;
    line                 = records;
    while(*line != '\0')
    {
        line_end = p101_strchr(env, line, '\n');
        if(line_end != NULL)
        {
            next_line  = line + (size_t)(line_end - line);
            *next_line = '\0';
        }
        else
        {
            next_line = NULL;
        }
        cursor = line;
        for(field_index = 0U; field_index < BUNDLE_FIELD_COUNT; field_index++)
        {
            fields[field_index] = p101_record_split(&cursor);
            if(fields[field_index] != NULL)
            {
                p101_record_unescape_field(fields[field_index]);
            }
        }
        if(fields[BUNDLE_FIELD_COUNT - 1U] == NULL || cursor != NULL)
        {
            P101_ERROR_RAISE_USER(err, "invalid semantic fact record", EINVAL);
            loaded = false;
            goto done;
        }
        fact         = &model->facts[model->fact_count];
        fact->kind   = analysis_kind(env, fields[0]);
        copied       = copy_bundle_field(env, err, fact->path, sizeof(fact->path), fields[1]);
        copied       = copy_bundle_field(env, err, fact->name, sizeof(fact->name), fields[2]) && copied;
        copied       = copy_bundle_field(env, err, fact->usr, sizeof(fact->usr), fields[3]) && copied;
        copied       = copy_bundle_field(env, err, fact->caller_usr, sizeof(fact->caller_usr), fields[4]) && copied;
        copied       = copy_bundle_field(env, err, fact->resolved, sizeof(fact->resolved), fields[5]) && copied;
        parse_status = p101_record_parse_size(fields[6], &fact->line);
        if(!copied || parse_status == 0)
        {
            P101_ERROR_RAISE_USER(err, "invalid semantic fact field", EINVAL);
            loaded = false;
            goto done;
        }
        model->fact_count++;
        if(next_line == NULL)
        {
            break;
        }
        line = next_line + 1;
    }
    loaded = true;

done:
    p101_free(env, text);
    return loaded;
}

static enum p101_c_analysis_kind analysis_kind(const struct p101_env *env, const char *text)
{
    static const char *const  names[] = {"FILE", "INCLUDE", "FUNCTION", "PARAMETER", "CALL", "TYPE", "ENUM", "ENUMERATOR", "MACRO", "NOTE", "MUTATION", "DIAGNOSTIC"};
    size_t                    index;
    int                       comparison;
    enum p101_c_analysis_kind kind;

    kind = P101_C_ANALYSIS_DIAGNOSTIC;
    for(index = 0U; index < sizeof(names) / sizeof(names[0]); index++)
    {
        comparison = p101_strcmp(env, text, names[index]);
        if(comparison == 0)
        {
            kind = (enum p101_c_analysis_kind)index;
            break;
        }
    }
    return kind;
}

static bool copy_bundle_field(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input)
{
    size_t length;
    bool   copied;

    length = p101_strlen(env, input);
    copied = length < output_size;
    if(copied)
    {
        p101_memcpy(env, output, input, length + 1U);
    }
    else
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
    }
    return copied;
}
