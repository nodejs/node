.text	

.globl	_ossl_aes_cbc_vaes_cleanup_eligible
.private_extern	_ossl_aes_cbc_vaes_cleanup_eligible

.p2align	4
_ossl_aes_cbc_vaes_cleanup_eligible:

.byte	243,15,30,250
	xorl	%eax,%eax
	.byte	0xf3,0xc3



.globl	_ossl_aes_cbc_vaes_cleanup
.private_extern	_ossl_aes_cbc_vaes_cleanup

_ossl_aes_cbc_vaes_cleanup:

.byte	243,15,30,250
.byte	0x0f,0x0b
	.byte	0xf3,0xc3


