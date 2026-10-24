.text	













.globl	ossl_aes_cfb128_vaes_eligible
.type	ossl_aes_cfb128_vaes_eligible,@function
.balign	64

ossl_aes_cfb128_vaes_eligible:
.cfi_startproc	
.byte	243,15,30,250

	movl	OPENSSL_ia32cap_P+8(%rip),%ecx
	xorl	%eax,%eax




	andl	$0x40030000,%ecx
	cmpl	$0x40030000,%ecx
	jne	.Laes_cfb128_vaes_eligible_done

	movl	OPENSSL_ia32cap_P+12(%rip),%ecx




	andl	$0x200,%ecx
	cmpl	$0x200,%ecx
	cmovel	%ecx,%eax

.Laes_cfb128_vaes_eligible_done:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_aes_cfb128_vaes_eligible, .-ossl_aes_cfb128_vaes_eligible
.globl	ossl_aes_cfb128_vaes_enc
.type	ossl_aes_cfb128_vaes_enc,@function
.balign	64
ossl_aes_cfb128_vaes_enc:
.cfi_startproc	
.byte	243,15,30,250

	movq	(%r9),%r11


	testq	%rdx,%rdx
	jz	.Laes_cfb128_vaes_enc_done

	testq	%r11,%r11
	jz	.Laes_cfb128_enc_mid





	movq	%rcx,%r10

	movq	$0x10,%rcx
	subq	%r11,%rcx
	cmpq	%rdx,%rcx
	cmovaq	%rdx,%rcx

	movq	$1,%rax
	shlq	%cl,%rax
	decq	%rax
	kmovq	%rax,%k1

	movq	%r11,%rax
	addq	%rcx,%rax
	andb	$0x0F,%al

	leaq	(%r11,%r8,1),%r11
	vmovdqu8	(%r11),%xmm0{%k1}{z}
	vmovdqu8	(%rdi),%xmm1{%k1}{z}
	vpxor	%xmm0,%xmm1,%xmm2
	vmovdqu8	%xmm2,(%rsi){%k1}
	vmovdqu8	%xmm2,(%r11){%k1}

	addq	%rcx,%rdi
	addq	%rcx,%rsi
	subq	%rcx,%rdx
	jz	.Laes_cfb128_enc_zero_pre

	movq	%r10,%rcx

.Laes_cfb128_enc_mid:
	vmovdqu8	0(%rcx),%xmm17
	vmovdqu8	16(%rcx),%xmm18
	vmovdqu8	32(%rcx),%xmm19
	vmovdqu8	48(%rcx),%xmm20
	vmovdqu8	64(%rcx),%xmm21
	vmovdqu8	80(%rcx),%xmm22
	vmovdqu8	96(%rcx),%xmm23
	vmovdqu8	112(%rcx),%xmm24
	vmovdqu8	128(%rcx),%xmm25
	vmovdqu8	144(%rcx),%xmm26
	vmovdqu8	160(%rcx),%xmm27
	vmovdqu8	176(%rcx),%xmm28
	vmovdqu8	192(%rcx),%xmm29
	vmovdqu8	208(%rcx),%xmm30
	vmovdqu8	224(%rcx),%xmm31

	movl	240(%rcx),%r11d





	vmovdqu	(%r8),%xmm2

	cmpq	$0x10,%rdx
	jb	.Laes_cfb128_enc_post

.balign	32
.Loop_aes_cfb128_enc_main:
	subq	$0x10,%rdx

	vmovdqu	(%rdi),%xmm3
	leaq	16(%rdi),%rdi
	vpxord	%xmm17,%xmm2,%xmm2
	vaesenc	%xmm18,%xmm2,%xmm2
	vaesenc	%xmm19,%xmm2,%xmm2
	vaesenc	%xmm20,%xmm2,%xmm2
	vaesenc	%xmm21,%xmm2,%xmm2
	vaesenc	%xmm22,%xmm2,%xmm2
	vaesenc	%xmm23,%xmm2,%xmm2
	vaesenc	%xmm24,%xmm2,%xmm2
	vaesenc	%xmm25,%xmm2,%xmm2
	vaesenc	%xmm26,%xmm2,%xmm2

	cmpl	$0x09,%r11d
	ja	.Laes_cfb128_enc_mid_192_256

	vaesenclast	%xmm27,%xmm2,%xmm2
	jmp	.Laes_cfb128_enc_mid_end

