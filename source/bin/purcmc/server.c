/*
** server.c -- the renderer server.
**
** Copyright (c) 2020 ~ 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of PurC Midnight Commander.
**
** PurcMC is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** PurcMC is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <purc/purc.h>
#include <glib.h>

#include "utils/kvlist.h"

#include "server.h"
#include "websocket.h"
#include "unixsocket.h"
#include "endpoint.h"

static PurCMCServer the_server;
static PurCMCServerConfig srvcfg;

#define PTR_FOR_US_LISTENER ((void *)1)
#define PTR_FOR_WS_LISTENER ((void *)2)

/* callbacks for socket servers */
// Allocate a PurCMCEndpoint structure for a new client and send `auth` packet.
static int
on_accepted (void* sock_srv, SockClient* client)
{
    PurCMCEndpoint* endpoint;

    (void)sock_srv;
    endpoint = new_endpoint (&the_server,
            (client->ct == CT_WEB_SOCKET) ? ET_WEB_SOCKET : ET_UNIX_SOCKET,
            client);

    if (endpoint == NULL)
        return PCRDR_SC_INSUFFICIENT_STORAGE;

    // send initial response
    return send_initial_response (&the_server, endpoint);
}

static int
on_packet (void* sock_srv, SockClient* client,
            char* body, unsigned int sz_body, int type)
{
    assert (client->entity);

    (void)sock_srv;
    (void)client;

    if (type == PT_TEXT) {
        int ret;
        pcrdr_msg *msg;
        PurCMCEndpoint *endpoint = container_of (client->entity, PurCMCEndpoint, entity);

        if (srvcfg.accesslog) {
            purc_log_info ("Got a packet from @%s/%s/%s:\n%s\n",
                    endpoint->host_name, endpoint->app_name,
                    endpoint->runner_name, body);
        }

        if ((ret = pcrdr_parse_packet (body, sz_body, &msg))) {
            purc_log_error ("Failed pcrdr_parse_packet: %s\n",
                    purc_get_error_message (ret));
            return PCRDR_SC_UNPROCESSABLE_PACKET;
        }

        ret = on_got_message (&the_server, endpoint, msg);
        pcrdr_release_message (msg);
        return ret;
    }
    else {
        /* discard all packet in binary */
        return PCRDR_SC_NOT_ACCEPTABLE;
    }

    return PCRDR_SC_OK;
}

#if !HAVE(SYS_EPOLL_H) && HAVE(SYS_SELECT_H)
static int
listen_new_client(int fd, void *ptr, bool rw)
{
    if (sorted_array_add (the_server.fd2clients, (void *)(intptr_t)fd, ptr)) {
        return -1;
    }

    FD_SET (fd, &the_server.rfdset);
    if (rw)
        FD_SET (fd, &the_server.wfdset);

    if (the_server.maxfd < fd)
        the_server.maxfd = fd;

    return 0;
}

static int
remove_listening_client (int fd)
{
    if (sorted_array_remove (the_server.fd2clients, (void *)(intptr_t)fd)) {
        FD_CLR (fd, &the_server.rfdset);
        FD_CLR (fd, &the_server.wfdset);
        return 0;
    }

    return -1;
}
#endif

