#ifndef P101_MODULE_MAP_CONSTANTS_H
#define P101_MODULE_MAP_CONSTANTS_H

enum
{
    EXIT_FINDINGS         = 1,
    EXIT_TROUBLE          = 2,
    MAX_MODULES           = 512,
    MAX_FILES             = 1024,
    MAX_FUNCTIONS         = 4096,
    MAX_INCLUDES          = 4096,
    MAX_MACROS            = 2048,
    MAX_TYPES             = 2048,
    MAX_CALLS             = 16384,
    MAX_NOTES             = 1024,
    MAX_LINE              = 4096,
    MAX_COMMAND           = 32768,
    MAX_SIGNATURE         = 8192,
    MAX_NAME              = 128,
    BUILD_PREFIX_LEN      = 6,
    COVERAGE_PREFIX_LEN   = 9,
    PROFILE_PREFIX_LEN    = 8,
    INCLUDE_DIRECTIVE_LEN = 8,
    DEFINE_DIRECTIVE_LEN  = 7,
    STATIC_KEYWORD_LEN    = 6,
    TYPEDEF_KEYWORD_LEN   = 7,
    STRUCT_KEYWORD_LEN    = 6,
    UNION_KEYWORD_LEN     = 5,
    ENUM_KEYWORD_LEN      = 4,
    WRAPPER_PREFIX_LEN    = 5,
    DEFAULT_MAX_FUNCS     = 12,
    DEFAULT_MAX_PUB       = 8
};

#endif    // P101_MODULE_MAP_CONSTANTS_H
