/*
 * ngtcp2
 *
 * Copyright (c) 2019 ngtcp2 contributors
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
#include "ngtcp2_qlog.h"

#include <assert.h>

#include "ngtcp2_str.h"
#include "ngtcp2_vec.h"

void ngtcp2_qlog_init(ngtcp2_qlog *qlog, ngtcp2_qlog_write write,
                      ngtcp2_tstamp ts, void *user_data) {
  qlog->write = write;
  qlog->ts = qlog->last_ts = ts;
  qlog->user_data = user_data;
}

static uint8_t *write_string(uint8_t *p, const ngtcp2_vec *s) {
  *p++ = '"';
  if (s->len) {
    p = ngtcp2_cpymem(p, s->base, s->len);
  }
  *p++ = '"';
  return p;
}

#define NGTCP2_LOWER_XDIGITS "0123456789abcdef"

static uint8_t *write_hex(uint8_t *p, const ngtcp2_vec *s) {
  const uint8_t *b = s->base, *end = s->base + s->len;
  *p++ = '"';
  for (; b != end; ++b) {
    *p++ = (uint8_t)NGTCP2_LOWER_XDIGITS[*b >> 4];
    *p++ = (uint8_t)NGTCP2_LOWER_XDIGITS[*b & 0xf];
  }
  *p++ = '"';
  return p;
}

static uint8_t *write_cid(uint8_t *p, const ngtcp2_cid *cid) {
  ngtcp2_vec value;
  return write_hex(p, ngtcp2_vec_init(&value, cid->data, cid->datalen));
}

static uint8_t *write_number(uint8_t *p, uint64_t n) {
  size_t nlen = 0;
  uint64_t t;
  uint8_t *res;

  if (n == 0) {
    *p++ = '0';
    return p;
  }
  for (t = n; t; t /= 10, ++nlen)
    ;
  p += nlen;
  res = p;
  for (; n; n /= 10) {
    *--p = (uint8_t)((n % 10) + '0');
  }
  return res;
}

static uint8_t *write_numstr(uint8_t *p, uint64_t n) {
  *p++ = '"';
  p = write_number(p, n);
  *p++ = '"';
  return p;
}

static uint8_t *write_tstamp(uint8_t *p, ngtcp2_tstamp ts) {
  return write_number(p, ts / NGTCP2_MILLISECONDS);
}

static uint8_t *write_duration(uint8_t *p, ngtcp2_duration duration) {
  return write_number(p, duration / NGTCP2_MILLISECONDS);
}

static uint8_t *write_bool(uint8_t *p, int b) {
  if (b) {
    return ngtcp2_cpymem(p, "true", sizeof("true") - 1);
  }
  return ngtcp2_cpymem(p, "false", sizeof("false") - 1);
}

static uint8_t *write_pair(uint8_t *p, const ngtcp2_vec *name,
                           const ngtcp2_vec *value) {
  p = write_string(p, name);
  *p++ = ':';
  return write_string(p, value);
}

static uint8_t *write_pair_hex(uint8_t *p, const ngtcp2_vec *name,
                               const ngtcp2_vec *value) {
  p = write_string(p, name);
  *p++ = ':';
  return write_hex(p, value);
}

static uint8_t *write_pair_numstr(uint8_t *p, const ngtcp2_vec *name,
                                  uint64_t value) {
  p = write_string(p, name);
  *p++ = ':';
  p = write_numstr(p, value);
  return p;
}

static uint8_t *write_pair_number(uint8_t *p, const ngtcp2_vec *name,
                                  uint64_t value) {
  p = write_string(p, name);
  *p++ = ':';
  return write_number(p, value);
}

static uint8_t *write_pair_duration(uint8_t *p, const ngtcp2_vec *name,
                                    ngtcp2_tstamp duration) {
  p = write_string(p, name);
  *p++ = ':';
  return write_duration(p, duration);
}

static uint8_t *write_pair_bool(uint8_t *p, const ngtcp2_vec *name, int b) {
  p = write_string(p, name);
  *p++ = ':';
  return write_bool(p, b);
}

static uint8_t *write_pair_cid(uint8_t *p, const ngtcp2_vec *name,
                               const ngtcp2_cid *cid) {
  p = write_string(p, name);
  *p++ = ':';
  return write_cid(p, cid);
}

static uint8_t *write_trace_start(uint8_t *p, int server) {
#define NGTCP2_M                                                               \
  "\"traces\":[{\"vantage_point\":{\"name\":\"ngtcp2\",\"type\":\""
  p = ngtcp2_cpymem(p, NGTCP2_M, sizeof(NGTCP2_M) - 1);
#undef NGTCP2_M
  if (server) {
    p = ngtcp2_cpymem(p, "server", sizeof("server") - 1);
  } else {
    p = ngtcp2_cpymem(p, "client", sizeof("client") - 1);
  }
  *p++ = '"';
  *p++ = '}';
  *p++ = ',';
  return p;
}

static uint8_t *write_common_fields(uint8_t *p, const ngtcp2_cid *odcid) {
  ngtcp2_vec name;
#define NGTCP2_M                                                               \
  "\"common_fields\":{\"protocol_type\":\"QUIC_HTTP3\",\"reference_"           \
  "time\":\"0\","
  p = ngtcp2_cpymem(p, NGTCP2_M, sizeof(NGTCP2_M) - 1);
#undef NGTCP2_M
  p = write_pair_cid(p, ngtcp2_vec_lit(&name, "group_id"), odcid);
  *p++ = ',';
  p = write_pair_cid(p, ngtcp2_vec_lit(&name, "ODCID"), odcid);
  *p++ = '}';
  *p++ = ',';
  return p;
}

static uint8_t *write_event_fields(uint8_t *p) {
#define NGTCP2_M                                                               \
  "\"event_fields\":[\"relative_time\",\"category\",\"event\",\"data\"],"
  p = ngtcp2_cpymem(p, NGTCP2_M, sizeof(NGTCP2_M) - 1);
#undef NGTCP2_M
  return p;
}

static uint8_t *write_events_start(uint8_t *p) {
#define NGTCP2_M "\"events\":["
  p = ngtcp2_cpymem(p, NGTCP2_M, sizeof(NGTCP2_M) - 1);
#undef NGTCP2_M
  return p;
}

static uint8_t *write_events_end(uint8_t *p) {
  *p++ = '[';
  *p++ = ']';
  *p++ = ']';
  return p;
}

static uint8_t *write_trace_end(uint8_t *p) {
  *p++ = '}';
  *p++ = ']';
  return p;
}

void ngtcp2_qlog_start(ngtcp2_qlog *qlog, const ngtcp2_cid *odcid, int server) {
  uint8_t buf[1024];
  ngtcp2_vec name, value;
  uint8_t *p = buf;

  if (!qlog->write) {
    return;
  }

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "qlog_version"),
                 ngtcp2_vec_lit(&value, "draft-01"));
  *p++ = ',';
  p = write_trace_start(p, server);
  p = write_common_fields(p, odcid);
  p = write_event_fields(p);
  p = write_events_start(p);

  qlog->write(qlog->user_data, buf, (size_t)(p - buf));
}

void ngtcp2_qlog_end(ngtcp2_qlog *qlog) {
  uint8_t buf[256];
  uint8_t *p = buf;

  if (!qlog->write) {
    return;
  }

  p = write_events_end(p);
  p = write_trace_end(p);
  *p++ = '}';

  qlog->write(qlog->user_data, buf, (size_t)(p - buf));
}

static uint8_t *write_pkt_hd(uint8_t *p, const ngtcp2_pkt_hd *hd,
                             size_t pktlen) {
  ngtcp2_vec value;

  /*
   * {"packet_number":"0000000000000000000","packet_size":0000000000000000000}
   */
