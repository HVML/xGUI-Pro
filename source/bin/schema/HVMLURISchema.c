/*
** HVMLURISchema.c -- implementation of hvml URI schema.
**
** Copyright (C) 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
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
#include "HVMLURISchema.h"
#include "BuildRevision.h"
//#include "LayouterWidgets.h"

#include "utils/hvml-uri.h"
#include "utils/load-asset.h"
#include "purcmc/server.h"
#include "purcmc/purcmc.h"

#include <webkit2/webkit2.h>
#include <purc/purc-pcrdr.h>
#include <purc/purc-helpers.h>
#include <gio/gunixinputstream.h>

#include <assert.h>

#if PLATFORM(MINIGUI)
#include "minigui/PurcmcCallbacks.h"
#include "minigui/LayouterWidgets.h"
#include "minigui/main.h"
#else
#include "gtk/PurcmcCallbacks.h"
#include "gtk/LayouterWidgets.h"
#include "gtk/main.h"
#endif

#ifndef WEBKIT_VERSION_STRING
#   define STR(x)                  #x
#   define STR2(x)                 STR(x)
#   define WEBKIT_VERSION_STRING   \
        STR2(WEBKIT_MAJOR_VERSION) "." STR2(WEBKIT_MAJOR_VERSION)
#endif

#define DEFAULT_SPLASH_FILE        "assets/splash.jpg"
#define SPLASH_FILE_NAME           "splash"
#define SPLASH_FILE_SUFFIX         "jpg"
#define QUERY_SEPERATOR            '?'

#define REDIRECT_LOCAL_FILE         0

struct hvml_broken_down_uri {
    const char *uri;
    const char *host;
    const char *port;
    const char *app;
    const char *runner;
    const char *group;
    const char *page;
    const char *real_app;
    const char *real_runner;
};

static void print_hvml_broken_down_uri(struct hvml_broken_down_uri *st)
{
    LOG_WARN("HVML URI: uri=%s|host=%s|port=%s|app=%s|runner=%s|group=%s"
            "|page=%s|real_app=%s|real_runner=%s\n",
            st->uri, st->host, st->port, st->app, st->runner, st->group,
            st->page, st->real_app, st->real_runner);
}

void initializeWebExtensionsCallback(WebKitWebContext *context,
        gpointer user_data)
{
    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    webkit_web_context_set_web_extensions_directory(context, webext_dir);
    webkit_web_context_set_web_extensions_initialization_user_data(context,
            g_variant_new_string("HVML"));
}

static const char *blank_page = "<html></html>";

static const char *blank_page_tmpl = ""
    "<html>"
    "  <head>"
    "    <style>"
    "body {"
    "  background: black url('data:image/jpg;base64,%s') no-repeat;"
    "  background-position: center;"
    "}"
    "    </style>"
    "  </head>"
    "  <body>"
    "  </body>"
    "</html>"
"";

static const char *cover_page = ""
    "<!DOCTYPE html>"
    "<html lang='zh' class='h-100'>"
    "  <head>"
    "    <meta charset='utf-8'>"
    "    <meta name='viewport' content='width=device-width, initial-scale=1'>"
    "    <meta name='description' content='The default HVML cover'>"
    "    <meta name='author' content='Vincent Wei'>"
    "    <title>Welcome to the HVML World</title>"
    ""
    "    <!-- Bootstrap core CSS -->"
    "    <link rel='stylesheet' href='//localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
    ""
    "    <meta name='theme-color' content='#7952b3'>"
    ""
    "    <style>"
    ".btn-secondary,"
    ".btn-secondary:hover,"
    ".btn-secondary:focus {"
    "  color: #333;"
    "  text-shadow: none; /* Prevent inheritance from `body` */"
    "}"
    ""
    "body {"
    "  text-shadow: 0 .05rem .1rem rgba(0, 0, 0, .5);"
    "  box-shadow: inset 0 0 5rem rgba(0, 0, 0, .5);"
    "}"
    ""
    ".cover-container {"
    "  max-width: 42em;"
    "}"
    ""
    ".nav-masthead .nav-link {"
    "  padding: .25rem 0;"
    "  font-weight: 700;"
    "  color: rgba(255, 255, 255, .5);"
    "  background-color: transparent;"
    "  border-bottom: .25rem solid transparent;"
    "}"
    ""
    ".nav-masthead .nav-link:hover,"
    ".nav-masthead .nav-link:focus {"
    "  border-bottom-color: rgba(255, 255, 255, .25);"
    "}"
    ""
    ".nav-masthead .nav-link + .nav-link {"
    "  margin-left: 1rem;"
    "}"
    ""
    ".nav-masthead .active {"
    "  color: #fff;"
    "  border-bottom-color: #fff;"
    "}"
    "    </style>"
    ""
    "  </head>"
    ""
    "  <body class='d-flex h-100 text-center text-white bg-dark'>"
    ""
    "    <div class='cover-container d-flex w-100 h-100 p-3 mx-auto flex-column'>"
    "      <header class='mb-auto'>"
    "        <div>"
    "          <h3 class='float-md-start mb-0'>HVML</h3>"
    "          <nav class='nav nav-masthead justify-content-center float-md-end'>"
    "            <a class='nav-link active' aria-current='page' href='https://hvml.fmsoft.cn/' target='_blank'>创新</a>"
    "            <a class='nav-link' href='https://github.com/HVML' target='_blank'>开源</a>"
    "            <a class='nav-link' href='https://hvml.fmsoft.cn/community' target='_blank'>协作</a>"
    "          </nav>"
    "        </div>"
    "      </header>"
    ""
    "      <main class='px-3'>"
    "         <img class='d-block mx-auto mb-4' src='//localhost/_renderer/_builtin/-/assets/hvml.png' height='100'>"
    "        <h1>让天下没有难写的程序！</h1>"
    "        <p class='lead'>HVML 是魏永明提出的全球首款可编程标记语言，昵称“呼噜猫”。HVML 和常见的编程语言，比如 Basic、Python、C/C++ 完全不同，HVML 提出了数据驱动的概念，而程序里边也没有传统的流程控制语句，所有的操作都基于数据……</p>"
    "        <p class='lead'>"
    "          <a href='https://hvml.fmsoft.cn' class='btn btn-lg btn-secondary fw-bold border-white bg-white' target='_blank'>了解五大特征</a>"
    "        </p>"
    "      </main>"
    "";

