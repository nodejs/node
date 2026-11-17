default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


EXTERN	OPENSSL_ia32cap_P

global	ml_dsa_ntt_avx2_capable

ALIGN	32
ml_dsa_ntt_avx2_capable:
	mov	rcx,QWORD[((OPENSSL_ia32cap_P+8))]
	xor	eax,eax
	and	ecx,32
	cmovnz	eax,ecx
	DB	0F3h,0C3h		;repret





section	.rdata rdata align=8




















ALIGN	64
zetas_inverse:
	DD	8380417 - 1976782,8380417 - 7534263,8380417 - 1400424,8380417 - 3937738,8380417 - 7018208,8380417 - 8332111,8380417 - 3919660,8380417 - 7826001
	DD	8380417 - 4834730,8380417 - 1612842,8380417 - 7403526,8380417 - 183443,8380417 - 6094090,8380417 - 7959518,8380417 - 6144432,8380417 - 5441381
	DD	8380417 - 4546524,8380417 - 8119771,8380417 - 7276084,8380417 - 6712985,8380417 - 1910376,8380417 - 6577327,8380417 - 1723600,8380417 - 7953734
	DD	8380417 - 472078,8380417 - 1717735,8380417 - 7404533,8380417 - 2213111,8380417 - 269760,8380417 - 3866901,8380417 - 3523897,8380417 - 5341501
	DD	8380417 - 6581310,8380417 - 4686184,8380417 - 1652634,8380417 - 810149,8380417 - 3014001,8380417 - 1616392,8380417 - 162844,8380417 - 5196991
	DD	8380417 - 7173032,8380417 - 185531,8380417 - 3369112,8380417 - 1957272,8380417 - 8215696,8380417 - 2454455,8380417 - 2432395,8380417 - 6366809
	DD	8380417 - 4603424,8380417 - 594136,8380417 - 4656147,8380417 - 5796124,8380417 - 6533464,8380417 - 6709241,8380417 - 5548557,8380417 - 7838005
	DD	8380417 - 3406031,8380417 - 2235880,8380417 - 777191,8380417 - 1500165,8380417 - 7005614,8380417 - 5834105,8380417 - 1917081,8380417 - 7100756
	DD	8380417 - 6417775,8380417 - 3306115,8380417 - 1312455,8380417 - 7929317,8380417 - 6950192,8380417 - 5062207,8380417 - 1237275,8380417 - 7047359
	DD	8380417 - 7329447,8380417 - 1903435,8380417 - 1869119,8380417 - 5386378,8380417 - 4832145,8380417 - 2635921,8380417 - 1250494,8380417 - 4613401
	DD	8380417 - 1595974,8380417 - 2486353,8380417 - 1247620,8380417 - 4055324,8380417 - 1265009,8380417 - 5790267,8380417 - 2691481,8380417 - 2842341
	DD	8380417 - 203044,8380417 - 1735879,8380417 - 5038140,8380417 - 3437287,8380417 - 4108315,8380417 - 5942594,8380417 - 286988,8380417 - 342297
	DD	8380417 - 4784579,8380417 - 7611795,8380417 - 7855319,8380417 - 4823422,8380417 - 3207046,8380417 - 2031748,8380417 - 5257975,8380417 - 7725090
	DD	8380417 - 7857917,8380417 - 8337157,8380417 - 6767243,8380417 - 495491,8380417 - 819034,8380417 - 909542,8380417 - 1859098,8380417 - 900702
	DD	8380417 - 5187039,8380417 - 7183191,8380417 - 4621053,8380417 - 4860065,8380417 - 3513181,8380417 - 7144689,8380417 - 2434439,8380417 - 266997
	DD	8380417 - 4817955,8380417 - 5933984,8380417 - 2244091,8380417 - 5037939,8380417 - 3817976,8380417 - 2316500,8380417 - 3407706,8380417 - 2091667
	DD	8380417 - 3839961,8380417 - 4751448,8380417 - 4499357,8380417 - 5361315,8380417 - 6940675,8380417 - 7567685,8380417 - 6795489,8380417 - 1285669
	DD	8380417 - 1341330,8380417 - 1315589,8380417 - 8202977,8380417 - 5971092,8380417 - 6529015,8380417 - 3159746,8380417 - 4827145,8380417 - 189548
	DD	8380417 - 7063561,8380417 - 759969,8380417 - 8169440,8380417 - 2389356,8380417 - 5130689,8380417 - 1653064,8380417 - 8371839,8380417 - 4656075
	DD	8380417 - 3958618,8380417 - 904516,8380417 - 7280319,8380417 - 44288,8380417 - 3097992,8380417 - 508951,8380417 - 264944,8380417 - 5037034
	DD	8380417 - 6949987,8380417 - 1852771,8380417 - 1349076,8380417 - 7998430,8380417 - 7072248,8380417 - 8357436,8380417 - 7151892,8380417 - 7709315
	DD	8380417 - 5903370,8380417 - 7969390,8380417 - 4686924,8380417 - 5412772,8380417 - 2715295,8380417 - 2147896,8380417 - 7396998,8380417 - 3412210
	DD	8380417 - 126922,8380417 - 4747489,8380417 - 5223087,8380417 - 5190273,8380417 - 7380215,8380417 - 4296819,8380417 - 1939314,8380417 - 7122806
	DD	8380417 - 6795196,8380417 - 2176455,8380417 - 3475950,8380417 - 6927966,8380417 - 5339162,8380417 - 4702672,8380417 - 6851714,8380417 - 4450022
	DD	8380417 - 5582638,8380417 - 2071892,8380417 - 5823537,8380417 - 3900724,8380417 - 3881043,8380417 - 954230,8380417 - 531354,8380417 - 811944
	DD	8380417 - 3699596,8380417 - 6779997,8380417 - 6239768,8380417 - 3507263,8380417 - 4558682,8380417 - 3505694,8380417 - 6736599,8380417 - 6681150
	DD	8380417 - 7841118,8380417 - 2348700,8380417 - 8079950,8380417 - 3539968,8380417 - 5512770,8380417 - 3574422,8380417 - 5336701,8380417 - 4519302
	DD	8380417 - 3915439,8380417 - 5842901,8380417 - 4788269,8380417 - 6718724,8380417 - 3530437,8380417 - 3077325,8380417 - 95776,8380417 - 2706023
	DD	8380417 - 280005,8380417 - 4010497,8380417 - 8360995,8380417 - 1757237,8380417 - 5102745,8380417 - 6980856,8380417 - 4520680,8380417 - 6262231
	DD	8380417 - 6271868,8380417 - 2619752,8380417 - 7260833,8380417 - 7830929,8380417 - 3585928,8380417 - 7300517,8380417 - 1024112,8380417 - 2725464
	DD	8380417 - 2680103,8380417 - 3111497,8380417 - 5495562,8380417 - 3119733,8380417 - 6288512,8380417 - 8021166,8380417 - 2353451,8380417 - 1826347
	DD	8380417 - 466468,8380417 - 7504169,8380417 - 7602457,8380417 - 237124,8380417 - 7861508,8380417 - 5771523,8380417 - 25847,8380417 - 4193792

ALIGN	32
idx_even:
	DD	0,2,4,6,0,2,4,6

ALIGN	32
idx_odd:
	DD	1,3,5,7,1,3,5,7


ALIGN	8
ml_dsa_q:
	DQ	8380417


ALIGN	8
ml_dsa_q_neg_inv:
	DQ	4236238847


ALIGN	8
ml_dsa_inverse_degree_montgomery:
	DQ	41978





section	.text code align=64





































global	ml_dsa_poly_ntt_mult_avx2

ALIGN	32
ml_dsa_poly_ntt_mult_avx2:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ml_dsa_poly_ntt_mult_avx2:
	mov	rdi,rcx
	mov	rsi,rdx
	mov	rdx,r8



	lea	rsp,[((-168))+rax]
	vmovaps	XMMWORD[rsp],xmm6
	vmovaps	XMMWORD[16+rsp],xmm7
	vmovaps	XMMWORD[32+rsp],xmm8
	vmovaps	XMMWORD[48+rsp],xmm9
	vmovaps	XMMWORD[64+rsp],xmm10
	vmovaps	XMMWORD[80+rsp],xmm11
	vmovaps	XMMWORD[96+rsp],xmm12
	vmovaps	XMMWORD[112+rsp],xmm13
	vmovaps	XMMWORD[128+rsp],xmm14
	vmovaps	XMMWORD[144+rsp],xmm15
