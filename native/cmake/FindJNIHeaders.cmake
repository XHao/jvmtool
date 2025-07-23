# FindJNIHeaders.cmake
# CMake module for finding JNI headers and libraries
#
# Depends on JAVA_HOME set by FindJavaHome.cmake
# Sets the following variables:
#   JNI_FOUND - Whether JNI was found
#   JNI_INCLUDE_DIRS - JNI header directories
#   JNI_LIBRARIES - JNI library files
#   JVM_LIBRARY - JVM library file path

include(CMakePrintHelpers)

# First try using CMake's built-in FindJNI
find_package(JNI QUIET)

if(JNI_FOUND)
    message(STATUS "✅ JNI found by CMake's find_package")
    cmake_print_variables(JNI_INCLUDE_DIRS JNI_LIBRARIES)
else()
    message(STATUS "JNI not found by find_package, attempting manual detection...")
    
    # Ensure JAVA_HOME is set
    if(NOT JAVA_HOME_FOUND OR NOT JAVA_HOME)
        message(FATAL_ERROR "JAVA_HOME must be set before searching for JNI. Please run FindJavaHome first.")
    endif()
    
    # Manual JNI search
    set(JNI_FOUND FALSE)
    set(JNI_INCLUDE_DIRS "")
    set(JNI_LIBRARIES "")
    set(JVM_LIBRARY "")
    
    # Set platform-specific include directories
    if(APPLE)
        set(JNI_PLATFORM_INCLUDE "darwin")
        set(JVM_LIB_NAMES "libjvm.dylib")
        set(JVM_LIB_PATHS
            "${JAVA_HOME}/lib"
            "${JAVA_HOME}/lib/server"
            "${JAVA_HOME}/jre/lib"
            "${JAVA_HOME}/jre/lib/server"
        )
    elseif(UNIX)
        set(JNI_PLATFORM_INCLUDE "linux")
        set(JVM_LIB_NAMES "libjvm.so")
        set(JVM_LIB_PATHS
            "${JAVA_HOME}/lib/server"
            "${JAVA_HOME}/lib/amd64/server"
            "${JAVA_HOME}/lib/x86_64/server"
            "${JAVA_HOME}/jre/lib/amd64/server"
            "${JAVA_HOME}/jre/lib/x86_64/server"
        )
    elseif(WIN32)
        set(JNI_PLATFORM_INCLUDE "win32")
        set(JVM_LIB_NAMES "jvm.lib" "jvm.dll")
        set(JVM_LIB_PATHS
            "${JAVA_HOME}/lib"
            "${JAVA_HOME}/bin/server"
            "${JAVA_HOME}/jre/bin/server"
        )
    endif()
    
    # Find JNI header files
    set(JNI_INCLUDE_DIRS
        "${JAVA_HOME}/include"
        "${JAVA_HOME}/include/${JNI_PLATFORM_INCLUDE}"
    )
    
    # Verify that the main JNI header file exists
    if(EXISTS "${JAVA_HOME}/include/jni.h")
        message(STATUS "Found jni.h: ${JAVA_HOME}/include/jni.h")
        
        # Verify platform-specific header file, considering possible symbolic links
        set(JNI_MD_PATHS
            "${JAVA_HOME}/include/${JNI_PLATFORM_INCLUDE}/jni_md.h"
            "${JAVA_HOME}/include/jni_md.h"
        )
        
        set(JNI_MD_FOUND FALSE)
        foreach(jni_md_path ${JNI_MD_PATHS})
            if(EXISTS "${jni_md_path}")
                message(STATUS "Found jni_md.h: ${jni_md_path}")
                set(JNI_MD_FOUND TRUE)
                break()
            endif()
        endforeach()
        
        if(JNI_MD_FOUND)
            
            # Find JVM library
            foreach(lib_path ${JVM_LIB_PATHS})
                foreach(lib_name ${JVM_LIB_NAMES})
                    set(potential_lib "${lib_path}/${lib_name}")
                    if(EXISTS "${potential_lib}")
                        set(JVM_LIBRARY "${potential_lib}")
                        message(STATUS "Found JVM library: ${JVM_LIBRARY}")
                        break()
                    endif()
                endforeach()
                if(JVM_LIBRARY)
                    break()
                endif()
            endforeach()
            
            # For some platforms, JVM library is not required (e.g., static linking)
            if(JVM_LIBRARY OR APPLE)
                set(JNI_LIBRARIES "${JVM_LIBRARY}")
                set(JNI_FOUND TRUE)
                message(STATUS "✅ JNI manually detected successfully")
            else()
                message(WARNING "JVM library not found in expected paths")
                cmake_print_variables(JVM_LIB_PATHS JVM_LIB_NAMES)
            endif()
        else()
            message(WARNING "Platform-specific JNI header (jni_md.h) not found in any expected location")
            message(STATUS "Searched paths:")
            foreach(path ${JNI_MD_PATHS})
                message(STATUS "  ${path}")
            endforeach()
        endif()
    else()
        message(WARNING "Main JNI header not found: ${JAVA_HOME}/include/jni.h")
    endif()
endif()

# Final verification
if(JNI_FOUND)
    message(STATUS "✅ JNI configuration complete")
    message(STATUS "  Include directories: ${JNI_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${JNI_LIBRARIES}")
    
    # Verify all include directories exist
    foreach(inc_dir ${JNI_INCLUDE_DIRS})
        if(NOT EXISTS "${inc_dir}")
            message(WARNING "JNI include directory does not exist: ${inc_dir}")
        endif()
    endforeach()
else()
    message(FATAL_ERROR "❌ JNI not found. Please ensure you have a complete JDK installation (not just JRE)")
endif()

# Mark as processed
mark_as_advanced(JNI_INCLUDE_DIRS JNI_LIBRARIES JVM_LIBRARY)
