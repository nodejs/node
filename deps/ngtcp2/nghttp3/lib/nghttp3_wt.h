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
#ifndef NGHTTP3_WT_H
#define NGHTTP3_WT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <nghttp3/nghttp3.h>

#include "nghttp3_stream.h"

/* NGHTTP3_WT_SESSION_FLAG_CONFIRMED indicates that WebTransport
   session has been established. */
#define NGHTTP3_WT_SESSION_FLAG_CONFIRMED 0x1
/* NGHTTP3_WT_SESSION_FLAG_RESP_SUBMITTED indicates that HTTP/3
   response has been submitted via nghttp3_conn_submit_wt_response. */
#define NGHTTP3_WT_SESSION_FLAG_RESP_SUBMITTED 0x2

typedef enum nghttp3_wt_ctrl_stream_state {
  NGHTTP3_WT_CTRL_STREAM_STATE_TYPE,
  NGHTTP3_WT_CTRL_STREAM_STATE_LENGTH,
  NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_CODE,
  NGHTTP3_WT_CTRL_STREAM_STATE_WT_CLOSE_SESSION_ERROR_MSG,
  NGHTTP3_WT_CTRL_STREAM_STATE_IGN,
} nghttp3_wt_ctrl_stream_state;

typedef struct nghttp3_wt_ctrl_read_state {
  nghttp3_varint_read_state rvint;
  nghttp3_exfr_cpsl cpsl;
  uint64_t left;
  size_t field_left;
  int state;
} nghttp3_wt_ctrl_read_state;

typedef struct nghttp3_wt_session {
  nghttp3_wt_ctrl_read_state rstate;
  struct {
    nghttp3_vec error_msg;
  } tx;
  struct {
    nghttp3_vec error_msg;
    uint32_t error_code;
  } rx;
  int64_t session_id;
  nghttp3_stream *head;
  uint32_t flags;
} nghttp3_wt_session;

void nghttp3_wt_session_add_stream(nghttp3_wt_session *wts,
                                   nghttp3_stream *stream);

void nghttp3_wt_session_remove_stream(nghttp3_wt_session *wts,
                                      nghttp3_stream *stream);

int nghttp3_wt_session_new(nghttp3_wt_session **pwts, int64_t stream_id,
                           const nghttp3_mem *mem);

void nghttp3_wt_session_del(nghttp3_wt_session *wts, const nghttp3_mem *mem);

void nghttp3_wt_ctrl_read_state_reset(nghttp3_wt_ctrl_read_state *rstate);

#endif /* !defined(NGHTTP3_WT_H) */
