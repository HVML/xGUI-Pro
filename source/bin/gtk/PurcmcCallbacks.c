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

#undef NDEBUG

#include "config.h"
#include "main.h"
#include "BrowserPane.h"
#include "BrowserPlainWindow.h"
#include "BuildRevision.h"
#include "PurcmcCallbacks.h"
#include "schema/HVMLURISchema.h"
#include "LayouterWidgets.h"

#include "purcmc/purcmc.h"
#include "layouter/layouter.h"
#include "utils/utils.h"

#include <errno.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>
#include <webkit2/webkit2.h>

static KVLIST(kv_app_workspace, NULL);

int pcmc_gtk_prepare(purcmc_server *srv)
{
    return 0;
}

void pcmc_gtk_cleanup(purcmc_server *srv)
{
    const char *name;
    void *next, *data;

    kvlist_for_each_safe(&kv_app_workspace, name, next, data) {
        purcmc_workspace *workspace = *(purcmc_workspace **)data;

        pcutils_kvlist_delete(workspace->page_owners);
        if (workspace->layouter) {
            ws_layouter_delete(workspace->layouter, NULL);
        }
    }

    kvlist_free(&kv_app_workspace);
}

static purcmc_workspace *create_or_get_workspace(purcmc_endpoint* endpoint)
{
    char app_key[PURC_LEN_ENDPOINT_NAME + 1];

    const char *host = purcmc_endpoint_host_name(endpoint);
    const char *app = purcmc_endpoint_app_name(endpoint);

    sprintf(app_key, "%s-%s", host, app);

    void *data;
    purcmc_workspace *workspace = NULL;
    if ((data = kvlist_get(&kv_app_workspace, app_key))) {
        workspace = *(purcmc_workspace **)data;
        assert(workspace);
    }
    else {
        workspace = calloc(1, sizeof(purcmc_workspace));
        if (workspace) {
            workspace->page_owners = pcutils_kvlist_new(NULL);
            if (workspace->page_owners == NULL)
                goto failed;

            workspace->layouter = NULL;
            kvlist_set(&kv_app_workspace, app_key, &workspace);
        }
    }

    return workspace;

failed:
    if (workspace)
        free(workspace);
    return NULL;
}

/*
 * Use this function to retrieve the endpoint of a session.
 * the endpoint might be deleted earlier than session.
 */
purcmc_endpoint* purcmc_get_endpoint_by_session(purcmc_session *sess)
{
    char host[PURC_LEN_HOST_NAME + 1];
    char app[PURC_LEN_APP_NAME + 1];
    char runner[PURC_LEN_RUNNER_NAME + 1];

    purc_hvml_uri_split(sess->uri_prefix, host, app, runner, NULL, NULL);

    char endpoint_name[PURC_LEN_ENDPOINT_NAME + 1];
    purc_assemble_endpoint_name(host, app, runner, endpoint_name);

    return purcmc_endpoint_from_name(sess->srv, endpoint_name);
}

struct packed_result {
    void *result_value;
    size_t plain_len;
    char plain[0];
};

bool gtk_pend_response(purcmc_session* sess, purcmc_page *page,
        const char *operation, const char *request_id, void *result_value,
        const char *plain)
{
    if (strcmp(request_id, PCRDR_REQUESTID_NORETURN) == 0) {
        LOG_WARN("Trying to pend a noreturn request\n");
        return false;
    }

    if (kvlist_get(&sess->pending_responses, request_id)) {
        LOG_ERROR("Duplicated requestId (%s) to pend.\n", request_id);
        return false;
    }
    else {
        struct packed_result *packed;
        size_t plain_len = plain ? strlen(plain) + 1 : 0;
        size_t sz = sizeof(*packed) + plain_len;
        packed = malloc(sz);
        packed->result_value = result_value;
        packed->plain_len = plain_len;
        if (plain_len > 0) {
            memcpy(packed->plain, plain, plain_len);
        }

        kvlist_set(&sess->pending_responses, request_id, &packed);
    }

    return true;
}

