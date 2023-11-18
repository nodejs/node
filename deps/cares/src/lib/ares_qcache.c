/* MIT License
 *
 * Copyright (c) 2023 Brad House
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"

struct ares__qcache {
  ares__htable_strvp_t *cache;
  ares__slist_t        *expire;
  unsigned int          max_ttl;
};

typedef struct {
  char              *key;
  ares_dns_record_t *dnsrec;
  time_t             expire_ts;
  time_t             insert_ts;
} ares__qcache_entry_t;

static char *ares__qcache_calc_key(const ares_dns_record_t *dnsrec)
{
  ares__buf_t     *buf = ares__buf_create();
  size_t           i;
  ares_status_t    status;
  ares_dns_flags_t flags;

  if (dnsrec == NULL || buf == NULL) {
    return NULL;
  }

  /* Format is OPCODE|FLAGS[|QTYPE1|QCLASS1|QNAME1]... */

  status = ares__buf_append_str(
    buf, ares_dns_opcode_tostr(ares_dns_record_get_opcode(dnsrec)));
  if (status != ARES_SUCCESS) {
    goto fail;
  }

  status = ares__buf_append_byte(buf, '|');
  if (status != ARES_SUCCESS) {
    goto fail;
  }

  flags = ares_dns_record_get_flags(dnsrec);
  /* Only care about RD and CD */
  if (flags & ARES_FLAG_RD) {
    status = ares__buf_append_str(buf, "rd");
    if (status != ARES_SUCCESS) {
      goto fail;
    }
  }
  if (flags & ARES_FLAG_CD) {
    status = ares__buf_append_str(buf, "cd");
    if (status != ARES_SUCCESS) {
      goto fail;
    }
  }

  for (i = 0; i < ares_dns_record_query_cnt(dnsrec); i++) {
    const char         *name;
    ares_dns_rec_type_t qtype;
    ares_dns_class_t    qclass;

    status = ares_dns_record_query_get(dnsrec, i, &name, &qtype, &qclass);
    if (status != ARES_SUCCESS) {
      goto fail;
    }

    status = ares__buf_append_byte(buf, '|');
    if (status != ARES_SUCCESS) {
      goto fail;
    }

    status = ares__buf_append_str(buf, ares_dns_rec_type_tostr(qtype));
    if (status != ARES_SUCCESS) {
      goto fail;
    }

    status = ares__buf_append_byte(buf, '|');
    if (status != ARES_SUCCESS) {
      goto fail;
    }

    status = ares__buf_append_str(buf, ares_dns_class_tostr(qclass));
    if (status != ARES_SUCCESS) {
      goto fail;
    }

    status = ares__buf_append_byte(buf, '|');
    if (status != ARES_SUCCESS) {
      goto fail;
    }

    status = ares__buf_append_str(buf, name);
    if (status != ARES_SUCCESS) {
      goto fail;
    }
  }

  return ares__buf_finish_str(buf, NULL);

fail:
  ares__buf_destroy(buf);
  return NULL;
}

static void ares__qcache_expire(ares__qcache_t       *cache,
                                const struct timeval *now)
{
  ares__slist_node_t *node;

  if (cache == NULL) {
    return;
  }

  while ((node = ares__slist_node_first(cache->expire)) != NULL) {
    const ares__qcache_entry_t *entry = ares__slist_node_val(node);
    if (entry->expire_ts > now->tv_sec) {
      break;
    }

    ares__htable_strvp_remove(cache->cache, entry->key);
    ares__slist_node_destroy(node);
  }
}

void ares__qcache_flush(ares__qcache_t *cache)
{
  struct timeval now;
  memset(&now, 0, sizeof(now));
  ares__qcache_expire(cache, &now);
}

void ares__qcache_destroy(ares__qcache_t *cache)
{
  if (cache == NULL) {
    return;
  }

  ares__htable_strvp_destroy(cache->cache);
  ares__slist_destroy(cache->expire);
  ares_free(cache);
}

static int ares__qcache_entry_sort_cb(const void *arg1, const void *arg2)
{
  const ares__qcache_entry_t *entry1 = arg1;
  const ares__qcache_entry_t *entry2 = arg2;

  if (entry1->expire_ts > entry2->expire_ts) {
    return 1;
  }

  if (entry1->expire_ts < entry2->expire_ts) {
    return -1;
  }

  return 0;
}

