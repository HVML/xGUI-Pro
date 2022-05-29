XGUIPRO_INCLUDE_CONFIG_FILES_IF_EXISTS()

configure_file(assets/hvml.js.in ${xGUIPro_DERIVED_SOURCES_DIR}/webext/hvml.js @ONLY)

set(XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR "${XGUIPRO_WEB_EXTENSIONS_OUTPUT_DIR}/assets")
set(XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR "${XGUIPRO_WEB_EXTENSIONS_INSTALL_DIR}/assets")

set(assets_SOURCES
    "${xGUIPro_DERIVED_SOURCES_DIR}/webext/hvml.js"
)

set(bootstrap_DIRECTORY
    "assets/bootstrap-5.1.3-dist"
)

set(assets_FILES
    "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/hvml.js"
)

add_custom_command(
    OUTPUT ${assets_FILES}
    DEPENDS ${assets_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${assets_SOURCES} ${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}
    COMMENT "Copy assets..."
    VERBATIM)

add_custom_target(assets DEPENDS ${assets_FILES})
add_dependencies(WebExtensionHVML assets)

file(ARCHIVE_EXTRACT
        INPUT "${XGUIPRO_BIN_DIR}/webext/assets/bootstrap-5.1.3-dist.zip"
        DESTINATION "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}"
        VERBOSE)

file(ARCHIVE_EXTRACT
        INPUT "${XGUIPRO_BIN_DIR}/webext/assets/bootstrap-icons-1.8.3.zip"
        DESTINATION "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}"
        VERBOSE)

install(FILES ${assets_SOURCES}
        DESTINATION ${XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR})

install(DIRECTORY "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/bootstrap-5.1.3-dist"
        DESTINATION ${XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR})

install(DIRECTORY "${XGUIPRO_WEBEXT_ASSETS_OUTPUT_DIR}/bootstrap-icons-1.8.3"
        DESTINATION ${XGUIPRO_WEBEXT_ASSETS_INSTALL_DIR})

