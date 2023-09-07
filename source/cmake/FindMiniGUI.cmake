# - Try to find MiniGUI and/or MiniGUI components
# Once done, this will define
#
#  MINIGUI_FOUND - system has MiniGUI.
#  MINIGUI_RUNMODE - the runtime mode of MiniGUI.
#  MINIGUI_INCLUDE_DIRS - the MiniGUI include directories
#  MINIGUI_LIBRARIES - the libraries to link to use MiniGUI.
#
# You can use predefined MINIGUI_LIBSUFFIX variable to override the default
# library suffix which was defined in minigui.pc file.
#
# Copyright (C) 2020, 2023 Beijing FMSoft Technologies Co., Ltd.
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

find_package(PkgConfig)
pkg_check_modules(PC_MINIGUI minigui)
set(MINIGUI_COMPILE_OPTIONS ${PC_MINIGUI_CFLAGS_OTHER})
set(MINIGUI_VERSION ${PC_MINIGUI_VERSION})

find_path(MINIGUI_INCLUDE_DIR
    NAMES "minigui/common.h"
    HINTS ${PC_MINIGUI_INCLUDEDIR} ${PC_MINIGUI_INCLUDE_DIR}
)

pkg_get_variable(PC_MINIGUI_RUNMODE minigui "runmode")
message(STATUS "Found MiniGUI runtime mode: ${PC_MINIGUI_RUNMODE}")

if (NOT MINIGUI_LIBSUFFIX)
    pkg_get_variable(PC_MINIGUI_LIBSUFFIX minigui "libsuffix")
    message(STATUS "Found MiniGUI library suffix: ${PC_MINIGUI_LIBSUFFIX}")
    set(MINIGUI_LIBSUFFIX ${PC_MINIGUI_LIBSUFFIX})
endif ()

find_library(MINIGUI_LIBRARY
    NAMES ${MINIGUI_NAMES} "minigui_${MINIGUI_LIBSUFFIX}"
    HINTS ${PC_MINIGUI_LIBDIR} ${PC_MINIGUI_LIBRARY_DIRS}
)

set(VERSION_OK TRUE)

if (PC_MINIGUI_VERSION)
    if (MINIGUI_FIND_VERSION_EXACT)
        if (NOT("${MINIGUI_FIND_VERSION}" VERSION_EQUAL "${PC_MINIGUI_VERSION}"))
            set(VERSION_OK FALSE)
        endif ()
    else ()
        if ("${PC_MINIGUI_VERSION}" VERSION_LESS "${MINIGUI_FIND_VERSION}")
            set(VERSION_OK FALSE)
        endif ()
    endif ()
endif ()

# Additional MiniGUI components.
foreach (_component ${MiniGUI_FIND_COMPONENTS})
    if (${_component} STREQUAL "mGEff")
        pkg_check_modules(PC_MGEFF mgeff)
        set(MGEFF_COMPILE_OPTIONS ${PC_MGEFF_CFLAGS_OTHER})
        set(MGEFF_VERSION ${PC_MGEFF_VERSION})
        find_path(MGEFF_INCLUDE_DIR
                NAMES "mgeff/mgeff.h"
                HINTS ${PC_MGEFF_INCLUDEDIR} ${PC_MGEFF_INCLUDE_DIR})
        find_library(MGEFF_LIBRARY
                NAMES "mgeff"
                HINTS ${PC_MINIGUI_LIBDIR} ${PC_MINIGUI_LIBRARY_DIRS})
    elseif (${_component} STREQUAL "mGUtils")
        pkg_check_modules(PC_MGUTILS mgutils)
        set(MGUTILS_COMPILE_OPTIONS ${PC_MGUTILS_CFLAGS_OTHER})
        set(MGUTILS_VERSION ${PC_MGUTILS_VERSION})
        find_path(MGUTILS_INCLUDE_DIR
                NAMES "mgutils/mgutils.h"
                HINTS ${PC_MGUTILS_INCLUDEDIR} ${PC_MGUTILS_INCLUDE_DIR})
        find_library(MGUTILS_LIBRARY
                NAMES "mgutils"
                HINTS ${PC_MINIGUI_LIBDIR} ${PC_MINIGUI_LIBRARY_DIRS})
    elseif (${_component} STREQUAL "mGPlus")
        pkg_check_modules(PC_MGPLUS mgplus)
        set(MGPLUS_COMPILE_OPTIONS ${PC_MGPLUS_CFLAGS_OTHER})
        set(MGPLUS_VERSION ${PC_MGPLUS_VERSION})
        find_path(MGPLUS_INCLUDE_DIR
                NAMES "mgplus/mgplus.h"
                HINTS ${PC_MGPLUS_INCLUDEDIR} ${PC_MGPLUS_INCLUDE_DIR})
        find_library(MGPLUS_LIBRARY
                NAMES "mgplus"
                HINTS ${PC_MINIGUI_LIBDIR} ${PC_MINIGUI_LIBRARY_DIRS})
    elseif (${_component} STREQUAL "mGNCS")
        pkg_check_modules(PC_MGNCS mgncs)
        set(MGNCS_COMPILE_OPTIONS ${PC_MGNCS_CFLAGS_OTHER})
        set(MGNCS_VERSION ${PC_MGNCS_VERSION})
        find_path(MGNCS_INCLUDE_DIR
                NAMES "mgncs/mgncs.h"
                HINTS ${PC_MGNCS_INCLUDEDIR} ${PC_MGNCS_INCLUDE_DIR})
        find_library(MGNCS_LIBRARY
                NAMES "mgncs"
                HINTS ${PC_MINIGUI_LIBDIR} ${PC_MINIGUI_LIBRARY_DIRS})
    elseif (${_component} STREQUAL "mGNCS4Touch")
        pkg_check_modules(PC_MGNCS4TOUCH mgncs4touch)
        set(MGNCS4TOUCH_COMPILE_OPTIONS ${PC_MGNCS4TOUCH_CFLAGS_OTHER})
        set(MGNCS4TOUCH_VERSION ${PC_MGNCS4TOUCH_VERSION})
        find_path(MGNCS4TOUCH_INCLUDE_DIR
                NAMES "mgncs4touch/mgncs4touch.h"
                HINTS ${PC_MGNCS4TOUCH_INCLUDEDIR} ${PC_MGNCS4TOUCH_INCLUDE_DIR})
        find_library(MGNCS4TOUCH_LIBRARY
                NAMES "mgncs4touch"
                HINTS ${PC_MINIGUI_LIBDIR} ${PC_MINIGUI_LIBRARY_DIRS})
    endif ()
