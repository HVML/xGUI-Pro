/*
** utils.c -- The internal utility interfaces
**
** Copyright (C) 2023 FMSoft (http://www.fmsoft.cn)
**
** Author: XueShuming
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include <webkit2/webkit2.h>
#include <purcmc/purcmc.h>
#include <schema/HbdrunURISchema.h>

#if PLATFORM(MINIGUI)
#include <minigui/BrowserPlainWindow.h>
#else
#include <gtk/BrowserPlainWindow.h>
#endif

#include "utils.h"

#if PLATFORM(MINIGUI)
extern HWND g_xgui_main_window;
#endif

time_t xgutils_get_monotoic_time_ms(void)
{
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * 1000 + tp.tv_nsec * 1.0E-6;
}

void xgutils_global_set_data(const char *key, void *pointer)
{
    GApplication *app = g_application_get_default();
    if (app) {
        g_object_set_data(G_OBJECT(app), key, pointer);
    }
}

void *xgutils_global_get_data(const char *key)
{
    GApplication *app = g_application_get_default();
    if (app) {
        return g_object_get_data(G_OBJECT(app), key);
    }
    return NULL;
}

int xgutils_show_confirm_window(const char *app_label, const char *app_desc,
        const char *app_icon, uint64_t timeout_seconds)
{
    (void) app_label;
    (void) app_desc;
    (void) app_icon;
    (void) timeout_seconds;

    if (!app_icon) {
        app_icon = "hvml://localhost/_renderer/_builtin/-/assets/hvml.png";
    }

    WebKitWebContext *web_context = xguitls_get_web_context();
    if (!web_context) {
        return -1;
    }

    struct purcmc_server *server = xguitls_get_purcmc_server();

    char *uri = g_strdup_printf("hbdrun://confirm?label=%s&desc=%s&icon=%s",
            app_label, app_desc, app_icon);

    BrowserPlainWindow *plainwin;
#if PLATFORM(MINIGUI)
    HWND hWnd = GetActiveWindow();
    hWnd = hWnd ? hWnd : g_xgui_main_window;
    plainwin = BROWSER_PLAIN_WINDOW(browser_plain_window_new(hWnd,
                web_context, app_label, app_label,
                WINDOW_LEVEL_HIGHER, NULL, TRUE));
    WebKitSettings *webkit_settings = purcmc_rdrsrv_get_user_data(server);
#if WEBKIT_CHECK_VERSION(2, 30, 0)
    WebKitWebsitePolicies *defaultWebsitePolicies = g_object_get_data(
            G_OBJECT(webkit_settings), "default-website-policies");
#endif

    WebKitWebViewParam param = {
        .webContext = web_context,
        .settings = webkit_settings,
        .userContentManager = NULL,
        .isControlledByAutomation = webkit_web_context_is_automation_allowed(web_context),
#if WEBKIT_CHECK_VERSION(2, 30, 0)
        .websitePolicies = defaultWebsitePolicies,
#endif
        .webViewId = IDC_BROWSER,
    };
    browser_plain_window_set_view(plainwin, &param);
    browser_plain_window_load_uri(plainwin, uri);
    WebKitWebView *web_view = browser_plain_window_get_view(plainwin);
    g_object_set_data(G_OBJECT(web_view), "purcmc-container", plainwin);
#else
    plainwin = BROWSER_PLAIN_WINDOW(browser_plain_window_new(NULL,
                web_context, label, label));
#endif

    GMainContext *context = g_main_context_default();
    int result = IDNO;
    while (true) {
        g_main_context_iteration(context, FALSE);
        char *p = g_object_get_data(G_OBJECT(web_view),
                BROWSER_HBDRUN_ACTION_PARAM_RESULT);
        if (p != NULL) {
            /* TODO : keep result */
            if (strcasecmp(p, CONFIRM_RESULT_DECLINE) == 0) {
                result = IDNO;
            }
            else {
                result = IDYES;
            }
            g_object_set_data(G_OBJECT(web_view),
                BROWSER_HBDRUN_ACTION_PARAM_RESULT, NULL);
            g_free(p);
            break;
        }
    }

    if (uri) {
        g_free(uri);
    }
    return result;
}