#define NGTCP2_QLOG_PKT_HD_OVERHEAD 73

  *p++ = '{';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&value, "packet_number"),
                        (uint64_t)hd->pkt_num);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&value, "packet_size"), pktlen);
  /* TODO Write DCIL and DCID */
  /* TODO Write SCIL and SCID */
  *p++ = '}';
  return p;
}

static ngtcp2_vec *qlog_pkt_type(ngtcp2_vec *dest, const ngtcp2_pkt_hd *hd) {
  if (hd->flags & NGTCP2_PKT_FLAG_LONG_FORM) {
    switch (hd->type) {
    case NGTCP2_PKT_INITIAL:
      return ngtcp2_vec_lit(dest, "initial");
    case NGTCP2_PKT_HANDSHAKE:
      return ngtcp2_vec_lit(dest, "handshake");
    case NGTCP2_PKT_0RTT:
      return ngtcp2_vec_lit(dest, "0RTT");
    default:
      return ngtcp2_vec_lit(dest, "unknown");
    }
  }

  return ngtcp2_vec_lit(dest, "1RTT");
}

static uint8_t *write_padding_frame(uint8_t *p, const ngtcp2_padding *fr) {
  ngtcp2_vec name, value;
  (void)fr;

  /* {"frame_type":"padding"} */
#define NGTCP2_QLOG_PADDING_FRAME_OVERHEAD 24

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "padding"));
  *p++ = '}';

  return p;
}

