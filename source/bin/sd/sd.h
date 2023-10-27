/*
** sd.h -- header for Service Discovery.
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

#ifndef Service_Discovery_h
#define Service_Discovery_h

#include <stddef.h>
#include "purcmc/purcmc.h"

#define SD_IP_V4_LEN        16

#ifdef __cplusplus
extern "C" {
#endif

struct sd_remote_service {
    uint32_t index;
    time_t   ct;

    char *full_name;
    char *reg_type;

    char *host;
    uint16_t port;

    char *txt;
    size_t nr_txt;

    struct purcmc_server *server;
    purcmc_endpoint *endpoint;
    void *hostingWindow;
};

const char *sd_get_local_hostname(void);
int sd_get_host_addr(const char *hostname, char *ipv4, size_t ipv4_sz,
        char *ipv6, size_t ipv6_sz);
int sd_get_local_ip(char *ipv4, size_t ipv4_sz, char *ipv6, size_t ipv6_sz);

void sd_remote_service_destroy(struct sd_remote_service *srv);

void post_new_rendereer_event(struct sd_remote_service *srv);

#ifdef __cplusplus
}
#endif

#endif  /* Service_Discovery_h */

