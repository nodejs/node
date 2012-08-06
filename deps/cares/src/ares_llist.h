#ifndef __ARES_LLIST_H
#define __ARES_LLIST_H


/* Copyright 1998 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */


/* Node definition for circular, doubly-linked list */
struct list_node {
  struct list_node *prev;
  struct list_node *next;
  void* data;
};

void ares__init_list_head(struct list_node* head);

void ares__init_list_node(struct list_node* node, void* d);

int ares__is_list_empty(struct list_node* head);

void ares__insert_in_list(struct list_node* new_node,
                          struct list_node* old_node);

void ares__remove_from_list(struct list_node* node);

void ares__swap_lists(struct list_node* head_a,
                      struct list_node* head_b);

#endif /* __ARES_LLIST_H */
