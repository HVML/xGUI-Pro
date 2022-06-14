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

    pchtml_html_document_t *dom_doc;

    struct sorted_array *sa_widget;

    void *ws_ctxt;
    wsltr_create_widget_fn cb_create_widget;
    wsltr_destroy_widget_fn cb_destroy_widget;
    wsltr_update_widget_fn cb_update_widget;
};

static inline pcdom_element_t *
find_section_ancestor(pcdom_element_t *element)
{
    pcdom_node_t *node = pcdom_interface_node(element);

    node = node->parent;
    while (node) {
        if (is_an_element_with_tag(node, "SECTION"))
            return pcdom_interface_element(node);

        node = node->parent;
    }

    return NULL;
}

static inline pcdom_element_t *
find_section_child(pcdom_element_t *element)
{
    pcdom_node_t *node = pcdom_interface_node(element);

    node = node->first_child;
    while (node) {
        if (is_an_element_with_tag(node, "SECTION"))
            return pcdom_interface_element(node);

        node = node->next;
    }

    return NULL;
}

static inline pcdom_element_t *
find_article_ancestor(pcdom_element_t *element)
{
    pcdom_node_t *node = pcdom_interface_node(element);

    node = node->parent;
    while (node) {
        if (is_an_element_with_tag(node, "ARTICLE"))
            return pcdom_interface_element(node);

        node = node->parent;
    }

    return NULL;
}

static inline void
fill_position(struct ws_widget_style *style, const HLBox *box)
{
    style->x = (int)(box->x + 0.5);
    style->y = (int)(box->y + 0.5);

    if (box->w == HL_AUTO) {
        style->w = 0;
    }
    else {
        style->w = (int)(box->w + 0.5);
    }

    if (box->h == HL_AUTO) {
        style->h = 0;
    }
    else {
        style->h = (int)(box->h + 0.5);
    }
}

static void get_element_name_title(pcdom_element_t *element,
        char **name, char **title)
{
    pcdom_attr_t *attr;

    *name  = NULL;
    *title = NULL;

    attr = pcdom_element_first_attribute(element);
    while (attr) {
        const char *attr_name, *attr_value;
        size_t sz;
        attr_name = (const char *)pcdom_attr_local_name(attr, &sz);
        if (strncasecmp(attr_name, "name", sizeof("name")) == 0) {
            attr_value = (const char *)pcdom_attr_value(attr, &sz);
            *name = strndup(attr_value, sz);
        }
        else if (strncasecmp(attr_name, "title", sizeof("title")) == 0) {
            attr_value = (const char *)pcdom_attr_value(attr, &sz);
            *title = strndup(attr_value, sz);
        }

        if (name && title)
            break;

        attr = pcdom_element_next_attribute(attr);
    }
}

#define ANONYMOUS_NAME      "annoymous"
#define UNTITLED            "Untitled"

static void *create_widget_for_element(struct ws_layouter *layouter,
        pcdom_element_t *element, ws_widget_type_t type, void *parent)
{
    const HLBox *box;
    box = domruler_get_element_bounding_box(layouter->ruler,
            pcdom_interface_node(element));
    if (box == NULL)
        return NULL;

    char *name, *title;
    get_element_name_title(element, &name, &title);

    struct ws_widget_style style = { 0, 0, 0, 0 };
    style.flags = WSWS_FLAG_NAME | WSWS_FLAG_TITLE |
        WSWS_FLAG_POSITION;
    style.name = name ? name : ANONYMOUS_NAME;
    style.title = title ? title: UNTITLED;
    fill_position(&style, box);

    void *widget = layouter->cb_create_widget(layouter->ws_ctxt,
            type, parent, &style);
    if (widget == NULL)
        return NULL;

    set_element_user_data(element, widget);
    if (!sorted_array_add(layouter->sa_widget,
                PTR2U64(widget), element)) {
        purc_log_warn("Failed to store widget/element pair\n");
    }

    return widget;
}

static bool destroy_widget_for_element(struct ws_layouter *layouter,
        pcdom_element_t *element)
{
    void *widget = get_element_user_data(element);
    if (widget) {
        sorted_array_remove(layouter->sa_widget, PTR2U64(widget));
        layouter->cb_destroy_widget(layouter->ws_ctxt, widget);
        return true;
    }

    return false;
}

