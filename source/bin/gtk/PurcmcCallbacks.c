/*
** PurcmcCallbacks.c -- The implementation of callbacks for PurCMC.
**
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
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
#include "BrowserWindow.h"
#include "BuildRevision.h"
#include "PurcmcCallbacks.h"
#include "HVMLURISchema.h"
#include "LayouterWidgets.h"

#include "purcmc/purcmc.h"
#include "layouter/layouter.h"

#include <errno.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

/*
 * Use this function to retrieve the endpoint of a session.
 * the endpoint might be deleted earlier than session.
 */
static inline purcmc_endpoint* get_endpoint_by_session(purcmc_session *sess)
{
    char host[PURC_LEN_HOST_NAME + 1];
    char app[PURC_LEN_APP_NAME + 1];
    char runner[PURC_LEN_RUNNER_NAME + 1];

    purc_hvml_uri_split(sess->uri_prefix, host, app, runner, NULL, NULL);

    char endpoint_name[PURC_LEN_ENDPOINT_NAME + 1];
    purc_assemble_endpoint_name(host, app, runner, endpoint_name);

    return purcmc_endpoint_from_name(sess->srv, endpoint_name);
}

bool gtk_pend_response(purcmc_session* sess, const char *operation,
        const char *request_id, void *result_value)
{
    if (kvlist_get(&sess->pending_responses, request_id)) {
        LOG_ERROR("Duplicated requestId (%s) to pend.\n", request_id);
        return false;
    }
    else
        kvlist_set(&sess->pending_responses, request_id, &result_value);

    return true;
}

static void finish_response(purcmc_session* sess, const char *request_id,
        unsigned int ret_code, purc_variant_t ret_data)
{
    void *data;

    if ((data = kvlist_get(&sess->pending_responses, request_id))) {
        void *result_value;
        result_value = *(void **)data;

        purcmc_endpoint* endpoint;
        endpoint = get_endpoint_by_session(sess);
        if (endpoint) {
            pcrdr_msg response = { };
            response.type = PCRDR_MSG_TYPE_RESPONSE;
            response.sourceURI = PURC_VARIANT_INVALID;
            response.requestId =
                purc_variant_make_string_static(request_id, false);
            response.retCode = ret_code;
            response.resultValue = PTR2U64(result_value);
            if (ret_code == PCRDR_SC_OK && ret_data) {
                response.dataType = PCRDR_MSG_DATA_TYPE_JSON;
                response.data = purc_variant_ref(ret_data);
            }
            else {
                response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
            }

            purcmc_endpoint_send_response(sess->srv, endpoint, &response);
        }

        kvlist_delete(&sess->pending_responses, request_id);
    }
}

static int state_string_to_value(const char *state)
{
    if (state) {
        if (strcasecmp(state, "Ok") == 0) {
            return PCRDR_SC_OK;
        }
        else if (strcasecmp(state, "NotFound") == 0) {
            return PCRDR_SC_NOT_FOUND;
        }
        else if (strcasecmp(state, "NotImplemented") == 0) {
            return PCRDR_SC_NOT_IMPLEMENTED;
        }
        else if (strcasecmp(state, "PartialContent") == 0) {
            return PCRDR_SC_PARTIAL_CONTENT;
        }
        else if (strcasecmp(state, "BadRequest") == 0) {
            return PCRDR_SC_BAD_REQUEST;
        }
        else {
            LOG_WARN("Unknown state: %s", state);
        }
    }

    return PCRDR_SC_INTERNAL_SERVER_ERROR;
}

static void handle_response_from_webpage(purcmc_session *sess,
        const char * str, size_t len)
{
    purc_variant_t result;
    result = purc_variant_make_from_json_string(str, len);

    const char *request_id = NULL;
    const char *state = NULL;

    purc_variant_t tmp;
    if ((tmp = purc_variant_object_get_by_ckey(result, "requestId"))) {
        request_id = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(result, "state"))) {
        state = purc_variant_get_string_const(tmp);
    }

    purc_variant_t ret_data;
    ret_data = purc_variant_object_get_by_ckey(result, "data");
    if (request_id) {
        finish_response(sess, request_id,
                state_string_to_value(state), ret_data);
    }
    else {
        LOG_ERROR("No requestId in the user message from webPage.\n");
    }

    purc_variant_unref(result);
}

