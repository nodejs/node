/*
 * Copyright (c) 2012 by Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ares_private.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "ares_nameser.h"

#include "ares_ipv6.h"
#include "ares_inet_net_pton.h"

#ifdef USE_WINSOCK
#  define SOCKERRNO        ((int)WSAGetLastError())
#  define SET_SOCKERRNO(x) (WSASetLastError((int)(x)))
#  undef EMSGSIZE
#  define EMSGSIZE WSAEMSGSIZE
#  undef ENOENT
#  define ENOENT WSA_INVALID_PARAMETER
#  undef EAFNOSUPPORT
#  define EAFNOSUPPORT WSAEAFNOSUPPORT
#else
#  define SOCKERRNO        (errno)
#  define SET_SOCKERRNO(x) (errno = (x))
#endif

const struct ares_in6_addr ares_in6addr_any = { { { 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                    0, 0, 0, 0, 0, 0, 0 } } };

/*
 * static int
 * inet_net_pton_ipv4(src, dst, size, allow_cidr)
 *      convert IPv4 network number from presentation to network format.
 *      accepts hex octets, hex strings, decimal octets, and, if
 *      "allow_cidr" is true, /CIDR.  "size" is in bytes and describes "dst".
 * return:
 *      number of bits, either imputed classfully or specified with /CIDR,
 *      or -1 if some failure occurred (check errno).  ENOENT means it was
 *      not an IPv4 network specification (including a /CIDR suffix when
 *      "allow_cidr" is false).
 * note:
 *      network byte order assumed.  this means 192.5.5.240/28 has
 *      0b11110000 in its fourth octet.
 * note:
 *      On Windows we store the error in the thread errno, not
 *      in the winsock error code. This is to avoid losing the
 *      actual last winsock error. So use macro ERRNO to fetch the
 *      errno this function sets when returning (-1), not SOCKERRNO.
 * author:
 *      Paul Vixie (ISC), June 1996
 */
static int ares_inet_net_pton_ipv4(const char *src, unsigned char *dst,
                                   size_t size, ares_bool_t allow_cidr)
{
  static const char    xdigits[] = "0123456789abcdef";
  static const char    digits[]  = "0123456789";
  int                  n;
  int                  ch;
  int                  tmp = 0;
  int                  dirty;
  int                  bits;
  const unsigned char *odst = dst;

  ch = *src++;
  if (ch == '0' && (src[0] == 'x' || src[0] == 'X') && ares_isascii(src[1]) &&
      ares_isxdigit(src[1])) {
    /* Hexadecimal: Eat nybble string. */
    if (!size) {
      goto emsgsize;
    }
    dirty = 0;
    src++; /* skip x or X. */
    while ((ch = *src++) != '\0' && ares_isascii(ch) && ares_isxdigit(ch)) {
      if (ares_isupper(ch)) {
        ch = ares_tolower((unsigned char)ch);
      }
      n = (int)(strchr(xdigits, ch) - xdigits);
      if (dirty == 0) {
        tmp = n;
      } else {
        tmp = (tmp << 4) | n;
      }
      if (++dirty == 2) {
        if (!size--) {
          goto emsgsize;
        }
        *dst++ = (unsigned char)tmp;
        dirty  = 0;
      }
    }
    if (dirty) { /* Odd trailing nybble? */
      if (!size--) {
        goto emsgsize;
      }
      *dst++ = (unsigned char)(tmp << 4);
    }
  } else if (ares_isascii(ch) && ares_isdigit(ch)) {
    /* Decimal: eat dotted digit string. */
    for (;;) {
      tmp = 0;
      do {
        n    = (int)(strchr(digits, ch) - digits);
        tmp *= 10;
        tmp += n;
        if (tmp > 255) {
          goto enoent;
        }
      } while ((ch = *src++) != '\0' && ares_isascii(ch) && ares_isdigit(ch));
      if (!size--) {
        goto emsgsize;
      }
      *dst++ = (unsigned char)tmp;
      if (ch == '\0' || ch == '/') {
        break;
      }
      if (ch != '.') {
        goto enoent;
      }
      ch = *src++;
      if (!ares_isascii(ch) || !ares_isdigit(ch)) {
        goto enoent;
      }
    }
  } else {
    goto enoent;
  }

  bits = -1;
  if (allow_cidr && ch == '/' && ares_isascii(src[0]) && ares_isdigit(src[0]) &&
      dst > odst) {
    /* CIDR width specifier.  Nothing can follow it. */
    ch   = *src++; /* Skip over the /. */
    bits = 0;
    do {
      n     = (int)(strchr(digits, ch) - digits);
      bits *= 10;
      bits += n;
      if (bits > 32) {
        goto enoent;
      }
    } while ((ch = *src++) != '\0' && ares_isascii(ch) && ares_isdigit(ch));
    if (ch != '\0') {
      goto enoent;
    }
  }

  /* Firey death and destruction unless we prefetched EOS. */
  if (ch != '\0') {
    goto enoent;
  }

  /* If nothing was written to the destination, we found no address. */
  if (dst == odst) {
    goto enoent; /* LCOV_EXCL_LINE: all valid paths above increment dst */
  }
  /* If no CIDR spec was given, infer width from net class. */
  if (bits == -1) {
    if (*odst >= 240) {        /* Class E */
      bits = 32;
    } else if (*odst >= 224) { /* Class D */
      bits = 8;
    } else if (*odst >= 192) { /* Class C */
      bits = 24;
    } else if (*odst >= 128) { /* Class B */
      bits = 16;
    } else {                   /* Class A */
      bits = 8;
    }
    /* If imputed mask is narrower than specified octets, widen. */
    if (bits < ((dst - odst) * 8)) {
      bits = (int)(dst - odst) * 8;
    }
    /*
     * If there are no additional bits specified for a class D
     * address adjust bits to 4.
     */
    if (bits == 8 && *odst == 224) {
      bits = 4;
    }
  }
  /* Extend network to cover the actual mask. */
  while (bits > ((dst - odst) * 8)) {
    if (!size--) {
      goto emsgsize;
    }
    *dst++ = '\0';
  }
  return bits;

enoent:
  SET_SOCKERRNO(ENOENT);
  return -1;

emsgsize:
  SET_SOCKERRNO(EMSGSIZE);
  return -1;
}

