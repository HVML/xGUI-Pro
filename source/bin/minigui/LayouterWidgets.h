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

#include "utils/list.h"
#include "utils/kvlist.h"
#include "utils/sorted-array.h"

/* handle types */
enum {
    HT_WORKSPACE = 0,
    HT_PLAINWIN,
    HT_TABBEDWIN,
    HT_CONTAINER,
    HT_PANE_TAB,
    HT_WEBVIEW,
};

struct purcmc_workspace {
    /* page to owners map */
    pcutils_kvlist_t    page_owners;

    /* manager of grouped plain windows and pages */
    struct ws_layouter *layouter;
};

struct purcmc_session {
    purcmc_server *srv;

    WebKitSettings *webkit_settings;
    WebKitWebContext *web_context;

    /* the sorted array of all valid handles */
    struct sorted_array *all_handles;

    /* the pending requests */
    struct kvlist pending_responses;

    /* the only workspace for all sessions of current app */
    purcmc_workspace *workspace;

    /* the URI prefix: hvml://<hostName>/<appName>/<runnerName>/ */
    char *uri_prefix;

    bool allow_switching_rdr;
    bool is_removing;
};

#ifdef __cplusplus
extern "C" {
#endif

void mg_imp_get_monitor_geometry(struct ws_metrics *ws_geometry);

void mg_imp_convert_style(struct ws_widget_info *style,
        purc_variant_t toolkit_style);

void mg_imp_evaluate_geometry(struct ws_widget_info *style,
        const char *layout_style, bool allow_scaling_by_density);

void mg_imp_evaluate_transition(struct ws_widget_info *style,
        const char *transition_style);

void *mg_imp_create_widget(void *workspace, void *session,
        ws_widget_type_t type, void *window,
        void *parent, void *init_arg, const struct ws_widget_info *style);

int  mg_imp_destroy_widget(void *workspace, void *session,
        void *window, void *widget, ws_widget_type_t type);

void mg_imp_update_widget(void *workspace, void *session, void *widget,
        ws_widget_type_t type, const struct ws_widget_info *style);

uint64_t mg_imp_get_last_widget(void *session);

bool mg_find_handle(void *session, uint64_t handle, void **data);



#ifdef __cplusplus
}
#endif

#endif  /* LayouterWidgets_h */

