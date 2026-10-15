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
 *
 * Optimized NTT implementation for ppc64le.
 */

#define Q_NEG_INV_OFFSET 16
#define ZETA_NTT_OFFSET 32

#define V_ZETA  2
#define V_Z0    2
#define V_Z1    3
#define V_Z2    4
#define V_Z3    5

#include "mldsa_ppc_macros_asm.inc"

.machine "any"
.text

.macro SAVE_REGS
        stdu    1, -352(1)
        mflr    0
        std     14, 56(1)
        std     15, 64(1)
        std     16, 72(1)
        std     17, 80(1)
        std     18, 88(1)
        std     19, 96(1)
        std     20, 104(1)
        std     21, 112(1)
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
        stxvx   32+26, 10, 1
        stxvx   32+27, 11, 1
        stxvx   32+28, 12, 1
        stxvx   32+29, 14, 1
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
        lxvx    32+26, 10, 1
        lxvx    32+27, 11, 1
        lxvx    32+28, 12, 1
        lxvx    32+29, 14, 1
        ld      14, 56(1)
        ld      15, 64(1)
        ld      16, 72(1)
        ld      17, 80(1)
        ld      18, 88(1)
        ld      19, 96(1)
        ld      20, 104(1)
        ld      21, 112(1)

        mtlr    0
        addi    1, 1, 352
.endm

/* ===================================================================== */
/*
 * Compute updated curr and peer coefficients.
 *   updated curr = reduce_once(curr + peer);
 *   updated peer = mod_sub(curr, peer);
 *
 *   Input curr : V26, V27, V28, V29
 *   Input peer : V10, V11, V12, V13
 *
 *   updated peer : V18, V19, V20, V21
 *   updated curr : V22, V23, V24, V25
 */
.macro _update_curr_peer_coeffs
        vadduwm 22, 26, 10
        mod_sub 18, 26, 10

        vadduwm 23, 27, 11
        mod_sub 19, 27, 11

        vadduwm 24, 28, 12
        mod_sub 20, 28, 12

        vadduwm 25, 29, 13
        mod_sub 21, 29, 13

        reduce_once_1x 22
        reduce_once_1x 23
        reduce_once_1x 24
        reduce_once_1x 25
.endm

/*
 * Transpose the final coefficients of 2-2 layout to the original
 * coefficient array order.
 *
 *   updated peer : V14, V15, V16, V17
 *   updated curr : V10, V11, V12, V13
 */
.macro _delayed_writes_xxperm
        xxpermdi 32+10, 32+22, 32+18, 0
        xxpermdi 32+14, 32+22, 32+18, 3
        xxpermdi 32+11, 32+23, 32+19, 0
        xxpermdi 32+15, 32+23, 32+19, 3
        xxpermdi 32+12, 32+24, 32+20, 0
        xxpermdi 32+16, 32+24, 32+20, 3
        xxpermdi 32+13, 32+25, 32+21, 0
        xxpermdi 32+17, 32+25, 32+21, 3
        delayed_writes_curr_peer 10, 14, 11, 15, 12, 16, 13, 17
.endm

/*
 * Transpose the final coefficients of 1-1-1-1 layout to the original
 * coefficient array order.
 *
 *   updated peer : V14, V15, V16, V17
 *   updated curr : V10, V11, V12, V13
 */
.macro _delayed_writes_vmrg
        vmrgew 10, 18, 22
        vmrgow 11, 18, 22
        vmrgew 12, 19, 23
        vmrgow 13, 19, 23
        vmrgew 14, 20, 24
        vmrgow 15, 20, 24
        vmrgew 16, 21, 25
        vmrgow 17, 21, 25
        delayed_writes_curr_peer 10, 11, 12, 13, 14, 15, 16, 17
.endm

/*
 * NTT computation for layers, 1, 2, 3, 4, 5, 6.
 */
