.text	
.globl	aesni_xts_128_encrypt_avx512
.globl	aesni_xts_128_decrypt_avx512

aesni_xts_128_encrypt_avx512:
aesni_xts_128_decrypt_avx512:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

.globl	aesni_xts_256_encrypt_avx512
.globl	aesni_xts_256_decrypt_avx512

aesni_xts_256_encrypt_avx512:
aesni_xts_256_decrypt_avx512:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

.globl	aesni_xts_avx512_eligible
.type	aesni_xts_avx512_eligible,@function
aesni_xts_avx512_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.size	aesni_xts_avx512_eligible, .-aesni_xts_avx512_eligible

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
