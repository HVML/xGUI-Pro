XGUIPRO_INCLUDE_CONFIG_FILES_IF_EXISTS()

set(XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR "${XGUIPRO_WEB_EXTENSIONS_OUTPUT_DIR}/assets")
set(XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR "${XGUIPRO_WEB_EXTENSIONS_INSTALL_DIR}/assets")

set(assets_SOURCES
    "${XGUIPRO_BIN_DIR}/webext/assets/hvml.js"
)

set(assets_FILES
    "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/hvml.js"
)

add_custom_command(
    OUTPUT ${assets_FILES}
    DEPENDS ${assets_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy ${assets_SOURCES} ${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}
    COMMENT "Copy assets..."
    VERBATIM)

add_custom_target(assets DEPENDS ${assets_FILES})
add_dependencies(WebExtensionHVML assets)

install(FILES ${assets_SOURCES} DESTINATION "${XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR}")

