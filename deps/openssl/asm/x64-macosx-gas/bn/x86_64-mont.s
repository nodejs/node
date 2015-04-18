.text


.globl	_bn_mul_mont

.p2align	4
_bn_mul_mont:
	testl	$3,%r9d
	jnz	L$mul_enter
	cmpl	$8,%r9d
	jb	L$mul_enter
	cmpq	%rsi,%rdx
	jne	L$mul4x_enter
	jmp	L$sqr4x_enter

.p2align	4
L$mul_enter:
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movl	%r9d,%r9d
	leaq	2(%r9),%r10
	movq	%rsp,%r11
	negq	%r10
	leaq	(%rsp,%r10,8),%rsp
	andq	$-1024,%rsp

	movq	%r11,8(%rsp,%r9,8)
L$mul_body:
	movq	%rdx,%r12
	movq	(%r8),%r8
	movq	(%r12),%rbx
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
	movq	(%r12,%r14,8),%rbx
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
bn_mul4x_mont:
L$mul4x_enter:
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movl	%r9d,%r9d
	leaq	4(%r9),%r10
	movq	%rsp,%r11
	negq	%r10
	leaq	(%rsp,%r10,8),%rsp
	andq	$-1024,%rsp

	movq	%r11,8(%rsp,%r9,8)
L$mul4x_body:
	movq	%rdi,16(%rsp,%r9,8)
	movq	%rdx,%r12
	movq	(%r8),%r8
	movq	(%r12),%rbx
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
	movq	(%r12,%r14,8),%rbx
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
	movq	%rdi,(%rsp)
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
	movq	%r13,-24(%rsp,%r15,8)
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
	movq	%rdi,-16(%rsp,%r15,8)
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
	movq	%r13,-8(%rsp,%r15,8)
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
	movq	%rdi,-32(%rsp,%r15,8)
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
	movq	%r13,-24(%rsp,%r15,8)
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
	movq	%rdi,-16(%rsp,%r15,8)
	movq	%rdx,%r13

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


.p2align	4
bn_sqr4x_mont:
L$sqr4x_enter:
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	shll	$3,%r9d
	xorq	%r10,%r10
	movq	%rsp,%r11
	subq	%r9,%r10
	movq	(%r8),%r8
	leaq	-72(%rsp,%r10,2),%rsp
	andq	$-1024,%rsp











	movq	%rdi,32(%rsp)
	movq	%rcx,40(%rsp)
	movq	%r8,48(%rsp)
	movq	%r11,56(%rsp)
L$sqr4x_body:







	leaq	32(%r10),%rbp
	leaq	(%rsi,%r9,1),%rsi

	movq	%r9,%rcx


	movq	-32(%rsi,%rbp,1),%r14
	leaq	64(%rsp,%r9,2),%rdi
	movq	-24(%rsi,%rbp,1),%rax
	leaq	-32(%rdi,%rbp,1),%rdi
	movq	-16(%rsi,%rbp,1),%rbx
	movq	%rax,%r15

	mulq	%r14
	movq	%rax,%r10
	movq	%rbx,%rax
	movq	%rdx,%r11
	movq	%r10,-24(%rdi,%rbp,1)

	xorq	%r10,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	movq	%r11,-16(%rdi,%rbp,1)

	leaq	-16(%rbp),%rcx


	movq	8(%rsi,%rcx,1),%rbx
	mulq	%r15
	movq	%rax,%r12
	movq	%rbx,%rax
	movq	%rdx,%r13

	xorq	%r11,%r11
	addq	%r12,%r10
	leaq	16(%rcx),%rcx
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,-8(%rdi,%rcx,1)
	jmp	L$sqr4x_1st

