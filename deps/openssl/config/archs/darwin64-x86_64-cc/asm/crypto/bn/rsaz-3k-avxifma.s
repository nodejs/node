.text	

.globl	_ossl_rsaz_amm52x30_x1_avxifma256

.p2align	5
_ossl_rsaz_amm52x30_x1_avxifma256:

.byte	243,15,30,250
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	vpxor	%ymm0,%ymm0,%ymm0
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8
	vmovapd	%ymm0,%ymm9
	vmovapd	%ymm0,%ymm10

	xorl	%r9d,%r9d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax


	movl	$7,%ebx

.p2align	5
L$loop7:
	movq	0(%r11),%r13

	vpbroadcastq	0(%r11),%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp
	movq	8(%r11),%r13

	vpbroadcastq	8(%r11),%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp
	movq	16(%r11),%r13

	vpbroadcastq	16(%r11),%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp
	movq	24(%r11),%r13

	vpbroadcastq	24(%r11),%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp
	leaq	32(%r11),%r11
	decl	%ebx
	jne	L$loop7
	movq	0(%r11),%r13

	vpbroadcastq	0(%r11),%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp
	movq	8(%r11),%r13

	vpbroadcastq	8(%r11),%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	%r8,%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm4,%ymm1
	vpsrlq	$52,%ymm5,%ymm2
	vpsrlq	$52,%ymm6,%ymm11
	vpsrlq	$52,%ymm7,%ymm12
	vpsrlq	$52,%ymm8,%ymm13
	vpsrlq	$52,%ymm9,%ymm14
	vpsrlq	$52,%ymm10,%ymm15

	leaq	-32(%rsp),%rsp
	vmovupd	%ymm3,(%rsp)


	vpermq	$144,%ymm15,%ymm15
	vpermq	$3,%ymm14,%ymm3
	vblendpd	$1,%ymm3,%ymm15,%ymm15

	vpermq	$144,%ymm14,%ymm14
	vpermq	$3,%ymm13,%ymm3
	vblendpd	$1,%ymm3,%ymm14,%ymm14

	vpermq	$144,%ymm13,%ymm13
	vpermq	$3,%ymm12,%ymm3
	vblendpd	$1,%ymm3,%ymm13,%ymm13

	vpermq	$144,%ymm12,%ymm12
	vpermq	$3,%ymm11,%ymm3
	vblendpd	$1,%ymm3,%ymm12,%ymm12

	vpermq	$144,%ymm11,%ymm11
	vpermq	$3,%ymm2,%ymm3
	vblendpd	$1,%ymm3,%ymm11,%ymm11

	vpermq	$144,%ymm2,%ymm2
	vpermq	$3,%ymm1,%ymm3
	vblendpd	$1,%ymm3,%ymm2,%ymm2

	vpermq	$144,%ymm1,%ymm1
	vpermq	$3,%ymm0,%ymm3
	vblendpd	$1,%ymm3,%ymm1,%ymm1

	vpermq	$144,%ymm0,%ymm0
	vpand	L$high64x3(%rip),%ymm0,%ymm0

	vmovupd	(%rsp),%ymm3
	leaq	32(%rsp),%rsp


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm4,%ymm4
	vpaddq	%ymm2,%ymm5,%ymm5
	vpaddq	%ymm11,%ymm6,%ymm6
	vpaddq	%ymm12,%ymm7,%ymm7
	vpaddq	%ymm13,%ymm8,%ymm8
	vpaddq	%ymm14,%ymm9,%ymm9
	vpaddq	%ymm15,%ymm10,%ymm10



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpgtq	L$mask52x4(%rip),%ymm4,%ymm1
	vmovmskpd	%ymm0,%r14d
	vmovmskpd	%ymm1,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm2
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm11
	vmovmskpd	%ymm2,%r13d
	vmovmskpd	%ymm11,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm12
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm12,%r12d
	vmovmskpd	%ymm13,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpgtq	L$mask52x4(%rip),%ymm9,%ymm14
	vpcmpgtq	L$mask52x4(%rip),%ymm10,%ymm15
	vmovmskpd	%ymm14,%r11d
	vmovmskpd	%ymm15,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpeqq	L$mask52x4(%rip),%ymm4,%ymm1
	vmovmskpd	%ymm0,%r9d
	vmovmskpd	%ymm1,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm2
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm11
	vmovmskpd	%ymm2,%r8d
	vmovmskpd	%ymm11,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm12
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm12,%edx
	vmovmskpd	%ymm13,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpeqq	L$mask52x4(%rip),%ymm9,%ymm14
	vpcmpeqq	L$mask52x4(%rip),%ymm10,%ymm15
	vmovmskpd	%ymm14,%ecx
	vmovmskpd	%ymm15,%ebx
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

	leaq	L$kmasklut(%rip),%rdx

	movb	%r14b,%r10b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm0
	shlq	$5,%r14
	vmovapd	(%rdx,%r14), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm3,%ymm3

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm4,%ymm4

	movb	%r13b,%r10b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm0
	shlq	$5,%r13
	vmovapd	(%rdx,%r13), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm5,%ymm5

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm6,%ymm6

	movb	%r12b,%r10b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm0
	shlq	$5,%r12
	vmovapd	(%rdx,%r12), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm7,%ymm7

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm8,%ymm8

	movb	%r11b,%r10b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm0
	shlq	$5,%r11
	vmovapd	(%rdx,%r11), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm9,%ymm9

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm10,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm10,%ymm10

	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9

	vpand	L$mask52x4(%rip),%ymm10,%ymm10

	vmovdqu	%ymm3,0(%rdi)
	vmovdqu	%ymm4,32(%rdi)
	vmovdqu	%ymm5,64(%rdi)
	vmovdqu	%ymm6,96(%rdi)
	vmovdqu	%ymm7,128(%rdi)
	vmovdqu	%ymm8,160(%rdi)
	vmovdqu	%ymm9,192(%rdi)
	vmovdqu	%ymm10,224(%rdi)

	vzeroupper
	leaq	(%rsp),%rax

	movq	0(%rax),%r15

	movq	8(%rax),%r14

	movq	16(%rax),%r13

	movq	24(%rax),%r12

	movq	32(%rax),%rbp

	movq	40(%rax),%rbx

	leaq	48(%rax),%rsp

