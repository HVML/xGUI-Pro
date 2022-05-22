/*
** WebExtensionHVML.c -- The WebKit2 web extension for HVML.
**
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
**
** This file is part of xGUI Pro.
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

#include <webkit2/webkit-web-extension.h>

#include "xguipro-version.h"
#include "xguipro-features.h"

#include "utils/hvml-uri.h"
#include "webext/log.h"

struct HVMLInfo {
    const char* vendor;
    int   version;
    char *protocol;
    char *hostName;
    char *appName;
    char *runnerName;
    JSCValue *onmessage;
};

static JSCValue * hvml_get_property(JSCClass *jsc_class,
        JSCContext *context, gpointer instance, const char *name)
{
    struct HVMLInfo *hvmlInfo = (struct HVMLInfo *)instance;

    if (g_strcmp0(name, "vendor") == 0) {
        return jsc_value_new_string(context, hvmlInfo->vendor);
    }
    if (g_strcmp0(name, "version") == 0) {
        return jsc_value_new_number(context, (double)hvmlInfo->version);
    }
    else if (g_strcmp0(name, "protocol") == 0) {
        return jsc_value_new_string(context, hvmlInfo->protocol);
    }
    else if (g_strcmp0(name, "hostName") == 0) {
        return jsc_value_new_string(context, hvmlInfo->hostName);
    }
    else if (g_strcmp0(name, "appName") == 0) {
        return jsc_value_new_string(context, hvmlInfo->appName);
    }
    else if (g_strcmp0(name, "runnerName") == 0) {
        return jsc_value_new_string(context, hvmlInfo->runnerName);
    }
    else if (g_strcmp0(name, "onmessage") == 0) {
        if (hvmlInfo->onmessage) {
            return hvmlInfo->onmessage;
        }
        return jsc_value_new_undefined(context);
    }

    return NULL;
}

static gboolean hvml_set_property(JSCClass *jsc_class,
        JSCContext *context, gpointer instance,
        const char *name, JSCValue *value)
{
    struct HVMLInfo *hvmlInfo = (struct HVMLInfo *)instance;

    if (g_strcmp0(name, "onmessage") == 0) {
        if (jsc_value_is_function(value)) {
            hvmlInfo->onmessage = value;
            return TRUE;
        }
    }

    return FALSE;
}

static JSCClassVTable hvmlVTable = {
    // get_property
    hvml_get_property,
    // set_property
    hvml_set_property,
    // has_property
    NULL,
    // delete_property
    NULL,
    // enumerate_properties
    NULL,
};

static void destroyed_notify(gpointer instance)
{
    struct HVMLInfo *hvmlInfo = (struct HVMLInfo *)instance;

    if (hvmlInfo->protocol)
        free(hvmlInfo->protocol);
    if (hvmlInfo->hostName)
        free(hvmlInfo->hostName);
    if (hvmlInfo->appName)
        free(hvmlInfo->appName);
    if (hvmlInfo->runnerName)
        free(hvmlInfo->runnerName);

    free(hvmlInfo);
}

static void create_hvml_instance(JSCContext *context,
        gchar *hostName, gchar *appName, gchar* runnerName)
{
    LOG_DEBUG("hostName (%s), appname (%s), runnerName (%s)\n",
             hostName, appName, runnerName);

    JSCClass* hvmlClass = jsc_context_register_class(context,
            "HVML", NULL, &hvmlVTable, destroyed_notify);

    struct HVMLInfo *hvmlInfo = calloc(1, sizeof(struct HVMLInfo));

    hvmlInfo->vendor = HVMLJS_VENDOR_STRING;
    hvmlInfo->version = HVMLJS_VERSION_CODE;
    hvmlInfo->protocol = strdup("PURCMC:100");
    hvmlInfo->hostName = hostName;
    hvmlInfo->appName = appName;
    hvmlInfo->runnerName = runnerName;

    JSCValue *HVML = jsc_value_new_object(context, hvmlInfo, hvmlClass);
    jsc_context_set_value(context, "HVML", HVML);

    LOG_DEBUG("HVML object set\n");
}

static char *load_asset_content(const char *file)
{
    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    char *buf = NULL;

    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    gchar *path = g_strdup_printf("%s/assets/%s", webext_dir, file);
    if (path) {
        LOG_DEBUG("path: %s\n", path);

        FILE *f = fopen(path, "r");
        free(path);

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

failed:
            fclose(f);
        }
        else {
            LOG_ERROR("failed to load asset %s\n", file);
        }
    }

    return buf;
}

static void
console_message_sent_callback(WebKitWebPage *web_page,
        WebKitConsoleMessage *console_message, gpointer user_data)
{
    LOG_INFO("%s (line: %u): %s\n",
            webkit_console_message_get_source_id(console_message),
            webkit_console_message_get_line(console_message),
            webkit_console_message_get_text(console_message));
}

static void
document_loaded_callback(WebKitWebPage *web_page, gpointer user_data)
{
    LOG_DEBUG("user_data (%p)\n", user_data);

    /* inject hvml.js */
    gchar *code = load_asset_content("hvml.js");
    if (code) {
        WebKitFrame *frame;
        frame = webkit_web_page_get_main_frame(web_page);

        JSCContext *context;
        context = webkit_frame_get_js_context_for_script_world(frame,
                webkit_script_world_get_default());

        JSCValue *result;
        result = jsc_context_evaluate_with_source_uri(context, code, -1,
                webkit_web_page_get_uri(web_page), 1);

        free(code);
        char *result_in_json = jsc_value_to_json(result, 0);

        LOG_INFO("result of injected script: (%s)\n", result_in_json);
        free(result_in_json);
    }
}