.p2align	4
L$sqr4x_1st:
	movq	(%rsi,%rcx,1),%rbx
	xorq	%r12,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	%rbx,%rax
	adcq	%rdx,%r12

	xorq	%r10,%r10
	addq	%r13,%r11
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	movq	%r11,(%rdi,%rcx,1)


	movq	8(%rsi,%rcx,1),%rbx
	xorq	%r13,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	%rdx,%r13

	xorq	%r11,%r11
	addq	%r12,%r10
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,8(%rdi,%rcx,1)

	movq	16(%rsi,%rcx,1),%rbx
	xorq	%r12,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	%rbx,%rax
	adcq	%rdx,%r12

	xorq	%r10,%r10
	addq	%r13,%r11
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	movq	%r11,16(%rdi,%rcx,1)


	movq	24(%rsi,%rcx,1),%rbx
	xorq	%r13,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	%rdx,%r13

	xorq	%r11,%r11
	addq	%r12,%r10
	leaq	32(%rcx),%rcx
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,-8(%rdi,%rcx,1)

	cmpq	$0,%rcx
	jne	L$sqr4x_1st

	xorq	%r12,%r12
	addq	%r11,%r13
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	adcq	%rdx,%r12

	movq	%r13,(%rdi)
	leaq	16(%rbp),%rbp
	movq	%r12,8(%rdi)
	jmp	L$sqr4x_outer

.p2align	4
L$sqr4x_outer:
	movq	-32(%rsi,%rbp,1),%r14
	leaq	64(%rsp,%r9,2),%rdi
	movq	-24(%rsi,%rbp,1),%rax
	leaq	-32(%rdi,%rbp,1),%rdi
	movq	-16(%rsi,%rbp,1),%rbx
	movq	%rax,%r15

	movq	-24(%rdi,%rbp,1),%r10
	xorq	%r11,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,-24(%rdi,%rbp,1)

	xorq	%r10,%r10
	addq	-16(%rdi,%rbp,1),%r11
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	movq	%r11,-16(%rdi,%rbp,1)

	leaq	-16(%rbp),%rcx
	xorq	%r12,%r12


	movq	8(%rsi,%rcx,1),%rbx
	xorq	%r13,%r13
	addq	8(%rdi,%rcx,1),%r12
	adcq	$0,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	%rdx,%r13

	xorq	%r11,%r11
	addq	%r12,%r10
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,8(%rdi,%rcx,1)

	leaq	16(%rcx),%rcx
	jmp	L$sqr4x_inner

.p2align	4
L$sqr4x_inner:
	movq	(%rsi,%rcx,1),%rbx
	xorq	%r12,%r12
	addq	(%rdi,%rcx,1),%r13
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	%rbx,%rax
	adcq	%rdx,%r12

	xorq	%r10,%r10
	addq	%r13,%r11
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	movq	%r11,(%rdi,%rcx,1)

	movq	8(%rsi,%rcx,1),%rbx
	xorq	%r13,%r13
	addq	8(%rdi,%rcx,1),%r12
	adcq	$0,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	%rdx,%r13

	xorq	%r11,%r11
	addq	%r12,%r10
	leaq	16(%rcx),%rcx
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,-8(%rdi,%rcx,1)

	cmpq	$0,%rcx
	jne	L$sqr4x_inner

	xorq	%r12,%r12
	addq	%r11,%r13
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	adcq	%rdx,%r12

	movq	%r13,(%rdi)
	movq	%r12,8(%rdi)

	addq	$16,%rbp
	jnz	L$sqr4x_outer


	movq	-32(%rsi),%r14
	leaq	64(%rsp,%r9,2),%rdi
	movq	-24(%rsi),%rax
	leaq	-32(%rdi,%rbp,1),%rdi
	movq	-16(%rsi),%rbx
	movq	%rax,%r15

	xorq	%r11,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,-24(%rdi)

	xorq	%r10,%r10
	addq	%r13,%r11
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	movq	%r11,-16(%rdi)

	movq	-8(%rsi),%rbx
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	$0,%rdx

	xorq	%r11,%r11
	addq	%r12,%r10
	movq	%rdx,%r13
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%rbx,%rax
	adcq	%rdx,%r11
	movq	%r10,-8(%rdi)

	xorq	%r12,%r12
	addq	%r11,%r13
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	-16(%rsi),%rax
	adcq	%rdx,%r12

	movq	%r13,(%rdi)
	movq	%r12,8(%rdi)

	mulq	%rbx
	addq	$16,%rbp
	xorq	%r14,%r14
	subq	%r9,%rbp
	xorq	%r15,%r15

	addq	%r12,%rax
	adcq	$0,%rdx
	movq	%rax,8(%rdi)
	movq	%rdx,16(%rdi)
	movq	%r15,24(%rdi)

	movq	-16(%rsi,%rbp,1),%rax
	leaq	64(%rsp,%r9,2),%rdi
	xorq	%r10,%r10
	movq	-24(%rdi,%rbp,2),%r11

	leaq	(%r14,%r10,2),%r12
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13
	shrq	$63,%r11
	orq	%r10,%r13
	movq	-16(%rdi,%rbp,2),%r10
	movq	%r11,%r14
	mulq	%rax
	negq	%r15
	movq	-8(%rdi,%rbp,2),%r11
	adcq	%rax,%r12
	movq	-8(%rsi,%rbp,1),%rax
	movq	%r12,-32(%rdi,%rbp,2)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx
	movq	%r13,-24(%rdi,%rbp,2)
	sbbq	%r15,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8
	shrq	$63,%r11
	orq	%r10,%r8
	movq	0(%rdi,%rbp,2),%r10
	movq	%r11,%r14
	mulq	%rax
	negq	%r15
	movq	8(%rdi,%rbp,2),%r11
	adcq	%rax,%rbx
	movq	0(%rsi,%rbp,1),%rax
	movq	%rbx,-16(%rdi,%rbp,2)
	adcq	%rdx,%r8
	leaq	16(%rbp),%rbp
	movq	%r8,-40(%rdi,%rbp,2)
	sbbq	%r15,%r15
	jmp	L$sqr4x_shift_n_add

