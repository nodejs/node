/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2013 nghttp2 contributors
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
#ifndef NGHTTP3_FRAME_H
#define NGHTTP3_FRAME_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <nghttp3/nghttp3.h>

#include "nghttp3_buf.h"

#define NGHTTP3_FRAME_DATA 0x00U
#define NGHTTP3_FRAME_HEADERS 0x01U
#define NGHTTP3_FRAME_CANCEL_PUSH 0x03U
#define NGHTTP3_FRAME_SETTINGS 0x04U
#define NGHTTP3_FRAME_PUSH_PROMISE 0x05U
#define NGHTTP3_FRAME_GOAWAY 0x07U
#define NGHTTP3_FRAME_MAX_PUSH_ID 0x0DU
/* PRIORITY_UPDATE: https://datatracker.ietf.org/doc/html/rfc9218 */
#define NGHTTP3_FRAME_PRIORITY_UPDATE 0x0F0700U
#define NGHTTP3_FRAME_PRIORITY_UPDATE_PUSH_ID 0x0F0701U
/* ORIGIN: https://datatracker.ietf.org/doc/html/rfc9412 */
#define NGHTTP3_FRAME_ORIGIN 0x0CU
/* WebTransport extended frame type */
#define NGHTTP3_FRAME_EX_WT 0x4000000000000001ULL
/* WT_STREAM:
   https://datatracker.ietf.org/doc/html/draft-ietf-webtrans-http3-14 */
#define NGHTTP3_EXFR_WT_STREAM_BIDI 0x41U
#define NGHTTP3_EXFR_WT_STREAM_UNI 0x54U
#define NGHTTP3_EXFR_WT_STREAM_DATA 0x00U

/* HTTP Capsule extended frame type */
#define NGHTTP3_FRAME_EX_CPSL 0x4000000000000002ULL
#define NGHTTP3_EXFR_CPSL_WT_CLOSE_SESSION 0x2843U
#define NGHTTP3_EXFR_CPSL_WT_DRAIN_SESSION 0x78AEU
#define NGHTTP3_EXFR_CPSL_WT_MAX_STREAMS_BIDI 0x190B4D3FU
#define NGHTTP3_EXFR_CPSL_WT_MAX_STREAMS_UNI 0x190B4D40U
#define NGHTTP3_EXFR_CPSL_WT_STREAMS_BLOCKED_BIDI 0x190B4D43U
#define NGHTTP3_EXFR_CPSL_WT_STREAMS_BLOCKED_UNI 0x190B4D44U
#define NGHTTP3_EXFR_CPSL_WT_MAX_DATA 0x190B4D3DU
#define NGHTTP3_EXFR_CPSL_WT_DATA_BLOCKED 0x190B4D41U

/* Frame types that are reserved for HTTP/2, and must not be used in
   HTTP/3. */
#define NGHTTP3_H2_FRAME_PRIORITY 0x02U
#define NGHTTP3_H2_FRAME_PING 0x06U
#define NGHTTP3_H2_FRAME_WINDOW_UPDATE 0x08U
#define NGHTTP3_H2_FRAME_CONTINUATION 0x9U

typedef struct nghttp3_frame_hd {
  uint64_t type;
} nghttp3_frame_hd;

typedef struct nghttp3_frame_data {
  uint64_t type;
  /* dr is set when sending DATA frame.  It is not used on
     reception. */
  nghttp3_data_reader dr;
} nghttp3_frame_data;

typedef struct nghttp3_frame_headers {
  uint64_t type;
  nghttp3_nv *nva;
  size_t nvlen;
} nghttp3_frame_headers;

#define NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE 0x06U
#define NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY 0x01U
#define NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS 0x07U
#define NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL 0x08U
#define NGHTTP3_SETTINGS_ID_H3_DATAGRAM 0x33U
/* https://datatracker.ietf.org/doc/html/draft-ietf-webtrans-http3-15 */
#define NGHTTP3_SETTINGS_ID_WT_ENABLED 0x2C7CF000U
/* https://datatracker.ietf.org/doc/html/draft-ietf-webtrans-http3-14 */
#define NGHTTP3_SETTINGS_ID_WT_MAX_SESSIONS 0x14E9CD29U
/* https://datatracker.ietf.org/doc/html/draft-ietf-webtrans-http3-07 */
#define NGHTTP3_SETTINGS_ID_WT_MAX_SESSIONS_DRAFT7 0xC671706AU
/* https://datatracker.ietf.org/doc/html/draft-ietf-webtrans-http3-02 */
#define NGHTTP3_SETTINGS_ID_ENABLE_WEBTRANSPORT_DRAFT2 0x2B603742U