static void finish_response(purcmc_session* sess, const char *request_id,
        unsigned int ret_code, purc_variant_t ret_data)
{
    void *data;

    if ((data = kvlist_get(&sess->pending_responses, request_id))) {
        struct packed_result *packed;
        packed = *(struct packed_result **)data;

        purcmc_endpoint* endpoint;
        endpoint = purcmc_get_endpoint_by_session(sess);
        if (endpoint) {
            pcrdr_msg response = { };
            response.type = PCRDR_MSG_TYPE_RESPONSE;
            response.sourceURI = PURC_VARIANT_INVALID;
            response.requestId =
                purc_variant_make_string_static(request_id, false);
            response.retCode = ret_code;
            response.resultValue = PTR2U64(packed->result_value);
            if (ret_code == PCRDR_SC_OK && ret_data) {
                response.dataType = PCRDR_MSG_DATA_TYPE_JSON;
                response.data = purc_variant_ref(ret_data);
            }
            else if (packed->plain_len > 0) {
                response.dataType = PCRDR_MSG_DATA_TYPE_PLAIN;
                response.data = purc_variant_make_string_static(packed->plain,
                        false);
            }
            else {
                response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
            }

            purcmc_endpoint_send_response(sess->srv, endpoint, &response);
        }

        free(packed);
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
    if (request_id && strcmp(request_id, PCRDR_REQUESTID_NORETURN)) {
        finish_response(sess, request_id,
                state_string_to_value(state), ret_data);
    }
    else {
        LOG_DEBUG("No normal requestId in the user message from webPage.\n");
    }

    purc_variant_unref(result);
}

static gboolean
user_message_received_callback(WebKitWebView *webview,
        WebKitUserMessage *message, gpointer user_data)
{
    purcmc_session *sess = user_data;

    const char* name = webkit_user_message_get_name(message);
    LOG_INFO("get message: %s\n", name);
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
            if (strcmp(strv[0], "page-load-begin") == 0) {
                // TODO
                goto out;
            }
            else if (strcmp(strv[0], "page-loaded") == 0) {
                // TODO
                goto out;
            }

            purcmc_endpoint* endpoint = purcmc_get_endpoint_by_session(sess);

            if (len == 4 && endpoint) {
                pcrdr_msg event = { };

                event.type = PCRDR_MSG_TYPE_EVENT;
                event.target = PCRDR_MSG_TARGET_DOM;
                event.targetValue = PTR2U64(webview);
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

out:
    return TRUE;
}

purcmc_session *gtk_create_session(purcmc_server *srv, purcmc_endpoint *endpt)
{
    purcmc_session* sess = calloc(1, sizeof(purcmc_session));

    sess->workspace = create_or_get_workspace(endpt);
    if (sess->workspace == NULL) {
        goto failed;
    }

    sess->uri_prefix = purc_hvml_uri_assemble_alloc(
            purcmc_endpoint_host_name(endpt),
            purcmc_endpoint_app_name(endpt),
            purcmc_endpoint_runner_name(endpt),
            NULL, NULL);
    if (sess->uri_prefix == NULL) {
        goto failed;
    }

    sess->all_handles = sorted_array_create(SAFLAG_DEFAULT, 8, NULL, NULL);
    if (sess->all_handles == NULL) {
        goto failed;
    }

    sess->srv = srv;
    WebKitSettings *webkit_settings = purcmc_rdrsrv_get_user_data(srv);
    WebKitWebsiteDataManager *manager;
    manager = g_object_get_data(G_OBJECT(webkit_settings),
            "default-website-data-manager");

    WebKitWebContext *web_context = g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
            "website-data-manager", manager,
            "process-swap-on-cross-site-navigation-enabled", TRUE,
#if !GTK_CHECK_VERSION(3, 98, 0) && WEBKIT_CHECK_VERSION(2, 30, 0)
            "use-system-appearance-for-scrollbars", FALSE,
#endif
            NULL);
    g_object_unref(manager);

    g_signal_connect(web_context, "initialize-web-extensions",
            G_CALLBACK(initializeWebExtensionsCallback), (gpointer)"HVML");

    // hvml schema
    webkit_web_context_register_uri_scheme(web_context,
            BROWSER_HVML_SCHEME,
            (WebKitURISchemeRequestCallback)hvmlURISchemeRequestCallback,
            web_context, NULL);

    sess->webkit_settings = webkit_settings;
    sess->web_context = web_context;
    sess->allow_switching_rdr = purcmc_endpoint_allow_switching_rdr(endpt);

    kvlist_init(&sess->pending_responses, NULL);
    return sess;

failed:
    if (sess->uri_prefix) {
        free(sess->uri_prefix);
    }

    if (sess->all_handles)
        sorted_array_destroy(sess->all_handles);

    free(sess);
    return NULL;
}

static int on_each_ostack(void *ctxt, const char *name, void *data)
{
    (void)name;

    purcmc_session *sess = ctxt;
    purc_page_ostack_t ostack = *(purc_page_ostack_t *)data;

    struct purc_page_owner to_reload;
    to_reload = purc_page_ostack_revoke_session(ostack, ctxt);
    if (to_reload.corh) {
        assert(to_reload.sess);
        // TODO: send reloadPage request to another endpoint
    }

    WebKitWebView *webview = purc_page_ostack_get_page(ostack);
    if (sorted_array_find(sess->all_handles, PTR2U64(webview), &data)) {
        assert((uintptr_t)data == HT_WEBVIEW);
        webkit_web_view_try_close(webview);
    }

    return 0;
}

int gtk_remove_session(purcmc_session *sess)
{
    LOG_DEBUG("removing session (%p)...\n", sess);

    LOG_DEBUG("destroy all windows/widgets created by this session...\n");
    pcutils_kvlist_for_each_safe(sess->workspace->page_owners, sess,
            on_each_ostack);

    LOG_DEBUG("deleting layouter ...\n");
    if (sess->workspace->layouter) {
        ws_layouter_delete(sess->workspace->layouter, sess);
        sess->workspace->layouter = NULL;
    }

    LOG_DEBUG("destroy sorted array for all handles...\n");
    sorted_array_destroy(sess->all_handles);

    LOG_DEBUG("destroy kvlist for pending responses...\n");
    const char *name;
    void *data;
    kvlist_for_each(&sess->pending_responses, name, data) {
        struct packed_result *packed;
        packed = *(struct packed_result **)data;
        free(packed);
    }
    kvlist_free(&sess->pending_responses);

    LOG_DEBUG("free session...\n");
    free(sess);

    LOG_DEBUG("done\n");
    return PCRDR_SC_OK;
}

static gboolean on_webview_close(WebKitWebView *webview, purcmc_session *sess)
{
    LOG_INFO("remove webview (%p) from session (%p)\n", webview, sess);

    if (sorted_array_remove(sess->all_handles, PTR2U64(webview))) {
        GtkWidget *container = g_object_get_data(G_OBJECT(webview),
                "purcmc-container");

        sorted_array_remove(sess->all_handles, PTR2U64(container));

        pcrdr_msg event = { };
        event.type = PCRDR_MSG_TYPE_EVENT;
        event.eventName = purc_variant_make_string_static("destroy", false);
        /* TODO: use real URI for the sourceURI */
        event.sourceURI = purc_variant_make_string_static(PCRDR_APP_RENDERER,
                false);
        event.elementType = PCRDR_MSG_ELEMENT_TYPE_VOID;
        event.elementValue = PURC_VARIANT_INVALID;
        event.property = PURC_VARIANT_INVALID;
        event.dataType = PCRDR_MSG_DATA_TYPE_VOID;

        purcmc_endpoint *endpoint = purcmc_get_endpoint_by_session(sess);
        if (BROWSER_IS_PLAIN_WINDOW(container)) {

            /* endpoint might be deleted already. */
            if (endpoint) {
                /* post `destroy` event for the plainwindow */
                event.target = PCRDR_MSG_TARGET_PLAINWINDOW;
                event.targetValue = PTR2U64(container);
                purcmc_endpoint_post_event(sess->srv, endpoint, &event);
            }

            purc_page_ostack_t ostack = g_object_get_data(G_OBJECT(webview),
                "purcmc-owner-stack");
            purc_page_ostack_delete(sess->workspace->page_owners, ostack);
            /* Not necessary to call this explicitly.
            gtk_imp_destroy_widget(&sess->workspace, plainwin,
                    WS_WIDGET_TYPE_PLAINWINDOW); */
        }
        else {
            /* endpoint might be deleted already. */
            if (endpoint) {
                /* post `destroy` event for the page */
                event.target = PCRDR_MSG_TARGET_WIDGET;
                event.targetValue = PTR2U64(container);
                purcmc_endpoint_post_event(sess->srv, endpoint, &event);
            }
        }
    }

    return FALSE;
}

static WebKitWebView *create_web_view(purcmc_session *sess)
{
#if WEBKIT_CHECK_VERSION(2, 30, 0)
    WebKitWebsitePolicies *website_policies;
    website_policies = g_object_get_data(G_OBJECT(sess->webkit_settings),
            "default-website-policies");
#endif

    WebKitUserContentManager *uc_manager;
    uc_manager = g_object_get_data(G_OBJECT(sess->webkit_settings),
            "default-user-content-manager");

    WebKitWebView *webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "web-context", sess->web_context,
                "settings", sess->webkit_settings,
                "user-content-manager", uc_manager,
                "is-controlled-by-automation", FALSE,
#if WEBKIT_CHECK_VERSION(2, 30, 0)
                "website-policies", website_policies,
#endif
                NULL));
    xgutils_webview_init_intrinsic_device_scale_factor(webView);
    return webView;
}

