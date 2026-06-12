.text
.p2align 3
.globl gcm_init_rv64i_zbc
.type gcm_init_rv64i_zbc,@function
gcm_init_rv64i_zbc:
    ld      a2,0(a1)
    ld      a3,8(a1)
            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a2, 1
        and     t1, t1, t0
        and     a2, a2, t0
        srli    a2, a2, 1
        or      a2, t1, a2

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a2, 2
        and     t1, t1, t0
        and     a2, a2, t0
        srli    a2, a2, 2
        or      a2, t1, a2

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a2, 4
        and     t1, t1, t0
        and     a2, a2, t0
        srli    a2, a2, 4
        or      a2, t1, a2

            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a3, 1
        and     t1, t1, t0
        and     a3, a3, t0
        srli    a3, a3, 1
        or      a3, t1, a3

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a3, 2
        and     t1, t1, t0
        and     a3, a3, t0
        srli    a3, a3, 2
        or      a3, t1, a3

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a3, 4
        and     t1, t1, t0
        and     a3, a3, t0
        srli    a3, a3, 4
        or      a3, t1, a3

            sb      a2, 7(a0)
        srli    t0, a2, 8
        sb      t0, 6(a0)
        srli    t0, a2, 16
        sb      t0, 5(a0)
        srli    t0, a2, 24
        sb      t0, 4(a0)
        srli    t0, a2, 32
        sb      t0, 3(a0)
        srli    t0, a2, 40
        sb      t0, 2(a0)
        srli    t0, a2, 48
        sb      t0, 1(a0)
        srli    t0, a2, 56
        sb      t0, 0(a0)

            sb      a3, 15(a0)
        srli    t0, a3, 8
        sb      t0, 14(a0)
        srli    t0, a3, 16
        sb      t0, 13(a0)
        srli    t0, a3, 24
        sb      t0, 12(a0)
        srli    t0, a3, 32
        sb      t0, 11(a0)
        srli    t0, a3, 40
        sb      t0, 10(a0)
        srli    t0, a3, 48
        sb      t0, 9(a0)
        srli    t0, a3, 56
        sb      t0, 8(a0)

    ret
.size gcm_init_rv64i_zbc,.-gcm_init_rv64i_zbc
.p2align 3
.globl gcm_init_rv64i_zbc__zbb
.type gcm_init_rv64i_zbc__zbb,@function
gcm_init_rv64i_zbc__zbb:
    ld      a2,0(a1)
    ld      a3,8(a1)
            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a2, 1
        and     t1, t1, t0
        and     a2, a2, t0
        srli    a2, a2, 1
        or      a2, t1, a2

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a2, 2
        and     t1, t1, t0
        and     a2, a2, t0
        srli    a2, a2, 2
        or      a2, t1, a2

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a2, 4
        and     t1, t1, t0
        and     a2, a2, t0
        srli    a2, a2, 4
        or      a2, t1, a2

            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a3, 1
        and     t1, t1, t0
        and     a3, a3, t0
        srli    a3, a3, 1
        or      a3, t1, a3

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a3, 2
        and     t1, t1, t0
        and     a3, a3, t0
        srli    a3, a3, 2
        or      a3, t1, a3

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a3, 4
        and     t1, t1, t0
        and     a3, a3, t0
        srli    a3, a3, 4
        or      a3, t1, a3

    .word 1803965971
    .word 1803998867
    sd      a2,0(a0)
    sd      a3,8(a0)
    ret
.size gcm_init_rv64i_zbc__zbb,.-gcm_init_rv64i_zbc__zbb
.p2align 3
.globl gcm_init_rv64i_zbc__zbkb
.type gcm_init_rv64i_zbc__zbkb,@function
gcm_init_rv64i_zbc__zbkb:
    ld      t0,0(a1)
    ld      t1,8(a1)
    .word 1752355475
    .word 1752388371
    .word 1803735699
    .word 1803768595
    sd      t0,0(a0)
    sd      t1,8(a0)
    ret
