/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
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
#ifndef NGHTTP3_CONN_H
#define NGHTTP3_CONN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp3/nghttp3.h>

#include "nghttp3_stream.h"
#include "nghttp3_map.h"
#include "nghttp3_qpack.h"
#include "nghttp3_tnode.h"
#include "nghttp3_idtr.h"
#include "nghttp3_gaptr.h"

#define NGHTTP3_VARINT_MAX ((1ull << 62) - 1)

/* NGHTTP3_QPACK_ENCODER_MAX_TABLE_CAPACITY is the maximum dynamic
   table size for QPACK encoder. */
#define NGHTTP3_QPACK_ENCODER_MAX_TABLE_CAPACITY 16384

/* NGHTTP3_QPACK_ENCODER_MAX_BLOCK_STREAMS is the maximum number of
   blocked streams for QPACK encoder. */
#define NGHTTP3_QPACK_ENCODER_MAX_BLOCK_STREAMS 100

/* NGHTTP3_PUSH_PROMISE_FLAG_NONE indicates that no flag is set. */
#define NGHTTP3_PUSH_PROMISE_FLAG_NONE 0x00
/* NGHTTP3_PUSH_PROMISE_FLAG_RECVED is set when PUSH_PROMISE is
   completely received. */
#define NGHTTP3_PUSH_PROMISE_FLAG_RECVED 0x01
/* NGHTTP3_PUSH_PROMISE_FLAG_RECV_CANCEL is set when push is
   cancelled by server before receiving PUSH_PROMISE completely.
   This flag should have no effect if push stream has already
   opened. */
#define NGHTTP3_PUSH_PROMISE_FLAG_RECV_CANCEL 0x02
/* NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL is set when push is
   canceled by the local endpoint. */
#define NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL 0x04
#define NGHTTP3_PUSH_PROMISE_FLAG_CANCELLED                                    \
  (NGHTTP3_PUSH_PROMISE_FLAG_RECV_CANCEL |                                     \
   NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL)
/* NGHTTP3_PUSH_PROMISE_FLAG_PUSH_ID_RECLAIMED indicates that
   unsent_max_pushes has been updated for this push ID. */
#define NGHTTP3_PUSH_PROMISE_FLAG_PUSH_ID_RECLAIMED 0x08
/* NGHTTP3_PUSH_PROMISE_FLAG_BOUND is set if nghttp3_push_promise
   object is bound to a stream where PUSH_PROMISE frame is received
   first.  nghttp3_push_promise object might be created before
   receiving any PUSH_PROMISE when pushed stream is received before
   it.*/
#define NGHTTP3_PUSH_PROMISE_FLAG_BOUND 0x10

typedef struct nghttp3_push_promise {
  nghttp3_map_entry me;
  nghttp3_tnode node;
  nghttp3_http_state http;
  /* stream is server initiated unidirectional stream which fulfils
     the push promise. */
  nghttp3_stream *stream;
  /* stream_id is the stream ID where this PUSH_PROMISE is first
     received.  PUSH_PROMISE with same push ID is allowed to be sent
     in the multiple streams.  We ignore those duplicated PUSH_PROMISE
     entirely because we don't see any value to add extra cost of
     processing for it.  Even ignoring those frame is not yet easy
     because we have to decode the header blocks.  Server push is
     overly complex and there is no good use case after all. */
  int64_t stream_id;
  /* flags is bitwise OR of zero or more of
     NGHTTP3_PUSH_PROMISE_FLAG_*. */
  uint16_t flags;
} nghttp3_push_promise;

/* NGHTTP3_CONN_FLAG_NONE indicates that no flag is set. */
#define NGHTTP3_CONN_FLAG_NONE 0x0000
/* NGHTTP3_CONN_FLAG_SETTINGS_RECVED is set when SETTINGS frame has
   been received. */
#define NGHTTP3_CONN_FLAG_SETTINGS_RECVED 0x0001
/* NGHTTP3_CONN_FLAG_CONTROL_OPENED is set when a control stream has
   opened. */
