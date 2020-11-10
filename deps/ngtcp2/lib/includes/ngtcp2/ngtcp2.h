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

/* NGTCP2_PROTO_VER is the supported QUIC protocol version. */
#define NGTCP2_PROTO_VER 0xff00001du
/* NGTCP2_PROTO_VER_MAX is the highest QUIC version the library
   supports. */
#define NGTCP2_PROTO_VER_MAX NGTCP2_PROTO_VER

#define NGTCP2_MAX_PKTLEN_IPV4 1252
#define NGTCP2_MAX_PKTLEN_IPV6 1232

/* NGTCP2_MIN_INITIAL_PKTLEN is the minimum UDP packet size for a
   packet sent by client which contains its first Initial packet. */
#define NGTCP2_MIN_INITIAL_PKTLEN 1200

/* NGTCP2_DEFAULT_MAX_PKTLEN is the default maximum size of UDP
   datagram payload that this endpoint transmits.  It is used by
   congestion controller to compute congestion window. */
#define NGTCP2_DEFAULT_MAX_PKTLEN 1200

/* NGTCP2_STATELESS_RESET_TOKENLEN is the length of Stateless Reset
   Token. */
#define NGTCP2_STATELESS_RESET_TOKENLEN 16

/* NGTCP2_MIN_STATELESS_RESET_RANDLEN is the minimum length of random
   bytes (Unpredictable Bits) in Stateless Retry packet */
#define NGTCP2_MIN_STATELESS_RESET_RANDLEN 5

/* NGTCP2_INITIAL_SALT is a salt value which is used to derive initial
   secret. */
#define NGTCP2_INITIAL_SALT                                                    \
  "\xaf\xbf\xec\x28\x99\x93\xd2\x4c\x9e\x97\x86\xf1\x9c\x61\x11\xe0\x43\x90"   \
  "\xa8\x99"

/* NGTCP2_HP_MASKLEN is the length of header protection mask. */
#define NGTCP2_HP_MASKLEN 5

/* NGTCP2_HP_SAMPLELEN is the number bytes sampled when encrypting a
   packet header. */
#define NGTCP2_HP_SAMPLELEN 16

/* NGTCP2_SECONDS is a count of tick which corresponds to 1 second. */
#define NGTCP2_SECONDS ((uint64_t)1000000000ULL)

/* NGTCP2_MILLISECONDS is a count of tick which corresponds to 1
   millisecond. */
#define NGTCP2_MILLISECONDS ((uint64_t)1000000ULL)

/* NGTCP2_MICROSECONDS is a count of tick which corresponds to 1
   microsecond. */
#define NGTCP2_MICROSECONDS ((uint64_t)1000ULL)

/* NGTCP2_NANOSECONDS is a count of tick which corresponds to 1
   nanosecond. */
