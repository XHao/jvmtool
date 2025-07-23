cmake_minimum_required(VERSION 3.10)
project(jvmtool_native_agent LANGUAGES C CXX)

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

file(GLOB AGENT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB AGENT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB MODULES_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.cpp"
)

add_library(jvmtool-agent SHARED 
    ${AGENT_HEADERS} 
    ${AGENT_SOURCES}
    ${MODULES_SOURCES}
)

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

# Find clang-format
if(ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    if(CLANG_FORMAT_EXE)
        message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")
        
        # Add format target
        add_custom_target(format
            COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${CMAKE_CURRENT_SOURCE_DIR}
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
        set(TIDY_INCLUDE_FLAGS "")
        foreach(include_dir ${JNI_INCLUDE_DIRS})
            set(TIDY_INCLUDE_FLAGS "${TIDY_INCLUDE_FLAGS} -I${include_dir}")
        endforeach()
        message(${TIDY_INCLUDE_FLAGS})
        
        add_custom_target(tidy
            COMMAND ${CLANG_TIDY_EXE} 
                    ${AGENT_HEADERS} 
                    ${AGENT_SOURCES}
                    ${MODULES_SOURCES}
                    --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
                    -- -I${CMAKE_CURRENT_SOURCE_DIR}/include ${TIDY_INCLUDE_FLAGS} -pthread
            COMMENT "Running clang-tidy on project files only"
            VERBATIM
        )
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()

# Find cppcheck
# CppCheck target with basic configuration and exclusions
if(CPPCHECK_FOUND)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE}
            --enable=warning,style,performance,portability
            --std=c++17
            --suppress=missingIncludeSystem
            --suppress=unmatchedSuppression
            --quiet
            --force
            --inline-suppr
            --suppress=missingInclude
            --suppress=syntaxError
            --suppress=unknownMacro
            --suppress=toomanyconfigs
            --suppress=normalCheckLevelMaxBranches
            --suppress=ctuOneDefinitionRuleViolation
            --suppress=checkersReport
            --exclude=build/_deps/
            --exclude=build/CMakeFiles/
            ${AGENT_HEADERS} 
            ${AGENT_SOURCES}
            ${MODULES_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis on project files only"
    )
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