/*
 * ngtcp2
 *
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
#ifndef NGTCP2_CONV_H
#define NGTCP2_CONV_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#ifdef HAVE_BYTESWAP_H
#  include <byteswap.h>
#endif /* HAVE_BYTESWAP_H */

#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#endif /* HAVE_ENDIAN_H */

#ifdef HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#endif /* HAVE_SYS_ENDIAN_H */

#include <ngtcp2/ngtcp2.h>

#if defined(HAVE_BSWAP_64) ||                                                  \
    (defined(HAVE_DECL_BSWAP_64) && HAVE_DECL_BSWAP_64 > 0)
#  define ngtcp2_bswap64 bswap_64
#else /* !HAVE_BSWAP_64 */
#  define ngtcp2_bswap64(N)                                                    \
    ((uint64_t)(ngtcp2_ntohl((uint32_t)(N))) << 32 |                           \
     ngtcp2_ntohl((uint32_t)((N) >> 32)))
#endif /* !HAVE_BSWAP_64 */

#if defined(HAVE_BE64TOH) ||                                                   \
    (defined(HAVE_DECL_BE64TOH) && HAVE_DECL_BE64TOH > 0)
#  define ngtcp2_ntohl64(N) be64toh(N)
#  define ngtcp2_htonl64(N) htobe64(N)
#else /* !HAVE_BE64TOH */
#  if defined(WORDS_BIGENDIAN)
#    define ngtcp2_ntohl64(N) (N)
#    define ngtcp2_htonl64(N) (N)
#  else /* !WORDS_BIGENDIAN */
#    define ngtcp2_ntohl64(N) ngtcp2_bswap64(N)
#    define ngtcp2_htonl64(N) ngtcp2_bswap64(N)
#  endif /* !WORDS_BIGENDIAN */
#endif   /* !HAVE_BE64TOH */

#if defined(WIN32)
/* Windows requires ws2_32 library for ntonl family functions.  We
   define inline functions for those function so that we don't have
   dependeny on that lib. */

#  ifdef _MSC_VER
#    define STIN static __inline
#  else
#    define STIN static inline
#  endif

STIN uint32_t ngtcp2_htonl(uint32_t hostlong) {
  uint32_t res;
  unsigned char *p = (unsigned char *)&res;
  *p++ = hostlong >> 24;
  *p++ = (hostlong >> 16) & 0xffu;
  *p++ = (hostlong >> 8) & 0xffu;
  *p = hostlong & 0xffu;
  return res;
}

STIN uint16_t ngtcp2_htons(uint16_t hostshort) {
  uint16_t res;
  unsigned char *p = (unsigned char *)&res;
  *p++ = hostshort >> 8;
  *p = hostshort & 0xffu;
  return res;
}

STIN uint32_t ngtcp2_ntohl(uint32_t netlong) {
  uint32_t res;
  unsigned char *p = (unsigned char *)&netlong;
  res = *p++ << 24;
  res += *p++ << 16;
  res += *p++ << 8;
  res += *p;
  return res;
}

STIN uint16_t ngtcp2_ntohs(uint16_t netshort) {
  uint16_t res;
  unsigned char *p = (unsigned char *)&netshort;
  res = *p++ << 8;
  res += *p;
  return res;
}

#else /* !WIN32 */

#  define ngtcp2_htonl htonl
#  define ngtcp2_htons htons
#  define ngtcp2_ntohl ntohl
#  define ngtcp2_ntohs ntohs

#endif /* !WIN32 */

/*
 * ngtcp2_get_uint64 reads 8 bytes from |p| as 64 bits unsigned
 * integer encoded as network byte order, and returns it in host byte
 * order.
 */
uint64_t ngtcp2_get_uint64(const uint8_t *p);

/*
 * ngtcp2_get_uint48 reads 6 bytes from |p| as 48 bits unsigned
 * integer encoded as network byte order, and returns it in host byte
 * order.
 */
uint64_t ngtcp2_get_uint48(const uint8_t *p);

/*
 * ngtcp2_get_uint32 reads 4 bytes from |p| as 32 bits unsigned
 * integer encoded as network byte order, and returns it in host byte
 * order.
 */
uint32_t ngtcp2_get_uint32(const uint8_t *p);

/*
 * ngtcp2_get_uint24 reads 3 bytes from |p| as 24 bits unsigned
 * integer encoded as network byte order, and returns it in host byte
 * order.
 */
uint32_t ngtcp2_get_uint24(const uint8_t *p);

/*
 * ngtcp2_get_uint16 reads 2 bytes from |p| as 16 bits unsigned
 * integer encoded as network byte order, and returns it in host byte
 * order.
 */
uint16_t ngtcp2_get_uint16(const uint8_t *p);