#define NGHTTP3_CONN_FLAG_CONTROL_OPENED 0x0002
/* NGHTTP3_CONN_FLAG_QPACK_ENCODER_OPENED is set when a QPACK encoder
   stream has opened. */
#define NGHTTP3_CONN_FLAG_QPACK_ENCODER_OPENED 0x0004
/* NGHTTP3_CONN_FLAG_QPACK_DECODER_OPENED is set when a QPACK decoder
   stream has opened. */
#define NGHTTP3_CONN_FLAG_QPACK_DECODER_OPENED 0x0008
/* NGHTTP3_CONN_FLAG_MAX_PUSH_ID_QUEUED indicates that MAX_PUSH_ID has
   been queued to control stream. */
#define NGHTTP3_CONN_FLAG_MAX_PUSH_ID_QUEUED 0x0010
/* NGHTTP3_CONN_FLAG_GOAWAY_RECVED indicates that GOAWAY frame has
   received. */
#define NGHTTP3_CONN_FLAG_GOAWAY_RECVED 0x0020
/* NGHTTP3_CONN_FLAG_GOAWAY_QUEUED indicates that GOAWAY frame has
   been submitted for transmission. */
#define NGHTTP3_CONN_FLAG_GOAWAY_QUEUED 0x0040

struct nghttp3_conn {
  nghttp3_callbacks callbacks;
  nghttp3_map streams;
  nghttp3_map pushes;
  nghttp3_qpack_decoder qdec;
  nghttp3_qpack_encoder qenc;
  nghttp3_pq qpack_blocked_streams;
  struct {
    nghttp3_pq spq;
  } sched[NGHTTP3_URGENCY_LEVELS];
  const nghttp3_mem *mem;
  void *user_data;
  int server;
  uint16_t flags;
  uint64_t next_seq;

  struct {
    nghttp3_settings settings;
    struct {
      /* max_pushes is the number of push IDs that local endpoint can
         issue.  This field is used by server only. */
      uint64_t max_pushes;
      /* next_push_id is the next push ID server uses.  This field is
         used by server only. */
      int64_t next_push_id;
    } uni;
  } local;

  struct {
    struct {
      nghttp3_idtr idtr;
      /* max_client_streams is the cumulative number of client
         initiated bidirectional stream ID the remote endpoint can
         issue.  This field is used on server side only. */
      uint64_t max_client_streams;
    } bidi;
    struct {
      /* push_idtr tracks which push ID has been used by remote
         server.  This field is used by client only. */
      nghttp3_gaptr push_idtr;
      /* unsent_max_pushes is the maximum number of push which the local
         endpoint can accept.  This limit is not yet notified to the
         remote endpoint.  This field is used by client only. */
      uint64_t unsent_max_pushes;
      /* max_push is the maximum number of push which the local
         endpoint can accept.  This field is used by client only. */
      uint64_t max_pushes;
    } uni;
    nghttp3_settings settings;
  } remote;

  struct {
    /* goaway_id is the latest ID received in GOAWAY frame. */
    int64_t goaway_id;

    int64_t max_stream_id_bidi;
    int64_t max_push_id;
  } rx;

  struct {
    struct {
      nghttp3_buf rbuf;
      nghttp3_buf ebuf;
    } qpack;
    nghttp3_stream *ctrl;
    nghttp3_stream *qenc;
    nghttp3_stream *qdec;
    /* goaway_id is the latest ID sent in GOAWAY frame. */
    int64_t goaway_id;
  } tx;
};

nghttp3_stream *nghttp3_conn_find_stream(nghttp3_conn *conn, int64_t stream_id);

nghttp3_push_promise *nghttp3_conn_find_push_promise(nghttp3_conn *conn,
                                                     int64_t push_id);

int nghttp3_conn_create_stream(nghttp3_conn *conn, nghttp3_stream **pstream,
                               int64_t stream_id);

int nghttp3_conn_create_push_promise(nghttp3_conn *conn,
                                     nghttp3_push_promise **ppp,
                                     int64_t push_id,
                                     nghttp3_tnode *assoc_tnode);

