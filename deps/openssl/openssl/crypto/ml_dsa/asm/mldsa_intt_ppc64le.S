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
 * Optimized inverse NTT implementation for ppc64le.
 */

#define Q_NEG_INV_OFFSET 16
#define FCONST_OFFSET 32
#define ZETA_INTT_OFFSET 48

#define V_F     2
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

/*
 * Compute updated curr and peer coefficients.
 *
 *  Input curr : V14, V15, V16, V17
 *  Input peer : V18, V19, V20, V21
 *
 *  updated curr: V26, V27, V28, V29
 *  updated peer: V6, V7, V8, V9
 */
.macro _Compute_4Coeff
        vadduwm 26, 14, 18
        vsubuwm 10, 14, 18
        vadduwm 6, 10, V_Q

        vadduwm 27, 15, 19
        vsubuwm 11, 15, 19
        vadduwm 7, 11, V_Q

        vadduwm 28, 16, 20
        vsubuwm 12, 16, 20
        vadduwm 8, 12, V_Q

        vadduwm 29, 17, 21
        vsubuwm 13, 17, 21
        vadduwm 9, 13, V_Q

        reduce_once_1x 26
        reduce_once_1x 27
        reduce_once_1x 28
        reduce_once_1x 29
.endm

/*
 * For Len=2,
 * Transpose the final coefficients of 2-2 layout to the original
 * coefficient array order.
 *
 *   updated peer : V15, V17, V19, V21
 *   updated curr : V14, V16, V18, V20
 */
.macro _delayed_writes_xxperm
        xxpermdi 32+14, 32+26, 32+10, 0
        xxpermdi 32+15, 32+26, 32+10, 3
        xxpermdi 32+16, 32+27, 32+11, 0
        xxpermdi 32+17, 32+27, 32+11, 3
        xxpermdi 32+18, 32+28, 32+12, 0
        xxpermdi 32+19, 32+28, 32+12, 3
        xxpermdi 32+20, 32+29, 32+13, 0
        xxpermdi 32+21, 32+29, 32+13, 3
        delayed_writes_curr_peer 14, 15, 16, 17, 18, 19, 20, 21
.endm

/*
 * For Len=1,
 * Transpose the final coefficients of 1-1-1-1 layout to the original
 * coefficient array order.
 *
 *   updated peer : V15, V17, V19, V21
 *   updated curr : V14, V16, V18, V20
 */
.macro _delayed_writes_vmrg
        vmrgew 14, 10, 26
        vmrgow 15, 10, 26
        vmrgew 16, 11, 27
        vmrgow 17, 11, 27
        vmrgew 18, 12, 28
        vmrgow 19, 12, 28
        vmrgew 20, 13, 29
        vmrgow 21, 13, 29
        delayed_writes_curr_peer 14, 15, 16, 17, 18, 19, 20, 21
.endm

/*
 * For Layer 1, Len= 1, layer with 1-1-1-1 layout.
 */
.macro INTT_Layer1_4x
        lxvd2x  32+V_Z0, 0, 14
        lxvd2x  32+V_Z1, 10, 14
        lxvd2x  32+V_Z2, 11, 14
        lxvd2x  32+V_Z3, 12, 14
        addi    14, 14, 64
        Load_vmrg_coeffs 14, 15, 16, 17, 18, 19, 20, 21
        _Compute_4Coeff
        mont_mul_reduce_4x V_Z0, V_Z1, V_Z2, V_Z3
        _delayed_writes_vmrg
        addi    5, 5, 128
.endm

/*
 * For Layer 2, Len= 2, layer with 2-2 layout.
 */
.macro INTT_Layer2_4x
        lxvd2x  32+V_Z0, 0, 14
        lxvd2x  32+V_Z1, 10, 14
        lxvd2x  32+V_Z2, 11, 14
        lxvd2x  32+V_Z3, 12, 14
        addi    14, 14, 64
        Load_xxperm_coeffs 14, 15, 16, 17, 18, 19, 20, 21
        _Compute_4Coeff
        mont_mul_reduce_4x V_Z0, V_Z1, V_Z2, V_Z3
        _delayed_writes_xxperm
        addi    5, 5, 128
