file(MAKE_DIRECTORY ${xGUIPro_DERIVED_SOURCES_DIR}/gtk)

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
    gtk/main.c
)

add_custom_command(
    OUTPUT ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.c
           ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.h
    MAIN_DEPENDENCY ${XGUIPRO_BIN_DIR}/gtk/browser-marshal.list
    COMMAND glib-genmarshal --prefix=browser_marshal ${XGUIPRO_BIN_DIR}/gtk/browser-marshal.list --body --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.c
    COMMAND glib-genmarshal --prefix=browser_marshal ${XGUIPRO_BIN_DIR}/gtk/browser-marshal.list --header --skip-source > ${xGUIPro_DERIVED_SOURCES_DIR}/gtk/BrowserMarshal.h
    VERBATIM)
