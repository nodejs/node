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

#define ZETA_INTT_OFFSET 0

#define PEER  17
#define CURR  18

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

/*
 * _intt_layer_reduce_4x- common code for layer 6 and 7
 *
 * Assuming input registers and output vectors as follows,
 *   input offsets - curr (0, r11, r15, r17), peer (r10, r12, r16, r18)
 *   output vectors - v20, v21, v22, v23, v24, v25, v26
 */
.macro _intt_layer_reduce_4x
        /* even - curr, odd - peer, odd is the updated peer */
        /* Compute (even - odd + kPrime) */
        vsubuhm  0, 10, 14
        vadduhm  20, 14, 10
        vadduhm  21, 0, V_kPrime16
        vsubuhm  0, 11, 15
        vadduhm  22, 15, 11
        vadduhm  23, 0, V_kPrime16
        vsubuhm  0, 12, 16
        vadduhm  24, 16, 12
        vadduhm  25, 0, V_kPrime16
        vsubuhm  0, 13, 17
        vadduhm  26, 17, 13
        vadduhm  27, 0, V_kPrime16
        zeta_scalar_mul 21, V_Z1, 8, 9       /* peer * zeta */
        zeta_scalar_mul 23, V_Z2, 18, 19       /* peer * zeta */
        Barrett_reduce_delayed_2x 8, 9, 18, 19, 21, 23
        zeta_scalar_mul 25, V_Z3, 8, 9       /* peer * zeta */
        zeta_scalar_mul 27, V_Z4, 18, 19       /* peer * zeta */
        Barrett_reduce_delayed_2x 8, 9, 18, 19, 25, 27
        reduce_once_4x 21, 23, 25, 27
.endm

/* Layer 1 layout -
 *  each load contains 2 curr and 2 peer elements.
 *    -> curr1 curr1 peer1 peer1 curr2 curr2 peer2 peer2
 *  vmrgew and vmrgow -> 8 curr elements and 8 peer elements
 */
/*
 * INTT layer 1,  Offset between legs = 2.
 */
.macro _load_transpose_layer1
        lxvd2x     32+0, 0, 5
        lxvd2x     32+6, 10, 5
        lxvd2x     32+7, 11, 5
        lxvd2x     32+8, 12, 5
        lxvd2x     32+9, 15, 5
        lxvd2x     32+10, 16, 5
        lxvd2x     32+11, 17, 5
        lxvd2x     32+12, 18, 5
        vmrgew 14, 0, 6
        vmrgew 15, 7, 8
        vmrgew 16, 9, 10
        vmrgew 17, 11, 12
        vmrgow 13, 11, 12
        vmrgow 12, 9, 10
        vmrgow 11, 7, 8
        vmrgow 10, 0, 6
.endm

.macro _intt_layer1_reduce_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        lvx     V_Z3, 0, 14
        addi  14, 14, 16
        lvx     V_Z4, 0, 14
        addi  14, 14, 16
        _load_transpose_layer1
        _intt_layer_reduce_4x
.endm

.macro INTT_Layer1_4x
        _intt_layer1_reduce_4x
        reduce_once_4x 20, 22, 24, 26
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

/* Layer 2 layout -
 *  each load contains 4 curr and 4 peer elements
 *  xxpermidi -> 8 curr elements and 8 peer elements
 */
/*
 * INTT layer 2,  Offset between legs = 4.
 */
.macro _load_transpose_layer2
        lxvd2x     4, 0, 5
        lxvd2x     5, 10, 5
        lxvd2x     6, 11, 5
        lxvd2x     7, 12, 5
        lxvd2x     8, 15, 5
        lxvd2x     9, 16, 5
        lxvd2x     10, 17, 5
        lxvd2x     11, 18, 5
        xxpermdi 32+14, 5, 4, 3
        xxpermdi 32+10, 5, 4, 0
        xxpermdi 32+15, 7, 6, 3
        xxpermdi 32+11, 7, 6, 0
        xxpermdi 32+16, 9, 8, 3
        xxpermdi 32+12, 9, 8, 0
        xxpermdi 32+17, 11, 10, 3
        xxpermdi 32+13, 11, 10, 0
.endm

.macro _intt_layer2_reduce_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        lvx     V_Z3, 0, 14
        addi  14, 14, 16
        lvx     V_Z4, 0, 14
        addi  14, 14, 16
        _load_transpose_layer2
        _intt_layer_reduce_4x
.endm

.macro INTT_Layer2_4x
        _intt_layer2_reduce_4x
        reduce_once_4x 20, 22, 24, 26
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

/* ===================================================================== */
/*
 * INTT computation for layer 3, 4, 5, 6 and 7.
 */

/* Load 8 vectors with 4x pipeline
 *
 * Assuming input registers and output vectors as follows,
 *   input offsets - curr (0, r11, r15, r17), peer (r10, r12, r16, r18)
 *   output vectors - v20, v21, v22, v23, v24, v25, v26
 */
