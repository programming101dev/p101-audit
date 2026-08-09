# Standalone Unity test tree for a p101-linked C program.
cmake_minimum_required(VERSION 3.14)
project(p101_tests C)
enable_testing()

if(POLICY CMP0083)
    cmake_policy(SET CMP0083 NEW)
    include(CheckPIESupported)
    check_pie_supported()
endif()

add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/unity")
target_compile_options(unity PRIVATE -w)

option(P101_TEST_COVERAGE "Instrument the code under test" OFF)
set(_COV "")
if (P101_TEST_COVERAGE)
    set(_COV --coverage)
endif ()

set(P101_LIBS p101_error p101_env p101_record p101_tool_event p101_c p101_c_facts p101_cli p101_filesystem p101_io p101_process p101_convert p101_util)
set(P101_PUBLIC_INCLUDE_DIRS "" CACHE STRING "Extra p101 include dirs")
set(P101_PUBLIC_LINK_DIRS "" CACHE STRING "Extra p101 link dirs")
separate_arguments(P101_PUBLIC_INCLUDE_DIRS_LIST NATIVE_COMMAND "${P101_PUBLIC_INCLUDE_DIRS}")
separate_arguments(P101_PUBLIC_LINK_DIRS_LIST NATIVE_COMMAND "${P101_PUBLIC_LINK_DIRS}")
set(_P101_INC_DIRS ${P101_PUBLIC_INCLUDE_DIRS_LIST} /usr/local/include /opt/homebrew/include /opt/local/include)
set(_P101_LIB_DIRS ${P101_PUBLIC_LINK_DIRS_LIST} /usr/local/lib /usr/local/lib64 /opt/homebrew/lib /opt/local/lib)

set(_P101_RESOLVED "")
foreach (_l IN LISTS P101_LIBS)
    unset(_P101_LIB_${_l} CACHE)
    unset(_P101_LIB_${_l})
    find_library(_P101_LIB_${_l} NAMES ${_l} PATHS ${_P101_LIB_DIRS} NO_DEFAULT_PATH)
    if (NOT _P101_LIB_${_l})
        find_library(_P101_LIB_${_l} NAMES ${_l})
    endif ()
    if (_P101_LIB_${_l})
        list(APPEND _P101_RESOLVED "${_P101_LIB_${_l}}")
    else ()
        message(WARNING "p101 library '${_l}' not found in ${_P101_LIB_DIRS} — install the p101 libs or add the path.")
    endif ()
endforeach ()

function(p101_add_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${_P101_INC_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    target_link_libraries(${name} PRIVATE unity ${_P101_RESOLVED} m)
    target_compile_options(${name} PRIVATE ${_COV})
    target_link_options(${name} PRIVATE ${_COV})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

p101_add_test(test_p101_string test_p101_string.c)
p101_add_test(test_contract_model
        test_contract_model.c
        ../src/cli.c
        ../src/contract.c
        ../src/contract_event.c
        ../src/contract_builder.c
        ../src/contract_model.c
        ../src/native_analysis.c
        ../src/report.c)
target_compile_definitions(test_contract_model PRIVATE P101_ERROR_CONTRACT_TESTING=1)

add_executable(p101_error_contract_coverage_cli
        ../src/cli.c
        ../src/contract.c
        ../src/contract_event.c
        ../src/contract_builder.c
        ../src/contract_model.c
        ../src/main.c
        ../src/native_analysis.c
        ../src/report.c)
target_include_directories(p101_error_contract_coverage_cli PRIVATE ${_P101_INC_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(p101_error_contract_coverage_cli PRIVATE ${_P101_RESOLVED} m)
target_compile_options(p101_error_contract_coverage_cli PRIVATE ${_COV})
target_link_options(p101_error_contract_coverage_cli PRIVATE ${_COV})
target_compile_definitions(p101_error_contract_coverage_cli PRIVATE P101_ERROR_CONTRACT_TESTING=1)
add_test(NAME test_cli COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.sh" "$<TARGET_FILE:p101_error_contract_coverage_cli>")