static void web_view_load_uri(WebKitWebView *webview,
        purcmc_session *sess, const char *group, const char *name,
        const char *request_id)
{
    g_signal_connect(webview, "close",
            G_CALLBACK(on_webview_close), sess);
    g_signal_connect(webview, "user-message-received",
            G_CALLBACK(user_message_received_callback),
            sess);

    char uri[strlen(sess->uri_prefix) + (group ? strlen(group) : 0) +
        strlen(name) + strlen(request_id) + 12];
    strcpy(uri, sess->uri_prefix);
    if (group) {
        strcat(uri, group);
        strcat(uri, "/");
    }
    else {
        strcat(uri, "-/");
    }
    strcat(uri, name);
    strcat(uri, "?irId=");
    strcat(uri, request_id);

    g_object_set_data(G_OBJECT(webview), "purcmc-session", sess);
    webkit_web_view_load_uri(webview, uri);
}

struct find_first_page {
    const char *prefix;
    const char *group;
    void *found;
    struct timespec min;
};

static int cb_find_first_page(void *ctxt, const char *id, void *data)
{
    struct find_first_page *info = ctxt;

    if (strncmp(info->prefix, id, strlen(info->prefix)))
        goto done;

    const char *group = strchr(id, PURC_SEP_GROUP_NAME);
    if (info->group && group) {
        group = group + 1;
        if (strcmp(group, info->group))
            goto done;
    }
    else if (info->group && group == NULL) {
        goto done;
    }
    else if (info->group == NULL && group) {
        goto done;
    }

    purc_page_ostack_t ostack = *(purc_page_ostack_t *)data;
    struct timespec birth = purc_page_ostack_get_birth(ostack);
    if (info->min.tv_sec > birth.tv_sec) {
        WebKitWebView *webview = purc_page_ostack_get_page(ostack);
        GtkWidget *widget = g_object_get_data(G_OBJECT(webview),
                "purcmc-container");
        info->found = widget;
        info->min = birth;
    }
    else if (info->min.tv_sec == birth.tv_sec &&
            info->min.tv_nsec > birth.tv_nsec) {
        WebKitWebView *webview = purc_page_ostack_get_page(ostack);
        GtkWidget *widget = g_object_get_data(G_OBJECT(webview),
                "purcmc-container");
        info->found = widget;
        info->min = birth;
    }

done:
    return 0;
}

struct find_last_page {
    const char *prefix;
    const char *group;
    void *found;
    struct timespec max;
};