.endm

.macro _load_curr_peer_coeffs
        lxvd2x  32+18, GPR_OFFSET_P0, 5
        lxvd2x  32+19, GPR_OFFSET_P1, 5
        lxvd2x  32+20, GPR_OFFSET_P2, 5
        lxvd2x  32+21, GPR_OFFSET_P3, 5

        lxvd2x  32+14, GPR_OFFSET_C0, 5
        lxvd2x  32+15, GPR_OFFSET_C1, 5
        lxvd2x  32+16, GPR_OFFSET_C2, 5
        lxvd2x  32+17, GPR_OFFSET_C3, 5
.endm

/*
 * For Layer 3, 4, 5, Len=4, 8, 16
 */
.macro INTT_Layer345_4x _vz0 _vz1 _vz2 _vz3
        _load_curr_peer_coeffs
        _Compute_4Coeff
        mont_mul_reduce_4x \_vz0, \_vz1, \_vz2, \_vz3
        delayed_writes_curr_peer 26, 10, 27, 11, 28, 12, 29, 13
        addi  5, 5, 128
.endm

/*
 * For Layer 6, 7, 8, Len=32, 64, 128
 */
.macro INTT_Layer678 _vz0 _vz1 _vz2 _vz3
        _load_curr_peer_coeffs
        _Compute_4Coeff
        mont_mul_reduce_4x \_vz0, \_vz1, \_vz2, \_vz3
        delayed_writes_curr_peer 26, 10, 27, 11, 28, 12, 29, 13
        addi  5, 5, 64
.endm

/* ===================================================================== */
/*
 * Supporting macros for Montgomery reduce loops with
 * constant f=41978 (mont^2/256).
 */
.macro _Reload_4coeffs
        lxvd2x  32+6, 0, 6
        lxvd2x  32+7, 10, 6
        lxvd2x  32+8, 11, 6
        lxvd2x  32+9, 12, 6
.endm

.macro _update_curr_coeffs_4x
        stxvd2x 32+10, 0, 6
        stxvd2x 32+11, 10, 6
        stxvd2x 32+12, 11, 6
        stxvd2x 32+13, 12, 6
        addi    6, 6, 64
.endm

.macro POLY_Mont_Reduce_4x
        _Reload_4coeffs
        mont_mul_reduce_4x V_F, V_F, V_F, V_F
        _update_curr_coeffs_4x

        _Reload_4coeffs
        mont_mul_reduce_4x V_F, V_F, V_F, V_F
        _update_curr_coeffs_4x

        _Reload_4coeffs
        mont_mul_reduce_4x V_F, V_F, V_F, V_F
        _update_curr_coeffs_4x

        _Reload_4coeffs
        mont_mul_reduce_4x V_F, V_F, V_F, V_F
        _update_curr_coeffs_4x
.endm
/* ===================================================================== */

/*
 * mldsa_poly_ntt_inverse_ppc(int32_t *r)
 *
 *   Compute Inverse NTT based on the following 8 layers -
 *     len = 1, 2, 4, 8, 16, 32, 64, 128.
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
.global mldsa_poly_ntt_inverse_ppc
.align 4
mldsa_poly_ntt_inverse_ppc:

        SAVE_REGS

        /* load Q and Q_NEG_INV */
        addis   8,2,.mldsa_consts@toc@ha
        addi    8,8,.mldsa_consts@toc@l
        lvx     V_Q, 0, 8
        li      10, Q_NEG_INV_OFFSET
        lvx     Q_NEG_INV, 10, 8

        /* set zetas array */
        addi    14, 8, ZETA_INTT_OFFSET

.align 4
        /*
         * Layer 1. len = 1
         */
        mr      5, 3

        li      GPR_OFFSET_C0, 0
        li      GPR_OFFSET_C1, 0x20
        li      GPR_OFFSET_C2, 0x40
        li      GPR_OFFSET_C3, 0x60

        li      GPR_OFFSET_P0, 0x10
        li      GPR_OFFSET_P1, 0x30
        li      GPR_OFFSET_P2, 0x50
        li      GPR_OFFSET_P3, 0x70

        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x
        INTT_Layer1_4x

