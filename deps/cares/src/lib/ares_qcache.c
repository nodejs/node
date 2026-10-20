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
#include "ares_private.h"

struct ares_qcache {
  ares_htable_strvp_t *cache;
  ares_slist_t        *expire;
  unsigned int         max_ttl;
};

typedef struct {
  char              *key;
  ares_dns_record_t *dnsrec;
  time_t             expire_ts;
  time_t             insert_ts;
} ares_qcache_entry_t;

char *ares_qcache_calc_key(const ares_dns_record_t *dnsrec)
{
  ares_buf_t      *buf = ares_buf_create();
  size_t           i;
  ares_status_t    status;
  ares_dns_flags_t flags;

  if (dnsrec == NULL || buf == NULL) {
    return NULL; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  /* Format is
   * OPCODE|FLAGS[|EDNS|VER|FLAGS[|ID:HEX]...]|QDCOUNT[|TYPE|CLASS|LEN:QNAME]...
   *
   * The EDNS section matters: an answer fetched under one set of EDNS options
   * is not interchangeable with one fetched under another.  A request with no
   * OPT record appends nothing; note that EDNS is on by default, so ordinary
   * lookups do carry one and their keys did change with this.
   *
   * Everything a caller does not control is emitted before the questions, so
   * the names are always last.  A name reaches the key unescaped, so anything
   * keyed after it could otherwise be imitated by a name containing that same
   * text, giving two different requests one key.  The question count is part
   * of that: without it, a request with two questions and a request with one
   * whose name spells out the second produce identical keys.  Each name is
   * also length-prefixed, so adjacent names cannot be made to run together
   * even when the question counts match. */

  status = ares_buf_append_str(
    buf, ares_dns_opcode_tostr(ares_dns_record_get_opcode(dnsrec)));
  if (status != ARES_SUCCESS) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  status = ares_buf_append_byte(buf, '|');
  if (status != ARES_SUCCESS) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  flags = ares_dns_record_get_flags(dnsrec);
  /* RD, CD and AD.  AD is here because RFC 6840 section 5.7 defines it in a
   * request as a signal that the requester wants the AD bit evaluated in the
   * response, and section 5.8 says a validating resolver should only set AD
   * in a response when the request carried DO or AD -- so an answer fetched
   * without it can legitimately differ from one fetched with it. */
  if (flags & ARES_FLAG_RD) {
    status = ares_buf_append_str(buf, "rd");
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }
  }
  if (flags & ARES_FLAG_CD) {
    status = ares_buf_append_str(buf, "cd");
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }
  }
  if (flags & ARES_FLAG_AD) {
    status = ares_buf_append_str(buf, "ad");
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }
  }

  /* EDNS state.  Skipped entirely when the request carries no OPT record. */
  {
    const ares_dns_rr_t *opt = ares_dns_get_opt_rr_const(dnsrec);

    if (opt != NULL) {
      size_t opt_cnt = ares_dns_rr_get_opt_cnt(opt, ARES_RR_OPT_OPTIONS);
      size_t j;

      status = ares_buf_append_str(buf, "|EDNS|");
      if (status != ARES_SUCCESS) {
        goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
      }
      status = ares_buf_append_num_hex(
        buf, ares_dns_rr_get_u8(opt, ARES_RR_OPT_VERSION), 0);
      if (status != ARES_SUCCESS) {
        goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
      }
      status = ares_buf_append_byte(buf, '|');
      if (status != ARES_SUCCESS) {
        goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
      }
      /* Carries the DO bit, so a DNSSEC-aware request cannot be served an
       * answer that was fetched without it. */
      status = ares_buf_append_num_hex(
        buf, ares_dns_rr_get_u16(opt, ARES_RR_OPT_FLAGS), 0);
      if (status != ARES_SUCCESS) {
        goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
      }

      /* The advertised UDP payload size is deliberately excluded: it
       * constrains how a response is transported, not what it says.  A server
       * that cannot fit the additional section may omit it without setting
       * the TC bit (RFC 2181 section 9), so two sizes can produce answers
       * that differ outside the answer section; that is accepted here. */
      for (j = 0; j < opt_cnt; j++) {
        const unsigned char *val     = NULL;
        size_t               val_len = 0;
        unsigned short       optid =
          ares_dns_rr_get_opt(opt, ARES_RR_OPT_OPTIONS, j, &val, &val_len);

        /* DNS cookies are per-server transport state and say nothing about
         * the content of an answer.  A cookie c-ares adds never reaches here
         * -- ares_cookie_apply() writes to query->query, after this key has
         * been derived -- so this only ever skips one the caller supplied. */
        if (optid == ARES_OPT_PARAM_COOKIE) {
          continue;
        }

        status = ares_buf_append_byte(buf, '|');
        if (status != ARES_SUCCESS) {
          goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
        }
        status = ares_buf_append_num_hex(buf, optid, 0);
        if (status != ARES_SUCCESS) {
          goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
        }
        status = ares_buf_append_byte(buf, ':');
        if (status != ARES_SUCCESS) {
          goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
        }
        if (val != NULL) {
          size_t k;
          for (k = 0; k < val_len; k++) {
            status = ares_buf_append_num_hex(buf, val[k], 2);
            if (status != ARES_SUCCESS) {
              goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
            }
          }
        }
      }
    }
  }

  status = ares_buf_append_byte(buf, '|');
  if (status != ARES_SUCCESS) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  status = ares_buf_append_num_dec(buf, ares_dns_record_query_cnt(dnsrec), 0);
  if (status != ARES_SUCCESS) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  for (i = 0; i < ares_dns_record_query_cnt(dnsrec); i++) {
    const char         *name;
    size_t              name_len;
    ares_dns_rec_type_t qtype;
    ares_dns_class_t    qclass;

    status = ares_dns_record_query_get(dnsrec, i, &name, &qtype, &qclass);
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: DefensiveCoding */
    }

    status = ares_buf_append_byte(buf, '|');
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }

    /* Numeric, not the mnemonic: ares_dns_rec_type_tostr() returns "UNKNOWN"
     * for every type outside its table, and ares_dns_record_query_add() does
     * not validate the type, so any 16-bit value reaches here through the
     * ordinary public API.  Two such lookups -- LOC and DNAME, say -- would
     * otherwise key identically and be answered with each other's records.
     * The key only has to be injective, not readable. */
    status = ares_buf_append_num_dec(buf, (size_t)qtype, 0);
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }

    status = ares_buf_append_byte(buf, '|');
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }

    /* Numeric for the same reason.  ares_dns_record_query_add() does reject
     * classes outside its list today, so the mnemonic is safe -- but the key
     * should not depend on that validation staying where it is. */
    status = ares_buf_append_num_dec(buf, (size_t)qclass, 0);
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }

    status = ares_buf_append_byte(buf, '|');
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }

    /* On queries, a '.' may be appended to the name to indicate an explicit
     * name lookup without performing a search.  Strip this since its not part
     * of a cached response. */
    name_len = ares_strlen(name);
    if (name_len && name[name_len - 1] == '.') {
      name_len--;
    }

    /* Length-prefix the name so it is self-delimiting.  The question count
     * above stops one question imitating two, but with the counts equal the
     * boundary between adjacent names is still guessable, since names reach
     * the key unescaped separated only by '|'.  A length makes each name
     * unambiguous for any question count. */
    status = ares_buf_append_num_dec(buf, name_len, 0);
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }

    status = ares_buf_append_byte(buf, ':');
    if (status != ARES_SUCCESS) {
      goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
    }

    if (name_len > 0) {
      status = ares_buf_append(buf, (const unsigned char *)name, name_len);
      if (status != ARES_SUCCESS) {
        goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
      }
    }
  }

  return ares_buf_finish_str(buf, NULL);

