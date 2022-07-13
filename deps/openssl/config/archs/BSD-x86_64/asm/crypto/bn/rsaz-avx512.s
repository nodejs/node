
.globl	ossl_rsaz_avx512ifma_eligible
.type	ossl_rsaz_avx512ifma_eligible,@function
.align	32
ossl_rsaz_avx512ifma_eligible:
	movl	OPENSSL_ia32cap_P+8(%rip),%ecx
	xorl	%eax,%eax
	andl	$2149777408,%ecx
	cmpl	$2149777408,%ecx
	cmovel	%ecx,%eax
	.byte	0xf3,0xc3
.size	ossl_rsaz_avx512ifma_eligible, .-ossl_rsaz_avx512ifma_eligible
.text	

.globl	ossl_rsaz_amm52x20_x1_256
.type	ossl_rsaz_amm52x20_x1_256,@function
.align	32
ossl_rsaz_amm52x20_x1_256:
.cfi_startproc	
.byte	243,15,30,250
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-16
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-24
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_adjust_cfa_offset	8
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_adjust_cfa_offset	8
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_adjust_cfa_offset	8
.cfi_offset	%r15,-56
.Lrsaz_amm52x20_x1_256_body:


	vpxord	%ymm0,%ymm0,%ymm0
	vmovdqa64	%ymm0,%ymm1
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm0,%ymm17
	vmovdqa64	%ymm0,%ymm18
	vmovdqa64	%ymm0,%ymm19

	xorl	%r9d,%r9d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax


	movl	$5,%ebx

