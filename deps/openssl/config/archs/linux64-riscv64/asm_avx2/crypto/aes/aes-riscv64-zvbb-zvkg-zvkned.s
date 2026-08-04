.text
.p2align 3
.globl rv64i_zvbb_zvkg_zvkned_aes_xts_encrypt
.type rv64i_zvbb_zvkg_zvkned_aes_xts_encrypt,@function
rv64i_zvbb_zvkg_zvkned_aes_xts_encrypt:
        # Load number of rounds
    lwu t0, 240(a4)
    .word 3439489111
    .word 34074119
    .word 34041479
    .word 2815667831
    addi t0, t0, -1
    addi a4, a4, 16
1:
    .word 34041479
    .word 2815503991
    addi t0, t0, -1
    addi a4, a4, 16
    bnez t0, 1b
    .word 34041479
    .word 2815536759


    # aes block size is 16
    andi a6, a2, 15
    mv t3, a2
    beqz a6, 1f
    sub a2, a2, a6
    addi t3, a2, -16
1:
    # We make the `LENGTH` become e32 length here.
    srli t4, a2, 2
    srli t3, t3, 2

    # Load number of rounds
    lwu t0, 240(a3)
    li t1, 14
    li t2, 10
    beq t0, t1, aes_xts_enc_256
    beq t0, t2, aes_xts_enc_128
.size rv64i_zvbb_zvkg_zvkned_aes_xts_encrypt,.-rv64i_zvbb_zvkg_zvkned_aes_xts_encrypt
.p2align 3
aes_xts_enc_128:
        # load input
    .word 221182167
    .word 33909767

    li t0, 5
    # We could simplify the initialization steps if we have `block<=1`.
    blt t4, t0, 1f

    # Note: We use `vgmul` for GF(2^128) multiplication. The `vgmul` uses
    # different order of coefficients. We should use`vbrev8` to reverse the
    # data when we use `vgmul`.
    .word 3439489111
    .word 1271144535
    .word 221179991
    .word 1577072727
    # v16: [r-IV0, r-IV0, ...]
    .word 2785257591

    # Prepare GF(2^128) multiplier [1, x, x^2, x^3, ...] in v8.
    slli t0, t4, 2
    .word 218296407
    # v2: [`1`, `1`, `1`, `1`, ...]
    .word 1577103703
    # v3: [`0`, `1`, `2`, `3`, ...]
    .word 1376297431
    .word 227733591
    # v4: [`1`, 0, `1`, 0, `1`, 0, `1`, 0, ...]
    .word 1243816535
    # v6: [`0`, 0, `1`, 0, `2`, 0, `3`, 0, ...]
    .word 1244865367
    slli t0, t4, 1
    .word 219344983
    # v8: [1<<0=1, 0, 0, 0, 1<<1=x, 0, 0, 0, 1<<2=x^2, 0, 0, 0, ...]
    .word 3594716247

    # Compute [r-IV0*1, r-IV0*x, r-IV0*x^2, r-IV0*x^3, ...] in v16
    .word 221179991
    .word 1250174039
    .word 2726865015

    # Compute [IV0*1, IV0*x, IV0*x^2, IV0*x^3, ...] in v28.
    # Reverse the bits order back.
    .word 1258565207

    # Prepare the x^n multiplier in v20. The `n` is the aes-xts block number
    # in a LMUL=4 register group.
    #   n = ((VLEN*LMUL)/(32*4)) = ((VLEN*4)/(32*4))
    #     = (VLEN/32)
    # We could use vsetvli with `e32, m1` to compute the `n` number.
    .word 218133207
    li t1, 1
    sll t0, t1, t0
    .word 3447812183
    .word 1577070679
    .word 3380670551
    .word 1577238615
    .word 3447812183
    .word 1241784407
    .word 221179991
    .word 1577073239
    .word 2785258103

    j 2f
1:
    .word 3439489111
    .word 1271146583
