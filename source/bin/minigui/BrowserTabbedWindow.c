/*
** BrowserTabbedWindow.c -- The implementation of BrowserTabbedWindow.
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
#include "BrowserTabbedWindow.h"

#include "BrowserPane.h"
#include "BrowserTab.h"
#include <string.h>

enum {
    PROP_0,

    PROP_NAME,
    PROP_TITLE,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_PARENT_WINDOW,

    N_PROPERTIES
};

struct _BrowserTabbedWindow {
    GObject parent;

    WebKitWebContext *webContext;
    BrowserTab *activeTab;
    int activeIdx;

    unsigned nrViews;
    GSList *viewContainers;

    HWND parentWindow;
    HWND mainFixed;

#if 0
    HWND propsheet;
#endif

    gchar *name;
    gchar *title;
    gint width;
    gint height;
};

struct _BrowserTabbedWindowClass {
    GObjectClass parent;
};

G_DEFINE_TYPE(BrowserTabbedWindow, browser_tabbed_window, G_TYPE_OBJECT)

static void browser_tabbed_window_init(BrowserTabbedWindow *window)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
}

static LRESULT BrowserTabbedWindowProc(HWND hWnd, UINT message, WPARAM wParam,
        LPARAM lParam)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    switch (message) {
        case MSG_CREATE:
            xgui_window_inc();
            break;

        case MSG_CLOSE:
            DestroyAllControls (hWnd);
            DestroyMainWindow (hWnd);
            return 0;

        case MSG_DESTROY:
            xgui_window_dec();
            break;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

#if 0
static void propsheetCallback(HWND hWnd, LINT id, int nc, DWORD add_data)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    BrowserTabbedWindow *window = (BrowserTabbedWindow *)GetWindowAdditionalData(hWnd);
    switch (nc) {
        case PSN_ACTIVE_CHANGED:
            {
                int idx = SendMessage(hWnd, PSM_GETACTIVEINDEX, 0, 0);
                window->activeIdx = idx;
            }
            break;
        default:
            break;
    }
}
#endif

static void browserTabbedWindowConstructed(GObject *gObject)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(gObject);
    G_OBJECT_CLASS(browser_tabbed_window_parent_class)->constructed(gObject);

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

    int w = window->width > 0 ? window->width : RECTW(rc);
    int h = window->height > 0 ? window->height : RECTH(rc);

    MAINWINCREATE CreateInfo;
    CreateInfo.dwStyle = WS_VISIBLE | WS_CAPTION ;
    CreateInfo.dwExStyle = WS_EX_NONE;
    CreateInfo.spCaption = window->title ? window->title : BROWSER_DEFAULT_TITLE;
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = BrowserTabbedWindowProc;
    CreateInfo.lx = rc.left;
    CreateInfo.ty = rc.top;
    CreateInfo.rx = w;
    CreateInfo.by = h;
    CreateInfo.iBkColor = COLOR_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = parent;
    window->mainFixed = CreateMainWindow (&CreateInfo);
    SetWindowAdditionalData(window->mainFixed, (DWORD)window);

#if 0
    GetClientRect(window->mainFixed, &rc);
    window->propsheet = CreateWindow (CTRL_PROPSHEET, NULL,
            WS_VISIBLE | PSS_COMPACTTAB,
            IDC_PROPSHEET,
            rc.left,
            rc.top,
            RECTW(rc),
            RECTH(rc),
            window->mainFixed, 0);
    SetWindowAdditionalData(window->propsheet, (DWORD)window);
    SetNotificationCallback(window->propsheet, propsheetCallback);
#endif

    ShowWindow(window->mainFixed, SW_SHOWNORMAL);
}

static void browserTabbedWindowSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(object);

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
    case PROP_WIDTH:
        {
            window->width = g_value_get_int(value);
        }
        break;
    case PROP_HEIGHT:
        {
            window->height = g_value_get_int(value);
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

static void browserTabbedWindowDispose(GObject *gObject)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    //BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(gObject);

    G_OBJECT_CLASS(browser_tabbed_window_parent_class)->dispose(gObject);
}

static void browserTabbedWindowFinalize(GObject *gObject)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(gObject);

    if (window->name) {
        g_free(window->name);
        window->name = NULL;
    }

    if (window->title) {
        g_free(window->title);
        window->title = NULL;
    }

    if (window->webContext) {
        g_object_unref(window->webContext);
    }

    G_OBJECT_CLASS(browser_tabbed_window_parent_class)->finalize(gObject);
}

static void browser_tabbed_window_class_init(BrowserTabbedWindowClass *klass)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserTabbedWindowConstructed;
    gobjectClass->set_property = browserTabbedWindowSetProperty;
    gobjectClass->dispose = browserTabbedWindowDispose;
    gobjectClass->finalize = browserTabbedWindowFinalize;

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
        PROP_WIDTH,
        g_param_spec_int(
            "width",
            "width",
            "The window width",
            0, G_MAXINT, 0,
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
        gobjectClass,
        PROP_HEIGHT,
        g_param_spec_int(
            "height",
            "height",
            "The window height",
            0, G_MAXINT, 0,
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
        gobjectClass,
        PROP_PARENT_WINDOW,
        g_param_spec_pointer(
            "parent-window",
            "parent window",
            "The parent window",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

/* Public API. */
BrowserTabbedWindow* browser_tabbed_window_new(HWND parent,
        WebKitWebContext *webContext, const char *name, const char *title,
        gint width, gint height)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(
            g_object_new(BROWSER_TYPE_TABBED_WINDOW,
                "name", name,
                "title", title,
                "width", width,
                "height", height,
                "parent-window", parent,
                NULL));

    if (webContext) {
        window->webContext = g_object_ref(webContext);
    }

    return window;
}

