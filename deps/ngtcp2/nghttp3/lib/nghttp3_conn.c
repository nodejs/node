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
#include "nghttp3_conn.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "nghttp3_mem.h"
#include "nghttp3_macro.h"
#include "nghttp3_err.h"
#include "nghttp3_conv.h"
#include "nghttp3_http.h"
#include "nghttp3_unreachable.h"
#include "nghttp3_settings.h"
#include "nghttp3_callbacks.h"
#include "nghttp3_wt.h"

nghttp3_objalloc_def(chunk, nghttp3_chunk, oplent)

/*
 * conn_remote_stream returns nonzero if |stream_id| is a remote
 * stream ID.
 */
static int conn_remote_stream(const nghttp3_conn *conn, int64_t stream_id) {
  if (conn->server) {
    return !(stream_id & 0x1);
  }

  return stream_id & 0x1;
}

/*
 * conn_remote_stream_uni returns nonzero if |stream_id| is remote
 * unidirectional stream ID.
 */
static int conn_remote_stream_uni(const nghttp3_conn *conn, int64_t stream_id) {
  if (conn->server) {
    return (stream_id & 0x03) == 0x02;
  }
  return (stream_id & 0x03) == 0x03;
}

static int conn_wt_enabled(const nghttp3_conn *conn) {
  const nghttp3_settings *local_settings = &conn->local.settings;
  const nghttp3_proto_settings *remote_settings = &conn->remote.settings;

  if (!local_settings->wt_enabled || !local_settings->h3_datagram) {
    return 0;
  }

  if (conn->server) {
    return (!(conn->flags & NGHTTP3_CONN_FLAG_SETTINGS_RECVED) ||
            /* TODO client sends SETTINGS_WT_ENABLED for draft
               versions only.  But some client implementations do not
               send it.  For interop purpose, do not require this
               remote setting for now. */
            (/* remote_settings->wt_enabled && */ remote_settings
               ->h3_datagram)) &&
           local_settings->enable_connect_protocol;
  }

  return remote_settings->wt_enabled &&
         remote_settings->enable_connect_protocol &&
         remote_settings->h3_datagram;
}

