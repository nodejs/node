.text
.balign 32
.globl bn_mul_mont
.type bn_mul_mont,@function
bn_mul_mont:
    andi  x17, x15, 7
    beqz  x17, bn_sqr8x_mont
.Lmul_mont:
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
    slli x15, x15, 3
    sub x26, sp, x15
    ld x7, 8(x11)    # ap[1]
    addi x11, x11, 16
    ld x14, 0(x14)    # n0,precomputed modular inverse
    andi x26, x26, -16    # address alignment
    ld x18, 0(x13)    # np[0]
    mv sp, x26    # alloca
    ld x19, 8(x13)    # np[1]
    addi x13, x13, 16


    mul x5, x6, x28    # ap[0]*bp[0]
    addi x25, x15, -16    # x25=(num-2)*8
    mulhu x6, x6, x28
    mul x29, x7, x28    # ap[1]*bp[0]
    mulhu x30, x7, x28

    mul x20, x5, x14    # montgomery reduction coefficient tp[0]*n0
    # montgomery optimization: np[0]*m1 ensures (np[0]*m1+lo0) has zero lower bits
    # only carry status needed, not full lo1 result
    # eliminates mul/adds instructions → Saves cycles & power
    # mul x31, x18, x20   // np[0]*m1
    # adds x31, x31, x5   // discarded
    mulhu x18, x18, x20
    snez x16, x5
    mul x21, x19, x20    # np[1]*m1
    add x18, x18, x16
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
    mul x29, x7, x28    # ap[j]*bp[0], j ranges from 2 to num-1
    add x18, x22, x16    # {np[j-1]*m1,high}+C_lo1, j ranges from 2 to num-1
    mulhu x30, x7, x28
    # partial product + reduction term
    add x31, x31, x5
    sltu x16, x31, x5
    mul x21, x19, x20    # np[j]*m1, j ranges from 2 to num-1
    add x18, x18, x16
    mulhu x22, x19, x20
    sd x31, 0(x26)    # tp[j-2], j ranges from 2 to num-1
    addi x26, x26, 8

    bnez x25, .L1st

.L1st_last_entry:
    # last partial product
    add x5, x29, x6    # {ap[j]*bp[0],low}+{ap[j-1]*bp[0],high}, j is num-1
    sltu x16, x5, x29
    sub x11, x11, x15    # rewind x11
    add x6, x30, x16    # {ap[j]*bp[0],high}+C_lo0, j is num-1

    # last reduction term
    add x31, x21, x18    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu x16, x31, x21
    sub x13, x13, x15    # rewind x13
    add x18, x22, x16    # {np[j]*m1,high}+C_lo1, j is num-1

    # last partial product + last reduction term
    add x31, x31, x5
    sltu x16, x31, x5

    add x18, x18, x6
    sltu x17, x18, x6
    add x18, x18, x16
    sltu x23, x18, x16
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
    addi x25, x15,-16    # x25=(num-2)*8
    mulhu x6, x6, x28
    ld x18, 0(x13)
    ld x19, 8(x13)
    addi x13, x13, 16

    mul x29, x7, x28    # ap[1]*bp[i], i ranges from 1 to num-1
    add x5, x5, x27    # ap[0]*bp[i] + last_tp[0] , i ranges from 1 to num-1
    sltu x16, x5, x27
    mulhu x30, x7, x28
    add x6, x6, x16    # x6 will not overflow

    # compute the modular reduction coefficient
    mul x20, x5, x14

    addi x24, x24, -8    # x24--, x24 ranges from (num-1)*8 to 0

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

    mul x29, x7, x28    # ap[j]*bp[i], j ranges from 2 to num-1, i ranges from 1 to num-1
    # partial product + reduction term
    add x5, x5, x27
    sltu x16, x5, x27
    mulhu x30, x7, x28
    add x6, x6, x16

    mul x21, x19, x20    # np[j]*m1, j ranges from 2 to num-1
    add x31, x31, x5
    sltu x16, x31, x5
    mulhu x22, x19, x20
    sd x31, -16(x26)    # tp[j-2], j ranges from 2 to num-1

    bnez x25, .Linner

.Linner_last_entry:
    ld x27, 0(x26)    # tp[j], j is num-1
    addi x26, x26, 8
    add x18, x18, x16

    # last partial product
    add x5, x29, x6    # {ap[j]*bp[i],low}+{ap[j-1]*bp[i],high}, j is num-1, i ranges from 1 to num-1
    sltu x16, x5, x29
    sub x11, x11, x15    # rewind x11
    add x6, x30, x16    # {ap[j]*bp[i],high}+C_lo0, j is num-1, i ranges from 1 to num-1

    # last reduction term
    add x31, x21, x18    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu x16, x31, x21

    sub x13, x13, x15    # rewind x13

    add x18, x22, x23
    sltu x17, x18, x23
    add x18, x18, x16    # {np[j]*m1,high}+C_lo1, j is num-1
    sltu x23, x18, x16
    or x16, x17, x23

    mv x23, x16    # update the upmost overflow bit

    # last partial product + last reduction term
    add x5, x5, x27
    sltu x16, x5, x27
    add x6, x6, x16

    add x31, x31, x5
    sltu x16, x31, x5

    add x18, x18, x6
    sltu x17, x18, x6
    add x18, x18, x16
    sltu x16, x18, x16
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

    li x16,0   # Custom, no borrow, C=0 (normal case, with borrow C=1)
    mv x11, x10
.Lsub:
    # tp[j]-np[j], j ranges from 0 to num-2, set carry flag
    sub x9, x27, x19
    sltu x17, x27, x9
    sub x7, x9, x16
    sltu x16, x9, x7
    or x16, x17, x16

    ld x27, 0(x26)    # tp[j], j ranges from 1 to num-1
    addi x26, x26, 8
    addi x25, x25, -8    # x25--, x25 ranges from (num-1)*8 to 0
    ld x19, 0(x13)
    addi x13, x13, 8

    sd x7, 0(x11)    # rp[j]=tp[j]-np[j], j ranges from 0 to num-2
    addi x11, x11, 8
    bnez x25, .Lsub

    # process the last word, tp[j]-np[j], j is num-1
    sub x9, x27, x19
    sltu x17, x27, x9
    sub x7, x9, x16
    sltu x16, x9, x7
    or x16, x17, x16

    # whether there is a borrow
    sub x9, x23, x16
    sltu x16, x23, x9
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
.size bn_mul_mont,.-bn_mul_mont
.type   bn_sqr8x_mont,%function
.balign 32
bn_sqr8x_mont:
    # If ap != bp, -> normal multiplication, jump to .Lmul_mont
    # If ap == bp, -> continue with optimized sqr8x path
    bne x11,x12, .Lmul_mont
    addi    sp,sp,-168
    sd      s0,88(sp)
    sd      s1,80(sp)
    sd      s2,72(sp)
    sd      s3,64(sp)
    sd      s4,56(sp)
    sd      s5,48(sp)
    sd      s6,40(sp)
    sd      s7,32(sp)
    sd      s8,24(sp)
    sd      s9,16(sp)
    sd      s11,8(sp)
    sd      s10,0(sp)
    mv x8, sp
