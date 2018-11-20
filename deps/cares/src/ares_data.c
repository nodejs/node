
/* Copyright (C) 2009-2013 by Daniel Stenberg
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

#include <stddef.h>

#include "ares.h"
#include "ares_data.h"
#include "ares_private.h"


/*
** ares_free_data() - c-ares external API function.
**
** This function must be used by the application to free data memory that
** has been internally allocated by some c-ares function and for which a
** pointer has already been returned to the calling application. The list
** of c-ares functions returning pointers that must be free'ed using this
** function is:
**
**   ares_get_servers()
**   ares_parse_srv_reply()
**   ares_parse_txt_reply()
*/

void ares_free_data(void *dataptr)
{
  struct ares_data *ptr;

  if (!dataptr)
    return;

#ifdef __INTEL_COMPILER
#  pragma warning(push)
#  pragma warning(disable:1684)
   /* 1684: conversion from pointer to same-sized integral type */
#endif

  ptr = (void *)((char *)dataptr - offsetof(struct ares_data, data));

#ifdef __INTEL_COMPILER
#  pragma warning(pop)
#endif

  if (ptr->mark != ARES_DATATYPE_MARK)
    return;

  switch (ptr->type)
    {
      case ARES_DATATYPE_MX_REPLY:

        if (ptr->data.mx_reply.next)
          ares_free_data(ptr->data.mx_reply.next);
        if (ptr->data.mx_reply.host)
          free(ptr->data.mx_reply.host);
        break;

      case ARES_DATATYPE_SRV_REPLY:

        if (ptr->data.srv_reply.next)
          ares_free_data(ptr->data.srv_reply.next);
        if (ptr->data.srv_reply.host)
          free(ptr->data.srv_reply.host);
        break;

      case ARES_DATATYPE_TXT_REPLY:

        if (ptr->data.txt_reply.next)
          ares_free_data(ptr->data.txt_reply.next);
        if (ptr->data.txt_reply.txt)
          free(ptr->data.txt_reply.txt);
        break;

      case ARES_DATATYPE_ADDR_NODE:

        if (ptr->data.addr_node.next)
          ares_free_data(ptr->data.addr_node.next);
        break;

      case ARES_DATATYPE_NAPTR_REPLY:

        if (ptr->data.naptr_reply.next)
          ares_free_data(ptr->data.naptr_reply.next);
        if (ptr->data.naptr_reply.flags)
          free(ptr->data.naptr_reply.flags);
        if (ptr->data.naptr_reply.service)
          free(ptr->data.naptr_reply.service);
        if (ptr->data.naptr_reply.regexp)
          free(ptr->data.naptr_reply.regexp);
        if (ptr->data.naptr_reply.replacement)
          free(ptr->data.naptr_reply.replacement);
        break;

      case ARES_DATATYPE_SOA_REPLY:
        if (ptr->data.soa_reply.nsname)
          free(ptr->data.soa_reply.nsname);
        if (ptr->data.soa_reply.hostmaster)
          free(ptr->data.soa_reply.hostmaster);
	break;

      default:
        return;
    }

  free(ptr);
}


/*
** ares_malloc_data() - c-ares internal helper function.
**
** This function allocates memory for a c-ares private ares_data struct
** for the specified ares_datatype, initializes c-ares private fields
** and zero initializes those which later might be used from the public
** API. It returns an interior pointer which can be passed by c-ares
** functions to the calling application, and that must be free'ed using
** c-ares external API function ares_free_data().
*/

void *ares_malloc_data(ares_datatype type)
{
  struct ares_data *ptr;

  ptr = malloc(sizeof(struct ares_data));
  if (!ptr)
    return NULL;

  switch (type)
    {
      case ARES_DATATYPE_MX_REPLY:
        ptr->data.mx_reply.next = NULL;
        ptr->data.mx_reply.host = NULL;
        ptr->data.mx_reply.priority = 0;
        break;

      case ARES_DATATYPE_SRV_REPLY:
        ptr->data.srv_reply.next = NULL;
        ptr->data.srv_reply.host = NULL;
        ptr->data.srv_reply.priority = 0;
        ptr->data.srv_reply.weight = 0;
        ptr->data.srv_reply.port = 0;
        break;

      case ARES_DATATYPE_TXT_REPLY:
        ptr->data.txt_reply.next = NULL;
        ptr->data.txt_reply.txt = NULL;
        ptr->data.txt_reply.length = 0;
        break;

      case ARES_DATATYPE_ADDR_NODE:
        ptr->data.addr_node.next = NULL;
        ptr->data.addr_node.family = 0;
        memset(&ptr->data.addr_node.addrV6, 0,
               sizeof(ptr->data.addr_node.addrV6));
        break;

      case ARES_DATATYPE_NAPTR_REPLY:
        ptr->data.naptr_reply.next = NULL;
        ptr->data.naptr_reply.flags = NULL;
        ptr->data.naptr_reply.service = NULL;
        ptr->data.naptr_reply.regexp = NULL;
        ptr->data.naptr_reply.replacement = NULL;
        ptr->data.naptr_reply.order = 0;
        ptr->data.naptr_reply.preference = 0;
        break;

      case ARES_DATATYPE_SOA_REPLY:
        ptr->data.soa_reply.nsname = NULL;
        ptr->data.soa_reply.hostmaster = NULL;
        ptr->data.soa_reply.serial = 0;
        ptr->data.soa_reply.refresh = 0;
        ptr->data.soa_reply.retry = 0;
        ptr->data.soa_reply.expire = 0;
        ptr->data.soa_reply.minttl = 0;
	break;

      default:
        free(ptr);
        return NULL;
    }

  ptr->mark = ARES_DATATYPE_MARK;
  ptr->type = type;

  return &ptr->data;
}
