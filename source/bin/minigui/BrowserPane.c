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
#include "BrowserPane.h"

#include "BrowserWindow.h"
#include <string.h>

enum {
    PROP_0,

    PROP_VIEW
};

G_DEFINE_TYPE(BrowserPane, browser_pane, G_TYPE_OBJECT)

static void browser_pane_init(BrowserPane *pane)
{
}

static void browser_pane_class_init(BrowserPaneClass *klass)
{
}

/* Public API. */
HWND browser_pane_new(WebKitWebView *view)
{
    return NULL;
}

WebKitWebView *browser_pane_get_web_view(BrowserPane *pane)
{
    return NULL;
}

void browser_pane_load_uri(BrowserPane *pane, const char *uri)
{
}

void browser_pane_set_status_text(BrowserPane *pane, const char *text)
{
}

void browser_pane_toggle_inspector(BrowserPane *pane)
{
}

void browser_pane_set_background_color(BrowserPane *pane, GAL_Color *rgba)
{
}

void browser_pane_start_search(BrowserPane *pane)
{
}

void browser_pane_stop_search(BrowserPane *pane)
{
}

void browser_pane_enter_fullscreen(BrowserPane *pane)
{
}

void browser_pane_leave_fullscreen(BrowserPane *pane)
{
}

