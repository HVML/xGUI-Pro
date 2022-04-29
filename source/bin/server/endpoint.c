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

#define srvcfg mc_global.rdr

#define DEF_NR_HANDLES  4

typedef struct PlainWindow {
    purc_variant_t      id;
    purc_variant_t      title;
    pcdom_document_t    *dom_doc;

    pchtml_html_parser_t *parser;
} PlainWindow;

struct SessionInfo_ {
    struct kvlist       wins;
    unsigned int        nr_wins;
};

Endpoint* new_endpoint (Server* srv, int type, void* client)
{
    struct timespec ts;
    Endpoint* endpoint = NULL;

    endpoint = (Endpoint *)calloc (sizeof (Endpoint), 1);
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
                ULOG_ERR ("Failed to store dangling endpoint\n");
                free (endpoint);
                return NULL;
            }
            break;

        default:
            ULOG_ERR ("Bad endpoint type\n");
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

static void remove_window(Endpoint *endpoint, PlainWindow *win)
{
    if (win->dom_doc) {
        char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];

        assemble_endpoint_name (endpoint, endpoint_name);
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

static void remove_session(Endpoint* endpoint)
{
    const char *name;
    void *next, *data;
    PlainWindow *win;

    if (endpoint->session_info) {
        kvlist_for_each_safe(&endpoint->session_info->wins, name, next, data) {
            win = *(PlainWindow **)data;

            kvlist_delete(&endpoint->session_info->wins, name);
            remove_window(endpoint, win);
        }
        kvlist_free(&endpoint->session_info->wins);
        free(endpoint->session_info);
        endpoint->session_info = NULL;
    }
}

int del_endpoint (Server* srv, Endpoint* endpoint, int cause)
{
    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];

    remove_session(endpoint);
    if (assemble_endpoint_name (endpoint, endpoint_name) > 0) {
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
    ULOG_WARN ("Endpoint (%s) removed\n", endpoint_name);
    return 0;
}

bool store_dangling_endpoint (Server* srv, Endpoint* endpoint)
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

bool remove_dangling_endpoint (Server* srv, Endpoint* endpoint)
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

bool make_endpoint_ready (Server* srv,
        const char* endpoint_name, Endpoint* endpoint)
{
    if (remove_dangling_endpoint (srv, endpoint)) {
        if (!kvlist_set (&srv->endpoint_list, endpoint_name, &endpoint)) {
            ULOG_ERR ("Failed to store the endpoint: %s\n", endpoint_name);
            return false;
        }

        endpoint->t_living = purc_get_monotoic_time ();
        endpoint->avl.key = endpoint;
        if (avl_insert (&srv->living_avl, &endpoint->avl)) {
            ULOG_ERR ("Failed to insert to the living AVL tree: %s\n", endpoint_name);
            assert (0);
            return false;
        }
        srv->nr_endpoints++;
    }
    else {
        ULOG_ERR ("Not found endpoint in dangling list: %s\n", endpoint_name);
        return false;
    }

    return true;
}

static void cleanup_endpoint_client (Server *srv, Endpoint* endpoint)
{
    if (endpoint->type == ET_UNIX_SOCKET) {
        endpoint->entity.client->entity = NULL;
        us_cleanup_client (srv->us_srv, (USClient*)endpoint->entity.client);
    }
    else if (endpoint->type == ET_WEB_SOCKET) {
        endpoint->entity.client->entity = NULL;
        ws_cleanup_client (srv->ws_srv, (WSClient*)endpoint->entity.client);
    }

    ULOG_WARN ("The endpoint (@%s/%s/%s) client cleaned up\n",
            endpoint->host_name, endpoint->app_name, endpoint->runner_name);
}

int check_no_responding_endpoints (Server *srv)
{
    int n = 0;
    time_t t_curr = purc_get_monotoic_time ();
    Endpoint *endpoint, *tmp;

    ULOG_INFO ("Checking no responding endpoints...\n");

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

            ULOG_INFO ("A no-responding client: %s\n", name);
        }
        else if (t_curr > endpoint->t_living + PCRDR_MAX_PING_TIME) {
            if (endpoint->type == ET_UNIX_SOCKET) {
                us_ping_client (srv->us_srv, (USClient *)endpoint->entity.client);
            }
            else if (endpoint->type == ET_WEB_SOCKET) {
                ws_ping_client (srv->ws_srv, (WSClient *)endpoint->entity.client);
            }

            ULOG_INFO ("Ping client: %s\n", name);
        }
        else {
            ULOG_INFO ("Skip left endpoints since (%s): %ld\n",
                    name, endpoint->t_living);
            break;
        }
    }

    ULOG_INFO ("Total endpoints removed: %d\n", n);
    return n;
}