L$ossl_rsaz_amm52x30_x1_avxifma256_epilogue:
	.byte	0xf3,0xc3


.section	__DATA,__const
.p2align	5
L$mask52x4:
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
.quad	0xfffffffffffff
L$high64x3:
.quad	0x0
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
L$kmasklut:

.quad	0x0
.quad	0x0
.quad	0x0
.quad	0x0

.quad	0xffffffffffffffff
.quad	0x0
.quad	0x0
.quad	0x0

.quad	0x0
.quad	0xffffffffffffffff
.quad	0x0
.quad	0x0

.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0x0
.quad	0x0

.quad	0x0
.quad	0x0
.quad	0xffffffffffffffff
.quad	0x0

.quad	0xffffffffffffffff
.quad	0x0
.quad	0xffffffffffffffff
.quad	0x0

.quad	0x0
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0x0

.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0x0

.quad	0x0
.quad	0x0
.quad	0x0
.quad	0xffffffffffffffff

.quad	0xffffffffffffffff
.quad	0x0
.quad	0x0
.quad	0xffffffffffffffff

.quad	0x0
.quad	0xffffffffffffffff
.quad	0x0
.quad	0xffffffffffffffff

.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0x0
.quad	0xffffffffffffffff

.quad	0x0
.quad	0x0
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff

.quad	0xffffffffffffffff
.quad	0x0
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff

.quad	0x0
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff

.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.quad	0xffffffffffffffff
.text	