static gboolean
user_message_received_callback(WebKitWebView *web_view,
        WebKitUserMessage *message, gpointer user_data)
{
    purcmc_session *sess = user_data;

    const char* name = webkit_user_message_get_name(message);
    if (strcmp(name, "page-ready") == 0) {
        GVariant *param = webkit_user_message_get_parameters(message);
        const char* type = g_variant_get_type_string(param);
        if (strcmp(type, "s") == 0) {
            size_t len;
            const char *str = g_variant_get_string(param, &len);
            handle_response_from_webpage(sess, str, len);
        }
        else {
            LOG_ERROR("the parameter of the message is not a string (%s)\n",
                    type);
        }
    }
    else if (strcmp(name, "event") == 0) {
        GVariant *param = webkit_user_message_get_parameters(message);
        const char* type = g_variant_get_type_string(param);
        if (strcmp(type, "as") == 0) {
            size_t len;
            const char **strv = g_variant_get_strv(param, &len);
            purcmc_endpoint* endpoint = get_endpoint_by_session(sess);

            if (len == 4 && endpoint) {
                pcrdr_msg event = { };

                event.type = PCRDR_MSG_TYPE_EVENT;
                event.target = PCRDR_MSG_TARGET_DOM;
                event.targetValue = PTR2U64(web_view);
                event.eventName =
                    purc_variant_make_string(strv[0], false);
                /* TODO: use URI for the sourceURI */
                event.sourceURI = purc_variant_make_string_static(
                        PCRDR_APP_RENDERER, false);
                if (strcasecmp(strv[1], "id") == 0) {
                    event.elementType = PCRDR_MSG_ELEMENT_TYPE_ID;
                    event.elementValue =
                        purc_variant_make_string(strv[2], false);
                }
                else {
                    event.elementType = PCRDR_MSG_ELEMENT_TYPE_HANDLE;
                    event.elementValue =
                        purc_variant_make_string(strv[2], false);
                }
                event.property = PURC_VARIANT_INVALID;

                event.dataType = PCRDR_MSG_DATA_TYPE_JSON;
                event.data =
                    purc_variant_make_from_json_string(strv[3],
                            strlen(strv[3]));
                if (event.data == PURC_VARIANT_INVALID) {
                    LOG_ERROR("bad JSON: %s\n", strv[2]);
                }

                free(strv);
                purcmc_endpoint_post_event(sess->srv, endpoint, &event);
            }
            else {
                LOG_ERROR("wrong parameters of event message (%s)\n", type);
            }
        }
        else {
            LOG_ERROR("the parameter of the event is not an array of string (%s)\n",
                    type);
        }
    }

    return TRUE;
}

purcmc_session *gtk_create_session(purcmc_server *srv, purcmc_endpoint *endpt)
{
    purcmc_session* sess = calloc(1, sizeof(purcmc_session));
    sess->uri_prefix = purc_hvml_uri_assemble_alloc(
            purcmc_endpoint_host_name(endpt),
            purcmc_endpoint_app_name(endpt),
            purcmc_endpoint_runner_name(endpt),
            NULL, NULL);
    if (sess->uri_prefix == NULL) {
        free(sess);
        return NULL;
    }

    sess->all_handles = sorted_array_create(SAFLAG_DEFAULT, 8, NULL, NULL);
    if (sess->all_handles == NULL) {
        free(sess);
        return NULL;
    }

    sess->srv = srv;
    WebKitSettings *webkit_settings = purcmc_rdrsrv_get_user_data(srv);
    WebKitWebsiteDataManager *manager;
    manager = g_object_get_data(G_OBJECT(webkit_settings),
            "default-website-data-manager");

    WebKitWebContext *web_context = g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
            "website-data-manager", manager,
            "process-swap-on-cross-site-navigation-enabled", TRUE,
#if !GTK_CHECK_VERSION(3, 98, 0)
            "use-system-appearance-for-scrollbars", FALSE,
#endif
            NULL);
    g_object_unref(manager);

    g_signal_connect(web_context, "initialize-web-extensions",
            G_CALLBACK(initializeWebExtensionsCallback), (gpointer)"HVML");