static void
window_object_cleared_callback(WebKitScriptWorld* world,
        WebKitWebPage* webPage, WebKitFrame* frame,
        WebKitWebExtension* extension)
{
    gchar *hostName, *appName, *runnerName;
    const gchar *uri = webkit_web_page_get_uri(webPage);

    LOG_DEBUG("uri (%s)\n", uri);
    if (hvml_uri_split(uri, &hostName, &appName, &runnerName, NULL, NULL)) {
        JSCContext *context;
        context = webkit_frame_get_js_context_for_script_world(frame, world);

        create_hvml_instance(context, hostName, appName, runnerName);

        g_signal_connect(webPage, "document-loaded",
                G_CALLBACK(document_loaded_callback),
                NULL);
        g_signal_connect(webPage, "console-message-sent",
                G_CALLBACK(console_message_sent_callback),
                NULL);
    }
}

static void
web_page_created_callback(WebKitWebExtension *extension,
        WebKitWebPage *web_page, gpointer user_data)
{
    LOG_DEBUG("Page %llu created for %s\n",
            (unsigned long long)webkit_web_page_get_id(web_page),
            webkit_web_page_get_uri(web_page));

#if 0
    WebKitFrame *frame = webkit_web_page_get_main_frame(web_page);
    JSCContext *context = webkit_frame_get_js_context(frame);

    create_hvml_instance(context, webkit_web_page_get_uri(web_page));
#endif
}

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data(WebKitWebExtension *extension,
        GVariant *user_data)
{
    my_log_enable(true, "WebExtensionHVML");

    if (user_data == NULL) {
        LOG_INFO("no user data\n");
    }
    else {
        const char* type = g_variant_get_type_string(user_data);
        if (strcmp(type, "u") == 0) {
            LOG_INFO("got desired user data: %u\n",
                    g_variant_get_uint32(user_data));
        }
        else {
            LOG_INFO("not desired user data type: %s\n", type);
        }
    }

    g_signal_connect(extension, "page-created",
            G_CALLBACK(web_page_created_callback),
            user_data);
    g_signal_connect(webkit_script_world_get_default(),
            "window-object-cleared",
            G_CALLBACK(window_object_cleared_callback),
            extension);
}

