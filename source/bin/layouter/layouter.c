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

#include "xguipro-version.h"
#include "xguipro-features.h"

#include "layouter.h"
#include "dom-ops.h"
#include "utils/load-asset.h"
#include "utils/sorted-array.h"

#include <domruler.h>
#include <glib.h>
#include <assert.h>

#define SA_INITIAL_SIZE        16

#define PTR2U64(p)              ((uint64_t)(uintptr_t)p)

struct ws_layouter {
    struct DOMRulerCtxt *ruler;

    char *def_css;
    size_t len_def_css;
    pchtml_html_document_t *dom_doc;

    struct sorted_array *sa_widget;

    ws_layouter_changed_fn cb_changed;
};

static pchtml_action_t
my_head_walker(pcdom_node_t *node, void *ctx)
{
    switch (node->type) {
    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
    {
        const char *name;
        size_t len;

        pcdom_element_t *element;
        element = pcdom_interface_element(node);
        name = (const char *)pcdom_element_local_name(element, &len);
        if (strncasecmp(name, "style", len) == 0) {
            struct ws_layouter *layouter = ctx;

            purc_log_info("got a style element\n");

            pcdom_node_t *child = node->first_child;
            while (child) {
                if (node->type == PCDOM_NODE_TYPE_TEXT) {
                    pcdom_text_t *text;

                    text = pcdom_interface_text(child);
                    const char *css = (const char *)text->char_data.data.data;
                    domruler_append_css(layouter->ruler,
                            css, text->char_data.data.length);
                }

                child = child->next;
            }
        }

        /* walk to the siblings. */
        return PCHTML_ACTION_NEXT;
    }

    default:
        /* ignore any unknown node types */
        break;

    }

    return PCHTML_ACTION_OK;
}

static void append_css_in_style_element(ws_layouter *layouter)
{
    pcdom_element_t *head = pchtml_doc_get_head(layouter->dom_doc);

    pcdom_node_simple_walk(pcdom_interface_node(head),
            my_head_walker, layouter);
}

/* Create a new layouter */
ws_layouter *ws_layouter_new(ws_metrics *metrics,
        const char *html_contents, size_t sz_html_contents,
        ws_layouter_changed_fn cb_changed)
{
    struct ws_layouter *layouter = calloc(1, sizeof(*layouter));
    if (layouter == NULL)
        return NULL;

    layouter->sa_widget = sorted_array_create(SAFLAG_DEFAULT,
            SA_INITIAL_SIZE, NULL, NULL);
    if (layouter->sa_widget == NULL) {
        goto failed;
    }

    layouter->ruler = domruler_create(metrics->width,
            metrics->height, metrics->dpi, metrics->density);
    if (layouter->ruler == NULL) {
        goto failed;
    }

    layouter->def_css =
        load_asset_content("WEBKIT_WEBEXT_DIR", WEBKIT_WEBEXT_DIR,
            "assets/workspace-layouter.css", &layouter->len_def_css);
    if (layouter->def_css) {
        domruler_append_css(layouter->ruler,
                layouter->def_css, layouter->len_def_css);
    }

    layouter->dom_doc = pchtml_html_document_create();
    int ret = pchtml_html_document_parse_with_buf(layouter->dom_doc,
            (const unsigned char *)html_contents, (sz_html_contents > 0) ?
            sz_html_contents : strlen(html_contents));
    if (ret) {
        purc_log_error("Failed to parse HTML contents for workspace layout.\n");
        goto failed;
    }

    dom_prepare_user_data(pcdom_interface_document(layouter->dom_doc));

    append_css_in_style_element(layouter);

    return layouter;

failed:
    if (layouter->sa_widget)
        sorted_array_destroy(layouter->sa_widget);
    if (layouter->ruler)
        domruler_destroy(layouter->ruler);
    if (layouter->dom_doc) {
        dom_cleanup_user_data(pcdom_interface_document(layouter->dom_doc));
        pchtml_html_document_destroy(layouter->dom_doc);
    }
    if (layouter->def_css) {
        free(layouter->def_css);
    }

    free(layouter);

    return NULL;
}

/* Destroy a layouter */
void ws_layouter_delete(ws_layouter *layouter)
{
    sorted_array_destroy(layouter->sa_widget);
    domruler_destroy(layouter->ruler);
    dom_cleanup_user_data(pcdom_interface_document(layouter->dom_doc));
    pchtml_html_document_destroy(layouter->dom_doc);
    if (layouter->def_css) {
        free(layouter->def_css);
    }

    free(layouter);
}

bool ws_layouter_add_page_groups(ws_layouter *layouter,
        const char *html_fragment, size_t sz_html_fragment)
{
    pcdom_element_t *body = pchtml_doc_get_body(layouter->dom_doc);
    pcdom_document_t *doc = pcdom_interface_document(layouter->dom_doc);

    pcdom_node_t *subtree = dom_parse_fragment(doc, body,
            html_fragment, sz_html_fragment);

    if (subtree) {
        dom_append_subtree_to_element(doc, body, subtree);

        /* TODO: re-layout */
        return true;
    }

    return false;
}

