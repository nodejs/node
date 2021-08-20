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
#ifndef NGTCP2_CONN_H
#define NGTCP2_CONN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2.h>

#include "ngtcp2_mem.h"
#include "ngtcp2_crypto.h"
#include "ngtcp2_acktr.h"
#include "ngtcp2_rtb.h"
#include "ngtcp2_strm.h"
#include "ngtcp2_mem.h"
#include "ngtcp2_idtr.h"
#include "ngtcp2_str.h"
#include "ngtcp2_pkt.h"
#include "ngtcp2_log.h"
#include "ngtcp2_pq.h"
#include "ngtcp2_cc.h"
#include "ngtcp2_pv.h"
#include "ngtcp2_cid.h"
#include "ngtcp2_buf.h"
#include "ngtcp2_ppe.h"
#include "ngtcp2_qlog.h"
#include "ngtcp2_rst.h"

typedef enum {
  /* Client specific handshake states */
  NGTCP2_CS_CLIENT_INITIAL,
  NGTCP2_CS_CLIENT_WAIT_HANDSHAKE,
  NGTCP2_CS_CLIENT_TLS_HANDSHAKE_FAILED,
  /* Server specific handshake states */
  NGTCP2_CS_SERVER_INITIAL,
  NGTCP2_CS_SERVER_WAIT_HANDSHAKE,
  NGTCP2_CS_SERVER_TLS_HANDSHAKE_FAILED,
  /* Shared by both client and server */
  NGTCP2_CS_POST_HANDSHAKE,
  NGTCP2_CS_CLOSING,
  NGTCP2_CS_DRAINING,
} ngtcp2_conn_state;

/* NGTCP2_MAX_STREAMS is the maximum number of streams. */
#define NGTCP2_MAX_STREAMS (1LL << 60)

/* NGTCP2_MAX_NUM_BUFFED_RX_PKTS is the maximum number of buffered
   reordered packets. */
#define NGTCP2_MAX_NUM_BUFFED_RX_PKTS 4

/* NGTCP2_MAX_REORDERED_CRYPTO_DATA is the maximum offset of crypto
   data which is not continuous.  In other words, there is a gap of
   unreceived data. */
#define NGTCP2_MAX_REORDERED_CRYPTO_DATA 65536

/* NGTCP2_MAX_RX_INITIAL_CRYPTO_DATA is the maximum offset of received
   crypto stream in Initial packet.  We set this hard limit here
   because crypto stream is unbounded. */
#define NGTCP2_MAX_RX_INITIAL_CRYPTO_DATA 65536
/* NGTCP2_MAX_RX_HANDSHAKE_CRYPTO_DATA is the maximum offset of
   received crypto stream in Handshake packet.  We set this hard limit
   here because crypto stream is unbounded. */
#define NGTCP2_MAX_RX_HANDSHAKE_CRYPTO_DATA 65536

/* NGTCP2_MAX_RETRIES is the number of Retry packet which client can
   accept. */
#define NGTCP2_MAX_RETRIES 3

/* NGTCP2_MAX_BOUND_DCID_POOL_SIZE is the maximum number of
   destination connection ID which have been bound to a particular
   path, but not yet used as primary path and path validation is not
   performed from the local endpoint. */
#define NGTCP2_MAX_BOUND_DCID_POOL_SIZE 4
/* NGTCP2_MAX_DCID_POOL_SIZE is the maximum number of destination
   connection ID the remote endpoint provides to store.  It must be
   the power of 2. */
#define NGTCP2_MAX_DCID_POOL_SIZE 8
/* NGTCP2_MAX_DCID_RETIRED_SIZE is the maximum number of retired DCID
   kept to catch in-flight packet on retired path. */
#define NGTCP2_MAX_DCID_RETIRED_SIZE 2
/* NGTCP2_MAX_SCID_POOL_SIZE is the maximum number of source
   connection ID the local endpoint provides to the remote endpoint.
   The chosen value was described in old draft.  Now a remote endpoint
   tells the maximum value.  The value can be quite large, and we have
   to put the sane limit.*/
#define NGTCP2_MAX_SCID_POOL_SIZE 8

/* NGTCP2_MAX_NON_ACK_TX_PKT is the maximum number of continuous non
   ACK-eliciting packets. */
#define NGTCP2_MAX_NON_ACK_TX_PKT 3

