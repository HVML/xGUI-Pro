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

unsigned nr_windows = 0;

WebKitWebView *xgui_create_webview(WebKitWebViewParam *param)
{
    const char *names[8];
    GValue    values[8] = {0, };
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
    nr_windows++;
}

void xgui_window_dec()
{
    nr_windows--;
    if (nr_windows <= 0) {
        PostQuitMessage(HWND_DESKTOP);
    }
}
