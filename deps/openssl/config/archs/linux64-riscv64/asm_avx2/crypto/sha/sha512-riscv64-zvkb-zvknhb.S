.text
.p2align 2
.globl sha512_block_data_order_zvkb_zvknhb
.type sha512_block_data_order_zvkb_zvknhb,@function
sha512_block_data_order_zvkb_zvknhb:
    .word 3448926295

    # H is stored as {a,b,c,d},{e,f,g,h}, but we need {f,e,b,a},{h,g,d,c}
    # The dst vtype is e64m2 and the index vtype is e8mf4.
    # We use index-load with the following index pattern at v1.
    #   i8 index:
    #     40, 32, 8, 0
    # Instead of setting the i8 index, we could use a single 32bit
    # little-endian value to cover the 4xi8 index.
    #   i32 value:
    #     0x 00 08 20 28
    li t4, 0x00082028
    .word 3439390807
    .word 1578025175

    addi t3, a0, 16

    # Use index-load to get {f,e,b,a},{h,g,d,c}
    .word 3448926295
    .word 102042375
    .word 102632455

    # Setup v0 mask for the vmerge to replace the first word (idx==0) in key-scheduling.
    # The AVL is 4 in SHA, so we could use a single e8(8 element masking) for masking.
    .word 3422613591
    .word 1577103447

    .word 3448926295

L_round_loop:
    # Load round constants K512
    la a3, K512

    # Decrement length by 1
    addi a2, a2, -1

    # Keep the current state as we need it later: H' = H+{a',b',c',...,h'}.
    .word 1577782615
    .word 1577848407

    # Load the 1024-bits of the message block in v10-v16 and perform the endian
    # swap.
    .word 33944839
    .word 1252304215
    addi a1, a1, 32
    .word 33945095
    .word 1254401623
    addi a1, a1, 32
    .word 33945351
    .word 1256499031
    addi a1, a1, 32
    .word 33945607
    .word 1258596439
    addi a1, a1, 32

    .rept 4
    # Quad-round 0 (+0, v10->v12->v14->v16)
    .word 34011655
    addi a3, a3, 32
    .word 54856023
    .word 3211340919
    .word 3146328951
    .word 1558579543
    .word 3072861559

    # Quad-round 1 (+1, v12->v14->v16->v10)
    .word 34011655
    addi a3, a3, 32
    .word 54921559
    .word 3211340919
    .word 3146328951
    .word 1560742231
    .word 3072665207

    # Quad-round 2 (+2, v14->v16->v10->v12)
    .word 34011655
    addi a3, a3, 32
    .word 54987095
    .word 3211340919
    .word 3146328951
    .word 1554516311
    .word 3072730999

    # Quad-round 3 (+3, v16->v10->v12->v14)
    .word 34011655
    addi a3, a3, 32
    .word 55052631
    .word 3211340919
    .word 3146328951
    .word 1556416855
    .word 3072796791
    .endr

    # Quad-round 16 (+0, v10->v12->v14->v16)
    # Note that we stop generating new message schedule words (Wt, v10-16)
    # as we already generated all the words we end up consuming (i.e., W[79:76]).
    .word 34011655
    addi a3, a3, 32
    .word 54856023
    .word 3211340919
    .word 3146328951

    # Quad-round 17 (+1, v12->v14->v16->v10)
    .word 34011655
    addi a3, a3, 32
    .word 54921559
    .word 3211340919
    .word 3146328951

    # Quad-round 18 (+2, v14->v16->v10->v12)
    .word 34011655
    addi a3, a3, 32
    .word 54987095
    .word 3211340919
    .word 3146328951

    # Quad-round 19 (+3, v16->v10->v12->v14)
    .word 34011655
    # No t1 increment needed.
    .word 55052631
    .word 3211340919
    .word 3146328951

    # H' = H+{a',b',c',...,h'}
    .word 61541207
    .word 63704151
    bnez a2, L_round_loop

    # Store {f,e,b,a},{h,g,d,c} back to {a,b,c,d},{e,f,g,h}.
    .word 102042407
    .word 102632487

    ret
.size sha512_block_data_order_zvkb_zvknhb,.-sha512_block_data_order_zvkb_zvknhb

.p2align 3
.type K512,@object
K512:
    .dword 0x428a2f98d728ae22, 0x7137449123ef65cd
    .dword 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc
    .dword 0x3956c25bf348b538, 0x59f111f1b605d019
    .dword 0x923f82a4af194f9b, 0xab1c5ed5da6d8118
    .dword 0xd807aa98a3030242, 0x12835b0145706fbe
    .dword 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2
    .dword 0x72be5d74f27b896f, 0x80deb1fe3b1696b1
    .dword 0x9bdc06a725c71235, 0xc19bf174cf692694
    .dword 0xe49b69c19ef14ad2, 0xefbe4786384f25e3
    .dword 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65
    .dword 0x2de92c6f592b0275, 0x4a7484aa6ea6e483
    .dword 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5
    .dword 0x983e5152ee66dfab, 0xa831c66d2db43210
    .dword 0xb00327c898fb213f, 0xbf597fc7beef0ee4
    .dword 0xc6e00bf33da88fc2, 0xd5a79147930aa725
    .dword 0x06ca6351e003826f, 0x142929670a0e6e70
    .dword 0x27b70a8546d22ffc, 0x2e1b21385c26c926
    .dword 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df
    .dword 0x650a73548baf63de, 0x766a0abb3c77b2a8
    .dword 0x81c2c92e47edaee6, 0x92722c851482353b
    .dword 0xa2bfe8a14cf10364, 0xa81a664bbc423001
    .dword 0xc24b8b70d0f89791, 0xc76c51a30654be30
    .dword 0xd192e819d6ef5218, 0xd69906245565a910
    .dword 0xf40e35855771202a, 0x106aa07032bbd1b8
    .dword 0x19a4c116b8d2d0c8, 0x1e376c085141ab53
    .dword 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8
    .dword 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb
    .dword 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3
    .dword 0x748f82ee5defb2fc, 0x78a5636f43172f60
    .dword 0x84c87814a1f0ab72, 0x8cc702081a6439ec
    .dword 0x90befffa23631e28, 0xa4506cebde82bde9
    .dword 0xbef9a3f7b2c67915, 0xc67178f2e372532b
    .dword 0xca273eceea26619c, 0xd186b8c721c0c207
    .dword 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178
    .dword 0x06f067aa72176fba, 0x0a637dc5a2c898a6
    .dword 0x113f9804bef90dae, 0x1b710b35131c471b
    .dword 0x28db77f523047d84, 0x32caab7b40c72493
    .dword 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c
    .dword 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a
    .dword 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
.size K512,.-K512
