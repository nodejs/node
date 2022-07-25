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

/*
 * conn_remote_stream_uni returns nonzero if |stream_id| is remote
 * unidirectional stream ID.
 */
static int conn_remote_stream_uni(nghttp3_conn *conn, int64_t stream_id) {
  if (conn->server) {
    return (stream_id & 0x03) == 0x02;
  }
  return (stream_id & 0x03) == 0x03;
}

static int conn_call_begin_headers(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.begin_headers) {
    return 0;
  }

  rv = conn->callbacks.begin_headers(conn, stream->node.nid.id, conn->user_data,
                                     stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_headers(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.end_headers) {
    return 0;
  }

  rv = conn->callbacks.end_headers(conn, stream->node.nid.id, conn->user_data,
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

  rv = conn->callbacks.begin_trailers(conn, stream->node.nid.id,
                                      conn->user_data, stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_trailers(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.end_trailers) {
    return 0;
  }

  rv = conn->callbacks.end_trailers(conn, stream->node.nid.id, conn->user_data,
                                    stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_begin_push_promise(nghttp3_conn *conn,
                                        nghttp3_stream *stream,
                                        int64_t push_id) {
  int rv;

  if (!conn->callbacks.begin_push_promise) {
    return 0;
  }

  rv = conn->callbacks.begin_push_promise(conn, stream->node.nid.id, push_id,
                                          conn->user_data, stream->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_push_promise(nghttp3_conn *conn,
                                      nghttp3_stream *stream, int64_t push_id) {
  int rv;

  if (!conn->callbacks.end_push_promise) {
    return 0;
  }

  rv = conn->callbacks.end_push_promise(conn, stream->node.nid.id, push_id,
                                        conn->user_data, stream->user_data);
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

  rv = conn->callbacks.end_stream(conn, stream->node.nid.id, conn->user_data,
                                  stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_cancel_push(nghttp3_conn *conn, int64_t push_id,
                                 nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.cancel_push) {
    return 0;
  }

  rv = conn->callbacks.cancel_push(
      conn, push_id, stream ? stream->node.nid.id : -1, conn->user_data,
      stream ? stream->user_data : NULL);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_send_stop_sending(nghttp3_conn *conn,
                                       nghttp3_stream *stream,
                                       uint64_t app_error_code) {
  int rv;

  if (!conn->callbacks.send_stop_sending) {
    return 0;
  }

  rv = conn->callbacks.send_stop_sending(conn, stream->node.nid.id,
                                         app_error_code, conn->user_data,
                                         stream->user_data);
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

  rv = conn->callbacks.reset_stream(conn, stream->node.nid.id, app_error_code,
                                    conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_push_stream(nghttp3_conn *conn, int64_t push_id,
                                 nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.push_stream) {
    return 0;
  }

  rv = conn->callbacks.push_stream(conn, push_id, stream->node.nid.id,
                                   conn->user_data);
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

  rv = conn->callbacks.deferred_consume(conn, stream->node.nid.id, nconsumed,
                                        conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
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
    return lhs->seq < rhs->seq;
  }

  return rhs->cycle - lhs->cycle <= NGHTTP3_TNODE_MAX_CYCLE_GAP;
}

static int conn_new(nghttp3_conn **pconn, int server,
                    const nghttp3_callbacks *callbacks,
                    const nghttp3_settings *settings, const nghttp3_mem *mem,
                    void *user_data) {
  int rv;
  nghttp3_conn *conn;
  size_t i;

  conn = nghttp3_mem_calloc(mem, 1, sizeof(nghttp3_conn));
  if (conn == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  rv = nghttp3_map_init(&conn->streams, mem);
  if (rv != 0) {
    goto streams_init_fail;
  }

  rv = nghttp3_map_init(&conn->pushes, mem);
  if (rv != 0) {
    goto pushes_init_fail;
  }

  rv = nghttp3_qpack_decoder_init(&conn->qdec,
                                  settings->qpack_max_table_capacity,
                                  settings->qpack_blocked_streams, mem);
  if (rv != 0) {
    goto qdec_init_fail;
  }

  rv = nghttp3_qpack_encoder_init(&conn->qenc, 0, 0, mem);
  if (rv != 0) {
    goto qenc_init_fail;
  }

  nghttp3_pq_init(&conn->qpack_blocked_streams, ricnt_less, mem);

  for (i = 0; i < NGHTTP3_URGENCY_LEVELS; ++i) {
    nghttp3_pq_init(&conn->sched[i].spq, cycle_less, mem);
  }

  rv = nghttp3_idtr_init(&conn->remote.bidi.idtr, server, mem);
  if (rv != 0) {
    goto remote_bidi_idtr_init_fail;
  }

  rv = nghttp3_gaptr_init(&conn->remote.uni.push_idtr, mem);
  if (rv != 0) {
    goto push_idtr_init_fail;
  }

  conn->callbacks = *callbacks;
  conn->local.settings = *settings;
  nghttp3_settings_default(&conn->remote.settings);
  conn->mem = mem;
  conn->user_data = user_data;
  conn->next_seq = 0;
  conn->server = server;
  conn->rx.goaway_id = NGHTTP3_VARINT_MAX + 1;
  conn->tx.goaway_id = NGHTTP3_VARINT_MAX + 1;
  conn->rx.max_stream_id_bidi = 0;
  conn->rx.max_push_id = 0;

  *pconn = conn;

  return 0;

push_idtr_init_fail:
  nghttp3_idtr_free(&conn->remote.bidi.idtr);
remote_bidi_idtr_init_fail:
  nghttp3_qpack_encoder_free(&conn->qenc);
qenc_init_fail:
  nghttp3_qpack_decoder_free(&conn->qdec);
qdec_init_fail:
  nghttp3_map_free(&conn->pushes);
pushes_init_fail:
  nghttp3_map_free(&conn->streams);
streams_init_fail:
  nghttp3_mem_free(mem, conn);

  return rv;
}

int nghttp3_conn_client_new(nghttp3_conn **pconn,
                            const nghttp3_callbacks *callbacks,
                            const nghttp3_settings *settings,
                            const nghttp3_mem *mem, void *user_data) {
  int rv;

  rv = conn_new(pconn, /* server = */ 0, callbacks, settings, mem, user_data);
  if (rv != 0) {
    return rv;
  }

  (*pconn)->remote.uni.unsent_max_pushes = settings->max_pushes;

  return 0;
}

int nghttp3_conn_server_new(nghttp3_conn **pconn,
                            const nghttp3_callbacks *callbacks,
                            const nghttp3_settings *settings,
                            const nghttp3_mem *mem, void *user_data) {
  int rv;

  rv = conn_new(pconn, /* server = */ 1, callbacks, settings, mem, user_data);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

static int free_push_promise(nghttp3_map_entry *ent, void *ptr) {
  nghttp3_push_promise *pp = nghttp3_struct_of(ent, nghttp3_push_promise, me);
  const nghttp3_mem *mem = ptr;

  nghttp3_push_promise_del(pp, mem);

  return 0;
}

static int free_stream(nghttp3_map_entry *ent, void *ptr) {
  nghttp3_stream *stream = nghttp3_struct_of(ent, nghttp3_stream, me);

  (void)ptr;

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

  nghttp3_gaptr_free(&conn->remote.uni.push_idtr);

  nghttp3_idtr_free(&conn->remote.bidi.idtr);

  for (i = 0; i < NGHTTP3_URGENCY_LEVELS; ++i) {
    nghttp3_pq_free(&conn->sched[i].spq);
  }

  nghttp3_pq_free(&conn->qpack_blocked_streams);

  nghttp3_qpack_encoder_free(&conn->qenc);
  nghttp3_qpack_decoder_free(&conn->qdec);

  nghttp3_map_each_free(&conn->pushes, free_push_promise, (void *)conn->mem);
  nghttp3_map_free(&conn->pushes);

  nghttp3_map_each_free(&conn->streams, free_stream, NULL);
  nghttp3_map_free(&conn->streams);

  nghttp3_mem_free(conn->mem, conn);
}

nghttp3_ssize nghttp3_conn_read_stream(nghttp3_conn *conn, int64_t stream_id,
                                       const uint8_t *src, size_t srclen,
                                       int fin) {
  nghttp3_stream *stream;
  size_t bidi_nproc;
  int rv;

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    /* TODO Assert idtr */
    /* QUIC transport ensures that this is new stream. */
    if (conn->server) {
      if (nghttp3_client_stream_bidi(stream_id)) {
        rv = nghttp3_idtr_open(&conn->remote.bidi.idtr, stream_id);
        assert(rv == 0);

        conn->rx.max_stream_id_bidi =
            nghttp3_max(conn->rx.max_stream_id_bidi, stream_id);
        rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
        if (rv != 0) {
          return rv;
        }

        if ((conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
            conn->tx.goaway_id <= stream_id) {
          stream->rstate.state = NGHTTP3_REQ_STREAM_STATE_IGN_REST;

          rv = nghttp3_conn_reject_stream(conn, stream);
          if (rv != 0) {
            return rv;
          }
        }
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
      stream->tx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
    } else if (nghttp3_stream_uni(stream_id)) {
      if (srclen == 0 && fin) {
        return 0;
      }

      rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
      if (rv != 0) {
        return rv;
      }

      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
      stream->tx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
    } else {
      /* client doesn't expect to receive new bidirectional stream
         from server. */
      return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
    }
  } else if (conn->server) {
    if (nghttp3_client_stream_bidi(stream_id)) {
      if (stream->rx.hstate == NGHTTP3_HTTP_STATE_NONE) {
        stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
        stream->tx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
      }
    }
  } else if (nghttp3_stream_uni(stream_id) &&
             stream->type == NGHTTP3_STREAM_TYPE_PUSH) {
    if (stream->rx.hstate == NGHTTP3_HTTP_STATE_NONE) {
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
      stream->tx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
    }
  }

  if (srclen == 0 && !fin) {
    return 0;
  }

  if (nghttp3_stream_uni(stream_id)) {
    return nghttp3_conn_read_uni(conn, stream, src, srclen, fin);
  }

  if (fin) {
    stream->flags |= NGHTTP3_STREAM_FLAG_READ_EOF;
  }
  return nghttp3_conn_read_bidi(conn, &bidi_nproc, stream, src, srclen, fin);
}

static nghttp3_ssize conn_read_type(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const uint8_t *src, size_t srclen,
                                    int fin) {
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  int64_t stream_type;

  assert(srclen);

  nread = nghttp3_read_varint(rvint, src, srclen, fin);
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
    if (conn->server) {
      return NGHTTP3_ERR_H3_STREAM_CREATION_ERROR;
    }
    stream->type = NGHTTP3_STREAM_TYPE_PUSH;
    rstate->state = NGHTTP3_PUSH_STREAM_STATE_PUSH_ID;
    break;
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
  default:
    stream->type = NGHTTP3_STREAM_TYPE_UNKNOWN;
    break;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_TYPE_IDENTIFIED;

  return nread;
}

static int conn_delete_stream(nghttp3_conn *conn, nghttp3_stream *stream);

nghttp3_ssize nghttp3_conn_read_uni(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const uint8_t *src, size_t srclen,
                                    int fin) {
  nghttp3_ssize nread = 0;
  nghttp3_ssize nconsumed = 0;
  size_t push_nproc;
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

      rv = conn_delete_stream(conn, stream);
      assert(0 == rv);

      return 0;
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

    if (srclen == 0) {
      return nread;
    }
  }

  switch (stream->type) {
  case NGHTTP3_STREAM_TYPE_CONTROL:
    if (fin) {
      return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_control(conn, stream, src, srclen);
    break;
  case NGHTTP3_STREAM_TYPE_PUSH:
    if (fin) {
      stream->flags |= NGHTTP3_STREAM_FLAG_READ_EOF;
    }
    nconsumed =
        nghttp3_conn_read_push(conn, &push_nproc, stream, src, srclen, fin);
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_ENCODER:
    if (fin) {
      return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_qpack_encoder(conn, src, srclen);
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_DECODER:
    if (fin) {
      return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_qpack_decoder(conn, src, srclen);
    break;
  case NGHTTP3_STREAM_TYPE_UNKNOWN:
    nconsumed = (nghttp3_ssize)srclen;

    rv = conn_call_send_stop_sending(conn, stream,
                                     NGHTTP3_H3_STREAM_CREATION_ERROR);
    if (rv != 0) {
      return rv;
    }
    break;
  default:
    /* unreachable */
    assert(0);
  }

  if (nconsumed < 0) {
    return nconsumed;
  }

  return nread + nconsumed;
}

static int frame_fin(nghttp3_stream_read_state *rstate, size_t len) {
  return (int64_t)len >= rstate->left;
}

nghttp3_ssize nghttp3_conn_read_control(nghttp3_conn *conn,
                                        nghttp3_stream *stream,
                                        const uint8_t *src, size_t srclen) {
  const uint8_t *p = src, *end = src + srclen;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  size_t nconsumed = 0;
  int busy = 0;
  size_t len;

  assert(srclen);

  for (; p != end || busy;) {
    busy = 0;
    switch (rstate->state) {
    case NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), /* fin = */ 0);
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
        break;
      }
      /* Fall through */
    case NGHTTP3_CTRL_STREAM_STATE_FRAME_LENGTH:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), /* fin = */ 0);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      rstate->left = rstate->fr.hd.length = rvint->acc;
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
      case NGHTTP3_FRAME_CANCEL_PUSH:
        if (rstate->left == 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_CANCEL_PUSH;
        break;
      case NGHTTP3_FRAME_SETTINGS:
        /* SETTINGS frame might be empty. */
        if (rstate->left == 0) {
          nghttp3_stream_read_state_reset(rstate);
          break;
        }
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
      case NGHTTP3_FRAME_DATA:
      case NGHTTP3_FRAME_HEADERS:
      case NGHTTP3_FRAME_PUSH_PROMISE:
      case NGHTTP3_H2_FRAME_PRIORITY:
      case NGHTTP3_H2_FRAME_PING:
      case NGHTTP3_H2_FRAME_WINDOW_UPDATE:
      case NGHTTP3_H2_FRAME_CONTINUATION:
        return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
      default:
        /* TODO Handle reserved frame type */
        busy = 1;
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
        break;
      }
      break;
    case NGHTTP3_CTRL_STREAM_STATE_CANCEL_PUSH:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }
      rstate->fr.cancel_push.push_id = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (conn->server) {
        rv = nghttp3_conn_on_server_cancel_push(conn, &rstate->fr.cancel_push);
      } else {
        rv = nghttp3_conn_on_client_cancel_push(conn, &rstate->fr.cancel_push);
      }
      if (rv != 0) {
        return rv;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS:
      for (; p != end;) {
        if (rstate->left == 0) {
          nghttp3_stream_read_state_reset(rstate);
          break;
        }
        /* Read Identifier */
        len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
        assert(len > 0);
        nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
        if (nread < 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        p += nread;
        nconsumed += (size_t)nread;
        rstate->left -= nread;
        if (rvint->left) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_ID;
          return (nghttp3_ssize)nconsumed;
        }
        rstate->fr.settings.iv[0].id = (uint64_t)rvint->acc;
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

        nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
        if (nread < 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        p += nread;
        nconsumed += (size_t)nread;
        rstate->left -= nread;
        if (rvint->left) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE;
          return (nghttp3_ssize)nconsumed;
        }
        rstate->fr.settings.iv[0].value = (uint64_t)rvint->acc;
        nghttp3_varint_read_state_reset(rvint);

        rv =
            nghttp3_conn_on_settings_entry_received(conn, &rstate->fr.settings);
        if (rv != 0) {
          return rv;
        }
      }
      break;
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS_ID:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }
      rstate->fr.settings.iv[0].id = (uint64_t)rvint->acc;
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
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }
      rstate->fr.settings.iv[0].value = (uint64_t)rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      rv = nghttp3_conn_on_settings_entry_received(conn, &rstate->fr.settings);
      if (rv != 0) {
        return rv;
      }

      if (rstate->left) {
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS;
        break;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_GOAWAY:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      if (conn->server && !nghttp3_client_stream_bidi(rvint->acc)) {
        return NGHTTP3_ERR_H3_ID_ERROR;
      }
      if (conn->rx.goaway_id < rvint->acc) {
        return NGHTTP3_ERR_H3_ID_ERROR;
      }

      conn->flags |= NGHTTP3_CONN_FLAG_GOAWAY_RECVED;
      conn->rx.goaway_id = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_MAX_PUSH_ID:
      /* server side only */
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        return (nghttp3_ssize)nconsumed;
      }

      if (conn->local.uni.max_pushes > (uint64_t)rvint->acc) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      conn->local.uni.max_pushes = (uint64_t)rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      p += len;
      nconsumed += len;
      rstate->left -= (int64_t)len;

      if (rstate->left) {
        return (nghttp3_ssize)nconsumed;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    default:
      /* unreachable */
      assert(0);
    }
  }

  return (nghttp3_ssize)nconsumed;
}

nghttp3_ssize nghttp3_conn_read_push(nghttp3_conn *conn, size_t *pnproc,
                                     nghttp3_stream *stream, const uint8_t *src,
                                     size_t srclen, int fin) {
  const uint8_t *p = src, *end = src ? src + srclen : src;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  size_t nconsumed = 0;
  int busy = 0;
  size_t len;
  int64_t push_id;

  if (stream->flags & (NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED |
                       NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED)) {
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
    case NGHTTP3_PUSH_STREAM_STATE_PUSH_ID:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), fin);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        goto almost_done;
      }

      push_id = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      rv = nghttp3_conn_on_stream_push_id(conn, stream, push_id);
      if (rv != 0) {
        if (rv == NGHTTP3_ERR_IGNORE_STREAM) {
          rstate->state = NGHTTP3_PUSH_STREAM_STATE_IGN_REST;
          break;
        }
        return (nghttp3_ssize)rv;
      }

      rstate->state = NGHTTP3_PUSH_STREAM_STATE_FRAME_TYPE;

      if (stream->flags & NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED) {
        if (p != end) {
          rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
          if (rv != 0) {
            return rv;
          }
        }
        *pnproc = (size_t)(p - src);
        return (nghttp3_ssize)nconsumed;
      }

      if (end == p) {
        goto almost_done;
      }

      /* Fall through */
    case NGHTTP3_PUSH_STREAM_STATE_FRAME_TYPE:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), fin);
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
      rstate->state = NGHTTP3_PUSH_STREAM_STATE_FRAME_LENGTH;
      if (p == end) {
        goto almost_done;
      }
      /* Fall through */
    case NGHTTP3_PUSH_STREAM_STATE_FRAME_LENGTH:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), fin);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->left = rstate->fr.hd.length = rvint->acc;
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
        rstate->state = NGHTTP3_PUSH_STREAM_STATE_DATA;
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
        case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
          rv = conn_call_begin_headers(conn, stream);
          break;
        case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
          rv = conn_call_begin_trailers(conn, stream);
          break;
        default:
          /* Unreachable */
          assert(0);
        }

        if (rv != 0) {
          return rv;
        }

        rstate->state = NGHTTP3_PUSH_STREAM_STATE_HEADERS;
        break;
      case NGHTTP3_FRAME_PUSH_PROMISE:
      case NGHTTP3_FRAME_CANCEL_PUSH:
      case NGHTTP3_FRAME_SETTINGS:
      case NGHTTP3_FRAME_GOAWAY:
      case NGHTTP3_FRAME_MAX_PUSH_ID:
      case NGHTTP3_H2_FRAME_PRIORITY:
      case NGHTTP3_H2_FRAME_PING:
      case NGHTTP3_H2_FRAME_WINDOW_UPDATE:
      case NGHTTP3_H2_FRAME_CONTINUATION:
        return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
      default:
        /* TODO Handle reserved frame type */
        busy = 1;
        rstate->state = NGHTTP3_PUSH_STREAM_STATE_IGN_FRAME;
        break;
      }
      break;
    case NGHTTP3_PUSH_STREAM_STATE_DATA:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      rv = nghttp3_conn_on_data(conn, stream, p, len);
      if (rv != 0) {
        return rv;
      }

      p += len;
      rstate->left -= (int64_t)len;

      if (rstate->left) {
        goto almost_done;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_DATA_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_PUSH_STREAM_STATE_HEADERS:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      nread = nghttp3_conn_on_headers(conn, stream, NULL, p, len,
                                      (int64_t)len == rstate->left);
      if (nread < 0) {
        return nread;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;

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
      case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
        rv = nghttp3_http_on_response_headers(&stream->rx.http);
        if (rv != 0) {
          return rv;
        }

        rv = conn_call_end_headers(conn, stream);
        break;
      case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
        rv = conn_call_end_trailers(conn, stream);
        break;
      default:
        /* Unreachable */
        assert(0);
      }

      if (rv != 0) {
        return rv;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_HEADERS_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_PUSH_STREAM_STATE_IGN_FRAME:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      p += len;
      nconsumed += len;
      rstate->left -= (int64_t)len;

      if (rstate->left) {
        goto almost_done;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_PUSH_STREAM_STATE_IGN_REST:
      nconsumed += (size_t)(end - p);
      *pnproc = (size_t)(p - src);
      return (nghttp3_ssize)nconsumed;
    }
  }

almost_done:
  if (fin) {
    switch (rstate->state) {
    case NGHTTP3_PUSH_STREAM_STATE_FRAME_TYPE:
      if (rvint->left) {
        return NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR;
      }
      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_MSG_END);
      if (rv != 0) {
        return rv;
      }
      rv = conn_call_end_stream(conn, stream);
      if (rv != 0) {
        return rv;
      }
      break;
    case NGHTTP3_PUSH_STREAM_STATE_IGN_REST:
      break;
    default:
      return NGHTTP3_ERR_H3_FRAME_ERROR;
    }
  }

  *pnproc = (size_t)(p - src);
  return (nghttp3_ssize)nconsumed;
}

static void conn_delete_push_promise(nghttp3_conn *conn,
                                     nghttp3_push_promise *pp) {
  int rv;

  rv = nghttp3_map_remove(&conn->pushes, (key_type)pp->node.nid.id);
  assert(0 == rv);

  if (!conn->server &&
      !(pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_PUSH_ID_RECLAIMED)) {
    ++conn->remote.uni.unsent_max_pushes;
  }

  nghttp3_push_promise_del(pp, conn->mem);
}

static int conn_delete_stream(nghttp3_conn *conn, nghttp3_stream *stream) {
  int bidi_or_push = nghttp3_stream_bidi_or_push(stream);
  int rv;

  if (bidi_or_push) {
    rv = nghttp3_http_on_remote_end_stream(stream);
    if (rv != 0) {
      return rv;
    }
  }

  rv = conn_call_deferred_consume(conn, stream,
                                  nghttp3_stream_get_buffered_datalen(stream));
  if (rv != 0) {
    return rv;
  }

  if (bidi_or_push && conn->callbacks.stream_close) {
    rv = conn->callbacks.stream_close(conn, stream->node.nid.id,
                                      stream->error_code, conn->user_data,
                                      stream->user_data);
    if (rv != 0) {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
  }

  rv = nghttp3_map_remove(&conn->streams, (key_type)stream->node.nid.id);

  assert(0 == rv);

  if (stream->pp) {
    assert(stream->type == NGHTTP3_STREAM_TYPE_PUSH);

    conn_delete_push_promise(conn, stream->pp);
  }

  nghttp3_stream_del(stream);

  return 0;
}

static int conn_process_blocked_stream_data(nghttp3_conn *conn,
                                            nghttp3_stream *stream) {
  nghttp3_buf *buf;
  size_t nproc;
  nghttp3_ssize nconsumed;
  int rv;
  size_t len;

  for (;;) {
    len = nghttp3_ringbuf_len(&stream->inq);
    if (len == 0) {
      break;
    }
    buf = nghttp3_ringbuf_get(&stream->inq, 0);
    if (nghttp3_stream_uni(stream->node.nid.id)) {
      nconsumed = nghttp3_conn_read_push(
          conn, &nproc, stream, buf->pos, nghttp3_buf_len(buf),
          len == 1 && (stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF));
    } else {
      nconsumed = nghttp3_conn_read_bidi(
          conn, &nproc, stream, buf->pos, nghttp3_buf_len(buf),
          len == 1 && (stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF));
    }
    if (nconsumed < 0) {
      return (int)nconsumed;
    }

    buf->pos += nproc;

    rv = conn_call_deferred_consume(conn, stream, (size_t)nconsumed);
    if (rv != 0) {
      return 0;
    }

    if (nghttp3_buf_len(buf) == 0) {
      nghttp3_buf_free(buf, stream->mem);
      nghttp3_ringbuf_pop_front(&stream->inq);
    }

    if (stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) {
      break;
    }
  }

  if (!(stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) &&
      (stream->flags & NGHTTP3_STREAM_FLAG_CLOSED)) {
    assert(stream->qpack_blocked_pe.index == NGHTTP3_PQ_BAD_INDEX);

    rv = conn_delete_stream(conn, stream);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

nghttp3_ssize nghttp3_conn_read_qpack_encoder(nghttp3_conn *conn,
                                              const uint8_t *src,
                                              size_t srclen) {
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
    if (nghttp3_qpack_stream_context_get_ricnt(&stream->qpack_sctx) >
        nghttp3_qpack_decoder_get_icnt(&conn->qdec)) {
      break;
    }

    nghttp3_conn_qpack_blocked_streams_pop(conn);
    stream->qpack_blocked_pe.index = NGHTTP3_PQ_BAD_INDEX;
    stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED;

    rv = conn_process_blocked_stream_data(conn, stream);
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

static int conn_update_stream_priority(nghttp3_conn *conn,
                                       nghttp3_stream *stream, uint8_t pri) {
  if (stream->node.pri == pri) {
    return 0;
  }

  nghttp3_conn_unschedule_stream(conn, stream);

  stream->node.pri = pri;

  assert(nghttp3_stream_bidi_or_push(stream));

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_schedule_stream(conn, stream);
  }

  return 0;
}

nghttp3_ssize nghttp3_conn_read_bidi(nghttp3_conn *conn, size_t *pnproc,
                                     nghttp3_stream *stream, const uint8_t *src,
                                     size_t srclen, int fin) {
  const uint8_t *p = src, *end = src ? src + srclen : src;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  size_t nconsumed = 0;
  int busy = 0;
  size_t len;
  nghttp3_push_promise *pp;
  nghttp3_push_promise fake_pp = {{0}, {{0}, 0, {0}, 0, 0, 0}, {0}, NULL, -1,
                                  0};
  nghttp3_frame_entry frent;

  if (stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) {
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
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), fin);
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
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), fin);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->left = rstate->fr.hd.length = rvint->acc;
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
          /* Unreachable */
          assert(0);
        }

        if (rv != 0) {
          return rv;
        }

        rstate->state = NGHTTP3_REQ_STREAM_STATE_HEADERS;
        break;
      case NGHTTP3_FRAME_PUSH_PROMISE:
        if (conn->server) {
          return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
        }

        if (rstate->left == 0) {
          return NGHTTP3_ERR_H3_FRAME_ERROR;
        }

        /* No stream->rx.hstate change with PUSH_PROMISE */

        rstate->state = NGHTTP3_REQ_STREAM_STATE_PUSH_PROMISE_PUSH_ID;
        break;
      case NGHTTP3_FRAME_CANCEL_PUSH:
      case NGHTTP3_FRAME_SETTINGS:
      case NGHTTP3_FRAME_GOAWAY:
      case NGHTTP3_FRAME_MAX_PUSH_ID:
      case NGHTTP3_H2_FRAME_PRIORITY:
      case NGHTTP3_H2_FRAME_PING:
      case NGHTTP3_H2_FRAME_WINDOW_UPDATE:
      case NGHTTP3_H2_FRAME_CONTINUATION:
        return NGHTTP3_ERR_H3_FRAME_UNEXPECTED;
      default:
        /* TODO Handle reserved frame type */
        busy = 1;
        rstate->state = NGHTTP3_REQ_STREAM_STATE_IGN_FRAME;
        break;
      }
      break;
    case NGHTTP3_REQ_STREAM_STATE_DATA:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      rv = nghttp3_conn_on_data(conn, stream, p, len);
      if (rv != 0) {
        return rv;
      }
      p += len;
      rstate->left -= (int64_t)len;

      if (rstate->left) {
        goto almost_done;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_DATA_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_PUSH_PROMISE_PUSH_ID:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p),
                                  (int64_t)len == rstate->left);
      if (nread < 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->fr.push_promise.push_id = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (rstate->left == 0) {
        return NGHTTP3_ERR_H3_FRAME_ERROR;
      }

      rv = nghttp3_conn_on_push_promise_push_id(
          conn, rstate->fr.push_promise.push_id, stream);
      if (rv != 0) {
        if (rv == NGHTTP3_ERR_IGNORE_PUSH_PROMISE) {
          rstate->state = NGHTTP3_REQ_STREAM_STATE_IGN_PUSH_PROMISE;
          if (p == end) {
            goto almost_done;
          }
          break;
        }

        return rv;
      }

      rstate->state = NGHTTP3_REQ_STREAM_STATE_PUSH_PROMISE;

      if (p == end) {
        goto almost_done;
      }
      /* Fall through */
    case NGHTTP3_REQ_STREAM_STATE_PUSH_PROMISE:
      pp =
          nghttp3_conn_find_push_promise(conn, rstate->fr.push_promise.push_id);

      assert(pp);

      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      nread = nghttp3_conn_on_headers(conn, stream, pp, p, len,
                                      (int64_t)len == rstate->left);
      if (nread < 0) {
        return nread;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;

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

      rv = nghttp3_http_on_request_headers(&pp->http);
      if (rv != 0) {
        return rv;
      }

      pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_RECVED;

      rv = conn_call_end_push_promise(conn, stream, pp->node.nid.id);
      if (rv != 0) {
        return rv;
      }

      /* Find pp again because application might call
         nghttp3_conn_cancel_push and it may delete pp. */
      pp =
          nghttp3_conn_find_push_promise(conn, rstate->fr.push_promise.push_id);
      if (!pp) {
        nghttp3_stream_read_state_reset(rstate);
        break;
      }

      if (!pp->stream && (pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_CANCELLED)) {
        if (pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_RECV_CANCEL) {
          rv = conn_call_cancel_push(conn, pp->node.nid.id, pp->stream);
          if (rv != 0) {
            return rv;
          }
        }

        conn_delete_push_promise(conn, pp);

        nghttp3_stream_read_state_reset(rstate);
        break;
      }

      if (pp->stream) {
        ++conn->remote.uni.unsent_max_pushes;
        pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_PUSH_ID_RECLAIMED;

        if (!(pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL)) {
          assert(pp->stream->flags & NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED);

          rv = conn_call_push_stream(conn, pp->node.nid.id, pp->stream);
          if (rv != 0) {
            return rv;
          }

          pp->stream->flags &=
              (uint16_t)~NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED;

          rv = conn_process_blocked_stream_data(conn, pp->stream);
          if (rv != 0) {
            return rv;
          }
        }
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_IGN_PUSH_PROMISE:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      nread = nghttp3_conn_on_headers(conn, stream, &fake_pp, p, len,
                                      (int64_t)len == rstate->left);
      if (nread < 0) {
        return nread;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;

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

      pp =
          nghttp3_conn_find_push_promise(conn, rstate->fr.push_promise.push_id);
      if (pp) {
        pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_RECVED;

        if ((conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
            conn->tx.goaway_id <= pp->node.nid.id) {
          if (pp->stream) {
            rv = nghttp3_conn_reject_push_stream(conn, pp->stream);
            if (rv != 0) {
              return rv;
            }
          } else {
            frent.fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
            frent.fr.cancel_push.push_id = pp->node.nid.id;

            rv = nghttp3_stream_frq_add(conn->tx.ctrl, &frent);
            if (rv != 0) {
              return rv;
            }

            conn_delete_push_promise(conn, pp);
          }
        }
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_HEADERS:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      nread = nghttp3_conn_on_headers(conn, stream, NULL, p, len,
                                      (int64_t)len == rstate->left);
      if (nread < 0) {
        return nread;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;

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
        /* Unreachable */
        assert(0);
      }

      if (rv != 0) {
        return rv;
      }

      switch (stream->rx.hstate) {
      case NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN:
        /* Only server utilizes priority information to schedule
           streams. */
        if (conn->server) {
          rv = conn_update_stream_priority(conn, stream, stream->rx.http.pri);
          if (rv != 0) {
            return rv;
          }
        }
        /* fall through */
      case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
        rv = conn_call_end_headers(conn, stream);
        break;
      case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
      case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
        rv = conn_call_end_trailers(conn, stream);
        break;
      default:
        /* Unreachable */
        assert(0);
      }

      if (rv != 0) {
        return rv;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_HEADERS_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_IGN_FRAME:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      p += len;
      nconsumed += len;
      rstate->left -= (int64_t)len;

      if (rstate->left) {
        goto almost_done;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_IGN_REST:
      nconsumed += (size_t)(end - p);
      *pnproc = (size_t)(p - src);
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

int nghttp3_conn_on_data(nghttp3_conn *conn, nghttp3_stream *stream,
                         const uint8_t *data, size_t datalen) {
  int rv;

  rv = nghttp3_http_on_data_chunk(stream, datalen);
  if (rv != 0) {
    return rv;
  }

  if (!conn->callbacks.recv_data) {
    return 0;
  }

  rv = conn->callbacks.recv_data(conn, stream->node.nid.id, data, datalen,
                                 conn->user_data, stream->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int push_idtr_push(nghttp3_gaptr *push_idtr, int64_t push_id) {
  int rv;

  rv = nghttp3_gaptr_push(push_idtr, (uint64_t)push_id, 1);
  if (rv != 0) {
    return rv;
  }

  /* Server SHOULD use push ID sequentially, but even if it does, we
     might see gaps in push IDs because we might stop reading stream
     which ignores PUSH_PROMISE.  In order to limit the number of
     gaps, drop earlier gaps if certain limit is reached.  This makes
     otherwise valid push ignored.*/
  if (nghttp3_ksl_len(&push_idtr->gap) > 100) {
    nghttp3_gaptr_drop_first_gap(push_idtr);
  }

  return 0;
}

int nghttp3_conn_on_push_promise_push_id(nghttp3_conn *conn, int64_t push_id,
                                         nghttp3_stream *stream) {
  int rv;
  nghttp3_gaptr *push_idtr = &conn->remote.uni.push_idtr;
  nghttp3_push_promise *pp;

  if (conn->remote.uni.max_pushes <= (uint64_t)push_id) {
    return NGHTTP3_ERR_H3_ID_ERROR;
  }

  pp = nghttp3_conn_find_push_promise(conn, push_id);
  if (pp) {
    if ((pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_BOUND) ||
        (pp->stream_id != -1 && pp->stream_id != stream->node.nid.id)) {
      return NGHTTP3_ERR_IGNORE_PUSH_PROMISE;
    }
    if (pp->stream) {
      assert(pp->stream->flags & NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED);
      /* Push unidirectional stream has already been received and
         blocked */
    } else if (pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_CANCELLED) {
      /* We will call begin_push_promise callback even if push is
         cancelled */
    } else {
      return NGHTTP3_ERR_H3_FRAME_ERROR;
    }

    assert(pp->stream_id == -1);

    pp->stream_id = stream->node.nid.id;
    pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_BOUND;
  } else if (nghttp3_gaptr_is_pushed(push_idtr, (uint64_t)push_id, 1)) {
    return NGHTTP3_ERR_IGNORE_PUSH_PROMISE;
  } else {
    rv = push_idtr_push(push_idtr, push_id);
    if (rv != 0) {
      return rv;
    }

    rv = nghttp3_conn_create_push_promise(conn, &pp, push_id, &stream->node);
    if (rv != 0) {
      return rv;
    }
  }

  conn->rx.max_push_id = nghttp3_max(conn->rx.max_push_id, push_id);

  if ((conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
      conn->tx.goaway_id <= push_id) {
    return NGHTTP3_ERR_IGNORE_PUSH_PROMISE;
  }

  rv = conn_call_begin_push_promise(conn, stream, push_id);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

int nghttp3_conn_on_client_cancel_push(nghttp3_conn *conn,
                                       const nghttp3_frame_cancel_push *fr) {
  nghttp3_push_promise *pp;
  nghttp3_gaptr *push_idtr = &conn->remote.uni.push_idtr;
  int rv;

  if (conn->remote.uni.max_pushes <= (uint64_t)fr->push_id) {
    return NGHTTP3_ERR_H3_ID_ERROR;
  }

  pp = nghttp3_conn_find_push_promise(conn, fr->push_id);
  if (pp == NULL) {
    if (nghttp3_gaptr_is_pushed(push_idtr, (uint64_t)fr->push_id, 1)) {
      /* push is already cancelled or server is misbehaving */
      return 0;
    }

    /* We have not received PUSH_PROMISE yet */
    rv = push_idtr_push(push_idtr, fr->push_id);
    if (rv != 0) {
      return rv;
    }

    conn->rx.max_push_id = nghttp3_max(conn->rx.max_push_id, fr->push_id);

    rv = nghttp3_conn_create_push_promise(conn, &pp, fr->push_id, NULL);
    if (rv != 0) {
      return rv;
    }

    pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_RECV_CANCEL;

    /* cancel_push callback will be called after PUSH_PROMISE frame is
       completely received. */

    return 0;
  }

  if (pp->stream) {
    return 0;
  }

  if (pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_RECVED) {
    rv = conn_call_cancel_push(conn, pp->node.nid.id, pp->stream);
    if (rv != 0) {
      return rv;
    }

    conn_delete_push_promise(conn, pp);

    return 0;
  }

  pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_RECV_CANCEL;

  return 0;
}

static nghttp3_pq *conn_get_sched_pq(nghttp3_conn *conn, nghttp3_tnode *tnode) {
  uint32_t urgency = nghttp3_pri_uint8_urgency(tnode->pri);

  assert(urgency < NGHTTP3_URGENCY_LEVELS);

  return &conn->sched[urgency].spq;
}

int nghttp3_conn_on_server_cancel_push(nghttp3_conn *conn,
                                       const nghttp3_frame_cancel_push *fr) {
  nghttp3_push_promise *pp;
  nghttp3_stream *stream;
  int rv;

  if (conn->local.uni.next_push_id <= fr->push_id) {
    return NGHTTP3_ERR_H3_ID_ERROR;
  }

  pp = nghttp3_conn_find_push_promise(conn, fr->push_id);
  if (pp == NULL) {
    return 0;
  }

  stream = pp->stream;

  rv = conn_call_cancel_push(conn, fr->push_id, stream);
  if (rv != 0) {
    return rv;
  }

  if (stream) {
    rv = nghttp3_conn_close_stream(conn, stream->node.nid.id,
                                   NGHTTP3_H3_REQUEST_CANCELLED);
    if (rv != 0) {
      assert(NGHTTP3_ERR_STREAM_NOT_FOUND != rv);
      return rv;
    }
    return 0;
  }

  nghttp3_tnode_unschedule(&pp->node, conn_get_sched_pq(conn, &pp->node));

  conn_delete_push_promise(conn, pp);

  return 0;
}

int nghttp3_conn_on_stream_push_id(nghttp3_conn *conn, nghttp3_stream *stream,
                                   int64_t push_id) {
  nghttp3_push_promise *pp;
  int rv;

  if (nghttp3_gaptr_is_pushed(&conn->remote.uni.push_idtr, (uint64_t)push_id,
                              1)) {
    pp = nghttp3_conn_find_push_promise(conn, push_id);
    if (pp) {
      if (pp->stream) {
        return NGHTTP3_ERR_H3_ID_ERROR;
      }
      pp->stream = stream;
      stream->pp = pp;

      assert(!(pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL));

      if ((conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
          conn->tx.goaway_id <= push_id) {
        rv = nghttp3_conn_reject_push_stream(conn, stream);
        if (rv != 0) {
          return rv;
        }
        return NGHTTP3_ERR_IGNORE_STREAM;
      }

      if (pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_RECVED) {
        ++conn->remote.uni.unsent_max_pushes;
        pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_PUSH_ID_RECLAIMED;

        return conn_call_push_stream(conn, push_id, stream);
      }

      stream->flags |= NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED;

      return 0;
    }

    /* Push ID has been received, but pp is gone.  This means that
       push is cancelled or server is misbehaving.  We have no
       information to distinguish the two, so just cancel QPACK stream
       just in case, and ask application to send STOP_SENDING and
       ignore all frames in this stream. */
    rv = nghttp3_conn_cancel_push_stream(conn, stream);
    if (rv != 0) {
      return rv;
    }
    return NGHTTP3_ERR_IGNORE_STREAM;
  }

  if (conn->remote.uni.max_pushes <= (uint64_t)push_id) {
    return NGHTTP3_ERR_H3_ID_ERROR;
  }

  rv = push_idtr_push(&conn->remote.uni.push_idtr, push_id);
  if (rv != 0) {
    return rv;
  }

  /* Don't know the associated stream of PUSH_PROMISE.  It doesn't
     matter because client sends nothing to this stream. */
  rv = nghttp3_conn_create_push_promise(conn, &pp, push_id, NULL);
  if (rv != 0) {
    return rv;
  }

  pp->stream = stream;
  stream->pp = pp;
  stream->flags |= NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED;

  return 0;
}

static nghttp3_ssize conn_decode_headers(nghttp3_conn *conn,
                                         nghttp3_stream *stream,
                                         nghttp3_push_promise *pp,
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
  int ignore_pp = 0;

  if (pp) {
    request = 1;
    ignore_pp = pp->stream_id != stream->node.nid.id;
    if (ignore_pp) {
      http = NULL;
    } else {
      http = &pp->http;
    }
  } else {
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
      /* Unreachable */
      assert(0);
    }
    http = &stream->rx.http;
  }

  nghttp3_buf_wrap_init(&buf, (uint8_t *)src, srclen);
  buf.last = buf.end;

  for (;;) {
    nread = nghttp3_qpack_decoder_read_request(qdec, &stream->qpack_sctx, &nv,
                                               &flags, buf.pos,
                                               nghttp3_buf_len(&buf), fin);

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
      if (ignore_pp) {
        nghttp3_rcbuf_decref(nv.name);
        nghttp3_rcbuf_decref(nv.value);

        continue;
      }

      assert(http);

      rv = nghttp3_http_on_header(http, stream->rstate.fr.hd.type, &nv, request,
                                  trailers);
      switch (rv) {
      case NGHTTP3_ERR_MALFORMED_HTTP_HEADER:
        break;
      case NGHTTP3_ERR_REMOVE_HTTP_HEADER:
        rv = 0;
        break;
      case 0:
        if (pp) {
          if (conn->callbacks.recv_push_promise) {
            rv = conn->callbacks.recv_push_promise(
                conn, stream->node.nid.id, pp->node.nid.id, nv.token, nv.name,
                nv.value, nv.flags, conn->user_data, stream->user_data);
          }
          break;
        }
        if (recv_header) {
          rv = recv_header(conn, stream->node.nid.id, nv.token, nv.name,
                           nv.value, nv.flags, conn->user_data,
                           stream->user_data);
        }
        break;
      default:
        /* Unreachable */
        assert(0);
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
                                      nghttp3_push_promise *pp,
                                      const uint8_t *src, size_t srclen,
                                      int fin) {
  if (srclen == 0 && !fin) {
    return 0;
  }

  return conn_decode_headers(conn, stream, pp, src, srclen, fin);
}

int nghttp3_conn_on_settings_entry_received(nghttp3_conn *conn,
                                            const nghttp3_frame_settings *fr) {
  const nghttp3_settings_entry *ent = &fr->iv[0];
  nghttp3_settings *dest = &conn->remote.settings;
  int rv;
  size_t max_table_capacity = SIZE_MAX;
  size_t max_blocked_streams = SIZE_MAX;

  /* TODO Check for duplicates */
  switch (ent->id) {
  case NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE:
    dest->max_field_section_size = ent->value;
    break;
  case NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY:
    dest->qpack_max_table_capacity = (size_t)ent->value;
    max_table_capacity =
        nghttp3_min(max_table_capacity, dest->qpack_max_table_capacity);
    rv = nghttp3_qpack_encoder_set_hard_max_dtable_size(&conn->qenc,
                                                        max_table_capacity);
    if (rv != 0) {
      return rv;
    }
    rv = nghttp3_qpack_encoder_set_max_dtable_size(&conn->qenc,
                                                   max_table_capacity);
    if (rv != 0) {
      return rv;
    }
    break;
  case NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS:
    dest->qpack_blocked_streams = (size_t)ent->value;
    max_blocked_streams =
        nghttp3_min(max_blocked_streams, dest->qpack_blocked_streams);
    rv =
        nghttp3_qpack_encoder_set_max_blocked(&conn->qenc, max_blocked_streams);
    if (rv != 0) {
      return rv;
    }
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

static int conn_stream_acked_data(nghttp3_stream *stream, int64_t stream_id,
                                  size_t datalen, void *user_data) {
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
  nghttp3_stream_callbacks callbacks = {
      conn_stream_acked_data,
  };

  rv = nghttp3_stream_new(&stream, stream_id, conn->next_seq, &callbacks,
                          conn->mem);
  if (rv != 0) {
    return rv;
  }

  stream->conn = conn;

  rv = nghttp3_map_insert(&conn->streams, &stream->me);
  if (rv != 0) {
    nghttp3_stream_del(stream);
    return rv;
  }

  ++conn->next_seq;
  *pstream = stream;

  return 0;
}

int nghttp3_conn_create_push_promise(nghttp3_conn *conn,
                                     nghttp3_push_promise **ppp,
                                     int64_t push_id,
                                     nghttp3_tnode *assoc_tnode) {
  nghttp3_push_promise *pp;
  int rv;

  rv = nghttp3_push_promise_new(&pp, push_id, conn->next_seq, assoc_tnode,
                                conn->mem);
  if (rv != 0) {
    return rv;
  }

  rv = nghttp3_map_insert(&conn->pushes, &pp->me);
  if (rv != 0) {
    nghttp3_push_promise_del(pp, conn->mem);
    return rv;
  }

  ++conn->next_seq;
  *ppp = pp;

  return 0;
}

nghttp3_stream *nghttp3_conn_find_stream(nghttp3_conn *conn,
                                         int64_t stream_id) {
  nghttp3_map_entry *me;

  me = nghttp3_map_find(&conn->streams, (key_type)stream_id);
  if (me == NULL) {
    return NULL;
  }

  return nghttp3_struct_of(me, nghttp3_stream, me);
}

nghttp3_push_promise *nghttp3_conn_find_push_promise(nghttp3_conn *conn,
                                                     int64_t push_id) {
  nghttp3_map_entry *me;

  me = nghttp3_map_find(&conn->pushes, (key_type)push_id);
  if (me == NULL) {
    return NULL;
  }

  return nghttp3_struct_of(me, nghttp3_push_promise, me);
}

int nghttp3_conn_bind_control_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream;
  nghttp3_frame_entry frent;
  int rv;

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

  frent.fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  frent.aux.settings.local_settings = &conn->local.settings;

  return nghttp3_stream_frq_add(stream, &frent);
}

int nghttp3_conn_bind_qpack_streams(nghttp3_conn *conn, int64_t qenc_stream_id,
                                    int64_t qdec_stream_id) {
  nghttp3_stream *stream;
  int rv;

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
  nghttp3_ssize n;

  assert(veccnt > 0);

  /* If stream is blocked by read callback, don't attempt to fill
     more. */
  if (!(stream->flags & NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED)) {
    rv = nghttp3_stream_fill_outq(stream);
    if (rv != 0) {
      return rv;
    }
  }

  if (!nghttp3_stream_uni(stream->node.nid.id) && conn->tx.qenc &&
      !nghttp3_stream_is_blocked(conn->tx.qenc)) {
    n = nghttp3_stream_writev(conn->tx.qenc, pfin, vec, veccnt);
    if (n < 0) {
      return n;
    }
    if (n) {
      *pstream_id = conn->tx.qenc->node.nid.id;
      return n;
    }
  }

  n = nghttp3_stream_writev(stream, pfin, vec, veccnt);
  if (n < 0) {
    return n;
  }
  /* We might just want to write stream fin without sending any stream
     data. */
  if (n == 0 && *pfin == 0) {
    return 0;
  }

  *pstream_id = stream->node.nid.id;

  return n;
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
    if (!(conn->flags & NGHTTP3_CONN_FLAG_MAX_PUSH_ID_QUEUED) &&
        !(conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED) &&
        conn->remote.uni.unsent_max_pushes > conn->remote.uni.max_pushes) {
      rv = nghttp3_conn_submit_max_push_id(conn);
      if (rv != 0) {
        return rv;
      }
    }

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

  if (nghttp3_stream_bidi_or_push(stream) &&
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

    if (tnode->nid.type == NGHTTP3_NODE_ID_TYPE_PUSH) {
      return nghttp3_struct_of(tnode, nghttp3_push_promise, node)->stream;
    }

    return nghttp3_struct_of(tnode, nghttp3_stream, node);
  }

  return NULL;
}

int nghttp3_conn_add_write_offset(nghttp3_conn *conn, int64_t stream_id,
                                  size_t n) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);
  int rv;

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  rv = nghttp3_stream_add_outq_offset(stream, n);
  if (rv != 0) {
    return rv;
  }

  stream->unscheduled_nwrite += n;

  if (!nghttp3_stream_bidi_or_push(stream)) {
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
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  return nghttp3_stream_add_ack_offset(stream, n);
}

static int conn_submit_headers_data(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const nghttp3_nv *nva, size_t nvlen,
                                    const nghttp3_data_reader *dr) {
  int rv;
  nghttp3_nv *nnva;
  nghttp3_frame_entry frent;

  rv = nghttp3_nva_copy(&nnva, nva, nvlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  frent.fr.hd.type = NGHTTP3_FRAME_HEADERS;
  frent.fr.headers.nva = nnva;
  frent.fr.headers.nvlen = nvlen;

  rv = nghttp3_stream_frq_add(stream, &frent);
  if (rv != 0) {
    nghttp3_nva_del(nnva, conn->mem);
    return rv;
  }

  if (dr) {
    frent.fr.hd.type = NGHTTP3_FRAME_DATA;
    frent.aux.data.dr = *dr;

    rv = nghttp3_stream_frq_add(stream, &frent);
    if (rv != 0) {
      return rv;
    }
  }

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_schedule_stream(conn, stream);
  }

  return 0;
}

static nghttp3_tnode *stream_get_dependency_node(nghttp3_stream *stream) {
  if (stream->pp) {
    assert(stream->type == NGHTTP3_STREAM_TYPE_PUSH);
    return &stream->pp->node;
  }

  return &stream->node;
}

int nghttp3_conn_schedule_stream(nghttp3_conn *conn, nghttp3_stream *stream) {
  /* Assume that stream stays on the same urgency level */
  int rv;

  rv = nghttp3_tnode_schedule(stream_get_dependency_node(stream),
                              conn_get_sched_pq(conn, &stream->node),
                              stream->unscheduled_nwrite);
  if (rv != 0) {
    return rv;
  }

  stream->unscheduled_nwrite = 0;

  return 0;
}

int nghttp3_conn_ensure_stream_scheduled(nghttp3_conn *conn,
                                         nghttp3_stream *stream) {
  if (nghttp3_tnode_is_scheduled(stream_get_dependency_node(stream))) {
    return 0;
  }

  return nghttp3_conn_schedule_stream(conn, stream);
}

void nghttp3_conn_unschedule_stream(nghttp3_conn *conn,
                                    nghttp3_stream *stream) {
  nghttp3_tnode_unschedule(stream_get_dependency_node(stream),
                           conn_get_sched_pq(conn, &stream->node));
}

int nghttp3_conn_submit_request(nghttp3_conn *conn, int64_t stream_id,
                                const nghttp3_nv *nva, size_t nvlen,
                                const nghttp3_data_reader *dr,
                                void *stream_user_data) {
  nghttp3_stream *stream;
  int rv;

  assert(!conn->server);
  assert(conn->tx.qenc);

  assert(nghttp3_client_stream_bidi(stream_id));

  /* TODO Should we check that stream_id is client stream_id? */
  /* TODO Check GOAWAY last stream ID */
  if (nghttp3_stream_uni(stream_id)) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

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
  stream->tx.hstate = NGHTTP3_HTTP_STATE_REQ_END;
  stream->user_data = stream_user_data;

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

int nghttp3_conn_submit_push_promise(nghttp3_conn *conn, int64_t *ppush_id,
                                     int64_t stream_id, const nghttp3_nv *nva,
                                     size_t nvlen) {
  nghttp3_stream *stream;
  int rv;
  nghttp3_nv *nnva;
  nghttp3_frame_entry frent;
  int64_t push_id;
  nghttp3_push_promise *pp;

  assert(conn->server);
  assert(conn->tx.qenc);
  assert(nghttp3_client_stream_bidi(stream_id));

  if (conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_RECVED) {
    return NGHTTP3_ERR_CONN_CLOSING;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  if (conn->local.uni.max_pushes <= (uint64_t)conn->local.uni.next_push_id) {
    return NGHTTP3_ERR_PUSH_ID_BLOCKED;
  }

  push_id = conn->local.uni.next_push_id;

  rv = nghttp3_conn_create_push_promise(conn, &pp, push_id, &stream->node);
  if (rv != 0) {
    return rv;
  }

  ++conn->local.uni.next_push_id;

  rv = nghttp3_nva_copy(&nnva, nva, nvlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  frent.fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  frent.fr.push_promise.push_id = push_id;
  frent.fr.push_promise.nva = nnva;
  frent.fr.push_promise.nvlen = nvlen;

  rv = nghttp3_stream_frq_add(stream, &frent);
  if (rv != 0) {
    nghttp3_nva_del(nnva, conn->mem);
    return rv;
  }

  *ppush_id = push_id;

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_schedule_stream(conn, stream);
  }

  return 0;
}

int nghttp3_conn_bind_push_stream(nghttp3_conn *conn, int64_t push_id,
                                  int64_t stream_id) {
  nghttp3_push_promise *pp;
  nghttp3_stream *stream;
  int rv;

  assert(conn->server);
  assert(nghttp3_server_stream_uni(stream_id));

  pp = nghttp3_conn_find_push_promise(conn, push_id);
  if (pp == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  assert(NULL == nghttp3_conn_find_stream(conn, stream_id));

  rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
  if (rv != 0) {
    return rv;
  }

  stream->type = NGHTTP3_STREAM_TYPE_PUSH;
  stream->pp = pp;

  pp->stream = stream;

  rv = nghttp3_stream_write_stream_type_push_id(stream);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

int nghttp3_conn_cancel_push(nghttp3_conn *conn, int64_t push_id) {
  if (conn->server) {
    return nghttp3_conn_server_cancel_push(conn, push_id);
  }
  return nghttp3_conn_client_cancel_push(conn, push_id);
}

int nghttp3_conn_server_cancel_push(nghttp3_conn *conn, int64_t push_id) {
  nghttp3_push_promise *pp;
  nghttp3_frame_entry frent;
  int rv;

  assert(conn->tx.ctrl);

  pp = nghttp3_conn_find_push_promise(conn, push_id);
  if (pp == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  if (!(pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL)) {
    frent.fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
    frent.fr.cancel_push.push_id = push_id;

    rv = nghttp3_stream_frq_add(conn->tx.ctrl, &frent);
    if (rv != 0) {
      return rv;
    }

    pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL;
  }

  if (pp->stream) {
    /* CANCEL_PUSH will be sent, but it does not affect pushed stream.
       Stream should be explicitly cancelled. */
    rv = conn_call_reset_stream(conn, pp->stream, NGHTTP3_H3_REQUEST_CANCELLED);
    if (rv != 0) {
      return rv;
    }
  }

  nghttp3_tnode_unschedule(&pp->node, conn_get_sched_pq(conn, &pp->node));

  conn_delete_push_promise(conn, pp);

  return 0;
}

int nghttp3_conn_client_cancel_push(nghttp3_conn *conn, int64_t push_id) {
  nghttp3_push_promise *pp;
  nghttp3_frame_entry frent;
  int rv;

  pp = nghttp3_conn_find_push_promise(conn, push_id);
  if (pp == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  if (pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL) {
    return 0;
  }

  if (!(pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_RECVED)) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  if (pp->stream) {
    pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_SENT_CANCEL;
    return nghttp3_conn_cancel_push_stream(conn, pp->stream);
  }

  frent.fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
  frent.fr.cancel_push.push_id = push_id;

  rv = nghttp3_stream_frq_add(conn->tx.ctrl, &frent);
  if (rv != 0) {
    return rv;
  }

  conn_delete_push_promise(conn, pp);

  return 0;
}

int nghttp3_conn_submit_shutdown_notice(nghttp3_conn *conn) {
  nghttp3_frame_entry frent;
  int rv;

  assert(conn->tx.ctrl);

  frent.fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  frent.fr.goaway.id = conn->server ? (1ull << 62) - 4 : (1ull << 62) - 1;

  assert(frent.fr.goaway.id <= conn->tx.goaway_id);

  rv = nghttp3_stream_frq_add(conn->tx.ctrl, &frent);
  if (rv != 0) {
    return rv;
  }

  conn->tx.goaway_id = frent.fr.goaway.id;
  conn->flags |= NGHTTP3_CONN_FLAG_GOAWAY_QUEUED;

  return 0;
}

int nghttp3_conn_shutdown(nghttp3_conn *conn) {
  nghttp3_frame_entry frent;
  int rv;

  assert(conn->tx.ctrl);

  frent.fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  if (conn->server) {
    frent.fr.goaway.id =
        nghttp3_min((1ll << 62) - 4, conn->rx.max_stream_id_bidi + 4);
  } else {
    frent.fr.goaway.id = nghttp3_min((1ll << 62) - 1, conn->rx.max_push_id + 1);
  }

  assert(frent.fr.goaway.id <= conn->tx.goaway_id);

  rv = nghttp3_stream_frq_add(conn->tx.ctrl, &frent);
  if (rv != 0) {
    return rv;
  }

  conn->tx.goaway_id = frent.fr.goaway.id;
  conn->flags |= NGHTTP3_CONN_FLAG_GOAWAY_QUEUED;

  return 0;
}

int nghttp3_conn_reject_stream(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  rv = nghttp3_qpack_decoder_cancel_stream(&conn->qdec, stream->node.nid.id);
  if (rv != 0) {
    return rv;
  }

  rv = conn_call_send_stop_sending(conn, stream, NGHTTP3_H3_REQUEST_REJECTED);
  if (rv != 0) {
    return rv;
  }

  return conn_call_reset_stream(conn, stream, NGHTTP3_H3_REQUEST_REJECTED);
}

static int conn_reject_push_stream(nghttp3_conn *conn, nghttp3_stream *stream,
                                   uint64_t app_error_code) {
  int rv;

  /* TODO Send Stream Cancellation if we have not processed all
     incoming stream data up to fin */
  rv = nghttp3_qpack_decoder_cancel_stream(&conn->qdec, stream->node.nid.id);
  if (rv != 0) {
    return rv;
  }

  return conn_call_send_stop_sending(conn, stream, app_error_code);
}

int nghttp3_conn_reject_push_stream(nghttp3_conn *conn,
                                    nghttp3_stream *stream) {
  return conn_reject_push_stream(conn, stream, NGHTTP3_H3_REQUEST_REJECTED);
}

int nghttp3_conn_cancel_push_stream(nghttp3_conn *conn,
                                    nghttp3_stream *stream) {
  return conn_reject_push_stream(conn, stream, NGHTTP3_H3_REQUEST_CANCELLED);
}

int nghttp3_conn_block_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_FC_BLOCKED;
  stream->unscheduled_nwrite = 0;

  if (nghttp3_stream_bidi_or_push(stream)) {
    nghttp3_conn_unschedule_stream(conn, stream);
  }

  return 0;
}

int nghttp3_conn_shutdown_stream_write(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_SHUT_WR;
  stream->unscheduled_nwrite = 0;

  if (nghttp3_stream_bidi_or_push(stream)) {
    nghttp3_conn_unschedule_stream(conn, stream);
  }

  return 0;
}

int nghttp3_conn_unblock_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_FC_BLOCKED;

  if (nghttp3_stream_bidi_or_push(stream) &&
      nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_ensure_stream_scheduled(conn, stream);
  }

  return 0;
}

int nghttp3_conn_resume_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED;

  if (nghttp3_stream_bidi_or_push(stream) &&
      nghttp3_stream_require_schedule(stream)) {
    return nghttp3_conn_ensure_stream_scheduled(conn, stream);
  }

  return 0;
}

int nghttp3_conn_close_stream(nghttp3_conn *conn, int64_t stream_id,
                              uint64_t app_error_code) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  if (nghttp3_stream_uni(stream_id) &&
      stream->type != NGHTTP3_STREAM_TYPE_PUSH &&
      stream->type != NGHTTP3_STREAM_TYPE_UNKNOWN) {
    return NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM;
  }

  stream->error_code = app_error_code;

  nghttp3_conn_unschedule_stream(conn, stream);

  if (stream->qpack_blocked_pe.index == NGHTTP3_PQ_BAD_INDEX &&
      (conn->server || !stream->pp ||
       (stream->pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_RECVED))) {
    return conn_delete_stream(conn, stream);
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_CLOSED;
  return 0;
}

int nghttp3_conn_reset_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream;

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream) {
    stream->flags |= NGHTTP3_STREAM_FLAG_RESET;
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

int nghttp3_conn_submit_max_push_id(nghttp3_conn *conn) {
  nghttp3_frame_entry frent;
  int rv;

  assert(conn->tx.ctrl);
  assert(!(conn->flags & NGHTTP3_CONN_FLAG_MAX_PUSH_ID_QUEUED));

  frent.fr.hd.type = NGHTTP3_FRAME_MAX_PUSH_ID;
  /* The acutal push_id is set when frame is serialized */

  rv = nghttp3_stream_frq_add(conn->tx.ctrl, &frent);
  if (rv != 0) {
    return rv;
  }

  conn->flags |= NGHTTP3_CONN_FLAG_MAX_PUSH_ID_QUEUED;

  return 0;
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

int64_t nghttp3_conn_get_frame_payload_left(nghttp3_conn *conn,
                                            int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  return stream->rstate.left;
}

int nghttp3_conn_get_stream_priority(nghttp3_conn *conn, nghttp3_pri *dest,
                                     int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  assert(conn->server);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  dest->urgency = nghttp3_pri_uint8_urgency(stream->rx.http.pri);
  dest->inc = nghttp3_pri_uint8_inc(stream->rx.http.pri);

  return 0;
}

int nghttp3_conn_set_stream_priority(nghttp3_conn *conn, int64_t stream_id,
                                     const nghttp3_pri *pri) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  assert(conn->server);
  assert(pri->urgency < NGHTTP3_URGENCY_LEVELS);
  assert(pri->inc == 0 || pri->inc == 1);

  if (stream == NULL) {
    return NGHTTP3_ERR_STREAM_NOT_FOUND;
  }

  stream->rx.http.pri = nghttp3_pri_to_uint8(pri);

  return conn_update_stream_priority(conn, stream, stream->rx.http.pri);
}

int nghttp3_conn_is_remote_qpack_encoder_stream(nghttp3_conn *conn,
                                                int64_t stream_id) {
  nghttp3_stream *stream;

  if (!conn_remote_stream_uni(conn, stream_id)) {
    return 0;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  return stream && stream->type == NGHTTP3_STREAM_TYPE_QPACK_ENCODER;
}

void nghttp3_settings_default(nghttp3_settings *settings) {
  memset(settings, 0, sizeof(nghttp3_settings));
  settings->max_field_section_size = NGHTTP3_VARINT_MAX;
}

int nghttp3_push_promise_new(nghttp3_push_promise **ppp, int64_t push_id,
                             uint64_t seq, nghttp3_tnode *assoc_tnode,
                             const nghttp3_mem *mem) {
  nghttp3_push_promise *pp;
  nghttp3_node_id nid;

  pp = nghttp3_mem_calloc(mem, 1, sizeof(nghttp3_push_promise));
  if (pp == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  nghttp3_tnode_init(
      &pp->node, nghttp3_node_id_init(&nid, NGHTTP3_NODE_ID_TYPE_PUSH, push_id),
      seq, NGHTTP3_DEFAULT_URGENCY);

  pp->me.key = (key_type)push_id;
  pp->node.nid.id = push_id;
  pp->http.status_code = -1;
  pp->http.content_length = -1;

  if (assoc_tnode) {
    assert(assoc_tnode->nid.type == NGHTTP3_NODE_ID_TYPE_STREAM);

    pp->stream_id = assoc_tnode->nid.id;
    pp->flags |= NGHTTP3_PUSH_PROMISE_FLAG_BOUND;
  } else {
    pp->stream_id = -1;
  }

  *ppp = pp;

  return 0;
}

void nghttp3_push_promise_del(nghttp3_push_promise *pp,
                              const nghttp3_mem *mem) {
  if (pp == NULL) {
    return;
  }

  nghttp3_tnode_free(&pp->node);

  nghttp3_mem_free(mem, pp);
}
