set(xGUIPro_DIR "${TOOLS_DIR}/xGUIPro/gtk")

file(MAKE_DIRECTORY ${xGUIPro_DERIVED_SOURCES_DIR})

set(xGUIPro_SOURCES
    ${xGUIPro_DERIVED_SOURCES_DIR}/BrowserMarshal.c
    BrowserCellRendererVariant.c
    BrowserCellRendererVariant.h
    BrowserDownloadsBar.c
    BrowserDownloadsBar.h
    BrowserSearchBox.c
    BrowserSearchBox.h
    BrowserSettingsDialog.c
    BrowserSettingsDialog.h
    BrowserTab.c
    BrowserTab.h
    BrowserWindow.c
    BrowserWindow.h
    main.c
)

set(xGUIPro_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${xGUIPro_DERIVED_SOURCES_DIR}
    ${WebKit2Gtk_DERIVED_SOURCES_DIR}
    ${WebKit2Gtk_FRAMEWORK_HEADERS_DIR}
    ${WebKit2Gtk_FRAMEWORK_HEADERS_DIR}/webkit2gtk-${WEBKITGTK_API_VERSION}
    ${CMAKE_SOURCE_DIR}/Source
)

set(xGUIPro_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

set(xGUIPro_PRIVATE_LIBRARIES
    ${GLIB_LIBRARIES}
    ${JavaScriptCore_LIBRARY_NAME}
    ${LIBSOUP_LIBRARIES}
    GTK::GTK
    WebKit::WebKit
)

set(xGUIPro_DEPENDENCIES
    NetworkProcess
    WebProcess
)

add_custom_command(
    OUTPUT ${xGUIPro_DERIVED_SOURCES_DIR}/BrowserMarshal.c
           ${xGUIPro_DERIVED_SOURCES_DIR}/BrowserMarshal.h
    MAIN_DEPENDENCY ${xGUIPro_DIR}/browser-marshal.list
    COMMAND glib-genmarshal --prefix=browser_marshal ${xGUIPro_DIR}/browser-marshal.list --body --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/BrowserMarshal.c
    COMMAND glib-genmarshal --prefix=browser_marshal ${xGUIPro_DIR}/browser-marshal.list --header --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/BrowserMarshal.h
    VERBATIM)

if (USE_GTK4)
    set(xGUIPro_PRIVATE_DEFINITIONS GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_0)
else ()
    set(xGUIPro_PRIVATE_DEFINITIONS GDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_22)
endif ()

if (DEVELOPER_MODE)
    list(APPEND xGUIPro_PRIVATE_DEFINITIONS -DWEBKIT_INJECTED_BUNDLE_PATH="${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif ()

WEBKIT_EXECUTABLE_DECLARE(xGUIPro)
WEBKIT_EXECUTABLE(xGUIPro)
WEBKIT_ADD_TARGET_CXX_FLAGS(xGUIPro -Wno-unused-parameter)

install(TARGETS xGUIPro DESTINATION "${LIBEXEC_INSTALL_DIR}")
