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

#include "mlkem_ppc_macros_asm.inc"

.machine "any"
.text

#define ZETA_NTT_OFFSET 0

#define V_Z1  28
#define V_Z2  29
#define V_Z3  30
#define V_Z4  31
#define V_ZETA  31

.macro SAVE_REGS
        stdu    1, -336(1)
        mflr    0
        std     14, 56(1)
        std     15, 64(1)
        std     16, 72(1)
        std     17, 80(1)
        std     18, 88(1)

        li      10, 128
        li      11, 144
        li      12, 160
        li      14, 176
        li      15, 192
        li      16, 208
        stxvx   32+20, 10, 1
        stxvx   32+21, 11, 1
        stxvx   32+22, 12, 1
        stxvx   32+23, 14, 1
        stxvx   32+24, 15, 1
        stxvx   32+25, 16, 1
        li      10, 224
        li      11, 240
        li      12, 256
        li      14, 272
        li      15, 288
        li      16, 304
        stxvx   32+26, 10, 1
        stxvx   32+27, 11, 1
        stxvx   32+28, 12, 1
        stxvx   32+29, 14, 1
        stxvx   32+30, 15, 1
        stxvx   32+31, 16, 1
.endm

.macro RESTORE_REGS
        li      10, 128
        li      11, 144
        li      12, 160
        li      14, 176
        li      15, 192
        li      16, 208
        lxvx    32+20, 10, 1
        lxvx    32+21, 11, 1
        lxvx    32+22, 12, 1
        lxvx    32+23, 14, 1
        lxvx    32+24, 15, 1
        lxvx    32+25, 16, 1
        li      10, 224
        li      11, 240
        li      12, 256
        li      14, 272
        li      15, 288
        li      16, 304
        lxvx    32+26, 10, 1
        lxvx    32+27, 11, 1
        lxvx    32+28, 12, 1
        lxvx    32+29, 14, 1
        lxvx    32+30, 15, 1
        lxvx    32+31, 16, 1
        ld      14, 56(1)
        ld      15, 64(1)
        ld      16, 72(1)
        ld      17, 80(1)
        ld      18, 88(1)

        mtlr    0
        addi    1, 1, 336
.endm

/* ===================================================================== */
/*
 * NTT computation for layer 1, 2, 3, 4 and 5.
 */
/* Load 8 vectors with 4x pipeline
 *
 * Assuming input registers and output vectors as follows,
 *   input offsets - curr (0, r11, r15, r17), peer (r10, r12, r16, r18)
 *   output vectors - v20, v21, v22, v23, v24, v25, v26
 */
.macro _ntt_scalar_reduce_4x _zeta1, _zeta2, _zeta3, _zeta4
        /* Load peer */
        lxvd2x     32+14, 10, 5
        lxvd2x     32+15, 12, 5
        lxvd2x     32+16, 16, 5
        lxvd2x     32+17, 18, 5

        zeta_scalar_mul 14, \_zeta1, 20, 21       /* peer * zeta */
        zeta_scalar_mul 15, \_zeta2, 22, 23       /* peer * zeta */
        zeta_scalar_mul 16, \_zeta3, 24, 25       /* peer * zeta */
        zeta_scalar_mul 17, \_zeta4, 26, 27       /* peer * zeta */
        Barrett_reduce_delayed 20, 21, 14
        Barrett_reduce_delayed 22, 23, 15
        Barrett_reduce_delayed 24, 25, 16
        Barrett_reduce_delayed 26, 27, 17
        reduce_once_4x 14, 15, 16, 17

        /* Load curr */
        lxvd2x     32+10, 0, 5
        lxvd2x     32+11, 11, 5
        lxvd2x     32+12, 15, 5
        lxvd2x     32+13, 17, 5

        /* even - curr, odd - peer, odd is the updated peer */
        /* Compute (even - odd + kPrime) */
        vsubuhm  0, 10, 14
        vadduhm  21, 0, V_kPrime16
        vadduhm  20, 14, 10
        vsubuhm  0, 11, 15
        vadduhm  23, 0, V_kPrime16
        vadduhm  22, 15, 11
        vsubuhm  0, 12, 16
        vadduhm  25, 0, V_kPrime16
        vadduhm  24, 16, 12
        vsubuhm  0, 13, 17
        vadduhm  27, 0, V_kPrime16
        vadduhm  26, 17, 13
