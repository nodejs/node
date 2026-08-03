default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


global	ossl_rsaz_amm52x40_x1_avxifma256
global	ossl_rsaz_amm52x40_x2_avxifma256
global	ossl_extract_multiplier_2x40_win5_avx

ossl_rsaz_amm52x40_x1_avxifma256:
ossl_rsaz_amm52x40_x2_avxifma256:
ossl_extract_multiplier_2x40_win5_avx:
DB	0x0f,0x0b
	DB	0F3h,0C3h		;repret