static const char *footer_on_ok = ""
    "      <footer class='mt-auto text-white-50'>"
    "        <h4>Powered by xGUI<sup>®</sup> Pro</h4>"
    "        <p>xGUI Pro is a modern, cross-platform, and advanced HVML renderer which is based on tailored <a href='https://webkit.org' target='_blank'>WebKit</a><br/>"
    "           xGUI Pro is a free software and licensed under GPLv3+"
    "        </p>"
    "        <p>Copyright © <a href='https://www.fmsoft.cn' target='_blank'>FMSoft Technologies</a> and others</p>"
    "        <p>"
    "           <small>Status: <strong hvml-handle='731128'>Checking...</strong>.<br/>"
    "           The contents in this page will be replaced by the HVML runner <span hvml-handle='790715'></span>.<br/>"
#if PLATFORM(MINIGUI)
    "           WebKit2HBD API Version " WEBKITHBD_API_VERSION_STRING ", WebKit Version " WEBKIT_VERSION_STRING ", Build " BUILD_REVISION "</small>"
#else
    "           WebKit2GTK API Version " WEBKITGTK_API_VERSION_STRING ", WebKit Version " WEBKIT_VERSION_STRING ", Build " BUILD_REVISION "</small>"
#endif
    "        </p>"
    "      </footer>"
    ""
    "    </div>"
    ""
    "  </body>"
    "</html>"
"";

static const char *footer_on_error = ""
    "      <footer class='mt-auto text-white-50'>"
    "        <h4>Powered by xGUI<sup>®</sup> Pro</h4>"
    "        <p>xGUI Pro is a modern, cross-platform, and advanced HVML renderer which is based on tailored <a href='https://webkit.org' target='_blank'>WebKit</a><br/>"
    "           xGUI Pro is a free software licensed under GPLv3+"
    "        </p>"
    "        <p>Copyright © <a href='https://www.fmsoft.cn' target='_blank'>FMSoft Technologies and others</a></p>"
    "        <p>"
    "           <small>Status: <strong hvml-handle='731128'>ERROR</strong>.<br/>"
    "           %s <span hvml-handle='790715'></span>.<br/>"
