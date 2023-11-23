/*
** HbdrunURISchema.c -- implementation of hbdrun URI schema.
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

#include "xguipro-features.h"
#include "HbdrunURISchema.h"
#include "BuildRevision.h"
//#include "LayouterWidgets.h"

#include "utils/utils.h"
#include "utils/hbdrun-uri.h"
#include "purcmc/server.h"
#include "purcmc/purcmc.h"

#include <webkit2/webkit2.h>
#include <purc/purc-pcrdr.h>
#include <purc/purc-helpers.h>
#include <gio/gunixinputstream.h>

#include <assert.h>
#include <stdio.h>

#define HBDRUN_SCHEMA_TYPE_VERSION          "version"
#define HBDRUN_SCHEMA_TYPE_APPS             "apps"
#define HBDRUN_SCHEMA_TYPE_STORE            "store"
#define HBDRUN_SCHEMA_TYPE_RUNNERS          "runners"
#define HBDRUN_SCHEMA_TYPE_CONFIRM          "confirm"
#define HBDRUN_SCHEMA_TYPE_ACTION           "action"

#define CONFIRM_BTN_TEXT_ACCEPT_ONCE        "接受一次"
#define CONFIRM_BTN_TEXT_ACCEPT_ALWAYS      "始终接受"
#define CONFIRM_BTN_TEXT_DECLINE            "拒绝"

typedef void (*hbdrun_handler)(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri);

static const char *error_page =
    "<html><body><h1>%d : %s</h1></body></html>";

static const char *confirm_success = "{'code':200}";

/* title, cards */
static const char *runners_page_templage = ""
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"    <head>"
"        <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>"
"        <meta name='viewport' content='width=device-width, initial-scale=1'>"
"        <!-- Bootstrap core CSS -->"
"        <link rel='stylesheet' href='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
"        <script type='text/javascript' src='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/js/bootstrap.bundle.min.js'></script>"
""
"        <style>"
"            .w-95 {"
"                width: 95%!important;"
"            }"
"        </style>"
"    </head>"
"    <body>"
"        <main>"
"            <div class='container px-4 py-5' id='custom-cards'>"
"                <h2 class='pb-2 border-bottom'>%s</h2>"
""
"                <div class='row row-cols-1 row-cols-lg-2 row-cols-xl-3 align-items-stretch g-4 py-5'>"
"%s"
"                </div>"
""
"            </div>"
"        </main>"
"    </body>"
"</html>"
"";

/* runner label, runner endpoint */
static const char *runner_template = ""
"                                <div class='list-group-item list-group-item-action d-flex' >"
"                                    <div>"
"                                        <input class='form-check-input me-1 h5' type='checkbox' value='' id='firstCheckbox'>"
"                                    </div>"
"                                    <div class='w-95'>"
"                                        <label class='form-check-label h5' for='firstCheckbox'>%s</label>"
"                                        <p class='mb-1 text-truncate'>%s</p>"
"                                    </div>"
"                                </div>"
"";

/* icon, app label, desc */
static const char *runners_card_tmpl_prefix = ""
"                    <div class='col mx-auto'>"
"                        <div class='card card-cover h-100 overflow-hidden text-bg-light rounded-4 shadow-lg p-3'>"
"                            <div class='d-inline-flex  align-items-center'>"
"                                <div class='rounded d-inline-flex align-items-center justify-content-center fs-2 mb-3'>"
"                                    <img class='d-block mx-auto rounded' src='%s' alt='' width='72' height='72' />"
"                                </div>"
"                                <h3 class='fs-2 ms-2'>%s</h3>"
"                            </div>"
"                            <p>%s</p>"
"                            <div class='list-group'>"
"";

/* button */
static const char *runners_card_tmpl_suffix = ""
"                            </div>"
"                            <button type='button' class='btn btn-primary m-3'>%s</button>"
"                        </div>"
"                    </div>"
"";

