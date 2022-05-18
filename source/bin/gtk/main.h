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

#include <glib.h>

typedef enum {
    XGUI_PRO_ERROR_INVALID_ABOUT_PATH,
    XGUI_PRO_ERROR_INVALID_HVML_URI
} xGUIProError;

static inline GQuark xguipro_error_quark()
{
    return g_quark_from_string("xguipro-quark");
}

#define XGUI_PRO_ERROR (xguipro_error_quark())

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif  /* main_h */

