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

#undef NDEBUG

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "endpoint.h"
#include "unixsocket.h"
#include "websocket.h"
#include "utils/utils.h"

#if PLATFORM(MINIGUI)
#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>
bool mg_find_handle(void *session, uint64_t handle, void **data);
#endif

const char *purcmc_endpoint_host_name(purcmc_endpoint *endpoint)
{
    return endpoint->host_name;
}

const char *purcmc_endpoint_app_name(purcmc_endpoint *endpoint)
{
    return endpoint->app_name;
}

const char *purcmc_endpoint_runner_name(purcmc_endpoint *endpoint)
{
    return endpoint->runner_name;
}

purcmc_endpoint *purcmc_endpoint_from_name(purcmc_server *srv,
        const char *endpoint_name)
{
    void *data;
    data = kvlist_get(&srv->endpoint_list, endpoint_name);
    if (data == NULL)
        return NULL;

    return *(purcmc_endpoint **)data;
}

bool purcmc_endpoint_allow_switching_rdr(purcmc_endpoint *endpoint)
{
    return endpoint->allow_switching_rdr;
}

static int do_send_message(purcmc_server *srv,
        purcmc_endpoint *endpoint, const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    size_t n;
    char buff[PCRDR_DEF_PACKET_BUFF_SIZE];

    if (endpoint->status == ES_CLOSING)
        return PCRDR_SC_NOT_READY;

    n = pcrdr_serialize_message_to_buffer(msg, buff, sizeof(buff));
    if (n > sizeof(buff)) {
        purc_log_error("The size of buffer for the message is too small.\n");
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
    }
    else if (send_packet_to_endpoint(srv, endpoint, buff, n)) {
        endpoint->status = ES_CLOSING;
        retv = PCRDR_SC_IOERR;
    }

    return retv;
}

int purcmc_endpoint_send_response(purcmc_server* srv,
        purcmc_endpoint* endpoint, const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_WRONG_MSG;

    /*
     * Only send it if requestId was not PCRDR_REQUESTID_NORETURN;
     * otherwise discard the message.
     */
    if (msg->type == PCRDR_MSG_TYPE_RESPONSE) {

        if (strcmp(purc_variant_get_string_const(msg->requestId),
                    PCRDR_REQUESTID_NORETURN)) {
            retv = do_send_message(srv, endpoint, msg);
        }
        else {
            retv = PCRDR_SC_OK;
        }

        purc_variant_unref(msg->requestId);
        if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON) {
            purc_variant_unref(msg->data);
        }
    }

    return retv;
}

/* Post an event message to HVML interpreter */
int purcmc_endpoint_post_event(purcmc_server *srv,
        purcmc_endpoint *endpoint, const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_WRONG_MSG;

    if (msg->type == PCRDR_MSG_TYPE_EVENT) {
        purc_log_debug("%s: post an event...\n", __func__);

        retv = do_send_message(srv, endpoint, msg);

        if (msg->eventName)
            purc_variant_unref(msg->eventName);
        if (msg->sourceURI)
            purc_variant_unref(msg->sourceURI);
        if (msg->elementValue)
            purc_variant_unref(msg->elementValue);
        if (msg->property)
            purc_variant_unref(msg->property);
        if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON) {
            purc_variant_unref(msg->data);
        }
    }

    return retv;
}

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
    endpoint->allow_switching_rdr = true;

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

purcmc_endpoint* get_curr_endpoint (purcmc_server* srv)
{
    const char* name;
    void *next, *data;
    purcmc_endpoint *endpoint = NULL;
    purcmc_endpoint *p;
#if PLATFORM(MINIGUI)
    HWND hWnd = GetActiveWindow();
#endif

    if (kvlist_is_empty(&srv->endpoint_list)) {
        goto out;
    }

    kvlist_for_each_safe(&srv->endpoint_list, name, next, data) {
        p = *(purcmc_endpoint **)data;

        if (p->type == ET_BUILTIN) {
            continue;
        }

#if PLATFORM(MINIGUI)
        if (hWnd) {
            DWORD data = GetWindowAdditionalData(hWnd);
            if (data) {
                if (mg_find_handle(p->session, PTR2U64(data), NULL)) {
                    endpoint = p;
                    goto out;
                }
            }
        }
#endif
        if (endpoint == NULL) {
            endpoint = p;
        }
        else if (p->t_created > endpoint->t_created) {
            endpoint = p;
        }
    }

out:
    return endpoint;
}

int del_endpoint(purcmc_server* srv, purcmc_endpoint* endpoint, int cause)
{
    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];

    if (endpoint->session) {
        srv->cbs.remove_session(endpoint->session);
        endpoint->session = NULL;
    }

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
    if (endpoint->app_label) free (endpoint->app_label);
    if (endpoint->app_desc) free (endpoint->app_desc);
    if (endpoint->runner_label) free (endpoint->runner_label);
    if (endpoint->app_icon) free (endpoint->app_icon);
    if (endpoint->signature) free (endpoint->signature);
    if (endpoint->request_id) free (endpoint->request_id);

    free (endpoint);
    purc_log_warn ("purcmc_endpoint (%s) removed\n", endpoint_name);
    return 0;
}

bool store_dangling_endpoint(purcmc_server* srv, purcmc_endpoint* endpoint)
{
    if (srv->dangling_endpoints == NULL)
        srv->dangling_endpoints = gslist_create(endpoint);
    else
        srv->dangling_endpoints =
            gslist_insert_append(srv->dangling_endpoints, endpoint);

    if (srv->dangling_endpoints)
        return true;

    return false;
}

bool remove_dangling_endpoint(purcmc_server* srv, purcmc_endpoint* endpoint)
{
    gs_list* node = srv->dangling_endpoints;

    while (node) {
        if (node->data == endpoint) {
            gslist_remove_node(&srv->dangling_endpoints, node);
            return true;
        }

        node = node->next;
    }

    return false;
}

bool make_endpoint_ready(purcmc_server* srv,
        const char* endpoint_name, purcmc_endpoint* endpoint)
{
    if (remove_dangling_endpoint(srv, endpoint)) {
        if (!kvlist_set(&srv->endpoint_list, endpoint_name, &endpoint)) {
            purc_log_error ("Failed to store the endpoint: %s\n", endpoint_name);
            return false;
        }

        endpoint->t_living = purc_get_monotoic_time();
        endpoint->avl.key = endpoint;
        if (avl_insert(&srv->living_avl, &endpoint->avl)) {
            purc_log_error("Failed to insert to the living AVL tree: %s\n",
                    endpoint_name);
            assert(0);
            return false;
        }
        srv->nr_endpoints++;
    }
    else {
        purc_log_error("Not found endpoint in dangling list: %s\n",
                endpoint_name);
        return false;
    }

    return true;
}

static void cleanup_endpoint_client(purcmc_server *srv, purcmc_endpoint* endpoint)
{
    if (endpoint->type == ET_UNIX_SOCKET) {
        endpoint->entity.client->entity = NULL;
        us_cleanup_client(srv->us_srv, (USClient*)endpoint->entity.client);
    }
    else if (endpoint->type == ET_WEB_SOCKET) {
        endpoint->entity.client->entity = NULL;
        ws_cleanup_client(srv->ws_srv, (WSClient*)endpoint->entity.client);
    }

    purc_log_warn("The endpoint (@%s/%s/%s) client cleaned up\n",
            endpoint->host_name, endpoint->app_name, endpoint->runner_name);
}