#define NGHTTP3_H2_SETTINGS_ID_ENABLE_PUSH 0x2U
#define NGHTTP3_H2_SETTINGS_ID_MAX_CONCURRENT_STREAMS 0x3U
#define NGHTTP3_H2_SETTINGS_ID_INITIAL_WINDOW_SIZE 0x4U
#define NGHTTP3_H2_SETTINGS_ID_MAX_FRAME_SIZE 0x5U

typedef struct nghttp3_settings_entry {
  uint64_t id;
  uint64_t value;
} nghttp3_settings_entry;

typedef struct nghttp3_frame_settings {
  uint64_t type;
  size_t niv;
  nghttp3_settings_entry *iv;
  /* local_settings is set when sending SETTINGS frame.  It is not
     used on reception. */
  const nghttp3_settings *local_settings;
} nghttp3_frame_settings;

typedef struct nghttp3_frame_goaway {
  uint64_t type;
  int64_t id;
} nghttp3_frame_goaway;

typedef struct nghttp3_frame_priority_update {
  uint64_t type;
  /* pri_elem_id is stream ID if type ==
     NGHTTP3_FRAME_PRIORITY_UPDATE.  It is push ID if type ==
     NGHTTP3_FRAME_PRIORITY_UPDATE_PUSH_ID.  It is undefined
     otherwise. */
  int64_t pri_elem_id;
  /* When sending this frame, data should point to the buffer
     containing a serialized priority field value and its length is
     set to datalen.  On reception, pri contains the decoded priority
     header value. */
  union {
    /* Unnamed struct first, so that in unit test, it is
       zero-initialized in the initialization without initializer,
       which is convenient to pass nghttp3_frame to
       nghttp3_write_frame test helper function. */
    struct {
      uint8_t *data;
      size_t datalen;
    };
    nghttp3_pri pri;
  };
} nghttp3_frame_priority_update;

typedef struct nghttp3_frame_origin {
  uint64_t type;
  /* These fields are only used by server to send ORIGIN frame.
     Client never use them. */
  nghttp3_vec origin_list;
} nghttp3_frame_origin;

typedef struct nghttp3_exfr_hd {
  uint64_t type;
} nghttp3_exfr_hd;

typedef struct nghttp3_exfr_wt_stream {
  uint64_t type;
  int64_t session_id;
  nghttp3_data_reader dr;
} nghttp3_exfr_wt_stream;

typedef union nghttp3_exfr_wt {
  nghttp3_exfr_hd hd;
  nghttp3_exfr_wt_stream wt_stream;
} nghttp3_exfr_wt;

typedef struct nghttp3_frame_ex_wt {
  uint64_t type;
  nghttp3_exfr_wt fr;
} nghttp3_frame_ex_wt;

typedef struct nghttp3_exfr_cpsl_wt_close_session {
  uint64_t type;
  nghttp3_vec error_msg;
  uint32_t error_code;
} nghttp3_exfr_cpsl_wt_close_session;

typedef union nghttp3_exfr_cpsl {
  nghttp3_exfr_hd hd;
  nghttp3_exfr_cpsl_wt_close_session wt_close_session;
} nghttp3_exfr_cpsl;

typedef struct nghttp3_frame_ex_cpsl {
  uint64_t type;
  nghttp3_exfr_cpsl fr;
} nghttp3_frame_ex_cpsl;

typedef union nghttp3_frame {
  nghttp3_frame_hd hd;
  nghttp3_frame_data data;
  nghttp3_frame_headers headers;
  nghttp3_frame_settings settings;
  nghttp3_frame_goaway goaway;
  nghttp3_frame_priority_update priority_update;
  nghttp3_frame_origin origin;
  nghttp3_frame_ex_wt wt;
  nghttp3_frame_ex_cpsl cpsl;
} nghttp3_frame;

/*
 * nghttp3_frame_write_hd writes frame header consisting of |type| and
 * |payloadlen| to |dest|.  This function assumes that |dest| has
 * enough space to write the frame header.
 *
 * This function returns |dest| plus the number of bytes written.
 */
uint8_t *nghttp3_frame_write_hd(uint8_t *dest, uint64_t type,
                                uint64_t payloadlen);

/*
 * nghttp3_frame_write_hd_len returns the number of bytes required to
 * write a frame header consisting of |type| and |payloadlen|.
 */
size_t nghttp3_frame_write_hd_len(uint64_t type, uint64_t payloadlen);

/*
 * nghttp3_frame_write_settings writes SETTINGS frame |fr| to |dest|.
 * This function assumes that |dest| has enough space to write |fr|.
 * |payloadlen| is the length of the frame payload.
 *
 * This function returns |dest| plus the number of bytes written.
 */
uint8_t *nghttp3_frame_write_settings(uint8_t *dest,
                                      const nghttp3_frame_settings *fr,
                                      uint64_t payloadlen);