.macro _intt_scalar_reduce_4x _zeta1, _zeta2, _zeta3, _zeta4
        /* Load peer */
        lxvd2x     32+14, 10, 5
        lxvd2x     32+15, 12, 5
        lxvd2x     32+16, 16, 5
        lxvd2x     32+17, 18, 5

        /* Load curr */
        lxvd2x     32+10, 0, 5
        lxvd2x     32+11, 11, 5
        lxvd2x     32+12, 15, 5
        lxvd2x     32+13, 17, 5

        /* even - curr, odd - peer, odd is the updated peer */
        /* Compute (even - odd + kPrime) */
        vsubuhm  0, 10, 14
        vadduhm  20, 14, 10
        vadduhm  21, 0, V_kPrime16
        vsubuhm  0, 11, 15
        vadduhm  22, 15, 11
        vadduhm  23, 0, V_kPrime16
        vsubuhm  0, 12, 16
        vadduhm  24, 16, 12
        vadduhm  25, 0, V_kPrime16
        vsubuhm  0, 13, 17
        vadduhm  26, 17, 13
        vadduhm  27, 0, V_kPrime16
        zeta_scalar_mul 21, \_zeta1, 8, 9       /* peer * zeta */
        zeta_scalar_mul 23, \_zeta2, 18, 19       /* peer * zeta */
        Barrett_reduce_delayed_2x 8, 9, 18, 19, 21, 23
        zeta_scalar_mul 25, \_zeta3, 8, 9       /* peer * zeta */
        zeta_scalar_mul 27, \_zeta4, 18, 19       /* peer * zeta */
        Barrett_reduce_delayed_2x 8, 9, 18, 19, 25, 27
        reduce_once_4x 21, 23, 25, 27
.endm

/*
 * INTT layer 3, Offset between legs = 8.
 */
.macro INTT_Layer3_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        lvx     V_Z3, 0, 14
        addi  14, 14, 16
        lvx     V_Z4, 0, 14
        addi  14, 14, 16
        _intt_scalar_reduce_4x V_Z1, V_Z2, V_Z3, V_Z4
        reduce_once_4x 20, 22, 24, 26
        delayed_writes_curr_peer 20, 21, 22, 23, 24, 25, 26, 27
        addi  5, 5, 128
.endm

/*
 * INTT layer 4, Offset between legs = 16.
 */
.macro INTT_Layer4_4x
        lvx     V_Z1, 0, 14
        addi  14, 14, 16
        lvx     V_Z2, 0, 14
        addi  14, 14, 16
        _intt_scalar_reduce_4x V_Z1, V_Z1, V_Z2, V_Z2
        reduce_once_4x 20, 22, 24, 26
        delayed_writes_curr_peer 20, 21, 22, 23, 24, 25, 26, 27
        addi  5, 5, 128
.endm

.macro intt_reduce_delayed_write
        _intt_scalar_reduce_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        reduce_once_4x 20, 22, 24, 26
        delayed_writes_curr_peer 20, 21, 22, 23, 24, 25, 26, 27
.endm

/*
 * INTT layer 5, Offset between legs = 32.
 */
.macro INTT_Layer5_4x
        lvx     V_ZETA, 0, 14
        addi  14, 14, 16
        intt_reduce_delayed_write
        addi  5, 5, 128
.endm

/*
 * INTT layer 6, Offset between legs = 64.
 */
.macro INTT_Layer6_4x
        intt_reduce_delayed_write
        addi  5, 5, 64
.endm

/*
 * INTT layer 7, Offset between legs = 128.
 */
.macro INTT_Layer7_4x
        intt_reduce_delayed_write
        addi  5, 5, 64
.endm

/* ===================================================================== */
/* Multiply a scalar by a constant (kInverseDegree=3303). */
.macro _mul_const_reduce_2x _x1, _x2
        vmulouh  6, \_x1, V_kInverseDegree
        vmuleuh  7, \_x1, V_kInverseDegree
        xxmrglw  32+28, 32+7, 32+6
        xxmrghw  32+29, 32+7, 32+6
        vmulouh  6, \_x2, V_kInverseDegree
        vmuleuh  7, \_x2, V_kInverseDegree
        xxmrglw  32+30, 32+7, 32+6
        xxmrghw  32+31, 32+7, 32+6
        Barrett_reduce_delayed_2x 28, 29, 30, 31, \_x1, \_x2
.endm