nghttp3_ssize nghttp3_conn_read_bidi(nghttp3_conn *conn, size_t *pnproc,
                                     nghttp3_stream *stream, const uint8_t *src,
                                     size_t srclen, int fin);

nghttp3_ssize nghttp3_conn_read_uni(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const uint8_t *src, size_t srclen, int fin);

nghttp3_ssize nghttp3_conn_read_control(nghttp3_conn *conn,
                                        nghttp3_stream *stream,
                                        const uint8_t *src, size_t srclen);

nghttp3_ssize nghttp3_conn_read_push(nghttp3_conn *conn, size_t *pnproc,
                                     nghttp3_stream *stream, const uint8_t *src,
                                     size_t srclen, int fin);

nghttp3_ssize nghttp3_conn_read_qpack_encoder(nghttp3_conn *conn,
                                              const uint8_t *src,
                                              size_t srclen);

nghttp3_ssize nghttp3_conn_read_qpack_decoder(nghttp3_conn *conn,
                                              const uint8_t *src,
                                              size_t srclen);

int nghttp3_conn_on_push_promise_push_id(nghttp3_conn *conn, int64_t push_id,
                                         nghttp3_stream *stream);

int nghttp3_conn_on_client_cancel_push(nghttp3_conn *conn,
                                       const nghttp3_frame_cancel_push *fr);

int nghttp3_conn_on_server_cancel_push(nghttp3_conn *conn,
                                       const nghttp3_frame_cancel_push *fr);

int nghttp3_conn_on_stream_push_id(nghttp3_conn *conn, nghttp3_stream *stream,
                                   int64_t push_id);

int nghttp3_conn_on_data(nghttp3_conn *conn, nghttp3_stream *stream,
                         const uint8_t *data, size_t datalen);

nghttp3_ssize nghttp3_conn_on_headers(nghttp3_conn *conn,
                                      nghttp3_stream *stream,
                                      nghttp3_push_promise *pp,
                                      const uint8_t *data, size_t datalen,
                                      int fin);

int nghttp3_conn_on_settings_entry_received(nghttp3_conn *conn,
                                            const nghttp3_frame_settings *fr);

int nghttp3_conn_qpack_blocked_streams_push(nghttp3_conn *conn,
                                            nghttp3_stream *stream);

void nghttp3_conn_qpack_blocked_streams_pop(nghttp3_conn *conn);

int nghttp3_conn_server_cancel_push(nghttp3_conn *conn, int64_t push_id);

int nghttp3_conn_client_cancel_push(nghttp3_conn *conn, int64_t push_id);

int nghttp3_conn_submit_max_push_id(nghttp3_conn *conn);

int nghttp3_conn_schedule_stream(nghttp3_conn *conn, nghttp3_stream *stream);

int nghttp3_conn_ensure_stream_scheduled(nghttp3_conn *conn,
                                         nghttp3_stream *stream);

void nghttp3_conn_unschedule_stream(nghttp3_conn *conn, nghttp3_stream *stream);

int nghttp3_conn_reject_stream(nghttp3_conn *conn, nghttp3_stream *stream);

int nghttp3_conn_reject_push_stream(nghttp3_conn *conn, nghttp3_stream *stream);

int nghttp3_conn_cancel_push_stream(nghttp3_conn *conn, nghttp3_stream *stream);

/*
 * nghttp3_conn_get_next_tx_stream returns next stream to send.  It
 * returns NULL if there is no such stream.
 */
nghttp3_stream *nghttp3_conn_get_next_tx_stream(nghttp3_conn *conn);

int nghttp3_push_promise_new(nghttp3_push_promise **ppp, int64_t push_id,
                             uint64_t seq, nghttp3_tnode *assoc_tnode,
                             const nghttp3_mem *mem);

void nghttp3_push_promise_del(nghttp3_push_promise *pp, const nghttp3_mem *mem);

#endif /* NGHTTP3_CONN_H */