HWND browser_tabbed_window_get_hwnd(BrowserTabbedWindow *window)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));
    return window->mainFixed;
}

WebKitWebContext *
browser_tabbed_window_get_web_context(BrowserTabbedWindow *window)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    return window->webContext;
}

HWND
browser_tabbed_window_create_or_get_toolbar(BrowserTabbedWindow *window)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    return HWND_INVALID;
}

HWND
browser_tabbed_window_create_layout_container(BrowserTabbedWindow *window,
        HWND container, const char *klass, const RECT *geometry)
{
    if (container == NULL || (void *)container == (void *)window) {
        container = window->mainFixed;
    }

    HWND hWnd =  CreateWindow(CTRL_STATIC,
                              "",
                              WS_CHILD | SS_NOTIFY | SS_SIMPLE | WS_VISIBLE,
                              IDC_CONTAINER,
                              geometry->left,
                              geometry->top,
                              RECTWP(geometry),
                              RECTHP(geometry),
                              container,
                              0);

    ShowWindow(hWnd, SW_SHOWNORMAL);
    return hWnd;
}

HWND
browser_tabbed_window_create_pane_container(BrowserTabbedWindow *window,
        HWND container, const char *klass, const RECT *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    if (container == NULL) {
        container = window->mainFixed;
    }

    HWND hWnd =  CreateWindow(CTRL_STATIC,
                              "",
                              WS_CHILD | SS_NOTIFY | SS_SIMPLE | WS_VISIBLE,
                              IDC_CONTAINER,
                              geometry->left,
                              geometry->top,
                              RECTWP(geometry),
                              RECTHP(geometry),
                              container,
                              0);

    g_warning("Creating pan container: (%d, %d; %d x %d)",
            geometry->left, geometry->top, RECTWP(geometry), RECTHP(geometry));
    window->viewContainers = g_slist_append(window->viewContainers, hWnd);

    ShowWindow(hWnd, SW_SHOWNORMAL);

    return hWnd;
}

BrowserPane *
browser_tabbed_window_append_view_pane(BrowserTabbedWindow *window,
        HWND container, WebKitWebViewParam *param,
        const RECT *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);


    param->webViewParent = container;
    param->webViewRect = *geometry;
    param->webViewId = IDC_BROWSER;

    BrowserPane *pane = browser_pane_new(param);

#if 0  // TODO
    WebKitWebView *webView = browser_pane_get_web_view(pane);
    g_signal_connect_after(webView, "close",
            G_CALLBACK(webViewClose), container);
#endif

    g_warning("Creating pan: (%d, %d; %d x %d)",
            geometry->left, geometry->top, RECTWP(geometry), RECTHP(geometry));

    window->nrViews++;
    return pane;
}
static void tab_container_callback(HWND hWnd, LINT id, int nc, DWORD add_data)
{

}

HWND
browser_tabbed_window_create_tab_container(BrowserTabbedWindow *window,
        HWND container, const RECT *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    if (container == NULL || (void *)container == (void *)window) {
        container = window->mainFixed;
    }

    HWND propsheet = CreateWindow (CTRL_PROPSHEET, NULL,
            WS_VISIBLE | PSS_COMPACTTAB,
            IDC_PROPSHEET,
            geometry->left,
            geometry->top,
            RECTWP(geometry),
            RECTHP(geometry),
            container, 0);
    SetWindowAdditionalData(propsheet, (DWORD)window);
    SetNotificationCallback(propsheet, tab_container_callback);

    window->viewContainers = g_slist_append(window->viewContainers, propsheet);
    ShowWindow(propsheet, SW_SHOWNORMAL);

    return propsheet;
}


BrowserTab *
browser_tabbed_window_append_view_tab(BrowserTabbedWindow *window,
        HWND container, WebKitWebViewParam *param)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    BrowserTab *tab = browser_tab_new(container, param);
#if 0
    WebKitWebView *webView = browser_tab_get_web_view(tab);
    g_signal_connect_after(webView, "close",
            G_CALLBACK(webViewClose), container);
#endif

    window->nrViews++;
    return tab;
}

void
browser_tabbed_window_clear_container(BrowserTabbedWindow *window,
        void *container)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
}

void browser_tabbed_window_clear_pane_or_tab(BrowserTabbedWindow *window,
        void *widget)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));

    WebKitWebView* web_view = NULL;
    if (BROWSER_IS_TAB(widget)) {
        BrowserTab *tab = BROWSER_TAB(widget);

        web_view = browser_tab_get_web_view(tab);
    }
    else if (BROWSER_IS_PANE(widget)) {
        BrowserPane *pane = BROWSER_PANE(widget);

        web_view = browser_pane_get_web_view(pane);
    }

    if (web_view)
        webkit_web_view_try_close(web_view);
}

void
browser_tabbed_window_load_uri(BrowserTabbedWindow *window,
        void *widget, const char *uri)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));
    g_return_if_fail(uri);

    if (widget == NULL)
        browser_tab_load_uri(window->activeTab, uri);
    else if (BROWSER_IS_TAB(widget)) {
        browser_tab_load_uri(BROWSER_TAB(widget), uri);
    }
    else if (BROWSER_IS_PANE(widget)) {
        browser_pane_load_uri(BROWSER_PANE(widget), uri);
    }
}

void browser_tabbed_window_set_background_color(BrowserTabbedWindow *window,
        GAL_Color *rgba)
{
    //fprintf(stderr, "#####> %s:%d:%s\n", __FILE__, __LINE__, __func__);
}