static uint8_t *write_ping_frame(uint8_t *p, const ngtcp2_ping *fr) {
  ngtcp2_vec name, value;
  (void)fr;

  /* {"frame_type":"ping"} */
#define NGTCP2_QLOG_PING_FRAME_OVERHEAD 21

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "ping"));
  *p++ = '}';

  return p;
}

static uint8_t *write_ack_frame(uint8_t *p, const ngtcp2_ack *fr) {
  ngtcp2_vec name, value;
  int64_t largest_ack, min_ack;
  size_t i;
  const ngtcp2_ack_blk *blk;

  /*
   * {"frame_type":"ack","ack_delay":0000000000000000000,"acked_ranges":[]}
   *
   * each range:
   * ["0000000000000000000","0000000000000000000"],
   */
#define NGTCP2_QLOG_ACK_FRAME_BASE_OVERHEAD 70
#define NGTCP2_QLOG_ACK_FRAME_RANGE_OVERHEAD 46

  *p++ = '{';
  /* TODO Handle ACK ECN */
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "ack"));
  *p++ = ',';
  p = write_pair_duration(p, ngtcp2_vec_lit(&name, "ack_delay"),
                          fr->ack_delay_unscaled);
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&name, "acked_ranges"));
  *p++ = ':';
  *p++ = '[';

  largest_ack = fr->largest_ack;
  min_ack = fr->largest_ack - (int64_t)fr->first_ack_blklen;

  *p++ = '[';
  p = write_numstr(p, (uint64_t)min_ack);
  if (largest_ack != min_ack) {
    *p++ = ',';
    p = write_numstr(p, (uint64_t)largest_ack);
  }
  *p++ = ']';

  for (i = 0; i < fr->num_blks; ++i) {
    blk = &fr->blks[i];
    largest_ack = min_ack - (int64_t)blk->gap - 2;
    min_ack = largest_ack - (int64_t)blk->blklen;
    *p++ = ',';
    *p++ = '[';
    p = write_numstr(p, (uint64_t)min_ack);
    if (largest_ack != min_ack) {
      *p++ = ',';
      p = write_numstr(p, (uint64_t)largest_ack);
    }
    *p++ = ']';
  }

  *p++ = ']';
  *p++ = '}';

  return p;
}

static uint8_t *write_reset_stream_frame(uint8_t *p,
                                         const ngtcp2_reset_stream *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"reset_stream","stream_id":"0000000000000000000","error_code":0000000000000000000,"final_size":"0000000000000000000"}
   */
#define NGTCP2_QLOG_RESET_STREAM_FRAME_OVERHEAD 131

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "reset_stream"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "stream_id"),
                        (uint64_t)fr->stream_id);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "error_code"),
                        fr->app_error_code);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "final_size"), fr->final_size);
  *p++ = '}';

  return p;
}

static uint8_t *write_stop_sending_frame(uint8_t *p,
                                         const ngtcp2_stop_sending *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"stop_sending","stream_id":"0000000000000000000","error_code":0000000000000000000}
   */
#define NGTCP2_QLOG_STOP_SENDING_FRAME_OVERHEAD 96

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "stop_sending"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "stream_id"),
                        (uint64_t)fr->stream_id);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "error_code"),
                        fr->app_error_code);
  *p++ = '}';

  return p;
}

static uint8_t *write_crypto_frame(uint8_t *p, const ngtcp2_crypto *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"crypto","offset":"0000000000000000000","length":0000000000000000000}
   */
#define NGTCP2_QLOG_CRYPTO_FRAME_OVERHEAD 83

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "crypto"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "offset"), fr->offset);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "length"),
                        ngtcp2_vec_len(fr->data, fr->datacnt));
  *p++ = '}';

  return p;
}

static uint8_t *write_new_token_frame(uint8_t *p, const ngtcp2_new_token *fr) {
  ngtcp2_vec name, value;
  (void)fr;

  /*
   * {"frame_type":"new_token"}
   */
#define NGTCP2_QLOG_NEW_TOKEN_FRAME_OVERHEAD 26

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "new_token"));
  /* TODO Write token here */
  *p++ = '}';

  return p;
}

