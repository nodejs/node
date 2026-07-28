.text	
.globl	ossl_vaes_vpclmulqdq_capable
.type	ossl_vaes_vpclmulqdq_capable,@function
ossl_vaes_vpclmulqdq_capable:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
.size	ossl_vaes_vpclmulqdq_capable, .-ossl_vaes_vpclmulqdq_capable

.globl	ossl_aes_gcm_init_avx512
.globl	ossl_aes_gcm_setiv_avx512
.globl	ossl_aes_gcm_update_aad_avx512
.globl	ossl_aes_gcm_encrypt_avx512
.globl	ossl_aes_gcm_decrypt_avx512
.globl	ossl_aes_gcm_finalize_avx512
.globl	ossl_gcm_gmult_avx512

.type	ossl_aes_gcm_init_avx512,@function
ossl_aes_gcm_init_avx512:
ossl_aes_gcm_setiv_avx512:
ossl_aes_gcm_update_aad_avx512:
ossl_aes_gcm_encrypt_avx512:
ossl_aes_gcm_decrypt_avx512:
ossl_aes_gcm_finalize_avx512:
ossl_gcm_gmult_avx512:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3
.size	ossl_aes_gcm_init_avx512, .-ossl_aes_gcm_init_avx512
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