static int conn_call_begin_headers(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.begin_headers) {
    return 0;
  }

  rv = conn->callbacks.begin_headers(conn, stream->node.id, conn->user_data,
                                     stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_headers(nghttp3_conn *conn, nghttp3_stream *stream,
                                 int fin) {
  int rv;

  if (!conn->callbacks.end_headers) {
    return 0;
  }

  rv = conn->callbacks.end_headers(conn, stream->node.id, fin, conn->user_data,
                                   stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_begin_trailers(nghttp3_conn *conn,
                                    nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.begin_trailers) {
    return 0;
  }

  rv = conn->callbacks.begin_trailers(conn, stream->node.id, conn->user_data,
                                      stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_trailers(nghttp3_conn *conn, nghttp3_stream *stream,
                                  int fin) {
  int rv;

  if (!conn->callbacks.end_trailers) {
    return 0;
  }

  rv = conn->callbacks.end_trailers(conn, stream->node.id, fin, conn->user_data,
                                    stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_stream(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.end_stream) {
    return 0;
  }

  rv = conn->callbacks.end_stream(conn, stream->node.id, conn->user_data,
                                  stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_stop_sending(nghttp3_conn *conn, nghttp3_stream *stream,
                                  uint64_t app_error_code) {
  int rv;

  if (!conn->callbacks.stop_sending) {
    return 0;
  }

  rv = conn->callbacks.stop_sending(conn, stream->node.id, app_error_code,
                                    conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_reset_stream(nghttp3_conn *conn, nghttp3_stream *stream,
                                  uint64_t app_error_code) {
  int rv;

  if (!conn->callbacks.reset_stream) {
    return 0;
  }

  rv = conn->callbacks.reset_stream(conn, stream->node.id, app_error_code,
                                    conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_deferred_consume(nghttp3_conn *conn,
                                      nghttp3_stream *stream,
                                      size_t nconsumed) {
  int rv;

  if (nconsumed == 0 || !conn->callbacks.deferred_consume) {
    return 0;
  }

  rv = conn->callbacks.deferred_consume(conn, stream->node.id, nconsumed,
                                        conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_recv_settings(nghttp3_conn *conn) {
  int rv;

  if (!conn->callbacks.recv_settings2) {
    if (!conn->callbacks.recv_settings) {
      return 0;
    }

    rv = conn->callbacks.recv_settings(
      conn,
      &(nghttp3_settings){
        .max_field_section_size = conn->remote.settings.max_field_section_size,
        .qpack_max_dtable_capacity =
          conn->remote.settings.qpack_max_dtable_capacity,
        .qpack_blocked_streams = conn->remote.settings.qpack_blocked_streams,
        .enable_connect_protocol =
          conn->remote.settings.enable_connect_protocol,
        .h3_datagram = conn->remote.settings.h3_datagram,
      },
      conn->user_data);
    if (rv != 0) {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }

    return 0;
  }

  rv = conn->callbacks.recv_settings2(conn, &conn->remote.settings,
                                      conn->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_recv_origin(nghttp3_conn *conn, const uint8_t *origin,
                                 size_t originlen) {
  if (!conn->callbacks.recv_origin) {
    return 0;
  }

  if (conn->callbacks.recv_origin(conn, origin, originlen, conn->user_data) !=
      0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_origin(nghttp3_conn *conn) {
  if (!conn->callbacks.end_origin) {
    return 0;
  }

  if (conn->callbacks.end_origin(conn, conn->user_data) != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_recv_data(nghttp3_conn *conn, const nghttp3_stream *stream,
                               const uint8_t *data, size_t datalen) {
  int rv;

  if (!conn->callbacks.recv_data) {
    return 0;
  }

  rv = conn->callbacks.recv_data(conn, stream->node.id, data, datalen,
                                 conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_recv_wt_data(nghttp3_conn *conn,
                                  const nghttp3_stream *stream,
                                  const uint8_t *data, size_t datalen) {
  nghttp3_wt_session *wt_session;
  int rv;

  if (!conn->callbacks.recv_wt_data) {
    return 0;
  }

  wt_session = stream->wt.session;

  assert(wt_session);

  rv = conn->callbacks.recv_wt_data(conn, wt_session->session_id,
                                    stream->node.id, data, datalen,
                                    conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_wt_data_stream_open(nghttp3_conn *conn,
                                         const nghttp3_stream *stream) {
  nghttp3_wt_session *wt_session;
  int rv;

  if (!conn->callbacks.wt_data_stream_open) {
    return 0;
  }

  wt_session = stream->wt.session;

  assert(wt_session);

  rv = conn->callbacks.wt_data_stream_open(conn, wt_session->session_id,
                                           stream->node.id, conn->user_data,
                                           stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_glitch_ratelim_drain(nghttp3_conn *conn, uint64_t n,
                                     nghttp3_tstamp ts) {
  if (ts == UINT64_MAX) {
    return 0;
  }

  return nghttp3_ratelim_drain(&conn->glitch_rlim, n, ts);
}

static int ricnt_less(const nghttp3_pq_entry *lhsx,
                      const nghttp3_pq_entry *rhsx) {
  nghttp3_stream *lhs =
    nghttp3_struct_of(lhsx, nghttp3_stream, qpack_blocked_pe);
  nghttp3_stream *rhs =
    nghttp3_struct_of(rhsx, nghttp3_stream, qpack_blocked_pe);

  return lhs->qpack_sctx.ricnt < rhs->qpack_sctx.ricnt;
}

static int cycle_less(const nghttp3_pq_entry *lhsx,
                      const nghttp3_pq_entry *rhsx) {
  const nghttp3_tnode *lhs = nghttp3_struct_of(lhsx, nghttp3_tnode, pe);
  const nghttp3_tnode *rhs = nghttp3_struct_of(rhsx, nghttp3_tnode, pe);

  if (lhs->cycle == rhs->cycle) {
    return lhs->id < rhs->id;
  }

  return rhs->cycle - lhs->cycle <= NGHTTP3_TNODE_MAX_CYCLE_GAP;
}

static int conn_new(nghttp3_conn **pconn, int server, int callbacks_version,
                    const nghttp3_callbacks *callbacks, int settings_version,
                    const nghttp3_settings *settings, const nghttp3_mem *mem,
                    void *user_data) {
  nghttp3_conn *conn;
  nghttp3_settings settings_latest;
  nghttp3_callbacks callbacks_latest;
  uint64_t map_seed;
  size_t i;
  (void)callbacks_version;

  settings = nghttp3_settings_convert_to_latest(&settings_latest,
                                                settings_version, settings);
  callbacks = nghttp3_callbacks_convert_to_latest(&callbacks_latest,
                                                  callbacks_version, callbacks);

  assert(settings->max_field_section_size <= NGHTTP3_VARINT_MAX);
  assert(settings->qpack_max_dtable_capacity <= NGHTTP3_VARINT_MAX);
  assert(settings->qpack_encoder_max_dtable_capacity <= NGHTTP3_VARINT_MAX);
  assert(settings->qpack_blocked_streams <= NGHTTP3_VARINT_MAX);

  if (mem == NULL) {
    mem = nghttp3_mem_default();
  }

  conn = nghttp3_mem_calloc(mem, 1, sizeof(nghttp3_conn));
  if (conn == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  nghttp3_objalloc_init(&conn->out_chunk_objalloc,
                        NGHTTP3_STREAM_MIN_CHUNK_SIZE * 16, mem);
  nghttp3_objalloc_stream_init(&conn->stream_objalloc, 8, mem);

  if (callbacks->rand) {
    callbacks->rand((uint8_t *)&map_seed, sizeof(map_seed));
  } else {
    map_seed = 0;
  }

  nghttp3_map_init(&conn->streams, map_seed, mem);

  nghttp3_qpack_decoder_init(&conn->qdec, settings->qpack_max_dtable_capacity,
                             settings->qpack_blocked_streams, mem);

  nghttp3_qpack_encoder_init(
    &conn->qenc, settings->qpack_encoder_max_dtable_capacity, ++map_seed, mem);
  nghttp3_qpack_encoder_set_indexing_strat(&conn->qenc,
                                           settings->qpack_indexing_strat);

  nghttp3_pq_init(&conn->qpack_blocked_streams, ricnt_less, mem);

  for (i = 0; i < NGHTTP3_URGENCY_LEVELS; ++i) {
    nghttp3_pq_init(&conn->sched[i].spq, cycle_less, mem);
  }

  nghttp3_idtr_init(&conn->remote.bidi.idtr, mem);

  nghttp3_ratelim_init(&conn->glitch_rlim, settings->glitch_ratelim_burst,
                       settings->glitch_ratelim_rate, 0);

  conn->callbacks = *callbacks;
  conn->local.settings = *settings;
  if (server) {
    if (settings->origin_list) {
      conn->local.settings.origin_list = &conn->local.origin_list;
      conn->local.origin_list = *settings->origin_list;
    }
  } else {
    conn->local.settings.enable_connect_protocol = 0;
    conn->local.settings.origin_list = NULL;
  }
  conn->remote.settings.max_field_section_size = NGHTTP3_VARINT_MAX;
  conn->mem = mem;
  conn->user_data = user_data;
  conn->server = server;
  conn->rx.goaway_id = NGHTTP3_VARINT_MAX + 1;
  conn->tx.goaway_id = NGHTTP3_VARINT_MAX + 1;
  conn->rx.max_stream_id_bidi = -4;

  *pconn = conn;

  return 0;
}

int nghttp3_conn_client_new_versioned(nghttp3_conn **pconn,
                                      int callbacks_version,
                                      const nghttp3_callbacks *callbacks,
                                      int settings_version,
                                      const nghttp3_settings *settings,
                                      const nghttp3_mem *mem, void *user_data) {
  int rv;

  rv = conn_new(pconn, /* server = */ 0, callbacks_version, callbacks,
                settings_version, settings, mem, user_data);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

int nghttp3_conn_server_new_versioned(nghttp3_conn **pconn,
                                      int callbacks_version,
                                      const nghttp3_callbacks *callbacks,
                                      int settings_version,
                                      const nghttp3_settings *settings,
                                      const nghttp3_mem *mem, void *user_data) {
  int rv;

  rv = conn_new(pconn, /* server = */ 1, callbacks_version, callbacks,
                settings_version, settings, mem, user_data);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

static void remove_wt_session_ref(nghttp3_wt_session *wt_session) {
  nghttp3_stream *stream;

  for (stream = wt_session->head; stream; stream = stream->wt.next) {
    assert(stream->wt.session == wt_session);

    stream->wt.session = NULL;
  }
}

static int free_stream(void *data, void *ptr) {
  nghttp3_stream *stream = data;

  (void)ptr;

  if (nghttp3_stream_wt_ctrl(stream)) {
    remove_wt_session_ref(stream->wt.session);
  }

  nghttp3_stream_del(stream);

  return 0;
}

void nghttp3_conn_del(nghttp3_conn *conn) {
  size_t i;

  if (conn == NULL) {
    return;
  }

  nghttp3_buf_free(&conn->tx.qpack.ebuf, conn->mem);
  nghttp3_buf_free(&conn->tx.qpack.rbuf, conn->mem);

  nghttp3_idtr_free(&conn->remote.bidi.idtr);

  for (i = 0; i < NGHTTP3_URGENCY_LEVELS; ++i) {
    nghttp3_pq_free(&conn->sched[i].spq);
  }

  nghttp3_pq_free(&conn->qpack_blocked_streams);

  nghttp3_qpack_encoder_free(&conn->qenc);
  nghttp3_qpack_decoder_free(&conn->qdec);

  nghttp3_map_each(&conn->streams, free_stream, NULL);
  nghttp3_map_free(&conn->streams);

  nghttp3_objalloc_free(&conn->stream_objalloc);
  nghttp3_objalloc_free(&conn->out_chunk_objalloc);

  nghttp3_mem_free(conn->mem, conn->rx.originbuf);

  nghttp3_mem_free(conn->mem, conn);
}

static int conn_bidi_idtr_open(nghttp3_conn *conn, int64_t stream_id) {
  int rv;

  rv = nghttp3_idtr_open(&conn->remote.bidi.idtr, stream_id);
  if (rv != 0) {
    return rv;
  }

  if (nghttp3_ksl_len(&conn->remote.bidi.idtr.gap.gap) > 32) {
    nghttp3_gaptr_drop_first_gap(&conn->remote.bidi.idtr.gap);
  }

  return 0;
}

nghttp3_ssize nghttp3_conn_read_stream(nghttp3_conn *conn, int64_t stream_id,
                                       const uint8_t *src, size_t srclen,
                                       int fin) {
  return nghttp3_conn_read_stream2(conn, stream_id, src, srclen, fin,
                                   UINT64_MAX);
}

nghttp3_ssize nghttp3_conn_read_stream2(nghttp3_conn *conn, int64_t stream_id,
                                        const uint8_t *src, size_t srclen,
                                        int fin, nghttp3_tstamp ts) {
  nghttp3_stream *stream;
  size_t bidi_nproc;
  int rv;

  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    /* TODO Assert idtr */
    /* QUIC transport ensures that this is new stream. */
    if (conn->server) {
      if (nghttp3_client_stream_bidi(stream_id)) {
        rv = conn_bidi_idtr_open(conn, stream_id);
        if (rv != 0) {
          if (nghttp3_err_is_fatal(rv)) {
            return rv;
          }

          /* Ignore return value.  We might drop the first gap if there
             are many gaps if QUIC stack allows too many holes in stream
             ID space.  idtr is used to decide whether PRIORITY_UPDATE
             frame should be ignored or not and the frame is optional.
             Ignoring them causes no harm. */
        }

        conn->rx.max_stream_id_bidi =
          nghttp3_max(conn->rx.max_stream_id_bidi, stream_id);
        rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
        if (rv != 0) {
          return rv;
        }

        if (conn_wt_enabled(conn)) {
          stream->flags |= NGHTTP3_STREAM_FLAG_MAYBE_WT_DATA;
        }

        if ((conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
            conn->tx.goaway_id <= stream_id) {
          stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

          rv = nghttp3_conn_reject_stream(conn, stream);
          if (rv != 0) {
            return rv;
          }
        }
      } else if (!nghttp3_client_stream_uni(stream_id)) {
        /* server does not expect to receive new server initiated
           bidirectional or unidirectional stream from client. */
        return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
      } else {
        /* unidirectional stream */
        if (srclen == 0 && fin) {
          return 0;
        }

        rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
        if (rv != 0) {
          return rv;
        }
      }

      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
    } else if (nghttp3_server_stream_uni(stream_id) ||
               (conn_wt_enabled(conn) &&
                nghttp3_server_stream_bidi(stream_id))) {
      if (srclen == 0 && fin) {
        return 0;
      }

      rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
      if (rv != 0) {
        return rv;
      }

      if (!(stream_id & 0x2) && conn_wt_enabled(conn)) {
        stream->flags |= NGHTTP3_STREAM_FLAG_MAYBE_WT_DATA;
      }

      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
    } else {
      /* client doesn't expect to receive new bidirectional stream or
         client initiated unidirectional stream from server. */
      return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
    }
  } else if (conn->server) {
    assert(nghttp3_client_stream_bidi(stream_id) ||
           nghttp3_client_stream_uni(stream_id) ||
           (conn_wt_enabled(conn) && nghttp3_server_stream_bidi(stream_id)));
  } else {
    assert(nghttp3_client_stream_bidi(stream_id) ||
           nghttp3_server_stream_uni(stream_id) ||
           (conn_wt_enabled(conn) && nghttp3_server_stream_bidi(stream_id)));
  }

  if (srclen == 0 && !fin) {
    return 0;
  }

  if (nghttp3_stream_uni(stream_id)) {
    return nghttp3_conn_read_uni(conn, stream, src, srclen, fin, ts);
  }

  if (fin) {
    stream->flags |= NGHTTP3_STREAM_FLAG_READ_EOF;
  }
  return nghttp3_conn_read_bidi(conn, &bidi_nproc, stream, src, srclen, fin,
                                ts);
}

static nghttp3_ssize conn_read_type(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const uint8_t *src, size_t srclen,
                                    int fin) {
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  uint64_t stream_type;

  assert(srclen);

  nread = nghttp3_read_varint(rvint, src, src + srclen, fin);
  if (nread < 0) {
    return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
  }

  if (rvint->left) {
    return nread;
  }

  stream_type = rvint->acc;
  nghttp3_varint_read_state_reset(rvint);

  switch (stream_type) {
  case NGHTTP3_STREAM_TYPE_CONTROL:
    if (conn->flags & NGHTTP3_CONN_FLAG_CONTROL_OPENED) {
      return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
    }
    conn->flags |= NGHTTP3_CONN_FLAG_CONTROL_OPENED;
    stream->type = NGHTTP3_STREAM_TYPE_CONTROL;
    rstate->state = NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE;
    break;
  case NGHTTP3_STREAM_TYPE_PUSH:
    return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
  case NGHTTP3_STREAM_TYPE_QPACK_ENCODER:
    if (conn->flags & NGHTTP3_CONN_FLAG_QPACK_ENCODER_OPENED) {
      return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
    }
    conn->flags |= NGHTTP3_CONN_FLAG_QPACK_ENCODER_OPENED;
    stream->type = NGHTTP3_STREAM_TYPE_QPACK_ENCODER;
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_DECODER:
    if (conn->flags & NGHTTP3_CONN_FLAG_QPACK_DECODER_OPENED) {
      return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
    }
    conn->flags |= NGHTTP3_CONN_FLAG_QPACK_DECODER_OPENED;
    stream->type = NGHTTP3_STREAM_TYPE_QPACK_DECODER;
    break;
  case NGHTTP3_STREAM_TYPE_WT_STREAM:
    if (!conn_wt_enabled(conn)) {
      return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
    }
    stream->type = NGHTTP3_STREAM_TYPE_WT_STREAM;
    break;
  default:
    stream->type = NGHTTP3_STREAM_TYPE_UNKNOWN;
    break;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_TYPE_IDENTIFIED;

  return nread;
}

static int conn_delete_stream(nghttp3_conn *conn, nghttp3_stream *stream,
                              uint32_t flags, uint64_t rx_app_error_code,
                              uint64_t tx_app_error_code);

nghttp3_ssize nghttp3_conn_read_uni(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const uint8_t *src, size_t srclen, int fin,
                                    nghttp3_tstamp ts) {
  nghttp3_ssize nread = 0;
  nghttp3_ssize nconsumed = 0;
  int rv;

  assert(srclen || fin);

  if (!(stream->flags & NGHTTP3_STREAM_FLAG_TYPE_IDENTIFIED)) {
    if (srclen == 0 && fin) {
      /* Ignore stream if it is closed before reading stream header.
         If it is closed while reading it, return error, making it
         consistent in our code base. */
      if (stream->rstate.rvint.left) {
        return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
      }

      /* Receiving too frequent 0 length unidirectional stream is
         suspicious. */
      if (conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
        return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
      }

      return conn_delete_stream(conn, stream, NGHTTP3_STREAM_CLOSE_FLAG_NONE, 0,
                                0);
    }
    nread = conn_read_type(conn, stream, src, srclen, fin);
    if (nread < 0) {
      return (int)nread;
    }
    if (!(stream->flags & NGHTTP3_STREAM_FLAG_TYPE_IDENTIFIED)) {
      assert((size_t)nread == srclen);
      return (nghttp3_ssize)srclen;
    }

    src += nread;
    srclen -= (size_t)nread;

    if (stream->type == NGHTTP3_STREAM_TYPE_UNKNOWN) {
      /* Receiving too frequent unknown stream type is suspicious.*/
      if (conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
        return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
      }

      if (!fin) {
        rv = conn_call_stop_sending(conn, stream,
                                    NGHTTP3_H3_STREAM_CREATION_ERROR);
        if (rv != 0) {
          return rv;
        }
      }
    }

    if (srclen == 0) {
      return nread;
    }
  }

  switch (stream->type) {
  case NGHTTP3_STREAM_TYPE_CONTROL:
    if (fin) {
      return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_control(conn, stream, src, srclen, ts);
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_ENCODER:
    if (fin) {
      return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_qpack_encoder(conn, src, srclen, ts);
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_DECODER:
    if (fin) {
      return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_qpack_decoder(conn, src, srclen);
    break;
  case NGHTTP3_STREAM_TYPE_WT_STREAM:
    nconsumed =
      nghttp3_conn_read_wt_stream_uni(conn, stream, src, srclen, fin, ts);
    break;
  case NGHTTP3_STREAM_TYPE_UNKNOWN:
    nconsumed = (nghttp3_ssize)srclen;
    break;
  default:
    nghttp3_unreachable();
  }

  if (nconsumed < 0) {
    return nconsumed;
  }

  return nread + nconsumed;
}

static void conn_reset_rx_originlen(nghttp3_conn *conn) {
  conn->rx.originlen_offset = 0;
  conn->rx.originlen = 0;
}

static int frame_fin(const nghttp3_stream_read_state *rstate, size_t len) {
  return len >= rstate->left;
}

nghttp3_ssize nghttp3_conn_read_control(nghttp3_conn *conn,
                                        nghttp3_stream *stream,
                                        const uint8_t *src, size_t srclen,
                                        nghttp3_tstamp ts) {
  const uint8_t *p = src, *end = src + srclen;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  size_t nconsumed = 0;
  int busy = 0;
  size_t len;
  const uint8_t *pri_field_value = NULL;
  size_t pri_field_valuelen = 0;

  assert(srclen);

  for (; p != end || busy;) {
    busy = 0;
    switch (rstate->state) {
    case NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, end, /* fin = */ 0);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      rstate->fr.hd.type = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);
      rstate->state = NGHTTP3_CTRL_STREAM_STATE_FRAME_LENGTH;
      if (p == end) {
        return (nghttp3_ssize)nconsumed;
      }
      /* Fall through */
    case NGHTTP3_CTRL_STREAM_STATE_FRAME_LENGTH:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, end, /* fin = */ 0);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      rstate->left = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (!(conn->flags & NGHTTP3_CONN_FLAG_SETTINGS_RECVED)) {
        if (rstate->fr.hd.type != NGHTTP3_FRAME_SETTINGS) {
          return NGHTTP3_ERR_H3_MISSING_SETTINGS;
        }
        conn->flags |= NGHTTP3_CONN_FLAG_SETTINGS_RECVED;
      } else if (rstate->fr.hd.type == NGHTTP3_FRAME_SETTINGS) {
        return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
      }

      switch (rstate->fr.hd.type) {
      case NGHTTP3_FRAME_SETTINGS:
        /* SETTINGS frame might be empty. */
        if (rstate->left == 0) {
          rv = nghttp3_conn_on_settings_received(conn);
          if (rv != 0) {
            return rv;
          }

          nghttp3_stream_read_state_reset(rstate);
          break;
        }
        rstate->fr.settings.iv = &rstate->iv;
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS;
        break;
      case NGHTTP3_FRAME_GOAWAY:
        if (rstate->left == 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_GOAWAY;
        break;
      case NGHTTP3_FRAME_MAX_PUSH_ID:
        if (!conn->server) {
          return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
        }
        if (rstate->left == 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_MAX_PUSH_ID;
        break;
      case NGHTTP3_FRAME_PRIORITY_UPDATE:
        if (!conn->server) {
          return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
        }
        if (rstate->left == 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        /* We do not expect too frequent priority updates. */
        if (conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
          return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
        }

        rstate->state = NGHTTP3_CTRL_STREAM_STATE_PRIORITY_UPDATE_PRI_ELEM_ID;
        break;
      case NGHTTP3_FRAME_PRIORITY_UPDATE_PUSH_ID:
        /* We do not support push */
        return NGHTTP3_ERR_H3_ID_ERROR;
      case NGHTTP3_FRAME_ORIGIN:
        /* We do not expect too frequent ORIGIN frames. */
        if (conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
          return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
        }

        if (conn->server ||
            (!conn->callbacks.recv_origin && !conn->callbacks.end_origin)) {
          busy = 1;
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
          break;
        }

        /* ORIGIN frame might be empty */
        if (rstate->left == 0) {
          rv = conn_call_end_origin(conn);
          if (rv != 0) {
            return rv;
          }

          nghttp3_stream_read_state_reset(rstate);

          break;
        }

        conn_reset_rx_originlen(conn);

        rstate->state = NGHTTP3_CTRL_STREAM_STATE_ORIGIN_ORIGIN_LEN;

        break;
      case NGHTTP3_FRAME_CANCEL_PUSH: /* We do not support push */
      case NGHTTP3_FRAME_DATA:
      case NGHTTP3_FRAME_HEADERS:
      case NGHTTP3_FRAME_PUSH_PROMISE:
      case NGHTTP3_H2_FRAME_PRIORITY:
      case NGHTTP3_H2_FRAME_PING:
      case NGHTTP3_H2_FRAME_WINDOW_UPDATE:
      case NGHTTP3_H2_FRAME_CONTINUATION:
        return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
      default:
        /* We do not expect too frequent unknown frames. */
        if (conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
          return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
        }

        /* TODO Handle reserved frame type */
        busy = 1;
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
        break;
      }
      break;
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS:
      for (;;) {
        if (rstate->left == 0) {
          rv = nghttp3_conn_on_settings_received(conn);
          if (rv != 0) {
            return rv;
          }

          nghttp3_stream_read_state_reset(rstate);
          break;
        }

        if (p == end) {
          return (nghttp3_ssize)nconsumed;
        }

        /* Read Identifier */
        len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
        assert(len > 0);
        nread = nghttp3_read_varint(rvint, p, p + len, frame_fin(rstate, len));
        if (nread < 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        p += nread;
        nconsumed += (size_t)nread;
        rstate->left -= (uint64_t)nread;
        if (rvint->left) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_ID;
          return (nghttp3_ssize)nconsumed;
        }
        rstate->fr.settings.iv[0].id = rvint->acc;
        nghttp3_varint_read_state_reset(rvint);

        /* Read Value */
        if (rstate->left == 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        len -= (size_t)nread;
        if (len == 0) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE;
          break;
        }

        nread = nghttp3_read_varint(rvint, p, p + len, frame_fin(rstate, len));
        if (nread < 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        p += nread;
        nconsumed += (size_t)nread;
        rstate->left -= (uint64_t)nread;
        if (rvint->left) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE;
          return (nghttp3_ssize)nconsumed;
        }
        rstate->fr.settings.iv[0].value = rvint->acc;
        nghttp3_varint_read_state_reset(rvint);

        rv =
          nghttp3_conn_on_settings_entry_received(conn, &rstate->fr.settings);
        if (rv != 0) {
          return rv;
        }
      }
      break;
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS_ID:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, p + len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= (uint64_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }
      rstate->fr.settings.iv[0].id = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (rstate->left == 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE;

      if (p == end) {
        return (nghttp3_ssize)nconsumed;
      }
      /* Fall through */
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, p + len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= (uint64_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }
      rstate->fr.settings.iv[0].value = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      rv = nghttp3_conn_on_settings_entry_received(conn, &rstate->fr.settings);
      if (rv != 0) {
        return rv;
      }

      if (rstate->left) {
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS;
        break;
      }

      rv = nghttp3_conn_on_settings_received(conn);
      if (rv != 0) {
        return rv;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_GOAWAY:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, p + len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= (uint64_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      if (!conn->server && !nghttp3_client_stream_bidi((int64_t)rvint->acc)) {
        return NGHTTP3_ERR_H3_ID_ERROR;
      }
      if (conn->rx.goaway_id < (int64_t)rvint->acc) {
        return NGHTTP3_ERR_H3_ID_ERROR;
      }

      /* Receiving same GOAWAY ID is suspicious. */
      if (conn->rx.goaway_id == (int64_t)rvint->acc &&
          conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
        return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
      }

      conn->flags |= NGHTTP3_CONN_FLAG_GOAWAY_RECVED;
      conn->rx.goaway_id = (int64_t)rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (conn->callbacks.shutdown) {
        rv =
          conn->callbacks.shutdown(conn, conn->rx.goaway_id, conn->user_data);
        if (rv != 0) {
          return NGHTTP3_ERR_CALLBACK_FAILURE;
        }
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_MAX_PUSH_ID:
      /* server side only */
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, p + len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= (uint64_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      if (conn->local.uni.max_pushes > rvint->acc + 1) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      /* Receiving same MAX_PUSH_ID is suspicious. */
      if (conn->local.uni.max_pushes == rvint->acc + 1 &&
          conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
        return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
      }

      conn->local.uni.max_pushes = rvint->acc + 1;
      nghttp3_varint_read_state_reset(rvint);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_PRIORITY_UPDATE_PRI_ELEM_ID:
      /* server side only */
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, p + len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= (uint64_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      rstate->fr.priority_update.pri_elem_id = (int64_t)rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (rstate->left == 0) {
        rstate->fr.priority_update.pri = (nghttp3_pri){
          .urgency = NGHTTP3_DEFAULT_URGENCY,
        };

        rv = nghttp3_conn_on_priority_update(conn, &rstate->fr.priority_update);
        if (rv != 0) {
          return rv;
        }

        nghttp3_stream_read_state_reset(rstate);
        break;
      }

      conn->rx.pri_fieldbuflen = 0;

      rstate->state = NGHTTP3_CTRL_STREAM_STATE_PRIORITY_UPDATE;

      if (p == end) {
        return (nghttp3_ssize)nconsumed;
      }

      /* Fall through */
    case NGHTTP3_CTRL_STREAM_STATE_PRIORITY_UPDATE:
      /* We need to buffer Priority Field Value because it might be
         fragmented. */
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      assert(len > 0);
      if (conn->rx.pri_fieldbuflen == 0 && rstate->left == len) {
        /* Everything is in the input buffer.  Apply same length
           limit we impose when buffering the field. */
        if (len > sizeof(conn->rx.pri_fieldbuf)) {
          busy = 1;
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
          break;
        }

        pri_field_value = p;
        pri_field_valuelen = len;
      } else if (len + conn->rx.pri_fieldbuflen >
                 sizeof(conn->rx.pri_fieldbuf)) {
        busy = 1;
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
        break;
      } else {
        memcpy(conn->rx.pri_fieldbuf + conn->rx.pri_fieldbuflen, p, len);
        conn->rx.pri_fieldbuflen += len;

        if (rstate->left == len) {
          pri_field_value = conn->rx.pri_fieldbuf;
          pri_field_valuelen = conn->rx.pri_fieldbuflen;
        }
      }

      p += len;
      nconsumed += len;
      rstate->left -= len;

      if (rstate->left) {
        return (nghttp3_ssize)nconsumed;
      }

      rstate->fr.priority_update.pri = (nghttp3_pri){
        .urgency = NGHTTP3_DEFAULT_URGENCY,
      };

      if (nghttp3_http_parse_priority(&rstate->fr.priority_update.pri,
                                      pri_field_value,
                                      pri_field_valuelen) != 0) {
        return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
      }

      rv = nghttp3_conn_on_priority_update(conn, &rstate->fr.priority_update);
      if (rv != 0) {
        return rv;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_ORIGIN_ORIGIN_LEN:
      /* client side only */
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));

      assert(len > 0);

      for (;;) {
        nread = 0;

        for (; conn->rx.originlen_offset < sizeof(conn->rx.originlen) &&
               (size_t)nread < len;
             ++conn->rx.originlen_offset, ++nread) {
          conn->rx.originlen <<= 8;
          conn->rx.originlen += *p++;
        }

        nconsumed += (size_t)nread;
        rstate->left -= (uint64_t)nread;
        len -= (size_t)nread;

        if (conn->rx.originlen_offset < sizeof(conn->rx.originlen)) {
          /* Needs another byte to parse Origin-Len */
          if (rstate->left == 0) {
            return NGHTTP3_ERR_H3_FRAME_ERROR;
          }

          return (nghttp3_ssize)nconsumed;
        }

        if (conn->rx.originlen == 0 || rstate->left < conn->rx.originlen) {
          /* While this seems OK in very loose RFC, allowing this
             sounds like an atack vector. */
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        if (len < conn->rx.originlen) {
          /* ASCII-Origin does not fit into this buffer.  Needs
             buffering. */
          if (conn->rx.originbuf == NULL) {
            conn->rx.originbuf = nghttp3_mem_malloc(conn->mem, UINT16_MAX);
            if (conn->rx.originbuf == NULL) {
              return NGHTTP3_ERR_NOMEM;
            }
          }

          memcpy(conn->rx.originbuf, p, len);

          /* No need to update p because we will return very soon. */
          nconsumed += len;
          rstate->left -= len;

          conn->rx.originbuflen = len;

          rstate->state = NGHTTP3_CTRL_STREAM_STATE_ORIGIN_ASCII_ORIGIN;

          return (nghttp3_ssize)nconsumed;
        }

        rv = conn_call_recv_origin(conn, p, conn->rx.originlen);
        if (rv != 0) {
          return rv;
        }

        p += conn->rx.originlen;
        nconsumed += conn->rx.originlen;
        rstate->left -= conn->rx.originlen;

        if (rstate->left == 0) {
          rv = conn_call_end_origin(conn);
          if (rv != 0) {
            return rv;
          }

          nghttp3_stream_read_state_reset(rstate);

          break;
        }

        len -= conn->rx.originlen;

        conn_reset_rx_originlen(conn);

        if (p == end) {
          return (nghttp3_ssize)nconsumed;
        }
      }

      break;
    case NGHTTP3_CTRL_STREAM_STATE_ORIGIN_ASCII_ORIGIN:
      /* client side only */
      len = nghttp3_min(conn->rx.originlen - conn->rx.originbuflen,
                        (size_t)(end - p));

      assert(len > 0);

      memcpy(conn->rx.originbuf + conn->rx.originbuflen, p, len);

      conn->rx.originbuflen += len;
      p += len;
      nconsumed += len;
      rstate->left -= len;

      if (conn->rx.originbuflen < conn->rx.originlen) {
        return (nghttp3_ssize)nconsumed;
      }

      rv = conn_call_recv_origin(conn, conn->rx.originbuf, conn->rx.originlen);
      if (rv != 0) {
        return rv;
      }

      if (rstate->left) {
        conn_reset_rx_originlen(conn);

        rstate->state = NGHTTP3_CTRL_STREAM_STATE_ORIGIN_ORIGIN_LEN;

        break;
      }

      rv = conn_call_end_origin(conn);
      if (rv != 0) {
        return rv;
      }

      nghttp3_stream_read_state_reset(rstate);

      break;
    case NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      p += len;
      nconsumed += len;
      rstate->left -= len;

      if (rstate->left) {
        return (nghttp3_ssize)nconsumed;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    default:
      nghttp3_unreachable();
    }
  }

  return (nghttp3_ssize)nconsumed;
}

static int conn_unlink_wt_session(nghttp3_conn *conn,
                                  nghttp3_stream *wt_ctrl_stream) {
  nghttp3_wt_session *wt_session = wt_ctrl_stream->wt.session;
  nghttp3_stream *stream, *next;
  int rv;
  (void)rv;

  for (stream = wt_session->head; stream;) {
    next = stream->wt.next;

    assert(stream->wt.session);

    stream->wt.session = NULL;
    stream->wt.prev = stream->wt.next = NULL;

    rv = nghttp3_conn_shutdown_wt_data_stream(conn, stream,
                                              NGHTTP3_WT_SESSION_GONE);
    if (rv != 0) {
      return rv;
    }

    stream = next;
  }

  wt_session->head = NULL;

  return 0;
}

static int conn_delete_stream(nghttp3_conn *conn, nghttp3_stream *stream,
                              uint32_t flags, uint64_t rx_app_error_code,
                              uint64_t tx_app_error_code) {
  int rv;
  uint64_t app_error_code;

  rv = conn_call_deferred_consume(conn, stream,
                                  nghttp3_stream_get_buffered_datalen(stream));
  if (rv != 0) {
    return rv;
  }

  if (stream->qpack_blocked_pe.index != NGHTTP3_PQ_BAD_INDEX) {
    nghttp3_conn_qpack_blocked_streams_remove(conn, stream);

    rv = nghttp3_qpack_decoder_cancel_stream(&conn->qdec, stream->node.id);
    if (rv != 0) {
      return rv;
    }
  }

  if (conn->callbacks.stream_close2) {
    rv = conn->callbacks.stream_close2(conn, flags, stream->node.id,
                                       rx_app_error_code, tx_app_error_code,
                                       conn->user_data, stream->user_data);
    if (rv != 0) {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
  } else if (conn->callbacks.stream_close) {
    app_error_code = NGHTTP3_H3_NO_ERROR;

    if (flags & NGHTTP3_STREAM_CLOSE_FLAG_RX_APP_ERROR_CODE_SET) {
      app_error_code = rx_app_error_code;
    }

    if (app_error_code == NGHTTP3_H3_NO_ERROR &&
        (flags & NGHTTP3_STREAM_CLOSE_FLAG_TX_APP_ERROR_CODE_SET)) {
      app_error_code = tx_app_error_code;
    }

    rv = conn->callbacks.stream_close(conn, stream->node.id, app_error_code,
                                      conn->user_data, stream->user_data);
    if (rv != 0) {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
  }

  if (nghttp3_stream_wt_ctrl(stream)) {
    rv = conn_unlink_wt_session(conn, stream);
    if (rv != 0) {
      return rv;
    }
  } else if (nghttp3_stream_wt_data(stream)) {
    nghttp3_wt_session_remove_stream(stream->wt.session, stream);
  }

  if (conn->server && nghttp3_client_stream_bidi(stream->node.id)) {
    assert(conn->remote.bidi.num_streams > 0);

    --conn->remote.bidi.num_streams;
  }

  rv =
    nghttp3_map_remove(&conn->streams, (nghttp3_map_key_type)stream->node.id);

  assert(0 == rv);

  nghttp3_stream_del(stream);

  return 0;
}

static int conn_process_blocked_stream_data(nghttp3_conn *conn,
                                            nghttp3_stream *stream,
                                            nghttp3_tstamp ts) {
  nghttp3_buf *buf;
  size_t nproc;
  nghttp3_ssize nconsumed;
  int rv;
  size_t len;

  assert(nghttp3_client_stream_bidi(stream->node.id));

  for (;;) {
    len = nghttp3_ringbuf_len(&stream->inq);
    if (len == 0) {
      break;
    }

    buf = nghttp3_ringbuf_get(&stream->inq, 0);

    nconsumed = nghttp3_conn_read_bidi(
      conn, &nproc, stream, buf->pos, nghttp3_buf_len(buf),
      len == 1 && (stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF), ts);
    if (nconsumed < 0) {
      return (int)nconsumed;
    }

    buf->pos += nproc;

    rv = conn_call_deferred_consume(conn, stream, (size_t)nconsumed);
    if (rv != 0) {
      return rv;
    }

    if (nghttp3_buf_len(buf) == 0) {
      nghttp3_buf_free(buf, stream->mem);
      nghttp3_ringbuf_pop_front(&stream->inq);
    }

    if (stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) {
      break;
    }
  }

  return 0;
}

nghttp3_ssize nghttp3_conn_read_qpack_encoder(nghttp3_conn *conn,
                                              const uint8_t *src, size_t srclen,
                                              nghttp3_tstamp ts) {
  nghttp3_ssize nconsumed =
    nghttp3_qpack_decoder_read_encoder(&conn->qdec, src, srclen);
  nghttp3_stream *stream;
  int rv;

  if (nconsumed < 0) {
    return nconsumed;
  }

  for (; !nghttp3_pq_empty(&conn->qpack_blocked_streams);) {
    stream = nghttp3_struct_of(nghttp3_pq_top(&conn->qpack_blocked_streams),
                               nghttp3_stream, qpack_blocked_pe);
    if (nghttp3_qpack_stream_context_get_ricnt2(&stream->qpack_sctx) >
        nghttp3_qpack_decoder_get_icnt(&conn->qdec)) {
      break;
    }

    nghttp3_conn_qpack_blocked_streams_pop(conn);
    stream->qpack_blocked_pe.index = NGHTTP3_PQ_BAD_INDEX;
    stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED;

    rv = conn_process_blocked_stream_data(conn, stream, ts);
    if (rv != 0) {
      return rv;
    }
  }

  return nconsumed;
}

nghttp3_ssize nghttp3_conn_read_qpack_decoder(nghttp3_conn *conn,
                                              const uint8_t *src,
                                              size_t srclen) {
  return nghttp3_qpack_encoder_read_decoder(&conn->qenc, src, srclen);
}

static nghttp3_tnode *stream_get_sched_node(nghttp3_stream *stream) {
  return &stream->node;
}

static int conn_update_stream_priority(nghttp3_conn *conn,
                                       nghttp3_stream *stream,
                                       const nghttp3_pri *pri) {
  assert(nghttp3_client_stream_bidi(stream->node.id));

  if (nghttp3_pri_eq(&stream->node.pri, pri)) {
    return 0;
  }

  nghttp3_conn_unschedule_stream(conn, stream);

  stream->node.pri = *pri;

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_schedule_stream(conn, stream);
  }

  return 0;
}

nghttp3_ssize nghttp3_conn_read_bidi(nghttp3_conn *conn, size_t *pnproc,
                                     nghttp3_stream *stream, const uint8_t *src,
                                     size_t srclen, int fin,
                                     nghttp3_tstamp ts) {
  const uint8_t *p = src, *end = src ? src + srclen : src;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_stream *wt_ctrl_stream;
  nghttp3_ssize nread;
  size_t nconsumed = 0;
  int busy = 0;
  size_t len;

  if (stream->flags & NGHTTP3_STREAM_FLAG_SHUT_RD) {
    *pnproc = srclen;

    return (nghttp3_ssize)srclen;
  }

  if (stream->flags & (NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED |
                       NGHTTP3_STREAM_FLAG_WT_SESSION_BLOCKED)) {
    *pnproc = 0;

    if (srclen == 0) {
      return 0;
    }

    rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
    if (rv != 0) {
      return rv;
    }
    return 0;
  }

  for (; p != end || busy;) {
    busy = 0;
    switch (rstate->state) {
    case NGHTTP3_REQ_STREAM_STATE_FRAME_TYPE:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, end, fin);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->fr.hd.type = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);
      rstate->state = NGHTTP3_REQ_STREAM_STATE_FRAME_LENGTH;
      if (p == end) {
        goto almost_done;
      }
      /* Fall through */
    case NGHTTP3_REQ_STREAM_STATE_FRAME_LENGTH:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, end, fin);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->left = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      switch (rstate->fr.hd.type) {
      case NGHTTP3_FRAME_DATA:
        rv = nghttp3_stream_transit_rx_http_state(
          stream, NGHTTP3_HTTP_EVENT_DATA_BEGIN);
        if (rv != 0) {
          return rv;
        }
        /* DATA frame might be empty. */
        if (rstate->left == 0) {
          rv = nghttp3_stream_transit_rx_http_state(
            stream, NGHTTP3_HTTP_EVENT_DATA_END);
          assert(0 == rv);

          nghttp3_stream_read_state_reset(rstate);
          break;
        }
        rstate->state = NGHTTP3_REQ_STREAM_STATE_DATA;
        break;
      case NGHTTP3_FRAME_HEADERS:
        rv = nghttp3_stream_transit_rx_http_state(
          stream, NGHTTP3_HTTP_EVENT_HEADERS_BEGIN);
        if (rv != 0) {
          return rv;
        }
        if (rstate->left == 0) {
          rv = nghttp3_stream_empty_headers_allowed(stream);
          if (rv != 0) {
            return rv;
          }

          rv = nghttp3_stream_transit_rx_http_state(
            stream, NGHTTP3_HTTP_EVENT_HEADERS_END);
          assert(0 == rv);

          nghttp3_stream_read_state_reset(rstate);
          break;
        }

        switch (stream->rx.hstate) {
        case NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN:
        case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
          rv = conn_call_begin_headers(conn, stream);
          break;
        case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
        case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
          rv = conn_call_begin_trailers(conn, stream);
          break;
        default:
          nghttp3_unreachable();
        }

        if (rv != 0) {
          return rv;
        }

        rstate->state = NGHTTP3_REQ_STREAM_STATE_HEADERS;
        break;
      case NGHTTP3_EXFR_WT_STREAM_BIDI:
        if (!nghttp3_stream_wt_data(stream) &&
            !(stream->flags & NGHTTP3_STREAM_FLAG_MAYBE_WT_DATA)) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_MAYBE_WT_DATA;

        if (conn->server) {
          if (stream->rx.hstate != NGHTTP3_HTTP_STATE_REQ_INITIAL) {
            return NGHTTP3_ERR_H3_FRAME_ERROR;
          }
        } else if (stream->rx.hstate != NGHTTP3_HTTP_STATE_RESP_INITIAL) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        /* rstate->left is Session ID */
        rv = nghttp3_conn_on_wt_stream(conn, stream, (int64_t)rstate->left);
        if (rv != 0) {
          if (rv != NGHTTP3_ERR_WT_SESSION_GONE) {
            return rv;
          }

          stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

          rv = nghttp3_conn_abort_stream(conn, stream, NGHTTP3_WT_SESSION_GONE);
          if (rv != 0) {
            return rv;
          }

          break;
        }

        rstate->state = NGHTTP3_REQ_STREAM_STATE_BEFORE_WT_DATA;

        break;
      case NGHTTP3_FRAME_PUSH_PROMISE: /* We do not support push */
      case NGHTTP3_FRAME_CANCEL_PUSH:
      case NGHTTP3_FRAME_SETTINGS:
      case NGHTTP3_FRAME_GOAWAY:
      case NGHTTP3_FRAME_MAX_PUSH_ID:
      case NGHTTP3_FRAME_PRIORITY_UPDATE:
      case NGHTTP3_FRAME_PRIORITY_UPDATE_PUSH_ID:
      case NGHTTP3_H2_FRAME_PRIORITY:
      case NGHTTP3_H2_FRAME_PING:
      case NGHTTP3_H2_FRAME_WINDOW_UPDATE:
      case NGHTTP3_H2_FRAME_CONTINUATION:
        return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
      default:
        /* We do not expect too frequent unknown frames. */
        if (conn_glitch_ratelim_drain(conn, 1, ts) != 0) {
          return NGHTTP3_ERR_H3_EXCESSIVE_LOAD;
        }

        stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_MAYBE_WT_DATA;

        /* TODO Handle reserved frame type */
        busy = 1;
        rstate->state = NGHTTP3_REQ_STREAM_STATE_IGN_FRAME;
        break;
      }
      break;
    case NGHTTP3_REQ_STREAM_STATE_DATA:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      nread = nghttp3_conn_on_data(conn, stream, p, len);
      if (nread < 0) {
        if (nread != NGHTTP3_ERR_WT_SESSION_GONE) {
          return nread;
        }

        rv = nghttp3_conn_shutdown_wt_session(conn, stream,
                                              NGHTTP3_WT_SESSION_GONE);
        if (rv != 0) {
          return rv;
        }

        /* Now that the stream is in
           NGHTTP3_REQ_STREAM_STATE_IGN_REST, end_stream callback is
           not called. */

        /* Pretend that all stream data have been consumed */
        nconsumed += len;

        goto almost_done;
      }
      p += len;
      nconsumed += (size_t)nread;
      rstate->left -= len;

      if (rstate->left) {
        goto almost_done;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_DATA_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_HEADERS:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      nread =
        nghttp3_conn_on_headers(conn, stream, p, len, len == rstate->left);
      if (nread < 0) {
        return nread;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= (uint64_t)nread;

      if (stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) {
        if (p != end && nghttp3_stream_get_buffered_datalen(stream) == 0) {
          rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
          if (rv != 0) {
            return rv;
          }
        }
        *pnproc = (size_t)(p - src);
        return (nghttp3_ssize)nconsumed;
      }

      if (rstate->left) {
        goto almost_done;
      }

      switch (stream->rx.hstate) {
      case NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN:
        rv = nghttp3_http_on_request_headers(&stream->rx.http);
        break;
      case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
        rv = nghttp3_http_on_response_headers(&stream->rx.http);
        break;
      case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
      case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
        rv = 0;
        break;
      default:
        nghttp3_unreachable();
      }

      if (rv != 0) {
        return rv;
      }

      switch (stream->rx.hstate) {
      case NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN:
        /* Only server utilizes priority information to schedule
           streams. */
        if (conn->server &&
            (stream->rx.http.flags & NGHTTP3_HTTP_FLAG_PRIORITY) &&
            !(stream->flags & NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED) &&
            !(stream->flags & NGHTTP3_STREAM_FLAG_SERVER_PRIORITY_SET)) {
          rv = conn_update_stream_priority(conn, stream, &stream->rx.http.pri);
          if (rv != 0) {
            return rv;
          }
        }

        rv = conn_call_end_headers(conn, stream, p == end && fin);
        break;
      case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
        rv = conn_call_end_headers(conn, stream, p == end && fin);
        break;
      case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
      case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
        rv = conn_call_end_trailers(conn, stream, p == end && fin);
        break;
      default:
        nghttp3_unreachable();
      }

      if (rv != 0) {
        return rv;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_HEADERS_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);

      if (conn->server) {
        if (stream->rx.hstate == NGHTTP3_HTTP_STATE_REQ_HEADERS_END) {
          if (stream->wt.session && (stream->wt.session->flags &
                                     NGHTTP3_WT_SESSION_FLAG_RESP_SUBMITTED)) {
            /* Server has submitted WebTransport session. */
            rv = nghttp3_conn_on_wt_session_confirmed(conn, stream, ts);
            if (rv != 0) {
              return rv;
            }
          } else if (stream->rx.http.flags & NGHTTP3_HTTP_FLAG_WEBTRANSPORT) {
            if (stream->flags & NGHTTP3_STREAM_FLAG_RESP_SUBMITTED) {
              /* Server refused WebTransport upgrade request */
              stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

              rv = conn_call_stop_sending(conn, stream, NGHTTP3_H3_NO_ERROR);
              if (rv != 0) {
                return rv;
              }
            } else {
              /* Server has not submitted response */
              stream->flags |= NGHTTP3_STREAM_FLAG_WT_SESSION_BLOCKED;
              if (p != end) {
                rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
                if (rv != 0) {
                  return rv;
                }
              }

              *pnproc = (size_t)(p - src);

              return (nghttp3_ssize)nconsumed;
            }
          }
        }
      } else if (stream->rx.hstate == NGHTTP3_HTTP_STATE_RESP_HEADERS_END &&
                 stream->wt.session) {
        if (stream->rx.http.status_code / 100 == 2) {
          rv = nghttp3_conn_on_wt_session_confirmed(conn, stream, ts);
          if (rv != 0) {
            return rv;
          }
        } else {
          /* Server refused WebTransport negotiation.  Reset the session
             stream.  This could be a redirect, but client is instructed
             not to follow the redirect automatically.  Most of the
             case, we cannot do anything but just close the stream. */
          stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

          rv = nghttp3_conn_abort_stream(conn, stream,
                                         NGHTTP3_H3_REQUEST_CANCELLED);
          if (rv != 0) {
            return rv;
          }
        }
      }

      break;
    case NGHTTP3_REQ_STREAM_STATE_IGN_FRAME:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      p += len;
      nconsumed += len;
      rstate->left -= len;

      if (rstate->left) {
        goto almost_done;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_BEFORE_WT_DATA:
      rstate->state = NGHTTP3_REQ_STREAM_STATE_WT_DATA;

      assert(stream->wt.session);

      wt_ctrl_stream =
        nghttp3_conn_find_stream(conn, stream->wt.session->session_id);

      if (!(wt_ctrl_stream->wt.session->flags &
            NGHTTP3_WT_SESSION_FLAG_CONFIRMED)) {
        stream->flags |= NGHTTP3_STREAM_FLAG_WT_SESSION_BLOCKED;

        if (p != end) {
          rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
          if (rv != 0) {
            return rv;
          }
        }

        *pnproc = (size_t)(p - src);

        return (nghttp3_ssize)nconsumed;
      }

      break;
    case NGHTTP3_REQ_STREAM_STATE_WT_DATA:
      rv = conn_call_recv_wt_data(conn, stream, p, (size_t)(end - p));
      if (rv != 0) {
        return rv;
      }

      p = end;

      goto almost_done;
    case NGHTTP3_REQ_STREAM_STATE_IGN_REST:
      nconsumed += (size_t)(end - p);
      *pnproc = (size_t)(end - src);
      return (nghttp3_ssize)nconsumed;
    }
  }

almost_done:
  if (fin) {
    switch (rstate->state) {
    case NGHTTP3_REQ_STREAM_STATE_FRAME_TYPE:
      if (rvint->left) {
        return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
      }
      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_MSG_END);
      if (rv != 0) {
        return rv;
      }

      /* Fall through */
      /* When a stream is closed without any data */
    case NGHTTP3_REQ_STREAM_STATE_BEFORE_WT_DATA:
    case NGHTTP3_REQ_STREAM_STATE_WT_DATA:
      rv = conn_call_end_stream(conn, stream);
      if (rv != 0) {
        return rv;
      }

      break;
    case NGHTTP3_REQ_STREAM_STATE_IGN_REST:
      break;
    default:
      return NGHTTP3_ERR_H3_FRAME_ERROR;
    }
  }

  *pnproc = (size_t)(p - src);
  return (nghttp3_ssize)nconsumed;
}

nghttp3_ssize nghttp3_conn_on_data(nghttp3_conn *conn, nghttp3_stream *stream,
                                   const uint8_t *data, size_t datalen) {
  int rv;

  rv = nghttp3_http_on_data_chunk(stream, datalen);
  if (rv != 0) {
    return rv;
  }

  if (!stream->wt.session) {
    return conn_call_recv_data(conn, stream, data, datalen);
  }

  /* The stream data must be buffered until WebTransport session has
     been confirmed. */
  assert(stream->wt.session->flags & NGHTTP3_WT_SESSION_FLAG_CONFIRMED);

  rv = nghttp3_conn_read_wt_ctrl_stream(conn, stream, data, datalen);
  if (rv != 0) {
    return rv;
  }

  /* WebTransport control stream has consumed all data */
  return (nghttp3_ssize)datalen;
}

static nghttp3_pq *conn_get_sched_pq(nghttp3_conn *conn, nghttp3_tnode *tnode) {
  assert(tnode->pri.urgency < NGHTTP3_URGENCY_LEVELS);

  return &conn->sched[tnode->pri.urgency].spq;
}

static nghttp3_ssize conn_decode_headers(nghttp3_conn *conn,
                                         nghttp3_stream *stream,
                                         const uint8_t *src, size_t srclen,
                                         int fin) {
  nghttp3_ssize nread;
  int rv;
  nghttp3_qpack_decoder *qdec = &conn->qdec;
  nghttp3_qpack_nv nv;
  uint8_t flags;
  nghttp3_buf buf;
  nghttp3_recv_header recv_header = NULL;
  nghttp3_http_state *http;
  int request = 0;
  int trailers = 0;

  switch (stream->rx.hstate) {
  case NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN:
    request = 1;
    /* Fall through */
  case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
    recv_header = conn->callbacks.recv_header;
    break;
  case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
    request = 1;
    /* Fall through */
  case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
    trailers = 1;
    recv_header = conn->callbacks.recv_trailer;
    break;
  default:
    nghttp3_unreachable();
  }
  http = &stream->rx.http;

  nghttp3_buf_wrap_init(&buf, (uint8_t *)src, srclen);
  buf.last = buf.end;

  for (;;) {
    nread =
      nghttp3_qpack_decoder_read_request(qdec, &stream->qpack_sctx, &nv, &flags,
                                         buf.pos, nghttp3_buf_len(&buf), fin);

    if (nread < 0) {
      return (int)nread;
    }

    buf.pos += nread;

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_BLOCKED) {
      if (conn->local.settings.qpack_blocked_streams <=
          nghttp3_pq_size(&conn->qpack_blocked_streams)) {
        return NGHTTP3_ERR_QPACK_DECOMPRESSION_FAILED;
      }

      stream->flags |= NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED;
      rv = nghttp3_conn_qpack_blocked_streams_push(conn, stream);
      if (rv != 0) {
        return rv;
      }
      break;
    }

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_FINAL) {
      nghttp3_qpack_stream_context_reset(&stream->qpack_sctx);
      break;
    }

    if (nread == 0) {
      break;
    }

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_EMIT) {
      rv = nghttp3_http_on_header(
        http, &nv, request, trailers,
        conn->server && conn->local.settings.enable_connect_protocol);
      switch (rv) {
      case NGHTTP3_ERR_MALFORMED_HTTP_HEADER:
        break;
      case NGHTTP3_ERR_REMOVE_HTTP_HEADER:
        rv = 0;
        break;
      case 0:
        if (recv_header) {
          rv = recv_header(conn, stream->node.id, nv.token, nv.name, nv.value,
                           nv.flags, conn->user_data, stream->user_data);
          if (rv != 0) {
            rv = NGHTTP3_ERR_CALLBACK_FAILURE;
          }
        }
        break;
      default:
        nghttp3_unreachable();
      }

      nghttp3_rcbuf_decref(nv.name);
      nghttp3_rcbuf_decref(nv.value);

      if (rv != 0) {
        return rv;
      }
    }
  }

  return buf.pos - src;
}

nghttp3_ssize nghttp3_conn_on_headers(nghttp3_conn *conn,
                                      nghttp3_stream *stream,
                                      const uint8_t *src, size_t srclen,
                                      int fin) {
  if (srclen == 0 && !fin) {
    return 0;
  }

  return conn_decode_headers(conn, stream, src, srclen, fin);
}

int nghttp3_conn_on_settings_entry_received(nghttp3_conn *conn,
                                            const nghttp3_frame_settings *fr) {
  const nghttp3_settings_entry *ent = &fr->iv[0];
  nghttp3_proto_settings *dest = &conn->remote.settings;

  /* TODO Check for duplicates */
  switch (ent->id) {
  case NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE:
    dest->max_field_section_size = ent->value;
    break;
  case NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY:
    if (dest->qpack_max_dtable_capacity != 0) {
      return NGHTTP3_ERR_H3_SETTINGS_ERROR;
    }

    if (ent->value == 0) {
      break;
    }

    dest->qpack_max_dtable_capacity = (size_t)ent->value;

    nghttp3_qpack_encoder_set_max_dtable_capacity(&conn->qenc,
                                                  (size_t)ent->value);
    break;
  case NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS:
    if (dest->qpack_blocked_streams != 0) {
      return NGHTTP3_ERR_H3_SETTINGS_ERROR;
    }

    if (ent->value == 0) {
      break;
    }

    dest->qpack_blocked_streams = (size_t)ent->value;

    nghttp3_qpack_encoder_set_max_blocked_streams(
      &conn->qenc, (size_t)nghttp3_min(100, ent->value));
    break;
  case NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL:
    if (conn->server) {
      break;
    }

    switch (ent->value) {
    case 0:
      if (dest->enable_connect_protocol) {
        return NGHTTP3_ERR_H3_SETTINGS_ERROR;
      }

      break;
    case 1:
      break;
    default:
      return NGHTTP3_ERR_H3_SETTINGS_ERROR;
    }

    dest->enable_connect_protocol = (uint8_t)ent->value;
    break;
  case NGHTTP3_SETTINGS_ID_H3_DATAGRAM:
    switch (ent->value) {
    case 0:
    case 1:
      break;
    default:
      return NGHTTP3_ERR_H3_SETTINGS_ERROR;
    }

    dest->h3_datagram = (uint8_t)ent->value;
    break;
  case NGHTTP3_SETTINGS_ID_WT_ENABLED:
    /* compat for pre draft-15 */
  case NGHTTP3_SETTINGS_ID_WT_MAX_SESSIONS:
  case NGHTTP3_SETTINGS_ID_WT_MAX_SESSIONS_DRAFT7:
    /* compat for ancient draft */
  case NGHTTP3_SETTINGS_ID_ENABLE_WEBTRANSPORT_DRAFT2:
    dest->wt_enabled = ent->value != 0;
    break;
  case NGHTTP3_H2_SETTINGS_ID_ENABLE_PUSH:
  case NGHTTP3_H2_SETTINGS_ID_MAX_CONCURRENT_STREAMS:
  case NGHTTP3_H2_SETTINGS_ID_INITIAL_WINDOW_SIZE:
  case NGHTTP3_H2_SETTINGS_ID_MAX_FRAME_SIZE:
    return NGHTTP3_ERR_H3_SETTINGS_ERROR;
  default:
    /* Ignore unknown settings ID */
    break;
  }

  return 0;
}

static int abort_wt_session(void *data, void *ptr) {
  nghttp3_conn *conn = ptr;
  nghttp3_stream *stream = data;

  if (!nghttp3_stream_wt_ctrl(stream)) {
    return 0;
  }

  stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

  return nghttp3_conn_abort_stream(conn, stream,
                                   NGHTTP3_H3_GENERAL_PROTOCOL_ERROR);
}

int nghttp3_conn_on_settings_received(nghttp3_conn *conn) {
  int rv;

  conn->flags |= NGHTTP3_CONN_FLAG_SETTINGS_RECVED;

  rv = conn_call_recv_settings(conn);
  if (rv != 0) {
    return rv;
  }

  if (!conn->local.settings.wt_enabled || conn_wt_enabled(conn)) {
    return 0;
  }

  return nghttp3_map_each(&conn->streams, abort_wt_session, conn);
}

static int
conn_on_priority_update_stream(nghttp3_conn *conn,
                               const nghttp3_frame_priority_update *fr) {
  int64_t stream_id = fr->pri_elem_id;
  nghttp3_stream *stream;
  int rv;

  if (!nghttp3_client_stream_bidi(stream_id) ||
      nghttp3_ord_stream_id(stream_id) > conn->remote.bidi.max_client_streams) {
    return NGHTTP3_ERR_H3_ID_ERROR;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    if ((conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
        conn->tx.goaway_id <= stream_id) {
      /* Connection is going down.  Ignore priority signal. */
      return 0;
    }

    rv = conn_bidi_idtr_open(conn, stream_id);
    if (rv != 0) {
      if (nghttp3_err_is_fatal(rv)) {
        return rv;
      }

      assert(rv == NGHTTP3_ERR_STREAM_IN_USE);

      /* The stream is gone.  Just ignore. */
      return 0;
    }

    conn->rx.max_stream_id_bidi =
      nghttp3_max(conn->rx.max_stream_id_bidi, stream_id);
    rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
    if (rv != 0) {
      return rv;
    }

    stream->node.pri = fr->pri;
    stream->flags |= NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED;

    if (conn_wt_enabled(conn)) {
      stream->flags |= NGHTTP3_STREAM_FLAG_MAYBE_WT_DATA;
    }

    stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;

    return 0;
  }

  if (stream->flags & NGHTTP3_STREAM_FLAG_SERVER_PRIORITY_SET) {
    return 0;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED;

  return conn_update_stream_priority(conn, stream, &fr->pri);
}

int nghttp3_conn_on_priority_update(nghttp3_conn *conn,
                                    const nghttp3_frame_priority_update *fr) {
  assert(conn->server);
  assert(fr->type == NGHTTP3_FRAME_PRIORITY_UPDATE);

  return conn_on_priority_update_stream(conn, fr);
}

static int conn_stream_acked_data(nghttp3_stream *stream, int64_t stream_id,
                                  uint64_t datalen, void *user_data) {
  nghttp3_conn *conn = stream->conn;
  int rv;

  if (!conn->callbacks.acked_stream_data) {
    return 0;
  }

  rv = conn->callbacks.acked_stream_data(conn, stream_id, datalen,
                                         conn->user_data, user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

int nghttp3_conn_create_stream(nghttp3_conn *conn, nghttp3_stream **pstream,
                               int64_t stream_id) {
  nghttp3_stream *stream;
  int rv;
  static const nghttp3_stream_callbacks callbacks = {
    .acked_data = conn_stream_acked_data,
  };

  rv = nghttp3_stream_new(&stream, stream_id, &callbacks,
                          &conn->out_chunk_objalloc, &conn->stream_objalloc,
                          conn->mem);
  if (rv != 0) {
    return rv;
  }

  stream->conn = conn;

  rv = nghttp3_map_insert(&conn->streams, (nghttp3_map_key_type)stream->node.id,
                          stream);
  if (rv != 0) {
    nghttp3_stream_del(stream);
    return rv;
  }

  if (conn->server && nghttp3_client_stream_bidi(stream_id)) {
    ++conn->remote.bidi.num_streams;
  }

  *pstream = stream;

  return 0;
}

nghttp3_stream *nghttp3_conn_find_stream(const nghttp3_conn *conn,
                                         int64_t stream_id) {
  return nghttp3_map_find(&conn->streams, (nghttp3_map_key_type)stream_id);
}

int nghttp3_conn_bind_control_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream;
  nghttp3_frame *fr;
  int rv;

  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);
  assert(!conn->server || nghttp3_server_stream_uni(stream_id));
  assert(conn->server || nghttp3_client_stream_uni(stream_id));

  if (conn->tx.ctrl) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
  if (rv != 0) {
    return rv;
  }

  stream->type = NGHTTP3_STREAM_TYPE_CONTROL;

  conn->tx.ctrl = stream;

  rv = nghttp3_stream_write_stream_type(stream);
  if (rv != 0) {
    return rv;
  }

  rv = nghttp3_stream_frq_emplace(stream, &fr);
  if (rv != 0) {
    return rv;
  }

  fr->settings = (nghttp3_frame_settings){
    .type = NGHTTP3_FRAME_SETTINGS,
    .local_settings = &conn->local.settings,
  };

  if (conn->local.settings.origin_list) {
    assert(conn->server);

    rv = nghttp3_stream_frq_emplace(stream, &fr);
    if (rv != 0) {
      return rv;
    }

    fr->origin = (nghttp3_frame_origin){
      .type = NGHTTP3_FRAME_ORIGIN,
      .origin_list = *conn->local.settings.origin_list,
    };
  }

  return 0;
}

int nghttp3_conn_bind_qpack_streams(nghttp3_conn *conn, int64_t qenc_stream_id,
                                    int64_t qdec_stream_id) {
  nghttp3_stream *stream;
  int rv;

  assert(qenc_stream_id >= 0);
  assert(qenc_stream_id <= (int64_t)NGHTTP3_MAX_VARINT);
  assert(qdec_stream_id >= 0);
  assert(qdec_stream_id <= (int64_t)NGHTTP3_MAX_VARINT);
  assert(!conn->server || nghttp3_server_stream_uni(qenc_stream_id));
  assert(!conn->server || nghttp3_server_stream_uni(qdec_stream_id));
  assert(conn->server || nghttp3_client_stream_uni(qenc_stream_id));
  assert(conn->server || nghttp3_client_stream_uni(qdec_stream_id));

  if (conn->tx.qenc || conn->tx.qdec) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, qenc_stream_id);
  if (rv != 0) {
    return rv;
  }

  stream->type = NGHTTP3_STREAM_TYPE_QPACK_ENCODER;

  conn->tx.qenc = stream;

  rv = nghttp3_stream_write_stream_type(stream);
  if (rv != 0) {
    return rv;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, qdec_stream_id);
  if (rv != 0) {
    return rv;
  }

  stream->type = NGHTTP3_STREAM_TYPE_QPACK_DECODER;

  conn->tx.qdec = stream;

  return nghttp3_stream_write_stream_type(stream);
}

static nghttp3_ssize conn_writev_stream(nghttp3_conn *conn, int64_t *pstream_id,
                                        int *pfin, nghttp3_vec *vec,
                                        size_t veccnt, nghttp3_stream *stream) {
  int rv;
  size_t n;

  assert(veccnt > 0);

  /* If stream is blocked by read callback, don't attempt to fill
     more. */
  if (!(stream->flags & NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED)) {
    rv = nghttp3_stream_fill_outq(stream);
    if (rv != 0) {
      return rv;
    }
  }

  if (!nghttp3_stream_uni(stream->node.id) && conn->tx.qenc &&
      !nghttp3_stream_is_blocked(conn->tx.qenc)) {
    n = nghttp3_stream_writev(conn->tx.qenc, pfin, vec, veccnt);
    if (n) {
      *pstream_id = conn->tx.qenc->node.id;
      return (nghttp3_ssize)n;
    }
  }

  n = nghttp3_stream_writev(stream, pfin, vec, veccnt);
  /* We might just want to write stream fin without sending any stream
     data. */
  if (n == 0 && *pfin == 0) {
    return 0;
  }

  *pstream_id = stream->node.id;

  return (nghttp3_ssize)n;
}

nghttp3_ssize nghttp3_conn_writev_stream(nghttp3_conn *conn,
                                         int64_t *pstream_id, int *pfin,
                                         nghttp3_vec *vec, size_t veccnt) {
  nghttp3_ssize ncnt;
  nghttp3_stream *stream;
  int rv;

  *pstream_id = -1;
  *pfin = 0;

  if (veccnt == 0) {
    return 0;
  }

  if (conn->tx.ctrl && !nghttp3_stream_is_blocked(conn->tx.ctrl)) {
    ncnt =
      conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, conn->tx.ctrl);
    if (ncnt) {
      return ncnt;
    }
  }

  if (conn->tx.qdec && !nghttp3_stream_is_blocked(conn->tx.qdec)) {
    rv = nghttp3_stream_write_qpack_decoder_stream(conn->tx.qdec);
    if (rv != 0) {
      return rv;
    }

    ncnt =
      conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, conn->tx.qdec);
    if (ncnt) {
      return ncnt;
    }
  }

  if (conn->tx.qenc && !nghttp3_stream_is_blocked(conn->tx.qenc)) {
    ncnt =
      conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, conn->tx.qenc);
    if (ncnt) {
      return ncnt;
    }
  }

  stream = nghttp3_conn_get_next_tx_stream(conn);
  if (stream == NULL) {
    return 0;
  }

  ncnt = conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, stream);
  if (ncnt < 0) {
    return ncnt;
  }

  if (nghttp3_stream_schedulable(stream) &&
      !nghttp3_stream_require_schedule(stream)) {
    nghttp3_conn_unschedule_stream(conn, stream);
  }

  return ncnt;
}

nghttp3_stream *nghttp3_conn_get_next_tx_stream(nghttp3_conn *conn) {
  size_t i;
  nghttp3_tnode *tnode;
  nghttp3_pq *pq;

  for (i = 0; i < NGHTTP3_URGENCY_LEVELS; ++i) {
    pq = &conn->sched[i].spq;
    if (nghttp3_pq_empty(pq)) {
      continue;
    }

    tnode = nghttp3_struct_of(nghttp3_pq_top(pq), nghttp3_tnode, pe);

    return nghttp3_struct_of(tnode, nghttp3_stream, node);
  }

  return NULL;
}

int nghttp3_conn_add_write_offset(nghttp3_conn *conn, int64_t stream_id,
                                  size_t n) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return 0;
  }

  nghttp3_stream_add_outq_offset(stream, n);

  stream->unscheduled_nwrite += n;

  if (!nghttp3_stream_schedulable(stream)) {
    return 0;
  }

  if (!nghttp3_stream_require_schedule(stream)) {
    nghttp3_conn_unschedule_stream(conn, stream);
    return 0;
  }

  if (stream->unscheduled_nwrite < NGHTTP3_STREAM_MIN_WRITELEN) {
    return 0;
  }

  return nghttp3_conn_schedule_stream(conn, stream);
}

int nghttp3_conn_add_ack_offset(nghttp3_conn *conn, int64_t stream_id,
                                uint64_t n) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return 0;
  }

  return nghttp3_stream_update_ack_offset(stream, stream->ack_offset + n);
}

int nghttp3_conn_update_ack_offset(nghttp3_conn *conn, int64_t stream_id,
                                   uint64_t offset) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return 0;
  }

  if (stream->ack_offset > offset) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  return nghttp3_stream_update_ack_offset(stream, offset);
}

static nghttp3_ssize wt_session_read_data(nghttp3_conn *conn, int64_t stream_id,
                                          nghttp3_vec *vec, size_t veccnt,
                                          uint32_t *pflags,
                                          void *conn_user_data,
                                          void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)vec;
  (void)veccnt;
  (void)pflags;
  (void)conn_user_data;
  (void)stream_user_data;

  return NGHTTP3_ERR_WOULDBLOCK;
}

static int conn_submit_headers_data(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const nghttp3_nv *nva, size_t nvlen,
                                    const nghttp3_data_reader *dr) {
  int rv;
  nghttp3_nv *nnva;
  nghttp3_frame *fr;

  rv = nghttp3_nva_copy(&nnva, nva, nvlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  rv = nghttp3_stream_frq_emplace(stream, &fr);
  if (rv != 0) {
    nghttp3_nva_del(nnva, conn->mem);
    return rv;
  }

  fr->headers = (nghttp3_frame_headers){
    .type = NGHTTP3_FRAME_HEADERS,
    .nva = nnva,
    .nvlen = nvlen,
  };

  if (dr && dr->read_data != wt_session_read_data) {
    rv = nghttp3_stream_frq_emplace(stream, &fr);
    if (rv != 0) {
      return rv;
    }

    fr->data = (nghttp3_frame_data){
      .type = NGHTTP3_FRAME_DATA,
      .dr = *dr,
    };
  }

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_schedule_stream(conn, stream);
  }

  return 0;
}

int nghttp3_conn_schedule_stream(nghttp3_conn *conn, nghttp3_stream *stream) {
  /* Assume that stream stays on the same urgency level */
  nghttp3_tnode *node = stream_get_sched_node(stream);
  int rv;

  rv = nghttp3_tnode_schedule(node, conn_get_sched_pq(conn, node),
                              stream->unscheduled_nwrite);
  if (rv != 0) {
    return rv;
  }

  stream->unscheduled_nwrite = 0;

  return 0;
}

int nghttp3_conn_ensure_stream_scheduled(nghttp3_conn *conn,
                                         nghttp3_stream *stream) {
  if (nghttp3_tnode_is_scheduled(stream_get_sched_node(stream))) {
    return 0;
  }

  return nghttp3_conn_schedule_stream(conn, stream);
}

void nghttp3_conn_unschedule_stream(nghttp3_conn *conn,
                                    nghttp3_stream *stream) {
  nghttp3_tnode *node = stream_get_sched_node(stream);

  nghttp3_tnode_unschedule(node, conn_get_sched_pq(conn, node));
}

int nghttp3_conn_submit_request(nghttp3_conn *conn, int64_t stream_id,
                                const nghttp3_nv *nva, size_t nvlen,
                                const nghttp3_data_reader *dr,
                                void *stream_user_data) {
  nghttp3_stream *stream;
  int rv;

  assert(!conn->server);
  assert(conn->tx.qenc);
  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);
  assert(nghttp3_client_stream_bidi(stream_id));

  /* TODO Check GOAWAY last stream ID */

  if (conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_RECVED) {
    return NGHTTP3_ERR_CONN_CLOSING;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream != NULL) {
    return NGHTTP3_ERR_STREAM_IN_USE;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
  if (rv != 0) {
    return rv;
  }
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  stream->user_data = stream_user_data;
  stream->node.pri.inc = 1;

  nghttp3_http_record_request_method(stream, nva, nvlen);

  if (dr == NULL) {
    stream->flags |= NGHTTP3_STREAM_FLAG_WRITE_END_STREAM;
  }

  return conn_submit_headers_data(conn, stream, nva, nvlen, dr);
}

int nghttp3_conn_submit_info(nghttp3_conn *conn, int64_t stream_id,
                             const nghttp3_nv *nva, size_t nvlen) {
  nghttp3_stream *stream;

  /* TODO Verify that it is allowed to send info (non-final response)
     now. */
  assert(conn->server);
  assert(conn->tx.qenc);

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  return conn_submit_headers_data(conn, stream, nva, nvlen, NULL);
}

int nghttp3_conn_submit_response(nghttp3_conn *conn, int64_t stream_id,
                                 const nghttp3_nv *nva, size_t nvlen,
                                 const nghttp3_data_reader *dr) {
  nghttp3_stream *stream;

  /* TODO Verify that it is allowed to send response now. */
  assert(conn->server);
  assert(conn->tx.qenc);

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  if (dr == NULL) {
    stream->flags |= NGHTTP3_STREAM_FLAG_WRITE_END_STREAM;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_RESP_SUBMITTED;

  return conn_submit_headers_data(conn, stream, nva, nvlen, dr);
}

int nghttp3_conn_submit_trailers(nghttp3_conn *conn, int64_t stream_id,
                                 const nghttp3_nv *nva, size_t nvlen) {
  nghttp3_stream *stream;

  /* TODO Verify that it is allowed to send trailer now. */
  assert(conn->tx.qenc);

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  if (stream->flags & NGHTTP3_STREAM_FLAG_WRITE_END_STREAM) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_WRITE_END_STREAM;

  return conn_submit_headers_data(conn, stream, nva, nvlen, NULL);
}

int nghttp3_conn_submit_shutdown_notice(nghttp3_conn *conn) {
  nghttp3_frame *fr;
  int rv;

  assert(conn->tx.ctrl);

  rv = nghttp3_stream_frq_emplace(conn->tx.ctrl, &fr);
  if (rv != 0) {
    return rv;
  }

  fr->goaway = (nghttp3_frame_goaway){
    .type = NGHTTP3_FRAME_GOAWAY,
    .id = conn->server ? NGHTTP3_SHUTDOWN_NOTICE_STREAM_ID
                       : NGHTTP3_SHUTDOWN_NOTICE_PUSH_ID,
  };

  assert(fr->goaway.id <= conn->tx.goaway_id);

  conn->tx.goaway_id = fr->goaway.id;
  conn->flags |= NGHTTP3_CONN_FLAG_GOAWAY_QUEUED;

  return 0;
}

int nghttp3_conn_shutdown(nghttp3_conn *conn) {
  nghttp3_frame *fr;
  int rv;

  assert(conn->tx.ctrl);

  rv = nghttp3_stream_frq_emplace(conn->tx.ctrl, &fr);
  if (rv != 0) {
    return rv;
  }

  fr->goaway = (nghttp3_frame_goaway){
    .type = NGHTTP3_FRAME_GOAWAY,
    .id = conn->server
            ? nghttp3_min((1LL << 62) - 4, conn->rx.max_stream_id_bidi + 4)
            : 0,
  };

  assert(fr->goaway.id <= conn->tx.goaway_id);

  conn->tx.goaway_id = fr->goaway.id;
  conn->flags |=
    NGHTTP3_CONN_FLAG_GOAWAY_QUEUED | NGHTTP3_CONN_FLAG_SHUTDOWN_COMMENCED;

  return 0;
}

int nghttp3_conn_reject_stream(nghttp3_conn *conn, nghttp3_stream *stream) {
  return nghttp3_conn_abort_stream(conn, stream, NGHTTP3_H3_REQUEST_REJECTED);
}

int nghttp3_conn_abort_stream(nghttp3_conn *conn, nghttp3_stream *stream,
                              uint64_t error_code) {
  int rv;
  int remote_uni = conn_remote_stream_uni(conn, stream->node.id);
  int bidi = !nghttp3_stream_uni(stream->node.id);

  if (remote_uni || bidi) {
    rv = conn_call_stop_sending(conn, stream, error_code);
    if (rv != 0) {
      return rv;
    }
  }

  if (remote_uni) {
    return 0;
  }

  return conn_call_reset_stream(conn, stream, error_code);
}

void nghttp3_conn_block_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_FC_BLOCKED;
  stream->unscheduled_nwrite = 0;

  if (nghttp3_client_stream_bidi(stream->node.id)) {
    nghttp3_conn_unschedule_stream(conn, stream);
  }
}

void nghttp3_conn_shutdown_stream_write(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_SHUT_WR;
  stream->unscheduled_nwrite = 0;

  if (nghttp3_client_stream_bidi(stream->node.id)) {
    nghttp3_conn_unschedule_stream(conn, stream);
  }
}

int nghttp3_conn_unblock_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return 0;
  }

  stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_FC_BLOCKED;

  if (nghttp3_stream_schedulable(stream) &&
      nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_ensure_stream_scheduled(conn, stream);
  }

  return 0;
}

int nghttp3_conn_is_stream_writable(nghttp3_conn *conn, int64_t stream_id) {
  return nghttp3_conn_is_stream_writable2(conn, stream_id);
}

int nghttp3_conn_is_stream_writable2(const nghttp3_conn *conn,
                                     int64_t stream_id) {
  const nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return 0;
  }

  return (stream->flags & (NGHTTP3_STREAM_FLAG_FC_BLOCKED |
                           NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED |
                           NGHTTP3_STREAM_FLAG_SHUT_WR)) == 0;
}

int nghttp3_conn_resume_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return 0;
  }

  stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED;

  if (nghttp3_stream_schedulable(stream) &&
      nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_ensure_stream_scheduled(conn, stream);
  }

  return 0;
}

int nghttp3_conn_close_stream(nghttp3_conn *conn, int64_t stream_id,
                              uint64_t app_error_code) {
  return nghttp3_conn_close_stream2(
    conn,
    NGHTTP3_STREAM_CLOSE_FLAG_RX_APP_ERROR_CODE_SET |
      NGHTTP3_STREAM_CLOSE_FLAG_TX_APP_ERROR_CODE_SET,
    stream_id, app_error_code, app_error_code);
}

int nghttp3_conn_close_stream2(nghttp3_conn *conn, uint32_t flags,
                               int64_t stream_id, uint64_t rx_app_error_code,
                               uint64_t tx_app_error_code) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  if (nghttp3_stream_critical(stream)) {
    return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
  }

  nghttp3_conn_unschedule_stream(conn, stream);

  return conn_delete_stream(conn, stream, flags, rx_app_error_code,
                            tx_app_error_code);
}

int nghttp3_conn_shutdown_stream_read(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream;

  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);

  if (!nghttp3_client_stream_bidi(stream_id)) {
    return 0;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream) {
    if (stream->flags & NGHTTP3_STREAM_FLAG_SHUT_RD) {
      return 0;
    }

    stream->flags |= NGHTTP3_STREAM_FLAG_SHUT_RD;

    /* If stream is WebTransport data stream, do not send QPACK Stream
       Cancellation. */
    if (nghttp3_stream_wt_data(stream) ||
        (stream->flags & NGHTTP3_STREAM_FLAG_WT_DATA)) {
      return 0;
    }
  }

  return nghttp3_qpack_decoder_cancel_stream(&conn->qdec, stream_id);
}

int nghttp3_conn_qpack_blocked_streams_push(nghttp3_conn *conn,
                                            nghttp3_stream *stream) {
  assert(stream->qpack_blocked_pe.index == NGHTTP3_PQ_BAD_INDEX);

  return nghttp3_pq_push(&conn->qpack_blocked_streams,
                         &stream->qpack_blocked_pe);
}

void nghttp3_conn_qpack_blocked_streams_pop(nghttp3_conn *conn) {
  assert(!nghttp3_pq_empty(&conn->qpack_blocked_streams));
  nghttp3_pq_pop(&conn->qpack_blocked_streams);
}

void nghttp3_conn_qpack_blocked_streams_remove(nghttp3_conn *conn,
                                               nghttp3_stream *stream) {
  assert(!nghttp3_pq_empty(&conn->qpack_blocked_streams));
  assert(stream->qpack_blocked_pe.index != NGHTTP3_PQ_BAD_INDEX);

  nghttp3_pq_remove(&conn->qpack_blocked_streams, &stream->qpack_blocked_pe);
}

void nghttp3_conn_set_max_client_streams_bidi(nghttp3_conn *conn,
                                              uint64_t max_streams) {
  assert(conn->server);
  assert(conn->remote.bidi.max_client_streams <= max_streams);

  conn->remote.bidi.max_client_streams = max_streams;
}

void nghttp3_conn_set_max_concurrent_streams(nghttp3_conn *conn,
                                             size_t max_concurrent_streams) {
  nghttp3_qpack_decoder_set_max_concurrent_streams(&conn->qdec,
                                                   max_concurrent_streams);
}

int nghttp3_conn_set_stream_user_data(nghttp3_conn *conn, int64_t stream_id,
                                      void *stream_user_data) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  stream->user_data = stream_user_data;

  return 0;
}

void *nghttp3_conn_get_stream_user_data(const nghttp3_conn *conn,
                                        int64_t stream_id) {
  nghttp3_stream *stream;

  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NULL;
  }

  return stream->user_data;
}

uint64_t nghttp3_conn_get_frame_payload_left(nghttp3_conn *conn,
                                             int64_t stream_id) {
  return nghttp3_conn_get_frame_payload_left2(conn, stream_id);
}

uint64_t nghttp3_conn_get_frame_payload_left2(const nghttp3_conn *conn,
                                              int64_t stream_id) {
  const nghttp3_stream *stream;
  int uni = 0;

  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);

  if (!nghttp3_client_stream_bidi(stream_id)) {
    uni = conn_remote_stream_uni(conn, stream_id);
    if (!uni) {
      return 0;
    }
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return 0;
  }

  if (uni && stream->type != NGHTTP3_STREAM_TYPE_CONTROL) {
    return 0;
  }

  return stream->rstate.left;
}

int nghttp3_conn_get_stream_priority_versioned(nghttp3_conn *conn,
                                               int pri_version,
                                               nghttp3_pri *dest,
                                               int64_t stream_id) {
  return nghttp3_conn_get_stream_priority2_versioned(conn, pri_version, dest,
                                                     stream_id);
}

int nghttp3_conn_get_stream_priority2_versioned(const nghttp3_conn *conn,
                                                int pri_version,
                                                nghttp3_pri *dest,
                                                int64_t stream_id) {
  const nghttp3_stream *stream;
  (void)pri_version;

  assert(conn->server);
  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);

  if (!nghttp3_client_stream_bidi(stream_id)) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  *dest = stream->node.pri;

  return 0;
}

int nghttp3_conn_set_client_stream_priority(nghttp3_conn *conn,
                                            int64_t stream_id,
                                            const uint8_t *data,
                                            size_t datalen) {
  nghttp3_stream *stream;
  nghttp3_frame *fr;
  int rv;
  uint8_t *buf = NULL;

  assert(!conn->server);
  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);

  if (!nghttp3_client_stream_bidi(stream_id)) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  if (datalen) {
    buf = nghttp3_mem_malloc(conn->mem, datalen);
    if (buf == NULL) {
      return NGHTTP3_ERR_NOMEM;
    }

    memcpy(buf, data, datalen);
  }

  rv = nghttp3_stream_frq_emplace(conn->tx.ctrl, &fr);
  if (rv != 0) {
    nghttp3_mem_free(conn->mem, buf);
    return rv;
  }

  fr->priority_update = (nghttp3_frame_priority_update){
    .type = NGHTTP3_FRAME_PRIORITY_UPDATE,
    .pri_elem_id = stream_id,
    .data = buf,
    .datalen = datalen,
  };

  return 0;
}

int nghttp3_conn_set_server_stream_priority_versioned(nghttp3_conn *conn,
                                                      int64_t stream_id,
                                                      int pri_version,
                                                      const nghttp3_pri *pri) {
  nghttp3_stream *stream;
  (void)pri_version;

  assert(conn->server);
  assert(pri->urgency < NGHTTP3_URGENCY_LEVELS);
  assert(pri->inc == 0 || pri->inc == 1);
  assert(stream_id >= 0);
  assert(stream_id <= (int64_t)NGHTTP3_MAX_VARINT);

  if (!nghttp3_client_stream_bidi(stream_id)) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_SERVER_PRIORITY_SET;

  return conn_update_stream_priority(conn, stream, pri);
}

int nghttp3_conn_is_drained(nghttp3_conn *conn) {
  return nghttp3_conn_is_drained2(conn);
}

int nghttp3_conn_is_drained2(const nghttp3_conn *conn) {
  assert(conn->server);

  return (conn->flags & NGHTTP3_CONN_FLAG_SHUTDOWN_COMMENCED) &&
         conn->remote.bidi.num_streams == 0 &&
         nghttp3_stream_outq_write_done(conn->tx.ctrl) &&
         nghttp3_ringbuf_len(&conn->tx.ctrl->frq) == 0;
}

int nghttp3_conn_is_stream_flushed(const nghttp3_conn *conn,
                                   int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);
  const nghttp3_frame *fr;

  if (!stream) {
    return 1;
  }

  if (!nghttp3_stream_outq_write_done(stream)) {
    return 0;
  }

  if (nghttp3_ringbuf_len(&stream->frq) == 0) {
    return 1;
  }

  fr = nghttp3_ringbuf_get(&stream->frq, 0);

  return fr->hd.type == NGHTTP3_FRAME_DATA ||
         (fr->hd.type == NGHTTP3_FRAME_EX_WT &&
          fr->wt.fr.hd.type == NGHTTP3_EXFR_WT_STREAM_DATA);
}

int nghttp3_conn_submit_wt_request(nghttp3_conn *conn, int64_t stream_id,
                                   const nghttp3_nv *nva, size_t nvlen,
                                   void *stream_user_data) {
  int rv;
  nghttp3_stream *stream;

  if (!conn_wt_enabled(conn)) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  rv = nghttp3_conn_submit_request(
    conn, stream_id, nva, nvlen,
    &(nghttp3_data_reader){.read_data = wt_session_read_data},
    stream_user_data);
  if (rv != 0) {
    return rv;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);

  assert(stream);

  return nghttp3_conn_open_wt_session(conn, stream);
}

int nghttp3_conn_submit_wt_response(nghttp3_conn *conn, int64_t stream_id,
                                    const nghttp3_nv *nva, size_t nvlen) {
  int rv;
  nghttp3_stream *stream;

  if (!conn_wt_enabled(conn)) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  rv = nghttp3_conn_submit_response(
    conn, stream_id, nva, nvlen,
    &(nghttp3_data_reader){.read_data = wt_session_read_data});
  if (rv != 0) {
    return rv;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);

  if (!stream->wt.session) {
    rv = nghttp3_conn_open_wt_session(conn, stream);
    if (rv != 0) {
      return rv;
    }
  }

  stream->wt.session->flags |= NGHTTP3_WT_SESSION_FLAG_RESP_SUBMITTED;

  return 0;
}

int nghttp3_conn_server_confirm_wt_session(nghttp3_conn *conn,
                                           int64_t session_id,
                                           nghttp3_tstamp ts) {
  nghttp3_stream *wt_ctrl_stream;

  wt_ctrl_stream = nghttp3_conn_find_stream(conn, session_id);
  if (!wt_ctrl_stream) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  assert(wt_ctrl_stream->wt.session);
  assert(wt_ctrl_stream->wt.session->flags &
         NGHTTP3_WT_SESSION_FLAG_RESP_SUBMITTED);

  wt_ctrl_stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_WT_SESSION_BLOCKED;

  return nghttp3_conn_on_wt_session_confirmed(conn, wt_ctrl_stream, ts);
}

int nghttp3_conn_open_wt_session(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  rv = nghttp3_wt_session_new(&stream->wt.session, stream->node.id, conn->mem);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

int nghttp3_conn_open_wt_data_stream(nghttp3_conn *conn, int64_t session_id,
                                     int64_t stream_id,
                                     const nghttp3_data_reader *dr,
                                     void *stream_user_data) {
  nghttp3_stream *stream, *wt_ctrl_stream;
  nghttp3_wt_session *wt_session;
  nghttp3_frame *fr;
  uint64_t type;
  int rv;
  int remote_bidi = 0;

  if (conn->server) {
    assert(nghttp3_client_stream_bidi(stream_id) ||
           nghttp3_server_stream_bidi(stream_id) ||
           nghttp3_server_stream_uni(stream_id));
  } else {
    assert(nghttp3_client_stream_bidi(stream_id) ||
           nghttp3_server_stream_bidi(stream_id) ||
           nghttp3_client_stream_uni(stream_id));
  }

  /* TODO Check session flow control */

  assert(dr);

  if (conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_RECVED) {
    return NGHTTP3_ERR_CONN_CLOSING;
  }

  wt_ctrl_stream = nghttp3_conn_find_stream(conn, session_id);
  if (!wt_ctrl_stream || !wt_ctrl_stream->wt.session) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  wt_session = wt_ctrl_stream->wt.session;

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream) {
    if (conn->server) {
      assert(nghttp3_client_stream_bidi(stream_id));
    } else {
      assert(nghttp3_server_stream_bidi(stream_id));
    }

    /* TODO verify that we do not start writing more than once. */

    /* Normally, stream->wt.session is not NULL because we must
       identify WT stream header first.  The only exception is a
       stream create by priority update on server side.  But it must
       be client initiated bidi stream, and we must wait for its WT
       header. */
    if (!stream->wt.session) {
      return NGHTTP3_ERR_INVALID_ARGUMENT;
    }

    if (stream->flags & NGHTTP3_STREAM_FLAG_WRITE_END_STREAM) {
      return NGHTTP3_ERR_INVALID_STATE;
    }

    remote_bidi = 1;

    if (stream_user_data) {
      stream->user_data = stream_user_data;
    }

    if (conn->server) {
      stream->flags |= NGHTTP3_STREAM_FLAG_SERVER_PRIORITY_SET;
    }

    assert(!nghttp3_tnode_is_scheduled(&stream->node));
  } else {
    if (conn->server) {
      assert(nghttp3_server_stream_bidi(stream_id) ||
             nghttp3_server_stream_uni(stream_id));
    } else {
      assert(nghttp3_client_stream_bidi(stream_id) ||
             nghttp3_client_stream_uni(stream_id));
    }

    rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
    if (rv != 0) {
      return rv;
    }

    nghttp3_wt_session_add_stream(wt_session, stream);

    if (conn->server) {
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
    } else {
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
    }

    stream->user_data = stream_user_data;

    if (stream_id & 0x2) {
      stream->flags |= NGHTTP3_STREAM_FLAG_SHUT_RD;
      stream->type = NGHTTP3_STREAM_TYPE_WT_STREAM;
    }
  }

  stream->node.pri = (nghttp3_pri){
    .urgency = NGHTTP3_DEFAULT_URGENCY,
    .inc = 1,
  };

  if (stream_id & 0x2) {
    type = NGHTTP3_EXFR_WT_STREAM_UNI;
  } else if (remote_bidi) {
    type = NGHTTP3_EXFR_WT_STREAM_DATA;
  } else {
    type = NGHTTP3_EXFR_WT_STREAM_BIDI;
    stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_BEFORE_WT_DATA;
  }

  rv = nghttp3_stream_frq_emplace(stream, &fr);
  if (rv != 0) {
    return rv;
  }

  fr->wt = (nghttp3_frame_ex_wt){
    .type = NGHTTP3_FRAME_EX_WT,
    .fr.wt_stream =
      {
        .type = type,
        .session_id = session_id,
        .dr = *dr,
      },
  };

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_ensure_stream_scheduled(conn, stream);
  }

  return 0;
}

int nghttp3_conn_close_wt_session(nghttp3_conn *conn, int64_t session_id,
                                  uint32_t wt_error_code, const uint8_t *msg,
                                  size_t msglen) {
  nghttp3_stream *stream;
  nghttp3_wt_session *wt_session;
  nghttp3_frame *fr;
  int rv;

  stream = nghttp3_conn_find_stream(conn, session_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  if (!nghttp3_stream_wt_ctrl(stream)) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  if (stream->flags & NGHTTP3_STREAM_FLAG_WRITE_END_STREAM) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_WRITE_END_STREAM;

  wt_session = stream->wt.session;

  assert(!wt_session->tx.error_msg.base);

  if (msglen) {
    wt_session->tx.error_msg.base = nghttp3_mem_malloc(conn->mem, msglen);
    if (!wt_session->tx.error_msg.base) {
      return NGHTTP3_ERR_NOMEM;
    }

    memcpy(wt_session->tx.error_msg.base, msg, msglen);
    wt_session->tx.error_msg.len = msglen;
  }

  rv = nghttp3_stream_frq_emplace(stream, &fr);
  if (rv != 0) {
    return rv;
  }

  fr->cpsl = (nghttp3_frame_ex_cpsl){
    .type = NGHTTP3_FRAME_EX_CPSL,
    .fr.wt_close_session =
      {
        .type = NGHTTP3_EXFR_CPSL_WT_CLOSE_SESSION,
        .error_code = wt_error_code,
        .error_msg = wt_session->tx.error_msg,
      },
  };

  if (nghttp3_stream_require_schedule(stream)) {
    rv = nghttp3_conn_schedule_stream(conn, stream);
    if (rv != 0) {
      return rv;
    }
  }

  rv = nghttp3_conn_shutdown_all_wt_data_streams(conn, stream,
                                                 NGHTTP3_WT_SESSION_GONE);
  if (rv != 0) {
    return rv;
  }

  stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

  return conn_call_stop_sending(conn, stream, NGHTTP3_WT_SESSION_GONE);
}

int nghttp3_conn_on_wt_stream(nghttp3_conn *conn, nghttp3_stream *stream,
                              int64_t session_id) {
  nghttp3_stream *wt_ctrl_stream;
  nghttp3_wt_session *wt_session;
  int rv;

  if (!nghttp3_client_stream_bidi(session_id)) {
    return NGHTTP3_ERR_WT_BUFFERED_STREAM_REJECTED;
  }

  if (stream->wt.session) {
    assert(stream->wt.session->session_id != stream->node.id);

    if (stream->wt.session->session_id != session_id) {
      return NGHTTP3_ERR_WT_BUFFERED_STREAM_REJECTED;
    }

    return 0;
  }

  wt_ctrl_stream = nghttp3_conn_find_stream(conn, session_id);
  if (!wt_ctrl_stream) {
    if (!conn->server) {
      /* On client's perspective, if session stream is not found, we are
         sure that session is gone. */
      return NGHTTP3_ERR_WT_SESSION_GONE;
    }

    if (nghttp3_ord_stream_id(session_id) >
        conn->remote.bidi.max_client_streams) {
      return NGHTTP3_ERR_WT_BUFFERED_STREAM_REJECTED;
    }

    if ((conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
        conn->tx.goaway_id <= session_id) {
      return NGHTTP3_ERR_WT_BUFFERED_STREAM_REJECTED;
    }

    rv = conn_bidi_idtr_open(conn, session_id);
    if (rv != 0) {
      if (nghttp3_err_is_fatal(rv)) {
        return rv;
      }

      return NGHTTP3_ERR_WT_SESSION_GONE;
    }

    conn->rx.max_stream_id_bidi =
      nghttp3_max(conn->rx.max_stream_id_bidi, session_id);
    rv = nghttp3_conn_create_stream(conn, &wt_ctrl_stream, session_id);
    if (rv != 0) {
      return rv;
    }

    wt_ctrl_stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
  }

  if (!wt_ctrl_stream->wt.session) {
    rv = nghttp3_conn_open_wt_session(conn, wt_ctrl_stream);
    if (rv != 0) {
      return rv;
    }
  }

  wt_session = wt_ctrl_stream->wt.session;

  assert(wt_session);

  nghttp3_wt_session_add_stream(wt_session, stream);

  if (wt_ctrl_stream->wt.session->flags & NGHTTP3_WT_SESSION_FLAG_CONFIRMED) {
    return conn_call_wt_data_stream_open(conn, stream);
  }

  return 0;
}

int nghttp3_conn_on_wt_session_confirmed(nghttp3_conn *conn,
                                         nghttp3_stream *wt_ctrl_stream,
                                         nghttp3_tstamp ts) {
  nghttp3_stream *stream;
  nghttp3_wt_session *wt_session = wt_ctrl_stream->wt.session;
  int rv;

  wt_session->flags |= NGHTTP3_WT_SESSION_FLAG_CONFIRMED;

  /* TODO Is stream gone during iteration? */
  for (stream = wt_session->head; stream; stream = stream->wt.next) {
    stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_WT_SESSION_BLOCKED;

    if (conn_remote_stream(conn, stream->node.id)) {
      rv = conn_call_wt_data_stream_open(conn, stream);
      if (rv != 0) {
        return rv;
      }
    }

    rv = nghttp3_conn_process_blocked_wt_stream_data(conn, stream, ts);
    if (rv != 0) {
      return rv;
    }
  }

  return nghttp3_conn_process_blocked_wt_stream_data(conn, wt_ctrl_stream, ts);
}

nghttp3_ssize nghttp3_conn_read_wt_stream_uni(nghttp3_conn *conn,
                                              nghttp3_stream *stream,
                                              const uint8_t *src, size_t srclen,
                                              int fin, nghttp3_tstamp ts) {
  const uint8_t *p = src, *end = src ? src + srclen : src;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  size_t nconsumed = 0;
  nghttp3_stream *wt_ctrl_stream;
  (void)ts;

  if ((stream->flags & NGHTTP3_STREAM_FLAG_SHUT_RD)) {
    return (nghttp3_ssize)srclen;
  }

  if (srclen == 0) {
    goto almost_done;
  }

  if (stream->flags & NGHTTP3_STREAM_FLAG_WT_SESSION_BLOCKED) {
    if (srclen == 0) {
      return 0;
    }

    rv = nghttp3_stream_buffer_data(stream, p, srclen);
    if (rv != 0) {
      return rv;
    }

    return 0;
  }

  switch (rstate->state) {
  case NGHTTP3_WT_STREAM_STATE_SESSION_ID:
    assert(end - p > 0);
    nread = nghttp3_read_varint(rvint, p, end, fin);
    if (nread < 0) {
      return NGHTTP3_ERR_H3_FRAME_ERROR;
    }

    p += nread;
    nconsumed += (size_t)nread;
    if (rvint->left) {
      /* TODO What should we do if unidirectional stream is closed
         before reading Session ID? */
      break;
    }

    rstate->left = rvint->acc;
    nghttp3_varint_read_state_reset(rvint);

    /* rstate->left is Session ID */
    rv = nghttp3_conn_on_wt_stream(conn, stream, (int64_t)rstate->left);
    if (rv != 0) {
      if (rv != NGHTTP3_ERR_WT_SESSION_GONE) {
        return rv;
      }

      stream->rstate.state = NGHTTP3_WT_STREAM_STATE_IGN_REST;

      rv = nghttp3_conn_abort_stream(conn, stream, NGHTTP3_WT_SESSION_GONE);
      if (rv != 0) {
        return rv;
      }

      nconsumed += (size_t)(end - p);

      return (nghttp3_ssize)nconsumed;
    }

    rstate->state = NGHTTP3_WT_STREAM_STATE_DATA;

    wt_ctrl_stream =
      nghttp3_conn_find_stream(conn, stream->wt.session->session_id);

    if (!(wt_ctrl_stream->wt.session->flags &
          NGHTTP3_WT_SESSION_FLAG_CONFIRMED)) {
      stream->flags |= NGHTTP3_STREAM_FLAG_WT_SESSION_BLOCKED;

      if (p != end) {
        rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
        if (rv != 0) {
          return rv;
        }
      }

      return (nghttp3_ssize)nconsumed;
    }

    if (p == end) {
      break;
    }

    /* Fall through */
  case NGHTTP3_WT_STREAM_STATE_DATA:
    rv = conn_call_recv_wt_data(conn, stream, p, (size_t)(end - p));
    if (rv != 0) {
      return rv;
    }

    break;
  case NGHTTP3_WT_STREAM_STATE_IGN_REST:
    nconsumed += (size_t)(end - p);

    return (nghttp3_ssize)nconsumed;
  }

almost_done:
  if (fin) {
    rv = conn_call_end_stream(conn, stream);
    if (rv != 0) {
      return rv;
    }
  }

  return (nghttp3_ssize)nconsumed;
}

int nghttp3_conn_process_blocked_wt_stream_data(nghttp3_conn *conn,
                                                nghttp3_stream *stream,
                                                nghttp3_tstamp ts) {
  nghttp3_buf *buf;
  nghttp3_ssize nconsumed;
  size_t nproc;
  int rv;
  size_t len;

  for (;;) {
    len = nghttp3_ringbuf_len(&stream->inq);
    if (len == 0) {
      break;
    }

    buf = nghttp3_ringbuf_get(&stream->inq, 0);

    if (nghttp3_stream_uni(stream->node.id)) {
      nconsumed = nghttp3_conn_read_wt_stream_uni(
        conn, stream, buf->pos, nghttp3_buf_len(buf),
        len == 1 && (stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF), ts);
    } else {
      nconsumed = nghttp3_conn_read_bidi(
        conn, &nproc, stream, buf->pos, nghttp3_buf_len(buf),
        len == 1 && (stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF), ts);
    }

    if (nconsumed < 0) {
      return (int)nconsumed;
    }

    rv = conn_call_deferred_consume(conn, stream, (size_t)nconsumed);
    if (rv != 0) {
      return rv;
    }

    nghttp3_buf_free(buf, stream->mem);
    nghttp3_ringbuf_pop_front(&stream->inq);
  }

  return 0;
}

int nghttp3_conn_shutdown_wt_session(nghttp3_conn *conn,
                                     nghttp3_stream *wt_ctrl_stream,
                                     uint64_t error_code) {
  int rv;

  rv =
    nghttp3_conn_shutdown_all_wt_data_streams(conn, wt_ctrl_stream, error_code);
  if (rv != 0) {
    return rv;
  }

  wt_ctrl_stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

  return nghttp3_conn_abort_stream(conn, wt_ctrl_stream, error_code);
}

int nghttp3_conn_shutdown_all_wt_data_streams(nghttp3_conn *conn,
                                              nghttp3_stream *wt_ctrl_stream,
                                              uint64_t error_code) {
  nghttp3_wt_session *wt_session = wt_ctrl_stream->wt.session;
  nghttp3_stream *stream;
  int rv;

  for (stream = wt_session->head; stream; stream = stream->wt.next) {
    rv = nghttp3_conn_shutdown_wt_data_stream(conn, stream, error_code);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

int nghttp3_conn_shutdown_wt_data_stream(nghttp3_conn *conn,
                                         nghttp3_stream *stream,
                                         uint64_t error_code) {
  if (stream->node.id & 0x2) {
    stream->rstate.state = NGHTTP3_WT_STREAM_STATE_IGN_REST;
  } else {
    stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;
  }

  return nghttp3_conn_abort_stream(conn, stream, error_code);
}

int64_t nghttp3_conn_get_stream_wt_session_id(const nghttp3_conn *conn,
                                              int64_t stream_id) {
  const nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (!stream || !nghttp3_stream_wt_data(stream)) {
    return -1;
  }

  return stream->wt.session->session_id;
}

int nghttp3_conn_read_wt_ctrl_stream(nghttp3_conn *conn,
                                     const nghttp3_stream *stream,
                                     const uint8_t *src, size_t srclen) {
  const uint8_t *p, *end;
  nghttp3_wt_session *wts = stream->wt.session;
  nghttp3_wt_ctrl_read_state *rstate = &wts->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  nghttp3_exfr_cpsl *cpsl = &rstate->cpsl;
  size_t len;
  size_t i;
  int rv;

  if (srclen == 0) {
    return 0;
  }

  p = src;
  end = src + srclen;

  for (; p != end;) {
    switch (rstate->state) {
    case NGHTTP3_WT_CTRL_STREAM_STATE_TYPE:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, end, /* fin = */ 0);

      assert(nread > 0);

      p += nread;
      if (rvint->left) {
        return 0;
      }

      rstate->cpsl.hd.type = rvint->acc;

      nghttp3_varint_read_state_reset(rvint);
      rstate->state = NGHTTP3_WT_CTRL_STREAM_STATE_LENGTH;
      if (p == end) {
        return 0;
      }
      /* Fall through */
    case NGHTTP3_WT_CTRL_STREAM_STATE_LENGTH:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, end, /* fin = */ 0);
      assert(nread > 0);

      p += nread;
      if (rvint->left) {
        return 0;
      }

      rstate->left = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      switch (rstate->cpsl.hd.type) {
      case NGHTTP3_EXFR_CPSL_WT_CLOSE_SESSION:
        if (rstate->left < sizeof(uint32_t) ||
            rstate->left > sizeof(uint32_t) + /* largest message size */ 1024) {
          /* TODO Find better error code */
          return NGHTTP3_ERR_H3_MESSAGE_ERROR;
        }

        rstate->field_left = sizeof(uint32_t);
        rstate->state =
          NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_CODE;

        break;
      default:
        /* TODO Add rate limit after we implement all supported
           capsules. */
        if (rstate->left == 0) {
          nghttp3_wt_ctrl_read_state_reset(rstate);
          break;
        }

        rstate->state = NGHTTP3_WT_CTRL_STREAM_STATE_IGN;
      }

      break;
    case NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_CODE:
      len = nghttp3_min(rstate->field_left, (size_t)(end - p));

      for (i = 0; i < len; ++i) {
        cpsl->wt_close_session.error_code <<= 8;
        cpsl->wt_close_session.error_code += *p++;
      }

      rstate->left -= len;
      rstate->field_left -= len;
      if (rstate->field_left) {
        break;
      }

      wts->rx.error_code = cpsl->wt_close_session.error_code;

      if (rstate->left == 0) {
        if (conn->callbacks.recv_wt_close_session) {
          rv = conn->callbacks.recv_wt_close_session(
            conn, wts->session_id, wts->rx.error_code, NULL, 0, conn->user_data,
            stream->user_data);
          if (rv != 0) {
            return NGHTTP3_ERR_CALLBACK_FAILURE;
          }
        }

        nghttp3_wt_ctrl_read_state_reset(rstate);

        return NGHTTP3_ERR_WT_SESSION_GONE;
      }

      rstate->state = NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_MSG;

      wts->rx.error_msg.base =
        nghttp3_mem_malloc(conn->mem, (size_t)rstate->left);
      if (!wts->rx.error_msg.base) {
        return NGHTTP3_ERR_NOMEM;
      }

      if (p == end) {
        return 0;
      }

      /* Fall through */
    case NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_MSG:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));

      memcpy(wts->rx.error_msg.base + wts->rx.error_msg.len, p, len);
      wts->rx.error_msg.len += len;

      p += len;
      rstate->left -= len;

      if (rstate->left) {
        break;
      }

      if (conn->callbacks.recv_wt_close_session) {
        rv = conn->callbacks.recv_wt_close_session(
          conn, wts->session_id, wts->rx.error_code, wts->rx.error_msg.base,
          wts->rx.error_msg.len, conn->user_data, stream->user_data);
        if (rv != 0) {
          return NGHTTP3_ERR_CALLBACK_FAILURE;
        }
      }

      nghttp3_wt_ctrl_read_state_reset(rstate);

      return NGHTTP3_ERR_WT_SESSION_GONE;
    case NGHTTP3_WT_CTRL_STREAM_STATE_IGN:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));
      p += len;
      rstate->left -= len;

      if (rstate->left) {
        return 0;
      }

      nghttp3_wt_ctrl_read_state_reset(rstate);

      break;
    }
  }

  return 0;
}