.balign	32
.Laes_cfb128_enc_mid_192_256:

	vaesenc	%xmm27,%xmm2,%xmm2
	vaesenc	%xmm28,%xmm2,%xmm2

	cmpl	$0x0B,%r11d
	ja	.Laes_cfb128_enc_mid_256

	vaesenclast	%xmm29,%xmm2,%xmm2
	jmp	.Laes_cfb128_enc_mid_end

.balign	32
.Laes_cfb128_enc_mid_256:

	vaesenc	%xmm29,%xmm2,%xmm2
	vaesenc	%xmm30,%xmm2,%xmm2
	vaesenclast	%xmm31,%xmm2,%xmm2

.balign	32
.Laes_cfb128_enc_mid_end:

	vpxor	%xmm3,%xmm2,%xmm2
	cmpq	$0x10,%rdx
	vmovdqu	%xmm2,(%rsi)
	leaq	16(%rsi),%rsi
	jae	.Loop_aes_cfb128_enc_main

	xorl	%eax,%eax

	vmovdqu	%xmm2,(%r8)

.Laes_cfb128_enc_post:





	testq	%rdx,%rdx
	jz	.Laes_cfb128_enc_zero_all
	vpxord	%xmm17,%xmm2,%xmm2
	vaesenc	%xmm18,%xmm2,%xmm2
	vaesenc	%xmm19,%xmm2,%xmm2
	vaesenc	%xmm20,%xmm2,%xmm2
	vaesenc	%xmm21,%xmm2,%xmm2
	vaesenc	%xmm22,%xmm2,%xmm2
	vaesenc	%xmm23,%xmm2,%xmm2
	vaesenc	%xmm24,%xmm2,%xmm2
	vaesenc	%xmm25,%xmm2,%xmm2
	vaesenc	%xmm26,%xmm2,%xmm2

	cmpl	$0x09,%r11d
	ja	.Laes_cfb128_enc_post_192_256

	vaesenclast	%xmm27,%xmm2,%xmm2
	jmp	.Laes_cfb128_enc_post_end

.balign	32
.Laes_cfb128_enc_post_192_256:

	vaesenc	%xmm27,%xmm2,%xmm2
	vaesenc	%xmm28,%xmm2,%xmm2

	cmpl	$0x0B,%r11d
	ja	.Laes_cfb128_enc_post_256

	vaesenclast	%xmm29,%xmm2,%xmm2
	jmp	.Laes_cfb128_enc_post_end

.balign	32
.Laes_cfb128_enc_post_256:

	vaesenc	%xmm29,%xmm2,%xmm2
	vaesenc	%xmm30,%xmm2,%xmm2
	vaesenclast	%xmm31,%xmm2,%xmm2

.balign	32
.Laes_cfb128_enc_post_end:

	movq	%rdx,%rax

	movq	$1,%r11
	movb	%dl,%cl
	shlq	%cl,%r11
	decq	%r11
	kmovq	%r11,%k1

	vmovdqu8	(%rdi),%xmm1{%k1}{z}
	vpxor	%xmm2,%xmm1,%xmm0
	vmovdqu8	%xmm0,(%rsi){%k1}
	vmovdqu8	%xmm0,(%r8)



.Laes_cfb128_enc_zero_all:
	vpxord	%xmm17,%xmm17,%xmm17
	vpxord	%xmm18,%xmm18,%xmm18
	vpxord	%xmm19,%xmm19,%xmm19
	vpxord	%xmm20,%xmm20,%xmm20
	vpxord	%xmm21,%xmm21,%xmm21
	vpxord	%xmm22,%xmm22,%xmm22
	vpxord	%xmm23,%xmm23,%xmm23
	vpxord	%xmm24,%xmm24,%xmm24
	vpxord	%xmm25,%xmm25,%xmm25
	vpxord	%xmm26,%xmm26,%xmm26
	vpxord	%xmm27,%xmm27,%xmm27
	vpxord	%xmm28,%xmm28,%xmm28
	vpxord	%xmm29,%xmm29,%xmm29
	vpxord	%xmm30,%xmm30,%xmm30
	vpxord	%xmm31,%xmm31,%xmm31

	vpxor	%xmm3,%xmm3,%xmm3

.Laes_cfb128_enc_zero_pre:
	vpxor	%xmm0,%xmm0,%xmm0
	vpxor	%xmm1,%xmm1,%xmm1
	vpxor	%xmm2,%xmm2,%xmm2

	movq	%rax,(%r9)

	vzeroupper

