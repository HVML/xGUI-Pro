set(WebKitWebExtension_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${xGUIPro_DERIVED_SOURCES_DIR}"
    "${XGUIPRO_LIB_DIR}"
    "${XGUIPRO_BIN_DIR}"
)

set(WebKitWebExtension_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${WebKit_INCLUDE_DIRS}
)

set(WebKitWebExtension_DEFINITIONS
)

set(WebKitWebExtension_LIBRARIES
    xGUIPro::xGUIPro
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    WebKit::JSC
    WebKit::WebKit
)

set(WebExtensionHVML_SOURCES
    log.c
    glib/WebExtensionHVML.c
)

macro(ADD_WK2_WEB_EXTENSION extension_name)
    add_library(${extension_name} MODULE ${ARGN})
    add_dependencies(${extension_name} xguipro)
    set_property(
        TARGET ${extension_name}
        APPEND
        PROPERTY COMPILE_DEFINITIONS WEBEXT_COMPILATION
    )
    set_target_properties(${extension_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${XGUIPRO_WEB_EXTENSIONS_OUTPUT_DIR}"
    )
    target_compile_definitions(${extension_name} PUBLIC ${WebKitWebExtension_DEFINITIONS})
    target_include_directories(${extension_name} PUBLIC ${WebKitWebExtension_INCLUDE_DIRECTORIES})
    target_include_directories(${extension_name} SYSTEM PUBLIC ${WebKitWebExtension_SYSTEM_INCLUDE_DIRECTORIES})
    target_link_libraries(${extension_name} ${WebKitWebExtension_LIBRARIES})
endmacro()

ADD_WK2_WEB_EXTENSION(WebExtensionHVML ${WebExtensionHVML_SOURCES})

install(TARGETS WebExtensionHVML DESTINATION "${XGUIPRO_WEB_EXTENSIONS_INSTALL_DIR}")
