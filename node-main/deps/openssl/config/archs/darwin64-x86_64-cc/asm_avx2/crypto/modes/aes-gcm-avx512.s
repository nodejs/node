.text	
.globl	_ossl_vaes_vpclmulqdq_capable

_ossl_vaes_vpclmulqdq_capable:
	xorl	%eax,%eax
	.byte	0xf3,0xc3


.globl	_ossl_aes_gcm_init_avx512
.globl	_ossl_aes_gcm_setiv_avx512
.globl	_ossl_aes_gcm_update_aad_avx512
.globl	_ossl_aes_gcm_encrypt_avx512
.globl	_ossl_aes_gcm_decrypt_avx512
.globl	_ossl_aes_gcm_finalize_avx512
.globl	_ossl_gcm_gmult_avx512


_ossl_aes_gcm_init_avx512:
_ossl_aes_gcm_setiv_avx512:
_ossl_aes_gcm_update_aad_avx512:
_ossl_aes_gcm_encrypt_avx512:
_ossl_aes_gcm_decrypt_avx512:
_ossl_aes_gcm_finalize_avx512:
_ossl_gcm_gmult_avx512:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

