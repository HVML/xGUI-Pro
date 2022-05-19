set_property(DIRECTORY . PROPERTY FOLDER "lib")

include(GlobalCommon)
include(MyMacros.cmake)

set(xGUIPro_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

set(xGUIPro_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${XGUIPRO_LIB_DIR}"
    "${xGUIPro_DERIVED_SOURCES_DIR}"
    "${FORWARDING_HEADERS_DIR}"
)

# TODO: List the headers should be installed to system here.
set(xGUIPro_INSTALLED_HEADERS
)

# TODO: List the directorires which do not contain any platform-dependent C source files
set(xGUIPro_PLATFORM_INDEPENDENT_DIRS
    "utils"
)

# This macro will append all C/CXX source files in ${xGUIPro_PLATFORM_INDEPENDENT_DIRS}
# to ${xGUIPro_SOURCES}
APPEND_ALL_SOURCE_FILES_IN_DIRLIST(xGUIPro_SOURCES "${xGUIPro_PLATFORM_INDEPENDENT_DIRS}")

# TODO: List the source files individually.
list(APPEND xGUIPro_SOURCES
)

# Put the CXX source files which can be unified in `Sources.txt`
list(APPEND xGUIPro_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(xGUIPro_LIBRARIES
    ${GLIB_LIBRARIES}
)

set(xGUIPro_INTERFACE_LIBRARIES xGUIPro)
set(xGUIPro_INTERFACE_INCLUDE_DIRECTORIES ${xGUIPro_PRIVATE_FRAMEWORK_HEADERS_DIR})

XGUIPRO_FRAMEWORK_DECLARE(xGUIPro)

XGUIPRO_WRAP_SOURCELIST(${xGUIPro_SOURCES})

XGUIPRO_COPY_FILES(xGUIPro_CopyPrivateHeaders
    DESTINATION ${xGUIPro_PRIVATE_FRAMEWORK_HEADERS_DIR}/xguipro
    FILES ${xGUIPro_PRIVATE_FRAMEWORK_HEADERS}
    FLATTENED
)
list(APPEND xGUIPro_INTERFACE_DEPENDENCIES xGUIPro_CopyPrivateHeaders)
# xGUIPro_CopyPrivateHeaders needs to have a direct or indirect
# dependency of xGUIPro target for CMake Visual Studio generator to
# eliminate duplicated custom commands. Otherwise, CSSPropertyNames.h
# will be generated in both projects.
if (NOT INTERNAL_BUILD)
    add_dependencies(xGUIPro_CopyPrivateHeaders xGUIPro)
endif ()

XGUIPRO_COMPUTE_SOURCES(xGUIPro)
XGUIPRO_FRAMEWORK(xGUIPro)
XGUIPRO_FRAMEWORK_TARGET(xGUIPro)

# The -ftree-sra optimization (implicit with -O2) causes crashes when
# allocating large chunks of memory using bmalloc on Intel 32bit.
# See https://bugs.webkit.org/show_bug.cgi?id=146440
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR)
if (CMAKE_COMPILER_IS_GNUCXX AND "${LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "(i[3-6]86|x86)$")
    XGUIPRO_ADD_TARGET_CXX_FLAGS(xGUIPro -fno-tree-sra)
endif ()

if (${xGUIPro_LIBRARY_TYPE} MATCHES "SHARED")
    set_target_properties(xGUIPro PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
# install(TARGETS xGUIPro DESTINATION "${LIB_INSTALL_DIR}")
endif ()

if (MSVC)
    set_target_properties(xGUIPro PROPERTIES
        COMPILE_PDB_NAME ${xGUIPro_OUTPUT_NAME}
    )
endif ()

# add_subdirectory(res)

# install(TARGETS xGUIPro DESTINATION "${LIB_INSTALL_DIR}/")

# install(FILES ${xGUIPro_INSTALLED_HEADERS} DESTINATION "${HEADER_INSTALL_DIR}/xguipro")
