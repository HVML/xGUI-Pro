# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#[=======================================================================[.rst:
FindCairo
--------------

Find Cairo headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``CairoHBD::CairoHBD``
  The Cairo library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``Cairo_FOUND``
  true if (the requested version of) Cairo is available.
``Cairo_VERSION``
  the version of Cairo.
``Cairo_LIBRARIES``
  the libraries to link against to use Cairo.
``Cairo_INCLUDE_DIRS``
  where to find the Cairo headers.
``Cairo_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

find_package(PkgConfig QUIET)
pkg_check_modules(PC_HICAIRO QUIET hicairo)
set(CairoHBD_COMPILE_OPTIONS ${PC_HICAIRO_CFLAGS_OTHER})
set(CairoHBD_VERSION ${PC_HICAIRO_VERSION})

find_path(CairoHBD_INCLUDE_DIR
    NAMES cairo.h
    HINTS ${PC_HICAIRO_INCLUDEDIR} ${PC_HICAIRO_INCLUDE_DIR}
    PATH_SUFFIXES cairo
)

find_library(CairoHBD_LIBRARY
    NAMES ${CairoHBD_NAMES} hicairo
    HINTS ${PC_HICAIRO_LIBDIR} ${PC_HICAIRO_LIBRARY_DIRS}
)

if (CairoHBD_INCLUDE_DIR AND NOT CairoHBD_VERSION)
    if (EXISTS "${CairoHBD_INCLUDE_DIR}/cairo-version.h")
        file(READ "${CairoHBD_INCLUDE_DIR}/cairo-version.h" Cairo_VERSION_CONTENT)

        string(REGEX MATCH "#define +HICAIRO_VERSION_MAJOR +([0-9]+)" _dummy "${CairoHBD_VERSION_CONTENT}")
        set(CairoHBD_VERSION_MAJOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +HICAIRO_VERSION_MINOR +([0-9]+)" _dummy "${CairoHBD_VERSION_CONTENT}")
        set(CairoHBD_VERSION_MINOR "${CMAKE_MATCH_1}")

        string(REGEX MATCH "#define +HICAIRO_VERSION_MICRO +([0-9]+)" _dummy "${CairoHBD_VERSION_CONTENT}")
        set(CairoHBD_VERSION_PATCH "${CMAKE_MATCH_1}")

        set(CairoHBD_VERSION "${CairoHBD_VERSION_MAJOR}.${CairoHBD_VERSION_MINOR}.${CairoHBD_VERSION_PATCH}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CairoHBD
    FOUND_VAR CairoHBD_FOUND
    REQUIRED_VARS CairoHBD_LIBRARY CairoHBD_INCLUDE_DIR
    VERSION_VAR CairoHBD_VERSION
)

if (CairoHBD_LIBRARY AND NOT TARGET CairoHBD::CairoHBD)
    add_library(CairoHBD::CairoHBD UNKNOWN IMPORTED GLOBAL)
    set_target_properties(CairoHBD::CairoHBD PROPERTIES
        IMPORTED_LOCATION "${CairoHBD_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${CairoHBD_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${CairoHBD_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(CairoHBD_INCLUDE_DIR CairoHBD_LIBRARIES)

if (CairoHBD_FOUND)
    set(CairoHBD_LIBRARIES ${CairoHBD_LIBRARY})
    set(CairoHBD_INCLUDE_DIRS ${CairoHBD_INCLUDE_DIR})
endif ()