/* timeout, decline, icon url, app label, desc, accept once, accept once, accept once, accept always, accept always, Decline  */
static const char *confirm_page_template = ""
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"    <head>"
"        <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>"
"        <meta name='viewport' content='width=device-width, initial-scale=1'>"
"        <!-- Bootstrap core CSS -->"
"        <link rel='stylesheet' href='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
"        <script type='text/javascript' src='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/js/bootstrap.bundle.min.js'></script>"
""
"        <script>"
"            let httpRequest;"
"            let timerId;"
"            let timeout = %s;"
"            let decline = '%s';"
""
"            window.addEventListener('load', (event) => {"
"                timerId = setInterval(function(){"
"                    const btn = document.getElementById('id_decline');"
"                    btn.textContent = decline + '(' + timeout + ')';"
"                    timeout = timeout - 1;"
"                    if (timeout == 0) {"
"                        clearInterval(timerId);"
"                        on_result(btn);"
"                    }"
"                }, 1000);"
"            });"
"            function on_radio_change(elem) {"
"                const btn = document.getElementById('id_accept');"
"                btn.textContent = elem.value;"
"                var result = elem.getAttribute('data-result');"
"                btn.setAttribute('data-result', result);"
"            }"
""
"            function on_action_result() {"
"                if (httpRequest.readyState === XMLHttpRequest.DONE) {"
"                    window.close();"
"                }"
"            }"
""
"            function on_result(elem)"
"            {"
"                var result = elem.getAttribute('data-result');"
"                var uri = 'hbdrun://action?type=confirm&result=' + result;"
"                httpRequest = new XMLHttpRequest();"
"                httpRequest.onreadystatechange = on_action_result;"
"                httpRequest.open('POST', uri);"
"                httpRequest.send();"
"            }"
"        </script>"
""
"        <style>"
"            html,body{"
"                height:100%;"
"                padding:0;"
"                margin:0;"
"            }"
"        </style>"
"    </head>"
"    <body>"
"        <div class='px-4 text-center w-100 h-100 d-flex flex-column align-items-center justify-content-center'>"
"            <img class='d-block mx-auto mb-4' src='%s' alt='' width='72' height='57'>"
"            <h1 class='display-5 fw-bold'>%s</h1>"
"            <div class='col-lg-6 mx-auto'>"
"                <p class='lead mb-4'>%s</p>"
"                <div class='d-grid gap-2 d-flex justify-content-around'>"
"                    <div class='btn-group'>"
"                        <button type='button' class='btn btn-primary' id='id_accept' data-result='AcceptOnce' onclick='on_result(this)'>%s</button>"
"                        <button type='button' class='btn btn-primary dropdown-toggle dropdown-toggle-split' data-bs-toggle='dropdown' aria-expanded='false'>"
"                        </button>"
"                        <ul class='dropdown-menu'>"
"                            <li>"
"                                <div class='form-check mx-1'>"
"                                    <input class='form-check-input' type='radio' name='acceptRadio' id='id_accept_once' data-result='AcceptOnce' value='%s' onchange='on_radio_change(this)' checked>"
"                                    <label class='form-check-label' for='id_accept_once'>"
"                                        %s"
"                                    </label>"
"                                </div>"
"                            </li>"
"                            <li>"
"                                <div class='form-check mx-1'>"
"                                    <input class='form-check-input' type='radio' name='acceptRadio' id='id_accept_always' data-result='AcceptAlways' value='%s' onchange='on_radio_change(this)' >"
"                                    <label class='form-check-label' for='id_accept_always'>"
"                                        %s"
"                                    </label>"
"                                </div>"
"                            </li>"
"                        </ul>"
"                    </div>"
"                    <button type='button' class='btn btn-outline-secondary' data-result='Decline' id='id_decline' onclick='on_result(this)'>%s</button>"
"                </div>"
"            </div>"
"        </div>"
"    </body>"
"</html>"
"";

static void send_response(WebKitURISchemeRequest *request, guint status_code,
        const char *content_type, char *contents, size_t nr_contents,
        GDestroyNotify notify)
{
    GInputStream *stream = NULL;
    WebKitURISchemeResponse *response = NULL;;

    stream = g_memory_input_stream_new_from_data(contents, nr_contents, notify);
    response = webkit_uri_scheme_response_new(stream, nr_contents);

    webkit_uri_scheme_response_set_status(response, status_code, NULL);
    webkit_uri_scheme_response_set_content_type(response, content_type);
    webkit_uri_scheme_request_finish_with_response(request, response);

    g_object_unref(response);
    g_object_unref(stream);
}