2:

        .word 3439489111
    .word 34005127
    addi a3, a3, 16
    .word 34005255
    addi a3, a3, 16
    .word 34005383
    addi a3, a3, 16
    .word 34005511
    addi a3, a3, 16
    .word 34005639
    addi a3, a3, 16
    .word 34005767
    addi a3, a3, 16
    .word 34005895
    addi a3, a3, 16
    .word 34006023
    addi a3, a3, 16
    .word 34006151
    addi a3, a3, 16
    .word 34006279
    addi a3, a3, 16
    .word 34006407


    .word 221182167
    j 1f

.Lenc_blocks_128:
    .word 221182167
    # load plaintext into v24
    .word 33909767
    # update iv
    .word 2739447927
    # reverse the iv's bits order back
    .word 1258565207
1:
    .word 797838423
    slli t0, a7, 2
    sub t4, t4, a7
    add a0, a0, t0
        .word 2786307191
    .word 2787191927
    .word 2788240503
    .word 2789289079
    .word 2790337655
    .word 2791386231
    .word 2792434807
    .word 2793483383
    .word 2794531959
    .word 2795580535
    .word 2796661879

    .word 797838423

    # store ciphertext
    .word 221147223
    .word 33942567
    add a1, a1, t0
    sub t3, t3, a7

    bnez t4, .Lenc_blocks_128

        bnez a6, 1f
    ret
1:
    # slidedown second to last block
    addi a7, a7, -4
    .word 3441586263
    # ciphertext
    .word 1065929815
    # multiplier
    .word 1057540183

    .word 3439489111
    .word 1577848023

    # load last block into v24
    # note: We should load the last block before store the second to last block
    #       for in-place operation.
    .word 134770775
    .word 33885191

    # setup `x` multiplier with byte-reversed order
    # 0b00000010 => 0b01000000 (0x40)
    li t0, 0x40
    .word 3439489111
    .word 1577074263
    .word 3355504727
    .word 1577242199

    # compute IV for last block
    .word 3439489111
    .word 2747836535
    .word 1258565207

    # store second to last block
    .word 201879639
    .word 33918119


    # xts last block
    .word 3439489111
    .word 797838423
        .word 2786307191
    .word 2787191927
    .word 2788240503
    .word 2789289079
    .word 2790337655
    .word 2791386231
    .word 2792434807
    .word 2793483383
    .word 2794531959
    .word 2795580535
    .word 2796661879

    .word 797838423

    # store last block ciphertext
    addi a1, a1, -16
    .word 33942567

    ret
.size aes_xts_enc_128,.-aes_xts_enc_128
.p2align 3
aes_xts_enc_256:
        # load input
    .word 221182167
    .word 33909767

    li t0, 5
    # We could simplify the initialization steps if we have `block<=1`.
    blt t4, t0, 1f

    # Note: We use `vgmul` for GF(2^128) multiplication. The `vgmul` uses
    # different order of coefficients. We should use`vbrev8` to reverse the
    # data when we use `vgmul`.
    .word 3439489111
    .word 1271144535
    .word 221179991
    .word 1577072727
    # v16: [r-IV0, r-IV0, ...]
    .word 2785257591

    # Prepare GF(2^128) multiplier [1, x, x^2, x^3, ...] in v8.
    slli t0, t4, 2
    .word 218296407
    # v2: [`1`, `1`, `1`, `1`, ...]
    .word 1577103703
    # v3: [`0`, `1`, `2`, `3`, ...]
    .word 1376297431
    .word 227733591
    # v4: [`1`, 0, `1`, 0, `1`, 0, `1`, 0, ...]
    .word 1243816535
    # v6: [`0`, 0, `1`, 0, `2`, 0, `3`, 0, ...]
    .word 1244865367
    slli t0, t4, 1
    .word 219344983
    # v8: [1<<0=1, 0, 0, 0, 1<<1=x, 0, 0, 0, 1<<2=x^2, 0, 0, 0, ...]
    .word 3594716247

    # Compute [r-IV0*1, r-IV0*x, r-IV0*x^2, r-IV0*x^3, ...] in v16
    .word 221179991
    .word 1250174039
    .word 2726865015

    # Compute [IV0*1, IV0*x, IV0*x^2, IV0*x^3, ...] in v28.
    # Reverse the bits order back.
    .word 1258565207

    # Prepare the x^n multiplier in v20. The `n` is the aes-xts block number
    # in a LMUL=4 register group.
    #   n = ((VLEN*LMUL)/(32*4)) = ((VLEN*4)/(32*4))
    #     = (VLEN/32)
    # We could use vsetvli with `e32, m1` to compute the `n` number.
    .word 218133207
    li t1, 1
    sll t0, t1, t0
    .word 3447812183
    .word 1577070679
    .word 3380670551
    .word 1577238615
    .word 3447812183
    .word 1241784407
    .word 221179991
    .word 1577073239
    .word 2785258103

    j 2f
