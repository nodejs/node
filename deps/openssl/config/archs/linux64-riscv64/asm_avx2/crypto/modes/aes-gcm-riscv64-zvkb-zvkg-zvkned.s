.text
.p2align 3
.globl rv64i_zvkb_zvkg_zvkned_aes_gcm_encrypt
.type rv64i_zvkb_zvkg_zvkned_aes_gcm_encrypt,@function
rv64i_zvkb_zvkg_zvkned_aes_gcm_encrypt:
    srli t0, a2, 4
    beqz t0, .Lenc_end
    slli t5, t0, 2

    mv a7, t5

        # Compute the AES-GCM full-block e32 length for `LMUL=4`. We will handle
    # the multiple AES-GCM blocks at the same time within `LMUL=4` register.
    # The AES-GCM's SEW is e32 and EGW is 128 bits.
    #   FULL_BLOCK_LEN32 = (VLEN*LMUL)/(EGW) * (EGW/SEW) = (VLEN*4)/(32*4) * 4
    #                    = (VLEN*4)/32
    # We could get the block_num using the VL value of `vsetvli with e32, m4`.
    .word 220231767
    # If `LEN32 % FULL_BLOCK_LEN32` is not equal to zero, we could fill the
    # zero padding data to make sure we could always handle FULL_BLOCK_LEN32
    # blocks for all iterations.

    ## Prepare the H^n multiplier in v16 for GCM multiplier. The `n` is the gcm
    ## block number in a LMUL=4 register group.
    ##   n = ((VLEN*LMUL)/(32*4)) = ((VLEN*4)/(32*4))
    ##     = (VLEN/32)
    ## We could use vsetvli with `e32, m1` to compute the `n` number.
    .word 218133207

    # The H is at `gcm128_context.Htable[0]`(addr(Xi)+16*2).
    addi t1, a5, 32
    .word 3439489111
    .word 33779591

    # Compute the H^n
    li t1, 1
1:
    .word 2750984183
    slli t1, t1, 1
    bltu t1, t0, 1b

    .word 220754007
    .word 1577072727
    .word 2817763447

    #### Load plaintext into v24 and handle padding. We also load the init tag
    #### data into v20 and prepare the AES ctr input data into v12 and v28.
    .word 1577073239

    ## Prepare the AES ctr input data into v12.
    # Setup ctr input mask.
    # ctr mask : [000100010001....]
    # Note: The actual vl should be `FULL_BLOCK_LEN32/4 * 2`, but we just use
    #   `FULL_BLOCK_LEN32` here.
    .word 201879639
    li t0, 0b10001000
    .word 1577238615
    # Load IV.
    .word 3439489111
    .word 34041735
    # Convert the big-endian counter into little-endian.
    .word 3305271383
    .word 1240772567
    # Splat the `single block of IV` to v12
    .word 220754007
    .word 1577072215
    .word 2817762935
    # Prepare the ctr counter into v8
    # v8: [x, x, x, 0, x, x, x, 1, x, x, x, 2, ...]
    .word 1342710871
    # Merge IV and ctr counter into v12.
    # v12:[x, x, x, count+0, x, x, x, count+1, ...]
    .word 86536279
    .word 12846679

    li t4, 0
    # Get the SEW32 size in the first round.
    # If we have the non-zero value for `LEN32&(FULL_BLOCK_LEN32-1)`, then
    # we will have the leading padding zero.
    addi t0, a6, -1
    and t0, t0, t5
    beqz t0, 1f

    ## with padding
    sub t5, t5, t0
    sub t4, a6, t0
    # padding block size
    srli t1, t4, 2
    # padding byte size
    slli t2, t4, 2

    # Adjust the ctr counter to make the counter start from `counter+0` for the
    # first non-padding block.
    .word 86536279
    .word 147015255
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    .word 1237626455

    # Prepare the mask for input loading in the first round. We use
    # `VL=FULL_BLOCK_LEN32` with the mask in the first round.
    # Adjust input ptr.
    sub a0, a0, t2
    # Adjust output ptr.
    sub a1, a1, t2
    .word 211316823
    .word 1376297303
    # We don't use the pseudo instruction `vmsgeu` here. Use `vmsgtu` instead.
    # The original code is:
    #   vmsgeu.vx v0, v2, t4
    addi t0, t4, -1
    .word 2049097815
    .word 220754007
    .word 1577073751
    # Load the input for length FULL_BLOCK_LEN32 with mask.
    .word 86536279
    .word 355335

    # Load the init `Xi` data to v20 with preceding zero padding.
    # Adjust Xi ptr.
    sub t0, a5, t2
    # Load for length `zero-padding-e32-length + 4`.
    addi t1, t4, 4
    .word 19099735
    .word 190983
    j 2f

