.text
.balign 32
.globl bn_mul_mont
.type   bn_mul_mont,@function
bn_mul_mont:
    addi    sp,sp,-96
    sd      x8,88(sp)
    sd      x18,80(sp)
    sd      x19,72(sp)
    sd      x20,64(sp)
    sd      x21,56(sp)
    sd      x22,48(sp)
    sd      x23,40(sp)
    sd      x24,32(sp)
    sd      x25,24(sp)
    sd      x26,16(sp)
    sd      x27,8(sp)
    sd      x9,0(sp)
    mv x8, sp
    ld x28, 0(x12)    # bp[0]
    addi x12, x12,8
    ld x6, 0(x11)    # ap[0]
    ld x7, 8(x11)    # ap[1]
    addi x11, x11, 16
    ld x14, 0(x14)    # n0,precomputed modular inverse
    ld x18, 0(x13)    # np[0]
    ld x19, 8(x13)    # np[1]
    addi x13, x13, 16

    slli x15, x15, 3
    sub x26, sp, x15
    andi x26, x26, -16    # address alignment
    mv sp, x26    # alloca

    addi x25, x15, -16    # x25=(num-2)*8

    mul x5, x6, x28    # ap[0]*bp[0]
    mulhu x6, x6, x28
    mul x29, x7, x28    # ap[1]*bp[0]
    mulhu x30, x7, x28

    mul x20, x5, x14    # montgomery reduction coefficient tp[0]*n0
    # montgomery optimization: np[0]*m1 ensures (np[0]*m1+lo0) has zero lower bits
    # only carry status needed, not full lo1 result
    # eliminates mul/adds instructions → Saves cycles & power
    # mul x31, x18, x20		// np[0]*m1
    # adds x31, x31, x5   // discarded
    mulhu x18, x18, x20
    snez x16, x5
    add x18, x18, x16
    mul x21, x19, x20    # np[1]*m1
    mulhu x22, x19, x20
    beqz x25, .L1st_last_entry

.L1st:
    ld x7, 0(x11)
    addi x11, x11, 8

    # compute the sum of partial products of a and b
    add x5, x29, x6    # {ap[j-1]*bp[0],low}+{ap[j-2]*bp[0],high}, j ranges from 2 to num-1
    sltu x16, x5, x29
    add x6, x30, x16    # {ap[j-1]*bp[0],high}+C_lo0, j ranges from 2 to num-1

    addi x25, x25, -8    # x25--, x25 ranges from (num-2)*8 to 0
    ld x19, 0(x13)
    addi x13, x13, 8

    # compute the sum of reduction term
    add x31, x21, x18    # {np[j-1]*m1,low}+{np[j-2]*m1,high}, j ranges from 2 to num-1
    sltu x16, x31, x21
    add x18, x22, x16    # {np[j-1]*m1,high}+C_lo1, j ranges from 2 to num-1

    # partial product + reduction term
    add x9, x31, x5
    sltu x16, x9, x31
    mv x31, x9
    add x18, x18, x16

    sd x31, 0(x26)    # tp[j-2], j ranges from 2 to num-1
    addi x26, x26, 8

    mul x29, x7, x28    # ap[j]*bp[0], j ranges from 2 to num-1
    mulhu x30, x7, x28
    mul x21, x19, x20    # np[j]*m1, j ranges from 2 to num-1
    mulhu x22, x19, x20
    bnez x25, .L1st

.L1st_last_entry:
    # last partial product
    add x5, x29, x6    # {ap[j]*bp[0],low}+{ap[j-1]*bp[0],high}, j is num-1
    sltu x16, x5, x29
    add x6, x30, x16    # {ap[j]*bp[0],high}+C_lo0, j is num-1

    sub x11, x11, x15    # rewind x11
    sub x13, x13, x15    # rewind x13

    # last reduction term
    add x31, x21, x18    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu x16, x31, x21
    add x18, x22, x16    # {np[j]*m1,high}+C_lo1, j is num-1

    # last partial product + last reduction term
    add x31, x31, x5
    sltu x16, x31, x5

    add x9, x18, x6
    sltu x17, x9, x18
    add x18, x9, x16
    sltu x23, x18, x9
    or x16, x17, x23    # carry2 and ovf are mutually exclusive, both cannot be 1 simultaneously
    mv x23, x16    # upmost overflow bit

    addi x24, x15, -8    # x24=(num-1)*8

    sd x31, 0(x26) # tp[j-1], j is num-1
    sd x18, 8(x26) # tp[j], j is num-1