static int getbits(const char *src, size_t *bitsp)
{
  static const char digits[] = "0123456789";
  size_t            n;
  size_t            val;
  char              ch;

  val = 0;
  n   = 0;
  while ((ch = *src++) != '\0') {
    const char *pch;

    pch = strchr(digits, ch);
    if (pch != NULL) {
      if (n++ != 0 && val == 0) { /* no leading zeros */
        return 0;
      }
      val *= 10;
      val += (size_t)(pch - digits);
      if (val > 128) { /* range */
        return 0;
      }
      continue;
    }
    return 0;
  }
  if (n == 0) {
    return 0;
  }
  *bitsp = val;
  return 1;
}

static int ares_inet_pton6(const char *src, unsigned char *dst)
{
  static const char xdigits_l[] = "0123456789abcdef";
  static const char xdigits_u[] = "0123456789ABCDEF";
  unsigned char     tmp[NS_IN6ADDRSZ];
  unsigned char    *tp;
  unsigned char    *endp;
  unsigned char    *colonp;
  const char       *xdigits;
  const char       *curtok;
  int               ch;
  int               saw_xdigit;
  int               count_xdigit;
  unsigned int      val;

  memset((tp = tmp), '\0', NS_IN6ADDRSZ);
  endp   = tp + NS_IN6ADDRSZ;
  colonp = NULL;
  /* Leading :: requires some special handling. */
  if (*src == ':') {
    if (*++src != ':') {
      goto enoent;
    }
  }
  curtok     = src;
  saw_xdigit = count_xdigit = 0;
  val                       = 0;
  while ((ch = *src++) != '\0') {
    const char *pch;

    if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL) {
      pch = strchr((xdigits = xdigits_u), ch);
    }
    if (pch != NULL) {
      if (count_xdigit >= 4) {
        goto enoent;
      }
      val <<= 4;
      val  |= (unsigned int)(pch - xdigits);
      if (val > 0xffff) {
        goto enoent;
      }
      saw_xdigit = 1;
      count_xdigit++;
      continue;
    }
    if (ch == ':') {
      curtok = src;
      if (!saw_xdigit) {
        if (colonp) {
          goto enoent;
        }
        colonp = tp;
        continue;
      } else if (*src == '\0') {
        goto enoent;
      }
      if (tp + NS_INT16SZ > endp) {
        goto enoent;
      }
      *tp++        = (unsigned char)(val >> 8) & 0xff;
      *tp++        = (unsigned char)val & 0xff;
      saw_xdigit   = 0;
      count_xdigit = 0;
      val          = 0;
      continue;
    }
    if (ch == '.' && ((tp + NS_INADDRSZ) <= endp) &&
        ares_inet_net_pton_ipv4(curtok, tp, NS_INADDRSZ, ARES_FALSE) > 0) {
      tp         += NS_INADDRSZ;
      saw_xdigit  = 0;
      break; /* '\0' was seen by inet_pton4(). */
    }
    goto enoent;
  }
  if (saw_xdigit) {
    if (tp + NS_INT16SZ > endp) {
      goto enoent;
    }
    *tp++ = (unsigned char)(val >> 8) & 0xff;
    *tp++ = (unsigned char)val & 0xff;
  }
  if (colonp != NULL) {
    /*
     * Since some memmove()'s erroneously fail to handle
     * overlapping regions, we'll do the shift by hand.
     */
    const int n = (int)(tp - colonp);
    int       i;

    if (tp == endp) {
      goto enoent;
    }
    for (i = 1; i <= n; i++) {
      endp[-i]      = colonp[n - i];
      colonp[n - i] = 0;
    }
    tp = endp;
  }
  if (tp != endp) {
    goto enoent;
  }

  memcpy(dst, tmp, NS_IN6ADDRSZ);
  return 1;