static uint8_t *write_stream_frame(uint8_t *p, const ngtcp2_stream *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"stream","stream_id":"0000000000000000000","offset":"0000000000000000000","length":0000000000000000000,"fin":true}
   */
#define NGTCP2_QLOG_STREAM_FRAME_OVERHEAD 128

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "stream"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "stream_id"),
                        (uint64_t)fr->stream_id);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "offset"), fr->offset);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "length"),
                        ngtcp2_vec_len(fr->data, fr->datacnt));
  if (fr->fin) {
    *p++ = ',';
    p = write_pair_bool(p, ngtcp2_vec_lit(&name, "fin"), 1);
  }
  *p++ = '}';

  return p;
}

static uint8_t *write_max_data_frame(uint8_t *p, const ngtcp2_max_data *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"max_data","maximum":"0000000000000000000"}
   */
#define NGTCP2_QLOG_MAX_DATA_FRAME_OVERHEAD 57

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "max_data"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "maximum"), fr->max_data);
  *p++ = '}';

  return p;
}

static uint8_t *write_max_stream_data_frame(uint8_t *p,
                                            const ngtcp2_max_stream_data *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"max_stream_data","stream_id":"0000000000000000000","maximum":"0000000000000000000"}
   */
#define NGTCP2_QLOG_MAX_STREAM_DATA_FRAME_OVERHEAD 98

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "max_stream_data"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "stream_id"),
                        (uint64_t)fr->stream_id);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "maximum"),
                        fr->max_stream_data);
  *p++ = '}';

  return p;
}

static uint8_t *write_max_streams_frame(uint8_t *p,
                                        const ngtcp2_max_streams *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"max_streams","stream_type":"unidirectional","maximum":"0000000000000000000"}
   */
#define NGTCP2_QLOG_MAX_STREAMS_FRAME_OVERHEAD 91

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "max_streams"));
  *p++ = ',';
  p = write_pair(p, ngtcp2_vec_lit(&name, "stream_type"),
                 fr->type == NGTCP2_FRAME_MAX_STREAMS_BIDI
                     ? ngtcp2_vec_lit(&value, "bidirectional")
                     : ngtcp2_vec_lit(&value, "unidirectional"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "maximum"), fr->max_streams);
  *p++ = '}';

  return p;
}

static uint8_t *write_data_blocked_frame(uint8_t *p,
                                         const ngtcp2_data_blocked *fr) {
  ngtcp2_vec name, value;
  (void)fr;

  /*
   * {"frame_type":"data_blocked"}
   */
#define NGTCP2_QLOG_DATA_BLOCKED_FRAME_OVERHEAD 29

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "data_blocked"));
  *p++ = '}';

  return p;
}

static uint8_t *
write_stream_data_blocked_frame(uint8_t *p,
                                const ngtcp2_stream_data_blocked *fr) {
  ngtcp2_vec name, value;
  (void)fr;

  /*
   * {"frame_type":"stream_data_blocked"}
   */
#define NGTCP2_QLOG_STREAM_DATA_BLOCKED_FRAME_OVERHEAD 36

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "stream_data_blocked"));
  *p++ = '}';

  return p;
}

static uint8_t *write_streams_blocked_frame(uint8_t *p,
                                            const ngtcp2_streams_blocked *fr) {
  ngtcp2_vec name, value;
  (void)fr;

  /*
   * {"frame_type":"streams_blocked"}
   */
#define NGTCP2_QLOG_STREAMS_BLOCKED_FRAME_OVERHEAD 32

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "streams_blocked"));
  *p++ = '}';

  return p;
}

static uint8_t *
write_new_connection_id_frame(uint8_t *p, const ngtcp2_new_connection_id *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"new_connection_id","sequence_number":"0000000000000000000","retire_prior_to":"0000000000000000000","length":0000000000000000000,"connection_id":"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","reset_token":"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"}
   */
#define NGTCP2_QLOG_NEW_CONNECTION_ID_FRAME_OVERHEAD 251

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "new_connection_id"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "sequence_number"), fr->seq);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "retire_prior_to"),
                        fr->retire_prior_to);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "length"), fr->cid.datalen);
  *p++ = ',';
  p = write_pair_cid(p, ngtcp2_vec_lit(&name, "connection_id"), &fr->cid);
  *p++ = ',';
  p = write_pair_hex(p, ngtcp2_vec_lit(&name, "reset_token"),
                     ngtcp2_vec_init(&value, fr->stateless_reset_token,
                                     sizeof(fr->stateless_reset_token)));
  *p++ = '}';

  return p;
}