#if 0
    if (enableSandbox)
        webkit_web_context_set_sandbox_enabled(web_context, TRUE);

    if (cookiesPolicy) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(web_context);
        GEnumClass *enumClass = g_type_class_ref(WEBKIT_TYPE_COOKIE_ACCEPT_POLICY);
        GEnumValue *enumValue = g_enum_get_value_by_nick(enumClass, cookiesPolicy);
        if (enumValue)
            webkit_cookie_manager_set_accept_policy(cookieManager, enumValue->value);
        g_type_class_unref(enumClass);
    }

    if (cookiesFile && !webkit_web_context_is_ephemeral(web_context)) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(web_context);
        WebKitCookiePersistentStorage storageType = g_str_has_suffix(cookiesFile, ".txt") ? WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT : WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE;
        webkit_cookie_manager_set_persistent_storage(cookieManager, cookiesFile, storageType);
    }

    // Enable the favicon database, by specifying the default directory.
    webkit_web_context_set_favicon_database_directory(web_context, NULL);

    webkit_web_context_register_uri_scheme(web_context, BROWSER_ABOUT_SCHEME, (WebKitURISchemeRequestCallback)aboutURISchemeRequestCallback, web_context, NULL);

    WebKitUserContentManager *userContentManager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(userContentManager, "aboutData");
    g_signal_connect(userContentManager, "script-message-received::aboutData", G_CALLBACK(aboutDataScriptMessageReceivedCallback), web_context);

    WebKitWebsitePolicies *defaultWebsitePolicies = webkit_website_policies_new_with_policies(
        "autoplay", autoplayPolicy,
        NULL);
#endif

    // hvml schema
    webkit_web_context_register_uri_scheme(web_context,
            BROWSER_HVML_SCHEME,
            (WebKitURISchemeRequestCallback)hvmlURISchemeRequestCallback,
            web_context, NULL);

    sess->webkit_settings = webkit_settings;
    sess->web_context = web_context;

    kvlist_init(&sess->pending_responses, NULL);
    kvlist_init(&sess->workspace.ug_wins, NULL);
    sess->workspace.layouter = NULL;
    sess->workspace.sess = sess;
    return sess;
}

#if 0
static WebKitWebView *
do_destroy_plainwin(purcmc_session *sess, purcmc_plainwin *plain_win)
{
    WebKitWebView *web_view = plain_win->web_view;
    LOG_DEBUG("destroy plain window with name (%s)...\n", plain_win->name);

    sorted_array_remove(sess->all_handles, PTR2U64(plain_win));
    sorted_array_remove(sess->all_handles, PTR2U64(web_view));
    kvlist_delete(&sess->workspace.ug_wins, plain_win->name);
    free(plain_win->name);
    free(plain_win->title);
    free(plain_win);

    return web_view;
}
#endif

int gtk_remove_session(purcmc_session *sess)
{
    const char *name;
    void *next, *data;
    purcmc_plainwin *plain_win;

    LOG_DEBUG("removing session (%p)...\n", sess);

    LOG_DEBUG("destroy all ungrouped plain windows...\n");
    kvlist_for_each_safe(&sess->workspace.ug_wins, name, next, data) {
        plain_win = *(purcmc_plainwin **)data;
        webkit_web_view_try_close(plain_win->web_view);
    }

    LOG_DEBUG("destroy kvlist for ungrouped plain windows...\n");
    kvlist_free(&sess->workspace.ug_wins);

    LOG_DEBUG("destroy sorted array for all handles...\n");
    sorted_array_destroy(sess->all_handles);

    LOG_DEBUG("destroy kvlist for pending responses...\n");
    kvlist_free(&sess->pending_responses);

    LOG_DEBUG("free session...\n");
    free(sess);

    LOG_DEBUG("done\n");
    return PCRDR_SC_OK;
}

