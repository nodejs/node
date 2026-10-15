.text	
.globl	ossl_aes_cfb128_vaes_enc
.globl	ossl_aes_cfb128_vaes_dec




ossl_aes_cfb128_vaes_enc:
ossl_aes_cfb128_vaes_dec:
.byte	0x0f,0x0b

	.byte	0xf3,0xc3











.globl	ossl_aes_cfb128_vaes_eligible
.type	ossl_aes_cfb128_vaes_eligible,@function
ossl_aes_cfb128_vaes_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.size	ossl_aes_cfb128_vaes_eligible, .-ossl_aes_cfb128_vaes_eligible
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
