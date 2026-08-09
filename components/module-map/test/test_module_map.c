#include "cli.h"
#include "constants.h"
#include "model.h"
#include "model_mutation.h"
#include "model_query.h"
#include "strings.h"
#include "unity.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

static struct p101_error *error;
static struct p101_env   *env;
void                      p101_module_map_run_more_tests(void);

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void reset_getopt(void)
{
#ifdef __GLIBC__
    optind = 0;
#else
    extern int optreset;
    optreset = 1;
    optind   = 1;
#endif
}

static void test_parse_accepts_layer_config(void)
{
    char            *argv[] = {"audit-modules", "-l", "layers.txt", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 4, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("layers.txt", args.layer_config_path);
    TEST_ASSERT_EQUAL_STRING("src", args.paths[0]);
}

static void test_parse_accepts_compile_database(void)
{
    char            *argv[] = {"audit-modules", "-C", "build-clang/compile_commands.json", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 4, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("build-clang/compile_commands.json", args.compile_db_path);
    TEST_ASSERT_EQUAL_STRING("src", args.paths[0]);
}

static void test_parse_accepts_library_mode(void)
{
    char            *argv[] = {"audit-modules", "-L", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 3, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_TRUE(args.library_mode);
    TEST_ASSERT_EQUAL_STRING("src", args.paths[0]);
}

static void test_parse_accepts_json_fact_snapshot(void)
{
    char            *argv[] = {"audit-modules", "-d:json", "-i", "facts.tsv", "src", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 5, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_TRUE(args.json);
    TEST_ASSERT_EQUAL_STRING("facts.tsv", args.facts_path);
}

static void test_fact_snapshot_conflicts_with_compile_database(void)
{
    char            *argv[] = {"audit-modules", "-i", "facts.tsv", "-C", "compile_commands.json", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    p101_module_map_arguments_init(env, &args);

    p101_module_map_parse_arguments(env, error, 5, argv, &args);
    p101_module_map_check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_has_error(error));
}

static void test_basename_no_suffix(void)
{
    char name[MAX_NAME];

    p101_module_map_basename_no_suffix(env, name, sizeof(name), "src/server.c");
    TEST_ASSERT_EQUAL_STRING("server", name);

    p101_module_map_include_to_module(env, name, sizeof(name), "p101/foo/bar.h");
    TEST_ASSERT_EQUAL_STRING("foo/bar", name);

    p101_module_map_include_to_module(env, name, sizeof(name), "p101/foo/p101_widget.h");
    TEST_ASSERT_EQUAL_STRING("foo/widget", name);

    p101_module_map_include_to_module(env, name, sizeof(name), "arpa/p101_inet.h");
    TEST_ASSERT_EQUAL_STRING("arpa/inet", name);

    p101_module_map_normalize_module_name(env, name, sizeof(name), "lib_memory/memory");
    TEST_ASSERT_EQUAL_STRING("lib_memory/memory", name);
}

static void test_public_function_requires_used_interface(void)
{
    static struct project_map map;
    struct source_file        source;
    struct source_file        header;
    struct source_file        caller;
    struct function_record   *function;

    p101_memset(env, &map, 0, sizeof(map));
    p101_memset(env, &source, 0, sizeof(source));
    p101_memset(env, &header, 0, sizeof(header));
    p101_memset(env, &caller, 0, sizeof(caller));

    p101_module_map_copy_string(env, source.path, sizeof(source.path), "src/tool.c");
    p101_module_map_copy_string(env, source.module, sizeof(source.module), "tool");
    source.is_header = false;

    p101_module_map_copy_string(env, header.path, sizeof(header.path), "include/tool.h");
    p101_module_map_copy_string(env, header.module, sizeof(header.module), "tool");
    header.is_header = true;

    p101_module_map_copy_string(env, caller.path, sizeof(caller.path), "src/main.c");
    p101_module_map_copy_string(env, caller.module, sizeof(caller.module), "main");
    caller.is_header = false;

    p101_module_map_add_source_file(env, error, &map, source.path, source.is_header);
    p101_module_map_add_source_file(env, error, &map, header.path, header.is_header);
    p101_module_map_add_source_file(env, error, &map, caller.path, caller.is_header);
    p101_module_map_add_function(env, error, &map, &source, "tool_run", "c:@F@tool_run", 10, false, false);
    p101_module_map_add_function(env, error, &map, &header, "tool_run", "c:@F@tool_run", 5, false, true);

    function = &map.functions[0];
    TEST_ASSERT_TRUE(p101_module_map_function_has_header_declaration(env, &map, function));
    TEST_ASSERT_FALSE(p101_module_map_function_used_outside_module(env, &map, function));

    p101_module_map_add_include(env, error, &map, &caller, "tool.h", NULL, 3, true);
    p101_module_map_add_call(env, error, &map, &caller, "tool_run", "c:@F@tool_run", "c:@F@caller_main", 4);
    TEST_ASSERT_TRUE(p101_module_map_function_used_outside_module(env, &map, function));
    TEST_ASSERT_TRUE(p101_module_map_function_has_non_static_definition(env, &map, &map.functions[1]));
}

static void test_explicit_relative_include_resolves_from_source_module(void)
{
    static struct project_map map;
    struct source_file        source;

    p101_memset(env, &map, 0, sizeof(map));
    p101_memset(env, &source, 0, sizeof(source));
    p101_module_map_copy_string(env, source.path, sizeof(source.path), "src/arpa/inet.c");
    p101_module_map_copy_string(env, source.module, sizeof(source.module), "arpa/inet");

    p101_module_map_add_include(env, error, &map, &source, "../p101_posix_internal.h", NULL, 17U, true);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT64(1U, map.include_count);
    TEST_ASSERT_EQUAL_STRING("posix_internal", map.includes[0].target);

    p101_module_map_add_include(env, error, &map, &source, "./p101_local.h", NULL, 18U, true);
    TEST_ASSERT_EQUAL_STRING("arpa/local", map.includes[1].target);

    p101_module_map_copy_string(env, source.module, sizeof(source.module), "one/two/main");
    p101_module_map_add_include(env, error, &map, &source, "../p101_sibling.h", NULL, 19U, true);
    TEST_ASSERT_EQUAL_STRING("one/sibling", map.includes[2].target);

    p101_module_map_copy_string(env, source.module, sizeof(source.module), "main");
    p101_module_map_add_include(env, error, &map, &source, "./p101_local.h", NULL, 20U, true);
    TEST_ASSERT_EQUAL_STRING("local", map.includes[3].target);
    p101_module_map_add_include(env, error, &map, &source, "../p101_outside.h", NULL, 21U, true);
    TEST_ASSERT_EQUAL_STRING("../outside", map.includes[4].target);
    p101_module_map_add_include(env, error, &map, &source, "../../p101_outside.h", NULL, 22U, true);
    TEST_ASSERT_EQUAL_STRING("../../outside", map.includes[5].target);
    p101_module_map_add_include(env, error, &map, &source, "./", NULL, 23U, true);
    TEST_ASSERT_EQUAL_STRING("", map.includes[6].target);
    p101_module_map_add_include(env, error, &map, &source, "./x", NULL, 24U, true);
    TEST_ASSERT_EQUAL_STRING("x", map.includes[7].target);
    p101_module_map_add_include(env, error, &map, &source, "./ab", NULL, 25U, true);
    TEST_ASSERT_EQUAL_STRING("ab", map.includes[8].target);
    p101_module_map_add_include(env, error, &map, &source, "./.x/file.h", NULL, 26U, true);
    TEST_ASSERT_EQUAL_STRING(".x/file", map.includes[9].target);

    p101_module_map_copy_string(env, source.module, sizeof(source.module), "ab/main");
    p101_module_map_add_include(env, error, &map, &source, "../file.h", NULL, 27U, true);
    TEST_ASSERT_EQUAL_STRING("file", map.includes[10].target);
    p101_module_map_copy_string(env, source.module, sizeof(source.module), ".x/main");
    p101_module_map_add_include(env, error, &map, &source, "../file.h", NULL, 28U, true);
    TEST_ASSERT_EQUAL_STRING("file", map.includes[11].target);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_layer_config);
    RUN_TEST(test_parse_accepts_compile_database);
    RUN_TEST(test_parse_accepts_library_mode);
    RUN_TEST(test_parse_accepts_json_fact_snapshot);
    RUN_TEST(test_fact_snapshot_conflicts_with_compile_database);
    RUN_TEST(test_basename_no_suffix);
    RUN_TEST(test_public_function_requires_used_interface);
    RUN_TEST(test_explicit_relative_include_resolves_from_source_module);
    p101_module_map_run_more_tests();
    return UNITY_END();
}
