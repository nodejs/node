.text
.p2align 2
.globl sha256_block_data_order_zvkb_zvknha_or_zvknhb
.type   sha256_block_data_order_zvkb_zvknha_or_zvknhb,@function
sha256_block_data_order_zvkb_zvknha_or_zvknhb:
    .word 3439489111

        la a3, K256 # Load round constants K256
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
    addi a3, a3, 16
    .word 34007047
    addi a3, a3, 16
    .word 34007175
    addi a3, a3, 16
    .word 34007303
    addi a3, a3, 16
    .word 34007431
    addi a3, a3, 16
    .word 34007559
    addi a3, a3, 16
    .word 34007687
    addi a3, a3, 16
    .word 34007815
    addi a3, a3, 16
    .word 34007943
    addi a3, a3, 16
    .word 34008071
    addi a3, a3, 16
    .word 34008199


    # H is stored as {a,b,c,d},{e,f,g,h}, but we need {f,e,b,a},{h,g,d,c}
    # The dst vtype is e32m1 and the index vtype is e8mf4.
    # We use index-load with the following index pattern at v26.
    #   i8 index:
    #     20, 16, 4, 0
    # Instead of setting the i8 index, we could use a single 32bit
    # little-endian value to cover the 4xi8 index.
    #   i32 value:
    #     0x 00 04 10 14
    li t4, 0x00041014
    .word 3439390807
    .word 1578028375

    addi t3, a0, 8

    # Use index-load to get {f,e,b,a},{h,g,d,c}
    .word 3439489111
    .word 128254727
    .word 128844679

    # Setup v0 mask for the vmerge to replace the first word (idx==0) in key-scheduling.
    # The AVL is 4 in SHA, so we could use a single e8(8 element masking) for masking.
    .word 3422613591
    .word 1577103447

    .word 3439489111

L_round_loop:
    # Decrement length by 1
    add a2, a2, -1

    # Keep the current state as we need it later: H' = H+{a',b',c',...,h'}.
    .word 1577258839
    .word 1577291735

    # Load the 512-bits of the message block in v1-v4 and perform
    # an endian swap on each 4 bytes element.
    .word 33939591
    .word 1242865879
    add a1, a1, 16
    .word 33939719
    .word 1243914583
    add a1, a1, 16
    .word 33939847
    .word 1244963287
    add a1, a1, 16
    .word 33939975
    .word 1246011991
    add a1, a1, 16

    # Quad-round 0 (+0, Wt from oldest to newest in v1->v2->v3->v4)
    .word 44073687
    .word 3194135543
    .word 3128075127
    .word 1546715863
    .word 3058835703  # Generate W[19:16]

    # Quad-round 1 (+1, v2->v3->v4->v1)
    .word 45155031
    .word 3194135543
    .word 3128075127
    .word 1547797207
    .word 3058737527  # Generate W[23:20]

    # Quad-round 2 (+2, v3->v4->v1->v2)
    .word 46236375
    .word 3194135543
    .word 3128075127
    .word 1544684247
    .word 3058770423  # Generate W[27:24]

    # Quad-round 3 (+3, v4->v1->v2->v3)
    .word 47317719
    .word 3194135543
    .word 3128075127
    .word 1545634519
    .word 3058803319  # Generate W[31:28]

    # Quad-round 4 (+0, v1->v2->v3->v4)
    .word 48267991
    .word 3194135543
    .word 3128075127
    .word 1546715863
    .word 3058835703  # Generate W[35:32]

    # Quad-round 5 (+1, v2->v3->v4->v1)
    .word 49349335
    .word 3194135543
    .word 3128075127
    .word 1547797207
    .word 3058737527  # Generate W[39:36]

    # Quad-round 6 (+2, v3->v4->v1->v2)
    .word 50430679
    .word 3194135543
    .word 3128075127
    .word 1544684247
    .word 3058770423  # Generate W[43:40]

    # Quad-round 7 (+3, v4->v1->v2->v3)
    .word 51512023
    .word 3194135543
    .word 3128075127
    .word 1545634519
    .word 3058803319  # Generate W[47:44]

    # Quad-round 8 (+0, v1->v2->v3->v4)
    .word 52462295
    .word 3194135543
    .word 3128075127
    .word 1546715863
    .word 3058835703  # Generate W[51:48]

    # Quad-round 9 (+1, v2->v3->v4->v1)
    .word 53543639
    .word 3194135543
    .word 3128075127
    .word 1547797207
    .word 3058737527  # Generate W[55:52]

    # Quad-round 10 (+2, v3->v4->v1->v2)
    .word 54624983
    .word 3194135543
    .word 3128075127
    .word 1544684247
    .word 3058770423  # Generate W[59:56]

    # Quad-round 11 (+3, v4->v1->v2->v3)
    .word 55706327
    .word 3194135543
    .word 3128075127
    .word 1545634519
    .word 3058803319  # Generate W[63:60]

    # Quad-round 12 (+0, v1->v2->v3->v4)
    # Note that we stop generating new message schedule words (Wt, v1-13)
    # as we already generated all the words we end up consuming (i.e., W[63:60]).
    .word 56656599
    .word 3194135543
    .word 3128075127

    # Quad-round 13 (+1, v2->v3->v4->v1)
    .word 57737943
    .word 3194135543
    .word 3128075127

    # Quad-round 14 (+2, v3->v4->v1->v2)
    .word 58819287
    .word 3194135543
    .word 3128075127

    # Quad-round 15 (+3, v4->v1->v2->v3)
    .word 59900631
    .word 3194135543
    .word 3128075127

    # H' = H+{a',b',c',...,h'}
    .word 65209175
    .word 66290647
    bnez a2, L_round_loop

    # Store {f,e,b,a},{h,g,d,c} back to {a,b,c,d},{e,f,g,h}.
    .word 128254759
    .word 128844711

    ret
.size sha256_block_data_order_zvkb_zvknha_or_zvknhb,.-sha256_block_data_order_zvkb_zvknha_or_zvknhb

.p2align 2
.type K256,@object
K256:
    .word 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
    .word 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
    .word 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
    .word 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
    .word 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
    .word 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
    .word 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
    .word 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
    .word 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
    .word 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
    .word 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
    .word 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
    .word 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
    .word 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
    .word 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
    .word 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
.size K256,.-K256
