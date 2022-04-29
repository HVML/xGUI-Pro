# The settings in this file are the xGUIPro project default values, and
# are recommended for most ports. Ports can override these settings in
# Options*.cmake, but should do so only if there is strong reason to
# deviate from the defaults of the xGUIPro project (e.g. if the feature
# requires platform-specific implementation that does not exist).

set(_XGUIPRO_AVAILABLE_OPTIONS "")

set(PUBLIC YES)
set(PRIVATE NO)

macro(_ENSURE_OPTION_MODIFICATION_IS_ALLOWED)
    if (NOT _SETTING_XGUIPRO_OPTIONS)
        message(FATAL_ERROR "Options must be set between XGUIPRO_OPTION_BEGIN and XGUIPRO_OPTION_END")
    endif ()
endmacro()

macro(_ENSURE_IS_XGUIPRO_OPTION _name)
    list(FIND _XGUIPRO_AVAILABLE_OPTIONS ${_name} ${_name}_OPTION_INDEX)
    if (${_name}_OPTION_INDEX EQUAL -1)
        message(FATAL_ERROR "${_name} is not a valid xGUIPro option")
    endif ()
endmacro()

macro(XGUIPRO_OPTION_DEFINE _name _description _public _initial_value)
    _ENSURE_OPTION_MODIFICATION_IS_ALLOWED()

    set(_XGUIPRO_AVAILABLE_OPTIONS_DESCRIPTION_${_name} ${_description})
    set(_XGUIPRO_AVAILABLE_OPTIONS_IS_PUBLIC_${_name} ${_public})
    set(_XGUIPRO_AVAILABLE_OPTIONS_INITIAL_VALUE_${_name} ${_initial_value})
    set(_XGUIPRO_AVAILABLE_OPTIONS_${_name}_CONFLICTS "")
    set(_XGUIPRO_AVAILABLE_OPTIONS_${_name}_DEPENDENCIES "")
    list(APPEND _XGUIPRO_AVAILABLE_OPTIONS ${_name})

    EXPOSE_VARIABLE_TO_BUILD(${_name})
endmacro()

macro(XGUIPRO_OPTION_DEFAULT_PORT_VALUE _name _public _value)
    _ENSURE_OPTION_MODIFICATION_IS_ALLOWED()
    _ENSURE_IS_XGUIPRO_OPTION(${_name})

    set(_XGUIPRO_AVAILABLE_OPTIONS_IS_PUBLIC_${_name} ${_public})
    set(_XGUIPRO_AVAILABLE_OPTIONS_INITIAL_VALUE_${_name} ${_value})
endmacro()

macro(XGUIPRO_OPTION_CONFLICT _name _conflict)
    _ENSURE_OPTION_MODIFICATION_IS_ALLOWED()
    _ENSURE_IS_XGUIPRO_OPTION(${_name})
    _ENSURE_IS_XGUIPRO_OPTION(${_conflict})

    list(APPEND _XGUIPRO_AVAILABLE_OPTIONS_${_name}_CONFLICTS ${_conflict})
endmacro()

macro(XGUIPRO_OPTION_DEPEND _name _depend)
    _ENSURE_OPTION_MODIFICATION_IS_ALLOWED()
    _ENSURE_IS_XGUIPRO_OPTION(${_name})
    _ENSURE_IS_XGUIPRO_OPTION(${_depend})

    list(APPEND _XGUIPRO_AVAILABLE_OPTIONS_${_name}_DEPENDENCIES ${_depend})
endmacro()