.size gcm_init_rv64i_zbc__zbkb,.-gcm_init_rv64i_zbc__zbkb
.p2align 3
.globl gcm_gmult_rv64i_zbc
.type gcm_gmult_rv64i_zbc,@function
gcm_gmult_rv64i_zbc:
    # Load Xi and bit-reverse it
    ld        a4, 0(a0)
    ld        a5, 8(a0)
            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a4, 1
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 1
        or      a4, t1, a4

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a4, 2
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 2
        or      a4, t1, a4

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a4, 4
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 4
        or      a4, t1, a4

            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a5, 1
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 1
        or      a5, t1, a5

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a5, 2
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 2
        or      a5, t1, a5

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a5, 4
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 4
        or      a5, t1, a5


    # Load the key (already bit-reversed)
    ld        a6, 0(a1)
    ld        a7, 8(a1)

    # Load the reduction constant
    la        t6, Lpolymod
    lbu       t6, 0(t6)

    # Multiplication (without Karatsuba)
    .word 186105395
    .word 186094515
    .word 186072883
    .word 186061619
    xor       t2, t2, t5
    .word 185057075
    .word 185048755
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 185024307
    .word 185012915
    xor       t1, t1, t5

    # Reduction with clmul
    .word 201211699
    .word 201203379
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 200523571
    .word 200515251
    xor       a5, t1, t5
    xor       a4, t0, t4

    # Bit-reverse Xi back and store it
            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a4, 1
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 1
        or      a4, t1, a4

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a4, 2
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 2
        or      a4, t1, a4

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a4, 4
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 4
        or      a4, t1, a4

            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a5, 1
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 1
        or      a5, t1, a5

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a5, 2
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 2
        or      a5, t1, a5

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a5, 4
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 4
        or      a5, t1, a5

    sd        a4, 0(a0)
    sd        a5, 8(a0)
    ret
.size gcm_gmult_rv64i_zbc,.-gcm_gmult_rv64i_zbc
.p2align 3
.globl gcm_gmult_rv64i_zbc__zbkb
.type gcm_gmult_rv64i_zbc__zbkb,@function
gcm_gmult_rv64i_zbc__zbkb:
    # Load Xi and bit-reverse it
    ld        a4, 0(a0)
    ld        a5, 8(a0)
    .word 1752651539
    .word 1752684435

    # Load the key (already bit-reversed)
    ld        a6, 0(a1)
    ld        a7, 8(a1)

    # Load the reduction constant
    la        t6, Lpolymod
    lbu       t6, 0(t6)

    # Multiplication (without Karatsuba)
    .word 186105395
    .word 186094515
    .word 186072883
    .word 186061619
    xor       t2, t2, t5
    .word 185057075
    .word 185048755
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 185024307
    .word 185012915
    xor       t1, t1, t5

    # Reduction with clmul
    .word 201211699
    .word 201203379
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 200523571
    .word 200515251
    xor       a5, t1, t5
    xor       a4, t0, t4

    # Bit-reverse Xi back and store it
    .word 1752651539
    .word 1752684435
    sd        a4, 0(a0)
    sd        a5, 8(a0)
    ret
.size gcm_gmult_rv64i_zbc__zbkb,.-gcm_gmult_rv64i_zbc__zbkb
.p2align 3
.globl gcm_ghash_rv64i_zbc
.type gcm_ghash_rv64i_zbc,@function
gcm_ghash_rv64i_zbc:
    # Load Xi and bit-reverse it
    ld        a4, 0(a0)
    ld        a5, 8(a0)
            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a4, 1
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 1
        or      a4, t1, a4

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a4, 2
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 2
        or      a4, t1, a4

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a4, 4
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 4
        or      a4, t1, a4

            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a5, 1
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 1
        or      a5, t1, a5

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a5, 2
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 2
        or      a5, t1, a5

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a5, 4
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 4
        or      a5, t1, a5


    # Load the key (already bit-reversed)
    ld        a6, 0(a1)
    ld        a7, 8(a1)

    # Load the reduction constant
    la        t6, Lpolymod
    lbu       t6, 0(t6)