.align	32
.Lloop5:
	movq	0(%r11),%r13

	vpbroadcastq	%r13,%ymm3
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm4
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm3,%ymm1
	vpmadd52luq	32(%rsi),%ymm3,%ymm16
	vpmadd52luq	64(%rsi),%ymm3,%ymm17
	vpmadd52luq	96(%rsi),%ymm3,%ymm18
	vpmadd52luq	128(%rsi),%ymm3,%ymm19

	vpmadd52luq	0(%rcx),%ymm4,%ymm1
	vpmadd52luq	32(%rcx),%ymm4,%ymm16
	vpmadd52luq	64(%rcx),%ymm4,%ymm17
	vpmadd52luq	96(%rcx),%ymm4,%ymm18
	vpmadd52luq	128(%rcx),%ymm4,%ymm19


	valignq	$1,%ymm1,%ymm16,%ymm1
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm1,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm3,%ymm1
	vpmadd52huq	32(%rsi),%ymm3,%ymm16
	vpmadd52huq	64(%rsi),%ymm3,%ymm17
	vpmadd52huq	96(%rsi),%ymm3,%ymm18
	vpmadd52huq	128(%rsi),%ymm3,%ymm19

	vpmadd52huq	0(%rcx),%ymm4,%ymm1
	vpmadd52huq	32(%rcx),%ymm4,%ymm16
	vpmadd52huq	64(%rcx),%ymm4,%ymm17
	vpmadd52huq	96(%rcx),%ymm4,%ymm18
	vpmadd52huq	128(%rcx),%ymm4,%ymm19
	movq	8(%r11),%r13

	vpbroadcastq	%r13,%ymm3
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm4
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm3,%ymm1
	vpmadd52luq	32(%rsi),%ymm3,%ymm16
	vpmadd52luq	64(%rsi),%ymm3,%ymm17
	vpmadd52luq	96(%rsi),%ymm3,%ymm18
	vpmadd52luq	128(%rsi),%ymm3,%ymm19

	vpmadd52luq	0(%rcx),%ymm4,%ymm1
	vpmadd52luq	32(%rcx),%ymm4,%ymm16
	vpmadd52luq	64(%rcx),%ymm4,%ymm17
	vpmadd52luq	96(%rcx),%ymm4,%ymm18
	vpmadd52luq	128(%rcx),%ymm4,%ymm19


	valignq	$1,%ymm1,%ymm16,%ymm1
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm1,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm3,%ymm1
	vpmadd52huq	32(%rsi),%ymm3,%ymm16
	vpmadd52huq	64(%rsi),%ymm3,%ymm17
	vpmadd52huq	96(%rsi),%ymm3,%ymm18
	vpmadd52huq	128(%rsi),%ymm3,%ymm19

	vpmadd52huq	0(%rcx),%ymm4,%ymm1
	vpmadd52huq	32(%rcx),%ymm4,%ymm16
	vpmadd52huq	64(%rcx),%ymm4,%ymm17
	vpmadd52huq	96(%rcx),%ymm4,%ymm18
	vpmadd52huq	128(%rcx),%ymm4,%ymm19
	movq	16(%r11),%r13

	vpbroadcastq	%r13,%ymm3
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm4
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm3,%ymm1
	vpmadd52luq	32(%rsi),%ymm3,%ymm16
	vpmadd52luq	64(%rsi),%ymm3,%ymm17
	vpmadd52luq	96(%rsi),%ymm3,%ymm18
	vpmadd52luq	128(%rsi),%ymm3,%ymm19

	vpmadd52luq	0(%rcx),%ymm4,%ymm1
	vpmadd52luq	32(%rcx),%ymm4,%ymm16
	vpmadd52luq	64(%rcx),%ymm4,%ymm17
	vpmadd52luq	96(%rcx),%ymm4,%ymm18
	vpmadd52luq	128(%rcx),%ymm4,%ymm19


	valignq	$1,%ymm1,%ymm16,%ymm1
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm1,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm3,%ymm1
	vpmadd52huq	32(%rsi),%ymm3,%ymm16
	vpmadd52huq	64(%rsi),%ymm3,%ymm17
	vpmadd52huq	96(%rsi),%ymm3,%ymm18
	vpmadd52huq	128(%rsi),%ymm3,%ymm19

	vpmadd52huq	0(%rcx),%ymm4,%ymm1
	vpmadd52huq	32(%rcx),%ymm4,%ymm16
	vpmadd52huq	64(%rcx),%ymm4,%ymm17
	vpmadd52huq	96(%rcx),%ymm4,%ymm18
	vpmadd52huq	128(%rcx),%ymm4,%ymm19
	movq	24(%r11),%r13

	vpbroadcastq	%r13,%ymm3
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm4
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm3,%ymm1
	vpmadd52luq	32(%rsi),%ymm3,%ymm16
	vpmadd52luq	64(%rsi),%ymm3,%ymm17
	vpmadd52luq	96(%rsi),%ymm3,%ymm18
	vpmadd52luq	128(%rsi),%ymm3,%ymm19

	vpmadd52luq	0(%rcx),%ymm4,%ymm1
	vpmadd52luq	32(%rcx),%ymm4,%ymm16
	vpmadd52luq	64(%rcx),%ymm4,%ymm17
	vpmadd52luq	96(%rcx),%ymm4,%ymm18
	vpmadd52luq	128(%rcx),%ymm4,%ymm19


	valignq	$1,%ymm1,%ymm16,%ymm1
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm1,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm3,%ymm1
	vpmadd52huq	32(%rsi),%ymm3,%ymm16
	vpmadd52huq	64(%rsi),%ymm3,%ymm17
	vpmadd52huq	96(%rsi),%ymm3,%ymm18
	vpmadd52huq	128(%rsi),%ymm3,%ymm19

	vpmadd52huq	0(%rcx),%ymm4,%ymm1
	vpmadd52huq	32(%rcx),%ymm4,%ymm16
	vpmadd52huq	64(%rcx),%ymm4,%ymm17
	vpmadd52huq	96(%rcx),%ymm4,%ymm18
	vpmadd52huq	128(%rcx),%ymm4,%ymm19
	leaq	32(%r11),%r11
	decl	%ebx
	jne	.Lloop5

	vmovdqa64	.Lmask52x4(%rip),%ymm4

	vpbroadcastq	%r9,%ymm3
	vpblendd	$3,%ymm3,%ymm1,%ymm1



	vpsrlq	$52,%ymm1,%ymm24
	vpsrlq	$52,%ymm16,%ymm25
	vpsrlq	$52,%ymm17,%ymm26
	vpsrlq	$52,%ymm18,%ymm27
	vpsrlq	$52,%ymm19,%ymm28


	valignq	$3,%ymm27,%ymm28,%ymm28
	valignq	$3,%ymm26,%ymm27,%ymm27
	valignq	$3,%ymm25,%ymm26,%ymm26
	valignq	$3,%ymm24,%ymm25,%ymm25
	valignq	$3,%ymm0,%ymm24,%ymm24


	vpandq	%ymm4,%ymm1,%ymm1
	vpandq	%ymm4,%ymm16,%ymm16
	vpandq	%ymm4,%ymm17,%ymm17
	vpandq	%ymm4,%ymm18,%ymm18
	vpandq	%ymm4,%ymm19,%ymm19


	vpaddq	%ymm24,%ymm1,%ymm1
	vpaddq	%ymm25,%ymm16,%ymm16
	vpaddq	%ymm26,%ymm17,%ymm17
	vpaddq	%ymm27,%ymm18,%ymm18
	vpaddq	%ymm28,%ymm19,%ymm19



	vpcmpuq	$1,%ymm1,%ymm4,%k1
	vpcmpuq	$1,%ymm16,%ymm4,%k2
	vpcmpuq	$1,%ymm17,%ymm4,%k3
	vpcmpuq	$1,%ymm18,%ymm4,%k4
	vpcmpuq	$1,%ymm19,%ymm4,%k5
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	kmovb	%k3,%r12d
	kmovb	%k4,%r11d
	kmovb	%k5,%r10d


	vpcmpuq	$0,%ymm1,%ymm4,%k1
	vpcmpuq	$0,%ymm16,%ymm4,%k2
	vpcmpuq	$0,%ymm17,%ymm4,%k3
	vpcmpuq	$0,%ymm18,%ymm4,%k4
	vpcmpuq	$0,%ymm19,%ymm4,%k5
	kmovb	%k1,%r9d
	kmovb	%k2,%r8d
	kmovb	%k3,%ebx
	kmovb	%k4,%ecx
	kmovb	%k5,%edx



	shlb	$4,%r13b
	orb	%r13b,%r14b
	shlb	$4,%r11b
	orb	%r11b,%r12b

	addb	%r14b,%r14b
	adcb	%r12b,%r12b
	adcb	%r10b,%r10b

	shlb	$4,%r8b
	orb	%r8b,%r9b
	shlb	$4,%cl
	orb	%cl,%bl

	addb	%r9b,%r14b
	adcb	%bl,%r12b
	adcb	%dl,%r10b

	xorb	%r9b,%r14b
	xorb	%bl,%r12b
	xorb	%dl,%r10b

	kmovb	%r14d,%k1
	shrb	$4,%r14b
	kmovb	%r14d,%k2
	kmovb	%r12d,%k3
	shrb	$4,%r12b
	kmovb	%r12d,%k4
	kmovb	%r10d,%k5


	vpsubq	%ymm4,%ymm1,%ymm1{%k1}
	vpsubq	%ymm4,%ymm16,%ymm16{%k2}
	vpsubq	%ymm4,%ymm17,%ymm17{%k3}
	vpsubq	%ymm4,%ymm18,%ymm18{%k4}
	vpsubq	%ymm4,%ymm19,%ymm19{%k5}

	vpandq	%ymm4,%ymm1,%ymm1
	vpandq	%ymm4,%ymm16,%ymm16
	vpandq	%ymm4,%ymm17,%ymm17
	vpandq	%ymm4,%ymm18,%ymm18
	vpandq	%ymm4,%ymm19,%ymm19

	vmovdqu64	%ymm1,(%rdi)
	vmovdqu64	%ymm16,32(%rdi)
	vmovdqu64	%ymm17,64(%rdi)
	vmovdqu64	%ymm18,96(%rdi)
	vmovdqu64	%ymm19,128(%rdi)

	vzeroupper
	movq	0(%rsp),%r15
