/*
 * ngtcp2
 *
 * Copyright (c) 2018 ngtcp2 contributors
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
#include "ngtcp2_log.h"

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif /* defined(HAVE_UNISTD_H) */
#include <assert.h>
#include <string.h>

#include "ngtcp2_str.h"
#include "ngtcp2_vec.h"
#include "ngtcp2_macro.h"
#include "ngtcp2_conv.h"
#include "ngtcp2_unreachable.h"
#include "ngtcp2_net.h"

void ngtcp2_log_init(ngtcp2_log *log, const ngtcp2_cid *scid,
                     ngtcp2_printf log_printf, ngtcp2_tstamp ts,
                     void *user_data) {
  if (scid) {
    ngtcp2_encode_hex_cstr(log->scid, scid->data, scid->datalen);
  } else {
    log->scid[0] = '\0';
  }
  log->log_printf = log_printf;
  log->events = 0xff;
  log->ts = log->last_ts = ts;
  log->user_data = user_data;
}

/*
 * # Log header
 *
 * <LEVEL><TIMESTAMP> <SCID> <EVENT>
 *
 * <LEVEL>:
 *   Log level.  I=Info, W=Warning, E=Error
 *
 * <TIMESTAMP>:
 *   Timestamp relative to ngtcp2_log.ts field in milliseconds
 *   resolution.
 *
 * <SCID>:
 *   Source Connection ID in hex string.
 *
 * <EVENT>:
 *   Event.  See ngtcp2_log_event.
 *
 * # Frame event
 *
 * <DIR> <PKN> <PKTNAME> <FRAMENAME>(<FRAMETYPE>)
 *
 * <DIR>:
 *   Flow direction.  tx=transmission, rx=reception
 *
 * <PKN>:
 *   Packet number.
 *
 * <PKTNAME>:
 *   Packet name.  (e.g., Initial, Handshake, 1RTT)
 *
 * <FRAMENAME>:
 *   Frame name.  (e.g., STREAM, ACK, PING)
 *
 * <FRAMETYPE>:
 *   Frame type in hex string.
 */

#define NGTCP2_LOG_PKT "%s %" PRId64 " %s"
#define NGTCP2_LOG_TP "remote transport_parameters"

#define NGTCP2_LOG_PKT_HD_FIELDS(DIR) (DIR), hd->pkt_num, strpkttype(hd)

static const char *strerrorcode(uint64_t error_code) {
  switch (error_code) {
  case NGTCP2_NO_ERROR:
    return "NO_ERROR";
  case NGTCP2_INTERNAL_ERROR:
    return "INTERNAL_ERROR";
  case NGTCP2_CONNECTION_REFUSED:
    return "CONNECTION_REFUSED";
  case NGTCP2_FLOW_CONTROL_ERROR:
    return "FLOW_CONTROL_ERROR";
  case NGTCP2_STREAM_LIMIT_ERROR:
    return "STREAM_LIMIT_ERROR";
  case NGTCP2_STREAM_STATE_ERROR:
    return "STREAM_STATE_ERROR";
  case NGTCP2_FINAL_SIZE_ERROR:
    return "FINAL_SIZE_ERROR";
  case NGTCP2_FRAME_ENCODING_ERROR:
    return "FRAME_ENCODING_ERROR";
  case NGTCP2_TRANSPORT_PARAMETER_ERROR:
    return "TRANSPORT_PARAMETER_ERROR";
  case NGTCP2_CONNECTION_ID_LIMIT_ERROR:
    return "CONNECTION_ID_LIMIT_ERROR";
  case NGTCP2_PROTOCOL_VIOLATION:
    return "PROTOCOL_VIOLATION";
  case NGTCP2_INVALID_TOKEN:
    return "INVALID_TOKEN";
  case NGTCP2_APPLICATION_ERROR:
    return "APPLICATION_ERROR";
  case NGTCP2_CRYPTO_BUFFER_EXCEEDED:
    return "CRYPTO_BUFFER_EXCEEDED";
  case NGTCP2_KEY_UPDATE_ERROR:
    return "KEY_UPDATE_ERROR";
  case NGTCP2_AEAD_LIMIT_REACHED:
    return "AEAD_LIMIT_REACHED";
  case NGTCP2_NO_VIABLE_PATH:
    return "NO_VIABLE_PATH";
  case NGTCP2_VERSION_NEGOTIATION_ERROR:
    return "VERSION_NEGOTIATION_ERROR";
  default:
    if (0x100u <= error_code && error_code <= 0x1ffu) {
      return "CRYPTO_ERROR";
    }
    return "(unknown)";
  }
}

