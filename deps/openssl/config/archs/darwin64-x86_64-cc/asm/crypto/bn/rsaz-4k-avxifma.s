.text	

.globl	_ossl_rsaz_amm52x40_x1_avxifma256

.p2align	5
_ossl_rsaz_amm52x40_x1_avxifma256:

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
	vmovapd	%ymm0,%ymm11
	vmovapd	%ymm0,%ymm12

	xorl	%r9d,%r9d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax


	movl	$10,%ebx

.p2align	5
L$loop10:
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

	leaq	-328(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52luq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52luq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52luq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52luq 288(%rcx), %ymm2, %ymm12
	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	vmovdqu	%ymm11,256(%rsp)
	vmovdqu	%ymm12,288(%rsp)
	movq	$0,320(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10
	vmovdqu	264(%rsp),%ymm11
	vmovdqu	296(%rsp),%ymm12

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52huq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52huq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52huq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52huq 288(%rcx), %ymm2, %ymm12
	leaq	328(%rsp),%rsp
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

	leaq	-328(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52luq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52luq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52luq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52luq 288(%rcx), %ymm2, %ymm12
	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	vmovdqu	%ymm11,256(%rsp)
	vmovdqu	%ymm12,288(%rsp)
	movq	$0,320(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10
	vmovdqu	264(%rsp),%ymm11
	vmovdqu	296(%rsp),%ymm12

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52huq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52huq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52huq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52huq 288(%rcx), %ymm2, %ymm12
	leaq	328(%rsp),%rsp
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

	leaq	-328(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52luq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52luq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52luq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52luq 288(%rcx), %ymm2, %ymm12
	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	vmovdqu	%ymm11,256(%rsp)
	vmovdqu	%ymm12,288(%rsp)
	movq	$0,320(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10
	vmovdqu	264(%rsp),%ymm11
	vmovdqu	296(%rsp),%ymm12

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52huq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52huq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52huq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52huq 288(%rcx), %ymm2, %ymm12
	leaq	328(%rsp),%rsp
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

	leaq	-328(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52luq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52luq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52luq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52luq 288(%rcx), %ymm2, %ymm12
	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	vmovdqu	%ymm11,256(%rsp)
	vmovdqu	%ymm12,288(%rsp)
	movq	$0,320(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10
	vmovdqu	264(%rsp),%ymm11
	vmovdqu	296(%rsp),%ymm12

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52huq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52huq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52huq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52huq 288(%rcx), %ymm2, %ymm12
	leaq	328(%rsp),%rsp
	leaq	32(%r11),%r11
	decl	%ebx
	jne	L$loop10

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3

	leaq	-640(%rsp),%rsp
	vmovupd	%ymm3,0(%rsp)
	vmovupd	%ymm4,32(%rsp)
	vmovupd	%ymm5,64(%rsp)
	vmovupd	%ymm6,96(%rsp)
	vmovupd	%ymm7,128(%rsp)
	vmovupd	%ymm8,160(%rsp)
	vmovupd	%ymm9,192(%rsp)
	vmovupd	%ymm10,224(%rsp)
	vmovupd	%ymm11,256(%rsp)
	vmovupd	%ymm12,288(%rsp)



	vpsrlq	$52,%ymm3,%ymm3
	vpsrlq	$52,%ymm4,%ymm4
	vpsrlq	$52,%ymm5,%ymm5
	vpsrlq	$52,%ymm6,%ymm6
	vpsrlq	$52,%ymm7,%ymm7
	vpsrlq	$52,%ymm8,%ymm8
	vpsrlq	$52,%ymm9,%ymm9
	vpsrlq	$52,%ymm10,%ymm10
	vpsrlq	$52,%ymm11,%ymm11
	vpsrlq	$52,%ymm12,%ymm12


	vpermq	$144,%ymm12,%ymm12
	vpermq	$3,%ymm11,%ymm13
	vblendpd	$1,%ymm13,%ymm12,%ymm12

	vpermq	$144,%ymm11,%ymm11
	vpermq	$3,%ymm10,%ymm13
	vblendpd	$1,%ymm13,%ymm11,%ymm11

	vpermq	$144,%ymm10,%ymm10
	vpermq	$3,%ymm9,%ymm13
	vblendpd	$1,%ymm13,%ymm10,%ymm10

	vpermq	$144,%ymm9,%ymm9
	vpermq	$3,%ymm8,%ymm13
	vblendpd	$1,%ymm13,%ymm9,%ymm9

	vpermq	$144,%ymm8,%ymm8
	vpermq	$3,%ymm7,%ymm13
	vblendpd	$1,%ymm13,%ymm8,%ymm8

	vpermq	$144,%ymm7,%ymm7
	vpermq	$3,%ymm6,%ymm13
	vblendpd	$1,%ymm13,%ymm7,%ymm7

	vpermq	$144,%ymm6,%ymm6
	vpermq	$3,%ymm5,%ymm13
	vblendpd	$1,%ymm13,%ymm6,%ymm6

	vpermq	$144,%ymm5,%ymm5
	vpermq	$3,%ymm4,%ymm13
	vblendpd	$1,%ymm13,%ymm5,%ymm5

	vpermq	$144,%ymm4,%ymm4
	vpermq	$3,%ymm3,%ymm13
	vblendpd	$1,%ymm13,%ymm4,%ymm4

	vpermq	$144,%ymm3,%ymm3
	vpand	L$high64x3(%rip),%ymm3,%ymm3

	vmovupd	%ymm3,320(%rsp)
	vmovupd	%ymm4,352(%rsp)
	vmovupd	%ymm5,384(%rsp)
	vmovupd	%ymm6,416(%rsp)
	vmovupd	%ymm7,448(%rsp)
	vmovupd	%ymm8,480(%rsp)
	vmovupd	%ymm9,512(%rsp)
	vmovupd	%ymm10,544(%rsp)
	vmovupd	%ymm11,576(%rsp)
	vmovupd	%ymm12,608(%rsp)

	vmovupd	0(%rsp),%ymm3
	vmovupd	32(%rsp),%ymm4
	vmovupd	64(%rsp),%ymm5
	vmovupd	96(%rsp),%ymm6
	vmovupd	128(%rsp),%ymm7
	vmovupd	160(%rsp),%ymm8
	vmovupd	192(%rsp),%ymm9
	vmovupd	224(%rsp),%ymm10
	vmovupd	256(%rsp),%ymm11
	vmovupd	288(%rsp),%ymm12


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12


	vpaddq	320(%rsp),%ymm3,%ymm3
	vpaddq	352(%rsp),%ymm4,%ymm4
	vpaddq	384(%rsp),%ymm5,%ymm5
	vpaddq	416(%rsp),%ymm6,%ymm6
	vpaddq	448(%rsp),%ymm7,%ymm7
	vpaddq	480(%rsp),%ymm8,%ymm8
	vpaddq	512(%rsp),%ymm9,%ymm9
	vpaddq	544(%rsp),%ymm10,%ymm10
	vpaddq	576(%rsp),%ymm11,%ymm11
	vpaddq	608(%rsp),%ymm12,%ymm12

	leaq	640(%rsp),%rsp



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm13
	vmovmskpd	%ymm13,%r14d
	vpcmpgtq	L$mask52x4(%rip),%ymm4,%ymm13
	vmovmskpd	%ymm13,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm13
	vmovmskpd	%ymm13,%r13d
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm13
	vmovmskpd	%ymm13,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm13
	vmovmskpd	%ymm13,%r12d
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm13,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpgtq	L$mask52x4(%rip),%ymm9,%ymm13
	vmovmskpd	%ymm13,%r11d
	vpcmpgtq	L$mask52x4(%rip),%ymm10,%ymm13
	vmovmskpd	%ymm13,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	vpcmpgtq	L$mask52x4(%rip),%ymm11,%ymm13
	vmovmskpd	%ymm13,%r10d
	vpcmpgtq	L$mask52x4(%rip),%ymm12,%ymm13
	vmovmskpd	%ymm13,%r9d
	shlb	$4,%r9b
	orb	%r9b,%r10b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b
	adcb	%r10b,%r10b


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm13
	vmovmskpd	%ymm13,%r9d
	vpcmpeqq	L$mask52x4(%rip),%ymm4,%ymm13
	vmovmskpd	%ymm13,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm13
	vmovmskpd	%ymm13,%r8d
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm13
	vmovmskpd	%ymm13,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm13
	vmovmskpd	%ymm13,%edx
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm13,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpeqq	L$mask52x4(%rip),%ymm9,%ymm13
	vmovmskpd	%ymm13,%ecx
	vpcmpeqq	L$mask52x4(%rip),%ymm10,%ymm13
	vmovmskpd	%ymm13,%ebx
	shlb	$4,%bl
	orb	%bl,%cl

	vpcmpeqq	L$mask52x4(%rip),%ymm11,%ymm13
	vmovmskpd	%ymm13,%ebx
	vpcmpeqq	L$mask52x4(%rip),%ymm12,%ymm13
	vmovmskpd	%ymm13,%eax
	shlb	$4,%al
	orb	%al,%bl

	addb	%r9b,%r14b
	adcb	%r8b,%r13b
	adcb	%dl,%r12b
	adcb	%cl,%r11b
	adcb	%bl,%r10b

	xorb	%r9b,%r14b
	xorb	%r8b,%r13b
	xorb	%dl,%r12b
	xorb	%cl,%r11b
	xorb	%bl,%r10b

	pushq	%r9
	pushq	%r8

	leaq	L$kmasklut(%rip),%r8

	movb	%r14b,%r9b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm13
	shlq	$5,%r14
	vmovapd	(%r8,%r14), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm3,%ymm3

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm4,%ymm4

	movb	%r13b,%r9b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm13
	shlq	$5,%r13
	vmovapd	(%r8,%r13), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm5,%ymm5

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm6,%ymm6

	movb	%r12b,%r9b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm13
	shlq	$5,%r12
	vmovapd	(%r8,%r12), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm7,%ymm7

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm8,%ymm8

	movb	%r11b,%r9b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm13
	shlq	$5,%r11
	vmovapd	(%r8,%r11), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm9,%ymm9

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm10,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm10,%ymm10

	movb	%r10b,%r9b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm11,%ymm13
	shlq	$5,%r10
	vmovapd	(%r8,%r10), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm11,%ymm11

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm12,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm12,%ymm12

	popq	%r8
	popq	%r9

	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9

	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12

	vmovdqu	%ymm3,0(%rdi)
	vmovdqu	%ymm4,32(%rdi)
	vmovdqu	%ymm5,64(%rdi)
	vmovdqu	%ymm6,96(%rdi)
	vmovdqu	%ymm7,128(%rdi)
	vmovdqu	%ymm8,160(%rdi)
	vmovdqu	%ymm9,192(%rdi)
	vmovdqu	%ymm10,224(%rdi)
	vmovdqu	%ymm11,256(%rdi)
	vmovdqu	%ymm12,288(%rdi)

	vzeroupper
	leaq	(%rsp),%rax

	movq	0(%rax),%r15

	movq	8(%rax),%r14

	movq	16(%rax),%r13

	movq	24(%rax),%r12

	movq	32(%rax),%rbp

	movq	40(%rax),%rbx

	leaq	48(%rax),%rsp

L$ossl_rsaz_amm52x40_x1_avxifma256_epilogue:

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

.globl	_ossl_rsaz_amm52x40_x2_avxifma256

.p2align	5
_ossl_rsaz_amm52x40_x2_avxifma256:

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
	vmovapd	%ymm0,%ymm11
	vmovapd	%ymm0,%ymm12

	xorl	%r9d,%r9d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax

	movl	$40,%ebx

.p2align	5
L$loop40:
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

	leaq	-328(%rsp),%rsp

{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52luq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52luq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52luq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52luq 288(%rcx), %ymm2, %ymm12
	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	vmovdqu	%ymm11,256(%rsp)
	vmovdqu	%ymm12,288(%rsp)
	movq	$0,320(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10
	vmovdqu	264(%rsp),%ymm11
	vmovdqu	296(%rsp),%ymm12

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52huq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52huq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52huq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52huq 288(%rcx), %ymm2, %ymm12
	leaq	328(%rsp),%rsp
	leaq	8(%r11),%r11
	decl	%ebx
	jne	L$loop40

	pushq	%r11
	pushq	%rsi
	pushq	%rcx
	pushq	%r8

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3

	leaq	-640(%rsp),%rsp
	vmovupd	%ymm3,0(%rsp)
	vmovupd	%ymm4,32(%rsp)
	vmovupd	%ymm5,64(%rsp)
	vmovupd	%ymm6,96(%rsp)
	vmovupd	%ymm7,128(%rsp)
	vmovupd	%ymm8,160(%rsp)
	vmovupd	%ymm9,192(%rsp)
	vmovupd	%ymm10,224(%rsp)
	vmovupd	%ymm11,256(%rsp)
	vmovupd	%ymm12,288(%rsp)



	vpsrlq	$52,%ymm3,%ymm3
	vpsrlq	$52,%ymm4,%ymm4
	vpsrlq	$52,%ymm5,%ymm5
	vpsrlq	$52,%ymm6,%ymm6
	vpsrlq	$52,%ymm7,%ymm7
	vpsrlq	$52,%ymm8,%ymm8
	vpsrlq	$52,%ymm9,%ymm9
	vpsrlq	$52,%ymm10,%ymm10
	vpsrlq	$52,%ymm11,%ymm11
	vpsrlq	$52,%ymm12,%ymm12


	vpermq	$144,%ymm12,%ymm12
	vpermq	$3,%ymm11,%ymm13
	vblendpd	$1,%ymm13,%ymm12,%ymm12

	vpermq	$144,%ymm11,%ymm11
	vpermq	$3,%ymm10,%ymm13
	vblendpd	$1,%ymm13,%ymm11,%ymm11

	vpermq	$144,%ymm10,%ymm10
	vpermq	$3,%ymm9,%ymm13
	vblendpd	$1,%ymm13,%ymm10,%ymm10

	vpermq	$144,%ymm9,%ymm9
	vpermq	$3,%ymm8,%ymm13
	vblendpd	$1,%ymm13,%ymm9,%ymm9

	vpermq	$144,%ymm8,%ymm8
	vpermq	$3,%ymm7,%ymm13
	vblendpd	$1,%ymm13,%ymm8,%ymm8

	vpermq	$144,%ymm7,%ymm7
	vpermq	$3,%ymm6,%ymm13
	vblendpd	$1,%ymm13,%ymm7,%ymm7

	vpermq	$144,%ymm6,%ymm6
	vpermq	$3,%ymm5,%ymm13
	vblendpd	$1,%ymm13,%ymm6,%ymm6

	vpermq	$144,%ymm5,%ymm5
	vpermq	$3,%ymm4,%ymm13
	vblendpd	$1,%ymm13,%ymm5,%ymm5

	vpermq	$144,%ymm4,%ymm4
	vpermq	$3,%ymm3,%ymm13
	vblendpd	$1,%ymm13,%ymm4,%ymm4

	vpermq	$144,%ymm3,%ymm3
	vpand	L$high64x3(%rip),%ymm3,%ymm3

	vmovupd	%ymm3,320(%rsp)
	vmovupd	%ymm4,352(%rsp)
	vmovupd	%ymm5,384(%rsp)
	vmovupd	%ymm6,416(%rsp)
	vmovupd	%ymm7,448(%rsp)
	vmovupd	%ymm8,480(%rsp)
	vmovupd	%ymm9,512(%rsp)
	vmovupd	%ymm10,544(%rsp)
	vmovupd	%ymm11,576(%rsp)
	vmovupd	%ymm12,608(%rsp)

	vmovupd	0(%rsp),%ymm3
	vmovupd	32(%rsp),%ymm4
	vmovupd	64(%rsp),%ymm5
	vmovupd	96(%rsp),%ymm6
	vmovupd	128(%rsp),%ymm7
	vmovupd	160(%rsp),%ymm8
	vmovupd	192(%rsp),%ymm9
	vmovupd	224(%rsp),%ymm10
	vmovupd	256(%rsp),%ymm11
	vmovupd	288(%rsp),%ymm12


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12


	vpaddq	320(%rsp),%ymm3,%ymm3
	vpaddq	352(%rsp),%ymm4,%ymm4
	vpaddq	384(%rsp),%ymm5,%ymm5
	vpaddq	416(%rsp),%ymm6,%ymm6
	vpaddq	448(%rsp),%ymm7,%ymm7
	vpaddq	480(%rsp),%ymm8,%ymm8
	vpaddq	512(%rsp),%ymm9,%ymm9
	vpaddq	544(%rsp),%ymm10,%ymm10
	vpaddq	576(%rsp),%ymm11,%ymm11
	vpaddq	608(%rsp),%ymm12,%ymm12

	leaq	640(%rsp),%rsp



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm13
	vmovmskpd	%ymm13,%r14d
	vpcmpgtq	L$mask52x4(%rip),%ymm4,%ymm13
	vmovmskpd	%ymm13,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm13
	vmovmskpd	%ymm13,%r13d
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm13
	vmovmskpd	%ymm13,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm13
	vmovmskpd	%ymm13,%r12d
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm13,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpgtq	L$mask52x4(%rip),%ymm9,%ymm13
	vmovmskpd	%ymm13,%r11d
	vpcmpgtq	L$mask52x4(%rip),%ymm10,%ymm13
	vmovmskpd	%ymm13,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	vpcmpgtq	L$mask52x4(%rip),%ymm11,%ymm13
	vmovmskpd	%ymm13,%r10d
	vpcmpgtq	L$mask52x4(%rip),%ymm12,%ymm13
	vmovmskpd	%ymm13,%r9d
	shlb	$4,%r9b
	orb	%r9b,%r10b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b
	adcb	%r10b,%r10b


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm13
	vmovmskpd	%ymm13,%r9d
	vpcmpeqq	L$mask52x4(%rip),%ymm4,%ymm13
	vmovmskpd	%ymm13,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm13
	vmovmskpd	%ymm13,%r8d
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm13
	vmovmskpd	%ymm13,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm13
	vmovmskpd	%ymm13,%edx
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm13,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpeqq	L$mask52x4(%rip),%ymm9,%ymm13
	vmovmskpd	%ymm13,%ecx
	vpcmpeqq	L$mask52x4(%rip),%ymm10,%ymm13
	vmovmskpd	%ymm13,%ebx
	shlb	$4,%bl
	orb	%bl,%cl

	vpcmpeqq	L$mask52x4(%rip),%ymm11,%ymm13
	vmovmskpd	%ymm13,%ebx
	vpcmpeqq	L$mask52x4(%rip),%ymm12,%ymm13
	vmovmskpd	%ymm13,%eax
	shlb	$4,%al
	orb	%al,%bl

	addb	%r9b,%r14b
	adcb	%r8b,%r13b
	adcb	%dl,%r12b
	adcb	%cl,%r11b
	adcb	%bl,%r10b

	xorb	%r9b,%r14b
	xorb	%r8b,%r13b
	xorb	%dl,%r12b
	xorb	%cl,%r11b
	xorb	%bl,%r10b

	pushq	%r9
	pushq	%r8

	leaq	L$kmasklut(%rip),%r8

	movb	%r14b,%r9b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm13
	shlq	$5,%r14
	vmovapd	(%r8,%r14), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm3,%ymm3

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm4,%ymm4

	movb	%r13b,%r9b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm13
	shlq	$5,%r13
	vmovapd	(%r8,%r13), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm5,%ymm5

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm6,%ymm6

	movb	%r12b,%r9b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm13
	shlq	$5,%r12
	vmovapd	(%r8,%r12), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm7,%ymm7

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm8,%ymm8

	movb	%r11b,%r9b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm13
	shlq	$5,%r11
	vmovapd	(%r8,%r11), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm9,%ymm9

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm10,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm10,%ymm10

	movb	%r10b,%r9b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm11,%ymm13
	shlq	$5,%r10
	vmovapd	(%r8,%r10), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm11,%ymm11

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm12,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm12,%ymm12

	popq	%r8
	popq	%r9

	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9

	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12

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
	vmovdqu	%ymm11,256(%rdi)
	vmovdqu	%ymm12,288(%rdi)

	xorl	%r15d,%r15d

	movq	$0xfffffffffffff,%rax

	movl	$40,%ebx

	vpxor	%ymm0,%ymm0,%ymm0
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8
	vmovapd	%ymm0,%ymm9
	vmovapd	%ymm0,%ymm10
	vmovapd	%ymm0,%ymm11
	vmovapd	%ymm0,%ymm12
.p2align	5
L$loop40_1:
	movq	0(%r11),%r13

	vpbroadcastq	0(%r11),%ymm1
	movq	320(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	movq	%r12,%r10
	adcq	$0,%r10

	movq	8(%r8),%r13
	imulq	%r9,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	320(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r9
	adcq	%r12,%r10

	shrq	$52,%r9
	salq	$12,%r10
	orq	%r10,%r9

	leaq	-328(%rsp),%rsp

{vex} vpmadd52luq 320(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 352(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 384(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 416(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 448(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 480(%rsi), %ymm1, %ymm8
{vex} vpmadd52luq 512(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 544(%rsi), %ymm1, %ymm10
{vex} vpmadd52luq 576(%rsi), %ymm1, %ymm11
{vex} vpmadd52luq 608(%rsi), %ymm1, %ymm12

{vex} vpmadd52luq 320(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 352(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 384(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 416(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 448(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 480(%rcx), %ymm2, %ymm8
{vex} vpmadd52luq 512(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 544(%rcx), %ymm2, %ymm10
{vex} vpmadd52luq 576(%rcx), %ymm2, %ymm11
{vex} vpmadd52luq 608(%rcx), %ymm2, %ymm12
	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm4,32(%rsp)
	vmovdqu	%ymm5,64(%rsp)
	vmovdqu	%ymm6,96(%rsp)
	vmovdqu	%ymm7,128(%rsp)
	vmovdqu	%ymm8,160(%rsp)
	vmovdqu	%ymm9,192(%rsp)
	vmovdqu	%ymm10,224(%rsp)
	vmovdqu	%ymm11,256(%rsp)
	vmovdqu	%ymm12,288(%rsp)
	movq	$0,320(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm4
	vmovdqu	72(%rsp),%ymm5
	vmovdqu	104(%rsp),%ymm6
	vmovdqu	136(%rsp),%ymm7
	vmovdqu	168(%rsp),%ymm8
	vmovdqu	200(%rsp),%ymm9
	vmovdqu	232(%rsp),%ymm10
	vmovdqu	264(%rsp),%ymm11
	vmovdqu	296(%rsp),%ymm12

	addq	8(%rsp),%r9

{vex} vpmadd52huq 320(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 352(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 384(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 416(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 448(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 480(%rsi), %ymm1, %ymm8
{vex} vpmadd52huq 512(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 544(%rsi), %ymm1, %ymm10
{vex} vpmadd52huq 576(%rsi), %ymm1, %ymm11
{vex} vpmadd52huq 608(%rsi), %ymm1, %ymm12

{vex} vpmadd52huq 320(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 352(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 384(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 416(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 448(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 480(%rcx), %ymm2, %ymm8
{vex} vpmadd52huq 512(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 544(%rcx), %ymm2, %ymm10
{vex} vpmadd52huq 576(%rcx), %ymm2, %ymm11
{vex} vpmadd52huq 608(%rcx), %ymm2, %ymm12
	leaq	328(%rsp),%rsp
	leaq	8(%r11),%r11
	decl	%ebx
	jne	L$loop40_1

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3

	leaq	-640(%rsp),%rsp
	vmovupd	%ymm3,0(%rsp)
	vmovupd	%ymm4,32(%rsp)
	vmovupd	%ymm5,64(%rsp)
	vmovupd	%ymm6,96(%rsp)
	vmovupd	%ymm7,128(%rsp)
	vmovupd	%ymm8,160(%rsp)
	vmovupd	%ymm9,192(%rsp)
	vmovupd	%ymm10,224(%rsp)
	vmovupd	%ymm11,256(%rsp)
	vmovupd	%ymm12,288(%rsp)



	vpsrlq	$52,%ymm3,%ymm3
	vpsrlq	$52,%ymm4,%ymm4
	vpsrlq	$52,%ymm5,%ymm5
	vpsrlq	$52,%ymm6,%ymm6
	vpsrlq	$52,%ymm7,%ymm7
	vpsrlq	$52,%ymm8,%ymm8
	vpsrlq	$52,%ymm9,%ymm9
	vpsrlq	$52,%ymm10,%ymm10
	vpsrlq	$52,%ymm11,%ymm11
	vpsrlq	$52,%ymm12,%ymm12


	vpermq	$144,%ymm12,%ymm12
	vpermq	$3,%ymm11,%ymm13
	vblendpd	$1,%ymm13,%ymm12,%ymm12

	vpermq	$144,%ymm11,%ymm11
	vpermq	$3,%ymm10,%ymm13
	vblendpd	$1,%ymm13,%ymm11,%ymm11

	vpermq	$144,%ymm10,%ymm10
	vpermq	$3,%ymm9,%ymm13
	vblendpd	$1,%ymm13,%ymm10,%ymm10

	vpermq	$144,%ymm9,%ymm9
	vpermq	$3,%ymm8,%ymm13
	vblendpd	$1,%ymm13,%ymm9,%ymm9

	vpermq	$144,%ymm8,%ymm8
	vpermq	$3,%ymm7,%ymm13
	vblendpd	$1,%ymm13,%ymm8,%ymm8

	vpermq	$144,%ymm7,%ymm7
	vpermq	$3,%ymm6,%ymm13
	vblendpd	$1,%ymm13,%ymm7,%ymm7

	vpermq	$144,%ymm6,%ymm6
	vpermq	$3,%ymm5,%ymm13
	vblendpd	$1,%ymm13,%ymm6,%ymm6

	vpermq	$144,%ymm5,%ymm5
	vpermq	$3,%ymm4,%ymm13
	vblendpd	$1,%ymm13,%ymm5,%ymm5

	vpermq	$144,%ymm4,%ymm4
	vpermq	$3,%ymm3,%ymm13
	vblendpd	$1,%ymm13,%ymm4,%ymm4

	vpermq	$144,%ymm3,%ymm3
	vpand	L$high64x3(%rip),%ymm3,%ymm3

	vmovupd	%ymm3,320(%rsp)
	vmovupd	%ymm4,352(%rsp)
	vmovupd	%ymm5,384(%rsp)
	vmovupd	%ymm6,416(%rsp)
	vmovupd	%ymm7,448(%rsp)
	vmovupd	%ymm8,480(%rsp)
	vmovupd	%ymm9,512(%rsp)
	vmovupd	%ymm10,544(%rsp)
	vmovupd	%ymm11,576(%rsp)
	vmovupd	%ymm12,608(%rsp)

	vmovupd	0(%rsp),%ymm3
	vmovupd	32(%rsp),%ymm4
	vmovupd	64(%rsp),%ymm5
	vmovupd	96(%rsp),%ymm6
	vmovupd	128(%rsp),%ymm7
	vmovupd	160(%rsp),%ymm8
	vmovupd	192(%rsp),%ymm9
	vmovupd	224(%rsp),%ymm10
	vmovupd	256(%rsp),%ymm11
	vmovupd	288(%rsp),%ymm12


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12


	vpaddq	320(%rsp),%ymm3,%ymm3
	vpaddq	352(%rsp),%ymm4,%ymm4
	vpaddq	384(%rsp),%ymm5,%ymm5
	vpaddq	416(%rsp),%ymm6,%ymm6
	vpaddq	448(%rsp),%ymm7,%ymm7
	vpaddq	480(%rsp),%ymm8,%ymm8
	vpaddq	512(%rsp),%ymm9,%ymm9
	vpaddq	544(%rsp),%ymm10,%ymm10
	vpaddq	576(%rsp),%ymm11,%ymm11
	vpaddq	608(%rsp),%ymm12,%ymm12

	leaq	640(%rsp),%rsp



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm13
	vmovmskpd	%ymm13,%r14d
	vpcmpgtq	L$mask52x4(%rip),%ymm4,%ymm13
	vmovmskpd	%ymm13,%r13d
	shlb	$4,%r13b
	orb	%r13b,%r14b

	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm13
	vmovmskpd	%ymm13,%r13d
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm13
	vmovmskpd	%ymm13,%r12d
	shlb	$4,%r12b
	orb	%r12b,%r13b

	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm13
	vmovmskpd	%ymm13,%r12d
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm13,%r11d
	shlb	$4,%r11b
	orb	%r11b,%r12b

	vpcmpgtq	L$mask52x4(%rip),%ymm9,%ymm13
	vmovmskpd	%ymm13,%r11d
	vpcmpgtq	L$mask52x4(%rip),%ymm10,%ymm13
	vmovmskpd	%ymm13,%r10d
	shlb	$4,%r10b
	orb	%r10b,%r11b

	vpcmpgtq	L$mask52x4(%rip),%ymm11,%ymm13
	vmovmskpd	%ymm13,%r10d
	vpcmpgtq	L$mask52x4(%rip),%ymm12,%ymm13
	vmovmskpd	%ymm13,%r9d
	shlb	$4,%r9b
	orb	%r9b,%r10b

	addb	%r14b,%r14b
	adcb	%r13b,%r13b
	adcb	%r12b,%r12b
	adcb	%r11b,%r11b
	adcb	%r10b,%r10b


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm13
	vmovmskpd	%ymm13,%r9d
	vpcmpeqq	L$mask52x4(%rip),%ymm4,%ymm13
	vmovmskpd	%ymm13,%r8d
	shlb	$4,%r8b
	orb	%r8b,%r9b

	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm13
	vmovmskpd	%ymm13,%r8d
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm13
	vmovmskpd	%ymm13,%edx
	shlb	$4,%dl
	orb	%dl,%r8b

	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm13
	vmovmskpd	%ymm13,%edx
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm13
	vmovmskpd	%ymm13,%ecx
	shlb	$4,%cl
	orb	%cl,%dl

	vpcmpeqq	L$mask52x4(%rip),%ymm9,%ymm13
	vmovmskpd	%ymm13,%ecx
	vpcmpeqq	L$mask52x4(%rip),%ymm10,%ymm13
	vmovmskpd	%ymm13,%ebx
	shlb	$4,%bl
	orb	%bl,%cl

	vpcmpeqq	L$mask52x4(%rip),%ymm11,%ymm13
	vmovmskpd	%ymm13,%ebx
	vpcmpeqq	L$mask52x4(%rip),%ymm12,%ymm13
	vmovmskpd	%ymm13,%eax
	shlb	$4,%al
	orb	%al,%bl

	addb	%r9b,%r14b
	adcb	%r8b,%r13b
	adcb	%dl,%r12b
	adcb	%cl,%r11b
	adcb	%bl,%r10b

	xorb	%r9b,%r14b
	xorb	%r8b,%r13b
	xorb	%dl,%r12b
	xorb	%cl,%r11b
	xorb	%bl,%r10b

	pushq	%r9
	pushq	%r8

	leaq	L$kmasklut(%rip),%r8

	movb	%r14b,%r9b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm13
	shlq	$5,%r14
	vmovapd	(%r8,%r14), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm3,%ymm3

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm4,%ymm4

	movb	%r13b,%r9b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm13
	shlq	$5,%r13
	vmovapd	(%r8,%r13), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm5,%ymm5

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm6,%ymm6

	movb	%r12b,%r9b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm13
	shlq	$5,%r12
	vmovapd	(%r8,%r12), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm7,%ymm7

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm8,%ymm8

	movb	%r11b,%r9b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm13
	shlq	$5,%r11
	vmovapd	(%r8,%r11), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm9,%ymm9

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm10,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm10,%ymm10

	movb	%r10b,%r9b
	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm11,%ymm13
	shlq	$5,%r10
	vmovapd	(%r8,%r10), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm11,%ymm11

	shrb	$4,%r9b
	andq	$0xf,%r9
	vpsubq	L$mask52x4(%rip),%ymm12,%ymm13
	shlq	$5,%r9
	vmovapd	(%r8,%r9), %ymm14
	vblendvpd	%ymm14,%ymm13,%ymm12,%ymm12

	popq	%r8
	popq	%r9

	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8
	vpand	L$mask52x4(%rip),%ymm9,%ymm9

	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12

	vmovdqu	%ymm3,320(%rdi)
	vmovdqu	%ymm4,352(%rdi)
	vmovdqu	%ymm5,384(%rdi)
	vmovdqu	%ymm6,416(%rdi)
	vmovdqu	%ymm7,448(%rdi)
	vmovdqu	%ymm8,480(%rdi)
	vmovdqu	%ymm9,512(%rdi)
	vmovdqu	%ymm10,544(%rdi)
	vmovdqu	%ymm11,576(%rdi)
	vmovdqu	%ymm12,608(%rdi)

	vzeroupper
	leaq	(%rsp),%rax

	movq	0(%rax),%r15

	movq	8(%rax),%r14

	movq	16(%rax),%r13

	movq	24(%rax),%r12

	movq	32(%rax),%rbp

	movq	40(%rax),%rbx

	leaq	48(%rax),%rsp

L$ossl_rsaz_amm52x40_x2_avxifma256_epilogue:
	.byte	0xf3,0xc3


.text	

.p2align	5
.globl	_ossl_extract_multiplier_2x40_win5_avx

_ossl_extract_multiplier_2x40_win5_avx:

.byte	243,15,30,250
	vmovapd	L$ones(%rip),%ymm14
	vmovq	%rdx,%xmm10
	vpbroadcastq	%xmm10,%ymm12
	vmovq	%rcx,%xmm10
	vpbroadcastq	%xmm10,%ymm13
	leaq	20480(%rsi),%rax


	movq	%rsi,%r10


	vpxor	%xmm0,%xmm0,%xmm0
	vmovapd	%ymm0,%ymm1
	vmovapd	%ymm0,%ymm2
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8
	vmovapd	%ymm0,%ymm9
	vpxor	%ymm11,%ymm11,%ymm11
.p2align	5
L$loop_0:
	vpcmpeqq	%ymm11,%ymm12,%ymm15
	vmovdqu	0(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm0,%ymm0
	vmovdqu	32(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm1,%ymm1
	vmovdqu	64(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm2,%ymm2
	vmovdqu	96(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm3,%ymm3
	vmovdqu	128(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm4,%ymm4
	vmovdqu	160(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm5,%ymm5
	vmovdqu	192(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm6,%ymm6
	vmovdqu	224(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm7,%ymm7
	vmovdqu	256(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm8,%ymm8
	vmovdqu	288(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm9,%ymm9
	vpaddq	%ymm14,%ymm11,%ymm11
	addq	$640,%rsi
	cmpq	%rsi,%rax
	jne	L$loop_0
	vmovdqu	%ymm0,0(%rdi)
	vmovdqu	%ymm1,32(%rdi)
	vmovdqu	%ymm2,64(%rdi)
	vmovdqu	%ymm3,96(%rdi)
	vmovdqu	%ymm4,128(%rdi)
	vmovdqu	%ymm5,160(%rdi)
	vmovdqu	%ymm6,192(%rdi)
	vmovdqu	%ymm7,224(%rdi)
	vmovdqu	%ymm8,256(%rdi)
	vmovdqu	%ymm9,288(%rdi)
	movq	%r10,%rsi
	vpxor	%ymm11,%ymm11,%ymm11
.p2align	5
L$loop_320:
	vpcmpeqq	%ymm11,%ymm13,%ymm15
	vmovdqu	320(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm0,%ymm0
	vmovdqu	352(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm1,%ymm1
	vmovdqu	384(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm2,%ymm2
	vmovdqu	416(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm3,%ymm3
	vmovdqu	448(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm4,%ymm4
	vmovdqu	480(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm5,%ymm5
	vmovdqu	512(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm6,%ymm6
	vmovdqu	544(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm7,%ymm7
	vmovdqu	576(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm8,%ymm8
	vmovdqu	608(%rsi),%ymm10

	vblendvpd	%ymm15,%ymm10,%ymm9,%ymm9
	vpaddq	%ymm14,%ymm11,%ymm11
	addq	$640,%rsi
	cmpq	%rsi,%rax
	jne	L$loop_320
	vmovdqu	%ymm0,320(%rdi)
	vmovdqu	%ymm1,352(%rdi)
	vmovdqu	%ymm2,384(%rdi)
	vmovdqu	%ymm3,416(%rdi)
	vmovdqu	%ymm4,448(%rdi)
	vmovdqu	%ymm5,480(%rdi)
	vmovdqu	%ymm6,512(%rdi)
	vmovdqu	%ymm7,544(%rdi)
	vmovdqu	%ymm8,576(%rdi)
	vmovdqu	%ymm9,608(%rdi)

	.byte	0xf3,0xc3


.section	__DATA,__const
.p2align	5
L$ones:
.quad	1,1,1,1
L$zeros:
.quad	0,0,0,0
