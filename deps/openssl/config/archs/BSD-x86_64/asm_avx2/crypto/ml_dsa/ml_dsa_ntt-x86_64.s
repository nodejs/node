.text	



.globl	ml_dsa_ntt_avx2_capable
.type	ml_dsa_ntt_avx2_capable,@function
.align	32
ml_dsa_ntt_avx2_capable:
	movq	OPENSSL_ia32cap_P+8(%rip),%rcx
	xorl	%eax,%eax
	andl	$32,%ecx
	cmovnzl	%ecx,%eax
	.byte	0xf3,0xc3
.size	ml_dsa_ntt_avx2_capable, .-ml_dsa_ntt_avx2_capable




.section	.rodata




















.align	64
zetas_inverse:
.long	8380417 - 1976782, 8380417 - 7534263, 8380417 - 1400424, 8380417 - 3937738, 8380417 - 7018208, 8380417 - 8332111, 8380417 - 3919660, 8380417 - 7826001
.long	8380417 - 4834730, 8380417 - 1612842, 8380417 - 7403526, 8380417 - 183443,  8380417 - 6094090, 8380417 - 7959518, 8380417 - 6144432, 8380417 - 5441381
.long	8380417 - 4546524, 8380417 - 8119771, 8380417 - 7276084, 8380417 - 6712985, 8380417 - 1910376, 8380417 - 6577327, 8380417 - 1723600, 8380417 - 7953734
.long	8380417 - 472078,  8380417 - 1717735, 8380417 - 7404533, 8380417 - 2213111, 8380417 - 269760,  8380417 - 3866901, 8380417 - 3523897, 8380417 - 5341501
.long	8380417 - 6581310, 8380417 - 4686184, 8380417 - 1652634, 8380417 - 810149,  8380417 - 3014001, 8380417 - 1616392, 8380417 - 162844,  8380417 - 5196991
.long	8380417 - 7173032, 8380417 - 185531,  8380417 - 3369112, 8380417 - 1957272, 8380417 - 8215696, 8380417 - 2454455, 8380417 - 2432395, 8380417 - 6366809
.long	8380417 - 4603424, 8380417 - 594136,  8380417 - 4656147, 8380417 - 5796124, 8380417 - 6533464, 8380417 - 6709241, 8380417 - 5548557, 8380417 - 7838005
.long	8380417 - 3406031, 8380417 - 2235880, 8380417 - 777191,  8380417 - 1500165, 8380417 - 7005614, 8380417 - 5834105, 8380417 - 1917081, 8380417 - 7100756
.long	8380417 - 6417775, 8380417 - 3306115, 8380417 - 1312455, 8380417 - 7929317, 8380417 - 6950192, 8380417 - 5062207, 8380417 - 1237275, 8380417 - 7047359
.long	8380417 - 7329447, 8380417 - 1903435, 8380417 - 1869119, 8380417 - 5386378, 8380417 - 4832145, 8380417 - 2635921, 8380417 - 1250494, 8380417 - 4613401
.long	8380417 - 1595974, 8380417 - 2486353, 8380417 - 1247620, 8380417 - 4055324, 8380417 - 1265009, 8380417 - 5790267, 8380417 - 2691481, 8380417 - 2842341
.long	8380417 - 203044,  8380417 - 1735879, 8380417 - 5038140, 8380417 - 3437287, 8380417 - 4108315, 8380417 - 5942594, 8380417 - 286988,  8380417 - 342297
.long	8380417 - 4784579, 8380417 - 7611795, 8380417 - 7855319, 8380417 - 4823422, 8380417 - 3207046, 8380417 - 2031748, 8380417 - 5257975, 8380417 - 7725090
.long	8380417 - 7857917, 8380417 - 8337157, 8380417 - 6767243, 8380417 - 495491,  8380417 - 819034,  8380417 - 909542,  8380417 - 1859098, 8380417 - 900702
.long	8380417 - 5187039, 8380417 - 7183191, 8380417 - 4621053, 8380417 - 4860065, 8380417 - 3513181, 8380417 - 7144689, 8380417 - 2434439, 8380417 - 266997
.long	8380417 - 4817955, 8380417 - 5933984, 8380417 - 2244091, 8380417 - 5037939, 8380417 - 3817976, 8380417 - 2316500, 8380417 - 3407706, 8380417 - 2091667
.long	8380417 - 3839961, 8380417 - 4751448, 8380417 - 4499357, 8380417 - 5361315, 8380417 - 6940675, 8380417 - 7567685, 8380417 - 6795489, 8380417 - 1285669
.long	8380417 - 1341330, 8380417 - 1315589, 8380417 - 8202977, 8380417 - 5971092, 8380417 - 6529015, 8380417 - 3159746, 8380417 - 4827145, 8380417 - 189548
.long	8380417 - 7063561, 8380417 - 759969,  8380417 - 8169440, 8380417 - 2389356, 8380417 - 5130689, 8380417 - 1653064, 8380417 - 8371839, 8380417 - 4656075
.long	8380417 - 3958618, 8380417 - 904516,  8380417 - 7280319, 8380417 - 44288,   8380417 - 3097992, 8380417 - 508951,  8380417 - 264944,  8380417 - 5037034
.long	8380417 - 6949987, 8380417 - 1852771, 8380417 - 1349076, 8380417 - 7998430, 8380417 - 7072248, 8380417 - 8357436, 8380417 - 7151892, 8380417 - 7709315
.long	8380417 - 5903370, 8380417 - 7969390, 8380417 - 4686924, 8380417 - 5412772, 8380417 - 2715295, 8380417 - 2147896, 8380417 - 7396998, 8380417 - 3412210
.long	8380417 - 126922,  8380417 - 4747489, 8380417 - 5223087, 8380417 - 5190273, 8380417 - 7380215, 8380417 - 4296819, 8380417 - 1939314, 8380417 - 7122806
.long	8380417 - 6795196, 8380417 - 2176455, 8380417 - 3475950, 8380417 - 6927966, 8380417 - 5339162, 8380417 - 4702672, 8380417 - 6851714, 8380417 - 4450022
.long	8380417 - 5582638, 8380417 - 2071892, 8380417 - 5823537, 8380417 - 3900724, 8380417 - 3881043, 8380417 - 954230,  8380417 - 531354,  8380417 - 811944
.long	8380417 - 3699596, 8380417 - 6779997, 8380417 - 6239768, 8380417 - 3507263, 8380417 - 4558682, 8380417 - 3505694, 8380417 - 6736599, 8380417 - 6681150
.long	8380417 - 7841118, 8380417 - 2348700, 8380417 - 8079950, 8380417 - 3539968, 8380417 - 5512770, 8380417 - 3574422, 8380417 - 5336701, 8380417 - 4519302
.long	8380417 - 3915439, 8380417 - 5842901, 8380417 - 4788269, 8380417 - 6718724, 8380417 - 3530437, 8380417 - 3077325, 8380417 - 95776,   8380417 - 2706023
.long	8380417 - 280005,  8380417 - 4010497, 8380417 - 8360995, 8380417 - 1757237, 8380417 - 5102745, 8380417 - 6980856, 8380417 - 4520680, 8380417 - 6262231
.long	8380417 - 6271868, 8380417 - 2619752, 8380417 - 7260833, 8380417 - 7830929, 8380417 - 3585928, 8380417 - 7300517, 8380417 - 1024112, 8380417 - 2725464
.long	8380417 - 2680103, 8380417 - 3111497, 8380417 - 5495562, 8380417 - 3119733, 8380417 - 6288512, 8380417 - 8021166, 8380417 - 2353451, 8380417 - 1826347
.long	8380417 - 466468,  8380417 - 7504169, 8380417 - 7602457, 8380417 - 237124,  8380417 - 7861508, 8380417 - 5771523, 8380417 - 25847,   8380417 - 4193792