static pchtml_action_t
append_style_walker(pcdom_node_t *node, void *ctxt)
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
            struct ws_layouter *layouter = ctxt;

            purc_log_debug("Got a style element\n");

            pcdom_node_t *child = node->first_child;
            while (child) {
                if (child->type == PCDOM_NODE_TYPE_TEXT) {
                    pcdom_text_t *text;

                    text = pcdom_interface_text(child);
                    const char *css = (const char *)text->char_data.data.data;
                    domruler_append_css(layouter->ruler,
                            css, text->char_data.data.length);
                    purc_log_debug("CSS totally %u bytes applied.\n",
                            (unsigned)text->char_data.data.length);
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

static void append_css_in_style_element(struct ws_layouter *layouter)
{
    pcdom_element_t *head = pchtml_doc_get_head(layouter->dom_doc);

    pcdom_node_simple_walk(pcdom_interface_node(head),
            append_style_walker, layouter);
}

struct ws_layouter *ws_layouter_new(struct ws_metrics *metrics,
        const char *html_contents, size_t sz_html_contents, void *ws_ctxt,
        wsltr_create_widget_fn cb_create_widget,
        wsltr_destroy_widget_fn cb_destroy_widget,
        wsltr_update_widget_fn cb_update_widget, int *retv)
{
    struct ws_layouter *layouter = calloc(1, sizeof(*layouter));
    if (layouter == NULL) {
        *retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        return NULL;
    }

    layouter->sa_widget = sorted_array_create(SAFLAG_DEFAULT,
            SA_INITIAL_SIZE, NULL, NULL);
    if (layouter->sa_widget == NULL) {
        *retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }

    layouter->ruler = domruler_create(metrics->width,
            metrics->height, metrics->dpi, metrics->density);
    if (layouter->ruler == NULL) {
        *retv = PCRDR_SC_INSUFFICIENT_STORAGE;
        goto failed;
    }

    char *def_css;
    size_t len_def_css;
    def_css = load_asset_content("WEBKIT_WEBEXT_DIR", WEBKIT_WEBEXT_DIR,
                DEF_LAYOUT_CSS, &len_def_css);
    if (def_css) {
        domruler_append_css(layouter->ruler, def_css, len_def_css);
        free(def_css);
    }
    else {
        purc_log_warn("Failed to load default CSS from: %s\n", DEF_LAYOUT_CSS);
        purc_log_warn("Please check your environment variable"
                " `WEBKIT_WEBEXT_DIR`\n");
    }

    layouter->dom_doc = pchtml_html_document_create();
    int ret = pchtml_html_document_parse_with_buf(layouter->dom_doc,
            (const unsigned char *)html_contents, (sz_html_contents > 0) ?
            sz_html_contents : strlen(html_contents));
    if (ret) {
        purc_log_error("Failed to parse HTML contents for workspace layout.\n");
        *retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        goto failed;
    }

    pcdom_element_t *body = pchtml_doc_get_body(layouter->dom_doc);
    if (body == NULL) {
        purc_log_error("No `body` element in layout DOM.\n");
        *retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    if (find_section_child(body) == NULL) {
        purc_log_error("No `section` element in layout DOM.\n");
        *retv = PCRDR_SC_BAD_REQUEST;
        goto failed;
    }

    dom_prepare_id_map(pcdom_interface_document(layouter->dom_doc));

    append_css_in_style_element(layouter);

    layouter->ws_ctxt = ws_ctxt;
    layouter->cb_create_widget = cb_create_widget;
    layouter->cb_destroy_widget = cb_destroy_widget;
    layouter->cb_update_widget = cb_update_widget;
    *retv = PCRDR_SC_OK;
    return layouter;

failed:
    if (layouter->sa_widget)
        sorted_array_destroy(layouter->sa_widget);
    if (layouter->ruler)
        domruler_destroy(layouter->ruler);
    if (layouter->dom_doc) {
        dom_cleanup_id_map(pcdom_interface_document(layouter->dom_doc));
        pchtml_html_document_destroy(layouter->dom_doc);
    }

    free(layouter);
    return NULL;
}

void ws_layouter_delete(struct ws_layouter *layouter)
{
    sorted_array_destroy(layouter->sa_widget);
    domruler_destroy(layouter->ruler);
    dom_cleanup_id_map(pcdom_interface_document(layouter->dom_doc));
    pchtml_html_document_destroy(layouter->dom_doc);

    free(layouter);
}

int ws_layouter_add_page_groups(struct ws_layouter *layouter,
        const char *html_fragment, size_t sz_html_fragment)
{
    pcdom_element_t *body = pchtml_doc_get_body(layouter->dom_doc);
    pcdom_document_t *doc = pcdom_interface_document(layouter->dom_doc);

    pcdom_node_t *subtree = dom_parse_fragment(doc, body,
            html_fragment, sz_html_fragment);

    if (subtree && subtree->first_child) {
        pcdom_node_t *section;
        section = subtree->first_child->first_child;

        if (is_an_element_with_tag(section, "SECTION")) {
            dom_append_subtree_to_element(doc, body, subtree);
            return PCRDR_SC_OK;
        }
        else {
            purc_log_warn("not a `section` element\n");
            return PCRDR_SC_BAD_REQUEST;
        }
    }

    return PCRDR_SC_INTERNAL_SERVER_ERROR;
}

struct destroy_widget_ctxt {
    unsigned nr_destroyed;
    struct ws_layouter *layouter;
};

static pchtml_action_t
destroy_widget_walker(pcdom_node_t *node, void *ctxt)
{
    switch (node->type) {
    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        if (node->user) {
            struct destroy_widget_ctxt *my_ctxt = ctxt;
            destroy_widget_for_element(my_ctxt->layouter,
                    pcdom_interface_element(node));
            my_ctxt->nr_destroyed++;

            node->user = NULL;
        }

        if (node->first_child != NULL) {
            return PCHTML_ACTION_OK;
        }

        /* walk to the siblings. */
        return PCHTML_ACTION_NEXT;

    default:
        /* ignore any unknown node types */
        break;
    }

    return PCHTML_ACTION_NEXT;
}

struct relayout_widget_ctxt {
    unsigned nr_laid;
    unsigned nr_ignored;
    unsigned nr_error;

    struct ws_layouter *layouter;
};

static pchtml_action_t
layout_widget_walker(pcdom_node_t *node, void *ctxt)
{
    switch (node->type) {
    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        if (node->user) {
            struct relayout_widget_ctxt *my_ctxt = ctxt;

            const HLBox *box;
            box = domruler_get_element_bounding_box(my_ctxt->layouter->ruler,
                    node);

            if (box) {
                struct ws_widget_style style = { 0 };

                style.flags = WSWS_FLAG_POSITION;
                fill_position(&style, box);
                my_ctxt->layouter->cb_update_widget(my_ctxt->layouter->ws_ctxt,
                        node->user, &style);
                my_ctxt->nr_laid++;
            }
            else {
                my_ctxt->nr_error++;
            }
        }

        if (node->first_child != NULL) {
            return PCHTML_ACTION_OK;
        }

        /* walk to the siblings. */
        return PCHTML_ACTION_NEXT;

    default:
        /* ignore any unknown node types */
        break;
    }

    return PCHTML_ACTION_NEXT;
}

static int
relayout(struct ws_layouter *layouter, pcdom_element_t *subtree_root)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *root = dom_doc->element;

    domruler_reset_elements(layouter->ruler);
    int ret = domruler_layout_pcdom_elements(layouter->ruler, root);
    if (ret) {
        purc_log_error("Failed to re-layout the widgets: %d.\n", ret);
        return PCRDR_SC_INTERNAL_SERVER_ERROR;
    }

    if (subtree_root == NULL) {
        subtree_root = root;
    }

    struct relayout_widget_ctxt ctxt = { 0, 0, 0, layouter };
    pcdom_node_t *node = pcdom_interface_node(subtree_root);
    pcdom_node_simple_walk(node, layout_widget_walker, &ctxt);
    return PCRDR_SC_OK;
}

int ws_layouter_remove_page_group(struct ws_layouter *layouter,
        const char *group_id)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = dom_get_element_by_id(dom_doc, group_id);

    if (element) {

        struct destroy_widget_ctxt ctxt = { 0, layouter };
        pcdom_node_t *node = pcdom_interface_node(element);
        pcdom_node_simple_walk(node, destroy_widget_walker, &ctxt);

        if (destroy_widget_for_element(layouter, element)) {
            ctxt.nr_destroyed++;
        }

        purc_log_info("Totatlly %u widget(s) destroyed.\n", ctxt.nr_destroyed);

        pcdom_element_t *section = find_section_ancestor(element);

        dom_erase_element(dom_doc, element);

        relayout(layouter, section);
        return PCRDR_SC_OK;
    }

    return PCRDR_SC_NOT_FOUND;
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
    "<figure id='%s-%s' class='%s' name='%s' title='%s'></figure>"

void *ws_layouter_add_plain_window(struct ws_layouter *layouter,
        const char *group_id, const char *window_name,
        const char *class_name, const char *title, purc_variant_t style,
        int *retv)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = dom_get_element_by_id(dom_doc, group_id);

    void *widget = NULL;
    if (element) {
        pcdom_node_t *subtree;
        /* the element must be a descendant of a `section` element */
        pcdom_element_t *section = find_section_ancestor(element);
        if (section == NULL) {
            purc_log_error("Cannot find the ancestor `section` element (%s)\n",
                    group_id);
            *retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        gchar *html_fragment = g_strdup_printf(HTML_FRAG_PLAINWINDOW,
                group_id, window_name, class_name ? class_name : "",
                window_name, title ? title : "");
        subtree = dom_parse_fragment(dom_doc, element,
            html_fragment, strlen(html_fragment));
        g_free(html_fragment);

        if (subtree) {
            dom_append_subtree_to_element(dom_doc, element, subtree);

            /* re-layout the exsiting widgets */
            relayout(layouter, section);

            /* create widget */
            pcdom_element_t *figure = find_page_element(dom_doc,
                    group_id, window_name);
            assert(figure);

            if (create_widget_for_element(layouter, figure,
                    WS_WIDGET_TYPE_PLAINWINDOW, NULL) == NULL) {
                *retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
                goto failed;
            }

            *retv = PCRDR_SC_OK;
        }
        else {
            *retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        }
    }
    else {
        *retv = PCRDR_SC_NOT_FOUND;
    }

failed:
    return widget;
}

/* Remove a plain window by identifier */
int ws_layouter_remove_plain_window_by_id(struct ws_layouter *layouter,
        const char *group_id, const char *window_name)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element =
        find_page_element(dom_doc, group_id, window_name);

    /* the element must be a `figure` element */
    if (element && has_tag(element, "FIGURE")) {
        pcdom_element_t *section = find_section_ancestor(element);

        destroy_widget_for_element(layouter, element);
        dom_erase_element(dom_doc, element);
        relayout(layouter, section);

        return PCRDR_SC_OK;
    }

    return PCRDR_SC_NOT_FOUND;
}

int ws_layouter_remove_plain_window_by_widget(struct ws_layouter *layouter,
        void *widget)
{
    void *data;
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);

    if (sorted_array_find(layouter->sa_widget, PTR2U64(widget), &data)) {
        pcdom_element_t *element = data;
        assert(element);

        /* the element must be a `figure` element */
        if (has_tag(element, "FIGURE")) {
            pcdom_element_t *section = find_section_ancestor(element);

            destroy_widget_for_element(layouter, element);
            dom_erase_element(dom_doc, element);
            relayout(layouter, section);

            return PCRDR_SC_OK;
        }
    }

    return PCRDR_SC_NOT_FOUND;
}

