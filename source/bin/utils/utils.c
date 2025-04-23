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

#include <gio/gio.h>
#include <glib-unix.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <purcmc/purcmc.h>
#include <purcmc/server.h>
#include <schema/HbdrunURISchema.h>
#include <webkit2/webkit2.h>

#if PLATFORM(MINIGUI)
#include <minigui/BrowserPlainWindow.h>
#include <minigui/FloatingWindow.h>
#else
#include <gtk/BrowserPlainWindow.h>
#endif

#include "layouter/layouter.h"

#include "utils.h"

#define XGUTILS_DPI_DEFAULT 96

#if PLATFORM(MINIGUI)
#define XGUTILS_DENSITY_DEFAULT "1.0"
#define LEN_ENGINE_NAME 23
extern HWND g_xgui_main_window;
extern HWND g_xgui_floating_window;
#else
extern GtkWidget* g_xgui_floating_window;
#endif

static float xgutils_density_minimal = 1.0f;
static float xgutils_density_default = 1.0f;

time_t xgutils_get_monotoic_time_ms(void)
{
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * 1000 + tp.tv_nsec * 1.0E-6;
}

void xgutils_global_set_data(const char* key, void* pointer)
{
    GApplication* app = g_application_get_default();
    if (app) {
        g_object_set_data(G_OBJECT(app), key, pointer);
    }
}

void* xgutils_global_get_data(const char* key)
{
    GApplication* app = g_application_get_default();
    if (app) {
        return g_object_get_data(G_OBJECT(app), key);
    }
    return NULL;
}

