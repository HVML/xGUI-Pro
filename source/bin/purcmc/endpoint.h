/*
** endpoint.h -- The endpoint management.
**
** Copyright (C) 2021, 2022 FMSoft (http://www.fmsoft.cn)
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

#ifndef XGUIPRO_PURCMC_ENDPOINT_H
#define XGUIPRO_PURCMC_ENDPOINT_H

#include <time.h>
#include <stdbool.h>
#include <string.h>

#include <purc/purc-variant.h>
#include <purc/purc-pcrdr.h>

#include "server.h"

purcmc_endpoint* new_endpoint (purcmc_server* srv, int type, void* client);
purcmc_endpoint* get_curr_endpoint (purcmc_server* srv);

/* causes to delete endpoint */
enum {
    CDE_SOCKET = 0,
    CDE_INITIALIZING,
    CDE_EXITING,
    CDE_LOST_CONNECTION,
    CDE_NO_RESPONDING,
};

int del_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint, int cause);

bool store_dangling_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint);
bool remove_dangling_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint);
bool make_endpoint_ready (purcmc_server* srv,
        const char* endpoint_name, purcmc_endpoint* endpoint);

int accept_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint);
int remove_endpoint (purcmc_server* srv, purcmc_endpoint* endpoint);

int check_no_responding_endpoints (purcmc_server *srv);
int check_dangling_endpoints (purcmc_server *srv);
int check_timeout_dangling_endpoints (purcmc_server *srv);

int send_packet_to_endpoint (purcmc_server* srv,
        purcmc_endpoint* endpoint, const char* body, int len_body);
int send_initial_response (purcmc_server* srv, purcmc_endpoint* endpoint);
int on_got_message(purcmc_server* srv, purcmc_endpoint* endpoint, const pcrdr_msg *msg);

static inline int
assemble_endpoint_name (purcmc_endpoint *endpoint, char *buff)
{
    if (endpoint->host_name && endpoint->app_name && endpoint->runner_name) {
        return purc_assemble_endpoint_name (endpoint->host_name,
                endpoint->app_name, endpoint->runner_name, buff);
    }

    return 0;
}

#endif /* !XGUIPRO_PURCMC_ENDPOINT_H */

