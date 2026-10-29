.text	

.globl	aesni_xts_avx512_eligible
.type	aesni_xts_avx512_eligible,@function
.align	32
aesni_xts_avx512_eligible:
	movl	OPENSSL_ia32cap_P+8(%rip),%ecx
	xorl	%eax,%eax

	andl	$0xc0030000,%ecx
	cmpl	$0xc0030000,%ecx
	jne	.L_done
	movl	OPENSSL_ia32cap_P+12(%rip),%ecx

	andl	$0x640,%ecx
	cmpl	$0x640,%ecx
	cmovel	%ecx,%eax
.L_done:
	.byte	0xf3,0xc3
.size	aesni_xts_avx512_eligible, .-aesni_xts_avx512_eligible
.globl	aesni_xts_128_encrypt_avx512
.hidden	aesni_xts_128_encrypt_avx512
.type	aesni_xts_128_encrypt_avx512,@function
.align	32
aesni_xts_128_encrypt_avx512:
.cfi_startproc	
.byte	243,15,30,250
	pushq	%rbp
	movq	%rsp,%rbp
	subq	$136,%rsp
	andq	$0xffffffffffffffc0,%rsp
	movq	%rbx,128(%rsp)
	movq	$0x87,%r10
	vmovdqu	(%r9),%xmm1
	vpxor	(%r8),%xmm1,%xmm1
	vaesenc	16(%r8),%xmm1,%xmm1
	vaesenc	32(%r8),%xmm1,%xmm1
	vaesenc	48(%r8),%xmm1,%xmm1
	vaesenc	64(%r8),%xmm1,%xmm1
	vaesenc	80(%r8),%xmm1,%xmm1
	vaesenc	96(%r8),%xmm1,%xmm1
	vaesenc	112(%r8),%xmm1,%xmm1
	vaesenc	128(%r8),%xmm1,%xmm1
	vaesenc	144(%r8),%xmm1,%xmm1
	vaesenclast	160(%r8),%xmm1,%xmm1
	vmovdqa	%xmm1,(%rsp)

	cmpq	$0x80,%rdx
	jl	.L_less_than_128_bytes_hEgxyDlCngwrfFe
	vpbroadcastq	%r10,%zmm25
	cmpq	$0x100,%rdx
	jge	.L_start_by16_hEgxyDlCngwrfFe
	cmpq	$0x80,%rdx
	jge	.L_start_by8_hEgxyDlCngwrfFe

.L_do_n_blocks_hEgxyDlCngwrfFe:
	cmpq	$0x0,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	cmpq	$0x70,%rdx
	jge	.L_remaining_num_blocks_is_7_hEgxyDlCngwrfFe
	cmpq	$0x60,%rdx
	jge	.L_remaining_num_blocks_is_6_hEgxyDlCngwrfFe
	cmpq	$0x50,%rdx
	jge	.L_remaining_num_blocks_is_5_hEgxyDlCngwrfFe
	cmpq	$0x40,%rdx
	jge	.L_remaining_num_blocks_is_4_hEgxyDlCngwrfFe
	cmpq	$0x30,%rdx
	jge	.L_remaining_num_blocks_is_3_hEgxyDlCngwrfFe
	cmpq	$0x20,%rdx
	jge	.L_remaining_num_blocks_is_2_hEgxyDlCngwrfFe
	cmpq	$0x10,%rdx
	jge	.L_remaining_num_blocks_is_1_hEgxyDlCngwrfFe
	vmovdqa	%xmm0,%xmm8
	vmovdqa	%xmm9,%xmm0
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe

.L_remaining_num_blocks_is_7_hEgxyDlCngwrfFe:
	movq	$0x0000ffffffffffff,%r8
	kmovq	%r8,%k1
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2{%k1}
	addq	$0x70,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	addq	$0x70,%rsi
	vextracti32x4	$0x2,%zmm2,%xmm8
	vextracti32x4	$0x3,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe

.L_remaining_num_blocks_is_6_hEgxyDlCngwrfFe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%ymm2
	addq	$0x60,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	addq	$0x60,%rsi
	vextracti32x4	$0x1,%zmm2,%xmm8
	vextracti32x4	$0x2,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe

