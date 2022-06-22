/*
** BrowserTabbedWindow.h -- The declaration of BrowserTabbedWindow.
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

#ifndef BrowserTabbedWindow_h
#define BrowserTabbedWindow_h

#include <webkit2/webkit2.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BROWSER_TYPE_TABBED_WINDOW            (browser_tabbed_window_get_type())
#define BROWSER_TABBED_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_TABBED_WINDOW, BrowserTabbedWindow))
#define BROWSER_TABBED_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_TABBED_WINDOW, BrowserTabbedWindowClass))
#define BROWSER_IS_TABBED_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_TABBED_WINDOW))
#define BROWSER_IS_TABBED_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_TABBED_WINDOW))
#define BROWSER_TABBED_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_TABBED_WINDOW, BrowserTabbedWindowClass))

typedef struct _BrowserTabbedWindow        BrowserTabbedWindow;
typedef struct _BrowserTabbedWindowClass   BrowserTabbedWindowClass;

GType browser_tabbed_window_get_type(void);

GtkWidget* browser_tabbed_window_new(GtkWindow*, WebKitWebContext*,
        const char*, const char*);

WebKitWebContext* browser_tabbed_window_get_web_context(BrowserTabbedWindow*);

/* Create or get the menubar widget (only one menubar in a tabbed window) */
GtkWidget* browser_tabbed_window_create_or_get_menubar(BrowserTabbedWindow*,
        const GdkRectangle*);

/* Create or get the toolbar widget (only one toolbar in a tabbed window) */
GtkWidget* browser_tabbed_window_create_or_get_toolbar(BrowserTabbedWindow*,
        const GdkRectangle*);

/* Create or get the container for BrowserTab widgets
   (only one container for BrowserTab in a tabbed window). */
GtkWidget* browser_tabbed_window_create_or_get_notebook(BrowserTabbedWindow*,
        const GdkRectangle*);

/* Create a BrowserPane widget for header, footer, and aside. */
GtkWidget* browser_tabbed_window_create_pane(BrowserTabbedWindow*,
        WebKitWebView*, const GdkRectangle*);

/* Create the container (a frame) for BrowserPane widgets. */
GtkWidget* browser_tabbed_window_create_frame(BrowserTabbedWindow*,
        const GdkRectangle*);

/* Create a BrowserPane widget in the specific frame widget. */
GtkWidget* browser_tabbed_window_create_pane_in_frame(BrowserTabbedWindow*,
        GtkWidget*, WebKitWebView*, const GdkRectangle*);

/* Set webview of a BrowserPane widget */
void browser_tabbed_window_set_view(BrowserTabbedWindow*, GtkWidget*,
        WebKitWebView*);

/* Append a webview to the only one notebook which acts as
   the container of BrowserTab. */
void browser_tabbed_window_append_view(BrowserTabbedWindow*, WebKitWebView*);

/* Load URI in the specific widget
   (or the active BrowserTab if the widget is NULL). */
void browser_tabbed_window_load_uri(BrowserTabbedWindow*, GtkWidget*,
        const char *uri);

void browser_tabbed_window_set_background_color(BrowserTabbedWindow*, GdkRGBA*);

G_END_DECLS

#endif