/* LCOV_EXCL_START: OutOfMemory */
fail:
  ares_buf_destroy(buf);
  return NULL;
  /* LCOV_EXCL_STOP */
}

static void ares_qcache_expire(ares_qcache_t *cache, const ares_timeval_t *now)
{
  ares_slist_node_t *node;

  if (cache == NULL) {
    return;
  }

  while ((node = ares_slist_node_first(cache->expire)) != NULL) {
    const ares_qcache_entry_t *entry = ares_slist_node_val(node);

    /* If now is NULL, we're flushing everything, so don't break */
    if (now != NULL && entry->expire_ts > now->sec) {
      break;
    }

    ares_htable_strvp_remove(cache->cache, entry->key);
    ares_slist_node_destroy(node);
  }
}

void ares_qcache_flush(ares_qcache_t *cache)
{
  ares_qcache_expire(cache, NULL /* flush all */);
}

void ares_qcache_destroy(ares_qcache_t *cache)
{
  if (cache == NULL) {
    return;
  }

  ares_htable_strvp_destroy(cache->cache);
  ares_slist_destroy(cache->expire);
  ares_free(cache);
}

static int ares_qcache_entry_sort_cb(const void *arg1, const void *arg2)
{
  const ares_qcache_entry_t *entry1 = arg1;
  const ares_qcache_entry_t *entry2 = arg2;

  if (entry1->expire_ts > entry2->expire_ts) {
    return 1;
  }

  if (entry1->expire_ts < entry2->expire_ts) {
    return -1;
  }

  return 0;
}