.Laes_cfb128_vaes_enc_done:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_aes_cfb128_vaes_enc,.-ossl_aes_cfb128_vaes_enc
.globl	ossl_aes_cfb128_vaes_dec
.type	ossl_aes_cfb128_vaes_dec,@function
.balign	64
ossl_aes_cfb128_vaes_dec:
.cfi_startproc	
.byte	243,15,30,250

	movq	(%r9),%r11


	testq	%rdx,%rdx
	jz	.Laes_cfb128_vaes_dec_done
	testq	%r11,%r11
	jz	.Laes_cfb128_dec_mid





	movq	%rcx,%r10

	movq	$0x10,%rcx
	subq	%r11,%rcx
	cmpq	%rdx,%rcx
	cmovaq	%rdx,%rcx

	movq	$1,%rax
	shlq	%cl,%rax
	decq	%rax
	kmovq	%rax,%k1

	leaq	(%r11,%rcx,1),%rax
	andb	$0x0F,%al

	leaq	(%r11,%r8,1),%r11
	vmovdqu8	(%r11),%xmm0{%k1}{z}
	vmovdqu8	(%rdi),%xmm1{%k1}{z}
	vpxor	%xmm0,%xmm1,%xmm2
	vmovdqu8	%xmm2,(%rsi){%k1}
	vmovdqu8	%xmm1,(%r11){%k1}

	addq	%rcx,%rdi
	addq	%rcx,%rsi
	subq	%rcx,%rdx
	jz	.Laes_cfb128_dec_zero_pre

	movq	%r10,%rcx

.Laes_cfb128_dec_mid:
	vbroadcasti32x4	0(%rcx),%zmm17
	vbroadcasti32x4	16(%rcx),%zmm18
	vbroadcasti32x4	32(%rcx),%zmm19
	vbroadcasti32x4	48(%rcx),%zmm20
	vbroadcasti32x4	64(%rcx),%zmm21
	vbroadcasti32x4	80(%rcx),%zmm22
	vbroadcasti32x4	96(%rcx),%zmm23
	vbroadcasti32x4	112(%rcx),%zmm24
	vbroadcasti32x4	128(%rcx),%zmm25
	vbroadcasti32x4	144(%rcx),%zmm26
	vbroadcasti32x4	160(%rcx),%zmm27
	vbroadcasti32x4	176(%rcx),%zmm28
	vbroadcasti32x4	192(%rcx),%zmm29
	vbroadcasti32x4	208(%rcx),%zmm30
	vbroadcasti32x4	224(%rcx),%zmm31

	movl	240(%rcx),%r11d






	vbroadcasti32x4	(%r8),%zmm2

	cmpq	$0x100,%rdx
	jb	.Laes_cfb128_dec_check_4x






.balign	32
.Loop_aes_cfb128_dec_mid_16x:
	subq	$0x100,%rdx




	vmovdqu32	(%rdi),%zmm3

	vmovdqu32	64(%rdi),%zmm5

	vmovdqu32	128(%rdi),%zmm1

	vmovdqu32	192(%rdi),%zmm16


	valignq	$6,%zmm2,%zmm3,%zmm2

	valignq	$6,%zmm3,%zmm5,%zmm4

	valignq	$6,%zmm5,%zmm1,%zmm0

	valignq	$6,%zmm1,%zmm16,%zmm6

	leaq	256(%rdi),%rdi
	vpxord	%zmm17,%zmm2,%zmm2
	vpxord	%zmm17,%zmm4,%zmm4
	vpxord	%zmm17,%zmm0,%zmm0
	vpxord	%zmm17,%zmm6,%zmm6

	vaesenc	%zmm18,%zmm2,%zmm2
	vaesenc	%zmm18,%zmm4,%zmm4
	vaesenc	%zmm18,%zmm0,%zmm0
	vaesenc	%zmm18,%zmm6,%zmm6

	vaesenc	%zmm19,%zmm2,%zmm2
	vaesenc	%zmm19,%zmm4,%zmm4
	vaesenc	%zmm19,%zmm0,%zmm0
	vaesenc	%zmm19,%zmm6,%zmm6

	vaesenc	%zmm20,%zmm2,%zmm2
	vaesenc	%zmm20,%zmm4,%zmm4
	vaesenc	%zmm20,%zmm0,%zmm0
	vaesenc	%zmm20,%zmm6,%zmm6

	vaesenc	%zmm21,%zmm2,%zmm2
	vaesenc	%zmm21,%zmm4,%zmm4
	vaesenc	%zmm21,%zmm0,%zmm0
	vaesenc	%zmm21,%zmm6,%zmm6

	vaesenc	%zmm22,%zmm2,%zmm2
	vaesenc	%zmm22,%zmm4,%zmm4
	vaesenc	%zmm22,%zmm0,%zmm0
	vaesenc	%zmm22,%zmm6,%zmm6

	vaesenc	%zmm23,%zmm2,%zmm2
	vaesenc	%zmm23,%zmm4,%zmm4
	vaesenc	%zmm23,%zmm0,%zmm0
	vaesenc	%zmm23,%zmm6,%zmm6

	vaesenc	%zmm24,%zmm2,%zmm2
	vaesenc	%zmm24,%zmm4,%zmm4
	vaesenc	%zmm24,%zmm0,%zmm0
	vaesenc	%zmm24,%zmm6,%zmm6

	vaesenc	%zmm25,%zmm2,%zmm2
	vaesenc	%zmm25,%zmm4,%zmm4
	vaesenc	%zmm25,%zmm0,%zmm0
	vaesenc	%zmm25,%zmm6,%zmm6

	vaesenc	%zmm26,%zmm2,%zmm2
	vaesenc	%zmm26,%zmm4,%zmm4
	vaesenc	%zmm26,%zmm0,%zmm0
	vaesenc	%zmm26,%zmm6,%zmm6

	cmpl	$0x09,%r11d
	ja	.Laes_cfb128_dec_mid_16x_192_256

	vaesenclast	%zmm27,%zmm2,%zmm2
	vaesenclast	%zmm27,%zmm4,%zmm4
	vaesenclast	%zmm27,%zmm0,%zmm0
	vaesenclast	%zmm27,%zmm6,%zmm6
	jmp	.Laes_cfb128_dec_mid_16x_end

