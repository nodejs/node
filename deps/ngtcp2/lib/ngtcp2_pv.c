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
#include "ngtcp2_pv.h"

#include <string.h>

#include "ngtcp2_mem.h"
#include "ngtcp2_log.h"
#include "ngtcp2_macro.h"
#include "ngtcp2_addr.h"

void ngtcp2_pv_entry_init(ngtcp2_pv_entry *pvent, const uint8_t *data,
                          ngtcp2_tstamp expiry) {
  memcpy(pvent->data, data, sizeof(pvent->data));
  pvent->expiry = expiry;
}

int ngtcp2_pv_new(ngtcp2_pv **ppv, const ngtcp2_dcid *dcid,
                  ngtcp2_duration timeout, uint8_t flags, ngtcp2_log *log,
                  const ngtcp2_mem *mem) {
  int rv;

  (*ppv) = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_pv));
  if (*ppv == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  rv = ngtcp2_ringbuf_init(&(*ppv)->ents, NGTCP2_PV_MAX_ENTRIES,
                           sizeof(ngtcp2_pv_entry), mem);
  if (rv != 0) {
    ngtcp2_mem_free(mem, *ppv);
    return 0;
  }

  ngtcp2_dcid_copy(&(*ppv)->dcid, dcid);

  (*ppv)->mem = mem;
  (*ppv)->log = log;
  (*ppv)->timeout = timeout;
  (*ppv)->started_ts = 0;
  (*ppv)->loss_count = 0;
  (*ppv)->flags = flags;

  return 0;
}

void ngtcp2_pv_del(ngtcp2_pv *pv) {
  if (pv == NULL) {
    return;
  }
  ngtcp2_ringbuf_free(&pv->ents);
  ngtcp2_mem_free(pv->mem, pv);
}

void ngtcp2_pv_ensure_start(ngtcp2_pv *pv, ngtcp2_tstamp ts) {
  if (pv->started_ts) {
    return;
  }
  pv->started_ts = ts;
}

void ngtcp2_pv_add_entry(ngtcp2_pv *pv, const uint8_t *data,
                         ngtcp2_tstamp expiry) {
  ngtcp2_pv_entry *ent = ngtcp2_ringbuf_push_back(&pv->ents);
  ngtcp2_pv_entry_init(ent, data, expiry);
}

int ngtcp2_pv_full(ngtcp2_pv *pv) { return ngtcp2_ringbuf_full(&pv->ents); }

int ngtcp2_pv_validate(ngtcp2_pv *pv, const uint8_t *data) {
  size_t len = ngtcp2_ringbuf_len(&pv->ents);
  size_t i;
  ngtcp2_pv_entry *ent;

  if (len == 0) {
    return NGTCP2_ERR_INVALID_STATE;
  }

  for (i = 0; i < len; ++i) {
    ent = ngtcp2_ringbuf_get(&pv->ents, i);
    if (memcmp(ent->data, data, sizeof(ent->data)) == 0) {
      ngtcp2_log_info(pv->log, NGTCP2_LOG_EVENT_PTV, "path has been validated");
      return 0;
    }
  }

  return NGTCP2_ERR_INVALID_ARGUMENT;
}

void ngtcp2_pv_handle_entry_expiry(ngtcp2_pv *pv, ngtcp2_tstamp ts) {
  ngtcp2_pv_entry *ent;

  if (ngtcp2_ringbuf_len(&pv->ents) == 0) {
    return;
  }

  ent = ngtcp2_ringbuf_get(&pv->ents, 0);
  if (ent->expiry <= ts) {
    ++pv->loss_count;

    ngtcp2_ringbuf_pop_front(&pv->ents);

    for (; ngtcp2_ringbuf_len(&pv->ents);) {
      ent = ngtcp2_ringbuf_get(&pv->ents, 0);
      if (ent->expiry <= ts) {
        ngtcp2_ringbuf_pop_front(&pv->ents);
        continue;
      }
      break;
    }
  }
}

int ngtcp2_pv_validation_timed_out(ngtcp2_pv *pv, ngtcp2_tstamp ts) {
  return pv->started_ts + pv->timeout <= ts;
}

ngtcp2_tstamp ngtcp2_pv_next_expiry(ngtcp2_pv *pv) {
  ngtcp2_tstamp t = UINT64_MAX;
  ngtcp2_pv_entry *ent;

  if (pv->started_ts) {
    t = pv->started_ts + pv->timeout;
  }

  if (ngtcp2_ringbuf_len(&pv->ents) == 0) {
    return t;
  }

  ent = ngtcp2_ringbuf_get(&pv->ents, 0);

  return ngtcp2_min(t, ent->expiry);
}