/* NGTCP2_ECN_MAX_NUM_VALIDATION_PKTS is the maximum number of ECN marked
   packets sent in NGTCP2_ECN_STATE_TESTING period. */
#define NGTCP2_ECN_MAX_NUM_VALIDATION_PKTS 10

/*
 * ngtcp2_max_frame is defined so that it covers the largest ACK
 * frame.
 */
typedef union ngtcp2_max_frame {
  ngtcp2_frame fr;
  struct {
    ngtcp2_ack ack;
    /* ack includes 1 ngtcp2_ack_blk. */
    ngtcp2_ack_blk blks[NGTCP2_MAX_ACK_BLKS - 1];
  } ackfr;
} ngtcp2_max_frame;

typedef struct ngtcp2_path_challenge_entry {
  ngtcp2_path_storage ps;
  uint8_t data[8];
} ngtcp2_path_challenge_entry;

void ngtcp2_path_challenge_entry_init(ngtcp2_path_challenge_entry *pcent,
                                      const ngtcp2_path *path,
                                      const uint8_t *data);

/* NGTCP2_CONN_FLAG_NONE indicates that no flag is set. */
#define NGTCP2_CONN_FLAG_NONE 0x00
/* NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED is set if handshake
   completed. */
#define NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED 0x01
/* NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED is set if connection ID is
   negotiated.  This is only used for client. */
#define NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED 0x02
/* NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED is set if transport
   parameters are received. */
#define NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED 0x04
/* NGTCP2_CONN_FLAG_RECV_PROTECTED_PKT is set when a protected packet
   is received, and decrypted successfully.  This flag is used to stop
   retransmitting handshake packets.  It might be replaced with an
   another mechanism when we implement key update. */
#define NGTCP2_CONN_FLAG_RECV_PROTECTED_PKT 0x08
/* NGTCP2_CONN_FLAG_RECV_RETRY is set when a client receives Retry
   packet. */
#define NGTCP2_CONN_FLAG_RECV_RETRY 0x10
/* NGTCP2_CONN_FLAG_EARLY_DATA_REJECTED is set when 0-RTT packet is
   rejected by a peer. */
#define NGTCP2_CONN_FLAG_EARLY_DATA_REJECTED 0x20
/* NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED is set when an endpoint
   confirmed completion of handshake. */
#define NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED 0x80
/* NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED is set when the
   library transitions its state to "post handshake". */
#define NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED 0x0100
/* NGTCP2_CONN_FLAG_HANDSHAKE_EARLY_RETRANSMIT is set when the early
   handshake retransmission has done when server receives overlapping
   Initial crypto data. */
#define NGTCP2_CONN_FLAG_HANDSHAKE_EARLY_RETRANSMIT 0x0200
/* NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED is set when key update is
   not confirmed by the local endpoint.  That is, it has not received
   ACK frame which acknowledges packet which is encrypted with new
   key. */
#define NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED 0x0800
/* NGTCP2_CONN_FLAG_PPE_PENDING is set when
   NGTCP2_WRITE_STREAM_FLAG_MORE is used and the intermediate state of
   ngtcp2_ppe is stored in pkt struct of ngtcp2_conn. */
#define NGTCP2_CONN_FLAG_PPE_PENDING 0x1000
/* NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE is set when idle timer
   should be restarted on next write. */
#define NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE 0x2000
/* NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED indicates that server as peer
   verified client address.  This flag is only used by client. */
#define NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED 0x4000

typedef struct ngtcp2_crypto_data {
  ngtcp2_buf buf;
  /* pkt_type is the type of packet to send data in buf.  If it is 0,
     it must be sent in Short packet.  Otherwise, it is sent the long
     packet type denoted by pkt_type. */
  uint8_t pkt_type;
} ngtcp2_crypto_data;