/*
 * ngtcp2_get_varint reads variable-length integer from |p|, and
 * returns it in host byte order.  The number of bytes read is stored
 * in |*plen|.
 */
uint64_t ngtcp2_get_varint(size_t *plen, const uint8_t *p);

/*
 * ngtcp2_get_pkt_num reads encoded packet number from |p|.  The
 * packet number is encoed in |pkt_numlen| bytes.
 */
int64_t ngtcp2_get_pkt_num(const uint8_t *p, size_t pkt_numlen);

/*
 * ngtcp2_put_uint64be writes |n| in host byte order in |p| in network
 * byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *ngtcp2_put_uint64be(uint8_t *p, uint64_t n);

/*
 * ngtcp2_put_uint48be writes |n| in host byte order in |p| in network
 * byte order.  It writes only least significant 48 bits.  It returns
 * the one beyond of the last written position.
 */
uint8_t *ngtcp2_put_uint48be(uint8_t *p, uint64_t n);

/*
 * ngtcp2_put_uint32be writes |n| in host byte order in |p| in network
 * byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *ngtcp2_put_uint32be(uint8_t *p, uint32_t n);

/*
 * ngtcp2_put_uint24be writes |n| in host byte order in |p| in network
 * byte order.  It writes only least significant 24 bits.  It returns
 * the one beyond of the last written position.
 */
uint8_t *ngtcp2_put_uint24be(uint8_t *p, uint32_t n);

/*
 * ngtcp2_put_uint16be writes |n| in host byte order in |p| in network
 * byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *ngtcp2_put_uint16be(uint8_t *p, uint16_t n);

/*
 * ngtcp2_put_varint writes |n| in |p| using variable-length integer
 * encoding.  It returns the one beyond of the last written position.
 */
uint8_t *ngtcp2_put_varint(uint8_t *p, uint64_t n);

/*
 * ngtcp2_put_varint14 writes |n| in |p| using variable-length integer
 * encoding.  |n| must be strictly less than 16384.  The function
 * always encodes |n| in 2 bytes.  It returns the one beyond of the
 * last written position.
 */
uint8_t *ngtcp2_put_varint14(uint8_t *p, uint16_t n);

/*
 * ngtcp2_put_pkt_num encodes |pkt_num| using |len| bytes.  It
 * returns the one beyond of the last written position.
 */
uint8_t *ngtcp2_put_pkt_num(uint8_t *p, int64_t pkt_num, size_t len);

/*
 * ngtcp2_get_varint_len returns the required number of bytes to read
 * variable-length integer starting at |p|.
 */
size_t ngtcp2_get_varint_len(const uint8_t *p);

/*
 * ngtcp2_put_varint_len returns the required number of bytes to
 * encode |n|.
 */
size_t ngtcp2_put_varint_len(uint64_t n);

/*
 * ngtcp2_nth_server_bidi_id returns |n|-th server bidirectional
 * stream ID.  If |n| is 0, it returns 0.  If the |n|-th stream ID is
 * larger than NGTCP2_MAX_SERVER_STREAM_ID_BIDI, this function returns
 * NGTCP2_MAX_SERVER_STREAM_ID_BIDI.
 */
int64_t ngtcp2_nth_server_bidi_id(uint64_t n);

/*
 * ngtcp2_nth_client_bidi_id returns |n|-th client bidirectional
 * stream ID.  If |n| is 0, it returns 0.  If the |n|-th stream ID is
 * larger than NGTCP2_MAX_CLIENT_STREAM_ID_BIDI, this function returns
 * NGTCP2_MAX_CLIENT_STREAM_ID_BIDI.
 */
int64_t ngtcp2_nth_client_bidi_id(uint64_t n);

/*
 * ngtcp2_nth_server_uni_id returns |n|-th server unidirectional
 * stream ID.  If |n| is 0, it returns 0.  If the |n|-th stream ID is
 * larger than NGTCP2_MAX_SERVER_STREAM_ID_UNI, this function returns
 * NGTCP2_MAX_SERVER_STREAM_ID_UNI.
 */
int64_t ngtcp2_nth_server_uni_id(uint64_t n);

/*
 * ngtcp2_nth_client_uni_id returns |n|-th client unidirectional
 * stream ID.  If |n| is 0, it returns 0.  If the |n|-th stream ID is
 * larger than NGTCP2_MAX_CLIENT_STREAM_ID_UNI, this function returns
 * NGTCP2_MAX_CLIENT_STREAM_ID_UNI.
 */
int64_t ngtcp2_nth_client_uni_id(uint64_t n);

/*
 * ngtcp2_ord_stream_id returns the ordinal number of |stream_id|.
 */
uint64_t ngtcp2_ord_stream_id(int64_t stream_id);

#endif /* NGTCP2_CONV_H */
