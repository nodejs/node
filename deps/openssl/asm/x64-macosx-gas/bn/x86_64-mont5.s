.text


.globl	_bn_mul_mont_gather5

.p2align	6
_bn_mul_mont_gather5:
	testl	$3,%r9d
	jnz	L$mul_enter
	cmpl	$8,%r9d
	jb	L$mul_enter
	jmp	L$mul4x_enter

.p2align	4
L$mul_enter:
	movl	%r9d,%r9d
	movd	8(%rsp),%xmm5
	leaq	L$inc(%rip),%r10
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

L$mul_alloca:
	movq	%rsp,%rax
	leaq	2(%r9),%r11
	negq	%r11
	leaq	-264(%rsp,%r11,8),%rsp
	andq	$-1024,%rsp

	movq	%rax,8(%rsp,%r9,8)
L$mul_body:






	subq	%rsp,%rax
	andq	$-4096,%rax
L$mul_page_walk:
	movq	(%rsp,%rax,1),%r11
	subq	$4096,%rax
.byte	0x2e

	jnc	L$mul_page_walk

	leaq	128(%rdx),%r12
	movdqa	0(%r10),%xmm0
	movdqa	16(%r10),%xmm1
	leaq	24-112(%rsp,%r9,8),%r10
	andq	$-16,%r10

	pshufd	$0,%xmm5,%xmm5
	movdqa	%xmm1,%xmm4
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
.byte	0x67
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,112(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,128(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,144(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,160(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,176(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,192(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,208(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,224(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,240(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,256(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,272(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,288(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,304(%r10)

	paddd	%xmm2,%xmm3
.byte	0x67
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,320(%r10)

	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,336(%r10)
	pand	64(%r12),%xmm0

	pand	80(%r12),%xmm1
	pand	96(%r12),%xmm2
	movdqa	%xmm3,352(%r10)
	pand	112(%r12),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-128(%r12),%xmm4
	movdqa	-112(%r12),%xmm5
	movdqa	-96(%r12),%xmm2
	pand	112(%r10),%xmm4
	movdqa	-80(%r12),%xmm3
	pand	128(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	144(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	160(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-64(%r12),%xmm4
	movdqa	-48(%r12),%xmm5
	movdqa	-32(%r12),%xmm2
	pand	176(%r10),%xmm4
	movdqa	-16(%r12),%xmm3
	pand	192(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	208(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	224(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	0(%r12),%xmm4
	movdqa	16(%r12),%xmm5
	movdqa	32(%r12),%xmm2
	pand	240(%r10),%xmm4
	movdqa	48(%r12),%xmm3
	pand	256(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	272(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	288(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	por	%xmm1,%xmm0
	pshufd	$78,%xmm0,%xmm1
	por	%xmm1,%xmm0
	leaq	256(%r12),%r12
.byte	102,72,15,126,195

	movq	(%r8),%r8
	movq	(%rsi),%rax

	xorq	%r14,%r14
	xorq	%r15,%r15

	movq	%r8,%rbp
	mulq	%rbx
	movq	%rax,%r10
	movq	(%rcx),%rax

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%r13

	leaq	1(%r15),%r15
	jmp	L$1st_enter

.p2align	4
L$1st:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%r13
	movq	%r10,%r11
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

L$1st_enter:
	mulq	%rbx
	addq	%rax,%r11
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	leaq	1(%r15),%r15
	movq	%rdx,%r10

	mulq	%rbp
	cmpq	%r9,%r15
	jne	L$1st

	addq	%rax,%r13
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13
	movq	%r10,%r11

	xorq	%rdx,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r9,8)
	movq	%rdx,(%rsp,%r9,8)

	leaq	1(%r14),%r14
	jmp	L$outer
.p2align	4
L$outer:
	leaq	24+128(%rsp,%r9,8),%rdx
	andq	$-16,%rdx
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r12),%xmm0
	movdqa	-112(%r12),%xmm1
	movdqa	-96(%r12),%xmm2
	movdqa	-80(%r12),%xmm3
	pand	-128(%rdx),%xmm0
	pand	-112(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r12),%xmm0
	movdqa	-48(%r12),%xmm1
	movdqa	-32(%r12),%xmm2
	movdqa	-16(%r12),%xmm3
	pand	-64(%rdx),%xmm0
	pand	-48(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r12),%xmm0
	movdqa	16(%r12),%xmm1
	movdqa	32(%r12),%xmm2
	movdqa	48(%r12),%xmm3
	pand	0(%rdx),%xmm0
	pand	16(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r12),%xmm0
	movdqa	80(%r12),%xmm1
	movdqa	96(%r12),%xmm2
	movdqa	112(%r12),%xmm3
	pand	64(%rdx),%xmm0
	pand	80(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
	pshufd	$78,%xmm4,%xmm0
	por	%xmm4,%xmm0
	leaq	256(%r12),%r12
.byte	102,72,15,126,195

	xorq	%r15,%r15
	movq	%r8,%rbp
	movq	(%rsp),%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx),%rax
	adcq	$0,%rdx

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	8(%rsp),%r10
	movq	%rdx,%r13

	leaq	1(%r15),%r15
	jmp	L$inner_enter

.p2align	4
L$inner:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	movq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

L$inner_enter:
	mulq	%rbx
	addq	%rax,%r11
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11
	leaq	1(%r15),%r15

	mulq	%rbp
	cmpq	%r9,%r15
	jne	L$inner

	addq	%rax,%r13
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	movq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

	xorq	%rdx,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r9,8)
	movq	%rdx,(%rsp,%r9,8)

	leaq	1(%r14),%r14
	cmpq	%r9,%r14
	jl	L$outer

	xorq	%r14,%r14
	movq	(%rsp),%rax
	leaq	(%rsp),%rsi
	movq	%r9,%r15
	jmp	L$sub
.p2align	4
L$sub:	sbbq	(%rcx,%r14,8),%rax
	movq	%rax,(%rdi,%r14,8)
	movq	8(%rsi,%r14,8),%rax
	leaq	1(%r14),%r14
	decq	%r15
	jnz	L$sub

	sbbq	$0,%rax
	xorq	%r14,%r14
	andq	%rax,%rsi
	notq	%rax
	movq	%rdi,%rcx
	andq	%rax,%rcx
	movq	%r9,%r15
	orq	%rcx,%rsi
.p2align	4
L$copy:
	movq	(%rsi,%r14,8),%rax
	movq	%r14,(%rsp,%r14,8)
	movq	%rax,(%rdi,%r14,8)
	leaq	1(%r14),%r14
	subq	$1,%r15
	jnz	L$copy

	movq	8(%rsp,%r9,8),%rsi
	movq	$1,%rax

	movq	(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbp
	movq	40(%rsi),%rbx
	leaq	48(%rsi),%rsp
L$mul_epilogue:
	.byte	0xf3,0xc3


.p2align	4
bn_mul4x_mont_gather5:
L$mul4x_enter:
	movl	%r9d,%r9d
	movd	8(%rsp),%xmm5
	leaq	L$inc(%rip),%r10
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

L$mul4x_alloca:
	movq	%rsp,%rax
	leaq	4(%r9),%r11
	negq	%r11
	leaq	-256(%rsp,%r11,8),%rsp
	andq	$-1024,%rsp

	movq	%rax,8(%rsp,%r9,8)
L$mul4x_body:
	subq	%rsp,%rax
	andq	$-4096,%rax
L$mul4x_page_walk:
	movq	(%rsp,%rax,1),%r11
	subq	$4096,%rax
.byte	0x2e

	jnc	L$mul4x_page_walk

	movq	%rdi,16(%rsp,%r9,8)
	leaq	128(%rdx),%r12
	movdqa	0(%r10),%xmm0
	movdqa	16(%r10),%xmm1
	leaq	32-112(%rsp,%r9,8),%r10

	pshufd	$0,%xmm5,%xmm5
	movdqa	%xmm1,%xmm4
.byte	0x67,0x67
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
.byte	0x67
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,112(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,128(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,144(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,160(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,176(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,192(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,208(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,224(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,240(%r10)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,256(%r10)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,272(%r10)
	movdqa	%xmm4,%xmm2

	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,288(%r10)
	movdqa	%xmm4,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,304(%r10)

	paddd	%xmm2,%xmm3
.byte	0x67
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,320(%r10)

	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,336(%r10)
	pand	64(%r12),%xmm0

	pand	80(%r12),%xmm1
	pand	96(%r12),%xmm2
	movdqa	%xmm3,352(%r10)
	pand	112(%r12),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-128(%r12),%xmm4
	movdqa	-112(%r12),%xmm5
	movdqa	-96(%r12),%xmm2
	pand	112(%r10),%xmm4
	movdqa	-80(%r12),%xmm3
	pand	128(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	144(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	160(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	-64(%r12),%xmm4
	movdqa	-48(%r12),%xmm5
	movdqa	-32(%r12),%xmm2
	pand	176(%r10),%xmm4
	movdqa	-16(%r12),%xmm3
	pand	192(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	208(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	224(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	movdqa	0(%r12),%xmm4
	movdqa	16(%r12),%xmm5
	movdqa	32(%r12),%xmm2
	pand	240(%r10),%xmm4
	movdqa	48(%r12),%xmm3
	pand	256(%r10),%xmm5
	por	%xmm4,%xmm0
	pand	272(%r10),%xmm2
	por	%xmm5,%xmm1
	pand	288(%r10),%xmm3
	por	%xmm2,%xmm0
	por	%xmm3,%xmm1
	por	%xmm1,%xmm0
	pshufd	$78,%xmm0,%xmm1
	por	%xmm1,%xmm0
	leaq	256(%r12),%r12
.byte	102,72,15,126,195

	movq	(%r8),%r8
	movq	(%rsi),%rax

	xorq	%r14,%r14
	xorq	%r15,%r15

	movq	%r8,%rbp
	mulq	%rbx
	movq	%rax,%r10
	movq	(%rcx),%rax

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	leaq	4(%r15),%r15
	adcq	$0,%rdx
	movq	%rdi,(%rsp)
	movq	%rdx,%r13
	jmp	L$1st4x
.p2align	4
L$1st4x:
	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)
	movq	%rdx,%r13

	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	leaq	4(%r15),%r15
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	-16(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)
	movq	%rdx,%r13
	cmpq	%r9,%r15
	jl	L$1st4x

	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)
	movq	%rdx,%r13

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdi,(%rsp,%r15,8)

	leaq	1(%r14),%r14
.p2align	2
L$outer4x:
	leaq	32+128(%rsp,%r9,8),%rdx
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r12),%xmm0
	movdqa	-112(%r12),%xmm1
	movdqa	-96(%r12),%xmm2
	movdqa	-80(%r12),%xmm3
	pand	-128(%rdx),%xmm0
	pand	-112(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r12),%xmm0
	movdqa	-48(%r12),%xmm1
	movdqa	-32(%r12),%xmm2
	movdqa	-16(%r12),%xmm3
	pand	-64(%rdx),%xmm0
	pand	-48(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r12),%xmm0
	movdqa	16(%r12),%xmm1
	movdqa	32(%r12),%xmm2
	movdqa	48(%r12),%xmm3
	pand	0(%rdx),%xmm0
	pand	16(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r12),%xmm0
	movdqa	80(%r12),%xmm1
	movdqa	96(%r12),%xmm2
	movdqa	112(%r12),%xmm3
	pand	64(%rdx),%xmm0
	pand	80(%rdx),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rdx),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rdx),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
	pshufd	$78,%xmm4,%xmm0
	por	%xmm4,%xmm0
	leaq	256(%r12),%r12
.byte	102,72,15,126,195

	xorq	%r15,%r15

	movq	(%rsp),%r10
	movq	%r8,%rbp
	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx),%rax
	adcq	$0,%rdx

	imulq	%r10,%rbp
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx),%rax
	adcq	$0,%rdx
	addq	8(%rsp),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	leaq	4(%r15),%r15
	adcq	$0,%rdx
	movq	%rdx,%r13
	jmp	L$inner4x
.p2align	4
L$inner4x:
	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-16(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%r13

	mulq	%rbx
	addq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-16(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	leaq	4(%r15),%r15
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	-16(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-40(%rsp,%r15,8)
	movq	%rdx,%r13
	cmpq	%r9,%r15
	jl	L$inner4x

	mulq	%rbx
	addq	%rax,%r10
	movq	-16(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-16(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%rax,%r13
	movq	-8(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx
	movq	%rdi,-32(%rsp,%r15,8)
	movq	%rdx,%rdi

	mulq	%rbx
	addq	%rax,%r11
	movq	-8(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	addq	-8(%rsp,%r15,8),%r11
	adcq	$0,%rdx
	leaq	1(%r14),%r14
	movq	%rdx,%r10

	mulq	%rbp
	addq	%rax,%rdi
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%rdi
	adcq	$0,%rdx
	movq	%r13,-24(%rsp,%r15,8)
	movq	%rdx,%r13

	movq	%rdi,-16(%rsp,%r15,8)

	xorq	%rdi,%rdi
	addq	%r10,%r13
	adcq	$0,%rdi
	addq	(%rsp,%r9,8),%r13
	adcq	$0,%rdi
	movq	%r13,-8(%rsp,%r15,8)
	movq	%rdi,(%rsp,%r15,8)

	cmpq	%r9,%r14
	jl	L$outer4x
	movq	16(%rsp,%r9,8),%rdi
	movq	0(%rsp),%rax
	pxor	%xmm0,%xmm0
	movq	8(%rsp),%rdx
	shrq	$2,%r9
	leaq	(%rsp),%rsi
	xorq	%r14,%r14

	subq	0(%rcx),%rax
	movq	16(%rsi),%rbx
	movq	24(%rsi),%rbp
	sbbq	8(%rcx),%rdx
	leaq	-1(%r9),%r15
	jmp	L$sub4x
.p2align	4
L$sub4x:
	movq	%rax,0(%rdi,%r14,8)
	movq	%rdx,8(%rdi,%r14,8)
	sbbq	16(%rcx,%r14,8),%rbx
	movq	32(%rsi,%r14,8),%rax
	movq	40(%rsi,%r14,8),%rdx
	sbbq	24(%rcx,%r14,8),%rbp
	movq	%rbx,16(%rdi,%r14,8)
	movq	%rbp,24(%rdi,%r14,8)
	sbbq	32(%rcx,%r14,8),%rax
	movq	48(%rsi,%r14,8),%rbx
	movq	56(%rsi,%r14,8),%rbp
	sbbq	40(%rcx,%r14,8),%rdx
	leaq	4(%r14),%r14
	decq	%r15
	jnz	L$sub4x

	movq	%rax,0(%rdi,%r14,8)
	movq	32(%rsi,%r14,8),%rax
	sbbq	16(%rcx,%r14,8),%rbx
	movq	%rdx,8(%rdi,%r14,8)
	sbbq	24(%rcx,%r14,8),%rbp
	movq	%rbx,16(%rdi,%r14,8)

	sbbq	$0,%rax
	movq	%rbp,24(%rdi,%r14,8)
	xorq	%r14,%r14
	andq	%rax,%rsi
	notq	%rax
	movq	%rdi,%rcx
	andq	%rax,%rcx
	leaq	-1(%r9),%r15
	orq	%rcx,%rsi

	movdqu	(%rsi),%xmm1
	movdqa	%xmm0,(%rsp)
	movdqu	%xmm1,(%rdi)
	jmp	L$copy4x
.p2align	4
L$copy4x:
	movdqu	16(%rsi,%r14,1),%xmm2
	movdqu	32(%rsi,%r14,1),%xmm1
	movdqa	%xmm0,16(%rsp,%r14,1)
	movdqu	%xmm2,16(%rdi,%r14,1)
	movdqa	%xmm0,32(%rsp,%r14,1)
	movdqu	%xmm1,32(%rdi,%r14,1)
	leaq	32(%r14),%r14
	decq	%r15
	jnz	L$copy4x

	shlq	$2,%r9
	movdqu	16(%rsi,%r14,1),%xmm2
	movdqa	%xmm0,16(%rsp,%r14,1)
	movdqu	%xmm2,16(%rdi,%r14,1)
	movq	8(%rsp,%r9,8),%rsi
	movq	$1,%rax

	movq	(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbp
	movq	40(%rsi),%rbx
	leaq	48(%rsi),%rsp
L$mul4x_epilogue:
	.byte	0xf3,0xc3

.globl	_bn_scatter5

.p2align	4
_bn_scatter5:
	cmpq	$0,%rsi
	jz	L$scatter_epilogue
	leaq	(%rdx,%rcx,8),%rdx
L$scatter:
	movq	(%rdi),%rax
	leaq	8(%rdi),%rdi
	movq	%rax,(%rdx)
	leaq	256(%rdx),%rdx
	subq	$1,%rsi
	jnz	L$scatter
L$scatter_epilogue:
	.byte	0xf3,0xc3


.globl	_bn_gather5

.p2align	4
_bn_gather5:
L$SEH_begin_bn_gather5:

.byte	0x4c,0x8d,0x14,0x24

.byte	0x48,0x81,0xec,0x08,0x01,0x00,0x00

	leaq	L$inc(%rip),%rax
	andq	$-16,%rsp

	movd	%ecx,%xmm5
	movdqa	0(%rax),%xmm0
	movdqa	16(%rax),%xmm1
	leaq	128(%rdx),%r11
	leaq	128(%rsp),%rax

	pshufd	$0,%xmm5,%xmm5
	movdqa	%xmm1,%xmm4
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,-128(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,-112(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,-96(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,-80(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,-64(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,-48(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,-32(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,-16(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,0(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,16(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,32(%rax)
	movdqa	%xmm4,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm5,%xmm0
	movdqa	%xmm3,48(%rax)
	movdqa	%xmm4,%xmm3

	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm5,%xmm1
	movdqa	%xmm0,64(%rax)
	movdqa	%xmm4,%xmm0

	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm5,%xmm2
	movdqa	%xmm1,80(%rax)
	movdqa	%xmm4,%xmm1

	paddd	%xmm3,%xmm0
	pcmpeqd	%xmm5,%xmm3
	movdqa	%xmm2,96(%rax)
	movdqa	%xmm4,%xmm2
	movdqa	%xmm3,112(%rax)
	jmp	L$gather

.p2align	5
L$gather:
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	movdqa	-128(%r11),%xmm0
	movdqa	-112(%r11),%xmm1
	movdqa	-96(%r11),%xmm2
	pand	-128(%rax),%xmm0
	movdqa	-80(%r11),%xmm3
	pand	-112(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	-96(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	-80(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	-64(%r11),%xmm0
	movdqa	-48(%r11),%xmm1
	movdqa	-32(%r11),%xmm2
	pand	-64(%rax),%xmm0
	movdqa	-16(%r11),%xmm3
	pand	-48(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	-32(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	-16(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	0(%r11),%xmm0
	movdqa	16(%r11),%xmm1
	movdqa	32(%r11),%xmm2
	pand	0(%rax),%xmm0
	movdqa	48(%r11),%xmm3
	pand	16(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	32(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	48(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqa	64(%r11),%xmm0
	movdqa	80(%r11),%xmm1
	movdqa	96(%r11),%xmm2
	pand	64(%rax),%xmm0
	movdqa	112(%r11),%xmm3
	pand	80(%rax),%xmm1
	por	%xmm0,%xmm4
	pand	96(%rax),%xmm2
	por	%xmm1,%xmm5
	pand	112(%rax),%xmm3
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	por	%xmm5,%xmm4
	leaq	256(%r11),%r11
	pshufd	$78,%xmm4,%xmm0
	por	%xmm4,%xmm0
	movq	%xmm0,(%rdi)
	leaq	8(%rdi),%rdi
	subq	$1,%rsi
	jnz	L$gather

	leaq	(%r10),%rsp
	.byte	0xf3,0xc3
L$SEH_end_bn_gather5:

.p2align	6
L$inc:
.long	0,0, 1,1
.long	2,2, 2,2
.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,119,105,116,104,32,115,99,97,116,116,101,114,47,103,97,116,104,101,114,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