1:
    ## without padding
    sub t5, t5, a6

    .word 220754007
    .word 33909767

    # Load the init Xi data to v20.
    .word 3372380247
    .word 34073095

    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 86536279
    .word 1577455191
    .word 1237626455
2:


    # Load number of rounds
    lwu t0, 240(a3)
    li t1, 14
    li t2, 12
    li t3, 10

    beq t0, t1, aes_gcm_enc_blocks_256
    beq t0, t2, aes_gcm_enc_blocks_192
    beq t0, t3, aes_gcm_enc_blocks_128

.Lenc_end:
    li a0, 0
    ret

.size rv64i_zvkb_zvkg_zvkned_aes_gcm_encrypt,.-rv64i_zvkb_zvkg_zvkned_aes_gcm_encrypt
.p2align 3
aes_gcm_enc_blocks_128:
    srli t6, a6, 2
    slli t0, a6, 2

        # Load all 11 aes round keys to v1-v11 registers.
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

    # We already have the ciphertext/plaintext and ctr data for the first round.
        .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr result.
    .word 801902167

        bnez t4, 1f

    ## without padding
    # Store ciphertext/plaintext
    .word 33943079
    j 2f

    ## with padding
1:
    # Store ciphertext/plaintext using mask
    .word 388647

    # Fill zero for the padding blocks
    .word 154071127
    .word 1577074263

    # We have used mask register for `INPUT_PADDING_MASK` before. We need to
    # setup the ctr mask back.
    # ctr mask : [000100010001....]
    .word 201879639
    li t1, 0b10001000
    .word 1577271383
2:



    add a0, a0, t0
    add a1, a1, t0


    .word 220754007

.Lenc_blocks_128:
    # Compute the partial tags.
    # The partial tags will multiply with [H^n, H^n, ..., H^n]
    #   [tag0, tag1, ...] =
    #     ([tag0, tag1, ...] + [ciphertext0, ciphertext1, ...] * [H^n, H^n, ..., H^n]
    # We will skip the [H^n, H^n, ..., H^n] multiplication for the last round.
    beqz t5, .Lenc_blocks_128_end
    .word 3004050039

        .word 86536279
    # Increase ctr in v12.
    .word 13616727
    sub t5, t5, a6
    # Load plaintext into v24
    .word 220229719
    .word 33909767
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    add a0, a0, t0
    .word 86011991
    .word 1237626455


        .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr ciphertext result.
    .word 801902167

    # Store ciphertext
    .word 33943079
    add a1, a1, t0

    j .Lenc_blocks_128
.Lenc_blocks_128_end:

    # Add ciphertext into partial tag
    .word 793643607

        .word 3441586263
    # Update current ctr value to v12
    .word 13616727
    # Convert ctr to big-endian counter.
    .word 1220847191
    .word 484903


        # The H is at `gcm128_context.Htable[0]` (addr(Xi)+16*2).
    # Load H to v1
    addi t1, a5, 32
    .word 3439489111
    .word 33775751
    # Multiply H for each partial tag and XOR them together.
    # Handle 1st partial tag
    .word 1577713751
    .word 2719522935
    # Handle 2nd to N-th partial tags
    li t1, 4
1:
    .word 3441586263
    .word 1061372503
    .word 3439489111
    .word 2987532407
    addi t1, t1, 4
    blt t1, a6, 1b


    # Save the final tag
    .word 34070567

    # return the processed size.
    slli a0, a7, 2
    ret
