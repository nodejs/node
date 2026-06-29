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

int nghttp3_wt_session_new(nghttp3_wt_session **pwts, int64_t session_id,
                           const nghttp3_mem *mem) {
  *pwts = nghttp3_mem_malloc(mem, sizeof(**pwts));
  if (*pwts == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  **pwts = (nghttp3_wt_session){
    .session_id = session_id,
  };

  return 0;
}

void nghttp3_wt_session_del(nghttp3_wt_session *wts, const nghttp3_mem *mem) {
  if (!wts) {
    return;
  }

  nghttp3_mem_free(mem, wts->rx.error_msg.base);
  nghttp3_mem_free(mem, wts->tx.error_msg.base);

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

void nghttp3_wt_ctrl_read_state_reset(nghttp3_wt_ctrl_read_state *rstate) {
  *rstate = (nghttp3_wt_ctrl_read_state){0};
}
