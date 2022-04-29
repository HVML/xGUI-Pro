/*
** unixsocket.c: Utilities for UNIX socket server.
**
** Copyright (c) 2020 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
**
** This file is part of hiBus.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/time.h>

#include "lib/hiboxcompat.h"

#include "server.h"
#include "unixsocket.h"

USServer *us_init (const ServerConfig* config)
{
    USServer *server = calloc (1, sizeof (USServer));

    server->listener = -1;
    server->config = config;
    return server;
}

void us_stop (USServer * server)
{
    close (server->listener);
    free (server);
}

/* returns fd if all OK, -1 on error */
int us_listen (USServer* server)
{
    int    fd, len;
    struct sockaddr_un unix_addr;

    /* create a Unix domain stream socket */
    if ((fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0) {
        ULOG_ERR ("Error duing calling `socket` in us_listen: %s\n", strerror (errno));
        return (-1);
    }

    fcntl (fd, F_SETFD, FD_CLOEXEC);

    /* in case it already exists */
    unlink (server->config->unixsocket);

    /* fill in socket address structure */
    memset (&unix_addr, 0, sizeof(unix_addr));
    unix_addr.sun_family = AF_UNIX;
    strcpy (unix_addr.sun_path, server->config->unixsocket);
    len = sizeof (unix_addr.sun_family) + strlen (unix_addr.sun_path);

    /* bind the name to the descriptor */
    if (bind (fd, (struct sockaddr *) &unix_addr, len) < 0) {
        ULOG_ERR ("Error duing calling `bind` in us_listen: %s\n", strerror (errno));
        goto error;
    }
    if (chmod (server->config->unixsocket, 0666) < 0) {
        ULOG_ERR ("Error duing calling `chmod` in us_listen: %s\n", strerror (errno));
        goto error;
    }

    /* tell kernel we're a server */
    if (listen (fd, server->config->backlog) < 0) {
        ULOG_ERR ("Error duing calling `listen` in us_listen: %s\n", strerror (errno));
        goto error;
    }

    server->listener = fd;
    return (fd);

error:
    close (fd);
    return (-1);
}

#define    STALE    30    /* client's name can't be older than this (sec) */

/* Wait for a client connection to arrive, and accept it.
 * We also obtain the client's pid from the pathname
 * that it must bind before calling us.
 */
/* returns new fd if all OK, < 0 on error */
static int us_accept (int listenfd, pid_t *pidptr, uid_t *uidptr)
{
    int                clifd;
    socklen_t          len;
    time_t             staletime;
    struct sockaddr_un unix_addr;
    struct stat        statbuf;
    const char*        pid_str;

    len = sizeof (unix_addr);
    if ((clifd = accept (listenfd, (struct sockaddr *) &unix_addr, &len)) < 0)
        return (-1);        /* often errno=EINTR, if signal caught */

    fcntl (clifd, F_SETFD, FD_CLOEXEC);

    /* obtain the client's uid from its calling address */
    len -= sizeof(unix_addr.sun_family);
    if (len <= 0) {
        ULOG_ERR ("Bad peer address in us_accept: %s\n", unix_addr.sun_path);
        goto error;
    }

    unix_addr.sun_path[len] = 0;            /* null terminate */
    ULOG_NOTE ("The peer address in us_accept: %s\n", unix_addr.sun_path);
    if (stat (unix_addr.sun_path, &statbuf) < 0) {
        ULOG_ERR ("Failed `stat` in us_accept: %s\n", strerror (errno));
        goto error;
    }
#ifdef S_ISSOCK    /* not defined for SVR4 */
    if (S_ISSOCK(statbuf.st_mode) == 0) {
        ULOG_ERR ("Not a socket: %s\n", unix_addr.sun_path);
        goto error;
    }
#endif
    if ((statbuf.st_mode & (S_IRWXG | S_IRWXO)) ||
            (statbuf.st_mode & S_IRWXU) != S_IRWXU) {
        ULOG_ERR ("Bad RW mode (rwx------): %s\n", unix_addr.sun_path);
        goto error;
    }

    staletime = time(NULL) - STALE;
    if (statbuf.st_atime < staletime ||
            statbuf.st_ctime < staletime ||
            statbuf.st_mtime < staletime) {
        ULOG_ERR ("i-node is too old: %s\n", unix_addr.sun_path);
        goto error;
    }

    if (uidptr != NULL)
        *uidptr = statbuf.st_uid;    /* return uid of caller */

    /* get pid of client from sun_path */
    pid_str = strrchr (unix_addr.sun_path, '-');
    pid_str++;

    *pidptr = atoi (pid_str);
    
    unlink (unix_addr.sun_path);        /* we're done with pathname now */
    return (clifd);

error:
    close (clifd);
    return -1;
}

/* Set the given file descriptor as NON BLOCKING. */
inline static int
set_nonblocking (int sock)
{
    if (fcntl (sock, F_SETFL, fcntl (sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
        ULOG_ERR ("Unable to set socket as non-blocking: %s.",
                strerror (errno));
        return -1;
    }

    return 0;
}

/* Handle a new UNIX socket connection. */
USClient *
us_handle_accept (USServer* server)
{
    USClient *usc = NULL;
    pid_t pid;
    uid_t uid;
    int newfd = -1;

    usc = (USClient *)calloc (sizeof (USClient), 1);
    if (usc == NULL) {
        ULOG_ERR ("Failed to callocate memory for Unix socket client\n");
        return NULL;
    }

    INIT_LIST_HEAD (&usc->pending);
    usc->sz_pending = 0;

    newfd = us_accept (server->listener, &pid, &uid);
    if (newfd < 0) {
        ULOG_ERR ("Failed to accept Unix socket: %d\n", newfd);
        goto failed;
    }

    if (set_nonblocking (newfd)) {
        goto cleanup;
    }

    usc->ct = CT_UNIX_SOCKET;
    usc->fd = newfd;
    usc->pid = pid;
    usc->uid = uid;
    server->nr_clients++;

    if (server->nr_clients > MAX_CLIENTS_EACH) {
        ULOG_WARN ("Too many clients (maximal clients allowed: %d)\n", MAX_CLIENTS_EACH);
        server->on_error (server, (SockClient *)usc, PCRDR_SC_SERVICE_UNAVAILABLE);
        goto cleanup;
    }

    if (server->on_accepted) {
        int ret_code;
        ret_code = server->on_accepted (server, (SockClient *)usc);
        if (ret_code != PCRDR_SC_OK) {
            ULOG_WARN ("Internal error after accepted this client (%d): %d\n",
                    newfd, ret_code);

            server->on_error (server, (SockClient *)usc, ret_code);
            goto cleanup;
        }
    }

    ULOG_NOTE ("Accepted a client via Unix socket: fd (%d), pid (%d), uid (%d)\n",
            newfd, pid, uid);
    return usc;

cleanup:
    us_cleanup_client (server, usc);
    return NULL;

failed:
    free (usc);
    return NULL;
}

/*
 * Clear pending data.
 */
static void us_clear_pending_data (USClient *client)
{
    struct list_head *p, *n;

    list_for_each_safe (p, n, &client->pending) {
        list_del (p);
        free (p);
    }

    client->sz_pending = 0;

    update_upper_entity_stats (client->entity, client->sz_pending, client->sz_packet);
}

/*
 * Queue new data.
 *
 * On success, true is returned.
 * On error, false is returned and the connection status is set.
 */
static bool us_queue_data (USClient *client, const char *buf, size_t len)
{
    USPendingData *pending_data;

    if ((pending_data = malloc (sizeof (USPendingData) + len)) == NULL) {
        us_clear_pending_data (client);
        client->status = US_ERR | US_CLOSE;
        return false;
    }

    memcpy (pending_data->data, buf, len);
    pending_data->szdata = len;
    pending_data->szsent = 0;

    list_add_tail (&pending_data->list, &client->pending);
    client->sz_pending += len;
    update_upper_entity_stats (client->entity, client->sz_pending, client->sz_packet);
    client->status |= US_SENDING;

    /* client probably is too slow, so stop queueing until everything is
     * sent */
    if (client->sz_pending >= SOCK_THROTTLE_THLD)
        client->status |= US_THROTTLING;

    return true;
}

/*
 * Send the given buffer to the given socket.
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned.
 */
static ssize_t us_write_data (USServer *server, USClient *client,
        const char *buffer, size_t len)
{
    ssize_t bytes = 0;

    bytes = write (client->fd, buffer, len);
    if (bytes == -1 && errno == EPIPE) {
        client->status = US_ERR | US_CLOSE;
        return -1;
    }

    /* did not send all of it... buffer it for a later attempt */
    if ((bytes > 0 && (size_t)bytes < len) ||
            (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
        us_queue_data (client, buffer + bytes, len - bytes);

        if (client->status & US_SENDING && server->on_pending)
            server->on_pending (server, (SockClient *)client);
    }

    return bytes;
}

/*
 * Send the queued up client's data to the given socket.
 *
 * On error, -1 is returned and the connection status is set.
 * On success, the number of bytes sent is returned.
 */
static ssize_t us_write_pending (USServer *server, USClient *client)
{
    ssize_t total_bytes = 0;
    struct list_head *p, *n;

    (void)server;

    list_for_each_safe (p, n, &client->pending) {
        ssize_t bytes;
        USPendingData *pending = (USPendingData *)p;

        bytes = write (client->fd,
                pending->data + pending->szsent, pending->szdata - pending->szsent);

        if (bytes > 0) {
            pending->szsent += bytes;
            if (pending->szsent >= pending->szdata) {
                list_del (p);
                free (p);
            }
            else {
                break;
            }

            total_bytes += bytes;
            client->sz_pending -= bytes;
            update_upper_entity_stats (client->entity,
                    client->sz_pending, client->sz_packet);
        }
        else if (bytes == -1 && errno == EPIPE) {
            client->status = US_ERR | US_CLOSE;
            return -1;
        }
        else if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return -1;
        }
    }

    return total_bytes;
}

/*
 * A wrapper of the system call write or send.
 *
 * On error, -1 is returned and the connection status is set as error.
 * On success, the number of bytes sent is returned.
 */
static ssize_t us_write (USServer *server, USClient *client,
        const void *buffer, size_t len)
{
    ssize_t bytes = 0;

    /* attempt to send the whole buffer */
    if (list_empty (&client->pending)) {
        bytes = us_write_data (server, client, buffer, len);
    }
    /* the pending list not empty, just append new data if we're not
     * throttling the client */
    else if (client->sz_pending < SOCK_THROTTLE_THLD) {
        if (us_queue_data (client, buffer, len))
            return bytes;
    }
    /* send from cache buffer */
    else {
        bytes = us_write_pending (server, client);
    }

    return bytes;
}

/* TODO: tune this for noblocking read */
#if 0
static inline ssize_t my_read (int fd, void* buff, size_t sz)
{
    ssize_t bytes;

again:
    bytes = read (fd, buff, sz);
    if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        goto again;

    return bytes;
}
#endif

/*
 * return values:
 * < 0: error
 * 0: payload read
 * 1: got packet;
 */
static int try_to_read_payload (USServer* server, USClient* usc)
{
    ssize_t n;

    (void)server;
    switch (usc->header.op) {
    case US_OPCODE_TEXT:
    case US_OPCODE_BIN:
        if ((n = read (usc->fd, usc->packet, usc->header.sz_payload))
                < usc->header.sz_payload) {
            ULOG_ERR ("Failed to read payload from Unix socket: %s\n",
                    strerror (errno));
            return PCRDR_ERROR_IO;
        }

        usc->sz_read = usc->header.sz_payload;
        if (usc->header.fragmented == 0) {
            return 1;
        }
        break;

    case US_OPCODE_CONTINUATION:
    case US_OPCODE_END:
        if (usc->packet == NULL ||
                (usc->sz_read + usc->header.sz_payload) > usc->sz_packet) {
            return PCRDR_ERROR_PROTOCOL;
        }

        if ((n = read (usc->fd, usc->packet + usc->sz_read,
                usc->header.sz_payload)) < usc->header.sz_payload) {
            ULOG_ERR ("Failed to read payload from Unix socket: %s\n",
                    strerror (errno));
            return PCRDR_ERROR_IO;
        }

        usc->sz_read += usc->header.sz_payload;
        if (usc->header.op == US_OPCODE_END) {
            return 1;
        }
        break;

    default:
        return PCRDR_ERROR_PROTOCOL;
    }

    return 0;
}

int us_handle_reads (USServer* server, USClient* usc)
{
    int retv, err_code = 0, sta_code = 0;
    ssize_t n = 0;

    /* if it is not waiting for payload, read a frame header */
    if (usc->status & US_WATING_FOR_PAYLOAD) {

        usc->status &= ~US_WATING_FOR_PAYLOAD;
        retv = try_to_read_payload (server, usc);
        if (retv > 0) {
            goto got_packet;
        }
        else if (retv == 0) {
            goto done;
        }
        else if (retv < 0) {
            err_code = retv;
            sta_code = PCRDR_SC_EXPECTATION_FAILED;
            goto done;
        }
    }
    else {
        n = read (usc->fd, &usc->header, sizeof (USFrameHeader));
        if (n < (ssize_t)sizeof (USFrameHeader)) {
            ULOG_ERR ("Failed to read frame header from Unix socket.\n");
            err_code = PCRDR_ERROR_IO;
            sta_code = PCRDR_SC_EXPECTATION_FAILED;
            goto done;
        }

        switch (usc->header.op) {
        case US_OPCODE_PING: {
            USFrameHeader header;
            header.op = US_OPCODE_PONG;
            header.fragmented = 0;
            header.sz_payload = 0;
            n = us_write (server, usc, &header, sizeof (USFrameHeader));
            if (n < 0) {
                ULOG_ERR ("Error when wirting socket: %s\n", strerror (errno));
                err_code = PCRDR_ERROR_IO;
                sta_code = PCRDR_SC_IOERR;
            }
            break;
        }

        case US_OPCODE_CLOSE:
            ULOG_WARN ("Peer closed\n");
            err_code = PCRDR_ERROR_PEER_CLOSED;
            sta_code = 0;
            break;

        case US_OPCODE_TEXT:
        case US_OPCODE_BIN: {
            if (usc->header.fragmented > 0 &&
                    usc->header.fragmented > usc->header.sz_payload) {
                usc->sz_packet = usc->header.fragmented;
            }
            else {
                usc->sz_packet = usc->header.sz_payload;
            }

            if (usc->sz_packet > PCRDR_MAX_INMEM_PAYLOAD_SIZE ||
                    usc->sz_packet == 0 ||
                    usc->header.sz_payload == 0) {
                err_code = PCRDR_ERROR_PROTOCOL;
                sta_code = PCRDR_SC_PACKET_TOO_LARGE;
                break;
            }

            clock_gettime (CLOCK_MONOTONIC, &usc->ts);
            if (usc->header.op == US_OPCODE_TEXT)
                usc->t_packet = PT_TEXT;
            else
                usc->t_packet = PT_BINARY;

            /* always reserve a space for null character */
            usc->packet = malloc (usc->sz_packet + 1);
            if (usc->packet == NULL) {
                ULOG_ERR ("Failed to allocate memory for packet (size: %u)\n",
                        usc->sz_packet);
                err_code = PCRDR_ERROR_NOMEM;
                sta_code = PCRDR_SC_INSUFFICIENT_STORAGE;
                break;
            }

            update_upper_entity_stats (usc->entity, usc->sz_pending, usc->sz_packet);

            retv = try_to_read_payload (server, usc);
            if (retv > 0) {
                goto got_packet;
            }
            else if (retv == 0) {
                goto done;
            }
            else if (retv == PCRDR_ERROR_IO) {
                usc->status |= US_WATING_FOR_PAYLOAD;
                goto done;
            }
            else {
                err_code = retv;
                sta_code = PCRDR_SC_EXPECTATION_FAILED;
                goto done;
            }
            break;
        }

        case US_OPCODE_CONTINUATION:
        case US_OPCODE_END:
            if (usc->header.sz_payload == 0) {
                err_code = PCRDR_ERROR_PROTOCOL;
                sta_code = PCRDR_SC_PACKET_TOO_LARGE;
                break;
            }

            retv = try_to_read_payload (server, usc);
            if (retv > 0) {
                goto got_packet;
            }
            else if (retv == 0) {
                goto done;
            }
            else if (retv == PCRDR_ERROR_IO) {
                usc->status |= US_WATING_FOR_PAYLOAD;
                goto done;
            }
            else {
                err_code = retv;
                sta_code = PCRDR_SC_EXPECTATION_FAILED;
                goto done;
            }
            break;

        case US_OPCODE_PONG: {
            Endpoint *endpoint = container_of (usc->entity, Endpoint, entity);

            assert (endpoint);

            ULOG_INFO ("Got a PONG frame from endpoint @%s/%s/%s\n",
                    endpoint->host_name, endpoint->app_name, endpoint->runner_name);
            break;
        }

        default:
            ULOG_ERR ("Unknown frame opcode: %d\n", usc->header.op);
            err_code = PCRDR_ERROR_PROTOCOL;
            sta_code = PCRDR_SC_EXPECTATION_FAILED;
            break;
        }
    }

done:
    if (err_code) {
        if (sta_code) {
            server->on_error (server, (SockClient*)usc, sta_code);
        }

        us_cleanup_client (server, usc);
    }

    return err_code;

got_packet:
    usc->packet [usc->sz_read] = '\0';
    sta_code = server->on_packet (server, (SockClient *)usc, usc->packet,
            (usc->t_packet == PT_TEXT) ? (usc->sz_read + 1) : usc->sz_read,
            usc->t_packet);
    free (usc->packet);
    usc->packet = NULL;
    usc->sz_packet = 0;
    usc->sz_read = 0;
    update_upper_entity_stats (usc->entity, usc->sz_pending, usc->sz_packet);

    if (sta_code != PCRDR_SC_OK) {
        ULOG_WARN ("Internal error after got a packet: %d\n", sta_code);

        server->on_error (server, (SockClient*)usc, sta_code);
        err_code = PCRDR_ERROR_SERVER_ERROR;

        us_cleanup_client (server, usc);
    }

    return err_code;
}

/*
 * Handle write.
 * 0: ok;
 * <0: socket closed
*/
int us_handle_writes (USServer *server, USClient *usc)
{
    us_write_pending (server, usc);
    if (list_empty (&usc->pending)) {
        usc->status &= ~US_SENDING;
    }

    if ((usc->status & US_CLOSE) && !(usc->status & US_SENDING)) {
        us_cleanup_client (server, usc);
        return -1;
    }

    return 0;
}

/*
 * Ping a specific client.
 *
 * return zero on success; none-zero on error.
 */
int us_ping_client (USServer* server, USClient* usc)
{
    USFrameHeader header;

    header.op = US_OPCODE_PING;
    header.fragmented = 0;
    header.sz_payload = 0;
    us_write (server, usc, &header, sizeof (USFrameHeader));

    if (usc->status & US_ERR)
        return -1;
    return 0;
}

/*
 * Ping a specific client.
 *
 * return zero on success; none-zero on error.
 */
int us_close_client (USServer* server, USClient* usc)
{
    USFrameHeader header;

    header.op = US_OPCODE_CLOSE;
    header.fragmented = 0;
    header.sz_payload = 0;
    us_write (server, usc, &header, sizeof (USFrameHeader));

    if (usc->status & US_ERR)
        return -1;
    return 0;
}

/*
 * Send a packet
 *
 * return zero on success; none-zero on error.
 */
int us_send_packet (USServer* server, USClient* usc,
        USOpcode op, const void* data, unsigned int sz)
{
    USFrameHeader header;

    switch (op) {
        case US_OPCODE_TEXT:
        case US_OPCODE_BIN:
            break;
        case US_OPCODE_PING:
            return us_ping_client (server, usc);
        case US_OPCODE_CLOSE:
            return us_close_client (server, usc);
        default:
            ULOG_WARN ("Unknown UnixSocket op code: %d\n", op);
            return -1;
    }

    if (sz > PCRDR_MAX_FRAME_PAYLOAD_SIZE) {
        const char* buff = data;
        unsigned int left = sz;

        do {
            if (left == sz) {
                header.op = op;
                header.fragmented = sz;
                header.sz_payload = PCRDR_MAX_FRAME_PAYLOAD_SIZE;
                left -= PCRDR_MAX_FRAME_PAYLOAD_SIZE;
            }
            else if (left > PCRDR_MAX_FRAME_PAYLOAD_SIZE) {
                header.op = US_OPCODE_CONTINUATION;
                header.fragmented = 0;
                header.sz_payload = PCRDR_MAX_FRAME_PAYLOAD_SIZE;
                left -= PCRDR_MAX_FRAME_PAYLOAD_SIZE;
            }
            else {
                header.op = US_OPCODE_END;
                header.fragmented = 0;
                header.sz_payload = left;
                left = 0;
            }

            us_write (server, usc, &header, sizeof (USFrameHeader));
            us_write (server, usc, buff, header.sz_payload);
            buff += header.sz_payload;

        } while (left > 0);
    }
    else {
        header.op = op;
        header.fragmented = 0;
        header.sz_payload = sz;
        us_write (server, usc, &header, sizeof (USFrameHeader));
        us_write (server, usc, data, sz);
    }

    if (usc->status & US_ERR) {
        ULOG_ERR ("Error when sending data to client: fd (%d), pid (%d)\n",
                usc->fd, usc->pid);
        return -1;
    }

    return 0;
}

int us_remove_dangling_client (USServer *server, USClient *usc)
{
    us_clear_pending_data (usc);

    if (usc->fd >= 0) {
        close (usc->fd);
    }

    usc->fd = -1;
    server->nr_clients--;
    assert (server->nr_clients >= 0);

    free (usc);
    return 0;
}

int us_cleanup_client (USServer *server, USClient *usc)
{
    server->on_close (server, (SockClient *)usc);

    return us_remove_dangling_client (server, usc);
}

