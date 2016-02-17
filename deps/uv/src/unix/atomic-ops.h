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

#ifndef UV_ATOMIC_OPS_H_
#define UV_ATOMIC_OPS_H_

#include "internal.h"  /* UV_UNUSED */

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#include <atomic.h>
#define __sync_val_compare_and_swap(p, o, n) atomic_cas_ptr(p, o, n)
#endif

UV_UNUSED(static int cmpxchgi(int* ptr, int oldval, int newval));
UV_UNUSED(static long cmpxchgl(long* ptr, long oldval, long newval));
UV_UNUSED(static void cpu_relax(void));

/* Prefer hand-rolled assembly over the gcc builtins because the latter also
 * issue full memory barriers.
 */
UV_UNUSED(static int cmpxchgi(int* ptr, int oldval, int newval)) {
#if defined(__i386__) || defined(__x86_64__)
  int out;
  __asm__ __volatile__ ("lock; cmpxchg %2, %1;"
                        : "=a" (out), "+m" (*(volatile int*) ptr)
                        : "r" (newval), "0" (oldval)
                        : "memory");
  return out;
#elif defined(_AIX) && defined(__xlC__)
  const int out = (*(volatile int*) ptr);
  __compare_and_swap(ptr, &oldval, newval);
  return out;
#else
  return __sync_val_compare_and_swap(ptr, oldval, newval);
#endif
}

UV_UNUSED(static long cmpxchgl(long* ptr, long oldval, long newval)) {
#if defined(__i386__) || defined(__x86_64__)
  long out;
  __asm__ __volatile__ ("lock; cmpxchg %2, %1;"
                        : "=a" (out), "+m" (*(volatile long*) ptr)
                        : "r" (newval), "0" (oldval)
                        : "memory");
  return out;
#elif defined(_AIX) && defined(__xlC__)
  const long out = (*(volatile int*) ptr);
# if defined(__64BIT__)
  __compare_and_swaplp(ptr, &oldval, newval);
# else
  __compare_and_swap(ptr, &oldval, newval);
# endif /* if defined(__64BIT__) */
  return out;
#else
  return __sync_val_compare_and_swap(ptr, oldval, newval);
#endif
}

UV_UNUSED(static void cpu_relax(void)) {
#if defined(__i386__) || defined(__x86_64__)
  __asm__ __volatile__ ("rep; nop");  /* a.k.a. PAUSE */
#endif
}

#endif  /* UV_ATOMIC_OPS_H_ */
