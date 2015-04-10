.text

.globl	_aesni_gcm_encrypt

_aesni_gcm_encrypt:
	xorl	%eax,%eax
	.byte	0xf3,0xc3


.globl	_aesni_gcm_decrypt

_aesni_gcm_decrypt:
	xorl	%eax,%eax
	.byte	0xf3,0xc3
