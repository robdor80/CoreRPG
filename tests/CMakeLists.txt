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
    content_compatibility_test.cpp
    content_catalog_test.cpp
    content_pack_validation_test.cpp
    content_manifest_test.cpp
    content_id_test.cpp
    random_test.cpp
    roll_action_test.cpp
    smoke_test.cpp
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

if(CORERPG_BUILD_CONTENT_JSON)
    add_executable(
        corerpg_content_json_tests
        content_json_document_decoder_test.cpp
        content_json_document_reader_test.cpp
        content_json_schema_validator_test.cpp
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