.endm

/* Delayed writes resulting curr and peer coefficients */
.macro delayed_writes_curr_peer _c1, _p1, _c2, _p2, _c3, _p3, _c4, _p4
        stxvd2x    32+\_c1, 0, 5
        stxvd2x    32+\_p1, 10, 5
        stxvd2x    32+\_c2, 11, 5
        stxvd2x    32+\_p2, 12, 5
        stxvd2x    32+\_c3, 15, 5
        stxvd2x    32+\_p3, 16, 5
        stxvd2x    32+\_c4, 17, 5
        stxvd2x    32+\_p4, 18, 5
.endm

.macro ntt_reduce_delayed_write
        _ntt_scalar_reduce_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        reduce_once_4x 20, 22, 24, 26
        reduce_once_4x 21, 23, 25, 27
        delayed_writes_curr_peer 20, 21, 22, 23, 24, 25, 26, 27
.endm

/*
 * NTT layer 1, Offset between legs = 128.
 */
.macro NTT_Layer1_4x
        ntt_reduce_delayed_write
        addi  5, 5, 64
.endm

/*
 * NTT layer 2, Offset between legs = 64.
 */
.macro NTT_Layer2_4x
        ntt_reduce_delayed_write
        addi  5, 5, 64
.endm

/*
 * NTT layer 3, Offset between legs = 32.
 */
.macro NTT_Layer3_4x
        lvx     V_ZETA, 0, 14
        addi  14, 14, 16
        ntt_reduce_delayed_write
        addi  5, 5, 128
.endm

/*
 * NTT layer 4, Offset between legs = 16.
 */
.macro NTT_Layer4_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        _ntt_scalar_reduce_4x V_Z1, V_Z1, V_Z2, V_Z2
        reduce_once_4x 20, 22, 24, 26
        reduce_once_4x 21, 23, 25, 27
        delayed_writes_curr_peer 20, 21, 22, 23, 24, 25, 26, 27
        addi  5, 5, 128
.endm

/*
 * NTT layer 5, Offset between legs = 8.
 */
.macro NTT_Layer5_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        lvx     V_Z3, 0, 14
        addi  14, 14, 16
        lvx     V_Z4, 0, 14
        addi  14, 14, 16
        _ntt_scalar_reduce_4x V_Z1, V_Z2, V_Z3, V_Z4
        reduce_once_4x 20, 22, 24, 26
        reduce_once_4x 21, 23, 25, 27
        delayed_writes_curr_peer 20, 21, 22, 23, 24, 25, 26, 27
        addi  5, 5, 128
.endm

/* ===================================================================== */
/* Layer 6 layout -
 *  each load contains 4 curr and 4 peer elements
 *  xxpermidi -> 8 curr elements and 8 peer elements
 */
.macro _load_transpose_layer6
        lxvd2x     4, 0, 5
        lxvd2x     5, 10, 5
        lxvd2x     6, 11, 5
        lxvd2x     7, 12, 5
        lxvd2x     8, 15, 5
        lxvd2x     9, 16, 5
        lxvd2x     10, 17, 5
        lxvd2x     11, 18, 5
        xxpermdi 32+14, 5, 4, 3
        xxpermdi     0, 5, 4, 0
        xxpermdi 32+15, 7, 6, 3
        xxpermdi     1, 7, 6, 0
        xxpermdi 32+16, 9, 8, 3
        xxpermdi     2, 9, 8, 0
        xxpermdi 32+17, 11, 10, 3
        xxpermdi     3, 11, 10, 0
.endm

