.text
.text
.p2align 3
.globl ossl_hwsm3_block_data_order_zvksh
.type ossl_hwsm3_block_data_order_zvksh,@function
ossl_hwsm3_block_data_order_zvksh:
    # Obtain VLEN and select the corresponding branch
    csrr t0, vlenb
    srl t1, t0, 5
    beqz t1, ossl_hwsm3_block_data_order_zvksh_zvl128
    srl t1, t0, 6
    beqz t1, ossl_hwsm3_block_data_order_zvksh_zvl256
ossl_hwsm3_block_data_order_zvksh_zvl512:
    .word 3238293591
    .word 33910023
    .word 1269083479
    .word 3439882327
    la t0, ORDER_BY_ZVL512_DATA
    .word 33746695
    addi t0, t0, 64
    .word 33746823
    la t0, ORDER_BY_ZVL512_EXP
    .word 33746567
    addi t0, t0, 64
    .word 33746439
    srli a6 , a2, 1
    andi a2, a2, 1
    beqz a6 , ossl_hwsm3_block_data_order_zvksh_zvl256
L_sm3_loop_zvl512:
    # Use indexed loads (ORDER_BY_RVV512_DATA) to load two blocks in the
    # word order expected by the later vrgather/vsm3c stages.
    .word 132505607
    .word 133554311
    .word 839812311
    .word 849249623
    .word 840828375
    .word 715490647
    .word 850298327
    .word 840861271
    .word 852395735
    .word 2182095223
    .word 841877335
    .word 718735063
    .word 853444439
    .word 841910231
    .word 855541847
    .word 2183176695
    .word 842926295
    .word 721979479
    .word 856590551
    .word 842959191
    .word 858687959
    .word 2184258167
    .word 843975255
    .word 725223895
    .word 859736663
    .word 844008151
    .word 861834071
    .word 2185339639
    .word 845024215
    .word 728468311
    .word 862882775
    .word 845057111
    .word 864980183
    .word 2186421111
    .word 846073303
    .word 731745495
    .word 2187502583
    .word 2188584055
    .word 1577913815
    .word 3238293591
    .word 2919247223
    .word 2928717175
    .word 2929798519
    .word 2930879863
    .word 2920426871
    .word 2931993975
    .word 2933075319
    .word 2934156663
    .word 2921606519
    .word 2935270775
    .word 2936352119
    .word 2937433463
    .word 2922786167
    .word 2938547575
    .word 2939628919
    .word 2940710263
    .word 2923965815
    .word 2941824375
    .word 2942905719
    .word 2943987063
    .word 2925145463
    .word 2945101175
    .word 2946182519
    .word 866026711
    .word 846103895
    .word 850298327
    .word 847119959
    .word 716572119
    .word 851347031
    .word 847152855
    .word 853444439
    .word 848168919
    .word 719816535
    .word 854493143
    .word 2929438071
    .word 2926325111
    .word 2930552183
    .word 2931633527
    .word 2932714871
    .word 2927504759
    .word 2933828983
    .word 2934910327
    .word 2935991671
    .word 3439882327
    .word 799903063
    .word 1000619479
    .word 1577946455
    .word 2919247223
    .word 2928717175
    .word 2929798519
    .word 2930879863
    .word 2920426871
    .word 2931993975
    .word 2933075319
    .word 2934156663
    .word 2921606519
    .word 2935270775
    .word 2936352119
    .word 2937433463
    .word 2922786167
    .word 2938547575
    .word 2939628919
    .word 2940710263
    .word 2923965815
    .word 2941824375
    .word 2942905719
    .word 2943987063
    .word 2925145463
    .word 2945101175
    .word 2946182519
    .word 866026711
    .word 846103895
    .word 850298327
    .word 847119959
    .word 716572119
    .word 851347031
    .word 847152855
    .word 853444439
    .word 848168919
    .word 719816535
    .word 854493143
    .word 2929438071
    .word 2926325111
    .word 2930552183
    .word 2931633527
    .word 2932714871
    .word 2927504759
    .word 2933828983
    .word 2934910327
    .word 2935991671
    .word 799903063
    .word 1067728343
    .word 1577946455
    addi a6 , a6 , -1
    addi a1, a1, 128
    bnez a6 , L_sm3_loop_zvl512
    .word 3439620183
    .word 1269083479
    .word 33910055
    bnez a2, ossl_hwsm3_block_data_order_zvksh_zvl256
    ret
ossl_hwsm3_block_data_order_zvksh_zvl256:
    .word 3439620183
    j ossl_hwsm3_block_data_order_zvksh_single
ossl_hwsm3_block_data_order_zvksh_zvl128:
    .word 3440668759
ossl_hwsm3_block_data_order_zvksh_single:
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

.section .rodata
.p2align 3
.type ORDER_BY_ZVL512_DATA,@object
ORDER_BY_ZVL512_DATA:
    .word 0, 4, 8, 12, 16, 20, 24, 28, 64, 68, 72, 76, 80, 84, 88, 92, 32, 36, 40, 44, 48, 52, 56, 60, 96, 100, 104, 108, 112, 116, 120, 124
.size ORDER_BY_ZVL512_DATA, .-ORDER_BY_ZVL512_DATA

.p2align 3
.type ORDER_BY_ZVL512_EXP,@object
ORDER_BY_ZVL512_EXP:
    .word 2, 3, 4, 5, 6, 7, 255, 255, 10, 11, 12, 13, 14, 15, 255, 255, 255, 255, 255, 255, 0, 1, 2, 3, 255, 255, 255, 255, 8, 9, 10, 11
.size ORDER_BY_ZVL512_EXP, .-ORDER_BY_ZVL512_EXP
