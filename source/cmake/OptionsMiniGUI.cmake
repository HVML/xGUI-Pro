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
find_package(PurC 0.9.12 REQUIRED)
find_package(DOMRuler 0.9.12 REQUIRED)
find_package(MiniGUI 5.0.14 REQUIRED COMPONENTS mGEff)
find_package(CairoHBD REQUIRED)

XGUIPRO_OPTION_DEFINE(USE_SOUP2 "Whether to enable usage of Soup 2 instead of Soup 3." PUBLIC ON)

XGUIPRO_OPTION_DEFAULT_PORT_VALUE(USE_SOUP2 PUBLIC ON)
XGUIPRO_OPTION_DEFAULT_PORT_VALUE(ENABLE_DEVELOPER_MODE PUBLIC OFF)

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

if (USE_SOUP2)
    set(JAVASCRIPTCOREHBD_PC_NAME javascriptcorehbd-4.0)
    set(WEBKIT2HBD_PC_NAME webkit2hbd-4.0)
    set(WEBKIT2HBD_API_VERSION 4.0)
    set(WEBKIT2HBD_API_DOC_VERSION 4.0)
else ()
    set(JAVASCRIPTCOREHBD_PC_NAME javascriptcorehbd-4.1)
    set(WEBKIT2HBD_PC_NAME webkit2hbd-4.1)
    set(WEBKIT2HBD_API_VERSION 4.1)
    # No API changes in 4.1, so keep using the same API documentation.
    set(WEBKIT2HBD_API_DOC_VERSION 4.0)
endif ()

find_package(WebKit2HBD 2.34.0 REQUIRED)

XGUIPRO_OPTION_END()

set(xGUIPRo_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/source/xguipro.pc)

set(xGUIProTestSupport_LIBRARY_TYPE STATIC)