.p2align	4
L$sqr4x_shift_n_add:
	leaq	(%r14,%r10,2),%r12
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13
	shrq	$63,%r11
	orq	%r10,%r13
	movq	-16(%rdi,%rbp,2),%r10
	movq	%r11,%r14
	mulq	%rax
	negq	%r15
	movq	-8(%rdi,%rbp,2),%r11
	adcq	%rax,%r12
	movq	-8(%rsi,%rbp,1),%rax
	movq	%r12,-32(%rdi,%rbp,2)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx
	movq	%r13,-24(%rdi,%rbp,2)
	sbbq	%r15,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8
	shrq	$63,%r11
	orq	%r10,%r8
	movq	0(%rdi,%rbp,2),%r10
	movq	%r11,%r14
	mulq	%rax
	negq	%r15
	movq	8(%rdi,%rbp,2),%r11
	adcq	%rax,%rbx
	movq	0(%rsi,%rbp,1),%rax
	movq	%rbx,-16(%rdi,%rbp,2)
	adcq	%rdx,%r8

	leaq	(%r14,%r10,2),%r12
	movq	%r8,-8(%rdi,%rbp,2)
	sbbq	%r15,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13
	shrq	$63,%r11
	orq	%r10,%r13
	movq	16(%rdi,%rbp,2),%r10
	movq	%r11,%r14
	mulq	%rax
	negq	%r15
	movq	24(%rdi,%rbp,2),%r11
	adcq	%rax,%r12
	movq	8(%rsi,%rbp,1),%rax
	movq	%r12,0(%rdi,%rbp,2)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx
	movq	%r13,8(%rdi,%rbp,2)
	sbbq	%r15,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8
	shrq	$63,%r11
	orq	%r10,%r8
	movq	32(%rdi,%rbp,2),%r10
	movq	%r11,%r14
	mulq	%rax
	negq	%r15
	movq	40(%rdi,%rbp,2),%r11
	adcq	%rax,%rbx
	movq	16(%rsi,%rbp,1),%rax
	movq	%rbx,16(%rdi,%rbp,2)
	adcq	%rdx,%r8
	movq	%r8,24(%rdi,%rbp,2)
	sbbq	%r15,%r15
	addq	$32,%rbp
	jnz	L$sqr4x_shift_n_add

	leaq	(%r14,%r10,2),%r12
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r13
	shrq	$63,%r11
	orq	%r10,%r13
	movq	-16(%rdi),%r10
	movq	%r11,%r14
	mulq	%rax
	negq	%r15
	movq	-8(%rdi),%r11
	adcq	%rax,%r12
	movq	-8(%rsi),%rax
	movq	%r12,-32(%rdi)
	adcq	%rdx,%r13

	leaq	(%r14,%r10,2),%rbx
	movq	%r13,-24(%rdi)
	sbbq	%r15,%r15
	shrq	$63,%r10
	leaq	(%rcx,%r11,2),%r8
	shrq	$63,%r11
	orq	%r10,%r8
	mulq	%rax
	negq	%r15
	adcq	%rax,%rbx
	adcq	%rdx,%r8
	movq	%rbx,-16(%rdi)
	movq	%r8,-8(%rdi)
	movq	40(%rsp),%rsi
	movq	48(%rsp),%r8
	xorq	%rcx,%rcx
	movq	%r9,0(%rsp)
	subq	%r9,%rcx
	movq	64(%rsp),%r10
	movq	%r8,%r14
	leaq	64(%rsp,%r9,2),%rax
	leaq	64(%rsp,%r9,1),%rdi
	movq	%rax,8(%rsp)
	leaq	(%rsi,%r9,1),%rsi
	xorq	%rbp,%rbp

	movq	0(%rsi,%rcx,1),%rax
	movq	8(%rsi,%rcx,1),%r9
	imulq	%r10,%r14
	movq	%rax,%rbx
	jmp	L$sqr4x_mont_outer