1:
    .word 3439489111
    .word 1271146583
2:

        .word 3439489111
    .word 34005127
    addi a3, a3, 16
    .word 34005255
    addi a3, a3, 16
    .word 34005383
    addi a3, a3, 16
    .word 34005511
    addi a3, a3, 16
    .word 34005639
    addi a3, a3, 16
    .word 34005767
    addi a3, a3, 16
    .word 34005895
    addi a3, a3, 16
    .word 34006023
    addi a3, a3, 16
    .word 34006151
    addi a3, a3, 16
    .word 34006279
    addi a3, a3, 16
    .word 34006407
    addi a3, a3, 16
    .word 34006535
    addi a3, a3, 16
    .word 34006663
    addi a3, a3, 16
    .word 34006791
    addi a3, a3, 16
    .word 34006919


    .word 221182167
    j 1f

.Lenc_blocks_256:
    .word 221182167
    # load plaintext into v24
    .word 33909767
    # update iv
    .word 2739447927
    # reverse the iv's bits order back
    .word 1258565207
1:
    .word 797838423
    slli t0, a7, 2
    sub t4, t4, a7
    add a0, a0, t0
        .word 2786307191
    .word 2787191927
    .word 2788240503
    .word 2789289079
    .word 2790337655
    .word 2791386231
    .word 2792434807
    .word 2793483383
    .word 2794531959
    .word 2795580535
    .word 2796629111
    .word 2797677687
    .word 2798726263
    .word 2799774839
    .word 2800856183

    .word 797838423

    # store ciphertext
    .word 221147223
    .word 33942567
    add a1, a1, t0
    sub t3, t3, a7

    bnez t4, .Lenc_blocks_256

        bnez a6, 1f
    ret
1:
    # slidedown second to last block
    addi a7, a7, -4
    .word 3441586263
    # ciphertext
    .word 1065929815
    # multiplier
    .word 1057540183

    .word 3439489111
    .word 1577848023

    # load last block into v24
    # note: We should load the last block before store the second to last block
    #       for in-place operation.
    .word 134770775
    .word 33885191

    # setup `x` multiplier with byte-reversed order
    # 0b00000010 => 0b01000000 (0x40)
    li t0, 0x40
    .word 3439489111
    .word 1577074263
    .word 3355504727
    .word 1577242199

    # compute IV for last block
    .word 3439489111
    .word 2747836535
    .word 1258565207

    # store second to last block
    .word 201879639
    .word 33918119


    # xts last block
    .word 3439489111
    .word 797838423
        .word 2786307191
    .word 2787191927
    .word 2788240503
    .word 2789289079
    .word 2790337655
    .word 2791386231
    .word 2792434807
    .word 2793483383
    .word 2794531959
    .word 2795580535
    .word 2796629111
    .word 2797677687
    .word 2798726263
    .word 2799774839
    .word 2800856183

    .word 797838423

    # store last block ciphertext
    addi a1, a1, -16
    .word 33942567

    ret
.size aes_xts_enc_256,.-aes_xts_enc_256
.p2align 3
.globl rv64i_zvbb_zvkg_zvkned_aes_xts_decrypt
.type rv64i_zvbb_zvkg_zvkned_aes_xts_decrypt,@function
rv64i_zvbb_zvkg_zvkned_aes_xts_decrypt:
        # Load number of rounds
    lwu t0, 240(a4)
    .word 3439489111
    .word 34074119
    .word 34041479
    .word 2815667831
    addi t0, t0, -1
    addi a4, a4, 16
