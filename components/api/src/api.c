#include "api.h"
#include <errno.h>
#include <glob.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_io/p101_stdio.h>
#include <p101_record/record.h>
#include <p101_tool_support/diagnostic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

enum
{
    API_MAX_FIELDS              = 32,
    API_TEXT_SIZE               = 512,
    API_PLATFORM_TEXT_SIZE      = 16,
    API_INITIAL_CAPACITY        = 256,
    API_GLOB_PATTERN_SIZE       = 4096,
    API_SNAPSHOT_FIELD_COUNT_V1 = 8,
    API_SNAPSHOT_FIELD_COUNT_V2 = 9,
    API_SNAPSHOT_FIELD_COUNT_V3 = 10,
    API_SNAPSHOT_FIELD_COUNT_V4 = 11,
    API_FIELD_FUNCTION          = 0,
    API_FIELD_USR               = 1,
    API_FIELD_LIBRARY           = 2,
    API_FIELD_PROVENANCE        = 3,
    API_FIELD_HEADER            = 4,
    API_FIELD_LINUX             = 5,
    API_FIELD_MACOS             = 6,
    API_FIELD_FREEBSD           = 7,
    API_FIELD_SIGNATURE         = 8,
    API_FIELD_OWNERSHIP         = 9,
    API_FIELD_KIND              = 10
};

struct api_record
{
    char function[API_TEXT_SIZE];
    char usr[API_TEXT_SIZE];
    char library[API_TEXT_SIZE];
    char provenance[API_TEXT_SIZE];
    char header[API_TEXT_SIZE];
    char linux_support[API_PLATFORM_TEXT_SIZE];
    char macos[API_PLATFORM_TEXT_SIZE];
    char freebsd[API_PLATFORM_TEXT_SIZE];
    char signature[API_TEXT_SIZE];
    char ownership[API_TEXT_SIZE];
    char kind[API_PLATFORM_TEXT_SIZE];
};

struct api_snapshot
{
    struct api_record *records;
    size_t             count;
    size_t             capacity;
};

static void                     api_snapshot_init(struct api_snapshot *snapshot);
static void                     api_snapshot_destroy(const struct p101_env *env, struct api_snapshot *snapshot);
static bool                     api_snapshot_add(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const struct api_record *record);
static bool                     copy_text(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input);
static bool                     load_manifest(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const char *path);
static bool                     load_snapshot(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const char *path);
static bool                     load_semantics(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const char *path);
static void                     sort_records(const struct p101_env *env, struct api_snapshot *snapshot);
static const struct api_record *find_record(const struct p101_env *env, const struct api_snapshot *snapshot, const char *usr);
static int                      report_finding(const struct p101_env *env, struct p101_error *err, const char *identifier, const struct api_record *record, const char *message);
static bool                     semantic_record_from_note(const struct p101_env *env, struct p101_error *err, struct api_record *record, const struct p101_c_fact *fact);

static void api_snapshot_init(struct api_snapshot *snapshot)
{
    snapshot->records  = NULL;
    snapshot->count    = 0U;
    snapshot->capacity = 0U;
}

static void api_snapshot_destroy(const struct p101_env *env, struct api_snapshot *snapshot)
{
    p101_free(env, snapshot->records);
    api_snapshot_init(snapshot);
}

static bool copy_text(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input)
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

static bool api_snapshot_add(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const struct api_record *record)
{
    struct api_record *resized;
    size_t             capacity;
    bool               added;

    added = false;
    if(snapshot->count == snapshot->capacity)
    {
        capacity = snapshot->capacity == 0U ? API_INITIAL_CAPACITY : snapshot->capacity * 2U;
        if(capacity < snapshot->capacity || capacity > SIZE_MAX / sizeof(*snapshot->records))
        {
            P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
            goto done;
        }
        resized = (struct api_record *)p101_realloc(env, err, snapshot->records, capacity * sizeof(*snapshot->records));
        if(resized == NULL)
        {
            goto done;
        }
        snapshot->records  = resized;
        snapshot->capacity = capacity;
    }
    snapshot->records[snapshot->count] = *record;
    snapshot->count++;
    added = true;

done:
    return added;
}

