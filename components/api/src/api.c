#include "api.h"
#include <errno.h>
#include <glob.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_io/p101_stdio.h>
#include <p101_record/record.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

enum
{
    API_MAX_FIELDS           = 32,
    API_TEXT_SIZE            = 512,
    API_PLATFORM_TEXT_SIZE   = 16,
    API_INITIAL_CAPACITY     = 256,
    API_GLOB_PATTERN_SIZE    = 4096,
    API_SNAPSHOT_FIELD_COUNT = 8,
    API_SNAPSHOT_HEADER_SIZE = sizeof("P101API\t1") - 1U,
    API_FIELD_FUNCTION       = 0,
    API_FIELD_USR            = 1,
    API_FIELD_LIBRARY        = 2,
    API_FIELD_PROVENANCE     = 3,
    API_FIELD_HEADER         = 4,
    API_FIELD_LINUX          = 5,
    API_FIELD_MACOS          = 6,
    API_FIELD_FREEBSD        = 7
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
static void                     sort_records(const struct p101_env *env, struct api_snapshot *snapshot);
static const struct api_record *find_record(const struct p101_env *env, const struct api_snapshot *snapshot, const char *usr);
static int                      report_finding(const struct p101_env *env, struct p101_error *err, const char *identifier, const struct api_record *record, const char *message);

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

int p101_api_snapshot(const struct p101_env *env, struct p101_error *err, const char *workspace, const char *output)
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
    p101_fputs(env, err, "P101API\t1\n", stream);
    for(index = 0U; index < snapshot.count; index++)
    {
        const struct api_record *record;

        record = &snapshot.records[index];
        p101_fprintf(env, err, stream, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", record->function, record->usr, record->library, record->provenance, record->header, record->linux_support, record->macos, record->freebsd);
    }
    p101_fclose(env, err, stream);
    if(p101_error_has_no_error(err))
    {
        p101_printf(env, err, "p101 API snapshot: %zu functions\n", snapshot.count);
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
    if(amount < 0 || p101_strncmp(env, line, "P101API\t1", API_SNAPSHOT_HEADER_SIZE) != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto close_stream;
    }
    for(;;)
    {
        char  *fields[API_SNAPSHOT_FIELD_COUNT];
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
        for(index = 0U; index < API_SNAPSHOT_FIELD_COUNT; index++)
        {
            fields[index] = p101_record_split(&cursor);
            if(fields[index] == NULL)
            {
                break;
            }
        }
        if(index != API_SNAPSHOT_FIELD_COUNT || cursor != NULL)
        {
            P101_ERROR_RAISE_ERRNO(err, EINVAL);
            break;
        }
        p101_memset(env, &record, 0, sizeof(record));
        if(!copy_text(env, err, record.function, sizeof(record.function), fields[API_FIELD_FUNCTION]) || !copy_text(env, err, record.usr, sizeof(record.usr), fields[API_FIELD_USR]) ||
           !copy_text(env, err, record.library, sizeof(record.library), fields[API_FIELD_LIBRARY]) || !copy_text(env, err, record.provenance, sizeof(record.provenance), fields[API_FIELD_PROVENANCE]) ||
           !copy_text(env, err, record.header, sizeof(record.header), fields[API_FIELD_HEADER]) || !copy_text(env, err, record.linux_support, sizeof(record.linux_support), fields[API_FIELD_LINUX]) ||
           !copy_text(env, err, record.macos, sizeof(record.macos), fields[API_FIELD_MACOS]) || !copy_text(env, err, record.freebsd, sizeof(record.freebsd), fields[API_FIELD_FREEBSD]) || !api_snapshot_add(env, err, snapshot, &record))
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
    return p101_fprintf(env, err, stderr, "%s:1:1: error: %s: %s [%s]\n", record->header[0] == '\0' ? "api-manifest.tsv" : record->header, record->function, message, identifier);
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
