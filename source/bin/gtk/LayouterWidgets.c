/*
** LayouterWidgets.c -- The management of widgets for layouter.
**
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
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

#include "config.h"
#include "main.h"
#include "BrowserPlainWindow.h"
#include "BrowserTabbedWindow.h"
#include "BuildRevision.h"
#include "PurcmcCallbacks.h"
#include "LayouterWidgets.h"

#include "purcmc/purcmc.h"
#include "layouter/layouter.h"

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

void gtk_imp_convert_style(struct ws_widget_info *style,
        purc_variant_t toolkit_style)
{
    style->darkMode = false;
    style->fullScreen = false;
    style->withToolbar = false;
    style->backgroundColor = NULL;
    style->flags |= WSWS_FLAG_TOOLKIT;

    if (toolkit_style == PURC_VARIANT_INVALID)
        return;

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style, "darkMode")) &&
            purc_variant_is_true(tmp)) {
        style->darkMode = true;
    }

    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style, "fullScreen")) &&
            purc_variant_is_true(tmp)) {
        style->fullScreen = true;
    }

    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style, "withToolbar")) &&
            purc_variant_is_true(tmp)) {
        style->withToolbar = true;
    }

    if ((tmp = purc_variant_object_get_by_ckey(toolkit_style,
                    "backgroundColor"))) {
        const char *value = purc_variant_get_string_const(tmp);
        if (value) {
            style->backgroundColor = value;
        }
    }
}

static purcmc_plainwin *create_plainwin(purcmc_workspace *workspace,
        WebKitWebView *web_view, const struct ws_widget_info *style)
{
    purcmc_session *sess = workspace->sess;

    BrowserPlainWindow *plainwin;
    plainwin = BROWSER_PLAIN_WINDOW(browser_plain_window_new(NULL,
                sess->web_context, style->name, style->title));

    GtkApplication *application;
    application = g_object_get_data(G_OBJECT(sess->webkit_settings),
            "gtk-application");

    gtk_application_add_window(GTK_APPLICATION(application),
            GTK_WINDOW(plainwin));

    if (style->flags & WSWS_FLAG_GEOMETRY) {
        LOG_DEBUG("the SIZE of creating plainwin: %d, %d; %u x %u\n",
                style->x, style->y, style->w, style->h);

        if (style->w > 0 && style->h > 0) {
            gtk_window_set_default_size(GTK_WINDOW(plainwin),
                    style->w, style->h);
            gtk_window_resize(GTK_WINDOW(plainwin), style->w, style->h);
        }

        gtk_window_move(GTK_WINDOW(plainwin), style->x, style->y);

        gtk_window_set_resizable(GTK_WINDOW(plainwin), FALSE);
    }

    if (style->darkMode) {
        g_object_set(gtk_widget_get_settings(GTK_WIDGET(plainwin)),
                "gtk-application-prefer-dark-theme", TRUE, NULL);
    }

    if (style->fullScreen) {
        gtk_window_fullscreen(GTK_WINDOW(plainwin));
    }

    if (style->backgroundColor) {
        GdkRGBA rgba;
        if (gdk_rgba_parse(&rgba, style->backgroundColor)) {
            browser_plain_window_set_background_color(plainwin, &rgba);
        }
    }

#if 0
    if (editorMode)
        webkit_web_view_set_editable(web_view, TRUE);
#endif

    g_object_set_data(G_OBJECT(web_view), "purcmc-container", plainwin);

    browser_plain_window_set_view(plainwin, web_view);

    return (struct purcmc_plainwin *)plainwin;
}

static BrowserTabbedWindow *create_tabbedwin(purcmc_workspace *workspace,
        void *init_arg, const struct ws_widget_info *style)
{
    purcmc_session *sess = workspace->sess;

    BrowserTabbedWindow *window;
    window = BROWSER_TABBED_WINDOW(browser_tabbed_window_new(NULL,
                sess->web_context, style->name, style->title,
                style->w, style->h));

    GtkApplication *application;
    application = g_object_get_data(G_OBJECT(sess->webkit_settings),
            "gtk-application");

    gtk_application_add_window(GTK_APPLICATION(application),
            GTK_WINDOW(window));

    if (style->withToolbar) {
        browser_tabbed_window_create_or_get_toolbar(window);
    }

    if (style->darkMode) {
        g_object_set(gtk_widget_get_settings(GTK_WIDGET(window)),
                "gtk-application-prefer-dark-theme", TRUE, NULL);
    }

    if (style->fullScreen) {
        gtk_window_fullscreen(GTK_WINDOW(window));
    }

    if (style->backgroundColor) {
        GdkRGBA rgba;
        if (gdk_rgba_parse(&rgba, style->backgroundColor)) {
            browser_tabbed_window_set_background_color(window, &rgba);
        }
    }

    LOG_INFO("the geometry of created tabbedWindow: %d, %d; %u x %u\n",
            style->x, style->y, style->w, style->h);
    gtk_window_move(GTK_WINDOW(window), style->x, style->y);
    gtk_widget_show(GTK_WIDGET(window));
    return window;
}

static GtkWidget *create_layout_container(purcmc_workspace *workspace,
        BrowserTabbedWindow *window, GtkWidget *container,
        const struct ws_widget_info *style)
{
    GdkRectangle geometry = { style->x, style->y, style->w, style->h };

    return browser_tabbed_window_create_layout_container(window,
            container, style->klass, &geometry);
}

static GtkWidget *create_pane_container(purcmc_workspace *workspace,
        BrowserTabbedWindow *window, GtkWidget *container,
        const struct ws_widget_info *style)
{
    GdkRectangle geometry = { style->x, style->y, style->w, style->h };

    return browser_tabbed_window_create_pane_container(window,
            container, style->klass, &geometry);
}

static GtkWidget *create_tab_container(purcmc_workspace *workspace,
        BrowserTabbedWindow *window, GtkWidget *container,
        const struct ws_widget_info *style)
{
    GdkRectangle geometry = { style->x, style->y, style->w, style->h };

    return browser_tabbed_window_create_tab_container(window,
            container, &geometry);
}

static GtkWidget *create_pane(purcmc_workspace *workspace,
        BrowserTabbedWindow *window, GtkWidget *container,
        WebKitWebView *web_view, const struct ws_widget_info *style)
{
    GdkRectangle geometry = { style->x, style->y, style->w, style->h };

    return browser_tabbed_window_append_view_pane(window,
            container, web_view, &geometry);
}

static GtkWidget *create_tab(purcmc_workspace *workspace,
        BrowserTabbedWindow *window, GtkWidget *container,
        WebKitWebView *web_view, const struct ws_widget_info *style)
{
    return browser_tabbed_window_append_view_tab(window,
            container, web_view);
}

void *
gtk_imp_create_widget(void *ws_ctxt, ws_widget_type_t type, void *window,
        void *container, void *init_arg, const struct ws_widget_info *style)
{
    switch (type) {
    case WS_WIDGET_TYPE_PLAINWINDOW:
        return create_plainwin(ws_ctxt, init_arg, style);

    case WS_WIDGET_TYPE_TABBEDWINDOW:
        return create_tabbedwin(ws_ctxt, init_arg, style);

    case WS_WIDGET_TYPE_CONTAINER:
        return create_layout_container(ws_ctxt, window, container, style);

    case WS_WIDGET_TYPE_PANEHOST:
        return create_pane_container(ws_ctxt, window, container, style);

    case WS_WIDGET_TYPE_TABHOST:
        return create_tab_container(ws_ctxt, window, container, style);

    case WS_WIDGET_TYPE_PANEDPAGE:
        return create_pane(ws_ctxt, window, container, init_arg, style);

    case WS_WIDGET_TYPE_TABBEDPAGE:
        return create_tab(ws_ctxt, window, container, init_arg, style);

    default:
        break;
    }

    return NULL;
}

static int destroy_plainwin(purcmc_workspace *workspace,
        purcmc_plainwin *plain_win)
{
    purcmc_session *sess = workspace->sess;

    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(plain_win), &data)) {
        return PCRDR_SC_NOT_FOUND;
    }

    if ((uintptr_t)data != HT_PLAINWIN) {
        return PCRDR_SC_BAD_REQUEST;
    }

    BrowserPlainWindow *gtkwin = BROWSER_PLAIN_WINDOW(plain_win);
    webkit_web_view_try_close(browser_plain_window_get_view(gtkwin));
    return PCRDR_SC_OK;
}

static int destroy_container_in_tabbedwin(purcmc_workspace *workspace,
        BrowserTabbedWindow *window, GtkWidget *container)
{
    browser_tabbed_window_clear_container(window, container);
    return PCRDR_SC_OK;
}

static int destroy_pane_or_tab_in_tabbedwin(purcmc_workspace *workspace,
        BrowserTabbedWindow *window, GtkWidget *pane_or_tab)
{
    browser_tabbed_window_clear_pane_or_tab(window, pane_or_tab);
    return PCRDR_SC_OK;
}

int
gtk_imp_destroy_widget(void *ws_ctxt, void *window, void *widget,
        ws_widget_type_t type)
{
    switch (type) {
    case WS_WIDGET_TYPE_PLAINWINDOW:
        return destroy_plainwin(ws_ctxt, widget);

    case WS_WIDGET_TYPE_TABBEDWINDOW:
    case WS_WIDGET_TYPE_CONTAINER:
    case WS_WIDGET_TYPE_PANEHOST:
    case WS_WIDGET_TYPE_TABHOST:
        return destroy_container_in_tabbedwin(ws_ctxt, window, widget);

    case WS_WIDGET_TYPE_PANEDPAGE:
    case WS_WIDGET_TYPE_TABBEDPAGE:
        return destroy_pane_or_tab_in_tabbedwin(ws_ctxt, window, widget);

    default:
        break;
    }

    return PCRDR_SC_OK;
}

void
gtk_imp_update_widget(void *ws_ctxt, void *widget,
        ws_widget_type_t type, const struct ws_widget_info *style)
{
}