static void ares_qcache_entry_destroy_cb(void *arg)
{
  ares_qcache_entry_t *entry = arg;
  if (entry == NULL) {
    return; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  ares_free(entry->key);
  ares_dns_record_destroy(entry->dnsrec);
  ares_free(entry);
}

ares_status_t ares_qcache_create(ares_rand_state *rand_state,
                                 unsigned int     max_ttl,
                                 ares_qcache_t  **cache_out)
{
  ares_status_t  status = ARES_SUCCESS;
  ares_qcache_t *cache;

  cache = ares_malloc_zero(sizeof(*cache));
  if (cache == NULL) {
    status = ARES_ENOMEM; /* LCOV_EXCL_LINE: OutOfMemory */
    goto done;            /* LCOV_EXCL_LINE: OutOfMemory */
  }

  cache->cache = ares_htable_strvp_create(NULL);
  if (cache->cache == NULL) {
    status = ARES_ENOMEM; /* LCOV_EXCL_LINE: OutOfMemory */
    goto done;            /* LCOV_EXCL_LINE: OutOfMemory */
  }

  cache->expire = ares_slist_create(rand_state, ares_qcache_entry_sort_cb,
                                    ares_qcache_entry_destroy_cb);
  if (cache->expire == NULL) {
    status = ARES_ENOMEM; /* LCOV_EXCL_LINE: OutOfMemory */
    goto done;            /* LCOV_EXCL_LINE: OutOfMemory */
  }

  cache->max_ttl = max_ttl;

done:
  if (status != ARES_SUCCESS) {
    *cache_out = NULL;
    ares_qcache_destroy(cache);
    return status;
  }

  *cache_out = cache;
  return status;
}

static unsigned int ares_qcache_calc_minttl(ares_dns_record_t *dnsrec)
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

      /* TTL is meaningless on these record types */
      if (type == ARES_REC_TYPE_OPT || type == ARES_REC_TYPE_SOA ||
          type == ARES_REC_TYPE_SIG) {
        continue;
      }

      if (ttl < minttl) {
        minttl = ttl;
      }
    }
  }

  return minttl;
}

static unsigned int ares_qcache_soa_minimum(ares_dns_record_t *dnsrec)
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

