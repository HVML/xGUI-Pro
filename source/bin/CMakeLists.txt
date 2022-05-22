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
)

list(APPEND xguipro_DEFINITIONS
)

XGUIPRO_EXECUTABLE(xguipro)

set(xguipro_PLATFORM_INDEPENDENT_DIRS
    "purcmc"
)

APPEND_ALL_SOURCE_FILES_IN_DIRLIST(xguipro_SOURCES "${xguipro_PLATFORM_INDEPENDENT_DIRS}")

list(APPEND xguipro_SOURCES
)

set(xguipro_LIBRARIES
    xGUIPro::xGUIPro
    WebKit::JSC
    WebKit::WebKit
    PurC::PurC
    pthread
)

XGUIPRO_INCLUDE_CONFIG_FILES_IF_EXISTS()

XGUIPRO_COMPUTE_SOURCES(xguipro)
XGUIPRO_FRAMEWORK(xguipro)

install(TARGETS xguipro DESTINATION "${EXEC_INSTALL_DIR}/")

