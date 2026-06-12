.text
.p2align 3
.globl sha256_block_data_order_zbb
.type   sha256_block_data_order_zbb,@function
sha256_block_data_order_zbb:

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

    la t0, K256

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

                lw t1, (4*0+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*0(sp)

            lw t4, 4*0(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


                lw t1, (4*1+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*1(sp)

            lw t4, 4*1(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


                lw t1, (4*2+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*2(sp)

            lw t4, 4*2(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


                lw t1, (4*3+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*3(sp)

            lw t4, 4*3(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



                lw t1, (4*4+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*4(sp)

            lw t4, 4*4(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


                lw t1, (4*5+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*5(sp)

            lw t4, 4*5(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


                lw t1, (4*6+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*6(sp)

            lw t4, 4*6(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


                lw t1, (4*7+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*7(sp)

            lw t4, 4*7(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



                lw t1, (4*8+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*8(sp)

            lw t4, 4*8(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


                lw t1, (4*9+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*9(sp)

            lw t4, 4*9(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


                lw t1, (4*10+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*10(sp)

            lw t4, 4*10(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


                lw t1, (4*11+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*11(sp)

            lw t4, 4*11(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



                lw t1, (4*12+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*12(sp)

            lw t4, 4*12(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


                lw t1, (4*13+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*13(sp)

            lw t4, 4*13(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


                lw t1, (4*14+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*14(sp)

            lw t4, 4*14(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


                lw t1, (4*15+0)(a1)
        .word 1803768595 # rev8 t1, t1
        srli t1, t1, 32
        sw t1, 4*15(sp)

            lw t4, 4*15(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



            lw t1, ((16-2)&0x0f)*4(sp)
    lw t2, ((16-15)&0x0f)*4(sp)
    lw t3, ((16-7)&0x0f)*4(sp)
    lw t4, (16&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(16&0x0f)(sp)

            lw t4, 4*16(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


            lw t1, ((17-2)&0x0f)*4(sp)
    lw t2, ((17-15)&0x0f)*4(sp)
    lw t3, ((17-7)&0x0f)*4(sp)
    lw t4, (17&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(17&0x0f)(sp)

            lw t4, 4*17(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


            lw t1, ((18-2)&0x0f)*4(sp)
    lw t2, ((18-15)&0x0f)*4(sp)
    lw t3, ((18-7)&0x0f)*4(sp)
    lw t4, (18&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(18&0x0f)(sp)

            lw t4, 4*18(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


            lw t1, ((19-2)&0x0f)*4(sp)
    lw t2, ((19-15)&0x0f)*4(sp)
    lw t3, ((19-7)&0x0f)*4(sp)
    lw t4, (19&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(19&0x0f)(sp)

            lw t4, 4*19(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



            lw t1, ((20-2)&0x0f)*4(sp)
    lw t2, ((20-15)&0x0f)*4(sp)
    lw t3, ((20-7)&0x0f)*4(sp)
    lw t4, (20&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(20&0x0f)(sp)

            lw t4, 4*20(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


            lw t1, ((21-2)&0x0f)*4(sp)
    lw t2, ((21-15)&0x0f)*4(sp)
    lw t3, ((21-7)&0x0f)*4(sp)
    lw t4, (21&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(21&0x0f)(sp)

            lw t4, 4*21(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


            lw t1, ((22-2)&0x0f)*4(sp)
    lw t2, ((22-15)&0x0f)*4(sp)
    lw t3, ((22-7)&0x0f)*4(sp)
    lw t4, (22&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(22&0x0f)(sp)

            lw t4, 4*22(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


            lw t1, ((23-2)&0x0f)*4(sp)
    lw t2, ((23-15)&0x0f)*4(sp)
    lw t3, ((23-7)&0x0f)*4(sp)
    lw t4, (23&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(23&0x0f)(sp)

            lw t4, 4*23(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



            lw t1, ((24-2)&0x0f)*4(sp)
    lw t2, ((24-15)&0x0f)*4(sp)
    lw t3, ((24-7)&0x0f)*4(sp)
    lw t4, (24&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(24&0x0f)(sp)

            lw t4, 4*24(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


            lw t1, ((25-2)&0x0f)*4(sp)
    lw t2, ((25-15)&0x0f)*4(sp)
    lw t3, ((25-7)&0x0f)*4(sp)
    lw t4, (25&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(25&0x0f)(sp)

            lw t4, 4*25(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


            lw t1, ((26-2)&0x0f)*4(sp)
    lw t2, ((26-15)&0x0f)*4(sp)
    lw t3, ((26-7)&0x0f)*4(sp)
    lw t4, (26&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(26&0x0f)(sp)

            lw t4, 4*26(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


            lw t1, ((27-2)&0x0f)*4(sp)
    lw t2, ((27-15)&0x0f)*4(sp)
    lw t3, ((27-7)&0x0f)*4(sp)
    lw t4, (27&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(27&0x0f)(sp)

            lw t4, 4*27(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



            lw t1, ((28-2)&0x0f)*4(sp)
    lw t2, ((28-15)&0x0f)*4(sp)
    lw t3, ((28-7)&0x0f)*4(sp)
    lw t4, (28&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(28&0x0f)(sp)

            lw t4, 4*28(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


            lw t1, ((29-2)&0x0f)*4(sp)
    lw t2, ((29-15)&0x0f)*4(sp)
    lw t3, ((29-7)&0x0f)*4(sp)
    lw t4, (29&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(29&0x0f)(sp)

            lw t4, 4*29(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


            lw t1, ((30-2)&0x0f)*4(sp)
    lw t2, ((30-15)&0x0f)*4(sp)
    lw t3, ((30-7)&0x0f)*4(sp)
    lw t4, (30&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(30&0x0f)(sp)

            lw t4, 4*30(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


            lw t1, ((31-2)&0x0f)*4(sp)
    lw t2, ((31-15)&0x0f)*4(sp)
    lw t3, ((31-7)&0x0f)*4(sp)
    lw t4, (31&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(31&0x0f)(sp)

            lw t4, 4*31(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



            lw t1, ((32-2)&0x0f)*4(sp)
    lw t2, ((32-15)&0x0f)*4(sp)
    lw t3, ((32-7)&0x0f)*4(sp)
    lw t4, (32&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(32&0x0f)(sp)

            lw t4, 4*32(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


            lw t1, ((33-2)&0x0f)*4(sp)
    lw t2, ((33-15)&0x0f)*4(sp)
    lw t3, ((33-7)&0x0f)*4(sp)
    lw t4, (33&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(33&0x0f)(sp)

            lw t4, 4*33(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


            lw t1, ((34-2)&0x0f)*4(sp)
    lw t2, ((34-15)&0x0f)*4(sp)
    lw t3, ((34-7)&0x0f)*4(sp)
    lw t4, (34&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(34&0x0f)(sp)

            lw t4, 4*34(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


            lw t1, ((35-2)&0x0f)*4(sp)
    lw t2, ((35-15)&0x0f)*4(sp)
    lw t3, ((35-7)&0x0f)*4(sp)
    lw t4, (35&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(35&0x0f)(sp)

            lw t4, 4*35(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



            lw t1, ((36-2)&0x0f)*4(sp)
    lw t2, ((36-15)&0x0f)*4(sp)
    lw t3, ((36-7)&0x0f)*4(sp)
    lw t4, (36&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(36&0x0f)(sp)

            lw t4, 4*36(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


            lw t1, ((37-2)&0x0f)*4(sp)
    lw t2, ((37-15)&0x0f)*4(sp)
    lw t3, ((37-7)&0x0f)*4(sp)
    lw t4, (37&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(37&0x0f)(sp)

            lw t4, 4*37(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


            lw t1, ((38-2)&0x0f)*4(sp)
    lw t2, ((38-15)&0x0f)*4(sp)
    lw t3, ((38-7)&0x0f)*4(sp)
    lw t4, (38&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(38&0x0f)(sp)

            lw t4, 4*38(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


            lw t1, ((39-2)&0x0f)*4(sp)
    lw t2, ((39-15)&0x0f)*4(sp)
    lw t3, ((39-7)&0x0f)*4(sp)
    lw t4, (39&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(39&0x0f)(sp)

            lw t4, 4*39(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



            lw t1, ((40-2)&0x0f)*4(sp)
    lw t2, ((40-15)&0x0f)*4(sp)
    lw t3, ((40-7)&0x0f)*4(sp)
    lw t4, (40&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(40&0x0f)(sp)

            lw t4, 4*40(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


            lw t1, ((41-2)&0x0f)*4(sp)
    lw t2, ((41-15)&0x0f)*4(sp)
    lw t3, ((41-7)&0x0f)*4(sp)
    lw t4, (41&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(41&0x0f)(sp)

            lw t4, 4*41(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


            lw t1, ((42-2)&0x0f)*4(sp)
    lw t2, ((42-15)&0x0f)*4(sp)
    lw t3, ((42-7)&0x0f)*4(sp)
    lw t4, (42&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(42&0x0f)(sp)

            lw t4, 4*42(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


            lw t1, ((43-2)&0x0f)*4(sp)
    lw t2, ((43-15)&0x0f)*4(sp)
    lw t3, ((43-7)&0x0f)*4(sp)
    lw t4, (43&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(43&0x0f)(sp)

            lw t4, 4*43(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



            lw t1, ((44-2)&0x0f)*4(sp)
    lw t2, ((44-15)&0x0f)*4(sp)
    lw t3, ((44-7)&0x0f)*4(sp)
    lw t4, (44&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(44&0x0f)(sp)

            lw t4, 4*44(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


            lw t1, ((45-2)&0x0f)*4(sp)
    lw t2, ((45-15)&0x0f)*4(sp)
    lw t3, ((45-7)&0x0f)*4(sp)
    lw t4, (45&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(45&0x0f)(sp)

            lw t4, 4*45(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


            lw t1, ((46-2)&0x0f)*4(sp)
    lw t2, ((46-15)&0x0f)*4(sp)
    lw t3, ((46-7)&0x0f)*4(sp)
    lw t4, (46&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(46&0x0f)(sp)

            lw t4, 4*46(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


            lw t1, ((47-2)&0x0f)*4(sp)
    lw t2, ((47-15)&0x0f)*4(sp)
    lw t3, ((47-7)&0x0f)*4(sp)
    lw t4, (47&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(47&0x0f)(sp)

            lw t4, 4*47(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



            lw t1, ((48-2)&0x0f)*4(sp)
    lw t2, ((48-15)&0x0f)*4(sp)
    lw t3, ((48-7)&0x0f)*4(sp)
    lw t4, (48&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(48&0x0f)(sp)

            lw t4, 4*48(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


            lw t1, ((49-2)&0x0f)*4(sp)
    lw t2, ((49-15)&0x0f)*4(sp)
    lw t3, ((49-7)&0x0f)*4(sp)
    lw t4, (49&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(49&0x0f)(sp)

            lw t4, 4*49(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


            lw t1, ((50-2)&0x0f)*4(sp)
    lw t2, ((50-15)&0x0f)*4(sp)
    lw t3, ((50-7)&0x0f)*4(sp)
    lw t4, (50&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(50&0x0f)(sp)

            lw t4, 4*50(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


            lw t1, ((51-2)&0x0f)*4(sp)
    lw t2, ((51-15)&0x0f)*4(sp)
    lw t3, ((51-7)&0x0f)*4(sp)
    lw t4, (51&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(51&0x0f)(sp)

            lw t4, 4*51(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



            lw t1, ((52-2)&0x0f)*4(sp)
    lw t2, ((52-15)&0x0f)*4(sp)
    lw t3, ((52-7)&0x0f)*4(sp)
    lw t4, (52&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(52&0x0f)(sp)

            lw t4, 4*52(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


            lw t1, ((53-2)&0x0f)*4(sp)
    lw t2, ((53-15)&0x0f)*4(sp)
    lw t3, ((53-7)&0x0f)*4(sp)
    lw t4, (53&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(53&0x0f)(sp)

            lw t4, 4*53(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


            lw t1, ((54-2)&0x0f)*4(sp)
    lw t2, ((54-15)&0x0f)*4(sp)
    lw t3, ((54-7)&0x0f)*4(sp)
    lw t4, (54&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(54&0x0f)(sp)

            lw t4, 4*54(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


            lw t1, ((55-2)&0x0f)*4(sp)
    lw t2, ((55-15)&0x0f)*4(sp)
    lw t3, ((55-7)&0x0f)*4(sp)
    lw t4, (55&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(55&0x0f)(sp)

            lw t4, 4*55(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



            lw t1, ((56-2)&0x0f)*4(sp)
    lw t2, ((56-15)&0x0f)*4(sp)
    lw t3, ((56-7)&0x0f)*4(sp)
    lw t4, (56&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(56&0x0f)(sp)

            lw t4, 4*56(t0)
    addw s9, s9, t1
    addw s9, s9, t4
        .word 1617646491  # roriw t2, s6, 6
        .word 1622892059  # roriw t3, s6, 11
        .word 1637572251  # roriw t4, s6, 25
    xor t2, t2, t3
    xor t1, s7, s8
    xor t2, t2, t4
    and t1, t1, s6
    addw s9, s9, t2
    xor t1, t1, s8
    addw t1, t1, s9

        # Sum0
        .word 1613321115  # roriw t2, s2, 2
        .word 1624858139  # roriw t3, s2, 13
        .word 1634295451  # roriw t4, s2, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s3, s4
    and t3, s3, s4
    and t4, t4, s2
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s5, s5, t1
    addw s9, t2, t1


            lw t1, ((57-2)&0x0f)*4(sp)
    lw t2, ((57-15)&0x0f)*4(sp)
    lw t3, ((57-7)&0x0f)*4(sp)
    lw t4, (57&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(57&0x0f)(sp)

            lw t4, 4*57(t0)
    addw s8, s8, t1
    addw s8, s8, t4
        .word 1617613723  # roriw t2, s5, 6
        .word 1622859291  # roriw t3, s5, 11
        .word 1637539483  # roriw t4, s5, 25
    xor t2, t2, t3
    xor t1, s6, s7
    xor t2, t2, t4
    and t1, t1, s5
    addw s8, s8, t2
    xor t1, t1, s7
    addw t1, t1, s8

        # Sum0
        .word 1613550491  # roriw t2, s9, 2
        .word 1625087515  # roriw t3, s9, 13
        .word 1634524827  # roriw t4, s9, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s2, s3
    and t3, s2, s3
    and t4, t4, s9
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s4, s4, t1
    addw s8, t2, t1


            lw t1, ((58-2)&0x0f)*4(sp)
    lw t2, ((58-15)&0x0f)*4(sp)
    lw t3, ((58-7)&0x0f)*4(sp)
    lw t4, (58&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(58&0x0f)(sp)

            lw t4, 4*58(t0)
    addw s7, s7, t1
    addw s7, s7, t4
        .word 1617580955  # roriw t2, s4, 6
        .word 1622826523  # roriw t3, s4, 11
        .word 1637506715  # roriw t4, s4, 25
    xor t2, t2, t3
    xor t1, s5, s6
    xor t2, t2, t4
    and t1, t1, s4
    addw s7, s7, t2
    xor t1, t1, s6
    addw t1, t1, s7

        # Sum0
        .word 1613517723  # roriw t2, s8, 2
        .word 1625054747  # roriw t3, s8, 13
        .word 1634492059  # roriw t4, s8, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s9, s2
    and t3, s9, s2
    and t4, t4, s8
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s3, s3, t1
    addw s7, t2, t1


            lw t1, ((59-2)&0x0f)*4(sp)
    lw t2, ((59-15)&0x0f)*4(sp)
    lw t3, ((59-7)&0x0f)*4(sp)
    lw t4, (59&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(59&0x0f)(sp)

            lw t4, 4*59(t0)
    addw s6, s6, t1
    addw s6, s6, t4
        .word 1617548187  # roriw t2, s3, 6
        .word 1622793755  # roriw t3, s3, 11
        .word 1637473947  # roriw t4, s3, 25
    xor t2, t2, t3
    xor t1, s4, s5
    xor t2, t2, t4
    and t1, t1, s3
    addw s6, s6, t2
    xor t1, t1, s5
    addw t1, t1, s6

        # Sum0
        .word 1613484955  # roriw t2, s7, 2
        .word 1625021979  # roriw t3, s7, 13
        .word 1634459291  # roriw t4, s7, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s8, s9
    and t3, s8, s9
    and t4, t4, s7
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s2, s2, t1
    addw s6, t2, t1



            lw t1, ((60-2)&0x0f)*4(sp)
    lw t2, ((60-15)&0x0f)*4(sp)
    lw t3, ((60-7)&0x0f)*4(sp)
    lw t4, (60&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(60&0x0f)(sp)

            lw t4, 4*60(t0)
    addw s5, s5, t1
    addw s5, s5, t4
        .word 1617515419  # roriw t2, s2, 6
        .word 1622760987  # roriw t3, s2, 11
        .word 1637441179  # roriw t4, s2, 25
    xor t2, t2, t3
    xor t1, s3, s4
    xor t2, t2, t4
    and t1, t1, s2
    addw s5, s5, t2
    xor t1, t1, s4
    addw t1, t1, s5

        # Sum0
        .word 1613452187  # roriw t2, s6, 2
        .word 1624989211  # roriw t3, s6, 13
        .word 1634426523  # roriw t4, s6, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s7, s8
    and t3, s7, s8
    and t4, t4, s6
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s9, s9, t1
    addw s5, t2, t1


            lw t1, ((61-2)&0x0f)*4(sp)
    lw t2, ((61-15)&0x0f)*4(sp)
    lw t3, ((61-7)&0x0f)*4(sp)
    lw t4, (61&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(61&0x0f)(sp)

            lw t4, 4*61(t0)
    addw s4, s4, t1
    addw s4, s4, t4
        .word 1617744795  # roriw t2, s9, 6
        .word 1622990363  # roriw t3, s9, 11
        .word 1637670555  # roriw t4, s9, 25
    xor t2, t2, t3
    xor t1, s2, s3
    xor t2, t2, t4
    and t1, t1, s9
    addw s4, s4, t2
    xor t1, t1, s3
    addw t1, t1, s4

        # Sum0
        .word 1613419419  # roriw t2, s5, 2
        .word 1624956443  # roriw t3, s5, 13
        .word 1634393755  # roriw t4, s5, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s6, s7
    and t3, s6, s7
    and t4, t4, s5
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s8, s8, t1
    addw s4, t2, t1


            lw t1, ((62-2)&0x0f)*4(sp)
    lw t2, ((62-15)&0x0f)*4(sp)
    lw t3, ((62-7)&0x0f)*4(sp)
    lw t4, (62&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(62&0x0f)(sp)

            lw t4, 4*62(t0)
    addw s3, s3, t1
    addw s3, s3, t4
        .word 1617712027  # roriw t2, s8, 6
        .word 1622957595  # roriw t3, s8, 11
        .word 1637637787  # roriw t4, s8, 25
    xor t2, t2, t3
    xor t1, s9, s2
    xor t2, t2, t4
    and t1, t1, s8
    addw s3, s3, t2
    xor t1, t1, s2
    addw t1, t1, s3

        # Sum0
        .word 1613386651  # roriw t2, s4, 2
        .word 1624923675  # roriw t3, s4, 13
        .word 1634360987  # roriw t4, s4, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s5, s6
    and t3, s5, s6
    and t4, t4, s4
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s7, s7, t1
    addw s3, t2, t1


            lw t1, ((63-2)&0x0f)*4(sp)
    lw t2, ((63-15)&0x0f)*4(sp)
    lw t3, ((63-7)&0x0f)*4(sp)
    lw t4, (63&0x0f)*4(sp)
        .word 1628659483  # roriw t5, t1, 17
        .word 1630756763  # roriw t6, t1, 19
    srliw t1, t1, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618206491  # roriw t5, t2, 7
        .word 1629740955  # roriw t6, t2, 18
    srliw t2, t2, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t1, t1, t2
    addw t1, t1, t4
    sw t1, 4*(63&0x0f)(sp)

            lw t4, 4*63(t0)
    addw s2, s2, t1
    addw s2, s2, t4
        .word 1617679259  # roriw t2, s7, 6
        .word 1622924827  # roriw t3, s7, 11
        .word 1637605019  # roriw t4, s7, 25
    xor t2, t2, t3
    xor t1, s8, s9
    xor t2, t2, t4
    and t1, t1, s7
    addw s2, s2, t2
    xor t1, t1, s9
    addw t1, t1, s2

        # Sum0
        .word 1613353883  # roriw t2, s3, 2
        .word 1624890907  # roriw t3, s3, 13
        .word 1634328219  # roriw t4, s3, 22
    xor t2, t2, t3
    xor t2, t2, t4
    # Maj
    xor t4, s4, s5
    and t3, s4, s5
    and t4, t4, s3
    xor t4, t4, t3
    # T2
    addw t2, t2, t4

    addw s6, s6, t1
    addw s2, t2, t1



    lw t1, 0(a0)
    lw t2, 4(a0)
    lw t3, 8(a0)
    lw t4, 12(a0)

    addw s2, s2, t1
    addw s3, s3, t2
    addw s4, s4, t3
    addw s5, s5, t4

    sw s2, 0(a0)
    sw s3, 4(a0)
    sw s4, 8(a0)
    sw s5, 12(a0)

    lw t1, 16(a0)
    lw t2, 20(a0)
    lw t3, 24(a0)
    lw t4, 28(a0)

    addw s6, s6, t1
    addw s7, s7, t2
    addw s8, s8, t3
    addw s9, s9, t4

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
.size sha256_block_data_order_zbb,.-sha256_block_data_order_zbb

.section .rodata
.p2align 3
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
