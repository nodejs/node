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
#ifndef NGTCP2_RTB_H
#define NGTCP2_RTB_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2.h>

#include "ngtcp2_pkt.h"
#include "ngtcp2_ksl.h"
#include "ngtcp2_pq.h"

typedef struct ngtcp2_conn ngtcp2_conn;
typedef struct ngtcp2_pktns ngtcp2_pktns;
typedef struct ngtcp2_log ngtcp2_log;
typedef struct ngtcp2_qlog ngtcp2_qlog;
typedef struct ngtcp2_strm ngtcp2_strm;
typedef struct ngtcp2_rst ngtcp2_rst;

/* NGTCP2_FRAME_CHAIN_BINDER_FLAG_NONE indicates that no flag is
   set. */
#define NGTCP2_FRAME_CHAIN_BINDER_FLAG_NONE 0x00
/* NGTCP2_FRAME_CHAIN_BINDER_FLAG_ACK indicates that an information
   which a frame carries has been acknowledged. */
#define NGTCP2_FRAME_CHAIN_BINDER_FLAG_ACK 0x01

/*
 * ngtcp2_frame_chain_binder binds 2 or more of ngtcp2_frame_chain to
 * share the acknowledgement state.  In general, all
 * ngtcp2_frame_chains bound to the same binder must have the same
 * information.
 */
typedef struct ngtcp2_frame_chain_binder {
  size_t refcount;
  /* flags is bitwise OR of zero or more of
     NGTCP2_FRAME_CHAIN_BINDER_FLAG_*. */
  uint32_t flags;
} ngtcp2_frame_chain_binder;

int ngtcp2_frame_chain_binder_new(ngtcp2_frame_chain_binder **pbinder,
                                  const ngtcp2_mem *mem);

typedef struct ngtcp2_frame_chain ngtcp2_frame_chain;

/*
 * ngtcp2_frame_chain chains frames in a single packet.
 */
struct ngtcp2_frame_chain {
  ngtcp2_frame_chain *next;
  ngtcp2_frame_chain_binder *binder;
  ngtcp2_frame fr;
};

/*
 * ngtcp2_bind_frame_chains binds two frame chains |a| and |b| using
 * new or existing ngtcp2_frame_chain_binder.  |a| might have non-NULL
 * a->binder.  |b| must not have non-NULL b->binder.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
int ngtcp2_bind_frame_chains(ngtcp2_frame_chain *a, ngtcp2_frame_chain *b,
                             const ngtcp2_mem *mem);

/* NGTCP2_MAX_STREAM_DATACNT is the maximum number of ngtcp2_vec that
   a ngtcp2_stream can include. */
#define NGTCP2_MAX_STREAM_DATACNT 256

/* NGTCP2_MAX_CRYPTO_DATACNT is the maximum number of ngtcp2_vec that
   a ngtcp2_crypto can include. */
#define NGTCP2_MAX_CRYPTO_DATACNT 8

/*
 * ngtcp2_frame_chain_new allocates ngtcp2_frame_chain object and
 * assigns its pointer to |*pfrc|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
int ngtcp2_frame_chain_new(ngtcp2_frame_chain **pfrc, const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_extralen_new works like ngtcp2_frame_chain_new,
 * but it allocates extra memory |extralen| in order to extend
 * ngtcp2_frame.
 */