#if PLATFORM(MINIGUI)
    "           WebKit2HBD API Version " WEBKITHBD_API_VERSION_STRING ", WebKit Version " WEBKIT_VERSION_STRING ", Build " BUILD_REVISION "</small>"
#else
    "           WebKit2GTK API Version " WEBKITGTK_API_VERSION_STRING ", WebKit Version " WEBKIT_VERSION_STRING ", Build " BUILD_REVISION "</small>"
#endif
    "        </p>"
    "      </footer>"
    ""
    "    </div>"
    ""
    "  </body>"
    "</html>"
"";

static char *load_app_runner_asset_content(const char *app, const char *runner,
        const char *name, const char *suffix, size_t *length, unsigned flags)
{
    char *buf = NULL;
    gchar *path;

    if (runner) {
        path = g_strdup_printf("/app/%s/exported/assets/%s_%s.%s", app, name,
                runner, suffix);
    }
    else {
        path = g_strdup_printf("/app/%s/exported/assets/%s.%s", app, name, suffix);
    }

    if (path) {
        FILE *f = fopen(path, "r");

        if (f) {
            if (fseek(f, 0, SEEK_END))
                goto failed;

            long len = ftell(f);
            if (len < 0)
                goto failed;

            buf = malloc(len + 1);
            if (buf == NULL)
                goto failed;

            fseek(f, 0, SEEK_SET);
            if (fread(buf, 1, len, f) < (size_t)len) {
                free(buf);
                buf = NULL;
            }
            buf[len] = '\0';

            if (length)
                *length = (size_t)len;
failed:
            fclose(f);

            if (flags & ASSET_FLAG_ONCE)
                remove(path);
        }
        else {
            goto done;
        }

    }

done:
    if (path)
        free(path);
    return buf;
}

static char *build_blank_page_with_bg(const char *app, const char *runner)
{
    gsize data_len;
    char *blank_page_with_bg = NULL;
    char *data = load_app_runner_asset_content(app, runner, SPLASH_FILE_NAME,
            SPLASH_FILE_SUFFIX, &data_len, 0);

    if (!data) {
        data = load_app_runner_asset_content(app, NULL, SPLASH_FILE_NAME,
            SPLASH_FILE_SUFFIX, &data_len, 0);
    }

    if (!data) {
        data = load_asset_content("WEBKIT_WEBEXT_DIR", WEBKIT_WEBEXT_DIR,
            DEFAULT_SPLASH_FILE, &data_len, 0);
    }

    if (!data) {
        goto out;
    }

    gchar *base64 = g_base64_encode((guchar*)data, data_len);
    if (!base64) {
        goto out_clear_data;
    }

    size_t nr = strlen(blank_page_tmpl) + strlen(base64);
    blank_page_with_bg = (char *)malloc(nr + 1);
    if (blank_page_with_bg == NULL) {
        goto out_clear_base64;
    }

    snprintf(blank_page_with_bg, nr, blank_page_tmpl, base64);

out_clear_base64:
    g_free(base64);

out_clear_data:
    free(data);

out:
    return blank_page_with_bg;
}

bool should_do_redirect(WebKitURISchemeRequest *request,
        purcmc_endpoint *endpoint, struct hvml_broken_down_uri *uri_st)
{
    (void) request;

    bool need_redirect = false;
    if (endpoint == NULL) {
        goto out;
    }

    if (endpoint->type == ET_UNIX_SOCKET) {
        if (strcmp(uri_st->group, PCRDR_GROUP_DYNAMIC) == 0) {
            need_redirect = true;
        }
    }
    else if (endpoint->type == ET_WEB_SOCKET) {
        char host_name[PURC_LEN_HOST_NAME + 1] = { 0 };
        /* TODO: get real local host name */
        strcpy(host_name, PCRDR_LOCALHOST);

        if ((strcmp(host_name, endpoint->host_name) == 0) &&
                (strcmp(uri_st->group, PCRDR_GROUP_STATIC) == 0)) {
            need_redirect = false;
        }
        else {
            need_redirect = true;
        }
    }

out:
    return need_redirect;
}

