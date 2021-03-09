/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2017 nghttp2 contributors
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
#ifndef NGTCP2_H
#define NGTCP2_H

/* Define WIN32 when build target is Win32 API (borrowed from
   libcurl) */
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#  define WIN32
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#if defined(_MSC_VER) && (_MSC_VER < 1800)
/* MSVC < 2013 does not have inttypes.h because it is not C99
   compliant.  See compiler macros and version number in
   https://sourceforge.net/p/predef/wiki/Compilers/ */
#  include <stdint.h>
#else /* !defined(_MSC_VER) || (_MSC_VER >= 1800) */
#  include <inttypes.h>
#endif /* !defined(_MSC_VER) || (_MSC_VER >= 1800) */
#include <sys/types.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef WIN32
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#endif

#include <ngtcp2/version.h>

#ifdef NGTCP2_STATICLIB
#  define NGTCP2_EXTERN
#elif defined(WIN32)
#  ifdef BUILDING_NGTCP2
#    define NGTCP2_EXTERN __declspec(dllexport)
#  else /* !BUILDING_NGTCP2 */
#    define NGTCP2_EXTERN __declspec(dllimport)
#  endif /* !BUILDING_NGTCP2 */
#else    /* !defined(WIN32) */
#  ifdef BUILDING_NGTCP2
#    define NGTCP2_EXTERN __attribute__((visibility("default")))
#  else /* !BUILDING_NGTCP2 */
#    define NGTCP2_EXTERN
#  endif /* !BUILDING_NGTCP2 */
#endif   /* !defined(WIN32) */

/**
 * @typedef
 *
 * :type:`ngtcp2_ssize` is signed counterpart of size_t.
 */
typedef ptrdiff_t ngtcp2_ssize;

/**
 * @functypedef
 *
 * Custom memory allocator to replace malloc().  The |mem_user_data|
 * is the mem_user_data member of :type:`ngtcp2_mem` structure.
 */
typedef void *(*ngtcp2_malloc)(size_t size, void *mem_user_data);

/**
 * @functypedef
 *
 * Custom memory allocator to replace free().  The |mem_user_data| is
 * the mem_user_data member of :type:`ngtcp2_mem` structure.
 */
typedef void (*ngtcp2_free)(void *ptr, void *mem_user_data);

/**
 * @functypedef
 *
 * Custom memory allocator to replace calloc().  The |mem_user_data|
 * is the mem_user_data member of :type:`ngtcp2_mem` structure.
 */
typedef void *(*ngtcp2_calloc)(size_t nmemb, size_t size, void *mem_user_data);

/**
 * @functypedef
 *
 * Custom memory allocator to replace realloc().  The |mem_user_data|
 * is the mem_user_data member of :type:`ngtcp2_mem` structure.
 */
typedef void *(*ngtcp2_realloc)(void *ptr, size_t size, void *mem_user_data);

/**
 * @struct
 *
 * Custom memory allocator functions and user defined pointer.  The
 * |mem_user_data| member is passed to each allocator function.  This
 * can be used, for example, to achieve per-session memory pool.
 *
 * In the following example code, ``my_malloc``, ``my_free``,
 * ``my_calloc`` and ``my_realloc`` are the replacement of the
 * standard allocators ``malloc``, ``free``, ``calloc`` and
 * ``realloc`` respectively::
 *
 *     void *my_malloc_cb(size_t size, void *mem_user_data) {
 *       return my_malloc(size);
 *     }
 *
 *     void my_free_cb(void *ptr, void *mem_user_data) { my_free(ptr); }
 *
 *     void *my_calloc_cb(size_t nmemb, size_t size, void *mem_user_data) {
 *       return my_calloc(nmemb, size);
 *     }
 *
 *     void *my_realloc_cb(void *ptr, size_t size, void *mem_user_data) {
 *       return my_realloc(ptr, size);
 *     }
 *
 *     void conn_new() {
 *       ngtcp2_mem mem = {NULL, my_malloc_cb, my_free_cb, my_calloc_cb,
 *                          my_realloc_cb};
 *
 *       ...
 *     }
 */
typedef struct ngtcp2_mem {
  /**
   * An arbitrary user supplied data.  This is passed to each
   * allocator function.
   */
  void *mem_user_data;
  /**
   * Custom allocator function to replace malloc().
   */
  ngtcp2_malloc malloc;
  /**
   * Custom allocator function to replace free().
   */
  ngtcp2_free free;
  /**
   * Custom allocator function to replace calloc().
   */
  ngtcp2_calloc calloc;
  /**
   * Custom allocator function to replace realloc().
   */
  ngtcp2_realloc realloc;
} ngtcp2_mem;

/**
 * @macrosection
 *
 * Time related macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_SECONDS` is a count of tick which corresponds to 1 second.
 */
#define NGTCP2_SECONDS ((uint64_t)1000000000ULL)

/**
 * @macro
 *
 * :macro:`NGTCP2_MILLISECONDS` is a count of tick which corresponds
 * to 1 millisecond.
 */
#define NGTCP2_MILLISECONDS ((uint64_t)1000000ULL)

/**
 * @macro
 *
 * :macro:`NGTCP2_MICROSECONDS` is a count of tick which corresponds
 * to 1 microsecond.
 */
#define NGTCP2_MICROSECONDS ((uint64_t)1000ULL)

/**
 * @macro
 *
 * :macro:`NGTCP2_NANOSECONDS` is a count of tick which corresponds to
 * 1 nanosecond.
 */
#define NGTCP2_NANOSECONDS ((uint64_t)1ULL)

/**
 * @macrosection
 *
 * QUIC protocol version macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_PROTO_VER_V1` is the QUIC version 1.
 */
#define NGTCP2_PROTO_VER_V1 0x00000001u

/**
 * @macro
 *
 * :macro:`NGTCP2_PROTO_VER_DRAFT_MAX` is the maximum QUIC draft
 * version that this library supports.
 */
#define NGTCP2_PROTO_VER_DRAFT_MAX 0xff000020u

/**
 * @macro
 *
 * :macro:`NGTCP2_PROTO_VER_DRAFT_MIN` is the minimum QUIC draft
 * version that this library supports.
 */
#define NGTCP2_PROTO_VER_DRAFT_MIN 0xff00001du

/**
 * @macro
 *
 * :macro:`NGTCP2_PROTO_VER_MAX` is the highest QUIC version that this
 * library supports.
 */
#define NGTCP2_PROTO_VER_MAX NGTCP2_PROTO_VER_V1

/**
 * @macro
 *
 * :macro:`NGTCP2_PROTO_VER_MIN` is the lowest QUIC version that this
 * library supports.
 */
#define NGTCP2_PROTO_VER_MIN NGTCP2_PROTO_VER_DRAFT_MIN

/**
 * @macrosection
 *
 * IP packet related macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_MAX_PKTLEN_IPV4` is the maximum datagram size of
 * IPv4 packet without PMTUD.
 */
#define NGTCP2_MAX_PKTLEN_IPV4 1252
/**
 * @macro
 *
 * :macro:`NGTCP2_MAX_PKTLEN_IPV6` is the maximum datagram size of
 * IPv6 packet without PMTUD.
 */
#define NGTCP2_MAX_PKTLEN_IPV6 1232

/**
 * @macro
 *
 * :macro:`NGTCP2_MIN_INITIAL_PKTLEN` is the minimum datagram size for
 * a packet sent by client which contains its first Initial packet.
 */
#define NGTCP2_MIN_INITIAL_PKTLEN 1200

/**
 * @macro
 *
 * :macro:`NGTCP2_DEFAULT_MAX_PKTLEN` is the default maximum datagram
 * size that this endpoint transmits.  It is used by congestion
 * controller to compute congestion window.
 */
#define NGTCP2_DEFAULT_MAX_PKTLEN 1200

/**
 * @macrosection
 *
 * QUIC specific macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_MAX_VARINT` is the maximum value which can be
 * encoded in variable-length integer encoding.
 */
#define NGTCP2_MAX_VARINT ((1ULL << 62) - 1)

/**
 * @macro
 *
 * :macro:`NGTCP2_STATELESS_RESET_TOKENLEN` is the length of Stateless
 * Reset Token.
 */
#define NGTCP2_STATELESS_RESET_TOKENLEN 16

/**
 * @macro
 *
 * :macro:`NGTCP2_MIN_STATELESS_RESET_RANDLEN` is the minimum length
 * of random bytes (Unpredictable Bits) in Stateless Retry packet
 */
#define NGTCP2_MIN_STATELESS_RESET_RANDLEN 5

/**
 * @macro
 *
 * :macro:`NGTCP2_RETRY_KEY_DRAFT` is an encryption key to create
 * integrity tag of Retry packet.  It is used for QUIC draft versions.
 */
#define NGTCP2_RETRY_KEY_DRAFT                                                 \
  "\xcc\xce\x18\x7e\xd0\x9a\x09\xd0\x57\x28\x15\x5a\x6c\xb9\x6b\xe1"

/**
 * @macro
 *
 * :macro:`NGTCP2_RETRY_NONCE_DRAFT` is nonce used when generating
 * integrity tag of Retry packet.  It is used for QUIC draft versions.
 */
#define NGTCP2_RETRY_NONCE_DRAFT                                               \
  "\xe5\x49\x30\xf9\x7f\x21\x36\xf0\x53\x0a\x8c\x1c"

/**
 * @macro
 *
 * :macro:`NGTCP2_RETRY_KEY_V1` is an encryption key to create
 * integrity tag of Retry packet.  It is used for QUIC v1.
 */
#define NGTCP2_RETRY_KEY_V1                                                    \
  "\xbe\x0c\x69\x0b\x9f\x66\x57\x5a\x1d\x76\x6b\x54\xe3\x68\xc8\x4e"

/**
 * @macro
 *
 * :macro:`NGTCP2_RETRY_NONCE_V1` is nonce used when generating integrity
 * tag of Retry packet.  It is used for QUIC v1.
 */
#define NGTCP2_RETRY_NONCE_V1 "\x46\x15\x99\xd3\x5d\x63\x2b\xf2\x23\x98\x25\xbb"

/**
 * @macro
 *
 * :macro:`NGTCP2_HP_MASKLEN` is the length of header protection mask.
 */
#define NGTCP2_HP_MASKLEN 5

/**
 * @macro
 *
 * :macro:`NGTCP2_HP_SAMPLELEN` is the number bytes sampled when
 * encrypting a packet header.
 */
#define NGTCP2_HP_SAMPLELEN 16

/**
 * @macro
 *
 * :macro:`NGTCP2_DEFAULT_INITIAL_RTT` is a default initial RTT.
 */
#define NGTCP2_DEFAULT_INITIAL_RTT (333 * NGTCP2_MILLISECONDS)

/**
 * @macro
 *
 * :macro:`NGTCP2_MAX_CIDLEN` is the maximum length of Connection ID.
 */
#define NGTCP2_MAX_CIDLEN 20

/**
 * @macro
 *
 * :macro:`NGTCP2_MIN_CIDLEN` is the minimum length of Connection ID.
 */
#define NGTCP2_MIN_CIDLEN 1

/**
 * @macro
 *
 * :macro:`NGTCP2_MIN_INITIAL_DCIDLEN` is the minimum length of
 * Destination Connection ID in Client Initial packet if it does not
 * bear token from Retry packet.
 */
#define NGTCP2_MIN_INITIAL_DCIDLEN 8

/**
 * @macrosection
 *
 * ECN related macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_ECN_NOT_ECT` indicates no ECN marking.
 */
#define NGTCP2_ECN_NOT_ECT 0x0

/**
 * @macro
 *
 * :macro:`NGTCP2_ECN_ECT_1` is ECT(1) codepoint.
 */
#define NGTCP2_ECN_ECT_1 0x1

/**
 * @macro
 *
 * :macro:`NGTCP2_ECN_ECT_0` is ECT(0) codepoint.
 */
#define NGTCP2_ECN_ECT_0 0x2

/**
 * @macro
 *
 * :macro:`NGTCP2_ECN_CE` is CE codepoint.
 */
#define NGTCP2_ECN_CE 0x3

/**
 * @macro
 *
 * :macro:`NGTCP2_ECN_MASK` is a bit mask to get ECN marking.
 */
#define NGTCP2_ECN_MASK 0x3

/**
 * @struct
 *
 * :type:`ngtcp2_pkt_info` is a packet metadata.
 */
typedef struct ngtcp2_pkt_info {
  /**
   * :member:`ecn <ngtcp2_pkt_info.ecn>` is ECN marking and when
   * passing `ngtcp2_conn_read_pkt()`, and it should be either
   * :macro:`NGTCP2_ECN_NOT_ECT`, :macro:`NGTCP2_ECN_ECT_1`,
   * :macro:`NGTCP2_ECN_ECT_0`, or :macro:`NGTCP2_ECN_CE`.
   */
  uint32_t ecn;
} ngtcp2_pkt_info;

/**
 * @macrosection
 *
 * ngtcp2 library error codes
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT` indicates that a passed
 * argument is invalid.
 */
#define NGTCP2_ERR_INVALID_ARGUMENT -201
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_NOBUF` indicates that a provided buffer does not
 * have enough space to store data.
 */
#define NGTCP2_ERR_NOBUF -203
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_PROTO` indicates a general protocol error.
 */
#define NGTCP2_ERR_PROTO -205
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_INVALID_STATE` indicates that a requested
 * operation is not allowed at the current connection state.
 */
#define NGTCP2_ERR_INVALID_STATE -206
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_ACK_FRAME` indicates that an invalid ACK frame
 * is received.
 */
#define NGTCP2_ERR_ACK_FRAME -207
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_STREAM_ID_BLOCKED` indicates that there is no
 * spare stream ID available.
 */
#define NGTCP2_ERR_STREAM_ID_BLOCKED -208
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_STREAM_IN_USE` indicates that a stream ID is
 * already in use.
 */
#define NGTCP2_ERR_STREAM_IN_USE -209
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_STREAM_DATA_BLOCKED` indicates that stream data
 * cannot be sent because of flow control.
 */
#define NGTCP2_ERR_STREAM_DATA_BLOCKED -210
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_FLOW_CONTROL` indicates flow control error.
 */
#define NGTCP2_ERR_FLOW_CONTROL -211
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_CONNECTION_ID_LIMIT` indicates that the number
 * of received Connection ID exceeds acceptable limit.
 */
#define NGTCP2_ERR_CONNECTION_ID_LIMIT -212
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_STREAM_LIMIT` indicates that a remote endpoint
 * opens more streams that is permitted.
 */
#define NGTCP2_ERR_STREAM_LIMIT -213
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_FINAL_SIZE` indicates that inconsistent final
 * size of a stream.
 */
#define NGTCP2_ERR_FINAL_SIZE -214
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_CRYPTO` indicates crypto (TLS) related error.
 */
#define NGTCP2_ERR_CRYPTO -215
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_PKT_NUM_EXHAUSTED` indicates that packet number
 * is exhausted.
 */
#define NGTCP2_ERR_PKT_NUM_EXHAUSTED -216
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM` indicates that a
 * required transport parameter is missing.
 */
#define NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM -217
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM` indicates that a
 * transport parameter is malformed.
 */
#define NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM -218
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_FRAME_ENCODING` indicates there is an error in
 * frame encoding.
 */
#define NGTCP2_ERR_FRAME_ENCODING -219
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_TLS_DECRYPT` indicates TLS decryption failure.
 */
#define NGTCP2_ERR_TLS_DECRYPT -220
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_STREAM_SHUT_WR` indicates no more data can be
 * sent to a stream.
 */
#define NGTCP2_ERR_STREAM_SHUT_WR -221
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_STREAM_NOT_FOUND` indicates that a stream was not
 * found.
 */
#define NGTCP2_ERR_STREAM_NOT_FOUND -222
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_STREAM_STATE` indicates that a requested
 * operation is not allowed at the current stream state.
 */
#define NGTCP2_ERR_STREAM_STATE -226
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_RECV_VERSION_NEGOTIATION` indicates that Version
 * Negotiation packet was received.
 */
#define NGTCP2_ERR_RECV_VERSION_NEGOTIATION -229
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_CLOSING` indicates that connection is in closing
 * state.
 */
#define NGTCP2_ERR_CLOSING -230
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_DRAINING` indicates that connection is in
 * draining state.
 */
#define NGTCP2_ERR_DRAINING -231
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_TRANSPORT_PARAM` indicates a general transport
 * parameter error.
 */
#define NGTCP2_ERR_TRANSPORT_PARAM -234
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_DISCARD_PKT` indicates a packet was discarded.
 */
#define NGTCP2_ERR_DISCARD_PKT -235
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_PATH_VALIDATION_FAILED` indicates that a path
 * validation failed.
 */
#define NGTCP2_ERR_PATH_VALIDATION_FAILED -236
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_CONN_ID_BLOCKED` indicates that there is no
 * spare Connection ID available.
 */
#define NGTCP2_ERR_CONN_ID_BLOCKED -237
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_INTERNAL` indicates an internal error.
 */
#define NGTCP2_ERR_INTERNAL -238
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_CRYPTO_BUFFER_EXCEEDED` indicates that a crypto
 * buffer exceeded.
 */
#define NGTCP2_ERR_CRYPTO_BUFFER_EXCEEDED -239
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_WRITE_MORE` indicates
 * :macro:`NGTCP2_WRITE_STREAM_FLAG_MORE` is used and a function call
 * succeeded.
 */
#define NGTCP2_ERR_WRITE_MORE -240
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_RETRY` indicates that server should send Retry
 * packet.
 */
#define NGTCP2_ERR_RETRY -241
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_DROP_CONN` indicates that an endpoint should
 * drop connection immediately.
 */
#define NGTCP2_ERR_DROP_CONN -242
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_AEAD_LIMIT_REACHED` indicates AEAD encryption
 * limit is reached and key update is not available.  An endpoint
 * should drop connection immediately.
 */
#define NGTCP2_ERR_AEAD_LIMIT_REACHED -243
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_NO_VIABLE_PATH` indicates that path validation
 * could not probe that a path is not capable of at least 1200 MTU.
 */
#define NGTCP2_ERR_NO_VIABLE_PATH -244
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_FATAL` indicates that error codes less than this
 * value is fatal error.  When this error is returned, an endpoint
 * should drop connection immediately.
 */
#define NGTCP2_ERR_FATAL -500
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_NOMEM` indicates out of memory.
 */
#define NGTCP2_ERR_NOMEM -501
/**
 * @macro
 *
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` indicates that user defined
 * callback function failed.
 */
#define NGTCP2_ERR_CALLBACK_FAILURE -502

/**
 * @macrosection
 *
 * QUIC packet header flags
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_PKT_FLAG_NONE` indicates no flag set.
 */
#define NGTCP2_PKT_FLAG_NONE 0

/**
 * @macro
 *
 * :macro:`NGTCP2_PKT_FLAG_LONG_FORM` indicates the Long packet
 * header.
 */
#define NGTCP2_PKT_FLAG_LONG_FORM 0x01

/**
 * @macro
 *
 * :macro:`NGTCP2_PKT_FLAG_KEY_PHASE` indicates Key Phase bit set.
 */
#define NGTCP2_PKT_FLAG_KEY_PHASE 0x04

