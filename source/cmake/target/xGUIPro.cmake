if (NOT TARGET xGUIPro::xGUIPro)
    if (NOT INTERNAL_BUILD)
        message(FATAL_ERROR "xGUIPro::xGUIPro target not found")
    endif ()

    # This should be moved to an if block if the Apple Mac/iOS build moves completely to CMake
    # Just assuming Windows for the moment
    add_library(xGUIPro::xGUIPro STATIC IMPORTED)
    set_target_properties(xGUIPro::xGUIPro PROPERTIES
        IMPORTED_LOCATION ${XGUIPRO_LIBRARIES_LINK_DIR}/xGUIPro${DEBUG_SUFFIX}.lib
    )
    set(xGUIPro_PRIVATE_FRAMEWORK_HEADERS_DIR "${CMAKE_SOURCE_DIR}/source/bin")
    target_include_directories(xGUIPro::xGUIPro INTERFACE
        ${xGUIPro_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
endif ()
