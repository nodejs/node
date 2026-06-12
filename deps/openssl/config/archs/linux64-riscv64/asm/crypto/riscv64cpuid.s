################################################################################
# int CRYPTO_memcmp(const void * in_a, const void * in_b, size_t len)
################################################################################
.text
.balign 16
.globl CRYPTO_memcmp
.type   CRYPTO_memcmp,@function
CRYPTO_memcmp:
    li      t0,0
    beqz    a2,2f   # len == 0
1:
    lbu     t1,0(a0)
    lbu     t2,0(a1)
    addi    a0,a0,1
    addi    a1,a1,1
    addi    a2,a2,-1
    xor     t1,t1,t2
    or      t0,t0,t1
    bgtz    a2,1b
2:
    mv      a0,t0
    ret
################################################################################
# void OPENSSL_cleanse(void *ptr, size_t len)
################################################################################
.text
.balign 16
.globl OPENSSL_cleanse
.type   OPENSSL_cleanse,@function
OPENSSL_cleanse:
    beqz    a1,2f         # len == 0, return
    srli    t0,a1,4
    bnez    t0,3f       # len > 15

1:  # Store <= 15 individual bytes
    sb      x0,0(a0)
    addi    a0,a0,1
    addi    a1,a1,-1
    bnez    a1,1b
2:
    ret

3:  # Store individual bytes until we are aligned
    andi    t0,a0,0x7
    beqz    t0,4f
    sb      x0,0(a0)
    addi    a0,a0,1
    addi    a1,a1,-1
    j       3b

4:  # Store aligned dwords
    li      t1,8
4:
    sd      x0,0(a0)
    addi    a0,a0,8
    addi    a1,a1,-8
    bge     a1,t1,4b  # if len>=8 loop
    bnez    a1,1b         # if len<8 and len != 0, store remaining bytes
    ret
################################################################################
# size_t riscv_vlen_asm(void)
# Return VLEN (i.e. the length of a vector register in bits).
.p2align 3
.globl riscv_vlen_asm
.type riscv_vlen_asm,@function
riscv_vlen_asm:
    # 0xc22 is CSR vlenb
    csrr a0, 0xc22
    slli a0, a0, 3
    ret
.size riscv_vlen_asm,.-riscv_vlen_asm