macro(XGUIPRO_OPTION_BEGIN)
    set(_SETTING_XGUIPRO_OPTIONS TRUE)

    if (WTF_CPU_ARM64 OR WTF_CPU_X86_64)
        set(USE_SYSTEM_MALLOC_DEFAULT OFF)
    elseif (WTF_CPU_ARM AND WTF_OS_LINUX AND ARM_THUMB2_DETECTED)
        set(USE_SYSTEM_MALLOC_DEFAULT OFF)
    elseif (WTF_CPU_MIPS AND WTF_OS_LINUX)
        set(USE_SYSTEM_MALLOC_DEFAULT OFF)
    else ()
        set(USE_SYSTEM_MALLOC_DEFAULT ON)
    endif ()

    if (DEFINED ClangTidy_EXE OR DEFINED IWYU_EXE)
        message(STATUS "Unified builds are disabled when analyzing sources")
        set(ENABLE_UNIFIED_BUILDS_DEFAULT OFF)
    else ()
        set(ENABLE_UNIFIED_BUILDS_DEFAULT ON)
    endif ()

    XGUIPRO_OPTION_DEFINE(ENABLE_CHARSET "Enable support for charsets" PUBLIC ON)
    XGUIPRO_OPTION_DEFINE(ENABLE_XML "Enable support for XML" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_CPIO "Toggle support for CPIO plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_EXTFS "Toggle support for EXTFS plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_FTP "Toggle support for FTP plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_NET "Toggle support for NET plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_SFS "Toggle support for SFS plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_SFTP "Toggle support for SFTP plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_UNDELFS "Toggle support for UNDELFS plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_TAR "Toggle support for TAR plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_VFS_FISH "Toggle support for FISH plugin of VFS" PUBLIC OFF)
    XGUIPRO_OPTION_DEFINE(ENABLE_API_TESTS "Enable public API unit tests" PUBLIC OFF)

    XGUIPRO_OPTION_DEFINE(USE_SYSTEM_MALLOC "Toggle system allocator instead of xGUIPro's custom allocator" PRIVATE ${USE_SYSTEM_MALLOC_DEFAULT})

    #XGUIPRO_OPTION_DEPEND(ENABLE_XSLT ENABLE_XML)
endmacro()

macro(_XGUIPRO_OPTION_ENFORCE_DEPENDS _name)
    foreach (_dependency ${_XGUIPRO_AVAILABLE_OPTIONS_${_name}_DEPENDENCIES})
        if (NOT ${_dependency})
            message(STATUS "Disabling ${_name} since ${_dependency} is disabled.")
            set(${_name} OFF)
            set(_OPTION_CHANGED TRUE)
            break ()
        endif ()
    endforeach ()
endmacro()

macro(_XGUIPRO_OPTION_ENFORCE_ALL_DEPENDS)
    set(_OPTION_CHANGED TRUE)
    while (${_OPTION_CHANGED})
        set(_OPTION_CHANGED FALSE)
        foreach (_name ${_XGUIPRO_AVAILABLE_OPTIONS})
            if (${_name})
                _XGUIPRO_OPTION_ENFORCE_DEPENDS(${_name})
            endif ()
        endforeach ()
    endwhile ()
endmacro()

macro(_XGUIPRO_OPTION_ENFORCE_CONFLICTS _name)
    foreach (_conflict ${_XGUIPRO_AVAILABLE_OPTIONS_${_name}_CONFLICTS})
        if (${_conflict})
            message(FATAL_ERROR "${_name} conflicts with ${_conflict}. You must disable one or the other.")
        endif ()
    endforeach ()
endmacro()

macro(_XGUIPRO_OPTION_ENFORCE_ALL_CONFLICTS)
    foreach (_name ${_XGUIPRO_AVAILABLE_OPTIONS})
        if (${_name})
            _XGUIPRO_OPTION_ENFORCE_CONFLICTS(${_name})
        endif ()
    endforeach ()
endmacro()

macro(XGUIPRO_OPTION_END)
    set(_SETTING_XGUIPRO_OPTIONS FALSE)

    list(SORT _XGUIPRO_AVAILABLE_OPTIONS)
    set(_MAX_FEATURE_LENGTH 0)
    foreach (_name ${_XGUIPRO_AVAILABLE_OPTIONS})
        string(LENGTH ${_name} _name_length)
        if (_name_length GREATER _MAX_FEATURE_LENGTH)
            set(_MAX_FEATURE_LENGTH ${_name_length})
        endif ()

        option(${_name} "${_XGUIPRO_AVAILABLE_OPTIONS_DESCRIPTION_${_name}}" ${_XGUIPRO_AVAILABLE_OPTIONS_INITIAL_VALUE_${_name}})
        if (NOT ${_XGUIPRO_AVAILABLE_OPTIONS_IS_PUBLIC_${_name}})
            mark_as_advanced(FORCE ${_name})
        endif ()
    endforeach ()

    # Run through every possible depends to make sure we have disabled anything
    # that could cause an unnecessary conflict before processing conflicts.
    _XGUIPRO_OPTION_ENFORCE_ALL_DEPENDS()
    _XGUIPRO_OPTION_ENFORCE_ALL_CONFLICTS()

    foreach (_name ${_XGUIPRO_AVAILABLE_OPTIONS})
        if (${_name})
            list(APPEND FEATURE_DEFINES ${_name})
            set(FEATURE_DEFINES_WITH_SPACE_SEPARATOR "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR} ${_name}")
        endif ()
    endforeach ()
endmacro()