.cfi_restore	%r15
	movq	8(%rsp),%r14
.cfi_restore	%r14
	movq	16(%rsp),%r13
.cfi_restore	%r13
	movq	24(%rsp),%r12
.cfi_restore	%r12
	movq	32(%rsp),%rbp
.cfi_restore	%rbp
	movq	40(%rsp),%rbx
.cfi_restore	%rbx
	leaq	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lrsaz_amm52x20_x1_256_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_rsaz_amm52x20_x1_256, .-ossl_rsaz_amm52x20_x1_256
.data	
.align	32
.Lmask52x4:
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.text	

.globl	ossl_rsaz_amm52x20_x2_256
.type	ossl_rsaz_amm52x20_x2_256,@function
.align	32
ossl_rsaz_amm52x20_x2_256:
.cfi_startproc	
.byte	243,15,30,250
	pushq	%rbx
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbx,-16
	pushq	%rbp
.cfi_adjust_cfa_offset	8
.cfi_offset	%rbp,-24
	pushq	%r12
.cfi_adjust_cfa_offset	8
.cfi_offset	%r12,-32
	pushq	%r13
.cfi_adjust_cfa_offset	8
.cfi_offset	%r13,-40
	pushq	%r14
.cfi_adjust_cfa_offset	8
.cfi_offset	%r14,-48
	pushq	%r15