.macro _ntt_body _vz0, _vz1, _vz2, _vz3
        lxvd2x  32+6, 5, GPR_OFFSET_P0
        lxvd2x  32+7, 5, GPR_OFFSET_P1
        lxvd2x  32+8, 5, GPR_OFFSET_P2
        lxvd2x  32+9, 5, GPR_OFFSET_P3
        mont_mul_reduce_4x \_vz0, \_vz1, \_vz2, \_vz3
        lxvd2x  32+26, 5, GPR_OFFSET_C0
        lxvd2x  32+27, 5, GPR_OFFSET_C1
        lxvd2x  32+28, 5, GPR_OFFSET_C2
        lxvd2x  32+29, 5, GPR_OFFSET_C3

        /* update curr and peer coefficients */
        _update_curr_peer_coeffs
.endm

/*
 * For Layer 1, 2, 3, Len=32, 64, 128
 */
.macro NTT_Layer123_4x _vz0, _vz1, _vz2, _vz3
        _ntt_body \_vz0, \_vz1, \_vz2, \_vz3
        delayed_writes_curr_peer 22, 18, 23, 19, 24, 20, 25, 21
        addi    5, 5, 64
.endm

/*
 * For Layer 4, 5, 6, Len=4, 8, 16
 */
.macro NTT_Layer456_4x _vz0, _vz1, _vz2, _vz3
        _ntt_body \_vz0, \_vz1, \_vz2, \_vz3
        delayed_writes_curr_peer 22, 18, 23, 19, 24, 20, 25, 21
        addi    5, 5, 128
.endm

/*
 * For Layer 7, Len= 2, layer with 2-2 layout.
 */
.macro NTT_Layer7_4x
        lxvd2x  32+V_Z0, 0, 14
        lxvd2x  32+V_Z1, 10, 14
        lxvd2x  32+V_Z2, 11, 14
        lxvd2x  32+V_Z3, 12, 14
        addi    14, 14, 64
        Load_xxperm_coeffs 26, 27, 28, 29, 6, 7, 8, 9
        mont_mul_reduce_4x V_Z0, V_Z1, V_Z2, V_Z3
        _update_curr_peer_coeffs
        _delayed_writes_xxperm
        addi    5, 5, 128
.endm

/*
 * For Layer 8, Len= 1, layer with 1-1-1-1 layout.
 */
.macro NTT_Layer8_4x
        lxvd2x  32+V_Z0, 0, 14
        lxvd2x  32+V_Z1, 10, 14
        lxvd2x  32+V_Z2, 11, 14
        lxvd2x  32+V_Z3, 12, 14
        addi    14, 14, 64
        Load_vmrg_coeffs 26, 27, 28, 29, 6, 7, 8, 9
        mont_mul_reduce_4x V_Z0, V_Z1, V_Z2, V_Z3
        _update_curr_peer_coeffs
        _delayed_writes_vmrg
        addi    5, 5, 128
.endm

/* ===================================================================== */
/*
 * mldsa_poly_ntt_ppc(int32_t *r)
 *   Compute forward NTT based on the following 8 layers -
 *     len = 128, 64, 32, 16, 8, 4, 2, 1.
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
 *   Each computation loads 8 vectors, 4 for each leg.
 *   The final coefficient (t) from each vector of leg1 and leg2 then do the
 *   add/sub operations to obtain the final results.
 *
 *   -> leg1 = leg1 + t, leg2 = leg1 - t
 *
 *   The resulting coefficients then store back to each leg's offset.
 *
 *   Each vector has the same corresponding zeta except len=2.
 *
 *   len=2 has 2-2 layout which means every 2 32-bit coefficients has the same zeta.
 *   e.g.
 *         coeff vector    a1   a2   a3  a4
 *         zeta  vector    z1   z1   z2  z2
 *
 *   For len=2, each vector will get permuted to leg1 and leg2. Zeta is
 *   pre-arranged for the leg1 and leg2.  After the computation, each vector needs
 *   to transpose back to its original 2-2 layout. 
 *
 *   For len=1, each vector will get permuted to leg1 and leg2. Zeta is
 *   pre-arranged for the leg1 and leg2.  After the computation, each vector needs
 *   to transpose back to its original 1-1-1-1 layout. 
 *
 * Registers used for offsets to coefficients, curr and peer.  Each layer may
 * reset these registers.
 *
 *   R9: offset to curr
 *   R11: offset to R9 + next curr offset
 *   R15: offset to R11 + next curr offset
 *   R17: offset to R15 + next curr offset
 *      
 *   R10: offset to peer
 *   R12: offset to R10 + next peer offset
 *   R16: offset to R12 + next peer offset
 *   R18: offset to R16 + next peer offset
 */