.align 4
        /*
         * Layer 2. len = 2
         */
        mr      5, 3

        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x
        INTT_Layer2_4x

.align 4
        /*
         * Layer 3. len = 4
         */
        mr      5, 3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z1, V_Z2, V_Z3

.align 4
        /*
         * Layer 4. len = 8
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
        INTT_Layer345_4x V_Z0, V_Z0, V_Z1, V_Z1
        INTT_Layer345_4x V_Z2, V_Z2, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z0, V_Z1, V_Z1
        INTT_Layer345_4x V_Z2, V_Z2, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z0, V_Z1, V_Z1
        INTT_Layer345_4x V_Z2, V_Z2, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z0, V_Z1, V_Z1
        INTT_Layer345_4x V_Z2, V_Z2, V_Z3, V_Z3

.align 4
        /*
         * Layer 5. len = 16
         */
        mr      5, 3

        li      GPR_OFFSET_C1, 0x10
        li      GPR_OFFSET_C2, 0x20
        li      GPR_OFFSET_C3, 0x30

        li      GPR_OFFSET_P0, 0x40
        li      GPR_OFFSET_P1, 0x50
        li      GPR_OFFSET_P2, 0x60
        li      GPR_OFFSET_P3, 0x70

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z0, V_Z0, V_Z0
        INTT_Layer345_4x V_Z1, V_Z1, V_Z1, V_Z1
        INTT_Layer345_4x V_Z2, V_Z2, V_Z2, V_Z2
        INTT_Layer345_4x V_Z3, V_Z3, V_Z3, V_Z3

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer345_4x V_Z0, V_Z0, V_Z0, V_Z0
        INTT_Layer345_4x V_Z1, V_Z1, V_Z1, V_Z1
        INTT_Layer345_4x V_Z2, V_Z2, V_Z2, V_Z2
        INTT_Layer345_4x V_Z3, V_Z3, V_Z3, V_Z3

.align 4
        /*
         * Layer 6. len = 32
         */
        mr      5, 3

        li      GPR_OFFSET_P0, 0x80
        li      GPR_OFFSET_P1, 0x90
        li      GPR_OFFSET_P2, 0xa0
        li      GPR_OFFSET_P3, 0xb0

        Load_4zetas V_Z0, V_Z1, V_Z2, V_Z3
        INTT_Layer678 V_Z0, V_Z0, V_Z0, V_Z0
        INTT_Layer678 V_Z0, V_Z0, V_Z0, V_Z0
        addi  5, 5, 0x80

        INTT_Layer678 V_Z1, V_Z1, V_Z1, V_Z1
        INTT_Layer678 V_Z1, V_Z1, V_Z1, V_Z1
        addi  5, 5, 0x80

        INTT_Layer678 V_Z2, V_Z2, V_Z2, V_Z2
        INTT_Layer678 V_Z2, V_Z2, V_Z2, V_Z2
        addi  5, 5, 0x80

        INTT_Layer678 V_Z3, V_Z3, V_Z3, V_Z3
        INTT_Layer678 V_Z3, V_Z3, V_Z3, V_Z3