.cfi_adjust_cfa_offset	8
.cfi_offset	%r15,-56
.Lrsaz_amm52x20_x2_256_body:


	vpxord	%ymm0,%ymm0,%ymm0
	vmovdqa64	%ymm0,%ymm1
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm0,%ymm17
	vmovdqa64	%ymm0,%ymm18
	vmovdqa64	%ymm0,%ymm19
	vmovdqa64	%ymm0,%ymm2
	vmovdqa64	%ymm0,%ymm20
	vmovdqa64	%ymm0,%ymm21
	vmovdqa64	%ymm0,%ymm22
	vmovdqa64	%ymm0,%ymm23

	xorl	%r9d,%r9d
	xorl	%r15d,%r15d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax

	movl	$20,%ebx

.align	32
.Lloop20:
	movq	0(%r11),%r13

	vpbroadcastq	%r13,%ymm3
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	(%r8),%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm4
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm3,%ymm1
	vpmadd52luq	32(%rsi),%ymm3,%ymm16
	vpmadd52luq	64(%rsi),%ymm3,%ymm17
	vpmadd52luq	96(%rsi),%ymm3,%ymm18
	vpmadd52luq	128(%rsi),%ymm3,%ymm19

	vpmadd52luq	0(%rcx),%ymm4,%ymm1
	vpmadd52luq	32(%rcx),%ymm4,%ymm16
	vpmadd52luq	64(%rcx),%ymm4,%ymm17
	vpmadd52luq	96(%rcx),%ymm4,%ymm18
	vpmadd52luq	128(%rcx),%ymm4,%ymm19


	valignq	$1,%ymm1,%ymm16,%ymm1
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm1,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm3,%ymm1
	vpmadd52huq	32(%rsi),%ymm3,%ymm16
	vpmadd52huq	64(%rsi),%ymm3,%ymm17
	vpmadd52huq	96(%rsi),%ymm3,%ymm18
	vpmadd52huq	128(%rsi),%ymm3,%ymm19

	vpmadd52huq	0(%rcx),%ymm4,%ymm1
	vpmadd52huq	32(%rcx),%ymm4,%ymm16
	vpmadd52huq	64(%rcx),%ymm4,%ymm17
	vpmadd52huq	96(%rcx),%ymm4,%ymm18
	vpmadd52huq	128(%rcx),%ymm4,%ymm19
	movq	160(%r11),%r13

	vpbroadcastq	%r13,%ymm3
	movq	160(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	movq	%r12,%r10
	adcq	$0,%r10

	movq	8(%r8),%r13
	imulq	%r15,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm4
	movq	160(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	adcq	%r12,%r10

	shrq	$52,%r15
	salq	$12,%r10
	orq	%r10,%r15

	vpmadd52luq	160(%rsi),%ymm3,%ymm2
	vpmadd52luq	192(%rsi),%ymm3,%ymm20
	vpmadd52luq	224(%rsi),%ymm3,%ymm21
	vpmadd52luq	256(%rsi),%ymm3,%ymm22
	vpmadd52luq	288(%rsi),%ymm3,%ymm23

	vpmadd52luq	160(%rcx),%ymm4,%ymm2
	vpmadd52luq	192(%rcx),%ymm4,%ymm20
	vpmadd52luq	224(%rcx),%ymm4,%ymm21
	vpmadd52luq	256(%rcx),%ymm4,%ymm22
	vpmadd52luq	288(%rcx),%ymm4,%ymm23


	valignq	$1,%ymm2,%ymm20,%ymm2
	valignq	$1,%ymm20,%ymm21,%ymm20
	valignq	$1,%ymm21,%ymm22,%ymm21
	valignq	$1,%ymm22,%ymm23,%ymm22
	valignq	$1,%ymm23,%ymm0,%ymm23

	vmovq	%xmm2,%r13
	addq	%r13,%r15

	vpmadd52huq	160(%rsi),%ymm3,%ymm2
	vpmadd52huq	192(%rsi),%ymm3,%ymm20
	vpmadd52huq	224(%rsi),%ymm3,%ymm21
	vpmadd52huq	256(%rsi),%ymm3,%ymm22
	vpmadd52huq	288(%rsi),%ymm3,%ymm23

	vpmadd52huq	160(%rcx),%ymm4,%ymm2
	vpmadd52huq	192(%rcx),%ymm4,%ymm20
	vpmadd52huq	224(%rcx),%ymm4,%ymm21
	vpmadd52huq	256(%rcx),%ymm4,%ymm22
	vpmadd52huq	288(%rcx),%ymm4,%ymm23
	leaq	8(%r11),%r11
	decl	%ebx
	jne	.Lloop20

	vmovdqa64	.Lmask52x4(%rip),%ymm4

	vpbroadcastq	%r9,%ymm3
	vpblendd	$3,%ymm3,%ymm1,%ymm1



	vpsrlq	$52,%ymm1,%ymm24
	vpsrlq	$52,%ymm16,%ymm25
	vpsrlq	$52,%ymm17,%ymm26
	vpsrlq	$52,%ymm18,%ymm27
	vpsrlq	$52,%ymm19,%ymm28


	valignq	$3,%ymm27,%ymm28,%ymm28
	valignq	$3,%ymm26,%ymm27,%ymm27
	valignq	$3,%ymm25,%ymm26,%ymm26
	valignq	$3,%ymm24,%ymm25,%ymm25
	valignq	$3,%ymm0,%ymm24,%ymm24


	vpandq	%ymm4,%ymm1,%ymm1
	vpandq	%ymm4,%ymm16,%ymm16
	vpandq	%ymm4,%ymm17,%ymm17
	vpandq	%ymm4,%ymm18,%ymm18
	vpandq	%ymm4,%ymm19,%ymm19


	vpaddq	%ymm24,%ymm1,%ymm1
	vpaddq	%ymm25,%ymm16,%ymm16
	vpaddq	%ymm26,%ymm17,%ymm17
	vpaddq	%ymm27,%ymm18,%ymm18
	vpaddq	%ymm28,%ymm19,%ymm19



	vpcmpuq	$1,%ymm1,%ymm4,%k1
	vpcmpuq	$1,%ymm16,%ymm4,%k2
	vpcmpuq	$1,%ymm17,%ymm4,%k3
	vpcmpuq	$1,%ymm18,%ymm4,%k4
	vpcmpuq	$1,%ymm19,%ymm4,%k5
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	kmovb	%k3,%r12d
	kmovb	%k4,%r11d
	kmovb	%k5,%r10d


	vpcmpuq	$0,%ymm1,%ymm4,%k1
	vpcmpuq	$0,%ymm16,%ymm4,%k2
	vpcmpuq	$0,%ymm17,%ymm4,%k3
	vpcmpuq	$0,%ymm18,%ymm4,%k4
	vpcmpuq	$0,%ymm19,%ymm4,%k5
	kmovb	%k1,%r9d
	kmovb	%k2,%r8d
	kmovb	%k3,%ebx
	kmovb	%k4,%ecx
	kmovb	%k5,%edx



	shlb	$4,%r13b
	orb	%r13b,%r14b
	shlb	$4,%r11b
	orb	%r11b,%r12b

	addb	%r14b,%r14b
	adcb	%r12b,%r12b
	adcb	%r10b,%r10b

	shlb	$4,%r8b
	orb	%r8b,%r9b
	shlb	$4,%cl
	orb	%cl,%bl

	addb	%r9b,%r14b
	adcb	%bl,%r12b
	adcb	%dl,%r10b

	xorb	%r9b,%r14b
	xorb	%bl,%r12b
	xorb	%dl,%r10b

	kmovb	%r14d,%k1
	shrb	$4,%r14b
	kmovb	%r14d,%k2
	kmovb	%r12d,%k3
	shrb	$4,%r12b
	kmovb	%r12d,%k4
	kmovb	%r10d,%k5


	vpsubq	%ymm4,%ymm1,%ymm1{%k1}
	vpsubq	%ymm4,%ymm16,%ymm16{%k2}
	vpsubq	%ymm4,%ymm17,%ymm17{%k3}
	vpsubq	%ymm4,%ymm18,%ymm18{%k4}
	vpsubq	%ymm4,%ymm19,%ymm19{%k5}

	vpandq	%ymm4,%ymm1,%ymm1
	vpandq	%ymm4,%ymm16,%ymm16
	vpandq	%ymm4,%ymm17,%ymm17
	vpandq	%ymm4,%ymm18,%ymm18
	vpandq	%ymm4,%ymm19,%ymm19

	vpbroadcastq	%r15,%ymm3
	vpblendd	$3,%ymm3,%ymm2,%ymm2



	vpsrlq	$52,%ymm2,%ymm24
	vpsrlq	$52,%ymm20,%ymm25
	vpsrlq	$52,%ymm21,%ymm26
	vpsrlq	$52,%ymm22,%ymm27
	vpsrlq	$52,%ymm23,%ymm28


	valignq	$3,%ymm27,%ymm28,%ymm28
	valignq	$3,%ymm26,%ymm27,%ymm27
	valignq	$3,%ymm25,%ymm26,%ymm26
	valignq	$3,%ymm24,%ymm25,%ymm25
	valignq	$3,%ymm0,%ymm24,%ymm24


	vpandq	%ymm4,%ymm2,%ymm2
	vpandq	%ymm4,%ymm20,%ymm20
	vpandq	%ymm4,%ymm21,%ymm21
	vpandq	%ymm4,%ymm22,%ymm22
	vpandq	%ymm4,%ymm23,%ymm23


	vpaddq	%ymm24,%ymm2,%ymm2
	vpaddq	%ymm25,%ymm20,%ymm20
	vpaddq	%ymm26,%ymm21,%ymm21
	vpaddq	%ymm27,%ymm22,%ymm22
	vpaddq	%ymm28,%ymm23,%ymm23



	vpcmpuq	$1,%ymm2,%ymm4,%k1
	vpcmpuq	$1,%ymm20,%ymm4,%k2
	vpcmpuq	$1,%ymm21,%ymm4,%k3
	vpcmpuq	$1,%ymm22,%ymm4,%k4
	vpcmpuq	$1,%ymm23,%ymm4,%k5
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	kmovb	%k3,%r12d
	kmovb	%k4,%r11d
	kmovb	%k5,%r10d


	vpcmpuq	$0,%ymm2,%ymm4,%k1
	vpcmpuq	$0,%ymm20,%ymm4,%k2
	vpcmpuq	$0,%ymm21,%ymm4,%k3
	vpcmpuq	$0,%ymm22,%ymm4,%k4
	vpcmpuq	$0,%ymm23,%ymm4,%k5
	kmovb	%k1,%r9d
	kmovb	%k2,%r8d
	kmovb	%k3,%ebx
	kmovb	%k4,%ecx
	kmovb	%k5,%edx



	shlb	$4,%r13b
	orb	%r13b,%r14b
	shlb	$4,%r11b
	orb	%r11b,%r12b

	addb	%r14b,%r14b
	adcb	%r12b,%r12b
	adcb	%r10b,%r10b

	shlb	$4,%r8b
	orb	%r8b,%r9b
	shlb	$4,%cl
	orb	%cl,%bl

	addb	%r9b,%r14b
	adcb	%bl,%r12b
	adcb	%dl,%r10b

	xorb	%r9b,%r14b
	xorb	%bl,%r12b
	xorb	%dl,%r10b

	kmovb	%r14d,%k1
	shrb	$4,%r14b
	kmovb	%r14d,%k2
	kmovb	%r12d,%k3
	shrb	$4,%r12b
	kmovb	%r12d,%k4
	kmovb	%r10d,%k5


	vpsubq	%ymm4,%ymm2,%ymm2{%k1}
	vpsubq	%ymm4,%ymm20,%ymm20{%k2}
	vpsubq	%ymm4,%ymm21,%ymm21{%k3}
	vpsubq	%ymm4,%ymm22,%ymm22{%k4}
	vpsubq	%ymm4,%ymm23,%ymm23{%k5}

	vpandq	%ymm4,%ymm2,%ymm2
	vpandq	%ymm4,%ymm20,%ymm20
	vpandq	%ymm4,%ymm21,%ymm21
	vpandq	%ymm4,%ymm22,%ymm22
	vpandq	%ymm4,%ymm23,%ymm23

	vmovdqu64	%ymm1,(%rdi)
	vmovdqu64	%ymm16,32(%rdi)
	vmovdqu64	%ymm17,64(%rdi)
	vmovdqu64	%ymm18,96(%rdi)
	vmovdqu64	%ymm19,128(%rdi)

	vmovdqu64	%ymm2,160(%rdi)
	vmovdqu64	%ymm20,192(%rdi)
	vmovdqu64	%ymm21,224(%rdi)
	vmovdqu64	%ymm22,256(%rdi)
	vmovdqu64	%ymm23,288(%rdi)

	vzeroupper
	movq	0(%rsp),%r15
.cfi_restore	%r15
	movq	8(%rsp),%r14
.cfi_restore	%r14
	movq	16(%rsp),%r13
.cfi_restore	%r13
	movq	24(%rsp),%r12
.cfi_restore	%r12
	movq	32(%rsp),%rbp
.cfi_restore	%rbp
	movq	40(%rsp),%rbx
.cfi_restore	%rbx
	leaq	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lrsaz_amm52x20_x2_256_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_rsaz_amm52x20_x2_256, .-ossl_rsaz_amm52x20_x2_256
.text	

.align	32
.globl	ossl_extract_multiplier_2x20_win5
.type	ossl_extract_multiplier_2x20_win5,@function
ossl_extract_multiplier_2x20_win5:
.cfi_startproc	
.byte	243,15,30,250
	leaq	(%rcx,%rcx,4),%rax
	salq	$5,%rax
	addq	%rax,%rsi

	vmovdqa64	.Lones(%rip),%ymm23
	vpbroadcastq	%rdx,%ymm22
	leaq	10240(%rsi),%rax

	vpxor	%xmm4,%xmm4,%xmm4
	vmovdqa64	%ymm4,%ymm3
	vmovdqa64	%ymm4,%ymm2
	vmovdqa64	%ymm4,%ymm1
	vmovdqa64	%ymm4,%ymm0
	vmovdqa64	%ymm4,%ymm21

.align	32
.Lloop:
	vpcmpq	$0,%ymm21,%ymm22,%k1
	addq	$320,%rsi
	vpaddq	%ymm23,%ymm21,%ymm21
	vmovdqu64	-320(%rsi),%ymm16
	vmovdqu64	-288(%rsi),%ymm17
	vmovdqu64	-256(%rsi),%ymm18
	vmovdqu64	-224(%rsi),%ymm19
	vmovdqu64	-192(%rsi),%ymm20
	vpblendmq	%ymm16,%ymm0,%ymm0{%k1}
	vpblendmq	%ymm17,%ymm1,%ymm1{%k1}
	vpblendmq	%ymm18,%ymm2,%ymm2{%k1}
	vpblendmq	%ymm19,%ymm3,%ymm3{%k1}
	vpblendmq	%ymm20,%ymm4,%ymm4{%k1}
	cmpq	%rsi,%rax
	jne	.Lloop

	vmovdqu64	%ymm0,(%rdi)
	vmovdqu64	%ymm1,32(%rdi)
	vmovdqu64	%ymm2,64(%rdi)
	vmovdqu64	%ymm3,96(%rdi)
	vmovdqu64	%ymm4,128(%rdi)

	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_extract_multiplier_2x20_win5, .-ossl_extract_multiplier_2x20_win5
.data	
.align	32
.Lones:
.quad	1,1,1,1
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