static int cb_find_last_page(void *ctxt, const char *id, void *data)
{
    struct find_last_page *info = ctxt;

    if (strncmp(info->prefix, id, strlen(info->prefix)))
        goto done;

    const char *group = strchr(id, PURC_SEP_GROUP_NAME);
    if (info->group && group) {
        group = group + 1;
        if (strcmp(group, info->group))
            goto done;
    }
    else if (info->group && group == NULL) {
        goto done;
    }
    else if (info->group == NULL && group) {
        goto done;
    }

    purc_page_ostack_t ostack = *(purc_page_ostack_t *)data;
    struct timespec birth = purc_page_ostack_get_birth(ostack);
    if (info->max.tv_sec < birth.tv_sec) {
        WebKitWebView *webview = purc_page_ostack_get_page(ostack);
        GtkWidget *widget = g_object_get_data(G_OBJECT(webview),
                "purcmc-container");
        info->found = widget;
        info->max = birth;
    }
    else if (info->max.tv_sec == birth.tv_sec &&
            info->max.tv_nsec < birth.tv_nsec) {
        WebKitWebView *webview = purc_page_ostack_get_page(ostack);
        GtkWidget *widget = g_object_get_data(G_OBJECT(webview),
                "purcmc-container");
        info->found = widget;
        info->max = birth;
    }

done:
    return 0;
}

struct find_active_page {
    const char *prefix;
    const char *group;
    void *found;
};

static int cb_find_active_page(void *ctxt, const char *id, void *data)
{
    struct find_active_page *info = ctxt;

    if (strncmp(info->prefix, id, strlen(info->prefix)))
        goto done;

    const char *group = strchr(id, PURC_SEP_GROUP_NAME);
    if (info->group && group) {
        group = group + 1;
        if (strcmp(group, info->group))
            goto done;
    }
    else if (info->group && group == NULL) {
        goto done;
    }
    else if (info->group == NULL && group) {
        goto done;
    }

    purc_page_ostack_t ostack = *(purc_page_ostack_t *)data;
    WebKitWebView *webview = purc_page_ostack_get_page(ostack);
    GtkWidget *widget = g_object_get_data(G_OBJECT(webview),
            "purcmc-container");
    if (gtk_widget_has_focus(widget)) {
        info->found = widget;
        return -1;  /* stop the travel */
    }

done:
    return 0;
}

purcmc_page *gtk_get_special_plainwin(purcmc_session *sess,
        purcmc_workspace *workspace, const char *group,
        pcrdr_resname_page_k page_type)
{
    purcmc_page *page = NULL;

    workspace = sess->workspace;
    switch(page_type) {
    case PCRDR_K_RESNAME_PAGE_first: {
        struct find_first_page ctxt = { };
        ctxt.prefix = PURC_PREFIX_PLAINWIN;
        ctxt.group = group;
        ctxt.found = NULL;
        clock_gettime(CLOCK_MONOTONIC, &ctxt.min);
        pcutils_kvlist_for_each(workspace->page_owners, &ctxt,
                cb_find_first_page);
        page = ctxt.found;
        break;
    }

    case PCRDR_K_RESNAME_PAGE_last: {
        struct find_last_page ctxt = { };
        ctxt.prefix = PURC_PREFIX_PLAINWIN;
        ctxt.group = group;
        ctxt.found = NULL;
        ctxt.max.tv_sec = 0;
        ctxt.max.tv_nsec = 0;
        pcutils_kvlist_for_each(workspace->page_owners, &ctxt,
                cb_find_last_page);
        page = ctxt.found;
        break;
    }

    case PCRDR_K_RESNAME_PAGE_active: {
        struct find_active_page ctxt = { };
        ctxt.prefix = PURC_PREFIX_PLAINWIN;
        ctxt.group = group;
        ctxt.found = NULL;
        pcutils_kvlist_for_each(workspace->page_owners, &ctxt,
                cb_find_active_page);
        page = ctxt.found;
        break;
    }

    }

    return page;
}

purcmc_page *gtk_get_special_widget(purcmc_session *sess,
        purcmc_workspace *workspace, const char *group,
        pcrdr_resname_page_k page_type)
{
    purcmc_page *page = NULL;

    workspace = sess->workspace;
    switch(page_type) {
    case PCRDR_K_RESNAME_PAGE_first: {
        struct find_first_page ctxt = { };
        ctxt.prefix = PURC_PREFIX_WIDGET;
        ctxt.group = group;
        ctxt.found = NULL;
        clock_gettime(CLOCK_MONOTONIC, &ctxt.min);
        pcutils_kvlist_for_each(workspace->page_owners, &ctxt,
                cb_find_first_page);
        page = ctxt.found;
        break;
    }

    case PCRDR_K_RESNAME_PAGE_last: {
        struct find_last_page ctxt = { };
        ctxt.prefix = PURC_PREFIX_WIDGET;
        ctxt.group = group;
        ctxt.found = NULL;
        ctxt.max.tv_sec = 0;
        ctxt.max.tv_nsec = 0;
        pcutils_kvlist_for_each(workspace->page_owners, &ctxt,
                cb_find_last_page);
        page = ctxt.found;
        break;
    }

    case PCRDR_K_RESNAME_PAGE_active: {
        struct find_active_page ctxt = { };
        ctxt.prefix = PURC_PREFIX_WIDGET;
        ctxt.group = group;
        ctxt.found = NULL;
        pcutils_kvlist_for_each(workspace->page_owners, &ctxt,
                cb_find_active_page);
        page = ctxt.found;
        break;
    }

    }

    return page;
}

