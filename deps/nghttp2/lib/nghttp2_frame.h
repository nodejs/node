/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
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
#ifndef NGHTTP2_FRAME_H
#define NGHTTP2_FRAME_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>
#include "nghttp2_hd.h"
#include "nghttp2_buf.h"

#define NGHTTP2_STREAM_ID_MASK ((1u << 31) - 1)
#define NGHTTP2_PRI_GROUP_ID_MASK ((1u << 31) - 1)
#define NGHTTP2_PRIORITY_MASK ((1u << 31) - 1)
#define NGHTTP2_WINDOW_SIZE_INCREMENT_MASK ((1u << 31) - 1)
#define NGHTTP2_SETTINGS_ID_MASK ((1 << 24) - 1)

/* The number of bytes of frame header. */
#define NGHTTP2_FRAME_HDLEN 9

#define NGHTTP2_MAX_FRAME_SIZE_MAX ((1 << 24) - 1)
#define NGHTTP2_MAX_FRAME_SIZE_MIN (1 << 14)

#define NGHTTP2_MAX_PAYLOADLEN 16384
/* The one frame buffer length for transmission.  We may use several of
   them to support CONTINUATION.  To account for Pad Length field, we
   allocate extra 1 byte, which saves extra large memcopying. */
#define NGHTTP2_FRAMEBUF_CHUNKLEN                                              \
  (NGHTTP2_FRAME_HDLEN + 1 + NGHTTP2_MAX_PAYLOADLEN)

/* The default length of DATA frame payload. */
#define NGHTTP2_DATA_PAYLOADLEN NGHTTP2_MAX_FRAME_SIZE_MIN

/* Maximum headers block size to send, calculated using
   nghttp2_hd_deflate_bound().  This is the default value, and can be
   overridden by nghttp2_option_set_max_send_header_block_length(). */
#define NGHTTP2_MAX_HEADERSLEN 65536

/* The number of bytes for each SETTINGS entry */
#define NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH 6

/* Length of priority related fields in HEADERS/PRIORITY frames */
#define NGHTTP2_PRIORITY_SPECLEN 5

/* Maximum length of padding in bytes. */
#define NGHTTP2_MAX_PADLEN 256

/* Union of extension frame payload */
typedef union {
  nghttp2_ext_altsvc altsvc;
  nghttp2_ext_origin origin;
} nghttp2_ext_frame_payload;

void nghttp2_frame_pack_frame_hd(uint8_t *buf, const nghttp2_frame_hd *hd);

void nghttp2_frame_unpack_frame_hd(nghttp2_frame_hd *hd, const uint8_t *buf);

/**
 * Initializes frame header |hd| with given parameters.  Reserved bit
 * is set to 0.
 */
void nghttp2_frame_hd_init(nghttp2_frame_hd *hd, size_t length, uint8_t type,
                           uint8_t flags, int32_t stream_id);

/**
 * Returns the number of priority field depending on the |flags|.  If
 * |flags| has neither NGHTTP2_FLAG_PRIORITY_GROUP nor
 * NGHTTP2_FLAG_PRIORITY_DEPENDENCY set, return 0.
 */
size_t nghttp2_frame_priority_len(uint8_t flags);

/**
 * Packs the |pri_spec| in |buf|.  This function assumes |buf| has
 * enough space for serialization.
 */
void nghttp2_frame_pack_priority_spec(uint8_t *buf,
                                      const nghttp2_priority_spec *pri_spec);

/**
 * Unpacks the priority specification from payload |payload| of length
 * |payloadlen| to |pri_spec|.  The |flags| is used to determine what
 * kind of priority specification is in |payload|.  This function
 * assumes the |payload| contains whole priority specification.
 */
void nghttp2_frame_unpack_priority_spec(nghttp2_priority_spec *pri_spec,
                                        const uint8_t *payload);

/*
 * Returns the offset from the HEADERS frame payload where the
 * compressed header block starts. The frame payload does not include
 * frame header.
 */
size_t nghttp2_frame_headers_payload_nv_offset(nghttp2_headers *frame);

