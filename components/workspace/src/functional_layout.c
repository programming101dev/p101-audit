#include "workspace_audit.h"
#include <dirent.h>
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/sys/p101_stat.h>

enum
{
    LAYOUT_LINE_SIZE             = 32768,
    LAYOUT_FIELD_COUNT           = 12,
    LAYOUT_CENTRAL_FIELD_COUNT   = 8,
    LAYOUT_NEEDLE_SIZE           = 128,
    LAYOUT_INITIAL_PATH_CAPACITY = 32,
    FIELD_FUNCTION               = 0,
    FIELD_FUNCTION_USR           = 1,
    FIELD_PROVENANCE             = 2,
    FIELD_CURRENT_SOURCE         = 3,
    FIELD_CURRENT_HEADER         = 4,
    FIELD_ORIGINAL_HEADER        = 6,
    FIELD_LINUX                  = 7,
    FIELD_MACOS                  = 8,
    FIELD_FREEBSD                = 9
};

enum
{
    CENTRAL_FIELD_FUNCTION       = 0,
    CENTRAL_FIELD_FUNCTION_USR   = 1,
    CENTRAL_FIELD_DOMAIN         = 2,
    CENTRAL_FIELD_PROVENANCE     = 3,
    CENTRAL_FIELD_CURRENT_SOURCE = 4,
    CENTRAL_FIELD_CURRENT_HEADER = 5
};

struct functional_domain
{
    const char *name;
    const char *repository;
    bool        source_is_namespaced;
};

static const struct functional_domain domains[] = {
    {"io",              "io",              false},
    {"filesystem",      "filesystem",      false},
    {"memory",          "memory",          false},
    {"process",         "process",         false},
    {"thread",          "concurrency",     true },
    {"sync",            "concurrency",     true },
    {"ipc",             "ipc",             false},
    {"network",         "network",         false},
    {"terminal",        "terminal",        false},
    {"time",            "time",            false},
    {"identity",        "identity",        false},
    {"text",            "text",            true },
    {"locale",          "text",            true },
    {"math",            "numeric",         true },
    {"search",          "search",          false},
    {"dynamic_linking", "dynamic_linking", false},
    {"diagnostics",     "diagnostics",     false},
    {"database",        "database",        false},
    {"cli",             "cli",             false},
    {"random",          "numeric",         true },
    {"host",            "host",            false},
};

struct retired_library
{
    const char *name;
    const char *source_directory;
};

static const struct retired_library retired[] = {
    {"lib_posix",          "posix"         },
    {"lib_posix_optional", "posix_optional"},
    {"lib_posix_xsi",      "posix_xsi"     },
    {"lib_unix",           "unix"          },
};

struct path_set
{
    char **items;
    size_t count;
    size_t capacity;
};

static void   path_set_init(struct path_set *set);
static void   path_set_destroy(const struct p101_env *env, struct path_set *set);
static bool   path_set_add(const struct p101_env *env, struct p101_error *err, struct path_set *set, const char *path);
static bool   path_set_contains(const struct p101_env *env, const struct path_set *set, const char *path);
static size_t split_fields(char *line, char **fields, size_t capacity);
static bool   derive_layout(const struct p101_env *env, struct p101_error *err, const char *domain, const char *original_header, char *source, size_t source_size, char *header, size_t header_size);
static bool   collect_files(const struct p101_env *env, struct p101_error *err, const char *root, const char *relative, const char *suffix, struct path_set *set);
static bool   collect_central_identities(const struct p101_env *env, struct p101_error *err, const char *path, struct path_set *identities, struct p101_workspace_audit_result *result);
static bool   validate_domain(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *domain, const char *repository, bool source_is_namespaced, const char *central_text,
                              struct path_set *local_identities, struct p101_workspace_audit_result *result);
static bool   cmake_set_matches(const struct p101_env *env, const char *text, const char *variable, const struct path_set *expected);
static bool   text_has_retired_target(const struct p101_env *env, const char *text);
static bool   scan_retired_references(const struct p101_env *env, struct p101_error *err, const char *root, const char *relative, struct p101_workspace_audit_result *result);
static bool   should_scan_text(const struct p101_env *env, const char *name);

