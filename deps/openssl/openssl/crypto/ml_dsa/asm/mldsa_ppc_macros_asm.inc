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

#define ML_DSA_Q 8380417   /* The modulus is 23 bits (2^23 - 2^13 + 1) */
#define ML_DSA_Q_NEG_INV 4236238847 /* Inverse of -q modulo 2^32 */

#define Q_NEG_INV    0
#define V_Q     1

#define GPR_OFFSET_C0 9
#define GPR_OFFSET_C1 11
#define GPR_OFFSET_C2 15
#define GPR_OFFSET_C3 17

#define GPR_OFFSET_P0 10
#define GPR_OFFSET_P1 12
#define GPR_OFFSET_P2 16
#define GPR_OFFSET_P3 18

/* ===================================================================== */
/*
 * For NTT implementation, initial peer and curr vectors setup as
 *  peer = V6, V7, V8, V9
 *  curr = V26, V27, V28, V29
 *
 * For Inverse NTT implementation, initial peer and curr vectors setup as
 *  peer = V18, V19, V20, V21
 *  curr = V14, V15, V16, V17
 */
/*
 * Load coefficients with 1-1-1-1 layout,
 *  each load contains 2 curr and 2 peer elements.
 *    -> curr1 peer1 curr2 peer2
 *       curr3 peer3 curr4 peer4
 *  vmrgew and vmrgow ->
 *       curr vector contains 4 curr elements and
 *       peer vector contains 4 peer elements
 *
 * In order to do the coefficients computation, zeta vector will arrange
 * in the proper order to match the multiplication.
 */
.macro Load_vmrg_coeffs _c1, _c2, _c3, _c4, _p1, _p2, _p3, _p4
        lxvd2x     32+10, GPR_OFFSET_C0, 5
        lxvd2x     32+11, GPR_OFFSET_P0, 5
        vmrgew \_p1, 10, 11
        vmrgow \_c1, 10, 11
        lxvd2x     32+12, GPR_OFFSET_C1, 5
        lxvd2x     32+13, GPR_OFFSET_P1, 5
        vmrgew \_p2, 12, 13
        vmrgow \_c2, 12, 13
        lxvd2x     32+10, GPR_OFFSET_C2, 5
        lxvd2x     32+11, GPR_OFFSET_P2, 5
        vmrgew \_p3, 10, 11
        vmrgow \_c3, 10, 11
        lxvd2x     32+12, GPR_OFFSET_C3, 5
        lxvd2x     32+13, GPR_OFFSET_P3, 5
        vmrgew \_p4, 12, 13
        vmrgow \_c4, 12, 13
.endm

/*
 * Load coefficients with 2-2 layout,
 *  each load contains 4 curr and 4 peer elements
 *    -> curr1 curr2 peer1 peer2
 *       curr3 curr3 pee43 peer4
 *  xxpermidi -> 4 curr elements and 4 peer elements
 *       curr vector contains 4 curr elements and
 *       peer vector contains 4 peer elements
 *
 * In order to do the coefficients computation, zeta vector will arrange
 * in the proper order to match the multiplication.
 */
.macro Load_xxperm_coeffs _c1, _c2, _c3, _c4, _p1, _p2, _p3, _p4
        lxvd2x     1, GPR_OFFSET_C0, 5
        lxvd2x     2, GPR_OFFSET_P0, 5
        xxpermdi 32+\_p1, 1, 2, 3
        xxpermdi 32+\_c1, 1, 2, 0
        lxvd2x     3, GPR_OFFSET_C1, 5
        lxvd2x     4, GPR_OFFSET_P1, 5
        xxpermdi 32+\_p2, 3, 4, 3
        xxpermdi 32+\_c2, 3, 4, 0
        lxvd2x     1, GPR_OFFSET_C2, 5
        lxvd2x     2, GPR_OFFSET_P2, 5
        xxpermdi 32+\_p3, 1, 2, 3
        xxpermdi 32+\_c3, 1, 2, 0
        lxvd2x     3, GPR_OFFSET_C3, 5
        lxvd2x     4, GPR_OFFSET_P3, 5
        xxpermdi 32+\_p4, 3, 4, 3
        xxpermdi 32+\_c4, 3, 4, 0
.endm

/*
 * Reduces x mod q in constant time
 * i.e. return x < q ? x : x - q;
 * @param x Where x is assumed to be in the range 0 <= x < 2*q
 * @returns the difference in the range 0..q-1
 */
.macro reduce_once_1x _vt
        vsubuwm  16, \_vt, V_Q
        vcmpgtsw 17, V_Q, \_vt
        xxsel   32+\_vt, 32+16, 32+\_vt, 32+17
.endm

