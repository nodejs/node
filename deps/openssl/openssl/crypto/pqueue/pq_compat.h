/* crypto/pqueue/pqueue_compat.h */
/* 
 * DTLS implementation written by Nagendra Modadugu
 * (nagendra@cs.stanford.edu) for the OpenSSL project 2005.  
 */
/* ====================================================================
 * Copyright (c) 1999-2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#ifndef HEADER_PQ_COMPAT_H
#define HEADER_PQ_COMPAT_H

#include <openssl/opensslconf.h>
#include <openssl/bn.h>

/* 
 * The purpose of this header file is for supporting 64-bit integer
 * manipulation on 32-bit (and lower) machines.  Currently the only
 * such environment is VMS, Utrix and those with smaller default integer
 * sizes than 32 bits.  For all such environment, we fall back to using
 * BIGNUM.  We may need to fine tune the conditions for systems that
 * are incorrectly configured.
 *
 * The only clients of this code are (1) pqueue for priority, and
 * (2) DTLS, for sequence number manipulation.
 */

#if (defined(THIRTY_TWO_BIT) && !defined(BN_LLONG)) || defined(SIXTEEN_BIT) || defined(EIGHT_BIT)

#define PQ_64BIT_IS_INTEGER 0
#define PQ_64BIT_IS_BIGNUM 1

#define PQ_64BIT     BIGNUM
#define PQ_64BIT_CTX BN_CTX

#define pq_64bit_init(x)           BN_init(x)
#define pq_64bit_free(x)           BN_free(x)

#define pq_64bit_ctx_new(ctx)      BN_CTX_new()
#define pq_64bit_ctx_free(x)       BN_CTX_free(x)

#define pq_64bit_assign(x, y)      BN_copy(x, y)
#define pq_64bit_assign_word(x, y) BN_set_word(x, y)
#define pq_64bit_gt(x, y)          BN_ucmp(x, y) >= 1 ? 1 : 0
#define pq_64bit_eq(x, y)          BN_ucmp(x, y) == 0 ? 1 : 0
#define pq_64bit_add_word(x, w)    BN_add_word(x, w)
#define pq_64bit_sub(r, x, y)      BN_sub(r, x, y)
#define pq_64bit_sub_word(x, w)    BN_sub_word(x, w)
#define pq_64bit_mod(r, x, n, ctx) BN_mod(r, x, n, ctx)

#define pq_64bit_bin2num(bn, bytes, len)   BN_bin2bn(bytes, len, bn)
#define pq_64bit_num2bin(bn, bytes)        BN_bn2bin(bn, bytes)
#define pq_64bit_get_word(x)               BN_get_word(x)
#define pq_64bit_is_bit_set(x, offset)     BN_is_bit_set(x, offset)
#define pq_64bit_lshift(r, x, shift)       BN_lshift(r, x, shift)
#define pq_64bit_set_bit(x, num)           BN_set_bit(x, num)
#define pq_64bit_get_length(x)             BN_num_bits((x))

#else

#define PQ_64BIT_IS_INTEGER 1
#define PQ_64BIT_IS_BIGNUM 0

#if defined(SIXTY_FOUR_BIT)
#define PQ_64BIT BN_ULONG
#define PQ_64BIT_PRINT "%lld"
#elif defined(SIXTY_FOUR_BIT_LONG)
#define PQ_64BIT BN_ULONG
#define PQ_64BIT_PRINT "%ld"
#elif defined(THIRTY_TWO_BIT)
#define PQ_64BIT BN_ULLONG
#define PQ_64BIT_PRINT "%lld"
#endif

#define PQ_64BIT_CTX      void

#define pq_64bit_init(x)
#define pq_64bit_free(x)
#define pq_64bit_ctx_new(ctx)        (ctx)
#define pq_64bit_ctx_free(x)

#define pq_64bit_assign(x, y)        (*(x) = *(y))
#define pq_64bit_assign_word(x, y)   (*(x) = y)
#define pq_64bit_gt(x, y)	         (*(x) > *(y))
#define pq_64bit_eq(x, y)            (*(x) == *(y))
#define pq_64bit_add_word(x, w)      (*(x) = (*(x) + (w)))
#define pq_64bit_sub(r, x, y)        (*(r) = (*(x) - *(y)))
#define pq_64bit_sub_word(x, w)      (*(x) = (*(x) - (w)))
#define pq_64bit_mod(r, x, n, ctx)

#define pq_64bit_bin2num(num, bytes, len) bytes_to_long_long(bytes, num)
#define pq_64bit_num2bin(num, bytes)      long_long_to_bytes(num, bytes)
#define pq_64bit_get_word(x)              *(x)
#define pq_64bit_lshift(r, x, shift)      (*(r) = (*(x) << (shift)))
#define pq_64bit_set_bit(x, num)          do { \
                                              PQ_64BIT mask = 1; \
                                              mask = mask << (num); \
                                              *(x) |= mask; \
                                          } while(0)
#endif /* OPENSSL_SYS_VMS */

#endif
