/* MIT License
 *
 * Copyright (c) 1998, 2011 Massachusetts Institute of Technology
 * Copyright (c) The c-ares project and its contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ares_setup.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#include "ares_nameser.h"

#include "ares.h"
#include "ares_nowarn.h"
#include "ares_private.h" /* for the memdebug */

/* Maximum number of indirections allowed for a name */
#define MAX_INDIRS 50

static int name_length(const unsigned char *encoded, const unsigned char *abuf,
                       int alen, int is_hostname);

/* Reserved characters for names that need to be escaped */
static int is_reservedch(int ch)
{
  switch (ch) {
    case '"':
    case '.':
    case ';':
    case '\\':
    case '(':
    case ')':
    case '@':
    case '$':
      return 1;
    default:
      break;
  }

  return 0;
}

static int ares__isprint(int ch)
{
  if (ch >= 0x20 && ch <= 0x7E)
    return 1;
  return 0;
}

/* Character set allowed by hostnames.  This is to include the normal
 * domain name character set plus:
 *  - underscores which are used in SRV records.
 *  - Forward slashes such as are used for classless in-addr.arpa 
 *    delegation (CNAMEs)
 *  - Asterisks may be used for wildcard domains in CNAMEs as seen in the
 *    real world.
 * While RFC 2181 section 11 does state not to do validation,
 * that applies to servers, not clients.  Vulnerabilities have been
 * reported when this validation is not performed.  Security is more
 * important than edge-case compatibility (which is probably invalid
 * anyhow). */
static int is_hostnamech(int ch)
{
  /* [A-Za-z0-9-*._/]
   * Don't use isalnum() as it is locale-specific
   */
  if (ch >= 'A' && ch <= 'Z')
    return 1;
  if (ch >= 'a' && ch <= 'z')
    return 1;
  if (ch >= '0' && ch <= '9')
    return 1;
  if (ch == '-' || ch == '.' || ch == '_' || ch == '/' || ch == '*')
    return 1;

  return 0;
}

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
 *
 * If the result is expected to be a hostname, then no escaped data is allowed
 * and will return error.
 */

int ares__expand_name_validated(const unsigned char *encoded,
                                const unsigned char *abuf,
                                int alen, char **s, long *enclen,
                                int is_hostname)
{
  int len, indir = 0;
  char *q;
  const unsigned char *p;
  union {
    ares_ssize_t sig;
     size_t uns;
  } nlen;

  nlen.sig = name_length(encoded, abuf, alen, is_hostname);
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
          int name_len = *p;
          len = name_len;
          p++;

          while (len--)
            {
              /* Output as \DDD for consistency with RFC1035 5.1, except
               * for the special case of a root name response  */
              if (!ares__isprint(*p) && !(name_len == 1 && *p == 0))
                {
                  *q++ = '\\';
                  *q++ = (char)('0' + *p / 100);
                  *q++ = (char)('0' + (*p % 100) / 10);
                  *q++ = (char)('0' + (*p % 10));
                }
              else if (is_reservedch(*p))
                {
                  *q++ = '\\';
                  *q++ = *p;
                }
              else
                {
                  *q++ = *p;
                }
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


int ares_expand_name(const unsigned char *encoded, const unsigned char *abuf,
                     int alen, char **s, long *enclen)
{
  return ares__expand_name_validated(encoded, abuf, alen, s, enclen, 0);
}

/* Return the length of the expansion of an encoded domain name, or
 * -1 if the encoding is invalid.
 */
static int name_length(const unsigned char *encoded, const unsigned char *abuf,
                       int alen, int is_hostname)
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
          int name_len = *encoded;
          offset = name_len;
          if (encoded + offset + 1 >= abuf + alen)
            return -1;
          encoded++;

          while (offset--)
            {
              if (!ares__isprint(*encoded) && !(name_len == 1 && *encoded == 0))
                {
                  if (is_hostname)
                    return -1;
                  n += 4;
                }
              else if (is_reservedch(*encoded))
                {
                  if (is_hostname)
                    return -1;
                  n += 2;
                }
              else
                {
                  if (is_hostname && !is_hostnamech(*encoded))
                    return -1;
                  n += 1;
                }
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

/* Like ares_expand_name_validated  but returns EBADRESP in case of invalid
 * input. */
int ares__expand_name_for_response(const unsigned char *encoded,
                                   const unsigned char *abuf, int alen,
                                   char **s, long *enclen, int is_hostname)
{
  int status = ares__expand_name_validated(encoded, abuf, alen, s, enclen,
    is_hostname);
  if (status == ARES_EBADNAME)
    status = ARES_EBADRESP;
  return status;
}
