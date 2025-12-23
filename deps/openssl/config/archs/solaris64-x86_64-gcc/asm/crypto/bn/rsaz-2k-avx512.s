
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

.globl	ossl_rsaz_amm52x20_x1_ifma256
.type	ossl_rsaz_amm52x20_x1_ifma256,@function
.align	32
ossl_rsaz_amm52x20_x1_ifma256:
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
.Lossl_rsaz_amm52x20_x1_ifma256_body:


	vpxord	%ymm0,%ymm0,%ymm0
	vmovdqa64	%ymm0,%ymm3
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

	vpbroadcastq	%r13,%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm1,%ymm3
	vpmadd52luq	32(%rsi),%ymm1,%ymm16
	vpmadd52luq	64(%rsi),%ymm1,%ymm17
	vpmadd52luq	96(%rsi),%ymm1,%ymm18
	vpmadd52luq	128(%rsi),%ymm1,%ymm19

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm16
	vpmadd52luq	64(%rcx),%ymm2,%ymm17
	vpmadd52luq	96(%rcx),%ymm2,%ymm18
	vpmadd52luq	128(%rcx),%ymm2,%ymm19


	valignq	$1,%ymm3,%ymm16,%ymm3
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm16
	vpmadd52huq	64(%rsi),%ymm1,%ymm17
	vpmadd52huq	96(%rsi),%ymm1,%ymm18
	vpmadd52huq	128(%rsi),%ymm1,%ymm19

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm16
	vpmadd52huq	64(%rcx),%ymm2,%ymm17
	vpmadd52huq	96(%rcx),%ymm2,%ymm18
	vpmadd52huq	128(%rcx),%ymm2,%ymm19
	movq	8(%r11),%r13

	vpbroadcastq	%r13,%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm1,%ymm3
	vpmadd52luq	32(%rsi),%ymm1,%ymm16
	vpmadd52luq	64(%rsi),%ymm1,%ymm17
	vpmadd52luq	96(%rsi),%ymm1,%ymm18
	vpmadd52luq	128(%rsi),%ymm1,%ymm19

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm16
	vpmadd52luq	64(%rcx),%ymm2,%ymm17
	vpmadd52luq	96(%rcx),%ymm2,%ymm18
	vpmadd52luq	128(%rcx),%ymm2,%ymm19


	valignq	$1,%ymm3,%ymm16,%ymm3
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm16
	vpmadd52huq	64(%rsi),%ymm1,%ymm17
	vpmadd52huq	96(%rsi),%ymm1,%ymm18
	vpmadd52huq	128(%rsi),%ymm1,%ymm19

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm16
	vpmadd52huq	64(%rcx),%ymm2,%ymm17
	vpmadd52huq	96(%rcx),%ymm2,%ymm18
	vpmadd52huq	128(%rcx),%ymm2,%ymm19
	movq	16(%r11),%r13

	vpbroadcastq	%r13,%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm1,%ymm3
	vpmadd52luq	32(%rsi),%ymm1,%ymm16
	vpmadd52luq	64(%rsi),%ymm1,%ymm17
	vpmadd52luq	96(%rsi),%ymm1,%ymm18
	vpmadd52luq	128(%rsi),%ymm1,%ymm19

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm16
	vpmadd52luq	64(%rcx),%ymm2,%ymm17
	vpmadd52luq	96(%rcx),%ymm2,%ymm18
	vpmadd52luq	128(%rcx),%ymm2,%ymm19


	valignq	$1,%ymm3,%ymm16,%ymm3
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm16
	vpmadd52huq	64(%rsi),%ymm1,%ymm17
	vpmadd52huq	96(%rsi),%ymm1,%ymm18
	vpmadd52huq	128(%rsi),%ymm1,%ymm19

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm16
	vpmadd52huq	64(%rcx),%ymm2,%ymm17
	vpmadd52huq	96(%rcx),%ymm2,%ymm18
	vpmadd52huq	128(%rcx),%ymm2,%ymm19
	movq	24(%r11),%r13

	vpbroadcastq	%r13,%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm1,%ymm3
	vpmadd52luq	32(%rsi),%ymm1,%ymm16
	vpmadd52luq	64(%rsi),%ymm1,%ymm17
	vpmadd52luq	96(%rsi),%ymm1,%ymm18
	vpmadd52luq	128(%rsi),%ymm1,%ymm19

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm16
	vpmadd52luq	64(%rcx),%ymm2,%ymm17
	vpmadd52luq	96(%rcx),%ymm2,%ymm18
	vpmadd52luq	128(%rcx),%ymm2,%ymm19


	valignq	$1,%ymm3,%ymm16,%ymm3
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm16
	vpmadd52huq	64(%rsi),%ymm1,%ymm17
	vpmadd52huq	96(%rsi),%ymm1,%ymm18
	vpmadd52huq	128(%rsi),%ymm1,%ymm19

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm16
	vpmadd52huq	64(%rcx),%ymm2,%ymm17
	vpmadd52huq	96(%rcx),%ymm2,%ymm18
	vpmadd52huq	128(%rcx),%ymm2,%ymm19
	leaq	32(%r11),%r11
	decl	%ebx
	jne	.Lloop5

	vpbroadcastq	%r9,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm16,%ymm1
	vpsrlq	$52,%ymm17,%ymm2
	vpsrlq	$52,%ymm18,%ymm25
	vpsrlq	$52,%ymm19,%ymm26


	valignq	$3,%ymm25,%ymm26,%ymm26
	valignq	$3,%ymm2,%ymm25,%ymm25
	valignq	$3,%ymm1,%ymm2,%ymm2
	valignq	$3,%ymm0,%ymm1,%ymm1
	valignq	$3,.Lzeros(%rip),%ymm0,%ymm0


	vpandq	.Lmask52x4(%rip),%ymm3,%ymm3
	vpandq	.Lmask52x4(%rip),%ymm16,%ymm16
	vpandq	.Lmask52x4(%rip),%ymm17,%ymm17
	vpandq	.Lmask52x4(%rip),%ymm18,%ymm18
	vpandq	.Lmask52x4(%rip),%ymm19,%ymm19


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm16,%ymm16
	vpaddq	%ymm2,%ymm17,%ymm17
	vpaddq	%ymm25,%ymm18,%ymm18
	vpaddq	%ymm26,%ymm19,%ymm19



	vpcmpuq	$6,.Lmask52x4(%rip),%ymm3,%k1
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm16,%k2
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm17,%k3
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm18,%k4
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm19,%k5
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	kmovb	%k3,%r12d
	kmovb	%k4,%r11d
	kmovb	%k5,%r10d


	vpcmpuq	$0,.Lmask52x4(%rip),%ymm3,%k1
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm16,%k2
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm17,%k3
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm18,%k4
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm19,%k5
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


	vpsubq	.Lmask52x4(%rip),%ymm3,%ymm3{%k1}
	vpsubq	.Lmask52x4(%rip),%ymm16,%ymm16{%k2}
	vpsubq	.Lmask52x4(%rip),%ymm17,%ymm17{%k3}
	vpsubq	.Lmask52x4(%rip),%ymm18,%ymm18{%k4}
	vpsubq	.Lmask52x4(%rip),%ymm19,%ymm19{%k5}

	vpandq	.Lmask52x4(%rip),%ymm3,%ymm3
	vpandq	.Lmask52x4(%rip),%ymm16,%ymm16
	vpandq	.Lmask52x4(%rip),%ymm17,%ymm17
	vpandq	.Lmask52x4(%rip),%ymm18,%ymm18
	vpandq	.Lmask52x4(%rip),%ymm19,%ymm19

	vmovdqu64	%ymm3,0(%rdi)
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
.Lossl_rsaz_amm52x20_x1_ifma256_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_rsaz_amm52x20_x1_ifma256, .-ossl_rsaz_amm52x20_x1_ifma256
.section	.rodata
.align	32
.Lmask52x4:
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.text	

