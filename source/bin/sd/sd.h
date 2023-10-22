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

struct sd_service *sd_service_register(const char *name, const char *type,
        const char *dom, const char *host, const char *port,
        const char *txt_record, size_t nr_txt_record);


int sd_service_destroy(struct sd_service *);




#ifdef __cplusplus
}
#endif

#endif  /* Service_Discovery_h */

