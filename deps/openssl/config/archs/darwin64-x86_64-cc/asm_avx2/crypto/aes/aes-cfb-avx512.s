.text	
.globl	_ossl_aes_cfb128_vaes_enc
.globl	_ossl_aes_cfb128_vaes_dec




_ossl_aes_cfb128_vaes_enc:
_ossl_aes_cfb128_vaes_dec:
.byte	0x0f,0x0b

	.byte	0xf3,0xc3











.globl	_ossl_aes_cfb128_vaes_eligible

_ossl_aes_cfb128_vaes_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3

