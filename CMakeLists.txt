cmake_minimum_required(VERSION 3.25)

project(
    CoreRPG
    VERSION 0.1.0
    DESCRIPTION "Portable, engine-agnostic RPG simulation core"
    LANGUAGES CXX
)

if(CMAKE_CONFIGURATION_TYPES AND DEFINED CMAKE_BUILD_TYPE)
    message(VERBOSE "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} is only used by single-config generators")
endif()

include(CTest)

option(
    CORERPG_BUILD_CONTENT_JSON
    "Build the optional CoreRPG JSON content integration library"
    OFF
)

add_library(CoreRPG STATIC)
add_library(CoreRPG::CoreRPG ALIAS CoreRPG)

target_sources(
    CoreRPG
    PRIVATE
        src/content_compatibility.cpp
        src/content_catalog.cpp
        src/content_diagnostic.cpp
        src/content_id.cpp
        src/semantic_version.cpp
        src/version_requirement.cpp
        src/random.cpp
        src/resource_kind.cpp
        src/roll_action.cpp
        src/version.cpp
        src/world_state.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/corerpg/content_compatibility.hpp
            include/corerpg/content_catalog.hpp
            include/corerpg/content_definition_input.hpp
            include/corerpg/content_diagnostic.hpp
            include/corerpg/content_manifest.hpp
            include/corerpg/content_pack_input.hpp
            include/corerpg/definition_reference.hpp
            include/corerpg/definition_id.hpp
            include/corerpg/entity_id.hpp
            include/corerpg/pack_dependency.hpp
            include/corerpg/pack_id.hpp
            include/corerpg/random.hpp
            include/corerpg/resource_definition.hpp
            include/corerpg/resource_kind.hpp
            include/corerpg/roll_action.hpp
            include/corerpg/semantic_version.hpp
            include/corerpg/version.hpp
            include/corerpg/version_requirement.hpp
            include/corerpg/world_state.hpp
)

target_compile_features(CoreRPG PUBLIC cxx_std_20)

set_target_properties(
    CoreRPG
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

target_include_directories(
    CoreRPG
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_definitions(
    CoreRPG
    PRIVATE CORERPG_VERSION_STRING="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(CoreRPG PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(CoreRPG PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CORERPG_BUILD_CONTENT_JSON)
    include(FetchContent)

    set(JSONCONS_BUILD_TESTS OFF CACHE BOOL "Build the jsoncons test suite" FORCE)

    FetchContent_Declare(
        jsoncons
        SYSTEM
        GIT_REPOSITORY https://github.com/danielaparker/jsoncons.git
        # jsoncons v1.9.0
        GIT_TAG bcb44594c50c495ee1e690602cdd71455942ad0e
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(jsoncons)

    add_library(CoreRPGContentJson STATIC)
    add_library(CoreRPG::ContentJson ALIAS CoreRPGContentJson)

    target_sources(
        CoreRPGContentJson
        PRIVATE
            src/content_json_backend.cpp
    )

    target_compile_features(CoreRPGContentJson PRIVATE cxx_std_20)

    set_target_properties(
        CoreRPGContentJson
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    target_link_libraries(
        CoreRPGContentJson
        PUBLIC
            CoreRPG::CoreRPG
        PRIVATE
            jsoncons
    )

    if(MSVC)
        target_compile_options(CoreRPGContentJson PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(CoreRPGContentJson PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