static bool load_manifest(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const char *path)
{
    struct api_record record;
    const char       *library_start;
    const char       *library_end;
    FILE             *stream;
    char             *line;
    size_t            capacity;
    ssize_t           amount;
    size_t            function_field;
    size_t            usr_field;
    size_t            provenance_field;
    size_t            header_field;
    size_t            linux_field;
    size_t            macos_field;
    size_t            freebsd_field;
    size_t            field_count;
    bool              loaded;

    loaded   = false;
    line     = NULL;
    capacity = 0U;
    stream   = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    amount = p101_getline(env, err, &line, &capacity, stream);
    if(amount < 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto close_stream;
    }
    function_field   = SIZE_MAX;
    usr_field        = SIZE_MAX;
    provenance_field = SIZE_MAX;
    header_field     = SIZE_MAX;
    linux_field      = SIZE_MAX;
    macos_field      = SIZE_MAX;
    freebsd_field    = SIZE_MAX;
    {
        char  *cursor;
        size_t index;

        while(amount > 0 && (line[(size_t)amount - 1U] == '\n' || line[(size_t)amount - 1U] == '\r'))
        {
            amount--;
            line[(size_t)amount] = '\0';
        }
        cursor = line;
        for(index = 0U; index < API_MAX_FIELDS && cursor != NULL; index++)
        {
            const char *name;

            name = p101_record_split(&cursor);
            if(name == NULL)
            {
                break;
            }
            if(p101_strcmp(env, name, "function") == 0)
            {
                function_field = index;
            }
            else if(p101_strcmp(env, name, "function_usr") == 0)
            {
                usr_field = index;
            }
            else if(p101_strcmp(env, name, "provenance") == 0)
            {
                provenance_field = index;
            }
            else if(p101_strcmp(env, name, "current_header") == 0)
            {
                header_field = index;
            }
            else if(p101_strcmp(env, name, "linux") == 0)
            {
                linux_field = index;
            }
            else if(p101_strcmp(env, name, "macos") == 0)
            {
                macos_field = index;
            }
            else if(p101_strcmp(env, name, "freebsd") == 0)
            {
                freebsd_field = index;
            }
        }
        field_count = index;
    }
    if(function_field == SIZE_MAX || usr_field == SIZE_MAX)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto close_stream;
    }
    library_start = p101_strstr(env, path, "/libraries/");
    if(library_start == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto close_stream;
    }
    library_start += sizeof("/libraries/") - 1U;
    library_end = p101_strchr(env, library_start, '/');
    if(library_end == NULL || (size_t)(library_end - library_start) >= sizeof(record.library))
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto close_stream;
    }
    for(;;)
    {
        char  *fields[API_MAX_FIELDS];
        char  *cursor;
        size_t index;

        amount = p101_getline(env, err, &line, &capacity, stream);
        if(amount < 0)
        {
            break;
        }
        while(amount > 0 && (line[(size_t)amount - 1U] == '\n' || line[(size_t)amount - 1U] == '\r'))
        {
            amount--;
            line[(size_t)amount] = '\0';
        }
        cursor = line;
        for(index = 0U; index < API_MAX_FIELDS && cursor != NULL; index++)
        {
            fields[index] = p101_record_split(&cursor);
            if(fields[index] == NULL)
            {
                break;
            }
        }
        if(index != field_count || cursor != NULL)
        {
            P101_ERROR_RAISE_ERRNO(err, EINVAL);
            break;
        }
        if(fields[function_field][0] == '\0' || fields[usr_field][0] == '\0')
        {
            continue;
        }
        p101_memset(env, &record, 0, sizeof(record));
        record.ownership[0] = '-';
        record.ownership[1] = '\0';
        copy_text(env, err, record.kind, sizeof(record.kind), "function");
        p101_memcpy(env, record.library, library_start, (size_t)(library_end - library_start));
        if(!copy_text(env, err, record.function, sizeof(record.function), fields[function_field]) || !copy_text(env, err, record.usr, sizeof(record.usr), fields[usr_field]) ||
           !copy_text(env, err, record.provenance, sizeof(record.provenance), provenance_field == SIZE_MAX ? "" : fields[provenance_field]) || !copy_text(env, err, record.header, sizeof(record.header), header_field == SIZE_MAX ? "" : fields[header_field]) ||
           !copy_text(env, err, record.linux_support, sizeof(record.linux_support), linux_field == SIZE_MAX ? "" : fields[linux_field]) || !copy_text(env, err, record.macos, sizeof(record.macos), macos_field == SIZE_MAX ? "" : fields[macos_field]) ||
           !copy_text(env, err, record.freebsd, sizeof(record.freebsd), freebsd_field == SIZE_MAX ? "" : fields[freebsd_field]) || !api_snapshot_add(env, err, snapshot, &record))
        {
            break;
        }
    }
    loaded = p101_error_has_no_error(err);