purcmc_page *gtk_find_page(purcmc_session *sess,
        purcmc_workspace *workspace, const char *page_id)
{
    workspace = sess->workspace;

    void *data;
    data = pcutils_kvlist_get(workspace->page_owners, page_id);
    if (data != NULL) {
        purc_page_ostack_t ostack = *(purc_page_ostack_t *)data;
        WebKitWebView *webview = purc_page_ostack_get_page(ostack);
        void *page = g_object_get_data(G_OBJECT(webview), "purcmc-container");
        return page;
    }

    return NULL;
}

#if 0
static void *get_widget_from_udom(purcmc_session *sess, purcmc_udom *udom,
        int *retv)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(udom), &data)) {
        *retv = PCRDR_SC_NOT_FOUND;
        return NULL;
    }

    if ((uintptr_t)data != HT_WEBVIEW) {
        *retv = PCRDR_SC_BAD_REQUEST;
        return NULL;
    }

    *retv = PCRDR_SC_OK;
    return g_object_get_data(G_OBJECT(udom), "purcmc-container");
}
#endif

static inline WebKitWebView *validate_handle(purcmc_session *sess,
        purcmc_page *page, int *retv)
{
    void *data;
    if (!sorted_array_find(sess->all_handles, PTR2U64(page), &data)) {
        *retv = PCRDR_SC_NOT_FOUND;
        return NULL;
    }

    if ((uintptr_t)data == HT_PLAINWIN) {
        BrowserPlainWindow *plainwin = BROWSER_PLAIN_WINDOW(page);
        return browser_plain_window_get_view(plainwin);
    }
    else if ((uintptr_t)data == HT_PANE_TAB) {
        BrowserPane *pane = BROWSER_PANE(page);
        return browser_pane_get_web_view(pane);
    }
    else if ((uintptr_t)data == HT_WEBVIEW) {
        return (WebKitWebView *)page;
    }
    else {
        *retv = PCRDR_SC_BAD_REQUEST;
        return NULL;
    }

    return (WebKitWebView *)page;
}

purcmc_page *gtk_create_plainwin(purcmc_session *sess,
        purcmc_workspace *workspace, const char *request_id,
        const char *page_id, const char *group, const char *name,
        const char *klass, const char *title, const char *layout_style,
        const char *transition_style, purc_variant_t toolkit_style, int *retv)
{
    (void) transition_style;
    void *plainwin = NULL;

    workspace = sess->workspace;
    if (pcutils_kvlist_get(workspace->page_owners, page_id)) {
        LOG_WARN("Duplicated page identifier: %s\n", page_id);
        *retv = PCRDR_SC_CONFLICT;
        goto done;
    }

    WebKitWebView *webview = create_web_view(sess);

    if (group == NULL) {
        /* create a ungrouped plain window */
        LOG_DEBUG("creating an ungrouped plain window with name (%s)\n", name);

        struct ws_widget_info style = { };
        style.flags = WSWS_FLAG_NAME | WSWS_FLAG_TITLE;
        style.name = name;
        style.title = title;
        gtk_imp_convert_style(&style, toolkit_style);

        if (layout_style) {
            gtk_imp_evaluate_geometry(&style, layout_style);
        }

        if (transition_style) {
            gtk_imp_evaluate_transition(&style, transition_style);
        }

        plainwin = gtk_imp_create_widget(workspace, sess,
                WS_WIDGET_TYPE_PLAINWINDOW, NULL, NULL, webview, &style);

    }
    else if (workspace->layouter == NULL) {
        *retv = PCRDR_SC_PRECONDITION_FAILED;
        goto done;
    }
    else {
        LOG_DEBUG("creating a grouped plain window with name (%s/%s)\n",
                group, name);

        /* create a plain window in the specified group */
        plainwin = ws_layouter_add_plain_window(workspace->layouter, sess,
                group, name, klass, title, layout_style,
                toolkit_style, webview, retv);
    }

    if (plainwin) {
        purc_page_ostack_t ostack =
            purc_page_ostack_new(workspace->page_owners, page_id, webview);
        g_object_set_data(G_OBJECT(webview), "purcmc-owner-stack", ostack);

        web_view_load_uri(webview, sess, group, name, request_id);

        gtk_widget_grab_focus(GTK_WIDGET(webview));
        gtk_widget_show(GTK_WIDGET(plainwin));

        sorted_array_add(sess->all_handles, PTR2U64(plainwin),
                INT2PTR(HT_PLAINWIN));
        sorted_array_add(sess->all_handles, PTR2U64(webview),
                INT2PTR(HT_WEBVIEW));
        *retv = 0;
    }
    else {
        LOG_ERROR("Failed to create a plain window: %s/%s\n", group, name);
        *retv = PCRDR_SC_INSUFFICIENT_STORAGE;
    }

done:
    return (purcmc_page *)plainwin;
}

