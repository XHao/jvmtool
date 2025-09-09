cmake_minimum_required(VERSION 3.10)
project(jvmtool_native_agent LANGUAGES C CXX)

# Generate compile_commands.json for VS Code
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add custom CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find JAVA_HOME
include(FindJavaHome)
if(NOT JAVA_HOME_FOUND)
    message(FATAL_ERROR "JAVA_HOME not found. Please install JDK or set JAVA_HOME environment variable.")
endif()

# Find JNI headers and libraries
include(FindJNIHeaders)
if(NOT JNI_FOUND)
    message(FATAL_ERROR "JNI not found. Please ensure you have a complete JDK installation.")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${JNI_INCLUDE_DIRS})

file(GLOB_RECURSE AGENT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.h"
)
file(GLOB_RECURSE AGENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
)

add_library(jvmtool-agent SHARED 
    ${AGENT_HEADERS} 
    ${AGENT_SOURCES}
)

# Coverage configuration
if(ENABLE_COVERAGE)
    message(STATUS "Coverage enabled")
    target_compile_options(jvmtool-agent PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_libraries(jvmtool-agent PRIVATE --coverage)
    
    # Add coverage flags to global compile options for test targets
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

set_target_properties(jvmtool-agent PROPERTIES
    OUTPUT_NAME "jvmtool-agent"
    PREFIX ""
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Code formatting and linting tools
option(ENABLE_CLANG_FORMAT "Enable clang-format" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
option(ENABLE_CPPCHECK "Enable cppcheck" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Find clang-format
if(ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    if(CLANG_FORMAT_EXE)
        message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")
        
        # Add format target
        add_custom_target(format
            COMMAND ${CLANG_FORMAT_EXE} -i -style=file 
                    ${AGENT_HEADERS} 
                    ${AGENT_SOURCES}
                    ${MODULES_SOURCES}
            COMMENT "Formatting code with clang-format"
            VERBATIM
        )
        
        # Add format-check target
        add_custom_target(format-check
            COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror -style=file 
                    ${AGENT_HEADERS} 
                    ${AGENT_SOURCES}
                    ${MODULES_SOURCES}
            COMMENT "Checking code format with clang-format"
            VERBATIM
        )
    else()
        message(WARNING "clang-format not found")
    endif()
endif()

# Find clang-tidy
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy
        PATHS 
            /opt/homebrew/bin
            /opt/homebrew/Cellar/llvm/*/bin
            /usr/local/bin
            /usr/bin
        NO_DEFAULT_PATH
    )
    if(NOT CLANG_TIDY_EXE)
        find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    endif()
    
    if(CLANG_TIDY_EXE)
        message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
        
        # Enable clang-tidy for the target
        set_target_properties(jvmtool-agent PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy"
        )
        # Build the include flags for clang-tidy
        set(TIDY_INCLUDE_FLAGS "")
        foreach(include_dir ${JNI_INCLUDE_DIRS})
            list(APPEND TIDY_INCLUDE_FLAGS "-I${include_dir}")
        endforeach()
        
        add_custom_target(tidy
            COMMAND ${CLANG_TIDY_EXE} 
                    ${AGENT_HEADERS} 
                    ${AGENT_SOURCES}
                    ${MODULES_SOURCES}
                    --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
                    -- -I${CMAKE_CURRENT_SOURCE_DIR}/include ${TIDY_INCLUDE_FLAGS} -pthread -xc++
            COMMENT "Running clang-tidy on project files only"
            VERBATIM
        )
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()

# Find cppcheck
if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXECUTABLE NAMES cppcheck)
    if(CPPCHECK_EXECUTABLE)
        message(STATUS "Found cppcheck: ${CPPCHECK_EXECUTABLE}")
        
        # CppCheck target with basic configuration and exclusions
        add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE}
            --enable=warning,style,performance,portability
            --std=c++17
            --language=c++
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --quiet
            --force
            --inline-suppr
            -i build/_deps/
            -i build/CMakeFiles/
            ${AGENT_HEADERS} 
            ${AGENT_SOURCES}
            ${MODULES_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis on project files only"
    )
    else()
        message(WARNING "cppcheck not found")
    endif()
endif()

# Add combined lint target
add_custom_target(lint)
if(TARGET format-check)
    add_dependencies(lint format-check)
endif()
if(TARGET tidy)
    add_dependencies(lint tidy)
endif()
if(TARGET cppcheck)
    add_dependencies(lint cppcheck)
endif()


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform-specific compiler optimization flags
if(APPLE)
    # macOS-specific definitions for ucontext
    add_compile_definitions(_XOPEN_SOURCE=700 _DARWIN_C_SOURCE)
    message(STATUS "Added macOS-specific compile definitions: _XOPEN_SOURCE=700 _DARWIN_C_SOURCE")
endif()

    # GCC/Clang compiler flags (Unix/Linux/macOS)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")
    # Enable additional warnings for GCC/Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")


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