static const char *strapperrorcode(uint64_t app_error_code) {
  (void)app_error_code;
  return "(unknown)";
}

static const char *strpkttype_long(uint8_t type) {
  switch (type) {
  case NGTCP2_PKT_INITIAL:
    return "Initial";
  case NGTCP2_PKT_RETRY:
    return "Retry";
  case NGTCP2_PKT_HANDSHAKE:
    return "Handshake";
  case NGTCP2_PKT_0RTT:
    return "0RTT";
  default:
    return "(unknown)";
  }
}

static const char *strpkttype(const ngtcp2_pkt_hd *hd) {
  if (hd->flags & NGTCP2_PKT_FLAG_LONG_FORM) {
    return strpkttype_long(hd->type);
  }

  switch (hd->type) {
  case NGTCP2_PKT_VERSION_NEGOTIATION:
    return "VN";
  case NGTCP2_PKT_STATELESS_RESET:
    return "SR";
  case NGTCP2_PKT_1RTT:
    return "1RTT";
  default:
    return "(unknown)";
  }
}

static const char *strpkttype_type_flags(uint8_t type, uint8_t flags) {
  return strpkttype(&(ngtcp2_pkt_hd){
    .type = type,
    .flags = flags,
  });
}

static void log_fr_stream(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                          const ngtcp2_stream *fr, const char *dir) {
  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " STREAM(0x%02" PRIx64 ") id=0x%" PRIx64
                   " fin=%d offset=%" PRIu64 " len=%" PRIu64 " uni=%d",
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type | fr->flags, fr->stream_id, fr->fin,
    fr->offset, ngtcp2_vec_len(fr->data, fr->datacnt),
    (fr->stream_id & 0x2) != 0);
}

static void log_fr_ack(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                       const ngtcp2_ack *fr, const char *dir) {
  int64_t largest_ack, min_ack;
  size_t i;

  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " ACK(0x%02" PRIx64 ") largest_ack=%" PRId64
                   " ack_delay=%" PRIu64 "(%" PRIu64 ") ack_range_count=%zu",
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->largest_ack,
    fr->ack_delay_unscaled / NGTCP2_MILLISECONDS, fr->ack_delay, fr->rangecnt);

  largest_ack = fr->largest_ack;
  min_ack = fr->largest_ack - (int64_t)fr->first_ack_range;

  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " ACK(0x%02" PRIx64 ") range=[%" PRId64
                                      "..%" PRId64 "] len=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, largest_ack,
                       min_ack, fr->first_ack_range);

  for (i = 0; i < fr->rangecnt; ++i) {
    const ngtcp2_ack_range *range = &fr->ranges[i];
    largest_ack = min_ack - (int64_t)range->gap - 2;
    min_ack = largest_ack - (int64_t)range->len;
    ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                         NGTCP2_LOG_PKT " ACK(0x%02" PRIx64 ") range=[%" PRId64
                                        "..%" PRId64 "] gap=%" PRIu64
                                        " len=%" PRIu64,
                         NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, largest_ack,
                         min_ack, range->gap, range->len);
  }

  if (fr->type == NGTCP2_FRAME_ACK_ECN) {
    ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                         NGTCP2_LOG_PKT " ACK(0x%02" PRIx64 ") ect0=%" PRIu64
                                        " ect1=%" PRIu64 " ce=%" PRIu64,
                         NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->ecn.ect0,
                         fr->ecn.ect1, fr->ecn.ce);
  }
}

static void log_fr_padding(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                           const ngtcp2_padding *fr, const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " PADDING(0x%02" PRIx64 ") len=%zu",
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->len);
}