#if 0
static void *get_plainwin_from_udom(purcmc_session *sess,
        purcmc_udom *udom, int *retv)
{
    void *data;
    void *plainwin = NULL;

    if (sorted_array_find(sess->all_handles, PTR2U64(udom), &data)) {
        if ((uintptr_t)data == HT_WEBVIEW) {
            plainwin = g_object_get_data(G_OBJECT(udom), "purcmc-container");
            if (plainwin == NULL)
                goto done;

            if (!sorted_array_find(sess->all_handles, PTR2U64(plainwin),
                        &data)) {

                if (sess->workspace->layouter) {
                    if (ws_layouter_retrieve_widget(sess->workspace->layouter,
                                plainwin) == WS_WIDGET_TYPE_PLAINWINDOW) {
                        goto done;
                    }
                }

                *retv = PCRDR_SC_NOT_FOUND;
                plainwin = NULL;
            }
            else if ((uintptr_t)data != HT_PLAINWIN) {
                *retv = PCRDR_SC_BAD_REQUEST;
                plainwin = NULL;
            }
        }
    }

done:
    return plainwin;
}
#endif

#define PLAINWIN_PROP_NAME                  "name"
#define PLAINWIN_PROP_CLASS                 "class"
#define PLAINWIN_PROP_TITLE                 "title"
#define PLAINWIN_PROP_LAYOUTSTYLE           "layoutStyle"
#define PLAINWIN_PROP_TOOLKITSTYLE          "toolkitStyle"
#define PLAINWIN_PROP_TRANSITIONSTYLE       "transitionStyle"

static int update_plainwin(BrowserPlainWindow *plainwin, const char *property,
        purc_variant_t value)
{
    if (strcmp(property, PLAINWIN_PROP_NAME) == 0) {
        /* Forbid to change name of a plain window */
        return PCRDR_SC_FORBIDDEN;
    }
    else if (strcmp(property, PLAINWIN_PROP_CLASS) == 0) {
        /* Not acceptable to change class of a plain window */
        return PCRDR_SC_NOT_ACCEPTABLE;
    }
    else if (strcmp(property, PLAINWIN_PROP_TITLE) == 0) {
        const char *title = purc_variant_get_string_const(value);
        if (title) {
            browser_plain_window_set_title(plainwin, title);
        }
        else {
            return PCRDR_SC_BAD_REQUEST;
        }
    }
    else if (strcmp(property, PLAINWIN_PROP_LAYOUTSTYLE) == 0) {
        /* TODO */
    }
    else if (strcmp(property, PLAINWIN_PROP_TOOLKITSTYLE) == 0) {
        /* TODO */
    }
    else if (strcmp(property, PLAINWIN_PROP_TRANSITIONSTYLE) == 0) {
        /* TODO */
    }

    return PCRDR_SC_OK;
}

int gtk_update_plainwin(purcmc_session *sess, purcmc_workspace *workspace,
        purcmc_page *page, const char *property, purc_variant_t value)
{
    int retv;

    workspace = sess->workspace;   /* only one workspace */
    if (validate_handle(sess, page, &retv) == NULL) {
        return retv;
    }

    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(page);
    if (property != NULL) {
        return update_plainwin(window, property, value);
    }

    /* handle transitionStyle first */
    purc_variant_t trans = purc_variant_object_get_by_ckey(value,
            PLAINWIN_PROP_TRANSITIONSTYLE);
    if (trans) {
        retv = update_plainwin(window, PLAINWIN_PROP_TRANSITIONSTYLE, trans);
        if (retv != PCRDR_SC_OK) {
            return retv;
        }
    }

    struct pcvrnt_object_iterator* it;
    it = pcvrnt_object_iterator_create_begin(value);
    while (it) {
        const char     *key = pcvrnt_object_iterator_get_ckey(it);
        purc_variant_t  val = pcvrnt_object_iterator_get_value(it);
        if (strcmp(key, PLAINWIN_PROP_TRANSITIONSTYLE) == 0) {
            goto next;
        }

        retv = update_plainwin(window, key, val);
        if (retv != PCRDR_SC_OK) {
            break;
        }

next:
        bool having = pcvrnt_object_iterator_next(it);
        if (!having) {
            break;
        }
    }
    pcvrnt_object_iterator_release(it);

    return retv;
}

int gtk_destroy_plainwin(purcmc_session *sess, purcmc_workspace *workspace,
        purcmc_page *page)
{
    int retv;
    if (validate_handle(sess, page, &retv) == NULL) {
        return retv;
    }

    void *plainwin = page;
    workspace = sess->workspace;
    return gtk_imp_destroy_widget(workspace, sess, plainwin, plainwin,
        WS_WIDGET_TYPE_PLAINWINDOW);
}

static void
request_ready_callback(GObject* obj, GAsyncResult* result, gpointer user_data)
{
    WebKitWebView *webview = WEBKIT_WEB_VIEW(obj);
    purcmc_session *sess = user_data;

    WebKitUserMessage * message;
    message = webkit_web_view_send_message_to_page_finish(webview, result, NULL);

    if (message) {
        GVariant *param = webkit_user_message_get_parameters(message);

        const char* type = g_variant_get_type_string(param);
        if (strcmp(type, "s") == 0) {
            size_t len;
            const char *str = g_variant_get_string(param, &len);

            LOG_DEBUG("The parameter of message named (%s): %s\n",
                    webkit_user_message_get_name(message),
                    g_variant_get_string(param, NULL));
            handle_response_from_webpage(sess, str, len);
        }
        else {
            LOG_DEBUG("Not supported parameter type: %s\n", type);
        }
    }
}