/*
 * _layer_reduce_4x- common code for layer 6 and 7
 *
 * Assuming input registers and output vectors as follows,
 *   input offsets - curr (0, r11, r15, r17), peer (r10, r12, r16, r18)
 *   output vectors - v20, v21, v22, v23, v24, v25, v26
 */
.macro _layer_reduce_4x
        zeta_scalar_mul 14, V_Z1, 20, 21       /* peer * zeta */
        zeta_scalar_mul 15, V_Z2, 22, 23       /* peer * zeta */
        zeta_scalar_mul 16, V_Z3, 24, 25       /* peer * zeta */
        zeta_scalar_mul 17, V_Z4, 26, 27       /* peer * zeta */
        Barrett_reduce_delayed 20, 21, 14
        Barrett_reduce_delayed 22, 23, 15
        Barrett_reduce_delayed 24, 25, 16
        Barrett_reduce_delayed 26, 27, 17
        reduce_once_4x 14, 15, 16, 17

        /* Load curr */
        xxlor     32+10, 0, 0
        xxlor     32+11, 1, 1
        xxlor     32+12, 2, 2
        xxlor     32+13, 3, 3

        /* even - curr, odd - peer, odd is the updated peer */
        /* Compute (even - odd + kPrime) */
        vsubuhm  0, 10, 14
        vadduhm  21, 0, V_kPrime16
        vadduhm  20, 14, 10
        vsubuhm  0, 11, 15
        vadduhm  23, 0, V_kPrime16
        vadduhm  22, 15, 11
        vsubuhm  0, 12, 16
        vadduhm  25, 0, V_kPrime16
        vadduhm  24, 16, 12
        vsubuhm  0, 13, 17
        vadduhm  27, 0, V_kPrime16
        vadduhm  26, 17, 13
.endm

.macro _ntt_layer6_reduce_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        lvx     V_Z3, 0, 14
        addi  14, 14, 16
        lvx     V_Z4, 0, 14
        addi  14, 14, 16
        _load_transpose_layer6
        _layer_reduce_4x
.endm

/*
 * NTT layer 6,  Offset between legs = 4.
 */
.macro NTT_Layer6_4x
        _ntt_layer6_reduce_4x
        reduce_once_4x 20, 22, 24, 26
        reduce_once_4x 21, 23, 25, 27

        xxpermdi 32+6, 32+20, 32+21, 3
        xxpermdi 32+7, 32+20, 32+21, 0
        xxpermdi 32+8, 32+22, 32+23, 3
        xxpermdi 32+9, 32+22, 32+23, 0
        xxpermdi 32+10, 32+24, 32+25, 3
        xxpermdi 32+11, 32+24, 32+25, 0
        xxpermdi 32+12, 32+26, 32+27, 3
        xxpermdi 32+13, 32+26, 32+27, 0
        delayed_writes_curr_peer 6, 7, 8, 9, 10, 11, 12, 13
        addi  5, 5, 128
.endm

/* Layer 7 layout -
 *  each load contains 2 curr and 2 peer elements.
 *    -> curr1 curr1 peer1 peer1 curr2 curr2 peer2 peer2
 *  vmrgew and vmrgow -> 8 curr elements and 8 peer elements
 */
.macro _load_transpose_layer7
        lxvd2x     32+0, 0, 5
        lxvd2x     32+6, 10, 5
        lxvd2x     32+7, 11, 5
        lxvd2x     32+8, 12, 5
        lxvd2x     32+9, 15, 5
        lxvd2x     32+10, 16, 5
        lxvd2x     32+11, 17, 5
        lxvd2x     32+12, 18, 5
        vmrgew 14, 0, 6
        vmrgow  0, 0, 6
        vmrgew 15, 7, 8
        vmrgow  6, 7, 8
        vmrgew 16, 9, 10
        vmrgow  8, 9, 10
        vmrgew 17, 11, 12
        vmrgow 13, 11, 12
        xxlor  0, 32+0, 32+0
        xxlor  1, 32+6, 32+6
        xxlor  2, 32+8, 32+8
        xxlor  3, 32+13, 32+13