typedef struct ngtcp2_pktns {
  struct {
    /* last_pkt_num is the packet number which the local endpoint sent
       last time.*/
    int64_t last_pkt_num;
    ngtcp2_frame_chain *frq;
    /* num_non_ack_pkt is the number of continuous non ACK-eliciting
       packets. */
    size_t num_non_ack_pkt;

    struct {
      /* ect0 is the number of QUIC packets, not UDP datagram, which
         are sent in UDP datagram with ECT0 marking. */
      size_t ect0;
      /* start_pkt_num is the lowest packet number that are sent
         during ECN validation period. */
      int64_t start_pkt_num;
      /* validation_pkt_sent is the number of QUIC packets sent during
         validation period. */
      size_t validation_pkt_sent;
      /* validation_pkt_lost is the number of QUIC packets lost during
         validation period. */
      size_t validation_pkt_lost;
    } ecn;
  } tx;

  struct {
    /* pngap tracks received packet number in order to suppress
       duplicated packet number. */
    ngtcp2_gaptr pngap;
    /* max_pkt_num is the largest packet number received so far. */
    int64_t max_pkt_num;
    /* max_pkt_ts is the timestamp when max_pkt_num packet is
       received. */
    ngtcp2_tstamp max_pkt_ts;
    /* max_ack_eliciting_pkt_num is the largest ack-eliciting packet
       number received so far. */
    int64_t max_ack_eliciting_pkt_num;
    /*
     * buffed_pkts is buffered packets which cannot be decrypted with
     * the current encryption level.
     *
     * In server Initial encryption level, 0-RTT packet may be buffered.
     * In server Handshake encryption level, Short packet may be buffered.
     *
     * In client Initial encryption level, Handshake or Short packet may
     * be buffered.  In client Handshake encryption level, Short packet
     * may be buffered.
     *
     * - 0-RTT packet is only buffered in server Initial encryption
     *   level ngtcp2_pktns.
     *
     * - Handshake packet is only buffered in client Handshake
     *   encryption level ngtcp2_pktns.
     *
     * - Short packet is only buffered in Short encryption level
     *   ngtcp2_pktns.
     */
    ngtcp2_pkt_chain *buffed_pkts;

    struct {
      /* ect0, ect1, and ce are the number of QUIC packets received
         with those markings. */
      size_t ect0;
      size_t ect1;
      size_t ce;
      struct {
        /* ect0, ect1, ce are the ECN counts received in the latest
           ACK frame. */
        size_t ect0;
        size_t ect1;
        size_t ce;
      } ack;
    } ecn;
  } rx;

  struct {
    struct {
      /* frq contains crypto data sorted by their offset. */
      ngtcp2_ksl frq;
      /* offset is the offset of crypto stream in this packet number
         space. */
      uint64_t offset;
      /* ckm is a cryptographic key, and iv to encrypt outgoing
         packets. */
      ngtcp2_crypto_km *ckm;
      /* hp_ctx is cipher context for packet header protection. */
      ngtcp2_crypto_cipher_ctx hp_ctx;
    } tx;

    struct {
      /* ckm is a cryptographic key, and iv to decrypt incoming
         packets. */
      ngtcp2_crypto_km *ckm;
      /* hp_ctx is cipher context for packet header protection. */
      ngtcp2_crypto_cipher_ctx hp_ctx;
    } rx;

    ngtcp2_strm strm;
    ngtcp2_crypto_ctx ctx;
  } crypto;

  ngtcp2_acktr acktr;
  ngtcp2_rtb rtb;
} ngtcp2_pktns;

typedef enum ngtcp2_ecn_state {
  NGTCP2_ECN_STATE_TESTING,
  NGTCP2_ECN_STATE_UNKNOWN,
  NGTCP2_ECN_STATE_FAILED,
  NGTCP2_ECN_STATE_CAPABLE,
} ngtcp2_ecn_state;

struct ngtcp2_conn {
  ngtcp2_conn_state state;
  ngtcp2_callbacks callbacks;
  /* rcid is a connection ID present in Initial or 0-RTT packet from
     client as destination connection ID.  Server uses this field to
     check that duplicated Initial or 0-RTT packet are indeed sent to
     this connection.  It is also sent to client as
     original_destination_connection_id transport parameter.  Client
     uses this field to validate original_destination_connection_id
     transport parameter if no Retry packet is involved. */
  ngtcp2_cid rcid;
  /* oscid is the source connection ID initially used by the local
     endpoint. */
  ngtcp2_cid oscid;
  /* retry_scid is the source connection ID from Retry packet.  Client
     records it in order to verify retry_source_connection_id
     transport parameter.  Server does not use this field. */
  ngtcp2_cid retry_scid;
  ngtcp2_pktns *in_pktns;
  ngtcp2_pktns *hs_pktns;
  ngtcp2_pktns pktns;