/**
 * @enum
 *
 * :type:`ngtcp2_pkt_type` defines QUIC packet types.
 */
typedef enum ngtcp2_pkt_type {
  /**
   * :enum:`NGTCP2_PKT_VERSION_NEGOTIATION` is defined by libngtcp2
   * for convenience.
   */
  NGTCP2_PKT_VERSION_NEGOTIATION = 0xf0,
  /**
   * :enum:`NGTCP2_PKT_INITIAL` indicates Initial packet.
   */
  NGTCP2_PKT_INITIAL = 0x0,
  /**
   * :enum:`NGTCP2_PKT_0RTT` indicates 0RTT packet.
   */
  NGTCP2_PKT_0RTT = 0x1,
  /**
   * :enum:`NGTCP2_PKT_HANDSHAKE` indicates Handshake packet.
   */
  NGTCP2_PKT_HANDSHAKE = 0x2,
  /**
   * :enum:`NGTCP2_PKT_RETRY` indicates Retry packet.
   */
  NGTCP2_PKT_RETRY = 0x3,
  /**
   * :enum:`NGTCP2_PKT_SHORT` is defined by libngtcp2 for convenience.
   */
  NGTCP2_PKT_SHORT = 0x70
} ngtcp2_pkt_type;

/**
 * @macrosection
 *
 * QUIC transport error code
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_NO_ERROR` is QUIC transport error code ``NO_ERROR``.
 */
#define NGTCP2_NO_ERROR 0x0u

/**
 * @macro
 *
 * :macro:`NGTCP2_INTERNAL_ERROR` is QUIC transport error code
 * ``INTERNAL_ERROR``.
 */
#define NGTCP2_INTERNAL_ERROR 0x1u

/**
 * @macro
 *
 * :macro:`NGTCP2_CONNECTION_REFUSED` is QUIC transport error code
 * ``CONNECTION_REFUSED``.
 */
#define NGTCP2_CONNECTION_REFUSED 0x2u

/**
 * @macro
 *
 * :macro:`NGTCP2_FLOW_CONTROL_ERROR` is QUIC transport error code
 * ``FLOW_CONTROL_ERROR``.
 */
#define NGTCP2_FLOW_CONTROL_ERROR 0x3u

/**
 * @macro
 *
 * :macro:`NGTCP2_STREAM_LIMIT_ERROR` is QUIC transport error code
 * ``STREAM_LIMIT_ERROR``.
 */
#define NGTCP2_STREAM_LIMIT_ERROR 0x4u

/**
 * @macro
 *
 * :macro:`NGTCP2_STREAM_STATE_ERROR` is QUIC transport error code
 * ``STREAM_STATE_ERROR``.
 */
#define NGTCP2_STREAM_STATE_ERROR 0x5u

/**
 * @macro
 *
 * :macro:`NGTCP2_FINAL_SIZE_ERROR` is QUIC transport error code
 * ``FINAL_SIZE_ERROR``.
 */
#define NGTCP2_FINAL_SIZE_ERROR 0x6u

/**
 * @macro
 *
 * :macro:`NGTCP2_FRAME_ENCODING_ERROR` is QUIC transport error code
 * ``FRAME_ENCODING_ERROR``.
 */
#define NGTCP2_FRAME_ENCODING_ERROR 0x7u

/**
 * @macro
 *
 * :macro:`NGTCP2_TRANSPORT_PARAMETER_ERROR` is QUIC transport error
 * code ``TRANSPORT_PARAMETER_ERROR``.
 */
#define NGTCP2_TRANSPORT_PARAMETER_ERROR 0x8u

/**
 * @macro
 *
 * :macro:`NGTCP2_CONNECTION_ID_LIMIT_ERROR` is QUIC transport error
 * code ``CONNECTION_ID_LIMIT_ERROR``.
 */
#define NGTCP2_CONNECTION_ID_LIMIT_ERROR 0x9u

/**
 * @macro
 *
 * :macro:`NGTCP2_PROTOCOL_VIOLATION` is QUIC transport error code
 * ``PROTOCOL_VIOLATION``.
 */
#define NGTCP2_PROTOCOL_VIOLATION 0xau

/**
 * @macro
 *
 * :macro:`NGTCP2_INVALID_TOKEN` is QUIC transport error code
 * ``INVALID_TOKEN``.
 */
#define NGTCP2_INVALID_TOKEN 0xbu

/**
 * @macro
 *
 * :macro:`NGTCP2_APPLICATION_ERROR` is QUIC transport error code
 * ``APPLICATION_ERROR``.
 */
#define NGTCP2_APPLICATION_ERROR 0xcu

/**
 * @macro
 *
 * :macro:`NGTCP2_CRYPTO_BUFFER_EXCEEDED` is QUIC transport error code
 * ``CRYPTO_BUFFER_EXCEEDED``.
 */
#define NGTCP2_CRYPTO_BUFFER_EXCEEDED 0xdu

/**
 * @macro
 *
 * :macro:`NGTCP2_KEY_UPDATE_ERROR` is QUIC transport error code
 * ``KEY_UPDATE_ERROR``.
 */
#define NGTCP2_KEY_UPDATE_ERROR 0xeu

/**
 * @macro
 *
 * :macro:`NGTCP2_AEAD_LIMIT_REACHED` is QUIC transport error code
 * ``AEAD_LIMIT_REACHED``.
 */
#define NGTCP2_AEAD_LIMIT_REACHED 0xfu

/**
 * @macro
 *
 * :macro:`NGTCP2_NO_VIABLE_PATH` is QUIC transport error code
 * ``NO_VIABLE_PATH``.
 */
#define NGTCP2_NO_VIABLE_PATH 0x10u

/**
 * @macro
 *
 * :macro:`NGTCP2_CRYPTO_ERROR` is QUIC transport error code
 * ``CRYPTO_ERROR``.
 */
#define NGTCP2_CRYPTO_ERROR 0x100u

/**
 * @enum
 *
 * :type:`ngtcp2_path_validation_result` defines path validation
 * result code.
 */
typedef enum ngtcp2_path_validation_result {
  /**
   * :enum:`NGTCP2_PATH_VALIDATION_RESULT_SUCCESS` indicates
   * successful validation.
   */
  NGTCP2_PATH_VALIDATION_RESULT_SUCCESS,
  /**
   * :enum:`NGTCP2_PATH_VALIDATION_RESULT_FAILURE` indicates
   * validation failure.
   */
  NGTCP2_PATH_VALIDATION_RESULT_FAILURE
} ngtcp2_path_validation_result;

/**
 * @typedef
 *
 * :type:`ngtcp2_tstamp` is a timestamp with nanosecond resolution.
 */
typedef uint64_t ngtcp2_tstamp;

/**
 * @typedef
 *
 * :type:`ngtcp2_duration` is a period of time in nanosecond
 * resolution.
 */
typedef uint64_t ngtcp2_duration;

/**
 * @struct
 *
 * :type:`ngtcp2_cid` holds a Connection ID.
 */
typedef struct ngtcp2_cid {
  /**
   * :member:`datalen <ngtcp2_cid.datalen>` is the length of
   * Connection ID.
   */
  size_t datalen;
  /**
   * :member:`data <ngtcp2_cid.data>` is the buffer to store
   * Connection ID.
   */
  uint8_t data[NGTCP2_MAX_CIDLEN];
} ngtcp2_cid;

/**
 * @struct
 *
 * :type:`ngtcp2_vec` is struct iovec compatible structure to
 * reference arbitrary array of bytes.
 */
typedef struct ngtcp2_vec {
  /**
   * :member:`base <ngtcp2_vec.base>` points to the data.
   */
  uint8_t *base;
  /**
   * :member:`len <ngtcp2_vec.len>` is the number of bytes which the
   * buffer pointed by base contains.
   */
  size_t len;
} ngtcp2_vec;

/**
 * @function
 *
 * `ngtcp2_cid_init` initializes Connection ID |cid| with the byte
 * string pointed by |data| and its length is |datalen|.  |datalen|
 * must be at least :macro:`NGTCP2_MIN_CIDLEN`, and at most
 * :macro:`NGTCP2_MAX_CIDLEN`.
 */
NGTCP2_EXTERN void ngtcp2_cid_init(ngtcp2_cid *cid, const uint8_t *data,
                                   size_t datalen);

/**
 * @struct
 *
 * :type:`ngtcp2_pkt_hd` represents QUIC packet header.
 */
typedef struct ngtcp2_pkt_hd {
  /**
   * :member:`dcid` is Destination Connection ID.
   */
  ngtcp2_cid dcid;
  /**
   * :member:`scid` is Source Connection ID.
   */
  ngtcp2_cid scid;
  /**
   * :member:`pkt_num` is a packet number.
   */
  int64_t pkt_num;
  /**
   * :member:`token` contains token for Initial
   * packet.
   */
  ngtcp2_vec token;
  /**
   * :member:`pkt_numlen` is the number of bytes spent to encode
   * :member:`pkt_num`.
   */
  size_t pkt_numlen;
  /**
   * :member:`len` is the sum of :member:`pkt_numlen` and the length
   * of QUIC packet payload.
   */
  size_t len;
  /**
   * :member:`version` is QUIC version.
   */
  uint32_t version;
  /**
   * :member:`type` is a type of QUIC packet.  See
   * :type:`ngtcp2_pkt_type`.
   */
  uint8_t type;
  /**
   * :member:`flags` is zero or more of NGTCP2_PKT_FLAG_*.  See
   * :macro:`NGTCP2_PKT_FLAG_NONE`.
   */
  uint8_t flags;
} ngtcp2_pkt_hd;

/**
 * @struct
 *
 * :type:`ngtcp2_pkt_stateless_reset` represents Stateless Reset.
 */
typedef struct ngtcp2_pkt_stateless_reset {
  /**
   * :member:`stateless_reset_token` contains stateless reset token.
   */
  uint8_t stateless_reset_token[NGTCP2_STATELESS_RESET_TOKENLEN];
  /**
   * :member:`rand` points a buffer which contains random bytes
   * section.
   */
  const uint8_t *rand;
  /**
   * :member:`randlen` is the number of random bytes.
   */
  size_t randlen;
} ngtcp2_pkt_stateless_reset;

typedef enum ngtcp2_transport_param_id {
  NGTCP2_TRANSPORT_PARAM_ORIGINAL_DESTINATION_CONNECTION_ID = 0x0000,
  NGTCP2_TRANSPORT_PARAM_MAX_IDLE_TIMEOUT = 0x0001,
  NGTCP2_TRANSPORT_PARAM_STATELESS_RESET_TOKEN = 0x0002,
  NGTCP2_TRANSPORT_PARAM_MAX_UDP_PAYLOAD_SIZE = 0x0003,
  NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_DATA = 0x0004,
  NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_LOCAL = 0x0005,
  NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_BIDI_REMOTE = 0x0006,
  NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAM_DATA_UNI = 0x0007,
  NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_BIDI = 0x0008,
  NGTCP2_TRANSPORT_PARAM_INITIAL_MAX_STREAMS_UNI = 0x0009,
  NGTCP2_TRANSPORT_PARAM_ACK_DELAY_EXPONENT = 0x000a,
  NGTCP2_TRANSPORT_PARAM_MAX_ACK_DELAY = 0x000b,
  NGTCP2_TRANSPORT_PARAM_DISABLE_ACTIVE_MIGRATION = 0x000c,
  NGTCP2_TRANSPORT_PARAM_PREFERRED_ADDRESS = 0x000d,
  NGTCP2_TRANSPORT_PARAM_ACTIVE_CONNECTION_ID_LIMIT = 0x000e,
  NGTCP2_TRANSPORT_PARAM_INITIAL_SOURCE_CONNECTION_ID = 0x000f,
  NGTCP2_TRANSPORT_PARAM_RETRY_SOURCE_CONNECTION_ID = 0x0010,
  NGTCP2_TRANSPORT_PARAM_MAX_DATAGRAM_FRAME_SIZE = 0x0020
} ngtcp2_transport_param_id;

/**
 * @enum
 *
 * :type:`ngtcp2_transport_params_type` defines TLS message type which
 * carries transport parameters.
 */
typedef enum ngtcp2_transport_params_type {
  /**
   * :enum:`NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO` is Client Hello
   * TLS message.
   */
  NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO,
  /**
   * :enum:`NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS` is
   * Encrypted Extensions TLS message.
   */
  NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS
} ngtcp2_transport_params_type;

/**
 * @enum
 *
 * ngtcp2_rand_usage describes the usage of the generated random data.
 */
typedef enum ngtcp2_rand_usage {
  NGTCP2_RAND_USAGE_NONE,
  /**
   * :enum:`NGTCP2_RAND_USAGE_PATH_CHALLENGE` indicates that random
   * value is used for PATH_CHALLENGE.
   */
  NGTCP2_RAND_USAGE_PATH_CHALLENGE
} ngtcp2_rand_usage;

/**
 * @macrosection
 *
 * QUIC transport parameters related macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_DEFAULT_MAX_UDP_PAYLOAD_SIZE` is the default value
 * of max_udp_payload_size transport parameter.
 */
#define NGTCP2_DEFAULT_MAX_UDP_PAYLOAD_SIZE 65527

/**
 * @macro
 *
 * :macro:`NGTCP2_DEFAULT_ACK_DELAY_EXPONENT` is a default value of
 * scaling factor of ACK Delay field in ACK frame.
 */
#define NGTCP2_DEFAULT_ACK_DELAY_EXPONENT 3

/**
 * @macro
 *
 * :macro:`NGTCP2_DEFAULT_MAX_ACK_DELAY` is a default value of the
 * maximum amount of time in nanoseconds by which endpoint delays
 * sending acknowledgement.
 */
#define NGTCP2_DEFAULT_MAX_ACK_DELAY (25 * NGTCP2_MILLISECONDS)

/**
 * @macro
 *
 * :macro:`NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT` is the default
 * value of active_connection_id_limit transport parameter value if
 * omitted.
 */
#define NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT 2

/**
 * @macro
 *
 * :macro:`NGTCP2_TLSEXT_QUIC_TRANSPORT_PARAMETERS` is TLS extension
 * type of quic_transport_parameters.
 */
#define NGTCP2_TLSEXT_QUIC_TRANSPORT_PARAMETERS 0xffa5u

/**
 * @struct
 *
 * :type:`ngtcp2_preferred_addr` represents preferred address
 * structure.
 */
typedef struct ngtcp2_preferred_addr {
  /**
   * :member:`cid` is a Connection ID.
   */
  ngtcp2_cid cid;
  /**
   * :member:`ipv4_port` is a port of IPv4 address.
   */
  uint16_t ipv4_port;
  /**
   * :member:`ipv6_port` is a port of IPv6 address.
   */
  uint16_t ipv6_port;
  /**
   * :member:`ipv4_addr` contains IPv4 address in network byte order.
   */
  uint8_t ipv4_addr[4];
  /**
   * :member:`ipv6_addr` contains IPv6 address in network byte order.
   */
  uint8_t ipv6_addr[16];
  /**
   * :member:`stateless_reset_token` contains stateless reset token.
   */
  uint8_t stateless_reset_token[NGTCP2_STATELESS_RESET_TOKENLEN];
} ngtcp2_preferred_addr;

/**
 * @struct
 *
 * :type:`ngtcp2_transport_params` represents QUIC transport
 * parameters.
 */
typedef struct ngtcp2_transport_params {
  /**
   * :member:`preferred_address` contains preferred address if
   * :member:`preferred_address_present` is nonzero.
   */
  ngtcp2_preferred_addr preferred_address;
  /**
   * :member:`original_dcid` is the Destination Connection ID field
   * from the first Initial packet from client.  Server must specify
   * this field.  It is expected that application knows the original
   * Destination Connection ID even if it sends Retry packet, for
   * example, by including it in retry token.  Otherwise, application
   * should not specify this field.
   */
  ngtcp2_cid original_dcid;
  /**
   * :member:`initial_scid` is the Source Connection ID field from the
   * first Initial packet the endpoint sends.  Application should not
   * specify this field.
   */
  ngtcp2_cid initial_scid;
  /**
   * :member:`retry_scid` is the Source Connection ID field from Retry
   * packet.  Only server uses this field.  If server application
   * received Initial packet with retry token from client and server
   * verified its token, server application must set Destination
   * Connection ID field from the Initial packet to this field and set
   * :member:`retry_scid_present` to nonzero.  Server application must
   * verify that the Destination Connection ID from Initial packet was
   * sent in Retry packet by, for example, including the Connection ID
   * in a token, or including it in AAD when encrypting a token.
   */
  ngtcp2_cid retry_scid;
  /**
   * :member:`initial_max_stream_data_bidi_local` is the size of flow
   * control window of locally initiated stream.  This is the number
   * of bytes that the remote endpoint can send and the local endpoint
   * must ensure that it has enough buffer to receive them.
   */
  uint64_t initial_max_stream_data_bidi_local;
  /**
   * :member:`initial_max_stream_data_bidi_remote` is the size of flow
   * control window of remotely initiated stream.  This is the number
   * of bytes that the remote endpoint can send and the local endpoint
   * must ensure that it has enough buffer to receive them.
   */
  uint64_t initial_max_stream_data_bidi_remote;
  /**
   * :member:`initial_max_stream_data_uni` is the size of flow control
   * window of remotely initiated unidirectional stream.  This is the
   * number of bytes that the remote endpoint can send and the local
   * endpoint must ensure that it has enough buffer to receive them.
   */
  uint64_t initial_max_stream_data_uni;
  /**
   * :member:`initial_max_data` is the connection level flow control
   * window.
   */
  uint64_t initial_max_data;
  /**
   * :member:`initial_max_streams_bidi` is the number of concurrent
   * streams that the remote endpoint can create.
   */
  uint64_t initial_max_streams_bidi;
  /**
   * :member:`initial_max_streams_uni` is the number of concurrent
   * unidirectional streams that the remote endpoint can create.
   */
  uint64_t initial_max_streams_uni;
  /**
   * :member:`max_idle_timeout` is a duration during which sender
   * allows quiescent.
   */
  ngtcp2_duration max_idle_timeout;
  /**
   * :member:`max_udp_payload_size` is the maximum datagram size that
   * the endpoint can receive.
   */
  uint64_t max_udp_payload_size;
  /**
   * :member:`active_connection_id_limit` is the maximum number of
   * Connection ID that sender can store.
   */
  uint64_t active_connection_id_limit;
  /**
   * :member:`ack_delay_exponent` is the exponent used in ACK Delay
   * field in ACK frame.
   */
  uint64_t ack_delay_exponent;
  /**
   * :member:`max_ack_delay` is the maximum acknowledgement delay by
   * which the endpoint will delay sending acknowledgements.
   */
  ngtcp2_duration max_ack_delay;
  /**
   * :member:`max_datagram_frame_size` is the maximum size of DATAGRAM
   * frame that this endpoint willingly receives.  Specifying 0
   * disables DATAGRAM support.  See
   * https://tools.ietf.org/html/draft-ietf-quic-datagram-01
   */
  uint64_t max_datagram_frame_size;
  /**
   * :member:`stateless_reset_token_present` is nonzero if
   * :member:`stateless_reset_token` field is set.
   */
  uint8_t stateless_reset_token_present;
  /**
   * :member:`disable_active_migration` is nonzero if the endpoint
   * does not support active connection migration.
   */
  uint8_t disable_active_migration;
  /**
   * :member:`retry_scid_present` is nonzero if :member:`retry_scid`
   * field is set.
   */
  uint8_t retry_scid_present;
  /**
   * :member:`preferred_address_present` is nonzero if
   * :member:`preferred_address` is set.
   */
  uint8_t preferred_address_present;
  /**
   * :member:`stateless_reset_token` contains stateless reset token.
   */
  uint8_t stateless_reset_token[NGTCP2_STATELESS_RESET_TOKENLEN];
} ngtcp2_transport_params;

