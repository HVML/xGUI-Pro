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

#define KEY_BROWSER_TABBED_WINDOW "browser_tabbed_wndow"

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

    gchar *name;
    gchar *title;
    gint width;
    gint height;
};

struct _BrowserTabbedWindowClass {
    GObjectClass parent;
};

enum {
    DESTROY,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(BrowserTabbedWindow, browser_tabbed_window, G_TYPE_OBJECT)

static void browser_tabbed_window_init(BrowserTabbedWindow *window)
{
}

static LRESULT BrowserTabbedWindowProc(HWND hWnd, UINT message, WPARAM wParam,
        LPARAM lParam)
{
    switch (message) {
        case MSG_CREATE:
            xgui_window_inc();
            break;

        case MSG_CLOSE:
            DestroyAllControls (hWnd);
            DestroyMainWindow (hWnd);
            return 0;

        case MSG_DESTROY:
            {
                void *container = (void *)GetWindowAdditionalData(hWnd);
                g_signal_emit(container, signals[DESTROY], 0, NULL);
                g_object_unref(container);
                xgui_window_dec();
            }
            break;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

static void browserTabbedWindowConstructed(GObject *gObject)
{
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
    CreateInfo.dwStyle = WS_VISIBLE;
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
    CreateInfo.iBkColor = COLOR_lightgray;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = parent;
    window->mainFixed = CreateMainWindow (&CreateInfo);
    SetWindowAdditionalData(window->mainFixed, (DWORD)window);

    ShowWindow(window->mainFixed, SW_SHOWNORMAL);
}

static void browserTabbedWindowSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
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
    //BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(gObject);

    G_OBJECT_CLASS(browser_tabbed_window_parent_class)->dispose(gObject);
}

static void browserTabbedWindowFinalize(GObject *gObject)
{
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
BrowserTabbedWindow* browser_tabbed_window_new(HWND parent,
        WebKitWebContext *webContext, const char *name, const char *title,
        gint width, gint height)
{
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
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));
    return window->mainFixed;
}

WebKitWebContext *
browser_tabbed_window_get_web_context(BrowserTabbedWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    return window->webContext;
}

HWND
browser_tabbed_window_create_or_get_toolbar(BrowserTabbedWindow *window)
{
    return HWND_INVALID;
}

BrowserLayoutContainer *
browser_tabbed_window_create_layout_container(BrowserTabbedWindow *window,
        GObject *container, const char *klass, const RECT *geometry)
{
    return browser_layout_container_new(window, container, klass, geometry);
}

BrowserPaneContainer *
browser_tabbed_window_create_pane_container(BrowserTabbedWindow *window,
        GObject *container, const char *klass, const RECT *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    BrowserPaneContainer *pane = browser_pane_container_new(window, container,
            klass, geometry);

    window->viewContainers = g_slist_append(window->viewContainers, pane);

    return pane;
}

static void
browserPaneWebViewClose(WebKitWebView *webView, BrowserPane *pane)
{
    BrowserTabbedWindow *window = (BrowserTabbedWindow *)
        g_object_get_data(G_OBJECT(pane), KEY_BROWSER_TABBED_WINDOW);
    if (window->nrViews == 1) {
        DestroyMainWindow(window->mainFixed);
        return;
    }

    if (BRW_PANE2VIEW(pane) == webView) {
        DestroyWindow(pane->hwnd);
        DestroyWindow(pane->parentHwnd);
    }

    window->nrViews--;
}

BrowserPane *
browser_tabbed_window_append_view_pane(BrowserTabbedWindow *window,
        GObject *container, WebKitWebViewParam *param,
        const RECT *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    BrowserPaneContainer *paneContainer = BROWSER_PANE_CONTAINER(container);
    HWND parentHwnd = browser_pane_container_get_hwnd(paneContainer);

    param->webViewParent = parentHwnd;
    param->webViewRect = *geometry;
    param->webViewId = IDC_BROWSER;

    BrowserPane *pane = browser_pane_new(param);
    g_object_set_data(G_OBJECT(pane), KEY_BROWSER_TABBED_WINDOW, window);

    browser_pane_container_add_child(paneContainer, pane);

    WebKitWebView *webView = browser_pane_get_web_view(pane);
    g_signal_connect_after(webView, "close",
            G_CALLBACK(browserPaneWebViewClose), pane);

    g_warning("Creating pan: (%d, %d; %d x %d)",
            geometry->left, geometry->top, RECTWP(geometry), RECTHP(geometry));

    window->nrViews++;
    return pane;
}

BrowserTabContainer *
browser_tabbed_window_create_tab_container(BrowserTabbedWindow *window,
        GObject *container, const RECT *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    BrowserTabContainer *tabC = browser_tab_container_new(window, container,
            geometry);

    window->viewContainers = g_slist_append(window->viewContainers, tabC);

    return tabC;
}

static void
browserTabWebViewClose(WebKitWebView *webView, BrowserTab *tab)
{
    BrowserTabbedWindow *window = (BrowserTabbedWindow *)
        g_object_get_data(G_OBJECT(tab), KEY_BROWSER_TABBED_WINDOW);
    if (window->nrViews == 1) {
        DestroyMainWindow(window->mainFixed);
        return;
    }

    HWND webHwnd = webkit_web_view_get_hwnd(webView);
    DestroyWindow(webHwnd);

    int idx = browser_tab_get_idx(tab);
    HWND psHwnd = browser_tab_get_propsheet(tab);
    SendMessage(psHwnd, PSM_REMOVEPAGE, (WPARAM)idx, 0);

    window->nrViews--;
}

BrowserTab *
browser_tabbed_window_append_view_tab(BrowserTabbedWindow *window,
        GObject *container, WebKitWebViewParam *param)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    BrowserTabContainer *tabContainer = BROWSER_TAB_CONTAINER(container);
    HWND propHwnd = browser_tab_container_get_hwnd(tabContainer);