/*
 * Packs HEADERS frame |frame| in wire format and store it in |bufs|.
 * This function expands |bufs| as necessary to store frame.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * frame->hd.length is assigned after length is determined during
 * packing process.  CONTINUATION frames are also serialized in this
 * function. This function does not handle padding.
 *
 * This function returns 0 if it succeeds, or returns one of the
 * following negative error codes:
 *
 * NGHTTP2_ERR_HEADER_COMP
 *     The deflate operation failed.
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 */
int nghttp2_frame_pack_headers(nghttp2_bufs *bufs, nghttp2_headers *frame,
                               nghttp2_hd_deflater *deflater);

/*
 * Unpacks HEADERS frame byte sequence into |frame|.  This function
 * only unapcks bytes that come before name/value header block and
 * after possible Pad Length field.
 *
 * This function always succeeds and returns 0.
 */
int nghttp2_frame_unpack_headers_payload(nghttp2_headers *frame,
                                         const uint8_t *payload);

/*
 * Packs PRIORITY frame |frame| in wire format and store it in
 * |bufs|.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function always succeeds and returns 0.
 */
int nghttp2_frame_pack_priority(nghttp2_bufs *bufs, nghttp2_priority *frame);

/*
 * Unpacks PRIORITY wire format into |frame|.
 */
void nghttp2_frame_unpack_priority_payload(nghttp2_priority *frame,
                                           const uint8_t *payload);

/*
 * Packs RST_STREAM frame |frame| in wire frame format and store it in
 * |bufs|.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function always succeeds and returns 0.
 */
int nghttp2_frame_pack_rst_stream(nghttp2_bufs *bufs,
                                  nghttp2_rst_stream *frame);

/*
 * Unpacks RST_STREAM frame byte sequence into |frame|.
 */
void nghttp2_frame_unpack_rst_stream_payload(nghttp2_rst_stream *frame,
                                             const uint8_t *payload);

/*
 * Packs SETTINGS frame |frame| in wire format and store it in
 * |bufs|.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function returns 0 if it succeeds, or returns one of the
 * following negative error codes:
 *
 * NGHTTP2_ERR_FRAME_SIZE_ERROR
 *     The length of the frame is too large.
 */
int nghttp2_frame_pack_settings(nghttp2_bufs *bufs, nghttp2_settings *frame);

/*
 * Packs the |iv|, which includes |niv| entries, in the |buf|,
 * assuming the |buf| has at least 8 * |niv| bytes.
 *
 * Returns the number of bytes written into the |buf|.
 */
size_t nghttp2_frame_pack_settings_payload(uint8_t *buf,
                                           const nghttp2_settings_entry *iv,
                                           size_t niv);

void nghttp2_frame_unpack_settings_entry(nghttp2_settings_entry *iv,
                                         const uint8_t *payload);

/*
 * Initializes payload of frame->settings.  The |frame| takes
 * ownership of |iv|.
 */
void nghttp2_frame_unpack_settings_payload(nghttp2_settings *frame,
                                           nghttp2_settings_entry *iv,
                                           size_t niv);

/*
 * Unpacks SETTINGS payload into |*iv_ptr|. The number of entries are
 * assigned to the |*niv_ptr|. This function allocates enough memory
 * to store the result in |*iv_ptr|. The caller is responsible to free
 * |*iv_ptr| after its use.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 */
int nghttp2_frame_unpack_settings_payload2(nghttp2_settings_entry **iv_ptr,
                                           size_t *niv_ptr,
                                           const uint8_t *payload,
                                           size_t payloadlen, nghttp2_mem *mem);

/*
 * Packs PUSH_PROMISE frame |frame| in wire format and store it in
 * |bufs|.  This function expands |bufs| as necessary to store
 * frame.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * frame->hd.length is assigned after length is determined during
 * packing process.  CONTINUATION frames are also serialized in this
 * function. This function does not handle padding.
 *
 * This function returns 0 if it succeeds, or returns one of the
 * following negative error codes:
 *
 * NGHTTP2_ERR_HEADER_COMP
 *     The deflate operation failed.
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 */
int nghttp2_frame_pack_push_promise(nghttp2_bufs *bufs,
                                    nghttp2_push_promise *frame,
                                    nghttp2_hd_deflater *deflater);

/*
 * Unpacks PUSH_PROMISE frame byte sequence into |frame|.  This
 * function only unapcks bytes that come before name/value header
 * block and after possible Pad Length field.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_PROTO
 *     TODO END_HEADERS flag is not set
 */
