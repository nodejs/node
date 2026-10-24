default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


global	ossl_aes_cbc_vaes_cleanup_eligible


ALIGN	16
ossl_aes_cbc_vaes_cleanup_eligible:

DB	243,15,30,250
	mov	eax,1
	DB	0F3h,0C3h		;repret









global	ossl_aes_cbc_vaes_cleanup


ALIGN	32
ossl_aes_cbc_vaes_cleanup:

DB	243,15,30,250
	vpxord	zmm0,zmm0,zmm0
	test	rdx,rdx
	jz	NEAR $L$vaes_cbc_clear_registers

$L$vaes_cbc_clear_keys:
	vmovdqu64	ZMMWORD[rcx],zmm0
	add	rcx,64
	dec	rdx
	jnz	NEAR $L$vaes_cbc_clear_keys

$L$vaes_cbc_clear_registers:


	vpxor	xmm0,xmm0,xmm0
	vpxor	xmm1,xmm1,xmm1
	vpxor	xmm2,xmm2,xmm2
	vpxor	xmm3,xmm3,xmm3
	vpxor	xmm4,xmm4,xmm4
	vpxor	xmm5,xmm5,xmm5
	vpxord	zmm16,zmm16,zmm16
	vpxord	zmm17,zmm17,zmm17
	vpxord	zmm18,zmm18,zmm18
	vpxord	zmm19,zmm19,zmm19
	vpxord	zmm20,zmm20,zmm20
	vpxord	zmm21,zmm21,zmm21
	vpxord	zmm22,zmm22,zmm22
	vpxord	zmm23,zmm23,zmm23
	vpxord	zmm24,zmm24,zmm24
	vpxord	zmm25,zmm25,zmm25
	vpxord	zmm26,zmm26,zmm26
	vpxord	zmm27,zmm27,zmm27
	vpxord	zmm28,zmm28,zmm28
	vpxord	zmm29,zmm29,zmm29
	vpxord	zmm30,zmm30,zmm30
	vpxord	zmm31,zmm31,zmm31

	vzeroupper


	xor	eax,eax
	xor	ecx,ecx
	xor	edx,edx
	xor	r8d,r8d
	xor	r9d,r9d
	xor	r10d,r10d
	xor	r11d,r11d
	DB	0F3h,0C3h		;repret