.L_remaining_num_blocks_is_5_hEgxyDlCngwrfFe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu	64(%rdi),%xmm2
	addq	$0x50,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu	%xmm2,64(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm2,%xmm8
	vextracti32x4	$0x1,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe

.L_remaining_num_blocks_is_4_hEgxyDlCngwrfFe:
	vmovdqu8	(%rdi),%zmm1
	addq	$0x40,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,(%rsi)
	addq	$0x40,%rsi
	vextracti32x4	$0x3,%zmm1,%xmm8
	vmovdqa64	%xmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_remaining_num_blocks_is_3_hEgxyDlCngwrfFe:
	movq	$-1,%r8
	shrq	$0x10,%r8
	kmovq	%r8,%k1
	vmovdqu8	(%rdi),%zmm1{%k1}
	addq	$0x30,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,(%rsi){%k1}
	addq	$0x30,%rsi
	vextracti32x4	$0x2,%zmm1,%xmm8
	vextracti32x4	$0x3,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_remaining_num_blocks_is_2_hEgxyDlCngwrfFe:
	vmovdqu8	(%rdi),%ymm1
	addq	$0x20,%rdi
	vbroadcasti32x4	(%rcx),%ymm0
	vpternlogq	$0x96,%ymm0,%ymm9,%ymm1
	vbroadcasti32x4	16(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	32(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	48(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	64(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	80(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	96(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	112(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	128(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	144(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	160(%rcx),%ymm0
	vaesenclast	%ymm0,%ymm1,%ymm1
	vpxorq	%ymm9,%ymm1,%ymm1
	vmovdqu	%ymm1,(%rsi)
	addq	$0x20,%rsi
	vextracti32x4	$0x1,%zmm1,%xmm8
	vextracti32x4	$0x2,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_remaining_num_blocks_is_1_hEgxyDlCngwrfFe:
	vmovdqu	(%rdi),%xmm1
	addq	$0x10,%rdi
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	(%rcx),%xmm1,%xmm1
	vaesenc	16(%rcx),%xmm1,%xmm1
	vaesenc	32(%rcx),%xmm1,%xmm1
	vaesenc	48(%rcx),%xmm1,%xmm1
	vaesenc	64(%rcx),%xmm1,%xmm1
	vaesenc	80(%rcx),%xmm1,%xmm1
	vaesenc	96(%rcx),%xmm1,%xmm1
	vaesenc	112(%rcx),%xmm1,%xmm1
	vaesenc	128(%rcx),%xmm1,%xmm1
	vaesenc	144(%rcx),%xmm1,%xmm1
	vaesenclast	160(%rcx),%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	%xmm1,(%rsi)
	addq	$0x10,%rsi
	vmovdqa	%xmm1,%xmm8
	vextracti32x4	$0x1,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe


.L_start_by16_hEgxyDlCngwrfFe:
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm11
	vpxord	%zmm14,%zmm11,%zmm11
	vpsrldq	$0xf,%zmm10,%zmm15
	vpclmulqdq	$0x0,%zmm25,%zmm15,%zmm16
	vpslldq	$0x1,%zmm10,%zmm12
	vpxord	%zmm16,%zmm12,%zmm12

.L_main_loop_run_16_hEgxyDlCngwrfFe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	vmovdqu8	128(%rdi),%zmm3
	vmovdqu8	192(%rdi),%zmm4
	addq	$0x100,%rdi
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4
	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm0,%zmm3,%zmm3
	vpxorq	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm11,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm11,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm12,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm12,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm15,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm15,%zmm17
	vpxord	%zmm14,%zmm17,%zmm17
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm16,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm16,%zmm18
	vpxord	%zmm14,%zmm18,%zmm18
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vaesenclast	%zmm0,%zmm3,%zmm3
	vaesenclast	%zmm0,%zmm4,%zmm4
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4

	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqa32	%zmm17,%zmm11
	vmovdqa32	%zmm18,%zmm12
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	vmovdqu8	%zmm3,128(%rsi)
	vmovdqu8	%zmm4,192(%rsi)
	addq	$0x100,%rsi
	subq	$0x100,%rdx
	cmpq	$0x100,%rdx
	jae	.L_main_loop_run_16_hEgxyDlCngwrfFe
	cmpq	$0x80,%rdx
	jae	.L_main_loop_run_8_hEgxyDlCngwrfFe
	vextracti32x4	$0x3,%zmm4,%xmm0
	jmp	.L_do_n_blocks_hEgxyDlCngwrfFe

.L_start_by8_hEgxyDlCngwrfFe:
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10

.L_main_loop_run_8_hEgxyDlCngwrfFe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	addq	$0x80,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vpsrldq	$0xf,%zmm10,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm10,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	addq	$0x80,%rsi
	subq	$0x80,%rdx
	cmpq	$0x80,%rdx
	jae	.L_main_loop_run_8_hEgxyDlCngwrfFe
	vextracti32x4	$0x3,%zmm2,%xmm0
	jmp	.L_do_n_blocks_hEgxyDlCngwrfFe

.L_steal_cipher_hEgxyDlCngwrfFe:
	vmovdqa	%xmm8,%xmm2
	leaq	vpshufb_shf_table(%rip),%rax
	vmovdqu	(%rax,%rdx,1),%xmm10
	vpshufb	%xmm10,%xmm8,%xmm8
	vmovdqu	-16(%rdi,%rdx,1),%xmm3
	vmovdqu	%xmm8,-16(%rsi,%rdx,1)
	leaq	vpshufb_shf_table(%rip),%rax
	addq	$16,%rax
	subq	%rdx,%rax
	vmovdqu	(%rax),%xmm10
	vpxor	mask1(%rip),%xmm10,%xmm10
	vpshufb	%xmm10,%xmm3,%xmm3
	vpblendvb	%xmm10,%xmm2,%xmm3,%xmm3
	vpxor	%xmm0,%xmm3,%xmm8
	vpxor	(%rcx),%xmm8,%xmm8
	vaesenc	16(%rcx),%xmm8,%xmm8
	vaesenc	32(%rcx),%xmm8,%xmm8
	vaesenc	48(%rcx),%xmm8,%xmm8
	vaesenc	64(%rcx),%xmm8,%xmm8
	vaesenc	80(%rcx),%xmm8,%xmm8
	vaesenc	96(%rcx),%xmm8,%xmm8
	vaesenc	112(%rcx),%xmm8,%xmm8
	vaesenc	128(%rcx),%xmm8,%xmm8
	vaesenc	144(%rcx),%xmm8,%xmm8
	vaesenclast	160(%rcx),%xmm8,%xmm8
	vpxor	%xmm0,%xmm8,%xmm8
	vmovdqu	%xmm8,-16(%rsi)
.L_ret_hEgxyDlCngwrfFe:
	movq	128(%rsp),%rbx
	xorq	%r8,%r8
	movq	%r8,128(%rsp)

	vpxorq	%zmm0,%zmm0,%zmm0
	movq	%rbp,%rsp
	popq	%rbp
	vzeroupper
	.byte	0xf3,0xc3

.L_less_than_128_bytes_hEgxyDlCngwrfFe:
	vpbroadcastq	%r10,%zmm25
	cmpq	$0x10,%rdx
	jb	.L_ret_hEgxyDlCngwrfFe
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movl	$0xaa,%r8d
	kmovq	%r8,%k2
	movq	%rdx,%r8
	andq	$0x70,%r8
	cmpq	$0x60,%r8
	je	.L_num_blocks_is_6_hEgxyDlCngwrfFe
	cmpq	$0x50,%r8
	je	.L_num_blocks_is_5_hEgxyDlCngwrfFe
	cmpq	$0x40,%r8
	je	.L_num_blocks_is_4_hEgxyDlCngwrfFe
	cmpq	$0x30,%r8
	je	.L_num_blocks_is_3_hEgxyDlCngwrfFe
	cmpq	$0x20,%r8
	je	.L_num_blocks_is_2_hEgxyDlCngwrfFe
	cmpq	$0x10,%r8
	je	.L_num_blocks_is_1_hEgxyDlCngwrfFe

.L_num_blocks_is_7_hEgxyDlCngwrfFe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	movq	$0x0000ffffffffffff,%r8
	kmovq	%r8,%k1
	vmovdqu8	0(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2{%k1}

	addq	$0x70,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,0(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	addq	$0x70,%rsi
	vextracti32x4	$0x2,%zmm2,%xmm8
	vextracti32x4	$0x3,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_num_blocks_is_6_hEgxyDlCngwrfFe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vmovdqu8	0(%rdi),%zmm1
	vmovdqu8	64(%rdi),%ymm2
	addq	$96,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,0(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	addq	$96,%rsi

	vextracti32x4	$0x1,%ymm2,%xmm8
	vextracti32x4	$0x2,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_num_blocks_is_5_hEgxyDlCngwrfFe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vmovdqu8	0(%rdi),%zmm1
	vmovdqu8	64(%rdi),%xmm2
	addq	$80,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,0(%rsi)
	vmovdqu8	%xmm2,64(%rsi)
	addq	$80,%rsi

	vmovdqa	%xmm2,%xmm8
	vextracti32x4	$0x1,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_num_blocks_is_4_hEgxyDlCngwrfFe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vmovdqu8	0(%rdi),%zmm1
	addq	$64,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,0(%rsi)
	addq	$64,%rsi
	vextracti32x4	$0x3,%zmm1,%xmm8
	vmovdqa	%xmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_num_blocks_is_3_hEgxyDlCngwrfFe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	movq	$0x0000ffffffffffff,%r8
	kmovq	%r8,%k1
	vmovdqu8	0(%rdi),%zmm1{%k1}
	addq	$48,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,0(%rsi){%k1}
	addq	$48,%rsi
	vextracti32x4	$2,%zmm1,%xmm8
	vextracti32x4	$3,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_num_blocks_is_2_hEgxyDlCngwrfFe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9

	vmovdqu8	0(%rdi),%ymm1
	addq	$32,%rdi
	vbroadcasti32x4	(%rcx),%ymm0
	vpternlogq	$0x96,%ymm0,%ymm9,%ymm1
	vbroadcasti32x4	16(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	32(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	48(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	64(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	80(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	96(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	112(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	128(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	144(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	160(%rcx),%ymm0
	vaesenclast	%ymm0,%ymm1,%ymm1
	vpxorq	%ymm9,%ymm1,%ymm1
	vmovdqu8	%ymm1,0(%rsi)
	addq	$32,%rsi

	vextracti32x4	$1,%ymm1,%xmm8
	vextracti32x4	$2,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.L_num_blocks_is_1_hEgxyDlCngwrfFe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9

	vmovdqu8	0(%rdi),%xmm1
	addq	$16,%rdi
	vbroadcasti32x4	(%rcx),%ymm0
	vpternlogq	$0x96,%ymm0,%ymm9,%ymm1
	vbroadcasti32x4	16(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	32(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	48(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	64(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	80(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	96(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	112(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	128(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	144(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	160(%rcx),%ymm0
	vaesenclast	%ymm0,%ymm1,%ymm1
	vpxorq	%ymm9,%ymm1,%ymm1
	vmovdqu8	%xmm1,0(%rsi)
	addq	$16,%rsi

	vmovdqa	%xmm1,%xmm8
	vextracti32x4	$1,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_hEgxyDlCngwrfFe
	jmp	.L_steal_cipher_hEgxyDlCngwrfFe
.cfi_endproc	
.globl	aesni_xts_128_decrypt_avx512
.hidden	aesni_xts_128_decrypt_avx512
.type	aesni_xts_128_decrypt_avx512,@function
.align	32
aesni_xts_128_decrypt_avx512:
.cfi_startproc	
.byte	243,15,30,250
	pushq	%rbp
	movq	%rsp,%rbp
	subq	$136,%rsp
	andq	$0xffffffffffffffc0,%rsp
	movq	%rbx,128(%rsp)
	movq	$0x87,%r10
	vmovdqu	(%r9),%xmm1
	vpxor	(%r8),%xmm1,%xmm1
	vaesenc	16(%r8),%xmm1,%xmm1
	vaesenc	32(%r8),%xmm1,%xmm1
	vaesenc	48(%r8),%xmm1,%xmm1
	vaesenc	64(%r8),%xmm1,%xmm1
	vaesenc	80(%r8),%xmm1,%xmm1
	vaesenc	96(%r8),%xmm1,%xmm1
	vaesenc	112(%r8),%xmm1,%xmm1
	vaesenc	128(%r8),%xmm1,%xmm1
	vaesenc	144(%r8),%xmm1,%xmm1
	vaesenclast	160(%r8),%xmm1,%xmm1
	vmovdqa	%xmm1,(%rsp)

	cmpq	$0x80,%rdx
	jb	.L_less_than_128_bytes_amivrujEyduiFoi
	vpbroadcastq	%r10,%zmm25
	cmpq	$0x100,%rdx
	jge	.L_start_by16_amivrujEyduiFoi
	jmp	.L_start_by8_amivrujEyduiFoi

.L_do_n_blocks_amivrujEyduiFoi:
	cmpq	$0x0,%rdx
	je	.L_ret_amivrujEyduiFoi
	cmpq	$0x70,%rdx
	jge	.L_remaining_num_blocks_is_7_amivrujEyduiFoi
	cmpq	$0x60,%rdx
	jge	.L_remaining_num_blocks_is_6_amivrujEyduiFoi
	cmpq	$0x50,%rdx
	jge	.L_remaining_num_blocks_is_5_amivrujEyduiFoi
	cmpq	$0x40,%rdx
	jge	.L_remaining_num_blocks_is_4_amivrujEyduiFoi
	cmpq	$0x30,%rdx
	jge	.L_remaining_num_blocks_is_3_amivrujEyduiFoi
	cmpq	$0x20,%rdx
	jge	.L_remaining_num_blocks_is_2_amivrujEyduiFoi
	cmpq	$0x10,%rdx
	jge	.L_remaining_num_blocks_is_1_amivrujEyduiFoi


	vmovdqu	%xmm5,%xmm1

	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	%xmm1,-16(%rsi)
	vmovdqa	%xmm1,%xmm8


	movq	$0x1,%r8
	kmovq	%r8,%k1
	vpsllq	$0x3f,%xmm9,%xmm13
	vpsraq	$0x3f,%xmm13,%xmm14
	vpandq	%xmm25,%xmm14,%xmm5
	vpxorq	%xmm5,%xmm9,%xmm9{%k1}
	vpsrldq	$0x8,%xmm9,%xmm10
.byte	98, 211, 181, 8, 115, 194, 1
	vpslldq	$0x8,%xmm13,%xmm13
	vpxorq	%xmm13,%xmm0,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_remaining_num_blocks_is_7_amivrujEyduiFoi:
	movq	$0xffffffffffffffff,%r8
	shrq	$0x10,%r8
	kmovq	%r8,%k1
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2{%k1}
	addq	$0x70,%rdi
	andq	$0xf,%rdx
	je	.L_done_7_remain_amivrujEyduiFoi
	vextracti32x4	$0x2,%zmm10,%xmm12
	vextracti32x4	$0x3,%zmm10,%xmm13
	vinserti32x4	$0x2,%xmm13,%zmm10,%zmm10

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	addq	$0x70,%rsi
	vextracti32x4	$0x2,%zmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_7_remain_amivrujEyduiFoi:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	jmp	.L_ret_amivrujEyduiFoi

.L_remaining_num_blocks_is_6_amivrujEyduiFoi:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%ymm2
	addq	$0x60,%rdi
	andq	$0xf,%rdx
	je	.L_done_6_remain_amivrujEyduiFoi
	vextracti32x4	$0x1,%zmm10,%xmm12
	vextracti32x4	$0x2,%zmm10,%xmm13
	vinserti32x4	$0x1,%xmm13,%zmm10,%zmm10

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	addq	$0x60,%rsi
	vextracti32x4	$0x1,%zmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_6_remain_amivrujEyduiFoi:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	jmp	.L_ret_amivrujEyduiFoi

.L_remaining_num_blocks_is_5_amivrujEyduiFoi:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu	64(%rdi),%xmm2
	addq	$0x50,%rdi
	andq	$0xf,%rdx
	je	.L_done_5_remain_amivrujEyduiFoi
	vmovdqa	%xmm10,%xmm12
	vextracti32x4	$0x1,%zmm10,%xmm10

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu	%xmm2,64(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_5_remain_amivrujEyduiFoi:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%xmm2,64(%rsi)
	jmp	.L_ret_amivrujEyduiFoi

.L_remaining_num_blocks_is_4_amivrujEyduiFoi:
	vmovdqu8	(%rdi),%zmm1
	addq	$0x40,%rdi
	andq	$0xf,%rdx
	je	.L_done_4_remain_amivrujEyduiFoi
	vextracti32x4	$0x3,%zmm9,%xmm12
	vinserti32x4	$0x3,%xmm10,%zmm9,%zmm9

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	addq	$0x40,%rsi
	vextracti32x4	$0x3,%zmm1,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_4_remain_amivrujEyduiFoi:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	jmp	.L_ret_amivrujEyduiFoi

.L_remaining_num_blocks_is_3_amivrujEyduiFoi:
	vmovdqu	(%rdi),%xmm1
	vmovdqu	16(%rdi),%xmm2
	vmovdqu	32(%rdi),%xmm3
	addq	$0x30,%rdi
	andq	$0xf,%rdx
	je	.L_done_3_remain_amivrujEyduiFoi
	vextracti32x4	$0x2,%zmm9,%xmm13
	vextracti32x4	$0x1,%zmm9,%xmm10
	vextracti32x4	$0x3,%zmm9,%xmm11
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	addq	$0x30,%rsi
	vmovdqa	%xmm3,%xmm8
	vmovdqa	%xmm13,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_3_remain_amivrujEyduiFoi:
	vextracti32x4	$0x1,%zmm9,%xmm10
	vextracti32x4	$0x2,%zmm9,%xmm11
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	jmp	.L_ret_amivrujEyduiFoi

.L_remaining_num_blocks_is_2_amivrujEyduiFoi:
	vmovdqu	(%rdi),%xmm1
	vmovdqu	16(%rdi),%xmm2
	addq	$0x20,%rdi
	andq	$0xf,%rdx
	je	.L_done_2_remain_amivrujEyduiFoi
	vextracti32x4	$0x2,%zmm9,%xmm10
	vextracti32x4	$0x1,%zmm9,%xmm12
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	addq	$0x20,%rsi
	vmovdqa	%xmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_2_remain_amivrujEyduiFoi:
	vextracti32x4	$0x1,%zmm9,%xmm10
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	jmp	.L_ret_amivrujEyduiFoi

.L_remaining_num_blocks_is_1_amivrujEyduiFoi:
	vmovdqu	(%rdi),%xmm1
	addq	$0x10,%rdi
	andq	$0xf,%rdx
	je	.L_done_1_remain_amivrujEyduiFoi
	vextracti32x4	$0x1,%zmm9,%xmm11
	vpxor	%xmm11,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm11,%xmm1,%xmm1
	vmovdqu	%xmm1,(%rsi)
	addq	$0x10,%rsi
	vmovdqa	%xmm1,%xmm8
	vmovdqa	%xmm9,%xmm0
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_1_remain_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	%xmm1,(%rsi)
	jmp	.L_ret_amivrujEyduiFoi

.L_start_by16_amivrujEyduiFoi:
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2


	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9


	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10


	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm11
	vpxord	%zmm14,%zmm11,%zmm11

	vpsrldq	$0xf,%zmm10,%zmm15
	vpclmulqdq	$0x0,%zmm25,%zmm15,%zmm16
	vpslldq	$0x1,%zmm10,%zmm12
	vpxord	%zmm16,%zmm12,%zmm12

.L_main_loop_run_16_amivrujEyduiFoi:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	vmovdqu8	128(%rdi),%zmm3
	vmovdqu8	192(%rdi),%zmm4
	vmovdqu8	240(%rdi),%xmm5
	addq	$0x100,%rdi
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4
	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm0,%zmm3,%zmm3
	vpxorq	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm11,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm11,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm12,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm12,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm15,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm15,%zmm17
	vpxord	%zmm14,%zmm17,%zmm17
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm16,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm16,%zmm18
	vpxord	%zmm14,%zmm18,%zmm18
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2
	vaesdeclast	%zmm0,%zmm3,%zmm3
	vaesdeclast	%zmm0,%zmm4,%zmm4
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4

	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqa32	%zmm17,%zmm11
	vmovdqa32	%zmm18,%zmm12
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	vmovdqu8	%zmm3,128(%rsi)
	vmovdqu8	%zmm4,192(%rsi)
	addq	$0x100,%rsi
	subq	$0x100,%rdx
	cmpq	$0x100,%rdx
	jge	.L_main_loop_run_16_amivrujEyduiFoi

	cmpq	$0x80,%rdx
	jge	.L_main_loop_run_8_amivrujEyduiFoi
	jmp	.L_do_n_blocks_amivrujEyduiFoi

.L_start_by8_amivrujEyduiFoi:

	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2


	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9


	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10

.L_main_loop_run_8_amivrujEyduiFoi:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	vmovdqu8	112(%rdi),%xmm5
	addq	$0x80,%rdi

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vpsrldq	$0xf,%zmm10,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm10,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	addq	$0x80,%rsi
	subq	$0x80,%rdx
	cmpq	$0x80,%rdx
	jge	.L_main_loop_run_8_amivrujEyduiFoi
	jmp	.L_do_n_blocks_amivrujEyduiFoi

.L_steal_cipher_amivrujEyduiFoi:

	vmovdqa	%xmm8,%xmm2


	leaq	vpshufb_shf_table(%rip),%rax
	vmovdqu	(%rax,%rdx,1),%xmm10
	vpshufb	%xmm10,%xmm8,%xmm8


	vmovdqu	-16(%rdi,%rdx,1),%xmm3
	vmovdqu	%xmm8,-16(%rsi,%rdx,1)


	leaq	vpshufb_shf_table(%rip),%rax
	addq	$16,%rax
	subq	%rdx,%rax
	vmovdqu	(%rax),%xmm10
	vpxor	mask1(%rip),%xmm10,%xmm10
	vpshufb	%xmm10,%xmm3,%xmm3

	vpblendvb	%xmm10,%xmm2,%xmm3,%xmm3


	vpxor	%xmm0,%xmm3,%xmm8


	vpxor	(%rcx),%xmm8,%xmm8
	vaesdec	16(%rcx),%xmm8,%xmm8
	vaesdec	32(%rcx),%xmm8,%xmm8
	vaesdec	48(%rcx),%xmm8,%xmm8
	vaesdec	64(%rcx),%xmm8,%xmm8
	vaesdec	80(%rcx),%xmm8,%xmm8
	vaesdec	96(%rcx),%xmm8,%xmm8
	vaesdec	112(%rcx),%xmm8,%xmm8
	vaesdec	128(%rcx),%xmm8,%xmm8
	vaesdec	144(%rcx),%xmm8,%xmm8
	vaesdeclast	160(%rcx),%xmm8,%xmm8

	vpxor	%xmm0,%xmm8,%xmm8

.L_done_amivrujEyduiFoi:

	vmovdqu	%xmm8,-16(%rsi)
.L_ret_amivrujEyduiFoi:
	movq	128(%rsp),%rbx
	xorq	%r8,%r8
	movq	%r8,128(%rsp)

	vpxorq	%zmm0,%zmm0,%zmm0
	movq	%rbp,%rsp
	popq	%rbp
	vzeroupper
	.byte	0xf3,0xc3

.L_less_than_128_bytes_amivrujEyduiFoi:
	cmpq	$0x10,%rdx
	jb	.L_ret_amivrujEyduiFoi

	movq	%rdx,%r8
	andq	$0x70,%r8
	cmpq	$0x60,%r8
	je	.L_num_blocks_is_6_amivrujEyduiFoi
	cmpq	$0x50,%r8
	je	.L_num_blocks_is_5_amivrujEyduiFoi
	cmpq	$0x40,%r8
	je	.L_num_blocks_is_4_amivrujEyduiFoi
	cmpq	$0x30,%r8
	je	.L_num_blocks_is_3_amivrujEyduiFoi
	cmpq	$0x20,%r8
	je	.L_num_blocks_is_2_amivrujEyduiFoi
	cmpq	$0x10,%r8
	je	.L_num_blocks_is_1_amivrujEyduiFoi

.L_num_blocks_is_7_amivrujEyduiFoi:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,64(%rsp)
	movq	%rbx,64 + 8(%rsp)
	vmovdqa	64(%rsp),%xmm13
	vmovdqu	64(%rdi),%xmm5
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,80(%rsp)
	movq	%rbx,80 + 8(%rsp)
	vmovdqa	80(%rsp),%xmm14
	vmovdqu	80(%rdi),%xmm6
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,96(%rsp)
	movq	%rbx,96 + 8(%rsp)
	vmovdqa	96(%rsp),%xmm15
	vmovdqu	96(%rdi),%xmm7
	addq	$0x70,%rdi
	andq	$0xf,%rdx
	je	.L_done_7_amivrujEyduiFoi

.L_steal_cipher_7_amivrujEyduiFoi:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm15,%xmm16
	vmovdqa	16(%rsp),%xmm15
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vpxor	%xmm0,%xmm7,%xmm7
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vaesdeclast	%xmm0,%xmm7,%xmm7
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	vmovdqu	%xmm6,80(%rsi)
	addq	$0x70,%rsi
	vmovdqa64	%xmm16,%xmm0
	vmovdqa	%xmm7,%xmm8
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_7_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vpxor	%xmm0,%xmm7,%xmm7
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vaesdeclast	%xmm0,%xmm7,%xmm7
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	vmovdqu	%xmm6,80(%rsi)
	addq	$0x70,%rsi
	vmovdqa	%xmm7,%xmm8
	jmp	.L_done_amivrujEyduiFoi

.L_num_blocks_is_6_amivrujEyduiFoi:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,64(%rsp)
	movq	%rbx,64 + 8(%rsp)
	vmovdqa	64(%rsp),%xmm13
	vmovdqu	64(%rdi),%xmm5
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,80(%rsp)
	movq	%rbx,80 + 8(%rsp)
	vmovdqa	80(%rsp),%xmm14
	vmovdqu	80(%rdi),%xmm6
	addq	$0x60,%rdi
	andq	$0xf,%rdx
	je	.L_done_6_amivrujEyduiFoi

.L_steal_cipher_6_amivrujEyduiFoi:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm14,%xmm15
	vmovdqa	16(%rsp),%xmm14
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	addq	$0x60,%rsi
	vmovdqa	%xmm15,%xmm0
	vmovdqa	%xmm6,%xmm8
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_6_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	addq	$0x60,%rsi
	vmovdqa	%xmm6,%xmm8
	jmp	.L_done_amivrujEyduiFoi

.L_num_blocks_is_5_amivrujEyduiFoi:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,64(%rsp)
	movq	%rbx,64 + 8(%rsp)
	vmovdqa	64(%rsp),%xmm13
	vmovdqu	64(%rdi),%xmm5
	addq	$0x50,%rdi
	andq	$0xf,%rdx
	je	.L_done_5_amivrujEyduiFoi

.L_steal_cipher_5_amivrujEyduiFoi:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm13,%xmm14
	vmovdqa	16(%rsp),%xmm13
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm14,%xmm0
	vmovdqa	%xmm5,%xmm8
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_5_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm5,%xmm8
	jmp	.L_done_amivrujEyduiFoi

.L_num_blocks_is_4_amivrujEyduiFoi:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	addq	$0x40,%rdi
	andq	$0xf,%rdx
	je	.L_done_4_amivrujEyduiFoi

.L_steal_cipher_4_amivrujEyduiFoi:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm12,%xmm13
	vmovdqa	16(%rsp),%xmm12
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	addq	$0x40,%rsi
	vmovdqa	%xmm13,%xmm0
	vmovdqa	%xmm4,%xmm8
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_4_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	addq	$0x40,%rsi
	vmovdqa	%xmm4,%xmm8
	jmp	.L_done_amivrujEyduiFoi

.L_num_blocks_is_3_amivrujEyduiFoi:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	addq	$0x30,%rdi
	andq	$0xf,%rdx
	je	.L_done_3_amivrujEyduiFoi

.L_steal_cipher_3_amivrujEyduiFoi:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm11,%xmm12
	vmovdqa	16(%rsp),%xmm11
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	addq	$0x30,%rsi
	vmovdqa	%xmm12,%xmm0
	vmovdqa	%xmm3,%xmm8
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_3_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	addq	$0x30,%rsi
	vmovdqa	%xmm3,%xmm8
	jmp	.L_done_amivrujEyduiFoi

.L_num_blocks_is_2_amivrujEyduiFoi:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	addq	$0x20,%rdi
	andq	$0xf,%rdx
	je	.L_done_2_amivrujEyduiFoi

.L_steal_cipher_2_amivrujEyduiFoi:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm10,%xmm11
	vmovdqa	16(%rsp),%xmm10
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	addq	$0x20,%rsi
	vmovdqa	%xmm11,%xmm0
	vmovdqa	%xmm2,%xmm8
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_2_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	addq	$0x20,%rsi
	vmovdqa	%xmm2,%xmm8
	jmp	.L_done_amivrujEyduiFoi

.L_num_blocks_is_1_amivrujEyduiFoi:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	addq	$0x10,%rdi
	andq	$0xf,%rdx
	je	.L_done_1_amivrujEyduiFoi

.L_steal_cipher_1_amivrujEyduiFoi:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm9,%xmm10
	vmovdqa	16(%rsp),%xmm9
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	addq	$0x10,%rsi
	vmovdqa	%xmm10,%xmm0
	vmovdqa	%xmm1,%xmm8
	jmp	.L_steal_cipher_amivrujEyduiFoi

.L_done_1_amivrujEyduiFoi:
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	addq	$0x10,%rsi
	vmovdqa	%xmm1,%xmm8
	jmp	.L_done_amivrujEyduiFoi
.cfi_endproc	
.globl	aesni_xts_256_encrypt_avx512
.hidden	aesni_xts_256_encrypt_avx512
.type	aesni_xts_256_encrypt_avx512,@function
.align	32
aesni_xts_256_encrypt_avx512:
.cfi_startproc	
.byte	243,15,30,250
	pushq	%rbp
	movq	%rsp,%rbp
	subq	$136,%rsp
	andq	$0xffffffffffffffc0,%rsp
	movq	%rbx,128(%rsp)
	movq	$0x87,%r10
	vmovdqu	(%r9),%xmm1
	vpxor	(%r8),%xmm1,%xmm1
	vaesenc	16(%r8),%xmm1,%xmm1
	vaesenc	32(%r8),%xmm1,%xmm1
	vaesenc	48(%r8),%xmm1,%xmm1
	vaesenc	64(%r8),%xmm1,%xmm1
	vaesenc	80(%r8),%xmm1,%xmm1
	vaesenc	96(%r8),%xmm1,%xmm1
	vaesenc	112(%r8),%xmm1,%xmm1
	vaesenc	128(%r8),%xmm1,%xmm1
	vaesenc	144(%r8),%xmm1,%xmm1
	vaesenc	160(%r8),%xmm1,%xmm1
	vaesenc	176(%r8),%xmm1,%xmm1
	vaesenc	192(%r8),%xmm1,%xmm1
	vaesenc	208(%r8),%xmm1,%xmm1
	vaesenclast	224(%r8),%xmm1,%xmm1
	vmovdqa	%xmm1,(%rsp)

	cmpq	$0x80,%rdx
	jl	.L_less_than_128_bytes_wcpqaDvsGlbjGoe
	vpbroadcastq	%r10,%zmm25
	cmpq	$0x100,%rdx
	jge	.L_start_by16_wcpqaDvsGlbjGoe
	cmpq	$0x80,%rdx
	jge	.L_start_by8_wcpqaDvsGlbjGoe

.L_do_n_blocks_wcpqaDvsGlbjGoe:
	cmpq	$0x0,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	cmpq	$0x70,%rdx
	jge	.L_remaining_num_blocks_is_7_wcpqaDvsGlbjGoe
	cmpq	$0x60,%rdx
	jge	.L_remaining_num_blocks_is_6_wcpqaDvsGlbjGoe
	cmpq	$0x50,%rdx
	jge	.L_remaining_num_blocks_is_5_wcpqaDvsGlbjGoe
	cmpq	$0x40,%rdx
	jge	.L_remaining_num_blocks_is_4_wcpqaDvsGlbjGoe
	cmpq	$0x30,%rdx
	jge	.L_remaining_num_blocks_is_3_wcpqaDvsGlbjGoe
	cmpq	$0x20,%rdx
	jge	.L_remaining_num_blocks_is_2_wcpqaDvsGlbjGoe
	cmpq	$0x10,%rdx
	jge	.L_remaining_num_blocks_is_1_wcpqaDvsGlbjGoe
	vmovdqa	%xmm0,%xmm8
	vmovdqa	%xmm9,%xmm0
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe

.L_remaining_num_blocks_is_7_wcpqaDvsGlbjGoe:
	movq	$0x0000ffffffffffff,%r8
	kmovq	%r8,%k1
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2{%k1}
	addq	$0x70,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	addq	$0x70,%rsi
	vextracti32x4	$0x2,%zmm2,%xmm8
	vextracti32x4	$0x3,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe

.L_remaining_num_blocks_is_6_wcpqaDvsGlbjGoe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%ymm2
	addq	$0x60,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	addq	$0x60,%rsi
	vextracti32x4	$0x1,%zmm2,%xmm8
	vextracti32x4	$0x2,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe

.L_remaining_num_blocks_is_5_wcpqaDvsGlbjGoe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu	64(%rdi),%xmm2
	addq	$0x50,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu	%xmm2,64(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm2,%xmm8
	vextracti32x4	$0x1,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe

.L_remaining_num_blocks_is_4_wcpqaDvsGlbjGoe:
	vmovdqu8	(%rdi),%zmm1
	addq	$0x40,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,(%rsi)
	addq	$0x40,%rsi
	vextracti32x4	$0x3,%zmm1,%xmm8
	vmovdqa64	%xmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_remaining_num_blocks_is_3_wcpqaDvsGlbjGoe:
	movq	$-1,%r8
	shrq	$0x10,%r8
	kmovq	%r8,%k1
	vmovdqu8	(%rdi),%zmm1{%k1}
	addq	$0x30,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,(%rsi){%k1}
	addq	$0x30,%rsi
	vextracti32x4	$0x2,%zmm1,%xmm8
	vextracti32x4	$0x3,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_remaining_num_blocks_is_2_wcpqaDvsGlbjGoe:
	vmovdqu8	(%rdi),%ymm1
	addq	$0x20,%rdi
	vbroadcasti32x4	(%rcx),%ymm0
	vpternlogq	$0x96,%ymm0,%ymm9,%ymm1
	vbroadcasti32x4	16(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	32(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	48(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	64(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	80(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	96(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	112(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	128(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	144(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	160(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	176(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	192(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	208(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	224(%rcx),%ymm0
	vaesenclast	%ymm0,%ymm1,%ymm1
	vpxorq	%ymm9,%ymm1,%ymm1
	vmovdqu	%ymm1,(%rsi)
	addq	$0x20,%rsi
	vextracti32x4	$0x1,%zmm1,%xmm8
	vextracti32x4	$0x2,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_remaining_num_blocks_is_1_wcpqaDvsGlbjGoe:
	vmovdqu	(%rdi),%xmm1
	addq	$0x10,%rdi
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	(%rcx),%xmm1,%xmm1
	vaesenc	16(%rcx),%xmm1,%xmm1
	vaesenc	32(%rcx),%xmm1,%xmm1
	vaesenc	48(%rcx),%xmm1,%xmm1
	vaesenc	64(%rcx),%xmm1,%xmm1
	vaesenc	80(%rcx),%xmm1,%xmm1
	vaesenc	96(%rcx),%xmm1,%xmm1
	vaesenc	112(%rcx),%xmm1,%xmm1
	vaesenc	128(%rcx),%xmm1,%xmm1
	vaesenc	144(%rcx),%xmm1,%xmm1
	vaesenc	160(%rcx),%xmm1,%xmm1
	vaesenc	176(%rcx),%xmm1,%xmm1
	vaesenc	192(%rcx),%xmm1,%xmm1
	vaesenc	208(%rcx),%xmm1,%xmm1
	vaesenclast	224(%rcx),%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	%xmm1,(%rsi)
	addq	$0x10,%rsi
	vmovdqa	%xmm1,%xmm8
	vextracti32x4	$0x1,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe


.L_start_by16_wcpqaDvsGlbjGoe:
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm11
	vpxord	%zmm14,%zmm11,%zmm11
	vpsrldq	$0xf,%zmm10,%zmm15
	vpclmulqdq	$0x0,%zmm25,%zmm15,%zmm16
	vpslldq	$0x1,%zmm10,%zmm12
	vpxord	%zmm16,%zmm12,%zmm12

.L_main_loop_run_16_wcpqaDvsGlbjGoe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	vmovdqu8	128(%rdi),%zmm3
	vmovdqu8	192(%rdi),%zmm4
	addq	$0x100,%rdi
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4
	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm0,%zmm3,%zmm3
	vpxorq	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm11,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm11,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm12,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm12,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm15,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm15,%zmm17
	vpxord	%zmm14,%zmm17,%zmm17
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm16,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm16,%zmm18
	vpxord	%zmm14,%zmm18,%zmm18
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vaesenc	%zmm0,%zmm3,%zmm3
	vaesenc	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vaesenclast	%zmm0,%zmm3,%zmm3
	vaesenclast	%zmm0,%zmm4,%zmm4
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4

	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqa32	%zmm17,%zmm11
	vmovdqa32	%zmm18,%zmm12
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	vmovdqu8	%zmm3,128(%rsi)
	vmovdqu8	%zmm4,192(%rsi)
	addq	$0x100,%rsi
	subq	$0x100,%rdx
	cmpq	$0x100,%rdx
	jae	.L_main_loop_run_16_wcpqaDvsGlbjGoe
	cmpq	$0x80,%rdx
	jae	.L_main_loop_run_8_wcpqaDvsGlbjGoe
	vextracti32x4	$0x3,%zmm4,%xmm0
	jmp	.L_do_n_blocks_wcpqaDvsGlbjGoe

.L_start_by8_wcpqaDvsGlbjGoe:
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10

.L_main_loop_run_8_wcpqaDvsGlbjGoe:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	addq	$0x80,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2
	vpsrldq	$0xf,%zmm10,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm10,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	addq	$0x80,%rsi
	subq	$0x80,%rdx
	cmpq	$0x80,%rdx
	jae	.L_main_loop_run_8_wcpqaDvsGlbjGoe
	vextracti32x4	$0x3,%zmm2,%xmm0
	jmp	.L_do_n_blocks_wcpqaDvsGlbjGoe

.L_steal_cipher_wcpqaDvsGlbjGoe:
	vmovdqa	%xmm8,%xmm2
	leaq	vpshufb_shf_table(%rip),%rax
	vmovdqu	(%rax,%rdx,1),%xmm10
	vpshufb	%xmm10,%xmm8,%xmm8
	vmovdqu	-16(%rdi,%rdx,1),%xmm3
	vmovdqu	%xmm8,-16(%rsi,%rdx,1)
	leaq	vpshufb_shf_table(%rip),%rax
	addq	$16,%rax
	subq	%rdx,%rax
	vmovdqu	(%rax),%xmm10
	vpxor	mask1(%rip),%xmm10,%xmm10
	vpshufb	%xmm10,%xmm3,%xmm3
	vpblendvb	%xmm10,%xmm2,%xmm3,%xmm3
	vpxor	%xmm0,%xmm3,%xmm8
	vpxor	(%rcx),%xmm8,%xmm8
	vaesenc	16(%rcx),%xmm8,%xmm8
	vaesenc	32(%rcx),%xmm8,%xmm8
	vaesenc	48(%rcx),%xmm8,%xmm8
	vaesenc	64(%rcx),%xmm8,%xmm8
	vaesenc	80(%rcx),%xmm8,%xmm8
	vaesenc	96(%rcx),%xmm8,%xmm8
	vaesenc	112(%rcx),%xmm8,%xmm8
	vaesenc	128(%rcx),%xmm8,%xmm8
	vaesenc	144(%rcx),%xmm8,%xmm8
	vaesenc	160(%rcx),%xmm8,%xmm8
	vaesenc	176(%rcx),%xmm8,%xmm8
	vaesenc	192(%rcx),%xmm8,%xmm8
	vaesenc	208(%rcx),%xmm8,%xmm8
	vaesenclast	224(%rcx),%xmm8,%xmm8
	vpxor	%xmm0,%xmm8,%xmm8
	vmovdqu	%xmm8,-16(%rsi)
.L_ret_wcpqaDvsGlbjGoe:
	movq	128(%rsp),%rbx
	xorq	%r8,%r8
	movq	%r8,128(%rsp)

	vpxorq	%zmm0,%zmm0,%zmm0
	movq	%rbp,%rsp
	popq	%rbp
	vzeroupper
	.byte	0xf3,0xc3

.L_less_than_128_bytes_wcpqaDvsGlbjGoe:
	vpbroadcastq	%r10,%zmm25
	cmpq	$0x10,%rdx
	jb	.L_ret_wcpqaDvsGlbjGoe
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movl	$0xaa,%r8d
	kmovq	%r8,%k2
	movq	%rdx,%r8
	andq	$0x70,%r8
	cmpq	$0x60,%r8
	je	.L_num_blocks_is_6_wcpqaDvsGlbjGoe
	cmpq	$0x50,%r8
	je	.L_num_blocks_is_5_wcpqaDvsGlbjGoe
	cmpq	$0x40,%r8
	je	.L_num_blocks_is_4_wcpqaDvsGlbjGoe
	cmpq	$0x30,%r8
	je	.L_num_blocks_is_3_wcpqaDvsGlbjGoe
	cmpq	$0x20,%r8
	je	.L_num_blocks_is_2_wcpqaDvsGlbjGoe
	cmpq	$0x10,%r8
	je	.L_num_blocks_is_1_wcpqaDvsGlbjGoe

.L_num_blocks_is_7_wcpqaDvsGlbjGoe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	movq	$0x0000ffffffffffff,%r8
	kmovq	%r8,%k1
	vmovdqu8	0(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2{%k1}

	addq	$0x70,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,0(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	addq	$0x70,%rsi
	vextracti32x4	$0x2,%zmm2,%xmm8
	vextracti32x4	$0x3,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_num_blocks_is_6_wcpqaDvsGlbjGoe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vmovdqu8	0(%rdi),%zmm1
	vmovdqu8	64(%rdi),%ymm2
	addq	$96,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,0(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	addq	$96,%rsi

	vextracti32x4	$0x1,%ymm2,%xmm8
	vextracti32x4	$0x2,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_num_blocks_is_5_wcpqaDvsGlbjGoe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vmovdqu8	0(%rdi),%zmm1
	vmovdqu8	64(%rdi),%xmm2
	addq	$80,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vpternlogq	$0x96,%zmm0,%zmm10,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vaesenc	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vaesenclast	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vmovdqu8	%zmm1,0(%rsi)
	vmovdqu8	%xmm2,64(%rsi)
	addq	$80,%rsi

	vmovdqa	%xmm2,%xmm8
	vextracti32x4	$0x1,%zmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_num_blocks_is_4_wcpqaDvsGlbjGoe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x00,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10
	vmovdqu8	0(%rdi),%zmm1
	addq	$64,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,0(%rsi)
	addq	$64,%rsi
	vextracti32x4	$0x3,%zmm1,%xmm8
	vmovdqa	%xmm10,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_num_blocks_is_3_wcpqaDvsGlbjGoe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9
	movq	$0x0000ffffffffffff,%r8
	kmovq	%r8,%k1
	vmovdqu8	0(%rdi),%zmm1{%k1}
	addq	$48,%rdi
	vbroadcasti32x4	(%rcx),%zmm0
	vpternlogq	$0x96,%zmm0,%zmm9,%zmm1
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	176(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	192(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	208(%rcx),%zmm0
	vaesenc	%zmm0,%zmm1,%zmm1
	vbroadcasti32x4	224(%rcx),%zmm0
	vaesenclast	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm9,%zmm1,%zmm1
	vmovdqu8	%zmm1,0(%rsi){%k1}
	addq	$48,%rsi
	vextracti32x4	$2,%zmm1,%xmm8
	vextracti32x4	$3,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_num_blocks_is_2_wcpqaDvsGlbjGoe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9

	vmovdqu8	0(%rdi),%ymm1
	addq	$32,%rdi
	vbroadcasti32x4	(%rcx),%ymm0
	vpternlogq	$0x96,%ymm0,%ymm9,%ymm1
	vbroadcasti32x4	16(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	32(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	48(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	64(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	80(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	96(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	112(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	128(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	144(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	160(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	176(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	192(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	208(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	224(%rcx),%ymm0
	vaesenclast	%ymm0,%ymm1,%ymm1
	vpxorq	%ymm9,%ymm1,%ymm1
	vmovdqu8	%ymm1,0(%rsi)
	addq	$32,%rsi

	vextracti32x4	$1,%ymm1,%xmm8
	vextracti32x4	$2,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.L_num_blocks_is_1_wcpqaDvsGlbjGoe:
	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x00,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9

	vmovdqu8	0(%rdi),%xmm1
	addq	$16,%rdi
	vbroadcasti32x4	(%rcx),%ymm0
	vpternlogq	$0x96,%ymm0,%ymm9,%ymm1
	vbroadcasti32x4	16(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	32(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	48(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	64(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	80(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	96(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	112(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	128(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	144(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	160(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	176(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	192(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	208(%rcx),%ymm0
	vaesenc	%ymm0,%ymm1,%ymm1
	vbroadcasti32x4	224(%rcx),%ymm0
	vaesenclast	%ymm0,%ymm1,%ymm1
	vpxorq	%ymm9,%ymm1,%ymm1
	vmovdqu8	%xmm1,0(%rsi)
	addq	$16,%rsi

	vmovdqa	%xmm1,%xmm8
	vextracti32x4	$1,%zmm9,%xmm0
	andq	$0xf,%rdx
	je	.L_ret_wcpqaDvsGlbjGoe
	jmp	.L_steal_cipher_wcpqaDvsGlbjGoe
.cfi_endproc	
.globl	aesni_xts_256_decrypt_avx512
.hidden	aesni_xts_256_decrypt_avx512
.type	aesni_xts_256_decrypt_avx512,@function
.align	32
aesni_xts_256_decrypt_avx512:
.cfi_startproc	
.byte	243,15,30,250
	pushq	%rbp
	movq	%rsp,%rbp
	subq	$136,%rsp
	andq	$0xffffffffffffffc0,%rsp
	movq	%rbx,128(%rsp)
	movq	$0x87,%r10
	vmovdqu	(%r9),%xmm1
	vpxor	(%r8),%xmm1,%xmm1
	vaesenc	16(%r8),%xmm1,%xmm1
	vaesenc	32(%r8),%xmm1,%xmm1
	vaesenc	48(%r8),%xmm1,%xmm1
	vaesenc	64(%r8),%xmm1,%xmm1
	vaesenc	80(%r8),%xmm1,%xmm1
	vaesenc	96(%r8),%xmm1,%xmm1
	vaesenc	112(%r8),%xmm1,%xmm1
	vaesenc	128(%r8),%xmm1,%xmm1
	vaesenc	144(%r8),%xmm1,%xmm1
	vaesenc	160(%r8),%xmm1,%xmm1
	vaesenc	176(%r8),%xmm1,%xmm1
	vaesenc	192(%r8),%xmm1,%xmm1
	vaesenc	208(%r8),%xmm1,%xmm1
	vaesenclast	224(%r8),%xmm1,%xmm1
	vmovdqa	%xmm1,(%rsp)

	cmpq	$0x80,%rdx
	jb	.L_less_than_128_bytes_EmbgEptodyewbFa
	vpbroadcastq	%r10,%zmm25
	cmpq	$0x100,%rdx
	jge	.L_start_by16_EmbgEptodyewbFa
	jmp	.L_start_by8_EmbgEptodyewbFa

.L_do_n_blocks_EmbgEptodyewbFa:
	cmpq	$0x0,%rdx
	je	.L_ret_EmbgEptodyewbFa
	cmpq	$0x70,%rdx
	jge	.L_remaining_num_blocks_is_7_EmbgEptodyewbFa
	cmpq	$0x60,%rdx
	jge	.L_remaining_num_blocks_is_6_EmbgEptodyewbFa
	cmpq	$0x50,%rdx
	jge	.L_remaining_num_blocks_is_5_EmbgEptodyewbFa
	cmpq	$0x40,%rdx
	jge	.L_remaining_num_blocks_is_4_EmbgEptodyewbFa
	cmpq	$0x30,%rdx
	jge	.L_remaining_num_blocks_is_3_EmbgEptodyewbFa
	cmpq	$0x20,%rdx
	jge	.L_remaining_num_blocks_is_2_EmbgEptodyewbFa
	cmpq	$0x10,%rdx
	jge	.L_remaining_num_blocks_is_1_EmbgEptodyewbFa


	vmovdqu	%xmm5,%xmm1

	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	%xmm1,-16(%rsi)
	vmovdqa	%xmm1,%xmm8


	movq	$0x1,%r8
	kmovq	%r8,%k1
	vpsllq	$0x3f,%xmm9,%xmm13
	vpsraq	$0x3f,%xmm13,%xmm14
	vpandq	%xmm25,%xmm14,%xmm5
	vpxorq	%xmm5,%xmm9,%xmm9{%k1}
	vpsrldq	$0x8,%xmm9,%xmm10
.byte	98, 211, 181, 8, 115, 194, 1
	vpslldq	$0x8,%xmm13,%xmm13
	vpxorq	%xmm13,%xmm0,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_remaining_num_blocks_is_7_EmbgEptodyewbFa:
	movq	$0xffffffffffffffff,%r8
	shrq	$0x10,%r8
	kmovq	%r8,%k1
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2{%k1}
	addq	$0x70,%rdi
	andq	$0xf,%rdx
	je	.L_done_7_remain_EmbgEptodyewbFa
	vextracti32x4	$0x2,%zmm10,%xmm12
	vextracti32x4	$0x3,%zmm10,%xmm13
	vinserti32x4	$0x2,%xmm13,%zmm10,%zmm10

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	addq	$0x70,%rsi
	vextracti32x4	$0x2,%zmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_7_remain_EmbgEptodyewbFa:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi){%k1}
	jmp	.L_ret_EmbgEptodyewbFa

.L_remaining_num_blocks_is_6_EmbgEptodyewbFa:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%ymm2
	addq	$0x60,%rdi
	andq	$0xf,%rdx
	je	.L_done_6_remain_EmbgEptodyewbFa
	vextracti32x4	$0x1,%zmm10,%xmm12
	vextracti32x4	$0x2,%zmm10,%xmm13
	vinserti32x4	$0x1,%xmm13,%zmm10,%zmm10

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	addq	$0x60,%rsi
	vextracti32x4	$0x1,%zmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_6_remain_EmbgEptodyewbFa:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%ymm2,64(%rsi)
	jmp	.L_ret_EmbgEptodyewbFa

.L_remaining_num_blocks_is_5_EmbgEptodyewbFa:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu	64(%rdi),%xmm2
	addq	$0x50,%rdi
	andq	$0xf,%rdx
	je	.L_done_5_remain_EmbgEptodyewbFa
	vmovdqa	%xmm10,%xmm12
	vextracti32x4	$0x1,%zmm10,%xmm10

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu	%xmm2,64(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_5_remain_EmbgEptodyewbFa:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%xmm2,64(%rsi)
	jmp	.L_ret_EmbgEptodyewbFa

.L_remaining_num_blocks_is_4_EmbgEptodyewbFa:
	vmovdqu8	(%rdi),%zmm1
	addq	$0x40,%rdi
	andq	$0xf,%rdx
	je	.L_done_4_remain_EmbgEptodyewbFa
	vextracti32x4	$0x3,%zmm9,%xmm12
	vinserti32x4	$0x3,%xmm10,%zmm9,%zmm9

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	addq	$0x40,%rsi
	vextracti32x4	$0x3,%zmm1,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_4_remain_EmbgEptodyewbFa:

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	jmp	.L_ret_EmbgEptodyewbFa

.L_remaining_num_blocks_is_3_EmbgEptodyewbFa:
	vmovdqu	(%rdi),%xmm1
	vmovdqu	16(%rdi),%xmm2
	vmovdqu	32(%rdi),%xmm3
	addq	$0x30,%rdi
	andq	$0xf,%rdx
	je	.L_done_3_remain_EmbgEptodyewbFa
	vextracti32x4	$0x2,%zmm9,%xmm13
	vextracti32x4	$0x1,%zmm9,%xmm10
	vextracti32x4	$0x3,%zmm9,%xmm11
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	addq	$0x30,%rsi
	vmovdqa	%xmm3,%xmm8
	vmovdqa	%xmm13,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_3_remain_EmbgEptodyewbFa:
	vextracti32x4	$0x1,%zmm9,%xmm10
	vextracti32x4	$0x2,%zmm9,%xmm11
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	jmp	.L_ret_EmbgEptodyewbFa

.L_remaining_num_blocks_is_2_EmbgEptodyewbFa:
	vmovdqu	(%rdi),%xmm1
	vmovdqu	16(%rdi),%xmm2
	addq	$0x20,%rdi
	andq	$0xf,%rdx
	je	.L_done_2_remain_EmbgEptodyewbFa
	vextracti32x4	$0x2,%zmm9,%xmm10
	vextracti32x4	$0x1,%zmm9,%xmm12
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	addq	$0x20,%rsi
	vmovdqa	%xmm2,%xmm8
	vmovdqa	%xmm12,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_2_remain_EmbgEptodyewbFa:
	vextracti32x4	$0x1,%zmm9,%xmm10
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	jmp	.L_ret_EmbgEptodyewbFa

.L_remaining_num_blocks_is_1_EmbgEptodyewbFa:
	vmovdqu	(%rdi),%xmm1
	addq	$0x10,%rdi
	andq	$0xf,%rdx
	je	.L_done_1_remain_EmbgEptodyewbFa
	vextracti32x4	$0x1,%zmm9,%xmm11
	vpxor	%xmm11,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm11,%xmm1,%xmm1
	vmovdqu	%xmm1,(%rsi)
	addq	$0x10,%rsi
	vmovdqa	%xmm1,%xmm8
	vmovdqa	%xmm9,%xmm0
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_1_remain_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	%xmm1,(%rsi)
	jmp	.L_ret_EmbgEptodyewbFa

.L_start_by16_EmbgEptodyewbFa:
	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2


	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9


	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10


	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm11
	vpxord	%zmm14,%zmm11,%zmm11

	vpsrldq	$0xf,%zmm10,%zmm15
	vpclmulqdq	$0x0,%zmm25,%zmm15,%zmm16
	vpslldq	$0x1,%zmm10,%zmm12
	vpxord	%zmm16,%zmm12,%zmm12

.L_main_loop_run_16_EmbgEptodyewbFa:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	vmovdqu8	128(%rdi),%zmm3
	vmovdqu8	192(%rdi),%zmm4
	vmovdqu8	240(%rdi),%xmm5
	addq	$0x100,%rdi
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4
	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vpxorq	%zmm0,%zmm3,%zmm3
	vpxorq	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm11,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm11,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm12,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm12,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16
	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm15,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm15,%zmm17
	vpxord	%zmm14,%zmm17,%zmm17
	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vpsrldq	$0xf,%zmm16,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm16,%zmm18
	vpxord	%zmm14,%zmm18,%zmm18
	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vaesdec	%zmm0,%zmm3,%zmm3
	vaesdec	%zmm0,%zmm4,%zmm4
	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2
	vaesdeclast	%zmm0,%zmm3,%zmm3
	vaesdeclast	%zmm0,%zmm4,%zmm4
	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2
	vpxorq	%zmm11,%zmm3,%zmm3
	vpxorq	%zmm12,%zmm4,%zmm4

	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqa32	%zmm17,%zmm11
	vmovdqa32	%zmm18,%zmm12
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	vmovdqu8	%zmm3,128(%rsi)
	vmovdqu8	%zmm4,192(%rsi)
	addq	$0x100,%rsi
	subq	$0x100,%rdx
	cmpq	$0x100,%rdx
	jge	.L_main_loop_run_16_EmbgEptodyewbFa

	cmpq	$0x80,%rdx
	jge	.L_main_loop_run_8_EmbgEptodyewbFa
	jmp	.L_do_n_blocks_EmbgEptodyewbFa

.L_start_by8_EmbgEptodyewbFa:

	vbroadcasti32x4	(%rsp),%zmm0
	vbroadcasti32x4	shufb_15_7(%rip),%zmm8
	movq	$0xaa,%r8
	kmovq	%r8,%k2


	vpshufb	%zmm8,%zmm0,%zmm1
	vpsllvq	const_dq3210(%rip),%zmm0,%zmm4
	vpsrlvq	const_dq5678(%rip),%zmm1,%zmm2
	vpclmulqdq	$0x0,%zmm25,%zmm2,%zmm3
	vpxorq	%zmm2,%zmm4,%zmm4{%k2}
	vpxord	%zmm4,%zmm3,%zmm9


	vpsllvq	const_dq7654(%rip),%zmm0,%zmm5
	vpsrlvq	const_dq1234(%rip),%zmm1,%zmm6
	vpclmulqdq	$0x0,%zmm25,%zmm6,%zmm7
	vpxorq	%zmm6,%zmm5,%zmm5{%k2}
	vpxord	%zmm5,%zmm7,%zmm10

.L_main_loop_run_8_EmbgEptodyewbFa:
	vmovdqu8	(%rdi),%zmm1
	vmovdqu8	64(%rdi),%zmm2
	vmovdqu8	112(%rdi),%xmm5
	addq	$0x80,%rdi

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vbroadcasti32x4	(%rcx),%zmm0
	vpxorq	%zmm0,%zmm1,%zmm1
	vpxorq	%zmm0,%zmm2,%zmm2
	vpsrldq	$0xf,%zmm9,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm9,%zmm15
	vpxord	%zmm14,%zmm15,%zmm15
	vbroadcasti32x4	16(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	32(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	48(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2
	vpsrldq	$0xf,%zmm10,%zmm13
	vpclmulqdq	$0x0,%zmm25,%zmm13,%zmm14
	vpslldq	$0x1,%zmm10,%zmm16
	vpxord	%zmm14,%zmm16,%zmm16

	vbroadcasti32x4	64(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	80(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	96(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	112(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	128(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	144(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	160(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	176(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	192(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	208(%rcx),%zmm0
	vaesdec	%zmm0,%zmm1,%zmm1
	vaesdec	%zmm0,%zmm2,%zmm2


	vbroadcasti32x4	224(%rcx),%zmm0
	vaesdeclast	%zmm0,%zmm1,%zmm1
	vaesdeclast	%zmm0,%zmm2,%zmm2

	vpxorq	%zmm9,%zmm1,%zmm1
	vpxorq	%zmm10,%zmm2,%zmm2


	vmovdqa32	%zmm15,%zmm9
	vmovdqa32	%zmm16,%zmm10
	vmovdqu8	%zmm1,(%rsi)
	vmovdqu8	%zmm2,64(%rsi)
	addq	$0x80,%rsi
	subq	$0x80,%rdx
	cmpq	$0x80,%rdx
	jge	.L_main_loop_run_8_EmbgEptodyewbFa
	jmp	.L_do_n_blocks_EmbgEptodyewbFa

.L_steal_cipher_EmbgEptodyewbFa:

	vmovdqa	%xmm8,%xmm2


	leaq	vpshufb_shf_table(%rip),%rax
	vmovdqu	(%rax,%rdx,1),%xmm10
	vpshufb	%xmm10,%xmm8,%xmm8


	vmovdqu	-16(%rdi,%rdx,1),%xmm3
	vmovdqu	%xmm8,-16(%rsi,%rdx,1)


	leaq	vpshufb_shf_table(%rip),%rax
	addq	$16,%rax
	subq	%rdx,%rax
	vmovdqu	(%rax),%xmm10
	vpxor	mask1(%rip),%xmm10,%xmm10
	vpshufb	%xmm10,%xmm3,%xmm3

	vpblendvb	%xmm10,%xmm2,%xmm3,%xmm3


	vpxor	%xmm0,%xmm3,%xmm8


	vpxor	(%rcx),%xmm8,%xmm8
	vaesdec	16(%rcx),%xmm8,%xmm8
	vaesdec	32(%rcx),%xmm8,%xmm8
	vaesdec	48(%rcx),%xmm8,%xmm8
	vaesdec	64(%rcx),%xmm8,%xmm8
	vaesdec	80(%rcx),%xmm8,%xmm8
	vaesdec	96(%rcx),%xmm8,%xmm8
	vaesdec	112(%rcx),%xmm8,%xmm8
	vaesdec	128(%rcx),%xmm8,%xmm8
	vaesdec	144(%rcx),%xmm8,%xmm8
	vaesdec	160(%rcx),%xmm8,%xmm8
	vaesdec	176(%rcx),%xmm8,%xmm8
	vaesdec	192(%rcx),%xmm8,%xmm8
	vaesdec	208(%rcx),%xmm8,%xmm8
	vaesdeclast	224(%rcx),%xmm8,%xmm8

	vpxor	%xmm0,%xmm8,%xmm8

.L_done_EmbgEptodyewbFa:

	vmovdqu	%xmm8,-16(%rsi)
.L_ret_EmbgEptodyewbFa:
	movq	128(%rsp),%rbx
	xorq	%r8,%r8
	movq	%r8,128(%rsp)

	vpxorq	%zmm0,%zmm0,%zmm0
	movq	%rbp,%rsp
	popq	%rbp
	vzeroupper
	.byte	0xf3,0xc3

.L_less_than_128_bytes_EmbgEptodyewbFa:
	cmpq	$0x10,%rdx
	jb	.L_ret_EmbgEptodyewbFa

	movq	%rdx,%r8
	andq	$0x70,%r8
	cmpq	$0x60,%r8
	je	.L_num_blocks_is_6_EmbgEptodyewbFa
	cmpq	$0x50,%r8
	je	.L_num_blocks_is_5_EmbgEptodyewbFa
	cmpq	$0x40,%r8
	je	.L_num_blocks_is_4_EmbgEptodyewbFa
	cmpq	$0x30,%r8
	je	.L_num_blocks_is_3_EmbgEptodyewbFa
	cmpq	$0x20,%r8
	je	.L_num_blocks_is_2_EmbgEptodyewbFa
	cmpq	$0x10,%r8
	je	.L_num_blocks_is_1_EmbgEptodyewbFa

.L_num_blocks_is_7_EmbgEptodyewbFa:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,64(%rsp)
	movq	%rbx,64 + 8(%rsp)
	vmovdqa	64(%rsp),%xmm13
	vmovdqu	64(%rdi),%xmm5
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,80(%rsp)
	movq	%rbx,80 + 8(%rsp)
	vmovdqa	80(%rsp),%xmm14
	vmovdqu	80(%rdi),%xmm6
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,96(%rsp)
	movq	%rbx,96 + 8(%rsp)
	vmovdqa	96(%rsp),%xmm15
	vmovdqu	96(%rdi),%xmm7
	addq	$0x70,%rdi
	andq	$0xf,%rdx
	je	.L_done_7_EmbgEptodyewbFa

.L_steal_cipher_7_EmbgEptodyewbFa:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm15,%xmm16
	vmovdqa	16(%rsp),%xmm15
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vpxor	%xmm0,%xmm7,%xmm7
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vaesdeclast	%xmm0,%xmm7,%xmm7
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	vmovdqu	%xmm6,80(%rsi)
	addq	$0x70,%rsi
	vmovdqa64	%xmm16,%xmm0
	vmovdqa	%xmm7,%xmm8
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_7_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vpxor	%xmm0,%xmm7,%xmm7
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vaesdec	%xmm0,%xmm7,%xmm7
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vaesdeclast	%xmm0,%xmm7,%xmm7
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vpxor	%xmm15,%xmm7,%xmm7
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	vmovdqu	%xmm6,80(%rsi)
	addq	$0x70,%rsi
	vmovdqa	%xmm7,%xmm8
	jmp	.L_done_EmbgEptodyewbFa

.L_num_blocks_is_6_EmbgEptodyewbFa:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,64(%rsp)
	movq	%rbx,64 + 8(%rsp)
	vmovdqa	64(%rsp),%xmm13
	vmovdqu	64(%rdi),%xmm5
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,80(%rsp)
	movq	%rbx,80 + 8(%rsp)
	vmovdqa	80(%rsp),%xmm14
	vmovdqu	80(%rdi),%xmm6
	addq	$0x60,%rdi
	andq	$0xf,%rdx
	je	.L_done_6_EmbgEptodyewbFa

.L_steal_cipher_6_EmbgEptodyewbFa:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm14,%xmm15
	vmovdqa	16(%rsp),%xmm14
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	addq	$0x60,%rsi
	vmovdqa	%xmm15,%xmm0
	vmovdqa	%xmm6,%xmm8
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_6_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vpxor	%xmm0,%xmm6,%xmm6
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vaesdec	%xmm0,%xmm6,%xmm6
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vaesdeclast	%xmm0,%xmm6,%xmm6
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vpxor	%xmm14,%xmm6,%xmm6
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	vmovdqu	%xmm5,64(%rsi)
	addq	$0x60,%rsi
	vmovdqa	%xmm6,%xmm8
	jmp	.L_done_EmbgEptodyewbFa

.L_num_blocks_is_5_EmbgEptodyewbFa:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,64(%rsp)
	movq	%rbx,64 + 8(%rsp)
	vmovdqa	64(%rsp),%xmm13
	vmovdqu	64(%rdi),%xmm5
	addq	$0x50,%rdi
	andq	$0xf,%rdx
	je	.L_done_5_EmbgEptodyewbFa

.L_steal_cipher_5_EmbgEptodyewbFa:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm13,%xmm14
	vmovdqa	16(%rsp),%xmm13
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm14,%xmm0
	vmovdqa	%xmm5,%xmm8
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_5_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vpxor	%xmm0,%xmm5,%xmm5
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vaesdec	%xmm0,%xmm5,%xmm5
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vaesdeclast	%xmm0,%xmm5,%xmm5
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vpxor	%xmm13,%xmm5,%xmm5
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	vmovdqu	%xmm4,48(%rsi)
	addq	$0x50,%rsi
	vmovdqa	%xmm5,%xmm8
	jmp	.L_done_EmbgEptodyewbFa

.L_num_blocks_is_4_EmbgEptodyewbFa:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,48(%rsp)
	movq	%rbx,48 + 8(%rsp)
	vmovdqa	48(%rsp),%xmm12
	vmovdqu	48(%rdi),%xmm4
	addq	$0x40,%rdi
	andq	$0xf,%rdx
	je	.L_done_4_EmbgEptodyewbFa

.L_steal_cipher_4_EmbgEptodyewbFa:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm12,%xmm13
	vmovdqa	16(%rsp),%xmm12
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	addq	$0x40,%rsi
	vmovdqa	%xmm13,%xmm0
	vmovdqa	%xmm4,%xmm8
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_4_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vpxor	%xmm0,%xmm4,%xmm4
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vaesdec	%xmm0,%xmm4,%xmm4
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vaesdeclast	%xmm0,%xmm4,%xmm4
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vpxor	%xmm12,%xmm4,%xmm4
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	vmovdqu	%xmm3,32(%rsi)
	addq	$0x40,%rsi
	vmovdqa	%xmm4,%xmm8
	jmp	.L_done_EmbgEptodyewbFa

.L_num_blocks_is_3_EmbgEptodyewbFa:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,32(%rsp)
	movq	%rbx,32 + 8(%rsp)
	vmovdqa	32(%rsp),%xmm11
	vmovdqu	32(%rdi),%xmm3
	addq	$0x30,%rdi
	andq	$0xf,%rdx
	je	.L_done_3_EmbgEptodyewbFa

.L_steal_cipher_3_EmbgEptodyewbFa:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm11,%xmm12
	vmovdqa	16(%rsp),%xmm11
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	addq	$0x30,%rsi
	vmovdqa	%xmm12,%xmm0
	vmovdqa	%xmm3,%xmm8
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_3_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vpxor	%xmm0,%xmm3,%xmm3
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vaesdec	%xmm0,%xmm3,%xmm3
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vaesdeclast	%xmm0,%xmm3,%xmm3
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vpxor	%xmm11,%xmm3,%xmm3
	vmovdqu	%xmm1,(%rsi)
	vmovdqu	%xmm2,16(%rsi)
	addq	$0x30,%rsi
	vmovdqa	%xmm3,%xmm8
	jmp	.L_done_EmbgEptodyewbFa

.L_num_blocks_is_2_EmbgEptodyewbFa:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,16 + 8(%rsp)
	vmovdqa	16(%rsp),%xmm10
	vmovdqu	16(%rdi),%xmm2
	addq	$0x20,%rdi
	andq	$0xf,%rdx
	je	.L_done_2_EmbgEptodyewbFa

.L_steal_cipher_2_EmbgEptodyewbFa:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm10,%xmm11
	vmovdqa	16(%rsp),%xmm10
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	addq	$0x20,%rsi
	vmovdqa	%xmm11,%xmm0
	vmovdqa	%xmm2,%xmm8
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_2_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vpxor	%xmm0,%xmm2,%xmm2
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vaesdec	%xmm0,%xmm2,%xmm2
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vaesdeclast	%xmm0,%xmm2,%xmm2
	vpxor	%xmm9,%xmm1,%xmm1
	vpxor	%xmm10,%xmm2,%xmm2
	vmovdqu	%xmm1,(%rsi)
	addq	$0x20,%rsi
	vmovdqa	%xmm2,%xmm8
	jmp	.L_done_EmbgEptodyewbFa

.L_num_blocks_is_1_EmbgEptodyewbFa:
	vmovdqa	0(%rsp),%xmm9
	movq	0(%rsp),%rax
	movq	8(%rsp),%rbx
	vmovdqu	0(%rdi),%xmm1
	addq	$0x10,%rdi
	andq	$0xf,%rdx
	je	.L_done_1_EmbgEptodyewbFa

.L_steal_cipher_1_EmbgEptodyewbFa:
	xorq	%r11,%r11
	shlq	$1,%rax
	adcq	%rbx,%rbx
	cmovcq	%r10,%r11
	xorq	%r11,%rax
	movq	%rax,16(%rsp)
	movq	%rbx,24(%rsp)
	vmovdqa64	%xmm9,%xmm10
	vmovdqa	16(%rsp),%xmm9
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	addq	$0x10,%rsi
	vmovdqa	%xmm10,%xmm0
	vmovdqa	%xmm1,%xmm8
	jmp	.L_steal_cipher_EmbgEptodyewbFa

.L_done_1_EmbgEptodyewbFa:
	vpxor	%xmm9,%xmm1,%xmm1
	vmovdqu	(%rcx),%xmm0
	vpxor	%xmm0,%xmm1,%xmm1
	vmovdqu	16(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	32(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	48(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	64(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	80(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	96(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	112(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	128(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	144(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	160(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	176(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	192(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	208(%rcx),%xmm0
	vaesdec	%xmm0,%xmm1,%xmm1
	vmovdqu	224(%rcx),%xmm0
	vaesdeclast	%xmm0,%xmm1,%xmm1
	vpxor	%xmm9,%xmm1,%xmm1
	addq	$0x10,%rsi
	vmovdqa	%xmm1,%xmm8
	jmp	.L_done_EmbgEptodyewbFa
.cfi_endproc	
.section	.rodata
.align	16

vpshufb_shf_table:
.quad	0x8786858483828100, 0x8f8e8d8c8b8a8988
.quad	0x0706050403020100, 0x000e0d0c0b0a0908

mask1:
.quad	0x8080808080808080, 0x8080808080808080

const_dq3210:
.quad	0, 0, 1, 1, 2, 2, 3, 3
const_dq5678:
.quad	8, 8, 7, 7, 6, 6, 5, 5
const_dq7654:
.quad	4, 4, 5, 5, 6, 6, 7, 7
const_dq1234:
.quad	4, 4, 3, 3, 2, 2, 1, 1

shufb_15_7:
.byte	15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 7, 0xff, 0xff
.byte	0xff, 0xff, 0xff, 0xff, 0xff

.text	
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