.align 4
        /*
         * Layer 7. len = 64
         */
        mr      5, 3

        li      GPR_OFFSET_P0, 0x100
        li      GPR_OFFSET_P1, 0x110
        li      GPR_OFFSET_P2, 0x120
        li      GPR_OFFSET_P3, 0x130

        lvx     V_ZETA, 0, 14
        addi    14, 14, 16
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        addi 5, 5, 0x100

        lvx     V_ZETA, 0, 14
        addi    14, 14, 16
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA

        /*
         * Layer 8. len = 128
         */
        mr      5, 3

        li      GPR_OFFSET_P0, 0x200
        li      GPR_OFFSET_P1, 0x210
        li      GPR_OFFSET_P2, 0x220
        li      GPR_OFFSET_P3, 0x230

        lvx     V_ZETA, 0, 14
        addi    14, 14, 16

        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA
        INTT_Layer678 V_ZETA, V_ZETA, V_ZETA, V_ZETA

        /*
         * Montgomery reduce loops with
         *   constant inverse_degree_montgomery f=41978 (mont^2/256)
         *
         *  updated coeff = montgomery_reduce((int64_t)f * coeff)
         */
        addi    10, 8, FCONST_OFFSET
        lvx     V_F, 0, 10

        li      10, 16
        li      11, 32
        li      12, 48

        mr      6, 3

        POLY_Mont_Reduce_4x
        POLY_Mont_Reduce_4x
        POLY_Mont_Reduce_4x
        POLY_Mont_Reduce_4x

        RESTORE_REGS
        blr
.size     mldsa_poly_ntt_inverse_ppc,.-mldsa_poly_ntt_inverse_ppc

.rodata
.align 4
.mldsa_consts:
.long ML_DSA_Q, ML_DSA_Q, ML_DSA_Q, ML_DSA_Q
.long ML_DSA_Q_NEG_INV, ML_DSA_Q_NEG_INV, ML_DSA_Q_NEG_INV, ML_DSA_Q_NEG_INV
/* Constant for INTT, f=mont^2/256 */
.long 41978, 41978, 41978, 41978