/* On success, takes ownership of dnsrec */
static ares_status_t ares_qcache_insert_int(ares_qcache_t        *qcache,
                                            ares_dns_record_t    *qresp,
                                            const char           *key,
                                            const ares_timeval_t *now)
{
  ares_qcache_entry_t *entry;
  unsigned int         ttl;
  ares_dns_rcode_t     rcode = ares_dns_record_get_rcode(qresp);
  ares_dns_flags_t     flags = ares_dns_record_get_flags(qresp);

  if (qcache == NULL || qresp == NULL) {
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
    ttl = ares_qcache_soa_minimum(qresp);
  } else {
    ttl = ares_qcache_calc_minttl(qresp);
  }

  if (ttl > qcache->max_ttl) {
    ttl = qcache->max_ttl;
  }

  /* Don't cache something that is already expired */
  if (ttl == 0) {
    return ARES_EREFUSED;
  }

  entry = ares_malloc_zero(sizeof(*entry));
  if (entry == NULL) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  entry->dnsrec    = qresp;
  entry->expire_ts = (time_t)now->sec + (time_t)ttl;
  entry->insert_ts = (time_t)now->sec;

  /* The key was derived from the caller's request when the query was created.
   * It must not be recomputed from query->query here: the EDNS fallback path
   * strips the OPT record from that record before a successful retry, so a
   * key derived from it would not match the one a later lookup computes. */
  entry->key = ares_strdup(key);
  if (entry->key == NULL) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  if (!ares_htable_strvp_insert(qcache->cache, entry->key, entry)) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  if (ares_slist_insert(qcache->expire, entry) == NULL) {
    goto fail; /* LCOV_EXCL_LINE: OutOfMemory */
  }

  return ARES_SUCCESS;

/* LCOV_EXCL_START: OutOfMemory */
fail:
  if (entry != NULL) {
    if (entry->key != NULL) {
      ares_htable_strvp_remove(qcache->cache, entry->key);
      ares_free(entry->key);
    }
    ares_free(entry);
  }
  return ARES_ENOMEM;
  /* LCOV_EXCL_STOP */
}

ares_status_t ares_qcache_fetch(ares_channel_t       *channel,
                                const ares_timeval_t *now, const char *key,
                                ares_dns_record_t **dnsrec_resp)
{
  ares_qcache_entry_t *entry;
  ares_status_t        status = ARES_SUCCESS;

  if (channel == NULL || key == NULL || dnsrec_resp == NULL) {
    return ARES_EFORMERR;
  }

  *dnsrec_resp = NULL;

  if (channel->qcache == NULL) {
    return ARES_ENOTFOUND;
  }

  ares_qcache_expire(channel->qcache, now);

  entry = ares_htable_strvp_get_direct(channel->qcache->cache, key);
  if (entry == NULL) {
    status = ARES_ENOTFOUND;
    goto done;
  }

  ares_dns_record_ttl_decrement(entry->dnsrec,
                                (unsigned int)(now->sec - entry->insert_ts));

  /* Hand back a copy the caller owns rather than the cache's own record.
   * This is passed straight into an application callback, and that callback
   * is allowed to re-enter c-ares.  Any lookup it starts runs the expiry
   * reaper first, and changing the server list flushes the cache outright --
   * either would destroy this entry while the callback still held a pointer
   * into it.  Mirrors the ownership fix already made on the insert side. */
  status = ares_dns_record_duplicate_ex(dnsrec_resp, entry->dnsrec);
  if (status != ARES_SUCCESS) {
    /* Could not hand back a private copy.  Report a miss so the lookup falls
     * through to a real query rather than failing outright: duplication
     * writes and re-parses the record, so it can fail for reasons other than
     * memory exhaustion, and none of them are a reason to fail a lookup that
     * would have gone to a server a moment earlier. */
    status = ARES_ENOTFOUND; /* LCOV_EXCL_LINE: UntestablePath */
    goto done;               /* LCOV_EXCL_LINE: UntestablePath */
  }

done:
  return status;
}

ares_status_t ares_qcache_insert(ares_channel_t          *channel,
                                 const ares_timeval_t    *now,
                                 const ares_query_t      *query,
                                 const ares_dns_record_t *dnsrec)
{
  ares_dns_record_t *dupdns;
  ares_status_t      status;

  /* Checked before the duplicate below, which is a full write and re-parse:
   * no key means caching is switched off, so the copy would be thrown away.
   * ares_init_options() always creates the cache object -- it is disabled by
   * a zero maximum TTL, not by a NULL pointer -- so this is the only place
   * the distinction is visible here. */
  if (query->qcache_key == NULL) {
    return ARES_ENOTIMP;
  }

  dupdns = ares_dns_record_duplicate(dnsrec);
  if (dupdns == NULL) {
    return ARES_ENOMEM;
  }

  status =
    ares_qcache_insert_int(channel->qcache, dupdns, query->qcache_key, now);
  if (status != ARES_SUCCESS) {
    ares_dns_record_destroy(dupdns);
  }
  return status;
}
