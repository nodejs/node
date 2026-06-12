.text
.balign 16
.globl rv64i_zkne_encrypt
.type   rv64i_zkne_encrypt,@function
rv64i_zkne_encrypt:
    addi    sp,sp,-16
    sd      x8,8(sp)
    sd      x9,0(sp)

    # Load input to block cipher
    ld      x6,0(x10)
    ld      x7,8(x10)

    # Load key
    ld      x13,0(x12)
    ld      x14,8(x12)

    # Load number of rounds
    lwu     x30,240(x12)

    # initial transformation
    xor     x6,x6,x13
    xor     x7,x7,x14

    # The main loop only executes the first N-1 rounds.
    add     x30,x30,-1

    # Do Nr - 1 rounds (final round is special)
1:
    .word 913507379
    .word 912491699

    # Update key ptr to point to next key in schedule
    add     x12,x12,16

    # Grab next key in schedule
    ld      x13,0(x12)
    ld      x14,8(x12)
    xor     x6,x8,x13
    xor     x7,x9,x14

    add     x30,x30,-1
    bgtz    x30,1b

    # final round
    .word 846398515
    .word 845382835

    # since not added 16 before
    ld      x13,16(x12)
    ld      x14,24(x12)
    xor     x6,x8,x13
    xor     x7,x9,x14

    sd      x6,0(x11)
    sd      x7,8(x11)

    # Pop registers and return
    ld      x8,8(sp)
    ld      x9,0(sp)
    addi    sp,sp,16
    ret
.text
.balign 16
.globl rv64i_zknd_decrypt
.type   rv64i_zknd_decrypt,@function
rv64i_zknd_decrypt:
    addi    sp,sp,-16
    sd      x8,8(sp)
    sd      x9,0(sp)

    # Load input to block cipher
    ld      x6,0(x10)
    ld      x7,8(x10)

    # Load number of rounds
    lwu     x30,240(x12)

    # Load the last key
    slli    x13,x30,4
    add     x12,x12,x13
    ld      x13,0(x12)
    ld      x14,8(x12)

    xor     x6,x6,x13
    xor     x7,x7,x14

    # The main loop only executes the first N-1 rounds.
    add     x30,x30,-1

    # Do Nr - 1 rounds (final round is special)
1:
    .word 1047725107
    .word 1046709427

    # Update key ptr to point to next key in schedule
    add     x12,x12,-16

    # Grab next key in schedule
    ld      x13,0(x12)
    ld      x14,8(x12)
    xor     x6,x8,x13
    xor     x7,x9,x14

    add     x30,x30,-1
    bgtz    x30,1b

    # final round
    .word 980616243
    .word 979600563

    add     x12,x12,-16
    ld      x13,0(x12)
    ld      x14,8(x12)
    xor     x6,x8,x13
    xor     x7,x9,x14

    sd      x6,0(x11)
    sd      x7,8(x11)
    # Pop registers and return
    ld      x8,8(sp)
    ld      x9,0(sp)
    addi    sp,sp,16
    ret
.text
.balign 16
.globl rv64i_zkne_set_encrypt_key
.type   rv64i_zkne_set_encrypt_key,@function
rv64i_zkne_set_encrypt_key:
    addi    sp,sp,-16
    sd      x8,0(sp)
    bnez    x10,1f        # if (!userKey || !key) return -1;
    bnez    x12,1f
    li      a0,-1
    ret
1:
    # Determine number of rounds from key size in bits
    li      x6,128
    bne     x11,x6,1f
    li      x7,10          # key->rounds = 10 if bits == 128
    sw      x7,240(x12)  # store key->rounds
    ld      x6,0(x10)
    ld      x7,8(x10)
    sd      x6,0(x12)
    sd      x7,8(x12)
    .word 822318099
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 823366675
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 824415251
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 825463827
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 826512403
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 827560979
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 828609555
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 829658131
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 830706707
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 831755283
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)

    j       4f
