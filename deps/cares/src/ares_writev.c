

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

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#include "ares.h"
#include "ares_private.h"

#ifndef HAVE_WRITEV
ssize_t ares_writev(ares_socket_t s, const struct iovec *iov, int iovcnt)
{
  char *buffer, *bp;
  int i;
  size_t bytes = 0;
  ssize_t result;

  /* Validate iovcnt */
  if (iovcnt <= 0)
  {
    SET_ERRNO(EINVAL);
    return (-1);
  }

  /* Validate and find the sum of the iov_len values in the iov array */
  for (i = 0; i < iovcnt; i++)
  {
    if (iov[i].iov_len > INT_MAX - bytes)
    {
      SET_ERRNO(EINVAL);
      return (-1);
    }
    bytes += iov[i].iov_len;
  }

  if (bytes == 0)
    return (0);

  /* Allocate a temporary buffer to hold the data */
  buffer = ares_malloc(bytes);
  if (!buffer)
  {
    SET_ERRNO(ENOMEM);
    return (-1);
  }

  /* Copy the data into buffer */
  for (bp = buffer, i = 0; i < iovcnt; ++i)
  {
    memcpy (bp, iov[i].iov_base, iov[i].iov_len);
    bp += iov[i].iov_len;
  }

  /* Send buffer contents */
  result = swrite(s, buffer, bytes);

  ares_free(buffer);

  return (result);
}
#endif

