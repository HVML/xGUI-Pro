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

#include <syslog.h>

#include "xguipro-version.h"

struct HVMLInfo {
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

    if (g_strcmp0(name, "version") == 0) {
        return jsc_value_new_number(context, (double)hvmlInfo->version);
    }
    if (g_strcmp0(name, "protocol") == 0) {
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

    syslog(LOG_INFO, "%s called\n", __func__);

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
    syslog(LOG_INFO, "%s: hostName (%s), appname (%s), runnerName (%s)\n",
            __func__, hostName, appName, runnerName);

    JSCClass* hvmlClass = jsc_context_register_class(context,
            "HVML", NULL, &hvmlVTable, destroyed_notify);

    struct HVMLInfo *hvmlInfo = calloc(1, sizeof(struct HVMLInfo));

    hvmlInfo->version = HVMLJS_VERSION_CODE;
    hvmlInfo->protocol = strdup("PURCMC:100");
    hvmlInfo->hostName = hostName;
    hvmlInfo->appName = appName;
    hvmlInfo->runnerName = runnerName;

    JSCValue *HVML = jsc_value_new_object(context, hvmlInfo, hvmlClass);
    jsc_context_set_value(context, "HVML", HVML);

    syslog(LOG_INFO, "%s: HVML object set\n", __func__);
}

static void
document_loaded_callback(WebKitWebPage *web_page, gpointer user_data)
{
    syslog(LOG_INFO, "%s: user_data (%p)\n", __func__, user_data);

    static const gchar *code = ""
    "var elem = document.getElementByHVMLHandle('77');"
    "elem.textContent = '@' + HVML.hostName + '/' + HVML.appName + '/' + HVML.runnerName;";

    WebKitFrame *frame;
    frame = webkit_web_page_get_main_frame(web_page);

    JSCContext *context;
    context = webkit_frame_get_js_context_for_script_world(frame,
            webkit_script_world_get_default());

    JSCValue *result;
    result = jsc_context_evaluate_with_source_uri(context, code, -1,
            webkit_web_page_get_uri(web_page), 1);
    char *result_in_json = jsc_value_to_json(result, 0);

    syslog(LOG_INFO, "%s: result (%s)\n", __func__, result_in_json);
    free(result_in_json);
}

static bool get_runner_info(const gchar *uri,
        gchar **hostName, gchar **appName, gchar **runnerName)
{
    gchar *schema, *host, *path;

    g_uri_split(uri, G_URI_FLAGS_NONE,
            &schema, NULL, &host, NULL, &path, NULL, NULL, NULL);

    gchar *basename = g_path_get_basename(path);
    gchar *dirname = g_path_get_dirname(path);

    const gchar *runner = basename;
    const gchar *app = dirname + 1;

    if (strcasecmp(schema, "hvml")) {
        free(basename);
        free(dirname);
        free(schema);
        free(host);
        free(path);
        return false;
    }

    *hostName = host;
    *appName = strdup(app);
    *runnerName = strdup(runner);

    free(basename);
    free(dirname);
    free(schema);
    free(path);
    return true;
}

static void
window_object_cleared_callback(WebKitScriptWorld* world,
        WebKitWebPage* webPage, WebKitFrame* frame,
        WebKitWebExtension* extension)
{
    gchar *hostName, *appName, *runnerName;
    const gchar *uri = webkit_web_page_get_uri(webPage);

    syslog(LOG_INFO, "%s: uri (%s)\n", __func__, uri);
    if (get_runner_info(uri, &hostName, &appName, &runnerName)) {
        JSCContext *context;
        context = webkit_frame_get_js_context_for_script_world(frame, world);

        create_hvml_instance(context, hostName, appName, runnerName);

        g_signal_connect(webPage, "document-loaded",
                G_CALLBACK(document_loaded_callback),
                NULL);
    }
}

static void
web_page_created_callback(WebKitWebExtension *extension,
        WebKitWebPage *web_page, gpointer user_data)
{
    syslog(LOG_INFO, "Page %llu created for %s\n",
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
    openlog("WebExtensionHVML", LOG_NDELAY | LOG_PID, LOG_USER);

    if (user_data == NULL) {
        syslog(LOG_INFO, "%s: no user data\n", __func__);
    }
    else {
        const char* type = g_variant_get_type_string(user_data);
        if (strcmp(type, "u") == 0) {
            syslog(LOG_INFO, "%s: got desired user data: %u\n",
                    __func__, g_variant_get_uint32(user_data));
        }
        else {
            syslog(LOG_INFO, "%s: not desired user data type: %s\n", __func__, type);
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

