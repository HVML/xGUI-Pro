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
#include <dns_sd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include <purc/purc.h>


#include "xguipro-features.h"
#include "sd.h"

#define MAX_TXT_RECORD_SIZE 8900
#define MAX_DOMAIN_NAME     1009
#define HOST_NAME_SUFFIX    ".local."

#define HexVal(X) ( ((X) >= '0' && (X) <= '9') ? ((X) - '0'     ) :  \
                    ((X) >= 'A' && (X) <= 'F') ? ((X) - 'A' + 10) :  \
                    ((X) >= 'a' && (X) <= 'f') ? ((X) - 'a' + 10) : 0)

#define HexPair(P) ((HexVal((P)[0]) << 4) | HexVal((P)[1]))

#define WS_SCHEMA       "ws://"
#define KEY_NEW_RENDERER_COMMON       "common"
#define KEY_NEW_RENDERER_URI          "uri"

#define V_NEW_RENDERER_SOCKET         "socket"

typedef union { unsigned char b[2]; unsigned short NotAnInteger; } Opaque16;

static uint32_t opinterface = kDNSServiceInterfaceIndexAny;

struct sd_service {
    DNSServiceRef sdref;
    DNSServiceRef browse_sdref;
    sd_service_browse_reply browse_cb;
    void *browse_cb_ctxt;
};

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
        struct sd_service *p = malloc(sizeof(struct sd_service));
        p->sdref = sdref;
        p->browse_sdref = NULL;
        p->browse_cb = NULL;
        p->browse_cb_ctxt = NULL;
        *srv = p;
    }

out:
    return ret;
}


int sd_service_destroy(struct sd_service *srv)
{
    if (srv) {
        if (srv->browse_sdref) {
            DNSServiceRefDeallocate(srv->browse_sdref);
        }
        if (srv->sdref) {
            DNSServiceRefDeallocate(srv->sdref);
        }
        free(srv);
    }
    return 0;
}

static int CopyLabels(char *dst, const char *lim, const char **srcp, int labels)
{
    const char *src = *srcp;
    while (*src != '.' || --labels > 0)
    {
        if (*src == '\\') *dst++ = *src++;  // Make sure "\." doesn't confuse us
        if (!*src || dst >= lim) return -1;
        *dst++ = *src++;
        if (!*src || dst >= lim) return -1;
    }
    *dst++ = 0;
    *srcp = src + 1;    // skip over final dot
    return 0;
}

static void resolve_cb(DNSServiceRef sdref,
        const DNSServiceFlags flags, uint32_t if_index,
        DNSServiceErrorType error_code,
        const char *fullname, const char *hosttarget, uint16_t opaqueport,
        uint16_t txt_len, const unsigned char *txt, void *context)
{
    (void) txt_len;
    union { uint16_t s; u_char b[2]; } port = { opaqueport };
    uint16_t u_port = ((uint16_t)port.b[0]) << 8 | port.b[1];
    const char *p = fullname;

    char n[MAX_DOMAIN_NAME];
    char t[MAX_DOMAIN_NAME];

    // Fetch name+type
    if (CopyLabels(n, n + MAX_DOMAIN_NAME, &p, 3)) {
        goto out;
    }

    p = fullname;
    // Skip first label
    if (CopyLabels(t, t + MAX_DOMAIN_NAME, &p, 1)) {
        goto out;
    }
    // Fetch next two labels (service type)
    if (CopyLabels(t, t + MAX_DOMAIN_NAME, &p, 2)) {
        goto out;
    }

    const unsigned char *const end = txt + 1 + txt[0];
    txt++;      // Skip over length byte

    struct sd_service *service = (struct sd_service *)context;
    service->browse_cb(service, error_code, if_index,
        fullname, t, hosttarget, u_port, (const char *)txt, end - txt,
        service->browse_cb_ctxt);

out:
    DNSServiceRefDeallocate(sdref);
}

static void browse_reply(DNSServiceRef sdref, const DNSServiceFlags flags,
    uint32_t if_index, int error_code, const char *service_name,
    const char *reg_type, const char *reply_domain, void *ctxt)
{
    if (!(flags & kDNSServiceFlagsAdd)) {
        return;
    }

    struct sd_service *p = (struct sd_service *)ctxt;
    DNSServiceRef newref = p->sdref;
    DNSServiceResolve(&newref, kDNSServiceFlagsShareConnection,
            if_index, service_name, reg_type, reply_domain,
            resolve_cb, ctxt);
}

int sd_start_browsing_service(struct sd_service **srv, const char *reg_type,
    const char *domain, sd_service_browse_reply cb, void *ctxt)
{
    int flags = kDNSServiceFlagsShareConnection;
    uint32_t if_index = opinterface;

    struct sd_service *service = *srv;
    if (service == NULL) {
        service = malloc(sizeof(struct sd_service));
        service->browse_cb= cb;
        service->browse_cb_ctxt = ctxt;
        DNSServiceCreateConnection(&service->sdref);
    }
    else {
        DNSServiceRefDeallocate(service->browse_sdref);
        service->browse_sdref = NULL;
    }

    service->browse_sdref = service->sdref;
    int ret = DNSServiceBrowse(&service->browse_sdref, flags, if_index,
            reg_type, domain, browse_reply, service);

    if (ret == kDNSServiceErr_NoError) {
        *srv = service;
    }
    else {
        free(service);
    }

    return ret;
}

void sd_stop_browsing_service(struct sd_service *srv)
{
    sd_service_destroy(srv);
}

int sd_service_get_fd(struct sd_service *srv)
{
    return DNSServiceRefSockFD(srv->sdref);
}

int sd_service_process_result(struct sd_service *srv)
{
    return DNSServiceProcessResult(srv->sdref);
}

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
    purc_variant_t v_common = purc_variant_make_string_static(V_NEW_RENDERER_SOCKET,
            false);

    purc_variant_object_set_by_ckey(event.data, KEY_NEW_RENDERER_COMMON, v_common);
    purc_variant_object_set_by_ckey(event.data, KEY_NEW_RENDERER_URI, v_uri);

    purcmc_endpoint_post_event(srv->server, srv->endpoint, &event);
}
