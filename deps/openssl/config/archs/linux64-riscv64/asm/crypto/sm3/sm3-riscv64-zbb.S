.text
.p2align 3
.globl ossl_sm3_block_data_order_zbb
.type   ossl_sm3_block_data_order_zbb,@function
ossl_sm3_block_data_order_zbb:

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

    la t0, SM3K

    # load ctx
    lw s2, 0(a0)
    lw s3, 4(a0)
    lw s4, 8(a0)
    lw s5, 12(a0)
    lw s6, 16(a0)
    lw s7, 20(a0)
    lw s8, 24(a0)
    lw s9, 28(a0)

L_round_loop:
    # Decrement length by 1
    addi a2, a2, -1

    
    lw t1, (a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, (sp)

    lw t1, 4(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 4(sp)

    lw t1, 8(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 8(sp)

    lw t1, 12(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 12(sp)

    lw t1, 16(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 16(sp)

    lw t1, 20(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 20(sp)

    lw t1, 24(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 24(sp)

    lw t1, 28(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 28(sp)

    lw t1, 32(a1)
    .word 1803768595
    srli t1, t1, 32
    sw t1, 32(sp)

    lw t1, 36(a1)
    .word 1803768595
    srli s0, t1, 32

    lw t1, 40(a1)
    .word 1803768595
    srli s1, t1, 32

    lw t1, 44(a1)
    .word 1803768595
    srli a5, t1, 32

    lw t1, 48(a1)
    .word 1803768595
    srli a6, t1, 32

    lw t1, 52(a1)
    .word 1803768595
    srli a7, t1, 32

    lw t1, 56(a1)
    .word 1803768595
    srli s10, t1, 32

    lw t1, 60(a1)
    .word 1803768595
    srli s11, t1, 32


        lw t5, ((0-0)&0x0F)*4(sp)
    lw t2, ((0-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*0(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s3, s4
    xor a3, a4, s2

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    xor a3, a4, s6

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((0-0)&0x0F)*4(sp)
    lw t2, ((0-9)&0x0F)*4(sp)
    mv t3, a7
    lw t4, ((0-13)&0x0F)*4(sp)
    mv t5, s1
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((0-0)&0x0F)*4(sp)

        lw t5, ((1-0)&0x0F)*4(sp)
    lw t2, ((1-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*1(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s2, s3
    xor a3, a4, s5

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    xor a3, a4, s9

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t1, ((1-0)&0x0F)*4(sp)
    lw t2, ((1-9)&0x0F)*4(sp)
    mv t3, s10
    lw t4, ((1-13)&0x0F)*4(sp)
    mv t5, a5
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((1-0)&0x0F)*4(sp)

        lw t5, ((2-0)&0x0F)*4(sp)
    lw t2, ((2-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*2(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s5, s2
    xor a3, a4, s4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    xor a3, a4, s8

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t1, ((2-0)&0x0F)*4(sp)
    mv t2, s0
    mv t3, s11
    lw t4, ((2-13)&0x0F)*4(sp)
    mv t5, a6
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((2-0)&0x0F)*4(sp)

        lw t5, ((3-0)&0x0F)*4(sp)
    lw t2, ((3-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*3(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s4, s5
    xor a3, a4, s3

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    xor a3, a4, s7

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        lw t1, ((3-0)&0x0F)*4(sp)
    mv t2, s1
    lw t3, ((3-3)&0x0F)*4(sp)
    lw t4, ((3-13)&0x0F)*4(sp)
    mv t5, a7
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((3-0)&0x0F)*4(sp)


        lw t5, ((4-0)&0x0F)*4(sp)
    lw t2, ((4-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*4(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s3, s4
    xor a3, a4, s2

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    xor a3, a4, s6

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((4-0)&0x0F)*4(sp)
    mv t2, a5
    lw t3, ((4-3)&0x0F)*4(sp)
    lw t4, ((4-13)&0x0F)*4(sp)
    mv t5, s10
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((4-0)&0x0F)*4(sp)

        lw t5, ((5-0)&0x0F)*4(sp)
    mv t2, s0
    xor t6, t5, t2
    lw t1, 4*5(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s2, s3
    xor a3, a4, s5

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    xor a3, a4, s9

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t1, ((5-0)&0x0F)*4(sp)
    mv t2, a6
    lw t3, ((5-3)&0x0F)*4(sp)
    lw t4, ((5-13)&0x0F)*4(sp)
    mv t5, s11
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((5-0)&0x0F)*4(sp)

        lw t5, ((6-0)&0x0F)*4(sp)
    mv t2, s1
    xor t6, t5, t2
    lw t1, 4*6(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s5, s2
    xor a3, a4, s4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    xor a3, a4, s8

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t1, ((6-0)&0x0F)*4(sp)
    mv t2, a7
    lw t3, ((6-3)&0x0F)*4(sp)
    mv t4, s0
    lw t5, ((6-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((6-0)&0x0F)*4(sp)

        lw t5, ((7-0)&0x0F)*4(sp)
    mv t2, a5
    xor t6, t5, t2
    lw t1, 4*7(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s4, s5
    xor a3, a4, s3

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    xor a3, a4, s7

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        lw t1, ((7-0)&0x0F)*4(sp)
    mv t2, s10
    lw t3, ((7-3)&0x0F)*4(sp)
    mv t4, s1
    lw t5, ((7-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((7-0)&0x0F)*4(sp)


        lw t5, ((8-0)&0x0F)*4(sp)
    mv t2, a6
    xor t6, t5, t2
    lw t1, 4*8(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s3, s4
    xor a3, a4, s2

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    xor a3, a4, s6

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((8-0)&0x0F)*4(sp)
    mv t2, s11
    lw t3, ((8-3)&0x0F)*4(sp)
    mv t4, a5
    lw t5, ((8-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((8-0)&0x0F)*4(sp)

        mv t5, s0
    mv t2, a7
    xor t6, t5, t2
    lw t1, 4*9(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s2, s3
    xor a3, a4, s5

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    xor a3, a4, s9

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t1, s0
    lw t2, ((9-9)&0x0F)*4(sp)
    lw t3, ((9-3)&0x0F)*4(sp)
    mv t4, a6
    lw t5, ((9-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s0, t6

        mv t5, s1
    mv t2, s10
    xor t6, t5, t2
    lw t1, 4*10(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s5, s2
    xor a3, a4, s4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    xor a3, a4, s8

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t1, s1
    lw t2, ((10-9)&0x0F)*4(sp)
    lw t3, ((10-3)&0x0F)*4(sp)
    mv t4, a7
    lw t5, ((10-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s1, t6

        mv t5, a5
    mv t2, s11
    xor t6, t5, t2
    lw t1, 4*11(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s4, s5
    xor a3, a4, s3

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    xor a3, a4, s7

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        mv t1, a5
    lw t2, ((11-9)&0x0F)*4(sp)
    lw t3, ((11-3)&0x0F)*4(sp)
    mv t4, s10
    lw t5, ((11-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a5, t6


        mv t5, a6
    lw t2, ((12-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*12(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s3, s4
    xor a3, a4, s2

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    xor a3, a4, s6

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        mv t1, a6
    lw t2, ((12-9)&0x0F)*4(sp)
    mv t3, s0
    mv t4, s11
    lw t5, ((12-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a6, t6

        mv t5, a7
    lw t2, ((13-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*13(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s2, s3
    xor a3, a4, s5

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    xor a3, a4, s9

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t1, a7
    lw t2, ((13-9)&0x0F)*4(sp)
    mv t3, s1
    lw t4, ((13-13)&0x0F)*4(sp)
    lw t5, ((13-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a7, t6

        mv t5, s10
    lw t2, ((14-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*14(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s5, s2
    xor a3, a4, s4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    xor a3, a4, s8

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t1, s10
    lw t2, ((14-9)&0x0F)*4(sp)
    mv t3, a5
    lw t4, ((14-13)&0x0F)*4(sp)
    lw t5, ((14-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s10, t6

        mv t5, s11
    lw t2, ((15-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*15(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        xor a4, s4, s5
    xor a3, a4, s3

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    xor a3, a4, s7

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        mv t1, s11
    lw t2, ((15-9)&0x0F)*4(sp)
    mv t3, a6
    lw t4, ((15-13)&0x0F)*4(sp)
    mv t5, s0
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s11, t6


        lw t5, ((16-0)&0x0F)*4(sp)
    lw t2, ((16-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*16(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((16-0)&0x0F)*4(sp)
    lw t2, ((16-9)&0x0F)*4(sp)
    mv t3, a7
    lw t4, ((16-13)&0x0F)*4(sp)
    mv t5, s1
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((16-0)&0x0F)*4(sp)

        lw t5, ((17-0)&0x0F)*4(sp)
    lw t2, ((17-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*17(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t1, ((17-0)&0x0F)*4(sp)
    lw t2, ((17-9)&0x0F)*4(sp)
    mv t3, s10
    lw t4, ((17-13)&0x0F)*4(sp)
    mv t5, a5
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((17-0)&0x0F)*4(sp)

        lw t5, ((18-0)&0x0F)*4(sp)
    lw t2, ((18-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*18(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t1, ((18-0)&0x0F)*4(sp)
    mv t2, s0
    mv t3, s11
    lw t4, ((18-13)&0x0F)*4(sp)
    mv t5, a6
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((18-0)&0x0F)*4(sp)

        lw t5, ((19-0)&0x0F)*4(sp)
    lw t2, ((19-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*19(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        lw t1, ((19-0)&0x0F)*4(sp)
    mv t2, s1
    lw t3, ((19-3)&0x0F)*4(sp)
    lw t4, ((19-13)&0x0F)*4(sp)
    mv t5, a7
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((19-0)&0x0F)*4(sp)


        lw t5, ((20-0)&0x0F)*4(sp)
    lw t2, ((20-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*20(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((20-0)&0x0F)*4(sp)
    mv t2, a5
    lw t3, ((20-3)&0x0F)*4(sp)
    lw t4, ((20-13)&0x0F)*4(sp)
    mv t5, s10
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((20-0)&0x0F)*4(sp)

        lw t5, ((21-0)&0x0F)*4(sp)
    mv t2, s0
    xor t6, t5, t2
    lw t1, 4*21(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t1, ((21-0)&0x0F)*4(sp)
    mv t2, a6
    lw t3, ((21-3)&0x0F)*4(sp)
    lw t4, ((21-13)&0x0F)*4(sp)
    mv t5, s11
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((21-0)&0x0F)*4(sp)

        lw t5, ((22-0)&0x0F)*4(sp)
    mv t2, s1
    xor t6, t5, t2
    lw t1, 4*22(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t1, ((22-0)&0x0F)*4(sp)
    mv t2, a7
    lw t3, ((22-3)&0x0F)*4(sp)
    mv t4, s0
    lw t5, ((22-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((22-0)&0x0F)*4(sp)

        lw t5, ((23-0)&0x0F)*4(sp)
    mv t2, a5
    xor t6, t5, t2
    lw t1, 4*23(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        lw t1, ((23-0)&0x0F)*4(sp)
    mv t2, s10
    lw t3, ((23-3)&0x0F)*4(sp)
    mv t4, s1
    lw t5, ((23-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((23-0)&0x0F)*4(sp)


        lw t5, ((24-0)&0x0F)*4(sp)
    mv t2, a6
    xor t6, t5, t2
    lw t1, 4*24(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((24-0)&0x0F)*4(sp)
    mv t2, s11
    lw t3, ((24-3)&0x0F)*4(sp)
    mv t4, a5
    lw t5, ((24-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((24-0)&0x0F)*4(sp)

        mv t5, s0
    mv t2, a7
    xor t6, t5, t2
    lw t1, 4*25(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t1, s0
    lw t2, ((25-9)&0x0F)*4(sp)
    lw t3, ((25-3)&0x0F)*4(sp)
    mv t4, a6
    lw t5, ((25-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s0, t6

        mv t5, s1
    mv t2, s10
    xor t6, t5, t2
    lw t1, 4*26(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t1, s1
    lw t2, ((26-9)&0x0F)*4(sp)
    lw t3, ((26-3)&0x0F)*4(sp)
    mv t4, a7
    lw t5, ((26-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s1, t6

        mv t5, a5
    mv t2, s11
    xor t6, t5, t2
    lw t1, 4*27(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        mv t1, a5
    lw t2, ((27-9)&0x0F)*4(sp)
    lw t3, ((27-3)&0x0F)*4(sp)
    mv t4, s10
    lw t5, ((27-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a5, t6


        mv t5, a6
    lw t2, ((28-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*28(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        mv t1, a6
    lw t2, ((28-9)&0x0F)*4(sp)
    mv t3, s0
    mv t4, s11
    lw t5, ((28-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a6, t6

        mv t5, a7
    lw t2, ((29-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*29(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t1, a7
    lw t2, ((29-9)&0x0F)*4(sp)
    mv t3, s1
    lw t4, ((29-13)&0x0F)*4(sp)
    lw t5, ((29-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a7, t6

        mv t5, s10
    lw t2, ((30-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*30(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t1, s10
    lw t2, ((30-9)&0x0F)*4(sp)
    mv t3, a5
    lw t4, ((30-13)&0x0F)*4(sp)
    lw t5, ((30-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s10, t6

        mv t5, s11
    lw t2, ((31-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*31(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        mv t1, s11
    lw t2, ((31-9)&0x0F)*4(sp)
    mv t3, a6
    lw t4, ((31-13)&0x0F)*4(sp)
    mv t5, s0
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s11, t6


        lw t5, ((32-0)&0x0F)*4(sp)
    lw t2, ((32-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*32(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((32-0)&0x0F)*4(sp)
    lw t2, ((32-9)&0x0F)*4(sp)
    mv t3, a7
    lw t4, ((32-13)&0x0F)*4(sp)
    mv t5, s1
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((32-0)&0x0F)*4(sp)

        lw t5, ((33-0)&0x0F)*4(sp)
    lw t2, ((33-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*33(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t1, ((33-0)&0x0F)*4(sp)
    lw t2, ((33-9)&0x0F)*4(sp)
    mv t3, s10
    lw t4, ((33-13)&0x0F)*4(sp)
    mv t5, a5
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((33-0)&0x0F)*4(sp)

        lw t5, ((34-0)&0x0F)*4(sp)
    lw t2, ((34-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*34(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t1, ((34-0)&0x0F)*4(sp)
    mv t2, s0
    mv t3, s11
    lw t4, ((34-13)&0x0F)*4(sp)
    mv t5, a6
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((34-0)&0x0F)*4(sp)

        lw t5, ((35-0)&0x0F)*4(sp)
    lw t2, ((35-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*35(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        lw t1, ((35-0)&0x0F)*4(sp)
    mv t2, s1
    lw t3, ((35-3)&0x0F)*4(sp)
    lw t4, ((35-13)&0x0F)*4(sp)
    mv t5, a7
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((35-0)&0x0F)*4(sp)


        lw t5, ((36-0)&0x0F)*4(sp)
    lw t2, ((36-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*36(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((36-0)&0x0F)*4(sp)
    mv t2, a5
    lw t3, ((36-3)&0x0F)*4(sp)
    lw t4, ((36-13)&0x0F)*4(sp)
    mv t5, s10
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((36-0)&0x0F)*4(sp)

        lw t5, ((37-0)&0x0F)*4(sp)
    mv t2, s0
    xor t6, t5, t2
    lw t1, 4*37(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t1, ((37-0)&0x0F)*4(sp)
    mv t2, a6
    lw t3, ((37-3)&0x0F)*4(sp)
    lw t4, ((37-13)&0x0F)*4(sp)
    mv t5, s11
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((37-0)&0x0F)*4(sp)

        lw t5, ((38-0)&0x0F)*4(sp)
    mv t2, s1
    xor t6, t5, t2
    lw t1, 4*38(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t1, ((38-0)&0x0F)*4(sp)
    mv t2, a7
    lw t3, ((38-3)&0x0F)*4(sp)
    mv t4, s0
    lw t5, ((38-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((38-0)&0x0F)*4(sp)

        lw t5, ((39-0)&0x0F)*4(sp)
    mv t2, a5
    xor t6, t5, t2
    lw t1, 4*39(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        lw t1, ((39-0)&0x0F)*4(sp)
    mv t2, s10
    lw t3, ((39-3)&0x0F)*4(sp)
    mv t4, s1
    lw t5, ((39-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((39-0)&0x0F)*4(sp)


        lw t5, ((40-0)&0x0F)*4(sp)
    mv t2, a6
    xor t6, t5, t2
    lw t1, 4*40(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((40-0)&0x0F)*4(sp)
    mv t2, s11
    lw t3, ((40-3)&0x0F)*4(sp)
    mv t4, a5
    lw t5, ((40-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((40-0)&0x0F)*4(sp)

        mv t5, s0
    mv t2, a7
    xor t6, t5, t2
    lw t1, 4*41(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t1, s0
    lw t2, ((41-9)&0x0F)*4(sp)
    lw t3, ((41-3)&0x0F)*4(sp)
    mv t4, a6
    lw t5, ((41-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s0, t6

        mv t5, s1
    mv t2, s10
    xor t6, t5, t2
    lw t1, 4*42(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t1, s1
    lw t2, ((42-9)&0x0F)*4(sp)
    lw t3, ((42-3)&0x0F)*4(sp)
    mv t4, a7
    lw t5, ((42-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s1, t6

        mv t5, a5
    mv t2, s11
    xor t6, t5, t2
    lw t1, 4*43(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        mv t1, a5
    lw t2, ((43-9)&0x0F)*4(sp)
    lw t3, ((43-3)&0x0F)*4(sp)
    mv t4, s10
    lw t5, ((43-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a5, t6


        mv t5, a6
    lw t2, ((44-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*44(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        mv t1, a6
    lw t2, ((44-9)&0x0F)*4(sp)
    mv t3, s0
    mv t4, s11
    lw t5, ((44-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a6, t6

        mv t5, a7
    lw t2, ((45-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*45(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t1, a7
    lw t2, ((45-9)&0x0F)*4(sp)
    mv t3, s1
    lw t4, ((45-13)&0x0F)*4(sp)
    lw t5, ((45-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv a7, t6

        mv t5, s10
    lw t2, ((46-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*46(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t1, s10
    lw t2, ((46-9)&0x0F)*4(sp)
    mv t3, a5
    lw t4, ((46-13)&0x0F)*4(sp)
    lw t5, ((46-6)&0x0F)*4(sp)
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s10, t6

        mv t5, s11
    lw t2, ((47-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*47(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        mv t1, s11
    lw t2, ((47-9)&0x0F)*4(sp)
    mv t3, a6
    lw t4, ((47-13)&0x0F)*4(sp)
    mv t5, s0
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    mv s11, t6


        lw t5, ((48-0)&0x0F)*4(sp)
    lw t2, ((48-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*48(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t1, ((48-0)&0x0F)*4(sp)
    lw t2, ((48-9)&0x0F)*4(sp)
    mv t3, a7
    lw t4, ((48-13)&0x0F)*4(sp)
    mv t5, s1
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((48-0)&0x0F)*4(sp)

        lw t5, ((49-0)&0x0F)*4(sp)
    lw t2, ((49-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*49(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t1, ((49-0)&0x0F)*4(sp)
    lw t2, ((49-9)&0x0F)*4(sp)
    mv t3, s10
    lw t4, ((49-13)&0x0F)*4(sp)
    mv t5, a5
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((49-0)&0x0F)*4(sp)

        lw t5, ((50-0)&0x0F)*4(sp)
    lw t2, ((50-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*50(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t1, ((50-0)&0x0F)*4(sp)
    mv t2, s0
    mv t3, s11
    lw t4, ((50-13)&0x0F)*4(sp)
    mv t5, a6
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((50-0)&0x0F)*4(sp)

        lw t5, ((51-0)&0x0F)*4(sp)
    lw t2, ((51-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*51(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3



        lw t1, ((51-0)&0x0F)*4(sp)
    mv t2, s1
    lw t3, ((51-3)&0x0F)*4(sp)
    lw t4, ((51-13)&0x0F)*4(sp)
    mv t5, a7
    xor a3, t1, t2
    .word 1629378331
    xor t6, a3, a4
        .word 1629476507
    .word 1621088027
    xor a3, a3, a4
    xor t6, t6, a3

    .word 1637799707
    xor t6, t6, a4
    xor t6, t6, t5
    sw t6, ((51-0)&0x0F)*4(sp)


        lw t5, ((52-0)&0x0F)*4(sp)
    lw t2, ((52-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*52(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        lw t5, ((53-0)&0x0F)*4(sp)
    mv t2, s0
    xor t6, t5, t2
    lw t1, 4*53(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        lw t5, ((54-0)&0x0F)*4(sp)
    mv t2, s1
    xor t6, t5, t2
    lw t1, 4*54(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        lw t5, ((55-0)&0x0F)*4(sp)
    mv t2, a5
    xor t6, t5, t2
    lw t1, 4*55(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3




        lw t5, ((56-0)&0x0F)*4(sp)
    mv t2, a6
    xor t6, t5, t2
    lw t1, 4*56(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        mv t5, s0
    mv t2, a7
    xor t6, t5, t2
    lw t1, 4*57(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t5, s1
    mv t2, s10
    xor t6, t5, t2
    lw t1, 4*58(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t5, a5
    mv t2, s11
    xor t6, t5, t2
    lw t1, 4*59(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3




        mv t5, a6
    lw t2, ((60-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*60(t0)
    .word 1632195483 # T2 = A12
    addw t3, t2, s6 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s2, s3
    and a3, a3, s4
    and a4, s2, s3
    or a3, a3, a4

    addw t4, a3, s5 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s5, t1, t6 # d = T1 + Wj
        xor a4, s7, s8
    and a3, a4, s6
    xor a3, a3, s8

    .word 1635375515
    .word 1625021339
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s9, s9, t1
        .word 1635571355
    .word 1627182875
    xor a3, a3, a4
    xor s9, s9, a3



        mv t5, a7
    lw t2, ((61-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*61(t0)
    .word 1632293787 # T2 = A12
    addw t3, t2, s9 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s5, s2
    and a3, a3, s3
    and a4, s5, s2
    or a3, a3, a4

    addw t4, a3, s4 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s4, t1, t6 # d = T1 + Wj
        xor a4, s6, s7
    and a3, a4, s9
    xor a3, a3, s7

    .word 1635342619
    .word 1624988443
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s8, s8, t1
        .word 1635538587
    .word 1627150107
    xor a3, a3, a4
    xor s8, s8, a3



        mv t5, s10
    lw t2, ((62-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*62(t0)
    .word 1632261019 # T2 = A12
    addw t3, t2, s8 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s4, s5
    and a3, a3, s2
    and a4, s4, s5
    or a3, a3, a4

    addw t4, a3, s3 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s3, t1, t6 # d = T1 + Wj
        xor a4, s9, s6
    and a3, a4, s8
    xor a3, a3, s6

    .word 1635441307
    .word 1625087131
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s7, s7, t1
        .word 1635505819
    .word 1627117339
    xor a3, a3, a4
    xor s7, s7, a3



        mv t5, s11
    lw t2, ((63-12)&0x0F)*4(sp)
    xor t6, t5, t2
    lw t1, 4*63(t0)
    .word 1632228251 # T2 = A12
    addw t3, t2, s7 # T3 = A12_SM = A12 + E
    addw t3, t3, t1 # T3 = A12_SM = A12 + E + Tj
    .word 1637768731 # T3 = SS1
        or a3, s3, s4
    and a3, a3, s5
    and a4, s3, s4
    or a3, a3, a4

    addw t4, a3, s2 # T4 = FF + D
    xor t1, t3, t2 # T1 = SS1 ^ A12
    addw t1, t1, t4 # T1 = T4 + T1 = FF + D + (SS1 ^ A12)
    addw s2, t1, t6 # d = T1 + Wj
        xor a4, s8, s9
    and a3, a4, s7
    xor a3, a3, s9

    .word 1635408411
    .word 1625054235
    addw t1, a3, t3 # T1 = GG + SS1
    addw t1, t1, t5 # T1 = GG + SS1 + Wj
    addw s6, s6, t1
        .word 1635473051
    .word 1627084571
    xor a3, a3, a4
    xor s6, s6, a3




    lw t1, 0(a0)
    lw t2, 4(a0)
    lw t3, 8(a0)
    lw t4, 12(a0)

    xor s2, s2, t1
    xor s3, s3, t2
    xor s4, s4, t3
    xor s5, s5, t4

    sw s2, 0(a0)
    sw s3, 4(a0)
    sw s4, 8(a0)
    sw s5, 12(a0)

    lw t1, 16(a0)
    lw t2, 20(a0)
    lw t3, 24(a0)
    lw t4, 28(a0)

    xor s6, s6, t1
    xor s7, s7, t2
    xor s8, s8, t3
    xor s9, s9, t4

    sw s6, 16(a0)
    sw s7, 20(a0)
    sw s8, 24(a0)
    sw s9, 28(a0)

    addi a1, a1, 64

    bnez a2, L_round_loop

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
.size ossl_sm3_block_data_order_zbb,.-ossl_sm3_block_data_order_zbb

.section .rodata
.p2align 3
.type SM3K,@object
SM3K:
    .word 0x79CC4519, 0xF3988A32, 0xE7311465, 0xCE6228CB
    .word 0x9CC45197, 0x3988A32F, 0x7311465E, 0xE6228CBC
    .word 0xCC451979, 0x988A32F3, 0x311465E7, 0x6228CBCE
    .word 0xC451979C, 0x88A32F39, 0x11465E73, 0x228CBCE6
    .word 0x9D8A7A87, 0x3B14F50F, 0x7629EA1E, 0xEC53D43C
    .word 0xD8A7A879, 0xB14F50F3, 0x629EA1E7, 0xC53D43CE
    .word 0x8A7A879D, 0x14F50F3B, 0x29EA1E76, 0x53D43CEC
    .word 0xA7A879D8, 0x4F50F3B1, 0x9EA1E762, 0x3D43CEC5
    .word 0x7A879D8A, 0xF50F3B14, 0xEA1E7629, 0xD43CEC53
    .word 0xA879D8A7, 0x50F3B14F, 0xA1E7629E, 0x43CEC53D
    .word 0x879D8A7A, 0x0F3B14F5, 0x1E7629EA, 0x3CEC53D4
    .word 0x79D8A7A8, 0xF3B14F50, 0xE7629EA1, 0xCEC53D43
    .word 0x9D8A7A87, 0x3B14F50F, 0x7629EA1E, 0xEC53D43C
    .word 0xD8A7A879, 0xB14F50F3, 0x629EA1E7, 0xC53D43CE
    .word 0x8A7A879D, 0x14F50F3B, 0x29EA1E76, 0x53D43CEC
    .word 0xA7A879D8, 0x4F50F3B1, 0x9EA1E762, 0x3D43CEC5
.size SM3K,.-SM3K
