.text
.p2align 3
.globl rv64i_zvkned_cbc_encrypt
.type rv64i_zvkned_cbc_encrypt,@function
rv64i_zvkned_cbc_encrypt:
    # check whether the length is a multiple of 16 and >= 16
    li t1, 16
    blt a2, t1, L_end
    andi t1, a2, 15
    bnez t1, L_end

    # Load number of rounds
    lwu t2, 240(a3)

    # Get proper routine for key size
    li t0, 10
    beq t2, t0, L_cbc_enc_128

    li t0, 12
    beq t2, t0, L_cbc_enc_192

    li t0, 14
    beq t2, t0, L_cbc_enc_256

    ret
.size rv64i_zvkned_cbc_encrypt,.-rv64i_zvkned_cbc_encrypt
.p2align 3
L_cbc_enc_128:
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


    # Load IV.
    .word 34039815

    .word 33909767
    .word 797445207
    j 2f

1:
    .word 33908871
    .word 797477975

2:
    # AES body
        .word 2786307191     # with round key w[ 0, 3]
    .word 2787191927    # with round key w[ 4, 7]
    .word 2788240503    # with round key w[ 8,11]
    .word 2789289079    # with round key w[12,15]
    .word 2790337655    # with round key w[16,19]
    .word 2791386231    # with round key w[20,23]
    .word 2792434807    # with round key w[24,27]
    .word 2793483383    # with round key w[28,31]
    .word 2794531959    # with round key w[32,35]
    .word 2795580535   # with round key w[36,39]
    .word 2796661879   # with round key w[40,43]


    .word 33942567

    addi a0, a0, 16
    addi a1, a1, 16
    addi a2, a2, -16

    bnez a2, 1b

    .word 34040871

    ret
.size L_cbc_enc_128,.-L_cbc_enc_128
.p2align 3
L_cbc_enc_192:
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


    # Load IV.
    .word 34039815

    .word 33909767
    .word 797445207
    j 2f

1:
    .word 33908871
    .word 797477975

2:
    # AES body
        .word 2786307191     # with round key w[ 0, 3]
    .word 2787191927    # with round key w[ 4, 7]
    .word 2788240503    # with round key w[ 8,11]
    .word 2789289079    # with round key w[12,15]
    .word 2790337655    # with round key w[16,19]
    .word 2791386231    # with round key w[20,23]
    .word 2792434807    # with round key w[24,27]
    .word 2793483383    # with round key w[28,31]
    .word 2794531959    # with round key w[32,35]
    .word 2795580535   # with round key w[36,39]
    .word 2796629111   # with round key w[40,43]
    .word 2797677687   # with round key w[44,47]
    .word 2798759031   # with round key w[48,51]


    .word 33942567

    addi a0, a0, 16
    addi a1, a1, 16
    addi a2, a2, -16

    bnez a2, 1b

    .word 34040871

    ret
.size L_cbc_enc_192,.-L_cbc_enc_192
.p2align 3
L_cbc_enc_256:
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


    # Load IV.
    .word 34039815

    .word 33909767
    .word 797445207
    j 2f

1:
    .word 33908871
    .word 797477975

2:
    # AES body
        .word 2786307191     # with round key w[ 0, 3]
    .word 2787191927    # with round key w[ 4, 7]
    .word 2788240503    # with round key w[ 8,11]
    .word 2789289079    # with round key w[12,15]
    .word 2790337655    # with round key w[16,19]
    .word 2791386231    # with round key w[20,23]
    .word 2792434807    # with round key w[24,27]
    .word 2793483383    # with round key w[28,31]
    .word 2794531959    # with round key w[32,35]
    .word 2795580535   # with round key w[36,39]
    .word 2796629111   # with round key w[40,43]
    .word 2797677687   # with round key w[44,47]
    .word 2798726263   # with round key w[48,51]
    .word 2799774839   # with round key w[52,55]
    .word 2800856183   # with round key w[56,59]


    .word 33942567

    addi a0, a0, 16
    addi a1, a1, 16
    addi a2, a2, -16

    bnez a2, 1b

    .word 34040871

    ret