/* Remove a page group */
bool ws_layouter_remove_page_group(ws_layouter *layouter,
        const char *group_id)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = dom_get_element_by_id(dom_doc, group_id);

    if (element) {
        dom_erase_element(dom_doc, element);

        /* TODO: re-layout */
        return true;
    }

    return false;
}

#define PAGE_ID "%s-%s"

static inline pcdom_element_t *
find_page_element(pcdom_document_t *dom_doc,
        const char* group_id, const char *window_name)
{
    gchar *page_id = g_strdup_printf(PAGE_ID, group_id, window_name);
    pcdom_element_t *element = dom_get_element_by_id(dom_doc, group_id);
    g_free(page_id);

    return element;
}

#define HTML_FRAG_PLAINWINDOW  \
    "<article id='%s-%s' class='plainwindow'></article>"

/* Add a plain window into a group */
bool ws_layouter_add_plain_window(ws_layouter *layouter,
        const char *window_name, const char *group_id, void *widget)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = dom_get_element_by_id(dom_doc, group_id);

    pcdom_node_t *subtree = NULL;
    if (element) {
        /* the element must be a `section` element */

        gchar *html_fragment = g_strdup_printf(HTML_FRAG_PLAINWINDOW,
                group_id, window_name);
        subtree = dom_parse_fragment(dom_doc, element,
            html_fragment, strlen(html_fragment));
        g_free(html_fragment);

        if (subtree) {
            dom_append_subtree_to_element(dom_doc, element, subtree);

            pcdom_element_t *article = find_page_element(dom_doc,
                    group_id, window_name);
            assert(article);

            pcdom_node_t *node = pcdom_interface_node(article);
            node->user = widget;

            if (sorted_array_add(layouter->sa_widget,
                        PTR2U64(widget), article)) {
                purc_log_warn("Failed to store widget/element pair\n");
            }
            else {
                /* TODO: re-layout */
            }
        }
    }

    return subtree != NULL;
}

/* Remove a plain window by identifier */
bool ws_layouter_remove_plain_window_by_id(ws_layouter *layouter,
        const char *window_name, const char *group_id)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = find_page_element(dom_doc, group_id, window_name);

    if (element) {
        /* the element must be a `article` element */
        dom_erase_element(dom_doc, element);

        /* TODO: re-layout */
        return true;
    }

    return false;
}

/* Remove a plain window by widget */
bool ws_layouter_remove_plain_window_by_widget(ws_layouter *layouter,
        void *widget)
{
    void *data;
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);

    if (sorted_array_find(layouter->sa_widget, PTR2U64(widget), &data)) {
        pcdom_element_t *element = data;
        dom_erase_element(dom_doc, element);

        /* TODO: re-layout */
        return true;
    }

    return false;
}

#define HTML_FRAG_PAGE  \
    "<figure id='%s-%s'></figure>"

bool ws_layouter_add_page(ws_layouter *layouter,
        const char *page_name, const char *group_id, void *widget)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = dom_get_element_by_id(dom_doc, group_id);

    pcdom_node_t *subtree = NULL;
    if (element) {
        /* the element must be a `div` element */

        gchar *html_fragment = g_strdup_printf(HTML_FRAG_PAGE,
                group_id, page_name);
        subtree = dom_parse_fragment(dom_doc, element,
            html_fragment, strlen(html_fragment));
        g_free(html_fragment);

        if (subtree) {
            dom_append_subtree_to_element(dom_doc, element, subtree);

            pcdom_element_t *figure = find_page_element(dom_doc,
                    group_id, page_name);
            assert(figure);

            pcdom_node_t *node = pcdom_interface_node(figure);
            node->user = widget;

            if (sorted_array_add(layouter->sa_widget,
                        PTR2U64(widget), figure)) {
                purc_log_warn("Failed to store widget/element pair\n");
            }
            else {
                /* TODO: re-layout */
            }
        }
    }

    return subtree != NULL;
}

/* Remove a page by identifier */
bool ws_layouter_remove_page_by_id(ws_layouter *layouter,
        const char *page_name, const char *group_id)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = find_page_element(dom_doc, group_id, page_name);

    if (element) {
        /* the element must be a `figure` element */
        dom_erase_element(dom_doc, element);

        /* TODO: re-layout */
        return true;
    }

    return false;
}

/* Remove a page by widget */
bool ws_layouter_remove_page_by_widget(ws_layouter *layouter,
        void *widget)
{
    void *data;
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);

    if (sorted_array_find(layouter->sa_widget, PTR2U64(widget), &data)) {
        pcdom_element_t *element = data;
        dom_erase_element(dom_doc, element);

        /* TODO: re-layout */
        return true;
    }

    return false;
}