int nghttp2_frame_unpack_push_promise_payload(nghttp2_push_promise *frame,
                                              const uint8_t *payload);

/*
 * Packs PING frame |frame| in wire format and store it in
 * |bufs|.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function always succeeds and returns 0.
 */
int nghttp2_frame_pack_ping(nghttp2_bufs *bufs, nghttp2_ping *frame);

/*
 * Unpacks PING wire format into |frame|.
 */
void nghttp2_frame_unpack_ping_payload(nghttp2_ping *frame,
                                       const uint8_t *payload);

/*
 * Packs GOAWAY frame |frame| in wire format and store it in |bufs|.
 * This function expands |bufs| as necessary to store frame.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 * NGHTTP2_ERR_FRAME_SIZE_ERROR
 *     The length of the frame is too large.
 */
int nghttp2_frame_pack_goaway(nghttp2_bufs *bufs, nghttp2_goaway *frame);

/*
 * Unpacks GOAWAY wire format into |frame|.  The |payload| of length
 * |payloadlen| contains first 8 bytes of payload.  The
 * |var_gift_payload| of length |var_gift_payloadlen| contains
 * remaining payload and its buffer is gifted to the function and then
 * |frame|.  The |var_gift_payloadlen| must be freed by
 * nghttp2_frame_goaway_free().
 */
void nghttp2_frame_unpack_goaway_payload(nghttp2_goaway *frame,
                                         const uint8_t *payload,
                                         uint8_t *var_gift_payload,
                                         size_t var_gift_payloadlen);

/*
 * Unpacks GOAWAY wire format into |frame|.  This function only exists
 * for unit test.  After allocating buffer for debug data, this
 * function internally calls nghttp2_frame_unpack_goaway_payload().
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 */
int nghttp2_frame_unpack_goaway_payload2(nghttp2_goaway *frame,
                                         const uint8_t *payload,
                                         size_t payloadlen, nghttp2_mem *mem);

/*
 * Packs WINDOW_UPDATE frame |frame| in wire frame format and store it
 * in |bufs|.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function always succeeds and returns 0.
 */
int nghttp2_frame_pack_window_update(nghttp2_bufs *bufs,
                                     nghttp2_window_update *frame);

/*
 * Unpacks WINDOW_UPDATE frame byte sequence into |frame|.
 */
void nghttp2_frame_unpack_window_update_payload(nghttp2_window_update *frame,
                                                const uint8_t *payload);

/*
 * Packs ALTSVC frame |frame| in wire frame format and store it in
 * |bufs|.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function always succeeds and returns 0.
 */
int nghttp2_frame_pack_altsvc(nghttp2_bufs *bufs, nghttp2_extension *ext);

/*
 * Unpacks ALTSVC wire format into |frame|.  The |payload| of
 * |payloadlen| bytes contains frame payload.  This function assumes
 * that frame->payload points to the nghttp2_ext_altsvc object.
 *
 * This function always succeeds and returns 0.
 */
void nghttp2_frame_unpack_altsvc_payload(nghttp2_extension *frame,
                                         size_t origin_len, uint8_t *payload,
                                         size_t payloadlen);

/*
 * Unpacks ALTSVC wire format into |frame|.  This function only exists
 * for unit test.  After allocating buffer for fields, this function
 * internally calls nghttp2_frame_unpack_altsvc_payload().
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 * NGHTTP2_ERR_FRAME_SIZE_ERROR
 *     The payload is too small.
 */
int nghttp2_frame_unpack_altsvc_payload2(nghttp2_extension *frame,
                                         const uint8_t *payload,
                                         size_t payloadlen, nghttp2_mem *mem);

/*
 * Packs ORIGIN frame |frame| in wire frame format and store it in
 * |bufs|.
 *
 * The caller must make sure that nghttp2_bufs_reset(bufs) is called
 * before calling this function.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_FRAME_SIZE_ERROR
 *     The length of the frame is too large.
 */
int nghttp2_frame_pack_origin(nghttp2_bufs *bufs, nghttp2_extension *ext);

/*
 * Unpacks ORIGIN wire format into |frame|.  The |payload| of length
 * |payloadlen| contains the frame payload.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 * NGHTTP2_ERR_FRAME_SIZE_ERROR
 *     The payload is too small.
 */
