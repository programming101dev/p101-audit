#include "arguments.h"
#include "cli.h"
#include "constants.h"
#include "fact_loader.h"
#include "model.h"
#include "model_mutation.h"
#include "model_notes.h"
#include "model_query.h"
#include "report.h"
#include "strings.h"
#include "unity.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>
#include <p101_io/p101_aio.h>
#include <p101_io/p101_fcntl.h>
#include <p101_io/p101_poll.h>
#include <p101_io/p101_stdio.h>
#include <p101_io/p101_unistd.h>
#include <p101_io/sys/p101_select.h>
#include <p101_io/sys/p101_uio.h>
#include <p101_process/p101_sched.h>
#include <p101_process/p101_setjmp.h>
#include <p101_process/p101_signal.h>
#include <p101_process/p101_spawn.h>
#include <p101_process/p101_stdio.h>
#include <p101_process/p101_stdlib.h>
#include <p101_process/p101_unistd.h>
#include <p101_process/sys/p101_resource.h>
#include <p101_process/sys/p101_times.h>
#include <p101_process/sys/p101_wait.h>
#include <unistd.h>

static struct p101_error *more_error;
static struct p101_env   *more_env;
struct source_file       *p101_module_map_test_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);
void                      p101_module_map_test_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);
bool                      p101_module_map_test_fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line);
bool                      p101_module_map_test_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name);
bool                      p101_module_map_test_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target);
void                      p101_module_map_test_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
void                      p101_module_map_test_write_finding(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, bool *first_json, size_t *finding_count, const char *path);

static void reset_error(void)
{
    if(p101_error_has_error(more_error))
    {
        p101_error_reset(more_error);
    }
}

static void test_strings_edges(void)
{
    char buffer[8];
    char blank[] = "   ";
    char trim[]  = " \t text \n";

    buffer[0] = 'x';
    p101_module_map_copy_string(more_env, buffer, 0U, "value");
    TEST_ASSERT_EQUAL_CHAR('x', buffer[0]);
    p101_module_map_copy_string(more_env, buffer, sizeof(buffer), "123456789");
    TEST_ASSERT_EQUAL_STRING("1234567", buffer);
    p101_module_map_copy_string(more_env, buffer, sizeof(buffer), "1234567");
    p101_module_map_append_string(more_env, buffer, sizeof(buffer), "x");
    TEST_ASSERT_EQUAL_STRING("1234567", buffer);
    p101_module_map_basename_no_suffix(more_env, buffer, sizeof(buffer), "p101_verylongname.c");
    TEST_ASSERT_EQUAL_STRING("verylon", buffer);
    p101_module_map_include_to_module(more_env, buffer, sizeof(buffer), "@chosen");
    TEST_ASSERT_EQUAL_STRING("chosen", buffer);
    p101_module_map_include_to_module(more_env, buffer, sizeof(buffer), "plain.h");
    TEST_ASSERT_EQUAL_STRING("plain", buffer);
    p101_module_map_include_to_module(more_env, buffer, sizeof(buffer), "p101_group/item.h");
    TEST_ASSERT_EQUAL_STRING("item", buffer);
    p101_module_map_include_to_module(more_env, buffer, sizeof(buffer), "longgroup/item.h");
    TEST_ASSERT_EQUAL_STRING("longgro", buffer);
    p101_module_map_normalize_module_name(more_env, buffer, 2U, "long/p101_name.h");
    TEST_ASSERT_EQUAL_STRING("l", buffer);
    TEST_ASSERT_EQUAL_STRING("", p101_module_map_trim_left(more_env, blank));
    TEST_ASSERT_EQUAL_STRING("text \n", p101_module_map_trim_left(more_env, trim));
    p101_module_map_trim_right(more_env, trim);
    TEST_ASSERT_EQUAL_STRING(" \t text", trim);
    TEST_ASSERT_EQUAL_STRING("plain.c", p101_module_map_path_basename(more_env, "plain.c"));
}