void finish_with_redirect(WebKitURISchemeRequest *request, const char *url)
{
    LOG_WARN("response with redirect url: %s\n", url);
    GInputStream *stream = NULL;
    WebKitURISchemeResponse *response = NULL;;

    gchar *contents = (gchar *)blank_page;
    gsize content_length = strlen(contents);

    stream = g_memory_input_stream_new_from_data(contents, content_length, NULL);
    response = webkit_uri_scheme_response_new(stream, content_length);

    SoupMessageHeaders *header = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
    soup_message_headers_append(header, "Location", url);
    webkit_uri_scheme_response_set_status(response, 302, NULL);
    webkit_uri_scheme_response_set_http_headers(response, header);
    webkit_uri_scheme_response_set_content_type(response, "text/html");

    webkit_uri_scheme_request_finish_with_response(request, response);

    g_object_unref(response);
    g_object_unref(stream);
}

void do_redirect(WebKitURISchemeRequest *request, purcmc_endpoint *endpoint,
        struct hvml_broken_down_uri *uri_st)
{
    GError *error = NULL;
    char schema[PURC_LEN_RUNNER_NAME + 1];

    if (strcmp(uri_st->runner, PCRDR_RUNNER_HTTP) == 0) {
        strcpy(schema, "http");
    }
    else if (strcmp(uri_st->runner, PCRDR_RUNNER_HTTPS) == 0) {
        strcpy(schema, "https");
    }
    else if (strcmp(uri_st->runner, PCRDR_RUNNER_FTP) == 0) {
        strcpy(schema, "ftp");
    }
    else if (strcmp(uri_st->runner, PCRDR_RUNNER_FTPS) == 0) {
        strcpy(schema, "ftps");
    }
    else {
        error = g_error_new(XGUI_PRO_ERROR,
                XGUI_PRO_ERROR_INVALID_HVML_URI,
                "Invalid runner name (%s)", uri_st->runner);
        webkit_uri_scheme_request_finish_error(request, error);
        return;
    }

    const char *query = uri_st->uri;
    while (*query && *query != QUERY_SEPERATOR) {
        query++;
    }

    if (query[0] == 0) {
        query = NULL;
    }
    else {
        query += 1;
        if (query[0] == 0) {
            query = NULL;
        }
    }

    size_t nr_url = PURC_LEN_HOST_NAME + PURC_LEN_APP_NAME +
        PURC_LEN_RUNNER_NAME;

    if (uri_st->port) {
        nr_url += strlen(uri_st->port);
    }

    if (uri_st->page) {
        nr_url += strlen(uri_st->page);
    }

    if (query) {
        nr_url += strlen(query);
    }

    char *url = malloc(nr_url + 1);
    if (!url) {
        error = g_error_new(XGUI_PRO_ERROR,
                XGUI_PRO_ERROR_INVALID_HVML_URI,
                "Failed to allocate memory for uri (%s)", uri_st->uri);
        webkit_uri_scheme_request_finish_error(request, error);
        return;
    }

    if (uri_st->port) {
        snprintf(url, nr_url, "%s://%s:%s/%s/exported/%s", schema,
                endpoint->host_name, uri_st->port, uri_st->real_app, uri_st->page);
    }
    else {
        snprintf(url, nr_url, "%s://%s/%s/exported/%s", schema,
                endpoint->host_name, uri_st->real_app, uri_st->page);
    }

    if (query) {
        strcat(url, "?");
        strcat(url, query);
    }

    LOG_DEBUG("origin uri (%s), redirect uri (%s)\n", uri_st->uri, url);

    finish_with_redirect(request, url);
    free(url);

}