bool p101_workspace_audit_run_functional_layout(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    char            repos_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            central_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char           *repos_text;
    char           *central_text;
    size_t          repos_length;
    size_t          central_length;
    size_t          index;
    char            needle[LAYOUT_NEEDLE_SIZE];
    int             written;
    const char     *match;
    bool            joined;
    bool            validated;
    bool            scanned;
    bool            success;
    struct path_set local_identities;
    struct path_set central_identities;

    P101_TRACE_SCOPE(env);
    repos_text   = NULL;
    central_text = NULL;
    success      = false;
    path_set_init(&local_identities);
    path_set_init(&central_identities);
    joined = p101_workspace_audit_join(env, err, repos_path, sizeof(repos_path), options->scripts_root, "repos.txt");
    if(!joined)
    {
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, central_path, sizeof(central_path), options->scripts_root, "contracts/wrapper-library-map.tsv");
    if(!joined)
    {
        goto done;
    }
    validated = p101_workspace_audit_read_file(env, err, repos_path, &repos_text, &repos_length);
    if(!validated)
    {
        goto done;
    }
    validated = p101_workspace_audit_read_file(env, err, central_path, &central_text, &central_length);
    if(!validated)
    {
        goto done;
    }
    (void)repos_length;
    (void)central_length;
    validated = collect_central_identities(env, err, central_path, &central_identities, result);
    if(!validated)
    {
        goto done;
    }
    for(index = 0U; index < sizeof(domains) / sizeof(domains[0]); index++)
    {
        written = p101_snprintf(env, err, needle, sizeof(needle), "../libraries/lib_%s|", domains[index].repository);
        if(written < 0 || (size_t)written >= sizeof(needle))
        {
            goto done;
        }
        match = p101_strstr(env, repos_text, needle);
        if(match == NULL)
        {
            written = p101_snprintf(env, err, needle, sizeof(needle), "repos.txt lacks owner lib_%s for functional domain %s", domains[index].repository, domains[index].name);
            if(written >= 0 && (size_t)written < sizeof(needle))
            {
                p101_workspace_audit_add(env, err, result, repos_path, needle);
            }
        }
        validated = validate_domain(env, err, options, domains[index].name, domains[index].repository, domains[index].source_is_namespaced, central_text, &local_identities, result);
        if(!validated)
        {
            goto done;
        }
    }
    for(index = 0U; index < sizeof(retired) / sizeof(retired[0]); index++)
    {
        written = p101_snprintf(env, err, needle, sizeof(needle), "../libraries/%s|", retired[index].name);
        if(written < 0 || (size_t)written >= sizeof(needle))
        {
            goto done;
        }
        match = p101_strstr(env, repos_text, needle);
        if(match != NULL)
        {
            written = p101_snprintf(env, err, needle, sizeof(needle), "repos.txt still admits retired library %s", retired[index].name);
            if(written >= 0 && (size_t)written < sizeof(needle))
            {
                p101_workspace_audit_add(env, err, result, repos_path, needle);
            }
        }
    }
    if(central_identities.count != local_identities.count)
    {
        p101_workspace_audit_add(env, err, result, central_path, "central and per-library ownership inventories differ");
    }
    for(index = 0U; index < central_identities.count; index++)
    {
        bool present;

        present = path_set_contains(env, &local_identities, central_identities.items[index]);
        if(!present)
        {
            p101_workspace_audit_add(env, err, result, central_path, "central ownership has no per-library row");
        }
    }
    scanned = scan_retired_references(env, err, options->workspace, "libraries", result);
    if(!scanned)
    {
        goto done;
    }
    scanned = scan_retired_references(env, err, options->workspace, "programs", result);
    if(!scanned)
    {
        goto done;
    }
    scanned = scan_retired_references(env, err, options->workspace, "templates", result);
    if(!scanned)
    {
        goto done;
    }
    scanned = scan_retired_references(env, err, options->workspace, "playgrounds", result);
    if(!scanned)
    {
        goto done;
    }
    scanned = scan_retired_references(env, err, options->workspace, "examples", result);
    if(!scanned)
    {
        goto done;
    }
    success = p101_error_has_no_error(err);

done:
    p101_free(env, repos_text);
    p101_free(env, central_text);
    path_set_destroy(env, &local_identities);
    path_set_destroy(env, &central_identities);
    return success;
}