.mldsa_izetas:
/* Zetas for Lane=1: setup as (3, 2, 1, 4) order */
.long 6979993, 6403635, 4442679, 846154, 4460757, 1362209, 554416, 48306
.long 976891, 3545687, 8196974, 6767575, 2235985, 2286327, 2939036, 420899
.long 1104333, 3833893, 1667432, 260646, 6656817, 6470041, 426683, 1803090
.long 975884, 7908339, 6167306, 6662682, 4856520, 8110657, 3038916, 4513516
.long 6727783, 1799107, 7570268, 3694233, 8217573, 5366416, 3183426, 6764025
.long 5011305, 1207385, 6423145, 8194886, 5948022, 164721, 2013608, 5925962
.long 3724270, 3776993, 2584293, 7786281, 2831860, 1846953, 542412, 1671176
.long 7603226, 4974386, 6880252, 6144537, 6463336, 1374803, 1279661, 2546312
.long 7067962, 1962642, 451100, 5074302, 7143142, 1430225, 1333058, 3318210
.long 6511298, 1050970, 2994039, 6476982, 7129923, 3548272, 3767016, 5744496
.long 7132797, 6784443, 4325093, 5894064, 5688936, 7115408, 5538076, 2590150
.long 3342277, 8177373, 4943130, 6644538, 8093429, 4272102, 8038120, 2437823
.long 525098, 3595838, 3556995, 768622, 3122442, 5173371, 655327, 6348669
.long 1613174, 522500, 7884926, 43260, 6521319, 7561383, 7479715, 7470875
.long 3759364, 3193378, 3520352, 1197226, 5945978, 4867236, 8113420, 1235728
.long 6136326, 3562462, 3342478, 2446433, 4972711, 4562441, 6288750, 6063917
/* For Len=2 */
.long 4540456, 4540456, 3628969, 3628969, 3881060, 3881060, 3019102, 3019102
.long 1439742, 1439742, 812732, 812732, 1584928, 1584928, 7094748, 7094748
.long 7039087, 7039087, 7064828, 7064828, 177440, 177440, 2409325, 2409325
.long 1851402, 1851402, 5220671, 5220671, 3553272, 3553272, 8190869, 8190869
.long 1316856, 1316856, 7620448, 7620448, 210977, 210977, 5991061, 5991061
.long 3249728, 3249728, 6727353, 6727353, 8578, 8578, 3724342, 3724342
.long 4421799, 4421799, 7475901, 7475901, 1100098, 1100098, 8336129, 8336129
.long 5282425, 5282425, 7871466, 7871466, 8115473, 8115473, 3343383, 3343383
.long 1430430, 1430430, 6527646, 6527646, 7031341, 7031341, 381987, 381987
.long 1308169, 1308169, 22981, 22981, 1228525, 1228525, 671102, 671102
.long 2477047, 2477047, 411027, 411027, 3693493, 3693493, 2967645, 2967645
.long 5665122, 5665122, 6232521, 6232521, 983419, 983419, 4968207, 4968207
.long 8253495, 8253495, 3632928, 3632928, 3157330, 3157330, 3190144, 3190144
.long 1000202, 1000202, 4083598, 4083598, 6441103, 6441103, 1257611, 1257611
.long 1585221, 1585221, 6203962, 6203962, 4904467, 4904467, 1452451, 1452451
.long 3041255, 3041255, 3677745, 3677745, 1528703, 1528703, 3930395, 3930395
/*  For Lane=4 */
.long 2797779, 2797779, 2797779, 2797779, 6308525, 6308525, 6308525, 6308525
.long 2556880, 2556880, 2556880, 2556880, 4479693, 4479693, 4479693, 4479693
.long 4499374, 4499374, 4499374, 4499374, 7426187, 7426187, 7426187, 7426187
.long 7849063, 7849063, 7849063, 7849063, 7568473, 7568473, 7568473, 7568473
.long 4680821, 4680821, 4680821, 4680821, 1600420, 1600420, 1600420, 1600420
.long 2140649, 2140649, 2140649, 2140649, 4873154, 4873154, 4873154, 4873154
.long 3821735, 3821735, 3821735, 3821735, 4874723, 4874723, 4874723, 4874723
.long 1643818, 1643818, 1643818, 1643818, 1699267, 1699267, 1699267, 1699267
.long 539299, 539299, 539299, 539299, 6031717, 6031717, 6031717, 6031717
.long 300467, 300467, 300467, 300467, 4840449, 4840449, 4840449, 4840449
.long 2867647, 2867647, 2867647, 2867647, 4805995, 4805995, 4805995, 4805995
.long 3043716, 3043716, 3043716, 3043716, 3861115, 3861115, 3861115, 3861115
.long 4464978, 4464978, 4464978, 4464978, 2537516, 2537516, 2537516, 2537516
.long 3592148, 3592148, 3592148, 3592148, 1661693, 1661693, 1661693, 1661693
.long 4849980, 4849980, 4849980, 4849980, 5303092, 5303092, 5303092, 5303092
.long 8284641, 8284641, 8284641, 8284641, 5674394, 5674394, 5674394, 5674394
/* zetas for other len */
.long 8100412, 8100412, 8100412, 8100412, 4369920, 4369920, 4369920, 4369920
.long 19422, 19422, 19422, 19422, 6623180, 6623180, 6623180, 6623180
.long 3277672, 3277672, 3277672, 3277672, 1399561, 1399561, 1399561, 1399561
.long 3859737, 3859737, 3859737, 3859737, 2118186, 2118186, 2118186, 2118186
.long 2108549, 2108549, 2108549, 2108549, 5760665, 5760665, 5760665, 5760665
.long 1119584, 1119584, 1119584, 1119584, 549488, 549488, 549488, 549488
.long 4794489, 4794489, 4794489, 4794489, 1079900, 1079900, 1079900, 1079900
.long 7356305, 7356305, 7356305, 7356305, 5654953, 5654953, 5654953, 5654953
.long 5700314, 5700314, 5700314, 5700314, 5268920, 5268920, 5268920, 5268920
.long 2884855, 2884855, 2884855, 2884855, 5260684, 5260684, 5260684, 5260684
.long 2091905, 2091905, 2091905, 2091905, 359251, 359251, 359251, 359251
.long 6026966, 6026966, 6026966, 6026966, 6554070, 6554070, 6554070, 6554070
.long 7913949, 7913949, 7913949, 7913949, 876248, 876248, 876248, 876248
.long 777960, 777960, 777960, 777960, 8143293, 8143293, 8143293, 8143293
.long 518909, 518909, 518909, 518909, 2608894, 2608894, 2608894, 2608894
.long 8354570, 8354570, 8354570, 8354570