    BrowserTab *tab = browser_tab_new(propHwnd, param);
    g_object_set_data(G_OBJECT(tab), KEY_BROWSER_TABBED_WINDOW, window);
    browser_tab_container_add_child(tabContainer, tab);

    WebKitWebView *webView = browser_tab_get_web_view(tab);
    g_signal_connect_after(webView, "close",
            G_CALLBACK(browserTabWebViewClose), tab);

    window->nrViews++;
    return tab;
}

static void
containerTryClose(BrowserTabbedWindow *window, void *container)
{
    GSList *webViews = NULL;

    if (BROWSER_IS_TAB_CONTAINER(container)) {
        BrowserTabContainer *tabContainer = BROWSER_TAB_CONTAINER(container);
        GSList *tabs = browser_tab_container_get_children(tabContainer);
        GSList *link;
        for (link = tabs; link; link = link->next) {
            BrowserTab *tab = BROWSER_TAB(link->data);
            webViews = g_slist_prepend(webViews, browser_tab_get_web_view(tab));
        }
    }
    else if (BROWSER_IS_PANE_CONTAINER(container)) {
        BrowserPaneContainer *paneContainer = BROWSER_PANE_CONTAINER(container);
        GSList *panes = browser_pane_container_get_children(paneContainer);

        GSList *link;
        for (link = panes; link; link = link->next) {
            BrowserPane *pane = BROWSER_PANE(link->data);
            webViews = g_slist_prepend(webViews, browser_pane_get_web_view(pane));
        }
    }

    if (webViews) {
        GSList *link;
        for (link = webViews; link; link = link->next) {
            webkit_web_view_try_close(link->data);
        }
        g_slist_free(webViews);
    }
}

static void
windowTryClose(BrowserTabbedWindow *window)
{
    if (window->viewContainers) {
        GSList *link;
        for (link = window->viewContainers; link; link = link->next)
            containerTryClose(window, link->data);
    }
}

void
browser_tabbed_window_clear_container(BrowserTabbedWindow *window,
        void *container)
{
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));

    if ((void *)window == (void *)container || container == NULL) {
        windowTryClose(window);
    }
    else {
        containerTryClose(window, container);
    }
}

void browser_tabbed_window_clear_pane_or_tab(BrowserTabbedWindow *window,
        void *widget)
{
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
}