/**
 * @struct
 *
 * :type:`ngtcp2_log` is ngtcp2 library internal logger.
 */
typedef struct ngtcp2_log ngtcp2_log;

/**
 * @enum
 *
 * :type:`ngtcp2_pktns_id` defines packet number space identifier.
 */
typedef enum ngtcp2_pktns_id {
  /**
   * :enum:`NGTCP2_PKTNS_ID_INITIAL` is the Initial packet number
   * space.
   */
  NGTCP2_PKTNS_ID_INITIAL,
  /**
   * :enum:`NGTCP2_PKTNS_ID_HANDSHAKE` is the Handshake packet number
   * space.
   */
  NGTCP2_PKTNS_ID_HANDSHAKE,
  /**
   * :enum:`NGTCP2_PKTNS_ID_APPLICATION` is the Application data
   * packet number space.
   */
  NGTCP2_PKTNS_ID_APPLICATION,
  /**
   * :enum:`NGTCP2_PKTNS_ID_MAX` is defined to get the number of
   * packet number spaces.
   */
  NGTCP2_PKTNS_ID_MAX
} ngtcp2_pktns_id;

/**
 * @struct
 *
 * :type:`ngtcp2_conn_stat` holds various connection statistics, and
 * computed data for recovery and congestion controller.
 */
typedef struct ngtcp2_conn_stat {
  /**
   * :member:`latest_rtt` is the latest RTT sample which is not
   * adjusted by acknowledgement delay.
   */
  ngtcp2_duration latest_rtt;
  /**
   * :member:`min_rtt` is the minimum RTT seen so far.  It is not
   * adjusted by acknowledgement delay.
   */
  ngtcp2_duration min_rtt;
  /**
   * :member:`smoothed_rtt` is the smoothed RTT.
   */
  ngtcp2_duration smoothed_rtt;
  /**
   * :member:`rttvar` is a mean deviation of observed RTT.
   */
  ngtcp2_duration rttvar;
  /**
   * :member:`initial_rtt` is the initial RTT which is used when no
   * RTT sample is available.
   */
  ngtcp2_duration initial_rtt;
  /**
   * :member:`first_rtt_sample_ts` is the timestamp when the first RTT
   * sample is obtained.
   */
  ngtcp2_tstamp first_rtt_sample_ts;
  /**
   * :member:`pto_count` is the count of successive PTO timer
   * expiration.
   */
  size_t pto_count;
  /**
   * :member:`loss_detection_timer` is the deadline of the current
   * loss detection timer.
   */
  ngtcp2_tstamp loss_detection_timer;
  /**
   * :member:`last_tx_pkt_ts` corresponds to
   * time_of_last_sent_ack_eliciting_packet in
   * draft-ietf-quic-recovery-32.
   */
  ngtcp2_tstamp last_tx_pkt_ts[NGTCP2_PKTNS_ID_MAX];
  /**
   * :member:`loss_time` corresponds to loss_time in
   * draft-ietf-quic-recovery-32.
   */
  ngtcp2_tstamp loss_time[NGTCP2_PKTNS_ID_MAX];
  /**
   * :member:`cwnd` is the size of congestion window.
   */
  uint64_t cwnd;
  /**
   * :member:`ssthresh` is slow start threshold.
   */
  uint64_t ssthresh;
  /**
   * :member:`congestion_recovery_start_ts` is the timestamp when
   * congestion recovery started.
   */
  ngtcp2_tstamp congestion_recovery_start_ts;
  /**
   * :member:`bytes_in_flight` is the number in bytes of all sent
   * packets which have not been acknowledged.
   */
  uint64_t bytes_in_flight;
  /**
   * :member:`max_udp_payload_size` is the maximum size of UDP
   * datagram payload that this endpoint transmits.  It is used by
   * congestion controller to compute congestion window.
   */
  size_t max_udp_payload_size;
  /**
   * :member:`delivery_rate_sec` is the current sending rate measured
   * in byte per second.
   */
  uint64_t delivery_rate_sec;
} ngtcp2_conn_stat;

/**
 * @enum
 *
 * :type:`ngtcp2_cc_algo` defines congestion control algorithms.
 */
typedef enum ngtcp2_cc_algo {
  /**
   * :enum:`NGTCP2_CC_ALGO_RENO` represents Reno.
   */
  NGTCP2_CC_ALGO_RENO = 0x00,
  /**
   * :enum:`NGTCP2_CC_ALGO_CUBIC` represents Cubic.
   */
  NGTCP2_CC_ALGO_CUBIC = 0x01,
  /**
   * :enum:`NGTCP2_CC_ALGO_CUSTOM` represents custom congestion
   * control algorithm.
   */
  NGTCP2_CC_ALGO_CUSTOM = 0xff
} ngtcp2_cc_algo;

/**
 * @struct
 *
 * :type:`ngtcp2_cc_base` is the base structure of custom congestion
 * control algorithm.  It must be the first field of custom congestion
 * controller.
 */
typedef struct ngtcp2_cc_base {
  /**
   * :member:`log` is ngtcp2 library internal logger.
   */
  ngtcp2_log *log;
} ngtcp2_cc_base;

/**
 * @struct
 *
 * :type:`ngtcp2_cc_pkt` is a convenient structure to include
 * acked/lost/sent packet.
 */
typedef struct ngtcp2_cc_pkt {
  /**
   * :member:`pkt_num` is the packet number
   */
  int64_t pkt_num;
  /**
   * :member:`pktlen` is the length of packet.
   */
  size_t pktlen;
  /**
   * :member:`pktns_id` is the ID of packet number space which this
   * packet belongs to.
   */
  ngtcp2_pktns_id pktns_id;
  /**
   * :member:`ts_sent` is the timestamp when packet is sent.
   */
  ngtcp2_tstamp ts_sent;
} ngtcp2_cc_pkt;

typedef struct ngtcp2_cc ngtcp2_cc;

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_on_pkt_acked` is a callback function which is
 * called with an acknowledged packet.
 */
typedef void (*ngtcp2_cc_on_pkt_acked)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                       const ngtcp2_cc_pkt *pkt,
                                       ngtcp2_tstamp ts);

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_congestion_event` is a callback function which is
 * called when congestion event happens (e.g., when packet is lost).
 */
typedef void (*ngtcp2_cc_congestion_event)(ngtcp2_cc *cc,
                                           ngtcp2_conn_stat *cstat,
                                           ngtcp2_tstamp ts_sent,
                                           ngtcp2_tstamp ts);

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_on_persistent_congestion` is a callback function
 * which is called when persistent congestion is established.
 */
typedef void (*ngtcp2_cc_on_persistent_congestion)(ngtcp2_cc *cc,
                                                   ngtcp2_conn_stat *cstat,
                                                   ngtcp2_tstamp ts);

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_on_ack_recv` is a callback function which is
 * called when an acknowledgement is received.
 */
typedef void (*ngtcp2_cc_on_ack_recv)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                      ngtcp2_tstamp ts);

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_on_pkt_sent` is a callback function which is
 * called when an ack-eliciting packet is sent.
 */
typedef void (*ngtcp2_cc_on_pkt_sent)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                      const ngtcp2_cc_pkt *pkt);

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_new_rtt_sample` is a callback function which is
 * called when new RTT sample is obtained.
 */
typedef void (*ngtcp2_cc_new_rtt_sample)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                         ngtcp2_tstamp ts);

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_reset` is a callback function which is called when
 * congestion state must be reset.
 */
typedef void (*ngtcp2_cc_reset)(ngtcp2_cc *cc);

/**
 * @enum
 *
 * :type:`ngtcp2_cc_event_type` defines congestion control events.
 */
typedef enum ngtcp2_cc_event_type {
  /**
   * :enum:`NGTCP2_CC_EVENT_TX_START` occurs when ack-eliciting packet
   * is sent and no other ack-eliciting packet is present.
   */
  NGTCP2_CC_EVENT_TYPE_TX_START
} ngtcp2_cc_event_type;

/**
 * @functypedef
 *
 * :type:`ngtcp2_cc_event` is a callback function which is called when
 * a specific event happens.
 */
typedef void (*ngtcp2_cc_event)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                ngtcp2_cc_event_type event, ngtcp2_tstamp ts);

/**
 * @struct
 *
 * :type:`ngtcp2_cc` is congestion control algorithm interface to
 * allow custom implementation.
 */
typedef struct ngtcp2_cc {
  /**
   * :member:`ccb` is a pointer to :type:`ngtcp2_cc_base` which
   * usually contains a state.
   */
  ngtcp2_cc_base *ccb;
  /**
   * :member:`on_pkt_acked` is a callback function which is called
   * when a packet is acknowledged.
   */
  ngtcp2_cc_on_pkt_acked on_pkt_acked;
  /**
   * :member:`congestion_event` is a callback function which is called
   * when congestion event happens (.e.g, packet is lost).
   */
  ngtcp2_cc_congestion_event congestion_event;
  /**
   * :member:`on_persistent_congestion` is a callback function which
   * is called when persistent congestion is established.
   */
  ngtcp2_cc_on_persistent_congestion on_persistent_congestion;
  /**
   * :member:`on_ack_recv` is a callback function which is called when
   * an acknowledgement is received.
   */
  ngtcp2_cc_on_ack_recv on_ack_recv;
  /**
   * :member:`on_pkt_sent` is a callback function which is called when
   * ack-eliciting packet is sent.
   */
  ngtcp2_cc_on_pkt_sent on_pkt_sent;
  /**
   * :member:`new_rtt_sample` is a callback function which is called
   * when new RTT sample is obtained.
   */
  ngtcp2_cc_new_rtt_sample new_rtt_sample;
  /**
   * :member:`reset` is a callback function which is called when
   * congestion control state must be reset.
   */
  ngtcp2_cc_reset reset;
  /**
   * :member:`event` is a callback function which is called when a
   * specific event happens.
   */
  ngtcp2_cc_event event;
} ngtcp2_cc;

/**
 * @functypedef
 *
 * :type:`ngtcp2_printf` is a callback function for logging.
 * |user_data| is the same object passed to `ngtcp2_conn_client_new`
 * or `ngtcp2_conn_server_new`.
 */
typedef void (*ngtcp2_printf)(void *user_data, const char *format, ...);

/**
 * @macrosection
 *
 * QLog related macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_QLOG_WRITE_FLAG_NONE` indicates no flag set.
 */
#define NGTCP2_QLOG_WRITE_FLAG_NONE 0
/**
 * @macro
 *
 * :macro:`NGTCP2_QLOG_WRITE_FLAG_FIN` indicates that this is the
 * final call to :type:`ngtcp2_qlog_write` in the current connection.
 */
#define NGTCP2_QLOG_WRITE_FLAG_FIN 0x01

/**
 * @struct
 *
 * :type:`ngtcp2_rand_ctx` is a wrapper around native random number
 * generator.  It is opaque to the ngtcp2 library.  This might be
 * useful if application needs to specify random number generator per
 * thread or per connection.
 */
typedef struct ngtcp2_rand_ctx {
  /**
   * :member:`native_handle` is a pointer to an underlying random
   * number generator.
   */
  void *native_handle;
} ngtcp2_rand_ctx;

/**
 * @functypedef
 *
 * :type:`ngtcp2_qlog_write` is a callback function which is called to
 * write qlog |data| of length |datalen| bytes.  |flags| is bitwise OR
 * of zero or more of NGTCP2_QLOG_WRITE_FLAG_*.  See
 * :macro:`NGTCP2_QLOG_WRITE_FLAG_NONE`.  If
 * :macro:`NGTCP2_QLOG_WRITE_FLAG_FIN` is set, |datalen| may be 0.
 */
typedef void (*ngtcp2_qlog_write)(void *user_data, uint32_t flags,
                                  const void *data, size_t datalen);

/**
 * @struct
 *
 * :type:`ngtcp2_qlog_settings` is a set of settings for qlog.
 */
typedef struct ngtcp2_qlog_settings {
  /**
   * :member:`odcid` is Original Destination Connection ID sent by
   * client.  It is used as group_id and ODCID fields.  Client ignores
   * this field and uses dcid parameter passed to
   * `ngtcp2_conn_client_new()`.
   */
  ngtcp2_cid odcid;
  /**
   * :member:`write` is a callback function to write qlog.  Setting
   * ``NULL`` disables qlog.
   */
  ngtcp2_qlog_write write;
} ngtcp2_qlog_settings;

/**
 * @struct
 *
 * :type:`ngtcp2_settings` defines QUIC connection settings.
 */
typedef struct ngtcp2_settings {
  /**
   * :member:`qlog` is qlog settings.
   */
  ngtcp2_qlog_settings qlog;
  /**
   * :member:`cc_algo` specifies congestion control algorithm.  If
   * :enum:`ngtcp2_cc_algo.NGTCP2_CC_ALGO_CUSTOM` is set, :member:`cc`
   * must be set to a pointer to custom congestion control algorithm.
   */
  ngtcp2_cc_algo cc_algo;
  /**
   * :member:`cc` is a pointer to custom congestion control algorithm.
   * :member:`cc_algo` must be set to
   * :enum:`ngtcp2_cc_algo.NGTCP2_CC_ALGO_CUSTOM` in order to enable
   * custom congestion control algorithm.
   */
  ngtcp2_cc *cc;
  /**
   * :member:`initial_ts` is an initial timestamp given to the
   * library.
   */
  ngtcp2_tstamp initial_ts;
  /**
   * :member:`initial_rtt` is an initial RTT.
   */
  ngtcp2_duration initial_rtt;
  /**
   * :member:`log_printf` is a function that the library uses to write
   * logs.  ``NULL`` means no logging output.  It is nothing to do
   * with qlog.
   */
  ngtcp2_printf log_printf;
  /**
   * :member:`max_udp_payload_size` is the maximum size of UDP
   * datagram payload that this endpoint transmits.  It is used by
   * congestion controller to compute congestion window.  If it is set
   * to 0, it defaults to :macro:`NGTCP2_DEFAULT_MAX_PKTLEN`.
   */
  size_t max_udp_payload_size;
  /**
   * :member:`token` is a token from Retry packet or NEW_TOKEN frame.
   *
   * Server sets this field if it received the token in Client Initial
   * packet and successfully validated.
   *
   * Client sets this field if it intends to send token in its Initial
   * packet.
   *
   * `ngtcp2_conn_server_new` and `ngtcp2_conn_client_new` make a copy
   * of token.
   */
  ngtcp2_vec token;
  /**
   * :member:`rand_ctx` is an optional random number generator to be
   * passed to :type:`ngtcp2_rand` callback.
   */
  ngtcp2_rand_ctx rand_ctx;
  /**
   * :member:`max_window` is the maximum connection-level flow control
   * window if connection-level window auto-tuning is enabled.  The
   * connection-level window auto tuning is enabled if nonzero value
   * is specified in this field.  The initial value of window size is
   * :member:`ngtcp2_transport_params.initial_max_data`.  The window
   * size is scaled up to the value specified in this field.
   */
  uint64_t max_window;
  /**
   * :member:`max_stream_window` is the maximum stream-level flow
   * control window if stream-level window auto-tuning is enabled.
   * The stream-level window auto-tuning is enabled if nonzero value
   * is specified in this field.  The initial value of window size is
   * :member:`ngtcp2_transport_params.initial_max_stream_data_bidi_remote`,
   * :member:`ngtcp2_transport_params.initial_max_stream_data_bidi_local`,
   * or :member:`ngtcp2_transport_params.initial_max_stream_data_uni`,
   * depending on the type of stream.  The window size is scaled up to
   * the value specified in this field.
   */
  uint64_t max_stream_window;
  /**
   * :member:`ack_thresh` is the maximum number of unacknowledged
   * packets before sending acknowledgement.  It triggers the
   * immediate acknowledgement.
   */
  size_t ack_thresh;
} ngtcp2_settings;

/**
 * @struct
 *
 * :type:`ngtcp2_addr` is the endpoint address.
 */
typedef struct ngtcp2_addr {
  /**
   * :member:`addrlen` is the length of addr.
   */
  size_t addrlen;
  /**
   * :member:`addr` points to the buffer which contains endpoint
   * address.  It must not be ``NULL``.
   */
  struct sockaddr *addr;
  /**
   * :member:`user_data` is an arbitrary data and opaque to the
   * library.
   */
  void *user_data;
} ngtcp2_addr;

/**
 * @struct
 *
 * :type:`ngtcp2_path` is the network endpoints where a packet is sent
 * and received.
 */
typedef struct ngtcp2_path {
  /**
   * :member:`local` is the address of local endpoint.
   */
  ngtcp2_addr local;
  /**
   * :member:`remote` is the address of remote endpoint.
   */
  ngtcp2_addr remote;
} ngtcp2_path;

/**
 * @struct
 *
 * :type:`ngtcp2_path_storage` is a convenient struct to have buffers
 * to store the longest addresses.
 */
typedef struct ngtcp2_path_storage {
  /**
   * :member:`local_addrbuf` is a buffer to store local address.
   */
  struct sockaddr_storage local_addrbuf;
  /**
   * :member:`remote_addrbuf` is a buffer to store remote address.
   */
  struct sockaddr_storage remote_addrbuf;
  /**
   * :member:`path` stores network path.
   */
  ngtcp2_path path;
} ngtcp2_path_storage;

/**
 * @struct
 *
 * :type:`ngtcp2_crypto_md` is a wrapper around native message digest
 * object.
 */
typedef struct ngtcp2_crypto_md {
  /**
   * :member:`native_handle` is a pointer to an underlying message
   * digest object.
   */
  void *native_handle;
} ngtcp2_crypto_md;

/**
 * @struct
 *
 * :type:`ngtcp2_crypto_aead` is a wrapper around native AEAD object.
 */
typedef struct ngtcp2_crypto_aead {
  /**
   * :member:`native_handle` is a pointer to an underlying AEAD
   * object.
   */
  void *native_handle;
  /**
   * :member:`max_overhead` is the number of additional bytes which
   * AEAD encryption needs on encryption.
   */
  size_t max_overhead;
} ngtcp2_crypto_aead;

/**
 * @struct
 *
 * :type:`ngtcp2_crypto_cipher` is a wrapper around native cipher
 * object.
 */
typedef struct ngtcp2_crypto_cipher {
  /**
   * :member:`native_handle` is a pointer to an underlying cipher
   * object.
   */
  void *native_handle;
} ngtcp2_crypto_cipher;

/**
 * @struct
 *
 * :type:`ngtcp2_crypto_aead_ctx` is a wrapper around native AEAD
 * cipher context object.  It should be initialized with a specific
 * key.  ngtcp2 library reuses this context object to encrypt or
 * decrypt multiple packets.
 */
