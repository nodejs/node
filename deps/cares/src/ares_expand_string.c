
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

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#else
#  include "nameser.h"
#endif

#include "ares.h"
#include "ares_private.h" /* for the memdebug */

/* Simply decodes a length-encoded character string. The first byte of the
 * input is the length of the string to be returned and the bytes thereafter
 * are the characters of the string. The returned result will be NULL
 * terminated.
 */
int ares_expand_string(const unsigned char *encoded,
                       const unsigned char *abuf,
                       int alen,
                       unsigned char **s,
                       long *enclen)
{
  unsigned char *q;
  union {
    ssize_t sig;
     size_t uns;
  } elen;

  if (encoded == abuf+alen)
    return ARES_EBADSTR;

  elen.uns = *encoded;
  if (encoded+elen.sig+1 > abuf+alen)
    return ARES_EBADSTR;

  encoded++;

  *s = ares_malloc(elen.uns+1);
  if (*s == NULL)
    return ARES_ENOMEM;
  q = *s;
  strncpy((char *)q, (char *)encoded, elen.uns);
  q[elen.uns] = '\0';

  *s = q;

  *enclen = (long)(elen.sig+1);

  return ARES_SUCCESS;
}