static void send_error_response(WebKitURISchemeRequest *request, guint status_code,
        const char *content_type, char *err_info, size_t nr_err_info,
        GDestroyNotify notify)
{
    char *contents = g_strdup_printf(error_page, status_code, err_info);
    send_response(request, status_code, content_type, contents, strlen(contents),
            g_free);
    if (notify) {
        notify(err_info);
    }
}

static void on_hbdrun_versions(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void on_hbdrun_apps(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void on_hbdrun_store(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void on_hbdrun_runners(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;

    const char *icon = "hvml://localhost/_renderer/_builtin/-/assets/hvml.png";
    /* runner label, runner endpoint */
    // runner_template = ""
    char *err_info = NULL;
    GOutputStream *card_output_stream = NULL;
    GOutputStream *runner_output_stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
    if (!runner_output_stream) {
        err_info = g_strdup_printf("Can not allocate memory (%s)", uri);
        goto error;
    }

    /* icon, app label, desc, runners, switch button text */
    // runners_card_templage = ""
    card_output_stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
    if (!card_output_stream) {
        err_info = g_strdup_printf("Can not allocate memory (%s)", uri);
        goto error;
    }

    const char *app_label = "智能面板";
    const char *app_desc = "这是智能面板的应用，主要用于显示设备态态，操控相关设备";
    const char *switch_btn = "切换";
    g_output_stream_printf(card_output_stream, NULL, NULL, NULL,
            runners_card_tmpl_prefix, icon, app_label, app_desc);

    /* TODO: multiply runners */
    {
        const char *label = "主行者";
        const char *endpoint = "edpt://localhost/cn.fmsoft.hvml.xGUIPro/lockscreen";
        g_output_stream_printf(card_output_stream, NULL, NULL, NULL, runner_template,
                label, endpoint);
        label = "次行者";
        endpoint = "edpt://localhost/cn.fmsoft.hvml.xGUIPro/main";
        g_output_stream_printf(card_output_stream, NULL, NULL, NULL, runner_template,
                label, endpoint);
    }

        g_output_stream_printf(card_output_stream, NULL, NULL, NULL,
                runners_card_tmpl_suffix, switch_btn);

    /* title, cards */
    // runners_page_templage;
    {
        const char *title = "所有应用";
        gpointer *cards = g_memory_output_stream_get_data(
                G_MEMORY_OUTPUT_STREAM(card_output_stream));
        char *contents = g_strdup_printf(runners_page_templage,
                title, cards);
        if (!contents) {
            err_info = g_strdup_printf("Can not allocate memory for runners page (%s)", uri);
            LOG_WARN("Can not allocate memory for runners page (%s)", uri);
            goto error;
        }
        send_response(request, 200, "text/html", contents, strlen(contents), g_free);
        if (card_output_stream) {
            g_object_unref(card_output_stream);
        }
        return;
    }

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }
    if (card_output_stream) {
        g_object_unref(card_output_stream);
    }
}

static void on_hbdrun_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
    size_t nr_uri = strlen(uri) + 1;

    char label[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_LABEL, label);
    char *app_label = g_uri_unescape_string(label, NULL);

    char desc[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_DESC, desc);
    char *app_desc = g_uri_unescape_string(desc, NULL);

    char icon[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_ICON, icon);
    char *app_icon = g_uri_unescape_string(icon, NULL);

    char timeout[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_TIMEOUT, timeout);
    char *s_timeout = g_uri_unescape_string(timeout, NULL);

    const char *accept_once = CONFIRM_BTN_TEXT_ACCEPT_ONCE;
    const char *accept_always = CONFIRM_BTN_TEXT_ACCEPT_ALWAYS;
    const char *decline = CONFIRM_BTN_TEXT_DECLINE;

    /* timeout, decline, icon url, app label, desc, accept once,
     * accept once, accept once, accept always, accept always, Decline  */
    char *err_info = NULL;
    char *contents = g_strdup_printf(confirm_page_template, timeout, decline,
            app_icon, app_label, app_desc, accept_once, accept_once, accept_once,
            accept_always, accept_always, decline);

    g_free(app_label);
    g_free(app_desc);
    g_free(app_icon);
    g_free(s_timeout);

    if (!contents) {
        err_info = g_strdup_printf("Can not allocate memory for confirm page (%s)", uri);
        LOG_WARN("Can not allocate memory for confirm page (%s)", uri);
        goto error;
    }
    send_response(request, 200, "text/html", contents, strlen(contents), g_free);
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }
    if (contents) {
        g_free(contents);
    }
}