.size aes_gcm_enc_blocks_128,.-aes_gcm_enc_blocks_128
.p2align 3
aes_gcm_enc_blocks_192:
    srli t6, a6, 2
    slli t0, a6, 2

        # We run out of 32 vector registers, so we just preserve some round keys
    # and load the remaining round keys inside the aes body.
    # We keep the round keys for:
    #   1, 2, 3, 5, 6, 7, 9, 10, 11 and 12th keys.
    # The following keys will be loaded in the aes body:
    #   4, 8 and 13th keys.
    .word 3439489111
    # key 1
    .word 34005127
    # key 2
    addi t1, a3, 16
    .word 33775879
    # key 3
    addi t1, a3, 32
    .word 33776007
    # key 5
    addi t1, a3, 64
    .word 33776135
    # key 6
    addi t1, a3, 80
    .word 33776263
    # key 7
    addi t1, a3, 96
    .word 33776391
    # key 9
    addi t1, a3, 128
    .word 33776519
    # key 10
    addi t1, a3, 144
    .word 33776647
    # key 11
    addi t1, a3, 160
    .word 33776775
    # key 12
    addi t1, a3, 176
    .word 33776903

    # We already have the ciphertext/plaintext and ctr data for the first round.
        # Load key 4
    .word 3439489111
    addi t1, a3, 48
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2796629623
    # Load key 8
    .word 3439489111
    addi t1, a3, 112
    .word 33777031
    .word 220754007
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 13
    .word 3439489111
    addi t1, a3, 192
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr result.
    .word 801902167

        bnez t4, 1f

    ## without padding
    # Store ciphertext/plaintext
    .word 33943079
    j 2f

    ## with padding
1:
    # Store ciphertext/plaintext using mask
    .word 388647

    # Fill zero for the padding blocks
    .word 154071127
    .word 1577074263

    # We have used mask register for `INPUT_PADDING_MASK` before. We need to
    # setup the ctr mask back.
    # ctr mask : [000100010001....]
    .word 201879639
    li t1, 0b10001000
    .word 1577271383
2:



    add a0, a0, t0
    add a1, a1, t0


    .word 220754007

.Lenc_blocks_192:
    # Compute the partial tags.
    # The partial tags will multiply with [H^n, H^n, ..., H^n]
    #   [tag0, tag1, ...] =
    #     ([tag0, tag1, ...] + [ciphertext0, ciphertext1, ...] * [H^n, H^n, ..., H^n]
    # We will skip the [H^n, H^n, ..., H^n] multiplication for the last round.
    beqz t5, .Lenc_blocks_192_end
    .word 3004050039

        .word 86536279
    # Increase ctr in v12.
    .word 13616727
    sub t5, t5, a6
    # Load plaintext into v24
    .word 220229719
    .word 33909767
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    add a0, a0, t0
    .word 86011991
    .word 1237626455


        # Load key 4
    .word 3439489111
    addi t1, a3, 48
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2796629623
    # Load key 8
    .word 3439489111
    addi t1, a3, 112
    .word 33777031
    .word 220754007
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 13
    .word 3439489111
    addi t1, a3, 192
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr ciphertext result.
    .word 801902167

    # Store ciphertext
    .word 33943079
    add a1, a1, t0

    j .Lenc_blocks_192
.Lenc_blocks_192_end:

    # Add ciphertext into partial tag
    .word 793643607

        .word 3441586263
    # Update current ctr value to v12
    .word 13616727
    # Convert ctr to big-endian counter.
    .word 1220847191
    .word 484903


        # The H is at `gcm128_context.Htable[0]` (addr(Xi)+16*2).
    # Load H to v1
    addi t1, a5, 32
    .word 3439489111
    .word 33775751
    # Multiply H for each partial tag and XOR them together.
    # Handle 1st partial tag
    .word 1577713751
    .word 2719522935
    # Handle 2nd to N-th partial tags
    li t1, 4
1:
    .word 3441586263
    .word 1061372503
    .word 3439489111
    .word 2987532407
    addi t1, t1, 4
    blt t1, a6, 1b


    # Save the final tag
    .word 34070567

    # return the processed size.
    slli a0, a7, 2
    ret