static int
on_pending (void* sock_srv, SockClient* client)
{
#if HAVE(SYS_EPOLL_H)
    struct epoll_event ev;

    (void)sock_srv;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.ptr = client;
    if (epoll_ctl (the_server.epollfd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
        purc_log_error ("Failed epoll_ctl to the client fd (%d): %s\n",
                client->fd, strerror (errno));
        assert (0);
    }
#elif HAVE(SYS_SELECT_H)
    (void)sock_srv;

    if (listen_new_client (client->fd, client, TRUE)) {
        purc_log_error ("Failed to insert to the client AVL tree: %d\n", client->fd);
        assert (0);
    }
#endif

    return 0;
}

static int
on_close (void* sock_srv, SockClient* client)
{
#if HAVE(SYS_EPOLL_H)
    (void)sock_srv;

    if (epoll_ctl (the_server.epollfd, EPOLL_CTL_DEL, client->fd, NULL) == -1) {
        purc_log_warn ("Failed to call epoll_ctl to delete the client fd (%d): %s\n",
                client->fd, strerror (errno));
    }
#elif HAVE(SYS_SELECT_H)
    (void)sock_srv;

    if (remove_listening_client (client->fd)) {
        purc_log_warn ("Failed to delete the client fd (%d) from the listening fdset\n",
                client->fd);
    }
#endif

    if (client->entity) {
        PurCMCEndpoint *endpoint = container_of (client->entity, PurCMCEndpoint, entity);
        char endpoint_name [PURC_LEN_ENDPOINT_NAME + 1];

        if (assemble_endpoint_name (endpoint, endpoint_name) > 0) {
            if (kvlist_delete (&the_server.endpoint_list, endpoint_name)) {
                the_server.nr_endpoints--;
                purc_log_info ("An authenticated endpoint removed: %s (%p), %d endpoints left.\n",
                        endpoint_name, endpoint, the_server.nr_endpoints);
            }
        }
        else {
            remove_dangling_endpoint (&the_server, endpoint);
            purc_log_info ("An endpoint not authenticated removed: (%p, %d), %d endpoints left.\n",
                    endpoint, endpoint->status, the_server.nr_endpoints);
        }

        del_endpoint (&the_server, endpoint, CDE_LOST_CONNECTION);

        client->entity = NULL;
    }

    return 0;
}

static void
on_error (void* sock_srv, SockClient* client, int err_code)
{
    int n;
    char buff [PCRDR_MIN_PACKET_BUFF_SIZE];

    if (err_code == PCRDR_SC_IOERR)
        return;

    n = snprintf (buff, sizeof (buff),
            "type:response\n"
            "requestId:0\n"
            "result:%d/0\n"
            "dataType:text\n"
            "dataLen:%d\n"
            " \n"
            "%s\n",
            err_code,
            (int)sizeof (SERVER_FEATURES) - 1, SERVER_FEATURES);

    if (n < 0 || (size_t)n >= sizeof (buff)) {
        // should never reach here
        assert (0);
    }

    if (client->ct == CT_UNIX_SOCKET) {
        us_send_packet (sock_srv, (USClient *)client, US_OPCODE_TEXT, buff, n);
    }
    else {
        ws_send_packet (sock_srv, (WSClient *)client, WS_OPCODE_TEXT, buff, n);
    }
}

static inline void
update_endpoint_living_time (PurCMCServer *srv, PurCMCEndpoint* endpoint)
{
    if (endpoint && endpoint->avl.key) {
        time_t t_curr = purc_get_monotoic_time ();

        if (endpoint->t_living != t_curr) {
            endpoint->t_living = t_curr;
            avl_delete (&srv->living_avl, &endpoint->avl);
            avl_insert (&srv->living_avl, &endpoint->avl);
        }
    }
}

static struct sigaction old_pipe_sa;

static void
sig_handler_ignore (int sig_number)
{
    if (sig_number == SIGINT) {
        the_server.running = false;
    }
    else if (sig_number == SIGPIPE) {
        if (old_pipe_sa.sa_handler) {
            old_pipe_sa.sa_handler (sig_number);
        }
    }
}

static int
setup_signal_pipe (void)
{
    struct sigaction sa;

    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = sig_handler_ignore;
    sigemptyset (&sa.sa_mask);

    if (sigaction (SIGPIPE, &sa, &old_pipe_sa) != 0) {
        perror ("sigaction()");
        return -1;
    }

    return 0;
}

/* max events for epoll */
#define MAX_EVENTS          10

static void
prepare_server (void)
{
#if HAVE(SYS_EPOLL_H)
    struct epoll_event ev;
#endif
    the_server.us_listener = the_server.ws_listener = -1;
    the_server.t_start = purc_get_monotoic_time ();
    the_server.t_elapsed = the_server.t_elapsed_last = 0;

    // create unix socket
    if ((the_server.us_listener = us_listen (the_server.us_srv)) < 0) {
        purc_log_error ("Unable to listen on Unix socket (%s)\n",
                srvcfg.unixsocket);
        goto error;
    }
    purc_log_info ("Listening on Unix Socket (%s)...\n", srvcfg.unixsocket);

    the_server.us_srv->on_accepted = on_accepted;
    the_server.us_srv->on_packet = on_packet;
    the_server.us_srv->on_pending = on_pending;
    the_server.us_srv->on_close = on_close;
    the_server.us_srv->on_error = on_error;

    // create web socket listener if enabled
    if (the_server.ws_srv) {
#if HAVE(LIBSSL)
        if (srvcfg.sslcert && srvcfg.sslkey) {
            purc_log_info ("==Using TLS/SSL==\n");
            srvcfg.use_ssl = 1;
            if (ws_initialize_ssl_ctx (the_server.ws_srv)) {
                purc_log_error ("Unable to initialize_ssl_ctx\n");
                goto error;
            }
        }
#else
        srvcfg.sslcert = srvcfg.sslkey = NULL;
#endif

        if ((the_server.ws_listener = ws_listen (the_server.ws_srv)) < 0) {
            purc_log_error ("Unable to listen on Web socket (%s, %s)\n",
                    srvcfg.addr, srvcfg.port);
            goto error;
        }

        the_server.ws_srv->on_accepted = on_accepted;
        the_server.ws_srv->on_packet = on_packet;
        the_server.ws_srv->on_pending = on_pending;
        the_server.ws_srv->on_close = on_close;
        the_server.ws_srv->on_error = on_error;
    }
    purc_log_info ("Listening on Web Socket (%s, %s) %s SSL...\n",
            srvcfg.addr, srvcfg.port, srvcfg.sslcert ? "with" : "without");

#if HAVE(SYS_EPOLL_H)
    the_server.epollfd = epoll_create1 (EPOLL_CLOEXEC);
    if (the_server.epollfd == -1) {
        purc_log_error ("Failed to call epoll_create1: %s\n", strerror (errno));
        goto error;
    }

    ev.events = EPOLLIN;
    ev.data.ptr = PTR_FOR_US_LISTENER;
    if (epoll_ctl (the_server.epollfd, EPOLL_CTL_ADD, the_server.us_listener, &ev) == -1) {
        purc_log_error ("Failed to call epoll_ctl with us_listener (%d): %s\n",
                the_server.us_listener, strerror (errno));
        goto error;
    }

    if (the_server.ws_listener >= 0) {
        ev.events = EPOLLIN;
        ev.data.ptr = PTR_FOR_WS_LISTENER;
        if (epoll_ctl (the_server.epollfd, EPOLL_CTL_ADD, the_server.ws_listener, &ev) == -1) {
            purc_log_error ("Failed to call epoll_ctl with ws_listener (%d): %s\n",
                    the_server.ws_listener, strerror (errno));
            goto error;
        }
    }
#elif HAVE(SYS_SELECT_H)
    listen_new_client (the_server.us_listener, PTR_FOR_US_LISTENER, FALSE);
    if (the_server.ws_listener >= 0) {
        listen_new_client (the_server.ws_listener, PTR_FOR_WS_LISTENER, FALSE);
    }
#endif

error:
    return;
}

#if HAVE(SYS_EPOLL_H)
void check_server_on_idle (void *data, void *info)
{
    int nfds, n;
    struct epoll_event ev, events[MAX_EVENTS];

    (void)data;

again:
    nfds = epoll_wait (the_server.epollfd, events, MAX_EVENTS, 0);
    if (nfds < 0) {
        if (errno == EINTR) {
            goto again;
        }

        purc_log_error ("Failed to call epoll_wait: %s\n", strerror (errno));
        goto error;
    }
    else if (nfds == 0) {
        the_server.t_elapsed = purc_get_monotoic_time () - the_server.t_start;
        if (the_server.t_elapsed != the_server.t_elapsed_last) {
            if (the_server.t_elapsed % 10 == 0) {
                check_no_responding_endpoints (&the_server);
            }
            else if (the_server.t_elapsed % 5 == 0) {
                check_dangling_endpoints (&the_server);
            }

            the_server.t_elapsed_last = the_server.t_elapsed;
        }
    }

    for (n = 0; n < nfds; ++n) {
        if (events[n].data.ptr == PTR_FOR_US_LISTENER) {
            USClient * client = us_handle_accept (the_server.us_srv);
            if (client == NULL) {
                purc_log_info ("Refused a client\n");
            }
            else {
                ev.events = EPOLLIN; /* do not use EPOLLET */
                ev.data.ptr = client;
                if (epoll_ctl (the_server.epollfd,
                            EPOLL_CTL_ADD, client->fd, &ev) == -1) {
                    purc_log_error ("Failed epoll_ctl for connected unix socket (%d): %s\n",
                            client->fd, strerror (errno));
                    goto error;
                }
            }
        }
        else if (events[n].data.ptr == PTR_FOR_WS_LISTENER) {
            WSClient * client = ws_handle_accept (the_server.ws_srv,
                    the_server.ws_listener);
            if (client == NULL) {
                purc_log_info ("Refused a client\n");
            }
            else {
                ev.events = EPOLLIN; /* do not use EPOLLET */
                ev.data.ptr = client;
                if (epoll_ctl(the_server.epollfd,
                            EPOLL_CTL_ADD, client->fd, &ev) == -1) {
                    purc_log_error ("Failed epoll_ctl for connected web socket (%d): %s\n",
                            client->fd, strerror (errno));
                    goto error;
                }
            }
        }
        else {
            USClient *usc = (USClient *)events[n].data.ptr;
            if (usc->ct == CT_UNIX_SOCKET) {

                if (events[n].events & EPOLLIN) {

                    if (usc->entity) {
                        PurCMCEndpoint *endpoint = container_of (usc->entity,
                                PurCMCEndpoint, entity);
                        update_endpoint_living_time (&the_server, endpoint);
                    }

                    us_handle_reads (the_server.us_srv, usc);
                }

                if (events[n].events & EPOLLOUT) {
                    us_handle_writes (the_server.us_srv, usc);

                    if (!(usc->status & US_SENDING) && !(usc->status & US_CLOSE)) {
                        ev.events = EPOLLIN;
                        ev.data.ptr = usc;
                        if (epoll_ctl (the_server.epollfd,
                                    EPOLL_CTL_MOD, usc->fd, &ev) == -1) {
                            purc_log_error ("Failed epoll_ctl for unix socket (%d): %s\n",
                                    usc->fd, strerror (errno));
                            goto error;
                        }
                    }
                }
            }
            else if (usc->ct == CT_WEB_SOCKET) {
                WSClient *wsc = (WSClient *)events[n].data.ptr;

                if (events[n].events & EPOLLIN) {
                    if (wsc->entity) {
                        PurCMCEndpoint *endpoint = container_of (usc->entity,
                                PurCMCEndpoint, entity);
                        update_endpoint_living_time (&the_server, endpoint);
                    }

                    ws_handle_reads (the_server.ws_srv, wsc);
                }

                if (events[n].events & EPOLLOUT) {
                    ws_handle_writes (the_server.ws_srv, wsc);

                    if (!(wsc->status & WS_SENDING) && !(wsc->status & WS_CLOSE)) {
                        ev.events = EPOLLIN;
                        ev.data.ptr = wsc;
                        if (epoll_ctl (the_server.epollfd,
                                    EPOLL_CTL_MOD, wsc->fd, &ev) == -1) {
                            purc_log_error ("Failed epoll_ctl for web socket (%d): %s\n",
                                    usc->fd, strerror (errno));
                            goto error;
                        }
                    }
                }
            }
            else {
                purc_log_error ("Bad socket type (%d): %s\n",
                        usc->ct, strerror (errno));
                goto error;
            }
        }
    }

error:
    return;
}

#elif HAVE(SYS_SELECT_H)

void check_server_on_idle (void *data)
{
    int retval;
    fd_set rset, wset;
    fd_set* rsetptr = NULL;
    fd_set* wsetptr = NULL;
    fd_set* esetptr = NULL;
    struct timeval sel_timeout = { 0, 0 };

    (void)data;

    /* a fdset got modified each time around */
    FD_COPY (&the_server.rfdset, &rset);
    rsetptr = &rset;

    FD_COPY (&the_server.wfdset, &wset);
    wsetptr = &wset;

again:
    if ((retval = select (the_server.maxfd + 1,
            rsetptr, wsetptr, esetptr, &sel_timeout)) < 0) {
        /* no event. */
        if (errno == EINTR) {
            goto again;
        }

        purc_log_error ("unexpected error of select(): %m\n");
        goto error;
    }
    else if (retval == 0) {
        the_server.t_elapsed = purc_get_monotoic_time () - the_server.t_start;
        if (the_server.t_elapsed != the_server.t_elapsed_last) {
            if (the_server.t_elapsed % 10 == 0) {
                check_no_responding_endpoints (&the_server);
            }
            else if (the_server.t_elapsed % 5 == 0) {
                check_dangling_endpoints (&the_server);
            }

            the_server.t_elapsed_last = the_server.t_elapsed;
        }
    }
    else {
        size_t i, nr_fds = sorted_array_count (the_server.fd2clients);
        int *fds = alloca(sizeof(int) * nr_fds);

        for (i = 0; i < nr_fds; i++) {
            fds[i] = (int)(intptr_t)sorted_array_get (the_server.fd2clients, i, NULL);
        }

        for (i = 0; i < nr_fds; i++) {
            int fd = fds[i];
            void *cli_node;

            if (FD_ISSET (fd, rsetptr)) {
                sorted_array_find (the_server.fd2clients, (void *)(intptr_t)fd, &cli_node);
                if (cli_node == PTR_FOR_US_LISTENER) {
                    USClient * client = us_handle_accept (the_server.us_srv);
                    if (client == NULL) {
                        purc_log_info ("Refused a client\n");
                    }
                    else if (listen_new_client (client->fd, client, FALSE)) {
                        purc_log_error ("Failed epoll_ctl for connected unix socket (%d): %s\n",
                                client->fd, strerror (errno));
                        goto error;
                    }
                }
                else if (cli_node == PTR_FOR_WS_LISTENER) {
                    WSClient * client = ws_handle_accept (the_server.ws_srv, the_server.ws_listener);
                    if (client == NULL) {
                        purc_log_info ("Refused a client\n");
                    }
                    else if (listen_new_client (client->fd, client, FALSE)) {
                        purc_log_error ("Failed epoll_ctl for connected web socket (%d): %s\n",
                                client->fd, strerror (errno));
                        goto error;
                    }
                }
                else {
                    USClient *usc = (USClient *)cli_node;
                    if (usc->ct == CT_UNIX_SOCKET) {
                        if (usc->entity) {
                            PurCMCEndpoint *endpoint = container_of (usc->entity,
                                    PurCMCEndpoint, entity);
                            update_endpoint_living_time (&the_server, endpoint);
                        }

                        us_handle_reads (the_server.us_srv, usc);
                    }
                    else if (usc->ct == CT_WEB_SOCKET) {
                        WSClient *wsc = (WSClient *)cli_node;
                        if (wsc->entity) {
                            PurCMCEndpoint *endpoint = container_of (usc->entity,
                                    PurCMCEndpoint, entity);
                            update_endpoint_living_time (&the_server, endpoint);
                        }

                        ws_handle_reads (the_server.ws_srv, wsc);
                    }
                    else {
                        purc_log_error ("Bad socket type (%d): %s\n",
                                usc->ct, strerror (errno));
                        goto error;
                    }
                }
            }

            if (FD_ISSET (fd, wsetptr)) {
                if (cli_node == PTR_FOR_US_LISTENER ||
                        cli_node == PTR_FOR_WS_LISTENER) {
                    assert (0);
                }
                else {
                    USClient *usc = (USClient *)cli_node;
                    if (usc->ct == CT_UNIX_SOCKET) {
                        us_handle_writes (the_server.us_srv, usc);

                        if (!(usc->status & US_SENDING) &&
                                !(usc->status & US_CLOSE)) {
                            FD_CLR (fd, &the_server.wfdset);
                        }
                    }
                    else if (usc->ct == CT_WEB_SOCKET) {
                        WSClient *wsc = (WSClient *)cli_node;
                        ws_handle_writes (the_server.ws_srv, wsc);

                        if (!(wsc->status & WS_SENDING) &&
                                !(wsc->status & WS_CLOSE)) {
                            FD_CLR (fd, &the_server.wfdset);
                        }
                    }
                }
            }
        }
    }

error:
    return;
}

#endif /* HAVE(SYS_SELECT_H) */

static int
comp_living_time (const void *k1, const void *k2, void *ptr)
{
    const PurCMCEndpoint *e1 = k1;
    const PurCMCEndpoint *e2 = k2;

    (void)ptr;
    return e1->t_living - e2->t_living;
}

#if !HAVE(SYS_EPOLL_H) && HAVE(SYS_SELECT_H)
static int
intcmp(const void *sortv1, const void *sortv2)
{
    int fd1 = (int)(intptr_t)sortv1;
    int fd2 = (int)(intptr_t)sortv2;

    return fd1 - fd2;
}
#endif

static int
init_server (void)
{
    int ret;

    ret = purc_init_ex (PURC_MODULE_EJSON,
            "cn.fmsoft.hvml.purcmc", "renderer", NULL);
    if (ret != PURC_ERROR_OK) {
        purc_log_error ("Failed to initialize the PurC modules: %s\n",
                purc_get_error_message (ret));
        return -1;
    }

    purc_enable_log(true, true);

#if !HAVE(SYS_EPOLL_H) && HAVE(SYS_SELECT_H)
    the_server.maxfd = -1;
    the_server.fd2clients = sorted_array_create(SAFLAG_DEFAULT, 4, NULL,
            intcmp);
    if (the_server.fd2clients == NULL)
        return -1;

    FD_ZERO(&the_server.rfdset);
    FD_ZERO(&the_server.wfdset);
#endif

    if (srvcfg.unixsocket == NULL) {
        srvcfg.unixsocket = g_strdup(PCRDR_PURCMC_US_PATH);
    }

    if (srvcfg.addr == NULL) {
        srvcfg.addr = g_strdup(PCRDR_LOCALHOST);
    }

    if (srvcfg.port == NULL) {
        srvcfg.port = g_strdup(PCRDR_PURCMC_WS_PORT);
    }

    if (srvcfg.max_frm_size == 0) {
        srvcfg.max_frm_size = PCRDR_MAX_FRAME_PAYLOAD_SIZE;
    }

    if (srvcfg.backlog == 0) {
        srvcfg.backlog = SOMAXCONN;
    }

    the_server.nr_endpoints = 0;
    the_server.running = true;

    /* TODO for host name */
    the_server.server_name = strdup (PCRDR_LOCALHOST);
    kvlist_init (&the_server.endpoint_list, NULL);
    avl_init (&the_server.living_avl, comp_living_time, true, NULL);

    return 0;
}

static void
deinit_server (void)
{
    const char* name;
    void *next, *data;
    PurCMCEndpoint *endpoint, *tmp;

#if !HAVE(SYS_EPOLL_H) && HAVE(SYS_SELECT_H)
    sorted_array_destroy(the_server.fd2clients);
#endif

    avl_remove_all_elements (&the_server.living_avl, endpoint, avl, tmp) {
        if (endpoint->type == ET_UNIX_SOCKET) {
            us_close_client (the_server.us_srv, (USClient *)endpoint->entity.client);
        }
        else if (endpoint->type == ET_WEB_SOCKET) {
            ws_close_client (the_server.ws_srv, (WSClient *)endpoint->entity.client);
        }
    }

    kvlist_for_each_safe (&the_server.endpoint_list, name, next, data) {
        //memcpy (&endpoint, data, sizeof (PurCMCEndpoint*));
        endpoint = *(PurCMCEndpoint **)data;

        if (endpoint->type != ET_BUILTIN) {
            purc_log_info ("Deleting endpoint: %s (%p) in deinit_server\n", name, endpoint);

            if (endpoint->type == ET_UNIX_SOCKET && endpoint->entity.client) {
                // avoid a duplicated call of del_endpoint
                endpoint->entity.client->entity = NULL;
                us_cleanup_client (the_server.us_srv, (USClient *)endpoint->entity.client);
            }
            else if (endpoint->type == ET_WEB_SOCKET && endpoint->entity.client) {
                // avoid a duplicated call of del_endpoint
                endpoint->entity.client->entity = NULL;
                ws_cleanup_client (the_server.ws_srv, (WSClient *)endpoint->entity.client);
            }

            del_endpoint (&the_server, endpoint, CDE_EXITING);
            kvlist_delete (&the_server.endpoint_list, name);
            the_server.nr_endpoints--;
        }
    }

    kvlist_free (&the_server.endpoint_list);

    if (the_server.dangling_endpoints) {
        gs_list* node = the_server.dangling_endpoints;

        while (node) {
            endpoint = (PurCMCEndpoint *)node->data;
            purc_log_warn ("Removing dangling endpoint: %p, type (%d), status (%d)\n",
                    endpoint, endpoint->type, endpoint->status);

            if (endpoint->type == ET_UNIX_SOCKET) {
                USClient* usc = (USClient *)endpoint->entity.client;
                us_remove_dangling_client (the_server.us_srv, usc);
            }
            else if (endpoint->type == ET_WEB_SOCKET) {
                WSClient* wsc = (WSClient *)endpoint->entity.client;
                ws_remove_dangling_client (the_server.ws_srv, wsc);
            }
            else {
                purc_log_warn ("Bad type of dangling endpoint\n");
            }

            del_endpoint (&the_server, endpoint, CDE_EXITING);

            node = node->next;
        }

        gslist_remove_nodes (the_server.dangling_endpoints);
    }

    us_stop (the_server.us_srv);
    if (the_server.ws_srv)
        ws_stop (the_server.ws_srv);

    free (the_server.server_name);

    purc_log_info ("the_server.nr_endpoints: %d\n", the_server.nr_endpoints);
    assert (the_server.nr_endpoints == 0);

    if (srvcfg.unixsocket) {
        g_free (srvcfg.unixsocket);
        srvcfg.unixsocket = NULL;
    }

    if (srvcfg.addr) {
        g_free (srvcfg.addr);
        srvcfg.addr = NULL;
    }

    if (srvcfg.port) {
        g_free (srvcfg.port);
        srvcfg.port = NULL;
    }
}

int
purcmc_rdr_server_init (void)
{
    int retval;

    srandom (time (NULL));

    if ((retval = init_server ())) {
        purc_log_error ("Error during init_server: %s\n",
                pcrdr_get_ret_message (retval));
        goto error;
    }

    if ((the_server.us_srv = us_init ((PurCMCServerConfig *)&srvcfg)) == NULL) {
        purc_log_error ("Error during us_init\n");
        goto error;
    }

    if (!srvcfg.nowebsocket) {
        if ((the_server.ws_srv = ws_init ((PurCMCServerConfig *)&srvcfg)) == NULL) {
            purc_log_error ("Error during ws_init\n");
            goto error;
        }
    }
    else {
        the_server.ws_srv = NULL;
        purc_log_info ("Skip web socket");
    }

    setup_signal_pipe ();
    prepare_server ();

    /* TODO
    extern hook_t *idle_hook;
    add_hook (&idle_hook, check_server_on_idle, &the_server);
    */

    return 0;

error:
    return 255;
}

int
purcmc_rdr_server_term (void)
{
    deinit_server ();
    purc_cleanup ();
    return 0;
}

