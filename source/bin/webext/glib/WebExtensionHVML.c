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
#include "utils/load-asset.h"

#include "webext/log.h"

struct HVMLInfo {
    const char* vendor;
    int   version;
    char *protocol;
    char *hostName;
    char *appName;
    char *runnerName;
    char *groupName;
    char *pageName;
    char *requestId;

    JSCValue *onrequest;
    JSCValue *onresponse;
    JSCValue *onevent;
};

static JSCValue * hvml_get_property(JSCClass *jsc_class,
        JSCContext *context, gpointer instance, const char *name)
{
    WebKitWebPage *web_page = instance;
    g_assert_true(WEBKIT_IS_WEB_PAGE(web_page));

    struct HVMLInfo *info;
    info = g_object_get_data(G_OBJECT(web_page), "hvml-instance");
    g_assert_true(info != NULL);

    if (g_strcmp0(name, "vendor") == 0) {
        return jsc_value_new_string(context, info->vendor);
    }
    if (g_strcmp0(name, "version") == 0) {
        return jsc_value_new_number(context, (double)info->version);
    }
    else if (g_strcmp0(name, "protocol") == 0) {
        return jsc_value_new_string(context, info->protocol);
    }
    else if (g_strcmp0(name, "hostName") == 0) {
        return jsc_value_new_string(context, info->hostName);
    }
    else if (g_strcmp0(name, "appName") == 0) {
        return jsc_value_new_string(context, info->appName);
    }
    else if (g_strcmp0(name, "runnerName") == 0) {
        return jsc_value_new_string(context, info->runnerName);
    }
    else if (g_strcmp0(name, "groupName") == 0) {
        if (info->groupName)
            return jsc_value_new_string(context, info->groupName);
        return jsc_value_new_undefined(context);
    }
    else if (g_strcmp0(name, "pageName") == 0) {
        return jsc_value_new_string(context, info->pageName);
    }
    else if (g_strcmp0(name, "requestId") == 0) {
        return jsc_value_new_string(context, info->requestId);
    }
    else if (g_strcmp0(name, "onrequest") == 0) {
        if (info->onrequest) {
            return info->onrequest;
        }
        return jsc_value_new_undefined(context);
    }
    else if (g_strcmp0(name, "onresponse") == 0) {
        if (info->onresponse) {
            return info->onresponse;
        }
        return jsc_value_new_undefined(context);
    }
    else if (g_strcmp0(name, "onevent") == 0) {
        if (info->onevent) {
            return info->onevent;
        }
        return jsc_value_new_undefined(context);
    }

    return NULL;
}

