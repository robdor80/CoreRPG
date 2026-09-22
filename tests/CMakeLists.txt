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

add_executable(corerpg_smoke_test smoke_test.cpp)

target_link_libraries(
    corerpg_smoke_test
    PRIVATE
        CoreRPG::CoreRPG
        GTest::gtest_main
)

set_target_properties(
    corerpg_smoke_test
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(corerpg_smoke_test PRIVATE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(corerpg_smoke_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(corerpg_smoke_test)
