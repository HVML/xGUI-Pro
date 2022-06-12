/*
** dom-ops.c -- The module implementation for DOM operations.
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

#include "dom-ops.h"

#include "utils/sorted-array.h"

#include <purc/purc-helpers.h>

#include <assert.h>

#define SA_INITIAL_SIZE        8

#define PTR2U64(p)      ((uint64_t)(uintptr_t)p)

static const char *get_element_id(pcdom_element_t *element, size_t *sz)
{
    pcdom_attr_t *attr_id = pcdom_element_id_attribute(element);

    if (attr_id) {
        const char *value;
        size_t len;
        value = (const char *)pcdom_attr_value(attr_id, &len);
        if (sz)
            *sz = len;
        return value;
    }

    return NULL;
}

struct my_tree_walker_ctxt {
    bool mark_dirty;
    bool add_or_remove;
    struct sorted_array *sa;
};

static pchtml_action_t
my_tree_walker(pcdom_node_t *node, void *ctx)
{
    struct my_tree_walker_ctxt *ctxt = ctx;
    const char *id;

    switch (node->type) {
    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        id = get_element_id(pcdom_interface_element(node), NULL);
        if (id) {
            if (ctxt->add_or_remove) {
                if (sorted_array_add(ctxt->sa, PTR2U64(id), node)) {
                    purc_log_warn("Failed to store id/element pair\n");
                }
            }
            else {
                if (!sorted_array_remove(ctxt->sa, PTR2U64(id))) {
                    purc_log_warn("Failed to remove id/element pair\n");
                }
            }
        }

        if (ctxt->mark_dirty)
            node->flags |= NF_DIRTY;

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
sa_string_cmp(uint64_t sortv1, uint64_t sortv2)
{
    const char *str1 = (void *)(uintptr_t)sortv1;
    const char *str2 = (void *)(uintptr_t)sortv2;

    return strcmp(str1, str2);
}

static bool
dom_build_id_element_map(pcdom_document_t *dom_doc)
{
    struct sorted_array *sa;

    assert(dom_doc->user == NULL);
    sa = sorted_array_create(SAFLAG_DEFAULT, SA_INITIAL_SIZE, NULL,
            sa_string_cmp);
    if (sa == NULL) {
        return false;
    }

    struct my_tree_walker_ctxt ctxt = {
        .mark_dirty     = false,
        .add_or_remove  = true,
        .sa             = sa,
    };

    pcdom_node_simple_walk(&dom_doc->node, my_tree_walker, &ctxt);
    dom_doc->user = sa;
    return true;
}

static bool
dom_destroy_id_element_map(pcdom_document_t *dom_doc)
{
    if (dom_doc->user == NULL) {
        return false;
    }

    sorted_array_destroy(dom_doc->user);
    dom_doc->user = NULL;
    return true;
}

pcdom_element_t *
dom_get_element_by_id(pcdom_document_t *dom_doc, const char *id)
{
    void* data;
    if (sorted_array_find(dom_doc->user, PTR2U64(id), &data))
        return (pcdom_element_t *)data;

    return NULL;
}

bool dom_prepare_user_data(pcdom_document_t *dom_doc)
{
    if (dom_doc->user == NULL) {
        dom_build_id_element_map(dom_doc);
    }
    else
        return false;

    return dom_doc->user != NULL;
}

bool dom_cleanup_user_data(pcdom_document_t *dom_doc)
{
    if (dom_doc->user == NULL) {
        return false;
    }

    dom_destroy_id_element_map(dom_doc);
    dom_doc->user = NULL;
    return true;
}

static bool
dom_merge_id_element_map(pcdom_document_t *dom_doc, pcdom_node_t *subtree)
{
    assert(dom_doc->user);

    struct my_tree_walker_ctxt ctxt = {
        .mark_dirty     = true,
        .add_or_remove  = true,
        .sa             = dom_doc->user,
    };

    pcdom_node_simple_walk(subtree, my_tree_walker, &ctxt);
    return true;
}

static bool
dom_subtract_id_element_map(pcdom_document_t *dom_doc, pcdom_node_t *subtree)
{
    assert(dom_doc->user);

    struct my_tree_walker_ctxt ctxt = {
        .mark_dirty     = false,
        .add_or_remove  = false,
        .sa             = dom_doc->user,
    };

    pcdom_node_simple_walk(subtree, my_tree_walker, &ctxt);
    return true;
}

pcdom_node_t *
dom_parse_fragment(pcdom_document_t *dom_doc,
        pcdom_element_t *parent, const char *fragment, size_t length)
{
    unsigned int status;
    pchtml_html_document_t *html_doc;
    pcdom_node_t *root = NULL;

    html_doc = (pchtml_html_document_t *)dom_doc;
    status = pchtml_html_document_parse_fragment_chunk_begin(html_doc, parent);
    if (status)
        goto failed;

    status = pchtml_html_document_parse_fragment_chunk(html_doc,
            (const unsigned char*)"<div>", 5);
    if (status)
        goto failed;

    status = pchtml_html_document_parse_fragment_chunk(html_doc,
            (const unsigned char*)fragment, length);
    if (status)
        goto failed;

    status = pchtml_html_document_parse_fragment_chunk(html_doc,
            (const unsigned char*)"</div>", 6);
    if (status)
        goto failed;

    root = pchtml_html_document_parse_fragment_chunk_end(html_doc);

failed:
    return root;
}

void
dom_append_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *parent = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_id_element_map(dom_doc, div);
        while (div->first_child) {
            pcdom_node_t *child = div->first_child;
            pcdom_node_remove(child);
            pcdom_node_append_child(parent, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_prepend_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *parent = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_id_element_map(dom_doc, div);
        while (div->last_child) {
            pcdom_node_t *child = div->last_child;
            pcdom_node_remove(child);
            pcdom_node_prepend_child(parent, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_insert_subtree_before_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *to = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_id_element_map(dom_doc, div);
        while (div->last_child) {
            pcdom_node_t *child = div->last_child;
            pcdom_node_remove(child);
            pcdom_node_insert_before(to, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_insert_subtree_after_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *to = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_id_element_map(dom_doc, div);
        while (div->first_child) {
            pcdom_node_t *child = div->first_child;
            pcdom_node_remove(child);
            pcdom_node_insert_after(to, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_displace_subtree_of_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *parent = pcdom_interface_node(element);

    dom_subtract_id_element_map(dom_doc, parent);
    while (parent->first_child != NULL) {
        pcdom_node_destroy_deep(parent->first_child);
    }

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_id_element_map(dom_doc, div);
        while (div->first_child) {
            pcdom_node_t *child = div->first_child;
            pcdom_node_remove(child);
            pcdom_node_append_child(parent, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_destroy_subtree(pcdom_node_t *subtree)
{
    pcdom_node_destroy_deep(subtree);
}

void
dom_erase_element(pcdom_document_t *dom_doc, pcdom_element_t *element)
{
    pcdom_node_t *node = pcdom_interface_node(element);
    const char *id;

    dom_subtract_id_element_map(dom_doc, node);
    pcdom_node_destroy_deep(node);

    id = get_element_id(element, NULL);
    if (id) {
        if (!sorted_array_remove(dom_doc->user, PTR2U64(id))) {
            purc_log_warn("Failed to store id/element pair\n");
        }
    }
}

void
dom_clear_element(pcdom_document_t *dom_doc, pcdom_element_t *element)
{
    pcdom_node_t *parent = pcdom_interface_node(element);
    dom_subtract_id_element_map(dom_doc, parent);
    while (parent->first_child != NULL) {
        pcdom_node_destroy_deep(parent->first_child);
    }
}

bool
dom_update_element(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property, const char* content, size_t sz_cnt)
{
    bool retv;

    if (strcmp(property, "textContent") == 0) {
        pcdom_node_t *parent = pcdom_interface_node(element);
        pcdom_text_t *text_node;

        text_node = pcdom_document_create_text_node(dom_doc,
            (const unsigned char *)content, sz_cnt);
        if (text_node) {
            dom_subtract_id_element_map(dom_doc, parent);
            pcdom_node_replace_all(parent, pcdom_interface_node(text_node));
        }

        retv = text_node ? true : false;
    }
    else if (strncmp(property, "attr.", 5) == 0) {
        property += 5;
        pcdom_attr_t *attr;

        attr = pcdom_element_set_attribute(element,
                (const unsigned char*)property, strlen(property),
                (const unsigned char*)content, sz_cnt);
        retv = attr ? true : false;
    }
    else {
        retv = false;
    }

    return retv;
}

bool
dom_remove_element_attr(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property)
{
    bool retv = false;

    if (strncmp(property, "attr.", 5) == 0) {
        property += 5;

        if (pcdom_element_remove_attribute(element,
                (const unsigned char*)property,
                strlen(property)) == PURC_ERROR_OK)
            retv = true;
    }

    return retv;
}

