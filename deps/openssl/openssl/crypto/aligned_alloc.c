/*
 * Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/e_os.h"
#include "internal/cryptlib.h"
#include "internal/mem_alloc_utils.h"
#include "crypto/cryptlib.h"
#include <stdlib.h>

void *ossl_malloc_align(size_t num, size_t alignment, void **freeptr,
                        const char *file, int line)
{
    size_t alloc_bytes;
    void *ret;

    *freeptr = NULL;

    /* Ensure that alignment is a power of two no larger than 65536 */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0
        || alignment > 65536) {
        ossl_report_alloc_err_inv(file, line);
        return NULL;
    }

    /*
     * Note: Windows supports an _aligned_malloc call, but we choose
     * not to use it here, because allocations from that function
     * require that they be freed via _aligned_free.  Given that
     * we can't differentiate plain malloc blocks from blocks obtained
     * via _aligned_malloc, just avoid its use entirely
     */

    if (ossl_unlikely(!ossl_size_add(num, alignment, &alloc_bytes, file, line)))
        return NULL;

    /*
     * Step 1: Allocate an amount of memory that is <alignment>
     * bytes bigger than requested
     */
    *freeptr = CRYPTO_malloc(alloc_bytes, file, line);
    if (*freeptr == NULL)
        return NULL;

    /*
     * Step 2: Add <alignment - 1> bytes to the pointer
     * This will cross the alignment boundary that is
     * requested
     */
    ret = (void *)((char *)*freeptr + (alignment - 1));

    /*
     * Step 3: Use the alignment as a mask to translate the
     * least significant bits of the allocation at the alignment
     * boundary to 0.  ret now holds a pointer to the memory
     * buffer at the requested alignment
     * NOTE: It is a documented requirement that alignment be a
     * power of 2, which is what allows this to work
     */
    ret = (void *)((uintptr_t)ret & (uintptr_t)(~(alignment - 1)));

    return ret;
}