.global mldsa_poly_ntt_ppc
.align 4
mldsa_poly_ntt_ppc:

        SAVE_REGS
 
        /* load Q and Q_NEG_INV */
        addis   8,2,.mldsa_consts@toc@ha
        addi    8,8,.mldsa_consts@toc@l
        lvx     V_Q, 0, 8
        li      10, Q_NEG_INV_OFFSET
        lvx     Q_NEG_INV, 10, 8

        /* set zetas array */
        addi      14, 8, ZETA_NTT_OFFSET

        /*
         * 1. Layer 1, len = 128
         */
        mr      5, 3

        li      GPR_OFFSET_C0, 0
        li      GPR_OFFSET_C1, 0x10
        li      GPR_OFFSET_C2, 0x20
        li      GPR_OFFSET_C3, 0x30

        li      GPR_OFFSET_P0, 0x200
        li      GPR_OFFSET_P1, 0x210
        li      GPR_OFFSET_P2, 0x220
        li      GPR_OFFSET_P3, 0x230

        lvx     V_ZETA, 0, 14
        addi    14, 14, 16

        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA

.align 4
        /*
         * Layer 2. len = 64
         */
        mr      5, 3

        li      GPR_OFFSET_P0, 0x100
        li      GPR_OFFSET_P1, 0x110
        li      GPR_OFFSET_P2, 0x120
        li      GPR_OFFSET_P3, 0x130

        lvx     V_ZETA, 0, 14
        addi    14, 14, 16
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        addi  5, 5, 0x100

        lvx     V_ZETA, 0, 14
        addi    14, 14, 16
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA
        NTT_Layer123_4x V_ZETA, V_ZETA, V_ZETA, V_ZETA

.align 4
        /*
         * Layer 3. len = 32
         */
        mr      5, 3

        li      GPR_OFFSET_P0, 0x80
        li      GPR_OFFSET_P1, 0x90
        li      GPR_OFFSET_P2, 0xa0
        li      GPR_OFFSET_P3, 0xb0

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer123_4x V_Z0, V_Z0, V_Z0, V_Z0
        NTT_Layer123_4x V_Z0, V_Z0, V_Z0, V_Z0
        addi  5, 5, 0x80

        NTT_Layer123_4x V_Z1, V_Z1, V_Z1, V_Z1
        NTT_Layer123_4x V_Z1, V_Z1, V_Z1, V_Z1
        addi  5, 5, 0x80

        NTT_Layer123_4x V_Z2, V_Z2, V_Z2, V_Z2
        NTT_Layer123_4x V_Z2, V_Z2, V_Z2, V_Z2
        addi  5, 5, 0x80

        NTT_Layer123_4x V_Z3, V_Z3, V_Z3, V_Z3
        NTT_Layer123_4x V_Z3, V_Z3, V_Z3, V_Z3

.align 4
        /*
         * Layer 4. len = 16
         */
        mr      5, 3

        li      GPR_OFFSET_P0, 0x40
        li      GPR_OFFSET_P1, 0x50
        li      GPR_OFFSET_P2, 0x60
        li      GPR_OFFSET_P3, 0x70

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z0, V_Z0, V_Z0
        NTT_Layer456_4x V_Z1, V_Z1, V_Z1, V_Z1
        NTT_Layer456_4x V_Z2, V_Z2, V_Z2, V_Z2
        NTT_Layer456_4x V_Z3, V_Z3, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z0, V_Z0, V_Z0
        NTT_Layer456_4x V_Z1, V_Z1, V_Z1, V_Z1
        NTT_Layer456_4x V_Z2, V_Z2, V_Z2, V_Z2
        NTT_Layer456_4x V_Z3, V_Z3, V_Z3, V_Z3

