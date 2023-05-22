/*
** main.h -- The common header.
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

#ifndef main_h
#define main_h

#include "xguipro-version.h"
#include "xguipro-features.h"

#include <purc/purc-helpers.h>
#include <glib.h>

#define BROWSER_DEFAULT_TITLE           "xGUI Pro"
#define BROWSER_DEFAULT_URL             "hvml://localhost/default"
#define BROWSER_ABOUT_SCHEME            "xguipro"
#define BROWSER_HVML_SCHEME             "hvml"

typedef enum {
    XGUI_PRO_ERROR_INVALID_ABOUT_PATH,
    XGUI_PRO_ERROR_INVALID_HVML_URI
} xGUIProError;

static inline GQuark xguipro_error_quark()
{
    return g_quark_from_string("xguipro-quark");
}

#define XGUI_PRO_ERROR (xguipro_error_quark())

#ifdef NDEBUG
#define LOG_DEBUG(x, ...)
#else
#define LOG_DEBUG(x, ...)   \
    purc_log_debug("%s: " x, __func__, ##__VA_ARGS__)
#endif /* not defined NDEBUG */

#define LOG_ERROR(x, ...)   \
    purc_log_error("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_WARN(x, ...)    \
    purc_log_warn("%s: " x, __func__, ##__VA_ARGS__)

#define LOG_INFO(x, ...)    \
    purc_log_info("%s: " x, __func__, ##__VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif  /* main_h */