.size L_cbc_enc_256,.-L_cbc_enc_256
.p2align 3
.globl rv64i_zvkned_cbc_decrypt
.type rv64i_zvkned_cbc_decrypt,@function
rv64i_zvkned_cbc_decrypt:
    # check whether the length is a multiple of 16 and >= 16
    li t1, 16
    blt a2, t1, L_end
    andi t1, a2, 15
    bnez t1, L_end

    # Load number of rounds
    lwu t2, 240(a3)

    # Get proper routine for key size
    li t0, 10
    beq t2, t0, L_cbc_dec_128

    li t0, 12
    beq t2, t0, L_cbc_dec_192

    li t0, 14
    beq t2, t0, L_cbc_dec_256

    ret
.size rv64i_zvkned_cbc_decrypt,.-rv64i_zvkned_cbc_decrypt
.p2align 3
L_cbc_dec_128:
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


    # Load IV.
    .word 34039815

    .word 33909767
    .word 1577846999
    j 2f

1:
    .word 33909767
    .word 1577846999
    addi a1, a1, 16

2:
    # AES body
        .word 2796792951   # with round key w[40,43]
    .word 2795514999  # with round key w[36,39]
    .word 2794466423   # with round key w[32,35]
    .word 2793417847   # with round key w[28,31]
    .word 2792369271   # with round key w[24,27]
    .word 2791320695   # with round key w[20,23]
    .word 2790272119   # with round key w[16,19]
    .word 2789223543   # with round key w[12,15]
    .word 2788174967   # with round key w[ 8,11]
    .word 2787126391   # with round key w[ 4, 7]
    .word 2786110583   # with round key w[ 0, 3]


    .word 797445207
    .word 33942567
    .word 1577617495

    addi a2, a2, -16
    addi a0, a0, 16

    bnez a2, 1b

    .word 34039847

    ret
.size L_cbc_dec_128,.-L_cbc_dec_128
.p2align 3
L_cbc_dec_192:
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


    # Load IV.
    .word 34039815

    .word 33909767
    .word 1577846999
    j 2f

1:
    .word 33909767
    .word 1577846999
    addi a1, a1, 16

2:
    # AES body
        .word 2798890103    # with round key w[48,51]
    .word 2797612151   # with round key w[44,47]
    .word 2796563575   # with round key w[40,43]
    .word 2795514999   # with round key w[36,39]
    .word 2794466423    # with round key w[32,35]
    .word 2793417847    # with round key w[28,31]
    .word 2792369271    # with round key w[24,27]
    .word 2791320695    # with round key w[20,23]
    .word 2790272119    # with round key w[16,19]
    .word 2789223543    # with round key w[12,15]
    .word 2788174967    # with round key w[ 8,11]
    .word 2787126391    # with round key w[ 4, 7]
    .word 2786110583    # with round key w[ 0, 3]


    .word 797445207
    .word 33942567
    .word 1577617495

    addi a2, a2, -16
    addi a0, a0, 16

    bnez a2, 1b

    .word 34039847

    ret
.size L_cbc_dec_192,.-L_cbc_dec_192
.p2align 3
L_cbc_dec_256:
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


    # Load IV.
    .word 34039815

    .word 33909767
    .word 1577846999
    j 2f

1:
    .word 33909767
    .word 1577846999
    addi a1, a1, 16

2:
    # AES body
        .word 2800987255    # with round key w[56,59]
    .word 2799709303   # with round key w[52,55]
    .word 2798660727   # with round key w[48,51]
    .word 2797612151   # with round key w[44,47]
    .word 2796563575   # with round key w[40,43]
    .word 2795514999   # with round key w[36,39]
    .word 2794466423    # with round key w[32,35]
    .word 2793417847    # with round key w[28,31]
    .word 2792369271    # with round key w[24,27]
    .word 2791320695    # with round key w[20,23]
    .word 2790272119    # with round key w[16,19]
    .word 2789223543    # with round key w[12,15]
    .word 2788174967    # with round key w[ 8,11]
    .word 2787126391    # with round key w[ 4, 7]
    .word 2786110583    # with round key w[ 0, 3]


    .word 797445207
    .word 33942567
    .word 1577617495

    addi a2, a2, -16
    addi a0, a0, 16

    bnez a2, 1b

    .word 34039847

    ret