1:
    .word 34041479
    .word 2815503991
    addi t0, t0, -1
    addi a4, a4, 16
    bnez t0, 1b
    .word 34041479
    .word 2815536759


    # aes block size is 16
    andi a6, a2, 15
    beqz a6, 1f
    sub a2, a2, a6
    addi a2, a2, -16
1:
    # We make the `LENGTH` become e32 length here.
    srli t4, a2, 2

    # Load number of rounds
    lwu t0, 240(a3)
    li t1, 14
    li t2, 10
    beq t0, t1, aes_xts_dec_256
    beq t0, t2, aes_xts_dec_128
.size rv64i_zvbb_zvkg_zvkned_aes_xts_decrypt,.-rv64i_zvbb_zvkg_zvkned_aes_xts_decrypt
.p2align 3
aes_xts_dec_128:
        # load input
    .word 221182167
    .word 33909767

    li t0, 5
    # We could simplify the initialization steps if we have `block<=1`.
    blt t4, t0, 1f

    # Note: We use `vgmul` for GF(2^128) multiplication. The `vgmul` uses
    # different order of coefficients. We should use`vbrev8` to reverse the
    # data when we use `vgmul`.
    .word 3439489111
    .word 1271144535
    .word 221179991
    .word 1577072727
    # v16: [r-IV0, r-IV0, ...]
    .word 2785257591

    # Prepare GF(2^128) multiplier [1, x, x^2, x^3, ...] in v8.
    slli t0, t4, 2
    .word 218296407
    # v2: [`1`, `1`, `1`, `1`, ...]
    .word 1577103703
    # v3: [`0`, `1`, `2`, `3`, ...]
    .word 1376297431
    .word 227733591
    # v4: [`1`, 0, `1`, 0, `1`, 0, `1`, 0, ...]
    .word 1243816535
    # v6: [`0`, 0, `1`, 0, `2`, 0, `3`, 0, ...]
    .word 1244865367
    slli t0, t4, 1
    .word 219344983
    # v8: [1<<0=1, 0, 0, 0, 1<<1=x, 0, 0, 0, 1<<2=x^2, 0, 0, 0, ...]
    .word 3594716247

    # Compute [r-IV0*1, r-IV0*x, r-IV0*x^2, r-IV0*x^3, ...] in v16
    .word 221179991
    .word 1250174039
    .word 2726865015

    # Compute [IV0*1, IV0*x, IV0*x^2, IV0*x^3, ...] in v28.
    # Reverse the bits order back.
    .word 1258565207

    # Prepare the x^n multiplier in v20. The `n` is the aes-xts block number
    # in a LMUL=4 register group.
    #   n = ((VLEN*LMUL)/(32*4)) = ((VLEN*4)/(32*4))
    #     = (VLEN/32)
    # We could use vsetvli with `e32, m1` to compute the `n` number.
    .word 218133207
    li t1, 1
    sll t0, t1, t0
    .word 3447812183
    .word 1577070679
    .word 3380670551
    .word 1577238615
    .word 3447812183
    .word 1241784407
    .word 221179991
    .word 1577073239
    .word 2785258103

    j 2f
1:
    .word 3439489111
    .word 1271146583
2:

        .word 3439489111
    .word 34005127
    addi a3, a3, 16
    .word 34005255
    addi a3, a3, 16
    .word 34005383
    addi a3, a3, 16
    .word 34005511
    addi a3, a3, 16
    .word 34005639
    addi a3, a3, 16
    .word 34005767
    addi a3, a3, 16
    .word 34005895
    addi a3, a3, 16
    .word 34006023
    addi a3, a3, 16
    .word 34006151
    addi a3, a3, 16
    .word 34006279
    addi a3, a3, 16
    .word 34006407


    beqz t4, 2f

    .word 221182167
    j 1f

.Ldec_blocks_128:
    .word 221182167
    # load ciphertext into v24
    .word 33909767
    # update iv
    .word 2739447927
    # reverse the iv's bits order back
    .word 1258565207
