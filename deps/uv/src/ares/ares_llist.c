
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

#include "ares_setup.h"

#include "ares.h"
#include "ares_private.h"

/* Routines for managing doubly-linked circular linked lists with a
 * dummy head.
 */

/* Initialize a new head node */
void ares__init_list_head(struct list_node* head) {
  head->prev = head;
  head->next = head;
  head->data = NULL;
}

/* Initialize a list node */
void ares__init_list_node(struct list_node* node, void* d) {
  node->prev = NULL;
  node->next = NULL;
  node->data = d;
}

/* Returns true iff the given list is empty */
int ares__is_list_empty(struct list_node* head) {
  return ((head->next == head) && (head->prev == head));
}

/* Inserts new_node before old_node */
void ares__insert_in_list(struct list_node* new_node,
                          struct list_node* old_node) {
  new_node->next = old_node;
  new_node->prev = old_node->prev;
  old_node->prev->next = new_node;
  old_node->prev = new_node;
}

/* Removes the node from the list it's in, if any */
void ares__remove_from_list(struct list_node* node) {
  if (node->next != NULL) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = NULL;
    node->next = NULL;
  }
}

/* Swap the contents of two lists */
void ares__swap_lists(struct list_node* head_a,
                      struct list_node* head_b) {
  int is_a_empty = ares__is_list_empty(head_a);
  int is_b_empty = ares__is_list_empty(head_b);
  struct list_node old_a = *head_a;
  struct list_node old_b = *head_b;

  if (is_a_empty) {
    ares__init_list_head(head_b);
  } else {
    *head_b = old_a;
    old_a.next->prev = head_b;
    old_a.prev->next = head_b;
  }
  if (is_b_empty) {
    ares__init_list_head(head_a);
  } else {
    *head_a = old_b;
    old_b.next->prev = head_a;
    old_b.prev->next = head_a;
  }
}
