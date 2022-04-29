# Copyright (C) 2021 Beijing FMSoft Technologies Co., Ltd.
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
FindPurC
--------------

Find PurC headers and libraries.

Imported Targets
^^^^^^^^^^^^^^^^

``PurC::PurC``
  The PurC library, if found.

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables in your project:

``PurC_FOUND``
  true if (the requested version of) PurC is available.
``PurC_VERSION``
  the version of PurC.
``PurC_LIBRARIES``
  the libraries to link against to use PurC.
``PurC_INCLUDE_DIRS``
  where to find the PurC headers.
``PurC_COMPILE_OPTIONS``
  this should be passed to target_compile_options(), if the
  target is not used for linking

#]=======================================================================]

# TODO: Remove when cmake_minimum_version bumped to 3.14.

find_package(PkgConfig QUIET)
pkg_check_modules(PC_PURC QUIET purc)
set(PurC_COMPILE_OPTIONS ${PC_PURC_CFLAGS_OTHER})
set(PurC_VERSION ${PC_PURC_VERSION})

find_path(PurC_INCLUDE_DIR
    NAMES purc/purc.h
    HINTS ${PC_PURC_INCLUDEDIR} ${PC_PURC_INCLUDE_DIR}
)

find_library(PurC_LIBRARY
    NAMES ${PurC_NAMES} purc
    HINTS ${PC_PURC_LIBDIR} ${PC_PURC_LIBRARY_DIRS}
)

if (PurC_INCLUDE_DIR AND NOT PurC_VERSION)
    if (EXISTS "${PurC_INCLUDE_DIR}/purc/purc-version.h")
        file(STRINGS ${PurC_INCLUDE_DIR}/purc/purc-version.h _ver_line
            REGEX "^#define PURC_VERSION_STRING  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
            LIMIT_COUNT 1)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
            PurC_VERSION "${_ver_line}")
        unset(_ver_line)
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PurC
    FOUND_VAR PurC_FOUND
    REQUIRED_VARS PurC_LIBRARY PurC_INCLUDE_DIR
    VERSION_VAR PurC_VERSION
)

if (PurC_LIBRARY AND NOT TARGET PurC::PurC)
    add_library(PurC::PurC UNKNOWN IMPORTED GLOBAL)
    set_target_properties(PurC::PurC PROPERTIES
        IMPORTED_LOCATION "${PurC_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${PurC_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${PurC_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(PurC_INCLUDE_DIR PurC_LIBRARIES)

if (PurC_FOUND)
    set(PurC_LIBRARIES ${PurC_LIBRARY})
    set(PurC_INCLUDE_DIRS ${PurC_INCLUDE_DIR})
endif ()