static gboolean on_webview_close(WebKitWebView *web_view, purcmc_session *sess)
{
    LOG_DEBUG("remove web_view (%p) from session (%p)\n", web_view, sess);

    if (sorted_array_remove(sess->all_handles, PTR2U64(web_view))) {
        purcmc_plainwin *plain_win = g_object_get_data(G_OBJECT(web_view),
                "purcmc-plainwin");

        pcrdr_msg event = { };
        event.type = PCRDR_MSG_TYPE_EVENT;
        event.eventName = purc_variant_make_string_static("close", false);
        /* TODO: user URI for the sourceURI */
        event.sourceURI = purc_variant_make_string_static(PCRDR_APP_RENDERER,
                false);
        event.elementType = PCRDR_MSG_ELEMENT_TYPE_VOID;
        event.elementValue = PURC_VARIANT_INVALID;
        event.property = PURC_VARIANT_INVALID;
        event.dataType = PCRDR_MSG_DATA_TYPE_VOID;

        if (plain_win) {
            purcmc_endpoint *endpoint = get_endpoint_by_session(sess);

            /* endpoint might be deleted already. */
            if (endpoint) {
                /* post close event for the plainwindow */
                LOG_DEBUG("post close event for the plainwindow (%p)\n", plain_win);
                event.target = PCRDR_MSG_TARGET_PLAINWINDOW;
                event.targetValue = PTR2U64(plain_win);
                purcmc_endpoint_post_event(sess->srv, endpoint, &event);
            }

            sorted_array_remove(sess->all_handles, PTR2U64(plain_win));
            kvlist_delete(&sess->workspace.ug_wins, plain_win->name);
            free(plain_win->name);
            free(plain_win->title);
            free(plain_win);
        }
        else {
            /* post close event for the page */
            event.target = PCRDR_MSG_TARGET_PAGE;
            event.targetValue = PTR2U64(web_view);
            purcmc_endpoint_post_event(sess->srv,
                get_endpoint_by_session(sess), &event);
        }
    }

    return FALSE;
}

purcmc_plainwin *gtk_create_plainwin(purcmc_session *sess,
        purcmc_workspace *workspace,
        const char *request_id, const char *gid, const char *name,
        const char *class_name, const char *title, const char *layout_style,
        purc_variant_t widget_style, int *retv)
{
    purcmc_plainwin *plain_win = NULL;

    assert(workspace == NULL);

    if (gid == NULL) {
        /* create a ungrouped plain window */
        LOG_DEBUG("try creating a plain window with name (%s)\n", name);

        if (kvlist_get(&sess->workspace.ug_wins, name)) {
            LOG_WARN("Duplicated ungrouped plain window: %s\n", name);
            *retv = PCRDR_SC_CONFLICT;
            goto done;
        }

        struct ws_widget_style style = { };
        style.flags = WSWS_FLAG_NAME | WSWS_FLAG_TITLE;
        style.name = name;
        style.title = title;
        gtk_convert_style(&style, widget_style);
        plain_win = gtk_create_widget(&sess->workspace,
                WS_WIDGET_TYPE_PLAINWINDOW, NULL, &style);

        if (plain_win == NULL) {
            LOG_ERROR("Failed to create a plain window: %s\n", name);
            *retv = PCRDR_SC_INSUFFICIENT_STORAGE;
            goto done;
        }

        g_signal_connect(plain_win->web_view, "close",
                G_CALLBACK(on_webview_close), sess);
        g_signal_connect(plain_win->web_view, "user-message-received",
                G_CALLBACK(user_message_received_callback),
                sess);

        char uri[strlen(sess->uri_prefix) + strlen(name) +
            strlen(request_id) + 12];
        strcpy(uri, sess->uri_prefix);
        strcat(uri, "-/");  // null group name
        strcat(uri, name);
        strcat(uri, "?irId=");
        strcat(uri, request_id);
        webkit_web_view_load_uri(plain_win->web_view, uri);

        gtk_widget_grab_focus(GTK_WIDGET(plain_win->web_view));
        gtk_widget_show(GTK_WIDGET(plain_win->main_win));

        kvlist_set(&sess->workspace.ug_wins, name, &plain_win);
        sorted_array_add(sess->all_handles, PTR2U64(plain_win),
                INT2PTR(HT_PLAINWIN));
        sorted_array_add(sess->all_handles, PTR2U64(plain_win->web_view),
                INT2PTR(HT_WEBVIEW));

        *retv = 0;  // pend the response
    }
    else if (workspace->layouter == NULL) {
        *retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        /* create a plain window in the specified group */
        plain_win = ws_layouter_add_plain_window(workspace->layouter, gid,
                name, class_name, title, layout_style, widget_style, retv);
        if (plain_win == NULL) {
            LOG_ERROR("Failed to create a plain window: %s\n", name);
            *retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
            goto done;
        }

        retv = 0;
    }

done:
    return plain_win;
}

