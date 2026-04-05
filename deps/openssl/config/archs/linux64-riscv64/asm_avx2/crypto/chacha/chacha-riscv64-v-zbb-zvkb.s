.text
.p2align 3
.globl ChaCha20_ctr32_v_zbb_zvkb
.type ChaCha20_ctr32_v_zbb_zvkb,@function
ChaCha20_ctr32_v_zbb_zvkb:
    addi sp, sp, -96
    sd s0, 0(sp)
    sd s1, 8(sp)
    sd s2, 16(sp)
    sd s3, 24(sp)
    sd s4, 32(sp)
    sd s5, 40(sp)
    sd s6, 48(sp)
    sd s7, 56(sp)
    sd s8, 64(sp)
    sd s9, 72(sp)
    sd s10, 80(sp)
    sd s11, 88(sp)
    addi sp, sp, -64

    lw t2, 0(a4)

.Lblock_loop:
    # We will use the scalar ALU for 1 chacha block.
    srli t3, a2, 6
    .word 219050839
    slli t4, t1, 6
    bltu t4, a2, 1f
    # Since there is no more chacha block existed, we need to split 1 block
    # from vector ALU.
    addi t4, t1, -1
    .word 219083607
1:

    #### chacha block data
    # init chacha const states into v0~v3
    # "expa" little endian
    li a5, 0x61707865
    .word 1577566295
    # "nd 3" little endian
    li a6, 0x3320646e
    .word 1577599191
    # "2-by" little endian
    li a7, 0x79622d32
    .word 1577632087
    # "te k" little endian
    li s0, 0x6b206574
    lw s1, 0(a3)
    .word 1577337303

    # init chacha key states into v4~v11
    lw s2, 4(a3)
    .word 1577370199
    lw s3, 8(a3)
    .word 1577665239
    lw s4, 12(a3)
    .word 1577698135
    lw s5, 16(a3)
    .word 1577731031
    lw s6, 20(a3)
    .word 1577763927
    lw s7, 24(a3)
    .word 1577796823
    lw s8, 28(a3)
    .word 1577829719
    .word 1577862615

    # init chacha key states into v12~v13
    lw s10, 4(a4)
    .word 1376298583
    lw s11, 8(a4)
    .word 46384727
    lw t0, 12(a4)
    .word 1577928407
    add s9, t2, t1

    # init chacha nonce states into v14~v15
    .word 1577961303
    .word 1577240535

    li t3, 64
    # load the top-half of input data into v16~v23
    .word 3955615751

    # till now in block_loop, we used:
    # - v0~v15 for chacha states.
    # - v16~v23 for top-half of input data.
    # - v24~v31 haven't been used yet.

    # 20 round groups
    li t3, 10
