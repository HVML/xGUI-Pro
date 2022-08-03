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

#define SA_INITIAL_SIZE 16

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

    ws_widget_type_t type;
    unsigned nr_children;
};

static const char *widget_types[] = {
    "NONE",
    "PLAINWINDOW",
    "TABBEDWINDOW",
    "HEADER",
    "MENUBAR",
    "TOOLBAR",
    "SIDEBAR",
    "FOOTER",
    "PANEL",
    "TABHOST",
    "PLAINPAGE",
    "TABPAGE",
};

void my_convert_style(struct ws_widget_info *style,
        purc_variant_t toolkit_style)
{
    // do nothing.
}

void *my_create_widget(void *workspace, void *session,
        ws_widget_type_t type, void *window, void *parent, void *init_arg,
        const struct ws_widget_info *style)
{
    struct test_ctxt *ctxt = workspace;

    purc_log_info("Creating a `%s` widget (%s, %s) (%d, %d; %u x %u)\n",
            widget_types[type], style->name, style->title,
            style->x, style->y, style->w, style->h);

    if (parent) {
        if (!sorted_array_find(ctxt->sa_widget, PTR2U64(parent), NULL)) {
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

    widget->type = type;
    widget->nr_children = 0;

    purc_log_info("Created widget (%p)\n", widget);

    if (parent) {
        struct test_widget *p = parent;
        p->nr_children++;
    }

    if (sorted_array_add(ctxt->sa_widget, PTR2U64(widget), NULL) != 0) {
        purc_log_warn("Failed to store the widget: %p\n", widget);
    }

    return widget;
}

int my_destroy_widget(void *workspace, void *session, void *window, void *widget,
        ws_widget_type_t type)
{
    struct test_ctxt *ctxt = workspace;

    purc_log_info("Destroying a widget (%p)\n", widget);
    if (widget == NULL) {
        purc_log_warn("Invalid widget value (NULL)\n");
        return PCRDR_SC_BAD_REQUEST;
    }

    if (!sorted_array_find(ctxt->sa_widget, PTR2U64(widget), NULL)) {
        purc_log_warn("Not existing widget (%p)\n", widget);
        return PCRDR_SC_BAD_REQUEST;
    }

    struct test_widget *w = widget;
    while (w->nr_children > 0) {
        purc_log_info("Destroying children of widget (%s, %s): %u\n",
                widget_types[w->type], w->name,
                w->nr_children);

        /* this is a container, destroy children recursively */
        size_t n = sorted_array_count(ctxt->sa_widget);

        for (size_t i = 0; i < n; i++) {
            struct test_widget *one;
            purc_log_info("checking slot %u/%u\n",
                    (unsigned)i, (unsigned)n);
            one = INT2PTR(sorted_array_get(ctxt->sa_widget, i, NULL));
            if (one->parent == w) {
                my_destroy_widget(ctxt, NULL, NULL, one, one->type);
            }

            n = sorted_array_count(ctxt->sa_widget);
        }

        purc_log_info("# children of widget (%s, %s): %u\n",
                widget_types[w->type], w->name,
                w->nr_children);
    }

    if (w->parent) {
        struct test_widget *p = w->parent;
        p->nr_children--;
    }

    if (!sorted_array_remove(ctxt->sa_widget, PTR2U64(widget))) {
        purc_log_warn("Not existing widget (%p)\n", widget);
        return PCRDR_SC_BAD_REQUEST;
    }

    purc_log_info("Widget (%p) destroyed\n", widget);
    free(w->name);
    free(w->title);
    free(w);

    return PCRDR_SC_OK;
}

void my_update_widget(void *workspace, void *session, void *widget,
        ws_widget_type_t type, const struct ws_widget_info *style)
{
    struct test_ctxt *ctxt = workspace;

    purc_log_info("Updating a widget (%p)\n", widget);

    if (widget == NULL) {
        purc_log_warn("Invalid widget value (NULL)\n");
        return;
    }

    if (!sorted_array_find(ctxt->sa_widget, PTR2U64(widget), NULL)) {
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

    if (style->flags & WSWS_FLAG_GEOMETRY) {
        w->x = style->x;
        w->y = style->y;
        w->w = style->w;
        w->h = style->h;
        purc_log_info("Position of widget (%p) updated: (%d, %d; %u x %u)\n",
                widget, w->x, w->y, w->w, w->h);
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

    ctxt.sa_widget = sorted_array_create(SAFLAG_DEFAULT,
            SA_INITIAL_SIZE, NULL, NULL);
    assert(ctxt.sa_widget);

    char *html;
    size_t len_html;
    html = load_asset_content(NULL, NULL,
                (argc > 1) ? argv[1] : "test_layouter.html", &len_html);
    assert(html);

    struct ws_layouter *layouter;
    layouter = ws_layouter_new(&metrics, html, len_html, &ctxt,
            my_convert_style, my_create_widget, my_destroy_widget,
            my_update_widget,
            &retv);
    free(html);

    if (layouter == NULL) {
        return retv;
    }

    if (argc > 1) {
        ws_layouter_delete(layouter);
        sorted_array_destroy(ctxt.sa_widget);
        return 0;
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

    retv = ws_layouter_add_widget_groups(layouter, new_page_groups,
            strlen(new_page_groups));
    assert(retv == PCRDR_SC_OK);

    element = dom_get_element_by_id(layouter->dom_doc, "freeWindows");
    assert(element);
    assert(has_tag(element, "SECTION"));

    element = dom_get_element_by_id(layouter->dom_doc, "theModals");
    assert(element);
    assert(has_tag(element, "SECTION"));

    retv = ws_layouter_remove_widget_group(layouter, NULL, "freeWindows");
    assert(retv == PCRDR_SC_OK);

    ws_layouter_add_plain_window(layouter, NULL,
        "freeWindows", "test", NULL, NULL, NULL,
        PURC_VARIANT_INVALID, NULL, &retv);
    assert(retv == PCRDR_SC_NOT_FOUND);

    void *widget;
    widget = ws_layouter_add_plain_window(layouter, NULL,
        "theModals", "test1", "main", "this is a test plain window", NULL,
        PURC_VARIANT_INVALID, NULL, &retv);
    assert(retv == PCRDR_SC_OK);
    assert(widget != NULL);

    element = dom_get_element_by_id(layouter->dom_doc, "theModals-test1");
    assert(has_tag(element, "FIGURE"));

    ws_layouter_add_plain_window(layouter, NULL,
        "theModals", "test2", "main", "this is a test plain window", NULL,
        PURC_VARIANT_INVALID, NULL, &retv);
    element = dom_get_element_by_id(layouter->dom_doc, "theModals-test2");
    assert(has_tag(element, "FIGURE"));

    retv = ws_layouter_remove_plain_window_by_id(layouter, NULL,
            "theModals", "test3");
    assert(retv == PCRDR_SC_NOT_FOUND);

    retv = ws_layouter_remove_plain_window_by_id(layouter, NULL,
            "theModals", "test2");
    assert(retv == PCRDR_SC_OK);

    retv = ws_layouter_remove_plain_window_by_handle(layouter, NULL, widget);
    assert(retv == PCRDR_SC_OK);

    ws_layouter_add_widget(layouter, NULL,
        "viewerBody", "panel1", "bar", NULL, NULL,
        PURC_VARIANT_INVALID, NULL, &retv);
    assert(retv == PCRDR_SC_BAD_REQUEST);

    ws_layouter_add_widget(layouter, NULL,
        "viewerBodyPanels", "panel1", "bar", NULL, NULL,
        PURC_VARIANT_INVALID, NULL, &retv);
    assert(retv == PCRDR_SC_OK);

    ws_layouter_add_widget(layouter, NULL,
        "viewerBodyTabs", "tab1", NULL, NULL, NULL,
        PURC_VARIANT_INVALID, NULL, &retv);
    assert(retv == PCRDR_SC_OK);

    widget = ws_layouter_add_widget(layouter, NULL,
        "viewerBodyTabs", "tab2", NULL, NULL, NULL,
        PURC_VARIANT_INVALID, NULL, &retv);
    assert(retv == PCRDR_SC_OK);
    assert(widget != NULL);

    ws_widget_type_t type;
    type = ws_layouter_retrieve_widget_by_id(layouter, "theModals", "test3");
    assert(type == WS_WIDGET_TYPE_NONE);

    retv = ws_layouter_remove_widget_by_id(layouter, NULL,
            "theModals", "test3");
    assert(retv == PCRDR_SC_NOT_FOUND);

    type = ws_layouter_retrieve_widget_by_id(layouter,
            "viewerBodyPanels", "panel1");
    assert(type == WS_WIDGET_TYPE_PANEDPAGE);

    type = ws_layouter_retrieve_widget(layouter, widget);
    assert(type == WS_WIDGET_TYPE_TABBEDPAGE);

    retv = ws_layouter_remove_widget_by_id(layouter, NULL,
            "viewerBodyPanels", "panel1");
    assert(retv == PCRDR_SC_OK);

    retv = ws_layouter_remove_widget_by_handle(layouter, NULL,
            widget);
    assert(retv == PCRDR_SC_OK);

    ws_layouter_delete(layouter);

    purc_log_info("Cleaning up widgets (%u)\n",
            (unsigned)sorted_array_count(ctxt.sa_widget));

    while (sorted_array_count(ctxt.sa_widget) > 0) {
        struct test_widget *widget;
        widget = INT2PTR(sorted_array_get(ctxt.sa_widget, 0, NULL));
        my_destroy_widget(&ctxt, NULL, NULL, widget, widget->type);
    }
    sorted_array_destroy(ctxt.sa_widget);

    purc_log_info("TEST DONE\n");

    return 0;
}

