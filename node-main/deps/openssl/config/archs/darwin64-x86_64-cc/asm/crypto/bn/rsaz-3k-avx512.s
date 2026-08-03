.text	

.globl	_ossl_rsaz_amm52x30_x1_ifma256

.p2align	5
_ossl_rsaz_amm52x30_x1_ifma256:

.byte	243,15,30,250
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	vpxord	%ymm0,%ymm0,%ymm0
	vmovdqa64	%ymm0,%ymm3
	vmovdqa64	%ymm0,%ymm4
	vmovdqa64	%ymm0,%ymm5
	vmovdqa64	%ymm0,%ymm6
	vmovdqa64	%ymm0,%ymm7
	vmovdqa64	%ymm0,%ymm8
	vmovdqa64	%ymm0,%ymm9
	vmovdqa64	%ymm0,%ymm10

	xorl	%r9d,%r9d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax


	movl	$7,%ebx

.p2align	5
L$loop7:
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
	vpmadd52luq	32(%rsi),%ymm1,%ymm4
	vpmadd52luq	64(%rsi),%ymm1,%ymm5
	vpmadd52luq	96(%rsi),%ymm1,%ymm6
	vpmadd52luq	128(%rsi),%ymm1,%ymm7
	vpmadd52luq	160(%rsi),%ymm1,%ymm8
	vpmadd52luq	192(%rsi),%ymm1,%ymm9
	vpmadd52luq	224(%rsi),%ymm1,%ymm10

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm4
	vpmadd52luq	64(%rcx),%ymm2,%ymm5
	vpmadd52luq	96(%rcx),%ymm2,%ymm6
	vpmadd52luq	128(%rcx),%ymm2,%ymm7
	vpmadd52luq	160(%rcx),%ymm2,%ymm8
	vpmadd52luq	192(%rcx),%ymm2,%ymm9
	vpmadd52luq	224(%rcx),%ymm2,%ymm10


	valignq	$1,%ymm3,%ymm4,%ymm3
	valignq	$1,%ymm4,%ymm5,%ymm4
	valignq	$1,%ymm5,%ymm6,%ymm5
	valignq	$1,%ymm6,%ymm7,%ymm6
	valignq	$1,%ymm7,%ymm8,%ymm7
	valignq	$1,%ymm8,%ymm9,%ymm8
	valignq	$1,%ymm9,%ymm10,%ymm9
	valignq	$1,%ymm10,%ymm0,%ymm10

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm4
	vpmadd52huq	64(%rsi),%ymm1,%ymm5
	vpmadd52huq	96(%rsi),%ymm1,%ymm6
	vpmadd52huq	128(%rsi),%ymm1,%ymm7
	vpmadd52huq	160(%rsi),%ymm1,%ymm8
	vpmadd52huq	192(%rsi),%ymm1,%ymm9
	vpmadd52huq	224(%rsi),%ymm1,%ymm10

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm4
	vpmadd52huq	64(%rcx),%ymm2,%ymm5
	vpmadd52huq	96(%rcx),%ymm2,%ymm6
	vpmadd52huq	128(%rcx),%ymm2,%ymm7
	vpmadd52huq	160(%rcx),%ymm2,%ymm8
	vpmadd52huq	192(%rcx),%ymm2,%ymm9
	vpmadd52huq	224(%rcx),%ymm2,%ymm10
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
	vpmadd52luq	32(%rsi),%ymm1,%ymm4
	vpmadd52luq	64(%rsi),%ymm1,%ymm5
	vpmadd52luq	96(%rsi),%ymm1,%ymm6
	vpmadd52luq	128(%rsi),%ymm1,%ymm7
	vpmadd52luq	160(%rsi),%ymm1,%ymm8
	vpmadd52luq	192(%rsi),%ymm1,%ymm9
	vpmadd52luq	224(%rsi),%ymm1,%ymm10

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm4
	vpmadd52luq	64(%rcx),%ymm2,%ymm5
	vpmadd52luq	96(%rcx),%ymm2,%ymm6
	vpmadd52luq	128(%rcx),%ymm2,%ymm7
	vpmadd52luq	160(%rcx),%ymm2,%ymm8
	vpmadd52luq	192(%rcx),%ymm2,%ymm9
	vpmadd52luq	224(%rcx),%ymm2,%ymm10


	valignq	$1,%ymm3,%ymm4,%ymm3
	valignq	$1,%ymm4,%ymm5,%ymm4
	valignq	$1,%ymm5,%ymm6,%ymm5
	valignq	$1,%ymm6,%ymm7,%ymm6
	valignq	$1,%ymm7,%ymm8,%ymm7
	valignq	$1,%ymm8,%ymm9,%ymm8
	valignq	$1,%ymm9,%ymm10,%ymm9
	valignq	$1,%ymm10,%ymm0,%ymm10

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm4
	vpmadd52huq	64(%rsi),%ymm1,%ymm5
	vpmadd52huq	96(%rsi),%ymm1,%ymm6
	vpmadd52huq	128(%rsi),%ymm1,%ymm7
	vpmadd52huq	160(%rsi),%ymm1,%ymm8
	vpmadd52huq	192(%rsi),%ymm1,%ymm9
	vpmadd52huq	224(%rsi),%ymm1,%ymm10

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm4
	vpmadd52huq	64(%rcx),%ymm2,%ymm5
	vpmadd52huq	96(%rcx),%ymm2,%ymm6
	vpmadd52huq	128(%rcx),%ymm2,%ymm7
	vpmadd52huq	160(%rcx),%ymm2,%ymm8
	vpmadd52huq	192(%rcx),%ymm2,%ymm9
	vpmadd52huq	224(%rcx),%ymm2,%ymm10
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
	vpmadd52luq	32(%rsi),%ymm1,%ymm4
	vpmadd52luq	64(%rsi),%ymm1,%ymm5
	vpmadd52luq	96(%rsi),%ymm1,%ymm6
	vpmadd52luq	128(%rsi),%ymm1,%ymm7
	vpmadd52luq	160(%rsi),%ymm1,%ymm8
	vpmadd52luq	192(%rsi),%ymm1,%ymm9
	vpmadd52luq	224(%rsi),%ymm1,%ymm10

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm4
	vpmadd52luq	64(%rcx),%ymm2,%ymm5
	vpmadd52luq	96(%rcx),%ymm2,%ymm6
	vpmadd52luq	128(%rcx),%ymm2,%ymm7
	vpmadd52luq	160(%rcx),%ymm2,%ymm8
	vpmadd52luq	192(%rcx),%ymm2,%ymm9
	vpmadd52luq	224(%rcx),%ymm2,%ymm10


	valignq	$1,%ymm3,%ymm4,%ymm3
	valignq	$1,%ymm4,%ymm5,%ymm4
	valignq	$1,%ymm5,%ymm6,%ymm5
	valignq	$1,%ymm6,%ymm7,%ymm6
	valignq	$1,%ymm7,%ymm8,%ymm7
	valignq	$1,%ymm8,%ymm9,%ymm8
	valignq	$1,%ymm9,%ymm10,%ymm9
	valignq	$1,%ymm10,%ymm0,%ymm10

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm4
	vpmadd52huq	64(%rsi),%ymm1,%ymm5
	vpmadd52huq	96(%rsi),%ymm1,%ymm6
	vpmadd52huq	128(%rsi),%ymm1,%ymm7
	vpmadd52huq	160(%rsi),%ymm1,%ymm8
	vpmadd52huq	192(%rsi),%ymm1,%ymm9
	vpmadd52huq	224(%rsi),%ymm1,%ymm10

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm4
	vpmadd52huq	64(%rcx),%ymm2,%ymm5
	vpmadd52huq	96(%rcx),%ymm2,%ymm6
	vpmadd52huq	128(%rcx),%ymm2,%ymm7
	vpmadd52huq	160(%rcx),%ymm2,%ymm8
	vpmadd52huq	192(%rcx),%ymm2,%ymm9
	vpmadd52huq	224(%rcx),%ymm2,%ymm10
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
	vpmadd52luq	32(%rsi),%ymm1,%ymm4
	vpmadd52luq	64(%rsi),%ymm1,%ymm5
	vpmadd52luq	96(%rsi),%ymm1,%ymm6
	vpmadd52luq	128(%rsi),%ymm1,%ymm7
	vpmadd52luq	160(%rsi),%ymm1,%ymm8
	vpmadd52luq	192(%rsi),%ymm1,%ymm9
	vpmadd52luq	224(%rsi),%ymm1,%ymm10

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm4
	vpmadd52luq	64(%rcx),%ymm2,%ymm5
	vpmadd52luq	96(%rcx),%ymm2,%ymm6
	vpmadd52luq	128(%rcx),%ymm2,%ymm7
	vpmadd52luq	160(%rcx),%ymm2,%ymm8
	vpmadd52luq	192(%rcx),%ymm2,%ymm9
	vpmadd52luq	224(%rcx),%ymm2,%ymm10


	valignq	$1,%ymm3,%ymm4,%ymm3
	valignq	$1,%ymm4,%ymm5,%ymm4
	valignq	$1,%ymm5,%ymm6,%ymm5
	valignq	$1,%ymm6,%ymm7,%ymm6
	valignq	$1,%ymm7,%ymm8,%ymm7
	valignq	$1,%ymm8,%ymm9,%ymm8
	valignq	$1,%ymm9,%ymm10,%ymm9
	valignq	$1,%ymm10,%ymm0,%ymm10

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm4
	vpmadd52huq	64(%rsi),%ymm1,%ymm5
	vpmadd52huq	96(%rsi),%ymm1,%ymm6
	vpmadd52huq	128(%rsi),%ymm1,%ymm7
	vpmadd52huq	160(%rsi),%ymm1,%ymm8
	vpmadd52huq	192(%rsi),%ymm1,%ymm9
	vpmadd52huq	224(%rsi),%ymm1,%ymm10

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm4
	vpmadd52huq	64(%rcx),%ymm2,%ymm5
	vpmadd52huq	96(%rcx),%ymm2,%ymm6
	vpmadd52huq	128(%rcx),%ymm2,%ymm7
	vpmadd52huq	160(%rcx),%ymm2,%ymm8
	vpmadd52huq	192(%rcx),%ymm2,%ymm9
	vpmadd52huq	224(%rcx),%ymm2,%ymm10
	leaq	32(%r11),%r11
	decl	%ebx
	jne	L$loop7
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
	vpmadd52luq	32(%rsi),%ymm1,%ymm4
	vpmadd52luq	64(%rsi),%ymm1,%ymm5
	vpmadd52luq	96(%rsi),%ymm1,%ymm6
	vpmadd52luq	128(%rsi),%ymm1,%ymm7
	vpmadd52luq	160(%rsi),%ymm1,%ymm8
	vpmadd52luq	192(%rsi),%ymm1,%ymm9
	vpmadd52luq	224(%rsi),%ymm1,%ymm10

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm4
	vpmadd52luq	64(%rcx),%ymm2,%ymm5
	vpmadd52luq	96(%rcx),%ymm2,%ymm6
	vpmadd52luq	128(%rcx),%ymm2,%ymm7
	vpmadd52luq	160(%rcx),%ymm2,%ymm8
	vpmadd52luq	192(%rcx),%ymm2,%ymm9
	vpmadd52luq	224(%rcx),%ymm2,%ymm10


	valignq	$1,%ymm3,%ymm4,%ymm3
	valignq	$1,%ymm4,%ymm5,%ymm4
	valignq	$1,%ymm5,%ymm6,%ymm5
	valignq	$1,%ymm6,%ymm7,%ymm6
	valignq	$1,%ymm7,%ymm8,%ymm7
	valignq	$1,%ymm8,%ymm9,%ymm8
	valignq	$1,%ymm9,%ymm10,%ymm9
	valignq	$1,%ymm10,%ymm0,%ymm10

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm4
	vpmadd52huq	64(%rsi),%ymm1,%ymm5
	vpmadd52huq	96(%rsi),%ymm1,%ymm6
	vpmadd52huq	128(%rsi),%ymm1,%ymm7
	vpmadd52huq	160(%rsi),%ymm1,%ymm8
	vpmadd52huq	192(%rsi),%ymm1,%ymm9
	vpmadd52huq	224(%rsi),%ymm1,%ymm10

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm4
	vpmadd52huq	64(%rcx),%ymm2,%ymm5
	vpmadd52huq	96(%rcx),%ymm2,%ymm6
	vpmadd52huq	128(%rcx),%ymm2,%ymm7
	vpmadd52huq	160(%rcx),%ymm2,%ymm8
	vpmadd52huq	192(%rcx),%ymm2,%ymm9
	vpmadd52huq	224(%rcx),%ymm2,%ymm10
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
	vpmadd52luq	32(%rsi),%ymm1,%ymm4
	vpmadd52luq	64(%rsi),%ymm1,%ymm5
	vpmadd52luq	96(%rsi),%ymm1,%ymm6
	vpmadd52luq	128(%rsi),%ymm1,%ymm7
	vpmadd52luq	160(%rsi),%ymm1,%ymm8
	vpmadd52luq	192(%rsi),%ymm1,%ymm9
	vpmadd52luq	224(%rsi),%ymm1,%ymm10

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm4
	vpmadd52luq	64(%rcx),%ymm2,%ymm5
	vpmadd52luq	96(%rcx),%ymm2,%ymm6
	vpmadd52luq	128(%rcx),%ymm2,%ymm7
	vpmadd52luq	160(%rcx),%ymm2,%ymm8
	vpmadd52luq	192(%rcx),%ymm2,%ymm9
	vpmadd52luq	224(%rcx),%ymm2,%ymm10


	valignq	$1,%ymm3,%ymm4,%ymm3
	valignq	$1,%ymm4,%ymm5,%ymm4
	valignq	$1,%ymm5,%ymm6,%ymm5
	valignq	$1,%ymm6,%ymm7,%ymm6
	valignq	$1,%ymm7,%ymm8,%ymm7
	valignq	$1,%ymm8,%ymm9,%ymm8
	valignq	$1,%ymm9,%ymm10,%ymm9
	valignq	$1,%ymm10,%ymm0,%ymm10

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm4
	vpmadd52huq	64(%rsi),%ymm1,%ymm5
	vpmadd52huq	96(%rsi),%ymm1,%ymm6
	vpmadd52huq	128(%rsi),%ymm1,%ymm7
	vpmadd52huq	160(%rsi),%ymm1,%ymm8
	vpmadd52huq	192(%rsi),%ymm1,%ymm9
	vpmadd52huq	224(%rsi),%ymm1,%ymm10

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm4
	vpmadd52huq	64(%rcx),%ymm2,%ymm5
	vpmadd52huq	96(%rcx),%ymm2,%ymm6
	vpmadd52huq	128(%rcx),%ymm2,%ymm7
	vpmadd52huq	160(%rcx),%ymm2,%ymm8
	vpmadd52huq	192(%rcx),%ymm2,%ymm9
	vpmadd52huq	224(%rcx),%ymm2,%ymm10

	vpbroadcastq	%r9,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm4,%ymm1
	vpsrlq	$52,%ymm5,%ymm2
	vpsrlq	$52,%ymm6,%ymm19
	vpsrlq	$52,%ymm7,%ymm20
	vpsrlq	$52,%ymm8,%ymm21
	vpsrlq	$52,%ymm9,%ymm22
	vpsrlq	$52,%ymm10,%ymm23


	valignq	$3,%ymm22,%ymm23,%ymm23
	valignq	$3,%ymm21,%ymm22,%ymm22
	valignq	$3,%ymm20,%ymm21,%ymm21
	valignq	$3,%ymm19,%ymm20,%ymm20
	valignq	$3,%ymm2,%ymm19,%ymm19
	valignq	$3,%ymm1,%ymm2,%ymm2
	valignq	$3,%ymm0,%ymm1,%ymm1
	valignq	$3,L$zeros(%rip),%ymm0,%ymm0


	vpandq	L$mask52x4(%rip),%ymm3,%ymm3
	vpandq	L$mask52x4(%rip),%ymm4,%ymm4
	vpandq	L$mask52x4(%rip),%ymm5,%ymm5
	vpandq	L$mask52x4(%rip),%ymm6,%ymm6
	vpandq	L$mask52x4(%rip),%ymm7,%ymm7
	vpandq	L$mask52x4(%rip),%ymm8,%ymm8
	vpandq	L$mask52x4(%rip),%ymm9,%ymm9
	vpandq	L$mask52x4(%rip),%ymm10,%ymm10


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm4,%ymm4
	vpaddq	%ymm2,%ymm5,%ymm5
	vpaddq	%ymm19,%ymm6,%ymm6
	vpaddq	%ymm20,%ymm7,%ymm7
	vpaddq	%ymm21,%ymm8,%ymm8
	vpaddq	%ymm22,%ymm9,%ymm9
	vpaddq	%ymm23,%ymm10,%ymm10



	vpcmpuq	$6,L$mask52x4(%rip),%ymm3,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm4,%k2
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm5,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm6,%k2
	kmovb	%k1,%r13d
	kmovb	%k2,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm7,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm8,%k2
	kmovb	%k1,%r12d
	kmovb	%k2,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm9,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm10,%k2
	kmovb	%k1,%r11d
	kmovb	%k2,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b


	vpcmpuq	$0,L$mask52x4(%rip),%ymm3,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm4,%k2
	kmovb	%k1,%r9d
	kmovb	%k2,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpuq	$0,L$mask52x4(%rip),%ymm5,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm6,%k2
	kmovb	%k1,%r8d
	kmovb	%k2,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpuq	$0,L$mask52x4(%rip),%ymm7,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm8,%k2
	kmovb	%k1,%edx
	kmovb	%k2,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpuq	$0,L$mask52x4(%rip),%ymm9,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm10,%k2
	kmovb	%k1,%ecx
	kmovb	%k2,%ebx
	shlb	$4,%bl
	orb	%bl,%cl

	addb	%r9b,%r14b
	adcb	%r8b,%r13b
	adcb	%dl,%r12b
	adcb	%cl,%r11b

	xorb	%r9b,%r14b
	xorb	%r8b,%r13b
	xorb	%dl,%r12b
	xorb	%cl,%r11b

	kmovb	%r14d,%k1
	shrb	$4,%r14b
	kmovb	%r14d,%k2
	kmovb	%r13d,%k3
	shrb	$4,%r13b
	kmovb	%r13d,%k4
	kmovb	%r12d,%k5
	shrb	$4,%r12b
	kmovb	%r12d,%k6
	kmovb	%r11d,%k7

	vpsubq	L$mask52x4(%rip),%ymm3,%ymm3{%k1}
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm4{%k2}
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm5{%k3}
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm6{%k4}
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm7{%k5}
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm8{%k6}
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm9{%k7}

	vpandq	L$mask52x4(%rip),%ymm3,%ymm3
	vpandq	L$mask52x4(%rip),%ymm4,%ymm4
	vpandq	L$mask52x4(%rip),%ymm5,%ymm5
	vpandq	L$mask52x4(%rip),%ymm6,%ymm6
	vpandq	L$mask52x4(%rip),%ymm7,%ymm7
	vpandq	L$mask52x4(%rip),%ymm8,%ymm8
	vpandq	L$mask52x4(%rip),%ymm9,%ymm9

	shrb	$4,%r11b
	kmovb	%r11d,%k1

	vpsubq	L$mask52x4(%rip),%ymm10,%ymm10{%k1}

	vpandq	L$mask52x4(%rip),%ymm10,%ymm10

	vmovdqu64	%ymm3,0(%rdi)
	vmovdqu64	%ymm4,32(%rdi)
	vmovdqu64	%ymm5,64(%rdi)
	vmovdqu64	%ymm6,96(%rdi)
	vmovdqu64	%ymm7,128(%rdi)
	vmovdqu64	%ymm8,160(%rdi)
	vmovdqu64	%ymm9,192(%rdi)
	vmovdqu64	%ymm10,224(%rdi)

	vzeroupper
	leaq	(%rsp),%rax

	movq	0(%rax),%r15

	movq	8(%rax),%r14

	movq	16(%rax),%r13

	movq	24(%rax),%r12

	movq	32(%rax),%rbp

	movq	40(%rax),%rbx

	leaq	48(%rax),%rsp