static void test_queries_edges(void)
{
    static struct project_map map;
    struct function_record    function;

    p101_memset(more_env, &map, 0, sizeof(map));
    p101_memset(more_env, &function, 0, sizeof(function));
    p101_module_map_copy_string(more_env, map.modules[0].name, sizeof(map.modules[0].name), "target");
    map.module_count = 1U;
    p101_module_map_copy_string(more_env, map.includes[0].from_module, sizeof(map.includes[0].from_module), "same");
    p101_module_map_copy_string(more_env, map.includes[0].target, sizeof(map.includes[0].target), "other");
    map.includes[0].is_local = false;
    p101_module_map_copy_string(more_env, map.includes[1].from_module, sizeof(map.includes[1].from_module), "same");
    p101_module_map_copy_string(more_env, map.includes[1].target, sizeof(map.includes[1].target), "target");
    map.includes[1].is_local = true;
    map.include_count        = 2U;
    p101_module_map_copy_string(more_env, map.calls[0].module, sizeof(map.calls[0].module), "same");
    p101_module_map_copy_string(more_env, map.calls[0].name, sizeof(map.calls[0].name), "symbol");
    p101_module_map_copy_string(more_env, map.calls[0].usr, sizeof(map.calls[0].usr), "c:@F@symbol");
    p101_module_map_copy_string(more_env, map.calls[1].module, sizeof(map.calls[1].module), "other");
    p101_module_map_copy_string(more_env, map.calls[1].name, sizeof(map.calls[1].name), "symbol");
    p101_module_map_copy_string(more_env, map.calls[1].usr, sizeof(map.calls[1].usr), "c:@F@symbol");
    map.call_count = 2U;
    p101_module_map_copy_string(more_env, function.module, sizeof(function.module), "same");
    p101_module_map_copy_string(more_env, function.name, sizeof(function.name), "symbol");
    p101_module_map_copy_string(more_env, function.usr, sizeof(function.usr), "c:@F@symbol");

    TEST_ASSERT_TRUE(p101_module_map_module_has_direct_include(more_env, &map, "same", "target"));
    TEST_ASSERT_FALSE(p101_module_map_module_has_direct_include(more_env, &map, "same", "missing"));
    TEST_ASSERT_TRUE(p101_module_map_symbol_used_outside_module(more_env, &map, "same", "symbol"));
    TEST_ASSERT_FALSE(p101_module_map_symbol_used_outside_module(more_env, &map, "same", "missing"));
    TEST_ASSERT_TRUE(p101_module_map_include_target_exists(more_env, &map, "target"));
    TEST_ASSERT_FALSE(p101_module_map_include_target_exists(more_env, &map, "missing"));
    TEST_ASSERT_TRUE(p101_module_map_module_used_outside_module(more_env, &map, "target"));
    TEST_ASSERT_FALSE(p101_module_map_module_used_outside_module(more_env, &map, "same"));
    TEST_ASSERT_TRUE(p101_module_map_function_used_outside_module(more_env, &map, &function));
    TEST_ASSERT_FALSE(p101_module_map_function_has_header_declaration(more_env, &map, &function));
    TEST_ASSERT_FALSE(p101_module_map_function_has_non_static_definition(more_env, &map, &function));

    map.function_count                     = 3U;
    map.functions[0]                       = function;
    map.functions[0].is_header_declaration = true;
    map.functions[1]                       = function;
    map.functions[1].is_header_declaration = false;
    map.functions[1].is_static             = true;
    map.functions[2]                       = function;
    map.functions[2].is_header_declaration = false;
    map.functions[2].is_static             = false;
    TEST_ASSERT_TRUE(p101_module_map_function_has_header_declaration(more_env, &map, &function));
    TEST_ASSERT_TRUE(p101_module_map_function_has_non_static_definition(more_env, &map, &function));

    p101_module_map_copy_string(more_env, map.functions[0].usr, sizeof(map.functions[0].usr), "c:@F@other");
    TEST_ASSERT_FALSE(p101_module_map_function_has_header_declaration(more_env, &map, &function));
    p101_module_map_copy_string(more_env, map.functions[0].usr, sizeof(map.functions[0].usr), function.usr);
    p101_module_map_copy_string(more_env, map.functions[0].module, sizeof(map.functions[0].module), "other");
    TEST_ASSERT_TRUE(p101_module_map_function_has_header_declaration(more_env, &map, &function));
    map.functions[0].is_header_declaration = false;
    map.functions[0].is_static             = true;
    map.functions[2].is_static             = true;
    map.functions[1].is_static             = false;
    p101_module_map_copy_string(more_env, map.functions[1].usr, sizeof(map.functions[1].usr), "c:@F@other");
    TEST_ASSERT_FALSE(p101_module_map_function_has_non_static_definition(more_env, &map, &function));
    p101_module_map_copy_string(more_env, map.functions[1].usr, sizeof(map.functions[1].usr), function.usr);
    p101_module_map_copy_string(more_env, map.functions[1].module, sizeof(map.functions[1].module), "other");
    TEST_ASSERT_TRUE(p101_module_map_function_has_non_static_definition(more_env, &map, &function));
}

