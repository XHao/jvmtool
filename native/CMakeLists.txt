cmake_minimum_required(VERSION 3.10)
project(jvmtool_native_agent LANGUAGES C CXX)

find_package(JNI REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${JNI_INCLUDE_DIRS})

file(GLOB AGENT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB AGENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cpp"
)

add_library(jvmtool-agent SHARED ${AGENT_HEADERS} ${AGENT_SOURCES})

set_target_properties(jvmtool-agent PROPERTIES
    OUTPUT_NAME "jvmtool-agent"
    PREFIX ""
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")


option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
    add_executable(jvmtool-agent-test ${TEST_SOURCES})
    target_include_directories(jvmtool-agent-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(jvmtool-agent-test PRIVATE jvmtool-agent gtest gtest_main)
    add_test(NAME jvmtool-agent-test COMMAND jvmtool-agent-test)
endif()