.size L_cbc_dec_256,.-L_cbc_dec_256
.p2align 3
.globl rv64i_zvkned_ecb_encrypt
.type rv64i_zvkned_ecb_encrypt,@function
rv64i_zvkned_ecb_encrypt:
    # Make the LEN become e32 length.
    srli t3, a2, 2

    # Load number of rounds
    lwu t2, 240(a3)

    # Get proper routine for key size
    li t0, 10
    beq t2, t0, L_ecb_enc_128

    li t0, 12
    beq t2, t0, L_ecb_enc_192

    li t0, 14
    beq t2, t0, L_ecb_enc_256

    ret
.size rv64i_zvkned_ecb_encrypt,.-rv64i_zvkned_ecb_encrypt
.p2align 3
L_ecb_enc_128:
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


1:
    .word 221149271
    slli t0, a6, 2
    sub t3, t3, a6

    .word 33909767

    # AES body
        .word 2786307191     # with round key w[ 0, 3]
    .word 2787191927    # with round key w[ 4, 7]
    .word 2788240503    # with round key w[ 8,11]
    .word 2789289079    # with round key w[12,15]
    .word 2790337655    # with round key w[16,19]
    .word 2791386231    # with round key w[20,23]
    .word 2792434807    # with round key w[24,27]
    .word 2793483383    # with round key w[28,31]
    .word 2794531959    # with round key w[32,35]
    .word 2795580535   # with round key w[36,39]
    .word 2796661879   # with round key w[40,43]


    .word 33942567

    add a0, a0, t0
    add a1, a1, t0

    bnez t3, 1b

    ret
.size L_ecb_enc_128,.-L_ecb_enc_128
.p2align 3
L_ecb_enc_192:
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


1:
    .word 221149271
    slli t0, a6, 2
    sub t3, t3, a6

    .word 33909767

    # AES body
        .word 2786307191     # with round key w[ 0, 3]
    .word 2787191927    # with round key w[ 4, 7]
    .word 2788240503    # with round key w[ 8,11]
    .word 2789289079    # with round key w[12,15]
    .word 2790337655    # with round key w[16,19]
    .word 2791386231    # with round key w[20,23]
    .word 2792434807    # with round key w[24,27]
    .word 2793483383    # with round key w[28,31]
    .word 2794531959    # with round key w[32,35]
    .word 2795580535   # with round key w[36,39]
    .word 2796629111   # with round key w[40,43]
    .word 2797677687   # with round key w[44,47]
    .word 2798759031   # with round key w[48,51]


    .word 33942567

    add a0, a0, t0
    add a1, a1, t0

    bnez t3, 1b

    ret
.size L_ecb_enc_192,.-L_ecb_enc_192
.p2align 3
L_ecb_enc_256:
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


1:
    .word 221149271
    slli t0, a6, 2
    sub t3, t3, a6

    .word 33909767

    # AES body
        .word 2786307191     # with round key w[ 0, 3]
    .word 2787191927    # with round key w[ 4, 7]
    .word 2788240503    # with round key w[ 8,11]
    .word 2789289079    # with round key w[12,15]
    .word 2790337655    # with round key w[16,19]
    .word 2791386231    # with round key w[20,23]
    .word 2792434807    # with round key w[24,27]
    .word 2793483383    # with round key w[28,31]
    .word 2794531959    # with round key w[32,35]
    .word 2795580535   # with round key w[36,39]
    .word 2796629111   # with round key w[40,43]
    .word 2797677687   # with round key w[44,47]
    .word 2798726263   # with round key w[48,51]
    .word 2799774839   # with round key w[52,55]
    .word 2800856183   # with round key w[56,59]


    .word 33942567

    add a0, a0, t0
    add a1, a1, t0

    bnez t3, 1b

    ret
.size L_ecb_enc_256,.-L_ecb_enc_256
.p2align 3
.globl rv64i_zvkned_ecb_decrypt
.type rv64i_zvkned_ecb_decrypt,@function
rv64i_zvkned_ecb_decrypt:
    # Make the LEN become e32 length.
    srli t3, a2, 2

    # Load number of rounds
    lwu t2, 240(a3)

    # Get proper routine for key size
    li t0, 10
    beq t2, t0, L_ecb_dec_128

    li t0, 12
    beq t2, t0, L_ecb_dec_192

    li t0, 14
    beq t2, t0, L_ecb_dec_256

    ret