macro(PRINT_XGUIPRO_OPTIONS)
    message(STATUS "Enabled features:")

    set(_should_print_dots ON)
    foreach (_name ${_XGUIPRO_AVAILABLE_OPTIONS})
        if (${_XGUIPRO_AVAILABLE_OPTIONS_IS_PUBLIC_${_name}})
            string(LENGTH ${_name} _name_length)
            set(_message " ${_name} ")

            # Print dots on every other row, for readability.
            foreach (IGNORE RANGE ${_name_length} ${_MAX_FEATURE_LENGTH})
                if (${_should_print_dots})
                    set(_message "${_message}.")
                else ()
                    set(_message "${_message} ")
                endif ()
            endforeach ()

            set(_should_print_dots (NOT ${_should_print_dots}))

            set(_message "${_message} ${${_name}}")
            message(STATUS "${_message}")
        endif ()
    endforeach ()
endmacro()

set(_XGUIPRO_CONFIG_FILE_VARIABLES "")

macro(EXPOSE_VARIABLE_TO_BUILD _variable_name)
    list(APPEND _XGUIPRO_CONFIG_FILE_VARIABLES ${_variable_name})
endmacro()

macro(SET_AND_EXPOSE_TO_BUILD _variable_name)
    # It's important to handle the case where the value isn't passed, because often
    # during configuration an empty variable is the result of a failed package search.
    if (${ARGC} GREATER 1)
        set(_variable_value ${ARGV1})
    else ()
        set(_variable_value OFF)
    endif ()

    set(${_variable_name} ${_variable_value})
    EXPOSE_VARIABLE_TO_BUILD(${_variable_name})
endmacro()

macro(_ADD_CONFIGURATION_LINE_TO_HEADER_STRING _string _variable_name _output_variable_name)
    if (${${_variable_name}})
        set(${_string} "${_file_contents}#define ${_output_variable_name} 1\n")
    else ()
        set(${_string} "${_file_contents}#define ${_output_variable_name} 0\n")
    endif ()
endmacro()

macro(CREATE_CONFIGURATION_HEADER)
    list(SORT _XGUIPRO_CONFIG_FILE_VARIABLES)
    set(_file_contents "#ifndef CMAKECONFIG_H\n")
    set(_file_contents "${_file_contents}#define CMAKECONFIG_H\n\n")

    foreach (_variable_name ${_XGUIPRO_CONFIG_FILE_VARIABLES})
        _ADD_CONFIGURATION_LINE_TO_HEADER_STRING(_file_contents ${_variable_name} ${_variable_name})
    endforeach ()
    set(_file_contents "${_file_contents}\n#endif /* CMAKECONFIG_H */\n")

    file(WRITE "${CMAKE_BINARY_DIR}/cmakeconfig.h.tmp" "${_file_contents}")
    execute_process(COMMAND ${CMAKE_COMMAND}
        -E copy_if_different
        "${CMAKE_BINARY_DIR}/cmakeconfig.h.tmp"
        "${CMAKE_BINARY_DIR}/cmakeconfig.h"
    )
    file(REMOVE "${CMAKE_BINARY_DIR}/cmakeconfig.h.tmp")
endmacro()

macro(XGUIPRO_CHECK_HAVE_INCLUDE _variable _header)
    check_include_file(${_header} ${_variable}_value)
    SET_AND_EXPOSE_TO_BUILD(${_variable} ${${_variable}_value})
endmacro()

macro(XGUIPRO_CHECK_HAVE_FUNCTION _variable _function)
    check_function_exists(${_function} ${_variable}_value)
    SET_AND_EXPOSE_TO_BUILD(${_variable} ${${_variable}_value})
endmacro()

macro(XGUIPRO_CHECK_HAVE_SYMBOL _variable _symbol _header)
    check_symbol_exists(${_symbol} "${_header}" ${_variable}_value)
    SET_AND_EXPOSE_TO_BUILD(${_variable} ${${_variable}_value})
endmacro()

macro(XGUIPRO_CHECK_HAVE_STRUCT _variable _struct _member _header)
    check_struct_has_member(${_struct} ${_member} "${_header}" ${_variable}_value)
    SET_AND_EXPOSE_TO_BUILD(${_variable} ${${_variable}_value})
endmacro()

macro(XGUIPRO_CHECK_SOURCE_COMPILES _variable _source)
    check_cxx_source_compiles("${_source}" ${_variable}_value)
    SET_AND_EXPOSE_TO_BUILD(${_variable} ${${_variable}_value})
endmacro()

option(ENABLE_EXPERIMENTAL_FEATURES "Enable experimental features" OFF)
SET_AND_EXPOSE_TO_BUILD(ENABLE_EXPERIMENTAL_FEATURES ${ENABLE_EXPERIMENTAL_FEATURES})
