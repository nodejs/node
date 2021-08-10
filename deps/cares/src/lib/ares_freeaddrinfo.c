
/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2019 by Andrew Selivanov
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

#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif

#include "ares.h"
#include "ares_private.h"

void ares__freeaddrinfo_cnames(struct ares_addrinfo_cname *head)
{
  struct ares_addrinfo_cname *current;
  while (head)
    {
      current = head;
      head = head->next;
      ares_free(current->alias);
      ares_free(current->name);
      ares_free(current);
    }
}

void ares__freeaddrinfo_nodes(struct ares_addrinfo_node *head)
{
  struct ares_addrinfo_node *current;
  while (head)
    {
      current = head;
      head = head->ai_next;
      ares_free(current->ai_addr);
      ares_free(current);
    }
}

void ares_freeaddrinfo(struct ares_addrinfo *ai)
{
  if (ai == NULL)
    return;
  ares__freeaddrinfo_cnames(ai->cnames);
  ares__freeaddrinfo_nodes(ai->nodes);
  ares_free(ai);
}