.size rv64i_zvkned_ecb_decrypt,.-rv64i_zvkned_ecb_decrypt
.p2align 3
L_ecb_dec_128:
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


1:
    .word 221149271
    slli t0, a6, 2
    sub t3, t3, a6

    .word 33909767

    # AES body
        .word 2796792951   # with round key w[40,43]
    .word 2795514999  # with round key w[36,39]
    .word 2794466423   # with round key w[32,35]
    .word 2793417847   # with round key w[28,31]
    .word 2792369271   # with round key w[24,27]
    .word 2791320695   # with round key w[20,23]
    .word 2790272119   # with round key w[16,19]
    .word 2789223543   # with round key w[12,15]
    .word 2788174967   # with round key w[ 8,11]
    .word 2787126391   # with round key w[ 4, 7]
    .word 2786110583   # with round key w[ 0, 3]


    .word 33942567

    add a0, a0, t0
    add a1, a1, t0

    bnez t3, 1b

    ret
.size L_ecb_dec_128,.-L_ecb_dec_128
.p2align 3
L_ecb_dec_192:
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


1:
    .word 221149271
    slli t0, a6, 2
    sub t3, t3, a6

    .word 33909767

    # AES body
        .word 2798890103    # with round key w[48,51]
    .word 2797612151   # with round key w[44,47]
    .word 2796563575   # with round key w[40,43]
    .word 2795514999   # with round key w[36,39]
    .word 2794466423    # with round key w[32,35]
    .word 2793417847    # with round key w[28,31]
    .word 2792369271    # with round key w[24,27]
    .word 2791320695    # with round key w[20,23]
    .word 2790272119    # with round key w[16,19]
    .word 2789223543    # with round key w[12,15]
    .word 2788174967    # with round key w[ 8,11]
    .word 2787126391    # with round key w[ 4, 7]
    .word 2786110583    # with round key w[ 0, 3]


    .word 33942567

    add a0, a0, t0
    add a1, a1, t0

    bnez t3, 1b

    ret
.size L_ecb_dec_192,.-L_ecb_dec_192
.p2align 3
L_ecb_dec_256:
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


1:
    .word 221149271
    slli t0, a6, 2
    sub t3, t3, a6

    .word 33909767

    # AES body
        .word 2800987255    # with round key w[56,59]
    .word 2799709303   # with round key w[52,55]
    .word 2798660727   # with round key w[48,51]
    .word 2797612151   # with round key w[44,47]
    .word 2796563575   # with round key w[40,43]
    .word 2795514999   # with round key w[36,39]
    .word 2794466423    # with round key w[32,35]
    .word 2793417847    # with round key w[28,31]
    .word 2792369271    # with round key w[24,27]
    .word 2791320695    # with round key w[20,23]
    .word 2790272119    # with round key w[16,19]
    .word 2789223543    # with round key w[12,15]
    .word 2788174967    # with round key w[ 8,11]
    .word 2787126391    # with round key w[ 4, 7]
    .word 2786110583    # with round key w[ 0, 3]


    .word 33942567

    add a0, a0, t0
    add a1, a1, t0

    bnez t3, 1b

    ret
.size L_ecb_dec_256,.-L_ecb_dec_256
.p2align 3
.globl rv64i_zvkned_set_encrypt_key
.type rv64i_zvkned_set_encrypt_key,@function
rv64i_zvkned_set_encrypt_key:
    # Get proper routine for key size
    li t1, 256
    beq a1, t1, L_set_key_256
    li t1, 128
    beq a1, t1, L_set_key_128

    j L_fail_m2

.size rv64i_zvkned_set_encrypt_key,.-rv64i_zvkned_set_encrypt_key
.p2align 3
.globl rv64i_zvkned_set_decrypt_key
.type rv64i_zvkned_set_decrypt_key,@function
rv64i_zvkned_set_decrypt_key:
    # Get proper routine for key size
    li t1, 256
    beq a1, t1, L_set_key_256
    li t1, 128
    beq a1, t1, L_set_key_128

    j L_fail_m2