static void log_fr_reset_stream(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                const ngtcp2_reset_stream *fr,
                                const char *dir) {
  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " RESET_STREAM(0x%02" PRIx64 ") id=0x%" PRIx64
                   " app_error_code=%s(0x%" PRIx64 ") final_size=%" PRIu64,
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->stream_id,
    strapperrorcode(fr->app_error_code), fr->app_error_code, fr->final_size);
}

static void log_fr_connection_close(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                    const ngtcp2_connection_close *fr,
                                    const char *dir) {
  char reason[256];
  size_t reasonlen = ngtcp2_min_size(sizeof(reason) - 1, fr->reasonlen);

  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " CONNECTION_CLOSE(0x%02" PRIx64 ") error_code=%s(0x%" PRIx64
                   ") "
                   "frame_type=%" PRIx64 " reason_len=%zu reason=[%s]",
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type,
    fr->type == NGTCP2_FRAME_CONNECTION_CLOSE ? strerrorcode(fr->error_code)
                                              : strapperrorcode(fr->error_code),
    fr->error_code, fr->frame_type, fr->reasonlen,
    ngtcp2_encode_printable_ascii_cstr(reason, fr->reason, reasonlen));
}

static void log_fr_max_data(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                            const ngtcp2_max_data *fr, const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " MAX_DATA(0x%02" PRIx64
                                      ") max_data=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->max_data);
}

static void log_fr_max_stream_data(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                   const ngtcp2_max_stream_data *fr,
                                   const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " MAX_STREAM_DATA(0x%02" PRIx64
                                      ") id=0x%" PRIx64
                                      " max_stream_data=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->stream_id,
                       fr->max_stream_data);
}

static void log_fr_max_streams(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                               const ngtcp2_max_streams *fr, const char *dir) {
  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " MAX_STREAMS(0x%02" PRIx64 ") max_streams=%" PRIu64,
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->max_streams);
}

static void log_fr_ping(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                        const ngtcp2_ping *fr, const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " PING(0x%02" PRIx64 ")",
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type);
}

static void log_fr_data_blocked(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                const ngtcp2_data_blocked *fr,
                                const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " DATA_BLOCKED(0x%02" PRIx64
                                      ") offset=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->offset);
}

static void log_fr_stream_data_blocked(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                       const ngtcp2_stream_data_blocked *fr,
                                       const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " STREAM_DATA_BLOCKED(0x%02" PRIx64
                                      ") id=0x%" PRIx64 " offset=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->stream_id,
                       fr->offset);
}

static void log_fr_streams_blocked(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                   const ngtcp2_streams_blocked *fr,
                                   const char *dir) {
  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " STREAMS_BLOCKED(0x%02" PRIx64 ") max_streams=%" PRIu64,
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->max_streams);
}

static void log_fr_new_connection_id(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                     const ngtcp2_new_connection_id *fr,
                                     const char *dir) {
  char buf[sizeof(fr->stateless_reset_token) * 2 + 1];
  char cid[sizeof(fr->cid.data) * 2 + 1];

  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " NEW_CONNECTION_ID(0x%02" PRIx64 ") seq=%" PRIu64
                   " cid=0x%s retire_prior_to=%" PRIu64
                   " stateless_reset_token=0x%s",
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->seq,
    ngtcp2_encode_hex_cstr(cid, fr->cid.data, fr->cid.datalen),
    fr->retire_prior_to,
    ngtcp2_encode_hex_cstr(buf, fr->stateless_reset_token,
                           sizeof(fr->stateless_reset_token)));
}

static void log_fr_stop_sending(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                const ngtcp2_stop_sending *fr,
                                const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " STOP_SENDING(0x%02" PRIx64
                                      ") id=0x%" PRIx64
                                      " app_error_code=%s(0x%" PRIx64 ")",
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->stream_id,
                       strapperrorcode(fr->app_error_code), fr->app_error_code);
}

static void log_fr_path_challenge(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                  const ngtcp2_path_challenge *fr,
                                  const char *dir) {
  char buf[sizeof(fr->data) * 2 + 1];

  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " PATH_CHALLENGE(0x%02" PRIx64
                                      ") data=0x%s",
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type,
                       ngtcp2_encode_hex_cstr(buf, fr->data, sizeof(fr->data)));
}

