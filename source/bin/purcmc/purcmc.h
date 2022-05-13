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

struct PurCMCSessionInfo;
typedef struct PurCMCSessionInfo PurCMCSessionInfo;

struct PurCMCEndpoint;
typedef struct PurCMCEndpoint PurCMCEndpoint;

/* The PurcMC Server */
struct PurCMCServer;
typedef struct PurCMCServer PurCMCServer;

/* Config Options */
typedef struct PurCMCServerConfig {
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
} PurCMCServerConfig;

int purcmc_rdr_server_init(PurCMCServerConfig* srvcfg);
int purcmc_rdr_server_deinit(void);

#endif /* !XGUIPRO_PURCMC_PURCMC_H*/