1:
    .word 797838423
    slli t0, a7, 2
    sub t4, t4, a7
    add a0, a0, t0
        .word 2796792951
    .word 2795514999
    .word 2794466423
    .word 2793417847
    .word 2792369271
    .word 2791320695
    .word 2790272119
    .word 2789223543
    .word 2788174967
    .word 2787126391
    .word 2786110583

    .word 797838423

    # store plaintext
    .word 33942567
    add a1, a1, t0

    bnez t4, .Ldec_blocks_128

2:
        bnez a6, 1f
    ret
1:
    # load second to last block's ciphertext
    .word 3439489111
    .word 33909767
    addi a0, a0, 16

    # setup `x` multiplier with byte-reversed order
    # 0b00000010 => 0b01000000 (0x40)
    li t0, 0x40
    .word 3439489111
    .word 1577073239
    .word 3355504727
    .word 1577241175

    beqz a2, 1f
    # slidedown third to last block
    addi a7, a7, -4
    .word 3441586263
    # multiplier
    .word 1057540183

    # compute IV for last block
    .word 3439489111
    .word 2739447927
    .word 1258565207

    # compute IV for second to last block
    .word 2739447927
    .word 1258565335
    j 2f
1:
    # compute IV for second to last block
    .word 3439489111
    .word 2739447927
    .word 1258565335
2:


    ## xts second to last block
    .word 3439489111
    .word 797871191
        .word 2796792951
    .word 2795514999
    .word 2794466423
    .word 2793417847
    .word 2792369271
    .word 2791320695
    .word 2790272119
    .word 2789223543
    .word 2788174967
    .word 2787126391
    .word 2786110583

    .word 797871191
    .word 1577848023

    # load last block ciphertext
    .word 134770775
    .word 33885191

    # store second to last block plaintext
    addi t0, a1, 16
    .word 33721511

    ## xts last block
    .word 3439489111
    .word 797838423
        .word 2796792951
    .word 2795514999
    .word 2794466423
    .word 2793417847
    .word 2792369271
    .word 2791320695
    .word 2790272119
    .word 2789223543
    .word 2788174967
    .word 2787126391
    .word 2786110583

    .word 797838423

    # store second to last block plaintext
    .word 33942567

    ret
.size aes_xts_dec_128,.-aes_xts_dec_128
.p2align 3
aes_xts_dec_256:
        # load input
    .word 221182167
    .word 33909767

    li t0, 5
    # We could simplify the initialization steps if we have `block<=1`.
    blt t4, t0, 1f

    # Note: We use `vgmul` for GF(2^128) multiplication. The `vgmul` uses
    # different order of coefficients. We should use`vbrev8` to reverse the
    # data when we use `vgmul`.
    .word 3439489111
    .word 1271144535
    .word 221179991
    .word 1577072727
    # v16: [r-IV0, r-IV0, ...]
    .word 2785257591

    # Prepare GF(2^128) multiplier [1, x, x^2, x^3, ...] in v8.
    slli t0, t4, 2
    .word 218296407
    # v2: [`1`, `1`, `1`, `1`, ...]
    .word 1577103703
    # v3: [`0`, `1`, `2`, `3`, ...]
    .word 1376297431
    .word 227733591
    # v4: [`1`, 0, `1`, 0, `1`, 0, `1`, 0, ...]
    .word 1243816535
    # v6: [`0`, 0, `1`, 0, `2`, 0, `3`, 0, ...]
    .word 1244865367
    slli t0, t4, 1
    .word 219344983
    # v8: [1<<0=1, 0, 0, 0, 1<<1=x, 0, 0, 0, 1<<2=x^2, 0, 0, 0, ...]
    .word 3594716247

    # Compute [r-IV0*1, r-IV0*x, r-IV0*x^2, r-IV0*x^3, ...] in v16
    .word 221179991
    .word 1250174039
    .word 2726865015

    # Compute [IV0*1, IV0*x, IV0*x^2, IV0*x^3, ...] in v28.
    # Reverse the bits order back.
    .word 1258565207

    # Prepare the x^n multiplier in v20. The `n` is the aes-xts block number
    # in a LMUL=4 register group.
    #   n = ((VLEN*LMUL)/(32*4)) = ((VLEN*4)/(32*4))
    #     = (VLEN/32)
    # We could use vsetvli with `e32, m1` to compute the `n` number.
    .word 218133207
    li t1, 1
    sll t0, t1, t0
    .word 3447812183
    .word 1577070679
    .word 3380670551
    .word 1577238615
    .word 3447812183
    .word 1241784407
    .word 221179991
    .word 1577073239
    .word 2785258103

    j 2f