.Lsqr8x_mont:
    ld x5, 0(x11)    # load a[0-7]
    ld x6, 8(x11)
    ld x7, 16(x11)
    ld x28, 24(x11)
    ld x9, 32(x11)
    ld x18, 40(x11)
    ld x19, 48(x11)
    ld x20, 56(x11)

    slli x16, x15, 4
    sub x12, sp, x16    # alloca sp-num*16
    slli x15, x15, 3
    ld x14, 0(x14)    # n0, precomputed modular inverse
    mv sp, x12    # alloca

    # Save temporary values to stack to release registers for subsequent operations
    addi x17,x15,-64    # number of bytes remaining to be zeroed
        sd x17,136(x8)
    # offload cnt
        sd x10,96(x8)
    # offload rp
        sd x13,104(x8)
    # offload np
        sd x15,112(x8)
    # offload num
        sd x14,120(x8)
    # offload n0
        sd x1,128(x8)
    # offload ra

    j .Lsqr8x_zero_start


    # Clear tp buffer to store partial products
    # Zero 128 bytes per iteration until cache cleared
.Lsqr8x_zero:
    addi x10,x10,-64    # cnt=cnt-64
        sd x10,136(x8)


    sd zero, 0(x12)
    sd zero, 8(x12)
    sd zero, 16(x12)
    sd zero, 24(x12)
    sd zero, 32(x12)
    sd zero, 40(x12)
    sd zero, 48(x12)
    sd zero, 56(x12)

.Lsqr8x_zero_start:
    sd zero, 64(x12)
    sd zero, 72(x12)
    sd zero, 80(x12)
    sd zero, 88(x12)
    sd zero, 96(x12)
    sd zero, 104(x12)
    sd zero, 112(x12)
    sd zero, 120(x12)
    addi x12,x12,128

        ld x10,136(x8)

    bnez x10,.Lsqr8x_zero    # cnt != 0 -> Lsqr8x_zero

        ld x14,112(x8)

    add x14,x11,x14    # ap_end = ap + num
        sd x14,144(x8)
 # offload ap_end, the last element of ap
    addi x11,x11,64    # ap += 64, skip initial 8 64-bit words
    mv x21, zero
    mv x22, zero
    mv x23, zero
    mv x24, zero
    mv x25, zero
    mv x31, zero
    mv x27, zero
    mv x26, zero
    mv x12,sp    # points to buffer start

    # Multiply everything but a[i]*a[i]