.macro scalar_mul_const_8x
        lxvd2x     32+20, 0, 5
        lxvd2x     32+21, 10, 5
        lxvd2x     32+22, 11, 5
        lxvd2x     32+23, 12, 5
        lxvd2x     32+24, 15, 5
        lxvd2x     32+25, 16, 5
        lxvd2x     32+26, 17, 5
        lxvd2x     32+27, 18, 5
        _mul_const_reduce_2x 20, 21
        _mul_const_reduce_2x 22, 23
        _mul_const_reduce_2x 24, 25
        _mul_const_reduce_2x 26, 27
        reduce_once_4x 20, 21, 22, 23
        reduce_once_4x 24, 25, 26, 27
        /* overload delayed_writes_curr_peer */
        delayed_writes_curr_peer 20, 21, 22, 23, 24, 25, 26, 27
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
 * mlkem_inverse_ntt_ppc(int16_t *r)
 *   Compute inverse NTT based on the following 7 layers -
 *     len = 2, 4, 8, 16, 32, 64, 128
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
 *   len=4 has 4-4 layout which means every 4 16-bit coefficients have the same zeta.
 *   and len=2 has 2-2-2-2 layout which means every 2 16-bit coefficients have the same zeta.
 *   e.g.
 *         coeff vector    a1   a2   a3  a4  a5  a6  a7  a8
 *         zeta  vector    z1   z1   z2  z2  z3  z3  z4  z4
 *
 *   For len=4 and len=2, each vector will get permuted to leg1 and leg2. Zeta is
 *   pre-arranged for the leg1 and leg2.  After the computation, each vector needs
 *   to transpose back to its original 4-4 or 2-2-2-2 layout.
 */
.global mlkem_inverse_ntt_ppc
.align 4
mlkem_inverse_ntt_ppc:

        SAVE_REGS

        Load_consts
        addis   8,2,mlkem_intt_zetas@toc@ha
        addi    8,8,mlkem_intt_zetas@toc@l
        addi    14, 8, ZETA_INTT_OFFSET

        li      10, 16        // offset to next peer
        li      11, 32
        li      12, 48
        li      15, 64
        li      16, 80
        li      17, 96
        li      18, 112

.align 4
        /*
         * Layer 1. len = 2
         *    Load zeta vectors in 2-2-2-2 layout
         */
        mr      5, 3

        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x

.align 4
        /*
         * Layer 22. len = 4
         *    Load zeta vectors in 4-4 layout
         */
        mr      5, 3

        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x

.align 4
        /*
         * Layer 3. len = 8
         */
        mr      5, 3

        INTT_Layer3_4x
        INTT_Layer3_4x
        INTT_Layer3_4x
        INTT_Layer3_4x

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

        INTT_Layer4_4x
        INTT_Layer4_4x
        INTT_Layer4_4x
        INTT_Layer4_4x

.align 4
        /*
         * Layer 5. len = 32
         */
        mr      5, 3
        li      10, 64        // offset to next peer
        li      11, 16
        li      12, 64+16
        li      15, 32
        li      16, 64+32
        li      17, 48
        li      18, 64+48

        INTT_Layer5_4x
        INTT_Layer5_4x
        INTT_Layer5_4x
        INTT_Layer5_4x

.align 4
        /*
         * Layer 6. len = 64
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
        INTT_Layer6_4x
        INTT_Layer6_4x
        addi  5, 5, 128
        lvx     V_ZETA, 0, 14
        addi  14, 14, 16
        INTT_Layer6_4x
        INTT_Layer6_4x
        addi  5, 5, 128

.align 4
        /*
         * Layer 7. len = 128
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
        INTT_Layer7_4x
        INTT_Layer7_4x
        INTT_Layer7_4x
        INTT_Layer7_4x

        /* Multiply a scalar by a constant. */
        mr      5, 3
        li      10, 16
        li      11, 32
        li      12, 48
        li      15, 64
        li      16, 80
        li      17, 96
        li      18, 112

        scalar_mul_const_8x
        scalar_mul_const_8x
        scalar_mul_const_8x
        scalar_mul_const_8x

        RESTORE_REGS
        blr
.size     mlkem_inverse_ntt_ppc,.-mlkem_inverse_ntt_ppc

.rodata
.align 4
ntt_consts:
.short kPrime, kPrime, kPrime, kPrime, kPrime, kPrime, kPrime, kPrime
.long  kPrime, kPrime, kPrime, kPrime
.long  kBarrettMultiplier, kBarrettMultiplier, kBarrettMultiplier, kBarrettMultiplier
.long  kBarrettShift, 0, kBarrettShift, 0
.short kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree, kInverseDegree

mlkem_intt_zetas:
/*
 * For intt Len=2
 * reorder zeta array, (1, 2, 3, 4) -> (4, 2, 3, 1)
 * Transpose z[0], z[1], z[2], z[3]
 *    -> z[4], z[4], z[2], z[2], z[3], z[3], z[1], z[1]
 */
