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
#ifndef NGHTTP2_MAP_H
#define NGHTTP2_MAP_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>
#include "nghttp2_int.h"
#include "nghttp2_mem.h"

/* Implementation of unordered map */

typedef int32_t key_type;

typedef struct nghttp2_map_entry {
  struct nghttp2_map_entry *next;
  key_type key;
#if SIZEOF_INT_P == 4
  /* we requires 8 bytes aligment */
  int64_t pad;
#endif
} nghttp2_map_entry;

typedef struct {
  nghttp2_map_entry **table;
  nghttp2_mem *mem;
  size_t size;
  uint32_t tablelen;
} nghttp2_map;

/*
 * Initializes the map |map|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *   Out of memory
 */
int nghttp2_map_init(nghttp2_map *map, nghttp2_mem *mem);

/*
 * Deallocates any resources allocated for |map|. The stored entries
 * are not freed by this function. Use nghttp2_map_each_free() to free
 * each entries.
 */
void nghttp2_map_free(nghttp2_map *map);

/*
 * Deallocates each entries using |func| function and any resources
 * allocated for |map|. The |func| function is responsible for freeing
 * given the |entry| object. The |ptr| will be passed to the |func| as
 * send argument. The return value of the |func| will be ignored.
 */
void nghttp2_map_each_free(nghttp2_map *map,
                           int (*func)(nghttp2_map_entry *entry, void *ptr),
                           void *ptr);

/*
 * Initializes the |entry| with the |key|. All entries to be inserted
 * to the map must be initialized with this function.
 */
void nghttp2_map_entry_init(nghttp2_map_entry *entry, key_type key);

/*
 * Inserts the new |entry| with the key |entry->key| to the map |map|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_INVALID_ARGUMENT
 *     The item associated by |key| already exists.
 * NGHTTP2_ERR_NOMEM
 *   Out of memory
 */
int nghttp2_map_insert(nghttp2_map *map, nghttp2_map_entry *entry);

/*
 * Returns the entry associated by the key |key|.  If there is no such
 * entry, this function returns NULL.
 */
nghttp2_map_entry *nghttp2_map_find(nghttp2_map *map, key_type key);

/*
 * Removes the entry associated by the key |key| from the |map|.  The
 * removed entry is not freed by this function.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_INVALID_ARGUMENT
 *     The entry associated by |key| does not exist.
 */
int nghttp2_map_remove(nghttp2_map *map, key_type key);

/*
 * Returns the number of items stored in the map |map|.
 */
size_t nghttp2_map_size(nghttp2_map *map);

/*
 * Applies the function |func| to each entry in the |map| with the
 * optional user supplied pointer |ptr|.
 *
 * If the |func| returns 0, this function calls the |func| with the
 * next entry. If the |func| returns nonzero, it will not call the
 * |func| for further entries and return the return value of the
 * |func| immediately.  Thus, this function returns 0 if all the
 * invocations of the |func| return 0, or nonzero value which the last
 * invocation of |func| returns.
 *
 * Don't use this function to free each entry. Use
 * nghttp2_map_each_free() instead.
 */
int nghttp2_map_each(nghttp2_map *map,
                     int (*func)(nghttp2_map_entry *entry, void *ptr),
                     void *ptr);

#endif /* NGHTTP2_MAP_H */