.balign 16
.Lsqr8x_outer_loop:
    # Compute cross products: a[j] * a[i] for all < j
    #                                                 a[1]a[0]  (i)
    #                                             a[2]a[0]
    #                                         a[3]a[0]
    #                                     a[4]a[0]
    #                                 a[5]a[0]
    #                             a[6]a[0]
    #                         a[7]a[0]
    #                                         a[2]a[1]  (ii)
    #                                     a[3]a[1]
    #                                 a[4]a[1]
    #                             a[5]a[1]
    #                         a[6]a[1]
    #                     a[7]a[1]
    #                                 a[3]a[2]  (iii)
    #                             a[4]a[2]
    #                         a[5]a[2]
    #                     a[6]a[2]
    #                 a[7]a[2]
    #                         a[4]a[3]  (iv)
    #                     a[5]a[3]
    #                 a[6]a[3]
    #             a[7]a[3]
    #                 a[5]a[4]  (v)
    #             a[6]a[4]
    #         a[7]a[4]
    #         a[6]a[5]  (vi)
    #     a[7]a[5]
    # a[7]a[6]  (vii)
    mul x16,x6,x5    # lo(a[1..7]*a[0])  (i)
    mul x17,x7,x5
    mul x29,x28,x5
    mul x30,x9,x5

        add x22, x22, x16
    sltu x15, x22, x16
    # t[1]+lo(a[1]*a[0])
    mul x16,x18,x5
        add x23, x23, x17
    sltu x10, x23, x17
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15
    # t[2]+lo(a[2]*a[0])
    mul x17,x19,x5
        add x24, x24, x29
    sltu x10, x24, x29
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15
    # t[3]+lo(a[3]*a[0])
    mul x29,x20,x5
        add x25, x25, x30
    sltu x10, x25, x30
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15
    # t[4]+lo(a[4]*a[0])

    mulhu x30,x6,x5    # hi(a[1..7]*a[0])
        add x31, x31, x16
    sltu x10, x31, x16
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15
    # t[5]+lo(a[5]*a[0])
    mulhu x16,x7,x5
        add x27, x27, x17
    sltu x10, x27, x17
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15
    # t[6]+lo(a[6]*a[0])
    mulhu x17,x28,x5
        add x26, x26, x29
    sltu x10, x26, x29
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t[7]+lo(a[7]*a[0])
    mulhu x29,x9,x5
    sd x21, 0(x12)    # offload acc0, store t[0]
    add x21,zero,x15    # t[8]=0+c_pre{t[7]+lo(a[7]*a[0])}
    sd x22, 8(x12)    # offload acc1, store t[1]
    addi x12,x12,16    # tp=tp+16,advance buffer pointer by 16 bytes

        add x23, x23, x30
    sltu x15, x23, x30
    # t[2]+hi(a[1]*a[0])
    mulhu x30,x18,x5
        add x24, x24, x16
    sltu x10, x24, x16
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15
    # t[3]+hi(a[2]*a[0])
    mulhu x16,x19,x5
        add x25, x25, x17
    sltu x10, x25, x17
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15
    # t[4]+hi(a[3]*a[0])
    mulhu x17,x20,x5
        add x31, x31, x29
    sltu x10, x31, x29
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15
    # t[5]+hi(a[4]*a[0])

    mul x29,x7,x6    # lo(a[2..7]*a[1])  (ii)
        add x27, x27, x30
    sltu x10, x27, x30
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15
    # t[6]+hi(a[5]*a[0])
    mul x30,x28,x6
        add x26, x26, x16
    sltu x10, x26, x16
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t[7]+hi(a[6]*a[0])
    mul x16,x9,x6
        add x21,x21,x17
    add x21,x21,x15
    # t[8]+hi(a[7]*a[0])
    mul x17,x18,x6

        add x24, x24, x29
    sltu x15, x24, x29
    # t[3]+ lo(a[2]*a[1])
    mul x29,x19,x6
        add x25, x25, x30
    sltu x10, x25, x30
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15
    # t[4]+ lo(a[3]*a[1])
    mul x30,x20,x6
        add x31, x31, x16
    sltu x10, x31, x16
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15
    # t[5]+ lo(a[4]*a[1])
    mulhu x16,x7,x6    # hi(a[2..7]*a[1])
        add x27, x27, x17
    sltu x10, x27, x17
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15
    # t[6]+ lo(a[5]*a[1])
    mulhu x17,x28,x6
        add x26, x26, x29
    sltu x10, x26, x29
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t[7]+ lo(a[6]*a[1])
    mulhu x29,x9,x6
        add x21, x21, x30
    sltu x10, x21, x30
    add x21, x21, x15
    sltu x15, x21, x15
    or x15, x10, x15
    # t[8]+ lo(a[7]*a[1])
    mulhu x30,x18,x6
    sd x23,0(x12)    # offload acc2, store t[2]
    add x22,zero,x15    # t[9]=0+c_pre{t[8]+ lo(a[7]*a[1])}
    sd x24,8(x12)    # offload acc3, store t[3]
    addi x12,x12,16    # tp=tp+16,advance buffer pointer by 16 bytes

        add x25, x25, x16
    sltu x15, x25, x16
    # t[4]+ hi(a[2]*a[1])
    mulhu x16,x19,x6
        add x31, x31, x17
    sltu x10, x31, x17
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15
    # t[5]+ hi(a[3]*a[1])
    mulhu x17,x20,x6
        add x27, x27, x29
    sltu x10, x27, x29
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15
    # t[6]+ hi(a[4]*a[1])
    mul x29,x28,x7    # lo(a[3..7]*a[2])  (iii)
        add x26, x26, x30
    sltu x10, x26, x30
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t[7]+ hi(a[5]*a[1])
    mul x30,x9,x7
        add x21, x21, x16
    sltu x10, x21, x16
    add x21, x21, x15
    sltu x15, x21, x15
    or x15, x10, x15
    # t[8]+ hi(a[6]*a[1])
    mul x16,x18,x7
        add x22,x22,x17
    add x22,x22,x15
    # t[9]+ hi(a[7]*a[1])
    mul x17,x19,x7
        add x31, x31, x29
    sltu x15, x31, x29
    # t[5]+ lo(a[3]*a[2])
    mul x29,x20,x7
        add x27, x27, x30
    sltu x10, x27, x30
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15
    # t[6]+ lo(a[4]*a[2])
    mulhu x30,x28,x7    # hi(a[3..7]*a[2])
        add x26, x26, x16
    sltu x10, x26, x16
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t[7]+ lo(a[5]*a[2])
    mulhu x16,x9,x7
        add x21, x21, x17
    sltu x10, x21, x17
    add x21, x21, x15
    sltu x15, x21, x15
    or x15, x10, x15
    # t[8]+ lo(a[6]*a[2])
    mulhu x17,x18,x7
        add x22, x22, x29
    sltu x10, x22, x29
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15
    # t[9]+ lo(a[7]*a[2])
    mulhu x29,x19,x7
    sd x25,0(x12)    # offload acc4, store t[4]
    add x23,zero,x15    # t[10]=0+c_pre{t[9]+ lo(a[7]*a[2])}
    sd x31,8(x12)    # offload acc5, store t[5]
    addi x12,x12,16    # tp=tp+16,advance buffer pointer by 16 bytes

        add x27, x27, x30
    sltu x15, x27, x30
    # t[6]+ hi(a[3]*a[2])
    mulhu x30,x20,x7
        add x26, x26, x16
    sltu x10, x26, x16
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t[7]+ hi(a4]*a[2])
    mul x16,x9,x28  # lo(a[4..7]*a[3])  (iv)
        add x21, x21, x17
    sltu x10, x21, x17
    add x21, x21, x15
    sltu x15, x21, x15
    or x15, x10, x15
    # t[8]+ hi(a5]*a[2])
    mul x17,x18,x28
        add x22, x22, x29
    sltu x10, x22, x29
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15
    # t[9]+ hi(a6]*a[2])
    mul x29,x19,x28
        add x23,x23,x30
    add x23,x23,x15
    # t[10]+ hi(a7]*a[2])
    mul x30,x20,x28

        add x26, x26, x16
    sltu x15, x26, x16
    # t[7]+ lo(a[4]*a[3])
    mulhu x16,x9,x28   # hi(a[4..7]*a[3])
        add x21, x21, x17
    sltu x10, x21, x17
    add x21, x21, x15
    sltu x15, x21, x15
    or x15, x10, x15
    # t[8]+ lo(a[5]*a[3])
    mulhu x17,x18,x28
        add x22, x22, x29
    sltu x10, x22, x29
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15
    # t[9]+ lo(a[6]*a[3])
    mulhu x29,x19,x28
        add x23, x23, x30
    sltu x10, x23, x30
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15
    # t[10]+ lo(a[7]*a[3])
    mulhu x30,x20,x28
    sd x27,0(x12)    # offload acc6, store t[6]
    add x24,zero,x15    # t[11]=0+c_pre{t[10]+ lo(a[7]*a[3])}
    sd x26,8(x12)    # offload acc7, store t[7]
    addi x12,x12,16    # tp=tp+16, advance buffer pointer by 16 bytes

        add x21, x21, x16
    sltu x15, x21, x16
    # t[8]+ hi(a[4]*a[3])
    mul x16,x18,x9    # lo(a[5..7]*a[4])  (v)
        add x22, x22, x17
    sltu x10, x22, x17
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15
    # t[9]+ hi(a[5]*a[3])
    mul x17,x19,x9
        add x23, x23, x29
    sltu x10, x23, x29
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15
    # t[10]+ hi(a[6]*a[3])
    mul x29,x20,x9
        add x24,x24,x30
    add x24,x24,x15
    # t[11]+ hi(a[7]*a[3])

    mulhu x30,x18,x9    # hi(a[5..7]*a[4])
        add x22, x22, x16
    sltu x15, x22, x16
    # t[9]+ lo(a[5]*a[4])
    mulhu x16,x19,x9
        add x23, x23, x17
    sltu x10, x23, x17
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15
    # t[10]+ lo(a[6]*a[4])
    mulhu x17,x20,x9
        add x24, x24, x29
    sltu x10, x24, x29
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15
    # t[11]+ lo(a[7]*a[4])
    mul x29,x19,x18    # lo(a[6..7]*a[5])  (vi)
    add x25,zero,x15    # t[12]=0+c_pre{t[11]+ lo(a[7]*a[4])}

        add x23, x23, x30
    sltu x15, x23, x30
    # t[10]+ hi(a[5]*a[4])
    mul x30,x20,x18
        add x24, x24, x16
    sltu x10, x24, x16
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15
    # t[11]+ hi(a[6]*a[4])
    mulhu x16,x19,x18    # hi(a[6..7]*a[5])
        add x25,x25,x17
    add x25,x25,x15
    # t[12]+ hi(a[7]*a[4])
    mulhu x17,x20,x18

        add x24, x24, x29
    sltu x15, x24, x29
    # t[11]+ lo(a[6]*a[5])
    mul x29,x20,x19    # lo(a[7]*a[6])  (vii)
        add x25, x25, x30
    sltu x10, x25, x30
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15
    # t[12]+ lo(a[7]*a[5])
    mulhu x30,x20,x19    # hi(a[7]*a[6])
    add x31,zero,x15    # t[13] =0+c_pre{t[12]+lo(a[7]*a[5])}

        add x25, x25, x16
    sltu x15, x25, x16
    # t[12]+ hi(a[6]*a[5])

        ld x14,144(x8)
 # load ap_end
    sub x14,x14,x11    # cnt = ap_end-ap, done yet?
        sd x14,136(x8)


        add x31,x31,x17
    add x31,x31,x15
    # t[13]+ hi(a[7]*a[5])

        add x31, x31, x29
    sltu x15, x31, x29
     # t[13]+ lo(a[7]*a[6])
        ld x14,112(x8)

        ld x10,144(x8)

    sub x16, x10, x14    # rewinded ap
    add x27,zero,x15    # t[14]=0+c_pre{t[13]+ lo(a[7]*a[6])}
    add x27,x27,x30    # t[14] + hi(a[7]*a[6])

    # Check if we have processed all elements of a:
    # If no remaining elements, jump to next step Lsqr8x_outer_break
    # Otherwise, load previous partial product from temp buffer,
    # add new a product, and continue inner loop .Lsqr8x_mul.
        ld x14,136(x8)
  # load cnt, cnt = ap_end-ap
    beqz x14, .Lsqr8x_outer_break

    mv x14,x5    # a0->temp,reuse register'temp'
    # loads next batch of data from temporary buffer
    ld x5,0(x12)
    ld x6,8(x12)
    ld x7,16(x12)
    ld x28,24(x12)
    ld x9,32(x12)
    ld x18,40(x12)
    ld x19,48(x12)
    ld x20,56(x12)

    # accumulate new data
        add x21, x21, x5
    sltu x15, x21, x5
    # t[8]~t[14]...
    ld x5,0(x11)    # load new data, a[8..15]...
        add x22, x22, x6
    sltu x10, x22, x6
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    ld x6,8(x11)
        add x23, x23, x7
    sltu x10, x23, x7
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    ld x7,16(x11)
        add x24, x24, x28
    sltu x10, x24, x28
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    ld x28,24(x11)
        add x25, x25, x9
    sltu x10, x25, x9
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

    ld x9,32(x11)
        add x31, x31, x18
    sltu x10, x31, x18
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

    ld x18,40(x11)
        add x27, x27, x19
    sltu x10, x27, x19
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

    ld x19,48(x11)

    mv x13,x11    # ap->np,reuse register'np',a[8]
        add x26, zero, x20
    sltu x10, x26, x20
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t[7]...
    ld x20,56(x11)
    add x11,x11,64    # move pointer forward by 64 bits (8 bytes),a[16]

    li x10,-64    # cnt=-64, set loop count
        sd x10,136(x8)


    # Compute cross products: (current 8 elements) x (remaining a)
    # Accumulate results in RAX (accumulator)
    #
    #                                                         a[8]a[0]
    #                                                     a[9]a[0]
    #                                                 a[a]a[0]
    #                                             a[b]a[0]
    #                                         a[c]a[0]
    #                                     a[d]a[0]
    #                                 a[e]a[0]
    #                             a[f]a[0]
    #                                                     a[8]a[1]
    #                         a[f]a[1]........................
    #                                                 a[8]a[2]
    #                     a[f]a[2]........................
    #                                             a[8]a[3]
    #                 a[f]a[3]........................
    #                                         a[8]a[4]
    #             a[f]a[4]........................
    #                                     a[8]a[5]
    #         a[f]a[5]........................
    #                                 a[8]a[6]
    #     a[f]a[6]........................
    #                             a[8]a[7]
    # a[f]a[7]........................
 .Lsqr8x_mul:
    mul x16,x5,x14    # lo(a[i+8..i+15]*a[i]),..,lo(a[i+8..i+15]*a[i+7])
    add x1,zero,x15    # carry bit
    mul x17,x6,x14

        ld x10,136(x8)

    add x10,x10,8    # cnt=cnt+8
        sd x10,136(x8)


    mul x29,x7,x14
    mul x30,x28,x14
        add x21, x21, x16
    sltu x15, x21, x16

    mul x16,x9,x14
        add x22, x22, x17
    sltu x10, x22, x17
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    mul x17,x18,x14
        add x23, x23, x29
    sltu x10, x23, x29
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    mul x29,x19,x14
        add x24, x24, x30
    sltu x10, x24, x30
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    mul x30,x20,x14
        add x25, x25, x16
    sltu x10, x25, x16
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

    mulhu x16,x5,x14    # hi(a[i+8..i+15]*a[i]),..,lo(a[i+8..i+15]*a[i+7])
        add x31, x31, x17
    sltu x10, x31, x17
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

    mulhu x17,x6,x14
        add x27, x27, x29
    sltu x10, x27, x29
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

    mulhu x29,x7,x14
        add x26, x26, x30
    sltu x10, x26, x30
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15

    mulhu x30,x28,x14
    add x1,x1,x15
    sd x21,0(x12)
    addi x12,x12,8    # move pointer forward by 8 bits (1 bytes)

        add x21, x22, x16
    sltu x15, x21, x16

    mulhu x16,x9,x14
        add x22, x23, x17
    sltu x10, x22, x17
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    mulhu x17,x18,x14
        add x23, x24, x29
    sltu x10, x23, x29
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    mulhu x29,x19,x14
        add x24, x25, x30
    sltu x10, x24, x30
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    mulhu x30,x20,x14

        ld x10,136(x8)

    add x10,x13,x10    # carry2 = np + cnt
    ld x14,0(x10)    # a[i+1],..,a[i+7],i=0
        add x25, x31, x16
    sltu x10, x25, x16
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

        add x31, x27, x17
    sltu x10, x31, x17
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

        add x27, x26, x29
    sltu x10, x27, x29
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

        add x26, x1, x30
    sltu x10, x26, x30
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15


    # Process current 8 elements:
    #  - If not finished: continue inner loop, .Lsqr8x_mul
    #  - If finished: check remaining a elements
    #     - If none: break out,jump to .Lsqr8x_break
    #     - Else: load new data and restart inner loop, .Lsqr8x_mul
        ld x10,136(x8)

    bnez x10,.Lsqr8x_mul

        ld x10,144(x8)

    beq x11, x10, .Lsqr8x_break    # done yet?

    # loads next batch of data from temporary buffer
    ld x5,0(x12)
    ld x6,8(x12)
    ld x7,16(x12)
    ld x28,24(x12)
    ld x9,32(x12)
    ld x18,40(x12)
    ld x19,48(x12)
    ld x20,56(x12)

    # accumulate new data
    # load new data, a[16..23]...
        add x21, x21, x5
    sltu x15, x21, x5

    ld x5,0(x11)
    ld x14, -64(x13)    # return to start of current 8 elements, a[0]、a[8]...
        add x22, x22, x6
    sltu x10, x22, x6
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    ld x6,8(x11)
        add x23, x23, x7
    sltu x10, x23, x7
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    ld x7,16(x11)
        add x24, x24, x28
    sltu x10, x24, x28
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    ld x28,24(x11)
        add x25, x25, x9
    sltu x10, x25, x9
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

    ld x9,32(x11)
        add x31, x31, x18
    sltu x10, x31, x18
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

    ld x18,40(x11)
        add x27, x27, x19
    sltu x10, x27, x19
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

    ld x19,48(x11)
    li x10,-64    # set loop count, cnt=-64, 8 times
        sd x10,136(x8)

        add x26, x26, x20
    sltu x10, x26, x20
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15

    ld x20,56(x11)
    add x11,x11,64    # move pointer forward by 64 bytes, a[24]...
    j .Lsqr8x_mul

    # Outer iteration completion:
    #    - Fetch next data chunk for upcoming iteration
    #    - Determine if this is the final iteration
    #    - Update pointer positions and buffer offsets