.balign	32
.Laes_cfb128_dec_mid_16x_192_256:

	vaesenc	%zmm27,%zmm2,%zmm2
	vaesenc	%zmm27,%zmm4,%zmm4
	vaesenc	%zmm27,%zmm0,%zmm0
	vaesenc	%zmm27,%zmm6,%zmm6

	vaesenc	%zmm28,%zmm2,%zmm2
	vaesenc	%zmm28,%zmm4,%zmm4
	vaesenc	%zmm28,%zmm0,%zmm0
	vaesenc	%zmm28,%zmm6,%zmm6

	cmpl	$0x0B,%r11d
	ja	.Laes_cfb128_dec_mid_16x_256

	vaesenclast	%zmm29,%zmm2,%zmm2
	vaesenclast	%zmm29,%zmm4,%zmm4
	vaesenclast	%zmm29,%zmm0,%zmm0
	vaesenclast	%zmm29,%zmm6,%zmm6
	jmp	.Laes_cfb128_dec_mid_16x_end

.balign	32
.Laes_cfb128_dec_mid_16x_256:

	vaesenc	%zmm29,%zmm2,%zmm2
	vaesenc	%zmm29,%zmm4,%zmm4
	vaesenc	%zmm29,%zmm0,%zmm0
	vaesenc	%zmm29,%zmm6,%zmm6

	vaesenc	%zmm30,%zmm2,%zmm2
	vaesenc	%zmm30,%zmm4,%zmm4
	vaesenc	%zmm30,%zmm0,%zmm0
	vaesenc	%zmm30,%zmm6,%zmm6

	vaesenclast	%zmm31,%zmm2,%zmm2
	vaesenclast	%zmm31,%zmm4,%zmm4
	vaesenclast	%zmm31,%zmm0,%zmm0
	vaesenclast	%zmm31,%zmm6,%zmm6

.balign	32
.Laes_cfb128_dec_mid_16x_end:

	vpxord	%zmm3,%zmm2,%zmm2
	vpxord	%zmm5,%zmm4,%zmm4
	vpxord	%zmm1,%zmm0,%zmm0
	vpxord	%zmm16,%zmm6,%zmm6

	cmpq	$0x100,%rdx

	vmovdqu32	%zmm2,(%rsi)
	vmovdqu32	%zmm4,64(%rsi)
	vmovdqu32	%zmm0,128(%rsi)
	vmovdqu32	%zmm6,192(%rsi)

	vmovdqu8	%zmm16,%zmm2

	leaq	256(%rsi),%rsi

	jae	.Loop_aes_cfb128_dec_mid_16x

	vextracti64x2	$3,%zmm16,%xmm2
	vinserti32x4	$3,%xmm2,%zmm2,%zmm2

	xorl	%eax,%eax

	vmovdqu	%xmm2,(%r8)

