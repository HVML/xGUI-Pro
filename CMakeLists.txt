# -----------------------------------------------------------------------------
# Determine CMake version and build type.
# -----------------------------------------------------------------------------
#
# NOTE: cmake_minimum_required() and project() *MUST* be the two first commands
# used, see https://cmake.org/cmake/help/v3.3/command/project.html -- the
# latter in particular handles loading a bunch of shared CMake definitions
# and loading the cross-compilation settings from CMAKE_TOOLCHAIN_FILE.
#

cmake_minimum_required(VERSION 3.18)

project(xGUIPro C CXX)

if (NOT CMAKE_BUILD_TYPE)
    message(WARNING "No CMAKE_BUILD_TYPE value specified, defaulting to RelWithDebInfo.")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
else ()
    message(STATUS "The CMake build type is: ${CMAKE_BUILD_TYPE}")
endif ()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/source/cmake")

# -----------------------------------------------------------------------------
# Determine which port will be built
# -----------------------------------------------------------------------------
set(ALL_PORTS
    GTK
    MiniGUI
)
set(PORT "NOPORT" CACHE STRING "choose which xGUI Pro port to build (one of ${ALL_PORTS})")

list(FIND ALL_PORTS ${PORT} RET)
if (${RET} EQUAL -1)
    message(FATAL_ERROR "Please choose which xGUI Pro port to build (one of ${ALL_PORTS})")
endif ()

string(TOLOWER ${PORT} XGUIPRO_PORT_DIR)

# -----------------------------------------------------------------------------
# Determine the compiler
# -----------------------------------------------------------------------------
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "AppleClang")
    set(COMPILER_IS_CLANG ON)
endif ()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "7.3.0")
        message(FATAL_ERROR "GCC 7.3 or newer is required to build xGUI Pro. Use a newer GCC version or Clang.")
    endif ()
endif ()

if (CMAKE_COMPILER_IS_GNUCXX OR COMPILER_IS_CLANG)
    set(COMPILER_IS_GCC_OR_CLANG ON)
endif ()

if (MSVC AND COMPILER_IS_CLANG)
    set(COMPILER_IS_CLANG_CL ON)
endif ()

# -----------------------------------------------------------------------------
# Determine the target processor
# -----------------------------------------------------------------------------
# Use MSVC_CXX_ARCHITECTURE_ID instead of CMAKE_SYSTEM_PROCESSOR when defined,
# since the later one just resolves to the host processor on Windows.
if (MSVC_CXX_ARCHITECTURE_ID)
    string(TOLOWER ${MSVC_CXX_ARCHITECTURE_ID} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
else ()
    string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_SYSTEM_PROCESSOR)
endif ()
if (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(^aarch64|^arm64)")
    set(WTF_CPU_ARM64 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(WTF_CPU_ARM 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^mips64")
    set(WTF_CPU_MIPS64 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
    set(WTF_CPU_MIPS 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(x64|x86_64|amd64)")
    # FORCE_32BIT is set in the build script when --32-bit is passed
    # on a Linux/intel 64bit host. This allows us to produce 32bit
    # binaries without setting the build up as a crosscompilation,
    # which is the only way to modify CMAKE_SYSTEM_PROCESSOR.
    if (FORCE_32BIT)
        set(WTF_CPU_X86 1)
    else ()
        set(WTF_CPU_X86_64 1)
    endif ()
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "(i[3-6]86|x86)")
    set(WTF_CPU_X86 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc")
    set(WTF_CPU_PPC 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64")
    set(WTF_CPU_PPC64 1)
elseif (LOWERCASE_CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
    set(WTF_CPU_PPC64LE 1)
else ()
    set(WTF_CPU_UNKNOWN 1)
endif ()

# -----------------------------------------------------------------------------
# Determine the operating system
# -----------------------------------------------------------------------------
if (UNIX)
    if (APPLE)
        set(WTF_OS_MAC_OS_X 1)
    elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(WTF_OS_LINUX 1)
    else ()
        set(WTF_OS_UNIX 1)
    endif ()
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(WTF_OS_WINDOWS 1)
elseif (CMAKE_SYSTEM_NAME MATCHES "Fuchsia")
    set(WTF_OS_FUCHSIA 1)
else ()
    message(FATAL_ERROR "Unknown OS '${CMAKE_SYSTEM_NAME}'")
endif ()

# -----------------------------------------------------------------------------
# Default library types
# -----------------------------------------------------------------------------
# Override these in Options*.cmake for your port as needed.
set(WTF_LIBRARY_TYPE STATIC)
set(xGUIPro_LIBRARY_TYPE STATIC)

set(CMAKE_POSITION_INDEPENDENT_CODE True)

# -----------------------------------------------------------------------------
# Common configuration
#------------------------------------------------------------------------------
include(GlobalCommon)

SET_PROJECT_VERSION(0 9 24)
set(HVMLJS_VERSION 13)
set(XGUIPRO_API_VERSION 0.0)

set(PACKAGE xguipro)
set(PACKAGE_NAME "xGUIPro")
set(PACKAGE_VENDOR "FMSoft")
set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc/${PACKAGE}")
set(DATAROOTDIR "${CMAKE_INSTALL_PREFIX}/share")
set(LIBEXECDIR "${CMAKE_INSTALL_PREFIX}/libexec")
set(DATADIR "${CMAKE_INSTALL_PREFIX}/share/${PACKAGE}")
set(EXTHELPERSDIR "${CMAKE_INSTALL_PREFIX}/lib/${PACKAGE}/ext.d")
add_definitions(-DPACKAGE="${PACKAGE}")

add_subdirectory(source)

# -----------------------------------------------------------------------------
# Print the features list last, for maximum visibility.
# -----------------------------------------------------------------------------
PRINT_XGUIPRO_OPTIONS()

# Packing stuff
set(CPACK_PACKAGE_VENDOR "FMSoft")
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "xGUI Pro: an advanced HVML Renderer based on WebKit.")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
      "/auto/;/cmake/;/.svn/;/.git/;/.bzr/;/CMakeFiles/;/_CPack_Packages/;CMakeCache.txt$;
      ~$;.swp$;.cmake$;
      ${CPACK_SOURCE_IGNORE_FILES}")

#include(CPack)