1:
    li      x6,192
    bne     x11,x6,2f
    li      x7,12          # key->rounds = 12 if bits == 192
    sw      x7,240(x12)  # store key->rounds
    ld      x6,0(x10)
    ld      x7,8(x10)
    ld      x8,16(x10)
    sd      x6,0(x12)
    sd      x7,8(x12)
    sd      x8,16(x12)
    .word 822351507
    .word 2120647475
    .word 2121466803
    .word 2122548275
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)
    sd          x8,16(x12)
    .word 823400083
    .word 2120647475
    .word 2121466803
    .word 2122548275
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)
    sd          x8,16(x12)
    .word 824448659
    .word 2120647475
    .word 2121466803
    .word 2122548275
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)
    sd          x8,16(x12)
    .word 825497235
    .word 2120647475
    .word 2121466803
    .word 2122548275
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)
    sd          x8,16(x12)
    .word 826545811
    .word 2120647475
    .word 2121466803
    .word 2122548275
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)
    sd          x8,16(x12)
    .word 827594387
    .word 2120647475
    .word 2121466803
    .word 2122548275
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)
    sd          x8,16(x12)
    .word 828642963
    .word 2120647475
    .word 2121466803
    .word 2122548275
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)
    sd          x8,16(x12)
    .word 829691539
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)

    j       4f
2:
    li      x7,14          # key->rounds = 14 if bits == 256
    li      x6,256
    beq     x11,x6,3f
    li      a0,-2           # If bits != 128, 192, or 256, return -2
    j       5f
3:
    sw      x7,240(x12)  # store key->rounds
    ld      x6,0(x10)
    ld      x7,8(x10)
    ld      x8,16(x10)
    ld      x13,24(x10)
    sd      x6,0(x12)
    sd      x7,8(x12)
    sd      x8,16(x12)
    sd      x13,24(x12)
    .word 822515475
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 832804627
    .word 2122777651
    .word 2127824563
    sd          x8,16(x12)
    sd          x13,24(x12)
    .word 823564051
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 832804627
    .word 2122777651
    .word 2127824563
    sd          x8,16(x12)
    sd          x13,24(x12)
    .word 824612627
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 832804627
    .word 2122777651
    .word 2127824563
    sd          x8,16(x12)
    sd          x13,24(x12)
    .word 825661203
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 832804627
    .word 2122777651
    .word 2127824563
    sd          x8,16(x12)
    sd          x13,24(x12)
    .word 826709779
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 832804627
    .word 2122777651
    .word 2127824563
    sd          x8,16(x12)
    sd          x13,24(x12)
    .word 827758355
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)
    .word 832804627
    .word 2122777651
    .word 2127824563
    sd          x8,16(x12)
    sd          x13,24(x12)
    .word 828806931
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)

4:  # return 0
    li      a0,0
5:  # return a0
    ld      x8,0(sp)
    addi    sp,sp,16
    ret
.text
.balign 16
.globl rv64i_zknd_set_decrypt_key
.type   rv64i_zknd_set_decrypt_key,@function
rv64i_zknd_set_decrypt_key:
    addi    sp,sp,-16
    sd      x8,0(sp)
    bnez    x10,1f        # if (!userKey || !key) return -1;
    bnez    x12,1f
    li      a0,-1
    ret
1:
    # Determine number of rounds from key size in bits
    li      x6,128
    bne     x11,x6,1f
    li      x7,10          # key->rounds = 10 if bits == 128
    sw      x7,240(x12)  # store key->rounds
    ld      x6,0(x10)
    ld      x7,8(x10)
    sd      x6,0(x12)
    sd      x7,8(x12)
    .word 822318099
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 823366675
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 824415251
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 825463827
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 826512403
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 827560979
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 828609555
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 829658131
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 830706707
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    .word 805508115
    sd          x8,0(x12)
    .word 805540883
    sd          x8,8(x12)
    .word 831755283
    .word 2120483635
    .word 2121466803
    add         x12,x12,16
    sd          x6,0(x12)
    sd          x7,8(x12)

    j       4f
