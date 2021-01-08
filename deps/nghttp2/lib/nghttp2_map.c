/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
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
#include "nghttp2_map.h"

#include <string.h>
#include <assert.h>

#include "nghttp2_helper.h"

#define INITIAL_TABLE_LENGTH 256

int nghttp2_map_init(nghttp2_map *map, nghttp2_mem *mem) {
  map->mem = mem;
  map->tablelen = INITIAL_TABLE_LENGTH;
  map->table =
      nghttp2_mem_calloc(mem, map->tablelen, sizeof(nghttp2_map_bucket));
  if (map->table == NULL) {
    return NGHTTP2_ERR_NOMEM;
  }

  map->size = 0;

  return 0;
}

void nghttp2_map_free(nghttp2_map *map) {
  size_t i;
  nghttp2_map_bucket *bkt;

  if (!map) {
    return;
  }

  for (i = 0; i < map->tablelen; ++i) {
    bkt = &map->table[i];
    if (bkt->ksl) {
      nghttp2_ksl_free(bkt->ksl);
      nghttp2_mem_free(map->mem, bkt->ksl);
    }
  }

  nghttp2_mem_free(map->mem, map->table);
}

void nghttp2_map_each_free(nghttp2_map *map,
                           int (*func)(nghttp2_map_entry *entry, void *ptr),
                           void *ptr) {
  uint32_t i;
  nghttp2_map_bucket *bkt;
  nghttp2_ksl_it it;

  for (i = 0; i < map->tablelen; ++i) {
    bkt = &map->table[i];

    if (bkt->ptr) {
      func(bkt->ptr, ptr);
      bkt->ptr = NULL;
      assert(bkt->ksl == NULL || nghttp2_ksl_len(bkt->ksl) == 0);
      continue;
    }

    if (bkt->ksl) {
      for (it = nghttp2_ksl_begin(bkt->ksl); !nghttp2_ksl_it_end(&it);
           nghttp2_ksl_it_next(&it)) {
        func(nghttp2_ksl_it_get(&it), ptr);
      }

      nghttp2_ksl_free(bkt->ksl);
      nghttp2_mem_free(map->mem, bkt->ksl);
      bkt->ksl = NULL;
    }
  }
}

int nghttp2_map_each(nghttp2_map *map,
                     int (*func)(nghttp2_map_entry *entry, void *ptr),
                     void *ptr) {
  int rv;
  uint32_t i;
  nghttp2_map_bucket *bkt;
  nghttp2_ksl_it it;

  for (i = 0; i < map->tablelen; ++i) {
    bkt = &map->table[i];

    if (bkt->ptr) {
      rv = func(bkt->ptr, ptr);
      if (rv != 0) {
        return rv;
      }
      assert(bkt->ksl == NULL || nghttp2_ksl_len(bkt->ksl) == 0);
      continue;
    }

    if (bkt->ksl) {
      for (it = nghttp2_ksl_begin(bkt->ksl); !nghttp2_ksl_it_end(&it);
           nghttp2_ksl_it_next(&it)) {
        rv = func(nghttp2_ksl_it_get(&it), ptr);
        if (rv != 0) {
          return rv;
        }
      }
    }
  }
  return 0;
}

void nghttp2_map_entry_init(nghttp2_map_entry *entry, key_type key) {
  entry->key = key;
  entry->next = NULL;
}

/* FNV1a hash */
static uint32_t hash(key_type key, uint32_t mod) {
  uint8_t *p, *end;
  uint32_t h = 0x811C9DC5u;

  p = (uint8_t *)&key;
  end = p + sizeof(key_type);

  for (; p != end;) {
    h ^= *p++;
    h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
  }

  return h & (mod - 1);
}

static int less(const nghttp2_ksl_key *lhs, const nghttp2_ksl_key *rhs) {
  return *(key_type *)lhs < *(key_type *)rhs;
}

static int map_insert(nghttp2_map *map, nghttp2_map_bucket *table,
                      uint32_t tablelen, nghttp2_map_entry *entry) {
  uint32_t h = hash(entry->key, tablelen);
  nghttp2_map_bucket *bkt = &table[h];
  nghttp2_mem *mem = map->mem;
  int rv;

  if (bkt->ptr == NULL &&
      (bkt->ksl == NULL || nghttp2_ksl_len(bkt->ksl) == 0)) {
    bkt->ptr = entry;
    return 0;
  }

  if (!bkt->ksl) {
    bkt->ksl = nghttp2_mem_malloc(mem, sizeof(*bkt->ksl));
    if (bkt->ksl == NULL) {
      return NGHTTP2_ERR_NOMEM;
    }
    nghttp2_ksl_init(bkt->ksl, less, sizeof(key_type), mem);
  }

  if (bkt->ptr) {
    rv = nghttp2_ksl_insert(bkt->ksl, NULL, &bkt->ptr->key, bkt->ptr);
    if (rv != 0) {
      return rv;
    }

    bkt->ptr = NULL;
  }

  return nghttp2_ksl_insert(bkt->ksl, NULL, &entry->key, entry);
}

