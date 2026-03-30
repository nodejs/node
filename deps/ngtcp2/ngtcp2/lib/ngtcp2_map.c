/*
 * ngtcp2
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
#include "ngtcp2_map.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "ngtcp2_conv.h"

#define NGTCP2_INITIAL_HASHBITS 4

void ngtcp2_map_init(ngtcp2_map *map, uint64_t seed, const ngtcp2_mem *mem) {
  *map = (ngtcp2_map){
    .mem = mem,
    .seed = seed,
  };
}

void ngtcp2_map_free(ngtcp2_map *map) {
  if (!map) {
    return;
  }

  ngtcp2_mem_free(map->mem, map->keys);
}

int ngtcp2_map_each(const ngtcp2_map *map, int (*func)(void *data, void *ptr),
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

/* Hasher from
   https://github.com/rust-lang/rustc-hash/blob/dc5c33f1283de2da64d8d7a06401d91aded03ad4/src/lib.rs
   to maximize the output's sensitivity to all input bits. */
#define NGTCP2_MAP_HASHER 0xf1357aea2e62a9c5ull
/* 64-bit Fibonacci hashing constant, Golden Ratio constant, to get
   the high bits with the good distribution. */
#define NGTCP2_MAP_FIBO 0x9e3779b97f4a7c15ull

static size_t map_index(const ngtcp2_map *map, ngtcp2_map_key_type key) {
  key += map->seed;
  key *= NGTCP2_MAP_HASHER;
  return (size_t)((key * NGTCP2_MAP_FIBO) >> (64 - map->hashbits));
}

#ifndef WIN32
void ngtcp2_map_print_distance(const ngtcp2_map *map) {
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
    fprintf(stderr, "@%zu key=%" PRIu64 " base=%zu distance=%u\n", i,
            map->keys[i], idx, map->psl[i] - 1);
  }
}
#endif /* !defined(WIN32) */

static void map_set_entry(ngtcp2_map *map, size_t idx, ngtcp2_map_key_type key,
                          void *data, size_t psl) {
  map->keys[idx] = key;
  map->data[idx] = data;
  map->psl[idx] = (uint8_t)psl;
}

#define NGTCP2_SWAP(TYPE, A, B)                                                \
  do {                                                                         \
    TYPE t = (TYPE) * (A);                                                     \
                                                                               \
    *(A) = *(B);                                                               \
    *(B) = t;                                                                  \
  } while (0)

/*
 * map_insert inserts |key| and |data| to |map|, and returns the index
 * where the pair is stored if it succeeds.  Otherwise, it returns one
 * of the following negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     The another data associated to |key| is already present.
 */
static ngtcp2_ssize map_insert(ngtcp2_map *map, ngtcp2_map_key_type key,
                               void *data) {
  size_t idx = map_index(map, key);
  size_t mask = ((size_t)1 << map->hashbits) - 1;
  size_t psl = 1;
  size_t kpsl;

  for (;;) {
    kpsl = map->psl[idx];

    if (kpsl == 0) {
      map_set_entry(map, idx, key, data, psl);
      ++map->size;

      return (ngtcp2_ssize)idx;
    }

    if (psl > kpsl) {
      NGTCP2_SWAP(ngtcp2_map_key_type, &key, &map->keys[idx]);
      NGTCP2_SWAP(void *, &data, &map->data[idx]);
      NGTCP2_SWAP(uint8_t, &psl, &map->psl[idx]);
    } else if (map->keys[idx] == key) {
      /* This check ensures that no duplicate keys are inserted.  But
         it is just a waste after first swap or if this function is
         called from map_resize.  That said, there is no difference
         with or without this conditional in performance wise. */
      return NGTCP2_ERR_INVALID_ARGUMENT;
    }

    ++psl;
    idx = (idx + 1) & mask;
  }
}

/* NGTCP2_MAP_MAX_HASHBITS is the maximum number of bits used for hash
   table.  The theoretical limit of the maximum number of keys that
   can be stored is 1 << NGTCP2_MAP_MAX_HASHBITS. */
#define NGTCP2_MAP_MAX_HASHBITS (sizeof(size_t) * 8 - 1)