close_stream:
    p101_fclose(env, P101_ERROR_OPTIONAL, stream);
done:
    p101_free(env, line);
    return loaded;
}

static bool load_semantics(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const char *path)
{
    FILE   *stream;
    char   *line;
    size_t  capacity;
    ssize_t amount;
    bool    loaded;

    line     = NULL;
    capacity = 0U;
    loaded   = false;
    stream   = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    for(;;)
    {
        struct p101_c_fact      fact;
        enum p101_c_fact_status parse_status;

        amount = p101_getline(env, err, &line, &capacity, stream);
        if(amount < 0)
        {
            break;
        }
        parse_status = p101_c_fact_parse_line(env, err, line, &fact);
        if(parse_status == P101_C_FACT_OTHER)
        {
            continue;
        }
        if(parse_status != P101_C_FACT_OK)
        {
            P101_ERROR_RAISE_ERRNO(err, EINVAL);
            goto close_stream;
        }
        if(fact.kind == P101_C_FACT_KIND_FUNCTION && fact.is_header && fact.usr != NULL && fact.type != NULL)
        {
            for(size_t index = 0U; index < snapshot->count; index++)
            {
                struct api_record *record;
                int                comparison;

                record     = &snapshot->records[index];
                comparison = p101_strcmp(env, record->usr, fact.usr);
                if(comparison != 0)
                {
                    continue;
                }
                if(record->signature[0] != '\0')
                {
                    comparison = p101_strcmp(env, record->signature, fact.type);
                    if(comparison != 0)
                    {
                        P101_ERROR_RAISE_ERRNO(err, EINVAL);
                    }
                }
                else
                {
                    copy_text(env, err, record->signature, sizeof(record->signature), fact.type);
                }
                break;
            }
        }
        else if(fact.kind == P101_C_FACT_KIND_NOTE && fact.is_header && fact.caller_usr != NULL && fact.value != NULL)
        {
            static const char role_prefix[] = "SEMANTIC_ROLE:p101:ownership:";
            int               prefix_comparison;

            struct api_record semantic_record;

            if(semantic_record_from_note(env, err, &semantic_record, &fact))
            {
                const struct api_record *existing;

                existing = find_record(env, snapshot, semantic_record.usr);
                if(existing != NULL)
                {
                    if(p101_strcmp(env, existing->signature, semantic_record.signature) != 0 || p101_strcmp(env, existing->kind, semantic_record.kind) != 0)
                    {
                        P101_ERROR_RAISE_ERRNO(err, EINVAL);
                        goto close_stream;
                    }
                    continue;
                }
                if(!api_snapshot_add(env, err, snapshot, &semantic_record))
                {
                    goto close_stream;
                }
                continue;
            }

            prefix_comparison = p101_strncmp(env, fact.value, role_prefix, sizeof(role_prefix) - 1U);
            if(prefix_comparison != 0)
            {
                continue;
            }
            for(size_t index = 0U; index < snapshot->count; index++)
            {
                struct api_record *record;
                int                comparison;

                record     = &snapshot->records[index];
                comparison = p101_strcmp(env, record->usr, fact.caller_usr);
                if(comparison != 0)
                {
                    continue;
                }
                comparison = p101_strcmp(env, record->ownership, "-");
                if(comparison != 0)
                {
                    comparison = p101_strcmp(env, record->ownership, fact.value);
                    if(comparison != 0)
                    {
                        P101_ERROR_RAISE_ERRNO(err, EINVAL);
                    }
                }
                else
                {
                    copy_text(env, err, record->ownership, sizeof(record->ownership), fact.value);
                }
                break;
            }
        }
    }
    for(size_t index = 0U; index < snapshot->count; index++)
    {
        if(snapshot->records[index].signature[0] == '\0')
        {
            P101_ERROR_RAISE_ERRNO(err, ENOENT);
            break;
        }
    }
    loaded = p101_error_has_no_error(err);

close_stream:
    p101_fclose(env, P101_ERROR_OPTIONAL, stream);
done:
    p101_free(env, line);
    return loaded;
}