.balign 16
.Lsqr8x_break:
    ld x5,0(x13)
    ld x6,8(x13)
    add x11,x13,64
    ld x7,16(x13)
    ld x28,24(x13)
        ld x10,144(x8)

    sub x16,x10,x11    # is it last iteration?
    ld x9,32(x13)
    ld x18,40(x13)
    sub x17,x12,x16
    ld x19,48(x13)
    ld x20,56(x13)
    beqz x16, .Lsqr8x_outer_loop

    sd x21,0(x12)
    sd x22,8(x12)
    ld x21,0(x17)
    ld x22,8(x17)
    sd x23,16(x12)
    sd x24,24(x12)
    ld x23,16(x17)
    ld x24,24(x17)
    sd x25,32(x12)
    sd x31,40(x12)
    ld x25,32(x17)
    ld x31,40(x17)
    sd x27,48(x12)
    sd x26,56(x12)
    mv x12,x17
    ld x27,48(x17)
    ld x26,56(x17)

    j .Lsqr8x_outer_loop
    # Squaring algorithm:
    # 1.Reload input
    # 2.Compute diagonal squares, a[n-1]*a[n-1]|...|a[0]*a[0]
    # 3.Cross terms x 2: first via lsl#1, rest via extr chain (128-bit << 1)
    # 4.Merge diagonal + cross terms x 2