L$ossl_rsaz_amm52x30_x1_ifma256_epilogue:
	.byte	0xf3,0xc3


.section	__DATA,__const
.p2align	5
L$mask52x4:
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.text	

.globl	_ossl_rsaz_amm52x30_x2_ifma256

.p2align	5
_ossl_rsaz_amm52x30_x2_ifma256:

.byte	243,15,30,250
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	vpxord	%ymm0,%ymm0,%ymm0
	vmovdqa64	%ymm0,%ymm3
	vmovdqa64	%ymm0,%ymm4
	vmovdqa64	%ymm0,%ymm5
	vmovdqa64	%ymm0,%ymm6
	vmovdqa64	%ymm0,%ymm7
	vmovdqa64	%ymm0,%ymm8
	vmovdqa64	%ymm0,%ymm9
	vmovdqa64	%ymm0,%ymm10

	vmovdqa64	%ymm0,%ymm11
	vmovdqa64	%ymm0,%ymm12
	vmovdqa64	%ymm0,%ymm13
	vmovdqa64	%ymm0,%ymm14
	vmovdqa64	%ymm0,%ymm15
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm0,%ymm17
	vmovdqa64	%ymm0,%ymm18


	xorl	%r9d,%r9d
	xorl	%r15d,%r15d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax

	movl	$30,%ebx

