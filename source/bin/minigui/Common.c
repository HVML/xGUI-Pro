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

WebKitWebView *xgui_create_webview(WebKitWebViewParam *param)
{
    return WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "is-controlled-by-automation", param->isControlledByAutomation,
                "settings", param->settings,
                "web-context", param->webContext,
                "user-content-manager", param->userContentManager,
#if WEBKIT_CHECK_VERSION(2, 30, 0)
                "website-policies", param->websitePolicies,
#endif
                "web-view-id", param->webViewId,
                "web-view-rect", param->webViewRect,
                "web-view-parent", param->webViewParent,
                NULL));
}