.p2align	4
L$sqr4x_mont_outer:
	xorq	%r11,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%r9,%rax
	adcq	%rdx,%r11
	movq	%r8,%r15

	xorq	%r10,%r10
	addq	8(%rdi,%rcx,1),%r11
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10

	imulq	%r11,%r15

	movq	16(%rsi,%rcx,1),%rbx
	xorq	%r13,%r13
	addq	%r11,%r12
	adcq	$0,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	%rdx,%r13
	movq	%r12,8(%rdi,%rcx,1)

	xorq	%r11,%r11
	addq	16(%rdi,%rcx,1),%r10
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%r9,%rax
	adcq	%rdx,%r11

	movq	24(%rsi,%rcx,1),%r9
	xorq	%r12,%r12
	addq	%r10,%r13
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	%r9,%rax
	adcq	%rdx,%r12
	movq	%r13,16(%rdi,%rcx,1)

	xorq	%r10,%r10
	addq	24(%rdi,%rcx,1),%r11
	leaq	32(%rcx),%rcx
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	jmp	L$sqr4x_mont_inner

.p2align	4
L$sqr4x_mont_inner:
	movq	(%rsi,%rcx,1),%rbx
	xorq	%r13,%r13
	addq	%r11,%r12
	adcq	$0,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	%rdx,%r13
	movq	%r12,-8(%rdi,%rcx,1)

	xorq	%r11,%r11
	addq	(%rdi,%rcx,1),%r10
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%r9,%rax
	adcq	%rdx,%r11

	movq	8(%rsi,%rcx,1),%r9
	xorq	%r12,%r12
	addq	%r10,%r13
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	%r9,%rax
	adcq	%rdx,%r12
	movq	%r13,(%rdi,%rcx,1)

	xorq	%r10,%r10
	addq	8(%rdi,%rcx,1),%r11
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10


	movq	16(%rsi,%rcx,1),%rbx
	xorq	%r13,%r13
	addq	%r11,%r12
	adcq	$0,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%rbx,%rax
	adcq	%rdx,%r13
	movq	%r12,8(%rdi,%rcx,1)

	xorq	%r11,%r11
	addq	16(%rdi,%rcx,1),%r10
	adcq	$0,%r11
	mulq	%r14
	addq	%rax,%r10
	movq	%r9,%rax
	adcq	%rdx,%r11

	movq	24(%rsi,%rcx,1),%r9
	xorq	%r12,%r12
	addq	%r10,%r13
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	%r9,%rax
	adcq	%rdx,%r12
	movq	%r13,16(%rdi,%rcx,1)

	xorq	%r10,%r10
	addq	24(%rdi,%rcx,1),%r11
	leaq	32(%rcx),%rcx
	adcq	$0,%r10
	mulq	%r14
	addq	%rax,%r11
	movq	%rbx,%rax
	adcq	%rdx,%r10
	cmpq	$0,%rcx
	jne	L$sqr4x_mont_inner

	subq	0(%rsp),%rcx
	movq	%r8,%r14

	xorq	%r13,%r13
	addq	%r11,%r12
	adcq	$0,%r13
	mulq	%r15
	addq	%rax,%r12
	movq	%r9,%rax
	adcq	%rdx,%r13
	movq	%r12,-8(%rdi)

	xorq	%r11,%r11
	addq	(%rdi),%r10
	adcq	$0,%r11
	movq	0(%rsi,%rcx,1),%rbx
	addq	%rbp,%r10
	adcq	$0,%r11

	imulq	16(%rdi,%rcx,1),%r14
	xorq	%r12,%r12
	movq	8(%rsi,%rcx,1),%r9
	addq	%r10,%r13
	movq	16(%rdi,%rcx,1),%r10
	adcq	$0,%r12
	mulq	%r15
	addq	%rax,%r13
	movq	%rbx,%rax
	adcq	%rdx,%r12
	movq	%r13,(%rdi)

	xorq	%rbp,%rbp
	addq	8(%rdi),%r12
	adcq	%rbp,%rbp
	addq	%r11,%r12
	leaq	16(%rdi),%rdi
	adcq	$0,%rbp
	movq	%r12,-8(%rdi)
	cmpq	8(%rsp),%rdi
	jb	L$sqr4x_mont_outer

	movq	0(%rsp),%r9
	movq	%rbp,(%rdi)
	movq	64(%rsp,%r9,1),%rax
	leaq	64(%rsp,%r9,1),%rbx
	movq	40(%rsp),%rsi
	shrq	$5,%r9
	movq	8(%rbx),%rdx
	xorq	%rbp,%rbp

	movq	32(%rsp),%rdi
	subq	0(%rsi),%rax
	movq	16(%rbx),%r10
	movq	24(%rbx),%r11
	sbbq	8(%rsi),%rdx
	leaq	-1(%r9),%rcx
	jmp	L$sqr4x_sub