#define NGTCP2_NANOSECONDS ((uint64_t)1ULL)

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef enum ngtcp2_lib_error : int {
#else
typedef enum ngtcp2_lib_error {
#endif
  NGTCP2_ERR_INVALID_ARGUMENT = -201,
  NGTCP2_ERR_UNKNOWN_PKT_TYPE = -202,
  NGTCP2_ERR_NOBUF = -203,
  NGTCP2_ERR_PROTO = -205,
  NGTCP2_ERR_INVALID_STATE = -206,
  NGTCP2_ERR_ACK_FRAME = -207,
  NGTCP2_ERR_STREAM_ID_BLOCKED = -208,
  NGTCP2_ERR_STREAM_IN_USE = -209,
  NGTCP2_ERR_STREAM_DATA_BLOCKED = -210,
  NGTCP2_ERR_FLOW_CONTROL = -211,
  NGTCP2_ERR_CONNECTION_ID_LIMIT = -212,
  NGTCP2_ERR_STREAM_LIMIT = -213,
  NGTCP2_ERR_FINAL_SIZE = -214,
  NGTCP2_ERR_CRYPTO = -215,
  NGTCP2_ERR_PKT_NUM_EXHAUSTED = -216,
  NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM = -217,
  NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM = -218,
  NGTCP2_ERR_FRAME_ENCODING = -219,
  NGTCP2_ERR_TLS_DECRYPT = -220,
  NGTCP2_ERR_STREAM_SHUT_WR = -221,
  NGTCP2_ERR_STREAM_NOT_FOUND = -222,
  NGTCP2_ERR_STREAM_STATE = -226,
  NGTCP2_ERR_RECV_VERSION_NEGOTIATION = -229,
  NGTCP2_ERR_CLOSING = -230,
  NGTCP2_ERR_DRAINING = -231,
  NGTCP2_ERR_TRANSPORT_PARAM = -234,
  NGTCP2_ERR_DISCARD_PKT = -235,
  NGTCP2_ERR_PATH_VALIDATION_FAILED = -236,
  NGTCP2_ERR_CONN_ID_BLOCKED = -237,
  NGTCP2_ERR_INTERNAL = -238,
  NGTCP2_ERR_CRYPTO_BUFFER_EXCEEDED = -239,
  NGTCP2_ERR_WRITE_STREAM_MORE = -240,
  NGTCP2_ERR_RETRY = -241,
  NGTCP2_ERR_DROP_CONN = -242,
  NGTCP2_ERR_FATAL = -500,
  NGTCP2_ERR_NOMEM = -501,
  NGTCP2_ERR_CALLBACK_FAILURE = -502,
} ngtcp2_lib_error;

typedef enum ngtcp2_pkt_flag {
  NGTCP2_PKT_FLAG_NONE = 0,
  NGTCP2_PKT_FLAG_LONG_FORM = 0x01,
  NGTCP2_PKT_FLAG_KEY_PHASE = 0x04
} ngtcp2_pkt_flag;

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef enum ngtcp2_pkt_type : int {
#else
typedef enum ngtcp2_pkt_type {
#endif
  /* NGTCP2_PKT_VERSION_NEGOTIATION is defined by libngtcp2 for
     convenience. */
  NGTCP2_PKT_VERSION_NEGOTIATION = 0xf0,
  NGTCP2_PKT_INITIAL = 0x0,
  NGTCP2_PKT_0RTT = 0x1,
  NGTCP2_PKT_HANDSHAKE = 0x2,
  NGTCP2_PKT_RETRY = 0x3,
  /* NGTCP2_PKT_SHORT is defined by libngtcp2 for convenience. */
  NGTCP2_PKT_SHORT = 0x70
} ngtcp2_pkt_type;

/* QUIC transport error code. */
#define NGTCP2_NO_ERROR 0x0u
#define NGTCP2_INTERNAL_ERROR 0x1u
#define NGTCP2_CONNECTION_REFUSED 0x2u
#define NGTCP2_FLOW_CONTROL_ERROR 0x3u
#define NGTCP2_STREAM_LIMIT_ERROR 0x4u
#define NGTCP2_STREAM_STATE_ERROR 0x5u
#define NGTCP2_FINAL_SIZE_ERROR 0x6u
#define NGTCP2_FRAME_ENCODING_ERROR 0x7u
#define NGTCP2_TRANSPORT_PARAMETER_ERROR 0x8u
#define NGTCP2_CONNECTION_ID_LIMIT_ERROR 0x9u
#define NGTCP2_PROTOCOL_VIOLATION 0xau
#define NGTCP2_INVALID_TOKEN 0xbu
#define NGTCP2_APPLICATION_ERROR 0xcu
#define NGTCP2_CRYPTO_BUFFER_EXCEEDED 0xdu
#define NGTCP2_KEY_UPDATE_ERROR 0xeu
#define NGTCP2_CRYPTO_ERROR 0x100u

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef enum ngtcp2_path_validation_result : int {
#else
typedef enum ngtcp2_path_validation_result {
#endif
  NGTCP2_PATH_VALIDATION_RESULT_SUCCESS,
  NGTCP2_PATH_VALIDATION_RESULT_FAILURE,
} ngtcp2_path_validation_result;

/*
 * ngtcp2_tstamp is a timestamp with nanosecond resolution.
 */
typedef uint64_t ngtcp2_tstamp;

/*
 * ngtcp2_duration is a period of time in nanosecond resolution.
 */
typedef uint64_t ngtcp2_duration;

/* NGTCP2_MAX_CIDLEN is the maximum length of Connection ID. */
#define NGTCP2_MAX_CIDLEN 20
/* NGTCP2_MIN_CIDLEN is the minimum length of Connection ID. */
#define NGTCP2_MIN_CIDLEN 1

/* NGTCP2_MIN_INITIAL_DCIDLEN is the minimum length of Destination
   Connection ID in Client Initial packet if it does not bear token
   from Retry packet. */
#define NGTCP2_MIN_INITIAL_DCIDLEN 8

/**
 * @struct
 *
 * ngtcp2_cid holds a Connection ID.
 */
typedef struct ngtcp2_cid {
  size_t datalen;
  uint8_t data[NGTCP2_MAX_CIDLEN];
} ngtcp2_cid;

/**
 * @struct
 *
 * ngtcp2_vec is struct iovec compatible structure to reference
 * arbitrary array of bytes.
 */
typedef struct ngtcp2_vec {
  /* base points to the data. */
  uint8_t *base;
  /* len is the number of bytes which the buffer pointed by base
     contains. */
  size_t len;
} ngtcp2_vec;

/**
 * @function
 *
 * `ngtcp2_cid_init` initializes Connection ID |cid| with the byte
 * string pointed by |data| and its length is |datalen|.  |datalen|
 * must be at least :enum:`NGTCP2_MIN_CIDLEN`, and at most
 * :enum:`NGTCP2_MAX_CIDLEN`.
 */
NGTCP2_EXTERN void ngtcp2_cid_init(ngtcp2_cid *cid, const uint8_t *data,
                                   size_t datalen);

typedef struct ngtcp2_pkt_hd {
  ngtcp2_cid dcid;
  ngtcp2_cid scid;
  int64_t pkt_num;
  ngtcp2_vec token;
  /**
   * pkt_numlen is the number of bytes spent to encode pkt_num.
   */
  size_t pkt_numlen;
  /**
   * len is the sum of pkt_numlen and the length of QUIC packet
   * payload.
   */
  size_t len;
  uint32_t version;
  uint8_t type;
  uint8_t flags;
} ngtcp2_pkt_hd;

typedef struct ngtcp2_pkt_stateless_reset {
  uint8_t stateless_reset_token[NGTCP2_STATELESS_RESET_TOKENLEN];
  const uint8_t *rand;
  size_t randlen;
} ngtcp2_pkt_stateless_reset;

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef enum ngtcp2_transport_param_id : int {
#else
typedef enum ngtcp2_transport_param_id {
#endif
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
  NGTCP2_TRANSPORT_PARAM_RETRY_SOURCE_CONNECTION_ID = 0x0010
} ngtcp2_transport_param_id;

#if defined(__cplusplus) && __cplusplus >= 201103L
typedef enum ngtcp2_transport_params_type : int {
#else
typedef enum ngtcp2_transport_params_type {
#endif
  NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO,
  NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS
} ngtcp2_transport_params_type;

/**
 * @enum
 *
 * ngtcp2_rand_ctx is a context where generated random value is used.
 */
#if defined(__cplusplus) && __cplusplus >= 201103L
typedef enum ngtcp2_rand_ctx : int {
#else
typedef enum ngtcp2_rand_ctx {
#endif
  NGTCP2_RAND_CTX_NONE,
  /**
   * NGTCP2_RAND_CTX_PATH_CHALLENGE indicates that random value is
   * used for PATH_CHALLENGE.
   */
  NGTCP2_RAND_CTX_PATH_CHALLENGE
} ngtcp2_rand_ctx;

/*
 * NGTCP2_DEFAULT_MAX_UDP_PAYLOAD_SIZE is the default value of
 * max_udp_payload_size transport parameter.
 */
#define NGTCP2_DEFAULT_MAX_UDP_PAYLOAD_SIZE 65527

/**
 * @macro
 *
 * NGTCP2_DEFAULT_ACK_DELAY_EXPONENT is a default value of scaling
 * factor of ACK Delay field in ACK frame.
 */
#define NGTCP2_DEFAULT_ACK_DELAY_EXPONENT 3

/**
 * @macro
 *
 * NGTCP2_DEFAULT_MAX_ACK_DELAY is a default value of the maximum
 * amount of time in nanoseconds by which endpoint delays sending
 * acknowledgement.
 */
#define NGTCP2_DEFAULT_MAX_ACK_DELAY (25 * NGTCP2_MILLISECONDS)

/**
 * @macro
 *
 * NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT is the default value of
 * active_connection_id_limit transport parameter value if omitted.
 */
#define NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT 2

/**
 * @macro
 *
 * NGTCP2_TLSEXT_QUIC_TRANSPORT_PARAMETERS is TLS extension type of
 * quic_transport_parameters.
 */
#define NGTCP2_TLSEXT_QUIC_TRANSPORT_PARAMETERS 0xffa5u

typedef struct ngtcp2_preferred_addr {
  ngtcp2_cid cid;
  uint16_t ipv4_port;
  uint16_t ipv6_port;
  uint8_t ipv4_addr[4];
  uint8_t ipv6_addr[16];
  uint8_t stateless_reset_token[NGTCP2_STATELESS_RESET_TOKENLEN];
} ngtcp2_preferred_addr;

typedef struct ngtcp2_transport_params {
  ngtcp2_preferred_addr preferred_address;
  /* original_dcid is the Destination Connection ID field from the
     first Initial packet from client.  Server must specify this
     field.  If application specifies retry_scid_present to nonzero,
     then it must also specify this field.  It is expected that
     application knows the original Destination Connection ID, for
     example, by including it in retry token.  Otherwise, application
     should not specify this field. */
  ngtcp2_cid original_dcid;
  /* initial_scid is the Source Connection ID field from the first
     Initial packet the endpoint sends.  Application should not
     specify this field. */
  ngtcp2_cid initial_scid;
  /* retry_scid is the Source Connection ID field from Retry packet.
     Only server uses this field.  If server application received
     Initial packet with retry token from client and server verified
     its token, server application must set Destination Connection ID
     field from the Initial packet to this field and set
     retry_scid_present to nonzero.  Server application must verify
     that the Destination Connection ID from Initial packet was sent
     in Retry packet by, for example, including the Connection ID in a
     token, or including it in AAD when encrypting a token. */
  ngtcp2_cid retry_scid;
  /* initial_max_stream_data_bidi_local is the size of flow control
     window of locally initiated stream.  This is the number of bytes
     that the remote endpoint can send and the local endpoint must
     ensure that it has enough buffer to receive them. */
  uint64_t initial_max_stream_data_bidi_local;
  /* initial_max_stream_data_bidi_remote is the size of flow control
     window of remotely initiated stream.  This is the number of bytes
     that the remote endpoint can send and the local endpoint must
     ensure that it has enough buffer to receive them. */
  uint64_t initial_max_stream_data_bidi_remote;
  /* initial_max_stream_data_uni is the size of flow control window of
     remotely initiated unidirectional stream.  This is the number of
     bytes that the remote endpoint can send and the local endpoint
     must ensure that it has enough buffer to receive them. */
  uint64_t initial_max_stream_data_uni;
  /* initial_max_data is the connection level flow control window. */
  uint64_t initial_max_data;
  /* initial_max_streams_bidi is the number of concurrent streams that
     the remote endpoint can create. */
  uint64_t initial_max_streams_bidi;
  /* initial_max_streams_uni is the number of concurrent
     unidirectional streams that the remote endpoint can create. */
  uint64_t initial_max_streams_uni;
  /* max_idle_timeout is a duration during which sender allows
     quiescent. */
  ngtcp2_duration max_idle_timeout;
  uint64_t max_udp_payload_size;
  /* active_connection_id_limit is the maximum number of Connection ID
     that sender can store. */
  uint64_t active_connection_id_limit;
  uint64_t ack_delay_exponent;
  ngtcp2_duration max_ack_delay;
  uint8_t stateless_reset_token_present;
  uint8_t disable_active_migration;
  uint8_t retry_scid_present;
  uint8_t preferred_address_present;
  uint8_t stateless_reset_token[NGTCP2_STATELESS_RESET_TOKENLEN];
} ngtcp2_transport_params;

typedef struct ngtcp2_log ngtcp2_log;

typedef enum ngtcp2_pktns_id {
  /* NGTCP2_PKTNS_ID_INITIAL is the Initial packet number space. */
  NGTCP2_PKTNS_ID_INITIAL,
  /* NGTCP2_PKTNS_ID_INITIAL is the Handshake packet number space. */
  NGTCP2_PKTNS_ID_HANDSHAKE,
  /* NGTCP2_PKTNS_ID_INITIAL is the Application data packet number
     space. */
  NGTCP2_PKTNS_ID_APP,
  /* NGTCP2_PKTNS_ID_MAX is defined to get the number of packet number
     spaces. */
  NGTCP2_PKTNS_ID_MAX
} ngtcp2_pktns_id;

/**
 * @struct
 *
 * ngtcp2_conn_stat holds various connection statistics, and computed
 * data for recovery and congestion controller.
 */
typedef struct ngtcp2_conn_stat {
  ngtcp2_duration latest_rtt;
  ngtcp2_duration min_rtt;
  ngtcp2_duration smoothed_rtt;
  ngtcp2_duration rttvar;
  size_t pto_count;
  ngtcp2_tstamp loss_detection_timer;
  /* last_tx_pkt_ts corresponds to
     time_of_last_sent_ack_eliciting_packet in
     draft-ietf-quic-recovery-25. */
  ngtcp2_tstamp last_tx_pkt_ts[NGTCP2_PKTNS_ID_MAX];
  ngtcp2_tstamp loss_time[NGTCP2_PKTNS_ID_MAX];
  uint64_t cwnd;
  uint64_t ssthresh;
  ngtcp2_tstamp congestion_recovery_start_ts;
  uint64_t bytes_in_flight;
  /* max_udp_payload_size is the maximum size of UDP datagram payload
     that this endpoint transmits.  It is used by congestion
     controller to compute congestion window. */
  size_t max_udp_payload_size;
  /* bytes_sent is the number of bytes sent in this particular
     connection.  It only includes data written by
     `ngtcp2_conn_writev_stream()` .*/
  uint64_t bytes_sent;
  /* bytes_recv is the number of bytes received in this particular
     connection, including discarded packets. */
  uint64_t bytes_recv;
  /* delivery_rate_sec is the current sending rate measured per
     second. */
  uint64_t delivery_rate_sec;
  /* recv_rate_sec is the current receiving rate of application data
     measured in per second. */
  uint64_t recv_rate_sec;
} ngtcp2_conn_stat;

typedef enum ngtcp2_cc_algo {
  NGTCP2_CC_ALGO_RENO = 0x00,
  NGTCP2_CC_ALGO_CUBIC = 0x01,
  NGTCP2_CC_ALGO_CUSTOM = 0xff
} ngtcp2_cc_algo;

typedef struct ngtcp2_cc_base {
  ngtcp2_log *log;
} ngtcp2_cc_base;

/* ngtcp2_cc_pkt is a convenient structure to include acked/lost/sent
   packet. */
typedef struct {
  /* pkt_num is the packet number */
  int64_t pkt_num;
  /* pktlen is the length of packet. */
  size_t pktlen;
  /* pktns_id is the ID of packet number space which this packet
     belongs to. */
  ngtcp2_pktns_id pktns_id;
  /* ts_sent is the timestamp when packet is sent. */
  ngtcp2_tstamp ts_sent;
} ngtcp2_cc_pkt;

typedef struct ngtcp2_cc ngtcp2_cc;

typedef void (*ngtcp2_cc_on_pkt_acked)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                       const ngtcp2_cc_pkt *pkt,
                                       ngtcp2_tstamp ts);

typedef void (*ngtcp2_cc_congestion_event)(ngtcp2_cc *cc,
                                           ngtcp2_conn_stat *cstat,
                                           ngtcp2_tstamp ts_sent,
                                           ngtcp2_tstamp ts);

typedef void (*ngtcp2_cc_on_persistent_congestion)(ngtcp2_cc *cc,
                                                   ngtcp2_conn_stat *cstat,
                                                   ngtcp2_tstamp ts);

typedef void (*ngtcp2_cc_on_ack_recv)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                      ngtcp2_tstamp ts);

typedef void (*ngtcp2_cc_on_pkt_sent)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                      const ngtcp2_cc_pkt *pkt);

typedef void (*ngtcp2_cc_new_rtt_sample)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                         ngtcp2_tstamp ts);

typedef void (*ngtcp2_cc_reset)(ngtcp2_cc *cc);

typedef enum ngtcp2_cc_event_type {
  /* NGTCP2_CC_EVENT_TX_START occurs when ack-eliciting packet is sent
     and no other ack-eliciting packet is present. */
  NGTCP2_CC_EVENT_TYPE_TX_START
} ngtcp2_cc_event_type;

typedef void (*ngtcp2_cc_event)(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                ngtcp2_cc_event_type event, ngtcp2_tstamp ts);

typedef struct ngtcp2_cc {
  ngtcp2_cc_base *ccb;
  ngtcp2_cc_on_pkt_acked on_pkt_acked;
  ngtcp2_cc_congestion_event congestion_event;
  ngtcp2_cc_on_persistent_congestion on_persistent_congestion;
  ngtcp2_cc_on_ack_recv on_ack_recv;
  ngtcp2_cc_on_pkt_sent on_pkt_sent;
  ngtcp2_cc_new_rtt_sample new_rtt_sample;
  ngtcp2_cc_reset reset;
  ngtcp2_cc_event event;
} ngtcp2_cc;

/* user_data is the same object passed to ngtcp2_conn_client_new or
   ngtcp2_conn_server_new. */
typedef void (*ngtcp2_printf)(void *user_data, const char *format, ...);

typedef void (*ngtcp2_qlog_write)(void *user_data, const void *data,
                                  size_t datalen);

typedef struct ngtcp2_qlog_settings {
  /* odcid is Original Destination Connection ID sent by client.  It
     is used as group_id and ODCID fields.  Client ignores this field
     and uses dcid parameter passed to `ngtcp2_conn_client_new()`. */
  ngtcp2_cid odcid;
  /* write is a callback function to write qlog.  Setting NULL
     disables qlog. */
  ngtcp2_qlog_write write;
} ngtcp2_qlog_settings;

typedef struct ngtcp2_settings {
  /* transport_params is the QUIC transport parameters to send. */
  ngtcp2_transport_params transport_params;
  ngtcp2_qlog_settings qlog;
  ngtcp2_cc_algo cc_algo;
  ngtcp2_cc *cc;
  /* initial_ts is an initial timestamp given to the library. */
  ngtcp2_tstamp initial_ts;
  /* log_printf is a function that the library uses to write logs.
     NULL means no logging output. */
  ngtcp2_printf log_printf;
  /* max_udp_payload_size is the maximum size of UDP datagram payload
     that this endpoint transmits.  It is used by congestion
     controller to compute congestion window.  If it is set to 0, it
     defaults to NGTCP2_DEFAULT_MAX_PKTLEN. */
  size_t max_udp_payload_size;
  /**
   * token is a token from Retry packet or NEW_TOKEN frame.
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
} ngtcp2_settings;

/**
 * @struct
 *
 * ngtcp2_addr is the endpoint address.
 */
typedef struct ngtcp2_addr {
  /* addrlen is the length of addr. */
  size_t addrlen;
  /* addr points to the buffer which contains endpoint address.  It is
     opaque to the ngtcp2 library.  It must not be NULL. */
  uint8_t *addr;
  /* user_data is an arbitrary data and opaque to the library. */
  void *user_data;
} ngtcp2_addr;

/**
 * @struct
 *
 * ngtcp2_path is the network endpoints where a packet is sent and
 * received.
 */
typedef struct ngtcp2_path {
  /* local is the address of local endpoint. */
  ngtcp2_addr local;
  /* remote is the address of remote endpoint. */
  ngtcp2_addr remote;
} ngtcp2_path;

/**
 * @struct
 *
 * ngtcp2_path_storage is a convenient struct to have buffers to store
 * the longest addresses.
 */
typedef struct ngtcp2_path_storage {
  uint8_t local_addrbuf[128];
  uint8_t remote_addrbuf[128];
  ngtcp2_path path;
} ngtcp2_path_storage;

/**
 * @struct
 *
 * `ngtcp2_crypto_md` is a wrapper around native message digest
 * object.
 *
 * If libngtcp2_crypto_openssl is linked, native_handle must be a
 * pointer to EVP_MD.
 */
typedef struct ngtcp2_crypto_md {
  void *native_handle;
} ngtcp2_crypto_md;

/**
 * @struct
 *
 * `ngtcp2_crypto_aead` is a wrapper around native AEAD object.
 *
 * If libngtcp2_crypto_openssl is linked, native_handle must be a
 * pointer to EVP_CIPHER.
 */
typedef struct ngtcp2_crypto_aead {
  void *native_handle;
} ngtcp2_crypto_aead;

/**
 * @struct
 *
 * `ngtcp2_crypto_cipher` is a wrapper around native cipher object.
 *
 * If libngtcp2_crypto_openssl is linked, native_handle must be a
 * pointer to EVP_CIPHER.
 */
typedef struct ngtcp2_crypto_cipher {
  void *native_handle;
} ngtcp2_crypto_cipher;

/**
 * @function
 *
 * `ngtcp2_crypto_ctx` is a convenient structure to bind all crypto
 * related objects in one place.  Use `ngtcp2_crypto_ctx_initial` to
 * initialize this struct for Initial packet encryption.  For
 * Handshake and Shortpackets, use `ngtcp2_crypto_ctx_tls`.
 */
typedef struct ngtcp2_crypto_ctx {
  ngtcp2_crypto_aead aead;
  ngtcp2_crypto_md md;
  ngtcp2_crypto_cipher hp;
  /* max_encryption is the number of encryption which this key can be
     used with. */
  uint64_t max_encryption;
  /* max_decryption_failure is the number of decryption failure with
     this key. */
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
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :enum:`NGTCP2_ERR_INVALID_ARGUMENT`:
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
 * :enum:`NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM`
 *     The required parameter is missing.
 * :enum:`NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM`
 *     The input is malformed.
 * :enum:`NGTCP2_ERR_INVALID_ARGUMENT`:
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
 * If the given packet is Short packet, |*pversion| will be
 * :macro:`NGTCP2_PROTO_VER`, |*pscid| will be NULL, and |*pscidlen|
 * will be 0.  Because the Short packet does not have the length of
 * Destination Connection ID, the caller has to pass the length in
 * |short_dcidlen|.  This function extracts the pointer to the
 * Destination Connection ID and assigns it to |*pdcid|.
 * |short_dcidlen| is assigned to |*pdcidlen|.
 *
 * This function returns 0 or 1 if it succeeds.  It returns 1 if
 * Version Negotiation packet should be sent.  Otherwise, one of the
 * following negative error code:
 *
 * :enum:`NGTCP2_ERR_INVALID_ARGUMENT`
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
 * :enum:`NGTCP2_MAX_CIDLEN`.  Consider to use
 * `ngtcp2_pkt_decode_version_cid` to get longer Connection ID.
 *
 * This function handles Version Negotiation specially.  If version
 * field is 0, |pkt| must contain Version Negotiation packet.  Version
 * Negotiation packet has random type in wire format.  For
 * convenience, this function sets
 * :enum:`NGTCP2_PKT_VERSION_NEGOTIATION` to dest->type, and set
 * dest->payloadlen and dest->pkt_num to 0.  Version Negotiation
 * packet occupies a single packet.
 *
 * It stores the result in the object pointed by |dest|, and returns
 * the number of bytes decoded to read the packet header if it
 * succeeds, or one of the following error codes:
 *
 * :enum:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     Packet is too short; or it is not a long header
 * :enum:`NGTCP2_ERR_UNKNOWN_PKT_TYPE`
 *     Packet type is unknown
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
 * :enum:`NGTCP2_MAX_CIDLEN`.  Consider to use
 * `ngtcp2_pkt_decode_version_cid` to get longer Connection ID.  It
 * stores the result in the object pointed by |dest|, and returns the
 * number of bytes decoded to read the packet header if it succeeds,
 * or one of the following error codes:
 *
 * :enum:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     Packet is too short; or it is not a short header
 * :enum:`NGTCP2_ERR_UNKNOWN_PKT_TYPE`
 *     Packet type is unknown
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
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :enum:`NGTCP2_ERR_INVALID_ARGUMENT`
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
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_write_version_negotiation(
    uint8_t *dest, size_t destlen, uint8_t unused_random, const uint8_t *dcid,
    size_t dcidlen, const uint8_t *scid, size_t scidlen, const uint32_t *sv,
    size_t nsv);

/**
 * @function
 *
 * `ngtcp2_pkt_get_type_long` returns the long packet type.  |c| is
 * the first byte of Long packet header.
 */
NGTCP2_EXTERN uint8_t ngtcp2_pkt_get_type_long(uint8_t c);

struct ngtcp2_conn;

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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
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
 * occurs, return :enum:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the
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
 * ngtcp2_crypto_level is encryption level.
 */
#if defined(__cplusplus) && __cplusplus >= 201103L
typedef enum ngtcp2_crypto_level : int {
#else
typedef enum ngtcp2_crypto_level {
#endif
  /**
   * NGTCP2_CRYPTO_LEVEL_INITIAL is Initial Keys encryption level.
   */
  NGTCP2_CRYPTO_LEVEL_INITIAL,
  /**
   * NGTCP2_CRYPTO_LEVEL_HANDSHAKE is Handshake Keys encryption level.
   */
  NGTCP2_CRYPTO_LEVEL_HANDSHAKE,
  /**
   * NGTCP2_CRYPTO_LEVEL_APP is Application Data (1-RTT) Keys
   * encryption level.
   */
  NGTCP2_CRYPTO_LEVEL_APP,
  /**
   * NGTCP2_CRYPTO_LEVEL_EARLY is Early Data (0-RTT) Keys encryption
   * level.
   */
  NGTCP2_CRYPTO_LEVEL_EARLY
} ngtcp2_crypto_level;

/**
 * @functypedef
 *
 * :type`ngtcp2_recv_crypto_data` is invoked when crypto data is
 * received.  The received data is pointed to by |data|, and its length
 * is |datalen|.  The |offset| specifies the offset where |data| is
 * positioned.  |user_data| is the arbitrary pointer passed to
 * `ngtcp2_conn_client_new` or `ngtcp2_conn_server_new`.  The ngtcp2
 * library ensures that the crypto data is passed to the application
 * in the increasing order of |offset|.  |datalen| is always strictly
 * greater than 0.  |crypto_level| indicates the encryption level
 * where this data is received.  Crypto data can never be received in
 * :enum:`NGTCP2_CRYPTO_LEVEL_EARLY`.
 *
 * The application should provide the given data to TLS stack.
 *
 * The callback function must return 0 if it succeeds.  If TLS stack
 * reported error, return :enum:`NGTCP2_ERR_CRYPTO`.  If application
 * encounters fatal error, return :enum:`NGTCP2_ERR_CALLBACK_FAILURE`
 * which makes the library call return immediately.  If the other
 * value is returned, it is treated as
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE`.
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * encryption key is passed as |key|.  The nonce is passed as |nonce|
 * of length |noncelen|.  The ad, Additional Data to AEAD, is passed
 * as |ad| of length |adlen|.
 *
 * The implementation of this callback must encrypt |plaintext| using
 * the negotiated cipher suite and write the ciphertext into the
 * buffer pointed by |dest|.  |dest| has enough capacity to store the
 * ciphertext.
 *
 * |dest| and |plaintext| may point to the same buffer.
 *
 * The callback function must return 0 if it succeeds, or
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
 * return immediately.
 */
typedef int (*ngtcp2_encrypt)(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                              const uint8_t *plaintext, size_t plaintextlen,
                              const uint8_t *key, const uint8_t *nonce,
                              size_t noncelen, const uint8_t *ad, size_t adlen);

/**
 * @functypedef
 *
 * :type:`ngtcp2_decrypt` is invoked when the ngtcp2 library asks the
 * application to decrypt packet payload.  The packet payload to
 * decrypt is passed as |ciphertext| of length |ciphertextlen|.  The
 * decryption key is passed as |key| of length |keylen|.  The nonce is
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
 * fails to decrypt data, return :enum:`NGTCP2_ERR_TLS_DECRYPT`.  For
 * any other errors, return :enum:`NGTCP2_ERR_CALLBACK_FAILURE` which
 * makes the library call return immediately.
 */
typedef int (*ngtcp2_decrypt)(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                              const uint8_t *ciphertext, size_t ciphertextlen,
                              const uint8_t *key, const uint8_t *nonce,
                              size_t noncelen, const uint8_t *ad, size_t adlen);

/**
 * @functypedef
 *
 * :type:`ngtcp2_hp_mask` is invoked when the ngtcp2 library asks the
 * application to produce mask to encrypt or decrypt packet header.
 * The key is passed as |hp_key|.  The sample is passed as |sample|.
 *
 * The implementation of this callback must produce a mask using the
 * header protection cipher suite specified by QUIC specification and
 * write the result into the buffer pointed by |dest|.  The length of
 * mask must be :macro:`NGTCP2_HP_MASKLEN`.  The library ensures that
 * |dest| has enough capacity.
 *
 * The callback function must return 0 if it succeeds, or
 *  :enum:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library call
 *  return immediately.
 */
typedef int (*ngtcp2_hp_mask)(uint8_t *dest, const ngtcp2_crypto_cipher *hp,
                              const uint8_t *hp_key, const uint8_t *sample);

/**
 * @enum
 *
 * ngtcp2_stream_data_flag defines the properties of the data emitted
 * via :type:`ngtcp2_recv_stream_data` callback function.
 */
typedef enum ngtcp2_stream_data_flag {
  NGTCP2_STREAM_DATA_FLAG_NONE = 0x00,
  /**
   * NGTCP2_STREAM_DATA_FLAG_FIN indicates that this chunk of data is
   * final piece of an incoming stream.
   */
  NGTCP2_STREAM_DATA_FLAG_FIN = 0x01,
  /**
   * NGTCP2_STREAM_DATA_FLAG_0RTT indicates that this chunk of data
   * contains data received in 0RTT packet and the handshake has not
   * completed yet, which means that the data might be replayed.
   */
  NGTCP2_STREAM_DATA_FLAG_0RTT = 0x02
} ngtcp2_stream_data_flag;

/**
 * @functypedef
 *
 * :type:`ngtcp2_recv_stream_data` is invoked when stream data is
 * received.  The stream is specified by |stream_id|.  |flags| is the
 * bitwise-OR of zero or more of ngtcp2_stream_data_flag.  If |flags|
 * & :enum:`NGTCP2_STREAM_DATA_FLAG_FIN` is nonzero, this portion of
 * the data is the last data in this stream.  |offset| is the offset
 * where this data begins.  The library ensures that data is passed to
 * the application in the non-decreasing order of |offset|.  The data
 * is passed as |data| of length |datalen|.  |datalen| may be 0 if and
 * only if |fin| is nonzero.
 *
 * The callback function must return 0 if it succeeds, or
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` which makes the library return
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
 * Returning :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
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
 * Returning :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
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
 * Returning :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
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
 * Returning :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
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
 * :enum:`NGTCP2_CRYPTO_LEVEL_EARLY`.  This works like
 * :type:`ngtcp2_acked_stream_data_offset` but crypto stream has no
 * stream_id and stream_user_data, and |datalen| never become 0.
 *
 * The implementation of this callback should return 0 if it succeeds.
 * Returning :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
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
 * Returning :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * bytes to the buffer pointed by |dest|.  |ctx| provides the context
 * how the provided random byte string is used.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_rand)(ngtcp2_conn *conn, uint8_t *dest, size_t destlen,
                           ngtcp2_rand_ctx ctx, void *user_data);

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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * materials.  The current set of secrets are given as
 * |current_rx_secret| and |current_tx_secret| of length |secretlen|.
 * They are decryption and encryption secrets respectively.
 *
 * The application has to generate new secrets and keys for both
 * encryption and decryption, and write decryption secret, key and IV
 * to the buffer pointed by |rx_secret|, |rx_key| and |rx_iv|
 * respectively.  Similarly, write encryption secret, key and IV to
 * the buffer pointed by |tx_secret|, |tx_key| and |tx_iv|.  All given
 * buffers have the enough capacity to store secret, key and IV.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_update_key)(ngtcp2_conn *conn, uint8_t *rx_secret,
                                 uint8_t *tx_secret, uint8_t *rx_key,
                                 uint8_t *rx_iv, uint8_t *tx_key,
                                 uint8_t *tx_iv,
                                 const uint8_t *current_rx_secret,
                                 const uint8_t *current_tx_secret,
                                 size_t secretlen, void *user_data);

/**
 * @functypedef
 *
 * :type:`ngtcp2_path_validation` is a callback function which tells
 * the application the outcome of path validation.  |path| is the path
 * that was validated.  If |res| is
 * :enum:`NGTCP2_PATH_VALIDATION_RESULT_SUCCESS`, the path validation
 * succeeded.  If |res| is
 * :enum:`NGTCP2_PATH_VALIDATION_RESULT_FAILURE`, the path validation
 * failed.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_select_preferred_addr)(ngtcp2_conn *conn,
                                            ngtcp2_addr *dest,
                                            const ngtcp2_preferred_addr *paddr,
                                            void *user_data);

typedef enum ngtcp2_connection_id_status_type {
  /* NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE indicates that a local
     endpoint starts using new destination Connection ID. */
  NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE,
  /* NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE indicates that a
     local endpoint stops using a given destination Connection ID. */
  NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE
} ngtcp2_connection_id_status_type;

/**
 * @functypedef
 *
 * :type:`ngtcp2_connection_id_status` is a callback function which is
 * called when the status of Connection ID changes.
 *
 * |token| is the associated stateless reset token and it is NULL if
 * no token is present.
 *
 * |type| is the one of the value defined in
 * :enum:`ngtcp2_connection_id_status_type`.  The new value might be
 * added in the future release.
 *
 * The callback function must return 0 if it succeeds.  Returning
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
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
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE` makes the library call return
 * immediately.
 */
typedef int (*ngtcp2_recv_new_token)(ngtcp2_conn *conn, const ngtcp2_vec *token,
                                     void *user_data);

typedef struct ngtcp2_conn_callbacks {
  /**
   * client_initial is a callback function which is invoked when
   * client asks TLS stack to produce first TLS cryptographic
   * handshake message.  This callback function must be specified.
   */
  ngtcp2_client_initial client_initial;
  /**
   * recv_client_initial is a callback function which is invoked when
   * a server receives the first packet from client.  This callback
   * function must be specified.
   */
  ngtcp2_recv_client_initial recv_client_initial;
  /**
   * recv_crypto_data is a callback function which is invoked when
   * cryptographic data (CRYPTO frame, in other words, TLS message) is
   * received.  This callback function must be specified.
   */
  ngtcp2_recv_crypto_data recv_crypto_data;
  /**
   * handshake_completed is a callback function which is invoked when
   * QUIC cryptographic handshake has completed.  This callback
   * function is optional.
   */
  ngtcp2_handshake_completed handshake_completed;
  /**
   * recv_version_negotiation is a callback function which is invoked
   * when Version Negotiation packet is received by a client.  This
   * callback function is optional.
   */
  ngtcp2_recv_version_negotiation recv_version_negotiation;
  /**
   * encrypt is a callback function which is invoked to encrypt a QUIC
   * packet.  This callback function must be specified.
   */
  ngtcp2_encrypt encrypt;
  /**
   * decrypt is a callback function which is invoked to decrypt a QUIC
   * packet.  This callback function must be specified.
   */
  ngtcp2_decrypt decrypt;
  /**
   * hp_mask is a callback function which is invoked to get a mask to
   * encrypt or decrypt packet header.  This callback function must be
   * specified.
   */
  ngtcp2_hp_mask hp_mask;
  /**
   * recv_stream_data is a callback function which is invoked when
   * STREAM data, which includes application data, is received.  This
   * callback function is optional.
   */
  ngtcp2_recv_stream_data recv_stream_data;
  /**
   * acked_crypto_offset is a callback function which is invoked when
   * CRYPTO data is acknowledged by a remote endpoint.  It tells an
   * application the largest offset of acknowledged CRYPTO data
   * without a gap so that the application can free memory for the
   * data.  This callback function is optional.
   */
  ngtcp2_acked_crypto_offset acked_crypto_offset;
  /**
   * acked_stream_data_offset is a callback function which is invoked
   * when STREAM data, which includes application data, is
   * acknowledged by a remote endpoint.  It tells an application the
   * largest offset of acknowledged STREAM data without a gap so that
   * application can free memory for the data.  This callback function
   * is optional.
   */
  ngtcp2_acked_stream_data_offset acked_stream_data_offset;
  /**
   * stream_open is a callback function which is invoked when new
   * remote stream is opened by a remote endpoint.  This callback
   * function is optional.
   */
  ngtcp2_stream_open stream_open;
  /**
   * stream_close is a callback function which is invoked when a
   * stream is closed.  This callback function is optional.
   */
  ngtcp2_stream_close stream_close;
  /**
   * recv_stateless_reset is a callback function which is invoked when
   * Stateless Reset packet is received.  This callback function is
   * optional.
   */
  ngtcp2_recv_stateless_reset recv_stateless_reset;
  /**
   * recv_retry is a callback function which is invoked when a client
   * receives Retry packet.  For client, this callback function must
   * be specified.  Server never receive Retry packet.
   */
  ngtcp2_recv_retry recv_retry;
  /**
   * extend_max_local_streams_bidi is a callback function which is
   * invoked when the number of bidirectional stream which a local
   * endpoint can open is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_streams extend_max_local_streams_bidi;
  /**
   * extend_max_local_streams_uni is a callback function which is
   * invoked when the number of unidirectional stream which a local
   * endpoint can open is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_streams extend_max_local_streams_uni;
  /**
   * rand is a callback function which is invoked when the library
   * needs unpredictable sequence of random data.  This callback
   * function must be specified.
   */
  ngtcp2_rand rand;
  /**
   * get_new_connection_id is a callback function which is invoked
   * when the library needs new connection ID.  This callback function
   * must be specified.
   */
  ngtcp2_get_new_connection_id get_new_connection_id;
  /**
   * remove_connection_id is a callback function which notifies an
   * application that connection ID is no longer used by a remote
   * endpoint.  This callback function is optional.
   */
  ngtcp2_remove_connection_id remove_connection_id;
  /**
   * update_key is a callback function which is invoked when the
   * library tells an application that it must update keying materials
   * and install new keys.  This function must be specified.
   */
  ngtcp2_update_key update_key;
  /**
   * path_validation is a callback function which is invoked when path
   * validation completed.  This function is optional.
   */
  ngtcp2_path_validation path_validation;
  /**
   * select_preferred_addr is a callback function which is invoked
   * when the library asks a client to select preferred address
   * presented by a server.  This function is optional.
   */
  ngtcp2_select_preferred_addr select_preferred_addr;
  /**
   * stream_reset is a callback function which is invoked when a
   * stream is reset by a remote endpoint.  This callback function is
   * optional.
   */
  ngtcp2_stream_reset stream_reset;
  /**
   * extend_max_remote_streams_bidi is a callback function which is
   * invoked when the number of bidirectional streams which a remote
   * endpoint can open is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_streams extend_max_remote_streams_bidi;
  /**
   * extend_max_remote_streams_uni is a callback function which is
   * invoked when the number of unidirectional streams which a remote
   * endpoint can open is increased.  This callback function is
   * optional.
   */
  ngtcp2_extend_max_streams extend_max_remote_streams_uni;
  /**
   * extend_max_stream_data is callback function which is invoked when
   * the maximum offset of STREAM data that a local endpoint can send
   * is increased.  This callback function is optional.
   */
  ngtcp2_extend_max_stream_data extend_max_stream_data;
  /**
   * dcid_status is a callback function which is invoked when the new
   * destination Connection ID is activated or the activated
   * destination Connection ID is now deactivated.
   */
  ngtcp2_connection_id_status dcid_status;
  /**
   * handshake_confirmed is a callback function which is invoked when
   * both endpoints agree that handshake has finished.  This field is
   * ignored by server because handshake_completed indicates the
   * handshake confirmation for server.
   */
  ngtcp2_handshake_confirmed handshake_confirmed;
  /**
   * recv_new_token is a callback function which is invoked when new
   * token is received from server.  This field is ignored by server.
   */
  ngtcp2_recv_new_token recv_new_token;
} ngtcp2_conn_callbacks;

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
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE
 *     Callback function failed.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_write_connection_close(
    uint8_t *dest, size_t destlen, const ngtcp2_cid *dcid,
    const ngtcp2_cid *scid, uint64_t error_code, ngtcp2_encrypt encrypt,
    const ngtcp2_crypto_aead *aead, const uint8_t *key, const uint8_t *iv,
    ngtcp2_hp_mask hp_mask, const ngtcp2_crypto_cipher *hp,
    const uint8_t *hp_key);

/**
 * @function
 *
 * `ngtcp2_pkt_write_retry` writes Retry packet in the buffer pointed
 * by |dest| whose length is |destlen|.  |odcid| specifies Original
 * Destination Connection ID.  |token| specifies Retry Token, and
 * |tokenlen| specifies its length.  |aead| must be AEAD_AES_128_GCM.
 *
 * This function returns the number of bytes written to the buffer, or
 * one of the following negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small.
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE
 *     Callback function failed.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_pkt_write_retry(
    uint8_t *dest, size_t destlen, const ngtcp2_cid *dcid,
    const ngtcp2_cid *scid, const ngtcp2_cid *odcid, const uint8_t *token,
    size_t tokenlen, ngtcp2_encrypt encrypt, const ngtcp2_crypto_aead *aead);

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
 * If |dest| is not NULL, and the return value is 0 or 1, the decoded
 * packet header is stored to the object pointed by |dest|.
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
 * connection is being established and must not be NULL.  |callbacks|,
 * and |settings| must not be NULL, and the function make a copy of
 * each of them.  |user_data| is the arbitrary pointer which is passed
 * to the user-defined callback functions.  If |mem| is NULL, the
 * memory allocator returned by `ngtcp2_mem_default()` is used.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_client_new(ngtcp2_conn **pconn, const ngtcp2_cid *dcid,
                       const ngtcp2_cid *scid, const ngtcp2_path *path,
                       uint32_t version, const ngtcp2_conn_callbacks *callbacks,
                       const ngtcp2_settings *settings, const ngtcp2_mem *mem,
                       void *user_data);

/**
 * @function
 *
 * `ngtcp2_conn_server_new` creates new :type:`ngtcp2_conn`, and
 * initializes it as server.  |dcid| is a destination connection ID.
 * |scid| is a source connection ID.  |path| is the network path where
 * this QUIC connection is being established and must not be NULL.
 * |version| is a QUIC version to use.  |callbacks|, and |settings|
 * must not be NULL, and the function make a copy of each of them.
 * |user_data| is the arbitrary pointer which is passed to the
 * user-defined callback functions.  If |mem| is NULL, the memory
 * allocator returned by `ngtcp2_mem_default()` is used.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_server_new(ngtcp2_conn **pconn, const ngtcp2_cid *dcid,
                       const ngtcp2_cid *scid, const ngtcp2_path *path,
                       uint32_t version, const ngtcp2_conn_callbacks *callbacks,
                       const ngtcp2_settings *settings, const ngtcp2_mem *mem,
                       void *user_data);

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
 * packet is delivered and must not be NULL.  This function performs
 * QUIC handshake as well.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * This function returns 0 if it succeeds, or negative error codes.
 * In general, if the error code which satisfies
 * ngtcp2_erro_is_fatal(err) != 0 is returned, the application should
 * just close the connection by calling
 * `ngtcp2_conn_write_connection_close` or just delete the QUIC
 * connection using `ngtcp2_conn_del`.  It is undefined to call the
 * other library functions.  If :enum:`NGTCP2_ERR_RETRY` is returned,
 * application must be a server and it must perform address validation
 * by sending Retry packet and close the connection.  If
 * :enum:`NGTCP2_ERR_DROP_CONN` is returned, server application must
 * drop the connection silently (without sending any CONNECTION_CLOSE
 * frame) and discard connection state.
 */
NGTCP2_EXTERN int ngtcp2_conn_read_pkt(ngtcp2_conn *conn,
                                       const ngtcp2_path *path,
                                       const uint8_t *pkt, size_t pktlen,
                                       ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_write_pkt` is equivalent to calling
 * `ngtcp2_conn_writev_stream` without specifying stream data and
 * :enum:`NGTCP2_WRITE_STREAM_FLAG_NONE` as flags.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_pkt(ngtcp2_conn *conn,
                                                 ngtcp2_path *path,
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
 * materials for Initial packets.  |rx_key| of length |keylen|, IV
 * |rx_iv| of length |rx_ivlen|, and packet header protection key
 * |rx_hp_key| of length |keylen| to decrypt incoming Initial packets.
 * Similarly, |tx_key|, |tx_iv| and |tx_hp_key| are for encrypt
 * outgoing packets and are the same length with the rx counterpart .
 * If they have already been set, they are overwritten.
 *
 * After receiving Retry packet, the DCID most likely changes.  In
 * that case, client application must generate these keying materials
 * again based on new DCID and install them again.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_initial_key(
    ngtcp2_conn *conn, const uint8_t *rx_key, const uint8_t *rx_iv,
    const uint8_t *rx_hp_key, const uint8_t *tx_key, const uint8_t *tx_iv,
    const uint8_t *tx_hp_key, size_t keylen, size_t ivlen);

/**
 * @function
 *
 * `ngtcp2_conn_install_rx_handshake_key` installs packet protection
 * keying materials for decrypting incoming Handshake packets.  |key|
 * of length |keylen|, IV |iv| of length |ivlen|, and packet header
 * protection key |hp_key| of length |keylen| to decrypt incoming
 * Handshake packets.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_install_rx_handshake_key(ngtcp2_conn *conn, const uint8_t *key,
                                     const uint8_t *iv, const uint8_t *hp_key,
                                     size_t keylen, size_t ivlen);

/**
 * @function
 *
 * `ngtcp2_conn_install_tx_handshake_key` installs packet protection
 * keying materials for encrypting outgoing Handshake packets.  |key|
 * of length |keylen|, IV |iv| of length |ivlen|, and packet header
 * protection key |hp_key| of length |keylen| to encrypt outgoing
 * Handshake packets.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_install_tx_handshake_key(ngtcp2_conn *conn, const uint8_t *key,
                                     const uint8_t *iv, const uint8_t *hp_key,
                                     size_t keylen, size_t ivlen);

/**
 * @function
 *
 * `ngtcp2_conn_set_aead_overhead` tells the ngtcp2 library the length
 * of AEAD tag which the negotiated cipher suites defines.  This
 * function must be called before encrypting or decrypting the
 * incoming packets other than Initial packets.
 */
NGTCP2_EXTERN void ngtcp2_conn_set_aead_overhead(ngtcp2_conn *conn,
                                                 size_t aead_overhead);

/**
 * @function
 *
 * `ngtcp2_conn_get_aead_overhead` returns the aead overhead passed to
 * `ngtcp2_conn_set_aead_overhead`. If `ngtcp2_conn_set_aead_overhead` hasn't
 * been called yet this function returns 0.
 */
NGTCP2_EXTERN size_t ngtcp2_conn_get_aead_overhead(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_install_early_key` installs packet protection key
 * |key| of length |keylen|, IV |iv| of length |ivlen|, and packet
 * header protection key |hp_key| of length |keylen| to encrypt (for
 * client)or decrypt (for server) 0RTT packets.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int ngtcp2_conn_install_early_key(ngtcp2_conn *conn,
                                                const uint8_t *key,
                                                const uint8_t *iv,
                                                const uint8_t *hp_key,
                                                size_t keylen, size_t ivlen);

/**
 * @function
 *
 * `ngtcp2_conn_install_rx_key` installs packet protection keying
 * materials for decrypting Short packets.  |secret| of length
 * |secretlen| is the decryption secret which is used to derive keying
 * materials passed to this function.  |key| of length |keylen|, IV
 * |iv| of length |ivlen|, and packet header protection key |hp_key|
 * of length |keylen| to decrypt incoming Short packets.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_install_rx_key(ngtcp2_conn *conn, const uint8_t *secret,
                           const uint8_t *key, const uint8_t *iv,
                           const uint8_t *hp_key, size_t secretlen,
                           size_t keylen, size_t ivlen);

/**
 * @function
 *
 * `ngtcp2_conn_install_tx_key` installs packet protection keying
 * materials for encrypting Short packets.  |secret| of length
 * |secretlen| is the encryption secret which is used to derive keying
 * materials passed to this function.  |key| of length |keylen|, IV
 * |iv| of length |ivlen|, and packet header protection key |hp_key|
 * of length |keylen| to encrypt outgoing Short packets.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory.
 */
NGTCP2_EXTERN int
ngtcp2_conn_install_tx_key(ngtcp2_conn *conn, const uint8_t *secret,
                           const uint8_t *key, const uint8_t *iv,
                           const uint8_t *hp_key, size_t secretlen,
                           size_t keylen, size_t ivlen);

/**
 * @function
 *
 * `ngtcp2_conn_initiate_key_update` initiates the key update.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_INVALID_STATE`
 *     The previous key update has not been confirmed yet; or key
 *     update is too frequent; or new keys are not available yet.
 */
NGTCP2_EXTERN int ngtcp2_conn_initiate_key_update(ngtcp2_conn *conn,
                                                  ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_set_tls_error` sets the TLS related error in |conn|.
 * In general, error code should be propagated via return value, but
 * sometimes ngtcp2 API is called inside callback function of TLS
 * stack and it does not allow to return ngtcp2 error code directly.
 * In this case, implementation can set the error code (e.g.,
 * NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM) using this function.
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
 * `ngtcp2_conn_loss_detection_expiry` returns the expiry time point
 * of loss detection timer.  Application should call
 * `ngtcp2_conn_on_loss_detection_timer` and `ngtcp2_conn_write_pkt`
 * (or `ngtcp2_conn_writev_stream`) when it expires.  It returns
 * UINT64_MAX if loss detection timer is not armed.
 */
NGTCP2_EXTERN ngtcp2_tstamp
ngtcp2_conn_loss_detection_expiry(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_ack_delay_expiry` returns the expiry time point of
 * delayed protected ACK.  Application should call
 * ngtcp2_conn_cancel_expired_ack_delay_timer() and
 * `ngtcp2_conn_write_pkt` (or `ngtcp2_conn_writev_stream`) when it
 * expires.  It returns UINT64_MAX if there is no expiry.
 */
NGTCP2_EXTERN ngtcp2_tstamp ngtcp2_conn_ack_delay_expiry(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_get_expiry` returns the next expiry time.  This
 * function returns the timestamp such that
 * min(ngtcp2_conn_loss_detection_expiry(conn),
 * ngtcp2_conn_ack_delay_expiry(conn), other timers in |conn|).
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
 * `ngtcp2_conn_cancel_expired_ack_delay_timer` stops expired ACK
 * delay timer.  |ts| is the current time.  This function must be
 * called when ngtcp2_conn_ack_delay_expiry() <= ts.
 */
NGTCP2_EXTERN void ngtcp2_conn_cancel_expired_ack_delay_timer(ngtcp2_conn *conn,
                                                              ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_get_idle_expiry` returns the time when a connection
 * should be closed if it continues to be idle.  If idle timeout is
 * disabled, this function returns UINT64_MAX.
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
 * :enum:`NGTCP2_ERR_PROTO`
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
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_STREAM_ID_BLOCKED`
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
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_STREAM_ID_BLOCKED`
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
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_STREAM_NOT_FOUND`
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
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_STREAM_NOT_FOUND`
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
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream does not exist
 */
NGTCP2_EXTERN int ngtcp2_conn_shutdown_stream_read(ngtcp2_conn *conn,
                                                   int64_t stream_id,
                                                   uint64_t app_error_code);

/**
 * @enum
 *
 * ngtcp2_write_stream_flag defines extra behaviour for
 * `ngtcp2_conn_writev_stream()`.
 */
typedef enum ngtcp2_write_stream_flag {
  NGTCP2_WRITE_STREAM_FLAG_NONE = 0x00,
  /**
   * NGTCP2_WRITE_STREAM_FLAG_MORE indicates that more stream data may
   * come and should be coalesced into the same packet if possible.
   */
  NGTCP2_WRITE_STREAM_FLAG_MORE = 0x01,
  /**
   * NGTCP2_WRITE_STREAM_FLAG_FIN indicates that the passed data is
   * the final part of a stream.
   */
  NGTCP2_WRITE_STREAM_FLAG_FIN = 0x02
} ngtcp2_write_stream_flag;

/**
 * @function
 *
 * `ngtcp2_conn_write_stream` is just like
 * `ngtcp2_conn_writev_stream`.  The only difference is that it
 * conveniently accepts a single buffer.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_stream(
    ngtcp2_conn *conn, ngtcp2_path *path, uint8_t *dest, size_t destlen,
    ngtcp2_ssize *pdatalen, uint32_t flags, int64_t stream_id,
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
 * If |path| is not NULL, this function stores the network path with
 * which the packet should be sent.  Each addr field must point to the
 * buffer which is at least 128 bytes.  ``sizeof(struct
 * sockaddr_storage)`` is enough.  The assignment might not be done if
 * nothing is written to |dest|.
 *
 * If the all given data is encoded as STREAM frame in |dest|, and if
 * |flags| & NGTCP2_WRITE_STREAM_FLAG_FIN is nonzero, fin flag is set
 * to outgoing STREAM frame.  Otherwise, fin flag in STREAM frame is
 * not set.
 *
 * This packet may contain frames other than STREAM frame.  The packet
 * might not contain STREAM frame if other frames occupy the packet.
 * In that case, |*pdatalen| would be -1 if |pdatalen| is not NULL.
 *
 * If |flags| & NGTCP2_WRITE_STREAM_FLAG_FIN is nonzero, and 0 length
 * STREAM frame is successfully serialized, |*pdatalen| would be 0.
 *
 * The number of data encoded in STREAM frame is stored in |*pdatalen|
 * if it is not NULL.  The caller must keep the portion of data
 * covered by |*pdatalen| bytes in tact until
 * :type:`ngtcp2_acked_stream_data_offset` indicates that they are
 * acknowledged by a remote endpoint or the stream is closed.
 *
 * If |flags| equals to :enum:`NGTCP2_WRITE_STREAM_FLAG_NONE`, this
 * function produces a single payload of UDP packet.  If the given
 * stream data is small (e.g., few bytes), the packet might be
 * severely under filled.  Too many small packet might increase
 * overall packet processing costs.  Unless there are retransmissions,
 * by default, application can only send 1 STREAM frame in one QUIC
 * packet.  In order to include more than 1 STREAM frame in one QUIC
 * packet, specify :enum:`NGTCP2_WRITE_STREAM_FLAG_MORE` in |flags|.
 * This is analogous to ``MSG_MORE`` flag in ``send(2)``.  If the
 * :enum:`NGTCP2_WRITE_STREAM_FLAG_MORE` is used, there are 4
 * outcomes:
 *
 * - The function returns the written length of packet just like
 *   without :enum:`NGTCP2_WRITE_STREAM_FLAG_MORE`.  This is because
 *   packet is nearly full and the library decided to make a complete
 *   packet.  In this case, |*pdatalen| == -1 is asserted.
 *
 * - The function returns :enum:`NGTCP2_ERR_WRITE_STREAM_MORE`.  In
 *   this case, |*pdatalen| >= 0 is asserted.  This indicates that
 *   application can call this function with different stream data to
 *   pack them into the same packet.  Application has to specify the
 *   same |conn|, |path|, |dest|, |destlen|, |pdatalen|, and |ts|
 *   parameters, otherwise the behaviour is undefined.  The
 *   application can change |flags|.
 *
 * - The function returns :enum:`NGTCP2_ERR_STREAM_DATA_BLOCKED` which
 *   indicates that stream is blocked because of flow control.
 *
 * - The other error might be returned just like without
 *   :enum:`NGTCP2_WRITE_STREAM_FLAG_MORE`.
 *
 * When application sees :enum:`NGTCP2_ERR_WRITE_STREAM_MORE`, it must
 * not call other ngtcp2 API functions (application can still call
 * `ngtcp2_conn_write_connection_close` or
 * `ngtcp2_conn_write_application_close` to handle error from this
 * function).  Just keep calling `ngtcp2_conn_writev_stream` or
 * `ngtcp2_conn_write_pkt` until it returns a positive number (which
 * indicates a complete packet is ready).  If |*pdatalen| >= 0, the
 * function always return :enum:`NGTCP2_ERR_WRITE_STREAM_MORE`.
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
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_STREAM_NOT_FOUND`
 *     Stream does not exist
 * :enum:`NGTCP2_ERR_STREAM_SHUT_WR`
 *     Stream is half closed (local); or stream is being reset.
 * :enum:`NGTCP2_ERR_PKT_NUM_EXHAUSTED`
 *     Packet number is exhausted, and cannot send any more packet.
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     User callback failed
 * :enum:`NGTCP2_ERR_STREAM_DATA_BLOCKED`
 *     Stream is blocked because of flow control.
 * :enum:`NGTCP2_ERR_WRITE_STREAM_MORE`
 *     (Only when :enum:`NGTCP2_WRITE_STREAM_FLAG_MORE` is specified)
 *     Application can call this function to pack more stream data
 *     into the same packet.  See above to know how it works.
 *
 * In general, if the error code which satisfies
 * ngtcp2_err_is_fatal(err) != 0 is returned, the application should
 * just close the connection by calling
 * `ngtcp2_conn_write_connection_close` or just delete the QUIC
 * connection using `ngtcp2_conn_del`.  It is undefined to call the
 * other library functions.
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_writev_stream(
    ngtcp2_conn *conn, ngtcp2_path *path, uint8_t *dest, size_t destlen,
    ngtcp2_ssize *pdatalen, uint32_t flags, int64_t stream_id,
    const ngtcp2_vec *datav, size_t datavcnt, ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_write_connection_close` writes a packet which contains
 * a CONNECTION_CLOSE frame in the buffer pointed by |dest| whose
 * capacity is |datalen|.
 *
 * If |path| is not NULL, this function stores the network path with
 * which the packet should be sent.  Each addr field must point to the
 * buffer which is at least 128 bytes.  ``sizeof(struct
 * sockaddr_storage)`` is enough.  The assignment might not be done if
 * nothing is written to |dest|.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * At the moment, successful call to this function makes connection
 * close.  We may change this behaviour in the future to allow
 * graceful shutdown.
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small
 * :enum:`NGTCP2_ERR_INVALID_STATE`
 *     The current state does not allow sending CONNECTION_CLOSE.
 * :enum:`NGTCP2_ERR_PKT_NUM_EXHAUSTED`
 *     Packet number is exhausted, and cannot send any more packet.
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     User callback failed
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_connection_close(
    ngtcp2_conn *conn, ngtcp2_path *path, uint8_t *dest, size_t destlen,
    uint64_t error_code, ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_write_application_close` writes a packet which
 * contains a APPLICATION_CLOSE frame in the buffer pointed by |dest|
 * whose capacity is |datalen|.
 *
 * If |path| is not NULL, this function stores the network path with
 * which the packet should be sent.  Each addr field must point to the
 * buffer which is at least 128 bytes.  ``sizeof(struct
 * sockaddr_storage)`` is enough.  The assignment might not be done if
 * nothing is written to |dest|.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * At the moment, successful call to this function makes connection
 * close.  We may change this behaviour in the future to allow
 * graceful shutdown.
 *
 * :enum:`NGTCP2_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGTCP2_ERR_NOBUF`
 *     Buffer is too small
 * :enum:`NGTCP2_ERR_INVALID_STATE`
 *     The current state does not allow sending APPLICATION_CLOSE.
 * :enum:`NGTCP2_ERR_PKT_NUM_EXHAUSTED`
 *     Packet number is exhausted, and cannot send any more packet.
 * :enum:`NGTCP2_ERR_CALLBACK_FAILURE`
 *     User callback failed
 */
NGTCP2_EXTERN ngtcp2_ssize ngtcp2_conn_write_application_close(
    ngtcp2_conn *conn, ngtcp2_path *path, uint8_t *dest, size_t destlen,
    uint64_t app_error_code, ngtcp2_tstamp ts);

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
 * :enum:`NGTCP2_ERR_STREAM_NOT_FOUND`
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
  /* seq is the sequence number of this Connection ID. */
  uint64_t seq;
  /* cid is Connection ID. */
  ngtcp2_cid cid;
  /* ps is the path which is associated to this Connection ID. */
  ngtcp2_path_storage ps;
  /* token is the stateless reset token for this Connection ID. */
  uint8_t token[NGTCP2_STATELESS_RESET_TOKENLEN];
  /* token_resent is nonzero if token contains stateless reset
     token. */
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
 * `ngtcp2_conn_write_stream` if it has data to send) to send TLP/RTO
 * probe packets.
 *
 * This function must not be called from inside the callback
 * functions.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_NOMEM`
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
 * :enum:`NGTCP2_ERR_NOMEM`
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
 * `ngtcp2_conn_get_remote_addr` returns the remote endpoint address
 * set in |conn|.
 */
NGTCP2_EXTERN const ngtcp2_addr *ngtcp2_conn_get_remote_addr(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_initiate_migration` starts connection migration to the
 * given |path| which must not be NULL.  Only client can initiate
 * migration.  This function does immediate migration; it does not
 * probe peer reachability from a new local address.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGTCP2_ERR_INVALID_STATE`
 *     Migration is disabled.
 * :enum:`NGTCP2_ERR_CONN_ID_BLOCKED`
 *     No unused connection ID is available.
 * :enum:`NGTCP2_ERR_INVALID_ARGUMENT`
 *     |path| equals the current path.
 * :enum:`NGTCP2_ERR_NOMEM`
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
 * `ngtcp2_conn_set_initial_crypto_ctx` sets |ctx| for
 * 0RTT/Handshake/Short packet encryption.  In other words, this
 * crypto context is used for all packets except for Initial packets.
 * The passed data will be passed to :type:`ngtcp2_encrypt`,
 * :type:`ngtcp2_decrypt` and :type:`ngtcp2_hp_mask` callbacks.
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
 * `ngtcp2_conn_set_retry_aead` sets |aead| for Retry integrity tag
 * verification.  It must be AEAD_AES_128_GCM.  This function must be
 * called if |conn| is initialized as client.  Server does not verify
 * the tag and has no need to call this function.
 */
NGTCP2_EXTERN void ngtcp2_conn_set_retry_aead(ngtcp2_conn *conn,
                                              const ngtcp2_crypto_aead *aead);

/**
 * @function
 *
 * `ngtcp2_conn_get_crypto_ctx` returns :type:`ngtcp2_crypto_ctx`
 * object for 0RTT/Handshake/Short packet encryption.
 */
NGTCP2_EXTERN const ngtcp2_crypto_ctx *
ngtcp2_conn_get_crypto_ctx(ngtcp2_conn *conn);

typedef enum ngtcp2_connection_close_error_code_type {
  /* NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT indicates the
     error code is QUIC transport error code. */
  NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT,
  /* NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION indicates the
     error code is application error code. */
  NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION,
} ngtcp2_connection_close_error_code_type;

typedef struct ngtcp2_connection_close_error_code {
  /* error_code is the error code for connection closure. */
  uint64_t error_code;
  /* type is the type of error_code. */
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
 * `ngtcp2_strerror` returns the text representation of |liberr|.
 */
NGTCP2_EXTERN const char *ngtcp2_strerror(int liberr);

/**
 * @function
 *
 * `ngtcp2_err_is_fatal` returns nonzero if |liberr| is a fatal error.
 */
NGTCP2_EXTERN int ngtcp2_err_is_fatal(int liberr);

/**
 * @function
 *
 * `ngtcp2_err_infer_quic_transport_error_code` returns a QUIC
 * transport error code which corresponds to |liberr|.
 */
NGTCP2_EXTERN uint64_t ngtcp2_err_infer_quic_transport_error_code(int liberr);

/**
 * @function
 *
 * `ngtcp2_addr_init` initializes |dest| with the given arguments and
 * returns |dest|.
 */
NGTCP2_EXTERN ngtcp2_addr *ngtcp2_addr_init(ngtcp2_addr *dest, const void *addr,
                                            size_t addrlen, void *user_data);

/**
 * @function
 *
 * `ngtcp2_path_storage_init` initializes |ps| with the given
 * arguments.  This function copies |local_addr| and |remote_addr|.
 */
NGTCP2_EXTERN void
ngtcp2_path_storage_init(ngtcp2_path_storage *ps, const void *local_addr,
                         size_t local_addrlen, void *local_user_data,
                         const void *remote_addr, size_t remote_addrlen,
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
 * * cc_algo = NGTCP2_CC_ALGO_CUBIC
 * * transport_params.max_udp_payload_size = NGTCP2_DEFAULT_MAX_UDP_PAYLOAD_SIZE
 * * transport_params.ack_delay_component = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT
 * * transport_params.max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY
 * * transport_params.active_connection_id_limit =
 *   NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT
 */
NGTCP2_EXTERN void ngtcp2_settings_default(ngtcp2_settings *settings);

/*
 * @function
 *
 * `ngtcp2_mem_default` returns the default, system standard memory
 * allocator.
 */
NGTCP2_EXTERN const ngtcp2_mem *ngtcp2_mem_default(void);

/**
 * @macro
 *
 * The age of :type:`ngtcp2_info`
 */
#define NGTCP2_VERSION_AGE 1

/**
 * @struct
 *
 * This struct is what `ngtcp2_version()` returns.  It holds
 * information about the particular ngtcp2 version.
 */
typedef struct ngtcp2_info {
  /**
   * Age of this struct.  This instance of ngtcp2 sets it to
   * :macro:`NGTCP2_VERSION_AGE` but a future version may bump it and
   * add more struct fields at the bottom
   */
  int age;
  /**
   * the :macro:`NGTCP2_VERSION_NUM` number (since age ==1)
   */
  int version_num;
  /**
   * points to the :macro:`NGTCP2_VERSION` string (since age ==1)
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

typedef enum {
  NGTCP2_LOG_EVENT_NONE,
  /* connection (catch-all) event */
  NGTCP2_LOG_EVENT_CON,
  /* packet event */
  NGTCP2_LOG_EVENT_PKT,
  /* frame event */
  NGTCP2_LOG_EVENT_FRM,
  /* recovery event */
  NGTCP2_LOG_EVENT_RCV,
  /* crypto event */
  NGTCP2_LOG_EVENT_CRY,
  /* path validation event */
  NGTCP2_LOG_EVENT_PTV,
} ngtcp2_log_event;

/**
 * @function
 *
 * `ngtcp2_log_info` writes info level log.
 */
NGTCP2_EXTERN void ngtcp2_log_info(ngtcp2_log *log, ngtcp2_log_event ev,
                                   const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* NGTCP2_H */
