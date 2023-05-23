/*
** BrowserPlainWindow.c -- The implementation of BrowserPlainWindow.
**
** Copyright (C) 2022, 2023 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
**
** This file is part of xGUI Pro, an advanced HVML renderer.
**
** xGUI Pro is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** xGUI Pro is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif
#include "main.h"
#include "BrowserPlainWindow.h"

#include "BrowserPane.h"
#include <string.h>

struct _BrowserPlainWindow {
    GObject parent;

    WebKitWebContext *webContext;
    BrowserPane *browserPane;

    gchar *sessionFile;

    gchar *name;
    gchar *title;
};

struct _BrowserPlainWindowClass {
    GObjectClass parent;
};

static const gdouble minimumZoomLevel = 0.5;
static const gdouble maximumZoomLevel = 3;
static const gdouble defaultZoomLevel = 1;
static const gdouble zoomStep = 1.2;

G_DEFINE_TYPE(BrowserPlainWindow, browser_plain_window,
        G_TYPE_OBJECT)

static void browser_plain_window_init(BrowserPlainWindow *window)
{
}

static void browser_plain_window_class_init(BrowserPlainWindowClass *klass)
{
}

/* Public API. */
HWND
browser_plain_window_new(HWND parent, WebKitWebContext *webContext,
        const char *name, const char *title)
{
    return NULL;
}

WebKitWebContext *
browser_plain_window_get_web_context(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);

    return window->webContext;
}

void browser_plain_window_set_view(BrowserPlainWindow *window,
        WebKitWebView *webView)
{
}

WebKitWebView *browser_plain_window_get_view(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);
    if (window->browserPane)
        return browser_pane_get_web_view(window->browserPane);

    return NULL;
}

void browser_plain_window_load_uri(BrowserPlainWindow *window, const char *uri)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    g_return_if_fail(window->browserPane);
    g_return_if_fail(uri);

    browser_pane_load_uri(window->browserPane, uri);
}

void browser_plain_window_load_session(BrowserPlainWindow *window,
        const char *sessionFile)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    g_return_if_fail(sessionFile);

    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);

    window->sessionFile = g_strdup(sessionFile);
    GKeyFile *session = g_key_file_new();
    GError *error = NULL;
    if (!g_key_file_load_from_file(session, sessionFile, G_KEY_FILE_NONE,
                &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Failed to open session file: %s", error->message);
        g_error_free(error);
        webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
        g_key_file_free(session);
        return;
    }

    gsize groupCount;
    gchar **groups = g_key_file_get_groups(session, &groupCount);
    if (!groupCount || groupCount > 1) {
        webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
        g_strfreev(groups);
        g_key_file_free(session);
        return;
    }

    WebKitWebView *previousWebView = NULL;
    WebKitWebViewSessionState *state = NULL;
    gchar *base64 = g_key_file_get_string(session, "plainwin", "state", NULL);
    if (base64) {
        gsize stateDataLength;
        guchar *stateData = g_base64_decode(base64, &stateDataLength);
        GBytes *bytes = g_bytes_new_take(stateData, stateDataLength);
        state = webkit_web_view_session_state_new(bytes);
        g_bytes_unref(bytes);
        g_free(base64);
    }

    if (!webView) {
        webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
            "web-context",
            webkit_web_view_get_context(previousWebView),
            "settings",
            webkit_web_view_get_settings(previousWebView),
            "user-content-manager",
            webkit_web_view_get_user_content_manager(previousWebView),
#if WEBKIT_CHECK_VERSION(2, 30, 0)
            "website-policies",
            webkit_web_view_get_website_policies(previousWebView),
#endif
            NULL));
        browser_plain_window_set_view(window, webView);
    }

    if (state) {
        webkit_web_view_restore_session_state(webView, state);
        webkit_web_view_session_state_unref(state);
    }

    WebKitBackForwardList *bfList =
        webkit_web_view_get_back_forward_list(webView);
    WebKitBackForwardListItem *item =
        webkit_back_forward_list_get_current_item(bfList);
    if (item)
        webkit_web_view_go_to_back_forward_list_item(webView, item);
    else
        webkit_web_view_load_uri(webView, "about:blank");

    previousWebView = webView;
    webView = NULL;

    g_strfreev(groups);
    g_key_file_free(session);
}

const char* browser_plain_window_get_name(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);

    return window->name;
}

void browser_plain_window_set_title(BrowserPlainWindow *window,
        const char *title)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));

    if (window->title) {
        g_free(window->title);
        window->title = NULL;
    }

    if (title) {
        window->title = g_strdup(title);
    }
}

void browser_plain_window_set_background_color(BrowserPlainWindow *window,
        GAL_Color *rgba)
{
}

