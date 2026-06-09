/*
 * ngtcp2
 *
 * Copyright (c) 2022 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGTCP2_NET_H
#define NGTCP2_NET_H

/* This header file is explicitly allowed to be shared with
   ngtcp2_crypto library. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif /* defined(HAVE_ARPA_INET_H) */

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif /* defined(HAVE_NETINET_IN_H) */

#ifdef HAVE_BYTESWAP_H
#  include <byteswap.h>
#endif /* defined(HAVE_BYTESWAP_H) */

#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#endif /* defined(HAVE_ENDIAN_H) */

#ifdef HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#endif /* defined(HAVE_SYS_ENDIAN_H) */

#ifdef __APPLE__
#  include <libkern/OSByteOrder.h>
#endif /* defined(__APPLE__) */

#include <ngtcp2/ngtcp2.h>

#if HAVE_DECL_BE64TOH
#  define ngtcp2_ntohl64(N) be64toh(N)
#  define ngtcp2_htonl64(N) htobe64(N)
#else /* !HAVE_DECL_BE64TOH */
#  ifdef WORDS_BIGENDIAN
#    define ngtcp2_ntohl64(N) (N)
#    define ngtcp2_htonl64(N) (N)
#  else /* !defined(WORDS_BIGENDIAN) */
#    if HAVE_DECL_BSWAP_64
#      define ngtcp2_bswap64(N) bswap_64(N)
#    elif defined(WIN32)
#      define ngtcp2_bswap64(N) _byteswap_uint64(N)
#    elif defined(__APPLE__)
#      define ngtcp2_bswap64(N) OSSwapInt64(N)
#    else /* !(HAVE_DECL_BSWAP_64 || defined(WIN32) || defined(__APPLE__)) */
#      define ngtcp2_bswap64(N)                                                \
        ((uint64_t)(ngtcp2_ntohl((uint32_t)(N))) << 32 |                       \
         ngtcp2_ntohl((uint32_t)((N) >> 32)))
#    endif /* !(HAVE_DECL_BSWAP_64 || defined(WIN32) || defined(__APPLE__)) */
#    define ngtcp2_ntohl64(N) ngtcp2_bswap64(N)
#    define ngtcp2_htonl64(N) ngtcp2_bswap64(N)
#  endif /* !defined(WORDS_BIGENDIAN) */
#endif   /* !HAVE_DECL_BE64TOH */

#ifdef WIN32
/* Windows requires ws2_32 library for ntonl family of functions.
   Instead of using them, use _byteswap_* functions.  This is fine
   because all platforms that can run Windows these days are little
   endian. */
#  define ngtcp2_htonl(N) _byteswap_ulong(N)
#  define ngtcp2_htons(N) _byteswap_ushort(N)
#  define ngtcp2_ntohl(N) _byteswap_ulong(N)
#  define ngtcp2_ntohs(N) _byteswap_ushort(N)
#else /* !defined(WIN32) */
#  define ngtcp2_htonl(N) htonl(N)
#  define ngtcp2_htons(N) htons(N)
#  define ngtcp2_ntohl(N) ntohl(N)
#  define ngtcp2_ntohs(N) ntohs(N)
#endif /* !defined(WIN32) */

#endif /* !defined(NGTCP2_NET_H) */
