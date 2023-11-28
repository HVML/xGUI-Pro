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
#include <minigui/FloatingWindow.h>
#else
#include <gtk/BrowserPlainWindow.h>
#endif

#include "utils.h"

#if PLATFORM(MINIGUI)
extern HWND g_xgui_main_window;
extern HWND g_xgui_floating_window;
#else
extern GtkWidget *g_xgui_floating_window;
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

static BrowserPlainWindow *create_plainwin_with_uri(const char *name,
        const char *title, const char *uri)
{
    BrowserPlainWindow *plainwin = NULL;
    WebKitWebContext *web_context = xguitls_get_web_context();
    if (!web_context) {
        return NULL;
    }

    struct purcmc_server *server = xguitls_get_purcmc_server();
    WebKitSettings *webkit_settings = purcmc_rdrsrv_get_user_data(server);

#if WEBKIT_CHECK_VERSION(2, 30, 0)
    WebKitWebsitePolicies *defaultWebsitePolicies = g_object_get_data(
            G_OBJECT(webkit_settings), "default-website-policies");
#endif

    WebKitUserContentManager *uc_manager;
    uc_manager = g_object_get_data(G_OBJECT(webkit_settings),
            "default-user-content-manager");

#if PLATFORM(MINIGUI)
    HWND hWnd = g_xgui_main_window;
    plainwin = BROWSER_PLAIN_WINDOW(browser_plain_window_new(hWnd,
                web_context, name, title,
                WINDOW_LEVEL_TOOLTIP, NULL, TRUE));

    WebKitWebViewParam param = {
        .webContext = web_context,
        .settings = webkit_settings,
        .userContentManager = uc_manager,
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
                web_context, name, title));

    GtkApplication *application;
    application = g_object_get_data(G_OBJECT(webkit_settings), "gtk-application");

    gtk_application_add_window(GTK_APPLICATION(application),
            GTK_WINDOW(plainwin));

    WebKitWebView *web_view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "web-context", web_context,
                "settings", webkit_settings,
                "user-content-manager", uc_manager,
                "is-controlled-by-automation",
                webkit_web_context_is_automation_allowed(web_context),
#if WEBKIT_CHECK_VERSION(2, 30, 0)
                "website-policies", defaultWebsitePolicies,
#endif
                NULL));

    g_object_set_data(G_OBJECT(web_view), "purcmc-container", plainwin);

    browser_plain_window_set_view(plainwin, web_view);
    browser_plain_window_load_uri(plainwin, uri);
    gtk_widget_show(GTK_WIDGET(plainwin));
#endif
    return plainwin;
}

int xgutils_show_confirm_window(const char *app_label, const char *app_desc,
        const char *app_icon, uint64_t timeout_seconds)
{
    int result = CONFIRM_RESULT_ID_DECLINE;
    if (!app_icon) {
        app_icon = "hvml://localhost/_renderer/_builtin/-/assets/hvml.png";
    }

    char *uri = g_strdup_printf("hbdrun://confirm?%s=%s&%s=%s&%s=%s&%s=%ld",
            CONFIRM_PARAM_LABEL, app_label,
            CONFIRM_PARAM_DESC, app_desc,
            CONFIRM_PARAM_ICON, app_icon,
            CONFIRM_PARAM_TIMEOUT, timeout_seconds);

    BrowserPlainWindow *plainwin;
    plainwin = create_plainwin_with_uri(app_label, app_label, uri);
    if (!plainwin) {
        goto out;
    }

    WebKitWebView *web_view = browser_plain_window_get_view(plainwin);

    GMainContext *context = g_main_context_default();
    while (true) {
        g_main_context_iteration(context, FALSE);
        char *p = g_object_get_data(G_OBJECT(web_view),
                BROWSER_HBDRUN_ACTION_PARAM_RESULT);
        if (p != NULL) {
            /* TODO : keep result */
            if (strcasecmp(p, CONFIRM_RESULT_DECLINE) == 0) {
                result = CONFIRM_RESULT_ID_DECLINE;
            }
            else if (strcasecmp(p, CONFIRM_RESULT_ACCEPT_ONCE) == 0) {
                result = CONFIRM_RESULT_ID_ACCEPT_ONCE;
            }
            else if (strcasecmp(p, CONFIRM_RESULT_ACCEPT_ALWAYS) == 0) {
                result = CONFIRM_RESULT_ID_ACCEPT_ALWAYS;
            }
            g_object_set_data(G_OBJECT(web_view),
                BROWSER_HBDRUN_ACTION_PARAM_RESULT, NULL);
            g_free(p);
            break;
        }
    }

out:
    if (uri) {
        g_free(uri);
    }
    return result;
}

int xgutils_show_runners_window(void)
{
    const char *uri = "hbdrun://runners";
    create_plainwin_with_uri("runners", "runners", uri);
    return 0;
}

int xguitls_shake_round_window(void)
{
#if PLATFORM(MINIGUI)
    SendNotifyMessage(g_xgui_floating_window,
            MSG_XGUIPRO_NEW_RDR, 0, 0);
#else
    g_signal_emit_by_name(g_xgui_floating_window, "shake-window");
#endif
    return 0;
}