int gtk_update_plainwin(purcmc_session *sess, purcmc_workspace *workspace,
        purcmc_plainwin *plain_win, const char *property, purc_variant_t value)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(plain_win), &data)) {

        if (workspace->layouter) {
            if (ws_layouter_retrieve_widget(workspace->layouter, plain_win) ==
                    WS_WIDGET_TYPE_PLAINWINDOW) {
                return ws_layouter_update_widget(workspace->layouter,
                        plain_win, property, value);
            }
        }

        return PCRDR_SC_NOT_FOUND;
    }

    if ((uintptr_t)data != HT_PLAINWIN) {
        return PCRDR_SC_BAD_REQUEST;
    }

    if (strcmp(property, "name") == 0) {
        /* Forbid to change name of a plain window */
        return PCRDR_SC_FORBIDDEN;
    }
    else if (strcmp(property, "class") == 0) {
        /* Not acceptable to change class of a plain window */
        return PCRDR_SC_NOT_ACCEPTABLE;
    }
    else if (strcmp(property, "title") == 0) {
        if (plain_win->title)
            free(plain_win->title);
        const char *title = purc_variant_get_string_const(value);
        if (title) {
            plain_win->title = strdup(title);
        }
        else {
            return PCRDR_SC_BAD_REQUEST;
        }
    }
    else if (strcmp(property, "layoutStyle") == 0) {
        /* TODO */
    }
    else if (strcmp(property, "widgetStyle") == 0) {
        /* TODO */
    }

    return PCRDR_SC_OK;
}

int gtk_destroy_plainwin(purcmc_session *sess, purcmc_workspace *workspace,
        purcmc_plainwin *plain_win)
{
    assert(workspace == NULL);

    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(plain_win), &data)) {
        return PCRDR_SC_NOT_FOUND;
    }

    if ((uintptr_t)data != HT_PLAINWIN) {
        return PCRDR_SC_BAD_REQUEST;
    }

    webkit_web_view_try_close(plain_win->web_view);
    return PCRDR_SC_OK;
}

purcmc_page *gtk_get_plainwin_page(purcmc_session *sess,
        purcmc_plainwin *plain_win, int *retv)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(plain_win), &data)) {
        *retv = PCRDR_SC_NOT_FOUND;
        return NULL;
    }

    if ((uintptr_t)data != HT_PLAINWIN) {
        *retv = PCRDR_SC_BAD_REQUEST;
        return NULL;
    }

    *retv = PCRDR_SC_OK;
    return (purcmc_page *)plain_win->web_view;
}

static inline WebKitWebView *validate_page(purcmc_session *sess,
        purcmc_page *page, int *retv)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(page), &data)) {
        *retv = PCRDR_SC_NOT_FOUND;
        return NULL;
    }

    if ((uintptr_t)data != HT_WEBVIEW) {
        *retv = PCRDR_SC_BAD_REQUEST;
        return NULL;
    }

    return (WebKitWebView *)page;
}

static void
request_ready_callback(GObject* obj, GAsyncResult* result, gpointer user_data)
{
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(obj);
    purcmc_session *sess = user_data;

    WebKitUserMessage * message;
    message = webkit_web_view_send_message_to_page_finish(web_view, result, NULL);

    if (message) {
        GVariant *param = webkit_user_message_get_parameters(message);

        const char* type = g_variant_get_type_string(param);
        if (strcmp(type, "s") == 0) {
            size_t len;
            const char *str = g_variant_get_string(param, &len);
            handle_response_from_webpage(sess, str, len);
            LOG_DEBUG("The parameter of message named (%s): %s\n",
                    webkit_user_message_get_name(message),
                    g_variant_get_string(param, NULL));
        }
        else {
            LOG_DEBUG("Not supported parameter type: %s\n", type);
        }
    }
}