.p2align	4
L$sqr4x_sub:
	movq	%rax,0(%rdi,%rbp,8)
	movq	%rdx,8(%rdi,%rbp,8)
	sbbq	16(%rsi,%rbp,8),%r10
	movq	32(%rbx,%rbp,8),%rax
	movq	40(%rbx,%rbp,8),%rdx
	sbbq	24(%rsi,%rbp,8),%r11
	movq	%r10,16(%rdi,%rbp,8)
	movq	%r11,24(%rdi,%rbp,8)
	sbbq	32(%rsi,%rbp,8),%rax
	movq	48(%rbx,%rbp,8),%r10
	movq	56(%rbx,%rbp,8),%r11
	sbbq	40(%rsi,%rbp,8),%rdx
	leaq	4(%rbp),%rbp
	decq	%rcx
	jnz	L$sqr4x_sub

	movq	%rax,0(%rdi,%rbp,8)
	movq	32(%rbx,%rbp,8),%rax
	sbbq	16(%rsi,%rbp,8),%r10
	movq	%rdx,8(%rdi,%rbp,8)
	sbbq	24(%rsi,%rbp,8),%r11
	movq	%r10,16(%rdi,%rbp,8)

	sbbq	$0,%rax
	movq	%r11,24(%rdi,%rbp,8)
	xorq	%rbp,%rbp
	andq	%rax,%rbx
	notq	%rax
	movq	%rdi,%rsi
	andq	%rax,%rsi
	leaq	-1(%r9),%rcx
	orq	%rsi,%rbx

	pxor	%xmm0,%xmm0
	leaq	64(%rsp,%r9,8),%rsi
	movdqu	(%rbx),%xmm1
	leaq	(%rsi,%r9,8),%rsi
	movdqa	%xmm0,64(%rsp)
	movdqa	%xmm0,(%rsi)
	movdqu	%xmm1,(%rdi)
	jmp	L$sqr4x_copy
.p2align	4
L$sqr4x_copy:
	movdqu	16(%rbx,%rbp,1),%xmm2
	movdqu	32(%rbx,%rbp,1),%xmm1
	movdqa	%xmm0,80(%rsp,%rbp,1)
	movdqa	%xmm0,96(%rsp,%rbp,1)
	movdqa	%xmm0,16(%rsi,%rbp,1)
	movdqa	%xmm0,32(%rsi,%rbp,1)
	movdqu	%xmm2,16(%rdi,%rbp,1)
	movdqu	%xmm1,32(%rdi,%rbp,1)
	leaq	32(%rbp),%rbp
	decq	%rcx
	jnz	L$sqr4x_copy

	movdqu	16(%rbx,%rbp,1),%xmm2
	movdqa	%xmm0,80(%rsp,%rbp,1)
	movdqa	%xmm0,16(%rsi,%rbp,1)
	movdqu	%xmm2,16(%rdi,%rbp,1)
	movq	56(%rsp),%rsi
	movq	$1,%rax
	movq	0(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbp
	movq	40(%rsi),%rbx
	leaq	48(%rsi),%rsp
L$sqr4x_epilogue:
	.byte	0xf3,0xc3

.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	4