.balign 16
.Lsqr8x_outer_break:
    ld x6,0(x16)    # recall that x16 is &a[0], load a[0 1 2 3]
    ld x28,8(x16)
    ld x17,8(sp)    # load previously computed cross product term from buffer
    ld x29,16(sp)
    ld x18,16(x16)
    ld x20,24(x16)

    add x11,x16,32    # a[4]
    ld x30,24(sp)
    sd x21,0(x12)
    ld x16,32(sp)
    sd x22,8(x12)

    mul x21,x6,x6    # lo(a[0]*a[0])
    sd x23,16(x12)
    sd x24,24(x12)
    mulhu x6,x6,x6    # hi(a[0]*a[0])
    sd x25,32(x12)
    sd x31,40(x12)
    mul x7,x28,x28    # lo(a[1]*a[1])
    sd x27,48(x12)
    sd x26,56(x12)
    mv x12,sp
    mulhu x28,x28,x28    # hi(a[1]*a[1])

    slli x10,x17,1    # cross terms x 2
        add x22, x6, x10
    sltu x15, x22, x10
    # (t1 << 1) + hi(a[0]*a[0])
        srli x10, x17, 63
    slli x17, x29, 1
    or   x17, x17, x10
    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2

        ld x10,112(x8)

    addi x10,x10,-32    # set loop count, cnt=num-32
        sd x10,136(x8)


    # Loop processing 4 diagonal elements per iteration:
    #  - Compute a[i]*a[i] for 4 elements
    #  - Accumulate left-shifted cross product terms
.Lsqr4x_shift_n_add:
        add x23, x7, x17
    sltu x10, x23, x17
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15
    # t1+ lo(a[1]*a[1])
        srli x10, x29, 63
    slli x29, x30, 1
    or   x29, x29, x10
    # t2 = (t3 << 1) + high(t2), cross term multiplied by 2

        ld x10,136(x8)

    addi x10,x10,-32    # cnt=cnt-32
        sd x10,136(x8)


        add x24, x28, x29
    sltu x10, x24, x29
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15
    # t2+ hi(a[1]*a[1])
    ld x17,40(x12)
    ld x29,48(x12)
    mul x9,x18,x18    # lo(a[2]*a[2])
    ld x6,0(x11)    # a[4 5]
    ld x28,8(x11)
    addi x11,x11,16
    mulhu x18,x18,x18    # hi(a[2]*a[2])
        srli x10, x30, 63
    slli x30, x16, 1
    or   x30, x30, x10
    # t3 = (t0 << 1) + high(t3), cross term multiplied by 2
    sd x21,0(x12)
    sd x22,8(x12)
    mul x19,x20,x20    # lo(a[3]*a[3])
    mulhu x20,x20,x20    # hi(a[3]*a[3])
        add x25, x9, x30
    sltu x10, x25, x30
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15
    # t3 + lo(a[2]*a[2])
        srli x10, x16, 63
    slli x16, x17, 1
    or   x16, x16, x10
    # t0 = (t1 << 1) + high(t0), cross term multiplied by 2
    sd x23,16(x12)
    sd x24,24(x12)
        add x31, x18, x16
    sltu x10, x31, x16
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15
    # t0 + hi(a[2]*a[2])
    ld x30,56(x12)
    ld x16,64(x12)
        srli x10, x17, 63
    slli x17, x29, 1
    or   x17, x17, x10
    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2
        add x27, x19, x17
    sltu x10, x27, x17
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15
    # t1 + lo(a[3]*a[3])
        srli x10, x29, 63
    slli x29, x30, 1
    or   x29, x29, x10
    # t2 = (t3 << 1) + high(t2), cross term multiplied by 2
        add x26, x20, x29
    sltu x10, x26, x29
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15
    # t2 + hi(a[3]*a[3])
    ld x17,72(x12)
    ld x29,80(x12)
    mul x5,x6,x6    # lo(a[4]*a[4])
    ld x18,0(x11)    # a[6 7]
    ld x20,8(x11)
    addi x11,x11,16    # move forward by two elements
    mulhu x6,x6,x6    # hi(a[4]*a[4])
    sd x25,32(x12)
    sd x31,40(x12)
    mul x7,x28,x28    # lo(a[5]*a[5])
    mulhu x28,x28,x28    # hi(a[5]*a[5])
        srli x10, x30, 63
    slli x30, x16, 1
    or   x30, x30, x10
    # t3 = (t0 << 1) + high(t3), cross term multiplied by 2
    sd x27,48(x12)
    sd x26,56(x12)
    addi x12,x12,64
        add x21, x5, x30
    sltu x10, x21, x30
    add x21, x21, x15
    sltu x15, x21, x15
    or x15, x10, x15
    # t3 + lo(a[4]*a[4])
        srli x10, x16, 63
    slli x16, x17, 1
    or   x16, x16, x10
    # t0 = (t1 << 1) + high(t0), cross term multiplied by 2
        add x22, x6, x16
    sltu x10, x22, x16
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15
    # t0 + hi(a[4]*a[4])
    ld x30,24(x12)
    ld x16,32(x12)
        srli x10, x17, 63
    slli x17, x29, 1
    or   x17, x17, x10
    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2

        ld x10,136(x8)

    bnez x10,.Lsqr4x_shift_n_add    # if cnt!=0, jump to .Lsqr4x_shift_n_add
    # Tail element handling for square computation
        ld x11,104(x8)
    # load np, N
        ld x14,120(x8)
 # load n0, n0`
        add x23, x7, x17
    sltu x10, x23, x17
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15
    # t1 + lo(a[n-3]*a[n-3])
        srli x10, x29, 63
    slli x29, x30, 1
    or   x29, x29, x10
    # t2 = (t3 << 1) + high(t2), cross term multiplied by 2
        add x24, x28, x29
    sltu x10, x24, x29
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15
    # t2 + hi(a[n-3]*a[n-3])
    ld x17,40(x12)
    ld x29,48(x12)
    mul x9,x18,x18    # lo(a[n-2]*a[n-2])
    mulhu x18,x18,x18    # hi(a[n-2]*a[n-2])
    sd x21,0(x12)
    sd x22,8(x12)
    mul x19,x20,x20    # lo(a[n-1]*a[n-1])
    mulhu x20,x20,x20    # hi(a[n-1]*a[n-1])
    sd x23,16(x12)
    sd x24,24(x12)
        srli x10, x30, 63
    slli x30, x16, 1
    or   x30, x30, x10
    # t3 = (t0 << 1) + high(t3), cross term multiplied by 2
        add x25, x9, x30
    sltu x10, x25, x30
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15
    # t3 + lo(a[n-2]*a[n-2])
        srli x10, x16, 63
    slli x16, x17, 1
    or   x16, x16, x10
    # t0 = (t1 << 1) + high(t0), cross term multiplied by 2
    ld x21,0(sp)
    ld x22,8(sp)
        add x31, x18, x16
    sltu x10, x31, x16
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15
    # t0 + hi(a[n-2]*a[n-2])
        srli x10, x17, 63
    slli x17, x29, 1
    or   x17, x17, x10
    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2
    ld x5,0(x11)    # load N[0..7]
    ld x6,8(x11)
        add x27, x19, x17
    sltu x10, x27, x17
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15
    # t1 + lo(a[n-1]*a[n-1])
        srli x10, x29, 63
    slli x29, zero, 1
    or   x29, x29, x10
    # t2 = high(t2)
    ld x7,16(x11)
    ld x28,24(x11)
        add x26,x20,x29
    add x26,x26,x15
     # t2 + hi(a[n-1]*a[n-1])
    ld x9,32(x11)
    ld x18,40(x11)

    # Reduce by 512 bits per iteration
    mul x1,x14,x21    # t[0]*n0, the modular reduction coefficient
    ld x19,48(x11)
    ld x20,56(x11)
        ld x10,112(x8)

    add x10,x11,x10    # np_end=np+num
        sd x10,152(x8)

    ld x23,16(sp)
    ld x24,24(sp)
    sd x25,32(x12)
    sd x31,40(x12)
    ld x25,32(sp)
    ld x31,40(sp)
    sd x27,48(x12)
    sd x26,56(x12)
    ld x27,48(sp)
    ld x26,56(sp)
    add x11,x11,64    # move pointer forward by 64 bytes, 8 elements
    mv x12,sp
    li x10,0
        sd x10,160(x8)
 # initial topmost carry as 0
    li x10,8    # set loop count, cnt=8
        sd x10,136(x8)


    # Montgomery reduction, t = t + m * N / 2^64
    # m = t[i] * n0 mod 2^64
    # single-round reduction process:Lsqr8x_reduction ——>Lsqr8x_tail->Lsqr8x_tail_break
    #                                                 |__Lsqr8x8_post_condition
    # after one round of reduction completes,
    # process the current m[i..i+7]*N+t until
    # all tp in the buffer have been reduced
 .Lsqr8x_reduction:
    # mul x16,x5,x1    # discarded
    mul x17,x6,x1    # lo(n[1-7])*lo(t[0]*n0)

        ld x10,136(x8)

    addi x10,x10,-1    # cnt=cnt-1
        sd x10,136(x8)


    mul x29,x7,x1
    sd x1,0(x12)    # put aside "na0=t[0]*n0" for tail processing
    addi x12,x12,8    # tp=tp+8
    mul x30,x28,x1

    # low64 partial product + reduction term
    snez x15, x21        # only carry status needed, not full lo1 result
    mul x16,x9,x1
        add x21, x22, x17
    sltu x10, x21, x17
    add x21, x21, x15
    sltu x15, x21, x15
    or x15, x10, x15

    mul x17,x18,x1
        add x22, x23, x29
    sltu x10, x22, x29
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    mul x29,x19,x1
        add x23, x24, x30
    sltu x10, x23, x30
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    mul x30,x20,x1
        add x24, x25, x16
    sltu x10, x24, x16
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    mulhu x16,x5,x1    # hi(n[0-7])*lo(t[0]*n0)
        add x25, x31, x17
    sltu x10, x25, x17
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

    mulhu x17,x6,x1
        add x31, x27, x29
    sltu x10, x31, x29
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

    mulhu x29,x7,x1
        add x27, x26, x30
    sltu x10, x27, x30
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

    mulhu x30,x28,x1
    add x26,zero,x15

    # high64 partial product + reduction term
        add x21, x21, x16
    sltu x15, x21, x16

    mulhu x16,x9,x1
        add x22, x22, x17
    sltu x10, x22, x17
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    mulhu x17,x18,x1
        add x23, x23, x29
    sltu x10, x23, x29
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    mulhu x29,x19,x1
        add x24, x24, x30
    sltu x10, x24, x30
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    mulhu x30,x20,x1
    mul x1,x14,x21    # next na0
        add x25, x25, x16
    sltu x10, x25, x16
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

        add x31, x31, x17
    sltu x10, x31, x17
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

        add x27, x27, x29
    sltu x10, x27, x29
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

        add x26,x26,x30
    add x26,x26,x15


        ld x10,136(x8)

    bnez x10,.Lsqr8x_reduction    # 8 iteration done?

    ld x16,0(x12)
    ld x17,8(x12)
    ld x29,16(x12)
    ld x30,24(x12)
    mv x13,x12
        ld x10,152(x8)

    sub x10,x10,x11    # cnt=np_end-np, done yet?
        sd x10,136(x8)


        add x21, x21, x16
    sltu x15, x21, x16

        add x22, x22, x17
    sltu x10, x22, x17
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    ld x16,32(x12)
    ld x17,40(x12)
        add x23, x23, x29
    sltu x10, x23, x29
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

        add x24, x24, x30
    sltu x10, x24, x30
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    ld x29,48(x12)
    ld x30,56(x12)
        add x25, x25, x16
    sltu x10, x25, x16
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

        add x31, x31, x17
    sltu x10, x31, x17
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

        add x27, x27, x29
    sltu x10, x27, x29
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

        add x26, x26, x30
    sltu x10, x26, x30
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15


    # check whether N has finished iteration.
    # If completed, proceed to the special scenario '8' for processing;
    # otherwise, continue with the reduction Lsqr8x_tail.
        ld x10,136(x8)

    beqz x10,.Lsqr8x8_post_condition

    ld x14,-64(x12)    # load the previous na0
    ld x5,0(x11)    # load next N[0..7]
    ld x6,8(x11)
    ld x7,16(x11)
    ld x28,24(x11)
    ld x9,32(x11)
    ld x18,40(x11)
    li x10,-64
        sd x10,136(x8)
  # set loop cnt,cnt=-64
    ld x19,48(x11)
    ld x20,56(x11)
    add x11,x11,64    # move pointer forward by next 8 elements