Lstep:
    # Load the input data, bit-reverse them, and XOR them with Xi
    ld        t4, 0(a2)
    ld        t5, 8(a2)
    add       a2, a2, 16
    add       a3, a3, -16
            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, t4, 1
        and     t1, t1, t0
        and     t4, t4, t0
        srli    t4, t4, 1
        or      t4, t1, t4

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, t4, 2
        and     t1, t1, t0
        and     t4, t4, t0
        srli    t4, t4, 2
        or      t4, t1, t4

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, t4, 4
        and     t1, t1, t0
        and     t4, t4, t0
        srli    t4, t4, 4
        or      t4, t1, t4

            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, t5, 1
        and     t1, t1, t0
        and     t5, t5, t0
        srli    t5, t5, 1
        or      t5, t1, t5

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, t5, 2
        and     t1, t1, t0
        and     t5, t5, t0
        srli    t5, t5, 2
        or      t5, t1, t5

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, t5, 4
        and     t1, t1, t0
        and     t5, t5, t0
        srli    t5, t5, 4
        or      t5, t1, t5

    xor       a4, a4, t4
    xor       a5, a5, t5

    # Multiplication (without Karatsuba)
    .word 186105395
    .word 186094515
    .word 186072883
    .word 186061619
    xor       t2, t2, t5
    .word 185057075
    .word 185048755
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 185024307
    .word 185012915
    xor       t1, t1, t5

    # Reduction with clmul
    .word 201211699
    .word 201203379
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 200523571
    .word 200515251
    xor       a5, t1, t5
    xor       a4, t0, t4

    # Iterate over all blocks
    bnez      a3, Lstep

    # Bit-reverse final Xi back and store it
            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a4, 1
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 1
        or      a4, t1, a4

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a4, 2
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 2
        or      a4, t1, a4

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a4, 4
        and     t1, t1, t0
        and     a4, a4, t0
        srli    a4, a4, 4
        or      a4, t1, a4

            la      t2, Lbrev8_const

        ld      t0, 0(t2)  # 0xAAAAAAAAAAAAAAAA
        slli    t1, a5, 1
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 1
        or      a5, t1, a5

        ld      t0, 8(t2)  # 0xCCCCCCCCCCCCCCCC
        slli    t1, a5, 2
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 2
        or      a5, t1, a5

        ld      t0, 16(t2) # 0xF0F0F0F0F0F0F0F0
        slli    t1, a5, 4
        and     t1, t1, t0
        and     a5, a5, t0
        srli    a5, a5, 4
        or      a5, t1, a5

    sd        a4, 0(a0)
    sd        a5, 8(a0)
    ret
.size gcm_ghash_rv64i_zbc,.-gcm_ghash_rv64i_zbc
.p2align 3
.globl gcm_ghash_rv64i_zbc__zbkb
.type gcm_ghash_rv64i_zbc__zbkb,@function
gcm_ghash_rv64i_zbc__zbkb:
    # Load Xi and bit-reverse it
    ld        a4, 0(a0)
    ld        a5, 8(a0)
    .word 1752651539
    .word 1752684435

    # Load the key (already bit-reversed)
    ld        a6, 0(a1)
    ld        a7, 8(a1)

    # Load the reduction constant
    la        t6, Lpolymod
    lbu       t6, 0(t6)

Lstep_zkbk:
    # Load the input data, bit-reverse them, and XOR them with Xi
    ld        t4, 0(a2)
    ld        t5, 8(a2)
    add       a2, a2, 16
    add       a3, a3, -16
    .word 1753144979
    .word 1753177875
    xor       a4, a4, t4
    xor       a5, a5, t5

    # Multiplication (without Karatsuba)
    .word 186105395
    .word 186094515
    .word 186072883
    .word 186061619
    xor       t2, t2, t5
    .word 185057075
    .word 185048755
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 185024307
    .word 185012915
    xor       t1, t1, t5

    # Reduction with clmul
    .word 201211699
    .word 201203379
    xor       t2, t2, t5
    xor       t1, t1, t4
    .word 200523571
    .word 200515251
    xor       a5, t1, t5
    xor       a4, t0, t4

    # Iterate over all blocks
    bnez      a3, Lstep_zkbk

    # Bit-reverse final Xi back and store it
    .word 1752651539
    .word 1752684435
    sd a4,  0(a0)
    sd a5,  8(a0)
    ret
.size gcm_ghash_rv64i_zbc__zbkb,.-gcm_ghash_rv64i_zbc__zbkb
.p2align 3
Lbrev8_const:
    .dword  0xAAAAAAAAAAAAAAAA
    .dword  0xCCCCCCCCCCCCCCCC
    .dword  0xF0F0F0F0F0F0F0F0
.size Lbrev8_const,.-Lbrev8_const

Lpolymod:
    .byte 0x87
.size Lpolymod,.-Lpolymod