static void log_fr_path_response(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                 const ngtcp2_path_response *fr,
                                 const char *dir) {
  char buf[sizeof(fr->data) * 2 + 1];

  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " PATH_RESPONSE(0x%02" PRIx64
                                      ") data=0x%s",
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type,
                       ngtcp2_encode_hex_cstr(buf, fr->data, sizeof(fr->data)));
}

static void log_fr_crypto(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                          const ngtcp2_stream *fr, const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " CRYPTO(0x%02" PRIx64 ") offset=%" PRIu64
                                      " len=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->offset,
                       ngtcp2_vec_len(fr->data, fr->datacnt));
}

static void log_fr_new_token(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                             const ngtcp2_new_token *fr, const char *dir) {
  /* Show at most first 64 bytes of token.  If token is longer than 64
     bytes, log first 64 bytes and then append "*" */
  char buf[128 + 1 + 1];
  char *p;

  if (fr->tokenlen > 64) {
    p = ngtcp2_encode_hex_cstr(buf, fr->token, 64);
    p[128] = '*';
    p[129] = '\0';
  } else {
    p = ngtcp2_encode_hex_cstr(buf, fr->token, fr->tokenlen);
  }

  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_FRM,
    NGTCP2_LOG_PKT " NEW_TOKEN(0x%02" PRIx64 ") token=0x%s len=%zu",
    NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, p, fr->tokenlen);
}

static void log_fr_retire_connection_id(ngtcp2_log *log,
                                        const ngtcp2_pkt_hd *hd,
                                        const ngtcp2_retire_connection_id *fr,
                                        const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " RETIRE_CONNECTION_ID(0x%02" PRIx64
                                      ") seq=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type, fr->seq);
}

static void log_fr_handshake_done(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                                  const ngtcp2_handshake_done *fr,
                                  const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " HANDSHAKE_DONE(0x%02" PRIx64 ")",
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type);
}

static void log_fr_datagram(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                            const ngtcp2_datagram *fr, const char *dir) {
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_FRM,
                       NGTCP2_LOG_PKT " DATAGRAM(0x%02" PRIx64 ") len=%" PRIu64,
                       NGTCP2_LOG_PKT_HD_FIELDS(dir), fr->type,
                       ngtcp2_vec_len(fr->data, fr->datacnt));
}

static void log_fr(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                   const ngtcp2_frame *fr, const char *dir) {
  switch (fr->hd.type) {
  case NGTCP2_FRAME_STREAM:
    log_fr_stream(log, hd, &fr->stream, dir);
    break;
  case NGTCP2_FRAME_ACK:
  case NGTCP2_FRAME_ACK_ECN:
    log_fr_ack(log, hd, &fr->ack, dir);
    break;
  case NGTCP2_FRAME_PADDING:
    log_fr_padding(log, hd, &fr->padding, dir);
    break;
  case NGTCP2_FRAME_RESET_STREAM:
    log_fr_reset_stream(log, hd, &fr->reset_stream, dir);
    break;
  case NGTCP2_FRAME_CONNECTION_CLOSE:
  case NGTCP2_FRAME_CONNECTION_CLOSE_APP:
    log_fr_connection_close(log, hd, &fr->connection_close, dir);
    break;
  case NGTCP2_FRAME_MAX_DATA:
    log_fr_max_data(log, hd, &fr->max_data, dir);
    break;
  case NGTCP2_FRAME_MAX_STREAM_DATA:
    log_fr_max_stream_data(log, hd, &fr->max_stream_data, dir);
    break;
  case NGTCP2_FRAME_MAX_STREAMS_BIDI:
  case NGTCP2_FRAME_MAX_STREAMS_UNI:
    log_fr_max_streams(log, hd, &fr->max_streams, dir);
    break;
  case NGTCP2_FRAME_PING:
    log_fr_ping(log, hd, &fr->ping, dir);
    break;
  case NGTCP2_FRAME_DATA_BLOCKED:
    log_fr_data_blocked(log, hd, &fr->data_blocked, dir);
    break;
  case NGTCP2_FRAME_STREAM_DATA_BLOCKED:
    log_fr_stream_data_blocked(log, hd, &fr->stream_data_blocked, dir);
    break;
  case NGTCP2_FRAME_STREAMS_BLOCKED_BIDI:
  case NGTCP2_FRAME_STREAMS_BLOCKED_UNI:
    log_fr_streams_blocked(log, hd, &fr->streams_blocked, dir);
    break;
  case NGTCP2_FRAME_NEW_CONNECTION_ID:
    log_fr_new_connection_id(log, hd, &fr->new_connection_id, dir);
    break;
  case NGTCP2_FRAME_STOP_SENDING:
    log_fr_stop_sending(log, hd, &fr->stop_sending, dir);
    break;
  case NGTCP2_FRAME_PATH_CHALLENGE:
    log_fr_path_challenge(log, hd, &fr->path_challenge, dir);
    break;
  case NGTCP2_FRAME_PATH_RESPONSE:
    log_fr_path_response(log, hd, &fr->path_response, dir);
    break;
  case NGTCP2_FRAME_CRYPTO:
    log_fr_crypto(log, hd, &fr->stream, dir);
    break;
  case NGTCP2_FRAME_NEW_TOKEN:
    log_fr_new_token(log, hd, &fr->new_token, dir);
    break;
  case NGTCP2_FRAME_RETIRE_CONNECTION_ID:
    log_fr_retire_connection_id(log, hd, &fr->retire_connection_id, dir);
    break;
  case NGTCP2_FRAME_HANDSHAKE_DONE:
    log_fr_handshake_done(log, hd, &fr->handshake_done, dir);
    break;
  case NGTCP2_FRAME_DATAGRAM:
  case NGTCP2_FRAME_DATAGRAM_LEN:
    log_fr_datagram(log, hd, &fr->datagram, dir);
    break;
  default:
    ngtcp2_unreachable();
  }
}