#define PAGE_MESSAGE_FORMAT  "{"    \
        "\"operation\":\"%s\","     \
        "\"requestId\":\"%s\","     \
        "\"data\":\"%s\"}"

purcmc_dom *gtk_load_or_write(purcmc_session *sess, purcmc_page *page,
            int op, const char *op_name, const char* request_id,
            const char *content, size_t length, int *retv)
{
    WebKitWebView *web_view = validate_page(sess, page, retv);
    if (web_view == NULL)
        return NULL;

    char *escaped = pcutils_escape_string_for_json(content);
    gchar *json = g_strdup_printf(PAGE_MESSAGE_FORMAT, op_name,
            request_id, escaped ? escaped : "");

    WebKitUserMessage * message = webkit_user_message_new("request",
            g_variant_new_string(json));
    g_free(json);
    free(escaped);

    webkit_web_view_send_message_to_page(web_view, message, NULL,
            request_ready_callback, sess);

    *retv = PCRDR_SC_OK;
    return (purcmc_dom *)web_view;
}

#define DOM_MESSAGE_FORMAT  "{"     \
        "\"operation\":\"%s\","     \
        "\"requestId\":\"%s\","     \
        "\"elementType\":\"%s\","   \
        "\"element\":\"%s\","  \
        "\"property\":\"%s\","  \
        "\"data\":\"%s\"}"

int gtk_update_dom(purcmc_session *sess, purcmc_dom *dom,
            int op, const char *op_name, const char* request_id,
            const char* element_type, const char* element_value,
            const char* property,
            const char *content, size_t length)
{
    int retv = PCRDR_SC_OK;

    WebKitWebView *web_view = validate_page(sess, (purcmc_page *)dom, &retv);
    if (web_view == NULL) {
        LOG_ERROR("Bad DOM pointer: %p.\n", dom);
        return retv;
    }

    if (property) {
        if (strncmp(property, "attr:", 5) == 0) {
            if (!purc_is_valid_token(property + 5, PURC_LEN_PROPERTY_NAME)) {
                LOG_WARN("Bad property: %s.\n", property);
                return PCRDR_SC_BAD_REQUEST;
            }
        }
    }

    char *element_escaped = NULL;
    if (element_value)
        element_escaped = pcutils_escape_string_for_json(element_value);

    char *escaped = NULL;
    if (content)
        escaped = pcutils_escape_string_for_json(content);

    gchar *json = g_strdup_printf(DOM_MESSAGE_FORMAT, op_name, request_id,
            element_type, element_escaped ? element_escaped : "",
            property ? property : "", escaped ? escaped : "");
    if (element_escaped)
        free(element_escaped);
    if (escaped)
        free(escaped);

    WebKitUserMessage * message = webkit_user_message_new("request",
            g_variant_new_string(json));
    g_free(json);

    webkit_web_view_send_message_to_page(web_view, message, NULL,
            request_ready_callback, sess);

    return 0;
}

#define DOM_MESSAGE_FORMAT_CALLMETHOD  "{"      \
        "\"operation\":\"callMethod\","         \
        "\"requestId\":\"%s\","                 \
        "\"elementType\":\"%s\","               \
        "\"element\":\"%s\","                   \
        "\"method\":\"%s\","                    \
        "\"arg\":%s}"