int nghttp2_frame_unpack_origin_payload(nghttp2_extension *frame,
                                        const uint8_t *payload,
                                        size_t payloadlen, nghttp2_mem *mem);
/*
 * Initializes HEADERS frame |frame| with given values.  |frame| takes
 * ownership of |nva|, so caller must not free it. If |stream_id| is
 * not assigned yet, it must be -1.
 */
void nghttp2_frame_headers_init(nghttp2_headers *frame, uint8_t flags,
                                int32_t stream_id, nghttp2_headers_category cat,
                                const nghttp2_priority_spec *pri_spec,
                                nghttp2_nv *nva, size_t nvlen);

void nghttp2_frame_headers_free(nghttp2_headers *frame, nghttp2_mem *mem);

void nghttp2_frame_priority_init(nghttp2_priority *frame, int32_t stream_id,
                                 const nghttp2_priority_spec *pri_spec);

void nghttp2_frame_priority_free(nghttp2_priority *frame);

void nghttp2_frame_rst_stream_init(nghttp2_rst_stream *frame, int32_t stream_id,
                                   uint32_t error_code);

void nghttp2_frame_rst_stream_free(nghttp2_rst_stream *frame);

/*
 * Initializes PUSH_PROMISE frame |frame| with given values.  |frame|
 * takes ownership of |nva|, so caller must not free it.
 */
void nghttp2_frame_push_promise_init(nghttp2_push_promise *frame, uint8_t flags,
                                     int32_t stream_id,
                                     int32_t promised_stream_id,
                                     nghttp2_nv *nva, size_t nvlen);

void nghttp2_frame_push_promise_free(nghttp2_push_promise *frame,
                                     nghttp2_mem *mem);

/*
 * Initializes SETTINGS frame |frame| with given values. |frame| takes
 * ownership of |iv|, so caller must not free it. The |flags| are
 * bitwise-OR of one or more of nghttp2_settings_flag.
 */
void nghttp2_frame_settings_init(nghttp2_settings *frame, uint8_t flags,
                                 nghttp2_settings_entry *iv, size_t niv);

void nghttp2_frame_settings_free(nghttp2_settings *frame, nghttp2_mem *mem);

/*
 * Initializes PING frame |frame| with given values. If the
 * |opqeue_data| is not NULL, it must point to 8 bytes memory region
 * of data. The data pointed by |opaque_data| is copied. It can be
 * NULL. In this case, 8 bytes NULL is used.
 */
void nghttp2_frame_ping_init(nghttp2_ping *frame, uint8_t flags,
                             const uint8_t *opque_data);

void nghttp2_frame_ping_free(nghttp2_ping *frame);

/*
 * Initializes GOAWAY frame |frame| with given values. On success,
 * this function takes ownership of |opaque_data|, so caller must not
 * free it. If the |opaque_data_len| is 0, opaque_data could be NULL.
 */
void nghttp2_frame_goaway_init(nghttp2_goaway *frame, int32_t last_stream_id,
                               uint32_t error_code, uint8_t *opaque_data,
                               size_t opaque_data_len);

void nghttp2_frame_goaway_free(nghttp2_goaway *frame, nghttp2_mem *mem);

void nghttp2_frame_window_update_init(nghttp2_window_update *frame,
                                      uint8_t flags, int32_t stream_id,
                                      int32_t window_size_increment);

void nghttp2_frame_window_update_free(nghttp2_window_update *frame);

void nghttp2_frame_extension_init(nghttp2_extension *frame, uint8_t type,
                                  uint8_t flags, int32_t stream_id,
                                  void *payload);

void nghttp2_frame_extension_free(nghttp2_extension *frame);

/*
 * Initializes ALTSVC frame |frame| with given values.  This function
 * assumes that frame->payload points to nghttp2_ext_altsvc object.
 * Also |origin| and |field_value| are allocated in single buffer,
 * starting |origin|.  On success, this function takes ownership of
 * |origin|, so caller must not free it.
 */
void nghttp2_frame_altsvc_init(nghttp2_extension *frame, int32_t stream_id,
                               uint8_t *origin, size_t origin_len,
                               uint8_t *field_value, size_t field_value_len);