.size rv64i_zvkned_set_decrypt_key,.-rv64i_zvkned_set_decrypt_key
.p2align 3
L_set_key_128:
    # Store the number of rounds
    li t2, 10
    sw t2, 240(a2)

    .word 0xc1027057

    # Load the key
    .word 33907975

    # Generate keys for round 2-11 into registers v11-v20.
    .word 2325784055   # v11 <- rk2  (w[ 4, 7])
    .word 2326865527   # v12 <- rk3  (w[ 8,11])
    .word 2327946999   # v13 <- rk4  (w[12,15])
    .word 2329028471   # v14 <- rk5  (w[16,19])
    .word 2330109943   # v15 <- rk6  (w[20,23])
    .word 2331191415   # v16 <- rk7  (w[24,27])
    .word 2332272887   # v17 <- rk8  (w[28,31])
    .word 2333354359   # v18 <- rk9  (w[32,35])
    .word 2334435831   # v19 <- rk10 (w[36,39])
    .word 2335517303  # v20 <- rk11 (w[40,43])

    # Store the round keys
    .word 33973543
    addi a2, a2, 16
    .word 33973671
    addi a2, a2, 16
    .word 33973799
    addi a2, a2, 16
    .word 33973927
    addi a2, a2, 16
    .word 33974055
    addi a2, a2, 16
    .word 33974183
    addi a2, a2, 16
    .word 33974311
    addi a2, a2, 16
    .word 33974439
    addi a2, a2, 16
    .word 33974567
    addi a2, a2, 16
    .word 33974695
    addi a2, a2, 16
    .word 33974823

    li a0, 1
    ret
.size L_set_key_128,.-L_set_key_128
.p2align 3
L_set_key_256:
    # Store the number of rounds
    li t2, 14
    sw t2, 240(a2)

    .word 0xc1027057

    # Load the key
    .word 33907975
    addi a0, a0, 16
    .word 33908103

    .word 1577387607
    .word 2863736439
    .word 1577420503
    .word 2864817911
    .word 1577453399
    .word 2865899383
    .word 1577486295
    .word 2866980855
    .word 1577519191
    .word 2868062327
    .word 1577552087
    .word 2869143799
    .word 1577584983
    .word 2870225271
    .word 1577617879
    .word 2871306743
    .word 1577650775
    .word 2872388215
    .word 1577683671
    .word 2873469687
    .word 1577716567
    .word 2874551159
    .word 1577749463
    .word 2875632631
    .word 1577782359
    .word 2876714103

    .word 33973543
    addi a2, a2, 16
    .word 33973671
    addi a2, a2, 16
    .word 33973799
    addi a2, a2, 16
    .word 33973927
    addi a2, a2, 16
    .word 33974055
    addi a2, a2, 16
    .word 33974183
    addi a2, a2, 16
    .word 33974311
    addi a2, a2, 16
    .word 33974439
    addi a2, a2, 16
    .word 33974567
    addi a2, a2, 16
    .word 33974695
    addi a2, a2, 16
    .word 33974823
    addi a2, a2, 16
    .word 33974951
    addi a2, a2, 16
    .word 33975079
    addi a2, a2, 16
    .word 33975207
    addi a2, a2, 16
    .word 33975335

    li a0, 1
    ret
.size L_set_key_256,.-L_set_key_256
.p2align 3
.globl rv64i_zvkned_encrypt
.type rv64i_zvkned_encrypt,@function
rv64i_zvkned_encrypt:
    # Load number of rounds
    lwu t5, 240(a2)

    # Get proper routine for key size
    li t6, 14
    beq t5, t6, L_enc_256
    li t6, 10
    beq t5, t6, L_enc_128
    li t6, 12
    beq t5, t6, L_enc_192

    j L_fail_m2