void load_local_assets(WebKitURISchemeRequest *request,
        purcmc_endpoint *endpoint, struct hvml_broken_down_uri *uri_st)
{
    (void) endpoint;
    GError *error = NULL;
    int fd = -1;
    ssize_t max_to_load = 1024 * 4;
    gchar *contents = NULL;
    gchar *content_type = NULL;
    gsize content_length = 0;

    char prefix[PURC_LEN_APP_NAME + PURC_LEN_RUNNER_NAME + 32];

    if (strcmp(uri_st->app, PCRDR_APP_SYSTEM) == 0) {
        if (strcmp(uri_st->runner, PCRDR_RUNNER_FILESYSTEM) == 0) {
            /* try to load asset from the system filesystem */
            strcpy(prefix, "/");
        }
        else {
            /* TODO */
            strcpy(prefix, "/");
        }
    }
    else {
        snprintf(prefix, sizeof(prefix), "/app/%s/exported", uri_st->real_app);
    }

    LOG_WARN("local res: uri=%s|prefix=%s|page=%s\n", uri_st->uri, prefix, uri_st->page);

    unsigned asset_flags = 0;
    char *once_val = NULL;
    if (purc_hvml_uri_get_query_value_alloc(uri_st->uri,
                "once", &once_val) && strcmp(once_val, "yes") == 0) {
        asset_flags |= ASSET_FLAG_ONCE;
    }
    if (once_val)
        free(once_val);

    if (asset_flags & ASSET_FLAG_ONCE) {
        /* If having ASSET_FLAG_ONCE load whole contents, and remove
           the asset file. */
        contents = load_asset_content(NULL, prefix, uri_st->page, &content_length,
                asset_flags);
        max_to_load = content_length;
    }
    else {
        contents = open_and_load_asset(NULL, prefix, uri_st->page, &max_to_load,
                &fd, &content_length);
    }

    if (contents == NULL) {
        error = g_error_new(XGUI_PRO_ERROR,
                XGUI_PRO_ERROR_INVALID_HVML_URI,
                "Can not load contents from file system (%s/%s)",
                prefix, uri_st->page);
        webkit_uri_scheme_request_finish_error(request, error);
        goto done;
    }

    gboolean result_uncertain;
    content_type = g_content_type_guess(uri_st->page,
            (const guchar *)contents, max_to_load, &result_uncertain);

    LOG_DEBUG("content type of URI (%s): %s (%s)\n",
            uri_st->uri, content_type,
            result_uncertain ? "uncertain" : "certain");

    if (result_uncertain) {
        if (content_type)
            free(content_type);
        content_type = NULL;
    }

    GInputStream *stream = NULL;
    if (content_length > max_to_load && fd >= 0) {
        free(contents);
        stream = g_unix_input_stream_new(fd, TRUE);
        LOG_DEBUG("make a unix input stream for URI: %s (%s)\n",
                uri_st->uri, content_type);
    }
    else {
        if (fd >= 0)
            close(fd);

        stream = g_memory_input_stream_new_from_data(contents, content_length,
                (contents != cover_page && contents != blank_page) ? g_free : NULL);
        LOG_DEBUG("make a memory input stream for URI: %s (%s)\n",
                uri_st->uri, content_type);
    }

    webkit_uri_scheme_request_finish(request, stream, content_length,
            content_type ? content_type : "application/octet-stream");
    g_object_unref(stream);

done:
    if (content_type)
        g_free(content_type);
}

void handle_origin_host_request(WebKitURISchemeRequest *request,
        struct hvml_broken_down_uri *uri_st)
{
    purcmc_session *sess = NULL;
    purcmc_endpoint *endpoint = NULL;

    char *port = strstr(uri_st->host, ":");
    if (port) {
        *port = 0;
        port += 1;
    }
    uri_st->port = port;

    WebKitWebView *webview = webkit_uri_scheme_request_get_web_view(request);
    /* get the real app name and/or real runner name */
    if (webview) {
        sess = g_object_get_data(G_OBJECT(webview), "purcmc-session");
        if (sess) {
            endpoint = purcmc_get_endpoint_by_session(sess);
            if (endpoint) {
                uri_st->real_app = endpoint->app_name;
                uri_st->real_runner = endpoint->runner_name;
            }
        }
    }

    print_hvml_broken_down_uri(uri_st);

    if (should_do_redirect(request, endpoint, uri_st)) {
        do_redirect(request, endpoint, uri_st);
    }
    else {
        load_local_assets(request, endpoint, uri_st);
    }
}

