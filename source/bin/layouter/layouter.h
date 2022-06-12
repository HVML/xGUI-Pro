/*
** layouter.h -- The module interface for page/window layouter.
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

#ifndef XGUIPRO_LAYOUTER_LAYOUTER_H
#define XGUIPRO_LAYOUTER_LAYOUTER_H

#include <stddef.h>
#include <stdbool.h>

/* The layouter object; `ws` means workspace */
struct ws_layouter;
typedef struct ws_layouter ws_layouter;

struct ws_metrics {
    unsigned int    width;
    unsigned int    height;
    unsigned int    dpi;
    unsigned int    density;
};
typedef struct ws_metrics ws_metrics;

typedef enum {
    WS_WIDGET_TYPE_PLAINWINDOW  = 0,
    WS_WIDGET_TYPE_HEADER,
    WS_WIDGET_TYPE_MENUBAR,
    WS_WIDGET_TYPE_NAVBAR,
    WS_WIDGET_TYPE_ASIDE,
    WS_WIDGET_TYPE_FOOTER,
    WS_WIDGET_TYPE_TABHOST,
} ws_widget_type_t;

struct ws_widget_style {
    int x, y, w, h;
};
typedef struct ws_widget_style ws_widget_style;

typedef void *(*wsltr_create_widget_fn)(void *ws_ctxt, ws_widget_type_t type,
        void *parent, const ws_widget_style *style);

typedef void (*wsltr_destroy_widget_fn)(void *ws_ctxt, void *widget);

typedef void (*wsltr_change_widget_fn)(void *ws_ctxt,
        void *widget, const ws_widget_style *style);

#ifdef __cplusplus
extern "C" {
#endif

/* Create a new layouter */
ws_layouter *ws_layouter_new(ws_metrics *metrics,
        const char *html_contents, size_t sz_html_contents, void *ws_ctxt,
        wsltr_create_widget_fn cb_create_widget,
        wsltr_destroy_widget_fn cb_destroy_widget,
        wsltr_change_widget_fn cb_change_widget);

/* Destroy a layouter */
void ws_layouter_delete(ws_layouter *layouter);

/* Add new page groups */
bool ws_layouter_add_page_groups(ws_layouter *layouter,
        const char *html_fragment, size_t sz_html_fragment);

/* Remove a page group */
bool ws_layouter_remove_page_group(ws_layouter *layouter,
        const char *group_id);

/* Add a plain window into a group */
bool ws_layouter_add_plain_window(ws_layouter *layouter,
        const char *window_name, const char *group_id);

/* Remove a plain window by identifier */
bool ws_layouter_remove_plain_window_by_id(ws_layouter *layouter,
        const char *window_name, const char *group_id);

/* Remove a plain window by widget */
bool ws_layouter_remove_plain_window_by_widget(ws_layouter *layouter,
        void *widget);

/* Add a page into a group */
bool ws_layouter_add_page(ws_layouter *layouter,
        const char *page_name, const char *group_id);

/* Remove a page by identifier */
bool ws_layouter_remove_page_by_id(ws_layouter *layouter,
        const char *page_name, const char *group_id);

/* Remove a page by widget */
bool ws_layouter_remove_page_by_widget(ws_layouter *layouter,
        void *widget);

#ifdef __cplusplus
}
#endif

#endif /* !XGUIPRO_LAYOUTER_LAYOUTER_H*/