  struct {
    /* current is the current destination connection ID. */
    ngtcp2_dcid current;
    /* bound is a set of destination connection IDs which are bound to
       particular paths.  These paths are not validated yet. */
    ngtcp2_ringbuf bound;
    /* unused is a set of unused CID received from peer. */
    ngtcp2_ringbuf unused;
    /* retired is a set of CID retired by local endpoint.  Keep them
       in 3*PTO to catch packets in flight along the old path. */
    ngtcp2_ringbuf retired;
    /* seqgap tracks received sequence numbers in order to ignore
       retransmitted duplicated NEW_CONNECTION_ID frame. */
    ngtcp2_gaptr seqgap;
    /* retire_prior_to is the largest retire_prior_to received so
       far. */
    uint64_t retire_prior_to;
    /* num_retire_queued is the number of RETIRE_CONNECTION_ID frames
       queued for transmission. */
    size_t num_retire_queued;
    /* zerolen_seq is a pseudo sequence number of zero-length
       Destination Connection ID in order to distinguish between
       them. */
    uint64_t zerolen_seq;
  } dcid;

  struct {
    /* set is a set of CID sent to peer.  The peer can use any CIDs in
       this set.  This includes used CID as well as unused ones. */
    ngtcp2_ksl set;
    /* used is a set of CID used by peer.  The sort function of this
       priority queue takes timestamp when CID is retired and sorts
       them in ascending order. */
    ngtcp2_pq used;
    /* last_seq is the last sequence number of connection ID. */
    uint64_t last_seq;
    /* num_retired is the number of retired Connection ID still
       included in set. */
    size_t num_retired;
  } scid;

  struct {
    /* strmq contains ngtcp2_strm which has frames to send. */
    ngtcp2_pq strmq;
    /* ack is ACK frame.  The underlying buffer is reused. */
    ngtcp2_frame *ack;
    /* max_ack_blks is the number of additional ngtcp2_ack_blk which
       ack can contain. */
    size_t max_ack_blks;
    /* offset is the offset the local endpoint has sent to the remote
       endpoint. */
    uint64_t offset;
    /* max_offset is the maximum offset that local endpoint can
       send. */
    uint64_t max_offset;
    /* last_max_data_ts is the timestamp when last MAX_DATA frame is
       sent. */
    ngtcp2_tstamp last_max_data_ts;

    struct {
      /* state is the state of ECN validation */
      ngtcp2_ecn_state state;
      /* validation_start_ts is the timestamp when ECN validation is
         started.  It is UINT64_MAX if it has not started yet. */
      ngtcp2_tstamp validation_start_ts;
      /* dgram_sent is the number of UDP datagram sent during ECN
         validation period. */
      size_t dgram_sent;
    } ecn;
  } tx;

  struct {
    /* unsent_max_offset is the maximum offset that remote endpoint
       can send without extending MAX_DATA.  This limit is not yet
       notified to the remote endpoint. */
    uint64_t unsent_max_offset;
    /* offset is the cumulative sum of stream data received for this
       connection. */
    uint64_t offset;
    /* max_offset is the maximum offset that remote endpoint can
       send. */
    uint64_t max_offset;
    /* window is the connection-level flow control window size. */
    uint64_t window;
    /* path_challenge stores received PATH_CHALLENGE data. */
    ngtcp2_ringbuf path_challenge;
    /* ccec is the received connection close error code. */
    ngtcp2_connection_close_error_code ccec;
  } rx;

  struct {
    ngtcp2_crypto_km *ckm;
    ngtcp2_crypto_cipher_ctx hp_ctx;
    ngtcp2_crypto_ctx ctx;
    /* discard_started_ts is the timestamp when the timer to discard
       early key has started.  Used by server only. */
    ngtcp2_tstamp discard_started_ts;
  } early;

  struct {
    ngtcp2_settings settings;
    /* transport_params is the local transport parameters.  It is used
       for Short packet only. */
    ngtcp2_transport_params transport_params;
    struct {
      /* max_streams is the maximum number of bidirectional streams which
         the local endpoint can open. */
      uint64_t max_streams;
      /* next_stream_id is the bidirectional stream ID which the local
         endpoint opens next. */
      int64_t next_stream_id;
    } bidi;

    struct {
      /* max_streams is the maximum number of unidirectional streams
         which the local endpoint can open. */
      uint64_t max_streams;
      /* next_stream_id is the unidirectional stream ID which the
         local endpoint opens next. */
      int64_t next_stream_id;
    } uni;
  } local;