static uint8_t *
write_retire_connection_id_frame(uint8_t *p,
                                 const ngtcp2_retire_connection_id *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"retire_connection_id","sequence_number":"0000000000000000000"}
   */
#define NGTCP2_QLOG_RETIRE_CONNECTION_ID_FRAME_OVERHEAD 77

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "retire_connection_id"));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "sequence_number"), fr->seq);
  *p++ = '}';

  return p;
}

static uint8_t *write_path_challenge_frame(uint8_t *p,
                                           const ngtcp2_path_challenge *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"path_challenge","data":"xxxxxxxxxxxxxxxx"}
   */
#define NGTCP2_QLOG_PATH_CHALLENGE_FRAME_OVERHEAD 57

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "path_challenge"));
  *p++ = ',';
  p = write_pair_hex(p, ngtcp2_vec_lit(&name, "data"),
                     ngtcp2_vec_init(&value, fr->data, sizeof(fr->data)));
  *p++ = '}';

  return p;
}

static uint8_t *write_path_response_frame(uint8_t *p,
                                          const ngtcp2_path_response *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"path_response","data":"xxxxxxxxxxxxxxxx"}
   */
#define NGTCP2_QLOG_PATH_RESPONSE_FRAME_OVERHEAD 56

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "path_response"));
  *p++ = ',';
  p = write_pair_hex(p, ngtcp2_vec_lit(&name, "data"),
                     ngtcp2_vec_init(&value, fr->data, sizeof(fr->data)));
  *p++ = '}';

  return p;
}

static uint8_t *
write_connection_close_frame(uint8_t *p, const ngtcp2_connection_close *fr) {
  ngtcp2_vec name, value;

  /*
   * {"frame_type":"connection_close","error_space":"application","error_code":0000000000000000000,"raw_error_code":0000000000000000000,"reason":""}
   */
#define NGTCP2_QLOG_CONNECTION_CLOSE_FRAME_OVERHEAD 143

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "connection_close"));
  *p++ = ',';
  p = write_pair(p, ngtcp2_vec_lit(&name, "error_space"),
                 fr->type == NGTCP2_FRAME_CONNECTION_CLOSE
                     ? ngtcp2_vec_lit(&value, "transport")
                     : ngtcp2_vec_lit(&value, "application"));
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "error_code"), fr->error_code);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "raw_error_code"),
                        fr->error_code);
  *p++ = ',';
  /* TODO Write reason by escaping non-printables */
  p = write_pair(p, ngtcp2_vec_lit(&name, "reason"),
                 ngtcp2_vec_lit(&value, ""));
  /* TODO Write trigger_frame_type */
  *p++ = '}';

  return p;
}

static uint8_t *write_handshake_done_frame(uint8_t *p,
                                           const ngtcp2_handshake_done *fr) {
  ngtcp2_vec name, value;
  (void)fr;

  /*
   * {"frame_type":"handshake_done"}
   */
#define NGTCP2_QLOG_HANDSHAKE_DONE_FRAME_OVERHEAD 31

  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "frame_type"),
                 ngtcp2_vec_lit(&value, "handshake_done"));
  *p++ = '}';

  return p;
}

static void qlog_pkt_write_start(ngtcp2_qlog *qlog, const ngtcp2_pkt_hd *hd,
                                 int sent) {
  uint8_t *p;
  ngtcp2_vec name, value;

  if (!qlog->write) {
    return;
  }

  ngtcp2_buf_reset(&qlog->buf);
  p = qlog->buf.last;

  *p++ = '[';
  p = write_tstamp(p, qlog->last_ts - qlog->ts);
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "transport"));
  *p++ = ',';
  p = write_string(p, sent ? ngtcp2_vec_lit(&value, "packet_sent")
                           : ngtcp2_vec_lit(&value, "packet_received"));
  *p++ = ',';
  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "packet_type"),
                 qlog_pkt_type(&value, hd));
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "frames"));
  *p++ = ':';
  *p++ = '[';

  qlog->buf.last = p;
}

static void qlog_pkt_write_end(ngtcp2_qlog *qlog, const ngtcp2_pkt_hd *hd,
                               size_t pktlen) {
  uint8_t *p = qlog->buf.last;
  ngtcp2_vec value;

  if (!qlog->write) {
    return;
  }

  /*
   * ],"header":}],
   */
#define NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD (14 + NGTCP2_QLOG_PKT_HD_OVERHEAD)

  assert(ngtcp2_buf_left(&qlog->buf) >= NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD);
  assert(ngtcp2_buf_len(&qlog->buf));

  /* Eat last ',' */
  if (*(p - 1) == ',') {
    --p;
  }
  *p++ = ']';
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "header"));
  *p++ = ':';
  p = write_pkt_hd(p, hd, pktlen);
  *p++ = '}';
  *p++ = ']';
  *p++ = ',';

  qlog->buf.last = p;

  qlog->write(qlog->user_data, qlog->buf.pos, ngtcp2_buf_len(&qlog->buf));
}

