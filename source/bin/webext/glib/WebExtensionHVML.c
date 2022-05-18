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

struct HVMLInfo {
    char *protocol;
    char *appName;
    char *runnerName;
    JSCValue *onmessage;
    bool wasDeleted;
};

static JSCValue * hvml_get_property(JSCClass *jsc_class,
        JSCContext *context, gpointer instance, const char *name)
{
    struct HVMLInfo *hvmlInfo = (struct HVMLInfo *)instance;

    if (g_strcmp0(name, "protocol") == 0) {
        return jsc_value_new_string(context, hvmlInfo->protocol);
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

static void destroyed_notify(gpointer instance)
{
    struct HVMLInfo *hvmlInfo = (struct HVMLInfo *)instance;
    hvmlInfo->wasDeleted = true;
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

static void
web_page_created_callback(WebKitWebExtension *extension,
        WebKitWebPage *web_page, gpointer user_data)
{
    g_print("Page %llu created for %s\n",
            (unsigned long long)webkit_web_page_get_id(web_page),
            webkit_web_page_get_uri(web_page));

    WebKitFrame *frame = webkit_web_page_get_main_frame(web_page);
    JSCContext *context = webkit_frame_get_js_context(frame);

    JSCClass* hvmlClass = jsc_context_register_class(context,
            "HVML", NULL, &hvmlVTable, destroyed_notify);

    struct HVMLInfo *hvmlInfo = calloc(1, sizeof(struct HVMLInfo));

    /* TODO: get data from user_data */
    hvmlInfo->protocol = strdup("PURCMC:100");
    hvmlInfo->appName = strdup("cn.fmsoft.hvml.test");
    hvmlInfo->runnerName = strdup("test");

    JSCValue *HVML = jsc_value_new_object(context, hvmlInfo, hvmlClass);
    jsc_context_set_value(context, "HVML", HVML);
}

G_MODULE_EXPORT void
webkit_web_extension_initialize_user_data(WebKitWebExtension *extension,
        const GVariant *user_data)
{
    (void)user_data;

    g_signal_connect(extension, "page-created",
            G_CALLBACK (web_page_created_callback),
            (gpointer)user_data);
}