uint64_t
gtk_register_crtn(purcmc_session *sess, purcmc_page *page,
        uint64_t crtn, int *retv)
{
    WebKitWebView *webview = validate_handle(sess, page, retv);
    if (webview == NULL) {
        return 0;
    }

    purc_page_ostack_t ostack = g_object_get_data(G_OBJECT(webview),
            "purcmc-owner-stack");
    struct purc_page_owner owner = { sess, crtn }, suppressed;
    suppressed = purc_page_ostack_register(ostack, owner);
    if (suppressed.corh && suppressed.sess != sess) {
        suppressed.corh = 0;
        /* TODO: send suppressPage request to another endpoint */
    }

    *retv = PCRDR_SC_OK;
    return suppressed.corh;
}

uint64_t
gtk_revoke_crtn(purcmc_session *sess, purcmc_page *page,
        uint64_t crtn, int *retv)
{
    WebKitWebView *webview = validate_handle(sess, page, retv);
    if (webview == NULL) {
        return 0;
    }

    purc_page_ostack_t ostack = g_object_get_data(G_OBJECT(webview),
            "purcmc-owner-stack");
    struct purc_page_owner owner = { sess, crtn }, to_reload;
    to_reload = purc_page_ostack_revoke(ostack, owner);
    if (to_reload.corh && to_reload.sess != sess) {
        to_reload.corh = 0;
        /* TODO: send reloadPage request to another endpoint */
    }

    *retv = PCRDR_SC_OK;
    return to_reload.corh;
}

#define PAGE_MESSAGE_FORMAT  "{"    \
        "\"operation\":\"%s\","     \
        "\"requestId\":\"%s\","     \
        "\"data\":\"%s\"}"

purcmc_udom *gtk_load_or_write(purcmc_session *sess, purcmc_page *page,
            int op, const char *op_name, const char* request_id,
            const char *content, size_t length,
            uint64_t crtn, char *buff, int *retv)
{
    WebKitWebView *webview = validate_handle(sess, page, retv);
    if (webview == NULL)
        return NULL;

    char *escaped = pcutils_escape_string_for_json(content);
    gchar *json = g_strdup_printf(PAGE_MESSAGE_FORMAT, op_name,
            request_id, escaped ? escaped : "");

    WebKitUserMessage * message = webkit_user_message_new("request",
            g_variant_new_string(json));
    g_free(json);
    free(escaped);

    webkit_web_view_send_message_to_page(webview, message, NULL,
            request_ready_callback, sess);

    if (op == PCRDR_K_OPERATION_LOAD || op == PCRDR_K_OPERATION_WRITEBEGIN) {
        purc_page_ostack_t ostack = g_object_get_data(G_OBJECT(webview),
                "purcmc-owner-stack");
        struct purc_page_owner owner = { sess, crtn }, suppressed;
        suppressed = purc_page_ostack_register(ostack, owner);

        if (suppressed.corh) {
            if (suppressed.sess == sess) {
                sprintf(buff,
                        "%llx", (unsigned long long int)suppressed.corh);
            }
            else {
                buff[0] = 0;
                /* TODO: send suppressPage request to another endpoint */
            }
        }
        else {
            buff[0] = 0;
        }
    }

    *retv = 0;
    return (purcmc_udom *)webview;
}

#define DOM_MESSAGE_FORMAT  "{"     \
        "\"operation\":\"%s\","     \
        "\"requestId\":\"%s\","     \
        "\"elementType\":\"%s\","   \
        "\"element\":\"%s\","       \
        "\"property\":\"%s\","      \
        "\"dataType\":\"%s\","      \
        "\"data\":\"%s\"}"