.align 4
        /*
         * Layer 5. len = 8
         */
        mr      5, 3

        li      GPR_OFFSET_C1, 0x10
        li      GPR_OFFSET_C2, 0x40
        li      GPR_OFFSET_C3, 0x50

        li      GPR_OFFSET_P0, 0x20
        li      GPR_OFFSET_P1, 0x30
        li      GPR_OFFSET_P2, 0x60
        li      GPR_OFFSET_P3, 0x70

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z0, V_Z1, V_Z1
        NTT_Layer456_4x V_Z2, V_Z2, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z0, V_Z1, V_Z1
        NTT_Layer456_4x V_Z2, V_Z2, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z0, V_Z1, V_Z1
        NTT_Layer456_4x V_Z2, V_Z2, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z0, V_Z1, V_Z1
        NTT_Layer456_4x V_Z2, V_Z2, V_Z3, V_Z3

.align 4
        /*
         * Layer 6. len = 4
         */
        mr      5, 3

        li      GPR_OFFSET_C1, 0x20
        li      GPR_OFFSET_C2, 0x40
        li      GPR_OFFSET_C3, 0x60

        li      GPR_OFFSET_P0, 0x10
        li      GPR_OFFSET_P1, 0x30
        li      GPR_OFFSET_P2, 0x50
        li      GPR_OFFSET_P3, 0x70

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        NTT_Layer456_4x V_Z0, V_Z1, V_Z2, V_Z3

.align 4
        /*
         * Layer 7. len = 2
         *   Layer 7 uses the same offset as Layer 6.
         */
        mr      5, 3

        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x
        NTT_Layer7_4x

.align 4
        /*
         * Layer 8. len = 1
         *   Layer 8 uses the same offset as Layer 6.
         */
        mr      5, 3

        NTT_Layer8_4x
        NTT_Layer8_4x
        NTT_Layer8_4x
        NTT_Layer8_4x
        NTT_Layer8_4x
        NTT_Layer8_4x
        NTT_Layer8_4x
        NTT_Layer8_4x

        RESTORE_REGS
        blr
.size     mldsa_poly_ntt_ppc,.-mldsa_poly_ntt_ppc

/* ===================================================================== */
.macro POLY_SAVE_REGS
        stdu    1, -256(1)
        mflr    0
        li      9, 128
        li      10, 144
        li      11, 160
        li      12, 176
        stxvx   32+20, 9, 1
        stxvx   32+21, 10, 1
        stxvx   32+22, 11, 1
        stxvx   32+23, 12, 1
        li      9, 192
        li      10, 208
        stxvx   32+24, 9, 1
        stxvx   32+25, 10, 1
.endm

.macro POLY_RESTORE_REGS
        li      9, 128
        li      10, 144
        li      11, 160
        li      12, 176
        lxvx    32+20, 9, 1
        lxvx    32+21, 10, 1
        lxvx    32+22, 11, 1
        lxvx    32+23, 12, 1
        li      9, 192
        li      10, 208
        lxvx    32+24, 9, 1
        lxvx    32+25, 10, 1

        mtlr    0
        addi    1, 1, 256
.endm

.macro _Load_poly_points_vectors
        lxvd2x     32+2, 0, 4
        lxvd2x     32+3, 10, 4
        lxvd2x     32+4, 11, 4
        lxvd2x     32+5, 12, 4

        lxvd2x     32+6, 0, 5
        lxvd2x     32+7, 10, 5
        lxvd2x     32+8, 11, 5
        lxvd2x     32+9, 12, 5
        addi    4, 4, 64
        addi    5, 5, 64
.endm

.macro _update_poly_coeffs
        stxvd2x     32+10, 0, 3
        stxvd2x     32+11, 10, 3
        stxvd2x     32+12, 11, 3
        stxvd2x     32+13, 12, 3
        addi    3, 3, 64
.endm

.macro _poly_ntt_mul_4x
        _Load_poly_points_vectors
        //_Mont_point_mul
        Mont_scalar_mul_4x 2, 3, 4, 5
        Mont_reduce_4x
        _update_poly_coeffs
