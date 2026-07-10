.text
.p2align 3
.globl gcm_init_rv64i_zvkg
.type gcm_init_rv64i_zvkg,@function
gcm_init_rv64i_zvkg:
    ld      a2, 0(a1)
    ld      a3, 8(a1)
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
.size gcm_init_rv64i_zvkg,.-gcm_init_rv64i_zvkg
.p2align 3
.globl gcm_init_rv64i_zvkg_zvkb
.type gcm_init_rv64i_zvkg_zvkb,@function
gcm_init_rv64i_zvkg_zvkb:
    .word 0xc1817057 # vsetivli x0, 2, e64, m1, ta, ma
    .word 33943559             # vle64.v v0, (a1)
    .word 1241817175            # vrev8.v v0, v0
    .word 33910823        # vse64.v v0, (a0)
    ret
.size gcm_init_rv64i_zvkg_zvkb,.-gcm_init_rv64i_zvkg_zvkb
.p2align 3
.globl gcm_gmult_rv64i_zvkg
.type gcm_gmult_rv64i_zvkg,@function
gcm_gmult_rv64i_zvkg:
    .word 0xc1027057
    .word 33939719
    .word 33906823
    .word 2720571639
    .word 33906855
    ret
.size gcm_gmult_rv64i_zvkg,.-gcm_gmult_rv64i_zvkg
.p2align 3
.globl gcm_ghash_rv64i_zvkg
.type gcm_ghash_rv64i_zvkg,@function
gcm_ghash_rv64i_zvkg:
    .word 0xc1027057
    .word 33939719
    .word 33906823

Lstep:
    .word 33972615
    add a2, a2, 16
    add a3, a3, -16
    .word 2988548343
    bnez a3, Lstep

    .word 33906855
    ret

.size gcm_ghash_rv64i_zvkg,.-gcm_ghash_rv64i_zvkg