.size rv64i_zvkned_encrypt,.-rv64i_zvkned_encrypt
.p2align 3
L_enc_128:
    .word 3439489111

    .word 33906823

    .word 33973511
    .word 2795741431    # with round key w[ 0, 3]
    addi a2, a2, 16
    .word 33973639
    .word 2796626167   # with round key w[ 4, 7]
    addi a2, a2, 16
    .word 33973767
    .word 2797674743   # with round key w[ 8,11]
    addi a2, a2, 16
    .word 33973895
    .word 2798723319   # with round key w[12,15]
    addi a2, a2, 16
    .word 33974023
    .word 2799771895   # with round key w[16,19]
    addi a2, a2, 16
    .word 33974151
    .word 2800820471   # with round key w[20,23]
    addi a2, a2, 16
    .word 33974279
    .word 2801869047   # with round key w[24,27]
    addi a2, a2, 16
    .word 33974407
    .word 2802917623   # with round key w[28,31]
    addi a2, a2, 16
    .word 33974535
    .word 2803966199   # with round key w[32,35]
    addi a2, a2, 16
    .word 33974663
    .word 2805014775   # with round key w[36,39]
    addi a2, a2, 16
    .word 33974791
    .word 2806096119   # with round key w[40,43]

    .word 33939623

    ret
.size L_enc_128,.-L_enc_128
.p2align 3
L_enc_192:
    .word 3439489111

    .word 33906823

    .word 33973511
    .word 2795741431     # with round key w[ 0, 3]
    addi a2, a2, 16
    .word 33973639
    .word 2796626167
    addi a2, a2, 16
    .word 33973767
    .word 2797674743
    addi a2, a2, 16
    .word 33973895
    .word 2798723319
    addi a2, a2, 16
    .word 33974023
    .word 2799771895
    addi a2, a2, 16
    .word 33974151
    .word 2800820471
    addi a2, a2, 16
    .word 33974279
    .word 2801869047
    addi a2, a2, 16
    .word 33974407
    .word 2802917623
    addi a2, a2, 16
    .word 33974535
    .word 2803966199
    addi a2, a2, 16
    .word 33974663
    .word 2805014775
    addi a2, a2, 16
    .word 33974791
    .word 2806063351
    addi a2, a2, 16
    .word 33974919
    .word 2807111927
    addi a2, a2, 16
    .word 33975047
    .word 2808193271

    .word 33939623
    ret
.size L_enc_192,.-L_enc_192
.p2align 3
L_enc_256:
    .word 3439489111

    .word 33906823

    .word 33973511
    .word 2795741431     # with round key w[ 0, 3]
    addi a2, a2, 16
    .word 33973639
    .word 2796626167
    addi a2, a2, 16
    .word 33973767
    .word 2797674743
    addi a2, a2, 16
    .word 33973895
    .word 2798723319
    addi a2, a2, 16
    .word 33974023
    .word 2799771895
    addi a2, a2, 16
    .word 33974151
    .word 2800820471
    addi a2, a2, 16
    .word 33974279
    .word 2801869047
    addi a2, a2, 16
    .word 33974407
    .word 2802917623
    addi a2, a2, 16
    .word 33974535
    .word 2803966199
    addi a2, a2, 16
    .word 33974663
    .word 2805014775
    addi a2, a2, 16
    .word 33974791
    .word 2806063351
    addi a2, a2, 16
    .word 33974919
    .word 2807111927
    addi a2, a2, 16
    .word 33975047
    .word 2808160503
    addi a2, a2, 16
    .word 33975175
    .word 2809209079
    addi a2, a2, 16
    .word 33975303
    .word 2810290423

    .word 33939623
    ret
.size L_enc_256,.-L_enc_256
.p2align 3
.globl rv64i_zvkned_decrypt
.type rv64i_zvkned_decrypt,@function
rv64i_zvkned_decrypt:
    # Load number of rounds
    lwu t5, 240(a2)

    # Get proper routine for key size
    li t6, 14
    beq t5, t6, L_dec_256
    li t6, 10
    beq t5, t6, L_dec_128
    li t6, 12
    beq t5, t6, L_dec_192

    j L_fail_m2