.Laes_cfb128_dec_check_4x:
	cmpq	$0x40,%rdx
	jb	.Laes_cfb128_dec_check_1x








.balign	32
.Loop_aes_cfb128_dec_mid_4x:
	subq	$0x40,%rdx


	vmovdqu32	(%rdi),%zmm3


	valignq	$6,%zmm2,%zmm3,%zmm2

	leaq	64(%rdi),%rdi
	vpxord	%zmm17,%zmm2,%zmm2
	vaesenc	%zmm18,%zmm2,%zmm2
	vaesenc	%zmm19,%zmm2,%zmm2
	vaesenc	%zmm20,%zmm2,%zmm2
	vaesenc	%zmm21,%zmm2,%zmm2
	vaesenc	%zmm22,%zmm2,%zmm2
	vaesenc	%zmm23,%zmm2,%zmm2
	vaesenc	%zmm24,%zmm2,%zmm2
	vaesenc	%zmm25,%zmm2,%zmm2
	vaesenc	%zmm26,%zmm2,%zmm2

	cmpl	$0x09,%r11d
	ja	.Laes_cfb128_dec_mid_4x_192_256

	vaesenclast	%zmm27,%zmm2,%zmm2
	jmp	.Laes_cfb128_dec_mid_4x_end

.balign	32
.Laes_cfb128_dec_mid_4x_192_256:

	vaesenc	%zmm27,%zmm2,%zmm2
	vaesenc	%zmm28,%zmm2,%zmm2

	cmpl	$0x0B,%r11d
	ja	.Laes_cfb128_dec_mid_4x_256

	vaesenclast	%zmm29,%zmm2,%zmm2
	jmp	.Laes_cfb128_dec_mid_4x_end

.balign	32
.Laes_cfb128_dec_mid_4x_256:

	vaesenc	%zmm29,%zmm2,%zmm2
	vaesenc	%zmm30,%zmm2,%zmm2
	vaesenclast	%zmm31,%zmm2,%zmm2

.balign	32
.Laes_cfb128_dec_mid_4x_end:
	vpxord	%zmm3,%zmm2,%zmm2
	cmpq	$0x40,%rdx
	vmovdqu32	%zmm2,(%rsi)
	vmovdqu8	%zmm3,%zmm2
	leaq	64(%rsi),%rsi

	jae	.Loop_aes_cfb128_dec_mid_4x

	vextracti64x2	$3,%zmm2,%xmm2


	xorl	%eax,%eax

	vmovdqu	%xmm2,(%r8)

.Laes_cfb128_dec_check_1x:
	cmpq	$0x10,%rdx
	jb	.Laes_cfb128_dec_post







.balign	32
.Loop_aes_cfb128_dec_mid_1x:
	subq	$0x10,%rdx

	vmovdqu	(%rdi),%xmm3
	leaq	16(%rdi),%rdi
	vpxord	%xmm17,%xmm2,%xmm2
	vaesenc	%xmm18,%xmm2,%xmm2
	vaesenc	%xmm19,%xmm2,%xmm2
	vaesenc	%xmm20,%xmm2,%xmm2
	vaesenc	%xmm21,%xmm2,%xmm2
	vaesenc	%xmm22,%xmm2,%xmm2
	vaesenc	%xmm23,%xmm2,%xmm2
	vaesenc	%xmm24,%xmm2,%xmm2
	vaesenc	%xmm25,%xmm2,%xmm2
	vaesenc	%xmm26,%xmm2,%xmm2

	cmpl	$0x09,%r11d
	ja	.Loop_aes_cfb128_dec_mid_1x_inner_192_256

	vaesenclast	%xmm27,%xmm2,%xmm2
	jmp	.Loop_aes_cfb128_dec_mid_1x_inner_end

.balign	32
.Loop_aes_cfb128_dec_mid_1x_inner_192_256:

	vaesenc	%xmm27,%xmm2,%xmm2
	vaesenc	%xmm28,%xmm2,%xmm2

	cmpl	$0x0B,%r11d
	ja	.Loop_aes_cfb128_dec_mid_1x_inner_256

	vaesenclast	%xmm29,%xmm2,%xmm2
	jmp	.Loop_aes_cfb128_dec_mid_1x_inner_end

.balign	32
.Loop_aes_cfb128_dec_mid_1x_inner_256:

	vaesenc	%xmm29,%xmm2,%xmm2
	vaesenc	%xmm30,%xmm2,%xmm2
	vaesenclast	%xmm31,%xmm2,%xmm2