static BrowserPlainWindow *create_plainwin_with_uri(const char *name,
        const char *title, const char *uri, int x, int y, int w, int h)
{
    BrowserPlainWindow* plainwin = NULL;
    WebKitWebContext* web_context = xguitls_get_web_context();
    if (!web_context) {
        return NULL;
    }

    struct purcmc_server* server = xguitls_get_purcmc_server();
    WebKitSettings* webkit_settings = purcmc_rdrsrv_get_user_data(server);

#if WEBKIT_CHECK_VERSION(2, 30, 0)
    WebKitWebsitePolicies* defaultWebsitePolicies = g_object_get_data(
        G_OBJECT(webkit_settings), "default-website-policies");
#endif

    WebKitUserContentManager* uc_manager;
    uc_manager = g_object_get_data(G_OBJECT(webkit_settings),
        "default-user-content-manager");

#if PLATFORM(MINIGUI)
    HWND hWnd = g_xgui_main_window;
    plainwin = BROWSER_PLAIN_WINDOW(browser_plain_window_new_ex(hWnd,
                web_context, name, title,
                WINDOW_LEVEL_TOOLTIP, NULL, TRUE, x, y, w, h));

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
    WebKitWebView* web_view = browser_plain_window_get_view(plainwin);
    g_object_set_data(G_OBJECT(web_view), "purcmc-container", plainwin);
#else
    plainwin = BROWSER_PLAIN_WINDOW(browser_plain_window_new(NULL,
        web_context, name, title));

    GtkApplication* application;
    application = g_object_get_data(G_OBJECT(webkit_settings), "gtk-application");

    gtk_application_add_window(GTK_APPLICATION(application),
        GTK_WINDOW(plainwin));

    WebKitWebView* web_view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
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

int xgutils_show_confirm_window(const char* app_name, const char* app_label,
    const char* app_desc, const char* app_icon, uint64_t timeout_seconds)
{
    int result = CONFIRM_RESULT_ID_DECLINE;
    if (!app_icon) {
        app_icon = "hvml://localhost/_renderer/_builtin/-/assets/hvml.png";
    }

    char* uri = g_strdup_printf("hbdrun://confirm?%s=%s&%s=%s&%s=%s&%s=%ld",
        CONFIRM_PARAM_LABEL, app_label,
        CONFIRM_PARAM_DESC, app_desc,
        CONFIRM_PARAM_ICON, app_icon,
        CONFIRM_PARAM_TIMEOUT, timeout_seconds);

    BrowserPlainWindow *plainwin;
    plainwin = create_plainwin_with_uri(app_label, app_label, uri, 0, 0, 0, 0);
    if (!plainwin) {
        goto out;
    }

    WebKitWebView* web_view = browser_plain_window_get_view(plainwin);

    GMainContext* context = g_main_context_default();
    while (true) {
        g_main_context_iteration(context, FALSE);
        char* p = g_object_get_data(G_OBJECT(web_view),
            BROWSER_HBDRUN_ACTION_PARAM_RESULT);
        if (p != NULL) {
            /* TODO : keep result */
            if (strcasecmp(p, CONFIRM_RESULT_DECLINE) == 0) {
                result = CONFIRM_RESULT_ID_DECLINE;
            } else if (strcasecmp(p, CONFIRM_RESULT_ACCEPT_ONCE) == 0) {
                result = CONFIRM_RESULT_ID_ACCEPT_ONCE;
            } else if (strcasecmp(p, CONFIRM_RESULT_ACCEPT_ALWAYS) == 0) {
                result = CONFIRM_RESULT_ID_ACCEPT_ALWAYS;
                xgutils_set_app_confirm(app_name);
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

#define LEN_BUFF_LONGLONGINT 128
int xgutils_show_dup_confirm_window(purcmc_endpoint *endpoint)
{
    bool live = false;
    struct purcmc_server *srv = xguitls_get_purcmc_server();
    gs_list* node = srv->dangling_endpoints;
    while (node) {
        if (node->data == endpoint) {
            live = true;
        }
        node = node->next;
    }

    if (!live) {
        purc_log_error ("The endpoint: %p not exists\n", endpoint);
        return CONFIRM_RESULT_ID_DECLINE;
    }

//    const char *app_name = endpoint->app_name;
    const char *app_label = endpoint->app_label;
    const char *app_desc = endpoint->app_desc;
    const char *app_icon = endpoint->app_icon;
    uint64_t timeout_seconds = endpoint->timeout_seconds;

    int result = CONFIRM_RESULT_ID_DECLINE;
    if (!app_icon) {
        app_icon = "hvml://localhost/_renderer/_builtin/-/assets/hvml.png";
    }

    char *uri = g_strdup_printf(
            "hbdrun://confirm?type=dup&%s=%s&%s=%s&%s=%s&%s=%ld",
            CONFIRM_PARAM_LABEL, app_label,
            CONFIRM_PARAM_DESC, app_desc,
            CONFIRM_PARAM_ICON, app_icon,
            CONFIRM_PARAM_TIMEOUT, timeout_seconds);

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
#if PLATFORM(MINIGUI)
    w = RECTW(g_rcScr) * 0.3;
    h = RECTH(g_rcScr) * 0.5;
    x = g_rcScr.right - w;
#else
    struct ws_metrics metrics;
    gtk_imp_get_monitor_geometry(&metrics);
    w = metrics.width * 0.3;
    h = metrics.height * 0.5;
    x = metrics.width - w;
#endif
    BrowserPlainWindow *plainwin;
    plainwin = create_plainwin_with_uri(app_label, app_label, uri, x, y, w, h);
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
            else if (strcasecmp(p, CONFIRM_RESULT_ACCEPT) == 0) {
                result = CONFIRM_RESULT_ID_ACCEPT;
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

int xgutils_show_dup_close_confirm_window(purcmc_endpoint *endpoint)
{
    const char *app_label = endpoint->app_label;

    int result = CONFIRM_RESULT_ID_DECLINE;

    char *uri = g_strdup("hbdrun://confirm?type=dupClose");
    purc_log_warn("------------xgutils_show_dup_close_confirm_window----------------\n");

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
#if PLATFORM(MINIGUI)
    HWND hWnd = g_xgui_main_window;
    RECT rc;
    GetWindowRect(hWnd, &rc);
    w = RECTW(rc) * 0.3;
    h = RECTH(rc) * 0.5;
    x = rc.right - w;
#endif
    BrowserPlainWindow *plainwin;
    plainwin = create_plainwin_with_uri(app_label, app_label, uri, x, y, w, h);
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
            else if (strcasecmp(p, CONFIRM_RESULT_ACCEPT) == 0) {
                result = CONFIRM_RESULT_ID_ACCEPT;
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
    create_plainwin_with_uri("runners", "runners", uri, 0, 0, 0, 0);
    return 0;
}

int xgutils_show_windows_window(void)
{
    const char *uri = "hbdrun://windows";
    create_plainwin_with_uri("windows", "windows", uri, 0, 0, 0, 0);
    return 0;
}

int xguitls_shake_round_window(void)
{
#if PLATFORM(MINIGUI)
    SendNotifyMessage(g_xgui_floating_window, MSG_XGUIPRO_NEW_RDR, 0, 0);
#else
    g_signal_emit_by_name(g_xgui_floating_window, "shake-window");
#endif
    return 0;
}

int remove_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint);
static gboolean show_dup_close_confirm_window_callback(gpointer data)
{
    purcmc_endpoint *endpoint = (purcmc_endpoint *)data;
    int ret = xgutils_show_dup_close_confirm_window(endpoint);
    if (ret != CONFIRM_RESULT_ID_DECLINE) {
        struct purcmc_server *server = xguitls_get_purcmc_server();

        const char *name;
        void *next, *data;
        kvlist_for_each_safe(&server->endpoint_list, name, next, data) {
            purcmc_endpoint *ept = *(purcmc_endpoint **)data;
            if (ept == endpoint) {
                remove_endpoint(server, endpoint);
            }
        }
    }
    return G_SOURCE_REMOVE;
}

static gboolean show_dup_close_confirm_window_cast_callback(gpointer data)
{
    purcmc_endpoint *endpoint = (purcmc_endpoint *)data;
    int ret = CONFIRM_RESULT_ACCEPT;
    if (ret != CONFIRM_RESULT_ID_DECLINE) {
        struct purcmc_server *server = xguitls_get_purcmc_server();

        const char *name;
        void *next, *data;
        kvlist_for_each_safe(&server->endpoint_list, name, next, data) {
            purcmc_endpoint *ept = *(purcmc_endpoint **)data;
            if (ept == endpoint) {
                remove_endpoint(server, endpoint);
            }
        }
    }
    return G_SOURCE_REMOVE;
}

static int show_dup_close_confirm_window(struct purcmc_server *server)
{
    const char *name;
    void *next, *data;
    purcmc_endpoint *endpoint = NULL;
    kvlist_for_each_safe(&server->endpoint_list, name, next, data) {
        purcmc_endpoint *ept = *(purcmc_endpoint **)data;
        if (endpoint == NULL || ept->t_created_session > endpoint->t_created_session) {
            endpoint = ept;
        }
    }

    g_timeout_add(200, show_dup_close_confirm_window_callback, endpoint);
    return 0;
}

static int show_dup_close_confirm_window_cast(struct purcmc_server *server)
{
    const char *name;
    void *next, *data;
    purcmc_endpoint *endpoint = NULL;
    kvlist_for_each_safe(&server->endpoint_list, name, next, data) {
        purcmc_endpoint *ept = *(purcmc_endpoint **)data;
        if (endpoint == NULL || ept->t_created_session > endpoint->t_created_session) {
            endpoint = ept;
        }
    }

    show_dup_close_confirm_window_cast_callback(endpoint);
    return 0;
}

static int show_dup_choose_window(struct purcmc_server *server)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
#if PLATFORM(MINIGUI)
    w = RECTW(g_rcScr) * 0.3;
    h = RECTH(g_rcScr) * 0.6;
    x = g_rcScr.right - w;
#else
    struct ws_metrics metrics;
    gtk_imp_get_monitor_geometry(&metrics);
    w = metrics.width * 0.3;
    h = metrics.height * 0.6;
    x = metrics.width - w;
#endif

    const char *uri = "hbdrun://dupchoose";
    create_plainwin_with_uri("dupchoose", "dupchoose", uri, x, y, w, h);
    return 0;
}

int xgutils_on_floating_button_click(void)
{
    struct purcmc_server *server = xguitls_get_purcmc_server();
    unsigned int count = kvlist_count(&server->endpoint_list);
    if (count > 0) {
        return show_dup_close_confirm_window(server);
    }
    return show_dup_choose_window(server);
}

int xgutils_close_screen_cast(void)
{
    struct purcmc_server *server = xguitls_get_purcmc_server();
    unsigned int count = kvlist_count(&server->endpoint_list);
    if (count > 1) {
        return show_dup_close_confirm_window_cast(server);
    }
    return -1;
}

#if PLATFORM(MINIGUI)
uint64_t mg_imp_get_last_widget(void *session);
#else
uint64_t gtk_imp_get_last_widget(void *session);
#endif

static uint64_t session_get_last_widget(void *session)
{
#if PLATFORM(MINIGUI)
    return mg_imp_get_last_widget(session);
#else
    return gtk_imp_get_last_widget(session);
#endif
}

int xgutils_split_screen(void)
{
    struct purcmc_server *server = xguitls_get_purcmc_server();
    unsigned int count = kvlist_count(&server->endpoint_list);
    if (count != 2) {
        return -1;
    }

    purcmc_endpoint *first = NULL;
    purcmc_endpoint *last = NULL;

    const char *name;
    void *next, *data;
    kvlist_for_each_safe(&server->endpoint_list, name, next, data) {
        purcmc_endpoint *ept = *(purcmc_endpoint **)data;
        if (first == NULL) {
            first = ept;
        }
        else if (ept->t_start_session < first->t_start_session) {
            last = first;
            first = ept;
        }
        else {
            last = ept;
        }
    }

    uint64_t firstHandle = session_get_last_widget(first->session);
    if (!firstHandle) {
        return -1;
    }

    uint64_t lastHandle = session_get_last_widget(last->session);
    if (!lastHandle) {
        return -1;
    }

    BrowserPlainWindow *firstWindow = (BrowserPlainWindow *) firstHandle;
    BrowserPlainWindow *lastWindow = (BrowserPlainWindow *) lastHandle;

#if PLATFORM(MINIGUI)
    int w = RECTW(g_rcScr) / 2;
    int h = RECTH(g_rcScr);
#else
    struct ws_metrics metrics;
    gtk_imp_get_monitor_geometry(&metrics);
    int w = metrics.width;
    int h = metrics.width;
#endif

    browser_plain_window_move(firstWindow, 0, 0, w, h, true);
    browser_plain_window_move(lastWindow, w, 0, w, h, true);
    return 0;
}

int accept_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint);


int xgutils_pull_screen_cast(void)
{
    struct purcmc_server *server = xguitls_get_purcmc_server();
    gs_list* node = server->dangling_endpoints;
    purcmc_endpoint *endpoint;

    if (!node) {
        /* not found app */
        purc_log_warn ("There are no dangling endpoints.\n");
        return 0;
    }

    endpoint = (purcmc_endpoint *)node->data;
    if (node->next) {
        return show_dup_choose_window(server);
    }

    if (endpoint->t_start_session == 0) {
        purc_log_warn ("The dangling endpoints not start session.\n");
        return 0;
    }

    return accept_endpoint(server, endpoint);
}

#define CONFIR_INFO_PATH "/app/cn.fmsoft.hybridos.xguipro/var/confirm_info.json"

purc_variant_t
xgutils_load_confirm_infos(void)
{
    purc_variant_t infos = purc_variant_load_from_json_file(CONFIR_INFO_PATH);

    if (!infos) {
        infos = purc_variant_make_object_0();
    }

    return infos;
}

void xgutils_save_confirm_infos(void)
{
    purc_variant_t infos = xgutils_get_confirm_infos();
    purc_rwstream_t rws = purc_rwstream_new_from_file(CONFIR_INFO_PATH, "w");
    purc_variant_serialize(infos, rws, 0, PCVRNT_SERIALIZE_OPT_PLAIN, NULL);
    purc_rwstream_destroy(rws);
}

purc_variant_t xgutils_get_confirm_infos(void)
{
    struct purcmc_server* server = xguitls_get_purcmc_server();
    if (!server->confirm_infos) {
        server->confirm_infos = xgutils_load_confirm_infos();
    }
    return server->confirm_infos;
}

bool xgutils_is_app_confirm(const char* app)
{
    purc_variant_t infos = xgutils_get_confirm_infos();
    purc_variant_t v = purc_variant_object_get_by_ckey(infos, app);
    purc_clr_error();
    return v ? true : false;
}

void xgutils_set_app_confirm(const char* app)
{
    purc_variant_t infos = xgutils_get_confirm_infos();
    purc_variant_t v = purc_variant_make_boolean(true);
    purc_variant_object_set_by_ckey(infos, app, v);
    purc_variant_unref(v);
    xgutils_save_confirm_infos();
}

static float parse_density(const char* density)
{
    if (!density) {
        return xgutils_density_default;
    }

    gdouble value;
    gchar* end;
    errno = 0;
    value = g_ascii_strtod(density, &end);
    if (errno == ERANGE || value > G_MAXFLOAT || value < G_MINFLOAT) {
        value = xgutils_density_default;
        goto out;
    }

    if (errno || density == end) {
        value = xgutils_density_default;
        goto out;
    }

    if (value < xgutils_density_minimal) {
        value = xgutils_density_minimal;
    }

out:
    return value;
}

#if PLATFORM(MINIGUI)
static void get_engine_from_etc(char* engine)
{
#if defined(WIN32) || !defined(__NOUNIX__)
    char* env_value;

    if ((env_value = getenv("MG_GAL_ENGINE"))) {
        strncpy(engine, env_value, LEN_ENGINE_NAME);
        engine[LEN_ENGINE_NAME] = '\0';
    } else
#endif
#ifndef _MG_MINIMALGDI
        if (GetMgEtcValue("system", "gal_engine", engine, LEN_ENGINE_NAME) < 0) {
        engine[0] = '\0';
    }
#else /* _MG_MINIMALGDI */
#ifdef _MGGAL_PCXVFB
    strcpy(engine, "pc_xvfb");
#else
    strcpy(engine, "dummy");
#endif
#endif /* _MG_MINIMALGDI */
}

static int get_dpi_from_etc(const char* engine)
{
    int dpi;

    if (GetMgEtcIntValue(engine, "density", &dpi) < 0)
        dpi = GDCAP_DPI_DEFAULT;
    else if (dpi < GDCAP_DPI_MINIMAL)
        dpi = GDCAP_DPI_MINIMAL;

    return dpi;
}

#define DENSITY_BUF_LEN 10
static float get_density_from_etc(const char* engine)
{
    char density[DENSITY_BUF_LEN + 1] = { 0 };

    if (GetMgEtcValue(engine, "density", density, DENSITY_BUF_LEN) < 0) {
        strcpy(density, XGUTILS_DENSITY_DEFAULT);
    }
    return parse_density(density);
}
#endif

float xgutils_get_density(void)
{
    const char* density = g_getenv("XGUIPRO_DENSITY");
    if (density) {
        return parse_density(density);
    }

#if PLATFORM(MINIGUI)
    char engine[LEN_ENGINE_NAME + 1] = { 0 };
    get_engine_from_etc(engine);
    return get_density_from_etc(engine);
#else
    return xgutils_density_default;
#endif
}

int xgutils_get_dpi(void)
{
#if PLATFORM(MINIGUI)
    char engine[LEN_ENGINE_NAME + 1] = { 0 };
    get_engine_from_etc(engine);
    return get_dpi_from_etc(engine);
#else
    return XGUTILS_DPI_DEFAULT;
#endif
}

void xgutils_set_webview_density(WebKitWebView* webview)
{
#if WEBKIT_CHECK_VERSION(2, 30, 0)
    webkit_web_view_set_zoom_level(webview, xgutils_get_density());
#else
    webkit_web_view_set_intrinsic_device_scale_factor(webview,
        xgutils_get_density());
#endif
}
