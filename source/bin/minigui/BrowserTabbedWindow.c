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
#include "BrowserWindow.h"
#include <string.h>

struct _BrowserTabbedWindow {
    GObject parent;

    WebKitWebContext *webContext;
    BrowserTab *activeTab;

    gchar *name;
    gchar *title;
    gint width;
    gint height;
};

struct _BrowserTabbedWindowClass {
    GObjectClass parent;
};

static const gdouble minimumZoomLevel = 0.5;
static const gdouble maximumZoomLevel = 3;
static const gdouble defaultZoomLevel = 1;
static const gdouble zoomStep = 1.2;

G_DEFINE_TYPE(BrowserTabbedWindow, browser_tabbed_window, G_TYPE_OBJECT)

static void browser_tabbed_window_init(BrowserTabbedWindow *window)
{
}

static void
browser_tabbed_window_class_init(BrowserTabbedWindowClass *klass)
{
}

/* Public API. */
HWND
browser_tabbed_window_new(HWND parent, WebKitWebContext *webContext,
        const char *name, const char *title, gint width, gint height)
{
    return NULL;
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
    return NULL;
}

HWND
browser_tabbed_window_create_layout_container(BrowserTabbedWindow *window,
        HWND container, const char *klass, const RECT *geometry)
{
    return NULL;
}

HWND
browser_tabbed_window_create_pane_container(BrowserTabbedWindow *window,
        HWND container, const char *klass, const RECT *geometry)
{
    return NULL;
}

HWND
browser_tabbed_window_create_tab_container(BrowserTabbedWindow *window,
        HWND container, const RECT *geometry)
{
    return NULL;
}

HWND
browser_tabbed_window_append_view_pane(BrowserTabbedWindow *window,
        HWND container, WebKitWebView *webView,
        const RECT *geometry)
{
    return NULL;
}

HWND
browser_tabbed_window_append_view_tab(BrowserTabbedWindow *window,
        HWND container, WebKitWebView *webView)
{
    return NULL;
}

void
browser_tabbed_window_clear_container(BrowserTabbedWindow *window,
        HWND container)
{
}

void browser_tabbed_window_clear_pane_or_tab(BrowserTabbedWindow *window,
        HWND widget)
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
        HWND widget, const char *uri)
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

