.text	

.globl	SHA3_avx512vl_capable
.type	SHA3_avx512vl_capable,@function
SHA3_avx512vl_capable:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.size	SHA3_avx512vl_capable, .-SHA3_avx512vl_capable

.globl	SHA3_shake128_x4_inc_absorb_avx512vl
.globl	SHA3_shake256_x4_inc_absorb_avx512vl
.globl	SHA3_shake128_x4_inc_finalize_avx512vl
.globl	SHA3_shake256_x4_inc_finalize_avx512vl
.globl	SHA3_shake128_x4_inc_squeeze_avx512vl
.globl	SHA3_shake256_x4_inc_squeeze_avx512vl
.globl	SHA3_shake128_x4_avx512vl
.globl	SHA3_shake256_x4_avx512vl
.type	SHA3_shake128_x4_inc_absorb_avx512vl,@function
SHA3_shake128_x4_inc_absorb_avx512vl:
SHA3_shake256_x4_inc_absorb_avx512vl:
SHA3_shake128_x4_inc_finalize_avx512vl:
SHA3_shake256_x4_inc_finalize_avx512vl:
SHA3_shake128_x4_inc_squeeze_avx512vl:
SHA3_shake256_x4_inc_squeeze_avx512vl:
SHA3_shake128_x4_avx512vl:
SHA3_shake256_x4_avx512vl:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3
.size	SHA3_shake128_x4_inc_absorb_avx512vl, .-SHA3_shake128_x4_inc_absorb_avx512vl
	.section ".note.gnu.property", "a"
	.p2align 3
	.long 1f - 0f
	.long 4f - 1f
	.long 5
0:
	# "GNU" encoded with .byte, since .asciz isn't supported
	# on Solaris.
	.byte 0x47
	.byte 0x4e
	.byte 0x55
	.byte 0
1:
	.p2align 3
	.long 0xc0000002
	.long 3f - 2f
2:
	.long 3
3:
	.p2align 3
4:
