
/* Copyright 1998, 2011 by the Massachusetts Institute of Technology.
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
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#  include <arpa/nameser_compat.h>
#endif

#include "ares.h"
#include "ares_nowarn.h"
#include "ares_private.h" /* for the memdebug */

/* Maximum number of indirections allowed for a name */
#define MAX_INDIRS 50

static int name_length(const unsigned char *encoded, const unsigned char *abuf,
                       int alen);

/* Expand an RFC1035-encoded domain name given by encoded.  The
 * containing message is given by abuf and alen.  The result given by
 * *s, which is set to a NUL-terminated allocated buffer.  *enclen is
 * set to the length of the encoded name (not the length of the
 * expanded name; the goal is to tell the caller how many bytes to
 * move forward to get past the encoded name).
 *
 * In the simple case, an encoded name is a series of labels, each
 * composed of a one-byte length (limited to values between 0 and 63
 * inclusive) followed by the label contents.  The name is terminated
 * by a zero-length label.
 *
 * In the more complicated case, a label may be terminated by an
 * indirection pointer, specified by two bytes with the high bits of
 * the first byte (corresponding to INDIR_MASK) set to 11.  With the
 * two high bits of the first byte stripped off, the indirection
 * pointer gives an offset from the beginning of the containing
 * message with more labels to decode.  Indirection can happen an
 * arbitrary number of times, so we have to detect loops.
 *
 * Since the expanded name uses '.' as a label separator, we use
 * backslashes to escape periods or backslashes in the expanded name.
 */

int ares_expand_name(const unsigned char *encoded, const unsigned char *abuf,
                     int alen, char **s, long *enclen)
{
  int len, indir = 0;
  char *q;
  const unsigned char *p;
  union {
    ares_ssize_t sig;
     size_t uns;
  } nlen;

  nlen.sig = name_length(encoded, abuf, alen);
  if (nlen.sig < 0)
    return ARES_EBADNAME;

  *s = ares_malloc(nlen.uns + 1);
  if (!*s)
    return ARES_ENOMEM;
  q = *s;

  if (nlen.uns == 0) {
    /* RFC2181 says this should be ".": the root of the DNS tree.
     * Since this function strips trailing dots though, it becomes ""
     */
    q[0] = '\0';

    /* indirect root label (like 0xc0 0x0c) is 2 bytes long (stupid, but
       valid) */
    if ((*encoded & INDIR_MASK) == INDIR_MASK)
      *enclen = 2L;
    else
      *enclen = 1L;  /* the caller should move one byte to get past this */

    return ARES_SUCCESS;
  }

  /* No error-checking necessary; it was all done by name_length(). */
  p = encoded;
  while (*p)
    {
      if ((*p & INDIR_MASK) == INDIR_MASK)
        {
          if (!indir)
            {
              *enclen = aresx_uztosl(p + 2U - encoded);
              indir = 1;
            }
          p = abuf + ((*p & ~INDIR_MASK) << 8 | *(p + 1));
        }
      else
        {
          len = *p;
          p++;
          while (len--)
            {
              if (*p == '.' || *p == '\\')
                *q++ = '\\';
              *q++ = *p;
              p++;
            }
          *q++ = '.';
        }
    }
  if (!indir)
    *enclen = aresx_uztosl(p + 1U - encoded);

  /* Nuke the trailing period if we wrote one. */
  if (q > *s)
    *(q - 1) = 0;
  else
    *q = 0; /* zero terminate; LCOV_EXCL_LINE: empty names exit above */

  return ARES_SUCCESS;
}

/* Return the length of the expansion of an encoded domain name, or
 * -1 if the encoding is invalid.
 */
static int name_length(const unsigned char *encoded, const unsigned char *abuf,
                       int alen)
{
  int n = 0, offset, indir = 0, top;

  /* Allow the caller to pass us abuf + alen and have us check for it. */
  if (encoded >= abuf + alen)
    return -1;

  while (*encoded)
    {
      top = (*encoded & INDIR_MASK);
      if (top == INDIR_MASK)
        {
          /* Check the offset and go there. */
          if (encoded + 1 >= abuf + alen)
            return -1;
          offset = (*encoded & ~INDIR_MASK) << 8 | *(encoded + 1);
          if (offset >= alen)
            return -1;
          encoded = abuf + offset;

          /* If we've seen more indirects than the message length,
           * then there's a loop.
           */
          ++indir;
          if (indir > alen || indir > MAX_INDIRS)
            return -1;
        }
      else if (top == 0x00)
        {
          offset = *encoded;
          if (encoded + offset + 1 >= abuf + alen)
            return -1;
          encoded++;
          while (offset--)
            {
              n += (*encoded == '.' || *encoded == '\\') ? 2 : 1;
              encoded++;
            }
          n++;
        }
      else
        {
          /* RFC 1035 4.1.4 says other options (01, 10) for top 2
           * bits are reserved.
           */
          return -1;
        }
    }

  /* If there were any labels at all, then the number of dots is one
   * less than the number of labels, so subtract one.
   */
  return (n) ? n - 1 : n;
}

/* Like ares_expand_name but returns EBADRESP in case of invalid input. */
int ares__expand_name_for_response(const unsigned char *encoded,
                                   const unsigned char *abuf, int alen,
                                   char **s, long *enclen)
{
  int status = ares_expand_name(encoded, abuf, alen, s, enclen);
  if (status == ARES_EBADNAME)
    status = ARES_EBADRESP;
  return status;
}
