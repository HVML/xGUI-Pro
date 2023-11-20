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
#include "purcmc/server.h"
#include "purcmc/purcmc.h"

#include <webkit2/webkit2.h>
#include <purc/purc-pcrdr.h>
#include <purc/purc-helpers.h>
#include <gio/gunixinputstream.h>

#include <assert.h>

#define HBDRUN_SCHEMA_TYPE_VERSION          "version"
#define HBDRUN_SCHEMA_TYPE_APPS             "apps"
#define HBDRUN_SCHEMA_TYPE_STORE            "store"
#define HBDRUN_SCHEMA_TYPE_RUNNERS          "runners"
#define HBDRUN_SCHEMA_TYPE_CONFIRM          "confirm"
#define HBDRUN_SCHEMA_TYPE_ACTION           "action"

typedef void (*hbdrun_handler)(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri);


static const char *not_found_page =
    "<html><body><h1>404 not found : %s</h1></body></html>";

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
}

static void on_hbdrun_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void on_hbdrun_action(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void finish_with_not_found(WebKitURISchemeRequest *request,
        const char *uri)
{
    GInputStream *stream = NULL;
    WebKitURISchemeResponse *response = NULL;;

    gsize content_length = strlen(not_found_page) + strlen(uri);
    gchar contents[content_length + 1];
    sprintf(contents, not_found_page, uri);

    stream = g_memory_input_stream_new_from_data(contents, content_length, NULL);
    response = webkit_uri_scheme_response_new(stream, content_length);

    webkit_uri_scheme_response_set_status(response, 404, NULL);
    webkit_uri_scheme_response_set_content_type(response, "text/html");
    webkit_uri_scheme_request_finish_with_response(request, response);

    g_object_unref(response);
    g_object_unref(stream);
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

    if (!purc_hvml_uri_split(uri,
            host, NULL, NULL, NULL, NULL) ||
            !purc_is_valid_host_name(host)) {
        LOG_WARN("Invalid hbdrun URI (%s): bad host", uri);
        goto error;
    }

    hbdrun_handler handler = find_hbdrun_handler(host);
    if (handler == NOT_FOUND_HANDLER) {
        LOG_WARN("Invalid hbdrun URI (%s): bad host", uri);
        goto error;
    }

    handler(request, webContext, uri);
    return;

error:
    finish_with_not_found(request, uri);
}

