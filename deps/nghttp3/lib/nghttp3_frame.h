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
#endif /* HAVE_CONFIG_H */

#include <nghttp3/nghttp3.h>

#include "nghttp3_buf.h"

/*
 * nghttp3_frame_write_hd writes frame header |hd| to |dest|.  This
 * function assumes that |dest| has enough space to write |hd|.
 *
 * This function returns |dest| plus the number of bytes written.
 */
uint8_t *nghttp3_frame_write_hd(uint8_t *dest, const nghttp3_frame_hd *hd);

/*
 * nghttp3_frame_write_hd_len returns the number of bytes required to
 * write |hd|.  hd->length must be set.
 */
size_t nghttp3_frame_write_hd_len(const nghttp3_frame_hd *hd);

/*
 * nghttp3_frame_write_settings writes SETTINGS frame |fr| to |dest|.
 * This function assumes that |dest| has enough space to write |fr|.
 *
 * This function returns |dest| plus the number of bytes written.
 */
uint8_t *nghttp3_frame_write_settings(uint8_t *dest,
                                      const nghttp3_frame_settings *fr);

/*
 * nghttp3_frame_write_settings_len returns the number of bytes
 * required to write |fr|.  fr->hd.length is ignored.  This function
 * stores payload length in |*ppayloadlen|.
 */
size_t nghttp3_frame_write_settings_len(int64_t *pppayloadlen,
                                        const nghttp3_frame_settings *fr);

/*
 * nghttp3_frame_write_cancel_push writes CANCEL_PUSH frame |fr| to
 * |dest|.  This function assumes that |dest| has enough space to
 * write |fr|.
 *
 * This function returns |dest| plus the number of bytes written.
 */
uint8_t *nghttp3_frame_write_cancel_push(uint8_t *dest,
                                         const nghttp3_frame_cancel_push *fr);

/*
 * nghttp3_frame_write_cancel_push_len returns the number of bytes
 * required to write |fr|.  fr->hd.length is ignored.  This function
 * stores payload length in |*ppayloadlen|.
 */
size_t nghttp3_frame_write_cancel_push_len(int64_t *ppayloadlen,
                                           const nghttp3_frame_cancel_push *fr);

/*
 * nghttp3_frame_write_max_push_id writes MAX_PUSH_ID frame |fr| to
 * |dest|.  This function assumes that |dest| has enough space to
 * write |fr|.
 *
 * This function returns |dest| plus the number of bytes written.
 */
uint8_t *nghttp3_frame_write_max_push_id(uint8_t *dest,
                                         const nghttp3_frame_max_push_id *fr);

/*
 * nghttp3_frame_write_max_push_id_len returns the number of bytes
 * required to write |fr|.  fr->hd.length is ignored.  This function
 * stores payload length in |*ppayloadlen|.
 */
size_t nghttp3_frame_write_max_push_id_len(int64_t *ppayloadlen,
                                           const nghttp3_frame_max_push_id *fr);

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
 * nghttp3_frame_push_promise_free frees memory allocated for |fr|.
 * It assumes that fr->nva is created by nghttp3_nva_copy() or NULL.
 */
void nghttp3_frame_push_promise_free(nghttp3_frame_push_promise *fr,
                                     const nghttp3_mem *mem);

#endif /* NGHTTP3_FRAME_H */