.Lsqr8x_tail:
    mul x16,x5,x14
    add x1,zero,x15    # carry bit, modulo-scheduled
    mul x17,x6,x14

        ld x10,136(x8)

    addi x10,x10,8    # cnt=cnt+8
        sd x10,136(x8)


    mul x29,x7,x14
    mul x30,x28,x14
    # low64 partial product + reduction term
        add x21, x21, x16
    sltu x15, x21, x16

    mul x16,x9,x14
        add x22, x22, x17
    sltu x10, x22, x17
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    mul x17,x18,x14
        add x23, x23, x29
    sltu x10, x23, x29
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    mul x29,x19,x14
        add x24, x24, x30
    sltu x10, x24, x30
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    mul x30,x20,x14
        add x25, x25, x16
    sltu x10, x25, x16
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

    mulhu x16,x5,x14
        add x31, x31, x17
    sltu x10, x31, x17
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

    mulhu x17,x6,x14
        add x27, x27, x29
    sltu x10, x27, x29
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

    mulhu x29,x7,x14
        add x26, x26, x30
    sltu x10, x26, x30
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15

    mulhu x30,x28,x14
    add x1,x1,x15
    sd x21,0(x12)
    addi x12,x12,8    # move pointer forward by next 8 elements

    # high64 partial product + reduction term
        add x21, x22, x16
    sltu x15, x21, x16

    mulhu x16,x9,x14
        add x22, x23, x17
    sltu x10, x22, x17
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    mulhu x17,x18,x14
        add x23, x24, x29
    sltu x10, x23, x29
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    mulhu x29,x19,x14
        add x24, x25, x30
    sltu x10, x24, x30
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    mulhu x30,x20,x14

        ld x10,136(x8)

    add x10,x13,x10
    ld x14,0(x10)    # next n0, ld 0(np_temp+cnt)

        add x25, x31, x16
    sltu x10, x25, x16
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

        add x31, x27, x17
    sltu x10, x31, x17
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

        add x27, x26, x29
    sltu x10, x27, x29
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

        add x26, x1, x30
    sltu x10, x26, x30
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15

        ld x10,136(x8)

    bnez x10,.Lsqr8x_tail    # 8 iteration done?

    ld x5,0(x12)
    ld x6,8(x12)
        ld x10,152(x8)

    sub x10,x10,x11    # done yet? cnt=np_end-np
        sd x10,136(x8)


        ld x16,152(x8)

        ld x17,112(x8)

    sub x29, x16, x17    # rewinded np, t2=np_end-num
    ld x7,16(x12)
    ld x28,24(x12)
    ld x9,32(x12)
    ld x18,40(x12)
    ld x19,48(x12)
    ld x20,56(x12)

        ld x10,136(x8)

    beqz x10,.Lsqr8x_tail_break    # exit loop when np=np_end

    ld x14,-64(x13)    # load the previous n0 value
        add x21, x21, x5
    sltu x15, x21, x5

        add x22, x22, x6
    sltu x10, x22, x6
    add x22, x22, x15
    sltu x15, x22, x15
    or x15, x10, x15

    ld x5,0(x11)    # next N[0..7]
    ld x6,8(x11)
        add x23, x23, x7
    sltu x10, x23, x7
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

        add x24, x24, x28
    sltu x10, x24, x28
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    ld x7,16(x11)
    ld x28,24(x11)
        add x25, x25, x9
    sltu x10, x25, x9
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

        add x31, x31, x18
    sltu x10, x31, x18
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

    ld x9,32(x11)
    ld x18,40(x11)
        add x27, x27, x19
    sltu x10, x27, x19
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15


    li x10,-64    # set loop cnt,cnt=-64
        sd x10,136(x8)


        add x26, x26, x20
    sltu x10, x26, x20
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15

    ld x19,48(x11)
    ld x20,56(x11)
    add x11,x11,64
    j .Lsqr8x_tail

