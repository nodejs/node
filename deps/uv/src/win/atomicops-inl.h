/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef UV_WIN_ATOMICOPS_INL_H_
#define UV_WIN_ATOMICOPS_INL_H_

#include "uv.h"
#include "internal.h"


/* Atomic set operation on char */
#ifdef _MSC_VER /* MSVC */

/* _InterlockedOr8 is supported by MSVC on x32 and x64. It is slightly less
 * efficient than InterlockedExchange, but InterlockedExchange8 does not exist,
 * and interlocked operations on larger targets might require the target to be
 * aligned. */
#pragma intrinsic(_InterlockedOr8)

static char INLINE uv__atomic_exchange_set(char volatile* target) {
  return _InterlockedOr8(target, 1);
}

#else /* GCC */

/* Mingw-32 version, hopefully this works for 64-bit gcc as well. */
static inline char uv__atomic_exchange_set(char volatile* target) {
  const char one = 1;
  char old_value;
  __asm__ __volatile__ ("lock xchgb %0, %1\n\t"
                        : "=r"(old_value), "=m"(*target)
                        : "0"(one), "m"(*target)
                        : "memory");
  return old_value;
}

#endif

#endif /* UV_WIN_ATOMICOPS_INL_H_ */