struct create_widget_ctxt {
    struct ws_layouter *layouter;
    void *tabbed_window;
};

static pchtml_action_t
create_widget_walker(pcdom_node_t *node, void *ctxt)
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

        struct create_widget_ctxt *my_ctxt = ctxt;
        if (strcasecmp(name, "HEADER") == 0) {
            create_widget_for_element(my_ctxt->layouter, element,
                    WS_WIDGET_TYPE_HEADER, my_ctxt->tabbed_window);
        }
        else if (strcasecmp(name, "MENU") == 0) {
            create_widget_for_element(my_ctxt->layouter, element,
                    WS_WIDGET_TYPE_MENUBAR, my_ctxt->tabbed_window);
        }
        else if (strcasecmp(name, "NAV") == 0) {
            create_widget_for_element(my_ctxt->layouter, element,
                    WS_WIDGET_TYPE_TOOLBAR, my_ctxt->tabbed_window);
        }
        else if (strcasecmp(name, "ASIDE") == 0) {
            create_widget_for_element(my_ctxt->layouter, element,
                    WS_WIDGET_TYPE_SIDEBAR, my_ctxt->tabbed_window);
        }
        else if (strcasecmp(name, "FOOTER") == 0) {
            create_widget_for_element(my_ctxt->layouter, element,
                    WS_WIDGET_TYPE_FOOTER, my_ctxt->tabbed_window);
        }
        else if (strcasecmp(name, "OL") == 0) {
            create_widget_for_element(my_ctxt->layouter, element,
                    WS_WIDGET_TYPE_PANEL, my_ctxt->tabbed_window);
        }
        else if (strcasecmp(name, "UL") == 0) {
            create_widget_for_element(my_ctxt->layouter, element,
                    WS_WIDGET_TYPE_TABHOST, my_ctxt->tabbed_window);
        }
        else if (strcasecmp(name, "DIV") == 0) {
            if (node->first_child != NULL) {
                return PCHTML_ACTION_OK;
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

static bool create_tabbed_window(struct ws_layouter *layouter,
        pcdom_element_t *article)
{
    void *tabbed_window;

    tabbed_window = get_element_user_data(article);
    if (tabbed_window == NULL) {
        tabbed_window = create_widget_for_element(layouter, article,
                WS_WIDGET_TYPE_TABBEDWINDOW, NULL);
    }

    if (tabbed_window == NULL)
        return false;

    struct create_widget_ctxt ctxt = { layouter, tabbed_window };
    pcdom_node_simple_walk(pcdom_interface_node(article),
            create_widget_walker, &ctxt);
    return true;
}

#define HTML_FRAG_PAGE  \
    "<li id='%s-%s' class='%s' name='%s' title='%s'></li>"

void *ws_layouter_add_page(struct ws_layouter *layouter,
        const char *group_id, const char *page_name,
        const char *class_name, const char *title, purc_variant_t style,
        int *retv)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = dom_get_element_by_id(dom_doc, group_id);

    void *widget = NULL;

    if (element) {
        ws_widget_type_t widget_type = WS_WIDGET_TYPE_NONE;

        /* the element must be a `ol` or `ul` element */
        if (has_tag(element, "OL")) {
            widget_type = WS_WIDGET_TYPE_PLAINPAGE;
        }
        else if (has_tag(element, "UL")) {
            widget_type = WS_WIDGET_TYPE_TABPAGE;
        }

        if (widget_type == WS_WIDGET_TYPE_NONE) {
            purc_log_error("Container is not a `OL` or `UL` element (%s)\n",
                    group_id);
            *retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        /* the element must be a descendant of an `article` element */
        pcdom_element_t *article = find_article_ancestor(element);
        if (article == NULL) {
            purc_log_error("Cannot find the ancestor `article` element (%s)\n",
                    group_id);
            *retv = PCRDR_SC_BAD_REQUEST;
            goto failed;
        }

        pcdom_node_t *subtree;
        gchar *html_fragment = g_strdup_printf(HTML_FRAG_PAGE,
                group_id, page_name, class_name ? class_name : "",
                page_name, title ? title : "");
        subtree = dom_parse_fragment(dom_doc, element,
            html_fragment, strlen(html_fragment));
        g_free(html_fragment);

        if (subtree) {
            dom_append_subtree_to_element(dom_doc, element, subtree);

            /* re-layout the exsiting widgets */
            relayout(layouter, article);

            void *parent = get_element_user_data(element);
            if (parent == NULL) {
                /* create the ancestor widgets */
                if (create_tabbed_window(layouter, article)) {
                    parent = get_element_user_data(element);
                }
            }

            if (parent == NULL) {
                *retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
                goto failed;
            }

            pcdom_element_t *li = find_page_element(dom_doc,
                    group_id, page_name);
            assert(li);

            if (create_widget_for_element(layouter, li,
                        widget_type, parent) == NULL) {
                *retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
                goto failed;
            }

            *retv = PCRDR_SC_OK;
        }
        else {
            *retv = PCRDR_SC_INTERNAL_SERVER_ERROR;
        }
    }
    else {
        *retv = PCRDR_SC_NOT_FOUND;
    }

failed:
    return widget;
}

int ws_layouter_remove_page_by_id(struct ws_layouter *layouter,
        const char *group_id, const char *page_name)
{
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);
    pcdom_element_t *element = find_page_element(dom_doc, group_id, page_name);

    /* the element must be a `li` element */
    if (element && has_tag(element, "LI")) {
        pcdom_element_t *article = find_article_ancestor(element);

        destroy_widget_for_element(layouter, element);
        dom_erase_element(dom_doc, element);
        relayout(layouter, article);

        return PCRDR_SC_OK;
    }

    return PCRDR_SC_NOT_FOUND;
}

int ws_layouter_remove_page_by_widget(struct ws_layouter *layouter,
        void *widget)
{
    void *data;
    pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);

    if (sorted_array_find(layouter->sa_widget, PTR2U64(widget), &data)) {
        pcdom_element_t *element = data;
        assert(element);

        /* the element must be a `LI` element */
        if (has_tag(element, "LI")) {
            pcdom_element_t *article = find_article_ancestor(element);

            destroy_widget_for_element(layouter, element);
            dom_erase_element(dom_doc, element);
            relayout(layouter, article);

            return PCRDR_SC_OK;
        }
    }

    return PCRDR_SC_NOT_FOUND;
}

