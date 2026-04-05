.text
.text
.p2align 3
.globl ossl_hwsm3_block_data_order_zvksh
.type ossl_hwsm3_block_data_order_zvksh,@function
ossl_hwsm3_block_data_order_zvksh:
    .word 3440668759

    # Load initial state of hash context (c->A-H).
    .word 33906695
    .word 1241817175

L_sm3_loop:
    # Copy the previous state to v2.
    # It will be XOR'ed with the current state at the end of the round.
    .word 1577058647

    # Load the 64B block in 2x32B chunks.
    .word 33940231 # v6 := {w7, ..., w0}
    addi a1, a1, 32

    .word 33940487 # v8 := {w15, ..., w8}
    addi a1, a1, 32

    addi a2, a2, -1

    # As vsm3c consumes only w0, w1, w4, w5 we need to slide the input
    # 2 elements down so we process elements w2, w3, w6, w7
    # This will be repeated for each odd round.
    .word 1046557271 # v4 := {X, X, w7, ..., w2}

    .word 2925535351
    .word 2923470967

    # Prepare a vector with {w11, ..., w4}
    .word 1044460119 # v4 := {X, X, X, X, w7, ..., w4}
    .word 981611095   # v4 := {w11, w10, w9, w8, w7, w6, w5, w4}

    .word 2923503735
    .word 1044460119 # v4 := {X, X, w11, w10, w9, w8, w7, w6}
    .word 2923536503

    .word 2927763575
    .word 1048654423 # v4 := {X, X, w15, w14, w13, w12, w11, w10}
    .word 2923602039

    .word 2189632375   # v6 := {w23, w22, w21, w20, w19, w18, w17, w16}

    # Prepare a register with {w19, w18, w17, w16, w15, w14, w13, w12}
    .word 1044460119 # v4 := {X, X, X, X, w15, w14, w13, w12}
    .word 979513943   # v4 := {w19, w18, w17, w16, w15, w14, w13, w12}

    .word 2923634807
    .word 1044460119 # v4 := {X, X, w19, w18, w17, w16, w15, w14}
    .word 2923667575

    .word 2925797495
    .word 1046557271 # v4 := {X, X, w23, w22, w21, w20, w19, w18}
    .word 2923733111

    .word 2187601015   # v8 := {w31, w30, w29, w28, w27, w26, w25, w24}

    # Prepare a register with {w27, w26, w25, w24, w23, w22, w21, w20}
    .word 1044460119 # v4 := {X, X, X, X, w23, w22, w21, w20}
    .word 981611095   # v4 := {w27, w26, w25, w24, w23, w22, w21, w20}

    .word 2923765879
    .word 1044460119 # v4 := {X, X, w27, w26, w25, w24, w23, w22}
    .word 2923798647

    .word 2928025719
    .word 1048654423 # v4 := {x, X, w31, w30, w29, w28, w27, w26}
    .word 2923864183

    .word 2189632375   # v6 := {w32, w33, w34, w35, w36, w37, w38, w39}

    # Prepare a register with {w35, w34, w33, w32, w31, w30, w29, w28}
    .word 1044460119 # v4 := {X, X, X, X, w31, w30, w29, w28}
    .word 979513943   # v4 := {w35, w34, w33, w32, w31, w30, w29, w28}

    .word 2923896951
    .word 1044460119 # v4 := {X, X, w35, w34, w33, w32, w31, w30}
    .word 2923929719

    .word 2926059639
    .word 1046557271 # v4 := {X, X, w39, w38, w37, w36, w35, w34}
    .word 2923995255

    .word 2187601015   # v8 := {w47, w46, w45, w44, w43, w42, w41, w40}

    # Prepare a register with {w43, w42, w41, w40, w39, w38, w37, w36}
    .word 1044460119 # v4 := {X, X, X, X, w39, w38, w37, w36}
    .word 981611095   # v4 := {w43, w42, w41, w40, w39, w38, w37, w36}

    .word 2924028023
    .word 1044460119 # v4 := {X, X, w43, w42, w41, w40, w39, w38}
    .word 2924060791

    .word 2928287863
    .word 1048654423 # v4 := {X, X, w47, w46, w45, w44, w43, w42}
    .word 2924126327

    .word 2189632375   # v6 := {w55, w54, w53, w52, w51, w50, w49, w48}

    # Prepare a register with {w51, w50, w49, w48, w47, w46, w45, w44}
    .word 1044460119 # v4 := {X, X, X, X, w47, w46, w45, w44}
    .word 979513943   # v4 := {w51, w50, w49, w48, w47, w46, w45, w44}

    .word 2924159095
    .word 1044460119 # v4 := {X, X, w51, w50, w49, w48, w47, w46}
    .word 2924191863

    .word 2926321783
    .word 1046557271 # v4 := {X, X, w55, w54, w53, w52, w51, w50}
    .word 2924257399

    .word 2187601015   # v8 := {w63, w62, w61, w60, w59, w58, w57, w56}

    # Prepare a register with {w59, w58, w57, w56, w55, w54, w53, w52}
    .word 1044460119 # v4 := {X, X, X, X, w55, w54, w53, w52}
    .word 981611095   # v4 := {w59, w58, w57, w56, w55, w54, w53, w52}

    .word 2924290167
    .word 1044460119 # v4 := {X, X, w59, w58, w57, w56, w55, w54}
    .word 2924322935

    .word 2928550007
    .word 1048654423 # v4 := {X, X, w63, w62, w61, w60, w59, w58}
    .word 2924388471

    .word 2189632375   # v6 := {w71, w70, w69, w68, w67, w66, w65, w64}

    # Prepare a register with {w67, w66, w65, w64, w63, w62, w61, w60}
    .word 1044460119 # v4 := {X, X, X, X, w63, w62, w61, w60}
    .word 979513943   # v4 := {w67, w66, w65, w64, w63, w62, w61, w60}

    .word 2924421239
    .word 1044460119 # v4 := {X, X, w67, w66, w65, w64, w63, w62}
    .word 2924454007

    # XOR in the previous state.
    .word 771817559

    bnez a2, L_sm3_loop     # Check if there are any more block to process
L_sm3_end:
    .word 1241817175
    .word 33906727
    ret

.size ossl_hwsm3_block_data_order_zvksh,.-ossl_hwsm3_block_data_order_zvksh