.endm

/*
 * mldsa_poly_ntt_mult(poly *c, const poly *a, const poly *b)
 */
.global mldsa_poly_ntt_mult_ppc
.align 4
mldsa_poly_ntt_mult_ppc:

        POLY_SAVE_REGS

        /* load Q and Q_NEG_INV */
        addis   8,2,.mldsa_consts@toc@ha
        addi    8,8,.mldsa_consts@toc@l
        lvx     V_Q, 0, 8
        li      10, 16
        lvx     Q_NEG_INV, 10, 8

        /*
         * Montgomery reduce loops for a * b
         *
         *  c[j] = montgomery_reduce(a[j] * b[j])
         */
        li       10, 16
        li       11, 32
        li       12, 48

        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x
        _poly_ntt_mul_4x

        POLY_RESTORE_REGS
        blr
.size     mldsa_poly_ntt_mult_ppc,.-mldsa_poly_ntt_mult_ppc
/* ===================================================================== */

.rodata
.align 4
.mldsa_consts:
.long ML_DSA_Q, ML_DSA_Q, ML_DSA_Q, ML_DSA_Q
.long ML_DSA_Q_NEG_INV, ML_DSA_Q_NEG_INV, ML_DSA_Q_NEG_INV, ML_DSA_Q_NEG_INV