static bool semantic_record_from_note(const struct p101_env *env, struct p101_error *err, struct api_record *record, const struct p101_c_fact *fact)
{
    static const struct
    {
        const char *prefix;
        const char *kind;
    } contracts[] = {
        {"API_TYPE_LAYOUT:",      "type-layout"},
        {"API_ENUMERATOR_VALUE:", "enum-value" },
        {"API_MACRO_VALUE:",      "macro-value"},
    };

    const char *library_start;
    const char *library_end;
    bool        found;

    found = false;
    for(size_t index = 0U; index < sizeof(contracts) / sizeof(contracts[0]); index++)
    {
        size_t prefix_length;
        int    comparison;

        prefix_length = p101_strlen(env, contracts[index].prefix);
        comparison    = p101_strncmp(env, fact->value, contracts[index].prefix, prefix_length);
        if(comparison != 0)
        {
            continue;
        }
        library_start = p101_strstr(env, fact->path, "/libraries/");
        if(library_start == NULL)
        {
            break;
        }
        library_start += sizeof("/libraries/") - 1U;
        library_end = p101_strchr(env, library_start, '/');
        if(library_end == NULL || (size_t)(library_end - library_start) >= sizeof(record->library))
        {
            P101_ERROR_RAISE_ERRNO(err, EINVAL);
            break;
        }
        p101_memset(env, record, 0, sizeof(*record));
        p101_memcpy(env, record->library, library_start, (size_t)(library_end - library_start));
        record->ownership[0] = '-';
        record->ownership[1] = '\0';
        if(!copy_text(env, err, record->function, sizeof(record->function), fact->caller == NULL ? "?" : fact->caller) || !copy_text(env, err, record->usr, sizeof(record->usr), fact->caller_usr) ||
           !copy_text(env, err, record->provenance, sizeof(record->provenance), "clang-semantic") || !copy_text(env, err, record->header, sizeof(record->header), fact->path) ||
           !copy_text(env, err, record->signature, sizeof(record->signature), fact->value + prefix_length) || !copy_text(env, err, record->kind, sizeof(record->kind), contracts[index].kind))
        {
            break;
        }
        found = true;
        break;
    }
    return found;
}

static void sort_records(const struct p101_env *env, struct api_snapshot *snapshot)
{
    for(size_t outer = 1U; outer < snapshot->count; outer++)
    {
        struct api_record value;
        size_t            inner;

        value = snapshot->records[outer];
        inner = outer;
        while(inner > 0U)
        {
            int comparison;

            comparison = p101_strcmp(env, snapshot->records[inner - 1U].usr, value.usr);
            if(comparison <= 0)
            {
                break;
            }
            snapshot->records[inner] = snapshot->records[inner - 1U];
            inner--;
        }
        snapshot->records[inner] = value;
    }
}