.endm

.macro _ntt_layer7_reduce_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        lvx     V_Z3, 0, 14
        addi  14, 14, 16
        lvx     V_Z4, 0, 14
        addi  14, 14, 16
        _load_transpose_layer7
        _layer_reduce_4x
.endm

/*
 * NTT layer 7,  Offset between legs = 2.
 */
.macro NTT_Layer7_4x
        _ntt_layer7_reduce_4x
        reduce_once_4x 20, 22, 24, 26
        reduce_once_4x 21, 23, 25, 27

        vmrgow  6, 21, 20
        vmrgew  7, 21, 20
        vmrgow  8, 23, 22
        vmrgew  9, 23, 22
        vmrgow  10, 25, 24
        vmrgew  11, 25, 24
        vmrgow  12, 27, 26
        vmrgew  13, 27, 26
        delayed_writes_curr_peer 7, 6, 9, 8, 11, 10, 13, 12
        addi  5, 5, 128
.endm

.macro Load_consts
        addis   10,2,ntt_consts@toc@ha
        addi    10,10,ntt_consts@toc@l
        lvx V_kPrime16, 0, 10
        li  7, 16
        lvx V_kPrime32, 7, 10
        li  7, 32
        lvx V_kBarrettMultiplier, 7, 10
        li  7, 48
        lvx V_kBarrettShift, 7, 10
        li  7, 64
        lvx V_kInverseDegree, 7, 10
.endm

/* ===================================================================== */
/*
 * mlkem_ntt_ppc(int16_t *r)
 *   Compute forward NTT based on the following 7 layers -
 *     len = 128, 64, 32, 16, 8, 4, 2.
 *
 *   Each layer compute the coefficients on 2 legs, start and start + len*2 offsets.
 *
 *   leg 1                        leg 2
 *   -----                        -----
 *   start                        start+len*2
 *   start+next                   start+len*2+next
 *   start+next+next              start+len*2+next+next
 *   start+next+next+next         start+len*2+next+next+next
 *
 *   The resulting coefficients then store back to each leg's offset.
 *
 *   Each vector has the same corresponding zeta except len=4 and len=2.
 *
 *   len=4 has 4-4 layout which means every 4 16-bit coefficients has the same zeta.
 *   and len=2 has 2-2-2-2 layout which means every 2 16-bit coefficients has the same zeta.
 *   e.g.
 *         coeff vector    a1   a2   a3  a4  a5  a6  a7  a8
 *         zeta  vector    z1   z1   z2  z2  z3  z3  z4  z4
 *
 *   For len=4 and len=2, each vector will get permuted to leg1 and leg2. Zeta is
 *   pre-arranged for the leg1 and leg2.  After the computation, each vector needs
 *   to transpose back to its original 4-4 or 2-2-2-2 layout.
 *
 */
.global mlkem_ntt_ppc
.align 4
mlkem_ntt_ppc:
        SAVE_REGS

        Load_consts
        addis   8,2,mlkem_ntt_zetas@toc@ha
        addi    8,8,mlkem_ntt_zetas@toc@l
        addi    14, 8, ZETA_NTT_OFFSET

.align 4
        /*
         * Layer 1. len = 128
         */
        mr      5, 3
        li      10, 256        // offset to next peer
        li      11, 16
        li      12, 256+16
        li      15, 32
        li      16, 256+32
        li      17, 48
        li      18, 256+48

        lvx     V_ZETA, 0, 14
        addi  14, 14, 16
        NTT_Layer1_4x
        NTT_Layer1_4x
        NTT_Layer1_4x
        NTT_Layer1_4x

.align 4
        /*
         * Layer 2. len = 64
         */
        mr      5, 3
        li      10, 128        // offset to next peer
        li      11, 16
        li      12, 128+16
        li      15, 32
        li      16, 128+32
        li      17, 48
        li      18, 128+48

        lvx     V_ZETA, 0, 14
        addi  14, 14, 16
        NTT_Layer2_4x
        NTT_Layer2_4x
        addi  5, 5, 128
        lvx     V_ZETA, 0, 14
        addi  14, 14, 16
        NTT_Layer2_4x
        NTT_Layer2_4x
        addi  5, 5, 128

