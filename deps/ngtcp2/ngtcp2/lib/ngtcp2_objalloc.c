/*
 * ngtcp2
 *
 * Copyright (c) 2022 ngtcp2 contributors
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
#include "ngtcp2_objalloc.h"

void ngtcp2_objalloc_init(ngtcp2_objalloc *objalloc, size_t blklen,
                          const ngtcp2_mem *mem) {
  ngtcp2_balloc_init(&objalloc->balloc, blklen, mem);
  ngtcp2_opl_init(&objalloc->opl);
}

void ngtcp2_objalloc_free(ngtcp2_objalloc *objalloc) {
  ngtcp2_balloc_free(&objalloc->balloc);
}

void ngtcp2_objalloc_clear(ngtcp2_objalloc *objalloc) {
  ngtcp2_opl_clear(&objalloc->opl);
  ngtcp2_balloc_clear(&objalloc->balloc);
}
