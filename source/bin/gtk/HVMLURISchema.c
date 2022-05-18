/*
 * Copyright (C) 2022 HVML Community <https://github.com/HVML>
 *
 */

#include "config.h"

#include "main.h"
#include "HVMLURISchema.h"

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
        return;
    }

    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    gchar *contents;
    contents = g_strdup_printf(
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<script src=\"file://%s/hvml.js\"></script>"
            "</head>"
            "<body>"
            "<h1>xGUI Pro for <span id=\"endpoint\"></span></h1>"
            "<p>This content will be replaced by the HVML runner (@%s/%s/%s)</p>"
            "</body>"
            "</html>",
        webext_dir, host, app, runner);

    g_free(basename);
    g_free(dirname);
    g_free(path);
    g_free(host);

    GInputStream *stream;
    gsize streamLength;
    streamLength = strlen(contents);
    stream = g_memory_input_stream_new_from_data(contents, streamLength, g_free);

    webkit_uri_scheme_request_finish(request, stream, streamLength, "text/html");
    g_object_unref(stream);
}