static void path_set_init(struct path_set *set)
{
    set->items    = NULL;
    set->count    = 0U;
    set->capacity = 0U;
}

static void path_set_destroy(const struct p101_env *env, struct path_set *set)
{
    size_t index;

    for(index = 0U; index < set->count; index++)
    {
        p101_free(env, set->items[index]);
    }
    p101_free(env, (void *)set->items);
    path_set_init(set);
}

static bool path_set_add(const struct p101_env *env, struct p101_error *err, struct path_set *set, const char *path)
{
    char **resized;
    char  *copy;
    size_t capacity;
    size_t length;
    bool   added;
    bool   present;

    added   = false;
    present = path_set_contains(env, set, path);
    if(present)
    {
        added = true;
        goto done;
    }
    if(set->count == set->capacity)
    {
        capacity = set->capacity == 0U ? LAYOUT_INITIAL_PATH_CAPACITY : set->capacity * 2U;
        if(capacity < set->capacity || capacity > SIZE_MAX / sizeof(*set->items))
        {
            P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
            goto done;
        }
        resized = (char **)p101_realloc(env, err, (void *)set->items, capacity * sizeof(*set->items));
        if(resized == NULL)
        {
            goto done;
        }
        set->items    = resized;
        set->capacity = capacity;
    }
    length = p101_strlen(env, path);
    copy   = (char *)p101_malloc(env, err, length + 1U);
    if(copy == NULL)
    {
        goto done;
    }
    p101_memcpy(env, copy, path, length + 1U);
    set->items[set->count] = copy;
    set->count++;
    added = true;

done:
    return added;
}

static bool path_set_contains(const struct p101_env *env, const struct path_set *set, const char *path)
{
    size_t index;
    bool   present;

    present = false;
    for(index = 0U; index < set->count && !present; index++)
    {
        int comparison;

        comparison = p101_strcmp(env, set->items[index], path);
        present    = comparison == 0;
    }
    return present;
}

