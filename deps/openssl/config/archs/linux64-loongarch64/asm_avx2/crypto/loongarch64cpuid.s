################################################################################
# int CRYPTO_memcmp(const void * in_a, const void * in_b, size_t len)
################################################################################
.text
.balign 16
.globl CRYPTO_memcmp
.type   CRYPTO_memcmp,@function
CRYPTO_memcmp:
    li.d    $r12,0
    beqz    $r6,2f   # len == 0
1:
    ld.bu   $r13,$r4,0
    ld.bu   $r14,$r5,0
    addi.d  $r4,$r4,1
    addi.d  $r5,$r5,1
    addi.d  $r6,$r6,-1
    xor     $r13,$r13,$r14
    or      $r12,$r12,$r13
    blt     $r0,$r6,1b
2:
    move    $r4,$r12
    jr      $r1
################################################################################
# void OPENSSL_cleanse(void *ptr, size_t len)
################################################################################
.text
.balign 16
.globl OPENSSL_cleanse
.type   OPENSSL_cleanse,@function
OPENSSL_cleanse:
    beqz    $r5,2f         # len == 0, return
    srli.d  $r12,$r5,4
    bnez    $r12,3f       # len > 15

1:  # Store <= 15 individual bytes
    st.b    $r0,$r4,0
    addi.d  $r4,$r4,1
    addi.d  $r5,$r5,-1
    bnez    $r5,1b
2:
    jr      $r1

3:  # Store individual bytes until we are aligned
    andi    $r12,$r4,0x7
    beqz    $r12,4f
    st.b    $r0,$r4,0
    addi.d  $r4,$r4,1
    addi.d  $r5,$r5,-1
    b       3b

4:  # Store aligned dwords
    li.d    $r13,8
4:
    st.d    $r0,$r4,0
    addi.d  $r4,$r4,8
    addi.d  $r5,$r5,-8
    bge     $r5,$r13,4b  # if len>=8 loop
    bnez    $r5,1b         # if len<8 and len != 0, store remaining bytes
    jr      $r1
################################################################################
# uint32_t OPENSSL_rdtsc(void)
################################################################################
.text
.balign 16
.globl OPENSSL_rdtsc
.type   OPENSSL_rdtsc,@function
OPENSSL_rdtsc:
    rdtimel.w $r4,$r0
    jr        $r1
