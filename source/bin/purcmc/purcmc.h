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

/* The PurcMC workspace */
struct purcmc_workspace;
typedef struct purcmc_workspace purcmc_workspace;

/* The PurcMC page (a plainwin or widget) */
struct purcmc_page;
typedef struct purcmc_page purcmc_page;

/* The PurcMC dom */
struct purcmc_udom;
typedef struct purcmc_udom purcmc_udom;

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
    int  (*prepare)(purcmc_server *);
    void (*cleanup)(purcmc_server *);

    purcmc_session *(*create_session)(purcmc_server *, purcmc_endpoint *);
    int (*remove_session)(purcmc_session *);

    /* nullable */
    purcmc_workspace *(*create_workspace)(purcmc_session *,
            const char *name, const char *title, purc_variant_t properties,
            int *retv);
    /* null if create_workspace is null */
    int (*update_workspace)(purcmc_session *, purcmc_workspace *,
            const char *property, const char *value);
    /* null if create_workspace is null */
    int (*destroy_workspace)(purcmc_session *, purcmc_workspace *);
    /* Since PURMC-120; null if create_workspace is null */
    purcmc_workspace *(*find_workspace)(purcmc_session *sess,
            const char *name);
    /* Since PURMC-120; null if create_workspace is null */
    purcmc_workspace *(*get_special_workspace)(purcmc_session *sess,
            pcrdr_resname_workspace_k v);

    /* nullable */
    int (*set_page_groups)(purcmc_session *, purcmc_workspace *,
            const char *content, size_t length);
    /* null if set_page_groups is null */
    int (*add_page_groups)(purcmc_session *, purcmc_workspace *,
            const char *content, size_t length);
    /* null if set_page_groups is null */
    int (*remove_page_group)(purcmc_session *, purcmc_workspace *,
            const char* gid);

    /* Since PURMC-120 */
    purcmc_page *(*find_page)(purcmc_session *sess,
            purcmc_workspace *workspace, const char *page_id);

    purcmc_page *(*create_plainwin)(purcmc_session *, purcmc_workspace *,
            const char *request_id,
            const char *page_id, const char *group, const char *name,
            const char *klass, const char *title, const char *layout_style,
            const char *window_level, purc_variant_t toolkit_style, int *retv);
    int (*update_plainwin)(purcmc_session *, purcmc_workspace *,
            purcmc_page *page, const char *property, purc_variant_t value);
    int (*destroy_plainwin)(purcmc_session *, purcmc_workspace *,
            purcmc_page *page);
    /* Since PURMC-120; nullable */
    purcmc_page *(*get_special_plainwin)(purcmc_session *sess,
            purcmc_workspace *workspace, const char *group,
            pcrdr_resname_page_k v);

    /* nullable */
    purcmc_page *(*create_widget)(purcmc_session *, purcmc_workspace *,
            const char *request_id,
            const char *page_id, const char *group, const char *name,
            const char *klass, const char *title, const char *layout_style,
            purc_variant_t toolkit_style, int *retv);
    /* null if create_widget is null */
    int (*update_widget)(purcmc_session *, purcmc_workspace *,
            purcmc_page *page, const char *property, purc_variant_t value);
    /* null if create_widget is null */
    int (*destroy_widget)(purcmc_session *, purcmc_workspace *,
            purcmc_page *page);
    /* Since PURMC-120; null if create_widget is null */
    purcmc_page *(*get_special_widget)(purcmc_session *sess,
            purcmc_workspace *workspace, const char *group,
            pcrdr_resname_page_k v);

    purcmc_udom *(*load)(purcmc_session *, purcmc_page *,
            int op, const char *op_name, const char *request_id,
            const char *content, size_t length,
            uint64_t crtn, char *suppressed, int *retv);
    purcmc_udom *(*write)(purcmc_session *, purcmc_page *,
            int op, const char *op_name, const char *request_id,
            const char *content, size_t length,
            uint64_t crtn, char *suppressed, int *retv);

    /* Since PURMC-120 */
    uint64_t (*register_crtn)(purcmc_session *sess, purcmc_page *,
            uint64_t crtn, int *retv);
    /* Since PURMC-120 */
    uint64_t (*revoke_crtn)(purcmc_session *sess, purcmc_page *,
            uint64_t crtn, int *retv);

    int (*update_dom)(purcmc_session *, purcmc_udom *,
            int op, const char *op_name, const char *request_id,
            const char* element_type, const char* element_value,
            const char* property, pcrdr_msg_data_type text_type,
            const char *content, size_t length);

    /* nullable */
    purc_variant_t (*call_method_in_session)(purcmc_session *,
            pcrdr_msg_target target, uint64_t target_value,
            const char *element_type, const char *element_value,
            const char *property, const char *method, purc_variant_t arg,
            int* retv);
    /* nullable */
    purc_variant_t (*call_method_in_dom)(purcmc_session *, const char *,
            purcmc_udom *, const char* element_type, const char* element_value,
            const char *method, purc_variant_t arg, int* retv);

    /* nullable */
    purc_variant_t (*get_property_in_session)(purcmc_session *,
            pcrdr_msg_target target, uint64_t target_value,
            const char *element_type, const char *element_value,
            const char *property, int *retv);
    /* nullable */
    purc_variant_t (*get_property_in_dom)(purcmc_session *, const char *,
            purcmc_udom *, const char* element_type, const char* element_value,
            const char *property, int *retv);

    /* nullable */
    purc_variant_t (*set_property_in_session)(purcmc_session *,
            pcrdr_msg_target target, uint64_t target_value,
            const char *element_type, const char *element_value,
            const char *property, purc_variant_t value, int *retv);
    /* nullable */
    purc_variant_t (*set_property_in_dom)(purcmc_session *, const char *,
            purcmc_udom *, const char* element_type, const char* element_value,
            const char *property, purc_variant_t value, int *retv);

    bool (*pend_response)(purcmc_session *, purcmc_page *, const char *operation,
            const char *request_id, void *result_value, const char *plain);
} purcmc_server_callbacks;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the PurCMC renderer server */
purcmc_server *purcmc_rdrsrv_init(purcmc_server_config* srvcfg,
        void *user_data, const purcmc_server_callbacks *cbs,
        const char *markup_langs,
        int nr_workspaces, int nr_plainwindows,
        int nr_tabbedwindows, int nr_tabbedpages);