purc_variant_t
gtk_call_method_in_dom(purcmc_session *sess, const char *request_id,
        purcmc_dom *dom, const char* element_type, const char* element_value,
        const char *method, purc_variant_t arg, int* retv)
{
    WebKitWebView *web_view = validate_page(sess, (purcmc_page *)dom, retv);
    if (web_view == NULL) {
        LOG_ERROR("Bad DOM pointer: %p.\n", dom);
        return PURC_VARIANT_INVALID;
    }

    char *element_escaped = NULL;
    if (element_value)
        element_escaped = pcutils_escape_string_for_json(element_value);

    char *method_escaped;
    method_escaped = pcutils_escape_string_for_json(method);

    char *arg_in_json = NULL;
    if (arg) {
        purc_rwstream_t buffer = NULL;
        buffer = purc_rwstream_new_buffer(PCRDR_MIN_PACKET_BUFF_SIZE,
                PCRDR_MAX_INMEM_PAYLOAD_SIZE);

        if (purc_variant_serialize(arg, buffer, 0,
                PCVARIANT_SERIALIZE_OPT_PLAIN, NULL) < 0) {
            *retv = PCRDR_SC_INSUFFICIENT_STORAGE;
            return PURC_VARIANT_INVALID;
        }

        purc_rwstream_write(buffer, "", 1); // the terminating null byte.

        arg_in_json = purc_rwstream_get_mem_buffer_ex(buffer,
                NULL, NULL, true);
        purc_rwstream_destroy(buffer);
    }

    gchar *json = g_strdup_printf(DOM_MESSAGE_FORMAT_CALLMETHOD, request_id,
            element_type, element_escaped ? element_escaped : "",
            method_escaped, arg_in_json ? arg_in_json : "null");
    if (element_escaped)
        free(element_escaped);
    free(method_escaped);
    if (arg_in_json)
        free(arg_in_json);

    WebKitUserMessage * message = webkit_user_message_new("request",
            g_variant_new_string(json));
    g_free(json);

    webkit_web_view_send_message_to_page(web_view, message, NULL,
            request_ready_callback, sess);

    *retv = 0;
    return PURC_VARIANT_INVALID;
}

#define DOM_MESSAGE_FORMAT_GETPROPERTY  "{"     \
        "\"operation\":\"getProperty\","        \
        "\"requestId\":\"%s\","                 \
        "\"elementType\":\"%s\","               \
        "\"element\":\"%s\","                   \
        "\"property\":\"%s\"}"                  \

purc_variant_t
gtk_get_property_in_dom(purcmc_session *sess, const char *request_id,
        purcmc_dom *dom, const char* element_type, const char* element_value,
        const char *property, int *retv)
{
    WebKitWebView *web_view = validate_page(sess, (purcmc_page *)dom, retv);
    if (web_view == NULL) {
        LOG_ERROR("Bad DOM pointer: %p.\n", dom);
        return PURC_VARIANT_INVALID;
    }

    if (!purc_is_valid_token(property, PURC_LEN_PROPERTY_NAME)) {
        *retv = PCRDR_SC_BAD_REQUEST;
        return PURC_VARIANT_INVALID;
    }

    char *element_escaped = NULL;
    if (element_value)
        element_escaped = pcutils_escape_string_for_json(element_value);

    gchar *json = g_strdup_printf(DOM_MESSAGE_FORMAT_GETPROPERTY, request_id,
            element_type ? element_type : "",
            element_escaped ? element_escaped : "", property);
    if (element_escaped)
        free(element_escaped);

    WebKitUserMessage * message = webkit_user_message_new("request",
            g_variant_new_string(json));
    g_free(json);

    webkit_web_view_send_message_to_page(web_view, message, NULL,
            request_ready_callback, sess);

    *retv = 0;
    return PURC_VARIANT_INVALID;
}

#define DOM_MESSAGE_FORMAT_SETPROPERTY  "{"     \
        "\"operation\":\"setProperty\","        \
        "\"requestId\":\"%s\","                 \
        "\"elementType\":\"%s\","               \
        "\"element\":\"%s\","                   \
        "\"property\":\"%s\","                  \
        "\"value\":%s}"

