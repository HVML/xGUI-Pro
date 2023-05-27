/*
** BrowserPane.c -- The implementation of BrowserPane.
**
** Copyright (C) 2023 FMSoft <http://www.fmsoft.cn>
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
** MERCHANPANEILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif
#include "main.h"
#include "BrowserWindow.h"
#include "BrowserTab.h"

#include <string.h>

enum {
    PROP_0,

    PROP_PARENT_WINDOW,

    N_PROPERTIES
};

struct _BrowserWindow {
    GObject parent;

    WebKitWebContext *webContext;
    BrowserTab *activeTab;

    HWND parentWindow;
    HWND hwnd;
    HWND uriEntry;
    HWND propsheet;

};

struct _BrowserWindowClass {
    GObjectClass parent;
};

G_DEFINE_TYPE(BrowserWindow, browser_window, G_TYPE_OBJECT)

static void browser_window_init(BrowserWindow *window)
{
}

static LRESULT BrowserWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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

static void uriEntryCallback(HWND hwnd, LINT id, int nc, DWORD add_data)
{
//    BrowserWindow *window = (BrowserWindow *)GetWindowAdditionalData2(hwnd);
}

static void browserWindowConstructed(GObject *gObject)
{
    BrowserWindow *window = BROWSER_WINDOW(gObject);
    G_OBJECT_CLASS(browser_window_parent_class)->constructed(gObject);

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
    CreateInfo.dwStyle = WS_VISIBLE | WS_CAPTION ;
    CreateInfo.dwExStyle = WS_EX_NONE;
    CreateInfo.spCaption = BROWSER_DEFAULT_TITLE;
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = BrowserWindowProc;
    CreateInfo.lx = rc.left;
    CreateInfo.ty = rc.top;
    CreateInfo.rx = w;
    CreateInfo.by = h;
    CreateInfo.iBkColor = COLOR_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = parent;
    window->hwnd = CreateMainWindow (&CreateInfo);
    SetWindowAdditionalData2(window->hwnd, (DWORD)window);

    window->uriEntry = CreateWindow (CTRL_EDIT,
            "", WS_CHILD | WS_VISIBLE | WS_BORDER,
            IDC_URI_ENTRY,
            rc.left + IDC_ADDRESS_LEFT,
            rc.top + IDC_ADDRESS_TOP,
            rc.right - rc.left - 2 * IDC_ADDRESS_LEFT - 5,
            IDC_ADDRESS_HEIGHT,
            window->hwnd, 0);
    SetWindowAdditionalData2(window->uriEntry, (DWORD)window);
    SetNotificationCallback (window->uriEntry, uriEntryCallback);

    int top = rc.top + IDC_ADDRESS_TOP + IDC_ADDRESS_HEIGHT + 5;
    int height = rc.bottom - top;
    window->propsheet = CreateWindow (CTRL_PROPSHEET, NULL,
            WS_VISIBLE | PSS_COMPACTTAB,
            IDC_PROPSHEET,
            rc.left,
            top,
            w,
            height,
            window->hwnd, 0);
    SetWindowAdditionalData2(window->propsheet, (DWORD)window);

    ShowWindow(window->hwnd, SW_SHOWNORMAL);
}

static void browserWindowSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    BrowserWindow *window = BROWSER_WINDOW(object);

    switch (propId) {
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

static void browserWindowDispose(GObject *gObject)
{
    //BrowserWindow *window = BROWSER_WINDOW(gObject);

    G_OBJECT_CLASS(browser_window_parent_class)->dispose(gObject);
}

static void browserWindowFinalize(GObject *gObject)
{
    BrowserWindow *window = BROWSER_WINDOW(gObject);

    if (window->webContext) {
        g_object_unref(window->webContext);
    }

    G_OBJECT_CLASS(browser_window_parent_class)->finalize(gObject);
}

static void browser_window_class_init(BrowserWindowClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserWindowConstructed;
    gobjectClass->set_property = browserWindowSetProperty;
    gobjectClass->dispose = browserWindowDispose;
    gobjectClass->finalize = browserWindowFinalize;

    g_object_class_install_property(
        gobjectClass,
        PROP_PARENT_WINDOW,
        g_param_spec_pointer(
            "parent-window",
            "parent window",
            "The parent window",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

BrowserWindow* browser_window_new(HWND parent, WebKitWebContext *webContext)
{
    BrowserWindow *window = BROWSER_WINDOW(g_object_new(BROWSER_TYPE_WINDOW,
                "parent-window", parent,
                NULL));

    if (webContext) {
        window->webContext = g_object_ref(webContext);
    }

    return window;
}

HWND browser_window_hwnd(BrowserWindow *window)
{
    g_return_if_fail(BROWSER_IS_WINDOW(window));
    return window->hwnd;
}

WebKitWebContext* browser_window_get_web_context(BrowserWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_WINDOW(window), NULL);

    return window->webContext;
}


void browser_window_append_view(BrowserWindow *window, WebKitWebView * webView)
{
}


void browser_window_load_uri(BrowserWindow *window, const char *uri)
{
}


void browser_window_load_session(BrowserWindow *window, const char *sessionFile)
{
}


void browser_window_set_background_color(BrowserWindow *window, GAL_Color *color)
{
}


WebKitWebView *browser_window_get_or_create_web_view_for_automation(
        BrowserWindow *window)
{
    return NULL;
}


WebKitWebView *browser_window_create_web_view_in_new_tab_for_automation(
        BrowserWindow *window)
{
    return NULL;
}


HWND browser_window_webview_create(WebKitWebView *webView,
        WebKitNavigationAction *action, BrowserWindow *window)
{
    return NULL;
}


