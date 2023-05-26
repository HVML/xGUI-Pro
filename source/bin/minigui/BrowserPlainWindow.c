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

enum {
    PROP_0,

    PROP_TITLE,
    PROP_NAME,

    N_PROPERTIES
};

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

static LRESULT PlainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case MSG_CREATE:
            break;

        case MSG_CLOSE:
            DestroyAllControls (hWnd);
            DestroyMainWindow (hWnd);
            return 0;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

static void browser_plain_window_init(BrowserPlainWindow *window)
{
}

static void browserPlainWindowConstructed(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);
    G_OBJECT_CLASS(browser_plain_window_parent_class)->constructed(gObject);

    MAINWINCREATE CreateInfo;
    CreateInfo.dwStyle = WS_VISIBLE | WS_CAPTION ;
    CreateInfo.dwExStyle = WS_EX_NONE;
    CreateInfo.spCaption = window->title ? window->title : BROWSER_DEFAULT_TITLE;
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = PlainWindowProc;
    CreateInfo.lx = 0;
    CreateInfo.ty = 0;
    CreateInfo.rx = g_screenRect.right;
    CreateInfo.by = g_screenRect.bottom;
    CreateInfo.iBkColor = COLOR_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = g_hMainWnd;
    window->hwnd = CreateMainWindow (&CreateInfo);
    ShowWindow(window->hwnd, SW_SHOWNORMAL);
}

static void browserPlainWindowSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(object);

    switch (propId) {
    case PROP_TITLE:
        {
            gchar *title = g_value_get_pointer(value);
            if (title) {
                window->title = g_strdup(title);
            }
        }
        break;
    case PROP_NAME:
        {
            gchar *name = g_value_get_pointer(value);
            if (name) {
                window->name = g_strdup(name);
            }
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
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
    gobjectClass->constructed = browserPlainWindowConstructed;
    gobjectClass->set_property = browserPlainWindowSetProperty;
    gobjectClass->dispose = browserPlainWindowDispose;
    gobjectClass->finalize = browserPlainWindowFinalize;

    g_object_class_install_property(
        gobjectClass,
        PROP_TITLE,
        g_param_spec_pointer(
            "title",
            "PlainWindow title",
            "The plain window title",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(
        gobjectClass,
        PROP_NAME,
        g_param_spec_pointer(
            "name",
            "PlainWindow name",
            "The plain window name",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

/* Public API. */
HWND
browser_plain_window_new(HWND parent, WebKitWebContext *webContext,
        const char *name, const char *title)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(webContext), NULL);
    assert(parent);

    BrowserPlainWindow *window =
          BROWSER_PLAIN_WINDOW(g_object_new(BROWSER_TYPE_PLAIN_WINDOW,
                      "name", name,
                      "title", title,
                      NULL));

    window->webContext = g_object_ref(webContext);
    window->parentWindow = parent;

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

static void webViewTitleChanged(WebKitWebView *webView,
        GParamSpec *pspec, BrowserPlainWindow *window)
{
    const char *title = webkit_web_view_get_title(webView);
    if (!title)
        title = window->title ? window->title : BROWSER_DEFAULT_TITLE;
    char *privateTitle = NULL;
    if (webkit_web_view_is_controlled_by_automation(webView))
        privateTitle = g_strdup_printf("[Automation] %s", title);
    else if (webkit_web_view_is_ephemeral(webView))
        privateTitle = g_strdup_printf("[Private] %s", title);
    SetWindowCaption(window->hwnd,
            privateTitle ? privateTitle : title);
    g_free(privateTitle);
}

static void browserPlainWindowSetupSignalHandlers(BrowserPlainWindow *window)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    webViewTitleChanged(webView, NULL, window);
    g_signal_connect(webView, "notify::title",
            G_CALLBACK(webViewTitleChanged), window);
}

void browser_plain_window_set_view(BrowserPlainWindow *window, WebKitWebViewParam *param)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));

    if (window->browserPane) {
        g_assert(browser_pane_get_web_view(window->browserPane));
        g_warning("Only one webView allowed in a plainwin.");
        return;
    }

    param->webViewParent = window->hwnd;

    RECT rc;
    GetClientRect(window->hwnd, &rc);
    SetRect(&param->webViewRect, 0, 0, RECTW(rc), RECTH(rc));
    window->browserPane = (BrowserPane*)browser_pane_new(param);
    g_signal_connect_after(window->browserPane->webView, "close", G_CALLBACK(webViewClose), window);

    ShowWindow(window->hwnd, SW_SHOW);

    browserPlainWindowSetupSignalHandlers(window);
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

