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
#include <dns_sd.h>

#include "xguipro-features.h"
#include "sd.h"

#define MAX_TXT_RECORD_SIZE 8900

#define HexVal(X) ( ((X) >= '0' && (X) <= '9') ? ((X) - '0'     ) :  \
                    ((X) >= 'A' && (X) <= 'F') ? ((X) - 'A' + 10) :  \
                    ((X) >= 'a' && (X) <= 'f') ? ((X) - 'a' + 10) : 0)

#define HexPair(P) ((HexVal((P)[0]) << 4) | HexVal((P)[1]))

typedef union { unsigned char b[2]; unsigned short NotAnInteger; } Opaque16;

static uint32_t opinterface = kDNSServiceInterfaceIndexAny;

int sd_service_register(struct sd_service **srv,
        const char *name, const char *type,
        const char *dom, const char *host, const char *port,
        const char **txt_record, size_t nr_txt_record)
{
    int ret = 0;
    DNSServiceRef sdref = NULL;
    DNSServiceFlags flags = 0;
    uint16_t PortAsNumber = atoi(port);
    Opaque16 registerPort = { { PortAsNumber >> 8, PortAsNumber & 0xFF } };
    unsigned char txt[MAX_TXT_RECORD_SIZE] = { 0 };
    unsigned char *ptr = txt;

    *srv = NULL;

    if (name[0] == '.' && name[1] == 0) {
        name = "";
    }

    if (dom[0] == '.' && dom[1] == 0) {
        dom = "";
    }

    if (nr_txt_record) {
        for (int i = 0; i < nr_txt_record; i++) {
            const char *p = txt_record[i];
            if (ptr >= txt + sizeof(txt)) {
                ret = kDNSServiceErr_BadParam;
                goto out;
            }

            *ptr = 0;
            while (*p && *ptr < 255) {
                if (ptr + 1 + *ptr >= txt + sizeof(txt)) {
                    ret = kDNSServiceErr_BadParam;
                    goto out;
                }

                if (p[0] != '\\' || p[1] == 0) {
                    ptr[++*ptr] = *p;
                    p+=1;
                }
                else if (p[1] == 'x' && isxdigit(p[2]) && isxdigit(p[3])) {
                    ptr[++*ptr] = HexPair(p+2);
                    p+=4;
                }
                else {
                    ptr[++*ptr] = p[1];
                    p+=2;
                }
            }
            ptr += 1 + *ptr;
        }
    }

    //flags |= kDNSServiceFlagsAllowRemoteQuery;
    //flags |= kDNSServiceFlagsNoAutoRenamee;

    ret = DNSServiceRegister(&sdref, flags, opinterface,
            name, type, dom, host, registerPort.NotAnInteger,
            (uint16_t) (ptr-txt), txt, NULL, NULL);
    if (ret == kDNSServiceErr_NoError) {
        *srv = (struct sd_service *) sdref;
    }

out:
    return ret;
}


int sd_service_destroy(struct sd_service *svs)
{
    DNSServiceRefDeallocate((DNSServiceRef)svs);
    return 0;
}

struct browser_cb_pair {
    sd_service_browse_reply cb;
    void *ctx;
};

static void browse_reply(DNSServiceRef sdref, const DNSServiceFlags flags,
    uint32_t interface_index, int error_code, const char *service_name,
    const char *reg_type, const char *reply_domain, void *ctx)
{
    struct browser_cb_pair *p = (struct browser_cb_pair *)ctx;
    p->cb((struct sd_service *)sdref, flags, interface_index, error_code,
            service_name, reg_type, reply_domain, p->ctx);
}

int sd_start_browsing_service(struct sd_service **srv, const char *reg_type,
    const char *domain, sd_service_browse_reply cb, void *ctx)
{
    DNSServiceRef sdref = NULL;
    int flags = 0;
    uint32_t interface_index = opinterface;

    struct browser_cb_pair *p = malloc(sizeof(struct browser_cb_pair));
    p->cb = cb;
    p->ctx = ctx;
    int ret = DNSServiceBrowse(&sdref, flags, interface_index, reg_type, domain,
            browse_reply, p);

    if (ret != kDNSServiceErr_NoError) {
        *srv = (struct sd_service *)sdref;
    }
    else {
        free(p);
    }

    return ret;
}

void sd_stop_browsing_service(struct sd_service *srv)
{
    if (srv) {
        DNSServiceRefDeallocate((DNSServiceRef)srv);
    }
}

int sd_service_get_fd(struct sd_service *srv)
{
    return DNSServiceRefSockFD((DNSServiceRef)srv);
}

int sd_service_process_result(struct sd_service *srv)
{
    return DNSServiceProcessResult((DNSServiceRef)srv);
}