void ngtcp2_qlog_write_frame(ngtcp2_qlog *qlog, const ngtcp2_frame *fr) {
  uint8_t *p = qlog->buf.last;

  if (!qlog->write) {
    return;
  }

  switch (fr->type) {
  case NGTCP2_FRAME_PADDING:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_PADDING_FRAME_OVERHEAD + 1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_padding_frame(p, &fr->padding);
    break;
  case NGTCP2_FRAME_PING:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_PING_FRAME_OVERHEAD + 1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_ping_frame(p, &fr->ping);
    break;
  case NGTCP2_FRAME_ACK:
  case NGTCP2_FRAME_ACK_ECN:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_ACK_FRAME_BASE_OVERHEAD +
            NGTCP2_QLOG_ACK_FRAME_RANGE_OVERHEAD * (1 + fr->ack.num_blks) + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_ack_frame(p, &fr->ack);
    break;
  case NGTCP2_FRAME_RESET_STREAM:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_RESET_STREAM_FRAME_OVERHEAD +
                                          1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_reset_stream_frame(p, &fr->reset_stream);
    break;
  case NGTCP2_FRAME_STOP_SENDING:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_STOP_SENDING_FRAME_OVERHEAD +
                                          1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_stop_sending_frame(p, &fr->stop_sending);
    break;
  case NGTCP2_FRAME_CRYPTO:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_CRYPTO_FRAME_OVERHEAD + 1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_crypto_frame(p, &fr->crypto);
    break;
  case NGTCP2_FRAME_NEW_TOKEN:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_NEW_TOKEN_FRAME_OVERHEAD + 1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_new_token_frame(p, &fr->new_token);
    break;
  case NGTCP2_FRAME_STREAM:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_STREAM_FRAME_OVERHEAD + 1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_stream_frame(p, &fr->stream);
    break;
  case NGTCP2_FRAME_MAX_DATA:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_MAX_DATA_FRAME_OVERHEAD + 1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_max_data_frame(p, &fr->max_data);
    break;
  case NGTCP2_FRAME_MAX_STREAM_DATA:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_MAX_STREAM_DATA_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_max_stream_data_frame(p, &fr->max_stream_data);
    break;
  case NGTCP2_FRAME_MAX_STREAMS_BIDI:
  case NGTCP2_FRAME_MAX_STREAMS_UNI:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_MAX_STREAMS_FRAME_OVERHEAD +
                                          1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_max_streams_frame(p, &fr->max_streams);
    break;
  case NGTCP2_FRAME_DATA_BLOCKED:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_DATA_BLOCKED_FRAME_OVERHEAD +
                                          1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_data_blocked_frame(p, &fr->data_blocked);
    break;
  case NGTCP2_FRAME_STREAM_DATA_BLOCKED:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_STREAM_DATA_BLOCKED_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_stream_data_blocked_frame(p, &fr->stream_data_blocked);
    break;
  case NGTCP2_FRAME_STREAMS_BLOCKED_BIDI:
  case NGTCP2_FRAME_STREAMS_BLOCKED_UNI:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_STREAMS_BLOCKED_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_streams_blocked_frame(p, &fr->streams_blocked);
    break;
  case NGTCP2_FRAME_NEW_CONNECTION_ID:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_NEW_CONNECTION_ID_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_new_connection_id_frame(p, &fr->new_connection_id);
    break;
  case NGTCP2_FRAME_RETIRE_CONNECTION_ID:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_RETIRE_CONNECTION_ID_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_retire_connection_id_frame(p, &fr->retire_connection_id);
    break;
  case NGTCP2_FRAME_PATH_CHALLENGE:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_PATH_CHALLENGE_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_path_challenge_frame(p, &fr->path_challenge);
    break;
  case NGTCP2_FRAME_PATH_RESPONSE:
    if (ngtcp2_buf_left(&qlog->buf) < NGTCP2_QLOG_PATH_RESPONSE_FRAME_OVERHEAD +
                                          1 +
                                          NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_path_response_frame(p, &fr->path_response);
    break;
  case NGTCP2_FRAME_CONNECTION_CLOSE:
  case NGTCP2_FRAME_CONNECTION_CLOSE_APP:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_CONNECTION_CLOSE_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_connection_close_frame(p, &fr->connection_close);
    break;
  case NGTCP2_FRAME_HANDSHAKE_DONE:
    if (ngtcp2_buf_left(&qlog->buf) <
        NGTCP2_QLOG_HANDSHAKE_DONE_FRAME_OVERHEAD + 1 +
            NGTCP2_QLOG_PKT_WRITE_END_OVERHEAD) {
      return;
    }
    p = write_handshake_done_frame(p, &fr->handshake_done);
    break;
  default:
    assert(0);
  }

  *p++ = ',';

  qlog->buf.last = p;
}

void ngtcp2_qlog_pkt_received_start(ngtcp2_qlog *qlog,
                                    const ngtcp2_pkt_hd *hd) {
  qlog_pkt_write_start(qlog, hd, /* sent = */ 0);
}

void ngtcp2_qlog_pkt_received_end(ngtcp2_qlog *qlog, const ngtcp2_pkt_hd *hd,
                                  size_t pktlen) {
  qlog_pkt_write_end(qlog, hd, pktlen);
}

void ngtcp2_qlog_pkt_sent_start(ngtcp2_qlog *qlog, const ngtcp2_pkt_hd *hd) {
  qlog_pkt_write_start(qlog, hd, /* sent = */ 1);
}

void ngtcp2_qlog_pkt_sent_end(ngtcp2_qlog *qlog, const ngtcp2_pkt_hd *hd,
                              size_t pktlen) {
  qlog_pkt_write_end(qlog, hd, pktlen);
}

void ngtcp2_qlog_parameters_set_transport_params(
    ngtcp2_qlog *qlog, const ngtcp2_transport_params *params, int local) {
  uint8_t buf[1024];
  uint8_t *p = buf;
  ngtcp2_vec name, value;
  const ngtcp2_preferred_addr *paddr;

  if (!qlog->write) {
    return;
  }

  *p++ = '[';
  p = write_tstamp(p, qlog->last_ts - qlog->ts);
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "transport"));
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "parameters_set"));
  *p++ = ',';
  *p++ = '{';
  p = write_pair(p, ngtcp2_vec_lit(&name, "owner"),
                 local ? ngtcp2_vec_lit(&value, "local")
                       : ngtcp2_vec_lit(&value, "remote"));
  *p++ = ',';
  if (params->original_connection_id_present) {
    p = write_pair_cid(p, ngtcp2_vec_lit(&name, "original_connection_id"),
                       &params->original_connection_id);
    *p++ = ',';
  }
  if (params->stateless_reset_token_present) {
    p = write_pair_hex(p, ngtcp2_vec_lit(&name, "stateless_reset_token"),
                       ngtcp2_vec_init(&value, params->stateless_reset_token,
                                       sizeof(params->stateless_reset_token)));
    *p++ = ',';
  }
  p = write_pair_bool(p, ngtcp2_vec_lit(&name, "disable_active_migration"),
                      params->disable_active_migration);
  *p++ = ',';
  p = write_pair_duration(p, ngtcp2_vec_lit(&name, "max_idle_timeout"),
                          params->max_idle_timeout);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "max_packet_size"),
                        params->max_packet_size);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "ack_delay_exponent"),
                        params->ack_delay_exponent);
  *p++ = ',';
  p = write_pair_duration(p, ngtcp2_vec_lit(&name, "max_ack_delay"),
                          params->max_ack_delay);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "active_connection_id_limit"),
                        params->active_connection_id_limit);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "initial_max_data"),
                        params->initial_max_data);
  *p++ = ',';
  p = write_pair_numstr(
      p, ngtcp2_vec_lit(&name, "initial_max_stream_data_bidi_local"),
      params->initial_max_stream_data_bidi_local);
  *p++ = ',';
  p = write_pair_numstr(
      p, ngtcp2_vec_lit(&name, "initial_max_stream_data_bidi_remote"),
      params->initial_max_stream_data_bidi_remote);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "initial_max_stream_data_uni"),
                        params->initial_max_stream_data_uni);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "initial_max_streams_bidi"),
                        params->initial_max_streams_bidi);
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "initial_max_streams_uni"),
                        params->initial_max_streams_uni);
  if (params->preferred_address_present) {
    *p++ = ',';
    paddr = &params->preferred_address;
    p = write_string(p, ngtcp2_vec_lit(&name, "preferred_address"));
    *p++ = ':';
    *p++ = '{';
    p = write_pair_hex(
        p, ngtcp2_vec_lit(&name, "ip_v4"),
        ngtcp2_vec_init(&value, paddr->ipv4_addr, sizeof(paddr->ipv4_addr)));
    *p++ = ',';
    p = write_pair_number(p, ngtcp2_vec_lit(&name, "port_v4"),
                          paddr->ipv4_port);
    *p++ = ',';
    p = write_pair_hex(
        p, ngtcp2_vec_lit(&name, "ip_v6"),
        ngtcp2_vec_init(&value, paddr->ipv6_addr, sizeof(paddr->ipv6_addr)));
    *p++ = ',';
    p = write_pair_number(p, ngtcp2_vec_lit(&name, "port_v6"),
                          paddr->ipv6_port);
    *p++ = ',';
    p = write_pair_cid(p, ngtcp2_vec_lit(&name, "connection_id"), &paddr->cid);
    *p++ = ',';
    p = write_pair_hex(p, ngtcp2_vec_lit(&name, "stateless_reset_token"),
                       ngtcp2_vec_init(&value, paddr->stateless_reset_token,
                                       sizeof(paddr->stateless_reset_token)));
    *p++ = '}';
  }
  *p++ = '}';
  *p++ = ']';
  *p++ = ',';

  qlog->write(qlog->user_data, buf, (size_t)(p - buf));
}

