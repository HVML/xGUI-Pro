/*
** dom-ops.h -- The module interface for DOM operations.
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

#ifndef XGUIPRO_LAYOUTER_DOM_OPS_H
#define XGUIPRO_LAYOUTER_DOM_OPS_H

#include <strings.h>

#include <purc/purc-dom.h>
#include <purc/purc-html.h>

#define NF_UNFOLDED         0x0001
#define NF_DIRTY            0x0002

#ifdef __cplusplus
extern "C" {
#endif

bool dom_prepare_user_data(pcdom_document_t *dom_doc);

bool dom_cleanup_user_data(pcdom_document_t *dom_doc);

pcdom_element_t *dom_get_element_by_id(pcdom_document_t *dom_doc,
        const char *id);

pcdom_node_t *dom_parse_fragment(pcdom_document_t *dom_doc,
        pcdom_element_t *parent, const char *fragment, size_t length);

void dom_append_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_prepend_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_insert_subtree_before_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_insert_subtree_after_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_displace_subtree_of_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_destroy_subtree(pcdom_node_t *subtree);

void dom_erase_element(pcdom_document_t *dom_doc, pcdom_element_t *element);

void dom_clear_element(pcdom_document_t *dom_doc, pcdom_element_t *element);

bool dom_update_element(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property, const char* content, size_t sz_cnt);

bool dom_remove_element_property(pcdom_document_t *dom_doc,
        pcdom_element_t *element, const char* property);

#ifdef __cplusplus
}
#endif

static inline
void set_element_user_data(pcdom_element_t *element, void *data)
{
    pcdom_node_t *node = pcdom_interface_node(element);
    node->user = data;
}

static inline
void *get_element_user_data(pcdom_element_t *element)
{
    pcdom_node_t *node = pcdom_interface_node(element);
    return node->user;
}

static inline bool
is_an_element_with_tag(pcdom_node_t *node, const char *tag)
{
    if (node && node->type == PCDOM_NODE_TYPE_COMMENT) {
        pcdom_element_t *element = pcdom_interface_element(node);
        size_t len;
        const char *tag_name;

        tag_name = (const char *)pcdom_element_local_name(element, &len);
        return strcasecmp(tag, tag_name) == 0;
    }

    return false;
}

#endif /* XGUIPRO_LAYOUTER_DOM_OPS_H */