1:
    li      x6,192
    bne     x11,x6,2f
    li      x7,12          # key->rounds = 12 if bits == 192
    sw      x7,240(x12)  # store key->rounds
    ld      x6,0(x10)
    ld      x7,8(x10)
    ld      x8,16(x10)
    sd      x6,0(x12)
    sd      x7,8(x12)
    .word 805574291
    sd      x13,16(x12)
    .word 822351507
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    .word 805508755
    sd          x13,0(x12)
    .word 805541523
    sd          x13,8(x12)
    # the reason is in ke192enc
    .word 2122548275
    .word 805574291
    sd          x13,16(x12)
    .word 823400083
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    .word 805508755
    sd          x13,0(x12)
    .word 805541523
    sd          x13,8(x12)
    # the reason is in ke192enc
    .word 2122548275
    .word 805574291
    sd          x13,16(x12)
    .word 824448659
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    .word 805508755
    sd          x13,0(x12)
    .word 805541523
    sd          x13,8(x12)
    # the reason is in ke192enc
    .word 2122548275
    .word 805574291
    sd          x13,16(x12)
    .word 825497235
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    .word 805508755
    sd          x13,0(x12)
    .word 805541523
    sd          x13,8(x12)
    # the reason is in ke192enc
    .word 2122548275
    .word 805574291
    sd          x13,16(x12)
    .word 826545811
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    .word 805508755
    sd          x13,0(x12)
    .word 805541523
    sd          x13,8(x12)
    # the reason is in ke192enc
    .word 2122548275
    .word 805574291
    sd          x13,16(x12)
    .word 827594387
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    .word 805508755
    sd          x13,0(x12)
    .word 805541523
    sd          x13,8(x12)
    # the reason is in ke192enc
    .word 2122548275
    .word 805574291
    sd          x13,16(x12)
    .word 828642963
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    .word 805508755
    sd          x13,0(x12)
    .word 805541523
    sd          x13,8(x12)
    # the reason is in ke192enc
    .word 2122548275
    .word 805574291
    sd          x13,16(x12)
    .word 829691539
    .word 2120647475
    .word 2121466803
    add         x12,x12,24
    sd          x6,0(x12)
    sd          x7,8(x12)

    j       4f
2:
    li      x7,14          # key->rounds = 14 if bits == 256
    li      x6,256
    beq     x11,x6,3f
    li      a0,-2           # If bits != 128, 192, or 256, return -2
    j       5f
3:
    sw      x7,240(x12)  # store key->rounds
    ld      x6,0(x10)
    ld      x7,8(x10)
    ld      x8,16(x10)
    ld      x13,24(x10)
    sd      x6,0(x12)
    sd      x7,8(x12)
    .word 805574419
    sd      x14,16(x12)
    .word 805738259
    sd      x14,24(x12)
    .word 822515475
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    .word 832804627
    .word 2122777651
    .word 2127824563
    .word 805508883
    sd          x14,0(x12)
    .word 805541651
    sd          x14,8(x12)
    .word 805574419
    sd          x14,16(x12)
    .word 805738259
    sd          x14,24(x12)
    .word 823564051
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    .word 832804627
    .word 2122777651
    .word 2127824563
    .word 805508883
    sd          x14,0(x12)
    .word 805541651
    sd          x14,8(x12)
    .word 805574419
    sd          x14,16(x12)
    .word 805738259
    sd          x14,24(x12)
    .word 824612627
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    .word 832804627
    .word 2122777651
    .word 2127824563
    .word 805508883
    sd          x14,0(x12)
    .word 805541651
    sd          x14,8(x12)
    .word 805574419
    sd          x14,16(x12)
    .word 805738259
    sd          x14,24(x12)
    .word 825661203
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    .word 832804627
    .word 2122777651
    .word 2127824563
    .word 805508883
    sd          x14,0(x12)
    .word 805541651
    sd          x14,8(x12)
    .word 805574419
    sd          x14,16(x12)
    .word 805738259
    sd          x14,24(x12)
    .word 826709779
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    .word 832804627
    .word 2122777651
    .word 2127824563
    .word 805508883
    sd          x14,0(x12)
    .word 805541651
    sd          x14,8(x12)
    .word 805574419
    sd          x14,16(x12)
    .word 805738259
    sd          x14,24(x12)
    .word 827758355
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    .word 832804627
    .word 2122777651
    .word 2127824563
    .word 805508883
    sd          x14,0(x12)
    .word 805541651
    sd          x14,8(x12)
    .word 805574419
    sd          x14,16(x12)
    .word 805738259
    sd          x14,24(x12)
    .word 828806931
    .word 2120680243
    .word 2121466803
    add         x12,x12,32
    sd          x6,0(x12)
    sd          x7,8(x12)
    # last two one dropped

4:  # return 0
    li      a0,0
5:  # return a0
    ld      x8,0(sp)
    addi    sp,sp,16
    ret
