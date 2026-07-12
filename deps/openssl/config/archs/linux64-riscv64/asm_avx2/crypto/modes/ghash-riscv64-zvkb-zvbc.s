.text
.p2align 3
.globl gcm_init_rv64i_zvkb_zvbc
.type gcm_init_rv64i_zvkb_zvbc,@function
gcm_init_rv64i_zvkb_zvbc:
    # Load/store data in reverse order.
    # This is needed as a part of endianness swap.
    add a1, a1, 8
    li t0, -8
    li t1, 63
    la t2, Lpolymod

    .word 0xc1817057 # vsetivli x0, 2, e64, m1, tu, mu

    .word 173404295    # vlse64.v v1, (a1), t0
    .word 33812743          # vle64.v v2, (t2)

    # Shift one left and get the carry bits.
    .word 2719171031     # vsrl.vx v3, v1, t1
    .word 2517676247         # vsll.vi v1, v1, 1

    # Use the fact that the polynomial degree is no more than 128,
    # i.e. only the LSB of the upper half could be set.
    # Thanks to this we don't need to do the full reduction here.
    # Instead simply subtract the reduction polynomial.
    # This idea was taken from x86 ghash implementation in OpenSSL.
    .word 976269911     # vslideup.vi v4, v3, 1
    .word 1043378647   # vslidedown.vi v3, v3, 1

    .word 1577136215              # vmv.v.i v0, 2
    .word 672268503    # vor.vv v1, v1, v4, v0.t

    # Need to set the mask to 3, if the carry bit is set.
    .word 1577156695            # vmv.v.v v0, v3
    .word 1577071063              # vmv.v.i v3, 0
    .word 1546760663      # vmerge.vim v3, v3, 3, v0
    .word 1577156695            # vmv.v.v v0, v3

    .word 739311831   # vxor.vv v1, v1, v2, v0.t

    .word 33910951        # vse64.v v1, (a0)
    ret
.size gcm_init_rv64i_zvkb_zvbc,.-gcm_init_rv64i_zvkb_zvbc
.text
.p2align 3
.globl gcm_gmult_rv64i_zvkb_zvbc
.type gcm_gmult_rv64i_zvkb_zvbc,@function
gcm_gmult_rv64i_zvkb_zvbc:
    ld t0, (a1)
    ld t1, 8(a1)
    li t2, 63
    la t3, Lpolymod
    ld t3, 8(t3)

    # Load/store data in reverse order.
    # This is needed as a part of endianness swap.
    add a0, a0, 8
    li t4, -8

    .word 0xc1817057 # vsetivli x0, 2, e64, m1, tu, mu

    .word 198537863    # vlse64.v v5, (a0), t4
    .word 1247060695            # vrev8.v v5, v5

    # Multiplication

    # Do two 64x64 multiplications in one go to save some time
    # and simplify things.

    # A = a1a0 (t1, t0)
    # B = b1b0 (v5)
    # C = c1c0 (256 bit)
    # c1 = a1b1 + (a0b1)h + (a1b0)h
    # c0 = a0b0 + (a0b1)l + (a1b0)h

    # v1 = (a0b1)l,(a0b0)l
    .word 844292311   # vclmul.vx v1, v5, t0
    # v3 = (a0b1)h,(a0b0)h
    .word 911401431  # vclmulh.vx v3, v5, t0

    # v4 = (a1b1)l,(a1b0)l
    .word 844325463   # vclmul.vx v4, v5, t1
    # v2 = (a1b1)h,(a1b0)h
    .word 911434071   # vclmulh.vx v2, v5, t1

    # Is there a better way to do this?
    # Would need to swap the order of elements within a vector register.
    .word 976270039     # vslideup.vi v5, v3, 1
    .word 977318743     # vslideup.vi v6, v4, 1
    .word 1043378647   # vslidedown.vi v3, v3, 1
    .word 1044427351   # vslidedown.vi v4, v4, 1

    .word 1577103447              # vmv.v.i v0, 1
    # v2 += (a0b1)h
    .word 740393303   # vxor.vv v2, v2, v3, v0.t
    # v2 += (a1b1)l
    .word 740426071   # vxor.vv v2, v2, v4, v0.t

    .word 1577136215              # vmv.v.i v0, 2
    # v1 += (a0b0)h,0
    .word 739410135   # vxor.vv v1, v1, v5, v0.t
    # v1 += (a1b0)l,0
    .word 739442903   # vxor.vv v1, v1, v6, v0.t

    # Now the 256bit product should be stored in (v2,v1)
    # v1 = (a0b1)l + (a0b0)h + (a1b0)l, (a0b0)l
    # v2 = (a1b1)h, (a1b0)h + (a0b1)h + (a1b1)l

    # Reduction
    # Let C := A*B = c3,c2,c1,c0 = v2[1],v2[0],v1[1],v1[0]
    # This is a slight variation of the Gueron's Montgomery reduction.
    # The difference being the order of some operations has been changed,
    # to make a better use of vclmul(h) instructions.

    # First step:
    # c1 += (c0 * P)l
    # vmv.v.i v0, 2
    .word 940618199 # vslideup.vi v3, v1, 1, v0.t
    .word 809394647 # vclmul.vx v3, v3, t3, v0.t
    .word 739344599   # vxor.vv v1, v1, v3, v0.t

    # Second step:
    # D = d1,d0 is final result
    # We want:
    # m1 = c1 + (c1 * P)h
    # m0 = (c1 * P)l + (c0 * P)h + c0
    # d1 = c3 + m1
    # d0 = c2 + m0

    #v3 = (c1 * P)l, 0
    .word 807297495 # vclmul.vx v3, v1, t3, v0.t
    #v4 = (c1 * P)h, (c0 * P)h
    .word 907960919   # vclmulh.vx v4, v1, t3

    .word 1577103447              # vmv.v.i v0, 1
    .word 1043378647   # vslidedown.vi v3, v3, 1

    .word 772931799       # vxor.vv v1, v1, v4
    .word 739344599   # vxor.vv v1, v1, v3, v0.t

    # XOR in the upper upper part of the product
    .word 773882199       # vxor.vv v2, v2, v1

    .word 1243914583            # vrev8.v v2, v2
    .word 198537511    # vsse64.v v2, (a0), t4
    ret