.balign 16
.Lsqr8x_tail_break:
        ld x14,120(x8)
    # load n0
    addi x10,x12,64    # cnt=cnt+64
        sd x10,136(x8)

        ld x10,160(x8)
 # load topmost

    snez x15, x10    # "move" topmost carry to carry bit
        add x16, x21, x5
    sltu x10, x16, x5
    add x16, x16, x15
    sltu x15, x16, x15
    or x15, x10, x15

        add x17, x22, x6
    sltu x10, x17, x6
    add x17, x17, x15
    sltu x15, x17, x15
    or x15, x10, x15

    ld x21,0(x13)    # load the current t[0..7] from tp
    ld x22,8(x13)
        add x23, x23, x7
    sltu x10, x23, x7
    add x23, x23, x15
    sltu x15, x23, x15
    or x15, x10, x15

    ld x5,0(x29)    # recall that x29 is &n[0], initialize, load N[0..7]
    ld x6,8(x29)
        add x24, x24, x28
    sltu x10, x24, x28
    add x24, x24, x15
    sltu x15, x24, x15
    or x15, x10, x15

    ld x7,16(x29)
    ld x28,24(x29)
        add x25, x25, x9
    sltu x10, x25, x9
    add x25, x25, x15
    sltu x15, x25, x15
    or x15, x10, x15

        add x31, x31, x18
    sltu x10, x31, x18
    add x31, x31, x15
    sltu x15, x31, x15
    or x15, x10, x15

    ld x9,32(x29)
    ld x18,40(x29)
        add x27, x27, x19
    sltu x10, x27, x19
    add x27, x27, x15
    sltu x15, x27, x15
    or x15, x10, x15

        add x26, x26, x20
    sltu x10, x26, x20
    add x26, x26, x15
    sltu x15, x26, x15
    or x15, x10, x15

    ld x19,48(x29)
    ld x20,56(x29)
    addi x11,x29,64    # np=np+64, move pointer forward by next 8 elements

        ld x10,160(x8)

    add x10,zero,x15    # topmost carry
        sd x10,160(x8)
 # offload topmost

    mul x1,x14,x21    # na0=n0*t[0]
    sd x16,0(x12)
    sd x17,8(x12)
    sd x23,16(x12)
    sd x24,24(x12)
    ld x23,16(x13)
    ld x24,24(x13)
    sd x25,32(x12)
    sd x31,40(x12)
    ld x25,32(x13)
    ld x31,40(x13)
        ld x17,136(x8)
    # load cnt, t1<-cnt
    sd x27,48(x12)
    sd x26,56(x12)
    mv x12,x13    # update tp<-np_temp
    ld x27,48(x13)
    ld x26,56(x13)
    li x10,8    #  set loop cnt, cnt=8
        sd x10,136(x8)

    bne x17,x8, .Lsqr8x_reduction    # if t1!=fp,jump to the next round of reduction and
                                      # continue with the reduction of the next set of na0.

        sd x14,120(x8)
    # offload n0
        ld x1,96(x8)

    add x12,x12,64
        sub x16, x21, x5
    sltu x15, x21, x16

        sub x14, x22, x6
    sltu x10, x22, x14
    sub x17, x14, x15
    sltu x15, x14, x17
    or x15, x10, x15

        ld x29,112(x8)

    addi x30,x29,-64    # cnt=num-64
        sd x30,136(x8)

    mv x10,x1    # ap_end=rp, rp copy
        sd x10,144(x8)


    # conditional subtraction, compute T-N
    # If T < N (borrow occurs), set t = T;
    # If T >= N (no borrow), set t = T-N
    # Process 8 elements per loop iteration.
.Lsqr8x_sub:
        sub x14, x23, x7
    sltu x10, x23, x14
    sub x29, x14, x15
    sltu x15, x14, x29
    or x15, x10, x15
    # t=t-N
    ld x5,0(x11)
    ld x6,8(x11)
        sub x14, x24, x28
    sltu x10, x24, x14
    sub x30, x14, x15
    sltu x15, x14, x30
    or x15, x10, x15

    sd x16,0(x1)    # sd result
    sd x17,8(x1)
        sub x14, x25, x9
    sltu x10, x25, x14
    sub x16, x14, x15
    sltu x15, x14, x16
    or x15, x10, x15

    ld x7,16(x11)
    ld x28,24(x11)
        sub x14, x31, x18
    sltu x10, x31, x14
    sub x17, x14, x15
    sltu x15, x14, x17
    or x15, x10, x15

    sd x29,16(x1)
    sd x30,24(x1)
        sub x14, x27, x19
    sltu x10, x27, x14
    sub x29, x14, x15
    sltu x15, x14, x29
    or x15, x10, x15

    ld x9,32(x11)
    ld x18,40(x11)
        sub x14, x26, x20
    sltu x10, x26, x14
    sub x30, x14, x15
    sltu x15, x14, x30
    or x15, x10, x15

    ld x19,48(x11)
    ld x20,56(x11)
    add x11,x11,64
    ld x21,0(x12)    # ld t[0..7]
    ld x22,8(x12)

        ld x10,136(x8)

    addi x10,x10,-64    # cnt=cnt-64
        sd x10,136(x8)


    ld x23,16(x12)
    ld x24,24(x12)
    ld x25,32(x12)
    ld x31,40(x12)
    ld x27,48(x12)
    ld x26,56(x12)
    add x12,x12,64    # move pointer forward by next 8 elements
    sd x16,32(x1)
    sd x17,40(x1)
        sub x14, x21, x5
    sltu x10, x21, x14
    sub x16, x14, x15
    sltu x15, x14, x16
    or x15, x10, x15

    sd x29,48(x1)
    sd x30,56(x1)
    add x1,x1,64
        sub x14, x22, x6
    sltu x10, x22, x14
    sub x17, x14, x15
    sltu x15, x14, x17
    or x15, x10, x15


        ld x10,136(x8)

    bnez x10,.Lsqr8x_sub    # # if cnt!=0, jump to Lsqr8x_sub

        sub x14, x23, x7
    sltu x10, x23, x14
    sub x29, x14, x15
    sltu x15, x14, x29
    or x15, x10, x15

    mv x12,sp
        ld x10,112(x8)

    add x11,sp,x10    # ap=sp-num

        ld x10,144(x8)

    ld x5,0(x10)    # ld 0(ap_end)
    ld x6,8(x10)

        sub x14, x24, x28
    sltu x10, x24, x14
    sub x30, x14, x15
    sltu x15, x14, x30
    or x15, x10, x15

    sd x16,0(x1)
    sd x17,8(x1)
        sub x14, x25, x9
    sltu x10, x25, x14
    sub x16, x14, x15
    sltu x15, x14, x16
    or x15, x10, x15

        ld x10,144(x8)

    ld x7,16(x10)
    ld x28,24(x10)
        sub x14, x31, x18
    sltu x10, x31, x14
    sub x17, x14, x15
    sltu x15, x14, x17
    or x15, x10, x15

    sd x29,16(x1)
    sd x30,24(x1)
        sub x14, x27, x19
    sltu x10, x27, x14
    sub x29, x14, x15
    sltu x15, x14, x29
    or x15, x10, x15

    ld x21,0(x11)
    ld x22,8(x11)
        sub x14, x26, x20
    sltu x10, x26, x14
    sub x30, x14, x15
    sltu x15, x14, x30
    or x15, x10, x15

    ld x23,16(x11)
    ld x24,24(x11)

        ld x14,160(x8)

    sub x10, x14, x15    # did it borrow?
    sltu x15, x14, x10
    xori x15, x15, 1

    sd x16,32(x1)
    sd x17,40(x1)
    sd x29,48(x1)
    sd x30,56(x1)

        ld x10,112(x8)

    addi x10,x10,-32    # cnt=num-32
        sd x10,136(x8)

        ld x1,128(x8)
    # offload ra


    # The csel conditional instruction selects the result based on the borrow flag.
    # If there is a borrow (C=0), select the original value acc
    # If there is no borrow (C=1), select the subtracted value a
    # Process 4 elements per loop iteration.