endforeach ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MiniGUI
    FOUND_VAR MINIGUI_FOUND
    REQUIRED_VARS MINIGUI_LIBRARY MINIGUI_INCLUDE_DIR
    VERSION_VAR MINIGUI_VERSION
)

if (MINIGUI_LIBRARY AND NOT TARGET MiniGUI::MiniGUI)
    add_library(MiniGUI::MiniGUI UNKNOWN IMPORTED GLOBAL)
    set_target_properties(MiniGUI::MiniGUI PROPERTIES
        IMPORTED_LOCATION "${MINIGUI_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${MINIGUI_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${MINIGUI_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(MINIGUI_INCLUDE_DIR MINIGUI_LIBRARIES)

if (MINIGUI_FOUND)
    set(MINIGUI_RUNMODE ${PC_MINIGUI_RUNMODE})
    set(MINIGUI_INCLUDE_DIRS ${MINIGUI_INCLUDE_DIR})
    set(MINIGUI_LIBRARIES ${MINIGUI_LIBRARY})
endif ()

if (MGEFF_LIBRARY AND NOT TARGET MiniGUI::mGEff)
    add_library(MiniGUI::mGEff UNKNOWN IMPORTED GLOBAL)
    set_target_properties(MiniGUI::mGEff PROPERTIES
        IMPORTED_LOCATION "${MGEFF_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${MGEFF_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${MGEFF_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(MGEFF_INCLUDE_DIR MGEFF_LIBRARIES)

if (MGUTILS_LIBRARY AND NOT TARGET MiniGUI::mGUtils)
    add_library(MiniGUI::mGUtils UNKNOWN IMPORTED GLOBAL)
    set_target_properties(MiniGUI::mGUtils PROPERTIES
        IMPORTED_LOCATION "${MGUTILS_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${MGUTILS_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${MGUTILS_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(MGUTILS_INCLUDE_DIR MGUTILS_LIBRARIES)

if (MGPLUS_LIBRARY AND NOT TARGET MiniGUI::mGPlus)
    add_library(MiniGUI::mGPlus UNKNOWN IMPORTED GLOBAL)
    set_target_properties(MiniGUI::mGPlus PROPERTIES
        IMPORTED_LOCATION "${MGPLUS_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${MGPLUS_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${MGPLUS_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(MGPLUS_INCLUDE_DIR MGPLUS_LIBRARIES)

if (MGNCS_LIBRARY AND NOT TARGET MiniGUI::mGNCS)
    add_library(MiniGUI::mGNCS UNKNOWN IMPORTED GLOBAL)
    set_target_properties(MiniGUI::mGNCS PROPERTIES
        IMPORTED_LOCATION "${MGNCS_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${MGNCS_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${MGNCS_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(MGNCS_INCLUDE_DIR MGNCS_LIBRARIES)

if (MGNCS4TOUCH_LIBRARY AND NOT TARGET MiniGUI::mGNCS4Touch)
    add_library(MiniGUI::mGNCS4Touch UNKNOWN IMPORTED GLOBAL)
    set_target_properties(MiniGUI::mGNCS4Touch PROPERTIES
        IMPORTED_LOCATION "${MGNCS4TOUCH_LIBRARY}"
        INTERFACE_COMPILE_OPTIONS "${MGNCS4TOUCH_COMPILE_OPTIONS}"
        INTERFACE_INCLUDE_DIRECTORIES "${MGNCS4TOUCH_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(MGNCS4TOUCH_INCLUDE_DIR MGNCS4TOUCH_LIBRARIES)

