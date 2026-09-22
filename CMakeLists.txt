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

add_library(CoreRPG STATIC)
add_library(CoreRPG::CoreRPG ALIAS CoreRPG)

target_sources(
    CoreRPG
    PRIVATE
        src/content_catalog.cpp
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
            include/corerpg/content_catalog.hpp
            include/corerpg/content_manifest.hpp
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

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