.align	32
idx_even:
.long	0,2,4,6, 0,2,4,6

.align	32
idx_odd:
.long	1,3,5,7, 1,3,5,7


.align	8
ml_dsa_q:
.quad	8380417


.align	8
ml_dsa_q_neg_inv:
.quad	4236238847


.align	8
ml_dsa_inverse_degree_montgomery:
.quad	41978





.text	




































.globl	ml_dsa_poly_ntt_mult_avx2
.type	ml_dsa_poly_ntt_mult_avx2,@function
.align	32
ml_dsa_poly_ntt_mult_avx2:
.cfi_startproc	
.Lntt_mult_body:
	vpbroadcastq	ml_dsa_q_neg_inv(%rip),%ymm14
	vpbroadcastd	ml_dsa_q(%rip),%ymm15
	xorl	%r10d,%r10d

.align	32
.Lmult_loop:

	vmovdqu	(%rdi,%r10,1),%ymm0
	vmovdqu	(%rsi,%r10,1),%ymm1



	vpmuludq	%ymm0,%ymm1,%ymm8
	vmovshdup	%ymm0,%ymm9
	vmovshdup	%ymm1,%ymm10
	vpmuludq	%ymm9,%ymm10,%ymm9

	vpmuludq	%ymm14,%ymm8,%ymm0
	vpmuludq	%ymm14,%ymm9,%ymm10


	vpmuludq	%ymm0,%ymm15,%ymm0
	vpmuludq	%ymm10,%ymm15,%ymm10


	vpaddq	%ymm0,%ymm8,%ymm0
	vpaddq	%ymm10,%ymm9,%ymm9


	vmovshdup	%ymm0,%ymm0
	vpblendd	$0xAA,%ymm9,%ymm0,%ymm0


	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpandn	%ymm15,%ymm8,%ymm8
	vpsubd	%ymm8,%ymm0,%ymm0

	vmovdqu	%ymm0,(%rdx,%r10,1)


	addl	$32,%r10d
	cmpl	$1024,%r10d
	jb	.Lmult_loop


	vzeroall
.Lntt_mult_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ml_dsa_poly_ntt_mult_avx2, .-ml_dsa_poly_ntt_mult_avx2






























