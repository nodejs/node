/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
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
#include "nghttp3_map.h"

#include <string.h>

#include "nghttp3_conv.h"

#define INITIAL_TABLE_LENGTH 256

int nghttp3_map_init(nghttp3_map *map, const nghttp3_mem *mem) {
  map->mem = mem;
  map->tablelen = INITIAL_TABLE_LENGTH;
  map->table =
      nghttp3_mem_calloc(mem, map->tablelen, sizeof(nghttp3_map_entry *));
  if (map->table == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  map->size = 0;

  return 0;
}

void nghttp3_map_free(nghttp3_map *map) {
  nghttp3_mem_free(map->mem, map->table);
}

void nghttp3_map_each_free(const nghttp3_map *map,
                           int (*func)(nghttp3_map_entry *entry, void *ptr),
                           void *ptr) {
  uint32_t i;
  for (i = 0; i < map->tablelen; ++i) {
    nghttp3_map_entry *entry;
    for (entry = map->table[i]; entry;) {
      nghttp3_map_entry *next = entry->next;
      func(entry, ptr);
      entry = next;
    }
    map->table[i] = NULL;
  }
}

int nghttp3_map_each(const nghttp3_map *map,
                     int (*func)(nghttp3_map_entry *entry, void *ptr),
                     void *ptr) {
  int rv;
  uint32_t i;
  for (i = 0; i < map->tablelen; ++i) {
    nghttp3_map_entry *entry, *next;
    for (entry = map->table[i]; entry;) {
      next = entry->next;
      rv = func(entry, ptr);
      if (rv != 0) {
        return rv;
      }
      entry = next;
    }
  }
  return 0;
}

void nghttp3_map_entry_init(nghttp3_map_entry *entry, key_type key) {
  entry->key = key;
  entry->next = NULL;
}

/* FNV1a hash */
static uint32_t hash(key_type key, uint32_t mod) {
  uint8_t *p, *end;
  uint32_t h = 0x811C9DC5u;

  key = nghttp3_htonl64(key);
  p = (uint8_t *)&key;
  end = p + sizeof(key_type);

  for (; p != end; ++p) {
    h ^= *p;
    h *= 0x01000193u;
  }

  return h & (mod - 1);
}

static int insert(nghttp3_map_entry **table, uint32_t tablelen,
                  nghttp3_map_entry *entry) {
  uint32_t h = hash(entry->key, tablelen);
  if (table[h] == NULL) {
    table[h] = entry;
  } else {
    nghttp3_map_entry *p;
    /* We won't allow duplicated key, so check it out. */
    for (p = table[h]; p; p = p->next) {
      if (p->key == entry->key) {
        return NGHTTP3_ERR_INVALID_ARGUMENT;
      }
    }
    entry->next = table[h];
    table[h] = entry;
  }
  return 0;
}

/* new_tablelen must be power of 2 */
static int resize(nghttp3_map *map, uint32_t new_tablelen) {
  uint32_t i;
  nghttp3_map_entry **new_table;

  new_table =
      nghttp3_mem_calloc(map->mem, new_tablelen, sizeof(nghttp3_map_entry *));
  if (new_table == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  for (i = 0; i < map->tablelen; ++i) {
    nghttp3_map_entry *entry;
    for (entry = map->table[i]; entry;) {
      nghttp3_map_entry *next = entry->next;
      entry->next = NULL;
      /* This function must succeed */
      insert(new_table, new_tablelen, entry);
      entry = next;
    }
  }
  nghttp3_mem_free(map->mem, map->table);
  map->tablelen = new_tablelen;
  map->table = new_table;

  return 0;
}

int nghttp3_map_insert(nghttp3_map *map, nghttp3_map_entry *new_entry) {
  int rv;
  /* Load factor is 0.75 */
  if ((map->size + 1) * 4 > map->tablelen * 3) {
    rv = resize(map, map->tablelen * 2);
    if (rv != 0) {
      return rv;
    }
  }
  rv = insert(map->table, map->tablelen, new_entry);
  if (rv != 0) {
    return rv;
  }
  ++map->size;
  return 0;
}

nghttp3_map_entry *nghttp3_map_find(const nghttp3_map *map, key_type key) {
  uint32_t h;
  nghttp3_map_entry *entry;
  h = hash(key, map->tablelen);
  for (entry = map->table[h]; entry; entry = entry->next) {
    if (entry->key == key) {
      return entry;
    }
  }
  return NULL;
}

int nghttp3_map_remove(nghttp3_map *map, key_type key) {
  uint32_t h;
  nghttp3_map_entry **dst;

  h = hash(key, map->tablelen);

  for (dst = &map->table[h]; *dst; dst = &(*dst)->next) {
    if ((*dst)->key != key) {
      continue;
    }

    *dst = (*dst)->next;
    --map->size;
    return 0;
  }
  return NGHTTP3_ERR_INVALID_ARGUMENT;
}

void nghttp3_map_clear(nghttp3_map *map) {
  uint32_t i;

  for (i = 0; i < map->tablelen; ++i) {
    map->table[i] = NULL;
  }

  map->size = 0;
}

size_t nghttp3_map_size(const nghttp3_map *map) { return map->size; }
