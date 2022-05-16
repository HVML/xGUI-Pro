/*
** endpoint.c -- The endpoint session management.
**
** Copyright (c) 2020 ~ 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnigth Commander.
**
** hiBus is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** hiBus is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <purc/purc-dom.h>
#include <purc/purc-html.h>

#include "endpoint.h"
#include "unixsocket.h"
#include "websocket.h"

#if 0
typedef struct PlainWindow {
    purc_variant_t      id;
    purc_variant_t      title;
    pcdom_document_t    *dom_doc;

    pchtml_html_parser_t *parser;
} PlainWindow;

struct purcmc_session {
    struct kvlist       wins;
    unsigned int        nr_wins;
};

static void remove_window(purcmc_endpoint *endpoint, PlainWindow *win)
{
    if (win->dom_doc) {
        char endpoint_name[PURC_LEN_ENDPOINT_NAME + 1];

        assemble_endpoint_name(endpoint, endpoint_name);
        domview_detach_window_dom(endpoint_name,
            purc_variant_get_string_const(win->id));
        dom_cleanup_user_data(win->dom_doc);
        pcdom_document_destroy(win->dom_doc);
    }

    if (win->parser) {
        pchtml_html_parser_destroy(win->parser);
    }

    if (win->id)
        purc_variant_unref(win->id);
    if (win->title)
        purc_variant_unref(win->title);
    free(win);
}

static void remove_session(purcmc_server* srv, purcmc_endpoint* endpoint)
{
    const char *name;
    void *next, *data;
    PlainWindow *win;

    if (endpoint->session) {
        kvlist_for_each_safe(&endpoint->session->wins, name, next, data) {
            win = *(PlainWindow **)data;

            kvlist_delete(&endpoint->session->wins, name);
            remove_window(endpoint, win);
        }
        kvlist_free(&endpoint->session->wins);
        free(endpoint->session);
        endpoint->session = NULL;
    }
}
#endif

purcmc_endpoint* new_endpoint(purcmc_server* srv, int type, void* client)
{
    struct timespec ts;
    purcmc_endpoint* endpoint = NULL;

    endpoint = (purcmc_endpoint *)calloc (sizeof (purcmc_endpoint), 1);
    if (endpoint == NULL)
        return NULL;

    clock_gettime (CLOCK_MONOTONIC, &ts);
    endpoint->t_created = ts.tv_sec;
    endpoint->t_living = ts.tv_sec;
    endpoint->avl.key = NULL;

    switch (type) {
        case ET_UNIX_SOCKET:
        case ET_WEB_SOCKET:
            endpoint->type = type;
            endpoint->status = ES_AUTHING;
            endpoint->entity.client = client;

            endpoint->host_name = NULL;
            endpoint->app_name = NULL;
            endpoint->runner_name = NULL;
            if (!store_dangling_endpoint (srv, endpoint)) {
                purc_log_error ("Failed to store dangling endpoint\n");
                free (endpoint);
                return NULL;
            }
            break;

        default:
            purc_log_error ("Bad endpoint type\n");
            free (endpoint);
            return NULL;
    }

    if (type == ET_UNIX_SOCKET) {
        USClient* usc = (USClient*)client;
        usc->entity = &endpoint->entity;
    }
    else if (type == ET_WEB_SOCKET) {
        WSClient* wsc = (WSClient*)client;
        wsc->entity = &endpoint->entity;
    }

    return endpoint;
}

int del_endpoint(purcmc_server* srv, purcmc_endpoint* endpoint, int cause)
{
    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];

    srv->cbs.remove_session(srv, endpoint->session);
    endpoint->session = NULL;

    if (assemble_endpoint_name(endpoint, endpoint_name) > 0) {
        if (endpoint->avl.key)
            avl_delete (&srv->living_avl, &endpoint->avl);
    }
    else {
        strcpy (endpoint_name, "@endpoint/not/authenticated");
    }

    if (endpoint->host_name) free (endpoint->host_name);
    if (endpoint->app_name) free (endpoint->app_name);
    if (endpoint->runner_name) free (endpoint->runner_name);

    free (endpoint);
    purc_log_warn ("purcmc_endpoint (%s) removed\n", endpoint_name);
    return 0;
}

bool store_dangling_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint)
{
    if (srv->dangling_endpoints == NULL)
        srv->dangling_endpoints = gslist_create (endpoint);
    else
        srv->dangling_endpoints =
            gslist_insert_append (srv->dangling_endpoints, endpoint);

    if (srv->dangling_endpoints)
        return true;

    return false;
}

bool remove_dangling_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint)
{
    gs_list* node = srv->dangling_endpoints;

    while (node) {
        if (node->data == endpoint) {
            gslist_remove_node (&srv->dangling_endpoints, node);
            return true;
        }

        node = node->next;
    }

    return false;
}

bool make_endpoint_ready (purcmc_server* srv,
        const char* endpoint_name, purcmc_endpoint* endpoint)
{
    if (remove_dangling_endpoint (srv, endpoint)) {
        if (!kvlist_set (&srv->endpoint_list, endpoint_name, &endpoint)) {
            purc_log_error ("Failed to store the endpoint: %s\n", endpoint_name);
            return false;
        }

        endpoint->t_living = purc_get_monotoic_time ();
        endpoint->avl.key = endpoint;
        if (avl_insert (&srv->living_avl, &endpoint->avl)) {
            purc_log_error ("Failed to insert to the living AVL tree: %s\n", endpoint_name);
            assert (0);
            return false;
        }
        srv->nr_endpoints++;
    }
    else {
        purc_log_error ("Not found endpoint in dangling list: %s\n", endpoint_name);
        return false;
    }

    return true;
}

static void cleanup_endpoint_client (purcmc_server *srv, purcmc_endpoint* endpoint)
{
    if (endpoint->type == ET_UNIX_SOCKET) {
        endpoint->entity.client->entity = NULL;
        us_cleanup_client (srv->us_srv, (USClient*)endpoint->entity.client);
    }
    else if (endpoint->type == ET_WEB_SOCKET) {
        endpoint->entity.client->entity = NULL;
        ws_cleanup_client (srv->ws_srv, (WSClient*)endpoint->entity.client);
    }

    purc_log_warn ("The endpoint (@%s/%s/%s) client cleaned up\n",
            endpoint->host_name, endpoint->app_name, endpoint->runner_name);
}

int check_no_responding_endpoints (purcmc_server *srv)
{
    int n = 0;
    time_t t_curr = purc_get_monotoic_time ();
    purcmc_endpoint *endpoint, *tmp;

    purc_log_info ("Checking no responding endpoints...\n");

    avl_for_each_element_safe (&srv->living_avl, endpoint, avl, tmp) {
        char name [PURC_LEN_ENDPOINT_NAME + 1];

        assert (endpoint->type != ET_BUILTIN);

        assemble_endpoint_name (endpoint, name);
        if (t_curr > endpoint->t_living + PCRDR_MAX_NO_RESPONDING_TIME) {

            kvlist_delete (&srv->endpoint_list, name);
            cleanup_endpoint_client (srv, endpoint);
            del_endpoint (srv, endpoint, CDE_NO_RESPONDING);
            srv->nr_endpoints--;
            n++;

            purc_log_info ("A no-responding client: %s\n", name);
        }
        else if (t_curr > endpoint->t_living + PCRDR_MAX_PING_TIME) {
            if (endpoint->type == ET_UNIX_SOCKET) {
                us_ping_client (srv->us_srv, (USClient *)endpoint->entity.client);
            }
            else if (endpoint->type == ET_WEB_SOCKET) {
                ws_ping_client (srv->ws_srv, (WSClient *)endpoint->entity.client);
            }

            purc_log_info ("Ping client: %s\n", name);
        }
        else {
            purc_log_info ("Skip left endpoints since (%s): %ld\n",
                    name, endpoint->t_living);
            break;
        }
    }

    purc_log_info ("Total endpoints removed: %d\n", n);
    return n;
}

int check_dangling_endpoints (purcmc_server *srv)
{
    int n = 0;
    time_t t_curr = purc_get_monotoic_time ();
    gs_list* node = srv->dangling_endpoints;

    while (node) {
        gs_list *next = node->next;
        purcmc_endpoint* endpoint = (purcmc_endpoint *)node->data;

        if (t_curr > endpoint->t_created + PCRDR_MAX_NO_RESPONDING_TIME) {
            gslist_remove_node (&srv->dangling_endpoints, node);
            cleanup_endpoint_client (srv, endpoint);
            del_endpoint (srv, endpoint, CDE_NO_RESPONDING);
            n++;
        }

        node = next;
    }

    return n;
}

int send_packet_to_endpoint (purcmc_server* srv,
        purcmc_endpoint* endpoint, const char* body, int len_body)
{
    if (endpoint->type == ET_UNIX_SOCKET) {
        return us_send_packet (srv->us_srv, (USClient *)endpoint->entity.client,
                US_OPCODE_TEXT, body, len_body);
    }
    else if (endpoint->type == ET_WEB_SOCKET) {
        return ws_send_packet (srv->ws_srv, (WSClient *)endpoint->entity.client,
                WS_OPCODE_TEXT, body, len_body);
    }

    return -1;
}

static int send_simple_response(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    size_t n;
    char buff [PCRDR_DEF_PACKET_BUFF_SIZE];

    n = pcrdr_serialize_message_to_buffer (msg, buff, sizeof(buff));
    if (n > sizeof(buff)) {
        purc_log_error ("The size of buffer for simple response packet is too small.\n");
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
    }
    else if (send_packet_to_endpoint (srv, endpoint, buff, n)) {
        endpoint->status = ES_CLOSING;
        retv = PCRDR_SC_IOERR;
    }

    return retv;
}

int send_initial_response (purcmc_server* srv, purcmc_endpoint* endpoint)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg *msg = NULL;

    msg = pcrdr_make_response_message ("0",
            PCRDR_SC_OK, 0,
            PCRDR_MSG_DATA_TYPE_TEXT, SERVER_FEATURES,
            sizeof (SERVER_FEATURES) - 1);
    if (msg == NULL) {
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        goto failed;
    }

    retv = send_simple_response(srv, endpoint, msg);

    pcrdr_release_message (msg);

failed:
    return retv;
}

typedef int (*request_handler)(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg);

static int authenticate_endpoint(purcmc_server* srv, purcmc_endpoint* endpoint,
        purc_variant_t data)
{
    const char* prot_name = NULL;
    const char *host_name = NULL, *app_name = NULL, *runner_name = NULL;
    uint64_t prot_ver = 0;
    char norm_host_name [PURC_LEN_HOST_NAME + 1];
    char norm_app_name [PURC_LEN_APP_NAME + 1];
    char norm_runner_name [PURC_LEN_RUNNER_NAME + 1];
    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];
    purc_variant_t tmp;

    if ((tmp = purc_variant_object_get_by_ckey(data, "protocolName"))) {
        prot_name = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data, "protocolVersion"))) {
        purc_variant_cast_to_ulongint(tmp, &prot_ver, true);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data, "hostName"))) {
        host_name = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data, "appName"))) {
        app_name = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(data, "runnerName"))) {
        runner_name = purc_variant_get_string_const(tmp);
    }

    if (prot_name == NULL || prot_ver > PCRDR_PURCMC_PROTOCOL_VERSION ||
            host_name == NULL || app_name == NULL || runner_name == NULL ||
            strcasecmp (prot_name, PCRDR_PURCMC_PROTOCOL_NAME)) {
        purc_log_warn ("Bad packet data for authentication: %s, %s, %s, %s\n",
                prot_name, host_name, app_name, runner_name);
        return PCRDR_SC_BAD_REQUEST;
    }

    if (prot_ver < PCRDR_PURCMC_MINIMAL_PROTOCOL_VERSION)
        return PCRDR_SC_UPGRADE_REQUIRED;

    if (!purc_is_valid_host_name (host_name) ||
            !purc_is_valid_app_name (app_name) ||
            !purc_is_valid_token (runner_name, PURC_LEN_RUNNER_NAME)) {
        purc_log_warn ("Bad endpoint name: @%s/%s/%s\n",
                host_name, app_name, runner_name);
        return PCRDR_SC_NOT_ACCEPTABLE;
    }

    purc_name_tolower_copy (host_name, norm_host_name, PURC_LEN_HOST_NAME);
    purc_name_tolower_copy (app_name, norm_app_name, PURC_LEN_APP_NAME);
    purc_name_tolower_copy (runner_name, norm_runner_name, PURC_LEN_RUNNER_NAME);
    host_name = norm_host_name;
    app_name = norm_app_name;
    runner_name = norm_runner_name;

    /* make endpoint ready here */
    if (endpoint->type == CT_UNIX_SOCKET) {
        /* override the host name */
        host_name = PCRDR_LOCALHOST;
    }
    else {
        /* TODO: handle hostname for web socket connections here */
        host_name = PCRDR_LOCALHOST;
    }

    purc_assemble_endpoint_name (host_name,
                    app_name, runner_name, endpoint_name);

    purc_log_info ("New endpoint: %s (%p)\n", endpoint_name, endpoint);

    if (kvlist_get (&srv->endpoint_list, endpoint_name)) {
        purc_log_warn ("Duplicated endpoint: %s\n", endpoint_name);
        return PCRDR_SC_CONFLICT;
    }

    if (!make_endpoint_ready (srv, endpoint_name, endpoint)) {
        purc_log_error ("Failed to store the endpoint: %s\n", endpoint_name);
        return PCRDR_SC_INSUFFICIENT_STORAGE;
    }

    purc_log_info ("New endpoint stored: %s (%p), %d endpoints totally.\n",
            endpoint_name, endpoint, srv->nr_endpoints);

    endpoint->host_name = strdup (host_name);
    endpoint->app_name = strdup (app_name);
    endpoint->runner_name = strdup (runner_name);
    endpoint->status = ES_READY;

    return PCRDR_SC_OK;
}