/*
 * Frees up resources under |frame|.  This function does not free
 * nghttp2_ext_altsvc object pointed by frame->payload.  This function
 * only frees origin pointed by nghttp2_ext_altsvc.origin.  Therefore,
 * other fields must be allocated in the same buffer with origin.
 */
void nghttp2_frame_altsvc_free(nghttp2_extension *frame, nghttp2_mem *mem);

/*
 * Initializes ORIGIN frame |frame| with given values.  This function
 * assumes that frame->payload points to nghttp2_ext_origin object.
 * Also |ov| and the memory pointed by the field of its elements are
 * allocated in single buffer, starting with |ov|.  On success, this
 * function takes ownership of |ov|, so caller must not free it.
 */
void nghttp2_frame_origin_init(nghttp2_extension *frame,
                               nghttp2_origin_entry *ov, size_t nov);

/*
 * Frees up resources under |frame|.  This function does not free
 * nghttp2_ext_origin object pointed by frame->payload.  This function
 * only frees nghttp2_ext_origin.ov.  Therefore, other fields must be
 * allocated in the same buffer with ov.
 */
void nghttp2_frame_origin_free(nghttp2_extension *frame, nghttp2_mem *mem);

/*
 * Returns the number of padding bytes after payload.  The total
 * padding length is given in the |padlen|.  The returned value does
 * not include the Pad Length field.  If |padlen| is 0, this function
 * returns 0, regardless of frame->hd.flags.
 */
size_t nghttp2_frame_trail_padlen(nghttp2_frame *frame, size_t padlen);

void nghttp2_frame_data_init(nghttp2_data *frame, uint8_t flags,
                             int32_t stream_id);

void nghttp2_frame_data_free(nghttp2_data *frame);

/*
 * Makes copy of |iv| and return the copy. The |niv| is the number of
 * entries in |iv|. This function returns the pointer to the copy if
 * it succeeds, or NULL.
 */
nghttp2_settings_entry *nghttp2_frame_iv_copy(const nghttp2_settings_entry *iv,
                                              size_t niv, nghttp2_mem *mem);

/*
 * Sorts the |nva| in ascending order of name and value. If names are
 * equivalent, sort them by value.
 */
void nghttp2_nv_array_sort(nghttp2_nv *nva, size_t nvlen);

/*
 * Copies name/value pairs from |nva|, which contains |nvlen| pairs,
 * to |*nva_ptr|, which is dynamically allocated so that all items can
 * be stored.  The resultant name and value in nghttp2_nv are
 * guaranteed to be NULL-terminated even if the input is not
 * null-terminated.
 *
 * The |*nva_ptr| must be freed using nghttp2_nv_array_del().
 *
 * This function returns 0 if it succeeds or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 */
int nghttp2_nv_array_copy(nghttp2_nv **nva_ptr, const nghttp2_nv *nva,
                          size_t nvlen, nghttp2_mem *mem);

/*
 * Returns nonzero if the name/value pair |a| equals to |b|. The name
 * is compared in case-sensitive, because we ensure that this function
 * is called after the name is lower-cased.
 */
int nghttp2_nv_equal(const nghttp2_nv *a, const nghttp2_nv *b);

/*
 * Frees |nva|.
 */
void nghttp2_nv_array_del(nghttp2_nv *nva, nghttp2_mem *mem);

/*
 * Checks that the |iv|, which includes |niv| entries, does not have
 * invalid values.
 *
 * This function returns nonzero if it succeeds, or 0.
 */
int nghttp2_iv_check(const nghttp2_settings_entry *iv, size_t niv);

/*
 * Sets Pad Length field and flags and adjusts frame header position
 * of each buffers in |bufs|.  The number of padding is given in the
 * |padlen| including Pad Length field.  The |hd| is the frame header
 * for the serialized data.  This function fills zeros padding region
 * unless framehd_only is nonzero.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *     Out of memory.
 * NGHTTP2_ERR_FRAME_SIZE_ERROR
 *     The length of the resulting frame is too large.
 */
int nghttp2_frame_add_pad(nghttp2_bufs *bufs, nghttp2_frame_hd *hd,
                          size_t padlen, int framehd_only);

#endif /* NGHTTP2_FRAME_H */