.align 4
        /*
         * Layer 3. len = 32
         */
        mr      5, 3
        li      10, 64        // offset to next peer
        li      11, 16
        li      12, 64+16
        li      15, 32
        li      16, 64+32
        li      17, 48
        li      18, 64+48

        NTT_Layer3_4x
        NTT_Layer3_4x
        NTT_Layer3_4x
        NTT_Layer3_4x

.align 4
        /*
         * Layer 4. len = 16
         */
        mr      5, 3
        li      10, 32        // offset to next peer
        li      11, 16
        li      12, 32+16
        li      15, 64
        li      16, 64+32
        li      17, 64+16
        li      18, 96+16

        NTT_Layer4_4x
        NTT_Layer4_4x
        NTT_Layer4_4x
        NTT_Layer4_4x

.align 4
        /*
         * Layer 5. len = 8
         */
        mr      5, 3
        li      10, 16        // offset to next peer
        li      11, 32
        li      12, 48
        li      15, 64
        li      16, 80
        li      17, 96
        li      18, 112

        NTT_Layer5_4x
        NTT_Layer5_4x
        NTT_Layer5_4x
        NTT_Layer5_4x

.align 4
        /*
         * Layer 6. len = 4
         *    Load zeta vectors in 4-4 layout
         */
        mr      5, 3

        NTT_Layer6_4x
        NTT_Layer6_4x
        NTT_Layer6_4x
        NTT_Layer6_4x

.align 4
        /*
         * Layer 7. len = 2
         *    Load zeta vectors in 2-2-2-2 layout
         */
        mr      5, 3
        li      10, 16

        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x

        RESTORE_REGS
        blr
.size     mlkem_ntt_ppc,.-mlkem_ntt_ppc

.rodata
.align 4
ntt_consts:
.short kPrime, kPrime, kPrime, kPrime, kPrime, kPrime, kPrime, kPrime
.long  kPrime, kPrime, kPrime, kPrime
.long  kBarrettMultiplier, kBarrettMultiplier, kBarrettMultiplier, kBarrettMultiplier
.long  kBarrettShift, 0, kBarrettShift, 0
.short kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree , kInverseDegree

