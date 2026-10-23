.text
.p2align 3
.globl ossl_md5_block_asm_data_order_zbb
.type ossl_md5_block_asm_data_order_zbb,@function
ossl_md5_block_asm_data_order_zbb:

    addi sp, sp, -64

    sd s0, 0(sp)
    sd s1, 8(sp)
    sd s2, 16(sp)
    sd s3, 24(sp)
    sd s4, 32(sp)
    sd s5, 40(sp)
    sd s6, 48(sp)
    sd s7, 56(sp)

    # load ctx
    lw a4, 0(a0)
    lw a5, 4(a0)
    lw a6, 8(a0)
    lw a7, 12(a0)

L_round_loop:

    ld s0, 0(a1)
    ld s1, 8(a1)
    ld s2, 16(a1)
    ld s3, 24(a1)
    ld s4, 32(a1)
    ld s5, 40(a1)
    ld s6, 48(a1)
    ld s7, 56(a1)

    mv t3, a4
    mv t4, a5
    mv t5, a6
    mv t6, a7

        li t0, 3614090360
        addw a4, a4, s0
    addw a4, a4, t0
    xor t0, a6, a7
    and t0, t0, a5
    xor t0, t0, a7
    addw a4, a4, t0
        .word 1637308187
    addw a4, a4, a5

        li t0, 3905402710
        addw a7, a7, t0
        srli t0, s0, 32
    addw a7, a7, t0
    xor t0, a5, a6
    and t0, t0, a4
    xor t0, t0, a6
    addw a7, a7, t0
        .word 1632163995
    addw a7, a7, a4

        li t0, 606105819
        addw a6, a6, s1
    addw a6, a6, t0
    xor t0, a4, a5
    and t0, t0, a7
    xor t0, t0, a5
    addw a6, a6, t0
        .word 1626888219
    addw a6, a6, a7

        li t0, 3250441966
        addw a5, a5, t0
        srli t0, s1, 32
    addw a5, a5, t0
    xor t0, a7, a4
    and t0, t0, a6
    xor t0, t0, a4
    addw a5, a5, t0
        .word 1621612443
    addw a5, a5, a6

        li t0, 4118548399
        addw a4, a4, s2
    addw a4, a4, t0
    xor t0, a6, a7
    and t0, t0, a5
    xor t0, t0, a7
    addw a4, a4, t0
        .word 1637308187
    addw a4, a4, a5

        li t0, 1200080426
        addw a7, a7, t0
        srli t0, s2, 32
    addw a7, a7, t0
    xor t0, a5, a6
    and t0, t0, a4
    xor t0, t0, a6
    addw a7, a7, t0
        .word 1632163995
    addw a7, a7, a4

        li t0, 2821735955
        addw a6, a6, s3
    addw a6, a6, t0
    xor t0, a4, a5
    and t0, t0, a7
    xor t0, t0, a5
    addw a6, a6, t0
        .word 1626888219
    addw a6, a6, a7

        li t0, 4249261313
        addw a5, a5, t0
        srli t0, s3, 32
    addw a5, a5, t0
    xor t0, a7, a4
    and t0, t0, a6
    xor t0, t0, a4
    addw a5, a5, t0
        .word 1621612443
    addw a5, a5, a6

        li t0, 1770035416
        addw a4, a4, s4
    addw a4, a4, t0
    xor t0, a6, a7
    and t0, t0, a5
    xor t0, t0, a7
    addw a4, a4, t0
        .word 1637308187
    addw a4, a4, a5

        li t0, 2336552879
        addw a7, a7, t0
        srli t0, s4, 32
    addw a7, a7, t0
    xor t0, a5, a6
    and t0, t0, a4
    xor t0, t0, a6
    addw a7, a7, t0
        .word 1632163995
    addw a7, a7, a4

        li t0, 4294925233
        addw a6, a6, s5
    addw a6, a6, t0
    xor t0, a4, a5
    and t0, t0, a7
    xor t0, t0, a5
    addw a6, a6, t0
        .word 1626888219
    addw a6, a6, a7

        li t0, 2304563134
        addw a5, a5, t0
        srli t0, s5, 32
    addw a5, a5, t0
    xor t0, a7, a4
    and t0, t0, a6
    xor t0, t0, a4
    addw a5, a5, t0
        .word 1621612443
    addw a5, a5, a6

        li t0, 1804603682
        addw a4, a4, s6
    addw a4, a4, t0
    xor t0, a6, a7
    and t0, t0, a5
    xor t0, t0, a7
    addw a4, a4, t0
        .word 1637308187
    addw a4, a4, a5

        li t0, 4254626195
        addw a7, a7, t0
        srli t0, s6, 32
    addw a7, a7, t0
    xor t0, a5, a6
    and t0, t0, a4
    xor t0, t0, a6
    addw a7, a7, t0
        .word 1632163995
    addw a7, a7, a4

        li t0, 2792965006
        addw a6, a6, s7
    addw a6, a6, t0
    xor t0, a4, a5
    and t0, t0, a7
    xor t0, t0, a5
    addw a6, a6, t0
        .word 1626888219
    addw a6, a6, a7

        li t0, 1236535329
        addw a5, a5, t0
        srli t0, s7, 32
    addw a5, a5, t0
    xor t0, a7, a4
    and t0, t0, a6
    xor t0, t0, a4
    addw a5, a5, t0
        .word 1621612443
    addw a5, a5, a6


        li t0, 4129170786
        addw a4, a4, t0
        srli t0, s0, 32
    addw a4, a4, t0
    xor t0, a5, a6
    and t0, t0, a7
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1639405339
    addw a4, a4, a5

        li t0, 3225465664
        addw a7, a7, s3
    addw a7, a7, t0
    xor t0, a4, a5
    and t0, t0, a6
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1635309723
    addw a7, a7, a4

        li t0, 643717713
        addw a6, a6, t0
        srli t0, s5, 32
    addw a6, a6, t0
    xor t0, a7, a4
    and t0, t0, a5
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1630033947
    addw a6, a6, a7

        li t0, 3921069994
        addw a5, a5, s0
    addw a5, a5, t0
    xor t0, a6, a7
    and t0, t0, a4
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1623709595
    addw a5, a5, a6

        li t0, 3593408605
        addw a4, a4, t0
        srli t0, s2, 32
    addw a4, a4, t0
    xor t0, a5, a6
    and t0, t0, a7
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1639405339
    addw a4, a4, a5

        li t0, 38016083
        addw a7, a7, s5
    addw a7, a7, t0
    xor t0, a4, a5
    and t0, t0, a6
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1635309723
    addw a7, a7, a4

        li t0, 3634488961
        addw a6, a6, t0
        srli t0, s7, 32
    addw a6, a6, t0
    xor t0, a7, a4
    and t0, t0, a5
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1630033947
    addw a6, a6, a7

        li t0, 3889429448
        addw a5, a5, s2
    addw a5, a5, t0
    xor t0, a6, a7
    and t0, t0, a4
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1623709595
    addw a5, a5, a6

        li t0, 568446438
        addw a4, a4, t0
        srli t0, s4, 32
    addw a4, a4, t0
    xor t0, a5, a6
    and t0, t0, a7
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1639405339
    addw a4, a4, a5

        li t0, 3275163606
        addw a7, a7, s7
    addw a7, a7, t0
    xor t0, a4, a5
    and t0, t0, a6
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1635309723
    addw a7, a7, a4

        li t0, 4107603335
        addw a6, a6, t0
        srli t0, s1, 32
    addw a6, a6, t0
    xor t0, a7, a4
    and t0, t0, a5
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1630033947
    addw a6, a6, a7

        li t0, 1163531501
        addw a5, a5, s4
    addw a5, a5, t0
    xor t0, a6, a7
    and t0, t0, a4
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1623709595
    addw a5, a5, a6

        li t0, 2850285829
        addw a4, a4, t0
        srli t0, s6, 32
    addw a4, a4, t0
    xor t0, a5, a6
    and t0, t0, a7
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1639405339
    addw a4, a4, a5

        li t0, 4243563512
        addw a7, a7, s1
    addw a7, a7, t0
    xor t0, a4, a5
    and t0, t0, a6
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1635309723
    addw a7, a7, a4

        li t0, 1735328473
        addw a6, a6, t0
        srli t0, s3, 32
    addw a6, a6, t0
    xor t0, a7, a4
    and t0, t0, a5
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1630033947
    addw a6, a6, a7

        li t0, 2368359562
        addw a5, a5, s6
    addw a5, a5, t0
    xor t0, a6, a7
    and t0, t0, a4
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1623709595
    addw a5, a5, a6


        li t0, 4294588738
        addw a4, a4, t0
        srli t0, s2, 32
    addw a4, a4, t0
    xor t0, a6, a7
    xor t0, t0, a5
    addw a4, a4, t0
        .word 1640453915
    addw a4, a4, a5

        li t0, 2272392833
        addw a7, a7, s4
    addw a7, a7, t0
    xor t0, a5, a6
    xor t0, t0, a4
    addw a7, a7, t0
        .word 1633212571
    addw a7, a7, a4

        li t0, 1839030562
        addw a6, a6, t0
        srli t0, s5, 32
    addw a6, a6, t0
    xor t0, a4, a5
    xor t0, t0, a7
    addw a6, a6, t0
        .word 1627936795
    addw a6, a6, a7

        li t0, 4259657740
        addw a5, a5, s7
    addw a5, a5, t0
    xor t0, a7, a4
    xor t0, t0, a6
    addw a5, a5, t0
        .word 1620563867
    addw a5, a5, a6

        li t0, 2763975236
        addw a4, a4, t0
        srli t0, s0, 32
    addw a4, a4, t0
    xor t0, a6, a7
    xor t0, t0, a5
    addw a4, a4, t0
        .word 1640453915
    addw a4, a4, a5

        li t0, 1272893353
        addw a7, a7, s2
    addw a7, a7, t0
    xor t0, a5, a6
    xor t0, t0, a4
    addw a7, a7, t0
        .word 1633212571
    addw a7, a7, a4

        li t0, 4139469664
        addw a6, a6, t0
        srli t0, s3, 32
    addw a6, a6, t0
    xor t0, a4, a5
    xor t0, t0, a7
    addw a6, a6, t0
        .word 1627936795
    addw a6, a6, a7

        li t0, 3200236656
        addw a5, a5, s5
    addw a5, a5, t0
    xor t0, a7, a4
    xor t0, t0, a6
    addw a5, a5, t0
        .word 1620563867
    addw a5, a5, a6

        li t0, 681279174
        addw a4, a4, t0
        srli t0, s6, 32
    addw a4, a4, t0
    xor t0, a6, a7
    xor t0, t0, a5
    addw a4, a4, t0
        .word 1640453915
    addw a4, a4, a5

        li t0, 3936430074
        addw a7, a7, s0
    addw a7, a7, t0
    xor t0, a5, a6
    xor t0, t0, a4
    addw a7, a7, t0
        .word 1633212571
    addw a7, a7, a4

        li t0, 3572445317
        addw a6, a6, t0
        srli t0, s1, 32
    addw a6, a6, t0
    xor t0, a4, a5
    xor t0, t0, a7
    addw a6, a6, t0
        .word 1627936795
    addw a6, a6, a7

        li t0, 76029189
        addw a5, a5, s3
    addw a5, a5, t0
    xor t0, a7, a4
    xor t0, t0, a6
    addw a5, a5, t0
        .word 1620563867
    addw a5, a5, a6

        li t0, 3654602809
        addw a4, a4, t0
        srli t0, s4, 32
    addw a4, a4, t0
    xor t0, a6, a7
    xor t0, t0, a5
    addw a4, a4, t0
        .word 1640453915
    addw a4, a4, a5

        li t0, 3873151461
        addw a7, a7, s6
    addw a7, a7, t0
    xor t0, a5, a6
    xor t0, t0, a4
    addw a7, a7, t0
        .word 1633212571
    addw a7, a7, a4

        li t0, 530742520
        addw a6, a6, t0
        srli t0, s7, 32
    addw a6, a6, t0
    xor t0, a4, a5
    xor t0, t0, a7
    addw a6, a6, t0
        .word 1627936795
    addw a6, a6, a7

        li t0, 3299628645
        addw a5, a5, s1
    addw a5, a5, t0
    xor t0, a7, a4
    xor t0, t0, a6
    addw a5, a5, t0
        .word 1620563867
    addw a5, a5, a6


        li t0, 4096336452
        addw a4, a4, s0
    addw a4, a4, t0
        .word 1092084403
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1638356763
    addw a4, a4, a5

        li t0, 1126891415
        addw a7, a7, t0
        srli t0, s3, 32
    addw a7, a7, t0
        .word 1091003059
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1634261147
    addw a7, a7, a4

        li t0, 2878612391
        addw a6, a6, s7
    addw a6, a6, t0
        .word 1090052787
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1628985371
    addw a6, a6, a7

        li t0, 4237533241
        addw a5, a5, t0
        srli t0, s2, 32
    addw a5, a5, t0
        .word 1088971443
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1622661019
    addw a5, a5, a6

        li t0, 1700485571
        addw a4, a4, s6
    addw a4, a4, t0
        .word 1092084403
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1638356763
    addw a4, a4, a5

        li t0, 2399980690
        addw a7, a7, t0
        srli t0, s1, 32
    addw a7, a7, t0
        .word 1091003059
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1634261147
    addw a7, a7, a4

        li t0, 4293915773
        addw a6, a6, s5
    addw a6, a6, t0
        .word 1090052787
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1628985371
    addw a6, a6, a7

        li t0, 2240044497
        addw a5, a5, t0
        srli t0, s0, 32
    addw a5, a5, t0
        .word 1088971443
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1622661019
    addw a5, a5, a6

        li t0, 1873313359
        addw a4, a4, s4
    addw a4, a4, t0
        .word 1092084403
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1638356763
    addw a4, a4, a5

        li t0, 4264355552
        addw a7, a7, t0
        srli t0, s7, 32
    addw a7, a7, t0
        .word 1091003059
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1634261147
    addw a7, a7, a4

        li t0, 2734768916
        addw a6, a6, s3
    addw a6, a6, t0
        .word 1090052787
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1628985371
    addw a6, a6, a7

        li t0, 1309151649
        addw a5, a5, t0
        srli t0, s6, 32
    addw a5, a5, t0
        .word 1088971443
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1622661019
    addw a5, a5, a6

        li t0, 4149444226
        addw a4, a4, s2
    addw a4, a4, t0
        .word 1092084403
    xor t0, t0, a6
    addw a4, a4, t0
        .word 1638356763
    addw a4, a4, a5

        li t0, 3174756917
        addw a7, a7, t0
        srli t0, s5, 32
    addw a7, a7, t0
        .word 1091003059
    xor t0, t0, a5
    addw a7, a7, t0
        .word 1634261147
    addw a7, a7, a4

        li t0, 718787259
        addw a6, a6, s1
    addw a6, a6, t0
        .word 1090052787
    xor t0, t0, a4
    addw a6, a6, t0
        .word 1628985371
    addw a6, a6, a7

        li t0, 3951481745
        addw a5, a5, t0
        srli t0, s4, 32
    addw a5, a5, t0
        .word 1088971443
    xor t0, t0, a7
    addw a5, a5, t0
        .word 1622661019
    addw a5, a5, a6


    addw a4, a4, t3
    addw a5, a5, t4
    addw a6, a6, t5
    addw a7, a7, t6

    addi a1, a1, 64

    addi a2, a2, -1

    bnez a2, L_round_loop

    sw a4, 0(a0)
    sw a5, 4(a0)
    sw a6, 8(a0)
    sw a7, 12(a0)

    ld s0, 0(sp)
    ld s1, 8(sp)
    ld s2, 16(sp)
    ld s3, 24(sp)
    ld s4, 32(sp)
    ld s5, 40(sp)
    ld s6, 48(sp)
    ld s7, 56(sp)

    addi sp, sp, 64

    ret
.size ossl_md5_block_asm_data_order_zbb,.-ossl_md5_block_asm_data_order_zbb
