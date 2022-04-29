/*
** endpoint.h -- The endpoint management.
**
** Copyright (C) 2020, 2021 FMSoft (http://www.fmsoft.cn)
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

#ifndef MC_RENDERER_ENDPOINT_H_
#define MC_RENDERER_ENDPOINT_H_

#include <time.h>
#include <stdbool.h>
#include <string.h>

#include <purc/purc-variant.h>
#include <purc/purc-pcrdr.h>

#include "server.h"

Endpoint* new_endpoint (Server* srv, int type, void* client);

/* causes to delete endpoint */
enum {
    CDE_SOCKET = 0,
    CDE_INITIALIZING,
    CDE_EXITING,
    CDE_LOST_CONNECTION,
    CDE_NO_RESPONDING,
};

int del_endpoint (Server* srv, Endpoint* endpoint, int cause);

bool store_dangling_endpoint (Server* srv, Endpoint* endpoint);
bool remove_dangling_endpoint (Server* srv, Endpoint* endpoint);
bool make_endpoint_ready (Server* srv,
        const char* endpoint_name, Endpoint* endpoint);

int check_no_responding_endpoints (Server *srv);
int check_dangling_endpoints (Server *srv);

int send_packet_to_endpoint (Server* srv,
        Endpoint* endpoint, const char* body, int len_body);
int send_initial_response (Server* srv, Endpoint* endpoint);
int on_got_message(Server* srv, Endpoint* endpoint, const pcrdr_msg *msg);

static inline int
assemble_endpoint_name (Endpoint *endpoint, char *buff)
{
    if (endpoint->host_name && endpoint->app_name && endpoint->runner_name) {
        return purc_assemble_endpoint_name (endpoint->host_name,
                endpoint->app_name, endpoint->runner_name, buff);
    }

    return 0;
}

#endif /* !MC_RENDERER_ENDPOINT_H_ */

