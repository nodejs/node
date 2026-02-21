.text	

.globl	_ossl_rsaz_avx512ifma_eligible

_ossl_rsaz_avx512ifma_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3


.globl	_ossl_rsaz_amm52x20_x1_ifma256
.globl	_ossl_rsaz_amm52x20_x2_ifma256
.globl	_ossl_extract_multiplier_2x20_win5

_ossl_rsaz_amm52x20_x1_ifma256:
_ossl_rsaz_amm52x20_x2_ifma256:
_ossl_extract_multiplier_2x20_win5:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