/* new_tablelen must be power of 2 */
static int map_resize(nghttp2_map *map, uint32_t new_tablelen) {
  uint32_t i;
  nghttp2_map_bucket *new_table;
  nghttp2_map_bucket *bkt;
  nghttp2_ksl_it it;
  int rv;

  new_table =
      nghttp2_mem_calloc(map->mem, new_tablelen, sizeof(nghttp2_map_bucket));
  if (new_table == NULL) {
    return NGHTTP2_ERR_NOMEM;
  }

  for (i = 0; i < map->tablelen; ++i) {
    bkt = &map->table[i];

    if (bkt->ptr) {
      rv = map_insert(map, new_table, new_tablelen, bkt->ptr);
      if (rv != 0) {
        goto fail;
      }
      assert(bkt->ksl == NULL || nghttp2_ksl_len(bkt->ksl) == 0);
      continue;
    }

    if (bkt->ksl) {
      for (it = nghttp2_ksl_begin(bkt->ksl); !nghttp2_ksl_it_end(&it);
           nghttp2_ksl_it_next(&it)) {
        rv = map_insert(map, new_table, new_tablelen, nghttp2_ksl_it_get(&it));
        if (rv != 0) {
          goto fail;
        }
      }
    }
  }

  for (i = 0; i < map->tablelen; ++i) {
    bkt = &map->table[i];
    if (bkt->ksl) {
      nghttp2_ksl_free(bkt->ksl);
      nghttp2_mem_free(map->mem, bkt->ksl);
    }
  }

  nghttp2_mem_free(map->mem, map->table);
  map->tablelen = new_tablelen;
  map->table = new_table;

  return 0;

fail:
  for (i = 0; i < new_tablelen; ++i) {
    bkt = &new_table[i];
    if (bkt->ksl) {
      nghttp2_ksl_free(bkt->ksl);
      nghttp2_mem_free(map->mem, bkt->ksl);
    }
  }

  return rv;
}

int nghttp2_map_insert(nghttp2_map *map, nghttp2_map_entry *new_entry) {
  int rv;

  /* Load factor is 0.75 */
  if ((map->size + 1) * 4 > map->tablelen * 3) {
    rv = map_resize(map, map->tablelen * 2);
    if (rv != 0) {
      return rv;
    }
  }
  rv = map_insert(map, map->table, map->tablelen, new_entry);
  if (rv != 0) {
    return rv;
  }
  ++map->size;
  return 0;
}

nghttp2_map_entry *nghttp2_map_find(nghttp2_map *map, key_type key) {
  nghttp2_map_bucket *bkt = &map->table[hash(key, map->tablelen)];
  nghttp2_ksl_it it;

  if (bkt->ptr) {
    if (bkt->ptr->key == key) {
      return bkt->ptr;
    }
    return NULL;
  }

  if (bkt->ksl) {
    it = nghttp2_ksl_lower_bound(bkt->ksl, &key);
    if (nghttp2_ksl_it_end(&it) ||
        *(key_type *)nghttp2_ksl_it_key(&it) != key) {
      return NULL;
    }
    return nghttp2_ksl_it_get(&it);
  }

  return NULL;
}

int nghttp2_map_remove(nghttp2_map *map, key_type key) {
  nghttp2_map_bucket *bkt = &map->table[hash(key, map->tablelen)];
  int rv;

  if (bkt->ptr) {
    if (bkt->ptr->key == key) {
      bkt->ptr = NULL;
      --map->size;
      return 0;
    }
    return NGHTTP2_ERR_INVALID_ARGUMENT;
  }

  if (bkt->ksl) {
    rv = nghttp2_ksl_remove(bkt->ksl, NULL, &key);
    if (rv != 0) {
      return rv;
    }
    --map->size;
    return 0;
  }

  return NGHTTP2_ERR_INVALID_ARGUMENT;
}

void nghttp2_map_clear(nghttp2_map *map) {
  uint32_t i;
  nghttp2_map_bucket *bkt;

  for (i = 0; i < map->tablelen; ++i) {
    bkt = &map->table[i];
    bkt->ptr = NULL;
    if (bkt->ksl) {
      nghttp2_ksl_free(bkt->ksl);
      nghttp2_mem_free(map->mem, bkt->ksl);
      bkt->ksl = NULL;
    }
  }

  map->size = 0;
}

size_t nghttp2_map_size(nghttp2_map *map) { return map->size; }