.globl	ml_dsa_poly_ntt_avx2
.type	ml_dsa_poly_ntt_avx2,@function
.align	32
ml_dsa_poly_ntt_avx2:
.cfi_startproc	
.Lntt_body:


	movq	%rsi,%r11


	vpbroadcastq	ml_dsa_q_neg_inv(%rip),%ymm14
	vpbroadcastd	ml_dsa_q(%rip),%ymm15






	vmovdqu	0+0(%rdi),%ymm0
	vmovdqu	0+128(%rdi),%ymm1
	vmovdqu	0+256(%rdi),%ymm2
	vmovdqu	0+384(%rdi),%ymm3
	vmovdqu	0+512(%rdi),%ymm4
	vmovdqu	0+640(%rdi),%ymm5
	vmovdqu	0+768(%rdi),%ymm6
	vmovdqu	0+896(%rdi),%ymm7





	vpbroadcastd	4(%r11),%ymm13

	vpmuludq	%ymm4,%ymm13,%ymm9
	vmovshdup	%ymm4,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm4


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm4,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm5,%ymm5

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm3,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm3,%ymm8,%ymm3



	vpcmpgtd	%ymm3,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm3,%ymm3
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	8(%r11),%ymm13

	vpmuludq	%ymm2,%ymm13,%ymm9
	vmovshdup	%ymm2,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm2


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	12(%r11),%ymm13

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm5,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm5,%ymm8,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm5,%ymm5
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	16(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm9
	vmovshdup	%ymm1,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm1


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm1,%ymm1
	vpbroadcastd	20(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	24(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm5,%ymm5
	vpbroadcastd	28(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm6,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm6,%ymm8,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm6,%ymm6
	vpsubd	%ymm9,%ymm7,%ymm7

	vmovdqu	%ymm0,0+0(%rdi)
	vmovdqu	%ymm1,0+128(%rdi)
	vmovdqu	%ymm2,0+256(%rdi)
	vmovdqu	%ymm3,0+384(%rdi)
	vmovdqu	%ymm4,0+512(%rdi)
	vmovdqu	%ymm5,0+640(%rdi)
	vmovdqu	%ymm6,0+768(%rdi)
	vmovdqu	%ymm7,0+896(%rdi)
	vmovdqu	32+0(%rdi),%ymm0
	vmovdqu	32+128(%rdi),%ymm1
	vmovdqu	32+256(%rdi),%ymm2
	vmovdqu	32+384(%rdi),%ymm3
	vmovdqu	32+512(%rdi),%ymm4
	vmovdqu	32+640(%rdi),%ymm5
	vmovdqu	32+768(%rdi),%ymm6
	vmovdqu	32+896(%rdi),%ymm7





	vpbroadcastd	4(%r11),%ymm13

	vpmuludq	%ymm4,%ymm13,%ymm9
	vmovshdup	%ymm4,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm4


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm4,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm5,%ymm5

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm3,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm3,%ymm8,%ymm3



	vpcmpgtd	%ymm3,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm3,%ymm3
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	8(%r11),%ymm13

	vpmuludq	%ymm2,%ymm13,%ymm9
	vmovshdup	%ymm2,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm2


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	12(%r11),%ymm13

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm5,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm5,%ymm8,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm5,%ymm5
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	16(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm9
	vmovshdup	%ymm1,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm1


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm1,%ymm1
	vpbroadcastd	20(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	24(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm5,%ymm5
	vpbroadcastd	28(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm6,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm6,%ymm8,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm6,%ymm6
	vpsubd	%ymm9,%ymm7,%ymm7

	vmovdqu	%ymm0,32+0(%rdi)
	vmovdqu	%ymm1,32+128(%rdi)
	vmovdqu	%ymm2,32+256(%rdi)
	vmovdqu	%ymm3,32+384(%rdi)
	vmovdqu	%ymm4,32+512(%rdi)
	vmovdqu	%ymm5,32+640(%rdi)
	vmovdqu	%ymm6,32+768(%rdi)
	vmovdqu	%ymm7,32+896(%rdi)
	vmovdqu	64+0(%rdi),%ymm0
	vmovdqu	64+128(%rdi),%ymm1
	vmovdqu	64+256(%rdi),%ymm2
	vmovdqu	64+384(%rdi),%ymm3
	vmovdqu	64+512(%rdi),%ymm4
	vmovdqu	64+640(%rdi),%ymm5
	vmovdqu	64+768(%rdi),%ymm6
	vmovdqu	64+896(%rdi),%ymm7





	vpbroadcastd	4(%r11),%ymm13

	vpmuludq	%ymm4,%ymm13,%ymm9
	vmovshdup	%ymm4,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm4


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm4,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm5,%ymm5

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm3,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm3,%ymm8,%ymm3



	vpcmpgtd	%ymm3,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm3,%ymm3
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	8(%r11),%ymm13

	vpmuludq	%ymm2,%ymm13,%ymm9
	vmovshdup	%ymm2,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm2


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	12(%r11),%ymm13

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm5,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm5,%ymm8,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm5,%ymm5
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	16(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm9
	vmovshdup	%ymm1,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm1


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm1,%ymm1
	vpbroadcastd	20(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	24(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm5,%ymm5
	vpbroadcastd	28(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm6,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm6,%ymm8,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm6,%ymm6
	vpsubd	%ymm9,%ymm7,%ymm7

	vmovdqu	%ymm0,64+0(%rdi)
	vmovdqu	%ymm1,64+128(%rdi)
	vmovdqu	%ymm2,64+256(%rdi)
	vmovdqu	%ymm3,64+384(%rdi)
	vmovdqu	%ymm4,64+512(%rdi)
	vmovdqu	%ymm5,64+640(%rdi)
	vmovdqu	%ymm6,64+768(%rdi)
	vmovdqu	%ymm7,64+896(%rdi)
	vmovdqu	96+0(%rdi),%ymm0
	vmovdqu	96+128(%rdi),%ymm1
	vmovdqu	96+256(%rdi),%ymm2
	vmovdqu	96+384(%rdi),%ymm3
	vmovdqu	96+512(%rdi),%ymm4
	vmovdqu	96+640(%rdi),%ymm5
	vmovdqu	96+768(%rdi),%ymm6
	vmovdqu	96+896(%rdi),%ymm7





	vpbroadcastd	4(%r11),%ymm13

	vpmuludq	%ymm4,%ymm13,%ymm9
	vmovshdup	%ymm4,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm4


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm4,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm5,%ymm5

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm3,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm3,%ymm8,%ymm3



	vpcmpgtd	%ymm3,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm3,%ymm3
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	8(%r11),%ymm13

	vpmuludq	%ymm2,%ymm13,%ymm9
	vmovshdup	%ymm2,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm2


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm1,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm1,%ymm8,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm1,%ymm1
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	12(%r11),%ymm13

	vpmuludq	%ymm6,%ymm13,%ymm9
	vmovshdup	%ymm6,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm6


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm5,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm5,%ymm8,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm5,%ymm5
	vpsubd	%ymm9,%ymm7,%ymm7





	vpbroadcastd	16(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm9
	vmovshdup	%ymm1,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm0,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm1


	vpaddd	%ymm0,%ymm8,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm8
	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm0,%ymm0
	vpsubd	%ymm9,%ymm1,%ymm1
	vpbroadcastd	20(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm9
	vmovshdup	%ymm3,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm2,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm3


	vpaddd	%ymm2,%ymm8,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm8
	vpcmpgtd	%ymm3,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm2,%ymm2
	vpsubd	%ymm9,%ymm3,%ymm3
	vpbroadcastd	24(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm9
	vmovshdup	%ymm5,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm4,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm5


	vpaddd	%ymm4,%ymm8,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm8
	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm4,%ymm4
	vpsubd	%ymm9,%ymm5,%ymm5
	vpbroadcastd	28(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm9
	vmovshdup	%ymm7,%ymm10
	vpmuludq	%ymm10,%ymm13,%ymm10

	vpmuludq	%ymm14,%ymm9,%ymm8
	vpmuludq	%ymm14,%ymm10,%ymm11


	vpmuludq	%ymm8,%ymm15,%ymm8
	vpmuludq	%ymm11,%ymm15,%ymm11


	vpaddq	%ymm8,%ymm9,%ymm8
	vpaddq	%ymm11,%ymm10,%ymm10


	vmovshdup	%ymm8,%ymm8
	vpblendd	$0xAA,%ymm10,%ymm8,%ymm8


	vpcmpgtd	%ymm8,%ymm15,%ymm9
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm9,%ymm8,%ymm8




	vpaddd	%ymm15,%ymm6,%ymm9
	vpsubd	%ymm8,%ymm9,%ymm7


	vpaddd	%ymm6,%ymm8,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm8
	vpcmpgtd	%ymm7,%ymm15,%ymm9
	vpandn	%ymm15,%ymm8,%ymm8
	vpandn	%ymm15,%ymm9,%ymm9
	vpsubd	%ymm8,%ymm6,%ymm6
	vpsubd	%ymm9,%ymm7,%ymm7

	vmovdqu	%ymm0,96+0(%rdi)
	vmovdqu	%ymm1,96+128(%rdi)
	vmovdqu	%ymm2,96+256(%rdi)
	vmovdqu	%ymm3,96+384(%rdi)
	vmovdqu	%ymm4,96+512(%rdi)
	vmovdqu	%ymm5,96+640(%rdi)
	vmovdqu	%ymm6,96+768(%rdi)
	vmovdqu	%ymm7,96+896(%rdi)


















	vpbroadcastd	32(%r11),%ymm13


	vmovdqu	0(%rdi),%ymm0
	vmovdqu	0+32(%rdi),%ymm1
	vmovdqu	0+64(%rdi),%ymm2
	vmovdqu	0+96(%rdi),%ymm3

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm2


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm1,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm1,%ymm9,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm1,%ymm1
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	32+4(%r11),%ymm13


	vmovdqu	0+128(%rdi),%ymm4
	vmovdqu	0+160(%rdi),%ymm5
	vmovdqu	0+192(%rdi),%ymm6
	vmovdqu	0+224(%rdi),%ymm7

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm6


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm5,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm5,%ymm9,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm5,%ymm5
	vpsubd	%ymm10,%ymm7,%ymm7






	vpbroadcastd	64(%r11),%ymm13














	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vpbroadcastd	64+4(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	64+8(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vpbroadcastd	64+12(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	128(%r11),%ymm13
	vpbroadcastd	128+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	128+8(%r11),%ymm13
	vpbroadcastd	128+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	128+16(%r11),%ymm13
	vpbroadcastd	128+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	128+24(%r11),%ymm13
	vpbroadcastd	128+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7
















	vmovdqu	256(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm1,%ymm0,%ymm8
	vpunpckhqdq	%ymm1,%ymm0,%ymm1

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	256+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm3,%ymm2,%ymm8
	vpunpckhqdq	%ymm3,%ymm2,%ymm3

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	256+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm5,%ymm4,%ymm8
	vpunpckhqdq	%ymm5,%ymm4,%ymm5

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	256+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm7,%ymm6,%ymm8
	vpunpckhqdq	%ymm7,%ymm6,%ymm7

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm1
	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vmovdqu	512(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	512+32(%r11),%ymm13


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm3
	vshufps	$0x44,%ymm9,%ymm8,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	512+64(%r11),%ymm13


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm5
	vshufps	$0x44,%ymm9,%ymm8,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	512+96(%r11),%ymm13


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm7
	vshufps	$0x44,%ymm9,%ymm8,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7


	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,0(%rdi)
	vmovdqu	%ymm11,0+32(%rdi)

	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,0+64(%rdi)
	vmovdqu	%ymm11,0+96(%rdi)


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,0+128(%rdi)
	vmovdqu	%ymm11,0+160(%rdi)

	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,0+192(%rdi)
	vmovdqu	%ymm11,0+224(%rdi)





	vpbroadcastd	40(%r11),%ymm13


	vmovdqu	256(%rdi),%ymm0
	vmovdqu	256+32(%rdi),%ymm1
	vmovdqu	256+64(%rdi),%ymm2
	vmovdqu	256+96(%rdi),%ymm3

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm2


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm1,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm1,%ymm9,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm1,%ymm1
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	40+4(%r11),%ymm13


	vmovdqu	256+128(%rdi),%ymm4
	vmovdqu	256+160(%rdi),%ymm5
	vmovdqu	256+192(%rdi),%ymm6
	vmovdqu	256+224(%rdi),%ymm7

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm6


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm5,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm5,%ymm9,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm5,%ymm5
	vpsubd	%ymm10,%ymm7,%ymm7






	vpbroadcastd	80(%r11),%ymm13














	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vpbroadcastd	80+4(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	80+8(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vpbroadcastd	80+12(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	160(%r11),%ymm13
	vpbroadcastd	160+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	160+8(%r11),%ymm13
	vpbroadcastd	160+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	160+16(%r11),%ymm13
	vpbroadcastd	160+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	160+24(%r11),%ymm13
	vpbroadcastd	160+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7
















	vmovdqu	320(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm1,%ymm0,%ymm8
	vpunpckhqdq	%ymm1,%ymm0,%ymm1

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	320+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm3,%ymm2,%ymm8
	vpunpckhqdq	%ymm3,%ymm2,%ymm3

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	320+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm5,%ymm4,%ymm8
	vpunpckhqdq	%ymm5,%ymm4,%ymm5

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	320+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm7,%ymm6,%ymm8
	vpunpckhqdq	%ymm7,%ymm6,%ymm7

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm1
	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vmovdqu	640(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	640+32(%r11),%ymm13


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm3
	vshufps	$0x44,%ymm9,%ymm8,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	640+64(%r11),%ymm13


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm5
	vshufps	$0x44,%ymm9,%ymm8,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	640+96(%r11),%ymm13


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm7
	vshufps	$0x44,%ymm9,%ymm8,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7


	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,256(%rdi)
	vmovdqu	%ymm11,256+32(%rdi)

	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,256+64(%rdi)
	vmovdqu	%ymm11,256+96(%rdi)


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,256+128(%rdi)
	vmovdqu	%ymm11,256+160(%rdi)

	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,256+192(%rdi)
	vmovdqu	%ymm11,256+224(%rdi)





	vpbroadcastd	48(%r11),%ymm13


	vmovdqu	512(%rdi),%ymm0
	vmovdqu	512+32(%rdi),%ymm1
	vmovdqu	512+64(%rdi),%ymm2
	vmovdqu	512+96(%rdi),%ymm3

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm2


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm1,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm1,%ymm9,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm1,%ymm1
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	48+4(%r11),%ymm13


	vmovdqu	512+128(%rdi),%ymm4
	vmovdqu	512+160(%rdi),%ymm5
	vmovdqu	512+192(%rdi),%ymm6
	vmovdqu	512+224(%rdi),%ymm7

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm6


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm5,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm5,%ymm9,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm5,%ymm5
	vpsubd	%ymm10,%ymm7,%ymm7






	vpbroadcastd	96(%r11),%ymm13














	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vpbroadcastd	96+4(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	96+8(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vpbroadcastd	96+12(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	192(%r11),%ymm13
	vpbroadcastd	192+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	192+8(%r11),%ymm13
	vpbroadcastd	192+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	192+16(%r11),%ymm13
	vpbroadcastd	192+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	192+24(%r11),%ymm13
	vpbroadcastd	192+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7
















	vmovdqu	384(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm1,%ymm0,%ymm8
	vpunpckhqdq	%ymm1,%ymm0,%ymm1

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	384+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm3,%ymm2,%ymm8
	vpunpckhqdq	%ymm3,%ymm2,%ymm3

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	384+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm5,%ymm4,%ymm8
	vpunpckhqdq	%ymm5,%ymm4,%ymm5

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	384+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm7,%ymm6,%ymm8
	vpunpckhqdq	%ymm7,%ymm6,%ymm7

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm1
	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vmovdqu	768(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	768+32(%r11),%ymm13


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm3
	vshufps	$0x44,%ymm9,%ymm8,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	768+64(%r11),%ymm13


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm5
	vshufps	$0x44,%ymm9,%ymm8,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	768+96(%r11),%ymm13


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm7
	vshufps	$0x44,%ymm9,%ymm8,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7


	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,512(%rdi)
	vmovdqu	%ymm11,512+32(%rdi)

	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,512+64(%rdi)
	vmovdqu	%ymm11,512+96(%rdi)


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,512+128(%rdi)
	vmovdqu	%ymm11,512+160(%rdi)

	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,512+192(%rdi)
	vmovdqu	%ymm11,512+224(%rdi)





	vpbroadcastd	56(%r11),%ymm13


	vmovdqu	768(%rdi),%ymm0
	vmovdqu	768+32(%rdi),%ymm1
	vmovdqu	768+64(%rdi),%ymm2
	vmovdqu	768+96(%rdi),%ymm3

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm2


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm1,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm1,%ymm9,%ymm1



	vpcmpgtd	%ymm1,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm1,%ymm1
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	56+4(%r11),%ymm13


	vmovdqu	768+128(%rdi),%ymm4
	vmovdqu	768+160(%rdi),%ymm5
	vmovdqu	768+192(%rdi),%ymm6
	vmovdqu	768+224(%rdi),%ymm7

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm6


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm5,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm5,%ymm9,%ymm5



	vpcmpgtd	%ymm5,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm5,%ymm5
	vpsubd	%ymm10,%ymm7,%ymm7






	vpbroadcastd	112(%r11),%ymm13














	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vpbroadcastd	112+4(%r11),%ymm13

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	112+8(%r11),%ymm13

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vpbroadcastd	112+12(%r11),%ymm13

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	224(%r11),%ymm13
	vpbroadcastd	224+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	224+8(%r11),%ymm13
	vpbroadcastd	224+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3

	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	224+16(%r11),%ymm13
	vpbroadcastd	224+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	224+24(%r11),%ymm13
	vpbroadcastd	224+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7
















	vmovdqu	448(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm1,%ymm0,%ymm8
	vpunpckhqdq	%ymm1,%ymm0,%ymm1

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm8,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	448+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm3,%ymm2,%ymm8
	vpunpckhqdq	%ymm3,%ymm2,%ymm3

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm8,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	448+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm5,%ymm4,%ymm8
	vpunpckhqdq	%ymm5,%ymm4,%ymm5

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm8,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	448+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpunpcklqdq	%ymm7,%ymm6,%ymm8
	vpunpckhqdq	%ymm7,%ymm6,%ymm7

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm8,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm8,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7












	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm1
	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vmovdqu	896(%r11),%ymm13

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm0,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm1


	vpaddd	%ymm0,%ymm9,%ymm0



	vpcmpgtd	%ymm0,%ymm15,%ymm9
	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm0,%ymm0
	vpsubd	%ymm10,%ymm1,%ymm1

	vmovdqu	896+32(%r11),%ymm13


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm3
	vshufps	$0x44,%ymm9,%ymm8,%ymm2

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm2,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm3


	vpaddd	%ymm2,%ymm9,%ymm2



	vpcmpgtd	%ymm2,%ymm15,%ymm9
	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm2,%ymm2
	vpsubd	%ymm10,%ymm3,%ymm3


	vmovdqu	896+64(%r11),%ymm13


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm5
	vshufps	$0x44,%ymm9,%ymm8,%ymm4

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm4,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm5


	vpaddd	%ymm4,%ymm9,%ymm4



	vpcmpgtd	%ymm4,%ymm15,%ymm9
	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm4,%ymm4
	vpsubd	%ymm10,%ymm5,%ymm5

	vmovdqu	896+96(%r11),%ymm13


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vshufps	$0xEE,%ymm9,%ymm8,%ymm7
	vshufps	$0x44,%ymm9,%ymm8,%ymm6

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm9
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm9,%ymm15,%ymm9
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm9,%ymm10,%ymm9
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm9,%ymm9
	vpblendd	$0xAA,%ymm11,%ymm9,%ymm9


	vpcmpgtd	%ymm9,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm9,%ymm9




	vpaddd	%ymm15,%ymm6,%ymm10
	vpsubd	%ymm9,%ymm10,%ymm7


	vpaddd	%ymm6,%ymm9,%ymm6



	vpcmpgtd	%ymm6,%ymm15,%ymm9
	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm9,%ymm9
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm9,%ymm6,%ymm6
	vpsubd	%ymm10,%ymm7,%ymm7


	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,768(%rdi)
	vmovdqu	%ymm11,768+32(%rdi)

	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,768+64(%rdi)
	vmovdqu	%ymm11,768+96(%rdi)


	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,768+128(%rdi)
	vmovdqu	%ymm11,768+160(%rdi)

	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm10
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm11
	vmovdqu	%ymm10,768+192(%rdi)
	vmovdqu	%ymm11,768+224(%rdi)


	vzeroall
.Lntt_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ml_dsa_poly_ntt_avx2, .-ml_dsa_poly_ntt_avx2




























.globl	ml_dsa_poly_ntt_inverse_avx2
.type	ml_dsa_poly_ntt_inverse_avx2,@function
.align	32
ml_dsa_poly_ntt_inverse_avx2:
.cfi_startproc	
.Lintt_body:
	leaq	zetas_inverse(%rip),%r11

	vpbroadcastq	ml_dsa_q_neg_inv(%rip),%ymm14
	vpbroadcastd	ml_dsa_q(%rip),%ymm15































	vmovdqu	0(%rdi),%ymm8
	vmovdqu	0+32(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm0


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm1


	vmovdqu	0(%r11),%ymm13



	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vmovdqu	0+64(%rdi),%ymm8
	vmovdqu	0+96(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm2


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm3


	vmovdqu	0+32(%r11),%ymm13



	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vmovdqu	0+128(%rdi),%ymm8
	vmovdqu	0+160(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm4


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm5


	vmovdqu	0+64(%r11),%ymm13



	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vmovdqu	0+192(%rdi),%ymm8
	vmovdqu	0+224(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm6


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm7


	vmovdqu	0+96(%r11),%ymm13



	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vshufps	$0xee,%ymm9,%ymm8,%ymm1


	vmovdqu	512(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm2


	vshufps	$0xee,%ymm9,%ymm8,%ymm3


	vmovdqu	512+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm4


	vshufps	$0xee,%ymm9,%ymm8,%ymm5


	vmovdqu	512+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm6


	vshufps	$0xee,%ymm9,%ymm8,%ymm7


	vmovdqu	512+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vshufps	$0x44,%ymm1,%ymm0,%ymm8


	vshufps	$0xee,%ymm1,%ymm0,%ymm1


	vpbroadcastd	768(%r11),%ymm13
	vpbroadcastd	768+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vshufps	$0x44,%ymm3,%ymm2,%ymm8
	vshufps	$0xee,%ymm3,%ymm2,%ymm3


	vpbroadcastd	768+8(%r11),%ymm13
	vpbroadcastd	768+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vshufps	$0x44,%ymm5,%ymm4,%ymm8
	vshufps	$0xee,%ymm5,%ymm4,%ymm5


	vpbroadcastd	768+16(%r11),%ymm13
	vpbroadcastd	768+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vshufps	$0x44,%ymm7,%ymm6,%ymm8
	vshufps	$0xee,%ymm7,%ymm6,%ymm7


	vpbroadcastd	768+24(%r11),%ymm13
	vpbroadcastd	768+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7








	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	896(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	896+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	896+8(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	896+12(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7







	vpbroadcastd	960(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	960+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7


	vmovdqu	%ymm0,0(%rdi)
	vmovdqu	%ymm1,0+32(%rdi)
	vmovdqu	%ymm2,0+64(%rdi)
	vmovdqu	%ymm3,0+96(%rdi)
	vmovdqu	%ymm4,0+128(%rdi)
	vmovdqu	%ymm5,0+160(%rdi)
	vmovdqu	%ymm6,0+192(%rdi)
	vmovdqu	%ymm7,0+224(%rdi)













	vmovdqu	256(%rdi),%ymm8
	vmovdqu	256+32(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm0


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm1


	vmovdqu	128(%r11),%ymm13



	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vmovdqu	256+64(%rdi),%ymm8
	vmovdqu	256+96(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm2


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm3


	vmovdqu	128+32(%r11),%ymm13



	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vmovdqu	256+128(%rdi),%ymm8
	vmovdqu	256+160(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm4


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm5


	vmovdqu	128+64(%r11),%ymm13



	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vmovdqu	256+192(%rdi),%ymm8
	vmovdqu	256+224(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm6


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm7


	vmovdqu	128+96(%r11),%ymm13



	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vshufps	$0xee,%ymm9,%ymm8,%ymm1


	vmovdqu	576(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm2


	vshufps	$0xee,%ymm9,%ymm8,%ymm3


	vmovdqu	576+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm4


	vshufps	$0xee,%ymm9,%ymm8,%ymm5


	vmovdqu	576+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm6


	vshufps	$0xee,%ymm9,%ymm8,%ymm7


	vmovdqu	576+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vshufps	$0x44,%ymm1,%ymm0,%ymm8


	vshufps	$0xee,%ymm1,%ymm0,%ymm1


	vpbroadcastd	800(%r11),%ymm13
	vpbroadcastd	800+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vshufps	$0x44,%ymm3,%ymm2,%ymm8
	vshufps	$0xee,%ymm3,%ymm2,%ymm3


	vpbroadcastd	800+8(%r11),%ymm13
	vpbroadcastd	800+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vshufps	$0x44,%ymm5,%ymm4,%ymm8
	vshufps	$0xee,%ymm5,%ymm4,%ymm5


	vpbroadcastd	800+16(%r11),%ymm13
	vpbroadcastd	800+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vshufps	$0x44,%ymm7,%ymm6,%ymm8
	vshufps	$0xee,%ymm7,%ymm6,%ymm7


	vpbroadcastd	800+24(%r11),%ymm13
	vpbroadcastd	800+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7








	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	912(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	912+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	912+8(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	912+12(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7







	vpbroadcastd	968(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	968+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7


	vmovdqu	%ymm0,256(%rdi)
	vmovdqu	%ymm1,256+32(%rdi)
	vmovdqu	%ymm2,256+64(%rdi)
	vmovdqu	%ymm3,256+96(%rdi)
	vmovdqu	%ymm4,256+128(%rdi)
	vmovdqu	%ymm5,256+160(%rdi)
	vmovdqu	%ymm6,256+192(%rdi)
	vmovdqu	%ymm7,256+224(%rdi)













	vmovdqu	512(%rdi),%ymm8
	vmovdqu	512+32(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm0


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm1


	vmovdqu	256(%r11),%ymm13



	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vmovdqu	512+64(%rdi),%ymm8
	vmovdqu	512+96(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm2


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm3


	vmovdqu	256+32(%r11),%ymm13



	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vmovdqu	512+128(%rdi),%ymm8
	vmovdqu	512+160(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm4


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm5


	vmovdqu	256+64(%r11),%ymm13



	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vmovdqu	512+192(%rdi),%ymm8
	vmovdqu	512+224(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm6


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm7


	vmovdqu	256+96(%r11),%ymm13



	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vshufps	$0xee,%ymm9,%ymm8,%ymm1


	vmovdqu	640(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm2


	vshufps	$0xee,%ymm9,%ymm8,%ymm3


	vmovdqu	640+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm4


	vshufps	$0xee,%ymm9,%ymm8,%ymm5


	vmovdqu	640+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm6


	vshufps	$0xee,%ymm9,%ymm8,%ymm7


	vmovdqu	640+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vshufps	$0x44,%ymm1,%ymm0,%ymm8


	vshufps	$0xee,%ymm1,%ymm0,%ymm1


	vpbroadcastd	832(%r11),%ymm13
	vpbroadcastd	832+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vshufps	$0x44,%ymm3,%ymm2,%ymm8
	vshufps	$0xee,%ymm3,%ymm2,%ymm3


	vpbroadcastd	832+8(%r11),%ymm13
	vpbroadcastd	832+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vshufps	$0x44,%ymm5,%ymm4,%ymm8
	vshufps	$0xee,%ymm5,%ymm4,%ymm5


	vpbroadcastd	832+16(%r11),%ymm13
	vpbroadcastd	832+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vshufps	$0x44,%ymm7,%ymm6,%ymm8
	vshufps	$0xee,%ymm7,%ymm6,%ymm7


	vpbroadcastd	832+24(%r11),%ymm13
	vpbroadcastd	832+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7








	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	928(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	928+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	928+8(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	928+12(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7







	vpbroadcastd	976(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	976+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7


	vmovdqu	%ymm0,512(%rdi)
	vmovdqu	%ymm1,512+32(%rdi)
	vmovdqu	%ymm2,512+64(%rdi)
	vmovdqu	%ymm3,512+96(%rdi)
	vmovdqu	%ymm4,512+128(%rdi)
	vmovdqu	%ymm5,512+160(%rdi)
	vmovdqu	%ymm6,512+192(%rdi)
	vmovdqu	%ymm7,512+224(%rdi)













	vmovdqu	768(%rdi),%ymm8
	vmovdqu	768+32(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm0


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm1


	vmovdqu	384(%r11),%ymm13



	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vmovdqu	768+64(%rdi),%ymm8
	vmovdqu	768+96(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm2


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm3


	vmovdqu	384+32(%r11),%ymm13



	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vmovdqu	768+128(%rdi),%ymm8
	vmovdqu	768+160(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm4


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm5


	vmovdqu	384+64(%r11),%ymm13



	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vmovdqu	768+192(%rdi),%ymm8
	vmovdqu	768+224(%rdi),%ymm9


	vmovdqa	idx_even(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm6


	vmovdqa	idx_odd(%rip),%ymm13
	vpermd	%ymm8,%ymm13,%ymm10
	vpermd	%ymm9,%ymm13,%ymm11
	vpblendd	$0xf0,%ymm11,%ymm10,%ymm7


	vmovdqu	384+96(%r11),%ymm13



	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vmovshdup	%ymm13,%ymm12
	vpmuludq	%ymm11,%ymm12,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vpunpckldq	%ymm1,%ymm0,%ymm8
	vpunpckhdq	%ymm1,%ymm0,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm0


	vshufps	$0xee,%ymm9,%ymm8,%ymm1


	vmovdqu	704(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1


	vpunpckldq	%ymm3,%ymm2,%ymm8
	vpunpckhdq	%ymm3,%ymm2,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm2


	vshufps	$0xee,%ymm9,%ymm8,%ymm3


	vmovdqu	704+16(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpunpckldq	%ymm5,%ymm4,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm4


	vshufps	$0xee,%ymm9,%ymm8,%ymm5


	vmovdqu	704+32(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm7,%ymm6,%ymm9


	vshufps	$0x44,%ymm9,%ymm8,%ymm6


	vshufps	$0xee,%ymm9,%ymm8,%ymm7


	vmovdqu	704+48(%r11),%xmm13
	vpmovzxdq	%xmm13,%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7














	vshufps	$0x44,%ymm1,%ymm0,%ymm8


	vshufps	$0xee,%ymm1,%ymm0,%ymm1


	vpbroadcastd	864(%r11),%ymm13
	vpbroadcastd	864+4(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vshufps	$0x44,%ymm3,%ymm2,%ymm8
	vshufps	$0xee,%ymm3,%ymm2,%ymm3


	vpbroadcastd	864+8(%r11),%ymm13
	vpbroadcastd	864+12(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vshufps	$0x44,%ymm5,%ymm4,%ymm8
	vshufps	$0xee,%ymm5,%ymm4,%ymm5


	vpbroadcastd	864+16(%r11),%ymm13
	vpbroadcastd	864+20(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vshufps	$0x44,%ymm7,%ymm6,%ymm8
	vshufps	$0xee,%ymm7,%ymm6,%ymm7


	vpbroadcastd	864+24(%r11),%ymm13
	vpbroadcastd	864+28(%r11),%ymm12
	vpblendd	$0xf0,%ymm12,%ymm13,%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7








	vperm2i128	$0x20,%ymm1,%ymm0,%ymm8
	vperm2i128	$0x31,%ymm1,%ymm0,%ymm1


	vpbroadcastd	944(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vperm2i128	$0x20,%ymm3,%ymm2,%ymm8
	vperm2i128	$0x31,%ymm3,%ymm2,%ymm3


	vpbroadcastd	944+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vperm2i128	$0x20,%ymm5,%ymm4,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm4,%ymm5


	vpbroadcastd	944+8(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vperm2i128	$0x20,%ymm7,%ymm6,%ymm8
	vperm2i128	$0x31,%ymm7,%ymm6,%ymm7


	vpbroadcastd	944+12(%r11),%ymm13


	vpaddd	%ymm15,%ymm8,%ymm11
	vpaddd	%ymm8,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7







	vpbroadcastd	984(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpbroadcastd	984+4(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7


	vmovdqu	%ymm0,768(%rdi)
	vmovdqu	%ymm1,768+32(%rdi)
	vmovdqu	%ymm2,768+64(%rdi)
	vmovdqu	%ymm3,768+96(%rdi)
	vmovdqu	%ymm4,768+128(%rdi)
	vmovdqu	%ymm5,768+160(%rdi)
	vmovdqu	%ymm6,768+192(%rdi)
	vmovdqu	%ymm7,768+224(%rdi)











	vmovdqu	0+0(%rdi),%ymm0
	vmovdqu	0+128(%rdi),%ymm1
	vmovdqu	0+256(%rdi),%ymm2
	vmovdqu	0+384(%rdi),%ymm3
	vmovdqu	0+512(%rdi),%ymm4
	vmovdqu	0+640(%rdi),%ymm5
	vmovdqu	0+768(%rdi),%ymm6
	vmovdqu	0+896(%rdi),%ymm7




	vpbroadcastd	992(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vpbroadcastd	996(%r11),%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1000(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vpbroadcastd	1004(%r11),%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1008(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1012(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1016(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm4,%ymm0
	vpsubd	%ymm4,%ymm11,%ymm4

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm5,%ymm1
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm6,%ymm2
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm3,%ymm11
	vpaddd	%ymm3,%ymm7,%ymm3
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	ml_dsa_inverse_degree_montgomery(%rip),%ymm13

	vpmuludq	%ymm0,%ymm13,%ymm10
	vmovshdup	%ymm0,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm0
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm0,%ymm15,%ymm0
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm0,%ymm10,%ymm0
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm0,%ymm0
	vpblendd	$0xAA,%ymm11,%ymm0,%ymm0


	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0

	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7

	vmovdqu	%ymm0,0+0(%rdi)
	vmovdqu	%ymm1,0+128(%rdi)
	vmovdqu	%ymm2,0+256(%rdi)
	vmovdqu	%ymm3,0+384(%rdi)
	vmovdqu	%ymm4,0+512(%rdi)
	vmovdqu	%ymm5,0+640(%rdi)
	vmovdqu	%ymm6,0+768(%rdi)
	vmovdqu	%ymm7,0+896(%rdi)
	vmovdqu	32+0(%rdi),%ymm0
	vmovdqu	32+128(%rdi),%ymm1
	vmovdqu	32+256(%rdi),%ymm2
	vmovdqu	32+384(%rdi),%ymm3
	vmovdqu	32+512(%rdi),%ymm4
	vmovdqu	32+640(%rdi),%ymm5
	vmovdqu	32+768(%rdi),%ymm6
	vmovdqu	32+896(%rdi),%ymm7




	vpbroadcastd	992(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vpbroadcastd	996(%r11),%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1000(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vpbroadcastd	1004(%r11),%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1008(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1012(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1016(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm4,%ymm0
	vpsubd	%ymm4,%ymm11,%ymm4

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm5,%ymm1
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm6,%ymm2
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm3,%ymm11
	vpaddd	%ymm3,%ymm7,%ymm3
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	ml_dsa_inverse_degree_montgomery(%rip),%ymm13

	vpmuludq	%ymm0,%ymm13,%ymm10
	vmovshdup	%ymm0,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm0
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm0,%ymm15,%ymm0
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm0,%ymm10,%ymm0
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm0,%ymm0
	vpblendd	$0xAA,%ymm11,%ymm0,%ymm0


	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0

	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7

	vmovdqu	%ymm0,32+0(%rdi)
	vmovdqu	%ymm1,32+128(%rdi)
	vmovdqu	%ymm2,32+256(%rdi)
	vmovdqu	%ymm3,32+384(%rdi)
	vmovdqu	%ymm4,32+512(%rdi)
	vmovdqu	%ymm5,32+640(%rdi)
	vmovdqu	%ymm6,32+768(%rdi)
	vmovdqu	%ymm7,32+896(%rdi)
	vmovdqu	64+0(%rdi),%ymm0
	vmovdqu	64+128(%rdi),%ymm1
	vmovdqu	64+256(%rdi),%ymm2
	vmovdqu	64+384(%rdi),%ymm3
	vmovdqu	64+512(%rdi),%ymm4
	vmovdqu	64+640(%rdi),%ymm5
	vmovdqu	64+768(%rdi),%ymm6
	vmovdqu	64+896(%rdi),%ymm7




	vpbroadcastd	992(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vpbroadcastd	996(%r11),%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1000(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vpbroadcastd	1004(%r11),%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1008(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1012(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1016(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm4,%ymm0
	vpsubd	%ymm4,%ymm11,%ymm4

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm5,%ymm1
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm6,%ymm2
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm3,%ymm11
	vpaddd	%ymm3,%ymm7,%ymm3
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	ml_dsa_inverse_degree_montgomery(%rip),%ymm13

	vpmuludq	%ymm0,%ymm13,%ymm10
	vmovshdup	%ymm0,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm0
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm0,%ymm15,%ymm0
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm0,%ymm10,%ymm0
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm0,%ymm0
	vpblendd	$0xAA,%ymm11,%ymm0,%ymm0


	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0

	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7

	vmovdqu	%ymm0,64+0(%rdi)
	vmovdqu	%ymm1,64+128(%rdi)
	vmovdqu	%ymm2,64+256(%rdi)
	vmovdqu	%ymm3,64+384(%rdi)
	vmovdqu	%ymm4,64+512(%rdi)
	vmovdqu	%ymm5,64+640(%rdi)
	vmovdqu	%ymm6,64+768(%rdi)
	vmovdqu	%ymm7,64+896(%rdi)
	vmovdqu	96+0(%rdi),%ymm0
	vmovdqu	96+128(%rdi),%ymm1
	vmovdqu	96+256(%rdi),%ymm2
	vmovdqu	96+384(%rdi),%ymm3
	vmovdqu	96+512(%rdi),%ymm4
	vmovdqu	96+640(%rdi),%ymm5
	vmovdqu	96+768(%rdi),%ymm6
	vmovdqu	96+896(%rdi),%ymm7




	vpbroadcastd	992(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm1,%ymm0
	vpsubd	%ymm1,%ymm11,%ymm1

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1
	vpbroadcastd	996(%r11),%ymm13


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm3,%ymm2
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1000(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm5,%ymm4
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5
	vpbroadcastd	1004(%r11),%ymm13


	vpaddd	%ymm15,%ymm6,%ymm11
	vpaddd	%ymm6,%ymm7,%ymm6
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1008(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm2,%ymm0
	vpsubd	%ymm2,%ymm11,%ymm2

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm3,%ymm1
	vpsubd	%ymm3,%ymm11,%ymm3

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3
	vpbroadcastd	1012(%r11),%ymm13


	vpaddd	%ymm15,%ymm4,%ymm11
	vpaddd	%ymm4,%ymm6,%ymm4
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm5,%ymm11
	vpaddd	%ymm5,%ymm7,%ymm5
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	1016(%r11),%ymm13


	vpaddd	%ymm15,%ymm0,%ymm11
	vpaddd	%ymm0,%ymm4,%ymm0
	vpsubd	%ymm4,%ymm11,%ymm4

	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0



	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4


	vpaddd	%ymm15,%ymm1,%ymm11
	vpaddd	%ymm1,%ymm5,%ymm1
	vpsubd	%ymm5,%ymm11,%ymm5

	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1



	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5


	vpaddd	%ymm15,%ymm2,%ymm11
	vpaddd	%ymm2,%ymm6,%ymm2
	vpsubd	%ymm6,%ymm11,%ymm6

	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2



	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6


	vpaddd	%ymm15,%ymm3,%ymm11
	vpaddd	%ymm3,%ymm7,%ymm3
	vpsubd	%ymm7,%ymm11,%ymm7

	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3



	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7




	vpbroadcastd	ml_dsa_inverse_degree_montgomery(%rip),%ymm13

	vpmuludq	%ymm0,%ymm13,%ymm10
	vmovshdup	%ymm0,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm0
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm0,%ymm15,%ymm0
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm0,%ymm10,%ymm0
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm0,%ymm0
	vpblendd	$0xAA,%ymm11,%ymm0,%ymm0


	vpcmpgtd	%ymm0,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm0,%ymm0

	vpmuludq	%ymm4,%ymm13,%ymm10
	vmovshdup	%ymm4,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm4
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm4,%ymm15,%ymm4
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm4,%ymm10,%ymm4
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm4,%ymm4
	vpblendd	$0xAA,%ymm11,%ymm4,%ymm4


	vpcmpgtd	%ymm4,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm4,%ymm4

	vpmuludq	%ymm1,%ymm13,%ymm10
	vmovshdup	%ymm1,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm1
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm1,%ymm15,%ymm1
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm1,%ymm10,%ymm1
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm1,%ymm1
	vpblendd	$0xAA,%ymm11,%ymm1,%ymm1


	vpcmpgtd	%ymm1,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm1,%ymm1

	vpmuludq	%ymm5,%ymm13,%ymm10
	vmovshdup	%ymm5,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm5
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm5,%ymm15,%ymm5
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm5,%ymm10,%ymm5
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm5,%ymm5
	vpblendd	$0xAA,%ymm11,%ymm5,%ymm5


	vpcmpgtd	%ymm5,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm5,%ymm5

	vpmuludq	%ymm2,%ymm13,%ymm10
	vmovshdup	%ymm2,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm2
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm2,%ymm15,%ymm2
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm2,%ymm10,%ymm2
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm2,%ymm2
	vpblendd	$0xAA,%ymm11,%ymm2,%ymm2


	vpcmpgtd	%ymm2,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm2,%ymm2

	vpmuludq	%ymm6,%ymm13,%ymm10
	vmovshdup	%ymm6,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm6
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm6,%ymm15,%ymm6
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm6,%ymm10,%ymm6
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm6,%ymm6
	vpblendd	$0xAA,%ymm11,%ymm6,%ymm6


	vpcmpgtd	%ymm6,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm6,%ymm6

	vpmuludq	%ymm3,%ymm13,%ymm10
	vmovshdup	%ymm3,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm3
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm3,%ymm15,%ymm3
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm3,%ymm10,%ymm3
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm3,%ymm3
	vpblendd	$0xAA,%ymm11,%ymm3,%ymm3


	vpcmpgtd	%ymm3,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm3,%ymm3

	vpmuludq	%ymm7,%ymm13,%ymm10
	vmovshdup	%ymm7,%ymm11
	vpmuludq	%ymm11,%ymm13,%ymm11

	vpmuludq	%ymm14,%ymm10,%ymm7
	vpmuludq	%ymm14,%ymm11,%ymm12


	vpmuludq	%ymm7,%ymm15,%ymm7
	vpmuludq	%ymm12,%ymm15,%ymm12


	vpaddq	%ymm7,%ymm10,%ymm7
	vpaddq	%ymm12,%ymm11,%ymm11


	vmovshdup	%ymm7,%ymm7
	vpblendd	$0xAA,%ymm11,%ymm7,%ymm7


	vpcmpgtd	%ymm7,%ymm15,%ymm10
	vpandn	%ymm15,%ymm10,%ymm10
	vpsubd	%ymm10,%ymm7,%ymm7

	vmovdqu	%ymm0,96+0(%rdi)
	vmovdqu	%ymm1,96+128(%rdi)
	vmovdqu	%ymm2,96+256(%rdi)
	vmovdqu	%ymm3,96+384(%rdi)
	vmovdqu	%ymm4,96+512(%rdi)
	vmovdqu	%ymm5,96+640(%rdi)
	vmovdqu	%ymm6,96+768(%rdi)
	vmovdqu	%ymm7,96+896(%rdi)


	vzeroall
.Lintt_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ml_dsa_poly_ntt_inverse_avx2, .-ml_dsa_poly_ntt_inverse_avx2
	.section ".note.gnu.property", "a"
	.p2align 3
	.long 1f - 0f
	.long 4f - 1f
	.long 5
0:
	# "GNU" encoded with .byte, since .asciz isn't supported
	# on Solaris.
	.byte 0x47
	.byte 0x4e
	.byte 0x55
	.byte 0
1:
	.p2align 3
	.long 0xc0000002
	.long 3f - 2f
2:
	.long 3
3:
	.p2align 3
4:
