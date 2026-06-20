/*
 * nghttp3
 *
 * Copyright (c) 2025 nghttp3 contributors
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
#include "nghttp3_wt.h"

#include <assert.h>

#include "nghttp3_mem.h"
#include "nghttp3_stream.h"

int nghttp3_wt_session_new(nghttp3_wt_session **pwts, int64_t session_id,
                           const nghttp3_mem *mem) {
  *pwts = nghttp3_mem_calloc(mem, 1, sizeof(**pwts));
  if (*pwts == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  (*pwts)->session_id = session_id;

  return 0;
}

void nghttp3_wt_session_del(nghttp3_wt_session *wts, const nghttp3_mem *mem) {
  if (!wts) {
    return;
  }

  if (wts->tx.error_msg.base) {
    nghttp3_mem_free(mem, wts->tx.error_msg.base);
  }

  nghttp3_mem_free(mem, wts);
}

void nghttp3_wt_session_add_stream(nghttp3_wt_session *wts,
                                   nghttp3_stream *stream) {
  assert(!stream->wt.session);
  assert(!stream->wt.prev);
  assert(!stream->wt.next);

  stream->wt.session = wts;
  stream->flags |= NGHTTP3_STREAM_FLAG_WT_DATA;

  if (wts->head) {
    stream->wt.next = wts->head;
    wts->head->wt.prev = stream;
  }

  wts->head = stream;
}

void nghttp3_wt_session_remove_stream(nghttp3_wt_session *wts,
                                      nghttp3_stream *stream) {
  assert(stream->wt.session);

  if (stream->wt.prev) {
    stream->wt.prev->wt.next = stream->wt.next;
  }

  if (stream->wt.next) {
    stream->wt.next->wt.prev = stream->wt.prev;
  }

  if (wts->head == stream) {
    wts->head = stream->wt.next;
  }

  stream->wt.session = NULL;
  stream->wt.prev = stream->wt.next = NULL;
}

int nghttp3_wt_session_read_stream(nghttp3_wt_session *wts, const uint8_t *src,
                                   size_t srclen) {
  const uint8_t *p, *end;
  nghttp3_wt_ctrl_read_state *rstate = &wts->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  nghttp3_ssize nread;
  nghttp3_exfr_cpsl *cpsl = &rstate->cpsl;
  size_t len;
  size_t i;

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

      wts->error_code = cpsl->wt_close_session.error_code;

      if (rstate->left == 0) {
        nghttp3_wt_ctrl_read_state_reset(rstate);

        return NGHTTP3_ERR_WT_SESSION_GONE;
      }

      rstate->state = NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_MSG;

      break;
    case NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_MSG:
      len = (size_t)nghttp3_min(rstate->left, (uint64_t)(end - p));

      rstate->left -= len;
      if (rstate->left) {
        break;
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

void nghttp3_wt_ctrl_read_state_reset(nghttp3_wt_ctrl_read_state *rstate) {
  *rstate = (nghttp3_wt_ctrl_read_state){0};
}