.size aes_gcm_enc_blocks_192,.-aes_gcm_enc_blocks_192
.p2align 3
aes_gcm_enc_blocks_256:
    srli t6, a6, 2
    slli t0, a6, 2

        # We run out of 32 vector registers, so we just preserve some round keys
    # and load the remaining round keys inside the aes body.
    # We keep the round keys for:
    #   1, 2, 4, 5, 7, 8, 10, 11, 13 and 14th keys.
    # The following keys will be loaded in the aes body:
    #   3, 6, 9, 12 and 15th keys.
    .word 3439489111
    # key 1
    .word 34005127
    # key 2
    addi t1, a3, 16
    .word 33775879
    # key 4
    addi t1, a3, 48
    .word 33776007
    # key 5
    addi t1, a3, 64
    .word 33776135
    # key 7
    addi t1, a3, 96
    .word 33776263
    # key 8
    addi t1, a3, 112
    .word 33776391
    # key 10
    addi t1, a3, 144
    .word 33776519
    # key 11
    addi t1, a3, 160
    .word 33776647
    # key 13
    addi t1, a3, 192
    .word 33776775
    # key 14
    addi t1, a3, 208
    .word 33776903

    # We already have the ciphertext/plaintext and ctr data for the first round.
        # Load key 3
    .word 3439489111
    addi t1, a3, 32
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2796629623
    # Load key 6
    .word 3439489111
    addi t1, a3, 80
    .word 33777031
    .word 220754007
    .word 2788241015
    .word 2789289591
    .word 2796629623
    # Load key 9
    .word 3439489111
    addi t1, a3, 128
    .word 33777031
    .word 220754007
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 12
    .word 3439489111
    addi t1, a3, 176
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2796629623
    # Load key 15
    .word 3439489111
    addi t1, a3, 224
    .word 33777031
    .word 220754007
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr result.
    .word 801902167

        bnez t4, 1f

    ## without padding
    # Store ciphertext/plaintext
    .word 33943079
    j 2f

    ## with padding
1:
    # Store ciphertext/plaintext using mask
    .word 388647

    # Fill zero for the padding blocks
    .word 154071127
    .word 1577074263

    # We have used mask register for `INPUT_PADDING_MASK` before. We need to
    # setup the ctr mask back.
    # ctr mask : [000100010001....]
    .word 201879639
    li t1, 0b10001000
    .word 1577271383
2:



    add a0, a0, t0
    add a1, a1, t0


    .word 220754007

.Lenc_blocks_256:
    # Compute the partial tags.
    # The partial tags will multiply with [H^n, H^n, ..., H^n]
    #   [tag0, tag1, ...] =
    #     ([tag0, tag1, ...] + [ciphertext0, ciphertext1, ...] * [H^n, H^n, ..., H^n]
    # We will skip the [H^n, H^n, ..., H^n] multiplication for the last round.
    beqz t5, .Lenc_blocks_256_end
    .word 3004050039

        .word 86536279
    # Increase ctr in v12.
    .word 13616727
    sub t5, t5, a6
    # Load plaintext into v24
    .word 220229719
    .word 33909767
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    add a0, a0, t0
    .word 86011991
    .word 1237626455


        # Load key 3
    .word 3439489111
    addi t1, a3, 32
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2796629623
    # Load key 6
    .word 3439489111
    addi t1, a3, 80
    .word 33777031
    .word 220754007
    .word 2788241015
    .word 2789289591
    .word 2796629623
    # Load key 9
    .word 3439489111
    addi t1, a3, 128
    .word 33777031
    .word 220754007
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 12
    .word 3439489111
    addi t1, a3, 176
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2796629623
    # Load key 15
    .word 3439489111
    addi t1, a3, 224
    .word 33777031
    .word 220754007
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr ciphertext result.
    .word 801902167

    # Store ciphertext
    .word 33943079
    add a1, a1, t0

    j .Lenc_blocks_256
.Lenc_blocks_256_end:

    # Add ciphertext into partial tag
    .word 793643607

        .word 3441586263
    # Update current ctr value to v12
    .word 13616727
    # Convert ctr to big-endian counter.
    .word 1220847191
    .word 484903


        # The H is at `gcm128_context.Htable[0]` (addr(Xi)+16*2).
    # Load H to v1
    addi t1, a5, 32
    .word 3439489111
    .word 33775751
    # Multiply H for each partial tag and XOR them together.
    # Handle 1st partial tag
    .word 1577713751
    .word 2719522935
    # Handle 2nd to N-th partial tags
    li t1, 4
1:
    .word 3441586263
    .word 1061372503
    .word 3439489111
    .word 2987532407
    addi t1, t1, 4
    blt t1, a6, 1b


    # Save the final tag
    .word 34070567

    # return the processed size.
    slli a0, a7, 2
    ret
