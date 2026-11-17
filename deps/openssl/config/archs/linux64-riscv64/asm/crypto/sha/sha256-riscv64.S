.text
.p2align 3
.globl sha256_block_data_order_riscv64
.type   sha256_block_data_order_riscv64,@function
sha256_block_data_order_riscv64:

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

    # b ^ c for round 0's Maj; later rounds reuse the previous a ^ b
    xor s10, s3, s4
                lbu t1, (4*0+0)(a1)
        lbu t2, (4*0+1)(a1)
        lbu t3, (4*0+2)(a1)
        lbu t4, (4*0+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*0(sp)

            lw t4, 4*0(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*1+0)(a1)
        lbu t2, (4*1+1)(a1)
        lbu t3, (4*1+2)(a1)
        lbu t4, (4*1+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*1(sp)

            lw t4, 4*1(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*2+0)(a1)
        lbu t2, (4*2+1)(a1)
        lbu t3, (4*2+2)(a1)
        lbu t4, (4*2+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*2(sp)

            lw t4, 4*2(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*3+0)(a1)
        lbu t2, (4*3+1)(a1)
        lbu t3, (4*3+2)(a1)
        lbu t4, (4*3+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*3(sp)

            lw t4, 4*3(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*4+0)(a1)
        lbu t2, (4*4+1)(a1)
        lbu t3, (4*4+2)(a1)
        lbu t4, (4*4+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*4(sp)

            lw t4, 4*4(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*5+0)(a1)
        lbu t2, (4*5+1)(a1)
        lbu t3, (4*5+2)(a1)
        lbu t4, (4*5+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*5(sp)

            lw t4, 4*5(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*6+0)(a1)
        lbu t2, (4*6+1)(a1)
        lbu t3, (4*6+2)(a1)
        lbu t4, (4*6+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*6(sp)

            lw t4, 4*6(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*7+0)(a1)
        lbu t2, (4*7+1)(a1)
        lbu t3, (4*7+2)(a1)
        lbu t4, (4*7+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*7(sp)

            lw t4, 4*7(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*8+0)(a1)
        lbu t2, (4*8+1)(a1)
        lbu t3, (4*8+2)(a1)
        lbu t4, (4*8+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*8(sp)

            lw t4, 4*8(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*9+0)(a1)
        lbu t2, (4*9+1)(a1)
        lbu t3, (4*9+2)(a1)
        lbu t4, (4*9+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*9(sp)

            lw t4, 4*9(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*10+0)(a1)
        lbu t2, (4*10+1)(a1)
        lbu t3, (4*10+2)(a1)
        lbu t4, (4*10+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*10(sp)

            lw t4, 4*10(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*11+0)(a1)
        lbu t2, (4*11+1)(a1)
        lbu t3, (4*11+2)(a1)
        lbu t4, (4*11+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*11(sp)

            lw t4, 4*11(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*12+0)(a1)
        lbu t2, (4*12+1)(a1)
        lbu t3, (4*12+2)(a1)
        lbu t4, (4*12+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*12(sp)

            lw t4, 4*12(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*13+0)(a1)
        lbu t2, (4*13+1)(a1)
        lbu t3, (4*13+2)(a1)
        lbu t4, (4*13+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*13(sp)

            lw t4, 4*13(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*14+0)(a1)
        lbu t2, (4*14+1)(a1)
        lbu t3, (4*14+2)(a1)
        lbu t4, (4*14+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a5, t1, t4
        sw a5, 4*14(sp)

            lw t4, 4*14(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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


                lbu t1, (4*15+0)(a1)
        lbu t2, (4*15+1)(a1)
        lbu t3, (4*15+2)(a1)
        lbu t4, (4*15+3)(a1)
        slliw t1, t1, 24
        slliw t2, t2, 16
        or t1, t1, t2
        slliw t3, t3, 8
        or t1, t1, t3
        or a6, t1, t4
        sw a6, 4*15(sp)

            lw t4, 4*15(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(16&0x0f)(sp)

            lw t4, 4*16(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(17&0x0f)(sp)

            lw t4, 4*17(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(18&0x0f)(sp)

            lw t4, 4*18(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(19&0x0f)(sp)

            lw t4, 4*19(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(20&0x0f)(sp)

            lw t4, 4*20(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(21&0x0f)(sp)

            lw t4, 4*21(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(22&0x0f)(sp)

            lw t4, 4*22(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(23&0x0f)(sp)

            lw t4, 4*23(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(24&0x0f)(sp)

            lw t4, 4*24(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(25&0x0f)(sp)

            lw t4, 4*25(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(26&0x0f)(sp)

            lw t4, 4*26(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(27&0x0f)(sp)

            lw t4, 4*27(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(28&0x0f)(sp)

            lw t4, 4*28(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(29&0x0f)(sp)

            lw t4, 4*29(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(30&0x0f)(sp)

            lw t4, 4*30(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(31&0x0f)(sp)

            lw t4, 4*31(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(32&0x0f)(sp)

            lw t4, 4*32(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(33&0x0f)(sp)

            lw t4, 4*33(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(34&0x0f)(sp)

            lw t4, 4*34(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(35&0x0f)(sp)

            lw t4, 4*35(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(36&0x0f)(sp)

            lw t4, 4*36(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(37&0x0f)(sp)

            lw t4, 4*37(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(38&0x0f)(sp)

            lw t4, 4*38(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(39&0x0f)(sp)

            lw t4, 4*39(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(40&0x0f)(sp)

            lw t4, 4*40(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(41&0x0f)(sp)

            lw t4, 4*41(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(42&0x0f)(sp)

            lw t4, 4*42(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(43&0x0f)(sp)

            lw t4, 4*43(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(44&0x0f)(sp)

            lw t4, 4*44(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(45&0x0f)(sp)

            lw t4, 4*45(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(46&0x0f)(sp)

            lw t4, 4*46(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(47&0x0f)(sp)

            lw t4, 4*47(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(48&0x0f)(sp)

            lw t4, 4*48(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(49&0x0f)(sp)

            lw t4, 4*49(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(50&0x0f)(sp)

            lw t4, 4*50(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(51&0x0f)(sp)

            lw t4, 4*51(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(52&0x0f)(sp)

            lw t4, 4*52(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(53&0x0f)(sp)

            lw t4, 4*53(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(54&0x0f)(sp)

            lw t4, 4*54(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(55&0x0f)(sp)

            lw t4, 4*55(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(56&0x0f)(sp)

            lw t4, 4*56(t0)
            srliw a3, s6, 6
    slliw a4, s6, (32-6)
    or t2, a3, a4

            srliw a3, s6, 11
    slliw a4, s6, (32-11)
    or t3, a3, a4

            srliw a3, s6, 25
    slliw a4, s6, (32-25)
    or t5, a3, a4

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
            srliw a3, s2, 2
    slliw a4, s2, (32-2)
    or t2, a3, a4

            srliw a3, s2, 13
    slliw a4, s2, (32-13)
    or t3, a3, a4

            srliw a3, s2, 22
    slliw a4, s2, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(57&0x0f)(sp)

            lw t4, 4*57(t0)
            srliw a3, s5, 6
    slliw a4, s5, (32-6)
    or t2, a3, a4

            srliw a3, s5, 11
    slliw a4, s5, (32-11)
    or t3, a3, a4

            srliw a3, s5, 25
    slliw a4, s5, (32-25)
    or t5, a3, a4

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
            srliw a3, s9, 2
    slliw a4, s9, (32-2)
    or t2, a3, a4

            srliw a3, s9, 13
    slliw a4, s9, (32-13)
    or t3, a3, a4

            srliw a3, s9, 22
    slliw a4, s9, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(58&0x0f)(sp)

            lw t4, 4*58(t0)
            srliw a3, s4, 6
    slliw a4, s4, (32-6)
    or t2, a3, a4

            srliw a3, s4, 11
    slliw a4, s4, (32-11)
    or t3, a3, a4

            srliw a3, s4, 25
    slliw a4, s4, (32-25)
    or t5, a3, a4

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
            srliw a3, s8, 2
    slliw a4, s8, (32-2)
    or t2, a3, a4

            srliw a3, s8, 13
    slliw a4, s8, (32-13)
    or t3, a3, a4

            srliw a3, s8, 22
    slliw a4, s8, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(59&0x0f)(sp)

            lw t4, 4*59(t0)
            srliw a3, s3, 6
    slliw a4, s3, (32-6)
    or t2, a3, a4

            srliw a3, s3, 11
    slliw a4, s3, (32-11)
    or t3, a3, a4

            srliw a3, s3, 25
    slliw a4, s3, (32-25)
    or t5, a3, a4

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
            srliw a3, s7, 2
    slliw a4, s7, (32-2)
    or t2, a3, a4

            srliw a3, s7, 13
    slliw a4, s7, (32-13)
    or t3, a3, a4

            srliw a3, s7, 22
    slliw a4, s7, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(60&0x0f)(sp)

            lw t4, 4*60(t0)
            srliw a3, s2, 6
    slliw a4, s2, (32-6)
    or t2, a3, a4

            srliw a3, s2, 11
    slliw a4, s2, (32-11)
    or t3, a3, a4

            srliw a3, s2, 25
    slliw a4, s2, (32-25)
    or t5, a3, a4

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
            srliw a3, s6, 2
    slliw a4, s6, (32-2)
    or t2, a3, a4

            srliw a3, s6, 13
    slliw a4, s6, (32-13)
    or t3, a3, a4

            srliw a3, s6, 22
    slliw a4, s6, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(61&0x0f)(sp)

            lw t4, 4*61(t0)
            srliw a3, s9, 6
    slliw a4, s9, (32-6)
    or t2, a3, a4

            srliw a3, s9, 11
    slliw a4, s9, (32-11)
    or t3, a3, a4

            srliw a3, s9, 25
    slliw a4, s9, (32-25)
    or t5, a3, a4

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
            srliw a3, s5, 2
    slliw a4, s5, (32-2)
    or t2, a3, a4

            srliw a3, s5, 13
    slliw a4, s5, (32-13)
    or t3, a3, a4

            srliw a3, s5, 22
    slliw a4, s5, (32-22)
    or t4, a3, a4

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
            srliw a3, a5, 17
    slliw a4, a5, (32-17)
    or t5, a3, a4

            srliw a3, a5, 19
    slliw a4, a5, (32-19)
    or t6, a3, a4

    srliw t1, a5, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, a7, 7
    slliw a4, a7, (32-7)
    or t5, a3, a4

            srliw a3, a7, 18
    slliw a4, a7, (32-18)
    or t6, a3, a4

    srliw t2, a7, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, s0
    addw a5, t1, t2
    sw a5, 4*(62&0x0f)(sp)

            lw t4, 4*62(t0)
            srliw a3, s8, 6
    slliw a4, s8, (32-6)
    or t2, a3, a4

            srliw a3, s8, 11
    slliw a4, s8, (32-11)
    or t3, a3, a4

            srliw a3, s8, 25
    slliw a4, s8, (32-25)
    or t5, a3, a4

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
            srliw a3, s4, 2
    slliw a4, s4, (32-2)
    or t2, a3, a4

            srliw a3, s4, 13
    slliw a4, s4, (32-13)
    or t3, a3, a4

            srliw a3, s4, 22
    slliw a4, s4, (32-22)
    or t4, a3, a4

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
            srliw a3, a6, 17
    slliw a4, a6, (32-17)
    or t5, a3, a4

            srliw a3, a6, 19
    slliw a4, a6, (32-19)
    or t6, a3, a4

    srliw t1, a6, 10
    xor t1, t1, t5
    xor t1, t1, t6
    addw t1, t1, t3
            srliw a3, s0, 7
    slliw a4, s0, (32-7)
    or t5, a3, a4

            srliw a3, s0, 18
    slliw a4, s0, (32-18)
    or t6, a3, a4

    srliw t2, s0, 3
    xor t2, t2, t5
    xor t2, t2, t6
    addw t2, t2, a7
    addw a6, t1, t2
    sw a6, 4*(63&0x0f)(sp)

            lw t4, 4*63(t0)
            srliw a3, s7, 6
    slliw a4, s7, (32-6)
    or t2, a3, a4

            srliw a3, s7, 11
    slliw a4, s7, (32-11)
    or t3, a3, a4

            srliw a3, s7, 25
    slliw a4, s7, (32-25)
    or t5, a3, a4

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
            srliw a3, s3, 2
    slliw a4, s3, (32-2)
    or t2, a3, a4

            srliw a3, s3, 13
    slliw a4, s3, (32-13)
    or t3, a3, a4

            srliw a3, s3, 22
    slliw a4, s3, (32-22)
    or t4, a3, a4

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
.size sha256_block_data_order_riscv64,.-sha256_block_data_order_riscv64

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