static void test_notes_and_limits(void)
{
    static struct project_map map;
    struct source_file        file;

    p101_memset(more_env, &map, 0, sizeof(map));
    p101_memset(more_env, &file, 0, sizeof(file));
    p101_module_map_copy_string(more_env, file.path, sizeof(file.path), "x.c");
    p101_module_map_copy_string(more_env, file.module, sizeof(file.module), "x");
    p101_module_map_note_error_use(more_env, more_error, &map, &file);
    p101_module_map_note_error_check(more_env, more_error, &map, &file);
    TEST_ASSERT_TRUE(map.modules[0].uses_error_object);
    TEST_ASSERT_TRUE(map.modules[0].checks_error_object);

    map.module_count = MAX_MODULES;
    p101_module_map_copy_string(more_env, file.module, sizeof(file.module), "new");
    p101_module_map_note_error_use(more_env, more_error, &map, &file);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    p101_module_map_note_error_check(more_env, more_error, &map, &file);
    reset_error();
    p101_memset(more_env, &map, 0, sizeof(map));
    map.file_count = MAX_FILES;
    p101_module_map_add_named_source_file(more_env, more_error, &map, "x.c", "x", false);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    map.file_count   = 0U;
    map.module_count = MAX_MODULES;
    p101_module_map_add_named_source_file(more_env, more_error, &map, "x.c", "new", false);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    p101_module_map_copy_string(more_env, file.module, sizeof(file.module), "new");
    p101_module_map_add_include(more_env, more_error, &map, &file, "x.h", "/tmp/x.h", 1U, true);
    reset_error();
    p101_module_map_add_function(more_env, more_error, &map, &file, "f", "c:@F@f", 1U, false, false);
    reset_error();
    p101_module_map_add_macro(more_env, more_error, &map, &file, "M", 1U);
    reset_error();
    p101_module_map_add_type(more_env, more_error, &map, &file, "T", 1U);
    reset_error();

    p101_memset(more_env, &map, 0, sizeof(map));
    p101_module_map_add_source_file(more_env, more_error, &map, "x.c", false);
    file = map.files[0];
#define TEST_LIMIT(field, maximum, call)                                                                                                                                                                                                                           \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        map.field = maximum;                                                                                                                                                                                                                                       \
        call;                                                                                                                                                                                                                                                      \
        TEST_ASSERT_TRUE(p101_error_has_error(more_error));                                                                                                                                                                                                        \
        reset_error();                                                                                                                                                                                                                                             \
        map.field = 0U;                                                                                                                                                                                                                                            \
    } while(0)
    TEST_LIMIT(include_count, MAX_INCLUDES, p101_module_map_add_include(more_env, more_error, &map, &file, "x.h", "/tmp/x.h", 1U, true));
    TEST_LIMIT(function_count, MAX_FUNCTIONS, p101_module_map_add_function(more_env, more_error, &map, &file, "f", "c:@F@f", 1U, false, false));
    TEST_LIMIT(macro_count, MAX_MACROS, p101_module_map_add_macro(more_env, more_error, &map, &file, "M", 1U));
    TEST_LIMIT(type_count, MAX_TYPES, p101_module_map_add_type(more_env, more_error, &map, &file, "T", 1U));
    TEST_LIMIT(call_count, MAX_CALLS, p101_module_map_add_call(more_env, more_error, &map, &file, "f", "c:@F@f", "c:@F@caller", 1U));