int ngtcp2_frame_chain_extralen_new(ngtcp2_frame_chain **pfrc, size_t extralen,
                                    const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_stream_datacnt_new works like
 * ngtcp2_frame_chain_new, but it allocates enough data to store
 * additional |datacnt| - 1 ngtcp2_vec object after ngtcp2_stream
 * object.  If |datacnt| equals to 1, ngtcp2_frame_chain_new is called
 * internally.
 */
int ngtcp2_frame_chain_stream_datacnt_new(ngtcp2_frame_chain **pfrc,
                                          size_t datacnt,
                                          const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_crypto_datacnt_new works like
 * ngtcp2_frame_chain_new, but it allocates enough data to store
 * additional |datacnt| - 1 ngtcp2_vec object after ngtcp2_crypto
 * object.  If |datacnt| equals to 1, ngtcp2_frame_chain_new is called
 * internally.
 */
int ngtcp2_frame_chain_crypto_datacnt_new(ngtcp2_frame_chain **pfrc,
                                          size_t datacnt,
                                          const ngtcp2_mem *mem);

int ngtcp2_frame_chain_new_token_new(ngtcp2_frame_chain **pfrc,
                                     const ngtcp2_vec *token,
                                     const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_del deallocates |frc|.  It also deallocates the
 * memory pointed by |frc|.
 */
void ngtcp2_frame_chain_del(ngtcp2_frame_chain *frc, const ngtcp2_mem *mem);

/*
 * ngtcp2_frame_chain_init initializes |frc|.
 */
void ngtcp2_frame_chain_init(ngtcp2_frame_chain *frc);

/*
 * ngtcp2_frame_chain_list_del deletes |frc|, and all objects
 * connected by next field.
 */
void ngtcp2_frame_chain_list_del(ngtcp2_frame_chain *frc,
                                 const ngtcp2_mem *mem);

/* NGTCP2_RTB_ENTRY_FLAG_NONE indicates that no flag is set. */
#define NGTCP2_RTB_ENTRY_FLAG_NONE 0x00
/* NGTCP2_RTB_ENTRY_FLAG_PROBE indicates that the entry includes a
   probe packet. */
#define NGTCP2_RTB_ENTRY_FLAG_PROBE 0x01
/* NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE indicates that the entry
   includes a frame which must be retransmitted until it is
   acknowledged.  In most cases, this flag is used along with
   NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING.  We have these 2 flags because
   NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE triggers PTO, but just
   NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING does not. */
#define NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE 0x02
/* NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING indicates that the entry
   elicits acknowledgement. */
#define NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING 0x04
/* NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED indicates that the packet has
   been reclaimed on PTO.  It is not marked lost yet and still
   consumes congestion window. */
#define NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED 0x08
/* NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED indicates that the entry
   has been marked lost and scheduled to retransmit. */
#define NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED 0x10
/* NGTCP2_RTB_ENTRY_FLAG_ECN indicates that the entry is included in a
   UDP datagram with ECN marking. */
#define NGTCP2_RTB_ENTRY_FLAG_ECN 0x20

typedef struct ngtcp2_rtb_entry ngtcp2_rtb_entry;

/*
 * ngtcp2_rtb_entry is an object stored in ngtcp2_rtb.  It corresponds
 * to the one packet which is waiting for its ACK.
 */
struct ngtcp2_rtb_entry {
  ngtcp2_rtb_entry *next;

  struct {
    int64_t pkt_num;
    uint8_t type;
    uint8_t flags;
  } hd;
  ngtcp2_frame_chain *frc;
  /* ts is the time point when a packet included in this entry is sent
     to a peer. */
  ngtcp2_tstamp ts;
  /* lost_ts is the time when this entry is marked lost. */
  ngtcp2_tstamp lost_ts;
  /* pktlen is the length of QUIC packet */
  size_t pktlen;
  struct {
    uint64_t delivered;
    ngtcp2_tstamp delivered_ts;
    ngtcp2_tstamp first_sent_ts;
    int is_app_limited;
  } rst;
  /* flags is bitwise-OR of zero or more of
     NGTCP2_RTB_ENTRY_FLAG_*. */
  uint8_t flags;
};

/*
 * ngtcp2_rtb_entry_new allocates ngtcp2_rtb_entry object, and assigns
 * its pointer to |*pent|.  On success, |*pent| takes ownership of
 * |frc|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
int ngtcp2_rtb_entry_new(ngtcp2_rtb_entry **pent, const ngtcp2_pkt_hd *hd,
                         ngtcp2_frame_chain *frc, ngtcp2_tstamp ts,
                         size_t pktlen, uint8_t flags, const ngtcp2_mem *mem);

/*
 * ngtcp2_rtb_entry_del deallocates |ent|.  It also frees memory
 * pointed by |ent|.
 */
void ngtcp2_rtb_entry_del(ngtcp2_rtb_entry *ent, const ngtcp2_mem *mem);

/*
 * ngtcp2_rtb tracks sent packets, and its ACK timeout for
 * retransmission.
 */
typedef struct ngtcp2_rtb {
  /* ents includes ngtcp2_rtb_entry sorted by decreasing order of
     packet number. */
  ngtcp2_ksl ents;
  /* crypto is CRYPTO stream. */
  ngtcp2_strm *crypto;
  ngtcp2_rst *rst;
  ngtcp2_cc *cc;
  ngtcp2_log *log;
  ngtcp2_qlog *qlog;
  const ngtcp2_mem *mem;
  /* largest_acked_tx_pkt_num is the largest packet number
     acknowledged by the peer. */
  int64_t largest_acked_tx_pkt_num;
  /* num_ack_eliciting is the number of ACK eliciting entries. */
  size_t num_ack_eliciting;
  /* num_retransmittable is the number of packets which contain frames
     that must be retransmitted on loss. */
  size_t num_retransmittable;
  /* probe_pkt_left is the number of probe packet to send */
  size_t probe_pkt_left;
  /* pktns_id is the identifier of packet number space. */
  ngtcp2_pktns_id pktns_id;
  /* cc_pkt_num is the smallest packet number that is contributed to
     ngtcp2_conn_stat.bytes_in_flight. */
  int64_t cc_pkt_num;
  /* cc_bytes_in_flight is the number of in-flight bytes that is
     contributed to ngtcp2_conn_stat.bytes_in_flight.  It only
     includes the bytes after congestion state is reset. */
  uint64_t cc_bytes_in_flight;
  /* persistent_congestion_start_ts is the time when persistent
     congestion evaluation is started.  It happens roughly after
     handshake is confirmed. */
  ngtcp2_tstamp persistent_congestion_start_ts;
  /* num_lost_pkts is the number entries in ents which has
     NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED flag set. */
  size_t num_lost_pkts;
} ngtcp2_rtb;

/*
 * ngtcp2_rtb_init initializes |rtb|.
 */
void ngtcp2_rtb_init(ngtcp2_rtb *rtb, ngtcp2_pktns_id pktns_id,
                     ngtcp2_strm *crypto, ngtcp2_rst *rst, ngtcp2_cc *cc,
                     ngtcp2_log *log, ngtcp2_qlog *qlog, const ngtcp2_mem *mem);

/*
 * ngtcp2_rtb_free deallocates resources allocated for |rtb|.
 */
void ngtcp2_rtb_free(ngtcp2_rtb *rtb);

/*
 * ngtcp2_rtb_add adds |ent| to |rtb|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
int ngtcp2_rtb_add(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                   ngtcp2_conn_stat *cstat);

/*
 * ngtcp2_rtb_head returns the iterator which points to the entry
 * which has the largest packet number.  If there is no entry,
 * returned value satisfies ngtcp2_ksl_it_end(&it) != 0.
 */
ngtcp2_ksl_it ngtcp2_rtb_head(ngtcp2_rtb *rtb);

/*
 * ngtcp2_rtb_recv_ack removes acked ngtcp2_rtb_entry from |rtb|.
 * |pkt_num| is a packet number which includes |fr|.  |pkt_ts| is the
 * timestamp when packet is received.  |ts| should be the current
 * time.  Usually they are the same, but for buffered packets,
 * |pkt_ts| would be earlier than |ts|.
 *
 * This function returns the number of newly acknowledged packets if
 * it succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
ngtcp2_ssize ngtcp2_rtb_recv_ack(ngtcp2_rtb *rtb, const ngtcp2_ack *fr,
                                 ngtcp2_conn_stat *cstat, ngtcp2_conn *conn,
                                 ngtcp2_pktns *pktns, ngtcp2_tstamp pkt_ts,
                                 ngtcp2_tstamp ts);

/*
 * ngtcp2_rtb_detect_lost_pkt detects lost packets and prepends the
 * frames contained them to |*pfrc|.  Even when this function fails,
 * some frames might be prepended to |*pfrc| and the caller should
 * handle them.
 */
int ngtcp2_rtb_detect_lost_pkt(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                               ngtcp2_pktns *pktns, ngtcp2_conn_stat *cstat,
                               ngtcp2_duration pto, ngtcp2_tstamp ts);

/*
 * ngtcp2_rtb_remove_expired_lost_pkt removes expired lost packet.
 */
void ngtcp2_rtb_remove_expired_lost_pkt(ngtcp2_rtb *rtb, ngtcp2_duration pto,
                                        ngtcp2_tstamp ts);

/*
 * ngtcp2_rtb_lost_pkt_ts returns the earliest time when the still
 * retained packet was lost.  It returns UINT64_MAX if no such packet
 * exists.
 */
ngtcp2_tstamp ngtcp2_rtb_lost_pkt_ts(ngtcp2_rtb *rtb);

/*
 * ngtcp2_rtb_remove_all removes all packets from |rtb| and prepends
 * all frames to |*pfrc|.  Even when this function fails, some frames
 * might be prepended to |*pfrc| and the caller should handle them.
 */
int ngtcp2_rtb_remove_all(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                          ngtcp2_pktns *pktns, ngtcp2_conn_stat *cstat);

/*
 * ngtcp2_rtb_empty returns nonzero if |rtb| have no entry.
 */
int ngtcp2_rtb_empty(ngtcp2_rtb *rtb);

/*
 * ngtcp2_rtb_reset_cc_state resets congestion state in |rtb|.
 * |cc_pkt_num| is the next outbound packet number which is sent under
 * new congestion state.
 */
void ngtcp2_rtb_reset_cc_state(ngtcp2_rtb *rtb, int64_t cc_pkt_num);

/*
 * ngtcp2_rtb_remove_expired_lost_pkt ensures that the number of lost
 * packets at most |n|.
 */
void ngtcp2_rtb_remove_excessive_lost_pkt(ngtcp2_rtb *rtb, size_t n);

/*
 * ngtcp2_rtb_reclaim_on_pto reclaims up to |num_pkts| packets which
 * are in-flight and not marked lost to send them in PTO probe.  The
 * reclaimed frames are chained to |*pfrc|.
 *
 * This function returns the number of packets reclaimed if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
ngtcp2_ssize ngtcp2_rtb_reclaim_on_pto(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                                       ngtcp2_pktns *pktns, size_t num_pkts);

#endif /* NGTCP2_RTB_H */