int ws_layouter_update_widget(struct ws_layouter *layouter,
        void *widget, const char *property, purc_variant_t value)
{
    struct ws_widget_style style = { 0 };

    if (strcasecmp(property, "name") == 0) {
        return PCRDR_SC_FORBIDDEN;
    }
    else if (strcasecmp(property, "class") == 0) {
        const char *class;
        size_t len;

        if ((class = purc_variant_get_string_const_ex(value, &len))) {
            void *data;
            pcdom_document_t *dom_doc =
                pcdom_interface_document(layouter->dom_doc);

            if (sorted_array_find(layouter->sa_widget,
                        PTR2U64(widget), &data)) {
                pcdom_element_t *element = data;

                bool retb;
                if (len > 0) {
                    retb = dom_update_element(dom_doc, element,
                            "attr.class", class, len);
                }
                else {
                    retb = dom_remove_element_property(dom_doc, element,
                            "attr.class");
                }

                if (retb) {
                    pcdom_element_t *section = find_section_ancestor(element);
                    relayout(layouter, section);
                    goto done;
                }
            }
        }

        return PCRDR_SC_BAD_REQUEST;
    }
    else if (strcasecmp(property, "title") == 0) {
        if ((style.title = purc_variant_get_string_const(value))) {
            style.flags |= WSWS_FLAG_TITLE;
            layouter->cb_update_widget(layouter->ws_ctxt, widget, &style);
            goto done;
        }

        return PCRDR_SC_BAD_REQUEST;
    }
    else if (strcasecmp(property, "style") == 0) {
        return PCRDR_SC_NOT_IMPLEMENTED; /* TODO */
    }

done:
    return PCRDR_SC_OK;
}