.Louter:
    ld x28, 0(x12)    # bp[i], i ranges from 1 to num-1
    addi x12, x12, 8
    ld x6, 0(x11)
    ld x7, 8(x11)
    addi x11, x11, 16
    ld x27, 0(sp)    # tp[0]
    addi x26, sp, 8    # tp[1]

    mul x5, x6, x28    # ap[0]*bp[i], i ranges from 1 to num-1
    mulhu x6, x6, x28

    addi x25, x15,-16    # x25=(num-2)*8
    ld x18, 0(x13)
    ld x19, 8(x13)
    addi x13, x13, 16

    mul x29, x7, x28    # ap[1]*bp[i], i ranges from 1 to num-1
    mulhu x30, x7, x28

    add x5, x5, x27    # ap[0]*bp[i] + last_tp[0] , i ranges from 1 to num-1
    sltu x16, x5, x27
    add x6, x6, x16    # x6 will not overflow

    # compute the modular reduction coefficient
    mul x20, x5, x14

    addi x24, x24, -8    # x24--, x24 ranges from (num-1)*8 to 0

    # mul x31, x18, x20	 # discarded
    # adds	x31, x31, x5   # discarded
    mulhu x18, x18, x20
    snez x16, x5
    mul x21, x19, x20    # np[1]*m1
    mulhu x22, x19, x20

    beqz x25, .Linner_last_entry

.Linner:
    ld x7, 0(x11)
    addi x11, x11, 8
    add x18, x18, x16

    ld x27, 0(x26)    # tp[j-1], j is 2 to num-1
    addi x26, x26, 8

    # compute the sum of partial products of a and b
    add x5, x29, x6    # {ap[j-1]*bp[i],low}+{ap[j-2]*bp[i],high}, j ranges from 2 to num-1, i ranges from 1 to num-1
    sltu x16, x5, x29
    add x6, x30, x16    # {ap[j-1]*bp[i],high}+C_lo0, j ranges from 2 to num-1, i ranges from 1 to num-1

    addi x25, x25, -8    # x25--, x25 ranges from (num-2)*8 to 0

    # compute the sum of reduction term
    add x31, x21, x18    # {np[j-1]*m1,low}+{np[j-2]*m1,high}, j ranges from 2 to num-1
    sltu x16, x31, x21
    add x18, x22, x16    # {np[j-1]*m1}+C_lo1, j ranges from 2 to num-1

    ld x19, 0(x13)
    addi x13, x13, 8

    # partial product + reduction term
    add x5, x5, x27
    sltu x16, x5, x27
    add x6, x6, x16

    add x31, x31, x5
    sltu x16, x31, x5

    sd x31, -16(x26)    # tp[j-2], j ranges from 2 to num-1

    mul x29, x7, x28    # ap[j]*bp[i], j ranges from 2 to num-1, i ranges from 1 to num-1
    mulhu x30, x7, x28
    mul x21, x19, x20    # np[j]*m1, j ranges from 2 to num-1
    mulhu x22, x19, x20

    bnez x25, .Linner

