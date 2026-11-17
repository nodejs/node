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

    andi a3, a1, 7
    beqz a3, L_round_loop
    andi a1, a1, -8
    slli a3, a3, 3
    li t1, 64
    sub a4, t1, a3

L_round_loop:
    # Decrement length by 1
    addi a2, a2, -1

    # b ^ c for round 0's Maj; later rounds reuse the previous a ^ b
    xor s10, s3, s4
    bnez a3, L_load_misaligned
                ld a6, 4*0(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*0(sp)
        sw a6, (4*0+4)(sp)

            lw t4, 4*0(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


        
            lw t4, 4*1(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


                ld a6, 4*2(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*2(sp)
        sw a6, (4*2+4)(sp)

            lw t4, 4*2(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


        
            lw t4, 4*3(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


                ld a6, 4*4(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*4(sp)
        sw a6, (4*4+4)(sp)

            lw t4, 4*4(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


        
            lw t4, 4*5(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


                ld a6, 4*6(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*6(sp)
        sw a6, (4*6+4)(sp)

            lw t4, 4*6(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


        
            lw t4, 4*7(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


                ld a6, 4*8(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*8(sp)
        sw a6, (4*8+4)(sp)

            lw t4, 4*8(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


        
            lw t4, 4*9(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


                ld a6, 4*10(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*10(sp)
        sw a6, (4*10+4)(sp)

            lw t4, 4*10(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


        
            lw t4, 4*11(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


                ld a6, 4*12(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*12(sp)
        sw a6, (4*12+4)(sp)

            lw t4, 4*12(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


        
            lw t4, 4*13(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


                ld a6, 4*14(a1)
        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*14(sp)
        sw a6, (4*14+4)(sp)

            lw t4, 4*14(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


        
            lw t4, 4*15(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


    j L_message_schedule
L_load_misaligned:
                    ld t5, 4*0(a1)
    ld t6, (4*0+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*0(sp)
        sw a6, (4*0+4)(sp)

            lw t4, 4*0(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


        
            lw t4, 4*1(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


                    ld t5, 4*2(a1)
    ld t6, (4*2+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*2(sp)
        sw a6, (4*2+4)(sp)

            lw t4, 4*2(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


        
            lw t4, 4*3(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


                    ld t5, 4*4(a1)
    ld t6, (4*4+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*4(sp)
        sw a6, (4*4+4)(sp)

            lw t4, 4*4(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


        
            lw t4, 4*5(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


                    ld t5, 4*6(a1)
    ld t6, (4*6+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*6(sp)
        sw a6, (4*6+4)(sp)

            lw t4, 4*6(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


        
            lw t4, 4*7(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


                    ld t5, 4*8(a1)
    ld t6, (4*8+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*8(sp)
        sw a6, (4*8+4)(sp)

            lw t4, 4*8(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


        
            lw t4, 4*9(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


                    ld t5, 4*10(a1)
    ld t6, (4*10+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*10(sp)
        sw a6, (4*10+4)(sp)

            lw t4, 4*10(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


        
            lw t4, 4*11(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


                    ld t5, 4*12(a1)
    ld t6, (4*12+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*12(sp)
        sw a6, (4*12+4)(sp)

            lw t4, 4*12(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


        
            lw t4, 4*13(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


                    ld t5, 4*14(a1)
    ld t6, (4*14+8)(a1)
    srl a6, t5, a3
    sll t6, t6, a4
    or a6, a6, t6

        .word 1804097555 # rev8 a6, a6
        srli a5, a6, 32
        sw a5, 4*14(sp)
        sw a6, (4*14+4)(sp)

            lw t4, 4*14(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


        
            lw t4, 4*15(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


L_message_schedule:
    # Round 16's W[i-16] is W[0]; later rounds reuse the previous W[i-15]
    lw s0, 0(sp)
            lw t3, ((16-7)&0x0f)*4(sp)
    lw a7, ((16-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(16&0x0f)(sp)

            lw t4, 4*16(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


            lw t3, ((17-7)&0x0f)*4(sp)
    lw s0, ((17-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(17&0x0f)(sp)

            lw t4, 4*17(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


            lw t3, ((18-7)&0x0f)*4(sp)
    lw a7, ((18-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(18&0x0f)(sp)

            lw t4, 4*18(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


            lw t3, ((19-7)&0x0f)*4(sp)
    lw s0, ((19-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(19&0x0f)(sp)

            lw t4, 4*19(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


            lw t3, ((20-7)&0x0f)*4(sp)
    lw a7, ((20-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(20&0x0f)(sp)

            lw t4, 4*20(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


            lw t3, ((21-7)&0x0f)*4(sp)
    lw s0, ((21-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(21&0x0f)(sp)

            lw t4, 4*21(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


            lw t3, ((22-7)&0x0f)*4(sp)
    lw a7, ((22-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(22&0x0f)(sp)

            lw t4, 4*22(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


            lw t3, ((23-7)&0x0f)*4(sp)
    lw s0, ((23-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(23&0x0f)(sp)

            lw t4, 4*23(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


            lw t3, ((24-7)&0x0f)*4(sp)
    lw a7, ((24-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(24&0x0f)(sp)

            lw t4, 4*24(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


            lw t3, ((25-7)&0x0f)*4(sp)
    lw s0, ((25-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(25&0x0f)(sp)

            lw t4, 4*25(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


            lw t3, ((26-7)&0x0f)*4(sp)
    lw a7, ((26-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(26&0x0f)(sp)

            lw t4, 4*26(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


            lw t3, ((27-7)&0x0f)*4(sp)
    lw s0, ((27-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(27&0x0f)(sp)

            lw t4, 4*27(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


            lw t3, ((28-7)&0x0f)*4(sp)
    lw a7, ((28-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(28&0x0f)(sp)

            lw t4, 4*28(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


            lw t3, ((29-7)&0x0f)*4(sp)
    lw s0, ((29-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(29&0x0f)(sp)

            lw t4, 4*29(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


            lw t3, ((30-7)&0x0f)*4(sp)
    lw a7, ((30-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(30&0x0f)(sp)

            lw t4, 4*30(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


            lw t3, ((31-7)&0x0f)*4(sp)
    lw s0, ((31-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(31&0x0f)(sp)

            lw t4, 4*31(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


            lw t3, ((32-7)&0x0f)*4(sp)
    lw a7, ((32-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(32&0x0f)(sp)

            lw t4, 4*32(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


            lw t3, ((33-7)&0x0f)*4(sp)
    lw s0, ((33-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(33&0x0f)(sp)

            lw t4, 4*33(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


            lw t3, ((34-7)&0x0f)*4(sp)
    lw a7, ((34-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(34&0x0f)(sp)

            lw t4, 4*34(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


            lw t3, ((35-7)&0x0f)*4(sp)
    lw s0, ((35-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(35&0x0f)(sp)

            lw t4, 4*35(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


            lw t3, ((36-7)&0x0f)*4(sp)
    lw a7, ((36-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(36&0x0f)(sp)

            lw t4, 4*36(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


            lw t3, ((37-7)&0x0f)*4(sp)
    lw s0, ((37-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(37&0x0f)(sp)

            lw t4, 4*37(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


            lw t3, ((38-7)&0x0f)*4(sp)
    lw a7, ((38-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(38&0x0f)(sp)

            lw t4, 4*38(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


            lw t3, ((39-7)&0x0f)*4(sp)
    lw s0, ((39-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(39&0x0f)(sp)

            lw t4, 4*39(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


            lw t3, ((40-7)&0x0f)*4(sp)
    lw a7, ((40-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(40&0x0f)(sp)

            lw t4, 4*40(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


            lw t3, ((41-7)&0x0f)*4(sp)
    lw s0, ((41-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(41&0x0f)(sp)

            lw t4, 4*41(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


            lw t3, ((42-7)&0x0f)*4(sp)
    lw a7, ((42-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(42&0x0f)(sp)

            lw t4, 4*42(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


            lw t3, ((43-7)&0x0f)*4(sp)
    lw s0, ((43-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(43&0x0f)(sp)

            lw t4, 4*43(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


            lw t3, ((44-7)&0x0f)*4(sp)
    lw a7, ((44-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(44&0x0f)(sp)

            lw t4, 4*44(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


            lw t3, ((45-7)&0x0f)*4(sp)
    lw s0, ((45-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(45&0x0f)(sp)

            lw t4, 4*45(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


            lw t3, ((46-7)&0x0f)*4(sp)
    lw a7, ((46-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(46&0x0f)(sp)

            lw t4, 4*46(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


            lw t3, ((47-7)&0x0f)*4(sp)
    lw s0, ((47-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(47&0x0f)(sp)

            lw t4, 4*47(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


            lw t3, ((48-7)&0x0f)*4(sp)
    lw a7, ((48-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(48&0x0f)(sp)

            lw t4, 4*48(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


            lw t3, ((49-7)&0x0f)*4(sp)
    lw s0, ((49-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(49&0x0f)(sp)

            lw t4, 4*49(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


            lw t3, ((50-7)&0x0f)*4(sp)
    lw a7, ((50-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(50&0x0f)(sp)

            lw t4, 4*50(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


            lw t3, ((51-7)&0x0f)*4(sp)
    lw s0, ((51-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(51&0x0f)(sp)

            lw t4, 4*51(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


            lw t3, ((52-7)&0x0f)*4(sp)
    lw a7, ((52-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(52&0x0f)(sp)

            lw t4, 4*52(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


            lw t3, ((53-7)&0x0f)*4(sp)
    lw s0, ((53-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(53&0x0f)(sp)

            lw t4, 4*53(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


            lw t3, ((54-7)&0x0f)*4(sp)
    lw a7, ((54-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(54&0x0f)(sp)

            lw t4, 4*54(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


            lw t3, ((55-7)&0x0f)*4(sp)
    lw s0, ((55-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(55&0x0f)(sp)

            lw t4, 4*55(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

    addw s6, s6, t1
    addw s2, t2, t1


            lw t3, ((56-7)&0x0f)*4(sp)
    lw a7, ((56-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(56&0x0f)(sp)

            lw t4, 4*56(t0)
        .word 1617646491
        .word 1622892059
        .word 1637572379
    addw s9, s9, a5
    xor t1, s7, s8
    addw s9, s9, t4
    xor t2, t2, t3
    and t1, t1, s6
    xor t2, t2, t5
    xor t1, t1, s8
    addw t2, t2, s9
    addw t1, t1, t2

        # Sum0
        .word 1613321115
        .word 1624858139
        .word 1634295451
    # Maj
    xor s1, s2, s3
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s3
    # T2
    addw t2, t2, t5

    addw s5, s5, t1
    addw s9, t2, t1


            lw t3, ((57-7)&0x0f)*4(sp)
    lw s0, ((57-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(57&0x0f)(sp)

            lw t4, 4*57(t0)
        .word 1617613723
        .word 1622859291
        .word 1637539611
    addw s8, s8, a6
    xor t1, s6, s7
    addw s8, s8, t4
    xor t2, t2, t3
    and t1, t1, s5
    xor t2, t2, t5
    xor t1, t1, s7
    addw t2, t2, s8
    addw t1, t1, t2

        # Sum0
        .word 1613550491
        .word 1625087515
        .word 1634524827
    # Maj
    xor s10, s9, s2
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s2
    # T2
    addw t2, t2, t5

    addw s4, s4, t1
    addw s8, t2, t1


            lw t3, ((58-7)&0x0f)*4(sp)
    lw a7, ((58-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(58&0x0f)(sp)

            lw t4, 4*58(t0)
        .word 1617580955
        .word 1622826523
        .word 1637506843
    addw s7, s7, a5
    xor t1, s5, s6
    addw s7, s7, t4
    xor t2, t2, t3
    and t1, t1, s4
    xor t2, t2, t5
    xor t1, t1, s6
    addw t2, t2, s7
    addw t1, t1, t2

        # Sum0
        .word 1613517723
        .word 1625054747
        .word 1634492059
    # Maj
    xor s1, s8, s9
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s9
    # T2
    addw t2, t2, t5

    addw s3, s3, t1
    addw s7, t2, t1


            lw t3, ((59-7)&0x0f)*4(sp)
    lw s0, ((59-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(59&0x0f)(sp)

            lw t4, 4*59(t0)
        .word 1617548187
        .word 1622793755
        .word 1637474075
    addw s6, s6, a6
    xor t1, s4, s5
    addw s6, s6, t4
    xor t2, t2, t3
    and t1, t1, s3
    xor t2, t2, t5
    xor t1, t1, s5
    addw t2, t2, s6
    addw t1, t1, t2

        # Sum0
        .word 1613484955
        .word 1625021979
        .word 1634459291
    # Maj
    xor s10, s7, s8
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s8
    # T2
    addw t2, t2, t5

    addw s2, s2, t1
    addw s6, t2, t1


            lw t3, ((60-7)&0x0f)*4(sp)
    lw a7, ((60-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(60&0x0f)(sp)

            lw t4, 4*60(t0)
        .word 1617515419
        .word 1622760987
        .word 1637441307
    addw s5, s5, a5
    xor t1, s3, s4
    addw s5, s5, t4
    xor t2, t2, t3
    and t1, t1, s2
    xor t2, t2, t5
    xor t1, t1, s4
    addw t2, t2, s5
    addw t1, t1, t2

        # Sum0
        .word 1613452187
        .word 1624989211
        .word 1634426523
    # Maj
    xor s1, s6, s7
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s7
    # T2
    addw t2, t2, t5

    addw s9, s9, t1
    addw s5, t2, t1


            lw t3, ((61-7)&0x0f)*4(sp)
    lw s0, ((61-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(61&0x0f)(sp)

            lw t4, 4*61(t0)
        .word 1617744795
        .word 1622990363
        .word 1637670683
    addw s4, s4, a6
    xor t1, s2, s3
    addw s4, s4, t4
    xor t2, t2, t3
    and t1, t1, s9
    xor t2, t2, t5
    xor t1, t1, s3
    addw t2, t2, s4
    addw t1, t1, t2

        # Sum0
        .word 1613419419
        .word 1624956443
        .word 1634393755
    # Maj
    xor s10, s5, s6
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s6
    # T2
    addw t2, t2, t5

    addw s8, s8, t1
    addw s4, t2, t1


            lw t3, ((62-7)&0x0f)*4(sp)
    lw a7, ((62-15)&0x0f)*4(sp)
        .word 1628954395
        .word 1631051675
    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618534171
        .word 1630068635
    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(62&0x0f)(sp)

            lw t4, 4*62(t0)
        .word 1617712027
        .word 1622957595
        .word 1637637915
    addw s3, s3, a5
    xor t1, s9, s2
    addw s3, s3, t4
    xor t2, t2, t3
    and t1, t1, s8
    xor t2, t2, t5
    xor t1, t1, s2
    addw t2, t2, s3
    addw t1, t1, t2

        # Sum0
        .word 1613386651
        .word 1624923675
        .word 1634360987
    # Maj
    xor s1, s4, s5
    xor t2, t2, t3
    and t5, s1, s10
    xor t2, t2, t4
    xor t5, t5, s5
    # T2
    addw t2, t2, t5

    addw s7, s7, t1
    addw s3, t2, t1


            lw t3, ((63-7)&0x0f)*4(sp)
    lw s0, ((63-15)&0x0f)*4(sp)
        .word 1628987163
        .word 1631084443
    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
        .word 1618239259
        .word 1629773723
    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(63&0x0f)(sp)

            lw t4, 4*63(t0)
        .word 1617679259
        .word 1622924827
        .word 1637605147
    addw s2, s2, a6
    xor t1, s8, s9
    addw s2, s2, t4
    xor t2, t2, t3
    and t1, t1, s7
    xor t2, t2, t5
    xor t1, t1, s9
    addw t2, t2, s2
    addw t1, t1, t2

        # Sum0
        .word 1613353883
        .word 1624890907
        .word 1634328219
    # Maj
    xor s10, s3, s4
    xor t2, t2, t3
    and t5, s10, s1
    xor t2, t2, t4
    xor t5, t5, s4
    # T2
    addw t2, t2, t5

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
