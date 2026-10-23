/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
/*
 * Copyright IBM Corp. 2025, 2026
 *
 * ===================================================================================
 * Written by Danny Tsen <dtsen@us.ibm.com>
 */

/* ===================================================================== */
/* Barrett reduce vectors and constants */
#define V_kPrime16 1
#define V_kPrime32 2
#define V_kBarrettMultiplier 3
#define V_kBarrettShift 4
#define V_kInverseDegree 5

#define ML_KEM_PRIME 3329
#define kPrime 3329
#define kBarrettShift 24
#define kBarrettMultiplier 5039
#define kInverseDegree 3303

/*
 * reduce_once reduces 0 <= x < 2*kPrime, mod kPrime.
 *
 * Subtract |q| if the input is larger, without exposing a side-channel,
 * avoiding the "clangover" attack.  See |constish_time_non_zero| for a
 * discussion on why the value barrier is by default omitted.
 */
.macro reduce_once _x
        vsubuhm  6, \_x, V_kPrime16
        vcmpgtuh 7, V_kPrime16, 6
        xxsel    32+\_x, 32+\_x, 32+6, 32+7
.endm

/* reduce_once_4x: pipeline reduce_once in 4x */
.macro reduce_once_4x _x1, _x2, _x3, _x4
        vsubuhm  6, \_x1, V_kPrime16
        vsubuhm  8, \_x2, V_kPrime16
        vsubuhm  10, \_x3, V_kPrime16
        vsubuhm  12, \_x4, V_kPrime16
        vcmpgtuh 7, V_kPrime16, 6
        vcmpgtuh 9, V_kPrime16, 8
        vcmpgtuh 11, V_kPrime16, 10
        vcmpgtuh 13, V_kPrime16, 12
        xxsel    32+\_x1, 32+\_x1, 32+6, 32+7
        xxsel    32+\_x2, 32+\_x2, 32+8, 32+9
        xxsel    32+\_x3, 32+\_x3, 32+10, 32+11
        xxsel    32+\_x4, 32+\_x4, 32+12, 32+13
.endm

/* scalar multiplication = coeff * zeta */
.macro zeta_scalar_mul _coeff, _zeta, _x0, _x1
        vmulouh  6, \_coeff, \_zeta
        vmuleuh  7, \_coeff, \_zeta
        xxmrglw  32+\_x0, 32+7, 32+6
        xxmrghw  32+\_x1, 32+7, 32+6
.endm

/*
 * Constant-time reduce x mod kPrime using Barrett reduction. x must be less
 * than kPrime + 2 * kPrime^2.  This is sufficient to reduce a product of
 * two already reduced u_int16 values, in fact it is sufficient for each
 * to be less than 2^12, because (kPrime * (2 * kPrime + 1)) > 2^24.
 *
 * This macro handles 8 coefficients elements.
 * Input vectors: x0, x1
 * Output vector:  updated peer
 * Scratch vectors: v6, v7, v10, v11, v12, v13
 *
 * Barrett_reduce_delayed-
 *    We delay reduce_once in 4x pipeline in each layer
 */
.macro Barrett_reduce_delayed _x0, _x1, _updated_peer
        /* uint64_t product = (uint64_t)x * kBarrettMultiplier; */
        vmulouw  10, \_x0, V_kBarrettMultiplier
        vmuleuw  11, \_x0, V_kBarrettMultiplier
        vmulouw  12, \_x1, V_kBarrettMultiplier
        vmuleuw  13, \_x1, V_kBarrettMultiplier

        /* uint32_t quotient = (uint32_t)(product >> kBarrettShift); */
        vsrd 10, 10, V_kBarrettShift
        vsrd 11, 11, V_kBarrettShift
        vsrd 12, 12, V_kBarrettShift
        vsrd 13, 13, V_kBarrettShift
        vmrgow  6, 11, 10
        vmrgow  7, 13, 12

        /* uint32_t remainder = x - quotient * kPrime; */
        vmuluwm  10, 6, V_kPrime32
        vmuluwm  11, 7, V_kPrime32
        vsubuwm  6, \_x0, 10
        vsubuwm  7, \_x1, 11
        vpkuwus  \_updated_peer, 7, 6
.endm

.macro Barrett_reduce _x0, _x1, _updated_peer
        Barrett_reduce_delayed \_x0, \_x1, \_updated_peer
        reduce_once \_updated_peer
.endm

/*
 * Barrett_reduce_delayed_2x - 2 Barrett_reduce_delayed in parallel
 *   This is only 2x since we don't have enough vectors to support 4x pipeline.
 */
.macro Barrett_reduce_delayed_2x _x0, _x1, _y0, _y1, _updated_peer1, _updated_peer2
        /* uint64_t product = (uint64_t)x * kBarrettMultiplier; */
        vmulouw  10, \_x0, V_kBarrettMultiplier
        vmuleuw  11, \_x0, V_kBarrettMultiplier
        vmulouw  12, \_x1, V_kBarrettMultiplier
        vmuleuw  13, \_x1, V_kBarrettMultiplier

        vmulouw  14, \_y0, V_kBarrettMultiplier
        vmuleuw  15, \_y0, V_kBarrettMultiplier
        vmulouw  16, \_y1, V_kBarrettMultiplier
        vmuleuw  17, \_y1, V_kBarrettMultiplier

        /* uint32_t quotient = (uint32_t)(product >> kBarrettShift); */
        vsrd 10, 10, V_kBarrettShift
        vsrd 11, 11, V_kBarrettShift
        vsrd 12, 12, V_kBarrettShift
        vsrd 13, 13, V_kBarrettShift

        vsrd 14, 14, V_kBarrettShift
        vsrd 15, 15, V_kBarrettShift
        vsrd 16, 16, V_kBarrettShift
        vsrd 17, 17, V_kBarrettShift
         vmrgow  6, 11, 10
         vmrgow  7, 13, 12
         vmrgow  12, 15, 14
         vmrgow  13, 17, 16

        /* uint32_t remainder = x - quotient * kPrime; */
        vmuluwm  10, 6, V_kPrime32
        vmuluwm  11, 7, V_kPrime32
        vmuluwm  14, 12, V_kPrime32
        vmuluwm  15, 13, V_kPrime32
         vsubuwm  6, \_x0, 10
         vsubuwm  7, \_x1, 11
         vsubuwm  16, \_y0, 14
         vsubuwm  17, \_y1, 15
        vpkuwus  \_updated_peer1, 7, 6
        vpkuwus  \_updated_peer2, 17, 16
.endm
/* ===================================================================== */