void ngtcp2_log_rx_fr(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                      const ngtcp2_frame *fr) {
  if (!log->log_printf || !(log->events & NGTCP2_LOG_EVENT_FRM)) {
    return;
  }

  log_fr(log, hd, fr, "rx");
}

void ngtcp2_log_tx_fr(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                      const ngtcp2_frame *fr) {
  if (!log->log_printf || !(log->events & NGTCP2_LOG_EVENT_FRM)) {
    return;
  }

  log_fr(log, hd, fr, "tx");
}

void ngtcp2_log_rx_vn(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                      const uint32_t *sv, size_t nsv) {
  size_t i;

  if (!log->log_printf || !(log->events & NGTCP2_LOG_EVENT_PKT)) {
    return;
  }

  for (i = 0; i < nsv; ++i) {
    ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_PKT, NGTCP2_LOG_PKT " v=0x%08x",
                         NGTCP2_LOG_PKT_HD_FIELDS("rx"), sv[i]);
  }
}

void ngtcp2_log_rx_sr(ngtcp2_log *log, const ngtcp2_pkt_stateless_reset *sr) {
  char buf[sizeof(sr->stateless_reset_token) * 2 + 1];
  ngtcp2_pkt_hd shd;
  ngtcp2_pkt_hd *hd = &shd;

  if (!log->log_printf || !(log->events & NGTCP2_LOG_EVENT_PKT)) {
    return;
  }

  shd = (ngtcp2_pkt_hd){
    .type = NGTCP2_PKT_STATELESS_RESET,
  };

  ngtcp2_log_infof_raw(
    log, NGTCP2_LOG_EVENT_PKT, NGTCP2_LOG_PKT " token=0x%s randlen=%zu",
    NGTCP2_LOG_PKT_HD_FIELDS("rx"),
    ngtcp2_encode_hex_cstr(buf, sr->stateless_reset_token,
                           sizeof(sr->stateless_reset_token)),
    sr->randlen);
}

