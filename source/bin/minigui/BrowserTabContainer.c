/*
** BrowserTabContainer.c -- The implementation of BrowserTabContainer.
**
** Copyright (C) 2022, 2023 FMSoft <http://www.fmsoft.cn>
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
#include "BrowserTabContainer.h"
#include "BrowserLayoutContainer.h"
#include "BrowserPaneContainer.h"
#include "BrowserTabbedWindow.h"

#include "Common.h"
#include <string.h>

enum {
    PROP_0,

    PROP_TABBED_WINDOW,
    PROP_CONTAINER,
    PROP_GEOMETRY,

    N_PROPERTIES
};

struct _BrowserTabContainer {
    GObject parent;

    GSList *children;
    BrowserTabbedWindow *tabbedWindow;
    GObject *container;
    gchar *klass;
    RECT geometry;

    HWND parentHwnd;
    HWND hwnd;  // propsheet
};

struct _BrowserTabContainerClass {
    GObjectClass parent;
};

G_DEFINE_TYPE(BrowserTabContainer, browser_tab_container,
        G_TYPE_OBJECT)

static void browser_tab_container_init(BrowserTabContainer *window)
{
}

static void tab_container_callback(HWND hWnd, LINT id, int nc, DWORD add_data)
{
}

static void browserPlainWindowConstructed(GObject *gObject)
{
    BrowserTabContainer *window = BROWSER_TAB_CONTAINER(gObject);
    G_OBJECT_CLASS(browser_tab_container_parent_class)->constructed(gObject);

    if (window->container == NULL || (void *)window->container ==
            (void *)window->tabbedWindow) {
        window->parentHwnd = browser_tabbed_window_get_hwnd(window->tabbedWindow);
    }
    else if (BROWSER_IS_LAYOUT_CONTAINER(window->container)) {
        window->parentHwnd = browser_layout_container_get_hwnd(
                BROWSER_LAYOUT_CONTAINER(window->container));
    }
    else if (BROWSER_IS_PANE_CONTAINER(window->container)) {
        window->parentHwnd = browser_pane_container_get_hwnd(
                BROWSER_PANE_CONTAINER(window->container));
    }

    HWND parentHwnd = window->parentHwnd;

    int w = RECTW(window->geometry);
    int h = RECTH(window->geometry);

    window->hwnd = CreateWindow (CTRL_PROPSHEET, NULL,
            WS_VISIBLE | PSS_COMPACTTAB,
            IDC_PROPSHEET,
            window->geometry.left,
            window->geometry.top,
            w,
            h,
            parentHwnd,
            0);
    SetWindowAdditionalData(window->hwnd, (DWORD)window);
    SetNotificationCallback(window->hwnd, tab_container_callback);

    ShowWindow(window->hwnd, SW_SHOWNORMAL);
}

static void browserPlainWindowSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    BrowserTabContainer *window = BROWSER_TAB_CONTAINER(object);

    switch (propId) {
    case PROP_TABBED_WINDOW:
        {
            gpointer *p = g_value_get_pointer(value);
            if (p) {
                window->tabbedWindow = (BrowserTabbedWindow *)p;
            }
        }
        break;

    case PROP_CONTAINER:
        {
            gpointer *p = g_value_get_pointer(value);
            if (p) {
                window->container = (GObject *)p;
            }
        }
        break;

    case PROP_GEOMETRY:
        {
            gpointer *p = g_value_get_pointer(value);
            if (p) {
                window->geometry = *(RECT *)p;
            }
        }
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browserPlainWindowDispose(GObject *gObject)
{
    //BrowserTabContainer *window = BROWSER_TAB_CONTAINER(gObject);

    G_OBJECT_CLASS(browser_tab_container_parent_class)->dispose(gObject);
}

static void browserPlainWindowFinalize(GObject *gObject)
{
    BrowserTabContainer *window = BROWSER_TAB_CONTAINER(gObject);

    if (window->klass) {
        g_free(window->klass);
        window->klass = NULL;
    }

    G_OBJECT_CLASS(browser_tab_container_parent_class)->finalize(gObject);
}

static void browser_tab_container_class_init(BrowserTabContainerClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserPlainWindowConstructed;
    gobjectClass->set_property = browserPlainWindowSetProperty;
    gobjectClass->dispose = browserPlainWindowDispose;
    gobjectClass->finalize = browserPlainWindowFinalize;

    g_object_class_install_property(
        gobjectClass,
        PROP_CONTAINER,
        g_param_spec_pointer(
            "container",
            "The Container of layout container",
            "The Container of layout container",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(
        gobjectClass,
        PROP_TABBED_WINDOW,
        g_param_spec_pointer(
            "tabbed-window",
            "tabbed window",
            "The tabbed window",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(
        gobjectClass,
        PROP_GEOMETRY,
        g_param_spec_pointer(
            "geometry",
            "geometry",
            "The gemoetry of layout container",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

/* Public API. */
BrowserTabContainer *
browser_tab_container_new(BrowserTabbedWindow *window,
        GObject *container, const RECT *geometry)
{
    BrowserTabContainer *layout_container =
          BROWSER_TAB_CONTAINER(g_object_new(BROWSER_TYPE_TAB_CONTAINER,
                      "tabbed-window", window,
                      "container", container,
                      "geometry", geometry,
                      NULL));

    return layout_container;
}

HWND browser_tab_container_get_hwnd(BrowserTabContainer *container)
{
    return container->hwnd;
}

void browser_tab_container_add_child(BrowserTabContainer *container,
        BrowserTab *tab)
{
    container->children = g_slist_append(container->children, tab);
}

GSList *browser_tab_container_get_children(BrowserTabContainer *container)
{
    return container->children;
}

