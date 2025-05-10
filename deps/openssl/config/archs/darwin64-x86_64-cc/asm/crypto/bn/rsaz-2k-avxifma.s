.text	

.globl	_ossl_rsaz_avxifma_eligible

.p2align	5
_ossl_rsaz_avxifma_eligible:
	movl	_OPENSSL_ia32cap_P+20(%rip),%ecx
	xorl	%eax,%eax
	andl	$8388608,%ecx
	cmpl	$8388608,%ecx
	cmovel	%ecx,%eax
	.byte	0xf3,0xc3

.text	

.globl	_ossl_rsaz_amm52x20_x1_avxifma256

.p2align	5
_ossl_rsaz_amm52x20_x1_avxifma256:

.byte	243,15,30,250
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$ossl_rsaz_amm52x20_x1_avxifma256_body:


	vpxor	%ymm0,%ymm0,%ymm0
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8

	xorl	%r9d,%r9d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax


	movl	$5,%ebx

.p2align	5
L$loop5:
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

	leaq	-168(%rsp),%rsp
{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm8


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm5,32(%rsp)
	vmovdqu	%ymm6,64(%rsp)
	vmovdqu	%ymm7,96(%rsp)
	vmovdqu	%ymm8,128(%rsp)
	movq	$0,160(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm5
	vmovdqu	72(%rsp),%ymm6
	vmovdqu	104(%rsp),%ymm7
	vmovdqu	136(%rsp),%ymm8

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm8
	leaq	168(%rsp),%rsp
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

	leaq	-168(%rsp),%rsp
{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm8


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm5,32(%rsp)
	vmovdqu	%ymm6,64(%rsp)
	vmovdqu	%ymm7,96(%rsp)
	vmovdqu	%ymm8,128(%rsp)
	movq	$0,160(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm5
	vmovdqu	72(%rsp),%ymm6
	vmovdqu	104(%rsp),%ymm7
	vmovdqu	136(%rsp),%ymm8

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm8
	leaq	168(%rsp),%rsp
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

	leaq	-168(%rsp),%rsp
{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm8


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm5,32(%rsp)
	vmovdqu	%ymm6,64(%rsp)
	vmovdqu	%ymm7,96(%rsp)
	vmovdqu	%ymm8,128(%rsp)
	movq	$0,160(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm5
	vmovdqu	72(%rsp),%ymm6
	vmovdqu	104(%rsp),%ymm7
	vmovdqu	136(%rsp),%ymm8

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm8
	leaq	168(%rsp),%rsp
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

	leaq	-168(%rsp),%rsp
{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm8


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm5,32(%rsp)
	vmovdqu	%ymm6,64(%rsp)
	vmovdqu	%ymm7,96(%rsp)
	vmovdqu	%ymm8,128(%rsp)
	movq	$0,160(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm5
	vmovdqu	72(%rsp),%ymm6
	vmovdqu	104(%rsp),%ymm7
	vmovdqu	136(%rsp),%ymm8

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm8
	leaq	168(%rsp),%rsp
	leaq	32(%r11),%r11
	decl	%ebx
	jne	L$loop5

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm5,%ymm1
	vpsrlq	$52,%ymm6,%ymm2
	vpsrlq	$52,%ymm7,%ymm13
	vpsrlq	$52,%ymm8,%ymm14


	vpermq	$144,%ymm14,%ymm14
	vpermq	$3,%ymm13,%ymm15
	vblendpd	$1,%ymm15,%ymm14,%ymm14

	vpermq	$144,%ymm13,%ymm13
	vpermq	$3,%ymm2,%ymm15
	vblendpd	$1,%ymm15,%ymm13,%ymm13

	vpermq	$144,%ymm2,%ymm2
	vpermq	$3,%ymm1,%ymm15
	vblendpd	$1,%ymm15,%ymm2,%ymm2

	vpermq	$144,%ymm1,%ymm1
	vpermq	$3,%ymm0,%ymm15
	vblendpd	$1,%ymm15,%ymm1,%ymm1

	vpermq	$144,%ymm0,%ymm0
	vpand	L$high64x3(%rip),%ymm0,%ymm0


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm5,%ymm5
	vpaddq	%ymm2,%ymm6,%ymm6
	vpaddq	%ymm13,%ymm7,%ymm7
	vpaddq	%ymm14,%ymm8,%ymm8



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm1
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm2
	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm13
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm14
	vmovmskpd	%ymm0,%r14d
	vmovmskpd	%ymm1,%r13d
	vmovmskpd	%ymm2,%r12d
	vmovmskpd	%ymm13,%r11d
	vmovmskpd	%ymm14,%r10d


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm1
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm2
	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm13
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm14
	vmovmskpd	%ymm0,%r9d
	vmovmskpd	%ymm1,%r8d
	vmovmskpd	%ymm2,%ebx
	vmovmskpd	%ymm13,%ecx
	vmovmskpd	%ymm14,%edx



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

	leaq	L$kmasklut(%rip),%rdx

	movb	%r14b,%r13b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm0
	shlq	$5,%r14
	vmovapd	(%rdx,%r14), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm3,%ymm3

	shrb	$4,%r13b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm0
	shlq	$5,%r13
	vmovapd	(%rdx,%r13), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm5,%ymm5

	movb	%r12b,%r11b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm0
	shlq	$5,%r12
	vmovapd	(%rdx,%r12), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm6,%ymm6

	shrb	$4,%r11b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm0
	shlq	$5,%r11
	vmovapd	(%rdx,%r11), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm7,%ymm7

	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm8,%ymm8


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8

	vmovdqu	%ymm3,0(%rdi)
	vmovdqu	%ymm5,32(%rdi)
	vmovdqu	%ymm6,64(%rdi)
	vmovdqu	%ymm7,96(%rdi)
	vmovdqu	%ymm8,128(%rdi)

	vzeroupper
	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbp

	movq	40(%rsp),%rbx

	leaq	48(%rsp),%rsp

L$ossl_rsaz_amm52x20_x1_avxifma256_epilogue:
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

.globl	_ossl_rsaz_amm52x20_x2_avxifma256

.p2align	5
_ossl_rsaz_amm52x20_x2_avxifma256:

.byte	243,15,30,250
	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$ossl_rsaz_amm52x20_x2_avxifma256_body:


	vpxor	%ymm0,%ymm0,%ymm0
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm9
	vmovapd	%ymm0,%ymm10
	vmovapd	%ymm0,%ymm11
	vmovapd	%ymm0,%ymm12

	xorl	%r9d,%r9d
	xorl	%r15d,%r15d

	movq	%rdx,%r11
	movq	$0xfffffffffffff,%rax

	movl	$20,%ebx

.p2align	5
L$loop20:
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

	leaq	-168(%rsp),%rsp
{vex} vpmadd52luq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52luq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52luq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52luq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52luq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52luq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52luq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52luq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52luq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52luq 128(%rcx), %ymm2, %ymm8


	vmovdqu	%ymm3,0(%rsp)
	vmovdqu	%ymm5,32(%rsp)
	vmovdqu	%ymm6,64(%rsp)
	vmovdqu	%ymm7,96(%rsp)
	vmovdqu	%ymm8,128(%rsp)
	movq	$0,160(%rsp)

	vmovdqu	8(%rsp),%ymm3
	vmovdqu	40(%rsp),%ymm5
	vmovdqu	72(%rsp),%ymm6
	vmovdqu	104(%rsp),%ymm7
	vmovdqu	136(%rsp),%ymm8

	addq	8(%rsp),%r9

{vex} vpmadd52huq 0(%rsi), %ymm1, %ymm3
{vex} vpmadd52huq 32(%rsi), %ymm1, %ymm5
{vex} vpmadd52huq 64(%rsi), %ymm1, %ymm6
{vex} vpmadd52huq 96(%rsi), %ymm1, %ymm7
{vex} vpmadd52huq 128(%rsi), %ymm1, %ymm8

{vex} vpmadd52huq 0(%rcx), %ymm2, %ymm3
{vex} vpmadd52huq 32(%rcx), %ymm2, %ymm5
{vex} vpmadd52huq 64(%rcx), %ymm2, %ymm6
{vex} vpmadd52huq 96(%rcx), %ymm2, %ymm7
{vex} vpmadd52huq 128(%rcx), %ymm2, %ymm8
	leaq	168(%rsp),%rsp
	movq	160(%r11),%r13

	vpbroadcastq	160(%r11),%ymm1
	movq	160(%rsi),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	movq	%r12,%r10
	adcq	$0,%r10

	movq	8(%r8),%r13
	imulq	%r15,%r13
	andq	%rax,%r13

	vmovq	%r13,%xmm2
	vpbroadcastq	%xmm2,%ymm2
	movq	160(%rcx),%rdx
	mulxq	%r13,%r13,%r12
	addq	%r13,%r15
	adcq	%r12,%r10

	shrq	$52,%r15
	salq	$12,%r10
	orq	%r10,%r15

	leaq	-168(%rsp),%rsp
{vex} vpmadd52luq 160(%rsi), %ymm1, %ymm4
{vex} vpmadd52luq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52luq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52luq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52luq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52luq 160(%rcx), %ymm2, %ymm4
{vex} vpmadd52luq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52luq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52luq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52luq 288(%rcx), %ymm2, %ymm12


	vmovdqu	%ymm4,0(%rsp)
	vmovdqu	%ymm9,32(%rsp)
	vmovdqu	%ymm10,64(%rsp)
	vmovdqu	%ymm11,96(%rsp)
	vmovdqu	%ymm12,128(%rsp)
	movq	$0,160(%rsp)

	vmovdqu	8(%rsp),%ymm4
	vmovdqu	40(%rsp),%ymm9
	vmovdqu	72(%rsp),%ymm10
	vmovdqu	104(%rsp),%ymm11
	vmovdqu	136(%rsp),%ymm12

	addq	8(%rsp),%r15

{vex} vpmadd52huq 160(%rsi), %ymm1, %ymm4
{vex} vpmadd52huq 192(%rsi), %ymm1, %ymm9
{vex} vpmadd52huq 224(%rsi), %ymm1, %ymm10
{vex} vpmadd52huq 256(%rsi), %ymm1, %ymm11
{vex} vpmadd52huq 288(%rsi), %ymm1, %ymm12

{vex} vpmadd52huq 160(%rcx), %ymm2, %ymm4
{vex} vpmadd52huq 192(%rcx), %ymm2, %ymm9
{vex} vpmadd52huq 224(%rcx), %ymm2, %ymm10
{vex} vpmadd52huq 256(%rcx), %ymm2, %ymm11
{vex} vpmadd52huq 288(%rcx), %ymm2, %ymm12
	leaq	168(%rsp),%rsp
	leaq	8(%r11),%r11
	decl	%ebx
	jne	L$loop20

	vmovq	%r9,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm3,%ymm3



	vpsrlq	$52,%ymm3,%ymm0
	vpsrlq	$52,%ymm5,%ymm1
	vpsrlq	$52,%ymm6,%ymm2
	vpsrlq	$52,%ymm7,%ymm13
	vpsrlq	$52,%ymm8,%ymm14


	vpermq	$144,%ymm14,%ymm14
	vpermq	$3,%ymm13,%ymm15
	vblendpd	$1,%ymm15,%ymm14,%ymm14

	vpermq	$144,%ymm13,%ymm13
	vpermq	$3,%ymm2,%ymm15
	vblendpd	$1,%ymm15,%ymm13,%ymm13

	vpermq	$144,%ymm2,%ymm2
	vpermq	$3,%ymm1,%ymm15
	vblendpd	$1,%ymm15,%ymm2,%ymm2

	vpermq	$144,%ymm1,%ymm1
	vpermq	$3,%ymm0,%ymm15
	vblendpd	$1,%ymm15,%ymm1,%ymm1

	vpermq	$144,%ymm0,%ymm0
	vpand	L$high64x3(%rip),%ymm0,%ymm0


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8


	vpaddq	%ymm0,%ymm3,%ymm3
	vpaddq	%ymm1,%ymm5,%ymm5
	vpaddq	%ymm2,%ymm6,%ymm6
	vpaddq	%ymm13,%ymm7,%ymm7
	vpaddq	%ymm14,%ymm8,%ymm8



	vpcmpgtq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpgtq	L$mask52x4(%rip),%ymm5,%ymm1
	vpcmpgtq	L$mask52x4(%rip),%ymm6,%ymm2
	vpcmpgtq	L$mask52x4(%rip),%ymm7,%ymm13
	vpcmpgtq	L$mask52x4(%rip),%ymm8,%ymm14
	vmovmskpd	%ymm0,%r14d
	vmovmskpd	%ymm1,%r13d
	vmovmskpd	%ymm2,%r12d
	vmovmskpd	%ymm13,%r11d
	vmovmskpd	%ymm14,%r10d


	vpcmpeqq	L$mask52x4(%rip),%ymm3,%ymm0
	vpcmpeqq	L$mask52x4(%rip),%ymm5,%ymm1
	vpcmpeqq	L$mask52x4(%rip),%ymm6,%ymm2
	vpcmpeqq	L$mask52x4(%rip),%ymm7,%ymm13
	vpcmpeqq	L$mask52x4(%rip),%ymm8,%ymm14
	vmovmskpd	%ymm0,%r9d
	vmovmskpd	%ymm1,%r8d
	vmovmskpd	%ymm2,%ebx
	vmovmskpd	%ymm13,%ecx
	vmovmskpd	%ymm14,%edx



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

	leaq	L$kmasklut(%rip),%rdx

	movb	%r14b,%r13b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm3,%ymm0
	shlq	$5,%r14
	vmovapd	(%rdx,%r14), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm3,%ymm3

	shrb	$4,%r13b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm5,%ymm0
	shlq	$5,%r13
	vmovapd	(%rdx,%r13), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm5,%ymm5

	movb	%r12b,%r11b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm6,%ymm0
	shlq	$5,%r12
	vmovapd	(%rdx,%r12), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm6,%ymm6

	shrb	$4,%r11b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm7,%ymm0
	shlq	$5,%r11
	vmovapd	(%rdx,%r11), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm7,%ymm7

	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm8,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm8,%ymm8


	vpand	L$mask52x4(%rip),%ymm3,%ymm3
	vpand	L$mask52x4(%rip),%ymm5,%ymm5
	vpand	L$mask52x4(%rip),%ymm6,%ymm6
	vpand	L$mask52x4(%rip),%ymm7,%ymm7
	vpand	L$mask52x4(%rip),%ymm8,%ymm8

	vmovq	%r15,%xmm0
	vpbroadcastq	%xmm0,%ymm0
	vpblendd	$3,%ymm0,%ymm4,%ymm4



	vpsrlq	$52,%ymm4,%ymm0
	vpsrlq	$52,%ymm9,%ymm1
	vpsrlq	$52,%ymm10,%ymm2
	vpsrlq	$52,%ymm11,%ymm13
	vpsrlq	$52,%ymm12,%ymm14


	vpermq	$144,%ymm14,%ymm14
	vpermq	$3,%ymm13,%ymm15
	vblendpd	$1,%ymm15,%ymm14,%ymm14

	vpermq	$144,%ymm13,%ymm13
	vpermq	$3,%ymm2,%ymm15
	vblendpd	$1,%ymm15,%ymm13,%ymm13

	vpermq	$144,%ymm2,%ymm2
	vpermq	$3,%ymm1,%ymm15
	vblendpd	$1,%ymm15,%ymm2,%ymm2

	vpermq	$144,%ymm1,%ymm1
	vpermq	$3,%ymm0,%ymm15
	vblendpd	$1,%ymm15,%ymm1,%ymm1

	vpermq	$144,%ymm0,%ymm0
	vpand	L$high64x3(%rip),%ymm0,%ymm0


	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12


	vpaddq	%ymm0,%ymm4,%ymm4
	vpaddq	%ymm1,%ymm9,%ymm9
	vpaddq	%ymm2,%ymm10,%ymm10
	vpaddq	%ymm13,%ymm11,%ymm11
	vpaddq	%ymm14,%ymm12,%ymm12



	vpcmpgtq	L$mask52x4(%rip),%ymm4,%ymm0
	vpcmpgtq	L$mask52x4(%rip),%ymm9,%ymm1
	vpcmpgtq	L$mask52x4(%rip),%ymm10,%ymm2
	vpcmpgtq	L$mask52x4(%rip),%ymm11,%ymm13
	vpcmpgtq	L$mask52x4(%rip),%ymm12,%ymm14
	vmovmskpd	%ymm0,%r14d
	vmovmskpd	%ymm1,%r13d
	vmovmskpd	%ymm2,%r12d
	vmovmskpd	%ymm13,%r11d
	vmovmskpd	%ymm14,%r10d


	vpcmpeqq	L$mask52x4(%rip),%ymm4,%ymm0
	vpcmpeqq	L$mask52x4(%rip),%ymm9,%ymm1
	vpcmpeqq	L$mask52x4(%rip),%ymm10,%ymm2
	vpcmpeqq	L$mask52x4(%rip),%ymm11,%ymm13
	vpcmpeqq	L$mask52x4(%rip),%ymm12,%ymm14
	vmovmskpd	%ymm0,%r9d
	vmovmskpd	%ymm1,%r8d
	vmovmskpd	%ymm2,%ebx
	vmovmskpd	%ymm13,%ecx
	vmovmskpd	%ymm14,%edx



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

	leaq	L$kmasklut(%rip),%rdx

	movb	%r14b,%r13b
	andq	$0xf,%r14
	vpsubq	L$mask52x4(%rip),%ymm4,%ymm0
	shlq	$5,%r14
	vmovapd	(%rdx,%r14), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm4,%ymm4

	shrb	$4,%r13b
	andq	$0xf,%r13
	vpsubq	L$mask52x4(%rip),%ymm9,%ymm0
	shlq	$5,%r13
	vmovapd	(%rdx,%r13), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm9,%ymm9

	movb	%r12b,%r11b
	andq	$0xf,%r12
	vpsubq	L$mask52x4(%rip),%ymm10,%ymm0
	shlq	$5,%r12
	vmovapd	(%rdx,%r12), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm10,%ymm10

	shrb	$4,%r11b
	andq	$0xf,%r11
	vpsubq	L$mask52x4(%rip),%ymm11,%ymm0
	shlq	$5,%r11
	vmovapd	(%rdx,%r11), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm11,%ymm11

	andq	$0xf,%r10
	vpsubq	L$mask52x4(%rip),%ymm12,%ymm0
	shlq	$5,%r10
	vmovapd	(%rdx,%r10), %ymm2
	vblendvpd	%ymm2,%ymm0,%ymm12,%ymm12


	vpand	L$mask52x4(%rip),%ymm4,%ymm4
	vpand	L$mask52x4(%rip),%ymm9,%ymm9
	vpand	L$mask52x4(%rip),%ymm10,%ymm10
	vpand	L$mask52x4(%rip),%ymm11,%ymm11
	vpand	L$mask52x4(%rip),%ymm12,%ymm12

	vmovdqu	%ymm3,0(%rdi)
	vmovdqu	%ymm5,32(%rdi)
	vmovdqu	%ymm6,64(%rdi)
	vmovdqu	%ymm7,96(%rdi)
	vmovdqu	%ymm8,128(%rdi)

	vmovdqu	%ymm4,160(%rdi)
	vmovdqu	%ymm9,192(%rdi)
	vmovdqu	%ymm10,224(%rdi)
	vmovdqu	%ymm11,256(%rdi)
	vmovdqu	%ymm12,288(%rdi)

	vzeroupper
	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbp

	movq	40(%rsp),%rbx

	leaq	48(%rsp),%rsp

L$ossl_rsaz_amm52x20_x2_avxifma256_epilogue:
	.byte	0xf3,0xc3


.text	

.p2align	5
.globl	_ossl_extract_multiplier_2x20_win5_avx

_ossl_extract_multiplier_2x20_win5_avx:

.byte	243,15,30,250
	vmovapd	L$ones(%rip),%ymm14
	vmovq	%rdx,%xmm10
	vpbroadcastq	%xmm10,%ymm12
	vmovq	%rcx,%xmm10
	vpbroadcastq	%xmm10,%ymm13
	leaq	10240(%rsi),%rax


	vpxor	%xmm0,%xmm0,%xmm0
	vmovapd	%ymm0,%ymm11
	vmovapd	%ymm0,%ymm1
	vmovapd	%ymm0,%ymm2
	vmovapd	%ymm0,%ymm3
	vmovapd	%ymm0,%ymm4
	vmovapd	%ymm0,%ymm5
	vmovapd	%ymm0,%ymm6
	vmovapd	%ymm0,%ymm7
	vmovapd	%ymm0,%ymm8
	vmovapd	%ymm0,%ymm9

.p2align	5
L$loop:
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
	vpcmpeqq	%ymm11,%ymm13,%ymm15
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
	addq	$320,%rsi
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
	vmovdqu	%ymm8,256(%rdi)
	vmovdqu	%ymm9,288(%rdi)
	.byte	0xf3,0xc3


.section	__DATA,__const
.p2align	5
L$ones:
.quad	1,1,1,1
L$zeros:
.quad	0,0,0,0