.Lround_loop:
    # we can use v24~v31 as temporary registers in round_loop.
    addi t3, t3, -1
        # a += b; d ^= a; d <<<= 16;
        .word 33685591
    add a5, a5, s1
    .word 34767063
    add a6, a6, s2
    .word 35848535
    add a7, a7, s3
    .word 36930007
    add s0, s0, s4
    .word 784336471
    xor s9, s9, a5
    .word 785417943
    xor s10, s10, a6
    .word 786499415
    xor s11, s11, a7
    .word 787580887
    xor t0, t0, s0
        .word 1388852823
        .word 1628232859
        .word 1389901527
        .word 1628265755
        .word 1390950231
        .word 1628298651
        .word 1391998935
        .word 1627574939

    # c += d; b ^= c; b <<<= 12;
        .word 42337367
    add s5, s5, s9
    .word 43418839
    add s6, s6, s10
    .word 44500311
    add s7, s7, s11
    .word 45581783
    add s8, s8, t0
    .word 776208983
    xor s1, s1, s5
    .word 777290455
    xor s2, s2, s6
    .word 778371927
    xor s3, s3, s7
    .word 779453399
    xor s4, s4, s8
        .word 1380594263
        .word 1631900827
        .word 1381642967
        .word 1632196891
        .word 1382691671
        .word 1632229787
        .word 1383740375
        .word 1632262683

    # a += b; d ^= a; d <<<= 8;
        .word 33685591
    add a5, a5, s1
    .word 34767063
    add a6, a6, s2
    .word 35848535
    add a7, a7, s3
    .word 36930007
    add s0, s0, s4
    .word 784336471
    xor s9, s9, a5
    .word 785417943
    xor s10, s10, a6
    .word 786499415
    xor s11, s11, a7
    .word 787580887
    xor t0, t0, s0
        .word 1389114967
        .word 1636621467
        .word 1390163671
        .word 1636654363
        .word 1391212375
        .word 1636687259
        .word 1392261079
        .word 1635963547

    # c += d; b ^= c; b <<<= 7;
        .word 42337367
    add s5, s5, s9
    .word 43418839
    add s6, s6, s10
    .word 44500311
    add s7, s7, s11
    .word 45581783
    add s8, s8, t0
    .word 776208983
    xor s1, s1, s5
    .word 777290455
    xor s2, s2, s6
    .word 778371927
    xor s3, s3, s7
    .word 779453399
    xor s4, s4, s8
        .word 1380758103
        .word 1637143707
        .word 1381806807
        .word 1637439771
        .word 1382855511
        .word 1637472667
        .word 1383904215
        .word 1637505563


        # a += b; d ^= a; d <<<= 16;
        .word 36831703
    add s0, s0, s1
    .word 33718359
    add a5, a5, s2
    .word 34799831
    add a6, a6, s3
    .word 35881303
    add a7, a7, s4
    .word 786532183
    xor s11, s11, s0
    .word 787482583
    xor t0, t0, a5
    .word 784369239
    xor s9, s9, a6
    .word 785450711
    xor s10, s10, a7
        .word 1390950231
        .word 1628298651
        .word 1391998935
        .word 1627574939
        .word 1388852823
        .word 1628232859
        .word 1389901527
        .word 1628265755

    # c += d; b ^= c; b <<<= 12;
        .word 43451607
    add s6, s6, s11
    .word 44533079
    add s7, s7, t0
    .word 45483479
    add s8, s8, s9
    .word 42370135
    add s5, s5, s10
    .word 776241751
    xor s1, s1, s6
    .word 777323223
    xor s2, s2, s7
    .word 778404695
    xor s3, s3, s8
    .word 779355095
    xor s4, s4, s5
        .word 1380594263
        .word 1631900827
        .word 1381642967
        .word 1632196891
        .word 1382691671
        .word 1632229787
        .word 1383740375
        .word 1632262683

    # a += b; d ^= a; d <<<= 8;
        .word 36831703
    add s0, s0, s1
    .word 33718359
    add a5, a5, s2
    .word 34799831
    add a6, a6, s3
    .word 35881303
    add a7, a7, s4
    .word 786532183
    xor s11, s11, s0
    .word 787482583
    xor t0, t0, a5
    .word 784369239
    xor s9, s9, a6
    .word 785450711
    xor s10, s10, a7
        .word 1391212375
        .word 1636687259
        .word 1392261079
        .word 1635963547
        .word 1389114967
        .word 1636621467
        .word 1390163671
        .word 1636654363

    # c += d; b ^= c; b <<<= 7;
        .word 43451607
    add s6, s6, s11
    .word 44533079
    add s7, s7, t0
    .word 45483479
    add s8, s8, s9
    .word 42370135
    add s5, s5, s10
    .word 776241751
    xor s1, s1, s6
    .word 777323223
    xor s2, s2, s7
    .word 778404695
    xor s3, s3, s8
    .word 779355095
    xor s4, s4, s5
        .word 1380758103
        .word 1637143707
        .word 1381806807
        .word 1637439771
        .word 1382855511
        .word 1637472667
        .word 1383904215
        .word 1637505563


    bnez t3, .Lround_loop

    li t3, 64
    # load the bottom-half of input data into v24~v31
    addi t4, a1, 32
    .word 3956206599

    # now, there are no free vector registers until the round_loop exits.

    # add chacha top-half initial block states
    # "expa" little endian
    li t3, 0x61707865
    .word 34488407
    add a5, a5, t3
    # "nd 3" little endian
    li t4, 0x3320646e
    .word 35569879
    add a6, a6, t4
    lw t3, 0(a3)
    # "2-by" little endian
    li t5, 0x79622d32
    .word 36651351
    add a7, a7, t5
    lw t4, 4(a3)
    # "te k" little endian
    li t6, 0x6b206574
    .word 37732823
    add s0, s0, t6
    lw t5, 8(a3)
    .word 38683223
    add s1, s1, t3
    lw t6, 12(a3)
    .word 39764695
    add s2, s2, t4
    .word 40846167
    add s3, s3, t5
    .word 41927639
    add s4, s4, t6

    # xor with the top-half input
    .word 788531287
    sw a5, 0(sp)
    sw a6, 4(sp)
    .word 789612759
    sw a7, 8(sp)
    sw s0, 12(sp)
    .word 790694231
    sw s1, 16(sp)
    sw s2, 20(sp)
    .word 791775703
    sw s3, 24(sp)
    sw s4, 28(sp)
    .word 792857175
    lw t3, 16(a3)
    .word 793938647
    lw t4, 20(a3)
    .word 795020119
    lw t5, 24(a3)
    .word 796101591

    # save the top-half of output from v16~v23
    li t6, 64
    .word 3958728743

    # add chacha bottom-half initial block states
    .word 42878039
    add s5, s5, t3
    lw t6, 28(a3)
    .word 43959511
    add s6, s6, t4
    lw t3, 4(a4)
    .word 45040983
    add s7, s7, t5
    lw t4, 8(a4)
    .word 46122455
    add s8, s8, t6
    lw t5, 12(a4)
    .word 1376297047
    add s9, s9, t2
    .word 46384727
    add s9, s9, t1
    .word 48121559
    add s10, s10, t3
    .word 49203031
    add s11, s11, t4
    .word 50284503
    add t0, t0, t5
    .word 46138967
    # xor with the bottom-half input
    .word 797183063
    sw s5, 32(sp)
    .word 798264535
    sw s6, 36(sp)
    .word 799346007
    sw s7, 40(sp)
    .word 800427479
    sw s8, 44(sp)
    .word 802590423
    sw s9, 48(sp)
    .word 801508951
    sw s10, 52(sp)
    .word 803671895
    sw s11, 56(sp)
    .word 804753367
    sw t0, 60(sp)

    # save the bottom-half of output from v24~v31
    li t3, 64
    addi t4, a0, 32
    .word 3956206631

    # the computed vector parts: `64 * VL`
    slli t3, t1, 6

    add a1, a1, t3
    add a0, a0, t3
    sub a2, a2, t3
    add t2, t2, t1

    # process the scalar data block
    addi t2, t2, 1
    li t3, 64
    .word 197549747
    sub a2, a2, t4
    mv t5, sp
.Lscalar_data_loop:
    .word 205452119
    # from this on, vector registers are grouped with lmul = 8
    .word 33915911
    .word 34539527
    .word 780665943
    .word 33883175
    add a1, a1, t1
    add a0, a0, t1
    add t5, t5, t1
    sub t4, t4, t1
    bnez t4, .Lscalar_data_loop

    bnez a2, .Lblock_loop

    addi sp, sp, 64
    ld s0, 0(sp)
    ld s1, 8(sp)
    ld s2, 16(sp)
    ld s3, 24(sp)
    ld s4, 32(sp)
    ld s5, 40(sp)
    ld s6, 48(sp)
    ld s7, 56(sp)
    ld s8, 64(sp)
    ld s9, 72(sp)
    ld s10, 80(sp)
    ld s11, 88(sp)
    addi sp, sp, 96

    ret
.size ChaCha20_ctr32_v_zbb_zvkb,.-ChaCha20_ctr32_v_zbb_zvkb