.globl	_ossl_rsaz_amm52x30_x2_avxifma256

.p2align	5
_ossl_rsaz_amm52x30_x2_avxifma256:

.byte	243,15,30,250
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	vpxor	%ymm0,%ymm0,%ymm0
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8
	vmovapd	%ymm0,%ymm9
	vmovapd	%ymm0,%ymm10

	xorl	%r9d,%r9d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax

	movl	$30,%ebx

.p2align	5
L$loop30:
	movq	0(%r11),%r13

	vpbroadcastq	0(%r11),%ymm1
	movq	0(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	(%r8),%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	0(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp
	leaq	8(%r11),%r11
	decl	%ebx
	jne	L$loop30

	pushq	%r11
	pushq	%rsi
	pushq	%rcx
	pushq	%r8

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm4,%ymm1
	vpsrlq	$52,%ymm5,%ymm2
	vpsrlq	$52,%ymm6,%ymm11
	vpsrlq	$52,%ymm7,%ymm12
	vpsrlq	$52,%ymm8,%ymm13
	vpsrlq	$52,%ymm9,%ymm14
	vpsrlq	$52,%ymm10,%ymm15

	leaq	-32(%rsp),%rsp
	vmovupd	%ymm3,(%rsp)


	vpermq	$144,%ymm15,%ymm15
	vpermq	$3,%ymm14,%ymm3
	vblendpd	$1,%ymm3,%ymm15,%ymm15

	vpermq	$144,%ymm14,%ymm14
	vpermq	$3,%ymm13,%ymm3
	vblendpd	$1,%ymm3,%ymm14,%ymm14

	vpermq	$144,%ymm13,%ymm13
	vpermq	$3,%ymm12,%ymm3
	vblendpd	$1,%ymm3,%ymm13,%ymm13

	vpermq	$144,%ymm12,%ymm12
	vpermq	$3,%ymm11,%ymm3
	vblendpd	$1,%ymm3,%ymm12,%ymm12

	vpermq	$144,%ymm11,%ymm11
	vpermq	$3,%ymm2,%ymm3
	vblendpd	$1,%ymm3,%ymm11,%ymm11

	vpermq	$144,%ymm2,%ymm2
	vpermq	$3,%ymm1,%ymm3
	vblendpd	$1,%ymm3,%ymm2,%ymm2

	vpermq	$144,%ymm1,%ymm1
	vpermq	$3,%ymm0,%ymm3
	vblendpd	$1,%ymm3,%ymm1,%ymm1

	vpermq	$144,%ymm0,%ymm0
	vpand	L$high64x3(%rip),%ymm0,%ymm0

	vmovupd	(%rsp),%ymm3
	leaq	32(%rsp),%rsp


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm4,%ymm4
	vpaddq	%ymm2,%ymm5,%ymm5
	vpaddq	%ymm11,%ymm6,%ymm6
	vpaddq	%ymm12,%ymm7,%ymm7
	vpaddq	%ymm13,%ymm8,%ymm8
	vpaddq	%ymm14,%ymm9,%ymm9
	vpaddq	%ymm15,%ymm10,%ymm10



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpgtq	L$mask52x4(%rip),%ymm4,%ymm1
	vmovmskpd	%ymm0,%r14d
	vmovmskpd	%ymm1,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm2
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm11
	vmovmskpd	%ymm2,%r13d
	vmovmskpd	%ymm11,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm12
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm12,%r12d
	vmovmskpd	%ymm13,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpgtq	L$mask52x4(%rip),%ymm9,%ymm14
	vpcmpgtq	L$mask52x4(%rip),%ymm10,%ymm15
	vmovmskpd	%ymm14,%r11d
	vmovmskpd	%ymm15,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpeqq	L$mask52x4(%rip),%ymm4,%ymm1
	vmovmskpd	%ymm0,%r9d
	vmovmskpd	%ymm1,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm2
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm11
	vmovmskpd	%ymm2,%r8d
	vmovmskpd	%ymm11,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm12
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm12,%edx
	vmovmskpd	%ymm13,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpeqq	L$mask52x4(%rip),%ymm9,%ymm14
	vpcmpeqq	L$mask52x4(%rip),%ymm10,%ymm15
	vmovmskpd	%ymm14,%ecx
	vmovmskpd	%ymm15,%ebx
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

	leaq	L$kmasklut(%rip),%rdx

	movb	%r14b,%r10b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm0
	shlq	$5,%r14
	vmovapd	(%rdx,%r14), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm3,%ymm3

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm4,%ymm4

	movb	%r13b,%r10b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm0
	shlq	$5,%r13
	vmovapd	(%rdx,%r13), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm5,%ymm5

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm6,%ymm6

	movb	%r12b,%r10b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm0
	shlq	$5,%r12
	vmovapd	(%rdx,%r12), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm7,%ymm7

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm8,%ymm8

	movb	%r11b,%r10b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm0
	shlq	$5,%r11
	vmovapd	(%rdx,%r11), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm9,%ymm9

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm10,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm10,%ymm10

	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9

	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	popq	%r8
	popq	%rcx
	popq	%rsi
	popq	%r11

	vmovdqu	%ymm3,0(%rdi)
	vmovdqu	%ymm4,32(%rdi)
	vmovdqu	%ymm5,64(%rdi)
	vmovdqu	%ymm6,96(%rdi)
	vmovdqu	%ymm7,128(%rdi)
	vmovdqu	%ymm8,160(%rdi)
	vmovdqu	%ymm9,192(%rdi)
	vmovdqu	%ymm10,224(%rdi)

	xorl	%r15d,%r15d

	leaq	16(%r11),%r11
	movq	$0xfffffffffffff,%rax

	movl	$30,%ebx

	vpxor	%ymm0,%ymm0,%ymm0
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8
	vmovapd	%ymm0,%ymm9
	vmovapd	%ymm0,%ymm10
.p2align	5
L$loop40:
	movq	0(%r11),%r13

	vpbroadcastq	0(%r11),%ymm1
	movq	256(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	8(%r8),%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	256(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-264(%rsp),%rsp

{vex} vpmadd52luq 256(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 288(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 320(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 352(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 384(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 416(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 448(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 480(%rsi), %ymm1, %ymm10

{vex} vpmadd52luq 256(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 288(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 320(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 352(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 384(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 416(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 448(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 480(%rcx), %ymm2, %ymm10


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	movq	$0,256(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10

	addq	8(%rsp),%r9

{vex} vpmadd52huq 256(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 288(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 320(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 352(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 384(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 416(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 448(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 480(%rsi), %ymm1, %ymm10

{vex} vpmadd52huq 256(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 288(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 320(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 352(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 384(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 416(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 448(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 480(%rcx), %ymm2, %ymm10

	leaq	264(%rsp),%rsp
	leaq	8(%r11),%r11
	decl	%ebx
	jne	L$loop40

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm4,%ymm1
	vpsrlq	$52,%ymm5,%ymm2
	vpsrlq	$52,%ymm6,%ymm11
	vpsrlq	$52,%ymm7,%ymm12
	vpsrlq	$52,%ymm8,%ymm13
	vpsrlq	$52,%ymm9,%ymm14
	vpsrlq	$52,%ymm10,%ymm15

	leaq	-32(%rsp),%rsp
	vmovupd	%ymm3,(%rsp)


	vpermq	$144,%ymm15,%ymm15
	vpermq	$3,%ymm14,%ymm3
	vblendpd	$1,%ymm3,%ymm15,%ymm15

	vpermq	$144,%ymm14,%ymm14
	vpermq	$3,%ymm13,%ymm3
	vblendpd	$1,%ymm3,%ymm14,%ymm14

	vpermq	$144,%ymm13,%ymm13
	vpermq	$3,%ymm12,%ymm3
	vblendpd	$1,%ymm3,%ymm13,%ymm13

	vpermq	$144,%ymm12,%ymm12
	vpermq	$3,%ymm11,%ymm3
	vblendpd	$1,%ymm3,%ymm12,%ymm12

	vpermq	$144,%ymm11,%ymm11
	vpermq	$3,%ymm2,%ymm3
	vblendpd	$1,%ymm3,%ymm11,%ymm11

	vpermq	$144,%ymm2,%ymm2
	vpermq	$3,%ymm1,%ymm3
	vblendpd	$1,%ymm3,%ymm2,%ymm2

	vpermq	$144,%ymm1,%ymm1
	vpermq	$3,%ymm0,%ymm3
	vblendpd	$1,%ymm3,%ymm1,%ymm1

	vpermq	$144,%ymm0,%ymm0
	vpand	L$high64x3(%rip),%ymm0,%ymm0

	vmovupd	(%rsp),%ymm3
	leaq	32(%rsp),%rsp


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm4,%ymm4
	vpaddq	%ymm2,%ymm5,%ymm5
	vpaddq	%ymm11,%ymm6,%ymm6
	vpaddq	%ymm12,%ymm7,%ymm7
	vpaddq	%ymm13,%ymm8,%ymm8
	vpaddq	%ymm14,%ymm9,%ymm9
	vpaddq	%ymm15,%ymm10,%ymm10



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpgtq	L$mask52x4(%rip),%ymm4,%ymm1
	vmovmskpd	%ymm0,%r14d
	vmovmskpd	%ymm1,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm2
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm11
	vmovmskpd	%ymm2,%r13d
	vmovmskpd	%ymm11,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm12
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm12,%r12d
	vmovmskpd	%ymm13,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpgtq	L$mask52x4(%rip),%ymm9,%ymm14
	vpcmpgtq	L$mask52x4(%rip),%ymm10,%ymm15
	vmovmskpd	%ymm14,%r11d
	vmovmskpd	%ymm15,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpeqq	L$mask52x4(%rip),%ymm4,%ymm1
	vmovmskpd	%ymm0,%r9d
	vmovmskpd	%ymm1,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm2
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm11
	vmovmskpd	%ymm2,%r8d
	vmovmskpd	%ymm11,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm12
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm12,%edx
	vmovmskpd	%ymm13,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpeqq	L$mask52x4(%rip),%ymm9,%ymm14
	vpcmpeqq	L$mask52x4(%rip),%ymm10,%ymm15
	vmovmskpd	%ymm14,%ecx
	vmovmskpd	%ymm15,%ebx
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

	leaq	L$kmasklut(%rip),%rdx

	movb	%r14b,%r10b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm0
	shlq	$5,%r14
	vmovapd	(%rdx,%r14), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm3,%ymm3

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm4,%ymm4

	movb	%r13b,%r10b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm0
	shlq	$5,%r13
	vmovapd	(%rdx,%r13), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm5,%ymm5

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm6,%ymm6

	movb	%r12b,%r10b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm0
	shlq	$5,%r12
	vmovapd	(%rdx,%r12), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm7,%ymm7

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm8,%ymm8

	movb	%r11b,%r10b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm0
	shlq	$5,%r11
	vmovapd	(%rdx,%r11), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm9,%ymm9

	shrb	$4,%r10b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm10,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm10,%ymm10

	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9

	vpand	L$mask52x4(%rip),%ymm10,%ymm10

	vmovdqu	%ymm3,256(%rdi)
	vmovdqu	%ymm4,288(%rdi)
	vmovdqu	%ymm5,320(%rdi)
	vmovdqu	%ymm6,352(%rdi)
	vmovdqu	%ymm7,384(%rdi)
	vmovdqu	%ymm8,416(%rdi)
	vmovdqu	%ymm9,448(%rdi)
	vmovdqu	%ymm10,480(%rdi)

	vzeroupper
	leaq	(%rsp),%rax

	movq	0(%rax),%r15

	movq	8(%rax),%r14

	movq	16(%rax),%r13

	movq	24(%rax),%r12

	movq	32(%rax),%rbp

	movq	40(%rax),%rbx

	leaq	48(%rax),%rsp

L$ossl_rsaz_amm52x30_x2_avxifma256_epilogue:
	.byte	0xf3,0xc3


.text	

.p2align	5
.globl	_ossl_extract_multiplier_2x30_win5_avx

_ossl_extract_multiplier_2x30_win5_avx:

.byte	243,15,30,250
	vmovapd	L$ones(%rip),%ymm12
	vmovq	%rdx,%xmm8
	vpbroadcastq	%xmm8,%ymm10
	vmovq	%rcx,%xmm8
	vpbroadcastq	%xmm8,%ymm11
	leaq	16384(%rsi),%rax


	vpxor	%xmm0,%xmm0,%xmm0
	vmovapd	%ymm0,%ymm9
	vmovapd	%ymm0,%ymm1
	vmovapd	%ymm0,%ymm2
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7

.p2align	5
L$loop:
	vpcmpeqq	%ymm9,%ymm10,%ymm13
	vmovdqu	0(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm0,%ymm0
	vmovdqu	32(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm1,%ymm1
	vmovdqu	64(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm2,%ymm2
	vmovdqu	96(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm3,%ymm3
	vmovdqu	128(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm4,%ymm4
	vmovdqu	160(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm5,%ymm5
	vmovdqu	192(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm6,%ymm6
	vmovdqu	224(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm7,%ymm7
	vpaddq	%ymm12,%ymm9,%ymm9
	addq	$512,%rsi
	cmpq	%rsi,%rax
	jne	L$loop
	vmovdqu	%ymm0,0(%rdi)
	vmovdqu	%ymm1,32(%rdi)
	vmovdqu	%ymm2,64(%rdi)
	vmovdqu	%ymm3,96(%rdi)
	vmovdqu	%ymm4,128(%rdi)
	vmovdqu	%ymm5,160(%rdi)
	vmovdqu	%ymm6,192(%rdi)
	vmovdqu	%ymm7,224(%rdi)
	leaq	-16384(%rax),%rsi


	vpxor	%xmm0,%xmm0,%xmm0
	vmovapd	%ymm0,%ymm9
	vmovapd	%ymm0,%ymm0
	vmovapd	%ymm0,%ymm1
	vmovapd	%ymm0,%ymm2
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7

.p2align	5
L$loop_8_15:
	vpcmpeqq	%ymm9,%ymm11,%ymm13
	vmovdqu	256(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm0,%ymm0
	vmovdqu	288(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm1,%ymm1
	vmovdqu	320(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm2,%ymm2
	vmovdqu	352(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm3,%ymm3
	vmovdqu	384(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm4,%ymm4
	vmovdqu	416(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm5,%ymm5
	vmovdqu	448(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm6,%ymm6
	vmovdqu	480(%rsi),%ymm8

	vblendvpd	%ymm13,%ymm8,%ymm7,%ymm7
	vpaddq	%ymm12,%ymm9,%ymm9
	addq	$512,%rsi
	cmpq	%rsi,%rax
	jne	L$loop_8_15
	vmovdqu	%ymm0,256(%rdi)
	vmovdqu	%ymm1,288(%rdi)
	vmovdqu	%ymm2,320(%rdi)
	vmovdqu	%ymm3,352(%rdi)
	vmovdqu	%ymm4,384(%rdi)
	vmovdqu	%ymm5,416(%rdi)
	vmovdqu	%ymm6,448(%rdi)
	vmovdqu	%ymm7,480(%rdi)

	.byte	0xf3,0xc3


.section	__DATA,__const
.p2align	5
L$ones:
.quad	1,1,1,1
L$zeros:
.quad	0,0,0,0