  struct {
    /* transport_params is the received transport parameters during
       handshake.  It is used for Short packet only. */
    ngtcp2_transport_params transport_params;
    /* pending_transport_params is received transport parameters
       during handshake.  It is copied to transport_params when 1RTT
       key is available. */
    ngtcp2_transport_params pending_transport_params;
    struct {
      ngtcp2_idtr idtr;
      /* unsent_max_streams is the maximum number of streams of peer
         initiated bidirectional stream which the local endpoint can
         accept.  This limit is not yet notified to the remote
         endpoint. */
      uint64_t unsent_max_streams;
      /* max_streams is the maximum number of streams of peer
         initiated bidirectional stream which the local endpoint can
         accept. */
      uint64_t max_streams;
    } bidi;

    struct {
      ngtcp2_idtr idtr;
      /* unsent_max_streams is the maximum number of streams of peer
         initiated unidirectional stream which the local endpoint can
         accept.  This limit is not yet notified to the remote
         endpoint. */
      uint64_t unsent_max_streams;
      /* max_streams is the maximum number of streams of peer
         initiated unidirectional stream which the local endpoint can
         accept. */
      uint64_t max_streams;
    } uni;
  } remote;

  struct {
    struct {
      /* new_tx_ckm is a new sender 1RTT key which has not been
         used. */
      ngtcp2_crypto_km *new_tx_ckm;
      /* new_rx_ckm is a new receiver 1RTT key which has not
         successfully decrypted incoming packet yet. */
      ngtcp2_crypto_km *new_rx_ckm;
      /* old_rx_ckm is an old receiver 1RTT key. */
      ngtcp2_crypto_km *old_rx_ckm;
      /* confirmed_ts is the time instant when the key update is
         confirmed by the local endpoint last time.  UINT64_MAX means
         undefined value. */
      ngtcp2_tstamp confirmed_ts;
    } key_update;

    /* tls_native_handle is a native handle to TLS session object. */
    void *tls_native_handle;
    /* decrypt_hp_buf is a buffer which is used to write unprotected
       packet header. */
    ngtcp2_vec decrypt_hp_buf;
    /* decrypt_buf is a buffer which is used to write decrypted data. */
    ngtcp2_vec decrypt_buf;
    /* retry_aead is AEAD to verify Retry packet integrity.  It is
       used by client only. */
    ngtcp2_crypto_aead retry_aead;
    /* retry_aead_ctx is AEAD cipher context to verify Retry packet
       integrity.  It is used by client only. */
    ngtcp2_crypto_aead_ctx retry_aead_ctx;
    /* tls_error is TLS related error. */
    int tls_error;
    /* decryption_failure_count is the number of received packets that
       fail authentication. */
    uint64_t decryption_failure_count;
  } crypto;

  /* pkt contains the packet intermediate construction data to support
     NGTCP2_WRITE_STREAM_FLAG_MORE */
  struct {
    ngtcp2_crypto_cc cc;
    ngtcp2_pkt_hd hd;
    ngtcp2_ppe ppe;
    ngtcp2_frame_chain **pfrc;
    int pkt_empty;
    int hd_logged;
    uint8_t rtb_entry_flags;
    int was_client_initial;
    ngtcp2_ssize hs_spktlen;
  } pkt;

  ngtcp2_map strms;
  ngtcp2_conn_stat cstat;
  ngtcp2_pv *pv;
  ngtcp2_log log;
  ngtcp2_qlog qlog;
  ngtcp2_rst rst;
  ngtcp2_cc_algo cc_algo;
  ngtcp2_cc cc;
  const ngtcp2_mem *mem;
  /* idle_ts is the time instant when idle timer started. */
  ngtcp2_tstamp idle_ts;
  void *user_data;
  uint32_t version;
  /* flags is bitwise OR of zero or more of NGTCP2_CONN_FLAG_*. */
  uint16_t flags;
  int server;
};

typedef enum ngtcp2_vmsg_type {
  NGTCP2_VMSG_TYPE_STREAM,
  NGTCP2_VMSG_TYPE_DATAGRAM,
} ngtcp2_vmsg_type;

