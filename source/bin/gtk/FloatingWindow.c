/*
** FloatingWindow.c -- The implementation of floating window.
**
** Copyright (C) 2023 FMSoft <http://www.fmsoft.cn>
**
** Author: XueShuming
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

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "FloatingWindow.h"
#include "utils/utils.h"

gboolean supports_alpha = FALSE;

static void screen_changed(GtkWidget *widget, GdkScreen *old_screen,
        gpointer userdata) {
    GdkScreen *screen = gtk_widget_get_screen(widget);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (!visual) {
        visual = gdk_screen_get_system_visual(screen);
        supports_alpha = FALSE;
    } else {
        supports_alpha = TRUE;
    }
    gtk_widget_set_visual(widget, visual);
}

static gboolean expose_draw(GtkWidget *widget, GdkEventExpose *event,
        gpointer userdata)
{
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
    if (supports_alpha) {
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    } else {
        cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    }

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy(cr);
    return FALSE;
}

static void window_clicked(GtkWidget *widget, gpointer data)
{
    (void) widget;
    (void) data;
    xgutils_show_runners_window();
}

static gboolean draw_normal_window(GtkWidget *widget, GdkEventExpose *event,
        gpointer userdata)
{
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
    if (supports_alpha) {
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    } else {
        cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    }

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);

    gint root_width,root_height;
    cairo_surface_t* m_bgImage = cairo_image_surface_create_from_png("/tmp/a.png");
    cairo_set_source_surface(cr, m_bgImage, 0, 0);

    gtk_window_get_size (GTK_WINDOW(widget), &root_width, &root_height);
    cairo_rectangle (cr, 0, 0, root_width, root_height);
    cairo_fill (cr);
    cairo_destroy(cr);
    return FALSE;
}

GtkWidget *create_normal_window(void)
{
    GtkWidget *window;
    GtkWidget *image;

    int win_w = 72;
    int win_h = 72;
    int x = 0;
    int y = 0;
    int sw = 0;
 //   int sh = 0;
    GdkDisplay* dsp = gdk_display_get_default();
    GdkMonitor* monitor = gdk_display_get_primary_monitor(dsp);
    if (monitor) {
        GdkRectangle geometry;
        gdk_monitor_get_geometry(monitor, &geometry);
        sw  = geometry.width;
        x = sw - win_w;
//        sh = geometry.height;
    }
#if 0
    else {
        sw = gdk_screen_width();
        sh = gdk_screen_height();
    }
#endif

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_window_set_decorated(GTK_WINDOW (window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(window), win_w, win_h);
    gtk_window_move(GTK_WINDOW(window), x, y);
    g_signal_connect(GTK_WINDOW(window), "button-press-event",
            G_CALLBACK(window_clicked), NULL);

    g_signal_connect(G_OBJECT(window), "draw", G_CALLBACK(draw_normal_window), NULL);
    g_signal_connect(G_OBJECT(window), "screen-changed", G_CALLBACK(screen_changed), NULL);
    g_signal_connect_swapped(G_OBJECT(window), "destroy",
    G_CALLBACK(gtk_main_quit), NULL);
    screen_changed(window, NULL, NULL);

    gtk_widget_show_all(window);
    return window;
}

static void clicked(GtkWindow *window, GdkEventButton *event, gpointer user_data) {
    /* toggle window manager frames */
    gtk_window_set_decorated(window, !gtk_window_get_decorated(window));
}

GtkWidget *create_transparent_window(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
    gtk_window_set_title(GTK_WINDOW(window), "Alpha Demo");
    g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);

    gtk_widget_set_app_paintable(window, TRUE);

    g_signal_connect(G_OBJECT(window), "draw", G_CALLBACK(expose_draw), NULL);
    g_signal_connect(G_OBJECT(window), "screen-changed", G_CALLBACK(screen_changed), NULL);

    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(clicked), NULL);

    GtkWidget* fixed_container = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window), fixed_container);
    GtkWidget* button = gtk_button_new_with_label("button1");
    gtk_widget_set_size_request(button, 100, 100);
    gtk_container_add(GTK_CONTAINER(fixed_container), button);

    screen_changed(window, NULL, NULL);

    gtk_widget_show_all(window);
    return window;
}

GtkWidget *create_floating_window(void)
{
    return create_normal_window();
}