static int on_start_session(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    purcmc_session *info = NULL;

    int retv = authenticate_endpoint(srv, endpoint, msg->data);

    endpoint->session = NULL;
    if (retv == PCRDR_SC_OK) {
        info = srv->cbs.create_session(srv, endpoint);
        if (info == NULL) {
            retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        }
        else {
            endpoint->session = info;
        }
    }

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)info;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_end_session(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    if (endpoint->session) {
        srv->cbs.remove_session(srv, endpoint->session);
        endpoint->session = NULL;
    }

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_create_plain_window(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response;

    purcmc_plainwin* win = NULL;

    const char* gid = NULL;
    const char* name = NULL;
    const char* title = NULL;
    const char* style = NULL;

    purc_variant_t tmp;

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_EJSON ||
            !purc_variant_is_object(msg->data)) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if ((tmp = purc_variant_object_get_by_ckey(msg->data, "gid"))) {
        gid = purc_variant_get_string_const(tmp);

        if (gid != NULL && !purc_is_valid_identifier(gid)) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }
    }

    if ((tmp = purc_variant_object_get_by_ckey(msg->data, "name"))) {
        name = purc_variant_get_string_const(tmp);

        if (name == NULL || !purc_is_valid_identifier(name)) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }
    }

    if ((tmp = purc_variant_object_get_by_ckey(msg->data, "title"))) {
        title = purc_variant_get_string_const(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(msg->data, "style"))) {
        style = purc_variant_get_string_const(tmp);
    }

