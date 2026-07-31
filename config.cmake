set(PROJECT_NAME "p101-wrapper-audit")
set(PROJECT_VERSION "2.0.0")
set(PROJECT_DESCRIPTION "Programming 101 native wrapper-boundary and C-fact auditor")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(STANDARD_FLAGS
        -D_POSIX_C_SOURCE=200809L
        -D_XOPEN_SOURCE=700
        -Werror
)
set(DARWIN_STANDARD_FLAGS -D_DARWIN_C_SOURCE)
set(LINUX_STANDARD_FLAGS)
set(BSD_STANDARD_FLAGS)

set(EXECUTABLE_TARGETS wrapper_audit c_facts)
set(LIBRARY_TARGETS "")
set(wrapper_audit_OUTPUT_NAME p101-wrapper-audit)
set(c_facts_OUTPUT_NAME p101-c-facts)

set(wrapper_audit_SOURCES
        src/artifacts.c
        src/cli.c
        src/inventory.c
        src/judge.c
        src/main.c
        src/model.c
        src/output.c
)
set(c_facts_SOURCES
        src/artifacts.c
        src/cli.c
        src/facts_main.c
        src/inventory.c
        src/judge.c
        src/model.c
        src/output.c
)
set(wrapper_audit_HEADERS
        include/model.h
        include/output.h
        include/cli.h
)
set(c_facts_HEADERS
        include/model.h
        include/output.h
        include/cli.h
)

set(P101_WRAPPER_AUDIT_LIBRARIES
        p101_error
        p101_env
        p101_tool_event
        p101_c
        p101_c_facts
        p101_filesystem
        p101_io
)
set(wrapper_audit_LINK_LIBRARIES ${P101_WRAPPER_AUDIT_LIBRARIES})
set(c_facts_LINK_LIBRARIES ${P101_WRAPPER_AUDIT_LIBRARIES})