.size rv64i_zvkned_decrypt,.-rv64i_zvkned_decrypt
.p2align 3
L_dec_128:
    .word 3439489111

    .word 33906823

    addi a2, a2, 160
    .word 33974791
    .word 2806227191    # with round key w[40,43]
    addi a2, a2, -16
    .word 33974663
    .word 2804949239   # with round key w[36,39]
    addi a2, a2, -16
    .word 33974535
    .word 2803900663   # with round key w[32,35]
    addi a2, a2, -16
    .word 33974407
    .word 2802852087   # with round key w[28,31]
    addi a2, a2, -16
    .word 33974279
    .word 2801803511   # with round key w[24,27]
    addi a2, a2, -16
    .word 33974151
    .word 2800754935   # with round key w[20,23]
    addi a2, a2, -16
    .word 33974023
    .word 2799706359   # with round key w[16,19]
    addi a2, a2, -16
    .word 33973895
    .word 2798657783   # with round key w[12,15]
    addi a2, a2, -16
    .word 33973767
    .word 2797609207   # with round key w[ 8,11]
    addi a2, a2, -16
    .word 33973639
    .word 2796560631   # with round key w[ 4, 7]
    addi a2, a2, -16
    .word 33973511
    .word 2795544823   # with round key w[ 0, 3]

    .word 33939623

    ret
.size L_dec_128,.-L_dec_128
.p2align 3
L_dec_192:
    .word 3439489111

    .word 33906823

    addi a2, a2, 192
    .word 33975047
    .word 2808324343    # with round key w[48,51]
    addi a2, a2, -16
    .word 33974919
    .word 2807046391   # with round key w[44,47]
    addi a2, a2, -16
    .word 33974791
    .word 2805997815    # with round key w[40,43]
    addi a2, a2, -16
    .word 33974663
    .word 2804949239   # with round key w[36,39]
    addi a2, a2, -16
    .word 33974535
    .word 2803900663   # with round key w[32,35]
    addi a2, a2, -16
    .word 33974407
    .word 2802852087   # with round key w[28,31]
    addi a2, a2, -16
    .word 33974279
    .word 2801803511   # with round key w[24,27]
    addi a2, a2, -16
    .word 33974151
    .word 2800754935   # with round key w[20,23]
    addi a2, a2, -16
    .word 33974023
    .word 2799706359   # with round key w[16,19]
    addi a2, a2, -16
    .word 33973895
    .word 2798657783   # with round key w[12,15]
    addi a2, a2, -16
    .word 33973767
    .word 2797609207   # with round key w[ 8,11]
    addi a2, a2, -16
    .word 33973639
    .word 2796560631   # with round key w[ 4, 7]
    addi a2, a2, -16
    .word 33973511
    .word 2795544823   # with round key w[ 0, 3]

    .word 33939623

    ret
.size L_dec_192,.-L_dec_192
.p2align 3
L_dec_256:
    .word 3439489111

    .word 33906823

    addi a2, a2, 224
    .word 33975303
    .word 2810421495    # with round key w[56,59]
    addi a2, a2, -16
    .word 33975175
    .word 2809143543   # with round key w[52,55]
    addi a2, a2, -16
    .word 33975047
    .word 2808094967    # with round key w[48,51]
    addi a2, a2, -16
    .word 33974919
    .word 2807046391   # with round key w[44,47]
    addi a2, a2, -16
    .word 33974791
    .word 2805997815    # with round key w[40,43]
    addi a2, a2, -16
    .word 33974663
    .word 2804949239   # with round key w[36,39]
    addi a2, a2, -16
    .word 33974535
    .word 2803900663   # with round key w[32,35]
    addi a2, a2, -16
    .word 33974407
    .word 2802852087   # with round key w[28,31]
    addi a2, a2, -16
    .word 33974279
    .word 2801803511   # with round key w[24,27]
    addi a2, a2, -16
    .word 33974151
    .word 2800754935   # with round key w[20,23]
    addi a2, a2, -16
    .word 33974023
    .word 2799706359   # with round key w[16,19]
    addi a2, a2, -16
    .word 33973895
    .word 2798657783   # with round key w[12,15]
    addi a2, a2, -16
    .word 33973767
    .word 2797609207   # with round key w[ 8,11]
    addi a2, a2, -16
    .word 33973639
    .word 2796560631   # with round key w[ 4, 7]
    addi a2, a2, -16
    .word 33973511
    .word 2795544823   # with round key w[ 0, 3]

    .word 33939623

    ret
.size L_dec_256,.-L_dec_256
L_fail_m2:
    li a0, -2
    ret
.size L_fail_m2,.-L_fail_m2

L_end:
  ret
.size L_end,.-L_end
