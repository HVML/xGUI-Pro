file(MAKE_DIRECTORY ${xGUIPro_DERIVED_SOURCES_DIR}/gtk)

configure_file(gtk/BuildRevision.h.in ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BuildRevision.h)

list(APPEND xguipro_PRIVATE_INCLUDE_DIRECTORIES
    "${xGUIPro_DERIVED_SOURCES_DIR}/gtk"
)

list(APPEND xguipro_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

list(APPEND xguipro_SOURCES
    ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.c
    gtk/BrowserCellRendererVariant.c
    gtk/BrowserCellRendererVariant.h
    gtk/BrowserDownloadsBar.c
    gtk/BrowserDownloadsBar.h
    gtk/BrowserSearchBox.c
    gtk/BrowserSearchBox.h
    gtk/BrowserSettingsDialog.c
    gtk/BrowserSettingsDialog.h
    gtk/BrowserTab.c
    gtk/BrowserTab.h
    gtk/BrowserWindow.c
    gtk/BrowserWindow.h
    gtk/PurcmcCallbacks.c
    gtk/PurcmcCallbacks.h
    gtk/HVMLURISchema.c
    gtk/HVMLURISchema.h
    gtk/main.c
)

list(APPEND xguipro_LIBRARIES
    ${LIBSOUP_LIBRARIES}
    GTK::GTK
)

if (HAVE_LIBSSL)
    list(APPEND xguipro_LIBRARIES ${OPENSSL_LIBRARIES})
endif (HAVE_LIBSSL)

add_custom_command(
    OUTPUT ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.c
           ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.h
    MAIN_DEPENDENCY ${XGUIPRO_BIN_DIR}/gtk/browser-marshal.list
    COMMAND glib-genmarshal --prefix=browser_marshal ${XGUIPRO_BIN_DIR}/gtk/browser-marshal.list --body --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.c
    COMMAND glib-genmarshal --prefix=browser_marshal ${XGUIPRO_BIN_DIR}/gtk/browser-marshal.list --header --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.h
    VERBATIM)