int gtk_update_dom(purcmc_session *sess, purcmc_udom *dom,
            int op, const char *op_name, const char* request_id,
            const char* element_type, const char* element_value,
            const char* property, pcrdr_msg_data_type text_type,
            const char *content, size_t length)
{
    int retv = PCRDR_SC_OK;

    WebKitWebView *webview = validate_handle(sess, (purcmc_page *)dom, &retv);
    if (webview == NULL) {
        LOG_ERROR("Bad DOM pointer: %p.\n", dom);
        return retv;
    }

    if (property) {
        if (strncmp(property, "attr.", 5) == 0) {
            if (!purc_is_valid_loose_token(property + 5, PURC_LEN_PROPERTY_NAME)) {
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
            property ? property : "", pcrdr_data_type_name(text_type),
            escaped ? escaped : "");
    if (element_escaped)
        free(element_escaped);
    if (escaped)
        free(escaped);

    WebKitUserMessage * message = webkit_user_message_new("request",
            g_variant_new_string(json));
    g_free(json);

    webkit_web_view_send_message_to_page(webview, message, NULL,
            request_ready_callback, sess);

    return 0;
}

#define DOM_MESSAGE_FORMAT_CALLMETHOD  "{"      \
        "\"operation\":\"callMethod\","         \
        "\"requestId\":\"%s\","                 \
        "\"elementType\":\"%s\","               \
        "\"element\":\"%s\","                   \
        "\"data\":{"                            \
        "\"method\":\"%s\","                    \
        "\"arg\":%s}}"

purc_variant_t
gtk_call_method_in_dom(purcmc_session *sess, const char *request_id,
        purcmc_udom *dom, const char* element_type, const char* element_value,
        const char *method, purc_variant_t arg, int* retv)
{
    WebKitWebView *webview = validate_handle(sess, (purcmc_page *)dom, retv);
    if (webview == NULL) {
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
                PCVRNT_SERIALIZE_OPT_PLAIN, NULL) < 0) {
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

    webkit_web_view_send_message_to_page(webview, message, NULL,
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
        purcmc_udom *dom, const char* element_type, const char* element_value,
        const char *property, int *retv)
{
    WebKitWebView *webview = validate_handle(sess, (purcmc_page *)dom, retv);
    if (webview == NULL) {
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

    webkit_web_view_send_message_to_page(webview, message, NULL,
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
        purcmc_udom *dom, const char* element_type, const char* element_value,
        const char *property, purc_variant_t value, int *retv)
{
    WebKitWebView *webview = validate_handle(sess, (purcmc_page *)dom, retv);
    if (webview == NULL) {
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
                PCVRNT_SERIALIZE_OPT_PLAIN, NULL) < 0) {
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

    webkit_web_view_send_message_to_page(webview, message, NULL,
            request_ready_callback, sess);

    *retv = 0;
    return PURC_VARIANT_INVALID;
}

int gtk_set_page_groups(purcmc_session *sess, purcmc_workspace *workspace,
        const char *content, size_t length)
{
    int retv;

    workspace = sess->workspace;   /* only one workspace */
    if (workspace->layouter == NULL) {
        struct ws_metrics metrics;
        gtk_imp_get_monitor_geometry(&metrics);
        LOG_INFO("Monitor size: %u x %u\n", metrics.width, metrics.height);

        workspace->layouter = ws_layouter_new(&metrics, content, length,
                workspace, gtk_imp_convert_style, gtk_imp_create_widget,
                gtk_imp_destroy_widget, gtk_imp_update_widget, &retv);
        if (workspace->layouter == NULL) {
            LOG_ERROR("Failed to create layouter\n");
        }
    }
    else {
        retv = PCRDR_SC_CONFLICT;
    }

    return retv;
}

int gtk_add_page_groups(purcmc_session *sess, purcmc_workspace *workspace,
        const char *content, size_t length)
{
    int retv;

    workspace = sess->workspace;   /* only one workspace */
    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        retv = ws_layouter_add_widget_groups(workspace->layouter,
                    content, length);
    }

    return retv;
}

int gtk_remove_page_group(purcmc_session *sess, purcmc_workspace *workspace,
        const char* group)
{
    int retv;

    workspace = sess->workspace;   /* only one workspace */
    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        retv = ws_layouter_remove_widget_group(workspace->layouter, sess, group);
    }

    return retv;
}

purcmc_page *
gtk_create_widget(purcmc_session *sess, purcmc_workspace *workspace,
            const char *request_id,
            const char *page_id, const char *group, const char *name,
            const char *klass, const char *title, const char *layout_style,
            purc_variant_t toolkit_style, int *retv)
{
    void *widget = NULL;

    workspace = sess->workspace;   /* only one workspace */
    if (pcutils_kvlist_get(workspace->page_owners, page_id)) {
        LOG_WARN("Duplicated page identifier: %s\n", page_id);
        *retv = PCRDR_SC_CONFLICT;
    }
    else if (workspace->layouter == NULL) {
        *retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        WebKitWebView *webview = create_web_view(sess);
        widget = ws_layouter_add_widget(workspace->layouter, sess,
                    group, name, klass, title,
                    layout_style, toolkit_style, webview, retv);

        if (widget) {
            purc_page_ostack_t ostack =
                purc_page_ostack_new(workspace->page_owners, page_id, webview);
            g_object_set_data(G_OBJECT(webview), "purcmc-owner-stack", ostack);

            web_view_load_uri(webview, sess, group, name, request_id);

            gtk_widget_grab_focus(GTK_WIDGET(webview));

            sorted_array_add(sess->all_handles, PTR2U64(widget),
                    INT2PTR(HT_PANE_TAB));
            sorted_array_add(sess->all_handles, PTR2U64(webview),
                    INT2PTR(HT_WEBVIEW));
            *retv = 0;
        }
        else {
            gtk_widget_destroy(GTK_WIDGET(webview));
        }
    }

    return (purcmc_page *)widget;
}

int gtk_update_widget(purcmc_session *sess, purcmc_workspace *workspace,
            purcmc_page *page, const char *property, purc_variant_t value)
{
    int retv;

    workspace = sess->workspace;   /* only one workspace */
    if (validate_handle(sess, page, &retv) == NULL) {
        return retv;
    }

    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        ws_widget_type_t type =
            ws_layouter_retrieve_widget(workspace->layouter, page);
        if (type == WS_WIDGET_TYPE_PANEDPAGE ||
                type == WS_WIDGET_TYPE_TABBEDPAGE) {
            retv = ws_layouter_update_widget(workspace->layouter, sess, page,
                    property, value);
        }
        else {
            retv = PCRDR_SC_BAD_REQUEST;
        }
    }

    return retv;
}

int gtk_destroy_widget(purcmc_session *sess, purcmc_workspace *workspace,
            purcmc_page *page)
{
    int retv;

    workspace = sess->workspace;   /* only one workspace */
    if (validate_handle(sess, page, &retv) == NULL) {
        return retv;
    }

    if (workspace->layouter == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
    }
    else {
        ws_widget_type_t type =
            ws_layouter_retrieve_widget(workspace->layouter, page);
        if (type == WS_WIDGET_TYPE_PANEDPAGE ||
                type == WS_WIDGET_TYPE_TABBEDPAGE) {
            if (ws_layouter_remove_widget_by_handle(workspace->layouter,
                        sess, page))
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

