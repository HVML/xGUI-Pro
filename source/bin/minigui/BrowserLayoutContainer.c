/*
** BrowserLayoutContainer.c -- The implementation of BrowserLayoutContainer.
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
#include "BrowserLayoutContainer.h"
#include "BrowserPaneContainer.h"
#include "BrowserTabbedWindow.h"

#include "Common.h"
#include <string.h>

enum {
    PROP_0,

    PROP_TABBED_WINDOW,
    PROP_CONTAINER,
    PROP_CLASS,
    PROP_GEOMETRY,

    N_PROPERTIES
};

struct _BrowserLayoutContainer {
    GObject parent;

    BrowserTabbedWindow *tabbedWindow;
    GObject *container;
    gchar *klass;
    RECT geometry;

    HWND parentHwnd;
    HWND hwnd;
};

struct _BrowserLayoutContainerClass {
    GObjectClass parent;
};

G_DEFINE_TYPE(BrowserLayoutContainer, browser_layout_container,
        G_TYPE_OBJECT)

static void browser_layout_container_init(BrowserLayoutContainer *window)
{
}

static void browserPlainWindowConstructed(GObject *gObject)
{
    BrowserLayoutContainer *window = BROWSER_LAYOUT_CONTAINER(gObject);
    G_OBJECT_CLASS(browser_layout_container_parent_class)->constructed(gObject);

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

    RECT rc;
    GetClientRect(parentHwnd, &rc);

    int w = RECTW(window->geometry);
    int h = RECTH(window->geometry);
    if (w == 0) {
        w = RECTW(rc);
    }
    if (h == 0) {
        h = RECTH(rc);
    }

    window->hwnd =  CreateWindow(CTRL_STATIC,
                              "",
                              WS_CHILD | SS_NOTIFY | SS_SIMPLE | WS_VISIBLE,
                              IDC_CONTAINER,
                              window->geometry.left,
                              window->geometry.top,
                              w,
                              h,
                              parentHwnd,
                              0);

    ShowWindow(window->hwnd, SW_SHOWNORMAL);
}

static void browserPlainWindowSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    BrowserLayoutContainer *window = BROWSER_LAYOUT_CONTAINER(object);

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

    case PROP_CLASS:
        {
            gchar *str = g_value_get_pointer(value);
            if (str) {
                window->klass = g_strdup(str);
            }
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browserPlainWindowDispose(GObject *gObject)
{
    //BrowserLayoutContainer *window = BROWSER_LAYOUT_CONTAINER(gObject);

    G_OBJECT_CLASS(browser_layout_container_parent_class)->dispose(gObject);
}

static void browserPlainWindowFinalize(GObject *gObject)
{
    BrowserLayoutContainer *window = BROWSER_LAYOUT_CONTAINER(gObject);

    if (window->klass) {
        g_free(window->klass);
        window->klass = NULL;
    }

    G_OBJECT_CLASS(browser_layout_container_parent_class)->finalize(gObject);
}

static void browser_layout_container_class_init(BrowserLayoutContainerClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserPlainWindowConstructed;
    gobjectClass->set_property = browserPlainWindowSetProperty;
    gobjectClass->dispose = browserPlainWindowDispose;
    gobjectClass->finalize = browserPlainWindowFinalize;

    g_object_class_install_property(
        gobjectClass,
        PROP_CLASS,
        g_param_spec_pointer(
            "class",
            "Layout container class",
            "The layout container style class",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
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
BrowserLayoutContainer *
browser_layout_container_new(BrowserTabbedWindow *window,
        GObject *container, const char *klass, const RECT *geometry)
{
    BrowserLayoutContainer *layout_container =
          BROWSER_LAYOUT_CONTAINER(g_object_new(BROWSER_TYPE_LAYOUT_CONTAINER,
                      "tabbed-window", window,
                      "container", container,
                      "class", klass,
                      "geometry", geometry,
                      NULL));

    return layout_container;
}

HWND browser_layout_container_get_hwnd(BrowserLayoutContainer *container)
{
    return container->hwnd;
}

