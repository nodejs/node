.text
.p2align 3
.globl sha512_block_data_order_zbb
.type   sha512_block_data_order_zbb,@function
sha512_block_data_order_zbb:

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

    addi sp, sp, -128

    la t0, K512

    # load ctx
    ld s2, 0(a0)
    ld s3, 8(a0)
    ld s4, 16(a0)
    ld s5, 24(a0)
    ld s6, 32(a0)
    ld s7, 40(a0)
    ld s8, 48(a0)
    ld s9, 56(a0)

L_round_loop:
    # Decrement length by 1
    addi a2, a2, -1

            ld t1, (8*0+0)(a1)
    .word 1803768595
    sd t1, 8*0(sp)

            ld t4, 8*0(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, (8*1+0)(a1)
    .word 1803768595
    sd t1, 8*1(sp)

            ld t4, 8*1(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, (8*2+0)(a1)
    .word 1803768595
    sd t1, 8*2(sp)

            ld t4, 8*2(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, (8*3+0)(a1)
    .word 1803768595
    sd t1, 8*3(sp)

            ld t4, 8*3(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, (8*4+0)(a1)
    .word 1803768595
    sd t1, 8*4(sp)

            ld t4, 8*4(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, (8*5+0)(a1)
    .word 1803768595
    sd t1, 8*5(sp)

            ld t4, 8*5(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, (8*6+0)(a1)
    .word 1803768595
    sd t1, 8*6(sp)

            ld t4, 8*6(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, (8*7+0)(a1)
    .word 1803768595
    sd t1, 8*7(sp)

            ld t4, 8*7(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, (8*8+0)(a1)
    .word 1803768595
    sd t1, 8*8(sp)

            ld t4, 8*8(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, (8*9+0)(a1)
    .word 1803768595
    sd t1, 8*9(sp)

            ld t4, 8*9(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, (8*10+0)(a1)
    .word 1803768595
    sd t1, 8*10(sp)

            ld t4, 8*10(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, (8*11+0)(a1)
    .word 1803768595
    sd t1, 8*11(sp)

            ld t4, 8*11(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, (8*12+0)(a1)
    .word 1803768595
    sd t1, 8*12(sp)

            ld t4, 8*12(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, (8*13+0)(a1)
    .word 1803768595
    sd t1, 8*13(sp)

            ld t4, 8*13(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, (8*14+0)(a1)
    .word 1803768595
    sd t1, 8*14(sp)

            ld t4, 8*14(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, (8*15+0)(a1)
    .word 1803768595
    sd t1, 8*15(sp)

            ld t4, 8*15(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((16-2)&0x0f)*8(sp)
    ld t2, ((16-15)&0x0f)*8(sp)
    ld t3, ((16-7)&0x0f)*8(sp)
    ld t4, (16&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(16&0x0f)(sp)

            ld t4, 8*16(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((17-2)&0x0f)*8(sp)
    ld t2, ((17-15)&0x0f)*8(sp)
    ld t3, ((17-7)&0x0f)*8(sp)
    ld t4, (17&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(17&0x0f)(sp)

            ld t4, 8*17(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((18-2)&0x0f)*8(sp)
    ld t2, ((18-15)&0x0f)*8(sp)
    ld t3, ((18-7)&0x0f)*8(sp)
    ld t4, (18&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(18&0x0f)(sp)

            ld t4, 8*18(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((19-2)&0x0f)*8(sp)
    ld t2, ((19-15)&0x0f)*8(sp)
    ld t3, ((19-7)&0x0f)*8(sp)
    ld t4, (19&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(19&0x0f)(sp)

            ld t4, 8*19(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((20-2)&0x0f)*8(sp)
    ld t2, ((20-15)&0x0f)*8(sp)
    ld t3, ((20-7)&0x0f)*8(sp)
    ld t4, (20&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(20&0x0f)(sp)

            ld t4, 8*20(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((21-2)&0x0f)*8(sp)
    ld t2, ((21-15)&0x0f)*8(sp)
    ld t3, ((21-7)&0x0f)*8(sp)
    ld t4, (21&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(21&0x0f)(sp)

            ld t4, 8*21(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((22-2)&0x0f)*8(sp)
    ld t2, ((22-15)&0x0f)*8(sp)
    ld t3, ((22-7)&0x0f)*8(sp)
    ld t4, (22&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(22&0x0f)(sp)

            ld t4, 8*22(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((23-2)&0x0f)*8(sp)
    ld t2, ((23-15)&0x0f)*8(sp)
    ld t3, ((23-7)&0x0f)*8(sp)
    ld t4, (23&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(23&0x0f)(sp)

            ld t4, 8*23(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((24-2)&0x0f)*8(sp)
    ld t2, ((24-15)&0x0f)*8(sp)
    ld t3, ((24-7)&0x0f)*8(sp)
    ld t4, (24&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(24&0x0f)(sp)

            ld t4, 8*24(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((25-2)&0x0f)*8(sp)
    ld t2, ((25-15)&0x0f)*8(sp)
    ld t3, ((25-7)&0x0f)*8(sp)
    ld t4, (25&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(25&0x0f)(sp)

            ld t4, 8*25(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((26-2)&0x0f)*8(sp)
    ld t2, ((26-15)&0x0f)*8(sp)
    ld t3, ((26-7)&0x0f)*8(sp)
    ld t4, (26&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(26&0x0f)(sp)

            ld t4, 8*26(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((27-2)&0x0f)*8(sp)
    ld t2, ((27-15)&0x0f)*8(sp)
    ld t3, ((27-7)&0x0f)*8(sp)
    ld t4, (27&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(27&0x0f)(sp)

            ld t4, 8*27(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((28-2)&0x0f)*8(sp)
    ld t2, ((28-15)&0x0f)*8(sp)
    ld t3, ((28-7)&0x0f)*8(sp)
    ld t4, (28&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(28&0x0f)(sp)

            ld t4, 8*28(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((29-2)&0x0f)*8(sp)
    ld t2, ((29-15)&0x0f)*8(sp)
    ld t3, ((29-7)&0x0f)*8(sp)
    ld t4, (29&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(29&0x0f)(sp)

            ld t4, 8*29(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((30-2)&0x0f)*8(sp)
    ld t2, ((30-15)&0x0f)*8(sp)
    ld t3, ((30-7)&0x0f)*8(sp)
    ld t4, (30&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(30&0x0f)(sp)

            ld t4, 8*30(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((31-2)&0x0f)*8(sp)
    ld t2, ((31-15)&0x0f)*8(sp)
    ld t3, ((31-7)&0x0f)*8(sp)
    ld t4, (31&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(31&0x0f)(sp)

            ld t4, 8*31(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((32-2)&0x0f)*8(sp)
    ld t2, ((32-15)&0x0f)*8(sp)
    ld t3, ((32-7)&0x0f)*8(sp)
    ld t4, (32&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(32&0x0f)(sp)

            ld t4, 8*32(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((33-2)&0x0f)*8(sp)
    ld t2, ((33-15)&0x0f)*8(sp)
    ld t3, ((33-7)&0x0f)*8(sp)
    ld t4, (33&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(33&0x0f)(sp)

            ld t4, 8*33(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((34-2)&0x0f)*8(sp)
    ld t2, ((34-15)&0x0f)*8(sp)
    ld t3, ((34-7)&0x0f)*8(sp)
    ld t4, (34&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(34&0x0f)(sp)

            ld t4, 8*34(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((35-2)&0x0f)*8(sp)
    ld t2, ((35-15)&0x0f)*8(sp)
    ld t3, ((35-7)&0x0f)*8(sp)
    ld t4, (35&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(35&0x0f)(sp)

            ld t4, 8*35(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((36-2)&0x0f)*8(sp)
    ld t2, ((36-15)&0x0f)*8(sp)
    ld t3, ((36-7)&0x0f)*8(sp)
    ld t4, (36&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(36&0x0f)(sp)

            ld t4, 8*36(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((37-2)&0x0f)*8(sp)
    ld t2, ((37-15)&0x0f)*8(sp)
    ld t3, ((37-7)&0x0f)*8(sp)
    ld t4, (37&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(37&0x0f)(sp)

            ld t4, 8*37(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((38-2)&0x0f)*8(sp)
    ld t2, ((38-15)&0x0f)*8(sp)
    ld t3, ((38-7)&0x0f)*8(sp)
    ld t4, (38&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(38&0x0f)(sp)

            ld t4, 8*38(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((39-2)&0x0f)*8(sp)
    ld t2, ((39-15)&0x0f)*8(sp)
    ld t3, ((39-7)&0x0f)*8(sp)
    ld t4, (39&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(39&0x0f)(sp)

            ld t4, 8*39(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((40-2)&0x0f)*8(sp)
    ld t2, ((40-15)&0x0f)*8(sp)
    ld t3, ((40-7)&0x0f)*8(sp)
    ld t4, (40&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(40&0x0f)(sp)

            ld t4, 8*40(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((41-2)&0x0f)*8(sp)
    ld t2, ((41-15)&0x0f)*8(sp)
    ld t3, ((41-7)&0x0f)*8(sp)
    ld t4, (41&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(41&0x0f)(sp)

            ld t4, 8*41(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((42-2)&0x0f)*8(sp)
    ld t2, ((42-15)&0x0f)*8(sp)
    ld t3, ((42-7)&0x0f)*8(sp)
    ld t4, (42&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(42&0x0f)(sp)

            ld t4, 8*42(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((43-2)&0x0f)*8(sp)
    ld t2, ((43-15)&0x0f)*8(sp)
    ld t3, ((43-7)&0x0f)*8(sp)
    ld t4, (43&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(43&0x0f)(sp)

            ld t4, 8*43(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((44-2)&0x0f)*8(sp)
    ld t2, ((44-15)&0x0f)*8(sp)
    ld t3, ((44-7)&0x0f)*8(sp)
    ld t4, (44&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(44&0x0f)(sp)

            ld t4, 8*44(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((45-2)&0x0f)*8(sp)
    ld t2, ((45-15)&0x0f)*8(sp)
    ld t3, ((45-7)&0x0f)*8(sp)
    ld t4, (45&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(45&0x0f)(sp)

            ld t4, 8*45(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((46-2)&0x0f)*8(sp)
    ld t2, ((46-15)&0x0f)*8(sp)
    ld t3, ((46-7)&0x0f)*8(sp)
    ld t4, (46&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(46&0x0f)(sp)

            ld t4, 8*46(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((47-2)&0x0f)*8(sp)
    ld t2, ((47-15)&0x0f)*8(sp)
    ld t3, ((47-7)&0x0f)*8(sp)
    ld t4, (47&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(47&0x0f)(sp)

            ld t4, 8*47(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((48-2)&0x0f)*8(sp)
    ld t2, ((48-15)&0x0f)*8(sp)
    ld t3, ((48-7)&0x0f)*8(sp)
    ld t4, (48&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(48&0x0f)(sp)

            ld t4, 8*48(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((49-2)&0x0f)*8(sp)
    ld t2, ((49-15)&0x0f)*8(sp)
    ld t3, ((49-7)&0x0f)*8(sp)
    ld t4, (49&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(49&0x0f)(sp)

            ld t4, 8*49(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((50-2)&0x0f)*8(sp)
    ld t2, ((50-15)&0x0f)*8(sp)
    ld t3, ((50-7)&0x0f)*8(sp)
    ld t4, (50&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(50&0x0f)(sp)

            ld t4, 8*50(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((51-2)&0x0f)*8(sp)
    ld t2, ((51-15)&0x0f)*8(sp)
    ld t3, ((51-7)&0x0f)*8(sp)
    ld t4, (51&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(51&0x0f)(sp)

            ld t4, 8*51(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((52-2)&0x0f)*8(sp)
    ld t2, ((52-15)&0x0f)*8(sp)
    ld t3, ((52-7)&0x0f)*8(sp)
    ld t4, (52&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(52&0x0f)(sp)

            ld t4, 8*52(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((53-2)&0x0f)*8(sp)
    ld t2, ((53-15)&0x0f)*8(sp)
    ld t3, ((53-7)&0x0f)*8(sp)
    ld t4, (53&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(53&0x0f)(sp)

            ld t4, 8*53(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((54-2)&0x0f)*8(sp)
    ld t2, ((54-15)&0x0f)*8(sp)
    ld t3, ((54-7)&0x0f)*8(sp)
    ld t4, (54&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(54&0x0f)(sp)

            ld t4, 8*54(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((55-2)&0x0f)*8(sp)
    ld t2, ((55-15)&0x0f)*8(sp)
    ld t3, ((55-7)&0x0f)*8(sp)
    ld t4, (55&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(55&0x0f)(sp)

            ld t4, 8*55(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((56-2)&0x0f)*8(sp)
    ld t2, ((56-15)&0x0f)*8(sp)
    ld t3, ((56-7)&0x0f)*8(sp)
    ld t4, (56&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(56&0x0f)(sp)

            ld t4, 8*56(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((57-2)&0x0f)*8(sp)
    ld t2, ((57-15)&0x0f)*8(sp)
    ld t3, ((57-7)&0x0f)*8(sp)
    ld t4, (57&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(57&0x0f)(sp)

            ld t4, 8*57(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((58-2)&0x0f)*8(sp)
    ld t2, ((58-15)&0x0f)*8(sp)
    ld t3, ((58-7)&0x0f)*8(sp)
    ld t4, (58&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(58&0x0f)(sp)

            ld t4, 8*58(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((59-2)&0x0f)*8(sp)
    ld t2, ((59-15)&0x0f)*8(sp)
    ld t3, ((59-7)&0x0f)*8(sp)
    ld t4, (59&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(59&0x0f)(sp)

            ld t4, 8*59(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((60-2)&0x0f)*8(sp)
    ld t2, ((60-15)&0x0f)*8(sp)
    ld t3, ((60-7)&0x0f)*8(sp)
    ld t4, (60&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(60&0x0f)(sp)

            ld t4, 8*60(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((61-2)&0x0f)*8(sp)
    ld t2, ((61-15)&0x0f)*8(sp)
    ld t3, ((61-7)&0x0f)*8(sp)
    ld t4, (61&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(61&0x0f)(sp)

            ld t4, 8*61(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((62-2)&0x0f)*8(sp)
    ld t2, ((62-15)&0x0f)*8(sp)
    ld t3, ((62-7)&0x0f)*8(sp)
    ld t4, (62&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(62&0x0f)(sp)

            ld t4, 8*62(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((63-2)&0x0f)*8(sp)
    ld t2, ((63-15)&0x0f)*8(sp)
    ld t3, ((63-7)&0x0f)*8(sp)
    ld t4, (63&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(63&0x0f)(sp)

            ld t4, 8*63(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((64-2)&0x0f)*8(sp)
    ld t2, ((64-15)&0x0f)*8(sp)
    ld t3, ((64-7)&0x0f)*8(sp)
    ld t4, (64&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(64&0x0f)(sp)

            ld t4, 8*64(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((65-2)&0x0f)*8(sp)
    ld t2, ((65-15)&0x0f)*8(sp)
    ld t3, ((65-7)&0x0f)*8(sp)
    ld t4, (65&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(65&0x0f)(sp)

            ld t4, 8*65(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((66-2)&0x0f)*8(sp)
    ld t2, ((66-15)&0x0f)*8(sp)
    ld t3, ((66-7)&0x0f)*8(sp)
    ld t4, (66&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(66&0x0f)(sp)

            ld t4, 8*66(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((67-2)&0x0f)*8(sp)
    ld t2, ((67-15)&0x0f)*8(sp)
    ld t3, ((67-7)&0x0f)*8(sp)
    ld t4, (67&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(67&0x0f)(sp)

            ld t4, 8*67(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((68-2)&0x0f)*8(sp)
    ld t2, ((68-15)&0x0f)*8(sp)
    ld t3, ((68-7)&0x0f)*8(sp)
    ld t4, (68&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(68&0x0f)(sp)

            ld t4, 8*68(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((69-2)&0x0f)*8(sp)
    ld t2, ((69-15)&0x0f)*8(sp)
    ld t3, ((69-7)&0x0f)*8(sp)
    ld t4, (69&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(69&0x0f)(sp)

            ld t4, 8*69(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((70-2)&0x0f)*8(sp)
    ld t2, ((70-15)&0x0f)*8(sp)
    ld t3, ((70-7)&0x0f)*8(sp)
    ld t4, (70&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(70&0x0f)(sp)

            ld t4, 8*70(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((71-2)&0x0f)*8(sp)
    ld t2, ((71-15)&0x0f)*8(sp)
    ld t3, ((71-7)&0x0f)*8(sp)
    ld t4, (71&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(71&0x0f)(sp)

            ld t4, 8*71(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



            ld t1, ((72-2)&0x0f)*8(sp)
    ld t2, ((72-15)&0x0f)*8(sp)
    ld t3, ((72-7)&0x0f)*8(sp)
    ld t4, (72&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(72&0x0f)(sp)

            ld t4, 8*72(t0)
    add s9, s9, t1
    add s9, s9, t4
    .word 1626035091
    .word 1630232083
    .word 1654349459
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    add s9, s9, t2
    xor t1, t1, s8
    add t1, t1, s9

        # Sigma0
    .word 1640584083
    .word 1646878227
    .word 1652121235
    xor t2, t2, t3
    # Maj
    xor t5, s3, s4
    and t3, s3, s4
    and t5, t5, s2
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s5, s5, t1
    add s9, t2, t1


            ld t1, ((73-2)&0x0f)*8(sp)
    ld t2, ((73-15)&0x0f)*8(sp)
    ld t3, ((73-7)&0x0f)*8(sp)
    ld t4, (73&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(73&0x0f)(sp)

            ld t4, 8*73(t0)
    add s8, s8, t1
    add s8, s8, t4
    .word 1626002323
    .word 1630199315
    .word 1654316691
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    add s8, s8, t2
    xor t1, t1, s7
    add t1, t1, s8

        # Sigma0
    .word 1640813459
    .word 1647107603
    .word 1652350611
    xor t2, t2, t3
    # Maj
    xor t5, s2, s3
    and t3, s2, s3
    and t5, t5, s9
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s4, s4, t1
    add s8, t2, t1


            ld t1, ((74-2)&0x0f)*8(sp)
    ld t2, ((74-15)&0x0f)*8(sp)
    ld t3, ((74-7)&0x0f)*8(sp)
    ld t4, (74&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(74&0x0f)(sp)

            ld t4, 8*74(t0)
    add s7, s7, t1
    add s7, s7, t4
    .word 1625969555
    .word 1630166547
    .word 1654283923
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    add s7, s7, t2
    xor t1, t1, s6
    add t1, t1, s7

        # Sigma0
    .word 1640780691
    .word 1647074835
    .word 1652317843
    xor t2, t2, t3
    # Maj
    xor t5, s9, s2
    and t3, s9, s2
    and t5, t5, s8
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s3, s3, t1
    add s7, t2, t1


            ld t1, ((75-2)&0x0f)*8(sp)
    ld t2, ((75-15)&0x0f)*8(sp)
    ld t3, ((75-7)&0x0f)*8(sp)
    ld t4, (75&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(75&0x0f)(sp)

            ld t4, 8*75(t0)
    add s6, s6, t1
    add s6, s6, t4
    .word 1625936787
    .word 1630133779
    .word 1654251155
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    add s6, s6, t2
    xor t1, t1, s5
    add t1, t1, s6

        # Sigma0
    .word 1640747923
    .word 1647042067
    .word 1652285075
    xor t2, t2, t3
    # Maj
    xor t5, s8, s9
    and t3, s8, s9
    and t5, t5, s7
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s2, s2, t1
    add s6, t2, t1



            ld t1, ((76-2)&0x0f)*8(sp)
    ld t2, ((76-15)&0x0f)*8(sp)
    ld t3, ((76-7)&0x0f)*8(sp)
    ld t4, (76&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(76&0x0f)(sp)

            ld t4, 8*76(t0)
    add s5, s5, t1
    add s5, s5, t4
    .word 1625904019
    .word 1630101011
    .word 1654218387
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    add s5, s5, t2
    xor t1, t1, s4
    add t1, t1, s5

        # Sigma0
    .word 1640715155
    .word 1647009299
    .word 1652252307
    xor t2, t2, t3
    # Maj
    xor t5, s7, s8
    and t3, s7, s8
    and t5, t5, s6
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s9, s9, t1
    add s5, t2, t1


            ld t1, ((77-2)&0x0f)*8(sp)
    ld t2, ((77-15)&0x0f)*8(sp)
    ld t3, ((77-7)&0x0f)*8(sp)
    ld t4, (77&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(77&0x0f)(sp)

            ld t4, 8*77(t0)
    add s4, s4, t1
    add s4, s4, t4
    .word 1626133395
    .word 1630330387
    .word 1654447763
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    add s4, s4, t2
    xor t1, t1, s3
    add t1, t1, s4

        # Sigma0
    .word 1640682387
    .word 1646976531
    .word 1652219539
    xor t2, t2, t3
    # Maj
    xor t5, s6, s7
    and t3, s6, s7
    and t5, t5, s5
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s8, s8, t1
    add s4, t2, t1


            ld t1, ((78-2)&0x0f)*8(sp)
    ld t2, ((78-15)&0x0f)*8(sp)
    ld t3, ((78-7)&0x0f)*8(sp)
    ld t4, (78&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(78&0x0f)(sp)

            ld t4, 8*78(t0)
    add s3, s3, t1
    add s3, s3, t4
    .word 1626100627
    .word 1630297619
    .word 1654414995
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    add s3, s3, t2
    xor t1, t1, s2
    add t1, t1, s3

        # Sigma0
    .word 1640649619
    .word 1646943763
    .word 1652186771
    xor t2, t2, t3
    # Maj
    xor t5, s5, s6
    and t3, s5, s6
    and t5, t5, s4
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s7, s7, t1
    add s3, t2, t1


            ld t1, ((79-2)&0x0f)*8(sp)
    ld t2, ((79-15)&0x0f)*8(sp)
    ld t3, ((79-7)&0x0f)*8(sp)
    ld t4, (79&0x0f)*8(sp)
    .word 1630756627
    .word 1674796947
    srli t1, t1, 6
    xor t1, t1, t5
    xor t1, t1, t6
    add t1, t1, t3
    .word 1611915027
    .word 1619255187
    srli t2, t2, 7
    xor t2, t2, t5
    xor t2, t2, t6
    add t1, t1, t2
    add t1, t1, t4
    sd t1, 8*(79&0x0f)(sp)

            ld t4, 8*79(t0)
    add s2, s2, t1
    add s2, s2, t4
    .word 1626067859
    .word 1630264851
    .word 1654382227
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    add s2, s2, t2
    xor t1, t1, s9
    add t1, t1, s2

        # Sigma0
    .word 1640616851
    .word 1646910995
    .word 1652154003
    xor t2, t2, t3
    # Maj
    xor t5, s4, s5
    and t3, s4, s5
    and t5, t5, s3
    xor t2, t2, t4
    xor t3, t3, t5
    # T2
    add t2, t2, t3

    add s6, s6, t1
    add s2, t2, t1



    ld t1, 0(a0)
    ld t2, 8(a0)
    ld t3, 16(a0)
    ld t4, 24(a0)

    add s2, s2, t1
    add s3, s3, t2
    add s4, s4, t3
    add s5, s5, t4

    sd s2, 0(a0)
    sd s3, 8(a0)
    sd s4, 16(a0)
    sd s5, 24(a0)

    ld t1, 32(a0)
    ld t2, 40(a0)
    ld t3, 48(a0)
    ld t4, 56(a0)

    add s6, s6, t1
    add s7, s7, t2
    add s8, s8, t3
    add s9, s9, t4

    sd s6, 32(a0)
    sd s7, 40(a0)
    sd s8, 48(a0)
    sd s9, 56(a0)

    addi a1, a1, 128

    bnez a2, L_round_loop

    addi sp, sp, 128

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
.size sha512_block_data_order_zbb,.-sha512_block_data_order_zbb

.section .rodata
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