void ngtcp2_qlog_metrics_updated(ngtcp2_qlog *qlog,
                                 const ngtcp2_rcvry_stat *rcs,
                                 ngtcp2_cc_stat *ccs) {
  uint8_t buf[1024];
  uint8_t *p = buf;
  ngtcp2_vec name, value;

  if (!qlog->write) {
    return;
  }

  *p++ = '[';
  p = write_tstamp(p, qlog->last_ts - qlog->ts);
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "recovery"));
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "metrics_updated"));
  *p++ = ',';
  *p++ = '{';

  if (rcs->min_rtt != UINT64_MAX) {
    p = write_pair_duration(p, ngtcp2_vec_lit(&name, "min_rtt"), rcs->min_rtt);
    *p++ = ',';
  }
  p = write_pair_duration(p, ngtcp2_vec_lit(&name, "smoothed_rtt"),
                          rcs->smoothed_rtt);
  *p++ = ',';
  p = write_pair_duration(p, ngtcp2_vec_lit(&name, "latest_rtt"),
                          rcs->latest_rtt);
  *p++ = ',';
  p = write_pair_duration(p, ngtcp2_vec_lit(&name, "rtt_variance"),
                          rcs->rttvar);
  *p++ = ',';
  /* TODO max_ack_delay? */
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "pto_count"), rcs->pto_count);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "congestion_window"),
                        ccs->cwnd);
  *p++ = ',';
  p = write_pair_number(p, ngtcp2_vec_lit(&name, "bytes_in_flight"),
                        ccs->bytes_in_flight);
  if (ccs->ssthresh != UINT64_MAX) {
    *p++ = ',';
    p = write_pair_number(p, ngtcp2_vec_lit(&name, "ssthresh"), ccs->ssthresh);
  }

  *p++ = '}';
  *p++ = ']';
  *p++ = ',';

  qlog->write(qlog->user_data, buf, (size_t)(p - buf));
}

void ngtcp2_qlog_pkt_lost(ngtcp2_qlog *qlog, ngtcp2_rtb_entry *ent) {
  uint8_t buf[256];
  uint8_t *p = buf;
  ngtcp2_vec name, value;
  ngtcp2_pkt_hd hd;

  if (!qlog->write) {
    return;
  }

  *p++ = '[';
  p = write_tstamp(p, qlog->last_ts - qlog->ts);
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "recovery"));
  *p++ = ',';
  p = write_string(p, ngtcp2_vec_lit(&value, "packet_lost"));
  *p++ = ',';
  *p++ = '{';

  hd.type = ent->hd.type;
  hd.flags = ent->hd.flags;

  p = write_pair(p, ngtcp2_vec_lit(&name, "packet_type"),
                 qlog_pkt_type(&value, &hd));
  *p++ = ',';
  p = write_pair_numstr(p, ngtcp2_vec_lit(&name, "packet_number"),
                        (uint64_t)ent->hd.pkt_num);
  *p++ = '}';
  *p++ = ']';
  *p++ = ',';

  qlog->write(qlog->user_data, buf, (size_t)(p - buf));
}
