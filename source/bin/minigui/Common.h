/*
** Common.h -- The declarations of common structure and api.
**
** Copyright (C) 2023 FMSoft (http://www.fmsoft.cn)
**
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

#ifndef Common_h
#define Common_h

#include <webkit2/webkit2.h>
#include <purc/purc-pcrdr.h>
#include <mgeff/mgeff.h>

#define IDC_BROWSER             140
#define IDC_PLAIN_WINDOW        141
#define IDC_PROPSHEET           142
#define IDC_URI_ENTRY           143
#define IDC_CONTAINER           144

#define IDC_ADDRESS_LEFT        5
#define IDC_ADDRESS_TOP         3
#define IDC_ADDRESS_HEIGHT      20

#define KEY_XGUI_APPLICATION    "xgui-application"

#ifdef __cplusplus
extern "C" {
#endif

extern GApplication *g_xgui_application;
extern HWND g_xgui_main_window;
extern HWND g_xgui_floating_window;
extern PBITMAP g_xgui_window_bg;

typedef struct {
    bool isControlledByAutomation;
    void *settings;
    void *userContentManager;
    void *webContext;
    void *websitePolicies;
    int  webViewId;
    void *webViewParent;
    void *relatedView;
    RECT webViewRect;
} WebKitWebViewParam;

WebKitWebView *xgui_create_webview(WebKitWebViewParam *param);

void xgui_window_inc();
void xgui_window_dec();

void xgui_destroy_event(pcrdr_msg *msg);

double xgui_get_current_time();

void xgui_load_window_bg();
void xgui_unload_window_bg();

void xgui_webview_draw_background_callback(HWND hWnd, HDC hdc, RECT rect);

enum EffMotionType xgui_get_motion_type(purc_window_transition_function func);

#ifdef __cplusplus
}
#endif

#endif  /* Common_h */

