/*
** LayouterWidgets.c -- The management of widgets for layouter.
**
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
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
#include "main.h"
#include "BrowserWindow.h"
#include "BuildRevision.h"
#include "PurcmcCallbacks.h"
#include "LayouterWidgets.h"

#include "purcmc/purcmc.h"
#include "layouter/layouter.h"

#include "utils/list.h"
#include "utils/kvlist.h"
#include "utils/sorted-array.h"

#include <errno.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

void *gtk_create_widget(void *ws_ctxt, ws_widget_type_t type,
        void *parent, const struct ws_widget_style *style)
{
    return NULL;
}

void gtk_destroy_widget(void *ws_ctxt, void *widget)
{
}

void gtk_update_widget(void *ws_ctxt,
        void *widget, const struct ws_widget_style *style)
{
}

