#ifndef P101_MODULE_MAP_MODEL_H
#define P101_MODULE_MAP_MODEL_H

#include "arguments.h"
#include "constants.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

struct source_file
{
    char path[PATH_LEN];
    char module[MAX_NAME];
    bool is_header;
};

struct module
{
    char   name[MAX_NAME];
    char   source_path[PATH_LEN];
    char   header_path[PATH_LEN];
    size_t source_count;
    size_t header_count;
    size_t function_count;
    size_t public_function_count;
    size_t static_function_count;
    size_t header_declaration_count;
    size_t macro_count;
    size_t type_count;
    size_t local_include_count;
    size_t external_include_count;
    bool   uses_error_object;
    bool   checks_error_object;
};

struct function_record
{
    char   name[MAX_NAME];
    char   usr[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_static;
    /*
     * True only for a non-defining declaration that appears in a header, which
     * is the one place a declaration states a module's interface. Both loaders
     * drop non-defining declarations found in .c files, so this never means
     * "some forward declaration somewhere".
     */
    bool is_header_declaration;
};

struct include_record
{
    char   from_module[MAX_NAME];
    char   target[MAX_NAME];
    char   path[PATH_LEN];
    char   resolved[PATH_LEN]; /* Resolved file, empty when unresolved; target stays a module name. */
    size_t line;
    bool   is_local;
};

struct macro_record
{
    char   name[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    /*
     * Whether the declaration sits in a header. The "is this exposed?"
     * rules ask only about a module's interface, and a declaration inside
     * a .c file is already private — reporting it would advise exactly
     * what the code is doing.
     */
    bool is_header;
};

struct type_record
{
    char   name[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    /*
     * Whether the declaration sits in a header. The "is this exposed?"
     * rules ask only about a module's interface, and a declaration inside
     * a .c file is already private — reporting it would advise exactly
     * what the code is doing.
     */
    bool is_header;
};

struct note_record
{
    char   name[MAX_NAME];
    char   symbol[MAX_NAME];
    char   symbol_usr[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_header;
};

struct call_record
{
    char   name[MAX_NAME];
    char   usr[MAX_NAME];
    char   caller_usr[MAX_NAME];
    char   module[MAX_NAME];
    char   path[PATH_LEN];
    size_t line;
    bool   is_header;
};

struct project_map
{
    struct source_file     files[MAX_FILES];
    struct module          modules[MAX_MODULES];
    struct function_record functions[MAX_FUNCTIONS];
    struct include_record  includes[MAX_INCLUDES];
    struct macro_record    macros[MAX_MACROS];
    struct type_record     types[MAX_TYPES];
    struct call_record     calls[MAX_CALLS];
    struct note_record     notes[MAX_NOTES];
    size_t                 file_count;
    size_t                 module_count;
    size_t                 function_count;
    size_t                 include_count;
    size_t                 macro_count;
    size_t                 type_count;
    size_t                 call_count;
    size_t                 note_count;
    size_t                 calls_dropped;
};

#endif    // P101_MODULE_MAP_MODEL_H
