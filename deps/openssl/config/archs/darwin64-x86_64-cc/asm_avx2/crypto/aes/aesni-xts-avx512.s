.text	
.globl	_aesni_xts_128_encrypt_avx512
.globl	_aesni_xts_128_decrypt_avx512

_aesni_xts_128_encrypt_avx512:
_aesni_xts_128_decrypt_avx512:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

.globl	_aesni_xts_256_encrypt_avx512
.globl	_aesni_xts_256_decrypt_avx512

_aesni_xts_256_encrypt_avx512:
_aesni_xts_256_decrypt_avx512:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

.globl	_aesni_xts_avx512_eligible

_aesni_xts_avx512_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3


