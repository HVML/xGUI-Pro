set_property(DIRECTORY . PROPERTY FOLDER "bin")

include(GlobalCommon)
include(target/xGUIPro)
include(MyMacros.cmake)

set(XGUIPRO_WEB_EXTENSIONS_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/webext")
set(XGUIPRO_WEB_EXTENSIONS_INSTALL_DIR "${LIB_INSTALL_DIR}/xguipro")

configure_file(xguipro-version.h.in ${xGUIPro_DERIVED_SOURCES_DIR}/xguipro-version.h)
configure_file(xguipro-features.h.in ${xGUIPro_DERIVED_SOURCES_DIR}/xguipro-features.h)

add_subdirectory(webext)

XGUIPRO_EXECUTABLE_DECLARE(xguipro)

list(APPEND xguipro_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${xGUIPro_DERIVED_SOURCES_DIR}"
    "${XGUIPRO_LIB_DIR}"
    "${XGUIPRO_BIN_DIR}"
)

list(APPEND xguipro_SYSTEM_INCLUDE_DIRECTORIES
    "${PurC_INCLUDE_DIR}"
)

list(APPEND xguipro_DEFINITIONS
)

XGUIPRO_EXECUTABLE(xguipro)

set(xguipro_PLATFORM_INDEPENDENT_DIRS
    "purcmc"
    "layouter"
    "schema"
    "sd"
    "utils"
)

APPEND_ALL_SOURCE_FILES_IN_DIRLIST(xguipro_SOURCES
        "${xguipro_PLATFORM_INDEPENDENT_DIRS}")

list(APPEND xguipro_SOURCES
)

set(xguipro_LIBRARIES
    xGUIPro::xGUIPro
    WebKit::JSC
    WebKit::WebKit
    DOMRuler::DOMRuler
    PurC::PurC
    pthread
)

XGUIPRO_INCLUDE_CONFIG_FILES_IF_EXISTS()

XGUIPRO_COMPUTE_SOURCES(xguipro)
XGUIPRO_FRAMEWORK(xguipro)

install(TARGETS xguipro DESTINATION "${EXEC_INSTALL_DIR}/")

XGUIPRO_EXECUTABLE_DECLARE(test_layouter)

list(APPEND test_layouter_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${xGUIPro_DERIVED_SOURCES_DIR}"
    "${XGUIPRO_LIB_DIR}"
    "${XGUIPRO_BIN_DIR}"
)

list(APPEND test_layouter_SYSTEM_INCLUDE_DIRECTORIES
    "${GLIB_INCLUDE_DIRS}"
    "${PurC_INCLUDE_DIR}"
)

list(APPEND test_layouter_DEFINITIONS
)

XGUIPRO_EXECUTABLE(test_layouter)

set(test_layouter_PLATFORM_INDEPENDENT_DIRS
    "layouter"
)

APPEND_ALL_SOURCE_FILES_IN_DIRLIST(test_layouter_SOURCES
        "${test_layouter_PLATFORM_INDEPENDENT_DIRS}")

list(APPEND test_layouter_SOURCES
    "test_layouter.c"
)

set(test_layouter_LIBRARIES
    xGUIPro::xGUIPro
    DOMRuler::DOMRuler
    PurC::PurC
    pthread
)

XGUIPRO_COMPUTE_SOURCES(test_layouter)
XGUIPRO_FRAMEWORK(test_layouter)

set(test_files_FILES
    "${CMAKE_BINARY_DIR}/test_layouter.html"
)

set(test_files_SOURCES
    "layout-panes.html"
    "test_layouter.html"
)

file(COPY ${test_files_SOURCES} DESTINATION "${CMAKE_BINARY_DIR}")

add_custom_target(test_files DEPENDS ${test_files_FILES})
add_dependencies(test_layouter test_files)