/* zetas */
.mldsa_zetas:
/* For Len=128, 64, 32, 16, 8 */
.long 25847, 25847, 25847, 25847, 5771523, 5771523, 5771523, 5771523
.long 7861508, 7861508, 7861508, 7861508, 237124, 237124, 237124, 237124
.long 7602457, 7602457, 7602457, 7602457, 7504169, 7504169, 7504169, 7504169
.long 466468, 466468, 466468, 466468, 1826347, 1826347, 1826347, 1826347
.long 2353451, 2353451, 2353451, 2353451, 8021166, 8021166, 8021166, 8021166
.long 6288512, 6288512, 6288512, 6288512, 3119733, 3119733, 3119733, 3119733
.long 5495562, 5495562, 5495562, 5495562, 3111497, 3111497, 3111497, 3111497
.long 2680103, 2680103, 2680103, 2680103, 2725464, 2725464, 2725464, 2725464
.long 1024112, 1024112, 1024112, 1024112, 7300517, 7300517, 7300517, 7300517
.long 3585928, 3585928, 3585928, 3585928, 7830929, 7830929, 7830929, 7830929
.long 7260833, 7260833, 7260833, 7260833, 2619752, 2619752, 2619752, 2619752
.long 6271868, 6271868, 6271868, 6271868, 6262231, 6262231, 6262231, 6262231
.long 4520680, 4520680, 4520680, 4520680, 6980856, 6980856, 6980856, 6980856
.long 5102745, 5102745, 5102745, 5102745, 1757237, 1757237, 1757237, 1757237
.long 8360995, 8360995, 8360995, 8360995, 4010497, 4010497, 4010497, 4010497
.long 280005, 280005, 280005, 280005
/*For Len=4 */
.long 2706023, 2706023, 2706023, 2706023, 95776, 95776, 95776, 95776
.long 3077325, 3077325, 3077325, 3077325, 3530437, 3530437, 3530437, 3530437
.long 6718724, 6718724, 6718724, 6718724, 4788269, 4788269, 4788269, 4788269
.long 5842901, 5842901, 5842901, 5842901, 3915439, 3915439, 3915439, 3915439
.long 4519302, 4519302, 4519302, 4519302, 5336701, 5336701, 5336701, 5336701
.long 3574422, 3574422, 3574422, 3574422, 5512770, 5512770, 5512770, 5512770
.long 3539968, 3539968, 3539968, 3539968, 8079950, 8079950, 8079950, 8079950
.long 2348700, 2348700, 2348700, 2348700, 7841118, 7841118, 7841118, 7841118
.long 6681150, 6681150, 6681150, 6681150, 6736599, 6736599, 6736599, 6736599
.long 3505694, 3505694, 3505694, 3505694, 4558682, 4558682, 4558682, 4558682
.long 3507263, 3507263, 3507263, 3507263, 6239768, 6239768, 6239768, 6239768
.long 6779997, 6779997, 6779997, 6779997, 3699596, 3699596, 3699596, 3699596
.long 811944, 811944, 811944, 811944, 531354, 531354, 531354, 531354
.long 954230, 954230, 954230, 954230, 3881043, 3881043, 3881043, 3881043
.long 3900724, 3900724, 3900724, 3900724, 5823537, 5823537, 5823537, 5823537
.long 2071892, 2071892, 2071892, 2071892, 5582638, 5582638, 5582638, 5582638
/* For Len=2 */
.long 4450022, 4450022, 6851714, 6851714, 4702672, 4702672, 5339162, 5339162
.long 6927966, 6927966, 3475950, 3475950, 2176455, 2176455, 6795196, 6795196
.long 7122806, 7122806, 1939314, 1939314, 4296819, 4296819, 7380215, 7380215
.long 5190273, 5190273, 5223087, 5223087, 4747489, 4747489, 126922, 126922
.long 3412210, 3412210, 7396998, 7396998, 2147896, 2147896, 2715295, 2715295
.long 5412772, 5412772, 4686924, 4686924, 7969390, 7969390, 5903370, 5903370
.long 7709315, 7709315, 7151892, 7151892, 8357436, 8357436, 7072248, 7072248
.long 7998430, 7998430, 1349076, 1349076, 1852771, 1852771, 6949987, 6949987
.long 5037034, 5037034, 264944, 264944, 508951, 508951, 3097992, 3097992
.long 44288, 44288, 7280319, 7280319, 904516, 904516, 3958618, 3958618
.long 4656075, 4656075, 8371839, 8371839, 1653064, 1653064, 5130689, 5130689
.long 2389356, 2389356, 8169440, 8169440, 759969, 759969, 7063561, 7063561
.long 189548, 189548, 4827145, 4827145, 3159746, 3159746, 6529015, 6529015
.long 5971092, 5971092, 8202977, 8202977, 1315589, 1315589, 1341330, 1341330
.long 1285669, 1285669, 6795489, 6795489, 7567685, 7567685, 6940675, 6940675
.long 5361315, 5361315, 4499357, 4499357, 4751448, 4751448, 3839961, 3839961
/* Setup zetas for Len=1 as (3, 2, 1, 4) order */
.long 2316500, 2091667, 3817976, 3407706, 5933984, 5037939, 4817955, 2244091
.long 7144689, 266997, 3513181, 2434439, 7183191, 4860065, 5187039, 4621053
.long 909542, 900702, 819034, 1859098, 8337157, 495491, 7857917, 6767243
.long 2031748, 7725090, 3207046, 5257975, 7611795, 4823422, 4784579, 7855319
.long 5942594, 342297, 4108315, 286988, 1735879, 3437287, 203044, 5038140
.long 5790267, 2842341, 1265009, 2691481, 2486353, 4055324, 1595974, 1247620
.long 2635921, 4613401, 4832145, 1250494, 1903435, 5386378, 7329447, 1869119
.long 5062207, 7047359, 6950192, 1237275, 3306115, 7929317, 6417775, 1312455
.long 5834105, 7100756, 7005614, 1917081, 2235880, 1500165, 3406031, 777191
.long 6709241, 7838005, 6533464, 5548557, 594136, 5796124, 4603424, 4656147
.long 2454455, 6366809, 8215696, 2432395, 185531, 1957272, 7173032, 3369112
.long 1616392, 5196991, 3014001, 162844, 4686184, 810149, 6581310, 1652634
.long 3866901, 5341501, 269760, 3523897, 1717735, 2213111, 472078, 7404533
.long 6577327, 7953734, 1910376, 1723600, 8119771, 6712985, 4546524, 7276084
.long 7959518, 5441381, 6094090, 6144432, 1612842, 183443, 4834730, 7403526
.long 8332111, 7826001, 7018208, 3919660, 7534263, 3937738, 1976782, 1400424