int p101_api_snapshot(const struct p101_env *env, struct p101_error *err, const char *workspace, const char *facts, const char *output)
{
    struct api_snapshot snapshot;
    glob_t              paths;
    FILE               *stream;
    char                pattern[API_GLOB_PATTERN_SIZE];
    size_t              index;
    int                 written;
    int                 glob_status;
    int                 status;

    api_snapshot_init(&snapshot);
    p101_memset(env, &paths, 0, sizeof(paths));
    status  = 2;
    written = p101_snprintf(env, err, pattern, sizeof(pattern), "%s/libraries/lib_*/api-manifest.tsv", workspace);
    if(written < 0 || (size_t)written >= sizeof(pattern))
    {
        goto done;
    }
    glob_status = p101_glob(env, err, pattern, 0, NULL, &paths);
    if(glob_status != 0)
    {
        goto done;
    }
    for(index = 0U; index < paths.gl_pathc; index++)
    {
        if(!load_manifest(env, err, &snapshot, paths.gl_pathv[index]))
        {
            goto done;
        }
    }
    if(snapshot.count == 0U)
    {
        P101_ERROR_RAISE_ERRNO(err, ENOENT);
        goto done;
    }
    if(!load_semantics(env, err, &snapshot, facts))
    {
        goto done;
    }
    sort_records(env, &snapshot);
    for(index = 1U; index < snapshot.count; index++)
    {
        if(p101_strcmp(env, snapshot.records[index - 1U].usr, snapshot.records[index].usr) == 0)
        {
            P101_ERROR_RAISE_ERRNO(err, EEXIST);
            goto done;
        }
    }
    stream = p101_fopen(env, err, output, "w");
    if(stream == NULL)
    {
        goto done;
    }
    p101_fputs(env, err, "P101API\t4\n", stream);
    for(index = 0U; index < snapshot.count; index++)
    {
        const struct api_record *record;

        record = &snapshot.records[index];
        p101_fprintf(env,
                     err,
                     stream,
                     "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                     record->function,
                     record->usr,
                     record->library,
                     record->provenance,
                     record->header,
                     record->linux_support,
                     record->macos,
                     record->freebsd,
                     record->signature,
                     record->ownership,
                     record->kind);
    }
    p101_fclose(env, err, stream);
    if(p101_error_has_no_error(err))
    {
        p101_printf(env, err, "p101 API snapshot: %zu entities\n", snapshot.count);
        status = 0;
    }

done:
    p101_globfree(env, &paths);
    api_snapshot_destroy(env, &snapshot);
    return status;
}

