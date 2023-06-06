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
    PROP_PARENT_WINDOW,

    N_PROPERTIES
};

struct _BrowserPlainWindow {
    GObject parent;

    WebKitWebViewParam param;
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

enum {
    DESTROY,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(BrowserPlainWindow, browser_plain_window,
        G_TYPE_OBJECT)

static LRESULT PlainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case MSG_CREATE:
            xgui_window_inc();
            break;

        case MSG_CLOSE:
            {
                BrowserPlainWindow *window = (BrowserPlainWindow *)
                    GetWindowAdditionalData(hWnd);
                WebKitWebView *webView = browser_pane_get_web_view(
                        window->browserPane);
                webkit_web_view_try_close(webView);
            }
            return 0;

        case MSG_DESTROY:
            {
                BrowserPlainWindow *window = (BrowserPlainWindow *)
                    GetWindowAdditionalData(hWnd);
                g_signal_emit(window, signals[DESTROY], 0, NULL);
                g_object_unref(window);
                xgui_window_dec();
            }
            break;
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

    RECT rc;
    HWND parent;
    if (window->parentWindow) {
        parent = window->parentWindow;
        GetClientRect(parent, &rc);
    }
    else {
        parent = HWND_DESKTOP;
        rc = GetScreenRect();
    }
    int w = RECTW(rc);
    int h = RECTH(rc);

    MAINWINCREATE CreateInfo;
    CreateInfo.dwStyle = WS_CHILD | WS_VISIBLE;
    CreateInfo.dwExStyle = WS_EX_NONE;
    CreateInfo.spCaption = window->title ? window->title : BROWSER_DEFAULT_TITLE;
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = PlainWindowProc;
    CreateInfo.lx = 0;
    CreateInfo.ty = 0;
    CreateInfo.rx = w;
    CreateInfo.by = h;
    CreateInfo.iBkColor = COLOR_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = parent;
    window->hwnd = CreateMainWindow (&CreateInfo);
    if (window->hwnd != HWND_INVALID) {
        SetWindowAdditionalData(window->hwnd, (DWORD)window);
        ShowWindow(window->hwnd, SW_SHOWNORMAL);
    }
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
    case PROP_PARENT_WINDOW:
        {
            gpointer *p = g_value_get_pointer(value);
            if (p) {
                window->parentWindow = (HWND)p;
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

    if (window->webContext) {
        g_signal_handlers_disconnect_matched(window->webContext,
                G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
        g_object_unref(window->webContext);
    }

    if (window->browserPane) {
        g_object_unref(window->browserPane);
        window->browserPane = NULL;
    }

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

    g_object_class_install_property(
            gobjectClass,
            PROP_PARENT_WINDOW,
            g_param_spec_pointer(
                "parent-window",
                "parent window",
                "The parent window",
                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

      signals[DESTROY] =
          g_signal_new("destroy",
                       G_TYPE_FROM_CLASS(klass),
                       G_SIGNAL_RUN_LAST,
                       0,
                       0, 0,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

/* Public API. */
BrowserPlainWindow *
browser_plain_window_new(HWND parent, WebKitWebContext *webContext,
        const char *name, const char *title)
{
    BrowserPlainWindow *window =
          BROWSER_PLAIN_WINDOW(g_object_new(BROWSER_TYPE_PLAIN_WINDOW,
                      "name", name,
                      "title", title,
                      "parent-window", parent,
                      NULL));

    if (webContext) {
        window->webContext = g_object_ref(webContext);
    }

    return window;
}

HWND browser_plain_window_get_hwnd(BrowserPlainWindow *window)
{
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
    HWND hwnd = window->hwnd;
    DestroyAllControls(hwnd);
    DestroyMainWindow(hwnd);
    MainWindowCleanup(hwnd);
}

#if 0
static char *getExternalURI(const char *uri)
{
    /* From the user point of view we support about: prefix. */
    if (uri && g_str_has_prefix(uri, BROWSER_ABOUT_SCHEME))
        return g_strconcat("about", uri + strlen(BROWSER_ABOUT_SCHEME), NULL);

    return g_strdup(uri);
}

static void webViewURIChanged(WebKitWebView *webView, GParamSpec *pspec,
        BrowserPlainWindow *window)
{
    char *externalURI = getExternalURI(webkit_web_view_get_uri(webView));
    g_free(externalURI);
}
#endif

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

static WebKitWebView *webViewCreate(WebKitWebView *webView,
        WebKitNavigationAction *navigation, BrowserPlainWindow *window)
{
    BrowserPlainWindow *newWindow = browser_plain_window_new(NULL,
            window->webContext, window->name, window->title);
    WebKitWebViewParam param = window->param;
    param.relatedView = webView;
    browser_plain_window_set_view(newWindow, &param);
    WebKitWebView *newWebView = browser_plain_window_get_view(newWindow);
    webkit_web_view_set_settings(newWebView,
            webkit_web_view_get_settings(webView));
    return newWebView;
}

static void browserPlainWindowSetupSignalHandlers(BrowserPlainWindow *window)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
#if 0
    webViewURIChanged(webView, NULL, window);
    g_signal_connect(webView, "notify::uri",
            G_CALLBACK(webViewURIChanged), window);
#endif
    webViewTitleChanged(webView, NULL, window);
    g_signal_connect(webView, "notify::title",
            G_CALLBACK(webViewTitleChanged), window);
    g_signal_connect(webView, "create",
            G_CALLBACK(webViewCreate), window);
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
    param->webContext = window->webContext;

    RECT rc;
    GetClientRect(window->hwnd, &rc);
    SetRect(&param->webViewRect, 0, 0, RECTW(rc), RECTH(rc));
    window->browserPane = (BrowserPane*)browser_pane_new(param);
    g_signal_connect_after(window->browserPane->webView, "close", G_CALLBACK(webViewClose), window);

    window->param = *param;
    ShowWindow(window->hwnd, SW_SHOW);
    SetFocus(window->hwnd);

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