static void on_hbdrun_action_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    char *err_info = NULL;
    char *result = NULL;
    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_RESULT, &result)) {
        err_info = g_strdup_printf("invalid result param (%s)", uri);
        goto error;
    }

    WebKitWebView *webview = webkit_uri_scheme_request_get_web_view(request);
    g_object_set_data(G_OBJECT(webview), BROWSER_HBDRUN_ACTION_PARAM_RESULT,
            result);

    send_response(request, 200, "application/json", (char *)confirm_success,
            strlen(confirm_success), NULL);
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }

    if (result) {
        g_free(result);
    }
}

static void on_hbdrun_action(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
    char *err_info = NULL;
    char *type = NULL;
    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_TYPE, &type)) {
        err_info = g_strdup_printf("invalid rquest type (%s)", uri);
        goto error;
    }

    /* confirm */
    if (strcasecmp(type, BROWSER_HBDRUN_ACTION_TYPE_CONFIRM) == 0) {
        on_hbdrun_action_confirm(request, webContext, uri);
    }

    if (type) {
        g_free(type);
    }
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }

    if (type) {
        g_free(type);
    }
}

static struct hbdrun_handler {
    const char *operation;
    hbdrun_handler handler;
} handlers[] = {
    { HBDRUN_SCHEMA_TYPE_ACTION,            on_hbdrun_action },
    { HBDRUN_SCHEMA_TYPE_APPS,              on_hbdrun_apps },
    { HBDRUN_SCHEMA_TYPE_CONFIRM,           on_hbdrun_confirm },
    { HBDRUN_SCHEMA_TYPE_RUNNERS,           on_hbdrun_runners },
    { HBDRUN_SCHEMA_TYPE_STORE,             on_hbdrun_store },
    { HBDRUN_SCHEMA_TYPE_VERSION,           on_hbdrun_versions },
};

#define NOT_FOUND_HANDLER   ((hbdrun_handler)-1)

static hbdrun_handler find_hbdrun_handler(const char* operation)
{
    static ssize_t max = sizeof(handlers)/sizeof(handlers[0]) - 1;

    ssize_t low = 0, high = max, mid;
    while (low <= high) {
        int cmp;

        mid = (low + high) / 2;
        cmp = strcasecmp(operation, handlers[mid].operation);
        if (cmp == 0) {
            goto found;
        }
        else {
            if (cmp < 0) {
                high = mid - 1;
            }
            else {
                low = mid + 1;
            }
        }
    }

    return NOT_FOUND_HANDLER;

found:
    return handlers[mid].handler;
}

void hbdrunURISchemeRequestCallback(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext)
{
    const char *uri = webkit_uri_scheme_request_get_uri(request);
    char host[PURC_LEN_HOST_NAME + 1];

    if (!hbdrun_uri_split(uri,
            host, NULL, NULL, NULL, NULL) ||
            !purc_is_valid_host_name(host)) {
        LOG_WARN("Invalid hbdrun URI (%s): bad host (%s)", uri, host);
        goto error;
    }

    hbdrun_handler handler = find_hbdrun_handler(host);
    if (handler == NOT_FOUND_HANDLER) {
        LOG_WARN("Invalid hbdrun URI (%s): not found handle for '%s'", uri, host);
        goto error;
    }

    handler(request, webContext, uri);
    return;

error:
    send_error_response(request, 404, "text/html", (char*)uri, strlen(uri), NULL);
}