ws_widget_type_t ws_layouter_retrieve_widget(struct ws_layouter *layouter,
        void *widget)
{
    void *data;
    ws_widget_type_t type = WS_WIDGET_TYPE_NONE;
    // pcdom_document_t *dom_doc = pcdom_interface_document(layouter->dom_doc);

    if (sorted_array_find(layouter->sa_widget, PTR2U64(widget), &data)) {
        pcdom_element_t *element = data;

        size_t len;
        const char *tag;
        tag = (const char *)pcdom_element_local_name(element, &len);
        if (strcasecmp(tag, "FIGURE") == 0) {
            type = WS_WIDGET_TYPE_PLAINWINDOW;
        }
        else if (strcasecmp(tag, "HEADER") == 0) {
            type = WS_WIDGET_TYPE_HEADER;
        }
        else if (strcasecmp(tag, "MENU") == 0) {
            type = WS_WIDGET_TYPE_MENUBAR;
        }
        else if (strcasecmp(tag, "NAV") == 0) {
            type = WS_WIDGET_TYPE_TOOLBAR;
        }
        else if (strcasecmp(tag, "ASIDE") == 0) {
            type = WS_WIDGET_TYPE_SIDEBAR;
        }
        else if (strcasecmp(tag, "FOOTER") == 0) {
            type = WS_WIDGET_TYPE_FOOTER;
        }
        else if (strcasecmp(tag, "UL") == 0) {
            type = WS_WIDGET_TYPE_TABHOST;
        }
        else if (strcasecmp(tag, "LI") == 0) {
            pcdom_element_t *parent = pcdom_interface_element(
                    pcdom_interface_node(element)->parent);

            tag = (const char *)pcdom_element_local_name(parent, &len);
            if (strcasecmp(tag, "OL") == 0) {
                type = WS_WIDGET_TYPE_PLAINPAGE;
            }
            else if (strcasecmp(tag, "UL") == 0) {
                type = WS_WIDGET_TYPE_TABPAGE;
            }
            else {
                type = WS_WIDGET_TYPE_NONE;
                purc_log_error("Parent of a LI is not a OL or UL (%s)\n",
                        tag);
            }
        }
    }

    return type;
}