/* zetas */
mlkem_ntt_zetas:
/* For ntt len = 128, 64, 32, 16, 8 */
.short  1729, 1729, 1729, 1729, 1729, 1729, 1729, 1729
.short  2580, 2580, 2580, 2580, 2580, 2580, 2580, 2580
.short  3289, 3289, 3289, 3289, 3289, 3289, 3289, 3289
.short  2642, 2642, 2642, 2642, 2642, 2642, 2642, 2642
.short  630, 630, 630, 630, 630, 630, 630, 630
.short  1897, 1897, 1897, 1897, 1897, 1897, 1897, 1897
.short  848, 848, 848, 848, 848, 848, 848, 848
.short  1062, 1062, 1062, 1062, 1062, 1062, 1062, 1062
.short  1919, 1919, 1919, 1919, 1919, 1919, 1919, 1919
.short  193, 193, 193, 193, 193, 193, 193, 193
.short  797, 797, 797, 797, 797, 797, 797, 797
.short  2786, 2786, 2786, 2786, 2786, 2786, 2786, 2786
.short  3260, 3260, 3260, 3260, 3260, 3260, 3260, 3260
.short  569, 569, 569, 569, 569, 569, 569, 569
.short  1746, 1746, 1746, 1746, 1746, 1746, 1746, 1746
.short  296, 296, 296, 296, 296, 296, 296, 296
.short  2447, 2447, 2447, 2447, 2447, 2447, 2447, 2447
.short  1339, 1339, 1339, 1339, 1339, 1339, 1339, 1339
.short  1476, 1476, 1476, 1476, 1476, 1476, 1476, 1476
.short  3046, 3046, 3046, 3046, 3046, 3046, 3046, 3046
.short  56, 56, 56, 56, 56, 56, 56, 56
.short  2240, 2240, 2240, 2240, 2240, 2240, 2240, 2240
.short  1333, 1333, 1333, 1333, 1333, 1333, 1333, 1333
.short  1426, 1426, 1426, 1426, 1426, 1426, 1426, 1426
.short  2094, 2094, 2094, 2094, 2094, 2094, 2094, 2094
.short  535, 535, 535, 535, 535, 535, 535, 535
.short  2882, 2882, 2882, 2882, 2882, 2882, 2882, 2882
.short  2393, 2393, 2393, 2393, 2393, 2393, 2393, 2393
.short  2879, 2879, 2879, 2879, 2879, 2879, 2879, 2879
.short  1974, 1974, 1974, 1974, 1974, 1974, 1974, 1974
.short  821, 821, 821, 821, 821, 821, 821, 821
/* For ntt len = 4 */
.short  289, 289, 289, 289, 331, 331, 331, 331
.short  3253, 3253, 3253, 3253, 1756, 1756, 1756, 1756
.short  1197, 1197, 1197, 1197, 2304, 2304, 2304, 2304
.short  2277, 2277, 2277, 2277, 2055, 2055, 2055, 2055
.short  650, 650, 650, 650, 1977, 1977, 1977, 1977
.short  2513, 2513, 2513, 2513, 632, 632, 632, 632
.short  2865, 2865, 2865, 2865, 33, 33, 33, 33
.short  1320, 1320, 1320, 1320, 1915, 1915, 1915, 1915
.short  2319, 2319, 2319, 2319, 1435, 1435, 1435, 1435
.short  807, 807, 807, 807, 452, 452, 452, 452
.short  1438, 1438, 1438, 1438, 2868, 2868, 2868, 2868
.short  1534, 1534, 1534, 1534, 2402, 2402, 2402, 2402
.short  2647, 2647, 2647, 2647, 2617, 2617, 2617, 2617
.short  1481, 1481, 1481, 1481, 648, 648, 648, 648
.short  2474, 2474, 2474, 2474, 3110, 3110, 3110, 3110
.short  1227, 1227, 1227, 1227, 910, 910, 910, 910
/*
 * For ntt Len=2
 * reorder zeta array, (1, 2, 3, 4) -> (4, 2, 3, 1)
 * Transpose z[0], z[1], z[2], z[3]
 *    -> z[4], z[4], z[2], z[2], z[3], z[3], z[1], z[1]
 */
.short  2649, 2649, 2761, 2761, 583, 583, 17, 17
.short  1100, 1100, 723, 723, 2288, 2288, 1637, 1637
.short  233, 233, 2662, 2662, 3281, 3281, 1409, 1409
.short  3050, 3050, 2156, 2156, 3015, 3015, 756, 756
.short  1789, 1789, 1651, 1651, 2789, 2789, 1703, 1703
.short  2687, 2687, 952, 952, 1461, 1461, 1847, 1847
.short  2388, 2388, 2308, 2308, 2437, 2437, 939, 939
.short  641, 641, 2337, 2337, 268, 268, 733, 733
.short  3220, 3220, 2298, 2298, 2037, 2037, 1584, 1584
.short  1645, 1645, 2549, 2549, 2090, 2090, 375, 375
.short  757, 757, 319, 319, 2773, 2773, 1063, 1063
.short  2594, 2594, 561, 561, 2466, 2466, 2099, 2099
.short  1026, 1026, 1092, 1092, 403, 403, 2804, 2804
.short  886, 886, 2150, 2150, 2775, 2775, 1143, 1143
.short  1029, 1029, 1212, 1212, 1874, 1874, 1722, 1722
.short  2154, 2154, 2935, 2935, 885, 885, 2110, 2110
