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

#include <stdlib.h>
#include <dns_sd.h>

#include "xguipro-features.h"
#include "sd.h"

#define MAX_TXT_RECORD_SIZE 8900

typedef union { unsigned char b[2]; unsigned short NotAnInteger; } Opaque16;

static uint32_t opinterface = kDNSServiceInterfaceIndexAny;

struct sd_service *sd_service_register(const char *name, const char *type,
        const char *dom, const char *host, const char *port,
        const char *txt_record, size_t nr_txt_record)
{
    DNSServiceRef sdref = NULL;
    DNSServiceFlags flags = 0;
    uint16_t PortAsNumber = atoi(port);
    Opaque16 registerPort = { { PortAsNumber >> 8, PortAsNumber & 0xFF } };

    if (name[0] == '.' && name[1] == 0) {
        name = "";
    }

    if (dom[0] == '.' && dom[1] == 0) {
        dom = "";
    }


    //flags |= kDNSServiceFlagsAllowRemoteQuery;
    //flags |= kDNSServiceFlagsNoAutoRenamee;

    DNSServiceErrorType ret = DNSServiceRegister(&sdref, flags, opinterface,
            name, type, dom, host, registerPort.NotAnInteger, nr_txt_record,
            txt_record, NULL, NULL);
    if (ret == kDNSServiceErr_NoError) {
        return (struct sd_service *) sdref;
    }
    return NULL;
}


int sd_service_destroy(struct sd_service *svs)
{
    DNSServiceRefDeallocate((DNSServiceRef)svs);
    return 0;
}
