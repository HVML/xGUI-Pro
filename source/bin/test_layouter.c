/*
** layouter.c -- The implementation of page/window layouter.
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

#undef NDEBUG

#include "xguipro-version.h"
#include "xguipro-features.h"

#include "utils/load-asset.h"
#include "utils/sorted-array.h"
#include "layouter/layouter.h"
#include "layouter/dom-ops.h"

#include <purc/purc.h>

#include <string.h>
#include <errno.h>
#include <assert.h>

struct ws_layouter {
    struct DOMRulerCtxt *ruler;
    pcdom_document_t *dom_doc;
};

struct test_ctxt {
    struct sorted_array *sa_widget;
};

struct test_widget {
    void *parent;

    char *name;
    char *title;

    int x, y;
    unsigned w, h;
};

void *my_create_widget(void *ws_ctxt, ws_widget_type_t type,
        void *parent, const struct ws_widget_style *style)
{
    struct test_ctxt *ctxt = ws_ctxt;

    purc_log_info("Creating a widget: postion (%d, %d), size (%u x %u)\n",
            style->x, style->y, style->w, style->h);

    if (parent) {
        void *data;
        if (!sorted_array_find(ctxt->sa_widget, PTR2U64(parent), &data)) {
            purc_log_warn("invalid parent: %p\n", parent);
            return NULL;
        }
    }

    struct test_widget *widget = calloc(1, sizeof(*widget));
    widget->parent = parent;
    widget->name = strdup(style->name);
    widget->title = strdup(style->title);
    widget->x = style->x;
    widget->y = style->y;
    widget->w = style->w;
    widget->h = style->h;

    sorted_array_add(ctxt->sa_widget, PTR2U64(widget), NULL);
    return widget;
}

void my_destroy_widget(void *ws_ctxt, void *widget)
{
    struct test_ctxt *ctxt = ws_ctxt;

    purc_log_info("Destroying a widget (%p)\n", widget);

    if (widget == NULL) {
        purc_log_warn("Invalid widget value (NULL)\n");
        return;
    }

    if (!sorted_array_remove(ctxt->sa_widget, PTR2U64(widget))) {
        purc_log_warn("Not existing widget (%p)\n", widget);
        return;
    }

    struct test_widget *w = widget;
    free(w->name);
    free(w->title);
    free(w);
    purc_log_info("Widget (%p) destroyed\n", widget);
}

void my_update_widget(void *ws_ctxt,
        void *widget, const struct ws_widget_style *style)
{
    struct test_ctxt *ctxt = ws_ctxt;

    purc_log_info("Updating a widget (%p)\n", widget);

    if (widget == NULL) {
        purc_log_warn("Invalid widget value (NULL)\n");
        return;
    }

    void *data;
    if (!sorted_array_find(ctxt->sa_widget, PTR2U64(widget), &data)) {
        purc_log_warn("Not existing widget (%p)\n", widget);
        return;
    }

    struct test_widget *w = widget;
    if (style->flags & WSWS_FLAG_NAME) {
        assert(style->name);

        free(w->name);
        w->name = strdup(style->name);
    }

    if (style->flags & WSWS_FLAG_TITLE) {
        assert(style->title);

        free(w->title);
        w->title = strdup(style->title);
    }

    if (style->flags & WSWS_FLAG_POSITION) {
        w->x = style->x;
        w->y = style->y;
        w->w = style->w;
        w->h = style->h;
    }

    purc_log_info("Widget (%p) updated\n", widget);
}

static const char *new_page_groups = ""
    "<section id='freeWindows'>"
    "</section>"
    "<section id='theModals'>"
    "</section>";

int main(int argc, char *argv[])
{
    int retv;
    struct test_ctxt ctxt;
    struct ws_metrics metrics = { 1024, 768, 96, 1 };

    char *html;
    size_t len_html;
    html = load_asset_content(NULL, NULL,
                "test_layouter.html", &len_html);
    assert(html);

    struct ws_layouter *layouter;
    layouter = ws_layouter_new(&metrics, html, len_html, &ctxt,
        my_create_widget, my_destroy_widget, my_update_widget,
        &retv);

    if (layouter == NULL) {
        return retv;
    }

    pcdom_element_t *element;

    element = dom_get_element_by_id(layouter->dom_doc, "mainHeader");
    assert(element && has_tag(element, "DIV"));

    element = dom_get_element_by_id(layouter->dom_doc, "mainBody");
    assert(element && has_tag(element, "DIV"));

    element = dom_get_element_by_id(layouter->dom_doc, "mainFooter");
    assert(element && has_tag(element, "DIV"));

    element = dom_get_element_by_id(layouter->dom_doc, "viewerBody");
    assert(element && has_tag(element, "ARTICLE"));

    retv = ws_layouter_add_page_groups(layouter, new_page_groups,
            strlen(new_page_groups));
    assert(retv == PCRDR_SC_OK);

    element = dom_get_element_by_id(layouter->dom_doc, "freeWindows");
    assert(element);
    assert(has_tag(element, "SECTION"));

    element = dom_get_element_by_id(layouter->dom_doc, "theModals");
    assert(element);
    assert(has_tag(element, "SECTION"));

    retv = ws_layouter_remove_page_group(layouter, "freeWindows");
    assert(retv == PCRDR_SC_OK);

    ws_layouter_add_plain_window(layouter,
        "freeWindows", "test", NULL, NULL, PURC_VARIANT_INVALID, &retv);
    assert(retv == PCRDR_SC_NOT_FOUND);

    ws_layouter_add_plain_window(layouter,
        "theModals", "test", "hc", "this is a test plain window",
        PURC_VARIANT_INVALID, &retv);

    assert(retv == PCRDR_SC_OK);

    ws_layouter_delete(layouter);

    return 0;
}