.size aes_gcm_enc_blocks_256,.-aes_gcm_enc_blocks_256
.p2align 3
.globl rv64i_zvkb_zvkg_zvkned_aes_gcm_decrypt
.type rv64i_zvkb_zvkg_zvkned_aes_gcm_decrypt,@function
rv64i_zvkb_zvkg_zvkned_aes_gcm_decrypt:
    srli t0, a2, 4
    beqz t0, .Ldec_end
    slli t5, t0, 2

    mv a7, t5

        # Compute the AES-GCM full-block e32 length for `LMUL=4`. We will handle
    # the multiple AES-GCM blocks at the same time within `LMUL=4` register.
    # The AES-GCM's SEW is e32 and EGW is 128 bits.
    #   FULL_BLOCK_LEN32 = (VLEN*LMUL)/(EGW) * (EGW/SEW) = (VLEN*4)/(32*4) * 4
    #                    = (VLEN*4)/32
    # We could get the block_num using the VL value of `vsetvli with e32, m4`.
    .word 220231767
    # If `LEN32 % FULL_BLOCK_LEN32` is not equal to zero, we could fill the
    # zero padding data to make sure we could always handle FULL_BLOCK_LEN32
    # blocks for all iterations.

    ## Prepare the H^n multiplier in v16 for GCM multiplier. The `n` is the gcm
    ## block number in a LMUL=4 register group.
    ##   n = ((VLEN*LMUL)/(32*4)) = ((VLEN*4)/(32*4))
    ##     = (VLEN/32)
    ## We could use vsetvli with `e32, m1` to compute the `n` number.
    .word 218133207

    # The H is at `gcm128_context.Htable[0]`(addr(Xi)+16*2).
    addi t1, a5, 32
    .word 3439489111
    .word 33779591

    # Compute the H^n
    li t1, 1
1:
    .word 2750984183
    slli t1, t1, 1
    bltu t1, t0, 1b

    .word 220754007
    .word 1577072727
    .word 2817763447

    #### Load plaintext into v24 and handle padding. We also load the init tag
    #### data into v20 and prepare the AES ctr input data into v12 and v28.
    .word 1577073239

    ## Prepare the AES ctr input data into v12.
    # Setup ctr input mask.
    # ctr mask : [000100010001....]
    # Note: The actual vl should be `FULL_BLOCK_LEN32/4 * 2`, but we just use
    #   `FULL_BLOCK_LEN32` here.
    .word 201879639
    li t0, 0b10001000
    .word 1577238615
    # Load IV.
    .word 3439489111
    .word 34041735
    # Convert the big-endian counter into little-endian.
    .word 3305271383
    .word 1240772567
    # Splat the `single block of IV` to v12
    .word 220754007
    .word 1577072215
    .word 2817762935
    # Prepare the ctr counter into v8
    # v8: [x, x, x, 0, x, x, x, 1, x, x, x, 2, ...]
    .word 1342710871
    # Merge IV and ctr counter into v12.
    # v12:[x, x, x, count+0, x, x, x, count+1, ...]
    .word 86536279
    .word 12846679

    li t4, 0
    # Get the SEW32 size in the first round.
    # If we have the non-zero value for `LEN32&(FULL_BLOCK_LEN32-1)`, then
    # we will have the leading padding zero.
    addi t0, a6, -1
    and t0, t0, t5
    beqz t0, 1f

    ## with padding
    sub t5, t5, t0
    sub t4, a6, t0
    # padding block size
    srli t1, t4, 2
    # padding byte size
    slli t2, t4, 2

    # Adjust the ctr counter to make the counter start from `counter+0` for the
    # first non-padding block.
    .word 86536279
    .word 147015255
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    .word 1237626455

    # Prepare the mask for input loading in the first round. We use
    # `VL=FULL_BLOCK_LEN32` with the mask in the first round.
    # Adjust input ptr.
    sub a0, a0, t2
    # Adjust output ptr.
    sub a1, a1, t2
    .word 211316823
    .word 1376297303
    # We don't use the pseudo instruction `vmsgeu` here. Use `vmsgtu` instead.
    # The original code is:
    #   vmsgeu.vx v0, v2, t4
    addi t0, t4, -1
    .word 2049097815
    .word 220754007
    .word 1577073751
    # Load the input for length FULL_BLOCK_LEN32 with mask.
    .word 86536279
    .word 355335

    # Load the init `Xi` data to v20 with preceding zero padding.
    # Adjust Xi ptr.
    sub t0, a5, t2
    # Load for length `zero-padding-e32-length + 4`.
    addi t1, t4, 4
    .word 19099735
    .word 190983
    j 2f

