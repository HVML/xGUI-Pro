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
#define XGUI_WINDOW_BG        "assets/splash.jpg"

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

static void calc_pos(int w, int h, int cw, int ch, int *ox, int *oy, int *ow, int *oh)
{
    float sw = (float) w / (float) cw;
    float sh = (float) h / (float) ch;

    if (sw > sh) {
        if (sh > 1.0f) {
            *ox = -(w - cw) / 2;
            *oy = -(h - ch) / 2;
            *ow = w;
            *oh = h;
        }
        else if (sh < 1.0f) {
            *ow = w / sh;
            *oh = ch;
            *ox = -(*ow - cw) / 2;
            *oy = 0;
        }
        else {
            *ox = -(w - cw) / 2;
            *oy = 0;
            *ow = w;
            *oh = h;
        }
    }
    else if (sh > sw) {
        if (sw > 1.0f) {
            *ox = -(w - cw) / 2;
            *oy = -(h - ch) / 2;
            *ow = w;
            *oh = h;
        }
        else if (sw < 1.0f) {
            *ow = cw;;
            *oh =  h / sw;
            *ox = 0;
            *oy = -(*oh - ch) / 2;
        }
        else {
            *ox = 0;
            *oy = -(h - ch) / 2;
            *ow = w;
            *oh = h;
        }
    }
    else {
        *ox = 0;
        *oy = 0;
        *ow = cw;
        *oh = ch;
    }

}

void xgui_webview_draw_background_callback(HWND hWnd, HDC hdc, RECT rect)
{
    int rw = RECTW(rect);
    int rh = RECTH(rect);

    int bw = g_xgui_window_bg->bmWidth;
    int bh = g_xgui_window_bg->bmHeight;

    int x = 0;
    int y = 0;
    int dw = 0;
    int dh = 0;
    calc_pos(bw, bh, rw, rh, &x, &y, &dw, &dh);
    FillBoxWithBitmap(hdc, x, y, dw, dh, g_xgui_window_bg);
}

enum EffMotionType xgui_get_motion_type(purc_window_transition_function func)
{
    switch (func) {
        case PURC_WINDOW_TRANSITION_FUNCTION_LINEAR:
            return Linear;
        case PURC_WINDOW_TRANSITION_FUNCTION_INQUAD:
            return InQuad;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTQUAD:
            return OutQuad;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTQUAD:
            return InOutQuad;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINQUAD:
            return OutInQuad;
        case PURC_WINDOW_TRANSITION_FUNCTION_INCUBIC:
            return InCubic;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTCUBIC:
            return OutCubic;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTCUBIC:
            return InOutCubic;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINCUBIC:
            return OutInCubic;
        case PURC_WINDOW_TRANSITION_FUNCTION_INQUART:
            return InQuart;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTQUART:
            return OutQuart;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTQUART:
            return InOutQuart;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINQUART:
            return OutInQuart;
        case PURC_WINDOW_TRANSITION_FUNCTION_INQUINT:
            return InQuint;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTQUINT:
            return OutQuint;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTQUINT:
            return InOutQuint;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINQUINT:
            return OutInQuint;
        case PURC_WINDOW_TRANSITION_FUNCTION_INSINE:
            return InSine;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTSINE:
            return OutSine;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTSINE:
            return InOutSine;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINSINE:
            return OutInSine;
        case PURC_WINDOW_TRANSITION_FUNCTION_INEXPO:
            return InExpo;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTEXPO:
            return OutExpo;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTEXPO:
            return InOutExpo;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINEXPO:
            return OutInExpo;
        case PURC_WINDOW_TRANSITION_FUNCTION_INCIRC:
            return InCirc;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTCIRC:
            return OutCirc;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTCIRC:
            return InOutCirc;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINCIRC:
            return OutInCirc;
        case PURC_WINDOW_TRANSITION_FUNCTION_INELASTIC:
            return InElastic;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTELASTIC:
            return OutElastic;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTELASTIC:
            return InOutElastic;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINELASTIC:
            return OutInElastic;
        case PURC_WINDOW_TRANSITION_FUNCTION_INBACK:
            return InBack;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTBACK:
            return OutBack;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTBACK:
            return InOutBack;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINBACK:
            return OutInBack;
        case PURC_WINDOW_TRANSITION_FUNCTION_INBOUNCE:
            return InBounce;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTBOUNCE:
            return OutBounce;
        case PURC_WINDOW_TRANSITION_FUNCTION_INOUTBOUNCE:
            return InOutBounce;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTINBOUNCE:
            return OutInBounce;
        case PURC_WINDOW_TRANSITION_FUNCTION_INCURVE:
            return InCurve;
        case PURC_WINDOW_TRANSITION_FUNCTION_OUTCURVE:
            return OutCurve;
        case PURC_WINDOW_TRANSITION_FUNCTION_SINECURVE:
            return SineCurve;
        case PURC_WINDOW_TRANSITION_FUNCTION_COSINECURVE:
            return CosineCurve;
        default:
            return NCurveTypes;
    }
}

