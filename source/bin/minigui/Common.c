/*
** Common.c -- The implementation of callbacks for PurCMC.
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

#undef NDEBUG

#include "config.h"
#include "Common.h"
#include "xguipro-features.h"

#include <assert.h>
#include <sys/time.h>

#define WEBVIEW_PARAM_COUNT   9
#define XGUI_WINDOW_BG        "assets/xgui-bg.png"

GApplication *g_xgui_application = NULL;
HWND g_xgui_main_window = HWND_NULL;
uint32_t g_xgui_window_count = 0;
BITMAP xgui_window_bg;
PBITMAP g_xgui_window_bg = NULL;

WebKitWebView *xgui_create_webview(WebKitWebViewParam *param)
{
    const char *names[WEBVIEW_PARAM_COUNT];
    GValue    values[WEBVIEW_PARAM_COUNT] = {0, };
    guint nr_params = 0;

    names[nr_params] = "is-controlled-by-automation";
    g_value_init(&values[nr_params], G_TYPE_BOOLEAN);
    g_value_set_boolean(&values[nr_params], param->isControlledByAutomation);
    nr_params++;

    if (param->settings) {
        names[nr_params] = "settings";
        g_value_init(&values[nr_params], G_TYPE_OBJECT);
        g_value_set_object(&values[nr_params], param->settings);
        nr_params++;
    }

    if (param->webContext) {
        names[nr_params] = "web-context";
        g_value_init(&values[nr_params], G_TYPE_OBJECT);
        g_value_set_object(&values[nr_params], param->webContext);
        nr_params++;
    }

    if (param->userContentManager) {
        names[nr_params] = "user-content-manager";
        g_value_init(&values[nr_params], G_TYPE_OBJECT);
        g_value_set_object(&values[nr_params], param->userContentManager);
        nr_params++;
    }

#if WEBKIT_CHECK_VERSION(2, 30, 0)
    if (param->websitePolicies) {
        names[nr_params] = "website-policies";
        g_value_init(&values[nr_params], G_TYPE_OBJECT);
        g_value_set_object(&values[nr_params], param->websitePolicies);
        nr_params++;
    }
#endif

    if (param->relatedView) {
        names[nr_params] = "related-view";
        g_value_init(&values[nr_params], G_TYPE_OBJECT);
        g_value_set_object(&values[nr_params], param->relatedView);
        nr_params++;
    }

    names[nr_params] = "web-view-id";
    g_value_init(&values[nr_params], G_TYPE_INT);
    g_value_set_int(&values[nr_params], param->webViewId);
    nr_params++;

    names[nr_params] = "web-view-rect";
    g_value_init(&values[nr_params], G_TYPE_POINTER);
    g_value_set_pointer(&values[nr_params], &param->webViewRect);
    nr_params++;

    names[nr_params] = "web-view-parent";
    g_value_init(&values[nr_params], G_TYPE_POINTER);
    g_value_set_pointer(&values[nr_params], param->webViewParent);
    nr_params++;

    return WEBKIT_WEB_VIEW(g_object_new_with_properties(WEBKIT_TYPE_WEB_VIEW,
                nr_params, names, values));
}

void xgui_window_inc()
{
    g_xgui_window_count++;
    g_application_hold(g_xgui_application);
}

void xgui_window_dec()
{
    g_xgui_window_count--;
    g_application_release(g_xgui_application);
}

void xgui_destroy_event(pcrdr_msg *msg)
{
    if (!msg) {
        return;
    }

    if (msg->eventName)
        purc_variant_unref(msg->eventName);
    if (msg->sourceURI)
        purc_variant_unref(msg->sourceURI);
    if (msg->elementValue)
        purc_variant_unref(msg->elementValue);
    if (msg->property)
        purc_variant_unref(msg->property);
    if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON) {
        purc_variant_unref(msg->data);
    }
}

double xgui_get_current_time()
{
      struct timeval now;
      gettimeofday(&now, 0);
      return now.tv_sec + now.tv_usec / 1000000.0;
}

void xgui_load_window_bg()
{
    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    gchar *path = g_strdup_printf("%s/%s", webext_dir ? webext_dir : ".", XGUI_WINDOW_BG);

    if (path) {
        LoadBitmapFromFile(HDC_SCREEN, &xgui_window_bg, path);
        g_xgui_window_bg = &xgui_window_bg;
        g_free(path);
    }
}

void xgui_unload_window_bg()
{
    if (g_xgui_window_bg) {
        UnloadBitmap(g_xgui_window_bg);
        g_xgui_window_bg = NULL;
    }
}

void xgui_webview_draw_background_callback(HWND hWnd, HDC hdc, RECT rect)
{
    int rw = RECTW(rect);
    int rh = RECTH(rect);

    gal_pixel oldBrushColor = SetBrushColor(hdc, PIXEL_black);
    FillBox(hdc, rect.left, rect.top, rw, rh);
    SetBrushColor(hdc, oldBrushColor);

    int bw = g_xgui_window_bg->bmWidth;
    int bh = g_xgui_window_bg->bmHeight;

    if (bw < rw || bh < rh) {
        int x = (rw - g_xgui_window_bg->bmWidth) / 2;
        int y = (rh - g_xgui_window_bg->bmHeight) / 2;
        FillBoxWithBitmap(hdc, x, y, bw, bh, g_xgui_window_bg);
    }
    else {
        FillBoxWithBitmap(hdc, 0, 0, rw, rh, g_xgui_window_bg);
    }
}