void hvmlURISchemeRequestCallback(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext)
{
    const char *uri = webkit_uri_scheme_request_get_uri(request);
    char host[PURC_LEN_HOST_NAME + 1];
    char app[PURC_LEN_APP_NAME + 1];
    char runner[PURC_LEN_RUNNER_NAME + 1];

    char *group = NULL;
    char *page = NULL;
    char *initial_request_id = NULL;

    ssize_t max_to_load = 1024 * 4;
    int fd = -1;
#if ENABLE(COVER_PAGE)
    gchar *contents = (gchar *)cover_page, *content_type = NULL;
#else
    gchar *contents = (gchar *)blank_page, *content_type = NULL;
#endif
    gsize content_length;

    GError *error = NULL;
    gchar *error_str = NULL;

    if (!purc_hvml_uri_split(uri,
            host, app, runner, NULL, NULL) ||
            !purc_is_valid_host_name(host) ||
            !purc_is_valid_app_name(app) ||
            !purc_is_valid_runner_name(runner)) {
        error_str = g_strdup_printf(
                "Invalid HVML URI (%s): bad host, app, or runner name", uri);
        goto error;
    }

    if (!purc_hvml_uri_split_alloc(uri, NULL, NULL, NULL, &group, &page)) {
        error_str = g_strdup_printf(
                "Invalid HVML URI (%s): bad group or page name", uri);
        goto error;
    }

    LOG_DEBUG("Try to load asset for URI: %s\n", uri);

    /* check if it is an asset which was built in the renderer */
    if (strcmp(host, PCRDR_LOCALHOST) == 0 &&
            strcmp(app, PCRDR_APP_RENDERER) == 0 &&
            strcmp(runner, PCRDR_RUNNER_BUILTIN) == 0 &&
            strcmp(group, PCRDR_GROUP_NULL) == 0) {

#if 0
        contents = load_asset_content("WEBKIT_WEBEXT_DIR", WEBKIT_WEBEXT_DIR,
                page, &content_length, 0);
        max_to_load = content_length;
#else
#if REDIRECT_LOCAL_FILE
        char rpath[PATH_MAX+1];
        sprintf(rpath, "file://%s/%s", WEBKIT_WEBEXT_DIR, page);
        finish_with_redirect(request, rpath);
        goto done;
#else
        contents = open_and_load_asset("WEBKIT_WEBEXT_DIR", WEBKIT_WEBEXT_DIR,
                page, &max_to_load, &fd, &content_length);
#endif
#endif

        if (contents == NULL) {
            error = g_error_new(XGUI_PRO_ERROR,
                    XGUI_PRO_ERROR_INVALID_HVML_URI,
                    "Can not load contents from asset file (%s)", page);
            webkit_uri_scheme_request_finish_error(request, error);
            goto done;
        }
    }
    else if (strcmp(host, PCRDR_LOCALHOST) == 0) {
        char prefix[PURC_LEN_APP_NAME + PURC_LEN_RUNNER_NAME + 32];

        if (strcmp(app, PCRDR_APP_SYSTEM) == 0) {
            if (strcmp(runner, PCRDR_RUNNER_FILESYSTEM) == 0) {
                /* try to load asset from the system filesystem */
                strcpy(prefix, "/");
            }
            else {
                /* TODO */
                strcpy(prefix, "/");
            }
        }
        else if (strcmp(app, PCRDR_RUNNER_FILESYSTEM) == 0) {
            if (strcmp(runner, "_file") == 0) {
                /* try to load asset from the system filesystem */
                strcpy(prefix, "/");
            }
            else {
                /* TODO */
                strcpy(prefix, "/");
            }
        }
        else if (strcmp(app, PCRDR_APP_SELF) == 0) {
            char my_app[PURC_LEN_APP_NAME + 1];
            char my_runner[PURC_LEN_RUNNER_NAME + 1];
            const char *real_app = NULL;
            const char *real_runner = NULL;

            WebKitWebView *webview;
            webview = webkit_uri_scheme_request_get_web_view(request);
            /* get the real app name and/or real runner name */
            if (webview) {
                purcmc_session *sess;
                sess = g_object_get_data(G_OBJECT(webview), "purcmc-session");
                if (sess) {
                    char my_host[PURC_LEN_HOST_NAME + 1];
                    purc_hvml_uri_split(sess->uri_prefix,
                            my_host, my_app, my_runner, NULL, NULL);

                    real_app = my_app;
                    if (strcmp(runner, PCRDR_RUNNER_SELF) == 0) {
                        real_runner = my_runner;
                    }
                }
            }

            if (real_app) {
                snprintf(prefix, sizeof(prefix), "/app/%s/%s/",
                        real_app, real_runner ? real_runner : runner);
            }
            else {
                error_str = g_strdup_printf(
                        "Invalid HVML URI (%s): bad app or runner name", uri);
                goto error;
            }
#if REDIRECT_LOCAL_FILE
            char rpath[PATH_MAX+1];
            sprintf(rpath, "file://%s/%s", prefix, page);
            finish_with_redirect(request, rpath);
            goto done;
#endif
        }
        else {
            if (!purc_hvml_uri_get_query_value_alloc(uri,
                        "irId", &initial_request_id) ||
                    !purc_is_valid_unique_id(initial_request_id)) {
                error_str = g_strdup_printf(
                        "Invalid HVML URI (%s): bad initial request identifier", uri);
            }
            else {
                contents = (char *)blank_page;
            }

            goto error;
        }

        unsigned asset_flags = 0;
        char *once_val = NULL;
        if (purc_hvml_uri_get_query_value_alloc(uri,
                    "once", &once_val) && strcmp(once_val, "yes") == 0) {
            asset_flags |= ASSET_FLAG_ONCE;
        }
        if (once_val)
            free(once_val);

        if (asset_flags & ASSET_FLAG_ONCE) {
            /* If having ASSET_FLAG_ONCE load whole contents, and remove
               the asset file. */
            contents = load_asset_content(NULL, prefix, page, &content_length,
                    asset_flags);
            max_to_load = content_length;
        }
        else {
            contents = open_and_load_asset(NULL, prefix, page, &max_to_load,
                    &fd, &content_length);
        }

        if (contents == NULL) {
            error = g_error_new(XGUI_PRO_ERROR,
                    XGUI_PRO_ERROR_INVALID_HVML_URI,
                    "Can not load contents from file system (%s/%s)",
                    prefix, page);
            webkit_uri_scheme_request_finish_error(request, error);
            goto done;
        }
    }
    else if (strncmp(host, PCRDR_ORIGINHOST, strlen(PCRDR_ORIGINHOST)) == 0) {
        struct hvml_broken_down_uri uri_st = {
            .uri = uri,
            .host = host,
            .port = NULL,
            .app = app,
            .runner = runner,
            .group = group,
            .page = page,
            .real_app = app,
            .real_runner = runner,
        };
        handle_origin_host_request(request, &uri_st);
        goto done;
    }

error:
    if (contents == cover_page) {
        content_length = strlen(contents);
        content_type = g_strdup("text/html");
        max_to_load = content_length;
    }
    else if (contents == blank_page) {
        char *data = build_blank_page_with_bg(app, runner);
        if (data != NULL) {
            contents = data;
        }
#if 0
#if PLATFORM(MINIGUI)
        WebKitWebView *webview = webkit_uri_scheme_request_get_web_view(request);
        webkit_web_view_set_display_suppressed(webview, true);
#endif
#endif
        content_length = strlen(contents);
        content_type = g_strdup("text/html");
        max_to_load = content_length;
    }
    else {
        gboolean result_uncertain;
        content_type = g_content_type_guess(page,
                (const guchar *)contents, max_to_load, &result_uncertain);

        LOG_DEBUG("content type of URI (%s): %s (%s)\n",
                uri, content_type,
                result_uncertain ? "uncertain" : "certain");

        if (result_uncertain) {
            if (content_type)
                free(content_type);
            content_type = NULL;
        }
    }

    GInputStream *stream = NULL;
    gsize footer_length = 0;
    if (content_length > max_to_load && fd >= 0) {
        free(contents);
        stream = g_unix_input_stream_new(fd, TRUE);
        LOG_DEBUG("make a unix input stream for URI: %s (%s)\n",
                uri, content_type);
    }
    else {
        if (fd >= 0)
            close(fd);

        stream = g_memory_input_stream_new_from_data(contents, content_length,
                (contents != cover_page && contents != blank_page) ? g_free : NULL);
        if (contents == cover_page) {

            if (error_str) {
                gchar *footer = NULL;
                footer = g_strdup_printf(footer_on_error, error_str);
                footer_length = strlen(footer);
                g_memory_input_stream_add_data((GMemoryInputStream *)stream,
                        footer, footer_length, g_free);
            }
            else {
                footer_length = strlen(footer_on_ok);
                g_memory_input_stream_add_data((GMemoryInputStream *)stream,
                        footer_on_ok, footer_length, NULL);
            }
        }
        LOG_DEBUG("make a memory input stream for URI: %s (%s)\n",
                uri, content_type);
    }

    webkit_uri_scheme_request_finish(request, stream,
            content_length + footer_length,
            content_type ? content_type : "application/octet-stream");
    g_object_unref(stream);

done:
    if (error_str)
        free(error_str);
    if (error)
        g_error_free(error);
    if (group)
        free(group);
    if (page)
        free(page);
    if (initial_request_id)
        free(initial_request_id);
    if (content_type)
        g_free(content_type);
}