int remove_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint)
{
    char name [PURC_LEN_ENDPOINT_NAME + 1];
    assemble_endpoint_name(endpoint, name);
    kvlist_delete(&srv->endpoint_list, name);
    cleanup_endpoint_client(srv, endpoint);
    purc_log_warn("------------remove_endpoint----------------\n");
    del_endpoint(srv, endpoint, CDE_NO_RESPONDING);
    srv->nr_endpoints--;
    return 0;
}

int check_no_responding_endpoints(purcmc_server *srv)
{
    int n = 0;
    time_t t_curr = purc_get_monotoic_time();
    purcmc_endpoint *endpoint, *tmp;

    purc_log_info("Checking no responding endpoints...\n");

    avl_for_each_element_safe(&srv->living_avl, endpoint, avl, tmp) {
        char name [PURC_LEN_ENDPOINT_NAME + 1];

        assert (endpoint->type != ET_BUILTIN);

        assemble_endpoint_name(endpoint, name);
        if (t_curr > endpoint->t_living + PCRDR_MAX_NO_RESPONDING_TIME) {

            kvlist_delete(&srv->endpoint_list, name);
            cleanup_endpoint_client(srv, endpoint);
            purc_log_warn("------------check_no_responding_endpoints----------------\n");
            del_endpoint(srv, endpoint, CDE_NO_RESPONDING);
            srv->nr_endpoints--;
            n++;

            purc_log_info("A no-responding client: %s\n", name);
        }
        else if (t_curr > endpoint->t_living + PCRDR_MAX_PING_TIME) {
            if (endpoint->type == ET_UNIX_SOCKET) {
                us_ping_client(srv->us_srv, (USClient *)endpoint->entity.client);
            }
            else if (endpoint->type == ET_WEB_SOCKET) {
                ws_ping_client(srv->ws_srv, (WSClient *)endpoint->entity.client);
            }

            purc_log_info("Ping client: %s\n", name);
        }
        else {
            purc_log_info("Skip left endpoints since (%s): %ld\n",
                    name, endpoint->t_living);
            break;
        }
    }

    purc_log_info("Total endpoints removed: %d\n", n);
    return n;
}

int check_dangling_endpoints(purcmc_server *srv)
{
    int n = 0;
    time_t t_curr = purc_get_monotoic_time();
    gs_list* node = srv->dangling_endpoints;

    char name [PURC_LEN_ENDPOINT_NAME + 1];
    while (node) {
        gs_list *next = node->next;
        purcmc_endpoint* endpoint = (purcmc_endpoint *)node->data;

        if (t_curr > endpoint->t_created + PCRDR_MAX_NO_RESPONDING_TIME) {
            gslist_remove_node(&srv->dangling_endpoints, node);
            cleanup_endpoint_client(srv, endpoint);
            assemble_endpoint_name(endpoint, name);
            purc_log_warn("------------check_dangling_endpoints----------------\n");
            del_endpoint(srv, endpoint, CDE_NO_RESPONDING);
            n++;
        }

        node = next;
    }

    return n;
}

int check_timeout_dangling_endpoints (purcmc_server *srv)
{
    int n = 0;
    time_t t_curr = purc_get_monotoic_time();
    gs_list* node = srv->dangling_endpoints;

    char name [PURC_LEN_ENDPOINT_NAME + 1];
    while (node) {
        gs_list *next = node->next;
        purcmc_endpoint* endpoint = (purcmc_endpoint *)node->data;

        if (t_curr > endpoint->t_start_session + endpoint->timeout_seconds) {
            assemble_endpoint_name(endpoint, name);
            purc_log_warn ("The endpoint (%s) startSession timeout (%lds)\n", name, endpoint->timeout_seconds);
            gslist_remove_node(&srv->dangling_endpoints, node);
            cleanup_endpoint_client(srv, endpoint);
            purc_log_warn("------------check_timeout_dangling_endpoints----------------\n");
            del_endpoint(srv, endpoint, CDE_NO_RESPONDING);
            n++;
        }


        node = next;
    }

    return n;
}

#define XGUIPRO_APP_NAME        "cn.fmsoft.hvml.xGUIPro"
#define KEY_CHALLENGECODE       "challengeCode"

char *gen_challenge_code()
{
    unsigned char ch_code_bin [PCUTILS_SHA256_DIGEST_SIZE];
    char key [32];
    char *ch_code = malloc(PCUTILS_SHA256_DIGEST_SIZE * 2 + 1);
    if (!ch_code) {
        goto out;
    }

    snprintf(key, sizeof (key), "xguipro-%ld", random ());

    pcutils_hmac_sha256 (ch_code_bin,
            (uint8_t*)XGUIPRO_APP_NAME, strlen(XGUIPRO_APP_NAME),
            (uint8_t*)key, strlen (key));
    pcutils_bin2hex (ch_code_bin, PCUTILS_SHA256_DIGEST_SIZE, ch_code, false);
    ch_code [PCUTILS_SHA256_DIGEST_SIZE * 2] = 0;

out:
    return ch_code;
}