$L$ntt_mult_body:
	vpbroadcastq	ymm14,QWORD[ml_dsa_q_neg_inv]
	vpbroadcastd	ymm15,DWORD[ml_dsa_q]
	xor	r10d,r10d

ALIGN	32
$L$mult_loop:

	vmovdqu	ymm0,YMMWORD[r10*1+rdi]
	vmovdqu	ymm1,YMMWORD[r10*1+rsi]



	vpmuludq	ymm8,ymm1,ymm0
	vmovshdup	ymm9,ymm0
	vmovshdup	ymm10,ymm1
	vpmuludq	ymm9,ymm10,ymm9

	vpmuludq	ymm0,ymm8,ymm14
	vpmuludq	ymm10,ymm9,ymm14


	vpmuludq	ymm0,ymm15,ymm0
	vpmuludq	ymm10,ymm15,ymm10


	vpaddq	ymm0,ymm8,ymm0
	vpaddq	ymm9,ymm9,ymm10


	vmovshdup	ymm0,ymm0
	vpblendd	ymm0,ymm0,ymm9,0xAA


	vpcmpgtd	ymm8,ymm15,ymm0
	vpandn	ymm8,ymm8,ymm15
	vpsubd	ymm0,ymm0,ymm8

	vmovdqu	YMMWORD[r10*1+rdx],ymm0


	add	r10d,8*4
	cmp	r10d,256*4
	jb	NEAR $L$mult_loop


	vzeroall
	vmovaps	xmm6,XMMWORD[rsp]
	vmovaps	xmm7,XMMWORD[16+rsp]
	vmovaps	xmm8,XMMWORD[32+rsp]
	vmovaps	xmm9,XMMWORD[48+rsp]
	vmovaps	xmm10,XMMWORD[64+rsp]
	vmovaps	xmm11,XMMWORD[80+rsp]
	vmovaps	xmm12,XMMWORD[96+rsp]
	vmovaps	xmm13,XMMWORD[112+rsp]
	vmovaps	xmm14,XMMWORD[128+rsp]
	vmovaps	xmm15,XMMWORD[144+rsp]
	lea	rsp,[rax]
$L$ntt_mult_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ml_dsa_poly_ntt_mult_avx2:






























global	ml_dsa_poly_ntt_avx2

ALIGN	32
ml_dsa_poly_ntt_avx2:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ml_dsa_poly_ntt_avx2:
	mov	rdi,rcx
	mov	rsi,rdx



	lea	rsp,[((-168))+rax]
	vmovaps	XMMWORD[rsp],xmm6
	vmovaps	XMMWORD[16+rsp],xmm7
	vmovaps	XMMWORD[32+rsp],xmm8
	vmovaps	XMMWORD[48+rsp],xmm9
	vmovaps	XMMWORD[64+rsp],xmm10
	vmovaps	XMMWORD[80+rsp],xmm11
	vmovaps	XMMWORD[96+rsp],xmm12
	vmovaps	XMMWORD[112+rsp],xmm13
	vmovaps	XMMWORD[128+rsp],xmm14
	vmovaps	XMMWORD[144+rsp],xmm15
