/*
** BrowserPane.h -- The declaration of BrowserPane.
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
** MERCHANPANEILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#ifndef BrowserPane_h
#define BrowserPane_h

#include <webkit2/webkit2.h>

#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>

#include "Common.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_PANE            (browser_pane_get_type())
#define BROWSER_PANE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_PANE, BrowserPane))
#define BROWSER_PANE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_PANE, BrowserPaneClass))
#define BROWSER_IS_PANE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_PANE))
#define BROWSER_IS_PANE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_PANE))
#define BROWSER_PANE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_PANE, BrowserPaneClass))

typedef struct _BrowserPane        BrowserPane;
typedef struct _BrowserPaneClass   BrowserPaneClass;

/*
 * BrowserPane only contains one webView and an overlay widget for messages.
 * BrowserTab is derived from BrowserPane.
 */
struct _BrowserPane {
    GObject parent;

    WebKitWebViewParam param;
    WebKitWebView *webView;

    HWND  hwnd;
    HWND  parentHwnd;
};

struct _BrowserPaneClass {
    GObjectClass parent;
};

GType browser_pane_get_type(void);

HWND browser_pane_new(WebKitWebView*);
HWND browser_pane_new_by_param(WebKitWebViewParam*);
WebKitWebView* browser_pane_get_web_view(BrowserPane*);
void browser_pane_load_uri(BrowserPane*, const char* uri);
void browser_pane_set_status_text(BrowserPane*, const char* text);
void browser_pane_toggle_inspector(BrowserPane*);
void browser_pane_set_background_color(BrowserPane*, GAL_Color* color);
void browser_pane_start_search(BrowserPane*);
void browser_pane_stop_search(BrowserPane*);
void browser_pane_enter_fullscreen(BrowserPane*);
void browser_pane_leave_fullscreen(BrowserPane*);

G_END_DECLS

#define BRW_PANE2VIEW(tab)  browser_pane_get_web_view((tab))

#endif
