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

#include <string.h>

struct _BrowserWindow {
    GObject parent;

    WebKitWebContext *webContext;
};

struct _BrowserWindowClass {
    GObjectClass parent;
};

G_DEFINE_TYPE(BrowserWindow, browser_window, G_TYPE_OBJECT)

static void browser_window_init(BrowserWindow *window)
{
}

static void browser_window_class_init(BrowserWindowClass *klass)
{
}

HWND browser_window_new(HWND parent, WebKitWebContext *webContext)
{
    return NULL;
}


WebKitWebContext* browser_window_get_web_context(BrowserWindow *window)
{
    return NULL;
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


