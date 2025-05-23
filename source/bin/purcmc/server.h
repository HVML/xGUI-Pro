/*
** server.h -- The internal interface for renderer server.
**
** Copyright (C) 2020 ~ 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
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

#ifndef XGUIPRO_PURCMC_SERVER_H
#define XGUIPRO_PURCMC_SERVER_H

#include <config.h>
#include <time.h>

#include <unistd.h>
#if HAVE(SYS_EPOLL_H)
#include <sys/epoll.h>
#elif HAVE(SYS_SELECT_H)
#include <sys/select.h>
#else
#error no `epoll` either `select` found.
#endif

#include <purc/purc-pcrdr.h>

#include "utils/list.h"
#include "utils/kvlist.h"
#include "utils/gslist.h"
#include "utils/sorted-array.h"

#include "purcmc.h"
#include "xguipro-version.h"

#define SERVER_APP_NAME     "cn.fmsoft.hvml.renderer"
#define SERVER_RUNNER_NAME  "purcmc"
#define XGUIPRO_APP_NAME    "cn.fmsoft.hvml.xGUIPro"

#define SERVER_FEATURES_FORMAT \
    PCRDR_PURCMC_PROTOCOL_NAME ":" PCRDR_PURCMC_PROTOCOL_VERSION_STRING "\n" \
    XGUIPRO_APP_NAME ":" XGUIPRO_VERSION_STRING "\n" \
    "%s\n" \
    "workspace:%d/tabbedWindow:%d/widgetInTabbedWindow:%d/plainWindow:%d\n" \
    "locale:%s\n" \
    "displayDensity:%s\n" \

/* max clients for each web socket and unix socket */
#define MAX_CLIENTS_EACH    512

/* 1 MiB throttle threshold per client */
#define SOCK_THROTTLE_THLD  (1024 * 1024)

/* packet body types */
enum {
    PT_TEXT = 0,
    PT_BINARY,
};

/* purcmc_endpoint types */
enum {
    ET_BUILTIN = 0,
    ET_UNIX_SOCKET,
    ET_WEB_SOCKET,
};

/* purcmc_endpoint status */
enum {
    ES_AUTHING = 0,     // authenticating
    ES_CLOSING,         // force to close the endpoint due to the failed authentication,
                        // RPC timeout, or ping-pong timeout.
    ES_READY,           // the endpoint is ready.
    ES_BUSY,            // the endpoint is busy for a call to procedure.
};

struct SockClient_;

/* A upper entity */
typedef struct UpperEntity_ {
    /* the size of memory used by the socket layer */
    size_t                  sz_sock_mem;

    /* the peak size of memory used by the socket layer */
    size_t                  peak_sz_sock_mem;

    /* the pointer to the socket client */
    struct SockClient_     *client;
} UpperEntity;

static inline void update_upper_entity_stats (UpperEntity *entity,
        size_t sz_pending_data, size_t sz_reading_data)
{
    if (entity) {
        entity->sz_sock_mem = sz_pending_data + sz_reading_data;
        if (entity->sz_sock_mem > entity->peak_sz_sock_mem)
            entity->peak_sz_sock_mem = entity->sz_sock_mem;
    }
}

/* A socket client */
typedef struct SockClient_ {
    /* the connection type of the socket */
    int                     ct;

    /* the file descriptor of the socket */
    int                     fd;

    /* time got the first frame of the current reading packet/message */
    struct timespec         ts;

    /* the pointer to the upper entity */
    struct UpperEntity_    *entity;
} SockClient;

/* A PurcMC purcmc_endpoint */
struct purcmc_endpoint
{
    int             type;
    unsigned int    status;
    UpperEntity     entity;

    time_t  t_created;
    time_t  t_living;
    time_t  t_start_session;
    time_t  t_created_session;
    uint64_t timeout_seconds;

    char*   host_name;
    char*   app_name;
    char*   runner_name;
    char*   app_label;
    char*   app_desc;
    char*   runner_label;
    char*   app_icon;
    char*   signature;

    /* start session request id , used for duplicate renderer*/
    char*   request_id;

    purcmc_session *session;

    /* AVL node for the AVL tree sorted by living time */
    struct avl_node avl;

    bool allow_switching_rdr;
    bool allow_scaling_by_density;
    bool is_duplicate;
};

struct dnssd_rdr {
    char *hostname;
    uint16_t port;
    char *text_record;
    uint16_t nr_text_record;
    time_t  last_update_at; // monotoic_time_ms
};

struct WSServer_;
struct USServer_;

/* The PurcMC purcmc_server */
struct purcmc_server
{
    char *features;
    int us_listener;
    int ws_listener;
#if HAVE(SYS_EPOLL_H)
    int epollfd;
#elif HAVE(SYS_SELECT_H)
    int maxfd;
    fd_set rfdset, wfdset;
    /* the AVL tree for the map from fd to client */
    struct sorted_array *fd2clients;
#endif
    unsigned int nr_endpoints;
    bool running;

    time_t t_start;
    time_t t_elapsed;
    time_t t_elapsed_last;

    char* server_name;

    struct WSServer_ *ws_srv;
    struct USServer_ *us_srv;

#if PCA_ENABLE_DNSSD
    struct purc_dnssd_conn *dnssd;
    void                   *registed_handle;
#if ENABLE(DNSSD_BROWSING)
    void                   *browsing_handle;
    unsigned int            browsing_timer_id;
#endif /* ENABLE(DNSSD_BROWSING) */
#endif /* PCA_ENABLE_DNSSD */

    /* The KV list using endpoint name as the key, and purcmc_endpoint* as the value */
    struct kvlist endpoint_list;

    /* The KV list using dnssd host:port as the key, and struct dnssd_rdr* as the value */
    struct kvlist dnssd_rdr_list;

    /* The accepted endpoints but waiting for authentification */
    gs_list *dangling_endpoints;

    /* the AVL tree of endpoints sorted by living time */
    struct avl_tree living_avl;

    /* The session list */
    gs_list *sess_list;

    /* the user data */
    void *user_data;

    purcmc_server_config *srvcfg;

    purc_variant_t confirm_infos;

    /* the callbacks */
    purcmc_server_callbacks cbs;
};

#endif /* !XGUIPRO_PURCMC_SERVER_H */