.globl	ossl_rsaz_amm52x20_x2_ifma256
.type	ossl_rsaz_amm52x20_x2_ifma256,@function
.align	32
ossl_rsaz_amm52x20_x2_ifma256:
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
.Lossl_rsaz_amm52x20_x2_ifma256_body:


	vpxord	%ymm0,%ymm0,%ymm0
	vmovdqa64	%ymm0,%ymm3
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm0,%ymm17
	vmovdqa64	%ymm0,%ymm18
	vmovdqa64	%ymm0,%ymm19
	vmovdqa64	%ymm0,%ymm4
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

	vpbroadcastq	%r13,%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	(%r8),%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	vpmadd52luq	0(%rsi),%ymm1,%ymm3
	vpmadd52luq	32(%rsi),%ymm1,%ymm16
	vpmadd52luq	64(%rsi),%ymm1,%ymm17
	vpmadd52luq	96(%rsi),%ymm1,%ymm18
	vpmadd52luq	128(%rsi),%ymm1,%ymm19

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm16
	vpmadd52luq	64(%rcx),%ymm2,%ymm17
	vpmadd52luq	96(%rcx),%ymm2,%ymm18
	vpmadd52luq	128(%rcx),%ymm2,%ymm19


	valignq	$1,%ymm3,%ymm16,%ymm3
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm19,%ymm18
	valignq	$1,%ymm19,%ymm0,%ymm19

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm16
	vpmadd52huq	64(%rsi),%ymm1,%ymm17
	vpmadd52huq	96(%rsi),%ymm1,%ymm18
	vpmadd52huq	128(%rsi),%ymm1,%ymm19

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm16
	vpmadd52huq	64(%rcx),%ymm2,%ymm17
	vpmadd52huq	96(%rcx),%ymm2,%ymm18
	vpmadd52huq	128(%rcx),%ymm2,%ymm19
	movq	160(%r11),%r13

	vpbroadcastq	%r13,%ymm1
	movq	160(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	movq	%r12,%r10
	adcq	$0,%r10

	movq	8(%r8),%r13
	imulq	%r15,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm2
	movq	160(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	adcq	%r12,%r10

	shrq	$52,%r15
	salq	$12,%r10
	orq	%r10,%r15

	vpmadd52luq	160(%rsi),%ymm1,%ymm4
	vpmadd52luq	192(%rsi),%ymm1,%ymm20
	vpmadd52luq	224(%rsi),%ymm1,%ymm21
	vpmadd52luq	256(%rsi),%ymm1,%ymm22
	vpmadd52luq	288(%rsi),%ymm1,%ymm23

	vpmadd52luq	160(%rcx),%ymm2,%ymm4
	vpmadd52luq	192(%rcx),%ymm2,%ymm20
	vpmadd52luq	224(%rcx),%ymm2,%ymm21
	vpmadd52luq	256(%rcx),%ymm2,%ymm22
	vpmadd52luq	288(%rcx),%ymm2,%ymm23


	valignq	$1,%ymm4,%ymm20,%ymm4
	valignq	$1,%ymm20,%ymm21,%ymm20
	valignq	$1,%ymm21,%ymm22,%ymm21
	valignq	$1,%ymm22,%ymm23,%ymm22
	valignq	$1,%ymm23,%ymm0,%ymm23

	vmovq	%xmm4,%r13
	addq	%r13,%r15

	vpmadd52huq	160(%rsi),%ymm1,%ymm4
	vpmadd52huq	192(%rsi),%ymm1,%ymm20
	vpmadd52huq	224(%rsi),%ymm1,%ymm21
	vpmadd52huq	256(%rsi),%ymm1,%ymm22
	vpmadd52huq	288(%rsi),%ymm1,%ymm23

	vpmadd52huq	160(%rcx),%ymm2,%ymm4
	vpmadd52huq	192(%rcx),%ymm2,%ymm20
	vpmadd52huq	224(%rcx),%ymm2,%ymm21
	vpmadd52huq	256(%rcx),%ymm2,%ymm22
	vpmadd52huq	288(%rcx),%ymm2,%ymm23
	leaq	8(%r11),%r11
	decl	%ebx
	jne	.Lloop20

	vpbroadcastq	%r9,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm16,%ymm1
	vpsrlq	$52,%ymm17,%ymm2
	vpsrlq	$52,%ymm18,%ymm25
	vpsrlq	$52,%ymm19,%ymm26


	valignq	$3,%ymm25,%ymm26,%ymm26
	valignq	$3,%ymm2,%ymm25,%ymm25
	valignq	$3,%ymm1,%ymm2,%ymm2
	valignq	$3,%ymm0,%ymm1,%ymm1
	valignq	$3,.Lzeros(%rip),%ymm0,%ymm0


	vpandq	.Lmask52x4(%rip),%ymm3,%ymm3
	vpandq	.Lmask52x4(%rip),%ymm16,%ymm16
	vpandq	.Lmask52x4(%rip),%ymm17,%ymm17
	vpandq	.Lmask52x4(%rip),%ymm18,%ymm18
	vpandq	.Lmask52x4(%rip),%ymm19,%ymm19


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm16,%ymm16
	vpaddq	%ymm2,%ymm17,%ymm17
	vpaddq	%ymm25,%ymm18,%ymm18
	vpaddq	%ymm26,%ymm19,%ymm19



	vpcmpuq	$6,.Lmask52x4(%rip),%ymm3,%k1
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm16,%k2
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm17,%k3
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm18,%k4
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm19,%k5
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	kmovb	%k3,%r12d
	kmovb	%k4,%r11d
	kmovb	%k5,%r10d


	vpcmpuq	$0,.Lmask52x4(%rip),%ymm3,%k1
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm16,%k2
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm17,%k3
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm18,%k4
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm19,%k5
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


	vpsubq	.Lmask52x4(%rip),%ymm3,%ymm3{%k1}
	vpsubq	.Lmask52x4(%rip),%ymm16,%ymm16{%k2}
	vpsubq	.Lmask52x4(%rip),%ymm17,%ymm17{%k3}
	vpsubq	.Lmask52x4(%rip),%ymm18,%ymm18{%k4}
	vpsubq	.Lmask52x4(%rip),%ymm19,%ymm19{%k5}

	vpandq	.Lmask52x4(%rip),%ymm3,%ymm3
	vpandq	.Lmask52x4(%rip),%ymm16,%ymm16
	vpandq	.Lmask52x4(%rip),%ymm17,%ymm17
	vpandq	.Lmask52x4(%rip),%ymm18,%ymm18
	vpandq	.Lmask52x4(%rip),%ymm19,%ymm19

	vpbroadcastq	%r15,%ymm0
	vpblendd	$3,%ymm0,%ymm4,%ymm4



	vpsrlq	$52,%ymm4,%ymm0
	vpsrlq	$52,%ymm20,%ymm1
	vpsrlq	$52,%ymm21,%ymm2
	vpsrlq	$52,%ymm22,%ymm25
	vpsrlq	$52,%ymm23,%ymm26


	valignq	$3,%ymm25,%ymm26,%ymm26
	valignq	$3,%ymm2,%ymm25,%ymm25
	valignq	$3,%ymm1,%ymm2,%ymm2
	valignq	$3,%ymm0,%ymm1,%ymm1
	valignq	$3,.Lzeros(%rip),%ymm0,%ymm0


	vpandq	.Lmask52x4(%rip),%ymm4,%ymm4
	vpandq	.Lmask52x4(%rip),%ymm20,%ymm20
	vpandq	.Lmask52x4(%rip),%ymm21,%ymm21
	vpandq	.Lmask52x4(%rip),%ymm22,%ymm22
	vpandq	.Lmask52x4(%rip),%ymm23,%ymm23


	vpaddq	%ymm0,%ymm4,%ymm4
	vpaddq	%ymm1,%ymm20,%ymm20
	vpaddq	%ymm2,%ymm21,%ymm21
	vpaddq	%ymm25,%ymm22,%ymm22
	vpaddq	%ymm26,%ymm23,%ymm23



	vpcmpuq	$6,.Lmask52x4(%rip),%ymm4,%k1
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm20,%k2
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm21,%k3
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm22,%k4
	vpcmpuq	$6,.Lmask52x4(%rip),%ymm23,%k5
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	kmovb	%k3,%r12d
	kmovb	%k4,%r11d
	kmovb	%k5,%r10d


	vpcmpuq	$0,.Lmask52x4(%rip),%ymm4,%k1
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm20,%k2
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm21,%k3
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm22,%k4
	vpcmpuq	$0,.Lmask52x4(%rip),%ymm23,%k5
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


	vpsubq	.Lmask52x4(%rip),%ymm4,%ymm4{%k1}
	vpsubq	.Lmask52x4(%rip),%ymm20,%ymm20{%k2}
	vpsubq	.Lmask52x4(%rip),%ymm21,%ymm21{%k3}
	vpsubq	.Lmask52x4(%rip),%ymm22,%ymm22{%k4}
	vpsubq	.Lmask52x4(%rip),%ymm23,%ymm23{%k5}

	vpandq	.Lmask52x4(%rip),%ymm4,%ymm4
	vpandq	.Lmask52x4(%rip),%ymm20,%ymm20
	vpandq	.Lmask52x4(%rip),%ymm21,%ymm21
	vpandq	.Lmask52x4(%rip),%ymm22,%ymm22
	vpandq	.Lmask52x4(%rip),%ymm23,%ymm23

	vmovdqu64	%ymm3,0(%rdi)
	vmovdqu64	%ymm16,32(%rdi)
	vmovdqu64	%ymm17,64(%rdi)
	vmovdqu64	%ymm18,96(%rdi)
	vmovdqu64	%ymm19,128(%rdi)

	vmovdqu64	%ymm4,160(%rdi)
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
.Lossl_rsaz_amm52x20_x2_ifma256_epilogue:
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_rsaz_amm52x20_x2_ifma256, .-ossl_rsaz_amm52x20_x2_ifma256
.text	

.align	32
.globl	ossl_extract_multiplier_2x20_win5
.type	ossl_extract_multiplier_2x20_win5,@function
ossl_extract_multiplier_2x20_win5:
.cfi_startproc	
.byte	243,15,30,250
	vmovdqa64	.Lones(%rip),%ymm24
	vpbroadcastq	%rdx,%ymm22
	vpbroadcastq	%rcx,%ymm23
	leaq	10240(%rsi),%rax


	vpxor	%xmm0,%xmm0,%xmm0
	vmovdqa64	%ymm0,%ymm21
	vmovdqa64	%ymm0,%ymm1
	vmovdqa64	%ymm0,%ymm2
	vmovdqa64	%ymm0,%ymm3
	vmovdqa64	%ymm0,%ymm4
	vmovdqa64	%ymm0,%ymm5
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm0,%ymm17
	vmovdqa64	%ymm0,%ymm18
	vmovdqa64	%ymm0,%ymm19

.align	32
.Lloop:
	vpcmpq	$0,%ymm21,%ymm22,%k1
	vpcmpq	$0,%ymm21,%ymm23,%k2
	vmovdqu64	0(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm0,%ymm0{%k1}
	vmovdqu64	32(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm1,%ymm1{%k1}
	vmovdqu64	64(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm2,%ymm2{%k1}
	vmovdqu64	96(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm3,%ymm3{%k1}
	vmovdqu64	128(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm4,%ymm4{%k1}
	vmovdqu64	160(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm5,%ymm5{%k2}
	vmovdqu64	192(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm16,%ymm16{%k2}
	vmovdqu64	224(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm17,%ymm17{%k2}
	vmovdqu64	256(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm18,%ymm18{%k2}
	vmovdqu64	288(%rsi),%ymm20
	vpblendmq	%ymm20,%ymm19,%ymm19{%k2}
	vpaddq	%ymm24,%ymm21,%ymm21
	addq	$320,%rsi
	cmpq	%rsi,%rax
	jne	.Lloop
	vmovdqu64	%ymm0,0(%rdi)
	vmovdqu64	%ymm1,32(%rdi)
	vmovdqu64	%ymm2,64(%rdi)
	vmovdqu64	%ymm3,96(%rdi)
	vmovdqu64	%ymm4,128(%rdi)
	vmovdqu64	%ymm5,160(%rdi)
	vmovdqu64	%ymm16,192(%rdi)
	vmovdqu64	%ymm17,224(%rdi)
	vmovdqu64	%ymm18,256(%rdi)
	vmovdqu64	%ymm19,288(%rdi)
	.byte	0xf3,0xc3
.cfi_endproc	
.size	ossl_extract_multiplier_2x20_win5, .-ossl_extract_multiplier_2x20_win5
.section	.rodata
.align	32
.Lones:
.quad	1,1,1,1
.Lzeros:
.quad	0,0,0,0
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
