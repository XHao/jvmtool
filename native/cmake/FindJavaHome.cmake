# FindJavaHome.cmake
# CMake module for finding and setting JAVA_HOME
#
# This module attempts to automatically find JAVA_HOME if not set
# Sets the following variables:
#   JAVA_HOME - JDK installation path
#   JAVA_HOME_FOUND - Whether a valid JAVA_HOME was found
#   JAVA_EXECUTABLE - Path to java executable
#   JAVAC_EXECUTABLE - Path to javac executable

include(CMakePrintHelpers)

# Initialize variables
set(JAVA_HOME_FOUND FALSE)
set(JAVA_HOME "")
set(JAVA_EXECUTABLE "")
set(JAVAC_EXECUTABLE "")

# If JAVA_HOME is already set, validate it directly
if(DEFINED ENV{JAVA_HOME} AND EXISTS "$ENV{JAVA_HOME}")
    set(JAVA_HOME "$ENV{JAVA_HOME}")
    cmake_print_variables(JAVA_HOME)
    message(STATUS "Using existing JAVA_HOME: ${JAVA_HOME}")
else()
    message(STATUS "JAVA_HOME not set or invalid, searching for JDK installation...")
    
    # Define JDK search paths for different platforms
    if(APPLE)
        # macOS-specific java_home command
        execute_process(
            COMMAND /usr/libexec/java_home
            OUTPUT_VARIABLE MACOS_JAVA_HOME
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE JAVA_HOME_RESULT
        )
        
        if(JAVA_HOME_RESULT EQUAL 0 AND MACOS_JAVA_HOME AND EXISTS "${MACOS_JAVA_HOME}")
            set(JAVA_HOME "${MACOS_JAVA_HOME}")
            message(STATUS "Found JAVA_HOME via /usr/libexec/java_home: ${JAVA_HOME}")
        else()
            # Common JDK installation paths on macOS
            set(JAVA_HOME_CANDIDATES
                "/Library/Java/JavaVirtualMachines/*/Contents/Home"
                "/System/Library/Frameworks/JavaVM.framework/Home"
                "/opt/homebrew/opt/openjdk/libexec/openjdk.jdk/Contents/Home"
                "/opt/homebrew/opt/openjdk@*/libexec/openjdk.jdk/Contents/Home"
                "/usr/local/opt/openjdk/libexec/openjdk.jdk/Contents/Home"
                "/usr/local/opt/openjdk@*/libexec/openjdk.jdk/Contents/Home"
            )
        endif()
    elseif(UNIX)
        # Common JDK installation paths on Linux
        set(JAVA_HOME_CANDIDATES
            "/usr/lib/jvm/default-java"
            "/usr/lib/jvm/java-*-openjdk*"
            "/usr/lib/jvm/java-*"
            "/opt/jdk*"
            "/opt/java*"
            "/usr/java/latest"
            "/usr/java/default"
            "/usr/local/java"
        )
    elseif(WIN32)
        # Common JDK installation paths on Windows
        set(JAVA_HOME_CANDIDATES
            "C:/Program Files/Java/jdk*"
            "C:/Program Files/OpenJDK/jdk*"
            "C:/Program Files (x86)/Java/jdk*"
            "C:/Program Files/Eclipse Adoptium/jdk*"
            "C:/Java/jdk*"
        )
    endif()
    
    # Search candidate paths
    if(NOT JAVA_HOME AND JAVA_HOME_CANDIDATES)
        foreach(candidate_pattern ${JAVA_HOME_CANDIDATES})
            file(GLOB candidate_paths ${candidate_pattern})
            if(candidate_paths)
                # Sort by version and select the latest
                list(SORT candidate_paths)
                list(REVERSE candidate_paths)
                
                foreach(candidate ${candidate_paths})
                    if(EXISTS "${candidate}" AND IS_DIRECTORY "${candidate}")
                        # Validate this is a valid JDK (contains include/jni.h)
                        if(EXISTS "${candidate}/include/jni.h")
                            set(JAVA_HOME "${candidate}")
                            message(STATUS "Found JAVA_HOME: ${JAVA_HOME}")
                            break()
                        endif()
                    endif()
                endforeach()
                
                if(JAVA_HOME)
                    break()
                endif()
            endif()
        endforeach()
    endif()
endif()