.Linner_last_entry:
    ld x27, 0(x26)    # tp[j], j is num-1
    addi x26, x26, 8
    add x18, x18, x16

    # last partial product
    add x5, x29, x6    # {ap[j]*bp[i],low}+{ap[j-1]*bp[i],high}, j is num-1, i ranges from 1 to num-1
    sltu x16, x5, x29
    add x6, x30, x16    # {ap[j]*bp[i],high}+C_lo0, j is num-1, i ranges from 1 to num-1

    sub x11, x11, x15    # rewind x11
    sub	x13, x13, x15    # rewind x13

    # last reduction term
    add x31, x21, x18    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu x16, x31, x21
    add x9, x22, x23
    sltu x17, x9, x22
    add x18, x9, x16    # {np[j]*m1,high}+C_lo1, j is num-1
    sltu x23, x18, x9
    or x16, x17, x23
    mv x23, x16    # update the upmost overflow bit

    # last partial product + last reduction term
    add x5, x5, x27
    sltu x16, x5, x27
    add x6, x6, x16

    add x31, x31, x5
    sltu x16, x31, x5
    add x9, x18, x6
    sltu x17, x9, x18
    add x18, x9, x16
    sltu x16, x18, x9
    or x16, x17, x16

    add x23, x23, x16    # upmost overflow bit

    sd x31, -16(x26)    # tp[j-1], j is num-1
    sd x18, -8(x26)    # tp[j], j is num-1
    bnez x24, .Louter

    ld x27, 0(sp)    # tp[0]
    addi x26, sp, 8
    ld x19, 0(x13)    # np[0]
    addi x13, x13, 8
    addi x25, x15, -8    # x25=(num-1)*8 and clear borrow
    sltu x16, x15, 8
    xori x16, x16, 1
    mv x11, x10
.Lsub:
    # tp[j]-np[j], j ranges from 0 to num-2, set carry flag
    xori x16, x16,1
    sub x9, x27, x19
    sltu x17, x27, x9
    sub x7, x9, x16
    sltu x16, x9, x7
    or x16, x17, x16
    xori x16, x16, 1

    ld x27, 0(x26)    # tp[j], j ranges from 1 to num-1
    addi x26, x26, 8
    addi x25, x25, -8    # x25--, x25 ranges from (num-1)*8 to 0
    ld x19, 0(x13)
    addi x13, x13, 8

    sd x7, 0(x11)    # rp[j]=tp[j]-np[j], j ranges from 0 to num-2
    addi x11, x11, 8
    bnez x25, .Lsub

    # process the last word, tp[j]-np[j], j is num-1
    xori x16, x16,1
    sub x9, x27, x19
    sltu x17, x27, x9
    sub x7, x9, x16
    sltu x16, x9, x7
    or x16, x17, x16
    xori x16, x16, 1

    # whether there is a borrow
    xori x16, x16, 1
    sub x9, x23, zero
    sltu x17, x23, x9
    sub x23, x9, x16
    sltu x16, x9, x23
    or x16, x17, x16
    xori x16, x16, 1

    sd x7, 0(x11)    # rp[j], j is num-1
    addi x11, x11, 8

    # conditional result copying and cleanup
    ld x27, 0(sp)    # tp[0]
    addi x26, sp, 8
    ld x7, 0(x10)    # rp[0]
    addi x10, x10, 8
    addi x15, x15, -8    # num--
    nop

.Lcond_copy:
    addi x15,x15, -8    # num--
    # conditionally selects value based on borrow flag:
    # when borrow occurs (borrow flag set): nj = tj (original t value)
    snez x16, x16
    sub x16, zero, x16
    xor x19, x27, x7
    and x19, x19, x16
    xor x19, x27, x19

    ld x27, 0(x26)
    addi x26, x26, 8
    ld x7, 0(x10)
    addi x10, x10, 8
    sd zero, -16(x26)    # wipe tp
    sd x19, -16(x10)    # result
    bnez x15, .Lcond_copy

    # process the last word
    snez x16, x16
    sub x16, zero, x16
    xor x19, x27, x7
    and x19, x19, x16
    xor x19, x27, x19

    sd zero, -8(x26)    # wipe tp
    sd x19, -8(x10)
    mv sp, x8
    li x10, 1
    ld      x8,88(sp)
    ld      x18,80(sp)
    ld      x19,72(sp)
    ld      x20,64(sp)
    ld      x21,56(sp)
    ld      x22,48(sp)
    ld      x23,40(sp)
    ld      x24,32(sp)
    ld      x25,24(sp)
    ld      x26,16(sp)
    ld      x27,8(sp)
    ld      x9,0(sp)
    addi    sp,sp,96
    ret
.size	bn_mul_mont,.-bn_mul_mont
