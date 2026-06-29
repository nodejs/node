default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


global	ossl_rsaz_avxifma_eligible

ossl_rsaz_avxifma_eligible:
	xor	eax,eax
	DB	0F3h,0C3h		;repret


global	ossl_rsaz_amm52x20_x1_avxifma256
global	ossl_rsaz_amm52x20_x2_avxifma256
global	ossl_extract_multiplier_2x20_win5_avx

ossl_rsaz_amm52x20_x1_avxifma256:
ossl_rsaz_amm52x20_x2_avxifma256:
ossl_extract_multiplier_2x20_win5_avx:
DB	0x0f,0x0b
	DB	0F3h,0C3h		;repret