.short  1219, 1219, 2444, 2444, 394, 394, 1175, 1175
.short  1607, 1607, 1455, 1455, 2117, 2117, 2300, 2300
.short  2186, 2186, 554, 554, 1179, 1179, 2443, 2443
.short  525, 525, 2926, 2926, 2237, 2237, 2303, 2303
.short  1230, 1230, 863, 863, 2768, 2768, 735, 735
.short  2266, 2266, 556, 556, 3010, 3010, 2572, 2572
.short  2954, 2954, 1239, 1239, 780, 780, 1684, 1684
.short  1745, 1745, 1292, 1292, 1031, 1031, 109, 109
.short  2596, 2596, 3061, 3061, 992, 992, 2688, 2688
.short  2390, 2390, 892, 892, 1021, 1021, 941, 941
.short  1482, 1482, 1868, 1868, 2377, 2377, 642, 642
.short  1626, 1626, 540, 540, 1678, 1678, 1540, 1540
.short  2573, 2573, 314, 314, 1173, 1173, 279, 279
.short  1920, 1920, 48, 48, 667, 667, 3096, 3096
.short  1692, 1692, 1041, 1041, 2606, 2606, 2229, 2229
.short  3312, 3312, 2746, 2746, 568, 568, 680, 680
/* For intt Len=4 */
.short  2419, 2419, 2419, 2419, 2102, 2102, 2102, 2102
.short  219, 219, 219, 219, 855, 855, 855, 855
.short  2681, 2681, 2681, 2681, 1848, 1848, 1848, 1848
.short  712, 712, 712, 712, 682, 682, 682, 682
.short  927, 927, 927, 927, 1795, 1795, 1795, 1795
.short  461, 461, 461, 461, 1891, 1891, 1891, 1891
.short  2877, 2877, 2877, 2877, 2522, 2522, 2522, 2522
.short  1894, 1894, 1894, 1894, 1010, 1010, 1010, 1010
.short  1414, 1414, 1414, 1414, 2009, 2009, 2009, 2009
.short  3296, 3296, 3296, 3296, 464, 464, 464, 464
.short  2697, 2697, 2697, 2697, 816, 816, 816, 816
.short  1352, 1352, 1352, 1352, 2679, 2679, 2679, 2679
.short  1274, 1274, 1274, 1274, 1052, 1052, 1052, 1052
.short  1025, 1025, 1025, 1025, 2132, 2132, 2132, 2132
.short  1573, 1573, 1573, 1573, 76, 76, 76, 76
.short  2998, 2998, 2998, 2998, 3040, 3040, 3040, 3040
/* For intt Len=8 and others */
.short  2508, 2508, 2508, 2508, 2508, 2508, 2508, 2508
.short  1355, 1355, 1355, 1355, 1355, 1355, 1355, 1355
.short  450, 450, 450, 450, 450, 450, 450, 450
.short  936, 936, 936, 936, 936, 936, 936, 936
.short  447, 447, 447, 447, 447, 447, 447, 447
.short  2794, 2794, 2794, 2794, 2794, 2794, 2794, 2794
.short  1235, 1235, 1235, 1235, 1235, 1235, 1235, 1235
.short  1903, 1903, 1903, 1903, 1903, 1903, 1903, 1903
.short  1996, 1996, 1996, 1996, 1996, 1996, 1996, 1996
.short  1089, 1089, 1089, 1089, 1089, 1089, 1089, 1089
.short  3273, 3273, 3273, 3273, 3273, 3273, 3273, 3273
.short  283, 283, 283, 283, 283, 283, 283, 283
.short  1853, 1853, 1853, 1853, 1853, 1853, 1853, 1853
.short  1990, 1990, 1990, 1990, 1990, 1990, 1990, 1990
.short  882, 882, 882, 882, 882, 882, 882, 882
.short  3033, 3033, 3033, 3033, 3033, 3033, 3033, 3033
.short  1583, 1583, 1583, 1583, 1583, 1583, 1583, 1583
.short  2760, 2760, 2760, 2760, 2760, 2760, 2760, 2760
.short  69, 69, 69, 69, 69, 69, 69, 69
.short  543, 543, 543, 543, 543, 543, 543, 543
.short  2532, 2532, 2532, 2532, 2532, 2532, 2532, 2532
.short  3136, 3136, 3136, 3136, 3136, 3136, 3136, 3136
.short  1410, 1410, 1410, 1410, 1410, 1410, 1410, 1410
.short  2267, 2267, 2267, 2267, 2267, 2267, 2267, 2267
.short  2481, 2481, 2481, 2481, 2481, 2481, 2481, 2481
.short  1432, 1432, 1432, 1432, 1432, 1432, 1432, 1432
.short  2699, 2699, 2699, 2699, 2699, 2699, 2699, 2699
.short  687, 687, 687, 687, 687, 687, 687, 687
.short  40, 40, 40, 40, 40, 40, 40, 40
.short  749, 749, 749, 749, 749, 749, 749, 749
.short  1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600
