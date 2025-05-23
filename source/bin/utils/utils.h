/*
** utils.h -- The internal utility interfaces
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

#ifndef XGUI_PRO_BIN_UTILS_H
#define XGUI_PRO_BIN_UTILS_H

#include <config.h>
#include <purc/purc-helpers.h>
#include <webkit2/webkit2.h>

#include <stddef.h>

#include <purcmc/server.h>

#ifdef NDEBUG
#define LOG_DEBUG(x, ...)
#else
#define LOG_DEBUG(x, ...)   \
    purc_log_debug("%s: " x, __func__, ##__VA_ARGS__)
#endif /* not defined NDEBUG */

#define LOG_ERROR(x, ...)   \
    purc_log_error("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_WARN(x, ...)    \
    purc_log_warn("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_INFO(x, ...)    \
    purc_log_info("%s: " x, __func__, ##__VA_ARGS__)

#define APP_PROP_PURCMC_SERVER      "app-prop-purcmc-server"
#define APP_PROP_WEB_CONTEXT        "app-prop-web-context"

#define xgutils_set_purcmc_server(server) \
    xgutils_global_set_data(APP_PROP_PURCMC_SERVER, server)
#define xguitls_get_purcmc_server() \
    (struct purcmc_server *)xgutils_global_get_data(APP_PROP_PURCMC_SERVER)

#define xgutils_set_web_context(ctxt) \
    xgutils_global_set_data(APP_PROP_WEB_CONTEXT, ctxt)
#define xguitls_get_web_context() \
    (WebKitWebContext *)xgutils_global_get_data(APP_PROP_WEB_CONTEXT)

#define CONFIRM_RESULT_ID_ACCEPT                        0
#define CONFIRM_RESULT_ID_ACCEPT_ONCE                   1
#define CONFIRM_RESULT_ID_ACCEPT_ALWAYS                 2
#define CONFIRM_RESULT_ID_DECLINE                       3

#if PLATFORM(MINIGUI)
#define     MSG_XGUIPRO_IDLE            (MSG_USER + 2002)
#endif

#ifdef __cplusplus
extern "C" {
#endif

time_t xgutils_get_monotoic_time_ms(void);

void xgutils_global_set_data(const char *key, void *pointer);
void *xgutils_global_get_data(const char *key);

int xgutils_show_confirm_window(const char *app_name, const char *app_label,
        const char *app_desc, const char *app_icon, uint64_t timeout_seconds);

int xgutils_show_dup_confirm_window(purcmc_endpoint *endpoint);
int xgutils_show_dup_close_confirm_window(purcmc_endpoint *endpoint);

int xgutils_show_runners_window(void);

int xgutils_show_windows_window(void);

int xguitls_shake_round_window(void);

int xgutils_on_floating_button_click(void);

int xgutils_close_screen_cast(void);

int xgutils_pull_screen_cast(void);

int xgutils_split_screen(void);

purc_variant_t
xgutils_load_confirm_infos(void);

void xgutils_save_confirm_infos(void);

purc_variant_t
xgutils_get_confirm_infos(void);

bool
xgutils_is_app_confirm(const char *app);

void
xgutils_set_app_confirm(const char *app);

float
xgutils_get_density(void);

int
xgutils_get_dpi(void);

void
xgutils_set_webview_density(WebKitWebView *webview);


#ifdef __cplusplus
}
#endif

#endif  /* XGUI_PRO_BIN_UTILS_H */