int send_initial_response(purcmc_server* srv, purcmc_endpoint* endpoint)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg *msg = NULL;

    if (endpoint->type == CT_UNIX_SOCKET) {
        msg = pcrdr_make_response_message(PCRDR_REQUESTID_INITIAL, NULL,
                PCRDR_SC_OK, 0,
                PCRDR_MSG_DATA_TYPE_PLAIN, srv->features,
                strlen(srv->features));
    }
    else {
        char *challenge_code = gen_challenge_code();
        if (!challenge_code) {
            retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
            goto failed;
        }

        size_t nr_buf = strlen(srv->features) + strlen(challenge_code)
            + strlen(KEY_CHALLENGECODE) + 2; // : + \n
        char *buf = malloc(nr_buf + 1);
        sprintf(buf, "%s%s:%s\n", srv->features, KEY_CHALLENGECODE, challenge_code);
        msg = pcrdr_make_response_message(PCRDR_REQUESTID_INITIAL, NULL,
                PCRDR_SC_OK, 0,
                PCRDR_MSG_DATA_TYPE_PLAIN, buf, nr_buf);
        free(challenge_code);
        free(buf);
    }

    if (msg == NULL) {
        retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        goto failed;
    }

    msg->requestId = purc_variant_ref(msg->requestId);
    retv = purcmc_endpoint_send_response(srv, endpoint, msg);
    pcrdr_release_message(msg);

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

    purc_variant_t icon = purc_variant_object_get_by_ckey(data, "appIcon");
    purc_variant_t label = purc_variant_object_get_by_ckey(data, "appLabel");
    purc_variant_t desc = purc_variant_object_get_by_ckey(data, "appDesc");
    purc_variant_t runner_label = purc_variant_object_get_by_ckey(data, "runnerLabel");
    if (!label || !desc) {
        return PCRDR_SC_UNAUTHORIZED;
    }

    const char *s_label = purc_variant_get_string_const(label);
    const char *s_desc = purc_variant_get_string_const(desc);
    const char *s_icon = purc_variant_get_string_const(icon);
    const char *s_runner_label = purc_variant_get_string_const(runner_label);

    /* popup auth window  */
    if ((tmp = purc_variant_object_get_by_ckey(data, "signature"))) {
        bool confirm = xgutils_is_app_confirm(app_name);
        if (!confirm) {
            uint64_t ut = 0;
            purc_variant_t timeout = purc_variant_object_get_by_ckey(data,
                    "timeoutSeconds");
            if (timeout) {
                purc_variant_cast_to_ulongint(timeout, &ut, false);
            }

            if (ut == 0) {
                ut = 10;
            }

            int auth_ret = xgutils_show_confirm_window(app_name, s_label, s_desc,
                    s_icon, ut);
            if (auth_ret == CONFIRM_RESULT_ID_DECLINE) {
                return PCRDR_SC_UNAUTHORIZED;
            }
        }
    }

    tmp = purc_variant_object_get_by_ckey(data, "allowSwitchingRdr");
    if (tmp) {
        endpoint->allow_switching_rdr = purc_variant_booleanize(tmp);
    }
    else {
        endpoint->allow_switching_rdr = true;
    }

    tmp = purc_variant_object_get_by_ckey(data, "allowScalingByDensity");
    if (tmp) {
        endpoint->allow_scaling_by_density = purc_variant_booleanize(tmp);
    }
    else {
        endpoint->allow_scaling_by_density = false;
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
        host_name = norm_host_name;
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
    endpoint->app_label = s_label ? strdup(s_label) : NULL;
    endpoint->app_desc = s_desc ? strdup(s_desc) : NULL;
    endpoint->runner_label = s_runner_label ? strdup(s_runner_label) : NULL;
    endpoint->app_icon = s_icon ? strdup(s_icon) : NULL;
    endpoint->status = ES_READY;

    return PCRDR_SC_OK;
}

static int parse_session_data(purcmc_server* srv, purcmc_endpoint* endpoint,
        purc_variant_t data)
{
    const char* prot_name = NULL;
    const char *host_name = NULL, *app_name = NULL, *runner_name = NULL;
    uint64_t prot_ver = 0;
    char norm_host_name [PURC_LEN_HOST_NAME + 1];
    char norm_app_name [PURC_LEN_APP_NAME + 1];
    char norm_runner_name [PURC_LEN_RUNNER_NAME + 1];
    char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];
    char edp_name [PURC_LEN_ENDPOINT_NAME + 1];
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

    purc_variant_t icon = purc_variant_object_get_by_ckey(data, "appIcon");
    purc_variant_t label = purc_variant_object_get_by_ckey(data, "appLabel");
    purc_variant_t desc = purc_variant_object_get_by_ckey(data, "appDesc");
    purc_variant_t runner_label = purc_variant_object_get_by_ckey(data, "runnerLabel");
    if (!label || !desc) {
        return PCRDR_SC_UNAUTHORIZED;
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
        host_name = norm_host_name;
    }

    purc_assemble_endpoint_name (host_name,
                    app_name, runner_name, endpoint_name);

    purc_log_info ("New endpoint: %s (%p)\n", endpoint_name, endpoint);

    if (kvlist_get (&srv->endpoint_list, endpoint_name)) {
        purc_log_warn ("Duplicated endpoint: %s\n", endpoint_name);
        return PCRDR_SC_CONFLICT;
    }

    gs_list* node = srv->dangling_endpoints;
    while (node) {
        purcmc_endpoint* edp = (purcmc_endpoint *)node->data;
        assemble_endpoint_name(edp, edp_name);
        if (strcasecmp(endpoint_name, edp_name) == 0) {
            purc_log_warn ("Duplicated endpoint: %s\n", endpoint_name);
            return PCRDR_SC_CONFLICT;
        }
        node = node->next;
    }

    tmp = purc_variant_object_get_by_ckey(data, "signature");
    if (tmp) {
        const char *signature = purc_variant_get_string_const(tmp);
        if (signature && signature[0]) {
            endpoint->signature = strdup(signature);
        }
    }

    tmp = purc_variant_object_get_by_ckey(data, "timeoutSeconds");
    if (tmp) {
        purc_variant_cast_to_ulongint(tmp, &endpoint->timeout_seconds, false);
    }
    else {
        endpoint->timeout_seconds = 10;
    }

    tmp = purc_variant_object_get_by_ckey(data, "allowSwitchingRdr");
    if (tmp) {
        endpoint->allow_switching_rdr = purc_variant_booleanize(tmp);
    }
    else {
        endpoint->allow_switching_rdr = true;
    }

    tmp = purc_variant_object_get_by_ckey(data, "allowScalingByDensity");
    if (tmp) {
        endpoint->allow_scaling_by_density = purc_variant_booleanize(tmp);
    }
    else {
        endpoint->allow_scaling_by_density = false;
    }

    const char *s_label = purc_variant_get_string_const(label);
    const char *s_desc = purc_variant_get_string_const(desc);
    const char *s_icon = purc_variant_get_string_const(icon);
    const char *s_runner_label = purc_variant_get_string_const(runner_label);

    endpoint->host_name = strdup (host_name);
    endpoint->app_name = strdup (app_name);
    endpoint->runner_name = strdup (runner_name);
    endpoint->app_label = s_label ? strdup(s_label) : NULL;
    endpoint->app_desc = s_desc ? strdup(s_desc) : NULL;
    endpoint->runner_label = s_runner_label ? strdup(s_runner_label) : NULL;
    endpoint->app_icon = s_icon ? strdup(s_icon) : NULL;
    endpoint->status = ES_AUTHING;

    return PCRDR_SC_OK;
}