/*
 * Calculate The positive value of (a-b) mod q in constant time.
 *
 * a - b mod q gives a value in the range -(q-1)..(q-1)
 * By adding q we get a range of 1..(2q-1).
 * Reducing this once then gives the range 0..q-1
 *
 * returns The value (q + a - b) mod q
 */
.macro mod_sub _x _a _b
        vadduwm 14, \_a, V_Q
        vsubuwm \_x, 14, \_b
        reduce_once_1x \_x
.endm

/* Delayed writes resulting curr and peer coefficients */
.macro delayed_writes_curr_peer _c1, _p1, _c2, _p2, _c3, _p3, _c4, _p4
        stxvd2x    32+\_c1, GPR_OFFSET_C0, 5
        stxvd2x    32+\_p1, GPR_OFFSET_P0, 5
        stxvd2x    32+\_c2, GPR_OFFSET_C1, 5
        stxvd2x    32+\_p2, GPR_OFFSET_P1, 5
        stxvd2x    32+\_c3, GPR_OFFSET_C2, 5
        stxvd2x    32+\_p3, GPR_OFFSET_P2, 5
        stxvd2x    32+\_c4, GPR_OFFSET_C3, 5
        stxvd2x    32+\_p4, GPR_OFFSET_P3, 5
.endm

.macro Load_4zetas _vz0, _vz1, _vz2, _vz3
        lvx     \_vz0, 0, 14
        addi    14, 14, 16
        lvx     \_vz1, 0, 14
        addi    14, 14, 16
        lvx     \_vz2, 0, 14
        addi    14, 14, 16
        lvx     \_vz3, 0, 14
        addi    14, 14, 16
.endm

/*
 * Montgomery multiply:
 *   a = zeta * peer or a = b * c
 *   Multiply of 2 numbers in montgomery form, in the range 0...(2^32)*q
 */
.macro Mont_scalar_mul_4x  _vz0 _vz1 _vz2 _vz3
        vmuleuw 10, 6, \_vz0
        vmulouw 11, 6, \_vz0
        vmuleuw 12, 7, \_vz1
        vmulouw 13, 7, \_vz1
        vmuleuw 14, 8, \_vz2
        vmulouw 15, 8, \_vz2
        vmuleuw 16, 9, \_vz3
        vmulouw 17, 9, \_vz3
.endm

/*
 * @brief When multiplying 2 numbers mod q that are in montgomery form, the
 * product mod q needs to be multiplied by 2^-32 to be in montgomery form.
 * See FIPS 204, Algorithm 49, MontgomeryReduce()
 * Note it is slightly different due to the input range being positive
 *
 * @param a is the result of a multiply of 2 numbers in montgomery form,
 *          in the range 0...(2^32)*q
 * @returns The Montgomery form of 'a' with multiplier 2^32 in the range 0..q-1
 *          The result is congruent to x * 2^-32 mod q
 */
.macro Mont_reduce_4x
        vmulouw 18, 10, Q_NEG_INV
        vmulouw 19, 11, Q_NEG_INV
        vmulouw 20, 12, Q_NEG_INV
        vmulouw 21, 13, Q_NEG_INV
        vmulouw 22, 14, Q_NEG_INV
        vmulouw 23, 15, Q_NEG_INV
        vmulouw 24, 16, Q_NEG_INV
        vmulouw 25, 17, Q_NEG_INV

        vmulouw 18, 18, V_Q
        vmulouw 19, 19, V_Q
        vmulouw 20, 20, V_Q
        vmulouw 21, 21, V_Q
        vmulouw 22, 22, V_Q
        vmulouw 23, 23, V_Q
        vmulouw 24, 24, V_Q
        vmulouw 25, 25, V_Q

        vaddudm 18, 18, 10
        vaddudm 19, 19, 11
        vaddudm 20, 20, 12
        vaddudm 21, 21, 13
        vaddudm 22, 22, 14
        vaddudm 23, 23, 15
        vaddudm 24, 24, 16
        vaddudm 25, 25, 17

        vmrgew  10, 18, 19
        vmrgew  11, 20, 21
        vmrgew  12, 22, 23
        vmrgew  13, 24, 25

        reduce_once_1x 10
        reduce_once_1x 11
        reduce_once_1x 12
        reduce_once_1x 13
.endm

/*
 * -----------------------------------
 * mont_mul_reduce_4x(_vz0, _vz1, _vz2, _vz3)
 */
.macro mont_mul_reduce_4x  _vz0 _vz1 _vz2 _vz3
        Mont_scalar_mul_4x \_vz0, \_vz1, \_vz2, \_vz3
        Mont_reduce_4x
.endm
/* ===================================================================== */
