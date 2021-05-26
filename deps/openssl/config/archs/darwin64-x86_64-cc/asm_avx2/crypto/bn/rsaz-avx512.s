.text	

.globl	_rsaz_avx512ifma_eligible

_rsaz_avx512ifma_eligible:
	xorl	%eax,%eax
	.byte	0xf3,0xc3


.globl	_RSAZ_amm52x20_x1_256
.globl	_RSAZ_amm52x20_x2_256
.globl	_extract_multiplier_2x20_win5

_RSAZ_amm52x20_x1_256:
_RSAZ_amm52x20_x2_256:
_extract_multiplier_2x20_win5:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3