#undef TEST_LIMIT
    TEST_ASSERT_EQUAL_UINT64(1U, map.calls_dropped);
}

static void test_argument_edges(void)
{
    struct arguments args;
    char            *many[P101_MODULE_MAP_MAX_PATHS + 3U];

    p101_memset(more_env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(more_env, &args);
    args.output_path = "";
    p101_module_map_check_arguments(more_env, more_error, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    args.output_path       = NULL;
    args.layer_config_path = "";
    p101_module_map_check_arguments(more_env, more_error, &args);
    reset_error();
    args.layer_config_path = NULL;
    args.compile_db_path   = "";
    p101_module_map_check_arguments(more_env, more_error, &args);
    reset_error();
    args.compile_db_path = NULL;
    args.facts_path      = "";
    p101_module_map_check_arguments(more_env, more_error, &args);
    reset_error();
    args.facts_path      = "facts";
    args.compile_db_path = "compile.json";
    p101_module_map_check_arguments(more_env, more_error, &args);
    reset_error();
    args.facts_path      = NULL;
    args.compile_db_path = NULL;
    args.max_functions   = 0U;
    p101_module_map_check_arguments(more_env, more_error, &args);
    reset_error();
    args.max_functions = 1U;
    args.max_public    = 0U;
    p101_module_map_check_arguments(more_env, more_error, &args);
    reset_error();

    for(size_t index = 0U; index < sizeof(many) / sizeof(many[0]); index++)
    {
        many[index] = "path";
    }
    many[0] = "tool";
#ifdef __GLIBC__
    optind = 0;
#else
    {
        extern int optreset;
        optreset = 1;
        optind   = 1;
    }
#endif
    p101_memset(more_env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(more_env, &args);
    p101_module_map_parse_arguments(more_env, more_error, (int)(sizeof(many) / sizeof(many[0])), many, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
}

static void test_fact_loader_internals(void)
{
    static struct project_map map;
    struct p101_c_fact        fact;
    struct source_file       *file;
    FILE                     *stream;
    char                      line[MAX_LINE];

    p101_memset(more_env, &map, 0, sizeof(map));
    p101_memset(more_env, &fact, 0, sizeof(fact));
    TEST_ASSERT_NULL(p101_module_map_test_file_for_fact(more_env, more_error, &map, NULL));
    TEST_ASSERT_NULL(p101_module_map_test_file_for_fact(more_env, more_error, &map, &fact));
    fact.path      = "x.c";
    fact.module    = "x";
    fact.is_header = false;
    file           = p101_module_map_test_file_for_fact(more_env, more_error, &map, &fact);
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_PTR(file, p101_module_map_test_file_for_fact(more_env, more_error, &map, &fact));
    p101_module_map_test_apply_fact(more_env, more_error, &map, NULL);

    fact.value = "value";
    fact.usr   = "c:@F@value";
    for(int kind = P101_C_FACT_KIND_UNKNOWN; kind <= P101_C_FACT_KIND_MACRO; kind++)
    {
        fact.kind = (enum p101_c_fact_kind)kind;
        p101_module_map_test_apply_fact(more_env, more_error, &map, &fact);
    }
    fact.kind = P101_C_FACT_KIND_CALL;
    fact.usr  = NULL;
    p101_module_map_test_apply_fact(more_env, more_error, &map, &fact);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();
    fact.usr   = "c:@F@value";
    fact.kind  = P101_C_FACT_KIND_NOTE;
    fact.value = "ERROR_USE";
    p101_module_map_test_apply_fact(more_env, more_error, &map, &fact);
    fact.value = "ERROR_CHECK";
    p101_module_map_test_apply_fact(more_env, more_error, &map, &fact);
    fact.value = "OTHER";
    p101_module_map_test_apply_fact(more_env, more_error, &map, &fact);
    fact.kind = (enum p101_c_fact_kind)999;
    p101_module_map_test_apply_fact(more_env, more_error, &map, &fact);

    map.file_count = MAX_FILES;
    fact.path      = "new.c";
    p101_module_map_test_apply_fact(more_env, more_error, &map, &fact);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();

    stream = p101_tmpfile(more_env, more_error);
    p101_memset(more_env, line, 'x', sizeof(line));
    line[sizeof(line) - 1U] = '\0';
    p101_fputs(more_env, more_error, "discarded\n", stream);
    (void)p101_fseek(more_env, more_error, stream, 0L, SEEK_SET);
    TEST_ASSERT_FALSE(p101_module_map_test_fact_line_is_complete(more_env, more_error, stream, line));
    p101_memset(more_env, line, 'x', sizeof(line));
    line[sizeof(line) - 2U] = '\n';
    line[sizeof(line) - 1U] = '\0';
    TEST_ASSERT_TRUE(p101_module_map_test_fact_line_is_complete(more_env, more_error, stream, line));
    p101_module_map_copy_string(more_env, line, sizeof(line), "short");
    TEST_ASSERT_TRUE(p101_module_map_test_fact_line_is_complete(more_env, more_error, stream, line));
    P101_ERROR_RAISE_CHECK(more_error);
    p101_memset(more_env, line, 'x', sizeof(line));
    line[sizeof(line) - 1U] = '\0';
    TEST_ASSERT_FALSE(p101_module_map_test_fact_line_is_complete(more_env, more_error, stream, line));
    reset_error();
    p101_fclose(more_env, more_error, stream);

    stream = p101_tmpfile(more_env, more_error);
    for(size_t index = 0U; index < MAX_LINE + 8U; index++)
    {
        p101_fputc(more_env, more_error, 'x', stream);
    }
    p101_fputc(more_env, more_error, '\n', stream);
    (void)p101_fseek(more_env, more_error, stream, 0L, SEEK_SET);
    p101_memset(more_env, line, 'x', sizeof(line));
    line[sizeof(line) - 1U] = '\0';
    TEST_ASSERT_FALSE(p101_module_map_test_fact_line_is_complete(more_env, more_error, stream, line));
    p101_fclose(more_env, more_error, stream);

    {
        struct arguments args;
        char             huge[MAX_COMMAND];

        p101_memset(more_env, &args, 0, sizeof(args));
        p101_memset(more_env, huge, 'x', sizeof(huge));
        huge[sizeof(huge) - 1U] = '\0';
        args.facts_path         = huge;
        p101_module_map_load_clang_facts(more_env, more_error, &map, &args);
        TEST_ASSERT_TRUE(p101_error_has_error(more_error));
        reset_error();
    }
}

static void test_fact_loader_io_failures(void)
{
    static struct project_map map;
    struct p101_error        *fault_error;
    struct p101_env          *fault_env;
    struct arguments          args;
    char                      path[] = "/tmp/audit-modules-facts-XXXXXX";
    FILE                     *stream;
    int                       fd;

    p101_memset(more_env, &args, 0, sizeof(args));
    args.verbose = true;
    p101_setenv(more_env, more_error, "P101_FAULT_CALL", "1", 1);
    p101_setenv(more_env, more_error, "P101_FAULT_NAME", "fprintf", 1);
    fault_error = p101_error_create(false);
    fault_env   = p101_env_create(fault_error, NULL);
    p101_module_map_load_clang_facts(fault_env, fault_error, &map, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(fault_error));
    p101_env_destroy(fault_env);
    p101_error_destroy(fault_error);
    p101_unsetenv(more_env, more_error, "P101_FAULT_CALL");
    p101_unsetenv(more_env, more_error, "P101_FAULT_NAME");

    fd     = p101_mkstemp(more_env, more_error, path);
    stream = p101_fdopen(more_env, more_error, fd, "w");
    p101_fputs(more_env, more_error, "P101FACT\t7\tFILE\tsrc/x.c\tx\t0\t1\n", stream);
    p101_fclose(more_env, more_error, stream);
    p101_memset(more_env, &args, 0, sizeof(args));
    args.facts_path = path;
    p101_setenv(more_env, more_error, "P101_FAULT_CALL", "1", 1);
    p101_setenv(more_env, more_error, "P101_FAULT_NAME", "fclose", 1);
    fault_error = p101_error_create(false);
    fault_env   = p101_env_create(fault_error, NULL);
    p101_module_map_load_clang_facts(fault_env, fault_error, &map, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(fault_error));
    p101_env_destroy(fault_env);
    p101_error_destroy(fault_error);
    p101_unsetenv(more_env, more_error, "P101_FAULT_CALL");
    p101_unsetenv(more_env, more_error, "P101_FAULT_NAME");
    p101_unlink(more_env, more_error, path);
}

static void set_function(struct function_record *function, const char *module, const char *name, bool is_static, bool is_header)
{
    const char *path;

    path = "x.c";
    if(is_header)
    {
        path = "x.h";
    }
    p101_memset(more_env, function, 0, sizeof(*function));
    p101_module_map_copy_string(more_env, function->module, sizeof(function->module), module);
    p101_module_map_copy_string(more_env, function->name, sizeof(function->name), name);
    p101_module_map_copy_string(more_env, function->path, sizeof(function->path), path);
    function->is_static             = is_static;
    function->is_header_declaration = is_header;
}

static void test_report_finding_predicates(void)
{
    static struct project_map map;
    struct arguments          args;
    FILE                     *stream;

    p101_memset(more_env, &map, 0, sizeof(map));
    p101_memset(more_env, &args, 0, sizeof(args));
    args.max_functions = DEFAULT_MAX_FUNCS;
    args.max_public    = DEFAULT_MAX_PUB;

    p101_module_map_copy_string(more_env, map.modules[0].name, sizeof(map.modules[0].name), "utils");
    map.modules[0].function_count = 5U;
    p101_module_map_copy_string(more_env, map.modules[1].name, sizeof(map.modules[1].name), "util");
    map.modules[1].function_count = 5U;
    p101_module_map_copy_string(more_env, map.modules[2].name, sizeof(map.modules[2].name), "util");
    map.modules[2].function_count = 4U;
    p101_module_map_copy_string(more_env, map.modules[3].name, sizeof(map.modules[3].name), "a");
    map.modules[3].source_count = 1U;
    map.module_count            = 4U;

    set_function(&map.functions[0], "a", "main", false, false);
    set_function(&map.functions[1], "a", "header_only", false, true);
    set_function(&map.functions[2], "a", "static_definition", true, false);
    set_function(&map.functions[3], "a", "declared", false, false);
    set_function(&map.functions[4], "a", "declared", false, true);
    set_function(&map.functions[5], "a", "candidate", false, false);
    set_function(&map.functions[6], "a", "externally_used", false, false);
    set_function(&map.functions[7], "main", "main_header", false, true);
    map.function_count = 8U;

    p101_module_map_copy_string(more_env, map.calls[0].module, sizeof(map.calls[0].module), "other");
    p101_module_map_copy_string(more_env, map.calls[0].name, sizeof(map.calls[0].name), "externally_used");
    p101_module_map_copy_string(more_env, map.calls[1].module, sizeof(map.calls[1].module), "other");
    p101_module_map_copy_string(more_env, map.calls[1].name, sizeof(map.calls[1].name), "USED_MACRO");
    map.call_count = 2U;

    p101_module_map_copy_string(more_env, map.macros[0].module, sizeof(map.macros[0].module), "private");
    p101_module_map_copy_string(more_env, map.macros[0].name, sizeof(map.macros[0].name), "USED_MACRO");
    p101_module_map_copy_string(more_env, map.macros[1].module, sizeof(map.macros[1].module), "private");
    p101_module_map_copy_string(more_env, map.macros[1].name, sizeof(map.macros[1].name), "UNUSED_MACRO");
    map.macro_count = 2U;

    stream = p101_tmpfile(more_env, more_error);
    (void)p101_module_map_write_findings(more_env, more_error, stream, &args, &map);
    p101_fclose(more_env, more_error, stream);
}

static void test_report_helper_edges(void)
{
    static struct project_map map;
    struct arguments          args;
    char                      path[] = "/tmp/audit-modules-layers-XXXXXX";
    FILE                     *stream;
    int                       fd;
    bool                      first_json;
    size_t                    finding_count;

    p101_memset(more_env, &map, 0, sizeof(map));
    p101_module_map_copy_string(more_env, map.modules[0].name, sizeof(map.modules[0].name), "empty");
    map.module_count = 1U;
    TEST_ASSERT_FALSE(p101_module_map_test_module_has_source(more_env, &map, "empty"));
    map.modules[0].source_count = 1U;
    TEST_ASSERT_TRUE(p101_module_map_test_module_has_source(more_env, &map, "empty"));
    TEST_ASSERT_FALSE(p101_module_map_test_module_has_source(more_env, &map, "missing"));

    p101_memset(more_env, &args, 0, sizeof(args));
    TEST_ASSERT_TRUE(p101_module_map_test_layer_allows_include(more_env, more_error, &args, "same", "same"));
    TEST_ASSERT_TRUE(p101_module_map_test_layer_allows_include(more_env, more_error, &args, "a", "b"));
    args.layer_config_path = "/missing/p101-layers";
    TEST_ASSERT_FALSE(p101_module_map_test_layer_allows_include(more_env, more_error, &args, "a", "b"));
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    reset_error();

    fd     = p101_mkstemp(more_env, more_error, path);
    stream = p101_fdopen(more_env, more_error, fd, "w");
    p101_fputs(more_env, more_error, "\n# comment\nnot an edge\na -> b\n", stream);
    p101_fclose(more_env, more_error, stream);
    args.layer_config_path = path;
    TEST_ASSERT_TRUE(p101_module_map_test_layer_allows_include(more_env, more_error, &args, "a", "b"));
    TEST_ASSERT_FALSE(p101_module_map_test_layer_allows_include(more_env, more_error, &args, "a", "c"));
    P101_ERROR_RAISE_CHECK(more_error);
    TEST_ASSERT_FALSE(p101_module_map_test_layer_allows_include(more_env, more_error, &args, "a", "c"));
    reset_error();
    p101_unlink(more_env, more_error, path);

    stream            = p101_tmpfile(more_env, more_error);
    map.calls_dropped = 1U;
    args.json         = false;
    (void)p101_module_map_write_report(more_env, more_error, stream, &args, &map);
    p101_module_map_test_write_json_string(more_env, more_error, stream, NULL);
    p101_module_map_test_write_json_string(more_env, more_error, stream, "\"\\\n\r\t\1z");
    first_json    = true;
    finding_count = 0U;
    args.json     = false;
    p101_module_map_test_write_finding(more_env, more_error, stream, &args, &first_json, &finding_count, NULL);
    p101_module_map_test_write_finding(more_env, more_error, stream, &args, &first_json, &finding_count, "");
    p101_module_map_test_write_finding(more_env, more_error, stream, &args, &first_json, &finding_count, "x.c");
    args.json  = true;
    first_json = true;
    p101_module_map_test_write_finding(more_env, more_error, stream, &args, &first_json, &finding_count, "x.c");
    p101_module_map_test_write_finding(more_env, more_error, stream, &args, &first_json, &finding_count, "x.c");
    P101_ERROR_RAISE_CHECK(more_error);
    p101_module_map_test_write_finding(more_env, more_error, stream, &args, &first_json, &finding_count, "x.c");
    reset_error();
    p101_fclose(more_env, more_error, stream);
}

void p101_module_map_run_more_tests(void)
{
    more_error = p101_error_create(false);
    more_env   = p101_env_create(more_error, NULL);
    RUN_TEST(test_strings_edges);
    RUN_TEST(test_queries_edges);
    RUN_TEST(test_notes_and_limits);
    RUN_TEST(test_argument_edges);
    RUN_TEST(test_fact_loader_internals);
    RUN_TEST(test_fact_loader_io_failures);
    RUN_TEST(test_report_helper_edges);
    RUN_TEST(test_report_finding_predicates);
    p101_env_destroy(more_env);
    p101_error_destroy(more_error);
}