.Lsqr4x_cond_copy:
        ld x10,136(x8)

    addi x10,x10,-32    # cnt=cnt_pre-32=num-32-32
        sd x10,136(x8)


        snez x15, x15
    sub x15, zero, x15
    xor x16, x21, x5
    and x16, x16, x15
    xor x16, x21, x16
    # when pre_borrow occurs, t0 = acc0
    sd zero,0(x12)
    sd zero,8(x12)
        snez x15, x15
    sub x15, zero, x15
    xor x17, x22, x6
    and x17, x17, x15
    xor x17, x22, x17

        ld x10,144(x8)

    ld x5,32(x10)
    ld x6,40(x10)
    ld x21,32(x11)
    ld x22,40(x11)
        snez x15, x15
    sub x15, zero, x15
    xor x29, x23, x7
    and x29, x29, x15
    xor x29, x23, x29

    sd zero,16(x12)
    sd zero,24(x12)
    add x12,x12,32
        snez x15, x15
    sub x15, zero, x15
    xor x30, x24, x28
    and x30, x30, x15
    xor x30, x24, x30

    ld x7,48(x10)
    ld x28,56(x10)
    ld x23,48(x11)
    ld x24,56(x11)
    add x11,x11,32
    sd x16,0(x10)
    sd x17,8(x10)
    sd x29,16(x10)
    sd x30,24(x10)
    add x10,x10,32    # ap_end=ap_end+32
        sd x10,144(x8)

    sd zero,0(x11)
    sd zero,8(x11)
    sd zero,16(x11)
    sd zero,24(x11)

        ld x10,136(x8)

    bnez x10,.Lsqr4x_cond_copy    # if cnt!=0, jump to Lsqr4x_cond_copy

        snez x15, x15
    sub x15, zero, x15
    xor x16, x21, x5
    and x16, x16, x15
    xor x16, x21, x16

    sd zero,0(x12)
    sd zero,8(x12)
        snez x15, x15
    sub x15, zero, x15
    xor x17, x22, x6
    and x17, x17, x15
    xor x17, x22, x17

    sd zero,16(x12)
    sd zero,24(x12)
        snez x15, x15
    sub x15, zero, x15
    xor x29, x23, x7
    and x29, x29, x15
    xor x29, x23, x29

        snez x15, x15
    sub x15, zero, x15
    xor x30, x24, x28
    and x30, x30, x15
    xor x30, x24, x30

        ld x10,144(x8)

    sd x16,0(x10)
    sd x17,8(x10)
    sd x29,16(x10)
    sd x30,24(x10)
    j .Lsqr8x_done

# process special case for 8-word boundary
.balign 16
.Lsqr8x8_post_condition:
    add x1,zero,x15
        sub x5, x21, x5
    sltu x15, x21, x5
    # acc0-7,carry hold result, a0-7 hold modulus
        ld x11,96(x8)
    # pull rp
        sub x14, x22, x6
    sltu x10, x22, x14
    sub x6, x14, x15
    sltu x15, x14, x6
    or x15, x10, x15

    sd zero,0(sp)
    sd zero,8(sp)
        sub x14, x23, x7
    sltu x10, x23, x14
    sub x7, x14, x15
    sltu x15, x14, x7
    or x15, x10, x15

    sd zero,16(sp)
    sd zero,24(sp)
        sub x14, x24, x28
    sltu x10, x24, x14
    sub x28, x14, x15
    sltu x15, x14, x28
    or x15, x10, x15

    sd zero,32(sp)
    sd zero,40(sp)
        sub x14, x25, x9
    sltu x10, x25, x14
    sub x9, x14, x15
    sltu x15, x14, x9
    or x15, x10, x15

    sd zero,48(sp)
    sd zero,56(sp)
        sub x14, x31, x18
    sltu x10, x31, x14
    sub x18, x14, x15
    sltu x15, x14, x18
    or x15, x10, x15

    sd zero,64(sp)
    sd zero,72(sp)
        sub x14, x27, x19
    sltu x10, x27, x14
    sub x19, x14, x15
    sltu x15, x14, x19
    or x15, x10, x15

    sd zero,80(sp)
    sd zero,88(sp)
        sub x14, x26, x20
    sltu x10, x26, x14
    sub x20, x14, x15
    sltu x15, x14, x20
    or x15, x10, x15

    sd zero,96(sp)
    sd zero,104(sp)
        sub x14, x1, zero
    sltu x10, x1, x14
    sub x1, x14, x15
    sltu x15, x14, x1
    or x15, x10, x15
    # did it borrow?
    xori x15, x15, 1
    sd zero,112(sp)
    sd zero,120(sp)

    # a0-7 hold result-modulus
        snez x15, x15
    sub x15, zero, x15
    xor x5, x21, x5
    and x5, x5, x15
    xor x5, x21, x5

        snez x15, x15
    sub x15, zero, x15
    xor x6, x22, x6
    and x6, x6, x15
    xor x6, x22, x6

        snez x15, x15
    sub x15, zero, x15
    xor x7, x23, x7
    and x7, x7, x15
    xor x7, x23, x7

        snez x15, x15
    sub x15, zero, x15
    xor x28, x24, x28
    and x28, x28, x15
    xor x28, x24, x28

    sd x5,0(x11)
    sd x6,8(x11)
        snez x15, x15
    sub x15, zero, x15
    xor x9, x25, x9
    and x9, x9, x15
    xor x9, x25, x9

        snez x15, x15
    sub x15, zero, x15
    xor x18, x31, x18
    and x18, x18, x15
    xor x18, x31, x18

    sd x7,16(x11)
    sd x28,24(x11)
        snez x15, x15
    sub x15, zero, x15
    xor x19, x27, x19
    and x19, x19, x15
    xor x19, x27, x19

        snez x15, x15
    sub x15, zero, x15
    xor x20, x26, x20
    and x20, x20, x15
    xor x20, x26, x20

    sd x9,32(x11)
    sd x18,40(x11)
    sd x19,48(x11)
    sd x20,56(x11)

        ld x1,128(x8)
    # pull ra return address
.Lsqr8x_done:
    mv sp, x8
    li x10, 1
    ld      s0,88(sp)
    ld      s1,80(sp)
    ld      s2,72(sp)
    ld      s3,64(sp)
    ld      s4,56(sp)
    ld      s5,48(sp)
    ld      s6,40(sp)
    ld      s7,32(sp)
    ld      s8,24(sp)
    ld      s9,16(sp)
    ld      s11,8(sp)
    ld      s10,0(sp)
    addi    sp,sp,168
    ret
.size bn_sqr8x_mont,.-bn_sqr8x_mont