typedef struct ngtcp2_crypto_aead_ctx {
  /**
   * :member:`native_handle` is a pointer to an underlying AEAD
   * context object.
   */
  void *native_handle;
} ngtcp2_crypto_aead_ctx;

/**
 * @struct
 *
 * :type:`ngtcp2_crypto_cipher_ctx` is a wrapper around native cipher
 * context object.  It should be initialized with a specific key.
 * ngtcp2 library reuses this context object to encrypt or decrypt
 * multiple packet headers.
 */
typedef struct ngtcp2_crypto_cipher_ctx {
  /**
   * :member:`native_handle` is a pointer to an underlying cipher
   * context object.
   */
  void *native_handle;
} ngtcp2_crypto_cipher_ctx;

/**
 * @struct
 *
 * :type:`ngtcp2_crypto_ctx` is a convenient structure to bind all
 * crypto related objects in one place.  Use
 * `ngtcp2_crypto_ctx_initial` to initialize this struct for Initial
 * packet encryption.  For Handshake and Short packets, use
 * `ngtcp2_crypto_ctx_tls`.
 */
typedef struct ngtcp2_crypto_ctx {
  /**
   * :member:`aead` is AEAD object.
   */
  ngtcp2_crypto_aead aead;
  /**
   * :member:`md` is message digest object.
   */
  ngtcp2_crypto_md md;
  /**
   * :member:`hp` is header protection cipher.
   */
  ngtcp2_crypto_cipher hp;
  /**
   * :member:`max_encryption` is the number of encryption which this
   * key can be used with.
   */
  uint64_t max_encryption;
  /**
   * :member:`max_decryption_failure` is the number of decryption
   * failure with this key.
   */
  uint64_t max_decryption_failure;
} ngtcp2_crypto_ctx;

/**
 * @function
 *
 * `ngtcp2_encode_transport_params` encodes |params| in |dest| of
 * length |destlen|.
 *
 * This function returns the number of written, or one of the
 * following negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`:
 *     |exttype| is invalid.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_encode_transport_params(
    uint8_t *dest, size_t destlen, ngtcp2_transport_params_type exttype,
    const ngtcp2_transport_params *params);

/**
 * @function
 *
 * `ngtcp2_decode_transport_params` decodes transport parameters in
 * |data| of length |datalen|, and stores the result in the object
 * pointed by |params|.
 *
 * If the optional parameters are missing, the default value is
 * assigned.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM`
 *     The required parameter is missing.
 * :macro:`NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM`
 *     The input is malformed.
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`:
 *     |exttype| is invalid.
 */
NGTCP2_EXTERN int
ngtcp2_decode_transport_params(ngtcp2_transport_params *params,
                               ngtcp2_transport_params_type exttype,
                               const uint8_t *data, size_t datalen);

/**
 * @function
 *
 * `ngtcp2_pkt_decode_version_cid` extracts QUIC version, Destination
 * Connection ID and Source Connection ID from the packet pointed by
 * |data| of length |datalen|.  This function can handle Connection ID
 * up to 255 bytes unlike `ngtcp2_pkt_decode_hd_long` or
 * `ngtcp2_pkt_decode_hd_short` which are only capable of handling
 * Connection ID less than or equal to :macro:`NGTCP2_MAX_CIDLEN`.
 * Longer Connection ID is only valid if the version is unsupported
 * QUIC version.
 *
 * If the given packet is Long packet, this function extracts the
 * version from the packet and assigns it to |*pversion|.  It also
 * extracts the pointer to the Destination Connection ID and its
 * length and assigns them to |*pdcid| and |*pdcidlen| respectively.
 * Similarly, it extracts the pointer to the Source Connection ID and
 * its length and assigns them to |*pscid| and |*pscidlen|
 * respectively.
 *
 * If the given packet is Short packet, |*pversion| will be 0,
 * |*pscid| will be ``NULL``, and |*pscidlen| will be 0.  Because the
 * Short packet does not have the length of Destination Connection ID,
 * the caller has to pass the length in |short_dcidlen|.  This
 * function extracts the pointer to the Destination Connection ID and
 * assigns it to |*pdcid|.  |short_dcidlen| is assigned to
 * |*pdcidlen|.
 *
 * This function returns 0 or 1 if it succeeds.  It returns 1 if
 * Version Negotiation packet should be sent.  Otherwise, one of the
 * following negative error code:
 *
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     The function could not decode the packet header.
 */
NGTCP2_EXTERN int
ngtcp2_pkt_decode_version_cid(uint32_t *pversion, const uint8_t **pdcid,
                              size_t *pdcidlen, const uint8_t **pscid,
                              size_t *pscidlen, const uint8_t *data,
                              size_t datalen, size_t short_dcidlen);

/**
 * @function
 *
 * `ngtcp2_pkt_decode_hd_long` decodes QUIC long packet header in
 * |pkt| of length |pktlen|.  This function only parses the input just
 * before packet number field.
 *
 * This function does not verify that length field is correct.  In
 * other words, this function succeeds even if length > |pktlen|.
 *
 * This function can handle Connection ID up to
 * :macro:`NGTCP2_MAX_CIDLEN`.  Consider to use
 * `ngtcp2_pkt_decode_version_cid` to get longer Connection ID.
 *
 * This function handles Version Negotiation specially.  If version
 * field is 0, |pkt| must contain Version Negotiation packet.  Version
 * Negotiation packet has random type in wire format.  For
 * convenience, this function sets
 * :enum:`ngtcp2_pkt_type.NGTCP2_PKT_VERSION_NEGOTIATION` to
 * dest->type, and set dest->payloadlen and dest->pkt_num to 0.
 * Version Negotiation packet occupies a single packet.
 *
 * It stores the result in the object pointed by |dest|, and returns
 * the number of bytes decoded to read the packet header if it
 * succeeds, or one of the following error codes:
 *
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     Packet is too short; or it is not a long header
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_decode_hd_long(ngtcp2_pkt_hd *dest,
                                                     const uint8_t *pkt,
                                                     size_t pktlen);

/**
 * @function
 *
 * `ngtcp2_pkt_decode_hd_short` decodes QUIC short packet header in
 * |pkt| of length |pktlen|.  |dcidlen| is the length of DCID in
 * packet header.  Short packet does not encode the length of
 * connection ID, thus we need the input from the outside.  This
 * function only parses the input just before packet number field.
 * This function can handle Connection ID up to
 * :macro:`NGTCP2_MAX_CIDLEN`.  Consider to use
 * `ngtcp2_pkt_decode_version_cid` to get longer Connection ID.  It
 * stores the result in the object pointed by |dest|, and returns the
 * number of bytes decoded to read the packet header if it succeeds,
 * or one of the following error codes:
 *
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     Packet is too short; or it is not a short header
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_decode_hd_short(ngtcp2_pkt_hd *dest,
                                                      const uint8_t *pkt,
                                                      size_t pktlen,
                                                      size_t dcidlen);

/**
 * @function
 *
 * `ngtcp2_pkt_write_stateless_reset` writes Stateless Reset packet in
 * the buffer pointed by |dest| whose length is |destlen|.
 * |stateless_reset_token| is a pointer to the Stateless Reset Token,
 * and its length must be :macro:`NGTCP2_STATELESS_RESET_TOKENLEN`
 * bytes long.  |rand| specifies the random octets preceding Stateless
 * Reset Token.  The length of |rand| is specified by |randlen| which
 * must be at least :macro:`NGTCP2_MIN_STATELESS_RETRY_RANDLEN` bytes
 * long.
 *
 * If |randlen| is too long to write them all in the buffer, |rand| is
 * written to the buffer as much as possible, and is truncated.
 *
 * This function returns the number of bytes written to the buffer, or
 * one of the following negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     |randlen| is strictly less than
 *     :macro:`NGTCP2_MIN_STATELESS_RETRY_RANDLEN`.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_write_stateless_reset(
    uint8_t *dest, size_t destlen, const uint8_t *stateless_reset_token,
    const uint8_t *rand, size_t randlen);

/**
 * @function
 *
 * `ngtcp2_pkt_write_version_negotiation` writes Version Negotiation
 * packet in the buffer pointed by |dest| whose length is |destlen|.
 * |unused_random| should be generated randomly.  |dcid| is the
 * destination connection ID which appears in a packet as a source
 * connection ID sent by client which caused version negotiation.
 * Similarly, |scid| is the source connection ID which appears in a
 * packet as a destination connection ID sent by client.  |sv| is a
 * list of supported versions, and |nsv| specifies the number of
 * supported versions included in |sv|.
 *
 * This function returns the number of bytes written to the buffer, or
 * one of the following negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_write_version_negotiation(
    uint8_t *dest, size_t destlen, uint8_t unused_random, const uint8_t *dcid,
    size_t dcidlen, const uint8_t *scid, size_t scidlen, const uint32_t *sv,
    size_t nsv);

/**
 * @struct
 *
 * :type:`ngtcp2_conn` represents a single QUIC connection.
 */
typedef struct ngtcp2_conn ngtcp2_conn;

/**
 * @functypedef
 *
 * :type:`ngtcp2_client_initial` is invoked when client application
 * asks TLS stack to produce first TLS cryptographic handshake data.
 *
 * This implementation of this callback must get the first handshake
 * data from TLS stack and pass it to ngtcp2 library using
 * `ngtcp2_conn_submit_crypto_data` function.  Make sure that before
 * calling `ngtcp2_conn_submit_crypto_data` function, client
 * application must create initial packet protection keys and IVs, and
 * provide them to ngtcp2 library using `ngtcp2_conn_set_initial_key`
 * and
 *
 * This callback function must return 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
 * return immediately.
 *
 * TODO: Define error code for TLS stack failure.  Suggestion:
 * NGTCP2_ERR_CRYPTO.
 */