purc_variant_t
gtk_set_property_in_dom(purcmc_session *sess, const char *request_id,
        purcmc_dom *dom, const char* element_type, const char* element_value,
        const char *property, purc_variant_t value, int *retv)
{
    WebKitWebView *web_view = validate_page(sess, (purcmc_page *)dom, retv);
    if (web_view == NULL) {
        LOG_ERROR("Bad DOM pointer: %p.\n", dom);
        return PURC_VARIANT_INVALID;
    }

    if (!purc_is_valid_token(property, PURC_LEN_PROPERTY_NAME)) {
        *retv = PCRDR_SC_BAD_REQUEST;
        return PURC_VARIANT_INVALID;
    }

    char *element_escaped = NULL;
    if (element_value)
        element_escaped = pcutils_escape_string_for_json(element_value);

    char *value_in_json = NULL;
    if (value) {
        purc_rwstream_t buffer = NULL;
        buffer = purc_rwstream_new_buffer(PCRDR_MIN_PACKET_BUFF_SIZE,
                PCRDR_MAX_INMEM_PAYLOAD_SIZE);

        if (purc_variant_serialize(value, buffer, 0,
                PCVARIANT_SERIALIZE_OPT_PLAIN, NULL) < 0) {
            *retv = PCRDR_SC_INSUFFICIENT_STORAGE;
            return PURC_VARIANT_INVALID;
        }

        purc_rwstream_write(buffer, "", 1); // the terminating null byte.

        value_in_json = purc_rwstream_get_mem_buffer_ex(buffer,
                NULL, NULL, true);
        purc_rwstream_destroy(buffer);
    }

    gchar *json = g_strdup_printf(DOM_MESSAGE_FORMAT_SETPROPERTY, request_id,
            element_type ? element_type : "",
            element_escaped ? element_escaped : "", property,
            value_in_json ? value_in_json : "null");
    if (element_escaped)
        free(element_escaped);
    if (value_in_json)
        free(value_in_json);

    WebKitUserMessage * message = webkit_user_message_new("request",
            g_variant_new_string(json));
    g_free(json);

    webkit_web_view_send_message_to_page(web_view, message, NULL,
            request_ready_callback, sess);

    *retv = 0;
    return PURC_VARIANT_INVALID;
}

int gtk_set_page_groups(purcmc_session *sess, purcmc_workspace *workspace,
        const char *content, size_t length)
{
    int retv;

    if (workspace->layouter == NULL) {
        struct ws_metrics metrics = { 1024, 768, 120, 10 }; /* TODO */

        workspace->layouter = ws_layouter_new(&metrics, content, length,
                workspace, gtk_convert_style, gtk_create_widget,
                gtk_destroy_widget, gtk_update_widget, &retv);
        if (workspace->layouter == NULL)
            return retv;
    }
    else {
        /* TODO: */
        retv = PCRDR_SC_NOT_IMPLEMENTED;
    }

    return retv;
}

int gtk_add_page_groups(purcmc_session *sess, purcmc_workspace *workspace,
        const char *content, size_t length)
{
    int retv;

    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        retv = ws_layouter_add_page_groups(workspace->layouter,
                    content, length);
    }

    return retv;
}

int gtk_remove_page_group(purcmc_session *sess, purcmc_workspace *workspace,
        const char* gid)
{
    int retv;

    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        retv = ws_layouter_remove_page_group(workspace->layouter, gid);
    }

    return retv;
}

purcmc_page *gtk_create_page(purcmc_session *sess, purcmc_workspace *workspace,
            const char *request_id, const char *gid, const char *name,
            const char *class_name, const char *title, const char *layout_style,
            purc_variant_t widget_style, int *retv)
{
    purcmc_page *page = NULL;

    if (workspace->layouter == NULL) {
        *retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        page = ws_layouter_add_page(workspace->layouter,
                    gid, name, class_name, title,
                    layout_style, widget_style, retv);
    }

    return page;
}

int gtk_update_page(purcmc_session *sess, purcmc_workspace *workspace,
            purcmc_page *page, const char *property, purc_variant_t value)
{
    int retv;

    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        ws_widget_type_t type =
            ws_layouter_retrieve_widget(workspace->layouter, page);
        if (type == WS_WIDGET_TYPE_PLAINPAGE ||
                type == WS_WIDGET_TYPE_TABPAGE) {
            retv = ws_layouter_update_widget(workspace->layouter, page,
                    property, value);
        }
        else {
            retv = PCRDR_SC_BAD_REQUEST;
        }
    }

    return retv;
}

int gtk_destroy_page(purcmc_session *sess, purcmc_workspace *workspace,
            purcmc_page *page)
{
    int retv;

    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        ws_widget_type_t type =
            ws_layouter_retrieve_widget(workspace->layouter, page);
        if (type == WS_WIDGET_TYPE_PLAINPAGE ||
                type == WS_WIDGET_TYPE_TABPAGE) {
            if (ws_layouter_remove_page_by_widget(workspace->layouter, page))
                retv = PCRDR_SC_OK;
            else
                retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        }
        else {
            retv = PCRDR_SC_BAD_REQUEST;
        }
    }

    return retv;
}

