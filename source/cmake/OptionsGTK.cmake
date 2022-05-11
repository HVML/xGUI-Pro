include(GNUInstallDirs)

XGUIPRO_OPTION_BEGIN()

CALCULATE_LIBRARY_VERSIONS_FROM_LIBTOOL_TRIPLE(XGUIPRO 0 0 0)

# These are shared variables, but we special case their definition so that we can use the
# CMAKE_INSTALL_* variables that are populated by the GNUInstallDirs macro.
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE PATH "Absolute path to library installation directory")
set(EXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE PATH "Absolute path to executable installation directory")
set(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/purcmc" CACHE PATH "Absolute path to install executables executed by the library")
set(HEADER_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}" CACHE PATH "Absolute path to header installation directory")
set(XGUIPRO_HEADER_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/purcmc" CACHE PATH "Absolute path to xGUIPro header installation directory")

add_definitions(-DBUILDING_LINUX__=1)
add_definitions(-DXGUIPRO_API_VERSION_STRING="${XGUIPRO_API_VERSION}")

find_package(GLIB 2.44.0 REQUIRED COMPONENTS gio gio-unix gmodule)
find_package(Ncurses 5.0 REQUIRED)
find_package(PurC 0.0.1 REQUIRED)
find_package(LibXml2 2.8.0)

if (NOT LIBXML2_FOUND)
    set(ENABLE_XML_DEFAULT OFF)
    SET_AND_EXPOSE_TO_BUILD(HAVE_LIBXML2 OFF)
else ()
    set(ENABLE_XML_DEFAULT ON)
    SET_AND_EXPOSE_TO_BUILD(HAVE_LIBXML2 ON)
endif ()

# Public options specific to the HybridOS port. Do not add any options here unless
# there is a strong reason we should support changing the value of the option,
# and the option is not relevant to any other xGUIPro ports.
#XGUIPRO_OPTION_DEFINE(USE_SYSTEMD "Whether to enable journald logging" PUBLIC ON)

# Private options specific to the HybridOS port. Changing these options is
# completely unsupported. They are intended for use only by xGUIPro developers.
#XGUIPRO_OPTION_DEFINE(USE_ANGLE_WEBGL "Whether to use ANGLE as WebGL backend." PRIVATE OFF)
#XGUIPRO_OPTION_DEPEND(ENABLE_WEBGL ENABLE_GRAPHICS_CONTEXT_GL)

XGUIPRO_OPTION_DEFAULT_PORT_VALUE(ENABLE_XML PUBLIC ${ENABLE_XML_DEFAULT})

# Finalize the value for all options. Do not attempt to use an option before
# this point, and do not attempt to change any option after this point.
XGUIPRO_OPTION_END()

set(xGUIPro_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/source/bin/purcmc.pc)

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
