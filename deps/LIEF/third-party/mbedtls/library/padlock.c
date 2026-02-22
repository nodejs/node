/*
 *  VIA PadLock support functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  This implementation is based on the VIA PadLock Programming Guide:
 *
 *  http://www.via.com.tw/en/downloads/whitepapers/initiatives/padlock/
 *  programming_guide.pdf
 */

#include "common.h"

#if defined(MBEDTLS_PADLOCK_C)

#include "padlock.h"

#include <string.h>

#if defined(MBEDTLS_VIA_PADLOCK_HAVE_CODE)

/*
 * PadLock detection routine
 */
int mbedtls_padlock_has_support(int feature)
{
    static int flags = -1;
    int ebx = 0, edx = 0;

    if (flags == -1) {
        asm ("movl  %%ebx, %0           \n\t"
             "movl  $0xC0000000, %%eax  \n\t"
             "cpuid                     \n\t"
             "cmpl  $0xC0000001, %%eax  \n\t"
             "movl  $0, %%edx           \n\t"
             "jb    1f                  \n\t"
             "movl  $0xC0000001, %%eax  \n\t"
             "cpuid                     \n\t"
             "1:                        \n\t"
             "movl  %%edx, %1           \n\t"
             "movl  %2, %%ebx           \n\t"
             : "=m" (ebx), "=m" (edx)
             :  "m" (ebx)
             : "eax", "ecx", "edx");

        flags = edx;
    }

    return flags & feature;
}

/*
 * PadLock AES-ECB block en(de)cryption
 */
int mbedtls_padlock_xcryptecb(mbedtls_aes_context *ctx,
                              int mode,
                              const unsigned char input[16],
                              unsigned char output[16])
{
    int ebx = 0;
    uint32_t *rk;
    uint32_t *blk;
    uint32_t *ctrl;
    unsigned char buf[256];

    rk = ctx->buf + ctx->rk_offset;

    if (((long) rk & 15) != 0) {
        return MBEDTLS_ERR_PADLOCK_DATA_MISALIGNED;
    }

    blk = MBEDTLS_PADLOCK_ALIGN16(buf);
    memcpy(blk, input, 16);

    ctrl = blk + 4;
    *ctrl = 0x80 | ctx->nr | ((ctx->nr + (mode^1) - 10) << 9);

    asm ("pushfl                        \n\t"
         "popfl                         \n\t"
         "movl    %%ebx, %0             \n\t"
         "movl    $1, %%ecx             \n\t"
         "movl    %2, %%edx             \n\t"
         "movl    %3, %%ebx             \n\t"
         "movl    %4, %%esi             \n\t"
         "movl    %4, %%edi             \n\t"
         ".byte  0xf3,0x0f,0xa7,0xc8    \n\t"
         "movl    %1, %%ebx             \n\t"
         : "=m" (ebx)
         :  "m" (ebx), "m" (ctrl), "m" (rk), "m" (blk)
         : "memory", "ecx", "edx", "esi", "edi");

    memcpy(output, blk, 16);

    return 0;
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
/*
 * PadLock AES-CBC buffer en(de)cryption
 */
int mbedtls_padlock_xcryptcbc(mbedtls_aes_context *ctx,
                              int mode,
                              size_t length,
                              unsigned char iv[16],
                              const unsigned char *input,
                              unsigned char *output)
{
    int ebx = 0;
    size_t count;
    uint32_t *rk;
    uint32_t *iw;
    uint32_t *ctrl;
    unsigned char buf[256];

    rk = ctx->buf + ctx->rk_offset;

    if (((long) input  & 15) != 0 ||
        ((long) output & 15) != 0 ||
        ((long) rk & 15) != 0) {
        return MBEDTLS_ERR_PADLOCK_DATA_MISALIGNED;
    }

    iw = MBEDTLS_PADLOCK_ALIGN16(buf);
    memcpy(iw, iv, 16);

    ctrl = iw + 4;
    *ctrl = 0x80 | ctx->nr | ((ctx->nr + (mode ^ 1) - 10) << 9);

    count = (length + 15) >> 4;

    asm ("pushfl                        \n\t"
         "popfl                         \n\t"
         "movl    %%ebx, %0             \n\t"
         "movl    %2, %%ecx             \n\t"
         "movl    %3, %%edx             \n\t"
         "movl    %4, %%ebx             \n\t"
         "movl    %5, %%esi             \n\t"
         "movl    %6, %%edi             \n\t"
         "movl    %7, %%eax             \n\t"
         ".byte  0xf3,0x0f,0xa7,0xd0    \n\t"
         "movl    %1, %%ebx             \n\t"
         : "=m" (ebx)
         :  "m" (ebx), "m" (count), "m" (ctrl),
         "m"  (rk), "m" (input), "m" (output), "m" (iw)
         : "memory", "eax", "ecx", "edx", "esi", "edi");

    memcpy(iv, iw, 16);

    return 0;
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#endif /* MBEDTLS_VIA_PADLOCK_HAVE_CODE */

#endif /* MBEDTLS_PADLOCK_C */