/* Check and dispatch messages from clients */
bool purcmc_rdrsrv_check(purcmc_server *srv);

/* Deinitialize the PurCMC renderer server */
int purcmc_rdrsrv_deinit(purcmc_server *srv);

/* retrieve the user data attached to the renederer server */
void *purcmc_rdrsrv_get_user_data(purcmc_server *srv);

/* retrieve the endpoint by endpoint name */
purcmc_endpoint *purcmc_endpoint_from_name(purcmc_server *srv,
        const char *endpoint_name);

/* Send a response message to HVML interpreter */
int purcmc_endpoint_send_response(purcmc_server *srv,
        purcmc_endpoint *endpoint, const pcrdr_msg *msg);

/* Post an event message to HVML interpreter */
int purcmc_endpoint_post_event(purcmc_server *srv,
        purcmc_endpoint *endpoint, const pcrdr_msg *msg);

/* Return the host name of the specified endpoint */
const char *purcmc_endpoint_host_name(purcmc_endpoint *endpoint);

/* Return the app name of the specified endpoint */
const char *purcmc_endpoint_app_name(purcmc_endpoint *endpoint);

/* Return the runner name of the specified endpoint */
const char *purcmc_endpoint_runner_name(purcmc_endpoint *endpoint);

#ifdef __cplusplus
}
#endif

#endif /* !XGUIPRO_PURCMC_PURCMC_H*/