#if 0
    PlainWindow *win;
    if ((win = calloc(1, sizeof(*win))) == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }

    if () {
        if (kvlist_get(&endpoint->session->wins, str)) {
            purc_log_warn("Duplicated plain window: %s\n", str);
            retv = PCRDR_SC_CONFLICT;
            goto failed;
        }

        if (!kvlist_set(&endpoint->session->wins, str, &win)) {
            retv = PCRDR_SC_INSUFFICIENT_STORAGE;
            goto failed;
        }

        win->id = purc_variant_ref(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(msg->data, "title"))) {
        win->title = purc_variant_ref(tmp);
    }

    if (retv != PCRDR_SC_OK && win) {
        remove_window(endpoint, win);
        win = NULL;
    }
#endif

    win = srv->cbs.create_plainwin(srv, endpoint->session, gid, name,
            title, style);
    if (win == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)win;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_update_plain_window(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    const char *element;
    purcmc_plainwin *win;
    pcrdr_msg response;

    element = purc_variant_get_string_const(msg->element);
    if (element == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        if (srv->cbs.get_plainwin_by_handle == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        win = srv->cbs.get_plainwin_by_handle(srv, endpoint->session, handle);
#if 0
        const char *key;
        void *data;
        kvlist_for_each(&endpoint->session->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if ((uint64_t)p == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
#endif

    }
    else if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        if (srv->cbs.get_plainwin_by_id == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        win = srv->cbs.get_plainwin_by_id(srv, endpoint->session, element);
#if 0
        void *data = kvlist_get(&endpoint->session->wins, element);

        if (data) {
            win = *(PlainWindow **)data;
        }
#endif
    }

    if (win == NULL) {
        purc_log_warn("Specified plain window not found: %s\n", element);
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    const char *property;
    property = purc_variant_get_string_const(msg->property);
    if (property == NULL || msg->dataType != PCRDR_MSG_DATA_TYPE_TEXT) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.update_plainwin(srv, win, property,
            purc_variant_get_string_const(msg->data));

#if 0
    if (win->title)
        purc_variant_unref(win->title);
    win->title = purc_variant_ref(msg->data);
    dom_set_title(win->dom_doc, purc_variant_get_string_const(win->title));
#endif

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)win;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_destroy_plain_window(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    const char *element;
    purcmc_plainwin *win;
    pcrdr_msg response;

    element = purc_variant_get_string_const(msg->element);
    if (element == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        if (srv->cbs.get_plainwin_by_handle == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        win = srv->cbs.get_plainwin_by_handle(srv, endpoint->session, handle);

#if 0
        const char *key;
        void *data;
        kvlist_for_each(&endpoint->session->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if ((uint64_t)p == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
#endif
    }
    else if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        if (srv->cbs.get_plainwin_by_id == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        win = srv->cbs.get_plainwin_by_id(srv, endpoint->session, element);

#if 0
        void *data = kvlist_get(&endpoint->session->wins, element);
        if (data) {
            win = *(PlainWindow **)data;
        }
#endif
    }

    if (win == NULL) {
        purc_log_warn("Specified plain window not found: %s\n", element);
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

#if 0
    retv = srv->remove_plainwin(srv, win);

    kvlist_delete(&endpoint->session->wins,
            purc_variant_get_string_const(win->id));
    remove_window(endpoint, win);
#endif

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)win;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_load(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    purcmc_plainwin *win = NULL;
    purcmc_dom *dom = NULL;

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_TEXT ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    doc_text = purc_variant_get_string_const_ex(msg->data, &doc_len);
    if (doc_text == NULL || doc_len == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW) {
        if (srv->cbs.get_plainwin_by_handle == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        win = srv->cbs.get_plainwin_by_handle(srv, endpoint->session,
                msg->targetValue);

#if 0
        kvlist_for_each(&endpoint->session->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
#endif
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (win == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    purcmc_page *page = srv->cbs.get_plainwin_page(srv, win);
    dom = srv->cbs.load(srv, page, doc_text, doc_len);
    if (dom == NULL) {
        retv = PCRDR_SC_UNPROCESSABLE_PACKET;
        goto failed;
    }

#if 0
    pchtml_html_parser_t *parser = NULL;
    pchtml_html_document_t *html_doc = NULL;

    parser = pchtml_html_parser_create();
    if (parser == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }
    pchtml_html_parser_init(parser);

    html_doc = pchtml_html_parse_with_buf(parser,
            (const unsigned char *)doc_text, doc_len);
    pchtml_html_parser_destroy(parser);

    if (html_doc == NULL) {
        retv = PCRDR_SC_UNPROCESSABLE_PACKET;
        goto failed;
    }

    char endpoint_name[PURC_LEN_ENDPOINT_NAME + 1];
    assemble_endpoint_name(endpoint, endpoint_name);

    if (win->dom_doc) {
        domview_detach_window_dom(endpoint_name,
            purc_variant_get_string_const(win->id));
        dom_cleanup_user_data(win->dom_doc);
        pcdom_document_destroy(win->dom_doc);
    }
    win->dom_doc = pcdom_interface_document(html_doc);
    dom_prepare_user_data(win->dom_doc, true);
    domview_attach_window_dom(endpoint_name,
            purc_variant_get_string_const(win->id),
            purc_variant_get_string_const(win->title),
            win->dom_doc);
#endif

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)dom;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_write_begin(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    purcmc_plainwin *win = NULL;
    purcmc_dom *dom = NULL;

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_TEXT ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    doc_text = purc_variant_get_string_const_ex(msg->data, &doc_len);
    if (doc_text == NULL || doc_len == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW) {
        if (srv->cbs.get_plainwin_by_handle == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        win = srv->cbs.get_plainwin_by_handle(srv, endpoint->session,
                msg->targetValue);
#if 0
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
#endif
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (win == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    purcmc_page *page = srv->cbs.get_plainwin_page(srv, win);
    dom = srv->cbs.write_begin(srv, page, doc_text, doc_len);
    if (dom == NULL) {
        retv = PCRDR_SC_UNPROCESSABLE_PACKET;
        goto failed;
    }

#if 0
    pchtml_html_parser_t *parser = NULL;
    pchtml_html_document_t *html_doc = NULL;

    if (win->parser) {
        retv = PCRDR_SC_EXPECTATION_FAILED;
        goto failed;
    }

    parser = pchtml_html_parser_create();
    if (parser == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }
    pchtml_html_parser_init(parser);
    win->parser = parser;

    html_doc = pchtml_html_parse_chunk_begin(parser);
    pchtml_html_parse_chunk_process(parser,
                (const unsigned char *)doc_text, doc_len);

    if (html_doc == NULL) {
        retv = PCRDR_SC_UNPROCESSABLE_PACKET;
        goto failed;
    }

    if (win->dom_doc) {
        char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];

        assemble_endpoint_name (endpoint, endpoint_name);
        domview_detach_window_dom(endpoint_name,
            purc_variant_get_string_const(win->id));
        dom_cleanup_user_data(win->dom_doc);
        pcdom_document_destroy(win->dom_doc);
    }
    win->dom_doc = pcdom_interface_document(html_doc);

    if (retv != PCRDR_SC_OK) {
        if (parser) {
            pchtml_html_parser_destroy(parser);
            win->parser = NULL;
        }
    }
#endif

failed:

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)dom;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_write_more(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    purcmc_page *page = NULL;
    purcmc_dom *dom = NULL;

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_TEXT ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    doc_text = purc_variant_get_string_const_ex(msg->data, &doc_len);
    if (doc_text == NULL || doc_len == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW) {
        purcmc_plainwin *win = NULL;

        if (srv->cbs.get_plainwin_by_handle == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        win = srv->cbs.get_plainwin_by_handle(srv, endpoint->session,
                msg->targetValue);
        if (win) {
            page = srv->cbs.get_plainwin_page(srv, win);
        }
#if 0
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
#endif
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (page == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    dom = srv->cbs.write_more(srv, page, doc_text, doc_len);
    if (dom == NULL) {
        retv = PCRDR_SC_UNPROCESSABLE_PACKET;
        goto failed;
    }

#if 0
    if (win->parser == NULL || win->dom_doc == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
        goto failed;
    }

    pchtml_html_parse_chunk_process(win->parser,
                (const unsigned char *)doc_text, doc_len);
#endif

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)dom;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_write_end(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    purcmc_page *page = NULL;
    purcmc_dom *dom = NULL;

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_TEXT ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    doc_text = purc_variant_get_string_const_ex(msg->data, &doc_len);
    if (doc_text == NULL || doc_len == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW) {
        purcmc_plainwin *win = NULL;

        if (srv->cbs.get_plainwin_by_handle == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        win = srv->cbs.get_plainwin_by_handle(srv, endpoint->session,
                msg->targetValue);
        if (win) {
            page = srv->cbs.get_plainwin_page(srv, win);
        }
#if 0
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
#endif
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (page == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    dom = srv->cbs.write_end(srv, page, doc_text, doc_len);
    if (dom == NULL) {
        retv = PCRDR_SC_UNPROCESSABLE_PACKET;
        goto failed;
    }

#if 0
    if (win->parser == NULL || win->dom_doc == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
        goto failed;
    }

    pchtml_html_parse_chunk_process(win->parser,
                (const unsigned char *)doc_text, doc_len);
    pchtml_html_parse_chunk_end(win->parser);

    pchtml_html_parser_destroy(win->parser);
    win->parser = NULL;

    dom_prepare_user_data(win->dom_doc, true);


    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];
    assemble_endpoint_name (endpoint, endpoint_name);
    domview_attach_window_dom(endpoint_name,
            purc_variant_get_string_const(win->id),
            purc_variant_get_string_const(win->title),
            win->dom_doc);
#endif

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)dom;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int operate_dom_element(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg, int op, pcrdr_msg *response)
{
    int retv;
    purcmc_dom *dom = NULL;

    if (msg->target == PCRDR_MSG_TARGET_DOM) {

        if (srv->cbs.get_dom_by_handle == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        dom = srv->cbs.get_dom_by_handle(srv, endpoint->session,
                msg->targetValue);
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (dom == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    retv = srv->cbs.operate_dom_element(srv, dom, op, msg);

failed:
    response->type = PCRDR_MSG_TYPE_RESPONSE;
    response->requestId = msg->requestId;
    response->retCode = retv;
    response->resultValue = 0;
    response->dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return retv;
}

static int on_append(purcmc_server* srv, purcmc_endpoint* endpoint, const pcrdr_msg *msg)
{
    pcrdr_msg response;

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_APPEND, &response);
    }
    else {
        response.type = PCRDR_MSG_TYPE_RESPONSE;
        response.requestId = msg->requestId;
        response.retCode = PCRDR_SC_METHOD_NOT_ALLOWED;
        response.resultValue = 0;
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return send_simple_response(srv, endpoint, &response);
}

static int on_prepend(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_PREPEND, &response);
    }
    else {
        response.type = PCRDR_MSG_TYPE_RESPONSE;
        response.requestId = msg->requestId;
        response.retCode = PCRDR_SC_METHOD_NOT_ALLOWED;
        response.resultValue = 0;
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return send_simple_response(srv, endpoint, &response);
}

static int on_insert_after(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_INSERTAFTER, &response);
    }
    else {
        response.type = PCRDR_MSG_TYPE_RESPONSE;
        response.requestId = msg->requestId;
        response.retCode = PCRDR_SC_METHOD_NOT_ALLOWED;
        response.resultValue = 0;
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return send_simple_response(srv, endpoint, &response);
}

static int on_insert_before(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_INSERTBEFORE, &response);
    }
    else {
        response.type = PCRDR_MSG_TYPE_RESPONSE;
        response.requestId = msg->requestId;
        response.retCode = PCRDR_SC_METHOD_NOT_ALLOWED;
        response.resultValue = 0;
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return send_simple_response(srv, endpoint, &response);
}

static int on_displace(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_DISPLACE, &response);
    }
    else {
        response.type = PCRDR_MSG_TYPE_RESPONSE;
        response.requestId = msg->requestId;
        response.retCode = PCRDR_SC_METHOD_NOT_ALLOWED;
        response.resultValue = 0;
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return send_simple_response(srv, endpoint, &response);
}

static int on_clear(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_CLEAR, &response);
    return send_simple_response(srv, endpoint, &response);
}

static int on_erase(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_ERASE, &response);
    return send_simple_response(srv, endpoint, &response);
}

static int on_update(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_UPDATE, &response);
    return send_simple_response(srv, endpoint, &response);
}

static struct request_handler {
    const char *operation;
    request_handler handler;
} handlers[] = {
    { PCRDR_OPERATION_APPEND, on_append },
    { PCRDR_OPERATION_CLEAR, on_clear },
    { PCRDR_OPERATION_CREATEPLAINWINDOW, on_create_plain_window },
    { PCRDR_OPERATION_CREATETABBEDWINDOW, NULL },
    { PCRDR_OPERATION_CREATETABPAGE, NULL },
    { PCRDR_OPERATION_CREATEWORKSPACE, NULL },
    { PCRDR_OPERATION_DESTROYPLAINWINDOW, on_destroy_plain_window },
    { PCRDR_OPERATION_DESTROYTABBEDWINDOW, NULL },
    { PCRDR_OPERATION_DESTROYTABPAGE, NULL },
    { PCRDR_OPERATION_DESTROYWORKSPACE, NULL },
    { PCRDR_OPERATION_DISPLACE, on_displace },
    { PCRDR_OPERATION_ENDSESSION, on_end_session },
    { PCRDR_OPERATION_ERASE, on_erase },
    { PCRDR_OPERATION_INSERTAFTER, on_insert_after },
    { PCRDR_OPERATION_INSERTBEFORE, on_insert_before },
    { PCRDR_OPERATION_LOAD, on_load },
    { PCRDR_OPERATION_PREPEND, on_prepend },
    { PCRDR_OPERATION_STARTSESSION, on_start_session },
    { PCRDR_OPERATION_UPDATE, on_update },
    { PCRDR_OPERATION_UPDATEPLAINWINDOW, on_update_plain_window },
    { PCRDR_OPERATION_UPDATETABBEDWINDOW, NULL },
    { PCRDR_OPERATION_UPDATETABPAGE, NULL },
    { PCRDR_OPERATION_UPDATEWORKSPACE, NULL },
    { PCRDR_OPERATION_WRITEBEGIN, on_write_begin },
    { PCRDR_OPERATION_WRITEEND, on_write_end },
    { PCRDR_OPERATION_WRITEMORE, on_write_more },
};

/* Make sure the number of handlers matches the number of operations */
#define _COMPILE_TIME_ASSERT(name, x)               \
       typedef int _dummy_ ## name[(x) * 2 - 1]
_COMPILE_TIME_ASSERT(hdl,
        sizeof(handlers)/sizeof(handlers[0]) == PCRDR_NR_OPERATIONS);
#undef _COMPILE_TIME_ASSERT

#define NOT_FOUND_HANDLER   ((request_handler)-1)

static request_handler find_request_handler(const char* operation)
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

int on_got_message(purcmc_server* srv, purcmc_endpoint* endpoint, const pcrdr_msg *msg)
{
    if (msg->type == PCRDR_MSG_TYPE_REQUEST) {
        request_handler handler = find_request_handler(
                purc_variant_get_string_const(msg->operation));

        purc_log_info("Got a request message: %s (handler: %p)\n",
                purc_variant_get_string_const(msg->operation), handler);

        if (handler == NOT_FOUND_HANDLER) {
            pcrdr_msg response;
            response.type = PCRDR_MSG_TYPE_RESPONSE;
            response.requestId = msg->requestId;
            response.retCode = PCRDR_SC_BAD_REQUEST;
            response.resultValue = 0;
            response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

            return send_simple_response(srv, endpoint, &response);
        }
        else if (handler) {
            return handler(srv, endpoint, msg);
        }
        else {
            pcrdr_msg response;
            response.type = PCRDR_MSG_TYPE_RESPONSE;
            response.requestId = msg->requestId;
            response.retCode = PCRDR_SC_NOT_IMPLEMENTED;
            response.resultValue = 0;
            response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

            return send_simple_response(srv, endpoint, &response);
        }
    }
    else if (msg->type == PCRDR_MSG_TYPE_EVENT) {
        // TODO
        purc_log_info("Got an event message: %s\n",
                purc_variant_get_string_const(msg->event));
    }
    else {
        // TODO
        purc_log_info("Got an unknown message: %d\n", msg->type);
    }

    return PCRDR_SC_OK;
}

