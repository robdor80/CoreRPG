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
        src/version.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/corerpg/version.hpp
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
