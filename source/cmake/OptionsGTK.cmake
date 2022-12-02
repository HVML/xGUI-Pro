include(GNUInstallDirs)

XGUIPRO_OPTION_BEGIN()

CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(XGUIPRO 0 0 0)

# These are shared variables, but we special case their definition so that we can use the
# CMAKE_INSTALL_* variables that are populated by the GNUInstallDirs macro.
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE PATH "Absolute path to library installation directory")
set(EXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE PATH "Absolute path to executable installation directory")
set(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/xguipro" CACHE PATH "Absolute path to install executables executed by the library")
set(HEADER_INSTALL_DIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}" CACHE PATH "Absolute path to header installation directory")
set(XGUIPRO_HEADER_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/xguipro" CACHE PATH "Absolute path to xGUIPro header installation directory")

add_definitions(-DBUILDING_LINUX__=1)
add_definitions(-DXGUIPRO_API_VERSION_STRING="${XGUIPRO_API_VERSION}")

find_package(GLIB 2.44.0 REQUIRED COMPONENTS gio gio-unix gmodule gobject)
find_package(PurC 0.9.2 REQUIRED)
find_package(DOMRuler 0.9.2 REQUIRED)

find_package(OpenSSL)
if (OpenSSL_FOUND)
    XGUIPRO_OPTION_DEFINE(HAVE_LIBSSL "Whether having OpenSSL." PUBLIC ON)
endif (OpenSSL_FOUND)

# Public options specific to the HybridOS port. Do not add any options here unless
# there is a strong reason we should support changing the value of the option,
# and the option is not relevant to any other xGUIPro ports.

XGUIPRO_OPTION_DEFINE(USE_GTK4 "Whether to enable usage of GTK4 instead of GTK3." PUBLIC OFF)
XGUIPRO_OPTION_DEFINE(USE_SOUP2 "Whether to enable usage of Soup 2 instead of Soup 3." PUBLIC ON)
#XGUIPRO_OPTION_DEFINE(USE_SYSTEMD "Whether to enable journald logging" PUBLIC ON)

XGUIPRO_OPTION_CONFLICT(USE_GTK4 USE_SOUP2)

# Private options specific to the HybridOS port. Changing these options is
# completely unsupported. They are intended for use only by xGUIPro developers.
#XGUIPRO_OPTION_DEFINE(USE_ANGLE_WEBGL "Whether to use ANGLE as WebGL backend." PRIVATE OFF)
#XGUIPRO_OPTION_DEPEND(ENABLE_WEBGL ENABLE_GRAPHICS_CONTEXT_GL)

XGUIPRO_OPTION_DEFAULT_PORT_VALUE(ENABLE_DEVELOPER_MODE PUBLIC OFF)

# Finalize the value for all options. Do not attempt to use an option before
# this point, and do not attempt to change any option after this point.
XGUIPRO_OPTION_END()

set(xGUIPRo_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/source/xguipro.pc)

if (USE_GTK4)
    set(GTK_MINIMUM_VERSION 3.98.5)
    set(GTK_PC_NAME gtk4)
else ()
    set(GTK_MINIMUM_VERSION 3.22.0)
    set(GTK_PC_NAME gtk+-3.0)
endif ()
find_package(GTK ${GTK_MINIMUM_VERSION} REQUIRED OPTIONAL_COMPONENTS unix-print)

if (USE_SOUP2)
    set(SOUP_MINIMUM_VERSION 2.54.0)
    set(SOUP_API_VERSION 2.4)
else ()
    set(SOUP_MINIMUM_VERSION 2.99.9)
    set(SOUP_API_VERSION 3.0)
    set(ENABLE_SERVER_PRECONNECT ON)
endif ()
find_package(LibSoup ${SOUP_MINIMUM_VERSION})

if (NOT LibSoup_FOUND)
    if (USE_SOUP2)
        message(FATAL_ERROR "libsoup is required.")
    else ()
        message(FATAL_ERROR "libsoup 3 is required. Enable USE_SOUP2 to use libsoup 2 (disables HTTP/2)")
    endif ()
endif ()

if (USE_GTK4)
    set(JAVASCRIPTCOREGTK_PC_NAME javascriptcoregtk-5.0)
    set(WEBKIT2GTK_PC_NAME webkit2gtk-5.0)
    set(WEBKIT2GTK_API_VERSION 5.0)
    set(WEBKIT2GTK_API_DOC_VERSION 5.0)
elseif (USE_SOUP2)
    set(JAVASCRIPTCOREGTK_PC_NAME javascriptcoregtk-4.0)
    set(WEBKIT2GTK_PC_NAME webkit2gtk-4.0)
    set(WEBKIT2GTK_API_VERSION 4.0)
    set(WEBKIT2GTK_API_DOC_VERSION 4.0)
else ()
    set(JAVASCRIPTCOREGTK_PC_NAME javascriptcoregtk-4.1)
    set(WEBKIT2GTK_PC_NAME webkit2gtk-4.1)
    set(WEBKIT2GTK_API_VERSION 4.1)
    # No API changes in 4.1, so keep using the same API documentation.
    set(WEBKIT2GTK_API_DOC_VERSION 4.0)
endif ()

find_package(WebKit2Gtk 2.34.1 REQUIRED)

# CMake does not automatically add --whole-archive when building shared objects from
# a list of convenience libraries. This can lead to missing symbols in the final output.
# We add --whole-archive to all libraries manually to prevent the linker from trimming
# symbols that we actually need later. With ld64 on darwin, we use -all_load instead.
macro(ADD_WHOLE_ARCHIVE_TO_LIBRARIES _list_name)
    if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        list(APPEND ${_list_name} -Wl,-all_load)
    else ()
        set(_tmp)
        foreach (item IN LISTS ${_list_name})
            if ("${item}" STREQUAL "PRIVATE" OR "${item}" STREQUAL "PUBLIC")
                list(APPEND _tmp "${item}")
            else ()
                list(APPEND _tmp -Wl,--whole-archive "${item}" -Wl,--no-whole-archive)
            endif ()
        endforeach ()
        set(${_list_name} ${_tmp})
    endif ()
endmacro()

#include(BubblewrapSandboxChecks)