1:
    ## without padding
    sub t5, t5, a6

    .word 220754007
    .word 33909767

    # Load the init Xi data to v20.
    .word 3372380247
    .word 34073095

    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 86536279
    .word 1577455191
    .word 1237626455
2:


    # Load number of rounds
    lwu t0, 240(a3)
    li t1, 14
    li t2, 12
    li t3, 10

    beq t0, t1, aes_gcm_dec_blocks_256
    beq t0, t2, aes_gcm_dec_blocks_192
    beq t0, t3, aes_gcm_dec_blocks_128

.Ldec_end:
    li a0, 0
    ret
.size rv64i_zvkb_zvkg_zvkned_aes_gcm_decrypt,.-rv64i_zvkb_zvkg_zvkned_aes_gcm_decrypt
.p2align 3
aes_gcm_dec_blocks_128:
    srli t6, a6, 2
    slli t0, a6, 2

        # Load all 11 aes round keys to v1-v11 registers.
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

    # We already have the ciphertext/plaintext and ctr data for the first round.
        .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr result.
    .word 801902167

        bnez t4, 1f

    ## without padding
    # Store ciphertext/plaintext
    .word 33943079
    j 2f

    ## with padding
1:
    # Store ciphertext/plaintext using mask
    .word 388647

    # Fill zero for the padding blocks
    .word 154071127
    .word 1577074263

    # We have used mask register for `INPUT_PADDING_MASK` before. We need to
    # setup the ctr mask back.
    # ctr mask : [000100010001....]
    .word 201879639
    li t1, 0b10001000
    .word 1577271383
2:



    add a0, a0, t0
    add a1, a1, t0


    .word 220754007

.Ldec_blocks_128:
    # Compute the partial tags.
    # The partial tags will multiply with [H^n, H^n, ..., H^n]
    #   [tag0, tag1, ...] =
    #     ([tag0, tag1, ...] + [ciphertext0, ciphertext1, ...] * [H^n, H^n, ..., H^n]
    # We will skip the [H^n, H^n, ..., H^n] multiplication for the last round.
    beqz t5, .Ldec_blocks_256_end
    .word 3003918967

        .word 86536279
    # Increase ctr in v12.
    .word 13616727
    sub t5, t5, a6
    # Load plaintext into v24
    .word 220229719
    .word 33909767
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    add a0, a0, t0
    .word 86011991
    .word 1237626455


        .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr plaintext result.
    .word 801902167

    # Store plaintext
    .word 33943079
    add a1, a1, t0

    j .Ldec_blocks_128
.Ldec_blocks_128_end:

    # Add ciphertext into partial tag
    .word 793512535

        .word 3441586263
    # Update current ctr value to v12
    .word 13616727
    # Convert ctr to big-endian counter.
    .word 1220847191
    .word 484903


        # The H is at `gcm128_context.Htable[0]` (addr(Xi)+16*2).
    # Load H to v1
    addi t1, a5, 32
    .word 3439489111
    .word 33775751
    # Multiply H for each partial tag and XOR them together.
    # Handle 1st partial tag
    .word 1577713751
    .word 2719522935
    # Handle 2nd to N-th partial tags
    li t1, 4
1:
    .word 3441586263
    .word 1061372503
    .word 3439489111
    .word 2987532407
    addi t1, t1, 4
    blt t1, a6, 1b


    # Save the final tag
    .word 34070567

    # return the processed size.
    slli a0, a7, 2
    ret
.size aes_gcm_dec_blocks_128,.-aes_gcm_dec_blocks_128
.p2align 3
aes_gcm_dec_blocks_192:
    srli t6, a6, 2
    slli t0, a6, 2

        # We run out of 32 vector registers, so we just preserve some round keys
    # and load the remaining round keys inside the aes body.
    # We keep the round keys for:
    #   1, 2, 3, 5, 6, 7, 9, 10, 11 and 12th keys.
    # The following keys will be loaded in the aes body:
    #   4, 8 and 13th keys.
    .word 3439489111
    # key 1
    .word 34005127
    # key 2
    addi t1, a3, 16
    .word 33775879
    # key 3
    addi t1, a3, 32
    .word 33776007
    # key 5
    addi t1, a3, 64
    .word 33776135
    # key 6
    addi t1, a3, 80
    .word 33776263
    # key 7
    addi t1, a3, 96
    .word 33776391
    # key 9
    addi t1, a3, 128
    .word 33776519
    # key 10
    addi t1, a3, 144
    .word 33776647
    # key 11
    addi t1, a3, 160
    .word 33776775
    # key 12
    addi t1, a3, 176
    .word 33776903

    # We already have the ciphertext/plaintext and ctr data for the first round.
        # Load key 4
    .word 3439489111
    addi t1, a3, 48
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2796629623
    # Load key 8
    .word 3439489111
    addi t1, a3, 112
    .word 33777031
    .word 220754007
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 13
    .word 3439489111
    addi t1, a3, 192
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr result.
    .word 801902167

        bnez t4, 1f

    ## without padding
    # Store ciphertext/plaintext
    .word 33943079
    j 2f

    ## with padding
