XGUIPRO_INCLUDE_CONFIG_FILES_IF_EXISTS()

set(hvml_js_SOURCES
    "${XGUIPRO_BIN_DIR}/webext/hvml.js"
)

set(hvml_js_FILES
    "${XGUIPRO_WEB_EXTENSIONS_OUTPUT_DIR}/hvml.js"
)

add_custom_command(
    OUTPUT ${hvml_js_FILES}
    DEPENDS ${hvml_js_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy ${hvml_js_SOURCES} ${XGUIPRO_WEB_EXTENSIONS_OUTPUT_DIR}
    COMMENT "Copy hvml.js..."
    VERBATIM)

add_custom_target(hvml_js DEPENDS ${hvml_js_FILES})
add_dependencies(WebExtensionHVML hvml_js)

install(FILES ${hvml_js_SOURCES} DESTINATION "${XGUIPRO_WEB_EXTENSIONS_INSTALL_DIR}")