.size gcm_gmult_rv64i_zvkb_zvbc,.-gcm_gmult_rv64i_zvkb_zvbc
.p2align 3
.globl gcm_ghash_rv64i_zvkb_zvbc
.type gcm_ghash_rv64i_zvkb_zvbc,@function
gcm_ghash_rv64i_zvkb_zvbc:
    ld t0, (a1)
    ld t1, 8(a1)
    li t2, 63
    la t3, Lpolymod
    ld t3, 8(t3)

    # Load/store data in reverse order.
    # This is needed as a part of endianness swap.
    add a0, a0, 8
    add a2, a2, 8
    li t4, -8

    .word 0xc1817057 # vsetivli x0, 2, e64, m1, tu, mu

    .word 198537863      # vlse64.v v5, (a0), t4

Lstep:
    # Read input data
    .word 198603655   # vle64.v v0, (a2)
    add a2, a2, 16
    add a3, a3, -16
    # XOR them into Xi
    .word 777224919       # vxor.vv v0, v0, v1

    .word 1247060695            # vrev8.v v5, v5

    # Multiplication

    # Do two 64x64 multiplications in one go to save some time
    # and simplify things.

    # A = a1a0 (t1, t0)
    # B = b1b0 (v5)
    # C = c1c0 (256 bit)
    # c1 = a1b1 + (a0b1)h + (a1b0)h
    # c0 = a0b0 + (a0b1)l + (a1b0)h

    # v1 = (a0b1)l,(a0b0)l
    .word 844292311   # vclmul.vx v1, v5, t0
    # v3 = (a0b1)h,(a0b0)h
    .word 911401431  # vclmulh.vx v3, v5, t0

    # v4 = (a1b1)l,(a1b0)l
    .word 844325463   # vclmul.vx v4, v5, t1
    # v2 = (a1b1)h,(a1b0)h
    .word 911434071   # vclmulh.vx v2, v5, t1

    # Is there a better way to do this?
    # Would need to swap the order of elements within a vector register.
    .word 976270039     # vslideup.vi v5, v3, 1
    .word 977318743     # vslideup.vi v6, v4, 1
    .word 1043378647   # vslidedown.vi v3, v3, 1
    .word 1044427351   # vslidedown.vi v4, v4, 1

    .word 1577103447              # vmv.v.i v0, 1
    # v2 += (a0b1)h
    .word 740393303   # vxor.vv v2, v2, v3, v0.t
    # v2 += (a1b1)l
    .word 740426071   # vxor.vv v2, v2, v4, v0.t

    .word 1577136215              # vmv.v.i v0, 2
    # v1 += (a0b0)h,0
    .word 739410135   # vxor.vv v1, v1, v5, v0.t
    # v1 += (a1b0)l,0
    .word 739442903   # vxor.vv v1, v1, v6, v0.t

    # Now the 256bit product should be stored in (v2,v1)
    # v1 = (a0b1)l + (a0b0)h + (a1b0)l, (a0b0)l
    # v2 = (a1b1)h, (a1b0)h + (a0b1)h + (a1b1)l

    # Reduction
    # Let C := A*B = c3,c2,c1,c0 = v2[1],v2[0],v1[1],v1[0]
    # This is a slight variation of the Gueron's Montgomery reduction.
    # The difference being the order of some operations has been changed,
    # to make a better use of vclmul(h) instructions.

    # First step:
    # c1 += (c0 * P)l
    # vmv.v.i v0, 2
    .word 940618199 # vslideup.vi v3, v1, 1, v0.t
    .word 809394647 # vclmul.vx v3, v3, t3, v0.t
    .word 739344599   # vxor.vv v1, v1, v3, v0.t

    # Second step:
    # D = d1,d0 is final result
    # We want:
    # m1 = c1 + (c1 * P)h
    # m0 = (c1 * P)l + (c0 * P)h + c0
    # d1 = c3 + m1
    # d0 = c2 + m0

    #v3 = (c1 * P)l, 0
    .word 807297495 # vclmul.vx v3, v1, t3, v0.t
    #v4 = (c1 * P)h, (c0 * P)h
    .word 907960919   # vclmulh.vx v4, v1, t3

    .word 1577103447              # vmv.v.i v0, 1
    .word 1043378647   # vslidedown.vi v3, v3, 1

    .word 772931799       # vxor.vv v1, v1, v4
    .word 739344599   # vxor.vv v1, v1, v3, v0.t

    # XOR in the upper upper part of the product
    .word 773882199       # vxor.vv v2, v2, v1

    .word 1243914967            # vrev8.v v2, v2

    bnez a3, Lstep

    .word 198537895    # vsse64.v v2, (a0), t4
    ret
.size gcm_ghash_rv64i_zvkb_zvbc,.-gcm_ghash_rv64i_zvkb_zvbc
.p2align 4
Lpolymod:
        .dword 0x0000000000000001
        .dword 0xc200000000000000
.size Lpolymod,.-Lpolymod