int check_dangling_endpoints (Server *srv)
{
    int n = 0;
    time_t t_curr = purc_get_monotoic_time ();
    gs_list* node = srv->dangling_endpoints;

    while (node) {
        gs_list *next = node->next;
        Endpoint* endpoint = (Endpoint *)node->data;

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

int send_packet_to_endpoint (Server* srv,
        Endpoint* endpoint, const char* body, int len_body)
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

static int send_simple_response(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    size_t n;
    char buff [PCRDR_DEF_PACKET_BUFF_SIZE];

    n = pcrdr_serialize_message_to_buffer (msg, buff, sizeof(buff));
    if (n > sizeof(buff)) {
        ULOG_ERR ("The size of buffer for simple response packet is too small.\n");
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
    }
    else if (send_packet_to_endpoint (srv, endpoint, buff, n)) {
        endpoint->status = ES_CLOSING;
        retv = PCRDR_SC_IOERR;
    }

    return retv;
}

int send_initial_response (Server* srv, Endpoint* endpoint)
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

typedef int (*request_handler)(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg);

static int authenticate_endpoint(Server* srv, Endpoint* endpoint,
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
        ULOG_WARN ("Bad packet data for authentication: %s, %s, %s, %s\n",
                prot_name, host_name, app_name, runner_name);
        return PCRDR_SC_BAD_REQUEST;
    }

    if (prot_ver < PCRDR_PURCMC_MINIMAL_PROTOCOL_VERSION)
        return PCRDR_SC_UPGRADE_REQUIRED;

    if (!purc_is_valid_host_name (host_name) ||
            !purc_is_valid_app_name (app_name) ||
            !purc_is_valid_token (runner_name, PURC_LEN_RUNNER_NAME)) {
        ULOG_WARN ("Bad endpoint name: @%s/%s/%s\n",
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

    ULOG_INFO ("New endpoint: %s (%p)\n", endpoint_name, endpoint);

    if (kvlist_get (&srv->endpoint_list, endpoint_name)) {
        ULOG_WARN ("Duplicated endpoint: %s\n", endpoint_name);
        return PCRDR_SC_CONFLICT;
    }

    if (!make_endpoint_ready (srv, endpoint_name, endpoint)) {
        ULOG_ERR ("Failed to store the endpoint: %s\n", endpoint_name);
        return PCRDR_SC_INSUFFICIENT_STORAGE;
    }

    ULOG_INFO ("New endpoint stored: %s (%p), %d endpoints totally.\n",
            endpoint_name, endpoint, srv->nr_endpoints);

    endpoint->host_name = strdup (host_name);
    endpoint->app_name = strdup (app_name);
    endpoint->runner_name = strdup (runner_name);
    endpoint->status = ES_READY;

    return PCRDR_SC_OK;
}

static int on_start_session(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    SessionInfo *info = NULL;

    int retv = authenticate_endpoint(srv, endpoint, msg->data);

    endpoint->session_info = NULL;
    if (retv == PCRDR_SC_OK) {
        info = calloc(1, sizeof(SessionInfo));
        if (info == NULL) {
            retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        }
        else {
            kvlist_init(&info->wins, NULL);
            endpoint->session_info = info;
        }
    }

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)info;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_end_session(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;

    remove_session(endpoint);

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_create_plain_window(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    PlainWindow *win;
    pcrdr_msg response;

    const char* str = NULL;
    purc_variant_t tmp;

    if ((win = calloc(1, sizeof(*win))) == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_EJSON ||
            !purc_variant_is_object(msg->data)) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if ((tmp = purc_variant_object_get_by_ckey(msg->data, "id"))) {
        str = purc_variant_get_string_const(tmp);
        if (!purc_is_valid_identifier(str)) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        if (kvlist_get(&endpoint->session_info->wins, str)) {
            ULOG_WARN("Duplicated plain window: %s\n", str);
            retv = PCRDR_SC_CONFLICT;
            goto failed;
        }

        if (!kvlist_set(&endpoint->session_info->wins, str, &win)) {
            retv = PCRDR_SC_INSUFFICIENT_STORAGE;
            goto failed;
        }

        win->id = purc_variant_ref(tmp);
    }

    if ((tmp = purc_variant_object_get_by_ckey(msg->data, "title"))) {
        win->title = purc_variant_ref(tmp);
    }

failed:
    if (retv != PCRDR_SC_OK && win) {
        remove_window(endpoint, win);
        win = NULL;
    }

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)win;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_update_plain_window(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    const char *key;
    void *data;
    const char *element;
    PlainWindow *win;
    pcrdr_msg response;

    element = purc_variant_get_string_const(msg->element);
    if (element == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        unsigned long long int p;

        p = strtoull(element, NULL, 16);
        kvlist_for_each(&endpoint->session_info->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if ((uint64_t)p == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
    }
    else if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        void *data = kvlist_get(&endpoint->session_info->wins, element);

        if (data) {
            win = *(PlainWindow **)data;
        }
    }

    if (win == NULL) {
        ULOG_WARN("Specified plain window not found: %s\n", element);
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    const char *property;
    property = purc_variant_get_string_const(msg->property);
    if (property == NULL || strcmp(property, "title") ||
            msg->dataType != PCRDR_MSG_DATA_TYPE_TEXT) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (win->title)
        purc_variant_unref(win->title);
    win->title = purc_variant_ref(msg->data);
    dom_set_title(win->dom_doc, purc_variant_get_string_const(win->title));

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)win;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_destroy_plain_window(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    const char *key;
    void *data;
    const char *element;
    PlainWindow *win;
    pcrdr_msg response;

    element = purc_variant_get_string_const(msg->element);
    if (element == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        unsigned long long int p;

        p = strtoull(element, NULL, 16);
        kvlist_for_each(&endpoint->session_info->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if ((uint64_t)p == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
    }
    else if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        void *data = kvlist_get(&endpoint->session_info->wins, element);

        if (data) {
            win = *(PlainWindow **)data;
        }
    }

    if (win == NULL) {
        ULOG_WARN("Specified plain window not found: %s\n", element);
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    kvlist_delete(&endpoint->session_info->wins,
            purc_variant_get_string_const(win->id));
    remove_window(endpoint, win);

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)win;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_load(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    PlainWindow *win = NULL;

    pchtml_html_parser_t *parser = NULL;
    pchtml_html_document_t *html_doc = NULL;

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
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session_info->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (win == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

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

    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];
    assemble_endpoint_name (endpoint, endpoint_name);

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

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)html_doc;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_write_begin(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    PlainWindow *win = NULL;

    pchtml_html_parser_t *parser = NULL;
    pchtml_html_document_t *html_doc = NULL;

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
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session_info->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (win == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

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

failed:
    if (retv != PCRDR_SC_OK) {
        if (parser) {
            pchtml_html_parser_destroy(parser);
            win->parser = NULL;
        }
    }

    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)html_doc;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_write_more(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    PlainWindow *win = NULL;

    pchtml_html_parser_t *parser = NULL;

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
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session_info->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (win == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    if (win->parser == NULL || win->dom_doc == NULL) {
        retv = PCRDR_SC_PRECONDITION_FAILED;
        goto failed;
    }

    parser = win->parser;
    pchtml_html_parse_chunk_process(parser,
                (const unsigned char *)doc_text, doc_len);

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)(win ? win->dom_doc : 0);
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static int on_write_end(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    PlainWindow *win = NULL;

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
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session_info->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp) {
                win = tmp;
                break;
            }
        }
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (win == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

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

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = msg->requestId;
    response.retCode = retv;
    response.resultValue = (uint64_t)(win ? win->dom_doc : 0);
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return send_simple_response(srv, endpoint, &response);
}

static PlainWindow *check_dom_request_msg(Endpoint *endpoint,
        const pcrdr_msg *msg,
        int *retv, const char **doc_text, size_t *doc_len)
{
    PlainWindow *win = NULL;

    if (msg->target == PCRDR_MSG_TARGET_DOM && msg->targetValue != 0) {
        const char *key;
        void *data;

        kvlist_for_each(&endpoint->session_info->wins, key, data) {
            PlainWindow *tmp = *(PlainWindow **)data;
            if (msg->targetValue == (uint64_t)tmp->dom_doc) {
                win = tmp;
                break;
            }
        }

        if (win == NULL) {
            *retv = PCRDR_SC_NOT_FOUND;
            goto failed;
        }

        // any operation except `erase` and `clear` should have a text data.
        const char *op = purc_variant_get_string_const(msg->operation);
        if (strcmp(PCRDR_OPERATION_ERASE, op) &&
                strcmp(PCRDR_OPERATION_ERASE, op)) {
            if (msg->dataType != PCRDR_MSG_DATA_TYPE_TEXT ||
                    msg->data == PURC_VARIANT_INVALID) {
                *retv = PCRDR_SC_BAD_REQUEST;
                goto failed;
            }

            *doc_text = purc_variant_get_string_const_ex(msg->data, doc_len);
            if (*doc_text == NULL || *doc_len == 0) {
                *retv = PCRDR_SC_BAD_REQUEST;
                goto failed;
            }
        }
    }
    else {
        *retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    /* we are in writeBegin/writeMore operations */
    if (win->parser || win->dom_doc == NULL) {
        win = NULL;
        *retv = PCRDR_SC_PRECONDITION_FAILED;
        goto failed;
    }

    *retv = PCRDR_SC_OK;

failed:
    return win;
}

static pcdom_element_t **get_dom_element_by_handle(pcdom_document_t *dom_doc,
        const char *handle, size_t *nr_elements)
{
    uint64_t hval;
    char *endptr;
    pcdom_element_t *element;
    pcdom_element_t **elements;

    *nr_elements = 0;
    elements = malloc(sizeof(pcdom_element_t *));
    if (elements == NULL)
        return NULL;

    hval = strtoull(handle, &endptr, 16);
    if (endptr == handle) {
        goto done;
    }

    element = dom_get_element_by_handle(dom_doc, hval);
    if (element == NULL) {
        goto done;
    }

    *nr_elements = 1;
    elements[0] = element;

done:
    return elements;
}

static pcdom_element_t **get_dom_elements_by_handles(pcdom_document_t *dom_doc,
        const char *handles, size_t *nr_elements)
{
    pcdom_element_t **elements;
    size_t allocated = DEF_NR_HANDLES;

    *nr_elements = 0;
    elements = malloc(sizeof(pcdom_element_t *) * DEF_NR_HANDLES);
    while (*handles) {
        uint64_t hval;
        char *endptr;

        hval = (uint64_t)strtoull(handles, &endptr, 16);
        if (endptr == handles) {
            break;
        }

        elements[*nr_elements] = dom_get_element_by_handle(dom_doc, hval);
        if (elements[*nr_elements]) {
            (*nr_elements)++;

            if (*nr_elements > PCRDR_MAX_HANDLES) {
                goto done;
            }

            if (*nr_elements > allocated) {
                allocated += DEF_NR_HANDLES;
                elements = realloc(elements, sizeof(pcdom_element_t *) * allocated);
                if (elements == NULL)
                    return NULL;
            }
        }
        // skip not found

        handles = endptr;
        while (*handles) {
            if (isxdigit(*handles))
                break;
            handles++;
        }
    }

done:
    return elements;
}

static int operate_dom_element(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg, int op, pcrdr_msg *response)
{
    int retv;
    const char *doc_frag_text;
    size_t doc_frag_len;
    PlainWindow *win;
    pcdom_element_t **elements = NULL;
    size_t nr_elements;
    pcdom_node_t *subtree = NULL;

    win = check_dom_request_msg(endpoint, msg,
            &retv, &doc_frag_text, &doc_frag_len);
    if (win == NULL)
        goto failed;

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        elements = get_dom_element_by_handle(win->dom_doc,
                purc_variant_get_string_const(msg->element), &nr_elements);
    }
    else if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLES) {
        elements = get_dom_elements_by_handles(win->dom_doc,
                purc_variant_get_string_const(msg->element), &nr_elements);
    }

    if (elements == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }

    if (nr_elements == 0) {
        retv = PCRDR_SC_NOT_FOUND;
        goto failed;
    }

    if (op == PCRDR_K_OPERATION_ERASE) {
        const char *property;
        property = purc_variant_get_string_const(msg->property);
        if (property) {
            for (size_t n = 0; n < nr_elements; n++) {
                dom_remove_element_attr(win->dom_doc, elements[n], property);
            }
        }
        else {
            for (size_t n = 0; n < nr_elements; n++) {
                dom_erase_element(win->dom_doc, elements[n]);
            }
        }
    }
    else if (op == PCRDR_K_OPERATION_CLEAR) {
        for (size_t n = 0; n < nr_elements; n++) {
            dom_clear_element(win->dom_doc, elements[n]);
        }
    }
    else if (op == PCRDR_K_OPERATION_UPDATE) {
        const char *property;
        property = purc_variant_get_string_const(msg->property);
        if (property == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        for (size_t n = 0; n < nr_elements; n++) {
            dom_update_element(win->dom_doc, elements[n], property,
                    doc_frag_text, doc_frag_len);
        }
    }
    else {
        void (*dom_op)(pcdom_document_t *, pcdom_element_t *, pcdom_node_t *);
        pcdom_element_t *parent = NULL;

        switch (op) {
        case PCRDR_K_OPERATION_APPEND:
            dom_op = dom_append_subtree_to_element;
            parent = elements[0];
            break;

        case PCRDR_K_OPERATION_PREPEND:
            dom_op = dom_prepend_subtree_to_element;
            parent = elements[0];
            break;

        case PCRDR_K_OPERATION_INSERTBEFORE:
            dom_op = dom_insert_subtree_before_element;
            parent = pcdom_interface_element(
                    pcdom_node_parent(
                        pcdom_interface_node(elements[0])));
            break;

        case PCRDR_K_OPERATION_INSERTAFTER:
            dom_op = dom_insert_subtree_after_element;
            parent = pcdom_interface_element(
                    pcdom_node_parent(
                        pcdom_interface_node(elements[0])));
            break;

        case PCRDR_K_OPERATION_DISPLACE:
            dom_op = dom_displace_subtree_of_element;
            parent = elements[0];
            break;

        default:
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        subtree = dom_parse_fragment(win->dom_doc,
                parent, doc_frag_text, doc_frag_len);
        if (subtree == NULL) {
            retv = PCRDR_SC_UNPROCESSABLE_PACKET;
            goto failed;
        }

        for (size_t n = 1; n < nr_elements; n++) {
            pcdom_node_t *cloned_subtree;
            cloned_subtree = dom_clone_subtree(win->dom_doc, subtree, n);
            if (cloned_subtree == NULL) {
                retv = PCRDR_SC_INSUFFICIENT_STORAGE;
                goto failed;
            }

            dom_op(win->dom_doc, elements[n], cloned_subtree);
        }
        dom_op(win->dom_doc, elements[0], subtree);
        subtree = NULL;
    }

    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];
    assemble_endpoint_name (endpoint, endpoint_name);
    domview_reload_window_dom(endpoint_name,
        purc_variant_get_string_const(win->id), elements[0]);

failed:
    if (elements)
        free(elements);
    if (subtree)
        dom_destroy_subtree(subtree);

    response->type = PCRDR_MSG_TYPE_RESPONSE;
    response->requestId = msg->requestId;
    response->retCode = retv;
    response->resultValue = (uint64_t)(win ? win->dom_doc : 0);
    response->dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return retv;
}

static int on_append(Server* srv, Endpoint* endpoint, const pcrdr_msg *msg)
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

static int on_prepend(Server* srv, Endpoint* endpoint,
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

static int on_insert_after(Server* srv, Endpoint* endpoint,
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

static int on_insert_before(Server* srv, Endpoint* endpoint,
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

static int on_displace(Server* srv, Endpoint* endpoint,
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

static int on_clear(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_CLEAR, &response);
    return send_simple_response(srv, endpoint, &response);
}

static int on_erase(Server* srv, Endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response;
    operate_dom_element(srv, endpoint, msg,
            PCRDR_K_OPERATION_ERASE, &response);
    return send_simple_response(srv, endpoint, &response);
}

static int on_update(Server* srv, Endpoint* endpoint,
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

int on_got_message(Server* srv, Endpoint* endpoint, const pcrdr_msg *msg)
{
    if (msg->type == PCRDR_MSG_TYPE_REQUEST) {
        request_handler handler = find_request_handler(
                purc_variant_get_string_const(msg->operation));

        ULOG_INFO("Got a request message: %s (handler: %p)\n",
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
        ULOG_INFO("Got an event message: %s\n",
                purc_variant_get_string_const(msg->event));
    }
    else {
        // TODO
        ULOG_INFO("Got an unknown message: %d\n", msg->type);
    }

    return PCRDR_SC_OK;
}