.balign	32
.Loop_aes_cfb128_dec_mid_1x_inner_end:
	vpxor	%xmm3,%xmm2,%xmm2
	cmpq	$0x10,%rdx
	vmovdqu	%xmm2,(%rsi)
	vmovdqu8	%xmm3,%xmm2
	leaq	16(%rsi),%rsi
	jae	.Loop_aes_cfb128_dec_mid_1x

	xorl	%eax,%eax

	vmovdqu	%xmm2,(%r8)

.Laes_cfb128_dec_post:





	testq	%rdx,%rdx
	jz	.Laes_cfb128_dec_zero_all
	vpxord	%xmm17,%xmm2,%xmm2
	vaesenc	%xmm18,%xmm2,%xmm2
	vaesenc	%xmm19,%xmm2,%xmm2
	vaesenc	%xmm20,%xmm2,%xmm2
	vaesenc	%xmm21,%xmm2,%xmm2
	vaesenc	%xmm22,%xmm2,%xmm2
	vaesenc	%xmm23,%xmm2,%xmm2
	vaesenc	%xmm24,%xmm2,%xmm2
	vaesenc	%xmm25,%xmm2,%xmm2
	vaesenc	%xmm26,%xmm2,%xmm2

	cmpl	$0x09,%r11d
	ja	.Loop_aes_cfb128_dec_post_192_256

	vaesenclast	%xmm27,%xmm2,%xmm2
	jmp	.Loop_aes_cfb128_dec_post_end

.balign	32
.Loop_aes_cfb128_dec_post_192_256:

	vaesenc	%xmm27,%xmm2,%xmm2
	vaesenc	%xmm28,%xmm2,%xmm2

	cmpl	$0x0B,%r11d
	ja	.Loop_aes_cfb128_dec_post_256

	vaesenclast	%xmm29,%xmm2,%xmm2
	jmp	.Loop_aes_cfb128_dec_post_end

.balign	32
.Loop_aes_cfb128_dec_post_256:

	vaesenc	%xmm29,%xmm2,%xmm2
	vaesenc	%xmm30,%xmm2,%xmm2
	vaesenclast	%xmm31,%xmm2,%xmm2

.balign	32
.Loop_aes_cfb128_dec_post_end:

	movq	%rdx,%rax
	movq	$1,%r11
	movb	%dl,%cl
	shlq	%cl,%r11
	decq	%r11
	kmovq	%r11,%k1

	vmovdqu8	(%rdi),%xmm1{%k1}{z}
	vpxor	%xmm2,%xmm1,%xmm0
	vmovdqu8	%xmm0,(%rsi){%k1}
	vpblendmb	%xmm1,%xmm2,%xmm2{%k1}

	vmovdqu8	%xmm2,(%r8)



.Laes_cfb128_dec_zero_all:
	vpxord	%xmm17,%xmm17,%xmm17
	vpxord	%xmm18,%xmm18,%xmm18
	vpxord	%xmm19,%xmm19,%xmm19
	vpxord	%xmm20,%xmm20,%xmm20
	vpxord	%xmm21,%xmm21,%xmm21
	vpxord	%xmm22,%xmm22,%xmm22
	vpxord	%xmm23,%xmm23,%xmm23
	vpxord	%xmm24,%xmm24,%xmm24
	vpxord	%xmm25,%xmm25,%xmm25
	vpxord	%xmm26,%xmm26,%xmm26
	vpxord	%xmm27,%xmm27,%xmm27
	vpxord	%xmm28,%xmm28,%xmm28
	vpxord	%xmm29,%xmm29,%xmm29
	vpxord	%xmm30,%xmm30,%xmm30
	vpxord	%xmm31,%xmm31,%xmm31

	vpxord	%xmm3,%xmm3,%xmm3
	vpxord	%xmm4,%xmm4,%xmm4
	vpxord	%xmm5,%xmm5,%xmm5
	vpxord	%xmm6,%xmm6,%xmm6
	vpxord	%xmm16,%xmm16,%xmm16

.Laes_cfb128_dec_zero_pre:

	vpxord	%xmm0,%xmm0,%xmm0
	vpxord	%xmm1,%xmm1,%xmm1
	vpxord	%xmm2,%xmm2,%xmm2

	vzeroupper
	movq	%rax,(%r9)

.Laes_cfb128_vaes_dec_done:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_aes_cfb128_vaes_dec,.-ossl_aes_cfb128_vaes_dec
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