#define  AUTO_ACCEPT_FIRST_CONN
static int on_start_session_duplicate(purcmc_server* srv,
        purcmc_endpoint* endpoint, const pcrdr_msg *msg)
{
    int retv;
    char endpoint_name[PURC_LEN_ENDPOINT_NAME + 1];
    pcrdr_msg response = { };
    purcmc_session *info = NULL;

    retv = parse_session_data(srv, endpoint, msg->data);
    if (retv != PCRDR_SC_OK) {
        goto failed;
    }

#ifdef AUTO_ACCEPT_FIRST_CONN
    if (kvlist_is_empty(&srv->endpoint_list) && endpoint->type == ET_UNIX_SOCKET) {
        goto auto_accept;
    }
#endif

    /* wait for user accept */
    /* 1. save request id */
    endpoint->request_id = strdup(purc_variant_get_string_const(msg->requestId));
    endpoint->t_start_session = purc_get_monotoic_time();

    return PCRDR_SC_OK;


#ifdef AUTO_ACCEPT_FIRST_CONN
auto_accept:
#endif
    assemble_endpoint_name(endpoint, endpoint_name);
    if (!make_endpoint_ready (srv, endpoint_name, endpoint)) {
        purc_log_error ("Failed to store the endpoint: %s\n", endpoint_name);
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }

    endpoint->session = NULL;
    info = srv->cbs.create_session(srv, endpoint);
    if (info == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
    }
    else {
        endpoint->session = info;
        endpoint->t_created_session = purc_get_monotoic_time();
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)info;
    if (info && srv->srvcfg) {
        response.dataType = PCRDR_MSG_DATA_TYPE_JSON;
        response.data = purc_variant_make_object_0();
        purc_variant_t name = purc_variant_make_string(srv->srvcfg->name, true);
        if (name) {
            purc_variant_object_set_by_ckey(response.data, "name", name);
            purc_variant_unref(name);
        }
    }
    else {
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

int accept_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint)
{
    int retv = PCRDR_SC_OK;
    char endpoint_name[PURC_LEN_ENDPOINT_NAME + 1];
    pcrdr_msg response = { };
    purcmc_session *info = NULL;

    bool live = false;
    gs_list* node = srv->dangling_endpoints;
    while (node) {
        if (node->data == endpoint) {
            live = true;
        }
        node = node->next;
    }

    if (!live) {
        purc_log_error ("The endpoint: %p not exists\n", endpoint);
        return 0;
    }

    assemble_endpoint_name(endpoint, endpoint_name);
    if (!make_endpoint_ready (srv, endpoint_name, endpoint)) {
        purc_log_error ("Failed to store the endpoint: %s\n", endpoint_name);
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }

    endpoint->session = NULL;
    info = srv->cbs.create_session(srv, endpoint);
    if (info == NULL) {
        retv = PCRDR_SC_INSUFFICIENT_STORAGE;
    }
    else {
        endpoint->session = info;
        endpoint->t_created_session = purc_get_monotoic_time();
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_make_string(endpoint->request_id, false);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)info;
    if (info && srv->srvcfg) {
        response.dataType = PCRDR_MSG_DATA_TYPE_JSON;
        response.data = purc_variant_make_object_0();
        purc_variant_t name = purc_variant_make_string(srv->srvcfg->name, true);
        if (name) {
            purc_variant_object_set_by_ckey(response.data, "name", name);
            purc_variant_unref(name);
        }
    }
    else {
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_start_session(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response = { };
    purcmc_session *info = NULL;

    purc_variant_t tmp = purc_variant_object_get_by_ckey(msg->data,
            "duplicate");
    if (tmp) {
        endpoint->is_duplicate = purc_variant_booleanize(tmp);
    }
    else {
        endpoint->is_duplicate = false;
    }

    if (endpoint->is_duplicate) {
        return on_start_session_duplicate(srv, endpoint, msg);
    }

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
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)info;
    if (info && srv->srvcfg) {
        response.dataType = PCRDR_MSG_DATA_TYPE_JSON;
        response.data = purc_variant_make_object_0();
        purc_variant_t name = purc_variant_make_string(srv->srvcfg->name, true);
        if (name) {
            purc_variant_object_set_by_ckey(response.data, "name", name);
            purc_variant_unref(name);
        }
    }
    else {
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_end_session(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    if (endpoint->session) {
        srv->cbs.remove_session(endpoint->session);
        endpoint->session = NULL;
    }

#if 0
    /* conn is closed */
    pcrdr_msg response = { };
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = PCRDR_SC_OK;
    response.resultValue = 0;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
#else
    return PCRDR_SC_OK;
#endif
}

static int on_create_workspace(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response = { };
    purcmc_workspace* workspace = NULL;

    if (srv->cbs.create_workspace == NULL) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    const char* name = NULL;
    const char* title = NULL;
    purc_variant_t tmp;

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_JSON ||
            !purc_variant_is_object(msg->data)) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
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

    workspace = srv->cbs.create_workspace(endpoint->session,
            name, title, msg->data, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                workspace, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)workspace;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_update_workspace(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    pcrdr_msg response = { };

    if (srv->cbs.create_workspace == NULL || srv->cbs.update_workspace) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        const char *element = purc_variant_get_string_const(msg->elementValue);
        if (element == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        workspace = (purcmc_workspace *)(uintptr_t)handle;
        if (workspace == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    const char *property;
    property = purc_variant_get_string_const(msg->property);
    if (property == NULL ||
            !purc_is_valid_token(property, PURC_LEN_PROPERTY_NAME) ||
            msg->dataType != PCRDR_MSG_DATA_TYPE_PLAIN) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.update_workspace(endpoint->session, workspace, property,
            purc_variant_get_string_const(msg->data));
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                workspace, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)workspace;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_destroy_workspace(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    pcrdr_msg response = { };

    if (srv->cbs.create_workspace == NULL || srv->cbs.destroy_workspace) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        const char *element = purc_variant_get_string_const(msg->elementValue);
        if (element == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        workspace = (purcmc_workspace *)(uintptr_t)handle;
        if (workspace == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.destroy_workspace(endpoint->session, workspace);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                workspace, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)workspace;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_set_page_groups(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response = { };
    purcmc_workspace* workspace = NULL;

    if (srv->cbs.set_page_groups == NULL) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (((msg->dataType != PCRDR_MSG_DATA_TYPE_HTML) &&
                (msg->dataType != PCRDR_MSG_DATA_TYPE_PLAIN)) ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    const char *content;
    size_t length;
    content = purc_variant_get_string_const_ex(msg->data, &length);
    if (content == NULL || length == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.set_page_groups(endpoint->session, workspace,
            content, length);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                workspace, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)workspace;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_add_page_groups(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    pcrdr_msg response = { };

    if (srv->cbs.set_page_groups == NULL ||
            srv->cbs.add_page_groups == NULL) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (((msg->dataType != PCRDR_MSG_DATA_TYPE_HTML) &&
                (msg->dataType != PCRDR_MSG_DATA_TYPE_PLAIN)) ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    const char *content;
    size_t length;
    content = purc_variant_get_string_const_ex(msg->data, &length);
    if (content == NULL || length == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.add_page_groups(endpoint->session, workspace,
            content, length);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                workspace, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)workspace;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_remove_page_group(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    const char *gid;
    pcrdr_msg response = { };

    if (srv->cbs.set_page_groups == NULL ||
            srv->cbs.remove_page_group == NULL) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        gid = purc_variant_get_string_const(msg->elementValue);
        if (gid == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.remove_page_group(endpoint->session, workspace, gid);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                workspace, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)workspace;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_create_plain_window(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response = { };

    purcmc_workspace* workspace = NULL;
    purcmc_page* page = NULL;

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    /* Since PURCMC-120, use element to specify the window name and group name:
        <window_name>[@<group_name>]
     */
    const char *name_group = NULL;
    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        name_group = purc_variant_get_string_const(msg->elementValue);
    }

    if (name_group == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    char idbuf[PURC_MAX_WIDGET_ID];
    char name[PURC_LEN_IDENTIFIER + 1];
    const char *group;
    group = purc_check_and_make_plainwin_id(idbuf, name, name_group);
    if (group == PURC_INVPTR) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    /* Since PURCMC-120, support the special page name. */
    if (name[0] == '_') {    // reserved name
        int v = pcrdr_check_reserved_page_name(name);
        if (v < 0) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto done;
        }

        /* support for reserved name */
        if (srv->cbs.get_special_plainwin) {
            page = srv->cbs.get_special_plainwin(endpoint->session, workspace,
                    group, (pcrdr_resname_page_k)v);
            retv = PCRDR_SC_OK;
            goto done;
        }
    }

    page = srv->cbs.find_page(endpoint->session, workspace, idbuf);
    if (page) {
        retv = PCRDR_SC_OK;
        goto done;
    }

    purc_variant_t tmp;
    const char* class = NULL;
    const char* title = NULL;
    const char* layout_style = NULL;
    const char* transition_style = NULL;
    /* XXX: windowLevel should be defined in toolkitStyle.
    const char* window_level = NULL; */
    purc_variant_t toolkit_style = PURC_VARIANT_INVALID;

    if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON
           && purc_variant_is_object(msg->data)) {
        if ((tmp = purc_variant_object_get_by_ckey(msg->data, "class"))) {
            class = purc_variant_get_string_const(tmp);
        }

        if ((tmp = purc_variant_object_get_by_ckey(msg->data, "title"))) {
            title = purc_variant_get_string_const(tmp);
        }

        if ((tmp = purc_variant_object_get_by_ckey(msg->data,
                        "layoutStyle"))) {
            layout_style = purc_variant_get_string_const(tmp);
        }

        /* XXX: windowLevel should be defined in toolkitStyle
        if ((tmp = purc_variant_object_get_by_ckey(msg->data,
                        "windowLevel"))) {
            window_level = purc_variant_get_string_const(tmp);
        }*/

        toolkit_style =
            purc_variant_object_get_by_ckey(msg->data, "toolkitStyle");

        if ((tmp = purc_variant_object_get_by_ckey(msg->data,
                        "transitionStyle"))) {
            transition_style = purc_variant_get_string_const(tmp);
        }
    }
    const char *request_id = purc_variant_get_string_const(msg->requestId);
    page = srv->cbs.create_plainwin(endpoint->session, workspace,
            request_id, idbuf, group, name, class, title, layout_style,
            transition_style, toolkit_style, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                request_id, page, NULL);
        return PCRDR_SC_OK;
    }

done:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)page;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_update_plain_window(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    purcmc_page *page = NULL;
    pcrdr_msg response = { };

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->dataType == PCRDR_MSG_DATA_TYPE_VOID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        const char *element = purc_variant_get_string_const(msg->elementValue);
        if (element == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        page = (void *)(uintptr_t)handle;
    }
    else if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        const char *name_group = purc_variant_get_string_const(msg->elementValue);

        if (name_group == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        char idbuf[PURC_MAX_WIDGET_ID];
        char name[PURC_LEN_IDENTIFIER + 1];
        const char *group;
        group = purc_check_and_make_plainwin_id(idbuf, name, name_group);
        if (group == PURC_INVPTR) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        /* Since PURCMC-120, support the special page name. */
        if (name[0] == '_') {    // reserved name
            int v = pcrdr_check_reserved_page_name(name);
            if (v < 0) {
                retv = PCRDR_SC_BAD_REQUEST;
                goto failed;
            }

            /* support for reserved name */
            if (srv->cbs.get_special_plainwin) {
                page = srv->cbs.get_special_plainwin(endpoint->session, workspace,
                        group, (pcrdr_resname_page_k)v);
            }
        }
        else {
            page = srv->cbs.find_page(endpoint->session, workspace, idbuf);
        }
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    const char *property;
    property = purc_variant_get_string_const(msg->property);
    if (property == NULL && !purc_variant_is_object(msg->data)) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (property && !purc_is_valid_token(property, PURC_LEN_PROPERTY_NAME)){
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.update_plainwin(endpoint->session, workspace, page,
            property, msg->data);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                page, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)page;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_destroy_plain_window(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    purcmc_page *page = NULL;
    pcrdr_msg response = { };

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        const char *element = purc_variant_get_string_const(msg->elementValue);
        if (element == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        page = (purcmc_page *)(uintptr_t)handle;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.destroy_plainwin(endpoint->session, workspace, page);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                page, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)page;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_create_widget(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response = { };
    purcmc_workspace* workspace = NULL;
    purcmc_page* page = NULL;

    if (srv->cbs.create_widget == NULL) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto done;
    }

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    const char* name_group = NULL;
    /* Since PURCMC-120, use element to specify the widget name and group name:
            <widget_name>@<group_name>
     */
    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_ID) {
        name_group = purc_variant_get_string_const(msg->elementValue);
    }

    if (name_group == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    purc_log_info("name of widget to create: %s\n", name_group);

    char idbuf[PURC_MAX_WIDGET_ID];
    char name[PURC_LEN_IDENTIFIER + 1];
    const char *group = purc_check_and_make_widget_id(idbuf, name, name_group);
    if (group == NULL) {
        purc_log_warn("no group specified when creating widget\n");
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    purc_log_info("group of widget to create: %s\n", group);

    /* Since PURCMC-120, support the special page name. */
    if (name[0] == '_') {    // reserved name
        int v = pcrdr_check_reserved_page_name(name);
        if (v < 0) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto done;
        }

        if (srv->cbs.get_special_widget) {
            page = srv->cbs.get_special_widget(endpoint->session, workspace,
                    group, (pcrdr_resname_page_k)v);
            retv = PCRDR_SC_OK;
            goto done;
        }
    }

    /* Since PURCMC-120, returns the page if it exists already. */
    page = srv->cbs.find_page(endpoint->session, workspace, idbuf);
    if (page) {
        retv = PCRDR_SC_OK;
        goto done;
    }

    const char* class = NULL;
    const char* title = NULL;
    const char* layout_style = NULL;
    purc_variant_t toolkit_style = PURC_VARIANT_INVALID;

    if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON
           && purc_variant_is_object(msg->data)) {
        purc_variant_t tmp;

        if ((tmp = purc_variant_object_get_by_ckey(msg->data, "class"))) {
            class = purc_variant_get_string_const(tmp);
        }

        if ((tmp = purc_variant_object_get_by_ckey(msg->data, "title"))) {
            title = purc_variant_get_string_const(tmp);
        }

        if ((tmp = purc_variant_object_get_by_ckey(msg->data,
                        "layoutStyle"))) {
            layout_style = purc_variant_get_string_const(tmp);
        }

        toolkit_style =
            purc_variant_object_get_by_ckey(msg->data, "toolkitStyle");
    }

    const char *request_id = purc_variant_get_string_const(msg->requestId);
    page = srv->cbs.create_widget(endpoint->session, workspace,
            request_id, idbuf, group, name, class, title, layout_style,
            toolkit_style, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                request_id,
                page, NULL);
        return PCRDR_SC_OK;
    }

done:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)page;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_update_widget(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    purcmc_page *page = NULL;
    pcrdr_msg response = { };

    if (srv->cbs.create_widget == NULL || srv->cbs.update_widget == NULL) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        const char *element = purc_variant_get_string_const(msg->elementValue);
        if (element == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        page = (void *)(uintptr_t)handle;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    const char *property;
    property = purc_variant_get_string_const(msg->property);
    if (property == NULL ||
            !purc_is_valid_token(property, PURC_LEN_PROPERTY_NAME) ||
            msg->dataType == PCRDR_MSG_DATA_TYPE_VOID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.update_widget(endpoint->session, workspace,
            page, property, msg->data);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                page, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)page;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_destroy_widget(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    purcmc_workspace *workspace = NULL;
    purcmc_page *page = NULL;
    pcrdr_msg response = { };

    if (srv->cbs.create_widget == NULL || srv->cbs.destroy_widget == NULL) {
        retv = PCRDR_SC_NOT_IMPLEMENTED;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_WORKSPACE) {
        workspace = (void *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType == PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        const char *element = purc_variant_get_string_const(msg->elementValue);
        if (element == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        unsigned long long int handle;
        handle = strtoull(element, NULL, 16);
        page = (void *)(uintptr_t)handle;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    retv = srv->cbs.destroy_widget(endpoint->session, workspace, page);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                page, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)page;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

#define LEN_BUFF_LONGLONGINT 128

static int on_load(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response = { };
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    purcmc_page *page = NULL;
    purcmc_udom *dom = NULL;
    char suppressed[LEN_BUFF_LONGLONGINT] = { };

    if (((msg->dataType != PCRDR_MSG_DATA_TYPE_HTML) &&
                (msg->dataType != PCRDR_MSG_DATA_TYPE_PLAIN)) ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    doc_text = purc_variant_get_string_const_ex(msg->data, &doc_len);
    if (doc_text == NULL || doc_len == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW ||
            msg->target == PCRDR_MSG_TARGET_WIDGET) {
        page = (void *)(uintptr_t)msg->targetValue;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    /* Since PURCMC-120, pass the coroutine handle */
    if (msg->elementType != PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    uint64_t crtn = (uint64_t)strtoull(
            purc_variant_get_string_const(msg->elementValue), NULL, 16);
    dom = srv->cbs.load(endpoint->session, page,
            PCRDR_K_OPERATION_LOAD, PCRDR_OPERATION_LOAD,
            purc_variant_get_string_const(msg->requestId),
            doc_text, doc_len, crtn, suppressed, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, page,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                dom, suppressed);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)dom;
    /* Since PURCMC-120, return the coroutine handle which is suppressed */
    if (suppressed[0]) {
        response.dataType = PCRDR_MSG_DATA_TYPE_PLAIN;
        response.data = purc_variant_make_string(suppressed, false);
    }
    else {
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_load_from_url(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response = { };
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    purcmc_page *page = NULL;
    purcmc_udom *dom = NULL;
    char suppressed[LEN_BUFF_LONGLONGINT] = { };

    if ((msg->dataType != PCRDR_MSG_DATA_TYPE_PLAIN) ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    doc_text = purc_variant_get_string_const_ex(msg->data, &doc_len);
    if (doc_text == NULL || doc_len == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW ||
            msg->target == PCRDR_MSG_TARGET_WIDGET) {
        page = (void *)(uintptr_t)msg->targetValue;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    /* Since PURCMC-120, pass the coroutine handle */
    if (msg->elementType != PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    uint64_t crtn = (uint64_t)strtoull(
            purc_variant_get_string_const(msg->elementValue), NULL, 16);
    dom = srv->cbs.load_from_url(endpoint->session, page,
            PCRDR_K_OPERATION_LOADFROMURL, PCRDR_OPERATION_LOADFROMURL,
            purc_variant_get_string_const(msg->requestId),
            doc_text, doc_len, crtn, suppressed, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, page,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                dom, suppressed);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)dom;
    /* Since PURCMC-120, return the coroutine handle which is suppressed */
    if (suppressed[0]) {
        response.dataType = PCRDR_MSG_DATA_TYPE_PLAIN;
        response.data = purc_variant_make_string(suppressed, false);
    }
    else {
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static inline int write_xxx(purcmc_server* srv, purcmc_endpoint* endpoint,
        int op, const char* op_name, const pcrdr_msg *msg)
{
    pcrdr_msg response = { };
    int retv = PCRDR_SC_OK;
    const char *doc_text;
    size_t doc_len;
    purcmc_page *page = NULL;
    purcmc_udom *dom = NULL;
    uint64_t crtn = 0;
    char suppressed[LEN_BUFF_LONGLONGINT] = { };

    if (((msg->dataType != PCRDR_MSG_DATA_TYPE_HTML) &&
                (msg->dataType != PCRDR_MSG_DATA_TYPE_PLAIN)) ||
            msg->data == PURC_VARIANT_INVALID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    doc_text = purc_variant_get_string_const_ex(msg->data, &doc_len);
    if (doc_text == NULL || doc_len == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW ||
            msg->target == PCRDR_MSG_TARGET_WIDGET) {
        page = (void *)(uintptr_t)msg->targetValue;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    /* Since PURCMC-120, pass the coroutine handle */
    if (op == PCRDR_K_OPERATION_WRITEBEGIN) {
        if (msg->elementType != PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        crtn = (uint64_t)strtoull(
                purc_variant_get_string_const(msg->elementValue), NULL, 16);
    }

    dom = srv->cbs.write(endpoint->session, page, op, op_name,
            purc_variant_get_string_const(msg->requestId),
            doc_text, doc_len, crtn, suppressed, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, page,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                dom, suppressed);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)dom;
    /* Since PURCMC-120, return the coroutine handle which is suppressed */
    if (suppressed[0]) {
        response.dataType = PCRDR_MSG_DATA_TYPE_PLAIN;
        response.data = purc_variant_make_string(suppressed, false);
    }
    else {
        response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    }


    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_write_begin(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return write_xxx(srv, endpoint,
            PCRDR_K_OPERATION_WRITEBEGIN,
            PCRDR_OPERATION_WRITEBEGIN,
            msg);
}

static int on_write_more(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return write_xxx(srv, endpoint,
            PCRDR_K_OPERATION_WRITEMORE,
            PCRDR_OPERATION_WRITEMORE,
            msg);
}

static int on_write_end(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return write_xxx(srv, endpoint,
            PCRDR_K_OPERATION_WRITEEND,
            PCRDR_OPERATION_WRITEEND,
            msg);
}

static int on_register(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response = { };
    int retv = PCRDR_SC_OK;
    purcmc_page *page = 0;
    uint64_t suppressed = 0;

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW ||
            msg->target == PCRDR_MSG_TARGET_WIDGET) {
        page = (void *)(uintptr_t)msg->targetValue;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType != PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    uint64_t crtn = (uint64_t)strtoull(
            purc_variant_get_string_const(msg->elementValue), NULL, 16);
    if (crtn == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    suppressed = srv->cbs.register_crtn(endpoint->session, page, crtn, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, page,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                page, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = suppressed;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_revoke(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    pcrdr_msg response = { };
    int retv = PCRDR_SC_OK;
    purcmc_page *page = 0;
    uint64_t to_reload = 0;

    if (msg->target == PCRDR_MSG_TARGET_PLAINWINDOW ||
            msg->target == PCRDR_MSG_TARGET_WIDGET) {
        page = (void *)(uintptr_t)msg->targetValue;
    }

    if (page == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->elementType != PCRDR_MSG_ELEMENT_TYPE_HANDLE) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    uint64_t crtn = (uint64_t)strtoull(
            purc_variant_get_string_const(msg->elementValue), NULL, 16);
    if (crtn == 0) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    to_reload = srv->cbs.revoke_crtn(endpoint->session, page, crtn, &retv);
    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, page,
                purc_variant_get_string_const(msg->operation),
                purc_variant_get_string_const(msg->requestId),
                page, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = to_reload;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int update_dom(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg, int op, const char *op_name)
{
    int retv;
    purcmc_udom *dom = NULL;
    pcrdr_msg response = { };

    if (msg->target == PCRDR_MSG_TARGET_DOM) {
        dom = (purcmc_udom *)(uintptr_t)msg->targetValue;
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    if (dom == NULL) {
        retv = PCRDR_SC_NOT_FOUND;
        goto done;
    }

    const char *content = NULL;
    size_t content_len = 0;
    if (op != PCRDR_K_OPERATION_ERASE && op != PCRDR_K_OPERATION_CLEAR) {
        if (msg->dataType == PCRDR_MSG_DATA_TYPE_JSON ||
                msg->dataType == PCRDR_MSG_DATA_TYPE_VOID ||
                msg->data == PURC_VARIANT_INVALID) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto done;
        }

        content = purc_variant_get_string_const_ex(msg->data, &content_len);
        if (content == NULL || content_len == 0) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto done;
        }
    }

    const char *element_type = NULL;
    switch (msg->elementType) {
        case PCRDR_MSG_ELEMENT_TYPE_HANDLE:
            element_type = "handle";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_HANDLES:
            element_type = "handles";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_ID:
            element_type = "id";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CLASS:
            element_type = "class";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_TAG:
            element_type = "tag";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CSS:
            element_type = "css";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_XPATH:
            element_type = "xpath";
            break;
        default:
            element_type = "void";
            break;
    }

    const char *element_value = purc_variant_get_string_const(msg->elementValue);
    if (element_type == NULL || element_value == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto done;
    }

    const char *request_id = purc_variant_get_string_const(msg->requestId);
    retv = srv->cbs.update_dom(endpoint->session, dom,
            op, op_name, request_id,
            element_type, element_value,
            purc_variant_get_string_const(msg->property),
            msg->dataType, content, content_len);
    if (retv == 0) {
        // Check if requestId is `noreturn`
        if (strcmp(request_id, PCRDR_REQUESTID_NORETURN)) {
            srv->cbs.pend_response(endpoint->session, (purcmc_page *)dom,
                    purc_variant_get_string_const(msg->operation),
                    request_id, dom, NULL);
        }
        return PCRDR_SC_OK;
    }

done:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = (uint64_t)(uintptr_t)dom;
    response.dataType = PCRDR_MSG_DATA_TYPE_VOID;
    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_append(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_APPEND,
            PCRDR_OPERATION_APPEND);
}

static int on_prepend(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_PREPEND,
            PCRDR_OPERATION_PREPEND);
}

static int on_insert_after(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_INSERTAFTER,
            PCRDR_OPERATION_INSERTAFTER);
}

static int on_insert_before(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_INSERTBEFORE,
            PCRDR_OPERATION_INSERTBEFORE);
}

static int on_displace(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_DISPLACE,
            PCRDR_OPERATION_DISPLACE);
}

static int on_clear(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_CLEAR,
            PCRDR_OPERATION_CLEAR);
}

static int on_erase(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_ERASE,
            PCRDR_OPERATION_ERASE);
}

static int on_update(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    return update_dom(srv, endpoint, msg,
            PCRDR_K_OPERATION_UPDATE,
            PCRDR_OPERATION_UPDATE);
}

static int on_call_method(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response = { };
    purc_variant_t result = PURC_VARIANT_INVALID;

    const char *request_id = purc_variant_get_string_const(msg->requestId);
    const char *method = NULL;
    purc_variant_t arg = PURC_VARIANT_INVALID;

    if (msg->dataType != PCRDR_MSG_DATA_TYPE_JSON) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if ((arg = purc_variant_object_get_by_ckey(msg->data, "method"))) {
        method = purc_variant_get_string_const(arg);
    }
    if (method == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    arg = purc_variant_object_get_by_ckey(msg->data, "arg");

    const char *element_type = NULL;
    switch (msg->elementType) {
        case PCRDR_MSG_ELEMENT_TYPE_HANDLE:
            element_type = "handle";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_HANDLES:
            element_type = "handles";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_ID:
            element_type = "id";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CLASS:
            element_type = "class";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_TAG:
            element_type = "tag";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CSS:
            element_type = "css";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_XPATH:
            element_type = "xpath";
            break;
        default:
            element_type = "void";
            break;
    }

    const char *element_value;
    element_value = purc_variant_get_string_const(msg->elementValue);

    if (msg->target == PCRDR_MSG_TARGET_DOM) {
        if (srv->cbs.call_method_in_dom == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        purcmc_udom *dom = NULL;
        dom = (purcmc_udom *)(uintptr_t)msg->targetValue;
        if (dom == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        result = srv->cbs.call_method_in_dom(endpoint->session, request_id,
                dom, element_type, element_value,
                method, arg, &retv);

    }
    else if (msg->target < PCRDR_MSG_TARGET_DOM) {
        if (srv->cbs.call_method_in_session == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        result = srv->cbs.call_method_in_session(endpoint->session,
                msg->target, msg->targetValue,
                element_type, element_value,
                purc_variant_get_string_const(msg->property),
                method, arg, &retv);
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (retv == 0) {
        // Check if requestId is `noreturn`
        if (strcmp(request_id, PCRDR_REQUESTID_NORETURN)) {
            srv->cbs.pend_response(endpoint->session, NULL,
                    purc_variant_get_string_const(msg->operation),
                    request_id,
                    (void *)(uintptr_t)msg->targetValue, NULL);
        }
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = msg->targetValue;
    response.dataType = result ?
        PCRDR_MSG_DATA_TYPE_JSON : PCRDR_MSG_DATA_TYPE_VOID;
    response.data = result;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_get_property(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response = { };
    purc_variant_t result = PURC_VARIANT_INVALID;

    const char *request_id = purc_variant_get_string_const(msg->requestId);

    const char *element_type = NULL;
    switch (msg->elementType) {
        case PCRDR_MSG_ELEMENT_TYPE_HANDLE:
            element_type = "handle";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_HANDLES:
            element_type = "handles";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_ID:
            element_type = "id";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CLASS:
            element_type = "class";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_TAG:
            element_type = "tag";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CSS:
            element_type = "css";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_XPATH:
            element_type = "xpath";
            break;
        default:
            element_type = "void";
            break;
    }

    const char *element_value;
    element_value = purc_variant_get_string_const(msg->elementValue);

    const char *property;
    property = purc_variant_get_string_const(msg->property);
    if (property == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_DOM) {
        if (srv->cbs.get_property_in_dom == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        purcmc_udom *dom = NULL;
        dom = (purcmc_udom *)(uintptr_t)msg->targetValue;
        if (dom == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        result = srv->cbs.get_property_in_dom(endpoint->session, request_id,
                dom, element_type, element_value,
                property, &retv);
    }
    else if (msg->target < PCRDR_MSG_TARGET_DOM) {
        if (srv->cbs.get_property_in_session == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        result = srv->cbs.get_property_in_session(endpoint->session,
                msg->target, msg->targetValue,
                element_type, element_value,
                property, &retv);
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (retv == 0) {
        srv->cbs.pend_response(endpoint->session, NULL,
                purc_variant_get_string_const(msg->operation),
                request_id,
                (void *)(uintptr_t)msg->targetValue, NULL);
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = msg->targetValue;
    response.dataType = result ?
        PCRDR_MSG_DATA_TYPE_JSON : PCRDR_MSG_DATA_TYPE_VOID;
    response.data = result;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static int on_set_property(purcmc_server* srv, purcmc_endpoint* endpoint,
        const pcrdr_msg *msg)
{
    int retv = PCRDR_SC_OK;
    pcrdr_msg response = { };
    purc_variant_t result = PURC_VARIANT_INVALID;

    const char *request_id = purc_variant_get_string_const(msg->requestId);

    if (msg->dataType == PCRDR_MSG_DATA_TYPE_VOID) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    const char *element_type = NULL;
    switch (msg->elementType) {
        case PCRDR_MSG_ELEMENT_TYPE_HANDLE:
            element_type = "handle";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_HANDLES:
            element_type = "handles";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_ID:
            element_type = "id";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CLASS:
            element_type = "class";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_TAG:
            element_type = "tag";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_CSS:
            element_type = "css";
            break;
        case PCRDR_MSG_ELEMENT_TYPE_XPATH:
            element_type = "xpath";
            break;
        default:
            element_type = "void";
            break;
    }

    const char *element_value;
    element_value = purc_variant_get_string_const(msg->elementValue);

    const char *property;
    property = purc_variant_get_string_const(msg->property);
    if (property == NULL) {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (msg->target == PCRDR_MSG_TARGET_DOM) {
        if (srv->cbs.set_property_in_dom == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        purcmc_udom *dom = NULL;
        dom = (purcmc_udom *)(uintptr_t)msg->targetValue;
        if (dom == NULL) {
            retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        result = srv->cbs.set_property_in_dom(endpoint->session, request_id,
                dom, element_type, element_value,
                property, msg->data, &retv);

    }
    else if (msg->target < PCRDR_MSG_TARGET_DOM) {
        if (srv->cbs.set_property_in_session == NULL) {
            retv = PCRDR_SC_NOT_IMPLEMENTED;
            goto failed;
        }

        result = srv->cbs.set_property_in_session(endpoint->session,
                msg->target, msg->targetValue,
                element_type, element_value,
                property, msg->data, &retv);
    }
    else {
        retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (retv == 0) {
        // Check if requestId is `noreturn`
        if (strcmp(request_id, PCRDR_REQUESTID_NORETURN)) {
            srv->cbs.pend_response(endpoint->session, NULL,
                    purc_variant_get_string_const(msg->operation),
                    request_id,
                    (void *)(uintptr_t)msg->targetValue, NULL);
        }
        return PCRDR_SC_OK;
    }

failed:
    response.type = PCRDR_MSG_TYPE_RESPONSE;
    response.requestId = purc_variant_ref(msg->requestId);
    response.sourceURI = PURC_VARIANT_INVALID;
    response.retCode = retv;
    response.resultValue = msg->targetValue;
    response.dataType = result ?
        PCRDR_MSG_DATA_TYPE_JSON : PCRDR_MSG_DATA_TYPE_VOID;
    response.data = result;

    return purcmc_endpoint_send_response(srv, endpoint, &response);
}

static struct request_handler {
    const char *operation;
    request_handler handler;
} handlers[] = {
    { PCRDR_OPERATION_ADDPAGEGROUPS, on_add_page_groups },
    { PCRDR_OPERATION_APPEND, on_append },
    { PCRDR_OPERATION_CALLMETHOD, on_call_method },
    { PCRDR_OPERATION_CLEAR, on_clear },
    { PCRDR_OPERATION_CREATEPLAINWINDOW, on_create_plain_window },
    { PCRDR_OPERATION_CREATEWIDGET, on_create_widget },
    { PCRDR_OPERATION_CREATEWORKSPACE, on_create_workspace },
    { PCRDR_OPERATION_DESTROYPLAINWINDOW, on_destroy_plain_window },
    { PCRDR_OPERATION_DESTROYWIDGET, on_destroy_widget },
    { PCRDR_OPERATION_DESTROYWORKSPACE, on_destroy_workspace },
    { PCRDR_OPERATION_DISPLACE, on_displace },
    { PCRDR_OPERATION_ENDSESSION, on_end_session },
    { PCRDR_OPERATION_ERASE, on_erase },
    { PCRDR_OPERATION_GETPROPERTY, on_get_property },
    { PCRDR_OPERATION_INSERTAFTER, on_insert_after },
    { PCRDR_OPERATION_INSERTBEFORE, on_insert_before },
    { PCRDR_OPERATION_LOAD, on_load },
    { PCRDR_OPERATION_LOADFROMURL, on_load_from_url },
    { PCRDR_OPERATION_PREPEND, on_prepend },
    { PCRDR_OPERATION_REGISTER, on_register },
    { PCRDR_OPERATION_REMOVEPAGEGROUP, on_remove_page_group },
    { PCRDR_OPERATION_REVOKE, on_revoke },
    { PCRDR_OPERATION_SETPAGEGROUPS, on_set_page_groups },
    { PCRDR_OPERATION_SETPROPERTY, on_set_property },
    { PCRDR_OPERATION_STARTSESSION, on_start_session },
    { PCRDR_OPERATION_UPDATE, on_update },
    { PCRDR_OPERATION_UPDATEPLAINWINDOW, on_update_plain_window },
    { PCRDR_OPERATION_UPDATEWIDGET, on_update_widget },
    { PCRDR_OPERATION_UPDATEWORKSPACE, on_update_workspace },
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
            pcrdr_msg response = { };
            response.type = PCRDR_MSG_TYPE_RESPONSE;
            response.requestId = purc_variant_ref(msg->requestId);
            response.sourceURI = PURC_VARIANT_INVALID;
            response.retCode = PCRDR_SC_BAD_REQUEST;
            response.resultValue = 0;
            response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

            return purcmc_endpoint_send_response(srv, endpoint, &response);
        }
        else if (handler) {
            return handler(srv, endpoint, msg);
        }
        else {
            pcrdr_msg response = { };
            response.type = PCRDR_MSG_TYPE_RESPONSE;
            response.requestId = purc_variant_ref(msg->requestId);
            response.sourceURI = PURC_VARIANT_INVALID;
            response.retCode = PCRDR_SC_NOT_IMPLEMENTED;
            response.resultValue = 0;
            response.dataType = PCRDR_MSG_DATA_TYPE_VOID;

            return purcmc_endpoint_send_response(srv, endpoint, &response);
        }
    }
    else if (msg->type == PCRDR_MSG_TYPE_EVENT) {
        // TODO
        purc_log_info("Got an event message: %s\n",
                purc_variant_get_string_const(msg->eventName));
    }
    else {
        // TODO
        purc_log_info("Got an unknown message: %d\n", msg->type);
    }

    return PCRDR_SC_OK;
}