static gboolean hvml_set_property(JSCClass *jsc_class,
        JSCContext *context, gpointer instance,
        const char *name, JSCValue *value)
{
    WebKitWebPage *web_page = instance;
    g_assert_true(WEBKIT_IS_WEB_PAGE(web_page));

    struct HVMLInfo *info;
    info = g_object_get_data(G_OBJECT(web_page), "hvml-instance");
    g_assert_true(info != NULL);

    if (g_strcmp0(name, "onrequest") == 0) {
        LOG_DEBUG("set HVML.onrequest with a value (%p)\n", value);
        if (jsc_value_is_function(value)) {
            LOG_DEBUG("value (%p) is a function\n", value);

            if (info->onrequest)
                g_object_unref(info->onrequest);
            g_object_ref(value);
            info->onrequest = value;
            return TRUE;
        }
    }
    else if (g_strcmp0(name, "onresponse") == 0) {
        if (jsc_value_is_function(value)) {
            if (info->onresponse)
                g_object_unref(info->onresponse);
            g_object_ref(value);
            info->onresponse = value;
            return TRUE;
        }
    }
    else if (g_strcmp0(name, "onevent") == 0) {
        if (jsc_value_is_function(value)) {
            if (info->onevent)
                g_object_unref(info->onevent);
            g_object_ref(value);
            info->onevent = value;
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

static void destroyed_notify(WebKitWebPage* web_page)
{
    struct HVMLInfo *info;

    info = g_object_get_data(G_OBJECT(web_page), "hvml-instance");
    if (info) {
        if (info->protocol)
            free(info->protocol);
        if (info->hostName)
            free(info->hostName);
        if (info->appName)
            free(info->appName);
        if (info->runnerName)
            free(info->runnerName);
        if (info->runnerName)
            free(info->runnerName);
        if (info->groupName)
            free(info->groupName);
        if (info->pageName)
            free(info->pageName);
        if (info->requestId)
            free(info->requestId);

        if (info->onrequest)
            g_object_unref(info->onrequest);
        if (info->onresponse)
            g_object_unref(info->onresponse);
        if (info->onevent)
            g_object_unref(info->onevent);

        free(info);
    }

    g_object_unref(web_page);
}

static gboolean on_hvml_post(WebKitWebPage* web_page,
        const char *event, const char* handle, const char *details_in_json)
{
    g_assert_true(WEBKIT_IS_WEB_PAGE(web_page));

    LOG_DEBUG("got an event (%s) from %s: %s\n",
            event, handle, details_in_json);

    const char *params[] = {
        event, handle, details_in_json,
    };

    WebKitUserMessage * message = webkit_user_message_new("event",
            g_variant_new_strv(params, sizeof(params)/sizeof(params[0])));
    webkit_web_page_send_message_to_view(web_page, message,
            NULL, NULL, NULL);

    return TRUE;
}

static struct HVMLInfo *create_hvml_instance(JSCContext *context,
        WebKitWebPage* web_page,
        char *host, char *app, char* runner, char *group, char *page,
        char *request_id)
{
    JSCClass* hvmlClass = jsc_context_register_class(context,
            "HVML", NULL, &hvmlVTable, (GDestroyNotify)destroyed_notify);

    jsc_class_add_method(hvmlClass, "post",
            G_CALLBACK(on_hvml_post), NULL, NULL,
            G_TYPE_BOOLEAN, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    struct HVMLInfo *info = calloc(1, sizeof(struct HVMLInfo));

    info->vendor = HVMLJS_VENDOR_STRING;
    info->version = HVMLJS_VERSION_CODE;
    info->protocol = strdup("PURCMC:100");
    info->hostName = host;
    info->appName = app;
    info->runnerName = runner;
    info->groupName = group;
    info->pageName = page;
    info->requestId = request_id;

    g_object_set_data(G_OBJECT(web_page), "hvml-instance", info);

    JSCValue *HVML;
    HVML = jsc_value_new_object(context, g_object_ref(web_page), hvmlClass);
    jsc_context_set_value(context, "HVML", HVML);
    return info;
}

static void
console_message_sent_callback(WebKitWebPage *web_page,
        WebKitConsoleMessage *console_message, gpointer user_data)
{
    LOG_JSC("%s from source %s, line: %u\n",
            webkit_console_message_get_text(console_message),
            webkit_console_message_get_source_id(console_message),
            webkit_console_message_get_line(console_message));
}

#define PAGE_READY_FORMAT  "{"      \
        "\"requestId\":\"%s\","     \
        "\"state\":\"Ok\"}"

static void
document_loaded_callback(WebKitWebPage *web_page, gpointer user_data)
{
    const char *uri = webkit_web_page_get_uri(web_page);
    LOG_DEBUG("uri: %s\n", uri);
    char request_id[128];
    if (!hvml_uri_get_query_value(uri, "irId", request_id)) {
        LOG_DEBUG("No initial request identifier passed\n");
        return;
    }

    LOG_DEBUG("injecting hvml.js to page (%p)\n", web_page);

    /* inject hvml.js */
    char *code = load_asset_content("WEBKIT_WEBEXT_DIR", WEBKIT_WEBEXT_DIR,
            "assets/hvml.js", NULL);

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

        char *json = jsc_value_to_json(result, 0);
        LOG_INFO("result of injected script: (%s)\n", json);
        if (json)
            free(json);

        json = g_strdup_printf(PAGE_READY_FORMAT, request_id);
        WebKitUserMessage * message = webkit_user_message_new("page-ready",
                g_variant_new_string(json));
        webkit_web_page_send_message_to_view(web_page, message,
                NULL, NULL, NULL);
        free(json);
    }
    else {
        LOG_ERROR("failed to load hvml.js\n");
    }
}

static gboolean
user_message_received_callback(WebKitWebPage *web_page,
        WebKitUserMessage *message, gpointer userData)
{
    struct HVMLInfo *info;
    info = g_object_get_data(G_OBJECT(web_page), "hvml-instance");
    g_assert_true(info != NULL);

    const char* name = webkit_user_message_get_name(message);
    LOG_DEBUG("Got a message with name (%s)\n", name);

    JSCValue *handler = NULL;
    if (strcmp(name, "request") == 0) {
        handler = info->onrequest;
    }
    else if (strcmp(name, "response") == 0) {
        handler = info->onresponse;
    }
    else if (strcmp(name, "event") == 0) {
        handler = info->onevent;
    }
    else {
        LOG_WARN("Unknown message name: %s\n", name);
        return FALSE;
    }

    if (handler == NULL || !jsc_value_is_function(handler)) {
        LOG_WARN("Handler not set for message (%s) or is not a function\n",
                name);
        return FALSE;
    }

    GVariant *param = webkit_user_message_get_parameters(message);
    const char* type = g_variant_get_type_string(param);
    if (strcmp(type, "s")) {
        LOG_ERROR("the parameter of the message is not a string (%s)\n", type);
        return FALSE;
    }

    JSCValue *result = jsc_value_function_call(handler,
            G_TYPE_STRING, g_variant_get_string(param, NULL),
            G_TYPE_NONE);

    char *result_in_json = jsc_value_to_json(result, 0);
    LOG_INFO("result of onrequest: (%s)\n", result_in_json);
    if (result_in_json) {
        webkit_user_message_send_reply(message,
                webkit_user_message_new(name,
                    g_variant_new_string(result_in_json)));
        free(result_in_json);
    }

    return TRUE;
}

static void
window_object_cleared_callback(WebKitScriptWorld* world,
        WebKitWebPage* web_page, WebKitFrame* frame,
        WebKitWebExtension* extension)
{
    const gchar *uri = webkit_web_page_get_uri(web_page);

    char *host = NULL, *app = NULL, *runner = NULL, *group = NULL, *page = NULL;
    char *request_id = NULL;
    if (hvml_uri_split_alloc(uri, &host, &app, &runner, &group, &page) &&
            hvml_uri_get_query_value_alloc(uri, "irId", &request_id)) {
        JSCContext *context;
        context = webkit_frame_get_js_context_for_script_world(frame, world);

        create_hvml_instance(context, web_page,
                host, app, runner, group, page, request_id);

        g_signal_connect(web_page, "document-loaded",
                G_CALLBACK(document_loaded_callback),
                NULL);
        g_signal_connect(web_page, "console-message-sent",
                G_CALLBACK(console_message_sent_callback),
                NULL);
        g_signal_connect(web_page, "user-message-received",
                G_CALLBACK(user_message_received_callback),
                NULL);
    }
    else {
        if (host) free(host);
        if (app) free(app);
        if (runner) free(runner);
        if (group) free(group);
        if (page) free(page);
        if (request_id) free(request_id);

        LOG_WARN("Invalid HVML URI: %s\n", uri);
    }
}

static void
web_page_created_callback(WebKitWebExtension *extension,
        WebKitWebPage *web_page, gpointer user_data)
{
    LOG_DEBUG("Page %llu created for %s\n",
            (unsigned long long)webkit_web_page_get_id(web_page),
            webkit_web_page_get_uri(web_page));
}

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data(WebKitWebExtension *extension,
        GVariant *user_data)
{
    my_log_enable(true, NULL);

    if (user_data == NULL) {
        LOG_INFO("no user data\n");
        goto failed;
    }
    else {
        const char* type = g_variant_get_type_string(user_data);
        if (strcmp(type, "s") == 0) {
            LOG_INFO("got desired user data: %s\n",
                    g_variant_get_string(user_data, NULL));
        }
        else {
            LOG_INFO("not desired user data type: %s\n", type);
            goto failed;
        }
    }

    g_signal_connect(extension, "page-created",
            G_CALLBACK(web_page_created_callback),
            NULL);
    g_signal_connect(webkit_script_world_get_default(),
            "window-object-cleared",
            G_CALLBACK(window_object_cleared_callback),
            extension);

failed:
    return;
}

