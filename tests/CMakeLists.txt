include(FetchContent)

set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
set(gtest_force_shared_crt ON)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(googletest)

add_executable(
    corerpg_tests
    action_test.cpp
    action_persistence_test.cpp
    action_determinism_test.cpp
    content_compatibility_test.cpp
    content_catalog_test.cpp
    content_pack_validation_test.cpp
    content_manifest_test.cpp
    content_id_test.cpp
    entity_structure_test.cpp
    inspector_diagnostic_test.cpp
    inspector_integration_test.cpp
    inspector_test.cpp
    knowledge_test.cpp
    knowledge_persistence_test.cpp
    visibility_test.cpp
    context_builder_test.cpp
    context_security_test.cpp
    population_test.cpp
    population_persistence_test.cpp
    persistence_test.cpp
    random_test.cpp
    random_streams_test.cpp
    rule_check_test.cpp
    roll_action_test.cpp
    smoke_test.cpp
    simulation_test.cpp
    world_state_integration_test.cpp
    world_state_test.cpp
)

target_link_libraries(
    corerpg_tests
    PRIVATE
        CoreRPG::CoreRPG
        GTest::gtest_main
)

set_target_properties(
    corerpg_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(corerpg_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(corerpg_tests)

# Keep allocation failure instrumentation isolated from all other test executables.
add_executable(corerpg_world_state_allocation_tests world_state_allocation_test.cpp)
target_link_libraries(
    corerpg_world_state_allocation_tests
    PRIVATE CoreRPG::CoreRPG GTest::gtest_main
)
set_target_properties(
    corerpg_world_state_allocation_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
)
if(MSVC)
    target_compile_options(corerpg_world_state_allocation_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_world_state_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(corerpg_world_state_allocation_tests)

# Persistence failures exercise every staged allocation without instrumenting other suites.
add_executable(corerpg_persistence_allocation_tests persistence_allocation_test.cpp)
target_link_libraries(corerpg_persistence_allocation_tests PRIVATE CoreRPG::CoreRPG GTest::gtest_main)
set_target_properties(corerpg_persistence_allocation_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(corerpg_persistence_allocation_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_persistence_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(corerpg_persistence_allocation_tests)

add_executable(corerpg_entity_structure_allocation_tests entity_structure_allocation_test.cpp)
target_link_libraries(corerpg_entity_structure_allocation_tests PRIVATE CoreRPG::CoreRPG GTest::gtest_main)
set_target_properties(corerpg_entity_structure_allocation_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(corerpg_entity_structure_allocation_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_entity_structure_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(corerpg_entity_structure_allocation_tests)

# Action/step allocation failures must not instrument any other test executable.
add_executable(corerpg_action_allocation_tests action_allocation_test.cpp)
target_link_libraries(corerpg_action_allocation_tests PRIVATE CoreRPG::CoreRPG GTest::gtest_main)
set_target_properties(corerpg_action_allocation_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(corerpg_action_allocation_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_action_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(corerpg_action_allocation_tests)

add_executable(corerpg_knowledge_allocation_tests knowledge_allocation_test.cpp)
target_link_libraries(corerpg_knowledge_allocation_tests PRIVATE CoreRPG::CoreRPG GTest::gtest_main)
set_target_properties(corerpg_knowledge_allocation_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(corerpg_knowledge_allocation_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_knowledge_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(corerpg_knowledge_allocation_tests)

if(CORERPG_BUILD_CONTENT_JSON)
    add_executable(
        corerpg_content_json_tests
        content_catalog_loader_test.cpp
        content_json_document_decoder_test.cpp
        content_json_document_reader_test.cpp
        content_json_schema_validator_test.cpp
        content_pack_loader_test.cpp
    )

    target_link_libraries(
        corerpg_content_json_tests
        PRIVATE
            CoreRPG::ContentJson
            GTest::gtest_main
    )

    target_compile_definitions(
        corerpg_content_json_tests
        PRIVATE CORERPG_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )

    set_target_properties(
        corerpg_content_json_tests
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    if(MSVC)
        target_compile_options(corerpg_content_json_tests PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(corerpg_content_json_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    gtest_discover_tests(corerpg_content_json_tests)
endif()

# Isolate Inspector allocation injection; ordinary queries must never allocate.
add_executable(corerpg_inspector_allocation_tests inspector_allocation_test.cpp)
target_link_libraries(corerpg_inspector_allocation_tests PRIVATE CoreRPG::CoreRPG GTest::gtest_main)
set_target_properties(corerpg_inspector_allocation_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(corerpg_inspector_allocation_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_inspector_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(corerpg_inspector_allocation_tests)

# Compile and execute the neutral Host example in every existing CI configuration.
add_executable(corerpg_context_host_example ../examples/authorized_context_host.cpp)
target_link_libraries(corerpg_context_host_example PRIVATE CoreRPG::CoreRPG)
set_target_properties(corerpg_context_host_example
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
add_test(NAME ContextHostExample COMMAND corerpg_context_host_example)

# Isolated global allocation instrumentation for the entire authorized pipeline.
add_executable(corerpg_context_allocation_tests context_allocation_test.cpp)
target_link_libraries(corerpg_context_allocation_tests PRIVATE CoreRPG::CoreRPG GTest::gtest_main)
set_target_properties(corerpg_context_allocation_tests
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
if(MSVC)
    target_compile_options(corerpg_context_allocation_tests PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_context_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
gtest_discover_tests(corerpg_context_allocation_tests)

# Optional, offline cross-repository test. Production libraries never fetch a game.
set(CORERPG_NIMROEL_SOURCE_DIR "" CACHE PATH "Pinned Nimroel checkout for 4.13 integration tests")
if(CORERPG_NIMROEL_SOURCE_DIR)
    if(NOT CORERPG_BUILD_CONTENT_JSON)
        message(FATAL_ERROR "Nimroel integration requires the optional ContentJson target")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(nimroel_pack "${CORERPG_NIMROEL_SOURCE_DIR}/ContentPacks/treskal-villages/pack")
    set(nimroel_validator "${CORERPG_NIMROEL_SOURCE_DIR}/Tools/CoreRPG413/validate.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${nimroel_validator}"
        --core-root "${PROJECT_SOURCE_DIR}" --source-root "${CORERPG_NIMROEL_SOURCE_DIR}"
        --pack "${nimroel_pack}" RESULT_VARIABLE nimroel_validation)
    if(NOT nimroel_validation EQUAL 0)
        message(FATAL_ERROR "Nimroel source/release preflight failed")
    endif()
    add_test(NAME NimroelReleasePreflight COMMAND "${Python3_EXECUTABLE}" "${nimroel_validator}"
        --core-root "${PROJECT_SOURCE_DIR}" --source-root "${CORERPG_NIMROEL_SOURCE_DIR}"
        --pack "${nimroel_pack}")
    set_tests_properties(NimroelReleasePreflight PROPERTIES FIXTURES_SETUP nimroel_release)
    add_executable(corerpg_nimroel_integration_tests nimroel_content_pack_test.cpp)
    target_link_libraries(corerpg_nimroel_integration_tests
        PRIVATE CoreRPG::ContentJson GTest::gtest_main jsoncons)
    target_compile_features(corerpg_nimroel_integration_tests PRIVATE cxx_std_20)
    target_compile_definitions(corerpg_nimroel_integration_tests
        PRIVATE CORERPG_NIMROEL_PACK_DIR="${nimroel_pack}")
    if(MSVC)
        target_compile_options(corerpg_nimroel_integration_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(corerpg_nimroel_integration_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    gtest_discover_tests(corerpg_nimroel_integration_tests PROPERTIES FIXTURES_REQUIRED nimroel_release)
endif()