static bool load_snapshot(const struct p101_env *env, struct p101_error *err, struct api_snapshot *snapshot, const char *path)
{
    struct api_record record;
    FILE             *stream;
    char             *line;
    size_t            capacity;
    ssize_t           amount;
    size_t            snapshot_field_count;
    bool              loaded;

    line     = NULL;
    capacity = 0U;
    loaded   = false;
    stream   = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    amount = p101_getline(env, err, &line, &capacity, stream);
    while(amount > 0 && (line[(size_t)amount - 1U] == '\n' || line[(size_t)amount - 1U] == '\r'))
    {
        amount--;
        line[(size_t)amount] = '\0';
    }
    snapshot_field_count = 0U;
    if(amount >= 0 && p101_strcmp(env, line, "P101API\t1") == 0)
    {
        snapshot_field_count = API_SNAPSHOT_FIELD_COUNT_V1;
    }
    else if(amount >= 0 && p101_strcmp(env, line, "P101API\t2") == 0)
    {
        snapshot_field_count = API_SNAPSHOT_FIELD_COUNT_V2;
    }
    else if(amount >= 0 && p101_strcmp(env, line, "P101API\t3") == 0)
    {
        snapshot_field_count = API_SNAPSHOT_FIELD_COUNT_V3;
    }
    else if(amount >= 0 && p101_strcmp(env, line, "P101API\t4") == 0)
    {
        snapshot_field_count = API_SNAPSHOT_FIELD_COUNT_V4;
    }
    if(snapshot_field_count == 0U)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto close_stream;
    }
    for(;;)
    {
        char  *fields[API_SNAPSHOT_FIELD_COUNT_V4];
        char  *cursor;
        size_t index;

        amount = p101_getline(env, err, &line, &capacity, stream);
        if(amount < 0)
        {
            break;
        }
        while(amount > 0 && (line[(size_t)amount - 1U] == '\n' || line[(size_t)amount - 1U] == '\r'))
        {
            amount--;
            line[(size_t)amount] = '\0';
        }
        cursor = line;
        for(index = 0U; index < snapshot_field_count; index++)
        {
            fields[index] = p101_record_split(&cursor);
            if(fields[index] == NULL)
            {
                break;
            }
        }
        if(index != snapshot_field_count || cursor != NULL)
        {
            P101_ERROR_RAISE_ERRNO(err, EINVAL);
            break;
        }
        p101_memset(env, &record, 0, sizeof(record));
        record.ownership[0] = '-';
        record.ownership[1] = '\0';
        copy_text(env, err, record.kind, sizeof(record.kind), "function");
        if(!copy_text(env, err, record.function, sizeof(record.function), fields[API_FIELD_FUNCTION]) || !copy_text(env, err, record.usr, sizeof(record.usr), fields[API_FIELD_USR]) ||
           !copy_text(env, err, record.library, sizeof(record.library), fields[API_FIELD_LIBRARY]) || !copy_text(env, err, record.provenance, sizeof(record.provenance), fields[API_FIELD_PROVENANCE]) ||
           !copy_text(env, err, record.header, sizeof(record.header), fields[API_FIELD_HEADER]) || !copy_text(env, err, record.linux_support, sizeof(record.linux_support), fields[API_FIELD_LINUX]) ||
           !copy_text(env, err, record.macos, sizeof(record.macos), fields[API_FIELD_MACOS]) || !copy_text(env, err, record.freebsd, sizeof(record.freebsd), fields[API_FIELD_FREEBSD]))
        {
            break;
        }
        if(snapshot_field_count == API_SNAPSHOT_FIELD_COUNT_V2 && !copy_text(env, err, record.signature, sizeof(record.signature), fields[API_FIELD_SIGNATURE]))
        {
            break;
        }
        if(snapshot_field_count >= API_SNAPSHOT_FIELD_COUNT_V3 && (!copy_text(env, err, record.signature, sizeof(record.signature), fields[API_FIELD_SIGNATURE]) || !copy_text(env, err, record.ownership, sizeof(record.ownership), fields[API_FIELD_OWNERSHIP])))
        {
            break;
        }
        if(snapshot_field_count == API_SNAPSHOT_FIELD_COUNT_V4 && !copy_text(env, err, record.kind, sizeof(record.kind), fields[API_FIELD_KIND]))
        {
            break;
        }
        if(!api_snapshot_add(env, err, snapshot, &record))
        {
            break;
        }
    }
    loaded = p101_error_has_no_error(err);

close_stream:
    p101_fclose(env, P101_ERROR_OPTIONAL, stream);
done:
    p101_free(env, line);
    return loaded;
}

static const struct api_record *find_record(const struct p101_env *env, const struct api_snapshot *snapshot, const char *usr)
{
    const struct api_record *record;
    size_t                   index;

    record = NULL;
    for(index = 0U; index < snapshot->count; index++)
    {
        if(p101_strcmp(env, snapshot->records[index].usr, usr) == 0)
        {
            record = &snapshot->records[index];
            break;
        }
    }
    return record;
}

static int report_finding(const struct p101_env *env, struct p101_error *err, const char *identifier, const struct api_record *record, const char *message)
{
    struct p101_tool_diagnostic diagnostic;
    char                        detail[API_TEXT_SIZE];
    const char                 *path;
    int                         format_status;
    int                         status;

    path          = record->header[0] == '\0' ? "api-manifest.tsv" : record->header;
    format_status = snprintf(detail, sizeof(detail), "%s: %s", record->function, message);
    if(format_status < 0 || (size_t)format_status >= sizeof(detail))
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        status = -1;
        goto done;
    }
    status = p101_tool_diagnostic_initialize_id(&diagnostic, identifier, P101_TOOL_DIAGNOSTIC_ERROR, path, 1U, 1U, record->function, detail);
    if(status == 0)
    {
        status = p101_tool_diagnostic_write(stderr, P101_TOOL_DIAGNOSTIC_TEXT, &diagnostic);
    }
    if(status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }

