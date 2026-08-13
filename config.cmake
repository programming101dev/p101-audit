set(PROJECT_NAME "p101-audit")
set(PROJECT_VERSION "3.0.0")
set(PROJECT_DESCRIPTION "Programming 101 semantic audit engines")
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

set(EXECUTABLE_TARGETS audit_wrappers audit_facts audit_errors audit_modules audit_doctor audit_workspace)
set(LIBRARY_TARGETS "")
set(audit_wrappers_OUTPUT_NAME audit-wrappers)
set(audit_facts_OUTPUT_NAME audit-facts)
set(audit_errors_OUTPUT_NAME audit-errors)
set(audit_modules_OUTPUT_NAME audit-modules)
set(audit_doctor_OUTPUT_NAME audit-doctor)
set(audit_workspace_OUTPUT_NAME audit-workspace)

set(audit_wrappers_SOURCES
        src/artifacts.c
        src/instrumentation.c
        src/cli.c
        src/inventory.c
        src/judge.c
        src/judge_support.c
        src/main.c
        src/model.c
        src/output.c
)
set(audit_facts_SOURCES
        src/artifacts.c
        src/instrumentation.c
        src/cli.c
        src/facts_main.c
        src/inventory.c
        src/judge.c
        src/judge_support.c
        src/model.c
        src/output.c
)
set(audit_workspace_SOURCES
        components/workspace/src/common.c
    components/workspace/src/fact_bundle.c
    components/workspace/src/boundaries.c
    components/workspace/src/instrumentation.c
    components/workspace/src/quality_contract.c
    components/workspace/src/sha256.c
    components/workspace/src/wrapper_unit_tests.c
        components/workspace/src/fault_semantics.c
        components/workspace/src/functional_layout.c
        components/workspace/src/json.c
        components/workspace/src/main.c
        components/workspace/src/native_parity.c
        components/workspace/src/source_responsibilities.c
        components/workspace/src/test_inventory.c
        src/instrumentation.c
        src/model.c
        src/output.c
)
set(audit_errors_SOURCES
        components/error-contract/src/cli.c
        components/error-contract/src/contract.c
        components/error-contract/src/contract_event.c
        components/error-contract/src/contract_builder.c
        components/error-contract/src/contract_model.c
        components/error-contract/src/main.c
        components/error-contract/src/native_analysis.c
        components/error-contract/src/report.c
)
set(audit_modules_SOURCES
        components/module-map/src/cli.c
        components/module-map/src/fact_loader.c
        components/module-map/src/idioms.c
        components/module-map/src/idioms_includes.c
        components/module-map/src/main.c
        components/module-map/src/model_mutation.c
        components/module-map/src/model_notes.c
        components/module-map/src/model_query.c
        components/module-map/src/native_analysis.c
        components/module-map/src/report.c
        components/module-map/src/runner.c
        components/module-map/src/strings.c
)
set(audit_doctor_SOURCES
        components/doctor/src/cli.c
        components/doctor/src/main.c
        components/doctor/src/paths.c
        components/doctor/src/report.c
        components/doctor/src/runner.c
        components/doctor/src/source_inputs.c
        components/doctor/src/status.c
)

set(audit_wrappers_HEADERS
        include/instrumentation.h
        include/model.h
        include/output.h
        include/cli.h
)
set(audit_facts_HEADERS
        include/instrumentation.h
        include/model.h
        include/output.h
        include/cli.h
)
file(GLOB audit_errors_HEADERS CONFIGURE_DEPENDS components/error-contract/include/*.h)
file(GLOB audit_modules_HEADERS CONFIGURE_DEPENDS components/module-map/include/*.h)
file(GLOB audit_doctor_HEADERS CONFIGURE_DEPENDS components/doctor/include/*.h)
set(audit_workspace_HEADERS
        include/instrumentation.h
        include/workspace_audit.h
        include/workspace_fact_bundle.h
        include/workspace_sha256.h
        include/workspace_analysis.h
        include/workspace_json.h
)

set(P101_WRAPPER_AUDIT_LIBRARIES
        p101_error
        p101_env
        p101_record
        p101_tool_event
        p101_c
        p101_c_facts
        p101_filesystem
        p101_io
)
set(audit_wrappers_LINK_LIBRARIES ${P101_WRAPPER_AUDIT_LIBRARIES})
set(audit_facts_LINK_LIBRARIES ${P101_WRAPPER_AUDIT_LIBRARIES})

set(P101_AUDIT_POLICY_LIBRARIES
        p101_error
        p101_env
        p101_record
        p101_tool_event
        p101_c
        p101_c_facts
        p101_cli
        p101_filesystem
        p101_io
        p101_process
        p101_convert
        p101_util
        m
)
set(audit_errors_LINK_LIBRARIES ${P101_AUDIT_POLICY_LIBRARIES})
set(audit_modules_LINK_LIBRARIES ${P101_AUDIT_POLICY_LIBRARIES})
set(audit_doctor_LINK_LIBRARIES ${P101_AUDIT_POLICY_LIBRARIES})
set(audit_workspace_LINK_LIBRARIES ${P101_AUDIT_POLICY_LIBRARIES})