static void ares__qcache_entry_destroy_cb(void *arg)
{
  ares__qcache_entry_t *entry = arg;
  if (entry == NULL) {
    return;
  }

  ares_free(entry->key);
  ares_dns_record_destroy(entry->dnsrec);
  ares_free(entry);
}

ares_status_t ares__qcache_create(ares_rand_state *rand_state,
                                  unsigned int     max_ttl,
                                  ares__qcache_t **cache_out)
{
  ares_status_t   status = ARES_SUCCESS;
  ares__qcache_t *cache;

  cache = ares_malloc_zero(sizeof(*cache));
  if (cache == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  cache->cache = ares__htable_strvp_create(NULL);
  if (cache->cache == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  cache->expire = ares__slist_create(rand_state, ares__qcache_entry_sort_cb,
                                     ares__qcache_entry_destroy_cb);
  if (cache->expire == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  cache->max_ttl = max_ttl;

done:
  if (status != ARES_SUCCESS) {
    *cache_out = NULL;
    ares__qcache_destroy(cache);
    return status;
  }

  *cache_out = cache;
  return status;
}

static unsigned int ares__qcache_calc_minttl(ares_dns_record_t *dnsrec)
{
  unsigned int minttl = 0xFFFFFFFF;
  size_t       sect;

  for (sect = ARES_SECTION_ANSWER; sect <= ARES_SECTION_ADDITIONAL; sect++) {
    size_t i;
    for (i = 0; i < ares_dns_record_rr_cnt(dnsrec, (ares_dns_section_t)sect);
         i++) {
      const ares_dns_rr_t *rr =
        ares_dns_record_rr_get(dnsrec, (ares_dns_section_t)sect, i);
      ares_dns_rec_type_t type = ares_dns_rr_get_type(rr);
      unsigned int        ttl  = ares_dns_rr_get_ttl(rr);
      if (type == ARES_REC_TYPE_OPT || type == ARES_REC_TYPE_SOA) {
        continue;
      }

      if (ttl < minttl) {
        minttl = ttl;
      }
    }
  }

  return minttl;
}

static unsigned int ares__qcache_soa_minimum(ares_dns_record_t *dnsrec)
{
  size_t i;

  /* RFC 2308 Section 5 says its the minimum of MINIMUM and the TTL of the
   * record. */
  for (i = 0; i < ares_dns_record_rr_cnt(dnsrec, ARES_SECTION_AUTHORITY); i++) {
    const ares_dns_rr_t *rr =
      ares_dns_record_rr_get(dnsrec, ARES_SECTION_AUTHORITY, i);
    ares_dns_rec_type_t type = ares_dns_rr_get_type(rr);
    unsigned int        ttl;
    unsigned int        minimum;

    if (type != ARES_REC_TYPE_SOA) {
      continue;
    }

    minimum = ares_dns_rr_get_u32(rr, ARES_RR_SOA_MINIMUM);
    ttl     = ares_dns_rr_get_ttl(rr);

    if (ttl > minimum) {
      return minimum;
    }
    return ttl;
  }

  return 0;
}

static char *ares__qcache_calc_key_frombuf(const unsigned char *qbuf,
                                           size_t               qlen)
{
  ares_status_t      status;
  ares_dns_record_t *dnsrec = NULL;
  char              *key    = NULL;

  status = ares_dns_parse(qbuf, qlen, 0, &dnsrec);
  if (status != ARES_SUCCESS) {
    goto done;
  }

  key = ares__qcache_calc_key(dnsrec);

done:
  ares_dns_record_destroy(dnsrec);
  return key;
}

/* On success, takes ownership of dnsrec */
static ares_status_t ares__qcache_insert(ares__qcache_t      *qcache,
                                         ares_dns_record_t   *dnsrec,
                                         const unsigned char *qbuf, size_t qlen,
                                         const struct timeval *now)
{
  ares__qcache_entry_t *entry;
  unsigned int          ttl;
  ares_dns_rcode_t      rcode = ares_dns_record_get_rcode(dnsrec);
  ares_dns_flags_t      flags = ares_dns_record_get_flags(dnsrec);

  if (qcache == NULL || dnsrec == NULL) {
    return ARES_EFORMERR;
  }

  /* Only save NOERROR or NXDOMAIN */
  if (rcode != ARES_RCODE_NOERROR && rcode != ARES_RCODE_NXDOMAIN) {
    return ARES_ENOTIMP;
  }

  /* Don't save truncated queries */
  if (flags & ARES_FLAG_TC) {
    return ARES_ENOTIMP;
  }

  /* Look at SOA for NXDOMAIN for minimum */
  if (rcode == ARES_RCODE_NXDOMAIN) {
    ttl = ares__qcache_soa_minimum(dnsrec);
  } else {
    ttl = ares__qcache_calc_minttl(dnsrec);
  }

  /* Don't cache something that is already expired */
  if (ttl == 0) {
    return ARES_EREFUSED;
  }

  if (ttl > qcache->max_ttl) {
    ttl = qcache->max_ttl;
  }

  entry = ares_malloc_zero(sizeof(*entry));
  if (entry == NULL) {
    goto fail;
  }

  entry->dnsrec    = dnsrec;
  entry->expire_ts = now->tv_sec + ttl;
  entry->insert_ts = now->tv_sec;

  /* We can't guarantee the server responded with the same flags as the
   * request had, so we have to re-parse the request in order to generate the
   * key for caching, but we'll only do this once we know for sure we really
   * want to cache it */
  entry->key = ares__qcache_calc_key_frombuf(qbuf, qlen);
  if (entry->key == NULL) {
    goto fail;
  }

  if (!ares__htable_strvp_insert(qcache->cache, entry->key, entry)) {
    goto fail;
  }

  if (ares__slist_insert(qcache->expire, entry) == NULL) {
    goto fail;
  }

  return ARES_SUCCESS;

fail:
  if (entry != NULL && entry->key != NULL) {
    ares__htable_strvp_remove(qcache->cache, entry->key);
    ares_free(entry->key);
    ares_free(entry);
  }
  return ARES_ENOMEM;
}

static ares_status_t ares__qcache_fetch(ares__qcache_t          *qcache,
                                        const ares_dns_record_t *dnsrec,
                                        const struct timeval    *now,
                                        unsigned char **buf, size_t *buf_len)
{
  char                 *key = NULL;
  ares__qcache_entry_t *entry;
  ares_status_t         status;

  if (qcache == NULL || dnsrec == NULL) {
    return ARES_EFORMERR;
  }

  ares__qcache_expire(qcache, now);

  key = ares__qcache_calc_key(dnsrec);
  if (key == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  entry = ares__htable_strvp_get_direct(qcache->cache, key);
  if (entry == NULL) {
    status = ARES_ENOTFOUND;
    goto done;
  }

  ares_dns_record_write_ttl_decrement(
    entry->dnsrec, (unsigned int)(now->tv_sec - entry->insert_ts));

  status = ares_dns_write(entry->dnsrec, buf, buf_len);

done:
  ares_free(key);
  return status;
}

ares_status_t ares_qcache_insert(ares_channel_t       *channel,
                                 const struct timeval *now,
                                 const struct query   *query,
                                 ares_dns_record_t    *dnsrec)
{
  return ares__qcache_insert(channel->qcache, dnsrec, query->qbuf, query->qlen,
                             now);
}

ares_status_t ares_qcache_fetch(ares_channel_t       *channel,
                                const struct timeval *now,
                                const unsigned char *qbuf, size_t qlen,
                                unsigned char **abuf, size_t *alen)
{
  ares_status_t      status;
  ares_dns_record_t *dnsrec = NULL;

  if (channel->qcache == NULL) {
    return ARES_ENOTFOUND;
  }

  status = ares_dns_parse(qbuf, qlen, 0, &dnsrec);
  if (status != ARES_SUCCESS) {
    goto done;
  }

  status = ares__qcache_fetch(channel->qcache, dnsrec, now, abuf, alen);

done:
  ares_dns_record_destroy(dnsrec);
  return status;
}