static size_t split_fields(char *line, char **fields, size_t capacity)
{
    size_t count;
    char  *cursor;

    count  = 0U;
    cursor = line;
    while(count < capacity)
    {
        fields[count] = cursor;
        count++;
        while(*cursor != '\0' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r')
        {
            cursor++;
        }
        if(*cursor != '\t')
        {
            *cursor = '\0';
            break;
        }
        *cursor = '\0';
        cursor++;
    }
    return count;
}

static bool derive_layout(const struct p101_env *env, struct p101_error *err, const char *domain, const char *original_header, char *source, size_t source_size, char *header, size_t header_size)
{
    const char *include_part;
    const char *package_end;
    const char *relative;
    const char *name;
    const char *prefix;
    const char *extension;
    size_t      directory_length;
    size_t      name_length;
    int         written;
    bool        derived;

    derived      = false;
    include_part = p101_strstr(env, original_header, "/include/");
    if(include_part == NULL)
    {
        P101_ERROR_RAISE_USER(err, "original_header has no include component", EINVAL);
        goto done;
    }
    package_end = p101_strchr(env, include_part + sizeof("/include/") - 1U, '/');
    if(package_end == NULL)
    {
        P101_ERROR_RAISE_USER(err, "original_header has no package component", EINVAL);
        goto done;
    }
    relative = package_end + 1;
    written  = p101_snprintf(env, err, header, header_size, "include/p101_%s/%s", domain, relative);
    if(written < 0 || (size_t)written >= header_size)
    {
        goto done;
    }
    name = p101_strrchr(env, relative, '/');
    if(name == NULL)
    {
        name             = relative;
        directory_length = 0U;
    }
    else
    {
        directory_length = (size_t)(name - relative) + 1U;
        name++;
    }
    prefix = name;
    if(p101_strncmp(env, name, "p101_", sizeof("p101_") - 1U) == 0)
    {
        prefix = name + sizeof("p101_") - 1U;
    }
    extension   = p101_strrchr(env, prefix, '.');
    name_length = extension == NULL ? p101_strlen(env, prefix) : (size_t)(extension - prefix);
    written     = p101_snprintf(env, err, source, source_size, "src/%.*s%.*s.c", (int)directory_length, relative, (int)name_length, prefix);
    if(written >= 0 && (size_t)written < source_size)
    {
        derived = true;
    }

done:
    return derived;
}

static bool collect_files(const struct p101_env *env, struct p101_error *err, const char *root, const char *relative, const char *suffix, struct path_set *set)    // NOLINT(misc-no-recursion): bounded workspace tree walk.
{
    char                 directory_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                 child_relative[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                 child_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    DIR                 *directory;
    const struct dirent *entry;
    bool                 joined;
    bool                 collected;
    bool                 nested;
    bool                 is_directory;
    int                  close_status;
    size_t               name_length;
    size_t               suffix_length;
    int                  comparison;
    int                  stat_status;
    struct stat          status_buffer;

    collected = false;
    joined    = p101_workspace_audit_join(env, err, directory_path, sizeof(directory_path), root, relative);
    if(!joined)
    {
        goto done;
    }
    directory = p101_opendir(env, err, directory_path);
    if(directory == NULL)
    {
        goto done;
    }
    suffix_length = p101_strlen(env, suffix);
    while(true)
    {
        entry = p101_readdir(env, err, directory);
        if(entry == NULL)
        {
            break;
        }
        comparison = p101_strcmp(env, entry->d_name, ".");
        if(comparison == 0)
        {
            continue;
        }
        comparison = p101_strcmp(env, entry->d_name, "..");
        if(comparison == 0)
        {
            continue;
        }
        joined = p101_workspace_audit_join(env, err, child_relative, sizeof(child_relative), relative, entry->d_name);
        if(!joined)
        {
            break;
        }
        joined = p101_workspace_audit_join(env, err, child_path, sizeof(child_path), root, child_relative);
        if(!joined)
        {
            break;
        }
        stat_status = p101_lstat(env, err, child_path, &status_buffer);
        if(stat_status != 0)
        {
            break;
        }
        is_directory = S_ISDIR(status_buffer.st_mode);
        if(is_directory)
        {
            nested = collect_files(env, err, root, child_relative, suffix, set);
            if(!nested)
            {
                break;
            }
            continue;
        }
        name_length = p101_strlen(env, entry->d_name);
        if(name_length < suffix_length)
        {
            continue;
        }
        comparison = p101_strcmp(env, entry->d_name + name_length - suffix_length, suffix);
        if(comparison != 0)
        {
            continue;
        }
        joined = path_set_add(env, err, set, child_relative);
        if(!joined)
        {
            break;
        }
    }
    close_status = p101_closedir(env, P101_ERROR_OPTIONAL, directory);
    if(close_status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
    collected = p101_error_has_no_error(err);

done:
    return collected;
}

static bool collect_central_identities(const struct p101_env *env, struct p101_error *err, const char *path, struct path_set *identities, struct p101_workspace_audit_result *result)
{
    char   line[LAYOUT_LINE_SIZE];
    char  *fields[LAYOUT_CENTRAL_FIELD_COUNT];
    FILE  *stream;
    size_t field_count;
    bool   present;
    bool   added;
    bool   collected;
    int    close_status;

    collected = false;
    stream    = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    (void)p101_fgets(env, err, line, sizeof(line), stream);
    while(true)
    {
        const char *read_result;

        read_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(read_result == NULL)
        {
            break;
        }
        field_count = split_fields(line, fields, LAYOUT_CENTRAL_FIELD_COUNT);
        if(field_count != LAYOUT_CENTRAL_FIELD_COUNT)
        {
            p101_workspace_audit_add(env, err, result, path, "central ownership contract has an incomplete row");
            continue;
        }
        present = path_set_contains(env, identities, fields[CENTRAL_FIELD_FUNCTION_USR]);
        if(present)
        {
            p101_workspace_audit_add(env, err, result, path, "central ownership contract duplicates a wrapper identity");
            continue;
        }
        added = path_set_add(env, err, identities, fields[CENTRAL_FIELD_FUNCTION_USR]);
        if(!added)
        {
            break;
        }
    }
    close_status = p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    if(close_status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
    collected = p101_error_has_no_error(err);

done:
    return collected;
}

static bool validate_domain(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *domain, const char *repository, bool source_is_namespaced, const char *central_text,
                            struct path_set *local_identities, struct p101_workspace_audit_result *result)
{
    char            repo_relative[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            repo_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            manifest_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            config_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            line[LAYOUT_LINE_SIZE];
    char           *fields[LAYOUT_FIELD_COUNT];
    size_t          field_count;
    FILE           *stream;
    struct path_set expected_sources;
    struct path_set expected_headers;
    struct path_set actual_sources;
    struct path_set actual_headers;
    char           *config_text;
    size_t          config_length;
    char            expected_source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            native_source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            expected_header[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            expected_current_source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            expected_current_header[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char            central_row[LAYOUT_LINE_SIZE];
    char            message[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    const char     *match;
    size_t          index;
    bool            joined;
    bool            derived;
    bool            present;
    bool            added;
    bool            valid;
    int             written;
    int             comparison;
    int             close_status;

    path_set_init(&expected_sources);
    path_set_init(&expected_headers);
    path_set_init(&actual_sources);
    path_set_init(&actual_headers);
    config_text = NULL;
    valid       = false;
    written     = p101_snprintf(env, err, repo_relative, sizeof(repo_relative), "libraries/lib_%s", repository);
    if(written < 0 || (size_t)written >= sizeof(repo_relative))
    {
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, repo_path, sizeof(repo_path), options->workspace, repo_relative);
    if(!joined)
    {
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, manifest_path, sizeof(manifest_path), repo_path, "api-manifest.tsv");
    if(!joined)
    {
        goto done;
    }
    stream = p101_fopen(env, err, manifest_path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    p101_fgets(env, err, line, sizeof(line), stream);
    while(true)
    {
        const char *read_result;

        read_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(read_result == NULL)
        {
            break;
        }
        field_count = split_fields(line, fields, LAYOUT_FIELD_COUNT);
        if(field_count != LAYOUT_FIELD_COUNT)
        {
            p101_workspace_audit_add(env, err, result, manifest_path, "api manifest has an incomplete row");
            continue;
        }
        written = p101_snprintf(env, err, central_row, sizeof(central_row), "%s\t%s\t%s\t", fields[FIELD_FUNCTION], fields[FIELD_FUNCTION_USR], domain);
        if(written < 0 || (size_t)written >= sizeof(central_row))
        {
            break;
        }
        match = p101_strstr(env, central_text, central_row);
        if(match == NULL)
        {
            continue;
        }
        derived = derive_layout(env, err, domain, fields[FIELD_ORIGINAL_HEADER], native_source, sizeof(native_source), expected_header, sizeof(expected_header));
        if(!derived)
        {
            break;
        }
        if(source_is_namespaced)
        {
            written = p101_snprintf(env, err, expected_source, sizeof(expected_source), "src/p101_%s/%s", domain, native_source + sizeof("src/") - 1U);
        }
        else
        {
            written = p101_snprintf(env, err, expected_source, sizeof(expected_source), "%s", native_source);
        }
        if(written < 0 || (size_t)written >= sizeof(expected_source))
        {
            break;
        }
        added = path_set_add(env, err, &expected_sources, expected_source);
        if(added)
        {
            added = path_set_add(env, err, &expected_headers, expected_header);
        }
        if(!added)
        {
            break;
        }
        present = path_set_contains(env, local_identities, fields[FIELD_FUNCTION_USR]);
        if(present)
        {
            written = p101_snprintf(env, err, message, sizeof(message), "%s is owned by more than one functional library", fields[FIELD_FUNCTION_USR]);
            if(written >= 0 && (size_t)written < sizeof(message))
            {
                p101_workspace_audit_add(env, err, result, manifest_path, message);
            }
        }
        else
        {
            path_set_add(env, err, local_identities, fields[FIELD_FUNCTION_USR]);
        }
        if(p101_strcmp(env, fields[FIELD_LINUX], "yes") != 0 || p101_strcmp(env, fields[FIELD_MACOS], "yes") != 0 || p101_strcmp(env, fields[FIELD_FREEBSD], "yes") != 0)
        {
            written = p101_snprintf(env, err, message, sizeof(message), "%s is not admitted on all three platforms", fields[FIELD_FUNCTION]);
            if(written >= 0 && (size_t)written < sizeof(message))
            {
                p101_workspace_audit_add(env, err, result, manifest_path, message);
            }
        }
        written = p101_snprintf(env, err, expected_current_source, sizeof(expected_current_source), "libraries/lib_%s/%s", repository, expected_source);
        written = written < 0 ? written : p101_snprintf(env, err, expected_current_header, sizeof(expected_current_header), "libraries/lib_%s/%s", repository, expected_header);
        if(written < 0)
        {
            break;
        }
        comparison = p101_strcmp(env, fields[FIELD_CURRENT_SOURCE], expected_current_source);
        if(comparison != 0)
        {
            p101_workspace_audit_add(env, err, result, manifest_path, "current_source is not native-shaped");
        }
        comparison = p101_strcmp(env, fields[FIELD_CURRENT_HEADER], expected_current_header);
        if(comparison != 0)
        {
            p101_workspace_audit_add(env, err, result, manifest_path, "current_header is not native-shaped");
        }
        written = p101_snprintf(env, err, central_row, sizeof(central_row), "%s\t%s\t%s\t%s\t%s\t%s\t", fields[FIELD_FUNCTION], fields[FIELD_FUNCTION_USR], domain, fields[FIELD_PROVENANCE], fields[FIELD_CURRENT_SOURCE], fields[FIELD_CURRENT_HEADER]);
        if(written >= 0 && (size_t)written < sizeof(central_row))
        {
            match = p101_strstr(env, central_text, central_row);
            if(match == NULL)
            {
                p101_workspace_audit_add(env, err, result, manifest_path, "central/per-library ownership drift");
            }
        }
        result->checks++;
    }
    close_status = p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    if(close_status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
    if(p101_error_has_error(err))
    {
        goto done;
    }
    joined = collect_files(env, err, repo_path, "src", ".c", &actual_sources);
    if(joined)
    {
        joined = collect_files(env, err, repo_path, "include", ".h", &actual_headers);
    }
    if(!joined)
    {
        goto done;
    }
    for(index = 0U; index < expected_sources.count; index++)
    {
        present = path_set_contains(env, &actual_sources, expected_sources.items[index]);
        if(!present)
        {
            p101_workspace_audit_add(env, err, result, repo_path, "native source layout drift");
        }
    }
    for(index = 0U; index < expected_headers.count; index++)
    {
        present = path_set_contains(env, &actual_headers, expected_headers.items[index]);
        if(!present)
        {
            p101_workspace_audit_add(env, err, result, repo_path, "native header layout drift");
        }
    }
    joined = p101_workspace_audit_join(env, err, config_path, sizeof(config_path), repo_path, "config.cmake");
    if(!joined)
    {
        goto done;
    }
    joined = p101_workspace_audit_read_file(env, err, config_path, &config_text, &config_length);
    if(!joined)
    {
        goto done;
    }
    (void)config_length;
    written = p101_snprintf(env, err, message, sizeof(message), "p101_%s_SOURCES", domain);
    if(written < 0 || (size_t)written >= sizeof(message))
    {
        goto done;
    }
    present = cmake_set_matches(env, config_text, message, &expected_sources);
    if(!present)
    {
        p101_workspace_audit_add(env, err, result, config_path, "source list does not match native layout");
    }
    written = p101_snprintf(env, err, message, sizeof(message), "p101_%s_HEADERS", domain);
    if(written < 0 || (size_t)written >= sizeof(message))
    {
        goto done;
    }
    present = cmake_set_matches(env, config_text, message, &expected_headers);
    if(!present)
    {
        p101_workspace_audit_add(env, err, result, config_path, "header list does not match native layout");
    }
    for(index = 0U; index < sizeof(retired) / sizeof(retired[0]); index++)
    {
        written = p101_snprintf(env, err, message, sizeof(message), "src/%s", retired[index].source_directory);
        if(written >= 0 && (size_t)written < sizeof(message))
        {
            char retired_path[P101_WORKSPACE_AUDIT_PATH_SIZE];

            joined = p101_workspace_audit_join(env, err, retired_path, sizeof(retired_path), repo_path, message);
            if(joined)
            {
                present = p101_workspace_audit_file_exists(env, err, retired_path);
                if(present)
                {
                    p101_workspace_audit_add(env, err, result, retired_path, "obsolete implementation origin directory");
                }
            }
        }
    }
    valid = p101_error_has_no_error(err);

done:
    p101_free(env, config_text);
    path_set_destroy(env, &expected_sources);
    path_set_destroy(env, &expected_headers);
    path_set_destroy(env, &actual_sources);
    path_set_destroy(env, &actual_headers);
    return valid;
}

static bool cmake_set_matches(const struct p101_env *env, const char *text, const char *variable, const struct path_set *expected)
{
    char        needle[LAYOUT_NEEDLE_SIZE];
    char        token[P101_WORKSPACE_AUDIT_PATH_SIZE];
    const char *cursor;
    const char *token_start;
    size_t      token_size;
    size_t      count;
    int         written;
    bool        present;
    bool        matches;

    written = p101_snprintf(env, P101_ERROR_OPTIONAL, needle, sizeof(needle), "set(%s", variable);
    matches = (written >= 0 && (size_t)written < sizeof(needle)) != 0;
    if(!matches)
    {
        goto done;
    }
    cursor = p101_strstr(env, text, needle);
    if(cursor == NULL)
    {
        matches = false;
        goto done;
    }
    cursor += (size_t)written;
    count = 0U;
    while(*cursor != '\0' && *cursor != ')')
    {
        while(*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
        {
            cursor++;
        }
        if(*cursor == '\0' || *cursor == ')')
        {
            break;
        }
        token_start = cursor;
        while(*cursor != '\0' && *cursor != ')' && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r')
        {
            cursor++;
        }
        token_size = (size_t)(cursor - token_start);
        if(token_size == 0U || token_size >= sizeof(token))
        {
            matches = false;
            break;
        }
        p101_memcpy(env, token, token_start, token_size);
        token[token_size] = '\0';
        present           = path_set_contains(env, expected, token);
        if(!present)
        {
            matches = false;
            break;
        }
        count++;
    }
    if(*cursor != ')' || count != expected->count)
    {
        matches = false;
    }

done:
    return matches;
}

static bool scan_retired_references(const struct p101_env *env, struct p101_error *err, const char *root, const char *relative, struct p101_workspace_audit_result *result)    // NOLINT(misc-no-recursion): bounded workspace tree walk.
{
    char                 directory_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                 child_relative[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                 child_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    DIR                 *directory;
    const struct dirent *entry;
    bool                 joined;
    bool                 scanned;
    bool                 nested;
    bool                 inspect;
    bool                 is_directory;
    char                *text;
    size_t               length;
    const char          *match;
    int                  comparison;
    int                  close_status;
    int                  stat_status;
    struct stat          status_buffer;

    scanned = false;
    joined  = p101_workspace_audit_join(env, err, directory_path, sizeof(directory_path), root, relative);
    if(!joined)
    {
        goto done;
    }
    directory = p101_opendir(env, err, directory_path);
    if(directory == NULL)
    {
        if(p101_error_is_errno(err, ENOENT))
        {
            p101_error_reset(err);
            scanned = true;
        }
        goto done;
    }
    while(true)
    {
        entry = p101_readdir(env, err, directory);
        if(entry == NULL)
        {
            break;
        }
        comparison = p101_strcmp(env, entry->d_name, ".");
        if(comparison == 0)
        {
            continue;
        }
        comparison = p101_strcmp(env, entry->d_name, "..");
        if(comparison == 0)
        {
            continue;
        }
        if(entry->d_name[0] == '.' || p101_strncmp(env, entry->d_name, "build", sizeof("build") - 1U) == 0)
        {
            continue;
        }
        joined = p101_workspace_audit_join(env, err, child_relative, sizeof(child_relative), relative, entry->d_name);
        if(!joined)
        {
            break;
        }
        joined = p101_workspace_audit_join(env, err, child_path, sizeof(child_path), root, child_relative);
        if(!joined)
        {
            break;
        }
        stat_status = p101_lstat(env, err, child_path, &status_buffer);
        if(stat_status != 0)
        {
            break;
        }
        is_directory = S_ISDIR(status_buffer.st_mode);
        if(is_directory)
        {
            comparison = p101_strcmp(env, entry->d_name, "design");
            if(comparison != 0)
            {
                nested = scan_retired_references(env, err, root, child_relative, result);
                if(!nested)
                {
                    break;
                }
            }
            continue;
        }
        inspect = should_scan_text(env, entry->d_name);
        if(!inspect)
        {
            continue;
        }
        text   = NULL;
        joined = p101_workspace_audit_read_file(env, err, child_path, &text, &length);
        if(!joined)
        {
            break;
        }
        (void)length;
        match = p101_strstr(env,
                            text,
                            "p101_"
                            "posix/");
        if(match == NULL)
        {
            match = p101_strstr(env,
                                text,
                                "p101_"
                                "posix_optional/");
        }
        if(match == NULL)
        {
            match = p101_strstr(env,
                                text,
                                "p101_"
                                "posix_xsi/");
        }
        if(match == NULL)
        {
            match = p101_strstr(env,
                                text,
                                "p101_"
                                "unix/");
        }
        if(match != NULL)
        {
            p101_workspace_audit_add(env, err, result, child_path, "retired public include");
        }
        comparison = p101_strcmp(env, entry->d_name, "config.cmake");
        if(comparison != 0)
        {
            comparison = p101_strcmp(env, entry->d_name, "CMakeLists.txt");
        }
        if(comparison == 0)
        {
            inspect = text_has_retired_target(env, text);
            if(inspect)
            {
                p101_workspace_audit_add(env, err, result, child_path, "retired link target");
            }
        }
        p101_free(env, text);
    }
    close_status = p101_closedir(env, P101_ERROR_OPTIONAL, directory);
    if(close_status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
    scanned = p101_error_has_no_error(err);

done:
    return scanned;
}

static bool should_scan_text(const struct p101_env *env, const char *name)
{
    const char *extension;
    bool        scan;

    extension = p101_strrchr(env, name, '.');
    scan      = false;
    if(extension != NULL)
    {
        int comparison;

        comparison = p101_strcmp(env, extension, ".c");
        scan       = comparison == 0;
        if(!scan)
        {
            comparison = p101_strcmp(env, extension, ".h");
            scan       = comparison == 0;
        }
        if(!scan)
        {
            comparison = p101_strcmp(env, extension, ".cpp");
            scan       = comparison == 0;
        }
        if(!scan)
        {
            comparison = p101_strcmp(env, extension, ".cmake");
            scan       = comparison == 0;
        }
        if(!scan)
        {
            comparison = p101_strcmp(env, extension, ".txt");
            scan       = comparison == 0;
        }
    }
    return scan;
}

static bool text_has_retired_target(const struct p101_env *env, const char *text)
{
    static const char *const targets[] = {"p101_posix", "p101_posix_optional", "p101_posix_xsi", "p101_unix"};
    const char              *cursor;
    const char              *line_end;
    size_t                   index;
    size_t                   target_size;
    int                      comparison;
    bool                     found;

    cursor = text;
    found  = false;
    while(*cursor != '\0' && !found)
    {
        const char *trimmed_end;
        size_t      line_size;

        while(*cursor == ' ' || *cursor == '\t')
        {
            cursor++;
        }
        line_end = cursor;
        while(*line_end != '\0' && *line_end != '\n' && *line_end != '\r')
        {
            line_end++;
        }
        trimmed_end = line_end;
        while(trimmed_end > cursor && (trimmed_end[-1] == ' ' || trimmed_end[-1] == '\t'))
        {
            trimmed_end--;
        }
        line_size = (size_t)(trimmed_end - cursor);
        for(index = 0U; index < sizeof(targets) / sizeof(targets[0]) && !found; index++)
        {
            target_size = p101_strlen(env, targets[index]);
            comparison  = -1;
            if(line_size == target_size)
            {
                comparison = p101_strncmp(env, cursor, targets[index], target_size);
            }
            found = comparison == 0;
        }
        cursor = line_end;
        while(*cursor == '\n' || *cursor == '\r')
        {
            cursor++;
        }
    }
    return found;
}
