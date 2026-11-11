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
#include <stdio.h>

#include "nghttp2_helper.h"

#define NGHTTP2_INITIAL_HASHBITS 4

void nghttp2_map_init(nghttp2_map *map, uint64_t seed, nghttp2_mem *mem) {
  *map = (nghttp2_map){
    .mem = mem,
    .seed = seed,
  };
}

void nghttp2_map_free(nghttp2_map *map) {
  if (!map) {
    return;
  }

  nghttp2_mem_free(map->mem, map->keys);
}

int nghttp2_map_each(const nghttp2_map *map, int (*func)(void *data, void *ptr),
                     void *ptr) {
  int rv;
  size_t i;
  size_t tablelen;

  if (map->size == 0) {
    return 0;
  }

  tablelen = (size_t)1 << map->hashbits;

  for (i = 0; i < tablelen; ++i) {
    if (map->psl[i] == 0) {
      continue;
    }

    rv = func(map->data[i], ptr);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

/* 64-bit finalizer (fmix64) from MurmurHash3 to maximize the output's
   sensitivity to all input bits. */
#define NGHTTP2_MAP_HASHER1 0xFF51AFD7ED558CCDULL
#define NGHTTP2_MAP_HASHER2 0xC4CEB9FE1A85EC53ULL

static size_t map_index(const nghttp2_map *map, nghttp2_map_key_type key) {
  key ^= map->seed;
  key ^= key >> 33;
  key *= NGHTTP2_MAP_HASHER1;
  key ^= key >> 33;
  key *= NGHTTP2_MAP_HASHER2;
  key ^= key >> 33;
  return (size_t)(key >> (64 - map->hashbits));
}

#ifndef WIN32
void nghttp2_map_print_distance(const nghttp2_map *map) {
  size_t i;
  size_t idx;
  size_t tablelen;

  if (map->size == 0) {
    return;
  }

  tablelen = (size_t)1 << map->hashbits;

  for (i = 0; i < tablelen; ++i) {
    if (map->psl[i] == 0) {
      fprintf(stderr, "@%zu <EMPTY>\n", i);
      continue;
    }

    idx = map_index(map, map->keys[i]);
    fprintf(stderr, "@%zu key=%" PRIu64 " base=%zu distance=%zu\n", i,
            map->keys[i], idx, map->psl[i] - 1);
  }
}
#endif /* !defined(WIN32) */

static void map_set_entry(nghttp2_map *map, size_t idx,
                          nghttp2_map_key_type key, size_t psl, void *data) {
  map->keys[idx] = key;
  map->psl[idx] = psl;
  map->data[idx] = data;
}

#define NGHTTP2_SWAP(TYPE, A, B)                                               \
  do {                                                                         \
    TYPE t = (TYPE) * (A);                                                     \
                                                                               \
    *(A) = *(B);                                                               \
    *(B) = t;                                                                  \
  } while (0)

/*
 * map_insert inserts |key| and |data| to |map|.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGHTTP2_ERR_INVALID_ARGUMENT
 *     The another data associated to |key| is already present.
 */
static int map_insert(nghttp2_map *map, nghttp2_map_key_type key, void *data) {
  size_t idx = map_index(map, key);
  size_t mask = ((size_t)1 << map->hashbits) - 1;
  size_t psl = 1;
  size_t kpsl;

  for (;;) {
    kpsl = map->psl[idx];

    if (kpsl == 0) {
      map_set_entry(map, idx, key, psl, data);
      ++map->size;

      return 0;
    }

    if (psl > kpsl) {
      NGHTTP2_SWAP(nghttp2_map_key_type, &key, &map->keys[idx]);
      NGHTTP2_SWAP(size_t, &psl, &map->psl[idx]);
      NGHTTP2_SWAP(void *, &data, &map->data[idx]);
    } else if (map->keys[idx] == key) {
      /* This check ensures that no duplicate keys are inserted.  But
         it is just a waste after first swap or if this function is
         called from map_resize.  That said, there is no difference
         with or without this conditional in performance wise. */
      return NGHTTP2_ERR_INVALID_ARGUMENT;
    }

    ++psl;
    idx = (idx + 1) & mask;
  }
}

/* NGHTTP2_MAP_MAX_HASHBITS is the maximum number of bits used for
   hash table.  The theoretical limit of the maximum number of keys
   that can be stored is 1 << NGHTTP2_MAP_MAX_HASHBITS. */
#define NGHTTP2_MAP_MAX_HASHBITS (sizeof(size_t) * 8 - 1)

static int map_resize(nghttp2_map *map, size_t new_hashbits) {
  size_t i;
  size_t tablelen;
  nghttp2_map new_map = {
    .mem = map->mem,
    .seed = map->seed,
    .hashbits = new_hashbits,
  };
  void *buf;
  int rv;
  (void)rv;

  if (new_hashbits > NGHTTP2_MAP_MAX_HASHBITS) {
    return NGHTTP2_ERR_NOMEM;
  }

  tablelen = (size_t)1 << new_hashbits;

  buf = nghttp2_mem_calloc(map->mem, tablelen,
                           sizeof(nghttp2_map_key_type) + sizeof(size_t) +
                             sizeof(void *));
  if (!buf) {
    return NGHTTP2_ERR_NOMEM;
  }

  new_map.keys = buf;
  new_map.psl = (size_t *)(void *)((uint8_t *)new_map.keys +
                                   tablelen * sizeof(nghttp2_map_key_type));
  new_map.data = (void *)((uint8_t *)new_map.psl + tablelen * sizeof(size_t));

  if (map->size) {
    tablelen = (size_t)1 << map->hashbits;

    for (i = 0; i < tablelen; ++i) {
      if (map->psl[i] == 0) {
        continue;
      }

      rv = map_insert(&new_map, map->keys[i], map->data[i]);

      /* map_insert must not fail because all keys are unique during
         resize. */
      assert(0 == rv);
    }
  }

  nghttp2_mem_free(map->mem, map->keys);
  map->keys = new_map.keys;
  map->psl = new_map.psl;
  map->data = new_map.data;
  map->hashbits = new_hashbits;

  return 0;
}

int nghttp2_map_insert(nghttp2_map *map, nghttp2_map_key_type key, void *data) {
  int rv;
  size_t tablelen;

  assert(data);

  /* tablelen is incorrect if map->hashbits == 0 which leads to
     tablelen = 1, but it is only used to check the load factor, and
     it works in this special case. */
  tablelen = (size_t)1 << map->hashbits;

  /* Load factor is 7 / 8.  Because tablelen is power of 2, (tablelen
     - (tablelen >> 3)) computes tablelen * 7 / 8. */
  if (map->size + 1 >= (tablelen - (tablelen >> 3))) {
    rv = map_resize(map, map->hashbits ? map->hashbits + 1
                                       : NGHTTP2_INITIAL_HASHBITS);
    if (rv != 0) {
      return rv;
    }
  }

  return map_insert(map, key, data);
}

void *nghttp2_map_find(const nghttp2_map *map, nghttp2_map_key_type key) {
  size_t idx;
  size_t psl = 1;
  size_t mask;

  if (map->size == 0) {
    return NULL;
  }

  idx = map_index(map, key);
  mask = ((size_t)1 << map->hashbits) - 1;

  for (;;) {
    if (psl > map->psl[idx]) {
      return NULL;
    }

    if (map->keys[idx] == key) {
      return map->data[idx];
    }

    ++psl;
    idx = (idx + 1) & mask;
  }
}

int nghttp2_map_remove(nghttp2_map *map, nghttp2_map_key_type key) {
  size_t idx;
  size_t dest;
  size_t psl = 1, kpsl;
  size_t mask;

  if (map->size == 0) {
    return NGHTTP2_ERR_INVALID_ARGUMENT;
  }

  idx = map_index(map, key);
  mask = ((size_t)1 << map->hashbits) - 1;

  for (;;) {
    if (psl > map->psl[idx]) {
      return NGHTTP2_ERR_INVALID_ARGUMENT;
    }

    if (map->keys[idx] == key) {
      dest = idx;
      idx = (idx + 1) & mask;

      for (;;) {
        kpsl = map->psl[idx];
        if (kpsl <= 1) {
          map->psl[dest] = 0;
          break;
        }

        map_set_entry(map, dest, map->keys[idx], kpsl - 1, map->data[idx]);

        dest = idx;

        idx = (idx + 1) & mask;
      }

      --map->size;

      return 0;
    }

    ++psl;
    idx = (idx + 1) & mask;
  }
}

void nghttp2_map_clear(nghttp2_map *map) {
  if (map->size == 0) {
    return;
  }

  memset(map->psl, 0, sizeof(*map->psl) * ((size_t)1 << map->hashbits));
  map->size = 0;
}

size_t nghttp2_map_size(const nghttp2_map *map) { return map->size; }