1:
    .word 3439489111
    .word 1271146583
2:

        .word 3439489111
    .word 34005127
    addi a3, a3, 16
    .word 34005255
    addi a3, a3, 16
    .word 34005383
    addi a3, a3, 16
    .word 34005511
    addi a3, a3, 16
    .word 34005639
    addi a3, a3, 16
    .word 34005767
    addi a3, a3, 16
    .word 34005895
    addi a3, a3, 16
    .word 34006023
    addi a3, a3, 16
    .word 34006151
    addi a3, a3, 16
    .word 34006279
    addi a3, a3, 16
    .word 34006407
    addi a3, a3, 16
    .word 34006535
    addi a3, a3, 16
    .word 34006663
    addi a3, a3, 16
    .word 34006791
    addi a3, a3, 16
    .word 34006919


    beqz t4, 2f

    .word 221182167
    j 1f

.Ldec_blocks_256:
    .word 221182167
    # load ciphertext into v24
    .word 33909767
    # update iv
    .word 2739447927
    # reverse the iv's bits order back
    .word 1258565207
1:
    .word 797838423
    slli t0, a7, 2
    sub t4, t4, a7
    add a0, a0, t0
        .word 2800987255
    .word 2799709303
    .word 2798660727
    .word 2797612151
    .word 2796563575
    .word 2795514999
    .word 2794466423
    .word 2793417847
    .word 2792369271
    .word 2791320695
    .word 2790272119
    .word 2789223543
    .word 2788174967
    .word 2787126391
    .word 2786110583

    .word 797838423

    # store plaintext
    .word 33942567
    add a1, a1, t0

    bnez t4, .Ldec_blocks_256

2:
        bnez a6, 1f
    ret
1:
    # load second to last block's ciphertext
    .word 3439489111
    .word 33909767
    addi a0, a0, 16

    # setup `x` multiplier with byte-reversed order
    # 0b00000010 => 0b01000000 (0x40)
    li t0, 0x40
    .word 3439489111
    .word 1577073239
    .word 3355504727
    .word 1577241175

    beqz a2, 1f
    # slidedown third to last block
    addi a7, a7, -4
    .word 3441586263
    # multiplier
    .word 1057540183

    # compute IV for last block
    .word 3439489111
    .word 2739447927
    .word 1258565207

    # compute IV for second to last block
    .word 2739447927
    .word 1258565335
    j 2f
1:
    # compute IV for second to last block
    .word 3439489111
    .word 2739447927
    .word 1258565335
2:


    ## xts second to last block
    .word 3439489111
    .word 797871191
        .word 2800987255
    .word 2799709303
    .word 2798660727
    .word 2797612151
    .word 2796563575
    .word 2795514999
    .word 2794466423
    .word 2793417847
    .word 2792369271
    .word 2791320695
    .word 2790272119
    .word 2789223543
    .word 2788174967
    .word 2787126391
    .word 2786110583

    .word 797871191
    .word 1577848023

    # load last block ciphertext
    .word 134770775
    .word 33885191

    # store second to last block plaintext
    addi t0, a1, 16
    .word 33721511

    ## xts last block
    .word 3439489111
    .word 797838423
        .word 2800987255
    .word 2799709303
    .word 2798660727
    .word 2797612151
    .word 2796563575
    .word 2795514999
    .word 2794466423
    .word 2793417847
    .word 2792369271
    .word 2791320695
    .word 2790272119
    .word 2789223543
    .word 2788174967
    .word 2787126391
    .word 2786110583

    .word 797838423

    # store second to last block plaintext
    .word 33942567

    ret
.size aes_xts_dec_256,.-aes_xts_dec_256