void ngtcp2_log_remote_tp(ngtcp2_log *log,
                          const ngtcp2_transport_params *params) {
  char token[NGTCP2_STATELESS_RESET_TOKENLEN * 2 + 1];
  char addr[16 * 2 + 7 + 1];
  char cid[NGTCP2_MAX_CIDLEN * 2 + 1];
  size_t i;
  const ngtcp2_sockaddr_in *sa_in;
  const ngtcp2_sockaddr_in6 *sa_in6;
  const uint8_t *p;
  uint32_t version;

  if (!log->log_printf || !(log->events & NGTCP2_LOG_EVENT_CRY)) {
    return;
  }

  if (params->stateless_reset_token_present) {
    ngtcp2_log_infof_raw(
      log, NGTCP2_LOG_EVENT_CRY, NGTCP2_LOG_TP " stateless_reset_token=0x%s",
      ngtcp2_encode_hex_cstr(token, params->stateless_reset_token,
                             sizeof(params->stateless_reset_token)));
  }

  if (params->preferred_addr_present) {
    if (params->preferred_addr.ipv4_present) {
      sa_in = &params->preferred_addr.ipv4;

      ngtcp2_log_infof_raw(
        log, NGTCP2_LOG_EVENT_CRY,
        NGTCP2_LOG_TP " preferred_address.ipv4_addr=%s",
        ngtcp2_encode_ipv4_cstr(addr, (const uint8_t *)&sa_in->sin_addr));
      ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                           NGTCP2_LOG_TP " preferred_address.ipv4_port=%u",
                           ngtcp2_ntohs(sa_in->sin_port));
    }

    if (params->preferred_addr.ipv6_present) {
      sa_in6 = &params->preferred_addr.ipv6;

      ngtcp2_log_infof_raw(
        log, NGTCP2_LOG_EVENT_CRY,
        NGTCP2_LOG_TP " preferred_address.ipv6_addr=%s",
        ngtcp2_encode_ipv6_cstr(addr, (const uint8_t *)&sa_in6->sin6_addr));
      ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                           NGTCP2_LOG_TP " preferred_address.ipv6_port=%u",
                           ngtcp2_ntohs(sa_in6->sin6_port));
    }

    ngtcp2_log_infof_raw(
      log, NGTCP2_LOG_EVENT_CRY, NGTCP2_LOG_TP " preferred_address.cid=0x%s",
      ngtcp2_encode_hex_cstr(cid, params->preferred_addr.cid.data,
                             params->preferred_addr.cid.datalen));
    ngtcp2_log_infof_raw(
      log, NGTCP2_LOG_EVENT_CRY,
      NGTCP2_LOG_TP " preferred_address.stateless_reset_token=0x%s",
      ngtcp2_encode_hex_cstr(
        token, params->preferred_addr.stateless_reset_token,
        sizeof(params->preferred_addr.stateless_reset_token)));
  }

  if (params->original_dcid_present) {
    ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                         NGTCP2_LOG_TP
                         " original_destination_connection_id=0x%s",
                         ngtcp2_encode_hex_cstr(cid, params->original_dcid.data,
                                                params->original_dcid.datalen));
  }

  if (params->retry_scid_present) {
    ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                         NGTCP2_LOG_TP " retry_source_connection_id=0x%s",
                         ngtcp2_encode_hex_cstr(cid, params->retry_scid.data,
                                                params->retry_scid.datalen));
  }

  if (params->initial_scid_present) {
    ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                         NGTCP2_LOG_TP " initial_source_connection_id=0x%s",
                         ngtcp2_encode_hex_cstr(cid, params->initial_scid.data,
                                                params->initial_scid.datalen));
  }

  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP
                       " initial_max_stream_data_bidi_local=%" PRIu64,
                       params->initial_max_stream_data_bidi_local);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP
                       " initial_max_stream_data_bidi_remote=%" PRIu64,
                       params->initial_max_stream_data_bidi_remote);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " initial_max_stream_data_uni=%" PRIu64,
                       params->initial_max_stream_data_uni);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " initial_max_data=%" PRIu64,
                       params->initial_max_data);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " initial_max_streams_bidi=%" PRIu64,
                       params->initial_max_streams_bidi);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " initial_max_streams_uni=%" PRIu64,
                       params->initial_max_streams_uni);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " max_idle_timeout=%" PRIu64,
                       params->max_idle_timeout / NGTCP2_MILLISECONDS);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " max_udp_payload_size=%" PRIu64,
                       params->max_udp_payload_size);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " ack_delay_exponent=%" PRIu64,
                       params->ack_delay_exponent);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " max_ack_delay=%" PRIu64,
                       params->max_ack_delay / NGTCP2_MILLISECONDS);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " active_connection_id_limit=%" PRIu64,
                       params->active_connection_id_limit);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " disable_active_migration=%d",
                       params->disable_active_migration);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " max_datagram_frame_size=%" PRIu64,
                       params->max_datagram_frame_size);
  ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                       NGTCP2_LOG_TP " grease_quic_bit=%d",
                       params->grease_quic_bit);

  if (params->version_info_present) {
    ngtcp2_log_infof_raw(log, NGTCP2_LOG_EVENT_CRY,
                         NGTCP2_LOG_TP
                         " version_information.chosen_version=0x%08x",
                         params->version_info.chosen_version);

    assert(!(params->version_info.available_versionslen & 0x3));

    for (i = 0, p = params->version_info.available_versions;
         i < params->version_info.available_versionslen;
         i += sizeof(uint32_t)) {
      p = ngtcp2_get_uint32be(&version, p);

      ngtcp2_log_infof_raw(
        log, NGTCP2_LOG_EVENT_CRY,
        NGTCP2_LOG_TP " version_information.available_versions[%zu]=0x%08x",
        i >> 2, version);
    }
  }
}

