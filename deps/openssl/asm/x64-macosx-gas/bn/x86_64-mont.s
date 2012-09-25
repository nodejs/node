.text


.globl	_bn_mul_mont

.p2align	4
_bn_mul_mont:
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
L$prologue:
	movq	%rdx,%r12

	movq	(%r8),%r8

	xorq	%r14,%r14
	xorq	%r15,%r15

	movq	(%r12),%rbx
	movq	(%rsi),%rax
	mulq	%rbx
	movq	%rax,%r10
	movq	%rdx,%r11

	imulq	%r8,%rax
	movq	%rax,%rbp

	mulq	(%rcx)
	addq	%r10,%rax
	adcq	$0,%rdx
	movq	%rdx,%r13

	leaq	1(%r15),%r15
L$1st:
	movq	(%rsi,%r15,8),%rax
	mulq	%rbx
	addq	%r11,%rax
	adcq	$0,%rdx
	movq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	movq	%rdx,%r11

	mulq	%rbp
	addq	%r13,%rax
	leaq	1(%r15),%r15
	adcq	$0,%rdx
	addq	%r10,%rax
	adcq	$0,%rdx
	movq	%rax,-16(%rsp,%r15,8)
	cmpq	%r9,%r15
	movq	%rdx,%r13
	jl	L$1st

	xorq	%rdx,%rdx
	addq	%r11,%r13
	adcq	$0,%rdx
	movq	%r13,-8(%rsp,%r9,8)
	movq	%rdx,(%rsp,%r9,8)

	leaq	1(%r14),%r14
.p2align	2
L$outer:
	xorq	%r15,%r15

	movq	(%r12,%r14,8),%rbx
	movq	(%rsi),%rax
	mulq	%rbx
	addq	(%rsp),%rax
	adcq	$0,%rdx
	movq	%rax,%r10
	movq	%rdx,%r11

	imulq	%r8,%rax
	movq	%rax,%rbp

	mulq	(%rcx,%r15,8)
	addq	%r10,%rax
	movq	8(%rsp),%r10
	adcq	$0,%rdx
	movq	%rdx,%r13

	leaq	1(%r15),%r15
.p2align	2
L$inner:
	movq	(%rsi,%r15,8),%rax
	mulq	%rbx
	addq	%r11,%rax
	adcq	$0,%rdx
	addq	%rax,%r10
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbp
	addq	%r13,%rax
	leaq	1(%r15),%r15
	adcq	$0,%rdx
	addq	%r10,%rax
	adcq	$0,%rdx
	movq	(%rsp,%r15,8),%r10
	cmpq	%r9,%r15
	movq	%rax,-16(%rsp,%r15,8)
	movq	%rdx,%r13
	jl	L$inner

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

	leaq	(%rsp),%rsi
	leaq	-1(%r9),%r15

	movq	(%rsi),%rax
	xorq	%r14,%r14
	jmp	L$sub
.p2align	4
L$sub:	sbbq	(%rcx,%r14,8),%rax
	movq	%rax,(%rdi,%r14,8)
	decq	%r15
	movq	8(%rsi,%r14,8),%rax
	leaq	1(%r14),%r14
	jge	L$sub

	sbbq	$0,%rax
	andq	%rax,%rsi
	notq	%rax
	movq	%rdi,%rcx
	andq	%rax,%rcx
	leaq	-1(%r9),%r15
	orq	%rcx,%rsi
.p2align	4
L$copy:
	movq	(%rsi,%r15,8),%rax
	movq	%rax,(%rdi,%r15,8)
	movq	%r14,(%rsp,%r15,8)
	decq	%r15
	jge	L$copy

	movq	8(%rsp,%r9,8),%rsi
	movq	$1,%rax
	movq	(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbp
	movq	40(%rsi),%rbx
	leaq	48(%rsi),%rsp
L$epilogue:
	.byte	0xf3,0xc3

.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	4
