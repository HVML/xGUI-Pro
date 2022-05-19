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

#include "main.h"
#include "HVMLURISchema.h"
#include "BuildRevision.h"

#include <webkit2/webkit2.h>
#include <purc/purc-helpers.h>

void hvmlURISchemeRequestCallback(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext)
{
    gchar *host, *path;

    g_uri_split(webkit_uri_scheme_request_get_uri(request), G_URI_FLAGS_NONE,
            NULL, NULL,
            &host, NULL, &path, NULL, NULL, NULL);

    gchar *basename = g_path_get_basename(path);
    gchar *dirname = g_path_get_dirname(path);

    const gchar *runner = basename;
    const gchar *app = dirname + 1;

    g_print("%s: host (%s) and path (%s)\n", __func__, host, path);
    if (!purc_is_valid_host_name(host) ||
            !purc_is_valid_app_name(app) ||
            !purc_is_valid_runner_name(runner)) {
        GError *error = g_error_new(XGUI_PRO_ERROR,
                XGUI_PRO_ERROR_INVALID_HVML_URI,
                "Invalid HVML uri: hvml://%s%s", host, path);
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
        goto error;
    }

    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    gchar *contents;
    contents = g_strdup_printf(
            "<!DOCTYPE html>"
            "<html>"
            "<body>"
            "<h1>xGUI Pro - an advanced HVML renderer</h1>"
            "<p>Status: <strong hvml:handle=\"731128\">Checking...</strong>.</p>"
            "<p>This content will be replaced by the HVML runner <span hvml:handle=\"790715\"></span>.</p>"
            "<p><small>WebKit2GTK API Version %s, WebKit Version %d.%d.%d, Build %s</small></p>"
            "</body>"
            "</html>",
            WEBKITGTK_API_VERSION_STRING,
            WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION,
            BUILD_REVISION);

    GInputStream *stream;
    gsize streamLength;
    streamLength = strlen(contents);
    stream = g_memory_input_stream_new_from_data(contents, streamLength, g_free);

    webkit_uri_scheme_request_finish(request, stream, streamLength, "text/html");
    g_object_unref(stream);

error:
    g_free(basename);
    g_free(dirname);
    g_free(path);
    g_free(host);
}