enoent:
  SET_SOCKERRNO(ENOENT);
  return -1;
}

static int ares_inet_net_pton_ipv6(const char *src, unsigned char *dst,
                                   size_t size)
{
  struct ares_in6_addr in6;
  int                  ret;
  size_t               bits;
  size_t               bytes;
  char                 buf[INET6_ADDRSTRLEN + sizeof("/128")];
  char                *sep;

  if (ares_strlen(src) >= sizeof buf) {
    SET_SOCKERRNO(EMSGSIZE);
    return -1;
  }
  ares_strcpy(buf, src, sizeof buf);

  sep = strchr(buf, '/');
  if (sep != NULL) {
    *sep++ = '\0';
  }

  ret = ares_inet_pton6(buf, (unsigned char *)&in6);
  if (ret != 1) {
    return -1;
  }

  if (sep == NULL) {
    bits = 128;
  } else {
    if (!getbits(sep, &bits)) {
      SET_SOCKERRNO(ENOENT);
      return -1;
    }
  }

  bytes = (bits + 7) / 8;
  if (bytes > size) {
    SET_SOCKERRNO(EMSGSIZE);
    return -1;
  }
  /* Only the first (bits+7)/8 bytes carry the network, zero the remainder of
   * the caller's buffer so it never holds uninitialized data. */
  memset(dst, 0, size);
  memcpy(dst, &in6, bytes);
  return (int)bits;
}

/* Parse a network, not an address.  See ares_inet_net_pton.h for the
 * contract and how it differs from ares_inet_pton().
 * author:
 *      Paul Vixie (ISC), June 1996
 */
int ares_inet_net_pton(int af, const char *src, void *dst, size_t size)
{
  switch (af) {
    case AF_INET:
      return ares_inet_net_pton_ipv4(src, dst, size, ARES_TRUE);
    case AF_INET6:
      return ares_inet_net_pton_ipv6(src, dst, size);
    default:
      return -1;
  }
}

/*
 * int
 * inet_pton(af, src, dst)
 *      convert an address (not a network) from presentation to network
 *      format, like POSIX inet_pton().  A "/bits" prefix length denotes a
 *      prefix, not an address (RFC 4291 Section 2.3 vs. the address text
 *      forms of Section 2.2), and is rejected; dst is always fully written
 *      on success (4 bytes for AF_INET, 16 bytes for AF_INET6).
 *      For a network with a prefix length, use ares_inet_net_pton().
 * return:
 *      1 on success, 0 if src is not a valid address for af (as POSIX
 *      inet_pton() does for any unparsable input), or -1 with errno set to
 *      EAFNOSUPPORT if af is not supported.
 */
int ares_inet_pton(int af, const char *src, void *dst)
{
  int result;

  if (af == AF_INET) {
    result =
      ares_inet_net_pton_ipv4(src, dst, sizeof(struct in_addr), ARES_FALSE);
  } else if (af == AF_INET6) {
    result = ares_inet_pton6(src, dst);
  } else {
    SET_SOCKERRNO(EAFNOSUPPORT);
    return -1;
  }
  /* Neither parser can fail for a system reason: any failure (ENOENT, or
   * EMSGSIZE for too many IPv4 octets as in "1.2.3.4.5") means src is not a
   * valid address. */
  return (result > -1) ? 1 : 0;
}
