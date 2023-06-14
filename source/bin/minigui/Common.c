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
#include <assert.h>

GApplication *g_xgui_application = NULL;
HWND g_xgui_main_window = HWND_INVALID;
uint32_t g_xgui_window_count = 0;

WebKitWebView *xgui_create_webview(WebKitWebViewParam *param)
{
    const char *names[9];
    GValue    values[9] = {0, };
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
#if 0
    if (g_xgui_window_count == 2) {
        ShowWindow(g_xgui_main_window, SW_HIDE);
    }
#endif
    g_application_hold(g_xgui_application);
}

void xgui_window_dec()
{
    g_xgui_window_count--;
#if 0
    if (g_xgui_window_count == 1) {
        ShowWindow(g_xgui_main_window, SW_SHOWNORMAL);
    }
#endif
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
