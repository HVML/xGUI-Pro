/*
** BrowserPlainWindow.h -- The declaration of BrowserPlainWindow.
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

#ifndef BrowserPlainWindow_h
#define BrowserPlainWindow_h

#include <webkit2/webkit2.h>
#include "Common.h"

G_BEGIN_DECLS

#define WINDOW_LEVEL_TOOLTIP       "tooltip"
#define WINDOW_LEVEL_GLOBAL        "global"
#define WINDOW_LEVEL_SCREENLOCK    "screenlock"
#define WINDOW_LEVEL_DOCKER        "docker"
#define WINDOW_LEVEL_HIGHER        "higher"
#define WINDOW_LEVEL_NORMAL        "normal"
#define WINDOW_LEVEL_LAUNCHER      "launcher"


#define BROWSER_TYPE_PLAIN_WINDOW            (browser_plain_window_get_type())
#define BROWSER_PLAIN_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_PLAIN_WINDOW, BrowserPlainWindow))
#define BROWSER_PLAIN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_PLAIN_WINDOW, BrowserPlainWindowClass))
#define BROWSER_IS_PLAIN_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_PLAIN_WINDOW))
#define BROWSER_IS_PLAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_PLAIN_WINDOW))
#define BROWSER_PLAIN_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_PLAIN_WINDOW, BrowserPlainWindowClass))

typedef struct _BrowserPlainWindow        BrowserPlainWindow;
typedef struct _BrowserPlainWindowClass   BrowserPlainWindowClass;

GType browser_plain_window_get_type(void);

BrowserPlainWindow* browser_plain_window_new(HWND, WebKitWebContext*,
        const char *name, const char *title, const char *window_level,
        gboolean forHVML);
HWND browser_plain_window_get_hwnd(BrowserPlainWindow*);
WebKitWebContext* browser_plain_window_get_web_context(BrowserPlainWindow*);
void browser_plain_window_set_view(BrowserPlainWindow*, WebKitWebViewParam*);
WebKitWebView* browser_plain_window_get_view(BrowserPlainWindow*);
const char* browser_plain_window_get_name(BrowserPlainWindow*);
void browser_plain_window_set_title(BrowserPlainWindow*, const char*);
void browser_plain_window_hide(BrowserPlainWindow*);
void browser_plain_window_show(BrowserPlainWindow*);

void browser_plain_window_load_uri(BrowserPlainWindow*, const char *);
void browser_plain_window_load_session(BrowserPlainWindow*, const char*);
void browser_plain_window_set_background_color(BrowserPlainWindow*, GAL_Color*);

G_END_DECLS

#endif