done:
    (void)env;
    return status;
}

int p101_api_compare(const struct p101_env *env, struct p101_error *err, const char *old_snapshot, const char *new_snapshot)
{
    struct api_snapshot      old_api;
    struct api_snapshot      new_api;
    const struct api_record *current;
    size_t                   additions;
    size_t                   findings;
    size_t                   index;
    int                      status;

    api_snapshot_init(&old_api);
    api_snapshot_init(&new_api);
    additions = 0U;
    findings  = 0U;
    status    = 2;
    if(!load_snapshot(env, err, &old_api, old_snapshot) || !load_snapshot(env, err, &new_api, new_snapshot))
    {
        goto done;
    }
    for(index = 0U; index < old_api.count; index++)
    {
        const struct api_record *previous;

        previous = &old_api.records[index];
        current  = find_record(env, &new_api, previous->usr);
        if(current == NULL)
        {
            report_finding(env, err, "P101-API-001", previous, "public API was removed");
            findings++;
            continue;
        }
        if(p101_strcmp(env, previous->library, current->library) != 0)
        {
            report_finding(env, err, "P101-API-002", current, "public API moved to another link library");
            findings++;
        }
        else if(previous->header[0] != '\0' && p101_strcmp(env, previous->header, current->header) != 0)
        {
            report_finding(env, err, "P101-API-003", current, "public API moved to another header");
            findings++;
        }
        if(p101_strcmp(env, previous->linux_support, "yes") == 0 && p101_strcmp(env, current->linux_support, "yes") != 0)
        {
            report_finding(env, err, "P101-API-004", current, "public API lost linux support");
            findings++;
        }
        if(p101_strcmp(env, previous->macos, "yes") == 0 && p101_strcmp(env, current->macos, "yes") != 0)
        {
            report_finding(env, err, "P101-API-004", current, "public API lost macos support");
            findings++;
        }
        if(p101_strcmp(env, previous->freebsd, "yes") == 0 && p101_strcmp(env, current->freebsd, "yes") != 0)
        {
            report_finding(env, err, "P101-API-004", current, "public API lost freebsd support");
            findings++;
        }
        if(previous->signature[0] != '\0' && current->signature[0] != '\0' && p101_strcmp(env, previous->signature, current->signature) != 0)
        {
            const char *identifier;
            const char *message;

            identifier = "P101-API-005";
            message    = "public API Clang-observed function type changed";
            if(p101_strcmp(env, previous->kind, "type-layout") == 0)
            {
                identifier = "P101-API-007";
                message    = "public struct or union size/alignment changed on this platform";
            }
            else if(p101_strcmp(env, previous->kind, "enum-value") == 0)
            {
                identifier = "P101-API-008";
                message    = "public enumerator value changed";
            }
            else if(p101_strcmp(env, previous->kind, "macro-value") == 0)
            {
                identifier = "P101-API-009";
                message    = "exported object-like macro value changed";
            }
            report_finding(env, err, identifier, current, message);
            findings++;
        }
        if(p101_strcmp(env, previous->ownership, "-") != 0 && p101_strcmp(env, previous->ownership, current->ownership) != 0)
        {
            report_finding(env, err, "P101-API-006", current, "public API ownership semantic role changed or was removed");
            findings++;
        }
    }
    for(index = 0U; index < new_api.count; index++)
    {
        current = find_record(env, &old_api, new_api.records[index].usr);
        if(current == NULL)
        {
            additions++;
        }
    }
    p101_printf(env, err, "p101 API diff: %zu additions, %zu breaking changes\n", additions, findings);
    if(p101_error_has_no_error(err))
    {
        status = findings == 0U ? 0 : 1;
    }

done:
    api_snapshot_destroy(env, &new_api);
    api_snapshot_destroy(env, &old_api);
    return status;
}