.p2align	5
L$loop30:
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
	vpmadd52luq	32(%rsi),%ymm1,%ymm4
	vpmadd52luq	64(%rsi),%ymm1,%ymm5
	vpmadd52luq	96(%rsi),%ymm1,%ymm6
	vpmadd52luq	128(%rsi),%ymm1,%ymm7
	vpmadd52luq	160(%rsi),%ymm1,%ymm8
	vpmadd52luq	192(%rsi),%ymm1,%ymm9
	vpmadd52luq	224(%rsi),%ymm1,%ymm10

	vpmadd52luq	0(%rcx),%ymm2,%ymm3
	vpmadd52luq	32(%rcx),%ymm2,%ymm4
	vpmadd52luq	64(%rcx),%ymm2,%ymm5
	vpmadd52luq	96(%rcx),%ymm2,%ymm6
	vpmadd52luq	128(%rcx),%ymm2,%ymm7
	vpmadd52luq	160(%rcx),%ymm2,%ymm8
	vpmadd52luq	192(%rcx),%ymm2,%ymm9
	vpmadd52luq	224(%rcx),%ymm2,%ymm10


	valignq	$1,%ymm3,%ymm4,%ymm3
	valignq	$1,%ymm4,%ymm5,%ymm4
	valignq	$1,%ymm5,%ymm6,%ymm5
	valignq	$1,%ymm6,%ymm7,%ymm6
	valignq	$1,%ymm7,%ymm8,%ymm7
	valignq	$1,%ymm8,%ymm9,%ymm8
	valignq	$1,%ymm9,%ymm10,%ymm9
	valignq	$1,%ymm10,%ymm0,%ymm10

	vmovq	%xmm3,%r13
	addq	%r13,%r9

	vpmadd52huq	0(%rsi),%ymm1,%ymm3
	vpmadd52huq	32(%rsi),%ymm1,%ymm4
	vpmadd52huq	64(%rsi),%ymm1,%ymm5
	vpmadd52huq	96(%rsi),%ymm1,%ymm6
	vpmadd52huq	128(%rsi),%ymm1,%ymm7
	vpmadd52huq	160(%rsi),%ymm1,%ymm8
	vpmadd52huq	192(%rsi),%ymm1,%ymm9
	vpmadd52huq	224(%rsi),%ymm1,%ymm10

	vpmadd52huq	0(%rcx),%ymm2,%ymm3
	vpmadd52huq	32(%rcx),%ymm2,%ymm4
	vpmadd52huq	64(%rcx),%ymm2,%ymm5
	vpmadd52huq	96(%rcx),%ymm2,%ymm6
	vpmadd52huq	128(%rcx),%ymm2,%ymm7
	vpmadd52huq	160(%rcx),%ymm2,%ymm8
	vpmadd52huq	192(%rcx),%ymm2,%ymm9
	vpmadd52huq	224(%rcx),%ymm2,%ymm10
	movq	256(%r11),%r13

	vpbroadcastq	%r13,%ymm1
	movq	256(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	movq	%r12,%r10
	adcq	$0,%r10

	movq	8(%r8),%r13
	imulq	%r15,%r13
	andq	%rax,%r13

	vpbroadcastq	%r13,%ymm2
	movq	256(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	adcq	%r12,%r10

	shrq	$52,%r15
	salq	$12,%r10
	orq	%r10,%r15

	vpmadd52luq	256(%rsi),%ymm1,%ymm11
	vpmadd52luq	288(%rsi),%ymm1,%ymm12
	vpmadd52luq	320(%rsi),%ymm1,%ymm13
	vpmadd52luq	352(%rsi),%ymm1,%ymm14
	vpmadd52luq	384(%rsi),%ymm1,%ymm15
	vpmadd52luq	416(%rsi),%ymm1,%ymm16
	vpmadd52luq	448(%rsi),%ymm1,%ymm17
	vpmadd52luq	480(%rsi),%ymm1,%ymm18

	vpmadd52luq	256(%rcx),%ymm2,%ymm11
	vpmadd52luq	288(%rcx),%ymm2,%ymm12
	vpmadd52luq	320(%rcx),%ymm2,%ymm13
	vpmadd52luq	352(%rcx),%ymm2,%ymm14
	vpmadd52luq	384(%rcx),%ymm2,%ymm15
	vpmadd52luq	416(%rcx),%ymm2,%ymm16
	vpmadd52luq	448(%rcx),%ymm2,%ymm17
	vpmadd52luq	480(%rcx),%ymm2,%ymm18


	valignq	$1,%ymm11,%ymm12,%ymm11
	valignq	$1,%ymm12,%ymm13,%ymm12
	valignq	$1,%ymm13,%ymm14,%ymm13
	valignq	$1,%ymm14,%ymm15,%ymm14
	valignq	$1,%ymm15,%ymm16,%ymm15
	valignq	$1,%ymm16,%ymm17,%ymm16
	valignq	$1,%ymm17,%ymm18,%ymm17
	valignq	$1,%ymm18,%ymm0,%ymm18

	vmovq	%xmm11,%r13
	addq	%r13,%r15

	vpmadd52huq	256(%rsi),%ymm1,%ymm11
	vpmadd52huq	288(%rsi),%ymm1,%ymm12
	vpmadd52huq	320(%rsi),%ymm1,%ymm13
	vpmadd52huq	352(%rsi),%ymm1,%ymm14
	vpmadd52huq	384(%rsi),%ymm1,%ymm15
	vpmadd52huq	416(%rsi),%ymm1,%ymm16
	vpmadd52huq	448(%rsi),%ymm1,%ymm17
	vpmadd52huq	480(%rsi),%ymm1,%ymm18

	vpmadd52huq	256(%rcx),%ymm2,%ymm11
	vpmadd52huq	288(%rcx),%ymm2,%ymm12
	vpmadd52huq	320(%rcx),%ymm2,%ymm13
	vpmadd52huq	352(%rcx),%ymm2,%ymm14
	vpmadd52huq	384(%rcx),%ymm2,%ymm15
	vpmadd52huq	416(%rcx),%ymm2,%ymm16
	vpmadd52huq	448(%rcx),%ymm2,%ymm17
	vpmadd52huq	480(%rcx),%ymm2,%ymm18
	leaq	8(%r11),%r11
	decl	%ebx
	jne	L$loop30

	vpbroadcastq	%r9,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm4,%ymm1
	vpsrlq	$52,%ymm5,%ymm2
	vpsrlq	$52,%ymm6,%ymm19
	vpsrlq	$52,%ymm7,%ymm20
	vpsrlq	$52,%ymm8,%ymm21
	vpsrlq	$52,%ymm9,%ymm22
	vpsrlq	$52,%ymm10,%ymm23


	valignq	$3,%ymm22,%ymm23,%ymm23
	valignq	$3,%ymm21,%ymm22,%ymm22
	valignq	$3,%ymm20,%ymm21,%ymm21
	valignq	$3,%ymm19,%ymm20,%ymm20
	valignq	$3,%ymm2,%ymm19,%ymm19
	valignq	$3,%ymm1,%ymm2,%ymm2
	valignq	$3,%ymm0,%ymm1,%ymm1
	valignq	$3,L$zeros(%rip),%ymm0,%ymm0


	vpandq	L$mask52x4(%rip),%ymm3,%ymm3
	vpandq	L$mask52x4(%rip),%ymm4,%ymm4
	vpandq	L$mask52x4(%rip),%ymm5,%ymm5
	vpandq	L$mask52x4(%rip),%ymm6,%ymm6
	vpandq	L$mask52x4(%rip),%ymm7,%ymm7
	vpandq	L$mask52x4(%rip),%ymm8,%ymm8
	vpandq	L$mask52x4(%rip),%ymm9,%ymm9
	vpandq	L$mask52x4(%rip),%ymm10,%ymm10


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm4,%ymm4
	vpaddq	%ymm2,%ymm5,%ymm5
	vpaddq	%ymm19,%ymm6,%ymm6
	vpaddq	%ymm20,%ymm7,%ymm7
	vpaddq	%ymm21,%ymm8,%ymm8
	vpaddq	%ymm22,%ymm9,%ymm9
	vpaddq	%ymm23,%ymm10,%ymm10



	vpcmpuq	$6,L$mask52x4(%rip),%ymm3,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm4,%k2
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm5,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm6,%k2
	kmovb	%k1,%r13d
	kmovb	%k2,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm7,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm8,%k2
	kmovb	%k1,%r12d
	kmovb	%k2,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm9,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm10,%k2
	kmovb	%k1,%r11d
	kmovb	%k2,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b


	vpcmpuq	$0,L$mask52x4(%rip),%ymm3,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm4,%k2
	kmovb	%k1,%r9d
	kmovb	%k2,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpuq	$0,L$mask52x4(%rip),%ymm5,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm6,%k2
	kmovb	%k1,%r8d
	kmovb	%k2,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpuq	$0,L$mask52x4(%rip),%ymm7,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm8,%k2
	kmovb	%k1,%edx
	kmovb	%k2,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpuq	$0,L$mask52x4(%rip),%ymm9,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm10,%k2
	kmovb	%k1,%ecx
	kmovb	%k2,%ebx
	shlb	$4,%bl
	orb	%bl,%cl

	addb	%r9b,%r14b
	adcb	%r8b,%r13b
	adcb	%dl,%r12b
	adcb	%cl,%r11b

	xorb	%r9b,%r14b
	xorb	%r8b,%r13b
	xorb	%dl,%r12b
	xorb	%cl,%r11b

	kmovb	%r14d,%k1
	shrb	$4,%r14b
	kmovb	%r14d,%k2
	kmovb	%r13d,%k3
	shrb	$4,%r13b
	kmovb	%r13d,%k4
	kmovb	%r12d,%k5
	shrb	$4,%r12b
	kmovb	%r12d,%k6
	kmovb	%r11d,%k7

	vpsubq	L$mask52x4(%rip),%ymm3,%ymm3{%k1}
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm4{%k2}
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm5{%k3}
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm6{%k4}
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm7{%k5}
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm8{%k6}
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm9{%k7}

	vpandq	L$mask52x4(%rip),%ymm3,%ymm3
	vpandq	L$mask52x4(%rip),%ymm4,%ymm4
	vpandq	L$mask52x4(%rip),%ymm5,%ymm5
	vpandq	L$mask52x4(%rip),%ymm6,%ymm6
	vpandq	L$mask52x4(%rip),%ymm7,%ymm7
	vpandq	L$mask52x4(%rip),%ymm8,%ymm8
	vpandq	L$mask52x4(%rip),%ymm9,%ymm9

	shrb	$4,%r11b
	kmovb	%r11d,%k1

	vpsubq	L$mask52x4(%rip),%ymm10,%ymm10{%k1}

	vpandq	L$mask52x4(%rip),%ymm10,%ymm10

	vpbroadcastq	%r15,%ymm0
	vpblendd	$3,%ymm0,%ymm11,%ymm11



	vpsrlq	$52,%ymm11,%ymm0
	vpsrlq	$52,%ymm12,%ymm1
	vpsrlq	$52,%ymm13,%ymm2
	vpsrlq	$52,%ymm14,%ymm19
	vpsrlq	$52,%ymm15,%ymm20
	vpsrlq	$52,%ymm16,%ymm21
	vpsrlq	$52,%ymm17,%ymm22
	vpsrlq	$52,%ymm18,%ymm23


	valignq	$3,%ymm22,%ymm23,%ymm23
	valignq	$3,%ymm21,%ymm22,%ymm22
	valignq	$3,%ymm20,%ymm21,%ymm21
	valignq	$3,%ymm19,%ymm20,%ymm20
	valignq	$3,%ymm2,%ymm19,%ymm19
	valignq	$3,%ymm1,%ymm2,%ymm2
	valignq	$3,%ymm0,%ymm1,%ymm1
	valignq	$3,L$zeros(%rip),%ymm0,%ymm0


	vpandq	L$mask52x4(%rip),%ymm11,%ymm11
	vpandq	L$mask52x4(%rip),%ymm12,%ymm12
	vpandq	L$mask52x4(%rip),%ymm13,%ymm13
	vpandq	L$mask52x4(%rip),%ymm14,%ymm14
	vpandq	L$mask52x4(%rip),%ymm15,%ymm15
	vpandq	L$mask52x4(%rip),%ymm16,%ymm16
	vpandq	L$mask52x4(%rip),%ymm17,%ymm17
	vpandq	L$mask52x4(%rip),%ymm18,%ymm18


	vpaddq	%ymm0,%ymm11,%ymm11
	vpaddq	%ymm1,%ymm12,%ymm12
	vpaddq	%ymm2,%ymm13,%ymm13
	vpaddq	%ymm19,%ymm14,%ymm14
	vpaddq	%ymm20,%ymm15,%ymm15
	vpaddq	%ymm21,%ymm16,%ymm16
	vpaddq	%ymm22,%ymm17,%ymm17
	vpaddq	%ymm23,%ymm18,%ymm18



	vpcmpuq	$6,L$mask52x4(%rip),%ymm11,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm12,%k2
	kmovb	%k1,%r14d
	kmovb	%k2,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm13,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm14,%k2
	kmovb	%k1,%r13d
	kmovb	%k2,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm15,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm16,%k2
	kmovb	%k1,%r12d
	kmovb	%k2,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpuq	$6,L$mask52x4(%rip),%ymm17,%k1
	vpcmpuq	$6,L$mask52x4(%rip),%ymm18,%k2
	kmovb	%k1,%r11d
	kmovb	%k2,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b


	vpcmpuq	$0,L$mask52x4(%rip),%ymm11,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm12,%k2
	kmovb	%k1,%r9d
	kmovb	%k2,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpuq	$0,L$mask52x4(%rip),%ymm13,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm14,%k2
	kmovb	%k1,%r8d
	kmovb	%k2,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpuq	$0,L$mask52x4(%rip),%ymm15,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm16,%k2
	kmovb	%k1,%edx
	kmovb	%k2,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpuq	$0,L$mask52x4(%rip),%ymm17,%k1
	vpcmpuq	$0,L$mask52x4(%rip),%ymm18,%k2
	kmovb	%k1,%ecx
	kmovb	%k2,%ebx
	shlb	$4,%bl
	orb	%bl,%cl

	addb	%r9b,%r14b
	adcb	%r8b,%r13b
	adcb	%dl,%r12b
	adcb	%cl,%r11b

	xorb	%r9b,%r14b
	xorb	%r8b,%r13b
	xorb	%dl,%r12b
	xorb	%cl,%r11b

	kmovb	%r14d,%k1
	shrb	$4,%r14b
	kmovb	%r14d,%k2
	kmovb	%r13d,%k3
	shrb	$4,%r13b
	kmovb	%r13d,%k4
	kmovb	%r12d,%k5
	shrb	$4,%r12b
	kmovb	%r12d,%k6
	kmovb	%r11d,%k7

	vpsubq	L$mask52x4(%rip),%ymm11,%ymm11{%k1}
	vpsubq	L$mask52x4(%rip),%ymm12,%ymm12{%k2}
	vpsubq	L$mask52x4(%rip),%ymm13,%ymm13{%k3}
	vpsubq	L$mask52x4(%rip),%ymm14,%ymm14{%k4}
	vpsubq	L$mask52x4(%rip),%ymm15,%ymm15{%k5}
	vpsubq	L$mask52x4(%rip),%ymm16,%ymm16{%k6}
	vpsubq	L$mask52x4(%rip),%ymm17,%ymm17{%k7}

	vpandq	L$mask52x4(%rip),%ymm11,%ymm11
	vpandq	L$mask52x4(%rip),%ymm12,%ymm12
	vpandq	L$mask52x4(%rip),%ymm13,%ymm13
	vpandq	L$mask52x4(%rip),%ymm14,%ymm14
	vpandq	L$mask52x4(%rip),%ymm15,%ymm15
	vpandq	L$mask52x4(%rip),%ymm16,%ymm16
	vpandq	L$mask52x4(%rip),%ymm17,%ymm17

	shrb	$4,%r11b
	kmovb	%r11d,%k1

	vpsubq	L$mask52x4(%rip),%ymm18,%ymm18{%k1}

	vpandq	L$mask52x4(%rip),%ymm18,%ymm18

	vmovdqu64	%ymm3,0(%rdi)
	vmovdqu64	%ymm4,32(%rdi)
	vmovdqu64	%ymm5,64(%rdi)
	vmovdqu64	%ymm6,96(%rdi)
	vmovdqu64	%ymm7,128(%rdi)
	vmovdqu64	%ymm8,160(%rdi)
	vmovdqu64	%ymm9,192(%rdi)
	vmovdqu64	%ymm10,224(%rdi)

	vmovdqu64	%ymm11,256(%rdi)
	vmovdqu64	%ymm12,288(%rdi)
	vmovdqu64	%ymm13,320(%rdi)
	vmovdqu64	%ymm14,352(%rdi)
	vmovdqu64	%ymm15,384(%rdi)
	vmovdqu64	%ymm16,416(%rdi)
	vmovdqu64	%ymm17,448(%rdi)
	vmovdqu64	%ymm18,480(%rdi)

	vzeroupper
	leaq	(%rsp),%rax

	movq	0(%rax),%r15

	movq	8(%rax),%r14

	movq	16(%rax),%r13

	movq	24(%rax),%r12

	movq	32(%rax),%rbp

	movq	40(%rax),%rbx

	leaq	48(%rax),%rsp

L$ossl_rsaz_amm52x30_x2_ifma256_epilogue:
	.byte	0xf3,0xc3


.text	

.p2align	5
.globl	_ossl_extract_multiplier_2x30_win5

_ossl_extract_multiplier_2x30_win5:

.byte	243,15,30,250
	vmovdqa64	L$ones(%rip),%ymm30
	vpbroadcastq	%rdx,%ymm28
	vpbroadcastq	%rcx,%ymm29
	leaq	16384(%rsi),%rax


	vpxor	%xmm0,%xmm0,%xmm0
	vmovdqa64	%ymm0,%ymm27
	vmovdqa64	%ymm0,%ymm1
	vmovdqa64	%ymm0,%ymm2
	vmovdqa64	%ymm0,%ymm3
	vmovdqa64	%ymm0,%ymm4
	vmovdqa64	%ymm0,%ymm5
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm0,%ymm17
	vmovdqa64	%ymm0,%ymm18
	vmovdqa64	%ymm0,%ymm19
	vmovdqa64	%ymm0,%ymm20
	vmovdqa64	%ymm0,%ymm21
	vmovdqa64	%ymm0,%ymm22
	vmovdqa64	%ymm0,%ymm23
	vmovdqa64	%ymm0,%ymm24
	vmovdqa64	%ymm0,%ymm25

.p2align	5
L$loop:
	vpcmpq	$0,%ymm27,%ymm28,%k1
	vpcmpq	$0,%ymm27,%ymm29,%k2
	vmovdqu64	0(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm0,%ymm0{%k1}
	vmovdqu64	32(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm1,%ymm1{%k1}
	vmovdqu64	64(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm2,%ymm2{%k1}
	vmovdqu64	96(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm3,%ymm3{%k1}
	vmovdqu64	128(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm4,%ymm4{%k1}
	vmovdqu64	160(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm5,%ymm5{%k1}
	vmovdqu64	192(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm16,%ymm16{%k1}
	vmovdqu64	224(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm17,%ymm17{%k1}
	vmovdqu64	256(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm18,%ymm18{%k2}
	vmovdqu64	288(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm19,%ymm19{%k2}
	vmovdqu64	320(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm20,%ymm20{%k2}
	vmovdqu64	352(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm21,%ymm21{%k2}
	vmovdqu64	384(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm22,%ymm22{%k2}
	vmovdqu64	416(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm23,%ymm23{%k2}
	vmovdqu64	448(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm24,%ymm24{%k2}
	vmovdqu64	480(%rsi),%ymm26
	vpblendmq	%ymm26,%ymm25,%ymm25{%k2}
	vpaddq	%ymm30,%ymm27,%ymm27
	addq	$512,%rsi
	cmpq	%rsi,%rax
	jne	L$loop
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
	vmovdqu64	%ymm20,320(%rdi)
	vmovdqu64	%ymm21,352(%rdi)
	vmovdqu64	%ymm22,384(%rdi)
	vmovdqu64	%ymm23,416(%rdi)
	vmovdqu64	%ymm24,448(%rdi)
	vmovdqu64	%ymm25,480(%rdi)

	.byte	0xf3,0xc3


.section	__DATA,__const
.p2align	5
L$ones:
.quad	1,1,1,1
L$zeros:
.quad	0,0,0,0
