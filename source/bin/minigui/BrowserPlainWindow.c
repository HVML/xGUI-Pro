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
#include "Common.h"
#include <string.h>

struct _BrowserPlainWindow {
    GObject parent;

    WebKitWebContext *webContext;
    BrowserPane *browserPane;

    gchar *name;
    gchar *title;

    HWND parentWindow;
    HWND hwnd;
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

static void browserPlainWindowDispose(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);

    if (window->parentWindow) {
        window->parentWindow = NULL;
    }

    G_OBJECT_CLASS(browser_plain_window_parent_class)->dispose(gObject);
}

static void browserPlainWindowFinalize(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);

    g_signal_handlers_disconnect_matched(window->webContext,
            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
    g_object_unref(window->webContext);

    if (window->name) {
        g_free(window->name);
        window->name = NULL;
    }

    if (window->title) {
        g_free(window->title);
        window->title = NULL;
    }

    G_OBJECT_CLASS(browser_plain_window_parent_class)->finalize(gObject);
}

static void browser_plain_window_class_init(BrowserPlainWindowClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->dispose = browserPlainWindowDispose;
    gobjectClass->finalize = browserPlainWindowFinalize;
}

/* Public API. */
HWND
browser_plain_window_new(HWND parent, WebKitWebContext *webContext,
        const char *name, const char *title)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(webContext), NULL);
    assert(parent);

    BrowserPlainWindow *window =
        BROWSER_PLAIN_WINDOW(g_object_new(BROWSER_TYPE_PLAIN_WINDOW, NULL));

    window->webContext = g_object_ref(webContext);
    window->parentWindow = parent;

    RECT rect;
    GetWindowRect(parent, &rect);

    window->hwnd = CreateWindow (CTRL_STATIC, "",
                          WS_VISIBLE | SS_CENTER, IDC_PLAIN_WINDOW,
                          rect.left, rect.top, RECTW(rect), RECTH(rect), parent, 0);
    if (name)
        window->name = g_strdup(name);

    if (title)
        window->title = g_strdup(title);

    return window->hwnd;
}

WebKitWebContext *
browser_plain_window_get_web_context(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);

    return window->webContext;
}

static void webViewClose(WebKitWebView *webView, BrowserPlainWindow *window)
{
    HWND hwnd = webkit_web_view_get_hwnd(webView);
    DestroyWindow(hwnd);
    DestroyWindow(window->hwnd);
}

void browser_plain_window_set_view(BrowserPlainWindow *window, WebKitWebViewParam *param)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));

    if (window->browserPane) {
        g_assert(browser_pane_get_web_view(window->browserPane));
        g_warning("Only one webView allowed in a plainwin.");
        return;
    }

    window->browserPane = (BrowserPane*)browser_pane_new(param);
    ShowWindow(window->hwnd, SW_SHOW);

    g_signal_connect_after(window->browserPane->webView, "close", G_CALLBACK(webViewClose), window);
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

