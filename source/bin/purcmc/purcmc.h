/*
** purcmc.h -- The module interface for PurCMC server.
**
** Copyright (C) 2022 FMSoft (http://www.fmsoft.cn)
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

#ifndef XGUIPRO_PURCMC_PURCMC_H
#define XGUIPRO_PURCMC_PURCMC_H

#include <purc/purc-pcrdr.h>

/* The PurcMC Server */
struct purcmc_server;
typedef struct purcmc_server purcmc_server;

/* The PurcMC endpoint */
struct purcmc_endpoint;
typedef struct purcmc_endpoint purcmc_endpoint;

/* The PurcMC session */
struct purcmc_session;
typedef struct purcmc_session purcmc_session;

/* The PurcMC plain window */
struct purcmc_plainwin;
typedef struct purcmc_plainwin purcmc_plainwin;

/* The PurcMC page */
struct purcmc_page;
typedef struct purcmc_page purcmc_page;

/* The PurcMC dom */
struct purcmc_dom;
typedef struct purcmc_dom purcmc_dom;

/* Config Options */
typedef struct purcmc_server_config {
    const char* app_name;
    const char* runner_name;

    int nowebsocket;
    int accesslog;
    int use_ssl;
    char *unixsocket;
    char *origin;
    char *addr;
    char *port;
    char *sslcert;
    char *sslkey;
    int max_frm_size;
    int backlog;
} purcmc_server_config;

typedef struct purcmc_server_callbacks {
    purcmc_session *(*create_session)(purcmc_server *, purcmc_endpoint *);
    int (*remove_session)(purcmc_server *, purcmc_session *);

    purcmc_plainwin *(*create_plainwin)(purcmc_server *, purcmc_session *,
            const char *gid, const char *name,
            const char *title, const char *style);
    purcmc_plainwin *(*get_plainwin_by_handle)(purcmc_server *, purcmc_session *,
            uint64_t handle);
    purcmc_plainwin *(*get_plainwin_by_id)(purcmc_server *, purcmc_session *,
            const char* id);

    int (*update_plainwin)(purcmc_server *, purcmc_plainwin *,
            const char *property, const char *value);
    int (*remove_plainwin)(purcmc_server *, purcmc_plainwin *);

    purcmc_page *(*get_plainwin_page)(purcmc_server *, purcmc_plainwin *);

    purcmc_dom *(*load)(purcmc_server *, purcmc_page *,
            const char *content, size_t length);

    purcmc_dom *(*write_begin)(purcmc_server *, purcmc_page *,
            const char *content, size_t length);

    purcmc_dom *(*write_more)(purcmc_server *, purcmc_page *,
            const char *content, size_t length);

    purcmc_dom *(*write_end)(purcmc_server *, purcmc_page *,
            const char *content, size_t length);

    purcmc_dom *(*get_dom_by_handle)(purcmc_server *, purcmc_session *,
            uint64_t handle);
    int (*operate_dom_element)(purcmc_server *, purcmc_dom *,
            int op, const pcrdr_msg *msg);
} purcmc_server_callbacks;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the PurCMC renderer server */
purcmc_server *purcmc_rdrsrv_init(purcmc_server_config* srvcfg);

/* Check and dispatch messages from clients */
int purcmc_rdrsrv_check(purcmc_server *srv);

/* Deinitialize the PurCMC renderer server */
int purcmc_rdrsrv_deinit(purcmc_server *srv);

/* Return the host name of the specified endpoint */
const char *purcmc_endpt_host_name(purcmc_endpoint *endpt);

/* Return the app name of the specified endpoint */
const char *purcmc_endpt_app_name(purcmc_endpoint *endpt);

/* Return the runner name of the specified endpoint */
const char *purcmc_endpt_runner_name(purcmc_endpoint *endpt);

#ifdef __cplusplus
}
#endif

#endif /* !XGUIPRO_PURCMC_PURCMC_H*/