static int map_resize(ngtcp2_map *map, size_t new_hashbits) {
  size_t i;
  size_t tablelen;
  ngtcp2_ssize idx;
  ngtcp2_map new_map = {
    .mem = map->mem,
    .seed = map->seed,
    .hashbits = new_hashbits,
  };
  void *buf;
  (void)idx;

  if (new_hashbits > NGTCP2_MAP_MAX_HASHBITS) {
    return NGTCP2_ERR_NOMEM;
  }

  tablelen = (size_t)1 << new_hashbits;

  buf = ngtcp2_mem_calloc(map->mem, tablelen,
                          sizeof(ngtcp2_map_key_type) + sizeof(void *) +
                            sizeof(uint8_t));
  if (buf == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  new_map.keys = buf;
  new_map.data =
    (void *)((uint8_t *)new_map.keys + tablelen * sizeof(ngtcp2_map_key_type));
  new_map.psl = (uint8_t *)new_map.data + tablelen * sizeof(void *);

  if (map->size) {
    tablelen = (size_t)1 << map->hashbits;

    for (i = 0; i < tablelen; ++i) {
      if (map->psl[i] == 0) {
        continue;
      }

      idx = map_insert(&new_map, map->keys[i], map->data[i]);

      /* map_insert must not fail because all keys are unique during
         resize. */
      assert(idx >= 0);
    }
  }

  ngtcp2_mem_free(map->mem, map->keys);
  map->keys = new_map.keys;
  map->data = new_map.data;
  map->psl = new_map.psl;
  map->hashbits = new_hashbits;

  return 0;
}

/* NGTCP2_MAX_PSL_RESIZE_THRESH is the maximum psl threshold.  If
   reached, resize the table. */
#define NGTCP2_MAX_PSL_RESIZE_THRESH 128

int ngtcp2_map_insert(ngtcp2_map *map, ngtcp2_map_key_type key, void *data) {
  int rv;
  size_t tablelen;
  ngtcp2_ssize idx;

  assert(data);

  /* tablelen is incorrect if map->hashbits == 0 which leads to
     tablelen = 1, but it is only used to check the load factor, and
     it works in this special case. */
  tablelen = (size_t)1 << map->hashbits;

  /* Load factor is 7 / 8.  Because tablelen is power of 2, (tablelen
     - (tablelen >> 3)) computes tablelen * 7 / 8. */
  if (map->size + 1 >= (tablelen - (tablelen >> 3))) {
    rv = map_resize(map, map->hashbits ? map->hashbits + 1
                                       : NGTCP2_INITIAL_HASHBITS);
    if (rv != 0) {
      return rv;
    }

    idx = map_insert(map, key, data);
    if (idx < 0) {
      return (int)idx;
    }

    return 0;
  }

  idx = map_insert(map, key, data);
  if (idx < 0) {
    return (int)idx;
  }

  /* Resize if psl reaches really large value which is almost
     improbable, but just in case. */
  if (map->psl[idx] - 1 < NGTCP2_MAX_PSL_RESIZE_THRESH) {
    return 0;
  }

  return map_resize(map, map->hashbits + 1);
}

void *ngtcp2_map_find(const ngtcp2_map *map, ngtcp2_map_key_type key) {
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

int ngtcp2_map_remove(ngtcp2_map *map, ngtcp2_map_key_type key) {
  size_t idx;
  size_t dest;
  size_t psl = 1, kpsl;
  size_t mask;

  if (map->size == 0) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  idx = map_index(map, key);
  mask = ((size_t)1 << map->hashbits) - 1;

  for (;;) {
    if (psl > map->psl[idx]) {
      return NGTCP2_ERR_INVALID_ARGUMENT;
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

        map_set_entry(map, dest, map->keys[idx], map->data[idx], kpsl - 1);

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

void ngtcp2_map_clear(ngtcp2_map *map) {
  if (map->size == 0) {
    return;
  }

  memset(map->psl, 0, sizeof(*map->psl) * ((size_t)1 << map->hashbits));
  map->size = 0;
}

size_t ngtcp2_map_size(const ngtcp2_map *map) { return map->size; }