1:
    # Store ciphertext/plaintext using mask
    .word 388647

    # Fill zero for the padding blocks
    .word 154071127
    .word 1577074263

    # We have used mask register for `INPUT_PADDING_MASK` before. We need to
    # setup the ctr mask back.
    # ctr mask : [000100010001....]
    .word 201879639
    li t1, 0b10001000
    .word 1577271383
2:



    add a0, a0, t0
    add a1, a1, t0


    .word 220754007

.Ldec_blocks_192:
    # Compute the partial tags.
    # The partial tags will multiply with [H^n, H^n, ..., H^n]
    #   [tag0, tag1, ...] =
    #     ([tag0, tag1, ...] + [ciphertext0, ciphertext1, ...] * [H^n, H^n, ..., H^n]
    # We will skip the [H^n, H^n, ..., H^n] multiplication for the last round.
    beqz t5, .Ldec_blocks_192_end
    .word 3003918967

        .word 86536279
    # Increase ctr in v12.
    .word 13616727
    sub t5, t5, a6
    # Load plaintext into v24
    .word 220229719
    .word 33909767
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    add a0, a0, t0
    .word 86011991
    .word 1237626455


        # Load key 4
    .word 3439489111
    addi t1, a3, 48
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2788241015
    .word 2796629623
    # Load key 8
    .word 3439489111
    addi t1, a3, 112
    .word 33777031
    .word 220754007
    .word 2789289591
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 13
    .word 3439489111
    addi t1, a3, 192
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr plaintext result.
    .word 801902167

    # Store plaintext
    .word 33943079
    add a1, a1, t0

    j .Ldec_blocks_192
.Ldec_blocks_192_end:

    # Add ciphertext into partial tag
    .word 793512535

        .word 3441586263
    # Update current ctr value to v12
    .word 13616727
    # Convert ctr to big-endian counter.
    .word 1220847191
    .word 484903


        # The H is at `gcm128_context.Htable[0]` (addr(Xi)+16*2).
    # Load H to v1
    addi t1, a5, 32
    .word 3439489111
    .word 33775751
    # Multiply H for each partial tag and XOR them together.
    # Handle 1st partial tag
    .word 1577713751
    .word 2719522935
    # Handle 2nd to N-th partial tags
    li t1, 4
1:
    .word 3441586263
    .word 1061372503
    .word 3439489111
    .word 2987532407
    addi t1, t1, 4
    blt t1, a6, 1b


    # Save the final tag
    .word 34070567

    # return the processed size.
    slli a0, a7, 2
    ret
