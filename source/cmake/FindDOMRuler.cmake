# Copyright (C) 2022 Beijing FMSoft Technologies Co., Ltd.
#
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
FindDOMRuler
--------------

Find DOMRuler headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``DOMRuler::DOMRuler``
  The DOMRuler library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``DOMRULER_FOUND``
  true if (the requested version of) DOMRuler is available.
``DOMRULER_VERSION``
  the version of DOMRuler.
``DOMRULER_LIBRARIES``
  the libraries to link against to use DOMRuler.
``DOMRULER_INCLUDE_DIRS``
  where to find the DOMRuler headers.
``DOMRULER_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

# TODO: Remove when cmake_minimum_version bumped to 3.14.

find_package(PkgConfig QUIET)
pkg_check_modules(PC_DOMRULER QUIET domruler)
set(DOMRULER_COMPILE_OPTIONS ${PC_DOMRULER_CFLAGS_OTHER})
set(DOMRULER_VERSION ${PC_DOMRULER_VERSION})

find_path(DOMRULER_INCLUDE_DIR
    NAMES domruler/domruler.h
    HINTS ${PC_DOMRULER_INCLUDEDIR} ${PC_DOMRULER_INCLUDE_DIR}
)

find_library(DOMRULER_LIBRARY
    NAMES ${DOMRULER_NAMES} domruler
    HINTS ${PC_DOMRULER_LIBDIR} ${PC_DOMRULER_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DOMRuler
    FOUND_VAR DOMRULER_FOUND
    REQUIRED_VARS DOMRULER_LIBRARY DOMRULER_INCLUDE_DIR
    VERSION_VAR DOMRULER_VERSION
)

if (DOMRULER_LIBRARY AND NOT TARGET DOMRuler::DOMRuler)
    add_library(DOMRuler::DOMRuler UNKNOWN IMPORTED GLOBAL)
    set_target_properties(DOMRuler::DOMRuler PROPERTIES
        IMPORTED_LOCATION "${DOMRULER_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${DOMRULER_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${DOMRULER_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(DOMRULER_INCLUDE_DIR DOMRULER_LIBRARIES)

if (DOMRULER_FOUND)
    set(DOMRULER_LIBRARIES ${DOMRULER_LIBRARY})
    set(DOMRULER_INCLUDE_DIRS ${DOMRULER_INCLUDE_DIR})
endif ()