void ngtcp2_log_pkt_lost(ngtcp2_log *log, int64_t pkt_num, uint8_t type,
                         uint8_t flags, ngtcp2_tstamp sent_ts) {
  if (!log->log_printf || !(log->events & NGTCP2_LOG_EVENT_LDC)) {
    return;
  }

  ngtcp2_log_infof(log, NGTCP2_LOG_EVENT_LDC,
                   "pkn=%" PRId64 " lost type=%s sent_ts=%" PRIu64, pkt_num,
                   strpkttype_type_flags(type, flags), sent_ts);
}

static void log_pkt_hd(ngtcp2_log *log, const ngtcp2_pkt_hd *hd,
                       const char *dir) {
  char dcid[sizeof(hd->dcid.data) * 2 + 1];
  char scid[sizeof(hd->scid.data) * 2 + 1];

  if (!log->log_printf || !(log->events & NGTCP2_LOG_EVENT_PKT)) {
    return;
  }

  if (hd->type == NGTCP2_PKT_1RTT) {
    ngtcp2_log_infof(
      log, NGTCP2_LOG_EVENT_PKT, "%s pkn=%" PRId64 " dcid=0x%s type=%s k=%d",
      dir, hd->pkt_num,
      ngtcp2_encode_hex_cstr(dcid, hd->dcid.data, hd->dcid.datalen),
      strpkttype(hd), (hd->flags & NGTCP2_PKT_FLAG_KEY_PHASE) != 0);
  } else {
    ngtcp2_log_infof(
      log, NGTCP2_LOG_EVENT_PKT,
      "%s pkn=%" PRId64 " dcid=0x%s scid=0x%s version=0x%08x type=%s len=%zu",
      dir, hd->pkt_num,
      ngtcp2_encode_hex_cstr(dcid, hd->dcid.data, hd->dcid.datalen),
      ngtcp2_encode_hex_cstr(scid, hd->scid.data, hd->scid.datalen),
      hd->version, strpkttype(hd), hd->len);
  }
}

void ngtcp2_log_rx_pkt_hd(ngtcp2_log *log, const ngtcp2_pkt_hd *hd) {
  log_pkt_hd(log, hd, "rx");
}

void ngtcp2_log_tx_pkt_hd(ngtcp2_log *log, const ngtcp2_pkt_hd *hd) {
  log_pkt_hd(log, hd, "tx");
}

void ngtcp2_log_tx_cancel(ngtcp2_log *log, const ngtcp2_pkt_hd *hd) {
  ngtcp2_log_infof(log, NGTCP2_LOG_EVENT_PKT,
                   "cancel tx pkn=%" PRId64 " type=%s", hd->pkt_num,
                   strpkttype(hd));
}

uint64_t ngtcp2_log_timestamp(const ngtcp2_log *log) {
  return (log->last_ts - log->ts) / NGTCP2_MILLISECONDS;
}