typedef struct ngtcp2_vmsg_stream {
  /* strm is a stream that data is sent to. */
  ngtcp2_strm *strm;
  /* flags is bitwise OR of zero or more of
     NGTCP2_WRITE_STREAM_FLAG_*. */
  uint32_t flags;
  /* data is the pointer to ngtcp2_vec array which contains the stream
     data to send. */
  const ngtcp2_vec *data;
  /* datacnt is the number of ngtcp2_vec pointed by data. */
  size_t datacnt;
  /* pdatalen is the pointer to the variable which the number of bytes
     written is assigned to if pdatalen is not NULL. */
  ngtcp2_ssize *pdatalen;
} ngtcp2_vmsg_stream;

typedef struct ngtcp2_vmsg_datagram {
  /* data is the pointer to ngtcp2_vec array which contains the data
     to send. */
  const ngtcp2_vec *data;
  /* datacnt is the number of ngtcp2_vec pointed by data. */
  size_t datacnt;
  /* flags is bitwise OR of zero or more of
     NGTCP2_WRITE_DATAGRAM_FLAG_*. */
  uint32_t flags;
  /* paccepted is the pointer to the variable which, if it is not
     NULL, is assigned nonzero if data is written to a packet. */
  int *paccepted;
} ngtcp2_vmsg_datagram;

typedef struct ngtcp2_vmsg {
  ngtcp2_vmsg_type type;
  union {
    ngtcp2_vmsg_stream stream;
    ngtcp2_vmsg_datagram datagram;
  };
} ngtcp2_vmsg;

/*
 * ngtcp2_conn_sched_ack stores packet number |pkt_num| and its
 * reception timestamp |ts| in order to send its ACK.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_PROTO
 *     Same packet number has already been added.
 */
int ngtcp2_conn_sched_ack(ngtcp2_conn *conn, ngtcp2_acktr *acktr,
                          int64_t pkt_num, int active_ack, ngtcp2_tstamp ts);

/*
 * ngtcp2_conn_find_stream returns a stream whose stream ID is
 * |stream_id|.  If no such stream is found, it returns NULL.
 */
ngtcp2_strm *ngtcp2_conn_find_stream(ngtcp2_conn *conn, int64_t stream_id);

/*
 * conn_init_stream initializes |strm|.  Its stream ID is |stream_id|.
 * This function adds |strm| to conn->strms.  |strm| must be allocated
 * by the caller.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-callback function failed.
 */
int ngtcp2_conn_init_stream(ngtcp2_conn *conn, ngtcp2_strm *strm,
                            int64_t stream_id, void *stream_user_data);

/*
 * ngtcp2_conn_close_stream closes stream |strm|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     Stream is not found.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
int ngtcp2_conn_close_stream(ngtcp2_conn *conn, ngtcp2_strm *strm,
                             uint64_t app_error_code);

/*
 * ngtcp2_conn_close_stream closes stream |strm| if no further
 * transmission and reception are allowed, and all reordered incoming
 * data are emitted to the application, and the transmitted data are
 * acked.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     Stream is not found.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
int ngtcp2_conn_close_stream_if_shut_rdwr(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                          uint64_t app_error_code);

/*
 * ngtcp2_conn_update_rtt updates RTT measurements.  |rtt| is a latest
 * RTT which is not adjusted by ack delay.  |ack_delay| is unscaled
 * ack_delay included in ACK frame.  |ack_delay| is actually tainted
 * (sent by peer), so don't assume that |ack_delay| is always smaller
 * than, or equals to |rtt|.
 */
void ngtcp2_conn_update_rtt(ngtcp2_conn *conn, ngtcp2_duration rtt,
                            ngtcp2_duration ack_delay, ngtcp2_tstamp ts);

void ngtcp2_conn_set_loss_detection_timer(ngtcp2_conn *conn, ngtcp2_tstamp ts);

/*
 * ngtcp2_conn_detect_lost_pkt detects lost packets.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
int ngtcp2_conn_detect_lost_pkt(ngtcp2_conn *conn, ngtcp2_pktns *pktns,
                                ngtcp2_conn_stat *cstat, ngtcp2_tstamp ts);

/*
 * ngtcp2_conn_tx_strmq_top returns the ngtcp2_strm which sits on the
 * top of queue.  tx_strmq must not be empty.
 */
ngtcp2_strm *ngtcp2_conn_tx_strmq_top(ngtcp2_conn *conn);

