/* Copyright (c) 2013, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef UV_SPINLOCK_H_
#define UV_SPINLOCK_H_

#include "internal.h"  /* ACCESS_ONCE, UV_UNUSED */
#include "atomic-ops.h"

#define UV_SPINLOCK_INITIALIZER { 0 }

typedef struct {
  int lock;
} uv_spinlock_t;

UV_UNUSED(static void uv_spinlock_init(uv_spinlock_t* spinlock));
UV_UNUSED(static void uv_spinlock_lock(uv_spinlock_t* spinlock));
UV_UNUSED(static void uv_spinlock_unlock(uv_spinlock_t* spinlock));
UV_UNUSED(static int uv_spinlock_trylock(uv_spinlock_t* spinlock));

UV_UNUSED(static void uv_spinlock_init(uv_spinlock_t* spinlock)) {
  ACCESS_ONCE(int, spinlock->lock) = 0;
}

UV_UNUSED(static void uv_spinlock_lock(uv_spinlock_t* spinlock)) {
  while (!uv_spinlock_trylock(spinlock)) cpu_relax();
}

UV_UNUSED(static void uv_spinlock_unlock(uv_spinlock_t* spinlock)) {
  ACCESS_ONCE(int, spinlock->lock) = 0;
}

UV_UNUSED(static int uv_spinlock_trylock(uv_spinlock_t* spinlock)) {
  /* TODO(bnoordhuis) Maybe change to a ticket lock to guarantee fair queueing.
   * Not really critical until we have locks that are (frequently) contended
   * for by several threads.
   */
  return 0 == cmpxchgi(&spinlock->lock, 0, 1);
}

#endif  /* UV_SPINLOCK_H_ */
