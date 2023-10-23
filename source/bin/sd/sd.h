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

#ifdef __cplusplus
extern "C" {
#endif

struct sd_service;

/* regist service */
int sd_service_register(struct sd_service **srv,
        const char *name, const char *type,
        const char *dom, const char *host, const char *port,
        const char **txt_record, size_t nr_txt_record);

int sd_service_destroy(struct sd_service *);


/* browsing service */
typedef void (*sd_service_browse_reply)(struct sd_service *srv,
        int error_code, uint32_t interface_index, const char *full_name,
        const char *reg_type, const char *host, uint16_t port,
        const char *txt, size_t nr_txt);

int sd_start_browsing_service(struct sd_service **srv, const char *reg_type,
    const char *domain, sd_service_browse_reply cb, void *ctx);

typedef void (*sd_stop_browse_cb)(struct sd_service *srv, void *ctx);

void sd_stop_browsing_service(struct sd_service *srv);


/* handle event  */
int sd_service_get_fd(struct sd_service *srv);

/*
 * This call will block until the daemon's response is received.
 * Use sd_service_get_fd() in conjunction with a run loop or select()
 * to determine the presence of a response from the server before calling
 * this function to process the reply without blocking.
 */
int sd_service_process_result(struct sd_service *srv);

#ifdef __cplusplus
}
#endif

#endif  /* Service_Discovery_h */