# Validate JAVA_HOME
if(JAVA_HOME)
    # Check if key files exist
    set(JAVA_HOME_VALID TRUE)
    
    # Check JNI header files
    if(NOT EXISTS "${JAVA_HOME}/include/jni.h")
        message(WARNING "JNI header not found in ${JAVA_HOME}/include/jni.h")
        set(JAVA_HOME_VALID FALSE)
    endif()
    
    # Check java executable
    if(WIN32)
        set(JAVA_EXECUTABLE "${JAVA_HOME}/bin/java.exe")
        set(JAVAC_EXECUTABLE "${JAVA_HOME}/bin/javac.exe")
    else()
        set(JAVA_EXECUTABLE "${JAVA_HOME}/bin/java")
        set(JAVAC_EXECUTABLE "${JAVA_HOME}/bin/javac")
    endif()
    
    if(NOT EXISTS "${JAVA_EXECUTABLE}")
        message(WARNING "Java executable not found: ${JAVA_EXECUTABLE}")
        set(JAVA_HOME_VALID FALSE)
    endif()
    
    if(NOT EXISTS "${JAVAC_EXECUTABLE}")
        message(WARNING "Javac executable not found: ${JAVAC_EXECUTABLE}")
        set(JAVA_HOME_VALID FALSE)
    endif()
    
    if(JAVA_HOME_VALID)
        # Set environment variable
        set(ENV{JAVA_HOME} "${JAVA_HOME}")
        set(JAVA_HOME_FOUND TRUE)
        
        # Get Java version information
        execute_process(
            COMMAND "${JAVA_EXECUTABLE}" -version
            ERROR_VARIABLE JAVA_VERSION_OUTPUT
            ERROR_STRIP_TRAILING_WHITESPACE
        )
        
        if(JAVA_VERSION_OUTPUT)
            string(REGEX MATCH "\"([0-9]+\\.?[0-9]*)" JAVA_VERSION_MATCH ${JAVA_VERSION_OUTPUT})
            if(JAVA_VERSION_MATCH)
                set(JAVA_VERSION ${CMAKE_MATCH_1})
                message(STATUS "Java version: ${JAVA_VERSION}")
            endif()
        endif()
        
        message(STATUS "✅ JAVA_HOME validation successful")
        message(STATUS "  JAVA_HOME: ${JAVA_HOME}")
        message(STATUS "  Java executable: ${JAVA_EXECUTABLE}")
        message(STATUS "  Javac executable: ${JAVAC_EXECUTABLE}")
    else()
        set(JAVA_HOME "")
        set(JAVA_HOME_FOUND FALSE)
    endif()
endif()

# If still not found, provide installation suggestions
if(NOT JAVA_HOME_FOUND)
    message(WARNING "❌ JAVA_HOME not found or invalid")
    message(STATUS "")
    message(STATUS "Please install JDK and set JAVA_HOME environment variable, or install using the following commands:")
    
    if(APPLE)
        message(STATUS "macOS:")
        message(STATUS "  brew install openjdk")
        message(STATUS "  echo 'export JAVA_HOME=$(brew --prefix openjdk)/libexec/openjdk.jdk/Contents/Home' >> ~/.zshrc")
        message(STATUS "  Or install a specific version:")
        message(STATUS "  brew install openjdk@11")
    elseif(UNIX)
        message(STATUS "Ubuntu/Debian:")
        message(STATUS "  sudo apt-get update && sudo apt-get install openjdk-11-jdk")
        message(STATUS "  export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64")
        message(STATUS "")
        message(STATUS "CentOS/RHEL:")
        message(STATUS "  sudo yum install java-11-openjdk-devel")
        message(STATUS "  export JAVA_HOME=/usr/lib/jvm/java-11-openjdk")
        message(STATUS "")
        message(STATUS "Fedora:")
        message(STATUS "  sudo dnf install java-11-openjdk-devel")
    elseif(WIN32)
        message(STATUS "Windows:")
        message(STATUS "  Download from Oracle: https://www.oracle.com/java/technologies/downloads/")
        message(STATUS "  Or use package managers:")
        message(STATUS "  choco install openjdk")
        message(STATUS "  scoop install openjdk")
    endif()
    
    message(STATUS "")
    message(STATUS "After installation, please set JAVA_HOME environment variable to point to JDK installation directory")
endif()

# Mark as advanced
mark_as_advanced(JAVA_HOME JAVA_EXECUTABLE JAVAC_EXECUTABLE JAVA_HOME_FOUND)