.size aes_gcm_dec_blocks_192,.-aes_gcm_dec_blocks_192
.p2align 3
aes_gcm_dec_blocks_256:
    srli t6, a6, 2
    slli t0, a6, 2

        # We run out of 32 vector registers, so we just preserve some round keys
    # and load the remaining round keys inside the aes body.
    # We keep the round keys for:
    #   1, 2, 4, 5, 7, 8, 10, 11, 13 and 14th keys.
    # The following keys will be loaded in the aes body:
    #   3, 6, 9, 12 and 15th keys.
    .word 3439489111
    # key 1
    .word 34005127
    # key 2
    addi t1, a3, 16
    .word 33775879
    # key 4
    addi t1, a3, 48
    .word 33776007
    # key 5
    addi t1, a3, 64
    .word 33776135
    # key 7
    addi t1, a3, 96
    .word 33776263
    # key 8
    addi t1, a3, 112
    .word 33776391
    # key 10
    addi t1, a3, 144
    .word 33776519
    # key 11
    addi t1, a3, 160
    .word 33776647
    # key 13
    addi t1, a3, 192
    .word 33776775
    # key 14
    addi t1, a3, 208
    .word 33776903

    # We already have the ciphertext/plaintext and ctr data for the first round.
        # Load key 3
    .word 3439489111
    addi t1, a3, 32
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2796629623
    # Load key 6
    .word 3439489111
    addi t1, a3, 80
    .word 33777031
    .word 220754007
    .word 2788241015
    .word 2789289591
    .word 2796629623
    # Load key 9
    .word 3439489111
    addi t1, a3, 128
    .word 33777031
    .word 220754007
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 12
    .word 3439489111
    addi t1, a3, 176
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2796629623
    # Load key 15
    .word 3439489111
    addi t1, a3, 224
    .word 33777031
    .word 220754007
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr result.
    .word 801902167

        bnez t4, 1f

    ## without padding
    # Store ciphertext/plaintext
    .word 33943079
    j 2f

    ## with padding
1:
    # Store ciphertext/plaintext using mask
    .word 388647

    # Fill zero for the padding blocks
    .word 154071127
    .word 1577074263

    # We have used mask register for `INPUT_PADDING_MASK` before. We need to
    # setup the ctr mask back.
    # ctr mask : [000100010001....]
    .word 201879639
    li t1, 0b10001000
    .word 1577271383
2:



    add a0, a0, t0
    add a1, a1, t0


    .word 220754007

.Ldec_blocks_256:
    # Compute the partial tags.
    # The partial tags will multiply with [H^n, H^n, ..., H^n]
    #   [tag0, tag1, ...] =
    #     ([tag0, tag1, ...] + [ciphertext0, ciphertext1, ...] * [H^n, H^n, ..., H^n]
    # We will skip the [H^n, H^n, ..., H^n] multiplication for the last round.
    beqz t5, .Ldec_blocks_256_end
    .word 3003918967

        .word 86536279
    # Increase ctr in v12.
    .word 13616727
    sub t5, t5, a6
    # Load plaintext into v24
    .word 220229719
    .word 33909767
    # Prepare the AES ctr input into v28.
    # The ctr data uses big-endian form.
    .word 1577455191
    add a0, a0, t0
    .word 86011991
    .word 1237626455


        # Load key 3
    .word 3439489111
    addi t1, a3, 32
    .word 33777031
    .word 220754007
    .word 2786307703
    .word 2787192439
    .word 2796629623
    # Load key 6
    .word 3439489111
    addi t1, a3, 80
    .word 33777031
    .word 220754007
    .word 2788241015
    .word 2789289591
    .word 2796629623
    # Load key 9
    .word 3439489111
    addi t1, a3, 128
    .word 33777031
    .word 220754007
    .word 2790338167
    .word 2791386743
    .word 2796629623
    # Load key 12
    .word 3439489111
    addi t1, a3, 176
    .word 33777031
    .word 220754007
    .word 2792435319
    .word 2793483895
    .word 2796629623
    # Load key 15
    .word 3439489111
    addi t1, a3, 224
    .word 33777031
    .word 220754007
    .word 2794532471
    .word 2795581047
    .word 2796662391


    # Compute AES ctr plaintext result.
    .word 801902167

    # Store plaintext
    .word 33943079
    add a1, a1, t0

    j .Ldec_blocks_256
.Ldec_blocks_256_end:

    # Add ciphertext into partial tag
    .word 793512535

        .word 3441586263
    # Update current ctr value to v12
    .word 13616727
    # Convert ctr to big-endian counter.
    .word 1220847191
    .word 484903


        # The H is at `gcm128_context.Htable[0]` (addr(Xi)+16*2).
    # Load H to v1
    addi t1, a5, 32
    .word 3439489111
    .word 33775751
    # Multiply H for each partial tag and XOR them together.
    # Handle 1st partial tag
    .word 1577713751
    .word 2719522935
    # Handle 2nd to N-th partial tags
    li t1, 4
1:
    .word 3441586263
    .word 1061372503
    .word 3439489111
    .word 2987532407
    addi t1, t1, 4
    blt t1, a6, 1b


    # Save the final tag
    .word 34070567

    # return the processed size.
    slli a0, a7, 2
    ret
.size aes_gcm_dec_blocks_256,.-aes_gcm_dec_blocks_256
