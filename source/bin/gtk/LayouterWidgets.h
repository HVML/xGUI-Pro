/*
** LayouterWidgets.h -- The management of widgets for layouter.
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

#ifndef LayouterWidgets_h
#define LayouterWidgets_h

#include "purcmc/purcmc.h"
#include "layouter/layouter.h"

#ifdef __cplusplus
extern "C" {
#endif

void *gtk_create_widget(void *ws_ctxt, ws_widget_type_t type,
        void *parent, const struct ws_widget_style *style);

void gtk_destroy_widget(void *ws_ctxt, void *widget);

void gtk_update_widget(void *ws_ctxt,
        void *widget, const struct ws_widget_style *style);

#ifdef __cplusplus
}
#endif

#endif  /* LayouterWidgets_h */