typedef int (*ngtcp2_client_initial)(ngtcp2_conn *conn, void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_client_initial` is invoked when server receives
 * Initial packet from client.  An server application must implement
 * this callback, and generate initial keys and IVs for both
 * transmission and reception.  Install them using
 * `ngtcp2_conn_set_initial_key`.  |dcid| is the destination
 * connection ID which client generated randomly.  It is used to
 * derive initial packet protection keys.
 *
 * The callback function must return 0 if it succeeds.  If an error
 * occurs, return :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the
 * library call return immediately.
 *
 * TODO: Define error code for TLS stack failure.  Suggestion:
 * NGTCP2_ERR_CRYPTO.
 */
typedef int (*ngtcp2_recv_client_initial)(ngtcp2_conn *conn,
                                          const ngtcp2_cid *dcid,
                                          void *user_data);

/**
 * @enum
 *
 * :type:`ngtcp2_crypto_level` is encryption level.
 */
typedef enum ngtcp2_crypto_level {
  /**
   * :enum:`NGTCP2_CRYPTO_LEVEL_INITIAL` is Initial Keys encryption
   * level.
   */
  NGTCP2_CRYPTO_LEVEL_INITIAL,
  /**
   * :enum:`NGTCP2_CRYPTO_LEVEL_HANDSHAKE` is Handshake Keys
   * encryption level.
   */
  NGTCP2_CRYPTO_LEVEL_HANDSHAKE,
  /**
   * :enum:`NGTCP2_CRYPTO_LEVEL_APPLICATION` is Application Data
   * (1-RTT) Keys encryption level.
   */
  NGTCP2_CRYPTO_LEVEL_APPLICATION,
  /**
   * :enum:`NGTCP2_CRYPTO_LEVEL_EARLY` is Early Data (0-RTT) Keys
   * encryption level.
   */
  NGTCP2_CRYPTO_LEVEL_EARLY
} ngtcp2_crypto_level;

/**
 * @functypedef
 *
 * :type`ngtcp2_recv_crypto_data` is invoked when crypto data is
 * received.  The received data is pointed to by |data|, and its
 * length is |datalen|.  The |offset| specifies the offset where
 * |data| is positioned.  |user_data| is the arbitrary pointer passed
 * to `ngtcp2_conn_client_new` or `ngtcp2_conn_server_new`.  The
 * ngtcp2 library ensures that the crypto data is passed to the
 * application in the increasing order of |offset|.  |datalen| is
 * always strictly greater than 0.  |crypto_level| indicates the
 * encryption level where this data is received.  Crypto data can
 * never be received in
 * :enum:`ngtcp2_crypto_level.NGTCP2_CRYPTO_LEVEL_EARLY`.
 *
 * The application should provide the given data to TLS stack.
 *
 * The callback function must return 0 if it succeeds.  If TLS stack
 * reported error, return :macro:`NGTCP2_ERR_CRYPTO`.  If application
 * encounters fatal error, return :macro:`NGTCP2_ERR_CALLBACK_FAILURE`
 * which makes the library call return immediately.  If the other
 * value is returned, it is treated as
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`.
 */
typedef int (*ngtcp2_recv_crypto_data)(ngtcp2_conn *conn,
                                       ngtcp2_crypto_level crypto_level,
                                       uint64_t offset, const uint8_t *data,
                                       size_t datalen, void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_handshake_completed` is invoked when QUIC
 * cryptographic handshake has completed.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_handshake_completed)(ngtcp2_conn *conn, void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_handshake_confirmed` is invoked when QUIC
 * cryptographic handshake is confirmed.  The handshake confirmation
 * means that both endpoints agree that handshake has finished.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_handshake_confirmed)(ngtcp2_conn *conn, void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_version_negotiation` is invoked when Version
 * Negotiation packet is received.  |hd| is the pointer to the QUIC
 * packet header object.  The vector |sv| of |nsv| elements contains
 * the QUIC version the server supports.  Since Version Negotiation is
 * only sent by server, this callback function is used by client only.
 *
 * The callback function must return 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
 * return immediately.
 */
typedef int (*ngtcp2_recv_version_negotiation)(ngtcp2_conn *conn,
                                               const ngtcp2_pkt_hd *hd,
                                               const uint32_t *sv, size_t nsv,
                                               void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_retry` is invoked when Retry packet is received.
 * This callback is client only.
 *
 * Application must regenerate packet protection key, IV, and header
 * protection key for Initial packets using the destination connection
 * ID obtained by `ngtcp2_conn_get_dcid()` and install them by calling
 * `ngtcp2_conn_install_initial_key()`.
 *
 * 0-RTT data accepted by the ngtcp2 library will be retransmitted by
 * the library automatically.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_recv_retry)(ngtcp2_conn *conn, const ngtcp2_pkt_hd *hd,
                                 void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_encrypt` is invoked when the ngtcp2 library asks the
 * application to encrypt packet payload.  The packet payload to
 * encrypt is passed as |plaintext| of length |plaintextlen|.  The
 * AEAD cipher is |aead|.  |aead_ctx| is the AEAD cipher context
 * object which is initialized with encryption key.  The nonce is
 * passed as |nonce| of length |noncelen|.  The ad, Additional Data to
 * AEAD, is passed as |ad| of length |adlen|.
 *
 * The implementation of this callback must encrypt |plaintext| using
 * the negotiated cipher suite and write the ciphertext into the
 * buffer pointed by |dest|.  |dest| has enough capacity to store the
 * ciphertext.
 *
 * |dest| and |plaintext| may point to the same buffer.
 *
 * The callback function must return 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
 * return immediately.
 */
typedef int (*ngtcp2_encrypt)(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                              const ngtcp2_crypto_aead_ctx *aead_ctx,
                              const uint8_t *plaintext, size_t plaintextlen,
                              const uint8_t *nonce, size_t noncelen,
                              const uint8_t *ad, size_t adlen);

/**
 * @functypedef
 *
 * :type:`ngtcp2_decrypt` is invoked when the ngtcp2 library asks the
 * application to decrypt packet payload.  The packet payload to
 * decrypt is passed as |ciphertext| of length |ciphertextlen|.  The
 * AEAD cipher is |aead|.  |aead_ctx| is the AEAD cipher context
 * object which is initialized with decryption key.  The nonce is
 * passed as |nonce| of length |noncelen|.  The ad, Additional Data to
 * AEAD, is passed as |ad| of length |adlen|.
 *
 * The implementation of this callback must decrypt |ciphertext| using
 * the negotiated cipher suite and write the ciphertext into the
 * buffer pointed by |dest|.  |dest| has enough capacity to store the
 * cleartext.
 *
 * |dest| and |ciphertext| may point to the same buffer.
 *
 * The callback function must return 0 if it succeeds.  If TLS stack
 * fails to decrypt data, return :macro:`NGTCP2_ERR_TLS_DECRYPT`.  For
 * any other errors, return :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which
 * makes the library call return immediately.
 */
typedef int (*ngtcp2_decrypt)(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                              const ngtcp2_crypto_aead_ctx *aead_ctx,
                              const uint8_t *ciphertext, size_t ciphertextlen,
                              const uint8_t *nonce, size_t noncelen,
                              const uint8_t *ad, size_t adlen);

/**
 * @functypedef
 *
 * :type:`ngtcp2_hp_mask` is invoked when the ngtcp2 library asks the
 * application to produce mask to encrypt or decrypt packet header.
 * The encryption cipher is |hp|.  |hp_ctx| is the cipher context
 * object which is initialized with header protection key.  The sample
 * is passed as |sample|.
 *
 * The implementation of this callback must produce a mask using the
 * header protection cipher suite specified by QUIC specification and
 * write the result into the buffer pointed by |dest|.  The length of
 * mask must be :macro:`NGTCP2_HP_MASKLEN`.  The library ensures that
 * |dest| has enough capacity.
 *
 * The callback function must return 0 if it succeeds, or
 *  :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
 *  return immediately.
 */
typedef int (*ngtcp2_hp_mask)(uint8_t *dest, const ngtcp2_crypto_cipher *hp,
                              const ngtcp2_crypto_cipher_ctx *hp_ctx,
                              const uint8_t *sample);

/**
 * @macrosection
 *
 * Stream data flags
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_STREAM_DATA_FLAG_NONE` indicates no flag set.
 */
#define NGTCP2_STREAM_DATA_FLAG_NONE 0x00

/**
 * @macro
 *
 * :macro:`NGTCP2_STREAM_DATA_FLAG_FIN` indicates that this chunk of
 * data is final piece of an incoming stream.
 */
#define NGTCP2_STREAM_DATA_FLAG_FIN 0x01

/**
 * @macro
 *
 * :macro:`NGTCP2_STREAM_DATA_FLAG_EARLY` indicates that this chunk of
 * data contains data received in 0RTT packet and the handshake has
 * not completed yet, which means that the data might be replayed.
 */
#define NGTCP2_STREAM_DATA_FLAG_EARLY 0x02

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_stream_data` is invoked when stream data is
 * received.  The stream is specified by |stream_id|.  |flags| is the
 * bitwise-OR of zero or more of NGTCP2_STREAM_DATA_FLAG_*.  See
 * :macro:`NGTCP2_STREAM_DATA_FLAG_NONE`.  If |flags| &
 * :macro:`NGTCP2_STREAM_DATA_FLAG_FIN` is nonzero, this portion of
 * the data is the last data in this stream.  |offset| is the offset
 * where this data begins.  The library ensures that data is passed to
 * the application in the non-decreasing order of |offset|.  The data
 * is passed as |data| of length |datalen|.  |datalen| may be 0 if and
 * only if |fin| is nonzero.
 *
 * If :macro:`NGTCP2_STREAM_DATA_FLAG_EARLY` is set in |flags|, it
 * indicates that a part of or whole data was received in 0RTT packet
 * and a handshake has not completed yet.
 *
 * The callback function must return 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library return
 * immediately.
 */
typedef int (*ngtcp2_recv_stream_data)(ngtcp2_conn *conn, uint32_t flags,
                                       int64_t stream_id, uint64_t offset,
                                       const uint8_t *data, size_t datalen,
                                       void *user_data, void *stream_user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_stream_open` is a callback function which is called
 * when remote stream is opened by peer.  This function is not called
 * if stream is opened by implicitly (we might reconsider this
 * behaviour).
 *
 * The implementation of this callback should return 0 if it succeeds.
 * Returning :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
 * call return immediately.
 */
typedef int (*ngtcp2_stream_open)(ngtcp2_conn *conn, int64_t stream_id,
                                  void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_stream_close` is invoked when a stream is closed.
 * This callback is not called when QUIC connection is closed before
 * existing streams are closed.  |app_error_code| indicates the error
 * code of this closure.
 *
 * The implementation of this callback should return 0 if it succeeds.
 * Returning :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
 * call return immediately.
 */
typedef int (*ngtcp2_stream_close)(ngtcp2_conn *conn, int64_t stream_id,
                                   uint64_t app_error_code, void *user_data,
                                   void *stream_user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_stream_reset` is invoked when a stream identified by
 * |stream_id| is reset by a remote endpoint.
 *
 * The implementation of this callback should return 0 if it succeeds.
 * Returning :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
 * call return immediately.
 */
typedef int (*ngtcp2_stream_reset)(ngtcp2_conn *conn, int64_t stream_id,
                                   uint64_t final_size, uint64_t app_error_code,
                                   void *user_data, void *stream_user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_acked_stream_data_offset` is a callback function
 * which is called when stream data is acked, and application can free
 * the data.  The acked range of data is [offset, offset + datalen).
 * For a given stream_id, this callback is called sequentially in
 * increasing order of |offset|.  |datalen| is normally strictly
 * greater than 0.  One exception is that when a packet which includes
 * STREAM frame which has fin flag set, and 0 length data, this
 * callback is invoked with 0 passed as |datalen|.
 *
 * If a stream is closed prematurely and stream data is still
 * in-flight, this callback function is not called for those data.
 *
 * The implementation of this callback should return 0 if it succeeds.
 * Returning :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
 * call return immediately.
 */
typedef int (*ngtcp2_acked_stream_data_offset)(
    ngtcp2_conn *conn, int64_t stream_id, uint64_t offset, uint64_t datalen,
    void *user_data, void *stream_user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_acked_crypto_offset` is a callback function which is
 * called when crypto stream data is acknowledged, and application can
 * free the data.  |crypto_level| indicates the encryption level where
 * this data was sent.  Crypto data never be sent in
 * :enum:`ngtcp2_crypto_level.NGTCP2_CRYPTO_LEVEL_EARLY`.  This works
 * like :type:`ngtcp2_acked_stream_data_offset` but crypto stream has
 * no stream_id and stream_user_data, and |datalen| never become 0.
 *
 * The implementation of this callback should return 0 if it succeeds.
 * Returning :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
 * call return immediately.
 */
typedef int (*ngtcp2_acked_crypto_offset)(ngtcp2_conn *conn,
                                          ngtcp2_crypto_level crypto_level,
                                          uint64_t offset, uint64_t datalen,
                                          void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_stateless_reset` is a callback function which is
 * called when Stateless Reset packet is received.  The stateless
 * reset details are given in |sr|.
 *
 * The implementation of this callback should return 0 if it succeeds.
 * Returning :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
 * call return immediately.
 */
typedef int (*ngtcp2_recv_stateless_reset)(ngtcp2_conn *conn,
                                           const ngtcp2_pkt_stateless_reset *sr,
                                           void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_extend_max_streams` is a callback function which is
 * called every time max stream ID is strictly extended.
 * |max_streams| is the cumulative number of streams which an endpoint
 * can open.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_extend_max_streams)(ngtcp2_conn *conn,
                                         uint64_t max_streams, void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_extend_max_stream_data` is a callback function which
 * is invoked when max stream data is extended.  |stream_id|
 * identifies the stream.  |max_data| is a cumulative number of bytes
 * the endpoint can send on this stream.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_extend_max_stream_data)(ngtcp2_conn *conn,
                                             int64_t stream_id,
                                             uint64_t max_data, void *user_data,
                                             void *stream_user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_rand` is a callback function to get randomized byte
 * string from application.  Application must fill random |destlen|
 * bytes to the buffer pointed by |dest|.  |usage| provides the usage
 * of the generated random data.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_rand)(uint8_t *dest, size_t destlen,
                           const ngtcp2_rand_ctx *rand_ctx,
                           ngtcp2_rand_usage usage);

/**
 * @functypedef
 *
 * :type:`ngtcp2_get_new_connection_id` is a callback function to ask
 * an application for new connection ID.  Application must generate
 * new unused connection ID with the exact |cidlen| bytes and store it
 * in |cid|.  It also has to generate stateless reset token into
 * |token|.  The length of stateless reset token is
 * :macro:`NGTCP2_STATELESS_RESET_TOKENLEN` and it is guaranteed that
 * the buffer pointed by |cid| has the sufficient space to store the
 * token.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_get_new_connection_id)(ngtcp2_conn *conn, ngtcp2_cid *cid,
                                            uint8_t *token, size_t cidlen,
                                            void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_remove_connection_id` is a callback function which
 * notifies the application that connection ID |cid| is no longer used
 * by remote endpoint.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_remove_connection_id)(ngtcp2_conn *conn,
                                           const ngtcp2_cid *cid,
                                           void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_update_key` is a callback function which tells the
 * application that it must generate new packet protection keying
 * materials and AEAD cipher context objects with new keys.  The
 * current set of secrets are given as |current_rx_secret| and
 * |current_tx_secret| of length |secretlen|.  They are decryption and
 * encryption secrets respectively.
 *
 * The application has to generate new secrets and keys for both
 * encryption and decryption, and write decryption secret and IV to
 * the buffer pointed by |rx_secret| and |rx_iv| respectively.  It
 * also has to create new AEAD cipher context object with new
 * decryption key and initialize |rx_aead_ctx| with it.  Similarly,
 * write encryption secret and IV to the buffer pointed by |tx_secret|
 * and |tx_iv|.  Create new AEAD cipher context object with new
 * encryption key and initialize |tx_aead_ctx| with it.  All given
 * buffers have the enough capacity to store secret, key and IV.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_update_key)(
    ngtcp2_conn *conn, uint8_t *rx_secret, uint8_t *tx_secret,
    ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_iv,
    ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_iv,
    const uint8_t *current_rx_secret, const uint8_t *current_tx_secret,
    size_t secretlen, void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_path_validation` is a callback function which tells
 * the application the outcome of path validation.  |path| is the path
 * that was validated.  If |res| is
 * :enum:`ngtcp2_path_validation_result.NGTCP2_PATH_VALIDATION_RESULT_SUCCESS`,
 * the path validation succeeded.  If |res| is
 * :enum:`ngtcp2_path_validation_result.NGTCP2_PATH_VALIDATION_RESULT_FAILURE`,
 * the path validation failed.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_path_validation)(ngtcp2_conn *conn,
                                      const ngtcp2_path *path,
                                      ngtcp2_path_validation_result res,
                                      void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_select_preferred_addr` is a callback function which
 * asks a client application to choose server address from preferred
 * addresses |paddr| received from server.  An application should
 * write preferred address in |dest|.  If an application denies the
 * preferred addresses, just leave |dest| unmodified (or set dest->len
 * to 0) and return 0.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_select_preferred_addr)(ngtcp2_conn *conn,
                                            ngtcp2_addr *dest,
                                            const ngtcp2_preferred_addr *paddr,
                                            void *user_data);

/**
 * @enum
 *
 * :type:`ngtcp2_connection_id_status_type` defines a set of status
 * for Destination Connection ID.
 */
typedef enum ngtcp2_connection_id_status_type {
  /**
   * :enum:`NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE` indicates that
   * a local endpoint starts using new destination Connection ID.
   */
  NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE,
  /**
   * :enum:`NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE` indicates
   * that a local endpoint stops using a given destination Connection
   * ID.
   */
  NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE
} ngtcp2_connection_id_status_type;

/**
 * @functypedef
 *
 * :type:`ngtcp2_connection_id_status` is a callback function which is
 * called when the status of Connection ID changes.
 *
 * |token| is the associated stateless reset token and it is ``NULL``
 * if no token is present.
 *
 * |type| is the one of the value defined in
 * :type:`ngtcp2_connection_id_status_type`.  The new value might be
 * added in the future release.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_connection_id_status)(ngtcp2_conn *conn, int type,
                                           uint64_t seq, const ngtcp2_cid *cid,
                                           const uint8_t *token,
                                           void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_new_token` is a callback function which is
 * called when new token is received from server.
 *
 * |token| is the received token.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_recv_new_token)(ngtcp2_conn *conn, const ngtcp2_vec *token,
                                     void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_delete_crypto_aead_ctx` is a callback function which
 * must delete the native object pointed by |aead_ctx|->native_handle.
 */
typedef void (*ngtcp2_delete_crypto_aead_ctx)(ngtcp2_conn *conn,
                                              ngtcp2_crypto_aead_ctx *aead_ctx,
                                              void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_delete_crypto_cipher_ctx` is a callback function
 * which must delete the native object pointed by
 * |cipher_ctx|->native_handle.
 */
typedef void (*ngtcp2_delete_crypto_cipher_ctx)(
    ngtcp2_conn *conn, ngtcp2_crypto_cipher_ctx *cipher_ctx, void *user_data);

/**
 * @macrosection
 *
 * Datagram flags
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_DATAGRAM_FLAG_NONE` indicates no flag set.
 */
#define NGTCP2_DATAGRAM_FLAG_NONE 0x00

/**
 * @macro
 *
 * :macro:`NGTCP2_DATAGRAM_FLAG_EARLY` indicates that DATAGRAM frame
 * is received in 0RTT packet and the handshake has not completed yet,
 * which means that the data might be replayed.
 */
#define NGTCP2_DATAGRAM_FLAG_EARLY 0x01

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_datagram` is invoked when DATAGRAM frame is
 * received.  |flags| is bitwise-OR of zero or more of
 * NGTCP2_DATAGRAM_FLAG_*.  See :macro:`NGTCP2_DATAGRAM_FLAG_NONE`.
 *
 * If :macro:`NGTCP2_DATAGRAM_FLAG_EARLY` is set in |flags|, it
 * indicates that DATAGRAM frame was received in 0RTT packet and a
 * handshake has not completed yet.
 *
 * The callback function must return 0 if it succeeds, or
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library return
 * immediately.
 */
typedef int (*ngtcp2_recv_datagram)(ngtcp2_conn *conn, uint32_t flags,
                                    const uint8_t *data, size_t datalen,
                                    void *user_data);

/**
 * @struct
 *
 * :type:`ngtcp2_callbacks` holds a set of callback functions.
 */
typedef struct ngtcp2_callbacks {
  /**
   * :member:`client_initial` is a callback function which is invoked
   * when client asks TLS stack to produce first TLS cryptographic
   * handshake message.  This callback function must be specified.
   */
  ngtcp2_client_initial client_initial;
  /**
   * :member:`recv_client_initial` is a callback function which is
   * invoked when a server receives the first packet from client.
   * This callback function must be specified.
   */
  ngtcp2_recv_client_initial recv_client_initial;
  /**
   * :member:`recv_crypto_data` is a callback function which is
   * invoked when cryptographic data (CRYPTO frame, in other words,
   * TLS message) is received.  This callback function must be
   * specified.
   */
  ngtcp2_recv_crypto_data recv_crypto_data;
  /**
   * :member:`handshake_completed` is a callback function which is
   * invoked when QUIC cryptographic handshake has completed.  This
   * callback function is optional.
   */
  ngtcp2_handshake_completed handshake_completed;
  /**
   * :member:`recv_version_negotiation` is a callback function which
   * is invoked when Version Negotiation packet is received by a
   * client.  This callback function is optional.
   */
  ngtcp2_recv_version_negotiation recv_version_negotiation;
  /**
   * :member:`encrypt` is a callback function which is invoked to
   * encrypt a QUIC packet.  This callback function must be specified.
   */
  ngtcp2_encrypt encrypt;
  /**
   * :member:`decrypt` is a callback function which is invoked to
   * decrypt a QUIC packet.  This callback function must be specified.
   */
  ngtcp2_decrypt decrypt;
  /**
   * :member:`hp_mask` is a callback function which is invoked to get
   * a mask to encrypt or decrypt packet header.  This callback
   * function must be specified.
   */
  ngtcp2_hp_mask hp_mask;
  /**
   * :member:`recv_stream_data` is a callback function which is
   * invoked when STREAM data, which includes application data, is
   * received.  This callback function is optional.
   */
  ngtcp2_recv_stream_data recv_stream_data;
  /**
   * :member:`acked_crypto_offset` is a callback function which is
   * invoked when CRYPTO data is acknowledged by a remote endpoint.
   * It tells an application the largest offset of acknowledged CRYPTO
   * data without a gap so that the application can free memory for
   * the data.  This callback function is optional.
   */
  ngtcp2_acked_crypto_offset acked_crypto_offset;
  /**
   * :member:`acked_stream_data_offset` is a callback function which
   * is invoked when STREAM data, which includes application data, is
   * acknowledged by a remote endpoint.  It tells an application the
   * largest offset of acknowledged STREAM data without a gap so that
   * application can free memory for the data.  This callback function
   * is optional.
   */
  ngtcp2_acked_stream_data_offset acked_stream_data_offset;
  /**
   * :member:`stream_open` is a callback function which is invoked
   * when new remote stream is opened by a remote endpoint.  This
   * callback function is optional.
   */
  ngtcp2_stream_open stream_open;
  /**
   * :member:`stream_close` is a callback function which is invoked
   * when a stream is closed.  This callback function is optional.
   */
  ngtcp2_stream_close stream_close;
  /**
   * :member:`recv_stateless_reset` is a callback function which is
   * invoked when Stateless Reset packet is received.  This callback
   * function is optional.
   */
  ngtcp2_recv_stateless_reset recv_stateless_reset;
  /**
   * :member:`recv_retry` is a callback function which is invoked when
   * a client receives Retry packet.  For client, this callback
   * function must be specified.  Server never receive Retry packet.
   */
  ngtcp2_recv_retry recv_retry;
  /**
   * :member:`extend_max_local_streams_bidi` is a callback function
   * which is invoked when the number of bidirectional stream which a
   * local endpoint can open is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_streams extend_max_local_streams_bidi;
  /**
   * :member:`extend_max_local_streams_uni` is a callback function
   * which is invoked when the number of unidirectional stream which a
   * local endpoint can open is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_streams extend_max_local_streams_uni;
  /**
   * :member:`rand` is a callback function which is invoked when the
   * library needs unpredictable sequence of random data.  This
   * callback function must be specified.
   */
  ngtcp2_rand rand;
  /**
   * :member:`get_new_connection_id` is a callback function which is
   * invoked when the library needs new connection ID.  This callback
   * function must be specified.
   */
  ngtcp2_get_new_connection_id get_new_connection_id;
  /**
   * :member:`remove_connection_id` is a callback function which
   * notifies an application that connection ID is no longer used by a
   * remote endpoint.  This callback function is optional.
   */
  ngtcp2_remove_connection_id remove_connection_id;
  /**
   * :member:`update_key` is a callback function which is invoked when
   * the library tells an application that it must update keying
   * materials and install new keys.  This function must be specified.
   */
  ngtcp2_update_key update_key;
  /**
   * :member:`path_validation` is a callback function which is invoked
   * when path validation completed.  This function is optional.
   */
  ngtcp2_path_validation path_validation;
  /**
   * :member:`select_preferred_addr` is a callback function which is
   * invoked when the library asks a client to select preferred
   * address presented by a server.  This function is optional.
   */
  ngtcp2_select_preferred_addr select_preferred_addr;
  /**
   * :member:`stream_reset` is a callback function which is invoked
   * when a stream is reset by a remote endpoint.  This callback
   * function is optional.
   */
  ngtcp2_stream_reset stream_reset;
  /**
   * :member:`extend_max_remote_streams_bidi` is a callback function
   * which is invoked when the number of bidirectional streams which a
   * remote endpoint can open is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_streams extend_max_remote_streams_bidi;
  /**
   * :member:`extend_max_remote_streams_uni` is a callback function
   * which is invoked when the number of unidirectional streams which
   * a remote endpoint can open is increased.  This callback function
   * is optional.
   */
  ngtcp2_extend_max_streams extend_max_remote_streams_uni;
  /**
   * :member:`extend_max_stream_data` is callback function which is
   * invoked when the maximum offset of STREAM data that a local
   * endpoint can send is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_stream_data extend_max_stream_data;
  /**
   * :member:`dcid_status` is a callback function which is invoked
   * when the new destination Connection ID is activated or the
   * activated destination Connection ID is now deactivated.
   */
  ngtcp2_connection_id_status dcid_status;
  /**
   * :member:`handshake_confirmed` is a callback function which is
   * invoked when both endpoints agree that handshake has finished.
   * This field is ignored by server because handshake_completed
   * indicates the handshake confirmation for server.
   */
  ngtcp2_handshake_confirmed handshake_confirmed;
  /**
   * :member:`recv_new_token` is a callback function which is invoked
   * when new token is received from server.  This field is ignored by
   * server.
   */
  ngtcp2_recv_new_token recv_new_token;
  /**
   * :member:`delete_crypto_aead_ctx` is a callback function which
   * deletes a given AEAD cipher context object.
   */
  ngtcp2_delete_crypto_aead_ctx delete_crypto_aead_ctx;
  /**
   * :member:`delete_crypto_cipher_ctx` is a callback function which
   * deletes a given cipher context object.
   */
  ngtcp2_delete_crypto_cipher_ctx delete_crypto_cipher_ctx;
  /**
   * :member:`recv_datagram` is a callback function which is invoked
   * when DATAGRAM frame is received.
   */
  ngtcp2_recv_datagram recv_datagram;
} ngtcp2_callbacks;

/**
 * @function
 *
 * `ngtcp2_pkt_write_connection_close` writes Initial packet
 * containing CONNECTION_CLOSE frame with the given |error_code| to
 * the buffer pointed by |dest| of length |destlen|.  All encryption
 * parameters are for Initial packet encryption.  The packet number is
 * always 0.
 *
 * The primary use case of this function is for server to send
 * CONNECTION_CLOSE frame in Initial packet to close connection
 * without committing the state when validating Retry token fails.
 *
 * This function returns the number of bytes written if it succeeds,
 * or one of the following negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     Callback function failed.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_write_connection_close(
    uint8_t *dest, size_t destlen, uint32_t version, const ngtcp2_cid *dcid,
    const ngtcp2_cid *scid, uint64_t error_code, ngtcp2_encrypt encrypt,
    const ngtcp2_crypto_aead *aead, const ngtcp2_crypto_aead_ctx *aead_ctx,
    const uint8_t *iv, ngtcp2_hp_mask hp_mask, const ngtcp2_crypto_cipher *hp,
    const ngtcp2_crypto_cipher_ctx *hp_ctx);

/**
 * @function
 *
 * `ngtcp2_pkt_write_retry` writes Retry packet in the buffer pointed
 * by |dest| whose length is |destlen|.  |odcid| specifies Original
 * Destination Connection ID.  |token| specifies Retry Token, and
 * |tokenlen| specifies its length.  |aead| must be AEAD_AES_128_GCM.
 * |aead_ctx| must be initialized with :macro:`NGTCP2_RETRY_KEY` as an
 * encryption key.
 *
 * This function returns the number of bytes written to the buffer, or
 * one of the following negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     Callback function failed.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_write_retry(
    uint8_t *dest, size_t destlen, uint32_t version, const ngtcp2_cid *dcid,
    const ngtcp2_cid *scid, const ngtcp2_cid *odcid, const uint8_t *token,
    size_t tokenlen, ngtcp2_encrypt encrypt, const ngtcp2_crypto_aead *aead,
    const ngtcp2_crypto_aead_ctx *aead_ctx);

/**
 * @function
 *
 * `ngtcp2_accept` is used by server implementation, and decides
 * whether packet |pkt| of length |pktlen| is acceptable for initial
 * packet from client.
 *
 * If it is acceptable, it returns 0.  If it is not acceptable, and
 * Version Negotiation packet is required to send, it returns 1.
 * Otherwise, it returns -1.
 *
 * If |dest| is not ``NULL``, and the return value is 0 or 1, the
 * decoded packet header is stored to the object pointed by |dest|.
 */
NGTCP2_EXTERN int ngtcp2_accept(ngtcp2_pkt_hd *dest, const uint8_t *pkt,
                                size_t pktlen);

/**
 * @function
 *
 * `ngtcp2_conn_client_new` creates new :type:`ngtcp2_conn`, and
 * initializes it as client.  |dcid| is randomized destination
 * connection ID.  |scid| is source connection ID.  |version| is a
 * QUIC version to use.  |path| is the network path where this QUIC
 * connection is being established and must not be ``NULL``.
 * |callbacks|, |settings|, and |params| must not be ``NULL``, and the
 * function make a copy of each of them.  |params| is local QUIC
 * transport parameters and sent to a remote endpoint during
 * handshake.  |user_data| is the arbitrary pointer which is passed to
 * the user-defined callback functions.  If |mem| is ``NULL``, the
 * memory allocator returned by `ngtcp2_mem_default()` is used.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_client_new(ngtcp2_conn **pconn, const ngtcp2_cid *dcid,
                       const ngtcp2_cid *scid, const ngtcp2_path *path,
                       uint32_t version, const ngtcp2_callbacks *callbacks,
                       const ngtcp2_settings *settings,
                       const ngtcp2_transport_params *params,
                       const ngtcp2_mem *mem, void *user_data);

/**
 * @function
 *
 * `ngtcp2_conn_server_new` creates new :type:`ngtcp2_conn`, and
 * initializes it as server.  |dcid| is a destination connection ID.
 * |scid| is a source connection ID.  |path| is the network path where
 * this QUIC connection is being established and must not be ``NULL`.
 * |version| is a QUIC version to use.  |callbacks|, |settings|, and
 * |params| must not be ``NULL``, and the function make a copy of each
 * of them.  |params| is local QUIC transport parameters and sent to a
 * remote endpoint during handshake.  |user_data| is the arbitrary
 * pointer which is passed to the user-defined callback functions.  If
 * |mem| is ``NULL``, the memory allocator returned by
 * `ngtcp2_mem_default()` is used.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_server_new(ngtcp2_conn **pconn, const ngtcp2_cid *dcid,
                       const ngtcp2_cid *scid, const ngtcp2_path *path,
                       uint32_t version, const ngtcp2_callbacks *callbacks,
                       const ngtcp2_settings *settings,
                       const ngtcp2_transport_params *params,
                       const ngtcp2_mem *mem, void *user_data);

/**
 * @function
 *
 * `ngtcp2_conn_del` frees resources allocated for |conn|.  It also
 * frees memory pointed by |conn|.
 */
NGTCP2_EXTERN void ngtcp2_conn_del(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_read_pkt` decrypts QUIC packet given in |pkt| of
 * length |pktlen| and processes it.  |path| is the network path the
 * packet is delivered and must not be ``NULL``.  |pi| is packet
 * metadata and must not be ``NULL``. This function performs QUIC
 * handshake as well.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * This function returns 0 if it succeeds, or negative error codes.
 * In general, if the error code which satisfies
 * `ngtcp2_err_is_fatal(err) <ngtcp2_err_is_fatal>` != 0 is returned,
 * the application should just close the connection by calling
 * `ngtcp2_conn_write_connection_close` or just delete the QUIC
 * connection using `ngtcp2_conn_del`.  It is undefined to call the
 * other library functions.  If :macro:`NGTCP2_ERR_RETRY` is returned,
 * application must be a server and it must perform address validation
 * by sending Retry packet and close the connection.  If
 * :macro:`NGTCP2_ERR_DROP_CONN` is returned, server application must
 * drop the connection silently (without sending any CONNECTION_CLOSE
 * frame) and discard connection state.
 */
NGTCP2_EXTERN int ngtcp2_conn_read_pkt(ngtcp2_conn *conn,
                                       const ngtcp2_path *path,
                                       const ngtcp2_pkt_info *pi,
                                       const uint8_t *pkt, size_t pktlen,
                                       ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_write_pkt` is equivalent to calling
 * `ngtcp2_conn_writev_stream` without specifying stream data and
 * :macro:`NGTCP2_WRITE_STREAM_FLAG_NONE` as flags.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_pkt(ngtcp2_conn *conn,
                                                 ngtcp2_path *path,
                                                 ngtcp2_pkt_info *pi,
                                                 uint8_t *dest, size_t destlen,
                                                 ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_handshake_completed` tells |conn| that the QUIC
 * handshake has completed.
 */
NGTCP2_EXTERN void ngtcp2_conn_handshake_completed(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_handshake_completed` returns nonzero if handshake
 * has completed.
 */
NGTCP2_EXTERN int ngtcp2_conn_get_handshake_completed(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_install_initial_key` installs packet protection keying
 * materials for Initial packets.  |rx_aead_ctx| is AEAD cipher
 * context object and must be initialized with decryption key, IV
 * |rx_iv| of length |rx_ivlen|, and packet header protection cipher
 * context object |rx_hp_ctx| to decrypt incoming Initial packets.
 * Similarly, |tx_aead_ctx|, |tx_iv| and |tx_hp_ctx| are for
 * encrypting outgoing packets and are the same length with the
 * decryption counterpart .  If they have already been set, they are
 * overwritten.
 *
 * If this function succeeds, |conn| takes ownership of |rx_aead_ctx|,
 * |rx_hp_ctx|, |tx_aead_ctx|, and |tx_hp_ctx|.
 * :type:`ngtcp2_delete_crypto_aead_ctx` and
 * :type:`ngtcp2_delete_crypto_cipher_ctx` will be called to delete
 * these objects when they are no longer used.  If this function
 * fails, the caller is responsible to delete them.
 *
 * After receiving Retry packet, the DCID most likely changes.  In
 * that case, client application must generate these keying materials
 * again based on new DCID and install them again.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_initial_key(
    ngtcp2_conn *conn, const ngtcp2_crypto_aead_ctx *rx_aead_ctx,
    const uint8_t *rx_iv, const ngtcp2_crypto_cipher_ctx *rx_hp_ctx,
    const ngtcp2_crypto_aead_ctx *tx_aead_ctx, const uint8_t *tx_iv,
    const ngtcp2_crypto_cipher_ctx *tx_hp_ctx, size_t ivlen);

/**
 * @function
 *
 * `ngtcp2_conn_install_rx_handshake_key` installs packet protection
 * keying materials for decrypting incoming Handshake packets.
 * |aead_ctx| is AEAD cipher context object which must be initialized
 * with decryption key, IV |iv| of length |ivlen|, and packet header
 * protection cipher context object |hp_ctx| to decrypt incoming
 * Handshake packets.
 *
 * If this function succeeds, |conn| takes ownership of |aead_ctx|,
 * and |hp_ctx|.  :type:`ngtcp2_delete_crypto_aead_ctx` and
 * :type:`ngtcp2_delete_crypto_cipher_ctx` will be called to delete
 * these objects when they are no longer used.  If this function
 * fails, the caller is responsible to delete them.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_rx_handshake_key(
    ngtcp2_conn *conn, const ngtcp2_crypto_aead_ctx *aead_ctx,
    const uint8_t *iv, size_t ivlen, const ngtcp2_crypto_cipher_ctx *hp_ctx);

/**
 * @function
 *
 * `ngtcp2_conn_install_tx_handshake_key` installs packet protection
 * keying materials for encrypting outgoing Handshake packets.
 * |aead_ctx| is AEAD cipher context object which must be initialized
 * with encryption key, IV |iv| of length |ivlen|, and packet header
 * protection cipher context object |hp_ctx| to encrypt outgoing
 * Handshake packets.
 *
 * If this function succeeds, |conn| takes ownership of |aead_ctx| and
 * |hp_ctx|.  :type:`ngtcp2_delete_crypto_aead_ctx` and
 * :type:`ngtcp2_delete_crypto_cipher_ctx` will be called to delete
 * these objects when they are no longer used.  If this function
 * fails, the caller is responsible to delete them.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_tx_handshake_key(
    ngtcp2_conn *conn, const ngtcp2_crypto_aead_ctx *aead_ctx,
    const uint8_t *iv, size_t ivlen, const ngtcp2_crypto_cipher_ctx *hp_ctx);

/**
 * @function
 *
 * `ngtcp2_conn_install_early_key` installs packet protection AEAD
 * cipher context object |aead_ctx|, IV |iv| of length |ivlen|, and
 * packet header protection cipher context object |hp_ctx| to encrypt
 * (for client) or decrypt (for server) 0RTT packets.
 *
 * If this function succeeds, |conn| takes ownership of |aead_ctx| and
 * |hp_ctx|.  :type:`ngtcp2_delete_crypto_aead_ctx` and
 * :type:`ngtcp2_delete_crypto_cipher_ctx` will be called to delete
 * these objects when they are no longer used.  If this function
 * fails, the caller is responsible to delete them.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_early_key(
    ngtcp2_conn *conn, const ngtcp2_crypto_aead_ctx *aead_ctx,
    const uint8_t *iv, size_t ivlen, const ngtcp2_crypto_cipher_ctx *hp_ctx);

/**
 * @function
 *
 * `ngtcp2_conn_install_rx_key` installs packet protection keying
 * materials for decrypting Short packets.  |secret| of length
 * |secretlen| is the decryption secret which is used to derive keying
 * materials passed to this function.  |aead_ctx| is AEAD cipher
 * context object which must be initialized with decryption key, IV
 * |iv| of length |ivlen|, and packet header protection cipher context
 * object |hp_ctx| to decrypt incoming Short packets.
 *
 * If this function succeeds, |conn| takes ownership of |aead_ctx| and
 * |hp_ctx|.  :type:`ngtcp2_delete_crypto_aead_ctx` and
 * :type:`ngtcp2_delete_crypto_cipher_ctx` will be called to delete
 * these objects when they are no longer used.  If this function
 * fails, the caller is responsible to delete them.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_rx_key(
    ngtcp2_conn *conn, const uint8_t *secret, size_t secretlen,
    const ngtcp2_crypto_aead_ctx *aead_ctx, const uint8_t *iv, size_t ivlen,
    const ngtcp2_crypto_cipher_ctx *hp_ctx);

/**
 * @function
 *
 * `ngtcp2_conn_install_tx_key` installs packet protection keying
 * materials for encrypting Short packets.  |secret| of length
 * |secretlen| is the encryption secret which is used to derive keying
 * materials passed to this function.  |aead_ctx| is AEAD cipher
 * context object which must be initialized with encryption key, IV
 * |iv| of length |ivlen|, and packet header protection cipher context
 * object |hp_ctx| to encrypt outgoing Short packets.
 *
 * If this function succeeds, |conn| takes ownership of |aead_ctx| and
 * |hp_ctx|.  :type:`ngtcp2_delete_crypto_aead_ctx` and
 * :type:`ngtcp2_delete_crypto_cipher_ctx` will be called to delete
 * these objects when they are no longer used.  If this function
 * fails, the caller is responsible to delete them.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_tx_key(
    ngtcp2_conn *conn, const uint8_t *secret, size_t secretlen,
    const ngtcp2_crypto_aead_ctx *aead_ctx, const uint8_t *iv, size_t ivlen,
    const ngtcp2_crypto_cipher_ctx *hp_ctx);

/**
 * @function
 *
 * `ngtcp2_conn_initiate_key_update` initiates the key update.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_INVALID_STATE`
 *     The previous key update has not been confirmed yet; or key
 *     update is too frequent; or new keys are not available yet.
 */
NGTCP2_EXTERN int ngtcp2_conn_initiate_key_update(ngtcp2_conn *conn,
                                                  ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_set_tls_error` sets the TLS related error |liberr| in
 * |conn|.  |liberr| must be one of ngtcp2 library error codes (which
 * is defined as NGTCP2_ERR_* macro, such as
 * :macro:`NGTCP2_ERR_TLS_DECRYPT`).  In general, error code should be
 * propagated via return value, but sometimes ngtcp2 API is called
 * inside callback function of TLS stack and it does not allow to
 * return ngtcp2 error code directly.  In this case, implementation
 * can set the error code (e.g.,
 * :macro:`NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM`) using this function.
 */
NGTCP2_EXTERN void ngtcp2_conn_set_tls_error(ngtcp2_conn *conn, int liberr);

/**
 * @function
 *
 * `ngtcp2_conn_get_tls_error` returns the value set by
 * `ngtcp2_conn_set_tls_error`.  If no value is set, this function
 * returns 0.
 */
NGTCP2_EXTERN int ngtcp2_conn_get_tls_error(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_expiry` returns the next expiry time.
 *
 * Call `ngtcp2_conn_handle_expiry()` and `ngtcp2_conn_write_pkt` (or
 * `ngtcp2_conn_writev_stream`) if expiry time is passed.
 */
NGTCP2_EXTERN ngtcp2_tstamp ngtcp2_conn_get_expiry(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_handle_expiry` handles expired timer.  It does nothing
 * if timer is not expired.
 */
NGTCP2_EXTERN int ngtcp2_conn_handle_expiry(ngtcp2_conn *conn,
                                            ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_get_idle_expiry` returns the time when a connection
 * should be closed if it continues to be idle.  If idle timeout is
 * disabled, this function returns ``UINT64_MAX``.
 */
NGTCP2_EXTERN ngtcp2_tstamp ngtcp2_conn_get_idle_expiry(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_pto` returns Probe Timeout (PTO).
 */
NGTCP2_EXTERN ngtcp2_duration ngtcp2_conn_get_pto(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_set_remote_transport_params` sets transport parameter
 * |params| to |conn|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_PROTO`
 *     If |conn| is server, and negotiated_version field is not the
 *     same as the used version.
 */
NGTCP2_EXTERN int
ngtcp2_conn_set_remote_transport_params(ngtcp2_conn *conn,
                                        const ngtcp2_transport_params *params);

/**
 * @function
 *
 * `ngtcp2_conn_get_remote_transport_params` fills settings values in
 * |params|.  original_connection_id and
 * original_connection_id_present are always zero filled.
 */
NGTCP2_EXTERN void
ngtcp2_conn_get_remote_transport_params(ngtcp2_conn *conn,
                                        ngtcp2_transport_params *params);

/**
 * @function
 *
 * `ngtcp2_conn_set_early_remote_transport_params` sets |params| as
 * transport parameter previously received from a server.  The
 * parameters are used to send 0-RTT data.  QUIC requires that client
 * application should remember transport parameter as well as session
 * ticket.
 *
 * At least following fields must be set:
 *
 * * initial_max_stream_id_bidi
 * * initial_max_stream_id_uni
 * * initial_max_stream_data_bidi_local
 * * initial_max_stream_data_bidi_remote
 * * initial_max_stream_data_uni
 * * initial_max_data
 */
NGTCP2_EXTERN void ngtcp2_conn_set_early_remote_transport_params(
    ngtcp2_conn *conn, const ngtcp2_transport_params *params);

/**
 * @function
 *
 * `ngtcp2_conn_set_local_transport_params` sets the local transport
 * parameters |params|.  This function can only be called by server.
 * Although the local transport parameters are passed to
 * `ngtcp2_conn_server_new`, server might want to update them after
 * ALPN is chosen.  In that case, server can update the transport
 * parameter with this function.  Server must call this function
 * before calling `ngtcp2_conn_install_tx_handshake_key`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_INVALID_STATE`
 *     `ngtcp2_conn_install_tx_handshake_key` has been called.
 */
NGTCP2_EXTERN int
ngtcp2_conn_set_local_transport_params(ngtcp2_conn *conn,
                                       const ngtcp2_transport_params *params);

/**
 * @function
 *
 * `ngtcp2_conn_get_local_transport_params` fills settings values in
 * |params|.
 */
NGTCP2_EXTERN void
ngtcp2_conn_get_local_transport_params(ngtcp2_conn *conn,
                                       ngtcp2_transport_params *params);

/**
 * @function
 *
 * `ngtcp2_conn_open_bidi_stream` opens new bidirectional stream.  The
 * |stream_user_data| is the user data specific to the stream.  The
 * open stream ID is stored in |*pstream_id|.
 *
 * Application can call this function before handshake completes.  For
 * 0RTT packet, application can call this function after calling
 * `ngtcp2_conn_set_early_remote_transport_params`.  For 1RTT packet,
 * application can call this function after calling
 * `ngtcp2_conn_set_remote_transport_params` and
 * `ngtcp2_conn_install_tx_key`.  If ngtcp2 crypto support library is
 * used, application can call this function after calling
 * `ngtcp2_crypto_derive_and_install_tx_key` for 1RTT packet.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_STREAM_ID_BLOCKED`
 *     The remote peer does not allow |stream_id| yet.
 */
NGTCP2_EXTERN int ngtcp2_conn_open_bidi_stream(ngtcp2_conn *conn,
                                               int64_t *pstream_id,
                                               void *stream_user_data);

/**
 * @function
 *
 * `ngtcp2_conn_open_uni_stream` opens new unidirectional stream.  The
 * |stream_user_data| is the user data specific to the stream.  The
 * open stream ID is stored in |*pstream_id|.
 *
 * Application can call this function before handshake completes.  For
 * 0RTT packet, application can call this function after calling
 * `ngtcp2_conn_set_early_remote_transport_params`.  For 1RTT packet,
 * application can call this function after calling
 * `ngtcp2_conn_set_remote_transport_params` and
 * `ngtcp2_conn_install_tx_key`.  If ngtcp2 crypto support library is
 * used, application can call this function after calling
 * `ngtcp2_crypto_derive_and_install_tx_key` for 1RTT packet.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_STREAM_ID_BLOCKED`
 *     The remote peer does not allow |stream_id| yet.
 */
NGTCP2_EXTERN int ngtcp2_conn_open_uni_stream(ngtcp2_conn *conn,
                                              int64_t *pstream_id,
                                              void *stream_user_data);

/**
 * @function
 *
 * `ngtcp2_conn_shutdown_stream` closes stream denoted by |stream_id|
 * abruptly.  |app_error_code| is one of application error codes, and
 * indicates the reason of shutdown.  Successful call of this function
 * does not immediately erase the state of the stream.  The actual
 * deletion is done when the remote endpoint sends acknowledgement.
 * Calling this function is equivalent to call
 * `ngtcp2_conn_shutdown_stream_read`, and
 * `ngtcp2_conn_shutdown_stream_write` sequentially.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream does not exist
 */
NGTCP2_EXTERN int ngtcp2_conn_shutdown_stream(ngtcp2_conn *conn,
                                              int64_t stream_id,
                                              uint64_t app_error_code);

/**
 * @function
 *
 * `ngtcp2_conn_shutdown_stream_write` closes write-side of stream
 * denoted by |stream_id| abruptly.  |app_error_code| is one of
 * application error codes, and indicates the reason of shutdown.  If
 * this function succeeds, no application data is sent to the remote
 * endpoint.  It discards all data which has not been acknowledged
 * yet.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream does not exist
 */
NGTCP2_EXTERN int ngtcp2_conn_shutdown_stream_write(ngtcp2_conn *conn,
                                                    int64_t stream_id,
                                                    uint64_t app_error_code);

/**
 * @function
 *
 * `ngtcp2_conn_shutdown_stream_read` closes read-side of stream
 * denoted by |stream_id| abruptly.  |app_error_code| is one of
 * application error codes, and indicates the reason of shutdown.  If
 * this function succeeds, no application data is forwarded to an
 * application layer.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream does not exist
 */
NGTCP2_EXTERN int ngtcp2_conn_shutdown_stream_read(ngtcp2_conn *conn,
                                                   int64_t stream_id,
                                                   uint64_t app_error_code);

/**
 * @macrosection
 *
 * Write stream data flags
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_WRITE_STREAM_FLAG_NONE` indicates no flag set.
 */
#define NGTCP2_WRITE_STREAM_FLAG_NONE 0x00

/**
 * @macro
 *
 * :macro:`NGTCP2_WRITE_STREAM_FLAG_MORE` indicates that more data may
 * come and should be coalesced into the same packet if possible.
 */
#define NGTCP2_WRITE_STREAM_FLAG_MORE 0x01

/**
 * @macro
 *
 * :macro:`NGTCP2_WRITE_STREAM_FLAG_FIN` indicates that the passed
 * data is the final part of a stream.
 */
#define NGTCP2_WRITE_STREAM_FLAG_FIN 0x02

/**
 * @function
 *
 * `ngtcp2_conn_write_stream` is just like
 * `ngtcp2_conn_writev_stream`.  The only difference is that it
 * conveniently accepts a single buffer.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_stream(
    ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
    size_t destlen, ngtcp2_ssize *pdatalen, uint32_t flags, int64_t stream_id,
    const uint8_t *data, size_t datalen, ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_writev_stream` writes a packet containing stream data
 * of stream denoted by |stream_id|.  The buffer of the packet is
 * pointed by |dest| of length |destlen|.  This function performs QUIC
 * handshake as well.
 *
 * Specifying -1 to |stream_id| means no new stream data to send.
 *
 * If |path| is not ``NULL``, this function stores the network path
 * with which the packet should be sent.  Each addr field must point
 * to the buffer which should be at least ``sizeof(struct
 * sockaddr_storage)`` bytes long.  The assignment might not be done
 * if nothing is written to |dest|.
 *
 * If |pi| is not ``NULL``, this function stores packet metadata in it
 * if it succeeds.  The metadata includes ECN markings.  When calling
 * this function again after it returns
 * :macro:`NGTCP2_ERR_WRITE_MORE`, caller must pass the same |pi| to
 * this function.
 *
 * If the all given data is encoded as STREAM frame in |dest|, and if
 * |flags| & :macro:`NGTCP2_WRITE_STREAM_FLAG_FIN` is nonzero, fin
 * flag is set to outgoing STREAM frame.  Otherwise, fin flag in
 * STREAM frame is not set.
 *
 * This packet may contain frames other than STREAM frame.  The packet
 * might not contain STREAM frame if other frames occupy the packet.
 * In that case, |*pdatalen| would be -1 if |pdatalen| is not
 * ``NULL``.
 *
 * If |flags| & :macro:`NGTCP2_WRITE_STREAM_FLAG_FIN` is nonzero, and
 * 0 length STREAM frame is successfully serialized, |*pdatalen| would
 * be 0.
 *
 * The number of data encoded in STREAM frame is stored in |*pdatalen|
 * if it is not ``NULL``.  The caller must keep the portion of data
 * covered by |*pdatalen| bytes in tact until
 * :type:`ngtcp2_acked_stream_data_offset` indicates that they are
 * acknowledged by a remote endpoint or the stream is closed.
 *
 * If |flags| equals to :macro:`NGTCP2_WRITE_STREAM_FLAG_NONE`, this
 * function produces a single payload of UDP packet.  If the given
 * stream data is small (e.g., few bytes), the packet might be
 * severely under filled.  Too many small packet might increase
 * overall packet processing costs.  Unless there are retransmissions,
 * by default, application can only send 1 STREAM frame in one QUIC
 * packet.  In order to include more than 1 STREAM frame in one QUIC
 * packet, specify :macro:`NGTCP2_WRITE_STREAM_FLAG_MORE` in |flags|.
 * This is analogous to ``MSG_MORE`` flag in ``send(2)``.  If the
 * :macro:`NGTCP2_WRITE_STREAM_FLAG_MORE` is used, there are 4
 * outcomes:
 *
 * - The function returns the written length of packet just like
 *   without :macro:`NGTCP2_WRITE_STREAM_FLAG_MORE`.  This is because
 *   packet is nearly full and the library decided to make a complete
 *   packet.  |*pdatalen| might be -1 or >= 0.
 *
 * - The function returns :macro:`NGTCP2_ERR_WRITE_MORE`.  In this
 *   case, |*pdatalen| >= 0 is asserted.  This indicates that
 *   application can call this function with different stream data (or
 *   `ngtcp2_conn_writev_datagram` if it has data to send in
 *   unreliable datagram) to pack them into the same packet.
 *   Application has to specify the same |conn|, |path|, |pi|, |dest|,
 *   |destlen|, and |ts| parameters, otherwise the behaviour is
 *   undefined.  The application can change |flags|.
 *
 * - The function returns :macro:`NGTCP2_ERR_STREAM_DATA_BLOCKED` which
 *   indicates that stream is blocked because of flow control.
 *
 * - The other error might be returned just like without
 *   :macro:`NGTCP2_WRITE_STREAM_FLAG_MORE`.
 *
 * When application sees :macro:`NGTCP2_ERR_WRITE_MORE`, it must not
 * call other ngtcp2 API functions (application can still call
 * `ngtcp2_conn_write_connection_close` or
 * `ngtcp2_conn_write_application_close` to handle error from this
 * function).  Just keep calling `ngtcp2_conn_writev_stream`,
 * `ngtcp2_conn_write_pkt`, or `ngtcp2_conn_writev_datagram` until it
 * returns a positive number (which indicates a complete packet is
 * ready).
 *
 * This function returns 0 if it cannot write any frame because buffer
 * is too small, or packet is congestion limited.  Application should
 * keep reading and wait for congestion window to grow.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream does not exist
 * :macro:`NGTCP2_ERR_STREAM_SHUT_WR`
 *     Stream is half closed (local); or stream is being reset.
 * :macro:`NGTCP2_ERR_PKT_NUM_EXHAUSTED`
 *     Packet number is exhausted, and cannot send any more packet.
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     User callback failed
 * :macro:`NGTCP2_ERR_STREAM_DATA_BLOCKED`
 *     Stream is blocked because of flow control.
 * :macro:`NGTCP2_ERR_WRITE_MORE`
 *     (Only when :macro:`NGTCP2_WRITE_STREAM_FLAG_MORE` is specified)
 *     Application can call this function to pack more stream data
 *     into the same packet.  See above to know how it works.
 *
 * In general, if the error code which satisfies
 * `ngtcp2_err_is_fatal(err) <ngtcp2_err_is_fatal>` != 0 is returned,
 * the application should just close the connection by calling
 * `ngtcp2_conn_write_connection_close` or just delete the QUIC
 * connection using `ngtcp2_conn_del`.  It is undefined to call the
 * other library functions.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_writev_stream(
    ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
    size_t destlen, ngtcp2_ssize *pdatalen, uint32_t flags, int64_t stream_id,
    const ngtcp2_vec *datav, size_t datavcnt, ngtcp2_tstamp ts);

/**
 * @macrosection
 *
 * Write datagram flags
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_WRITE_DATAGRAM_FLAG_NONE` indicates no flag set.
 */
#define NGTCP2_WRITE_DATAGRAM_FLAG_NONE 0x00

/**
 * @macro
 *
 * :macro:`NGTCP2_WRITE_DATAGRAM_FLAG_MORE` indicates that more data
 * may come and should be coalesced into the same packet if possible.
 */
#define NGTCP2_WRITE_DATAGRAM_FLAG_MORE 0x01

/**
 * @function
 *
 * `ngtcp2_conn_writev_datagram` writes a packet containing unreliable
 * data in DATAGRAM frame.  The buffer of the packet is pointed by
 * |dest| of length |destlen|.  This function performs QUIC handshake
 * as well.
 *
 * For |path| and |pi| parameters, refer to
 * `ngtcp2_conn_writev_stream`.
 *
 * If the given data is written to the buffer, nonzero value is
 * assigned to |*paccepted| if it is not NULL.  The data in DATAGRAM
 * frame cannot be fragmented; writing partial data is not possible.
 *
 * This function might write other frames other than DATAGRAM, just
 * like `ngtcp2_conn_writev_stream`.
 *
 * If the function returns 0, it means that no more data cannot be
 * sent because of congestion control limit; or, data does not fit
 * into the provided buffer; or, a local endpoint, as a server, is
 * unable to send data because of its amplification limit.  In this
 * case, |*paccepted| is assigned zero if it is not NULL.
 *
 * If :macro:`NGTCP2_WRITE_DATAGRAM_FLAG_MORE` is set in |flags|,
 * there are 3 outcomes:
 *
 * - The function returns the written length of packet just like
 *   without :macro:`NGTCP2_WRITE_DATAGRAM_FLAG_MORE`.  This is
 *   because packet is nearly full and the library decided to make a
 *   complete packet.  |*paccepted| might be zero or nonzero.
 *
 * - The function returns :macro:`NGTCP2_ERR_WRITE_MORE`.  In this
 *   case, |*paccepted| != 0 is asserted.  This indicates that
 *   application can call this function with another unreliable data
 *   (or `ngtcp2_conn_writev_stream` if it has stream data to send) to
 *   pack them into the same packet.  Application has to specify the
 *   same |conn|, |path|, |pi|, |dest|, |destlen|, and |ts|
 *   parameters, otherwise the behaviour is undefined.  The
 *   application can change |flags|.
 *
 * - The other error might be returned just like without
 *   :macro:`NGTCP2_WRITE_DATAGRAM_FLAG_MORE`.
 *
 * When application sees :macro:`NGTCP2_ERR_WRITE_MORE`, it must not
 * call other ngtcp2 API functions (application can still call
 * `ngtcp2_conn_write_connection_close` or
 * `ngtcp2_conn_write_application_close` to handle error from this
 * function).  Just keep calling `ngtcp2_conn_writev_datagram`,
 * `ngtcp2_conn_writev_stream` or `ngtcp2_conn_write_pkt` until it
 * returns a positive number (which indicates a complete packet is
 * ready).
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_PKT_NUM_EXHAUSTED`
 *     Packet number is exhausted, and cannot send any more packet.
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     User callback failed
 * :macro:`NGTCP2_ERR_WRITE_MORE`
 *     (Only when :macro:`NGTCP2_WRITE_DATAGRAM_FLAG_MORE` is
 *     specified) Application can call this function to pack more data
 *     into the same packet.  See above to know how it works.
 * :macro:`NGTCP2_ERR_INVALID_STATE`
 *     A remote endpoint did not express the DATAGRAM frame support.
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     The provisional DATAGRAM frame size exceeds the maximum
 *     DATAGRAM frame size that a remote endpoint can receive.
 *
 * In general, if the error code which satisfies
 * `ngtcp2_err_is_fatal(err) <ngtcp2_err_is_fatal>` != 0 is returned,
 * the application should just close the connection by calling
 * `ngtcp2_conn_write_connection_close` or just delete the QUIC
 * connection using `ngtcp2_conn_del`.  It is undefined to call the
 * other library functions.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_writev_datagram(
    ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
    size_t destlen, int *paccepted, uint32_t flags, const ngtcp2_vec *datav,
    size_t datavcnt, ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_write_connection_close` writes a packet which contains
 * a CONNECTION_CLOSE frame (type 0x1c) in the buffer pointed by
 * |dest| whose capacity is |datalen|.
 *
 * If |path| is not ``NULL``, this function stores the network path
 * with which the packet should be sent.  Each addr field must point
 * to the buffer which should be at least ``sizeof(struct
 * sockaddr_storage)`` bytes long.  The assignment might not be done
 * if nothing is written to |dest|.
 *
 * If |pi| is not ``NULL``, this function stores packet metadata in it
 * if it succeeds.  The metadata includes ECN markings.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * At the moment, successful call to this function makes connection
 * close.  We may change this behaviour in the future to allow
 * graceful shutdown.
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small
 * :macro:`NGTCP2_ERR_INVALID_STATE`
 *     The current state does not allow sending CONNECTION_CLOSE.
 * :macro:`NGTCP2_ERR_PKT_NUM_EXHAUSTED`
 *     Packet number is exhausted, and cannot send any more packet.
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     User callback failed
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_connection_close(
    ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
    size_t destlen, uint64_t error_code, ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_write_application_close` writes a packet which
 * contains a CONNECTION_CLOSE frame (type 0x1d) in the buffer pointed
 * by |dest| whose capacity is |datalen|.
 *
 * If |path| is not ``NULL``, this function stores the network path
 * with which the packet should be sent.  Each addr field must point
 * to the buffer which should be at least ``sizeof(struct
 * sockaddr_storage)`` bytes long.  The assignment might not be done
 * if nothing is written to |dest|.
 *
 * If |pi| is not ``NULL``, this function stores packet metadata in it
 * if it succeeds.  The metadata includes ECN markings.
 *
 * If handshake has not been confirmed yet, CONNECTION_CLOSE (type
 * 0x1c) with error code :macro:`NGTCP2_APPLICATION_ERROR` is written
 * instead.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * At the moment, successful call to this function makes connection
 * close.  We may change this behaviour in the future to allow
 * graceful shutdown.
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :macro:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small
 * :macro:`NGTCP2_ERR_INVALID_STATE`
 *     The current state does not allow sending CONNECTION_CLOSE.
 * :macro:`NGTCP2_ERR_PKT_NUM_EXHAUSTED`
 *     Packet number is exhausted, and cannot send any more packet.
 * :macro:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     User callback failed
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_application_close(
    ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
    size_t destlen, uint64_t app_error_code, ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_is_in_closing_period` returns nonzero if |conn| is in
 * closing period.
 */
NGTCP2_EXTERN int ngtcp2_conn_is_in_closing_period(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_is_in_draining_period` returns nonzero if |conn| is in
 * draining period.
 */
NGTCP2_EXTERN int ngtcp2_conn_is_in_draining_period(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_extend_max_stream_offset` extends stream's max stream
 * data value by |datalen|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream was not found
 */
NGTCP2_EXTERN int ngtcp2_conn_extend_max_stream_offset(ngtcp2_conn *conn,
                                                       int64_t stream_id,
                                                       uint64_t datalen);

/**
 * @function
 *
 * `ngtcp2_conn_extend_max_offset` extends max data offset by
 * |datalen|.
 */
NGTCP2_EXTERN void ngtcp2_conn_extend_max_offset(ngtcp2_conn *conn,
                                                 uint64_t datalen);

/**
 * @function
 *
 * `ngtcp2_conn_extend_max_streams_bidi` extends the number of maximum
 * local bidirectional streams that a remote endpoint can open by |n|.
 *
 * The library does not increase maximum stream limit automatically.
 * The exception is when a stream is closed without
 * :type:`ngtcp2_stream_open` callback being called.  In this case,
 * stream limit is increased automatically.
 */
NGTCP2_EXTERN void ngtcp2_conn_extend_max_streams_bidi(ngtcp2_conn *conn,
                                                       size_t n);

/**
 * @function
 *
 * `ngtcp2_conn_extend_max_streams_uni` extends the number of maximum
 * local unidirectional streams that a remote endpoint can open by
 * |n|.
 *
 * The library does not increase maximum stream limit automatically.
 * The exception is when a stream is closed without
 * :type:`ngtcp2_stream_open` callback being called.  In this case,
 * stream limit is increased automatically.
 */
NGTCP2_EXTERN void ngtcp2_conn_extend_max_streams_uni(ngtcp2_conn *conn,
                                                      size_t n);

/**
 * @function
 *
 * `ngtcp2_conn_get_dcid` returns the non-NULL pointer to destination
 * connection ID.  If no destination connection ID is present, the
 * return value is not ``NULL``, and its datalen field is 0.
 */
NGTCP2_EXTERN const ngtcp2_cid *ngtcp2_conn_get_dcid(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_num_scid` returns the number of source connection
 * IDs which the local endpoint has provided to the peer and have not
 * retired.
 */
NGTCP2_EXTERN size_t ngtcp2_conn_get_num_scid(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_scid` writes the all source connection IDs which
 * the local endpoint has provided to the peer and have not retired in
 * |dest|.  The buffer pointed by |dest| must have
 * ``sizeof(ngtcp2_cid) * n`` bytes available, where n is the return
 * value of `ngtcp2_conn_get_num_scid()`.
 */
NGTCP2_EXTERN size_t ngtcp2_conn_get_scid(ngtcp2_conn *conn, ngtcp2_cid *dest);

/**
 * @function
 *
 * `ngtcp2_conn_get_num_active_dcid` returns the number of the active
 * destination connection ID.
 */
NGTCP2_EXTERN size_t ngtcp2_conn_get_num_active_dcid(ngtcp2_conn *conn);

/**
 * @struct
 *
 * :type:`ngtcp2_cid_token` is the convenient struct to store
 * Connection ID, its associated path, and stateless reset token.
 */
typedef struct ngtcp2_cid_token {
  /**
   * :member:`seq` is the sequence number of this Connection ID.
   */
  uint64_t seq;
  /**
   * :member:`cid` is Connection ID.
   */
  ngtcp2_cid cid;
  /**
   * :member:`ps` is the path which is associated to this Connection
   * ID.
   */
  ngtcp2_path_storage ps;
  /**
   * :member:`token` is the stateless reset token for this Connection
   * ID.
   */
  uint8_t token[NGTCP2_STATELESS_RESET_TOKENLEN];
  /**
   * :member:`token_present` is nonzero if token contains stateless
   * reset token.
   */
  uint8_t token_present;
} ngtcp2_cid_token;

/**
 * @function
 *
 * `ngtcp2_conn_get_active_dcid` writes the all active destination
 * connection IDs and tokens to |dest|.  The buffer pointed by |dest|
 * must have ``sizeof(ngtcp2_cid_token) * n`` bytes available, where n
 * is the return value of `ngtcp2_conn_get_num_active_dcid()`.
 */
NGTCP2_EXTERN size_t ngtcp2_conn_get_active_dcid(ngtcp2_conn *conn,
                                                 ngtcp2_cid_token *dest);

/**
 * @function
 *
 * `ngtcp2_conn_get_negotiated_version` returns the negotiated version.
 */
NGTCP2_EXTERN uint32_t ngtcp2_conn_get_negotiated_version(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_early_data_rejected` tells |conn| that 0-RTT data was
 * rejected by a server.
 */
NGTCP2_EXTERN int ngtcp2_conn_early_data_rejected(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_conn_stat` assigns connection statistics data to
 * |*cstat|.
 */
NGTCP2_EXTERN void ngtcp2_conn_get_conn_stat(ngtcp2_conn *conn,
                                             ngtcp2_conn_stat *cstat);

/**
 * @function
 *
 * `ngtcp2_conn_on_loss_detection_timer` should be called when a timer
 * returned from `ngtcp2_conn_earliest_expiry` fires.
 *
 * Application should call `ngtcp2_conn_handshake` if handshake has
 * not completed, otherwise `ngtcp2_conn_write_pkt` (or
 * `ngtcp2_conn_write_stream` if it has data to send) to send PTO
 * probe packets.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 */
NGTCP2_EXTERN int ngtcp2_conn_on_loss_detection_timer(ngtcp2_conn *conn,
                                                      ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_submit_crypto_data` submits crypto stream data |data|
 * of length |datalen| to the library for transmission.  The
 * encryption level is given in |crypto_level|.
 *
 * Application should keep the buffer pointed by |data| alive until
 * the data is acknowledged.  The acknowledgement is notified by
 * :type:`ngtcp2_acked_crypto_offset` callback.
 */
NGTCP2_EXTERN int
ngtcp2_conn_submit_crypto_data(ngtcp2_conn *conn,
                               ngtcp2_crypto_level crypto_level,
                               const uint8_t *data, const size_t datalen);

/**
 * @function
 *
 * `ngtcp2_conn_submit_new_token` submits address validation token.
 * It is sent in NEW_TOKEN frame.  Only server can call this function.
 * |tokenlen| must not be 0.
 *
 * This function makes a copy of the buffer pointed by |token| of
 * length |tokenlen|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_submit_new_token(ngtcp2_conn *conn,
                                               const uint8_t *token,
                                               size_t tokenlen);

/**
 * @function
 *
 * `ngtcp2_conn_set_local_addr` sets local endpoint address |addr| to
 * |conn|.
 */
NGTCP2_EXTERN void ngtcp2_conn_set_local_addr(ngtcp2_conn *conn,
                                              const ngtcp2_addr *addr);

/**
 * @function
 *
 * `ngtcp2_conn_set_remote_addr` sets remote endpoint address |addr|
 * to |conn|.
 */
NGTCP2_EXTERN void ngtcp2_conn_set_remote_addr(ngtcp2_conn *conn,
                                               const ngtcp2_addr *addr);

/**
 * @function
 *
 * `ngtcp2_conn_get_path` returns the current path.
 */
NGTCP2_EXTERN const ngtcp2_path *ngtcp2_conn_get_path(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_initiate_migration` starts connection migration to the
 * given |path| which must not be ``NULL``.  Only client can initiate
 * migration.  This function does immediate migration; it does not
 * probe peer reachability from a new local address.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_INVALID_STATE`
 *     Migration is disabled.
 * :macro:`NGTCP2_ERR_CONN_ID_BLOCKED`
 *     No unused connection ID is available.
 * :macro:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     |path| equals the current path.
 * :macro:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 */
NGTCP2_EXTERN int ngtcp2_conn_initiate_migration(ngtcp2_conn *conn,
                                                 const ngtcp2_path *path,
                                                 ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_get_max_local_streams_uni` returns the cumulative
 * number of streams which local endpoint can open.
 */
NGTCP2_EXTERN uint64_t ngtcp2_conn_get_max_local_streams_uni(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_max_data_left` returns the number of bytes that
 * this local endpoint can send in this connection.
 */
NGTCP2_EXTERN uint64_t ngtcp2_conn_get_max_data_left(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_streams_bidi_left` returns the number of
 * bidirectional streams which the local endpoint can open without
 * violating stream concurrency limit.
 */
NGTCP2_EXTERN uint64_t ngtcp2_conn_get_streams_bidi_left(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_streams_uni_left` returns the number of
 * unidirectional streams which the local endpoint can open without
 * violating stream concurrency limit.
 */
NGTCP2_EXTERN uint64_t ngtcp2_conn_get_streams_uni_left(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_set_initial_crypto_ctx` sets |ctx| for Initial packet
 * encryption.  The passed data will be passed to
 * :type:`ngtcp2_encrypt`, :type:`ngtcp2_decrypt` and
 * :type:`ngtcp2_hp_mask` callbacks.
 */
NGTCP2_EXTERN void
ngtcp2_conn_set_initial_crypto_ctx(ngtcp2_conn *conn,
                                   const ngtcp2_crypto_ctx *ctx);

/**
 * @function
 *
 * `ngtcp2_conn_get_initial_crypto_ctx` returns
 * :type:`ngtcp2_crypto_ctx` object for Initial packet encryption.
 */
NGTCP2_EXTERN const ngtcp2_crypto_ctx *
ngtcp2_conn_get_initial_crypto_ctx(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_set_crypto_ctx` sets |ctx| for Handshake/Short packet
 * encryption.  The passed data will be passed to
 * :type:`ngtcp2_encrypt`, :type:`ngtcp2_decrypt` and
 * :type:`ngtcp2_hp_mask` callbacks.
 */
NGTCP2_EXTERN void ngtcp2_conn_set_crypto_ctx(ngtcp2_conn *conn,
                                              const ngtcp2_crypto_ctx *ctx);

/**
 * @function
 *
 * `ngtcp2_conn_get_tls_native_handle` returns TLS native handle set by
 * `ngtcp2_conn_set_tls_native_handle()`.
 */
NGTCP2_EXTERN void *ngtcp2_conn_get_tls_native_handle(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_set_tls_native_handle` sets TLS native handle
 * |tls_native_handle| to |conn|.  Internally, it is used as an opaque
 * pointer.
 */
NGTCP2_EXTERN void ngtcp2_conn_set_tls_native_handle(ngtcp2_conn *conn,
                                                     void *tls_native_handle);

/**
 * @function
 *
 * `ngtcp2_conn_set_retry_aead` sets |aead| and |aead_ctx| for Retry
 * integrity tag verification.  |aead| must be AEAD_AES_128_GCM.
 * |aead_ctx| must be initialized with :macro:`NGTCP2_RETRY_KEY` as
 * encryption key.  This function must be called if |conn| is
 * initialized as client.  Server does not verify the tag and has no
 * need to call this function.
 *
 * If this function succeeds, |conn| takes ownership of |aead_ctx|.
 * :type:`ngtcp2_delete_crypto_aead_ctx` will be called to delete this
 * object when it is no longer used.  If this function fails, the
 * caller is responsible to delete it.
 */
NGTCP2_EXTERN void
ngtcp2_conn_set_retry_aead(ngtcp2_conn *conn, const ngtcp2_crypto_aead *aead,
                           const ngtcp2_crypto_aead_ctx *aead_ctx);

/**
 * @function
 *
 * `ngtcp2_conn_get_crypto_ctx` returns :type:`ngtcp2_crypto_ctx`
 * object for Handshake/Short packet encryption.
 */
NGTCP2_EXTERN const ngtcp2_crypto_ctx *
ngtcp2_conn_get_crypto_ctx(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_set_early_crypto_ctx` sets |ctx| for 0RTT packet
 * encryption.  The passed data will be passed to
 * :type:`ngtcp2_encrypt`, :type:`ngtcp2_decrypt` and
 * :type:`ngtcp2_hp_mask` callbacks.
 */
NGTCP2_EXTERN void
ngtcp2_conn_set_early_crypto_ctx(ngtcp2_conn *conn,
                                 const ngtcp2_crypto_ctx *ctx);

/**
 * @function
 *
 * `ngtcp2_conn_get_early_crypto_ctx` returns
 * :type:`ngtcp2_crypto_ctx` object for 0RTT packet encryption.
 */
NGTCP2_EXTERN const ngtcp2_crypto_ctx *
ngtcp2_conn_get_early_crypto_ctx(ngtcp2_conn *conn);

/**
 * @enum
 *
 * :type:`ngtcp2_connection_close_error_code_type` defines connection
 * error code type.
 */
typedef enum ngtcp2_connection_close_error_code_type {
  /**
   * :enum:`NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT`
   * indicates the error code is QUIC transport error code.
   */
  NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT,
  /**
   * :enum:`NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION`
   * indicates the error code is application error code.
   */
  NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION,
} ngtcp2_connection_close_error_code_type;

/**
 * @struct
 *
 * `ngtcp2_connection_close_error_code` contains connection error code
 * and its type.
 */
typedef struct ngtcp2_connection_close_error_code {
  /**
   * :member:`error_code` is the error code for connection closure.
   */
  uint64_t error_code;
  /**
   * :member:`type` is the type of :member:`error_code`.
   */
  ngtcp2_connection_close_error_code_type type;
} ngtcp2_connection_close_error_code;

/**
 * @function
 *
 * `ngtcp2_conn_get_connection_close_error_code` stores the received
 * connection close error code in |ccec|.
 */
NGTCP2_EXTERN void ngtcp2_conn_get_connection_close_error_code(
    ngtcp2_conn *conn, ngtcp2_connection_close_error_code *ccec);

/**
 * @function
 *
 * `ngtcp2_conn_is_local_stream` returns nonzero if |stream_id| denotes the
 * stream which a local endpoint issues.
 */
NGTCP2_EXTERN int ngtcp2_conn_is_local_stream(ngtcp2_conn *conn,
                                              int64_t stream_id);

/**
 * @function
 *
 * `ngtcp2_conn_is_server` returns nonzero if |conn| is initialized as
 * server.
 */
NGTCP2_EXTERN int ngtcp2_conn_is_server(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_after_retry` returns nonzero if |conn| as a client has
 * received Retry packet from server and successfully validated it.
 */
NGTCP2_EXTERN int ngtcp2_conn_after_retry(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_set_stream_user_data` sets |stream_user_data| to the
 * stream identified by |stream_id|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :macro:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream does not exist
 */
NGTCP2_EXTERN int ngtcp2_conn_set_stream_user_data(ngtcp2_conn *conn,
                                                   int64_t stream_id,
                                                   void *stream_user_data);

/**
 * @function
 *
 * `ngtcp2_strerror` returns the text representation of |liberr|.
 * |liberr| must be one of ngtcp2 library error codes (which is
 * defined as NGTCP2_ERR_* macro, such as
 * :macro:`NGTCP2_ERR_TLS_DECRYPT`).
 */
NGTCP2_EXTERN const char *ngtcp2_strerror(int liberr);

/**
 * @function
 *
 * `ngtcp2_err_is_fatal` returns nonzero if |liberr| is a fatal error.
 * |liberr| must be one of ngtcp2 library error codes (which is
 * defined as NGTCP2_ERR_* macro, such as
 * :macro:`NGTCP2_ERR_TLS_DECRYPT`).
 */
NGTCP2_EXTERN int ngtcp2_err_is_fatal(int liberr);

/**
 * @function
 *
 * `ngtcp2_err_infer_quic_transport_error_code` returns a QUIC
 * transport error code which corresponds to |liberr|.  |liberr| must
 * be one of ngtcp2 library error codes (which is defined as
 * NGTCP2_ERR_* macro, such as :macro:`NGTCP2_ERR_TLS_DECRYPT`).
 */
NGTCP2_EXTERN uint64_t ngtcp2_err_infer_quic_transport_error_code(int liberr);

/**
 * @function
 *
 * `ngtcp2_addr_init` initializes |dest| with the given arguments and
 * returns |dest|.
 */
NGTCP2_EXTERN ngtcp2_addr *ngtcp2_addr_init(ngtcp2_addr *dest,
                                            const struct sockaddr *addr,
                                            size_t addrlen, void *user_data);

/**
 * @function
 *
 * `ngtcp2_path_storage_init` initializes |ps| with the given
 * arguments.  This function copies |local_addr| and |remote_addr|.
 */
NGTCP2_EXTERN void ngtcp2_path_storage_init(ngtcp2_path_storage *ps,
                                            const struct sockaddr *local_addr,
                                            size_t local_addrlen,
                                            void *local_user_data,
                                            const struct sockaddr *remote_addr,
                                            size_t remote_addrlen,
                                            void *remote_user_data);

/**
 * @function
 *
 * `ngtcp2_path_storage_zero` initializes |ps| with the zero length
 * addresses.
 */
NGTCP2_EXTERN void ngtcp2_path_storage_zero(ngtcp2_path_storage *ps);

/**
 * @function
 *
 * `ngtcp2_settings_default` initializes |settings| with the default
 * values.  First this function fills |settings| with 0 and set the
 * default value to the following fields:
 *
 * * :type:`cc_algo <ngtcp2_settings.cc_algo>` =
 *   :enum:`ngtcp2_cc_algo.NGTCP2_CC_ALGO_CUBIC`
 * * :type:`initial_rtt <ngtcp2_settings.initial_rtt>` =
 *   :macro:`NGTCP2_DEFAULT_INITIAL_RTT`
 * * :type:`ack_thresh <ngtcp2_settings.ack_thresh>` = 2
 */
NGTCP2_EXTERN void ngtcp2_settings_default(ngtcp2_settings *settings);

/**
 * @function
 *
 * `ngtcp2_transport_params_default` initializes |params| with the
 * default values.  First this function fills |params| with 0 and set
 * the default value to the following fields:
 *
 * * :type:`max_udp_payload_size
 *   <ngtcp2_transport_params.max_udp_payload_size>` =
 *   :macro:`NGTCP2_DEFAULT_MAX_UDP_PAYLOAD_SIZE`
 * * :type:`ack_delay_exponent
 *   <ngtcp2_transport_params.ack_delay_exponent>` =
 *   :macro:`NGTCP2_DEFAULT_ACK_DELAY_EXPONENT`
 * * :type:`max_ack_delay <ngtcp2_transport_params.max_ack_delay>` =
 *   :macro:`NGTCP2_DEFAULT_MAX_ACK_DELAY`
 * * :type:`active_connection_id_limit
 *   <ngtcp2_transport_params.active_connection_id_limit>` =
 *   :macro:`NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT`
 */
NGTCP2_EXTERN void
ngtcp2_transport_params_default(ngtcp2_transport_params *params);

/**
 * @function
 *
 * `ngtcp2_mem_default` returns the default, system standard memory
 * allocator.
 */
NGTCP2_EXTERN const ngtcp2_mem *ngtcp2_mem_default(void);

/**
 * @macrosection
 *
 * ngtcp2_info macros
 */

/**
 * @macro
 *
 * :macro:`NGTCP2_VERSION_AGE` is the age of :type:`ngtcp2_info`
 */
#define NGTCP2_VERSION_AGE 1

/**
 * @struct
 *
 * :type:`ngtcp2_info` is what `ngtcp2_version()` returns.  It holds
 * information about the particular ngtcp2 version.
 */
typedef struct ngtcp2_info {
  /**
   * :member:`age` is the age of this struct.  This instance of ngtcp2
   * sets it to :macro:`NGTCP2_VERSION_AGE` but a future version may
   * bump it and add more struct fields at the bottom
   */
  int age;
  /**
   * :member:`version_num` is the :macro:`NGTCP2_VERSION_NUM` number
   * (since age ==1)
   */
  int version_num;
  /**
   * :member:`version_str` points to the :macro:`NGTCP2_VERSION`
   * string (since age ==1)
   */
  const char *version_str;
  /* -------- the above fields all exist when age == 1 */
} ngtcp2_info;

/**
 * @function
 *
 * Returns a pointer to a ngtcp2_info struct with version information
 * about the run-time library in use.  The |least_version| argument
 * can be set to a 24 bit numerical value for the least accepted
 * version number and if the condition is not met, this function will
 * return a ``NULL``.  Pass in 0 to skip the version checking.
 */
NGTCP2_EXTERN ngtcp2_info *ngtcp2_version(int least_version);

/**
 * @function
 *
 * `ngtcp2_is_bidi_stream` returns nonzero if |stream_id| denotes
 * bidirectional stream.
 */
NGTCP2_EXTERN int ngtcp2_is_bidi_stream(int64_t stream_id);

/**
 * @enum
 *
 * :type:`ngtcp2_log_event` defines an event of ngtcp2 library
 * internal logger.
 */
typedef enum {
  /**
   * :enum:`NGTCP2_LOG_EVENT_NONE` represents no event.
   */
  NGTCP2_LOG_EVENT_NONE,
  /**
   * :enum:`NGTCP2_LOG_EVENT_CON` is a connection (catch-all) event
   */
  NGTCP2_LOG_EVENT_CON,
  /**
   * :enum:`NGTCP2_LOG_EVENT_PKT` is a packet event.
   */
  NGTCP2_LOG_EVENT_PKT,
  /**
   * :enum:`NGTCP2_LOG_EVENT_FRM` is a QUIC frame event.
   */
  NGTCP2_LOG_EVENT_FRM,
  /**
   * :enum:`NGTCP2_LOG_EVENT_RCV` is a congestion and recovery event.
   */
  NGTCP2_LOG_EVENT_RCV,
  /**
   * :enum:`NGTCP2_LOG_EVENT_CRY` is a crypto event.
   */
  NGTCP2_LOG_EVENT_CRY,
  /**
   * :enum:`NGTCP2_LOG_EVENT_PTV` is a path validation event.
   */
  NGTCP2_LOG_EVENT_PTV,
} ngtcp2_log_event;

/**
 * @function
 *
 * `ngtcp2_log_info` writes info level log.
 */
NGTCP2_EXTERN void ngtcp2_log_info(ngtcp2_log *log, ngtcp2_log_event ev,
                                   const char *fmt, ...);

/**
 * @function
 *
 * `ngtcp2_path_copy` copies |src| into |dest|.  This function assumes
 * that |dest| has enough buffer to store the deep copy of src->local
 * and src->remote.
 */
NGTCP2_EXTERN void ngtcp2_path_copy(ngtcp2_path *dest, const ngtcp2_path *src);

/**
 * @function
 *
 * `ngtcp2_path_eq` returns nonzero if |a| and |b| shares the same
 * local and remote addresses.
 */
NGTCP2_EXTERN int ngtcp2_path_eq(const ngtcp2_path *a, const ngtcp2_path *b);

#ifdef __cplusplus
}
#endif

#endif /* NGTCP2_H */
