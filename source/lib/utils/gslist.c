/**
 * gslist.c -- A Singly link list implementation
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
 * Copyright (c) 2020 FMSoft <https://www.fmsoft.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gslist.h"

/* Instantiate a new Single linked-list node.
 *
 * On error, aborts if node can't be malloc'd.
 * On success, the gs_list node. */
gs_list *
gslist_create (void *data)
{
  gs_list *node = malloc (sizeof (gs_list));
  node->data = data;
  node->next = NULL;

  return node;
}

/* Create and insert a node after a given node.
 *
 * On error, aborts if node can't be malloc'd.
 * On success, the newly created node. */
gs_list *
gslist_insert_append (gs_list * node, void *data)
{
  gs_list *newnode;
  newnode = gslist_create (data);
  newnode->next = node->next;
  node->next = newnode;

  return newnode;
}

/* Create and insert a node in front of the list.
 *
 * On error, aborts if node can't be malloc'd.
 * On success, the newly created node. */
gs_list *
gslist_insert_prepend (gs_list * list, void *data)
{
  gs_list *newnode;
  newnode = gslist_create (data);
  newnode->next = list;

  return newnode;
}

/* Find a node given a pointer to a function that compares them.
 *
 * If comparison fails, NULL is returned.
 * On success, the existing node is returned. */
gs_list *
gslist_find (gs_list * node, int (*func) (void *, void *), void *data)
{
  while (node) {
    if (func (node->data, data) > 0)
      return node;
    node = node->next;
  }

  return NULL;
}

/* Remove all nodes from the list.
 *
 * On success, 0 is returned. */
int
gslist_remove_nodes (gs_list * list)
{
  gs_list *tmp;
  while (list != NULL) {
    tmp = list->next;
    /* VW: do not free data
    if (list->data)
      free (list->data);
    */
    free (list);
    list = tmp;
  }

  return 0;
}

/* Remove the given node from the list.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
int
gslist_remove_node (gs_list ** list, gs_list * node)
{
  gs_list **current = list, *next = NULL;
  for (; *current; current = &(*current)->next) {
    if ((*current) != node)
      continue;

    next = (*current)->next;
    /* VW: do not free data
    if ((*current)->data)
      free ((*current)->data);
    */
    free (*current);
    *current = next;
    return 0;
  }
  return 1;
}

/* Iterate over the single linked-list and call function pointer.
 *
 * If function pointer does not return 0, -1 is returned.
 * On success, 0 is returned. */
int
gslist_foreach (gs_list * node, int (*func) (void *, void *), void *user_data)
{
  while (node) {
    if (func (node->data, user_data) != 0)
      return -1;
    node = node->next;
  }

  return 0;
}

/* Count the number of elements on the linked-list.
 *
 * On success, the number of elements is returned. */
int
gslist_count (gs_list * node)
{
  int count = 0;
  while (node != 0) {
    count++;
    node = node->next;
  }
  return count;
}