$L$ntt_body:


	mov	r11,rsi


	vpbroadcastq	ymm14,QWORD[ml_dsa_q_neg_inv]
	vpbroadcastd	ymm15,DWORD[ml_dsa_q]






	vmovdqu	ymm0,YMMWORD[((0+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((0+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((0+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((0+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((0+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((0+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((0+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((0+896))+rdi]





	vpbroadcastd	ymm13,DWORD[4+r11]

	vpmuludq	ymm9,ymm13,ymm4
	vmovshdup	ymm10,ymm4
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm4,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm4
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm4,ymm4,ymm9

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm5,ymm5,ymm9

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm3,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm3,ymm8,ymm3



	vpcmpgtd	ymm8,ymm15,ymm3
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm3,ymm3,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[8+r11]

	vpmuludq	ymm9,ymm13,ymm2
	vmovshdup	ymm10,ymm2
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm2,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm2
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm2,ymm2,ymm9

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[12+r11]

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm5,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm5,ymm8,ymm5



	vpcmpgtd	ymm8,ymm15,ymm5
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm5,ymm5,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[16+r11]

	vpmuludq	ymm9,ymm13,ymm1
	vmovshdup	ymm10,ymm1
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm1,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm1
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm1,ymm1,ymm9
	vpbroadcastd	ymm13,DWORD[20+r11]

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[24+r11]

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm5,ymm5,ymm9
	vpbroadcastd	ymm13,DWORD[28+r11]

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm6,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm6,ymm8,ymm6



	vpcmpgtd	ymm8,ymm15,ymm6
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm6,ymm6,ymm8
	vpsubd	ymm7,ymm7,ymm9

	vmovdqu	YMMWORD[(0+0)+rdi],ymm0
	vmovdqu	YMMWORD[(0+128)+rdi],ymm1
	vmovdqu	YMMWORD[(0+256)+rdi],ymm2
	vmovdqu	YMMWORD[(0+384)+rdi],ymm3
	vmovdqu	YMMWORD[(0+512)+rdi],ymm4
	vmovdqu	YMMWORD[(0+640)+rdi],ymm5
	vmovdqu	YMMWORD[(0+768)+rdi],ymm6
	vmovdqu	YMMWORD[(0+896)+rdi],ymm7
	vmovdqu	ymm0,YMMWORD[((32+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((32+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((32+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((32+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((32+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((32+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((32+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((32+896))+rdi]





	vpbroadcastd	ymm13,DWORD[4+r11]

	vpmuludq	ymm9,ymm13,ymm4
	vmovshdup	ymm10,ymm4
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm4,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm4
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm4,ymm4,ymm9

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm5,ymm5,ymm9

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm3,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm3,ymm8,ymm3



	vpcmpgtd	ymm8,ymm15,ymm3
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm3,ymm3,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[8+r11]

	vpmuludq	ymm9,ymm13,ymm2
	vmovshdup	ymm10,ymm2
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm2,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm2
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm2,ymm2,ymm9

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[12+r11]

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm5,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm5,ymm8,ymm5



	vpcmpgtd	ymm8,ymm15,ymm5
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm5,ymm5,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[16+r11]

	vpmuludq	ymm9,ymm13,ymm1
	vmovshdup	ymm10,ymm1
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm1,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm1
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm1,ymm1,ymm9
	vpbroadcastd	ymm13,DWORD[20+r11]

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[24+r11]

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm5,ymm5,ymm9
	vpbroadcastd	ymm13,DWORD[28+r11]

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm6,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm6,ymm8,ymm6



	vpcmpgtd	ymm8,ymm15,ymm6
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm6,ymm6,ymm8
	vpsubd	ymm7,ymm7,ymm9

	vmovdqu	YMMWORD[(32+0)+rdi],ymm0
	vmovdqu	YMMWORD[(32+128)+rdi],ymm1
	vmovdqu	YMMWORD[(32+256)+rdi],ymm2
	vmovdqu	YMMWORD[(32+384)+rdi],ymm3
	vmovdqu	YMMWORD[(32+512)+rdi],ymm4
	vmovdqu	YMMWORD[(32+640)+rdi],ymm5
	vmovdqu	YMMWORD[(32+768)+rdi],ymm6
	vmovdqu	YMMWORD[(32+896)+rdi],ymm7
	vmovdqu	ymm0,YMMWORD[((64+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((64+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((64+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((64+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((64+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((64+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((64+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((64+896))+rdi]





	vpbroadcastd	ymm13,DWORD[4+r11]

	vpmuludq	ymm9,ymm13,ymm4
	vmovshdup	ymm10,ymm4
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm4,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm4
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm4,ymm4,ymm9

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm5,ymm5,ymm9

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm3,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm3,ymm8,ymm3



	vpcmpgtd	ymm8,ymm15,ymm3
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm3,ymm3,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[8+r11]

	vpmuludq	ymm9,ymm13,ymm2
	vmovshdup	ymm10,ymm2
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm2,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm2
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm2,ymm2,ymm9

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[12+r11]

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm5,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm5,ymm8,ymm5



	vpcmpgtd	ymm8,ymm15,ymm5
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm5,ymm5,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[16+r11]

	vpmuludq	ymm9,ymm13,ymm1
	vmovshdup	ymm10,ymm1
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm1,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm1
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm1,ymm1,ymm9
	vpbroadcastd	ymm13,DWORD[20+r11]

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[24+r11]

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm5,ymm5,ymm9
	vpbroadcastd	ymm13,DWORD[28+r11]

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm6,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm6,ymm8,ymm6



	vpcmpgtd	ymm8,ymm15,ymm6
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm6,ymm6,ymm8
	vpsubd	ymm7,ymm7,ymm9

	vmovdqu	YMMWORD[(64+0)+rdi],ymm0
	vmovdqu	YMMWORD[(64+128)+rdi],ymm1
	vmovdqu	YMMWORD[(64+256)+rdi],ymm2
	vmovdqu	YMMWORD[(64+384)+rdi],ymm3
	vmovdqu	YMMWORD[(64+512)+rdi],ymm4
	vmovdqu	YMMWORD[(64+640)+rdi],ymm5
	vmovdqu	YMMWORD[(64+768)+rdi],ymm6
	vmovdqu	YMMWORD[(64+896)+rdi],ymm7
	vmovdqu	ymm0,YMMWORD[((96+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((96+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((96+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((96+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((96+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((96+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((96+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((96+896))+rdi]





	vpbroadcastd	ymm13,DWORD[4+r11]

	vpmuludq	ymm9,ymm13,ymm4
	vmovshdup	ymm10,ymm4
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm4,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm4
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm4,ymm4,ymm9

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm5,ymm5,ymm9

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm3,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm3,ymm8,ymm3



	vpcmpgtd	ymm8,ymm15,ymm3
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm3,ymm3,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[8+r11]

	vpmuludq	ymm9,ymm13,ymm2
	vmovshdup	ymm10,ymm2
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm2,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm2
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm2,ymm2,ymm9

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm1,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm1,ymm8,ymm1



	vpcmpgtd	ymm8,ymm15,ymm1
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm1,ymm1,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[12+r11]

	vpmuludq	ymm9,ymm13,ymm6
	vmovshdup	ymm10,ymm6
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm6,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm6
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm6,ymm6,ymm9

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm5,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm5,ymm8,ymm5



	vpcmpgtd	ymm8,ymm15,ymm5
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm5,ymm5,ymm8
	vpsubd	ymm7,ymm7,ymm9





	vpbroadcastd	ymm13,DWORD[16+r11]

	vpmuludq	ymm9,ymm13,ymm1
	vmovshdup	ymm10,ymm1
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm0,ymm15
	vpsubd	ymm1,ymm9,ymm8


	vpaddd	ymm0,ymm8,ymm0



	vpcmpgtd	ymm8,ymm15,ymm0
	vpcmpgtd	ymm9,ymm15,ymm1
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm0,ymm0,ymm8
	vpsubd	ymm1,ymm1,ymm9
	vpbroadcastd	ymm13,DWORD[20+r11]

	vpmuludq	ymm9,ymm13,ymm3
	vmovshdup	ymm10,ymm3
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm2,ymm15
	vpsubd	ymm3,ymm9,ymm8


	vpaddd	ymm2,ymm8,ymm2



	vpcmpgtd	ymm8,ymm15,ymm2
	vpcmpgtd	ymm9,ymm15,ymm3
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm2,ymm2,ymm8
	vpsubd	ymm3,ymm3,ymm9
	vpbroadcastd	ymm13,DWORD[24+r11]

	vpmuludq	ymm9,ymm13,ymm5
	vmovshdup	ymm10,ymm5
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm4,ymm15
	vpsubd	ymm5,ymm9,ymm8


	vpaddd	ymm4,ymm8,ymm4



	vpcmpgtd	ymm8,ymm15,ymm4
	vpcmpgtd	ymm9,ymm15,ymm5
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm4,ymm4,ymm8
	vpsubd	ymm5,ymm5,ymm9
	vpbroadcastd	ymm13,DWORD[28+r11]

	vpmuludq	ymm9,ymm13,ymm7
	vmovshdup	ymm10,ymm7
	vpmuludq	ymm10,ymm13,ymm10

	vpmuludq	ymm8,ymm9,ymm14
	vpmuludq	ymm11,ymm10,ymm14


	vpmuludq	ymm8,ymm15,ymm8
	vpmuludq	ymm11,ymm15,ymm11


	vpaddq	ymm8,ymm9,ymm8
	vpaddq	ymm10,ymm10,ymm11


	vmovshdup	ymm8,ymm8
	vpblendd	ymm8,ymm8,ymm10,0xAA


	vpcmpgtd	ymm9,ymm15,ymm8
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm8,ymm8,ymm9




	vpaddd	ymm9,ymm6,ymm15
	vpsubd	ymm7,ymm9,ymm8


	vpaddd	ymm6,ymm8,ymm6



	vpcmpgtd	ymm8,ymm15,ymm6
	vpcmpgtd	ymm9,ymm15,ymm7
	vpandn	ymm8,ymm8,ymm15
	vpandn	ymm9,ymm9,ymm15
	vpsubd	ymm6,ymm6,ymm8
	vpsubd	ymm7,ymm7,ymm9

	vmovdqu	YMMWORD[(96+0)+rdi],ymm0
	vmovdqu	YMMWORD[(96+128)+rdi],ymm1
	vmovdqu	YMMWORD[(96+256)+rdi],ymm2
	vmovdqu	YMMWORD[(96+384)+rdi],ymm3
	vmovdqu	YMMWORD[(96+512)+rdi],ymm4
	vmovdqu	YMMWORD[(96+640)+rdi],ymm5
	vmovdqu	YMMWORD[(96+768)+rdi],ymm6
	vmovdqu	YMMWORD[(96+896)+rdi],ymm7


















	vpbroadcastd	ymm13,DWORD[32+r11]


	vmovdqu	ymm0,YMMWORD[rdi]
	vmovdqu	ymm1,YMMWORD[((0+32))+rdi]
	vmovdqu	ymm2,YMMWORD[((0+64))+rdi]
	vmovdqu	ymm3,YMMWORD[((0+96))+rdi]

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm2,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm1,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm1,ymm9,ymm1



	vpcmpgtd	ymm9,ymm15,ymm1
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((32+4))+r11]


	vmovdqu	ymm4,YMMWORD[((0+128))+rdi]
	vmovdqu	ymm5,YMMWORD[((0+160))+rdi]
	vmovdqu	ymm6,YMMWORD[((0+192))+rdi]
	vmovdqu	ymm7,YMMWORD[((0+224))+rdi]

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm6,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm5,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm5,ymm9,ymm5



	vpcmpgtd	ymm9,ymm15,ymm5
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm9
	vpsubd	ymm7,ymm7,ymm10






	vpbroadcastd	ymm13,DWORD[64+r11]














	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vpbroadcastd	ymm13,DWORD[((64+4))+r11]

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((64+8))+r11]

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vpbroadcastd	ymm13,DWORD[((64+12))+r11]

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[128+r11]
	vpbroadcastd	ymm12,DWORD[((128+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((128+8))+r11]
	vpbroadcastd	ymm12,DWORD[((128+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((128+16))+r11]
	vpbroadcastd	ymm12,DWORD[((128+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((128+24))+r11]
	vpbroadcastd	ymm12,DWORD[((128+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10
















	vmovdqu	xmm13,XMMWORD[256+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm0,ymm1
	vpunpckhqdq	ymm1,ymm0,ymm1

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	xmm13,XMMWORD[((256+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm2,ymm3
	vpunpckhqdq	ymm3,ymm2,ymm3

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	xmm13,XMMWORD[((256+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm4,ymm5
	vpunpckhqdq	ymm5,ymm4,ymm5

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	xmm13,XMMWORD[((256+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm6,ymm7
	vpunpckhqdq	ymm7,ymm6,ymm7

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vshufps	ymm1,ymm8,ymm9,0xEE
	vshufps	ymm0,ymm8,ymm9,0x44


	vmovdqu	ymm13,YMMWORD[512+r11]

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	ymm13,YMMWORD[((512+32))+r11]


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vshufps	ymm3,ymm8,ymm9,0xEE
	vshufps	ymm2,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	ymm13,YMMWORD[((512+64))+r11]


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vshufps	ymm5,ymm8,ymm9,0xEE
	vshufps	ymm4,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	ymm13,YMMWORD[((512+96))+r11]


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vshufps	ymm7,ymm8,ymm9,0xEE
	vshufps	ymm6,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10


	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[rdi],ymm10
	vmovdqu	YMMWORD[(0+32)+rdi],ymm11

	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(0+64)+rdi],ymm10
	vmovdqu	YMMWORD[(0+96)+rdi],ymm11


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(0+128)+rdi],ymm10
	vmovdqu	YMMWORD[(0+160)+rdi],ymm11

	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(0+192)+rdi],ymm10
	vmovdqu	YMMWORD[(0+224)+rdi],ymm11





	vpbroadcastd	ymm13,DWORD[40+r11]


	vmovdqu	ymm0,YMMWORD[256+rdi]
	vmovdqu	ymm1,YMMWORD[((256+32))+rdi]
	vmovdqu	ymm2,YMMWORD[((256+64))+rdi]
	vmovdqu	ymm3,YMMWORD[((256+96))+rdi]

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm2,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm1,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm1,ymm9,ymm1



	vpcmpgtd	ymm9,ymm15,ymm1
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((40+4))+r11]


	vmovdqu	ymm4,YMMWORD[((256+128))+rdi]
	vmovdqu	ymm5,YMMWORD[((256+160))+rdi]
	vmovdqu	ymm6,YMMWORD[((256+192))+rdi]
	vmovdqu	ymm7,YMMWORD[((256+224))+rdi]

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm6,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm5,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm5,ymm9,ymm5



	vpcmpgtd	ymm9,ymm15,ymm5
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm9
	vpsubd	ymm7,ymm7,ymm10






	vpbroadcastd	ymm13,DWORD[80+r11]














	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vpbroadcastd	ymm13,DWORD[((80+4))+r11]

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((80+8))+r11]

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vpbroadcastd	ymm13,DWORD[((80+12))+r11]

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[160+r11]
	vpbroadcastd	ymm12,DWORD[((160+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((160+8))+r11]
	vpbroadcastd	ymm12,DWORD[((160+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((160+16))+r11]
	vpbroadcastd	ymm12,DWORD[((160+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((160+24))+r11]
	vpbroadcastd	ymm12,DWORD[((160+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10
















	vmovdqu	xmm13,XMMWORD[320+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm0,ymm1
	vpunpckhqdq	ymm1,ymm0,ymm1

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	xmm13,XMMWORD[((320+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm2,ymm3
	vpunpckhqdq	ymm3,ymm2,ymm3

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	xmm13,XMMWORD[((320+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm4,ymm5
	vpunpckhqdq	ymm5,ymm4,ymm5

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	xmm13,XMMWORD[((320+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm6,ymm7
	vpunpckhqdq	ymm7,ymm6,ymm7

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vshufps	ymm1,ymm8,ymm9,0xEE
	vshufps	ymm0,ymm8,ymm9,0x44


	vmovdqu	ymm13,YMMWORD[640+r11]

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	ymm13,YMMWORD[((640+32))+r11]


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vshufps	ymm3,ymm8,ymm9,0xEE
	vshufps	ymm2,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	ymm13,YMMWORD[((640+64))+r11]


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vshufps	ymm5,ymm8,ymm9,0xEE
	vshufps	ymm4,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	ymm13,YMMWORD[((640+96))+r11]


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vshufps	ymm7,ymm8,ymm9,0xEE
	vshufps	ymm6,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10


	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[256+rdi],ymm10
	vmovdqu	YMMWORD[(256+32)+rdi],ymm11

	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(256+64)+rdi],ymm10
	vmovdqu	YMMWORD[(256+96)+rdi],ymm11


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(256+128)+rdi],ymm10
	vmovdqu	YMMWORD[(256+160)+rdi],ymm11

	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(256+192)+rdi],ymm10
	vmovdqu	YMMWORD[(256+224)+rdi],ymm11





	vpbroadcastd	ymm13,DWORD[48+r11]


	vmovdqu	ymm0,YMMWORD[512+rdi]
	vmovdqu	ymm1,YMMWORD[((512+32))+rdi]
	vmovdqu	ymm2,YMMWORD[((512+64))+rdi]
	vmovdqu	ymm3,YMMWORD[((512+96))+rdi]

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm2,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm1,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm1,ymm9,ymm1



	vpcmpgtd	ymm9,ymm15,ymm1
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((48+4))+r11]


	vmovdqu	ymm4,YMMWORD[((512+128))+rdi]
	vmovdqu	ymm5,YMMWORD[((512+160))+rdi]
	vmovdqu	ymm6,YMMWORD[((512+192))+rdi]
	vmovdqu	ymm7,YMMWORD[((512+224))+rdi]

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm6,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm5,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm5,ymm9,ymm5



	vpcmpgtd	ymm9,ymm15,ymm5
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm9
	vpsubd	ymm7,ymm7,ymm10






	vpbroadcastd	ymm13,DWORD[96+r11]














	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vpbroadcastd	ymm13,DWORD[((96+4))+r11]

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((96+8))+r11]

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vpbroadcastd	ymm13,DWORD[((96+12))+r11]

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[192+r11]
	vpbroadcastd	ymm12,DWORD[((192+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((192+8))+r11]
	vpbroadcastd	ymm12,DWORD[((192+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((192+16))+r11]
	vpbroadcastd	ymm12,DWORD[((192+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((192+24))+r11]
	vpbroadcastd	ymm12,DWORD[((192+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10
















	vmovdqu	xmm13,XMMWORD[384+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm0,ymm1
	vpunpckhqdq	ymm1,ymm0,ymm1

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	xmm13,XMMWORD[((384+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm2,ymm3
	vpunpckhqdq	ymm3,ymm2,ymm3

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	xmm13,XMMWORD[((384+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm4,ymm5
	vpunpckhqdq	ymm5,ymm4,ymm5

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	xmm13,XMMWORD[((384+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm6,ymm7
	vpunpckhqdq	ymm7,ymm6,ymm7

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vshufps	ymm1,ymm8,ymm9,0xEE
	vshufps	ymm0,ymm8,ymm9,0x44


	vmovdqu	ymm13,YMMWORD[768+r11]

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	ymm13,YMMWORD[((768+32))+r11]


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vshufps	ymm3,ymm8,ymm9,0xEE
	vshufps	ymm2,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	ymm13,YMMWORD[((768+64))+r11]


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vshufps	ymm5,ymm8,ymm9,0xEE
	vshufps	ymm4,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	ymm13,YMMWORD[((768+96))+r11]


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vshufps	ymm7,ymm8,ymm9,0xEE
	vshufps	ymm6,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10


	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[512+rdi],ymm10
	vmovdqu	YMMWORD[(512+32)+rdi],ymm11

	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(512+64)+rdi],ymm10
	vmovdqu	YMMWORD[(512+96)+rdi],ymm11


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(512+128)+rdi],ymm10
	vmovdqu	YMMWORD[(512+160)+rdi],ymm11

	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(512+192)+rdi],ymm10
	vmovdqu	YMMWORD[(512+224)+rdi],ymm11





	vpbroadcastd	ymm13,DWORD[56+r11]


	vmovdqu	ymm0,YMMWORD[768+rdi]
	vmovdqu	ymm1,YMMWORD[((768+32))+rdi]
	vmovdqu	ymm2,YMMWORD[((768+64))+rdi]
	vmovdqu	ymm3,YMMWORD[((768+96))+rdi]

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm2,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm1,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm1,ymm9,ymm1



	vpcmpgtd	ymm9,ymm15,ymm1
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((56+4))+r11]


	vmovdqu	ymm4,YMMWORD[((768+128))+rdi]
	vmovdqu	ymm5,YMMWORD[((768+160))+rdi]
	vmovdqu	ymm6,YMMWORD[((768+192))+rdi]
	vmovdqu	ymm7,YMMWORD[((768+224))+rdi]

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm6,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm5,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm5,ymm9,ymm5



	vpcmpgtd	ymm9,ymm15,ymm5
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm9
	vpsubd	ymm7,ymm7,ymm10






	vpbroadcastd	ymm13,DWORD[112+r11]














	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vpbroadcastd	ymm13,DWORD[((112+4))+r11]

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((112+8))+r11]

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vpbroadcastd	ymm13,DWORD[((112+12))+r11]

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[224+r11]
	vpbroadcastd	ymm12,DWORD[((224+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((224+8))+r11]
	vpbroadcastd	ymm12,DWORD[((224+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10

	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((224+16))+r11]
	vpbroadcastd	ymm12,DWORD[((224+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((224+24))+r11]
	vpbroadcastd	ymm12,DWORD[((224+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10
















	vmovdqu	xmm13,XMMWORD[448+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm0,ymm1
	vpunpckhqdq	ymm1,ymm0,ymm1

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	xmm13,XMMWORD[((448+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm2,ymm3
	vpunpckhqdq	ymm3,ymm2,ymm3

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	xmm13,XMMWORD[((448+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm4,ymm5
	vpunpckhqdq	ymm5,ymm4,ymm5

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	xmm13,XMMWORD[((448+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpunpcklqdq	ymm8,ymm6,ymm7
	vpunpckhqdq	ymm7,ymm6,ymm7

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm8,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm8



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10












	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vshufps	ymm1,ymm8,ymm9,0xEE
	vshufps	ymm0,ymm8,ymm9,0x44


	vmovdqu	ymm13,YMMWORD[896+r11]

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm0,ymm15
	vpsubd	ymm1,ymm10,ymm9


	vpaddd	ymm0,ymm9,ymm0



	vpcmpgtd	ymm9,ymm15,ymm0
	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm9
	vpsubd	ymm1,ymm1,ymm10

	vmovdqu	ymm13,YMMWORD[((896+32))+r11]


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vshufps	ymm3,ymm8,ymm9,0xEE
	vshufps	ymm2,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm2,ymm15
	vpsubd	ymm3,ymm10,ymm9


	vpaddd	ymm2,ymm9,ymm2



	vpcmpgtd	ymm9,ymm15,ymm2
	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm9
	vpsubd	ymm3,ymm3,ymm10


	vmovdqu	ymm13,YMMWORD[((896+64))+r11]


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vshufps	ymm5,ymm8,ymm9,0xEE
	vshufps	ymm4,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm4,ymm15
	vpsubd	ymm5,ymm10,ymm9


	vpaddd	ymm4,ymm9,ymm4



	vpcmpgtd	ymm9,ymm15,ymm4
	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm9
	vpsubd	ymm5,ymm5,ymm10

	vmovdqu	ymm13,YMMWORD[((896+96))+r11]


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vshufps	ymm7,ymm8,ymm9,0xEE
	vshufps	ymm6,ymm8,ymm9,0x44

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm9,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm9,ymm15,ymm9
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm9,ymm10,ymm9
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm9,ymm9
	vpblendd	ymm9,ymm9,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm9
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm9,ymm9,ymm10




	vpaddd	ymm10,ymm6,ymm15
	vpsubd	ymm7,ymm10,ymm9


	vpaddd	ymm6,ymm9,ymm6



	vpcmpgtd	ymm9,ymm15,ymm6
	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm9,ymm9,ymm15
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm9
	vpsubd	ymm7,ymm7,ymm10


	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[768+rdi],ymm10
	vmovdqu	YMMWORD[(768+32)+rdi],ymm11

	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(768+64)+rdi],ymm10
	vmovdqu	YMMWORD[(768+96)+rdi],ymm11


	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(768+128)+rdi],ymm10
	vmovdqu	YMMWORD[(768+160)+rdi],ymm11

	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7

	vperm2i128	ymm10,ymm8,ymm9,0x20
	vperm2i128	ymm11,ymm8,ymm9,0x31
	vmovdqu	YMMWORD[(768+192)+rdi],ymm10
	vmovdqu	YMMWORD[(768+224)+rdi],ymm11


	vzeroall
	vmovaps	xmm6,XMMWORD[rsp]
	vmovaps	xmm7,XMMWORD[16+rsp]
	vmovaps	xmm8,XMMWORD[32+rsp]
	vmovaps	xmm9,XMMWORD[48+rsp]
	vmovaps	xmm10,XMMWORD[64+rsp]
	vmovaps	xmm11,XMMWORD[80+rsp]
	vmovaps	xmm12,XMMWORD[96+rsp]
	vmovaps	xmm13,XMMWORD[112+rsp]
	vmovaps	xmm14,XMMWORD[128+rsp]
	vmovaps	xmm15,XMMWORD[144+rsp]
	lea	rsp,[rax]
$L$ntt_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ml_dsa_poly_ntt_avx2:




























global	ml_dsa_poly_ntt_inverse_avx2

ALIGN	32
ml_dsa_poly_ntt_inverse_avx2:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_ml_dsa_poly_ntt_inverse_avx2:
	mov	rdi,rcx



	lea	rsp,[((-168))+rax]
	vmovaps	XMMWORD[rsp],xmm6
	vmovaps	XMMWORD[16+rsp],xmm7
	vmovaps	XMMWORD[32+rsp],xmm8
	vmovaps	XMMWORD[48+rsp],xmm9
	vmovaps	XMMWORD[64+rsp],xmm10
	vmovaps	XMMWORD[80+rsp],xmm11
	vmovaps	XMMWORD[96+rsp],xmm12
	vmovaps	XMMWORD[112+rsp],xmm13
	vmovaps	XMMWORD[128+rsp],xmm14
	vmovaps	XMMWORD[144+rsp],xmm15
$L$intt_body:
	lea	r11,[zetas_inverse]

	vpbroadcastq	ymm14,QWORD[ml_dsa_q_neg_inv]
	vpbroadcastd	ymm15,DWORD[ml_dsa_q]































	vmovdqu	ymm8,YMMWORD[rdi]
	vmovdqu	ymm9,YMMWORD[((0+32))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm0,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm1,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[r11]



	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vmovdqu	ymm8,YMMWORD[((0+64))+rdi]
	vmovdqu	ymm9,YMMWORD[((0+96))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm2,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm3,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((0+32))+r11]



	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vmovdqu	ymm8,YMMWORD[((0+128))+rdi]
	vmovdqu	ymm9,YMMWORD[((0+160))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm4,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm5,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((0+64))+r11]



	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vmovdqu	ymm8,YMMWORD[((0+192))+rdi]
	vmovdqu	ymm9,YMMWORD[((0+224))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm6,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm7,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((0+96))+r11]



	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1


	vshufps	ymm0,ymm8,ymm9,0x44


	vshufps	ymm1,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[512+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3


	vshufps	ymm2,ymm8,ymm9,0x44


	vshufps	ymm3,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((512+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5


	vshufps	ymm4,ymm8,ymm9,0x44


	vshufps	ymm5,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((512+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7


	vshufps	ymm6,ymm8,ymm9,0x44


	vshufps	ymm7,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((512+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vshufps	ymm8,ymm0,ymm1,0x44


	vshufps	ymm1,ymm0,ymm1,0xee


	vpbroadcastd	ymm13,DWORD[768+r11]
	vpbroadcastd	ymm12,DWORD[((768+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vshufps	ymm8,ymm2,ymm3,0x44
	vshufps	ymm3,ymm2,ymm3,0xee


	vpbroadcastd	ymm13,DWORD[((768+8))+r11]
	vpbroadcastd	ymm12,DWORD[((768+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vshufps	ymm8,ymm4,ymm5,0x44
	vshufps	ymm5,ymm4,ymm5,0xee


	vpbroadcastd	ymm13,DWORD[((768+16))+r11]
	vpbroadcastd	ymm12,DWORD[((768+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vshufps	ymm8,ymm6,ymm7,0x44
	vshufps	ymm7,ymm6,ymm7,0xee


	vpbroadcastd	ymm13,DWORD[((768+24))+r11]
	vpbroadcastd	ymm12,DWORD[((768+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10








	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[896+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((896+4))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((896+8))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((896+12))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10







	vpbroadcastd	ymm13,DWORD[960+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((960+4))+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10


	vmovdqu	YMMWORD[rdi],ymm0
	vmovdqu	YMMWORD[(0+32)+rdi],ymm1
	vmovdqu	YMMWORD[(0+64)+rdi],ymm2
	vmovdqu	YMMWORD[(0+96)+rdi],ymm3
	vmovdqu	YMMWORD[(0+128)+rdi],ymm4
	vmovdqu	YMMWORD[(0+160)+rdi],ymm5
	vmovdqu	YMMWORD[(0+192)+rdi],ymm6
	vmovdqu	YMMWORD[(0+224)+rdi],ymm7













	vmovdqu	ymm8,YMMWORD[256+rdi]
	vmovdqu	ymm9,YMMWORD[((256+32))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm0,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm1,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[128+r11]



	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vmovdqu	ymm8,YMMWORD[((256+64))+rdi]
	vmovdqu	ymm9,YMMWORD[((256+96))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm2,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm3,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((128+32))+r11]



	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vmovdqu	ymm8,YMMWORD[((256+128))+rdi]
	vmovdqu	ymm9,YMMWORD[((256+160))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm4,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm5,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((128+64))+r11]



	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vmovdqu	ymm8,YMMWORD[((256+192))+rdi]
	vmovdqu	ymm9,YMMWORD[((256+224))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm6,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm7,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((128+96))+r11]



	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1


	vshufps	ymm0,ymm8,ymm9,0x44


	vshufps	ymm1,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[576+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3


	vshufps	ymm2,ymm8,ymm9,0x44


	vshufps	ymm3,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((576+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5


	vshufps	ymm4,ymm8,ymm9,0x44


	vshufps	ymm5,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((576+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7


	vshufps	ymm6,ymm8,ymm9,0x44


	vshufps	ymm7,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((576+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vshufps	ymm8,ymm0,ymm1,0x44


	vshufps	ymm1,ymm0,ymm1,0xee


	vpbroadcastd	ymm13,DWORD[800+r11]
	vpbroadcastd	ymm12,DWORD[((800+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vshufps	ymm8,ymm2,ymm3,0x44
	vshufps	ymm3,ymm2,ymm3,0xee


	vpbroadcastd	ymm13,DWORD[((800+8))+r11]
	vpbroadcastd	ymm12,DWORD[((800+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vshufps	ymm8,ymm4,ymm5,0x44
	vshufps	ymm5,ymm4,ymm5,0xee


	vpbroadcastd	ymm13,DWORD[((800+16))+r11]
	vpbroadcastd	ymm12,DWORD[((800+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vshufps	ymm8,ymm6,ymm7,0x44
	vshufps	ymm7,ymm6,ymm7,0xee


	vpbroadcastd	ymm13,DWORD[((800+24))+r11]
	vpbroadcastd	ymm12,DWORD[((800+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10








	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[912+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((912+4))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((912+8))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((912+12))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10







	vpbroadcastd	ymm13,DWORD[968+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((968+4))+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10


	vmovdqu	YMMWORD[256+rdi],ymm0
	vmovdqu	YMMWORD[(256+32)+rdi],ymm1
	vmovdqu	YMMWORD[(256+64)+rdi],ymm2
	vmovdqu	YMMWORD[(256+96)+rdi],ymm3
	vmovdqu	YMMWORD[(256+128)+rdi],ymm4
	vmovdqu	YMMWORD[(256+160)+rdi],ymm5
	vmovdqu	YMMWORD[(256+192)+rdi],ymm6
	vmovdqu	YMMWORD[(256+224)+rdi],ymm7













	vmovdqu	ymm8,YMMWORD[512+rdi]
	vmovdqu	ymm9,YMMWORD[((512+32))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm0,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm1,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[256+r11]



	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vmovdqu	ymm8,YMMWORD[((512+64))+rdi]
	vmovdqu	ymm9,YMMWORD[((512+96))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm2,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm3,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((256+32))+r11]



	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vmovdqu	ymm8,YMMWORD[((512+128))+rdi]
	vmovdqu	ymm9,YMMWORD[((512+160))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm4,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm5,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((256+64))+r11]



	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vmovdqu	ymm8,YMMWORD[((512+192))+rdi]
	vmovdqu	ymm9,YMMWORD[((512+224))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm6,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm7,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((256+96))+r11]



	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1


	vshufps	ymm0,ymm8,ymm9,0x44


	vshufps	ymm1,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[640+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3


	vshufps	ymm2,ymm8,ymm9,0x44


	vshufps	ymm3,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((640+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5


	vshufps	ymm4,ymm8,ymm9,0x44


	vshufps	ymm5,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((640+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7


	vshufps	ymm6,ymm8,ymm9,0x44


	vshufps	ymm7,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((640+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vshufps	ymm8,ymm0,ymm1,0x44


	vshufps	ymm1,ymm0,ymm1,0xee


	vpbroadcastd	ymm13,DWORD[832+r11]
	vpbroadcastd	ymm12,DWORD[((832+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vshufps	ymm8,ymm2,ymm3,0x44
	vshufps	ymm3,ymm2,ymm3,0xee


	vpbroadcastd	ymm13,DWORD[((832+8))+r11]
	vpbroadcastd	ymm12,DWORD[((832+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vshufps	ymm8,ymm4,ymm5,0x44
	vshufps	ymm5,ymm4,ymm5,0xee


	vpbroadcastd	ymm13,DWORD[((832+16))+r11]
	vpbroadcastd	ymm12,DWORD[((832+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vshufps	ymm8,ymm6,ymm7,0x44
	vshufps	ymm7,ymm6,ymm7,0xee


	vpbroadcastd	ymm13,DWORD[((832+24))+r11]
	vpbroadcastd	ymm12,DWORD[((832+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10








	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[928+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((928+4))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((928+8))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((928+12))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10







	vpbroadcastd	ymm13,DWORD[976+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((976+4))+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10


	vmovdqu	YMMWORD[512+rdi],ymm0
	vmovdqu	YMMWORD[(512+32)+rdi],ymm1
	vmovdqu	YMMWORD[(512+64)+rdi],ymm2
	vmovdqu	YMMWORD[(512+96)+rdi],ymm3
	vmovdqu	YMMWORD[(512+128)+rdi],ymm4
	vmovdqu	YMMWORD[(512+160)+rdi],ymm5
	vmovdqu	YMMWORD[(512+192)+rdi],ymm6
	vmovdqu	YMMWORD[(512+224)+rdi],ymm7













	vmovdqu	ymm8,YMMWORD[768+rdi]
	vmovdqu	ymm9,YMMWORD[((768+32))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm0,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm1,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[384+r11]



	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vmovdqu	ymm8,YMMWORD[((768+64))+rdi]
	vmovdqu	ymm9,YMMWORD[((768+96))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm2,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm3,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((384+32))+r11]



	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vmovdqu	ymm8,YMMWORD[((768+128))+rdi]
	vmovdqu	ymm9,YMMWORD[((768+160))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm4,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm5,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((384+64))+r11]



	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vmovdqu	ymm8,YMMWORD[((768+192))+rdi]
	vmovdqu	ymm9,YMMWORD[((768+224))+rdi]


	vmovdqa	ymm13,YMMWORD[idx_even]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm6,ymm10,ymm11,0xf0


	vmovdqa	ymm13,YMMWORD[idx_odd]
	vpermd	ymm10,ymm13,ymm8
	vpermd	ymm11,ymm13,ymm9
	vpblendd	ymm7,ymm10,ymm11,0xf0


	vmovdqu	ymm13,YMMWORD[((384+96))+r11]



	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vmovshdup	ymm12,ymm13
	vpmuludq	ymm11,ymm12,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vpunpckldq	ymm8,ymm0,ymm1
	vpunpckhdq	ymm9,ymm0,ymm1


	vshufps	ymm0,ymm8,ymm9,0x44


	vshufps	ymm1,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[704+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10


	vpunpckldq	ymm8,ymm2,ymm3
	vpunpckhdq	ymm9,ymm2,ymm3


	vshufps	ymm2,ymm8,ymm9,0x44


	vshufps	ymm3,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((704+16))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpunpckldq	ymm8,ymm4,ymm5
	vpunpckhdq	ymm9,ymm4,ymm5


	vshufps	ymm4,ymm8,ymm9,0x44


	vshufps	ymm5,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((704+32))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpunpckldq	ymm8,ymm6,ymm7
	vpunpckhdq	ymm9,ymm6,ymm7


	vshufps	ymm6,ymm8,ymm9,0x44


	vshufps	ymm7,ymm8,ymm9,0xee


	vmovdqu	xmm13,XMMWORD[((704+48))+r11]
	vpmovzxdq	ymm13,xmm13


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10














	vshufps	ymm8,ymm0,ymm1,0x44


	vshufps	ymm1,ymm0,ymm1,0xee


	vpbroadcastd	ymm13,DWORD[864+r11]
	vpbroadcastd	ymm12,DWORD[((864+4))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vshufps	ymm8,ymm2,ymm3,0x44
	vshufps	ymm3,ymm2,ymm3,0xee


	vpbroadcastd	ymm13,DWORD[((864+8))+r11]
	vpbroadcastd	ymm12,DWORD[((864+12))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vshufps	ymm8,ymm4,ymm5,0x44
	vshufps	ymm5,ymm4,ymm5,0xee


	vpbroadcastd	ymm13,DWORD[((864+16))+r11]
	vpbroadcastd	ymm12,DWORD[((864+20))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vshufps	ymm8,ymm6,ymm7,0x44
	vshufps	ymm7,ymm6,ymm7,0xee


	vpbroadcastd	ymm13,DWORD[((864+24))+r11]
	vpbroadcastd	ymm12,DWORD[((864+28))+r11]
	vpblendd	ymm13,ymm13,ymm12,0xf0


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10








	vperm2i128	ymm8,ymm0,ymm1,0x20
	vperm2i128	ymm1,ymm0,ymm1,0x31


	vpbroadcastd	ymm13,DWORD[944+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm0,ymm1,ymm8
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vperm2i128	ymm8,ymm2,ymm3,0x20
	vperm2i128	ymm3,ymm2,ymm3,0x31


	vpbroadcastd	ymm13,DWORD[((944+4))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm2,ymm3,ymm8
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vperm2i128	ymm8,ymm4,ymm5,0x20
	vperm2i128	ymm5,ymm4,ymm5,0x31


	vpbroadcastd	ymm13,DWORD[((944+8))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm4,ymm5,ymm8
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vperm2i128	ymm8,ymm6,ymm7,0x20
	vperm2i128	ymm7,ymm6,ymm7,0x31


	vpbroadcastd	ymm13,DWORD[((944+12))+r11]


	vpaddd	ymm11,ymm8,ymm15
	vpaddd	ymm6,ymm7,ymm8
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10







	vpbroadcastd	ymm13,DWORD[984+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpbroadcastd	ymm13,DWORD[((984+4))+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10


	vmovdqu	YMMWORD[768+rdi],ymm0
	vmovdqu	YMMWORD[(768+32)+rdi],ymm1
	vmovdqu	YMMWORD[(768+64)+rdi],ymm2
	vmovdqu	YMMWORD[(768+96)+rdi],ymm3
	vmovdqu	YMMWORD[(768+128)+rdi],ymm4
	vmovdqu	YMMWORD[(768+160)+rdi],ymm5
	vmovdqu	YMMWORD[(768+192)+rdi],ymm6
	vmovdqu	YMMWORD[(768+224)+rdi],ymm7











	vmovdqu	ymm0,YMMWORD[((0+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((0+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((0+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((0+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((0+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((0+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((0+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((0+896))+rdi]




	vpbroadcastd	ymm13,DWORD[992+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vpbroadcastd	ymm13,DWORD[996+r11]


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1000+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vpbroadcastd	ymm13,DWORD[1004+r11]


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1008+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1012+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1016+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm4,ymm0
	vpsubd	ymm4,ymm11,ymm4

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm5,ymm1
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm6,ymm2
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm3,ymm15
	vpaddd	ymm3,ymm7,ymm3
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[ml_dsa_inverse_degree_montgomery]

	vpmuludq	ymm10,ymm13,ymm0
	vmovshdup	ymm11,ymm0
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm0,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm0,ymm15,ymm0
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm0,ymm10,ymm0
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm0,ymm0
	vpblendd	ymm0,ymm0,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10

	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10

	vmovdqu	YMMWORD[(0+0)+rdi],ymm0
	vmovdqu	YMMWORD[(0+128)+rdi],ymm1
	vmovdqu	YMMWORD[(0+256)+rdi],ymm2
	vmovdqu	YMMWORD[(0+384)+rdi],ymm3
	vmovdqu	YMMWORD[(0+512)+rdi],ymm4
	vmovdqu	YMMWORD[(0+640)+rdi],ymm5
	vmovdqu	YMMWORD[(0+768)+rdi],ymm6
	vmovdqu	YMMWORD[(0+896)+rdi],ymm7
	vmovdqu	ymm0,YMMWORD[((32+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((32+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((32+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((32+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((32+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((32+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((32+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((32+896))+rdi]




	vpbroadcastd	ymm13,DWORD[992+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vpbroadcastd	ymm13,DWORD[996+r11]


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1000+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vpbroadcastd	ymm13,DWORD[1004+r11]


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1008+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1012+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1016+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm4,ymm0
	vpsubd	ymm4,ymm11,ymm4

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm5,ymm1
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm6,ymm2
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm3,ymm15
	vpaddd	ymm3,ymm7,ymm3
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[ml_dsa_inverse_degree_montgomery]

	vpmuludq	ymm10,ymm13,ymm0
	vmovshdup	ymm11,ymm0
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm0,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm0,ymm15,ymm0
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm0,ymm10,ymm0
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm0,ymm0
	vpblendd	ymm0,ymm0,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10

	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10

	vmovdqu	YMMWORD[(32+0)+rdi],ymm0
	vmovdqu	YMMWORD[(32+128)+rdi],ymm1
	vmovdqu	YMMWORD[(32+256)+rdi],ymm2
	vmovdqu	YMMWORD[(32+384)+rdi],ymm3
	vmovdqu	YMMWORD[(32+512)+rdi],ymm4
	vmovdqu	YMMWORD[(32+640)+rdi],ymm5
	vmovdqu	YMMWORD[(32+768)+rdi],ymm6
	vmovdqu	YMMWORD[(32+896)+rdi],ymm7
	vmovdqu	ymm0,YMMWORD[((64+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((64+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((64+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((64+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((64+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((64+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((64+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((64+896))+rdi]




	vpbroadcastd	ymm13,DWORD[992+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vpbroadcastd	ymm13,DWORD[996+r11]


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1000+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vpbroadcastd	ymm13,DWORD[1004+r11]


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1008+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1012+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1016+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm4,ymm0
	vpsubd	ymm4,ymm11,ymm4

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm5,ymm1
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm6,ymm2
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm3,ymm15
	vpaddd	ymm3,ymm7,ymm3
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[ml_dsa_inverse_degree_montgomery]

	vpmuludq	ymm10,ymm13,ymm0
	vmovshdup	ymm11,ymm0
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm0,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm0,ymm15,ymm0
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm0,ymm10,ymm0
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm0,ymm0
	vpblendd	ymm0,ymm0,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10

	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10

	vmovdqu	YMMWORD[(64+0)+rdi],ymm0
	vmovdqu	YMMWORD[(64+128)+rdi],ymm1
	vmovdqu	YMMWORD[(64+256)+rdi],ymm2
	vmovdqu	YMMWORD[(64+384)+rdi],ymm3
	vmovdqu	YMMWORD[(64+512)+rdi],ymm4
	vmovdqu	YMMWORD[(64+640)+rdi],ymm5
	vmovdqu	YMMWORD[(64+768)+rdi],ymm6
	vmovdqu	YMMWORD[(64+896)+rdi],ymm7
	vmovdqu	ymm0,YMMWORD[((96+0))+rdi]
	vmovdqu	ymm1,YMMWORD[((96+128))+rdi]
	vmovdqu	ymm2,YMMWORD[((96+256))+rdi]
	vmovdqu	ymm3,YMMWORD[((96+384))+rdi]
	vmovdqu	ymm4,YMMWORD[((96+512))+rdi]
	vmovdqu	ymm5,YMMWORD[((96+640))+rdi]
	vmovdqu	ymm6,YMMWORD[((96+768))+rdi]
	vmovdqu	ymm7,YMMWORD[((96+896))+rdi]




	vpbroadcastd	ymm13,DWORD[992+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm1,ymm0
	vpsubd	ymm1,ymm11,ymm1

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10
	vpbroadcastd	ymm13,DWORD[996+r11]


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm3,ymm2
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1000+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm5,ymm4
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10
	vpbroadcastd	ymm13,DWORD[1004+r11]


	vpaddd	ymm11,ymm6,ymm15
	vpaddd	ymm6,ymm7,ymm6
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1008+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm2,ymm0
	vpsubd	ymm2,ymm11,ymm2

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm3,ymm1
	vpsubd	ymm3,ymm11,ymm3

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10
	vpbroadcastd	ymm13,DWORD[1012+r11]


	vpaddd	ymm11,ymm4,ymm15
	vpaddd	ymm4,ymm6,ymm4
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm5,ymm15
	vpaddd	ymm5,ymm7,ymm5
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[1016+r11]


	vpaddd	ymm11,ymm0,ymm15
	vpaddd	ymm0,ymm4,ymm0
	vpsubd	ymm4,ymm11,ymm4

	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10



	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10


	vpaddd	ymm11,ymm1,ymm15
	vpaddd	ymm1,ymm5,ymm1
	vpsubd	ymm5,ymm11,ymm5

	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10



	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10


	vpaddd	ymm11,ymm2,ymm15
	vpaddd	ymm2,ymm6,ymm2
	vpsubd	ymm6,ymm11,ymm6

	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10



	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10


	vpaddd	ymm11,ymm3,ymm15
	vpaddd	ymm3,ymm7,ymm3
	vpsubd	ymm7,ymm11,ymm7

	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10



	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10




	vpbroadcastd	ymm13,DWORD[ml_dsa_inverse_degree_montgomery]

	vpmuludq	ymm10,ymm13,ymm0
	vmovshdup	ymm11,ymm0
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm0,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm0,ymm15,ymm0
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm0,ymm10,ymm0
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm0,ymm0
	vpblendd	ymm0,ymm0,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm0
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm0,ymm0,ymm10

	vpmuludq	ymm10,ymm13,ymm4
	vmovshdup	ymm11,ymm4
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm4,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm4,ymm15,ymm4
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm4,ymm10,ymm4
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm4,ymm4
	vpblendd	ymm4,ymm4,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm4
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm4,ymm4,ymm10

	vpmuludq	ymm10,ymm13,ymm1
	vmovshdup	ymm11,ymm1
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm1,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm1,ymm15,ymm1
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm1,ymm10,ymm1
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm1,ymm1
	vpblendd	ymm1,ymm1,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm1
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm1,ymm1,ymm10

	vpmuludq	ymm10,ymm13,ymm5
	vmovshdup	ymm11,ymm5
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm5,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm5,ymm15,ymm5
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm5,ymm10,ymm5
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm5,ymm5
	vpblendd	ymm5,ymm5,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm5
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm5,ymm5,ymm10

	vpmuludq	ymm10,ymm13,ymm2
	vmovshdup	ymm11,ymm2
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm2,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm2,ymm15,ymm2
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm2,ymm10,ymm2
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm2,ymm2
	vpblendd	ymm2,ymm2,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm2
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm2,ymm2,ymm10

	vpmuludq	ymm10,ymm13,ymm6
	vmovshdup	ymm11,ymm6
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm6,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm6,ymm15,ymm6
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm6,ymm10,ymm6
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm6,ymm6
	vpblendd	ymm6,ymm6,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm6
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm6,ymm6,ymm10

	vpmuludq	ymm10,ymm13,ymm3
	vmovshdup	ymm11,ymm3
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm3,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm3,ymm15,ymm3
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm3,ymm10,ymm3
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm3,ymm3
	vpblendd	ymm3,ymm3,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm3
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm3,ymm3,ymm10

	vpmuludq	ymm10,ymm13,ymm7
	vmovshdup	ymm11,ymm7
	vpmuludq	ymm11,ymm13,ymm11

	vpmuludq	ymm7,ymm10,ymm14
	vpmuludq	ymm12,ymm11,ymm14


	vpmuludq	ymm7,ymm15,ymm7
	vpmuludq	ymm12,ymm15,ymm12


	vpaddq	ymm7,ymm10,ymm7
	vpaddq	ymm11,ymm11,ymm12


	vmovshdup	ymm7,ymm7
	vpblendd	ymm7,ymm7,ymm11,0xAA


	vpcmpgtd	ymm10,ymm15,ymm7
	vpandn	ymm10,ymm10,ymm15
	vpsubd	ymm7,ymm7,ymm10

	vmovdqu	YMMWORD[(96+0)+rdi],ymm0
	vmovdqu	YMMWORD[(96+128)+rdi],ymm1
	vmovdqu	YMMWORD[(96+256)+rdi],ymm2
	vmovdqu	YMMWORD[(96+384)+rdi],ymm3
	vmovdqu	YMMWORD[(96+512)+rdi],ymm4
	vmovdqu	YMMWORD[(96+640)+rdi],ymm5
	vmovdqu	YMMWORD[(96+768)+rdi],ymm6
	vmovdqu	YMMWORD[(96+896)+rdi],ymm7


	vzeroall
	vmovaps	xmm6,XMMWORD[rsp]
	vmovaps	xmm7,XMMWORD[16+rsp]
	vmovaps	xmm8,XMMWORD[32+rsp]
	vmovaps	xmm9,XMMWORD[48+rsp]
	vmovaps	xmm10,XMMWORD[64+rsp]
	vmovaps	xmm11,XMMWORD[80+rsp]
	vmovaps	xmm12,XMMWORD[96+rsp]
	vmovaps	xmm13,XMMWORD[112+rsp]
	vmovaps	xmm14,XMMWORD[128+rsp]
	vmovaps	xmm15,XMMWORD[144+rsp]
	lea	rsp,[rax]
$L$intt_epilogue:
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret

$L$SEH_end_ml_dsa_poly_ntt_inverse_avx2:
EXTERN	__imp_RtlVirtualUnwind

ALIGN	16
ntt_se_handler:
	push	rsi
	push	rdi
	push	rbx
	push	rbp
	push	r12
	push	r13
	push	r14
	push	r15
	pushfq
	sub	rsp,64

	mov	rax,QWORD[120+r8]
	mov	rbx,QWORD[248+r8]

	mov	rsi,QWORD[8+r9]
	mov	r11,QWORD[56+r9]

	mov	r10d,DWORD[r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jb	NEAR $L$ntt_in_prologue

	mov	r10d,DWORD[4+r11]
	lea	r10,[r10*1+rsi]
	cmp	rbx,r10
	jae	NEAR $L$ntt_in_prologue



	mov	rsi,QWORD[152+r8]
	lea	rdi,[512+r8]
	mov	ecx,20
	DD	0xa548f3fc

$L$ntt_in_prologue:

	mov	rcx,QWORD[8+rax]
	mov	rdx,QWORD[16+rax]
	mov	QWORD[176+r8],rcx
	mov	QWORD[168+r8],rdx
	mov	QWORD[152+r8],rax

	mov	rdi,QWORD[40+r9]
	mov	rsi,r8
	mov	ecx,154
	DD	0xa548f3fc

	mov	rsi,r9
	xor	rcx,rcx
	mov	rdx,QWORD[8+rsi]
	mov	r8,QWORD[rsi]
	mov	r9,QWORD[16+rsi]
	mov	r10,QWORD[40+rsi]
	lea	r11,[56+rsi]
	lea	r12,[24+rsi]
	mov	QWORD[32+rsp],r10
	mov	QWORD[40+rsp],r11
	mov	QWORD[48+rsp],r12
	mov	QWORD[56+rsp],rcx
	call	QWORD[__imp_RtlVirtualUnwind]

	mov	eax,1
	add	rsp,64
	popfq
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	rbp
	pop	rbx
	pop	rdi
	pop	rsi
	DB	0F3h,0C3h		;repret


section	.pdata rdata align=4
ALIGN	4
	DD	$L$SEH_begin_ml_dsa_poly_ntt_mult_avx2 wrt ..imagebase
	DD	$L$SEH_end_ml_dsa_poly_ntt_mult_avx2 wrt ..imagebase
	DD	$L$SEH_info_ml_dsa_poly_ntt_mult_avx2 wrt ..imagebase
	DD	$L$SEH_begin_ml_dsa_poly_ntt_avx2 wrt ..imagebase
	DD	$L$SEH_end_ml_dsa_poly_ntt_avx2 wrt ..imagebase
	DD	$L$SEH_info_ml_dsa_poly_ntt_avx2 wrt ..imagebase
	DD	$L$SEH_begin_ml_dsa_poly_ntt_inverse_avx2 wrt ..imagebase
	DD	$L$SEH_end_ml_dsa_poly_ntt_inverse_avx2 wrt ..imagebase
	DD	$L$SEH_info_ml_dsa_poly_ntt_inverse_avx2 wrt ..imagebase

section	.xdata rdata align=8
ALIGN	8
$L$SEH_info_ml_dsa_poly_ntt_mult_avx2:
DB	9,0,0,0
	DD	ntt_se_handler wrt ..imagebase
	DD	$L$ntt_mult_body wrt ..imagebase,$L$ntt_mult_epilogue wrt ..imagebase
$L$SEH_info_ml_dsa_poly_ntt_avx2:
DB	9,0,0,0
	DD	ntt_se_handler wrt ..imagebase
	DD	$L$ntt_body wrt ..imagebase,$L$ntt_epilogue wrt ..imagebase
$L$SEH_info_ml_dsa_poly_ntt_inverse_avx2:
DB	9,0,0,0
	DD	ntt_se_handler wrt ..imagebase
	DD	$L$intt_body wrt ..imagebase,$L$intt_epilogue wrt ..imagebase
