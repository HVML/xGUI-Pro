file(MAKE_DIRECTORY ${xGUIPro_DERIVED_SOURCES_DIR}/minigui)

configure_file(minigui/BuildRevision.h.in ${xGUIPro_DERIVED_SOURCES_DIR}/minigui/BuildRevision.h)

list(APPEND xguipro_PRIVATE_INCLUDE_DIRECTORIES
    "${xGUIPro_DERIVED_SOURCES_DIR}/minigui"
)

list(APPEND xguipro_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

list(APPEND xguipro_SOURCES
    ${xGUIPro_DERIVED_SOURCES_DIR}/minigui/BrowserMarshal.c
    minigui/Common.c
    minigui/BrowserTab.c
    minigui/BrowserTab.h
    minigui/BrowserPane.c
    minigui/BrowserPane.h
    minigui/BrowserPlainWindow.c
    minigui/BrowserPlainWindow.h
    minigui/BrowserTabbedWindow.c
    minigui/BrowserTabbedWindow.h
    minigui/BrowserLayoutContainer.h
    minigui/BrowserLayoutContainer.c
    minigui/BrowserPaneContainer.h
    minigui/BrowserPaneContainer.c
    minigui/BrowserTabContainer.h
    minigui/BrowserTabContainer.c
    minigui/PurcmcCallbacks.c
    minigui/PurcmcCallbacks.h
    minigui/LayouterWidgets.c
    minigui/LayouterWidgets.h
    minigui/main.c
    minigui/FloatingToolWindow.c
    minigui/PopupTipWindow.c
    minigui/AuthWindow.c
)

list(APPEND xguipro_LIBRARIES
    ${LIBSOUP_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    CairoHBD::CairoHBD
    MiniGUI::MiniGUI
    MiniGUI::mGEff
)

if (HAVE_LIBSSL)
    list(APPEND xguipro_LIBRARIES ${OPENSSL_LIBRARIES})
endif (HAVE_LIBSSL)

add_custom_command(
    OUTPUT ${xGUIPro_DERIVED_SOURCES_DIR}/minigui/BrowserMarshal.c
           ${xGUIPro_DERIVED_SOURCES_DIR}/minigui/BrowserMarshal.h
    MAIN_DEPENDENCY ${XGUIPRO_BIN_DIR}/minigui/browser-marshal.list
    COMMAND glib-genmarshal --prefix=browser_marshal ${XGUIPRO_BIN_DIR}/minigui/browser-marshal.list --body --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/minigui/BrowserMarshal.c
    COMMAND glib-genmarshal --prefix=browser_marshal ${XGUIPRO_BIN_DIR}/minigui/browser-marshal.list --header --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/minigui/BrowserMarshal.h
    VERBATIM)