/*
 * nghttp3_frame_write_settings_len returns the number of bytes
 * required to write |fr|.  This function stores the frame payload
 * length in |*ppayloadlen|.
 */
size_t nghttp3_frame_write_settings_len(uint64_t *pppayloadlen,
                                        const nghttp3_frame_settings *fr);

/*
 * nghttp3_frame_write_goaway writes GOAWAY frame |fr| to |dest|.
 * This function assumes that |dest| has enough space to write |fr|.
 * |payloadlen| is the length of the frame payload.
 *
 * This function returns |dest| plus the number of bytes written.
 */
uint8_t *nghttp3_frame_write_goaway(uint8_t *dest,
                                    const nghttp3_frame_goaway *fr,
                                    uint64_t payloadlen);

/*
 * nghttp3_frame_write_goaway_len returns the number of bytes required
 * to write |fr|.  This function stores the frame payload length in
 * |*ppayloadlen|.
 */
size_t nghttp3_frame_write_goaway_len(uint64_t *ppayloadlen,
                                      const nghttp3_frame_goaway *fr);

/*
 * nghttp3_frame_write_priority_update writes PRIORITY_UPDATE frame
 * |fr| to |dest|.  This function assumes that |dest| has enough space
 * to write |fr|.  |payloadlen| is the length of the frame payload.
 *
 * This function returns |dest| plus the number of bytes written;
 */
uint8_t *nghttp3_frame_write_priority_update(
  uint8_t *dest, const nghttp3_frame_priority_update *fr, uint64_t payloadlen);

/*
 * nghttp3_frame_write_priority_update_len returns the number of bytes
 * required to write |fr|.  This function stores the frame payload
 * length in |*ppayloadlen|.
 */
size_t nghttp3_frame_write_priority_update_len(
  uint64_t *ppayloadlen, const nghttp3_frame_priority_update *fr);

/*
 * nghttp3_frame_write_origin writes ORIGIN frame |fr| to |dest|.
 * This function assumes that |dest| has enough space to write |fr|.
 * |payloadlen| is the length of the frame payload.
 *
 * This function returns |dest| plus the number of bytes written;
 */
uint8_t *nghttp3_frame_write_origin(uint8_t *dest,
                                    const nghttp3_frame_origin *fr,
                                    uint64_t payloadlen);

/*
 * nghttp3_frame_write_origin_len returns the number of bytes required
 * to write |fr|.  This function stores the frame payload length in
 * |*ppayloadlen|.
 */
size_t nghttp3_frame_write_origin_len(uint64_t *ppayloadlen,
                                      const nghttp3_frame_origin *fr);

uint8_t *nghttp3_frame_write_wt_stream(uint8_t *dest,
                                       const nghttp3_exfr_wt_stream *fr);

size_t nghttp3_frame_write_wt_stream_len(const nghttp3_exfr_wt_stream *fr);

uint8_t *nghttp3_frame_write_cpsl_wt_close_session(
  uint8_t *dest, const nghttp3_exfr_cpsl_wt_close_session *fr,
  uint64_t payloadlen);

size_t nghttp3_frame_write_cpsl_wt_close_session_len(
  uint64_t *ppayloadlen, const nghttp3_exfr_cpsl_wt_close_session *fr);

/*
 * nghttp3_nva_copy copies name/value pairs from |nva|, which contains
 * |nvlen| pairs, to |*nva_ptr|, which is dynamically allocated so
 * that all items can be stored.  The resultant name and value in
 * nghttp2_nv are guaranteed to be NULL-terminated even if the input
 * is not null-terminated.
 *
 * The |*pnva| must be freed using nghttp3_nva_del().
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * NGHTTP3_ERR_NOMEM
 *     Out of memory.
 */
int nghttp3_nva_copy(nghttp3_nv **pnva, const nghttp3_nv *nva, size_t nvlen,
                     const nghttp3_mem *mem);

/*
 * nghttp3_nva_del frees |nva|.
 */
void nghttp3_nva_del(nghttp3_nv *nva, const nghttp3_mem *mem);

/*
 * nghttp3_frame_headers_free frees memory allocated for |fr|.  It
 * assumes that fr->nva is created by nghttp3_nva_copy() or NULL.
 */
void nghttp3_frame_headers_free(nghttp3_frame_headers *fr,
                                const nghttp3_mem *mem);

/*
 * nghttp3_frame_priority_update_free frees memory allocated for |fr|.
 * This function should only be called for an outgoing frame.
 */
void nghttp3_frame_priority_update_free(nghttp3_frame_priority_update *fr,
                                        const nghttp3_mem *mem);

#endif /* !defined(NGHTTP3_FRAME_H) */
