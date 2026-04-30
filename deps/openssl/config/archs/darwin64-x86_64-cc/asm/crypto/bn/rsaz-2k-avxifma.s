.text	

.globl	_ossl_rsaz_avxifma_eligible

_ossl_rsaz_avxifma_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3


.globl	_ossl_rsaz_amm52x20_x1_avxifma256
.globl	_ossl_rsaz_amm52x20_x2_avxifma256
.globl	_ossl_extract_multiplier_2x20_win5_avx

_ossl_rsaz_amm52x20_x1_avxifma256:
_ossl_rsaz_amm52x20_x2_avxifma256:
_ossl_extract_multiplier_2x20_win5_avx:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

