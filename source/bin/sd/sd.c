/*
** sd.c -- implementation of Service Discovery.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>


#include <purc/purc.h>


#include "xguipro-features.h"
#include "sd.h"

#define HOST_NAME_SUFFIX              ".local."
#define WS_SCHEMA                     "ws://"
#define KEY_NEW_RENDERER_COMM         "comm"
#define KEY_NEW_RENDERER_URI          "uri"
#define V_WEBSOCKET                   "websocket"

const char *sd_get_local_hostname(void)
{
    static char hostname[1024] = {0};
    struct addrinfo hints, *info, *p;
    int ret;

    if (hostname[0]) {
        return hostname;
    }

    gethostname(hostname, 1023);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    if ((ret = getaddrinfo(hostname, "http", &hints, &info)) != 0) {
        hostname[0] = 0;
        return NULL;
    }

    hostname[0] = 0;
    for(p = info; p != NULL; p = p->ai_next) {
        strcpy(hostname, p->ai_canonname);
        strcat(hostname, HOST_NAME_SUFFIX);
    }

    freeaddrinfo(info);
    return hostname;
}

int sd_get_host_addr(const char *hostname, char *ipv4, size_t ipv4_sz,
        char *ipv6, size_t ipv6_sz)
{
    struct addrinfo hints, *info, *p;
    int ret;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    if ((ret = getaddrinfo(hostname, "http", &hints, &info)) != 0) {
        return -1;
    }

    for(p = info; p != NULL; p = p->ai_next) {
        void *addr = NULL;
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *si = (struct sockaddr_in *)p->ai_addr;
            addr = &(si->sin_addr);
            if (ipv4 && ipv4_sz) {
                inet_ntop(p->ai_family, addr, ipv4, ipv4_sz);
            }
        } else {
            struct sockaddr_in6 *si = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(si->sin6_addr);
            if (ipv6 && ipv6_sz) {
                inet_ntop(p->ai_family, addr, ipv6, ipv6_sz);
            }
        }
    }

    freeaddrinfo(info);
    return 0;
}


int sd_get_local_ip(char *ipv4, size_t ipv4_sz, char *ipv6, size_t ipv6_sz)
{
    struct ifaddrs *addrs, *ifa;
    void *in_addr;

    if (getifaddrs(&addrs) != 0) {
        return -1;
    }

    for (ifa = addrs; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        if (!(ifa->ifa_flags & IFF_UP)) {
            continue;
        }

        if ((strcmp("lo", ifa->ifa_name) == 0) ||
                !(ifa->ifa_flags & (IFF_RUNNING))) {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *si = (struct sockaddr_in *)ifa->ifa_addr;
            in_addr = &(si->sin_addr);
            if (ipv4 && ipv4_sz) {
                inet_ntop(ifa->ifa_addr->sa_family, in_addr, ipv4, ipv4_sz);
            }
        } else {
            struct sockaddr_in6 *si = (struct sockaddr_in6 *)ifa->ifa_addr;
            in_addr = &(si->sin6_addr);
            if (ipv6 && ipv6_sz) {
                inet_ntop(ifa->ifa_addr->sa_family, in_addr, ipv6, ipv6_sz);
            }
        }
    }
    freeifaddrs(addrs);

    return 0;
}

void sd_remote_service_destroy(struct sd_remote_service *srv)
{
    if (!srv) {
        return;
    }
    if (srv->full_name) {
        free(srv->full_name);
    }
    if (srv->reg_type) {
        free(srv->reg_type);
    }
    if (srv->host) {
        free(srv->host);
    }
    if (srv->txt) {
        free(srv->txt);
    }
}

void post_new_rendereer_event(struct sd_remote_service *srv)
{
    //struct purcmc_server *server;
    //purcmc_endpoint *endpoint;
    pcrdr_msg event = { };
    event.type = PCRDR_MSG_TYPE_EVENT;
    event.target = PCRDR_MSG_TARGET_SESSION;
    event.targetValue = 0;
    event.eventName =
        purc_variant_make_string_static("newRenderer", false);
    /* TODO: use real URI for the sourceURI */
    event.sourceURI = purc_variant_make_string_static(PCRDR_APP_RENDERER,
            false);
    event.elementType = PCRDR_MSG_ELEMENT_TYPE_VOID;
    event.property = PURC_VARIANT_INVALID;
    event.dataType = PCRDR_MSG_DATA_TYPE_JSON;
    event.data = purc_variant_make_object(0, PURC_VARIANT_INVALID,
            PURC_VARIANT_INVALID);

    size_t nr = strlen(WS_SCHEMA) + strlen(srv->host) + 10;
    char uri[nr];
    sprintf(uri, "%s%s:%d", WS_SCHEMA, srv->host, srv->port);
    purc_variant_t v_uri = purc_variant_make_string_static(uri, false);
    purc_variant_t v_common = purc_variant_make_string_static(V_WEBSOCKET,
            false);

    purc_variant_object_set_by_ckey(event.data, KEY_NEW_RENDERER_COMM, v_common);
    purc_variant_object_set_by_ckey(event.data, KEY_NEW_RENDERER_URI, v_uri);

    purcmc_endpoint_post_event(srv->server, srv->endpoint, &event);
}

