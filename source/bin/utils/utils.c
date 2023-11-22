/*
** utils.c -- The internal utility interfaces
**
** Copyright (C) 2023 FMSoft (http://www.fmsoft.cn)
**
** Author: XueShuming
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
#include <time.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>


time_t xgutils_get_monotoic_time_ms(void)
{
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec * 1000 + tp.tv_nsec * 1.0E-6;
}

void xgutils_global_set_data(const char *key, void *pointer)
{
    GApplication *app = g_application_get_default();
    if (app) {
        g_object_set_data(G_OBJECT(app), key, pointer);
    }
}

void *xgutils_global_get_data(const char *key)
{
    GApplication *app = g_application_get_default();
    if (app) {
        return g_object_get_data(G_OBJECT(app), key);
    }
    return NULL;
}

