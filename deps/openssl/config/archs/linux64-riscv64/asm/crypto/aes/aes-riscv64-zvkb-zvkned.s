.text
.p2align 3
.globl rv64i_zvkb_zvkned_ctr32_encrypt_blocks
.type rv64i_zvkb_zvkned_ctr32_encrypt_blocks,@function
rv64i_zvkb_zvkned_ctr32_encrypt_blocks:
    beqz a2, 1f

    # Load number of rounds
    lwu t0, 240(a3)
    li t1, 14
    li t2, 12
    li t3, 10

    slli t5, a2, 2

    beq t0, t1, ctr32_encrypt_blocks_256
    beq t0, t2, ctr32_encrypt_blocks_192
    beq t0, t3, ctr32_encrypt_blocks_128

1:
    ret

.size rv64i_zvkb_zvkned_ctr32_encrypt_blocks,.-rv64i_zvkb_zvkned_ctr32_encrypt_blocks
.p2align 3
ctr32_encrypt_blocks_128:
    # Load all 11 round keys to v1-v11 registers.
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

        # Setup mask into v0
    # The mask pattern for 4*N-th elements
    # mask v0: [000100010001....]
    # Note:
    #   We could setup the mask just for the maximum element length instead of
    #   the VLMAX.
    li t0, 0b10001000
    .word 201356247
    .word 1577238615
    # Load IV.
    # v31:[IV0, IV1, IV2, big-endian count]
    .word 3439489111
    .word 34041735
    # Convert the big-endian counter into little-endian.
    .word 3305271383
    .word 1240772567
    # Splat the IV to v16
    .word 221212759
    .word 1577072727
    .word 2817763447
    # Prepare the ctr pattern into v20
    # v20: [x, x, x, 0, x, x, x, 1, x, x, x, 2, ...]
    .word 1342712407
    # v16:[IV0, IV1, IV2, count+0, IV0, IV1, IV2, count+1, ...]
    .word 86998743
    .word 17434711


    ##### AES body
    j 2f
1:
    .word 86998743
    # Increase ctr in v16.
    .word 17811543
2:
    # Load plaintext into v20
    .word 33909255
    slli t0, t4, 2
    srli t6, t4, 2
    sub t5, t5, t4
    add a0, a0, t0
    # Prepare the AES ctr input into v24.
    # The ctr data uses big-endian form.
    .word 1577585751
    .word 1233431639

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

    # ciphertext
    .word 797576279

    # Store the ciphertext.
    .word 33942567
    add a1, a1, t0

    bnez t5, 1b

    ret
.size ctr32_encrypt_blocks_128,.-ctr32_encrypt_blocks_128
.p2align 3
ctr32_encrypt_blocks_192:
    # Load all 13 round keys to v1-v13 registers.
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

        # Setup mask into v0
    # The mask pattern for 4*N-th elements
    # mask v0: [000100010001....]
    # Note:
    #   We could setup the mask just for the maximum element length instead of
    #   the VLMAX.
    li t0, 0b10001000
    .word 201356247
    .word 1577238615
    # Load IV.
    # v31:[IV0, IV1, IV2, big-endian count]
    .word 3439489111
    .word 34041735
    # Convert the big-endian counter into little-endian.
    .word 3305271383
    .word 1240772567
    # Splat the IV to v16
    .word 221212759
    .word 1577072727
    .word 2817763447
    # Prepare the ctr pattern into v20
    # v20: [x, x, x, 0, x, x, x, 1, x, x, x, 2, ...]
    .word 1342712407
    # v16:[IV0, IV1, IV2, count+0, IV0, IV1, IV2, count+1, ...]
    .word 86998743
    .word 17434711


    ##### AES body
    j 2f
1:
    .word 86998743
    # Increase ctr in v16.
    .word 17811543
2:
    # Load plaintext into v20
    .word 33909255
    slli t0, t4, 2
    srli t6, t4, 2
    sub t5, t5, t4
    add a0, a0, t0
    # Prepare the AES ctr input into v24.
    # The ctr data uses big-endian form.
    .word 1577585751
    .word 1233431639

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
    .word 2798759031

    # ciphertext
    .word 797576279

    # Store the ciphertext.
    .word 33942567
    add a1, a1, t0

    bnez t5, 1b

    ret
.size ctr32_encrypt_blocks_192,.-ctr32_encrypt_blocks_192
.p2align 3
ctr32_encrypt_blocks_256:
    # Load all 15 round keys to v1-v15 registers.
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

        # Setup mask into v0
    # The mask pattern for 4*N-th elements
    # mask v0: [000100010001....]
    # Note:
    #   We could setup the mask just for the maximum element length instead of
    #   the VLMAX.
    li t0, 0b10001000
    .word 201356247
    .word 1577238615
    # Load IV.
    # v31:[IV0, IV1, IV2, big-endian count]
    .word 3439489111
    .word 34041735
    # Convert the big-endian counter into little-endian.
    .word 3305271383
    .word 1240772567
    # Splat the IV to v16
    .word 221212759
    .word 1577072727
    .word 2817763447
    # Prepare the ctr pattern into v20
    # v20: [x, x, x, 0, x, x, x, 1, x, x, x, 2, ...]
    .word 1342712407
    # v16:[IV0, IV1, IV2, count+0, IV0, IV1, IV2, count+1, ...]
    .word 86998743
    .word 17434711


    ##### AES body
    j 2f
1:
    .word 86998743
    # Increase ctr in v16.
    .word 17811543
2:
    # Load plaintext into v20
    .word 33909255
    slli t0, t4, 2
    srli t6, t4, 2
    sub t5, t5, t4
    add a0, a0, t0
    # Prepare the AES ctr input into v24.
    # The ctr data uses big-endian form.
    .word 1577585751
    .word 1233431639

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

    # ciphertext
    .word 797576279

    # Store the ciphertext.
    .word 33942567
    add a1, a1, t0

    bnez t5, 1b

    ret
.size ctr32_encrypt_blocks_256,.-ctr32_encrypt_blocks_256