/*
 * ngtcp2_conn_tx_strmq_pop pops the ngtcp2_strm from the queue.
 * tx_strmq must not be empty.
 */
void ngtcp2_conn_tx_strmq_pop(ngtcp2_conn *conn);

/*
 * ngtcp2_conn_tx_strmq_push pushes |strm| into tx_strmq.
 *
 *  This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
int ngtcp2_conn_tx_strmq_push(ngtcp2_conn *conn, ngtcp2_strm *strm);

/*
 * ngtcp2_conn_internal_expiry returns the minimum expiry time among
 * all timers in |conn|.
 */
ngtcp2_tstamp ngtcp2_conn_internal_expiry(ngtcp2_conn *conn);

ngtcp2_ssize ngtcp2_conn_write_vmsg(ngtcp2_conn *conn, ngtcp2_path *path,
                                    ngtcp2_pkt_info *pi, uint8_t *dest,
                                    size_t destlen, ngtcp2_vmsg *vmsg,
                                    ngtcp2_tstamp ts);

/*
 * ngtcp2_conn_write_single_frame_pkt writes a packet which contains |fr|
 * frame only in the buffer pointed by |dest| whose length if
 * |destlen|.  |type| is a long packet type to send.  If |type| is 0,
 * Short packet is used.  |dcid| is used as a destination connection
 * ID.
 *
 * The packet written by this function will not be retransmitted.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
ngtcp2_ssize ngtcp2_conn_write_single_frame_pkt(
    ngtcp2_conn *conn, ngtcp2_pkt_info *pi, uint8_t *dest, size_t destlen,
    uint8_t type, const ngtcp2_cid *dcid, ngtcp2_frame *fr, uint8_t rtb_flags,
    const ngtcp2_path *path, ngtcp2_tstamp ts);

/*
 * ngtcp2_conn_commit_local_transport_params commits the local
 * transport parameters, which is currently set to
 * conn->local.settings.transport_params.  This function will do some
 * amends on transport parameters for adjusting default values.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     CID in preferred address equals to the original SCID.
 */
int ngtcp2_conn_commit_local_transport_params(ngtcp2_conn *conn);

/*
 * ngtcp2_conn_lost_pkt_expiry returns the earliest expiry time of
 * lost packet.
 */
ngtcp2_tstamp ngtcp2_conn_lost_pkt_expiry(ngtcp2_conn *conn);

/*
 * ngtcp2_conn_remove_lost_pkt removes the expired lost packet.
 */
void ngtcp2_conn_remove_lost_pkt(ngtcp2_conn *conn, ngtcp2_tstamp ts);

/*
 * ngtcp2_conn_resched_frames reschedules frames linked from |*pfrc|
 * for retransmission.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
int ngtcp2_conn_resched_frames(ngtcp2_conn *conn, ngtcp2_pktns *pktns,
                               ngtcp2_frame_chain **pfrc);

uint64_t ngtcp2_conn_tx_strmq_first_cycle(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_ack_delay_expiry` returns the expiry time point of
 * delayed protected ACK.  One should call
 * `ngtcp2_conn_cancel_expired_ack_delay_timer` and
 * `ngtcp2_conn_write_pkt` (or `ngtcp2_conn_writev_stream`) when it
 * expires.  It returns UINT64_MAX if there is no expiry.
 */
ngtcp2_tstamp ngtcp2_conn_ack_delay_expiry(ngtcp2_conn *conn);

/**
 * @function
 *
 * `ngtcp2_conn_cancel_expired_ack_delay_timer` stops expired ACK
 * delay timer.  |ts| is the current time.  This function must be
 * called when `ngtcp2_conn_ack_delay_expiry` <= ts.
 */
void ngtcp2_conn_cancel_expired_ack_delay_timer(ngtcp2_conn *conn,
                                                ngtcp2_tstamp ts);

/**
 * @function
 *
 * `ngtcp2_conn_loss_detection_expiry` returns the expiry time point
 * of loss detection timer.  One should call
 * `ngtcp2_conn_on_loss_detection_timer` and `ngtcp2_conn_write_pkt`
 * (or `ngtcp2_conn_writev_stream`) when it expires.  It returns
 * UINT64_MAX if loss detection timer is not armed.
 */
ngtcp2_tstamp ngtcp2_conn_loss_detection_expiry(ngtcp2_conn *conn);

#endif /* NGTCP2_CONN_H */
