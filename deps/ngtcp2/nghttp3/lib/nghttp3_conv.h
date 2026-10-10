/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2017 ngtcp2 contributors
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
#ifndef NGHTTP3_CONV_H
#define NGHTTP3_CONV_H

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

#include <nghttp3/nghttp3.h>

#define NGHTTP3_VARINT_MAX ((1ULL << 62) - 1)

#if HAVE_DECL_BE64TOH
#  define nghttp3_ntohl64(N) be64toh(N)
#  define nghttp3_htonl64(N) htobe64(N)
#else /* !HAVE_DECL_BE64TOH */
#  ifdef WORDS_BIGENDIAN
#    define nghttp3_ntohl64(N) (N)
#    define nghttp3_htonl64(N) (N)
#  else /* !defined(WORDS_BIGENDIAN) */
#    if HAVE_DECL_BSWAP_64
#      define nghttp3_bswap64(N) bswap_64(N)
#    elif defined(WIN32)
#      define nghttp3_bswap64(N) _byteswap_uint64(N)
#    elif defined(__APPLE__)
#      define nghttp3_bswap64(N) OSSwapInt64(N)
#    else /* !(HAVE_DECL_BSWAP_64 || defined(WIN32) || defined(__APPLE__)) */
#      define nghttp3_bswap64(N)                                               \
        ((uint64_t)(ntohl((uint32_t)(N))) << 32 | ntohl((uint32_t)((N) >> 32)))
#    endif /* !(HAVE_DECL_BSWAP_64 || defined(WIN32) || defined(__APPLE__)) */
#    define nghttp3_ntohl64(N) nghttp3_bswap64(N)
#    define nghttp3_htonl64(N) nghttp3_bswap64(N)
#  endif /* !defined(WORDS_BIGENDIAN) */
#endif   /* !HAVE_DECL_BE64TOH */

#ifdef WIN32
/* Windows requires ws2_32 library for ntonl family of functions.
   Instead of using them, use _byteswap_* functions.  This is fine
   because all platforms that can run Windows these days are little
   endian. */
#  define htonl(N) _byteswap_ulong(N)
#  define htons(N) _byteswap_ushort(N)
#  define ntohl(N) _byteswap_ulong(N)
#  define ntohs(N) _byteswap_ushort(N)
#endif /* defined(WIN32) */

/*
 * nghttp3_get_uint32be reads 4 bytes from |p| as 32 bits unsigned
 * integer encoded as network byte order, and stores it in the buffer
 * pointed by |dest| in host byte order.  It returns |p| + 4.
 */
const uint8_t *nghttp3_get_uint32be(uint32_t *dest, const uint8_t *p);

/*
 * nghttp3_put_uint64be writes |n| in host byte order in |p| in
 * network byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *nghttp3_put_uint64be(uint8_t *p, uint64_t n);

/*
 * nghttp3_put_uint32be writes |n| in host byte order in |p| in
 * network byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *nghttp3_put_uint32be(uint8_t *p, uint32_t n);

/*
 * nghttp3_put_uint16be writes |n| in host byte order in |p| in
 * network byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *nghttp3_put_uint16be(uint8_t *p, uint16_t n);

/*
 * nghttp3_ord_stream_id returns the ordinal number of |stream_id|.
 */
uint64_t nghttp3_ord_stream_id(int64_t stream_id);

#endif /* !defined(NGHTTP3_CONV_H) */
