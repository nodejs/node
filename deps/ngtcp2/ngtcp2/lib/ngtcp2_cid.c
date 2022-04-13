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
#include "ngtcp2_cid.h"

#include <assert.h>
#include <string.h>

#include "ngtcp2_path.h"
#include "ngtcp2_str.h"

void ngtcp2_cid_zero(ngtcp2_cid *cid) { cid->datalen = 0; }

void ngtcp2_cid_init(ngtcp2_cid *cid, const uint8_t *data, size_t datalen) {
  assert(datalen <= NGTCP2_MAX_CIDLEN);

  cid->datalen = datalen;
  if (datalen) {
    ngtcp2_cpymem(cid->data, data, datalen);
  }
}

int ngtcp2_cid_eq(const ngtcp2_cid *cid, const ngtcp2_cid *other) {
  return cid->datalen == other->datalen &&
         0 == memcmp(cid->data, other->data, cid->datalen);
}

int ngtcp2_cid_less(const ngtcp2_cid *lhs, const ngtcp2_cid *rhs) {
  int s = lhs->datalen < rhs->datalen;
  size_t n = s ? lhs->datalen : rhs->datalen;
  int c = memcmp(lhs->data, rhs->data, n);

  return c < 0 || (c == 0 && s);
}

int ngtcp2_cid_empty(const ngtcp2_cid *cid) { return cid->datalen == 0; }

void ngtcp2_scid_init(ngtcp2_scid *scid, uint64_t seq, const ngtcp2_cid *cid,
                      const uint8_t *token) {
  scid->pe.index = NGTCP2_PQ_BAD_INDEX;
  scid->seq = seq;
  scid->cid = *cid;
  scid->ts_retired = UINT64_MAX;
  scid->flags = NGTCP2_SCID_FLAG_NONE;
  if (token) {
    memcpy(scid->token, token, NGTCP2_STATELESS_RESET_TOKENLEN);
  } else {
    memset(scid->token, 0, NGTCP2_STATELESS_RESET_TOKENLEN);
  }
}

void ngtcp2_scid_copy(ngtcp2_scid *dest, const ngtcp2_scid *src) {
  ngtcp2_scid_init(dest, src->seq, &src->cid, src->token);
  dest->ts_retired = src->ts_retired;
  dest->flags = src->flags;
}

void ngtcp2_dcid_init(ngtcp2_dcid *dcid, uint64_t seq, const ngtcp2_cid *cid,
                      const uint8_t *token) {
  dcid->seq = seq;
  dcid->cid = *cid;
  if (token) {
    memcpy(dcid->token, token, NGTCP2_STATELESS_RESET_TOKENLEN);
  } else {
    memset(dcid->token, 0, NGTCP2_STATELESS_RESET_TOKENLEN);
  }
  ngtcp2_path_storage_zero(&dcid->ps);
  dcid->ts_retired = UINT64_MAX;
  dcid->flags = NGTCP2_DCID_FLAG_NONE;
  dcid->bytes_sent = 0;
  dcid->bytes_recv = 0;
}

void ngtcp2_dcid_copy(ngtcp2_dcid *dest, const ngtcp2_dcid *src) {
  ngtcp2_dcid_init(dest, src->seq, &src->cid, src->token);
  ngtcp2_path_copy(&dest->ps.path, &src->ps.path);
  dest->ts_retired = src->ts_retired;
  dest->flags = src->flags;
  dest->bytes_sent = src->bytes_sent;
  dest->bytes_recv = src->bytes_recv;
}

void ngtcp2_dcid_copy_cid_token(ngtcp2_dcid *dest, const ngtcp2_dcid *src) {
  dest->seq = src->seq;
  dest->cid = src->cid;
  memcpy(dest->token, src->token, NGTCP2_STATELESS_RESET_TOKENLEN);
}

int ngtcp2_dcid_verify_uniqueness(ngtcp2_dcid *dcid, uint64_t seq,
                                  const ngtcp2_cid *cid, const uint8_t *token) {
  if (dcid->seq == seq) {
    return ngtcp2_cid_eq(&dcid->cid, cid) &&
                   memcmp(dcid->token, token,
                          NGTCP2_STATELESS_RESET_TOKENLEN) == 0
               ? 0
               : NGTCP2_ERR_PROTO;
  }

  return !ngtcp2_cid_eq(&dcid->cid, cid) ? 0 : NGTCP2_ERR_PROTO;
}
