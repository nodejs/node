.text	



.globl	_rsaz_512_sqr

.p2align	5
_rsaz_512_sqr:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	subq	$128+24,%rsp

L$sqr_body:
.byte	102,72,15,110,202
	movq	(%rsi),%rdx
	movq	8(%rsi),%rax
	movq	%rcx,128(%rsp)
	movl	$0x80100,%r11d
	andl	_OPENSSL_ia32cap_P+8(%rip),%r11d
	cmpl	$0x80100,%r11d
	je	L$oop_sqrx
	jmp	L$oop_sqr

.p2align	5
L$oop_sqr:
	movl	%r8d,128+8(%rsp)

	movq	%rdx,%rbx
	movq	%rax,%rbp
	mulq	%rdx
	movq	%rax,%r8
	movq	16(%rsi),%rax
	movq	%rdx,%r9

	mulq	%rbx
	addq	%rax,%r9
	movq	24(%rsi),%rax
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	32(%rsi),%rax
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r11
	movq	40(%rsi),%rax
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r12
	movq	48(%rsi),%rax
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r13
	movq	56(%rsi),%rax
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r14
	movq	%rbx,%rax
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r8,%r8
	movq	%rdx,%r15
	adcq	$0,%rcx

	mulq	%rax
	addq	%r8,%rdx
	adcq	$0,%rcx

	movq	%rax,(%rsp)
	movq	%rdx,8(%rsp)


	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	movq	24(%rsi),%rax
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r11
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r12
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r12
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r13
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r13
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r14
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r14
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%rbp
	addq	%rax,%r15
	movq	%rbp,%rax
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx

	xorq	%rbx,%rbx
	addq	%r9,%r9
	movq	%rdx,%r8
	adcq	%r10,%r10
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	movq	16(%rsi),%rbp
	addq	%rax,%r9
	movq	24(%rsi),%rax
	adcq	%rdx,%r10
	adcq	$0,%rbx

	movq	%r9,16(%rsp)
	movq	%r10,24(%rsp)


	mulq	%rbp
	addq	%rax,%r12
	movq	32(%rsi),%rax
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r13
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r13
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r14
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r14
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r15
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r15
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%rbp
	addq	%rax,%r8
	movq	%rbp,%rax
	adcq	$0,%rdx
	addq	%rcx,%r8
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r11,%r11
	movq	%rdx,%r9
	adcq	%r12,%r12
	adcq	$0,%rcx

	mulq	%rax

	addq	%rbx,%rax
	movq	24(%rsi),%r10
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	%rdx,%r12
	adcq	$0,%rcx

	movq	%r11,32(%rsp)
	movq	%r12,40(%rsp)


	movq	%rax,%r11
	mulq	%r10
	addq	%rax,%r14
	movq	40(%rsi),%rax
	movq	%rdx,%rbx
	adcq	$0,%rbx

	movq	%rax,%r12
	mulq	%r10
	addq	%rax,%r15
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r15
	movq	%rdx,%rbx
	adcq	$0,%rbx

	movq	%rax,%rbp
	mulq	%r10
	addq	%rax,%r8
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rbx,%r8
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%r10
	addq	%rax,%r9
	movq	%r10,%rax
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx

	xorq	%rbx,%rbx
	addq	%r13,%r13
	movq	%rdx,%r10
	adcq	%r14,%r14
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	addq	%rax,%r13
	movq	%r12,%rax
	adcq	%rdx,%r14
	adcq	$0,%rbx

	movq	%r13,48(%rsp)
	movq	%r14,56(%rsp)


	mulq	%r11
	addq	%rax,%r8
	movq	%rbp,%rax
	movq	%rdx,%rcx
	adcq	$0,%rcx

	mulq	%r11
	addq	%rax,%r9
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%rcx,%r9
	movq	%rdx,%rcx
	adcq	$0,%rcx

	movq	%rax,%r14
	mulq	%r11
	addq	%rax,%r10
	movq	%r11,%rax
	adcq	$0,%rdx
	addq	%rcx,%r10
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r15,%r15
	movq	%rdx,%r11
	adcq	%r8,%r8
	adcq	$0,%rcx

	mulq	%rax

	addq	%rbx,%rax
	addq	%rax,%r15
	movq	%rbp,%rax
	adcq	%rdx,%r8
	adcq	$0,%rcx

	movq	%r15,64(%rsp)
	movq	%r8,72(%rsp)


	mulq	%r12
	addq	%rax,%r10
	movq	%r14,%rax
	movq	%rdx,%rbx
	adcq	$0,%rbx

	mulq	%r12
	addq	%rax,%r11
	movq	%r12,%rax
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx

	xorq	%rbx,%rbx
	addq	%r9,%r9
	movq	%rdx,%r12
	adcq	%r10,%r10
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	addq	%rax,%r9
	movq	%r14,%rax
	adcq	%rdx,%r10
	adcq	$0,%rbx

	movq	%r9,80(%rsp)
	movq	%r10,88(%rsp)


	mulq	%rbp
	addq	%rax,%r12
	movq	%rbp,%rax
	adcq	$0,%rdx

	xorq	%rcx,%rcx
	addq	%r11,%r11
	movq	%rdx,%r13
	adcq	%r12,%r12
	adcq	$0,%rcx

	mulq	%rax

	addq	%rbx,%rax
	addq	%rax,%r11
	movq	%r14,%rax
	adcq	%rdx,%r12
	adcq	$0,%rcx

	movq	%r11,96(%rsp)
	movq	%r12,104(%rsp)


	xorq	%rbx,%rbx
	addq	%r13,%r13
	adcq	$0,%rbx

	mulq	%rax

	addq	%rcx,%rax
	addq	%r13,%rax
	adcq	%rbx,%rdx

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15
.byte	102,72,15,126,205

	movq	%rax,112(%rsp)
	movq	%rdx,120(%rsp)

	call	__rsaz_512_reduce

	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	movq	%r8,%rdx
	movq	%r9,%rax
	movl	128+8(%rsp),%r8d
	movq	%rdi,%rsi

	decl	%r8d
	jnz	L$oop_sqr
	jmp	L$sqr_tail

.p2align	5
L$oop_sqrx:
	movl	%r8d,128+8(%rsp)
.byte	102,72,15,110,199

	mulxq	%rax,%r8,%r9
	movq	%rax,%rbx

	mulxq	16(%rsi),%rcx,%r10
	xorq	%rbp,%rbp

	mulxq	24(%rsi),%rax,%r11
	adcxq	%rcx,%r9

.byte	0xc4,0x62,0xf3,0xf6,0xa6,0x20,0x00,0x00,0x00
	adcxq	%rax,%r10

.byte	0xc4,0x62,0xfb,0xf6,0xae,0x28,0x00,0x00,0x00
	adcxq	%rcx,%r11

	mulxq	48(%rsi),%rcx,%r14
	adcxq	%rax,%r12
	adcxq	%rcx,%r13

	mulxq	56(%rsi),%rax,%r15
	adcxq	%rax,%r14
	adcxq	%rbp,%r15

	mulxq	%rdx,%rax,%rdi
	movq	%rbx,%rdx
	xorq	%rcx,%rcx
	adoxq	%r8,%r8
	adcxq	%rdi,%r8
	adoxq	%rbp,%rcx
	adcxq	%rbp,%rcx

	movq	%rax,(%rsp)
	movq	%r8,8(%rsp)


.byte	0xc4,0xe2,0xfb,0xf6,0x9e,0x10,0x00,0x00,0x00
	adoxq	%rax,%r10
	adcxq	%rbx,%r11

	mulxq	24(%rsi),%rdi,%r8
	adoxq	%rdi,%r11
.byte	0x66
	adcxq	%r8,%r12

	mulxq	32(%rsi),%rax,%rbx
	adoxq	%rax,%r12
	adcxq	%rbx,%r13

	mulxq	40(%rsi),%rdi,%r8
	adoxq	%rdi,%r13
	adcxq	%r8,%r14

.byte	0xc4,0xe2,0xfb,0xf6,0x9e,0x30,0x00,0x00,0x00
	adoxq	%rax,%r14
	adcxq	%rbx,%r15

.byte	0xc4,0x62,0xc3,0xf6,0x86,0x38,0x00,0x00,0x00
	adoxq	%rdi,%r15
	adcxq	%rbp,%r8
	mulxq	%rdx,%rax,%rdi
	adoxq	%rbp,%r8
.byte	0x48,0x8b,0x96,0x10,0x00,0x00,0x00

	xorq	%rbx,%rbx
	adoxq	%r9,%r9

	adcxq	%rcx,%rax
	adoxq	%r10,%r10
	adcxq	%rax,%r9
	adoxq	%rbp,%rbx
	adcxq	%rdi,%r10
	adcxq	%rbp,%rbx

	movq	%r9,16(%rsp)
.byte	0x4c,0x89,0x94,0x24,0x18,0x00,0x00,0x00


	mulxq	24(%rsi),%rdi,%r9
	adoxq	%rdi,%r12
	adcxq	%r9,%r13

	mulxq	32(%rsi),%rax,%rcx
	adoxq	%rax,%r13
	adcxq	%rcx,%r14

.byte	0xc4,0x62,0xc3,0xf6,0x8e,0x28,0x00,0x00,0x00
	adoxq	%rdi,%r14
	adcxq	%r9,%r15

.byte	0xc4,0xe2,0xfb,0xf6,0x8e,0x30,0x00,0x00,0x00
	adoxq	%rax,%r15
	adcxq	%rcx,%r8

	mulxq	56(%rsi),%rdi,%r9
	adoxq	%rdi,%r8
	adcxq	%rbp,%r9
	mulxq	%rdx,%rax,%rdi
	adoxq	%rbp,%r9
	movq	24(%rsi),%rdx

	xorq	%rcx,%rcx
	adoxq	%r11,%r11

	adcxq	%rbx,%rax
	adoxq	%r12,%r12
	adcxq	%rax,%r11
	adoxq	%rbp,%rcx
	adcxq	%rdi,%r12
	adcxq	%rbp,%rcx

	movq	%r11,32(%rsp)
	movq	%r12,40(%rsp)


	mulxq	32(%rsi),%rax,%rbx
	adoxq	%rax,%r14
	adcxq	%rbx,%r15

	mulxq	40(%rsi),%rdi,%r10
	adoxq	%rdi,%r15
	adcxq	%r10,%r8

	mulxq	48(%rsi),%rax,%rbx
	adoxq	%rax,%r8
	adcxq	%rbx,%r9

	mulxq	56(%rsi),%rdi,%r10
	adoxq	%rdi,%r9
	adcxq	%rbp,%r10
	mulxq	%rdx,%rax,%rdi
	adoxq	%rbp,%r10
	movq	32(%rsi),%rdx

	xorq	%rbx,%rbx
	adoxq	%r13,%r13

	adcxq	%rcx,%rax
	adoxq	%r14,%r14
	adcxq	%rax,%r13
	adoxq	%rbp,%rbx
	adcxq	%rdi,%r14
	adcxq	%rbp,%rbx

	movq	%r13,48(%rsp)
	movq	%r14,56(%rsp)


	mulxq	40(%rsi),%rdi,%r11
	adoxq	%rdi,%r8
	adcxq	%r11,%r9

	mulxq	48(%rsi),%rax,%rcx
	adoxq	%rax,%r9
	adcxq	%rcx,%r10

	mulxq	56(%rsi),%rdi,%r11
	adoxq	%rdi,%r10
	adcxq	%rbp,%r11
	mulxq	%rdx,%rax,%rdi
	movq	40(%rsi),%rdx
	adoxq	%rbp,%r11

	xorq	%rcx,%rcx
	adoxq	%r15,%r15

	adcxq	%rbx,%rax
	adoxq	%r8,%r8
	adcxq	%rax,%r15
	adoxq	%rbp,%rcx
	adcxq	%rdi,%r8
	adcxq	%rbp,%rcx

	movq	%r15,64(%rsp)
	movq	%r8,72(%rsp)


.byte	0xc4,0xe2,0xfb,0xf6,0x9e,0x30,0x00,0x00,0x00
	adoxq	%rax,%r10
	adcxq	%rbx,%r11

.byte	0xc4,0x62,0xc3,0xf6,0xa6,0x38,0x00,0x00,0x00
	adoxq	%rdi,%r11
	adcxq	%rbp,%r12
	mulxq	%rdx,%rax,%rdi
	adoxq	%rbp,%r12
	movq	48(%rsi),%rdx

	xorq	%rbx,%rbx
	adoxq	%r9,%r9

	adcxq	%rcx,%rax
	adoxq	%r10,%r10
	adcxq	%rax,%r9
	adcxq	%rdi,%r10
	adoxq	%rbp,%rbx
	adcxq	%rbp,%rbx

	movq	%r9,80(%rsp)
	movq	%r10,88(%rsp)


.byte	0xc4,0x62,0xfb,0xf6,0xae,0x38,0x00,0x00,0x00
	adoxq	%rax,%r12
	adoxq	%rbp,%r13

	mulxq	%rdx,%rax,%rdi
	xorq	%rcx,%rcx
	movq	56(%rsi),%rdx
	adoxq	%r11,%r11

	adcxq	%rbx,%rax
	adoxq	%r12,%r12
	adcxq	%rax,%r11
	adoxq	%rbp,%rcx
	adcxq	%rdi,%r12
	adcxq	%rbp,%rcx

.byte	0x4c,0x89,0x9c,0x24,0x60,0x00,0x00,0x00
.byte	0x4c,0x89,0xa4,0x24,0x68,0x00,0x00,0x00


	mulxq	%rdx,%rax,%rdx
	xorq	%rbx,%rbx
	adoxq	%r13,%r13

	adcxq	%rcx,%rax
	adoxq	%rbp,%rbx
	adcxq	%r13,%rax
	adcxq	%rdx,%rbx

.byte	102,72,15,126,199
.byte	102,72,15,126,205

	movq	128(%rsp),%rdx
	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	movq	%rax,112(%rsp)
	movq	%rbx,120(%rsp)

	call	__rsaz_512_reducex

	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	movq	%r8,%rdx
	movq	%r9,%rax
	movl	128+8(%rsp),%r8d
	movq	%rdi,%rsi

	decl	%r8d
	jnz	L$oop_sqrx

L$sqr_tail:

	leaq	128+24+48(%rsp),%rax

	movq	-48(%rax),%r15

	movq	-40(%rax),%r14

	movq	-32(%rax),%r13

	movq	-24(%rax),%r12

	movq	-16(%rax),%rbp

	movq	-8(%rax),%rbx

	leaq	(%rax),%rsp

L$sqr_epilogue:
	.byte	0xf3,0xc3


.globl	_rsaz_512_mul

.p2align	5
_rsaz_512_mul:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	subq	$128+24,%rsp

L$mul_body:
.byte	102,72,15,110,199
.byte	102,72,15,110,201
	movq	%r8,128(%rsp)
	movl	$0x80100,%r11d
	andl	_OPENSSL_ia32cap_P+8(%rip),%r11d
	cmpl	$0x80100,%r11d
	je	L$mulx
	movq	(%rdx),%rbx
	movq	%rdx,%rbp
	call	__rsaz_512_mul

.byte	102,72,15,126,199
.byte	102,72,15,126,205

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reduce
	jmp	L$mul_tail

.p2align	5
L$mulx:
	movq	%rdx,%rbp
	movq	(%rdx),%rdx
	call	__rsaz_512_mulx

.byte	102,72,15,126,199
.byte	102,72,15,126,205

	movq	128(%rsp),%rdx
	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reducex
L$mul_tail:
	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	leaq	128+24+48(%rsp),%rax

	movq	-48(%rax),%r15

	movq	-40(%rax),%r14

	movq	-32(%rax),%r13

	movq	-24(%rax),%r12

	movq	-16(%rax),%rbp

	movq	-8(%rax),%rbx

	leaq	(%rax),%rsp

L$mul_epilogue:
	.byte	0xf3,0xc3


.globl	_rsaz_512_mul_gather4

.p2align	5
_rsaz_512_mul_gather4:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	subq	$152,%rsp

L$mul_gather4_body:
	movd	%r9d,%xmm8
	movdqa	L$inc+16(%rip),%xmm1
	movdqa	L$inc(%rip),%xmm0

	pshufd	$0,%xmm8,%xmm8
	movdqa	%xmm1,%xmm7
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm8,%xmm0
	movdqa	%xmm7,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm8,%xmm1
	movdqa	%xmm7,%xmm4
	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm8,%xmm2
	movdqa	%xmm7,%xmm5
	paddd	%xmm3,%xmm4
	pcmpeqd	%xmm8,%xmm3
	movdqa	%xmm7,%xmm6
	paddd	%xmm4,%xmm5
	pcmpeqd	%xmm8,%xmm4
	paddd	%xmm5,%xmm6
	pcmpeqd	%xmm8,%xmm5
	paddd	%xmm6,%xmm7
	pcmpeqd	%xmm8,%xmm6
	pcmpeqd	%xmm8,%xmm7

	movdqa	0(%rdx),%xmm8
	movdqa	16(%rdx),%xmm9
	movdqa	32(%rdx),%xmm10
	movdqa	48(%rdx),%xmm11
	pand	%xmm0,%xmm8
	movdqa	64(%rdx),%xmm12
	pand	%xmm1,%xmm9
	movdqa	80(%rdx),%xmm13
	pand	%xmm2,%xmm10
	movdqa	96(%rdx),%xmm14
	pand	%xmm3,%xmm11
	movdqa	112(%rdx),%xmm15
	leaq	128(%rdx),%rbp
	pand	%xmm4,%xmm12
	pand	%xmm5,%xmm13
	pand	%xmm6,%xmm14
	pand	%xmm7,%xmm15
	por	%xmm10,%xmm8
	por	%xmm11,%xmm9
	por	%xmm12,%xmm8
	por	%xmm13,%xmm9
	por	%xmm14,%xmm8
	por	%xmm15,%xmm9

	por	%xmm9,%xmm8
	pshufd	$0x4e,%xmm8,%xmm9
	por	%xmm9,%xmm8
	movl	$0x80100,%r11d
	andl	_OPENSSL_ia32cap_P+8(%rip),%r11d
	cmpl	$0x80100,%r11d
	je	L$mulx_gather
.byte	102,76,15,126,195

	movq	%r8,128(%rsp)
	movq	%rdi,128+8(%rsp)
	movq	%rcx,128+16(%rsp)

	movq	(%rsi),%rax
	movq	8(%rsi),%rcx
	mulq	%rbx
	movq	%rax,(%rsp)
	movq	%rcx,%rax
	movq	%rdx,%r8

	mulq	%rbx
	addq	%rax,%r8
	movq	16(%rsi),%rax
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r9
	movq	24(%rsi),%rax
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	32(%rsi),%rax
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r11
	movq	40(%rsi),%rax
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r12
	movq	48(%rsi),%rax
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r13
	movq	56(%rsi),%rax
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r14
	movq	(%rsi),%rax
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rsp),%rdi
	movl	$7,%ecx
	jmp	L$oop_mul_gather

.p2align	5
L$oop_mul_gather:
	movdqa	0(%rbp),%xmm8
	movdqa	16(%rbp),%xmm9
	movdqa	32(%rbp),%xmm10
	movdqa	48(%rbp),%xmm11
	pand	%xmm0,%xmm8
	movdqa	64(%rbp),%xmm12
	pand	%xmm1,%xmm9
	movdqa	80(%rbp),%xmm13
	pand	%xmm2,%xmm10
	movdqa	96(%rbp),%xmm14
	pand	%xmm3,%xmm11
	movdqa	112(%rbp),%xmm15
	leaq	128(%rbp),%rbp
	pand	%xmm4,%xmm12
	pand	%xmm5,%xmm13
	pand	%xmm6,%xmm14
	pand	%xmm7,%xmm15
	por	%xmm10,%xmm8
	por	%xmm11,%xmm9
	por	%xmm12,%xmm8
	por	%xmm13,%xmm9
	por	%xmm14,%xmm8
	por	%xmm15,%xmm9

	por	%xmm9,%xmm8
	pshufd	$0x4e,%xmm8,%xmm9
	por	%xmm9,%xmm8
.byte	102,76,15,126,195

	mulq	%rbx
	addq	%rax,%r8
	movq	8(%rsi),%rax
	movq	%r8,(%rdi)
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r15
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rdi),%rdi

	decl	%ecx
	jnz	L$oop_mul_gather

	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	movq	128+8(%rsp),%rdi
	movq	128+16(%rsp),%rbp

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reduce
	jmp	L$mul_gather_tail

.p2align	5
L$mulx_gather:
.byte	102,76,15,126,194

	movq	%r8,128(%rsp)
	movq	%rdi,128+8(%rsp)
	movq	%rcx,128+16(%rsp)

	mulxq	(%rsi),%rbx,%r8
	movq	%rbx,(%rsp)
	xorl	%edi,%edi

	mulxq	8(%rsi),%rax,%r9

	mulxq	16(%rsi),%rbx,%r10
	adcxq	%rax,%r8

	mulxq	24(%rsi),%rax,%r11
	adcxq	%rbx,%r9

	mulxq	32(%rsi),%rbx,%r12
	adcxq	%rax,%r10

	mulxq	40(%rsi),%rax,%r13
	adcxq	%rbx,%r11

	mulxq	48(%rsi),%rbx,%r14
	adcxq	%rax,%r12

	mulxq	56(%rsi),%rax,%r15
	adcxq	%rbx,%r13
	adcxq	%rax,%r14
.byte	0x67
	movq	%r8,%rbx
	adcxq	%rdi,%r15

	movq	$-7,%rcx
	jmp	L$oop_mulx_gather

.p2align	5
L$oop_mulx_gather:
	movdqa	0(%rbp),%xmm8
	movdqa	16(%rbp),%xmm9
	movdqa	32(%rbp),%xmm10
	movdqa	48(%rbp),%xmm11
	pand	%xmm0,%xmm8
	movdqa	64(%rbp),%xmm12
	pand	%xmm1,%xmm9
	movdqa	80(%rbp),%xmm13
	pand	%xmm2,%xmm10
	movdqa	96(%rbp),%xmm14
	pand	%xmm3,%xmm11
	movdqa	112(%rbp),%xmm15
	leaq	128(%rbp),%rbp
	pand	%xmm4,%xmm12
	pand	%xmm5,%xmm13
	pand	%xmm6,%xmm14
	pand	%xmm7,%xmm15
	por	%xmm10,%xmm8
	por	%xmm11,%xmm9
	por	%xmm12,%xmm8
	por	%xmm13,%xmm9
	por	%xmm14,%xmm8
	por	%xmm15,%xmm9

	por	%xmm9,%xmm8
	pshufd	$0x4e,%xmm8,%xmm9
	por	%xmm9,%xmm8
.byte	102,76,15,126,194

.byte	0xc4,0x62,0xfb,0xf6,0x86,0x00,0x00,0x00,0x00
	adcxq	%rax,%rbx
	adoxq	%r9,%r8

	mulxq	8(%rsi),%rax,%r9
	adcxq	%rax,%r8
	adoxq	%r10,%r9

	mulxq	16(%rsi),%rax,%r10
	adcxq	%rax,%r9
	adoxq	%r11,%r10

.byte	0xc4,0x62,0xfb,0xf6,0x9e,0x18,0x00,0x00,0x00
	adcxq	%rax,%r10
	adoxq	%r12,%r11

	mulxq	32(%rsi),%rax,%r12
	adcxq	%rax,%r11
	adoxq	%r13,%r12

	mulxq	40(%rsi),%rax,%r13
	adcxq	%rax,%r12
	adoxq	%r14,%r13

.byte	0xc4,0x62,0xfb,0xf6,0xb6,0x30,0x00,0x00,0x00
	adcxq	%rax,%r13
.byte	0x67
	adoxq	%r15,%r14

	mulxq	56(%rsi),%rax,%r15
	movq	%rbx,64(%rsp,%rcx,8)
	adcxq	%rax,%r14
	adoxq	%rdi,%r15
	movq	%r8,%rbx
	adcxq	%rdi,%r15

	incq	%rcx
	jnz	L$oop_mulx_gather

	movq	%r8,64(%rsp)
	movq	%r9,64+8(%rsp)
	movq	%r10,64+16(%rsp)
	movq	%r11,64+24(%rsp)
	movq	%r12,64+32(%rsp)
	movq	%r13,64+40(%rsp)
	movq	%r14,64+48(%rsp)
	movq	%r15,64+56(%rsp)

	movq	128(%rsp),%rdx
	movq	128+8(%rsp),%rdi
	movq	128+16(%rsp),%rbp

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reducex

L$mul_gather_tail:
	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	leaq	128+24+48(%rsp),%rax

	movq	-48(%rax),%r15

	movq	-40(%rax),%r14

	movq	-32(%rax),%r13

	movq	-24(%rax),%r12

	movq	-16(%rax),%rbp

	movq	-8(%rax),%rbx

	leaq	(%rax),%rsp

L$mul_gather4_epilogue:
	.byte	0xf3,0xc3


.globl	_rsaz_512_mul_scatter4

.p2align	5
_rsaz_512_mul_scatter4:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	movl	%r9d,%r9d
	subq	$128+24,%rsp

L$mul_scatter4_body:
	leaq	(%r8,%r9,8),%r8
.byte	102,72,15,110,199
.byte	102,72,15,110,202
.byte	102,73,15,110,208
	movq	%rcx,128(%rsp)

	movq	%rdi,%rbp
	movl	$0x80100,%r11d
	andl	_OPENSSL_ia32cap_P+8(%rip),%r11d
	cmpl	$0x80100,%r11d
	je	L$mulx_scatter
	movq	(%rdi),%rbx
	call	__rsaz_512_mul

.byte	102,72,15,126,199
.byte	102,72,15,126,205

	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reduce
	jmp	L$mul_scatter_tail

.p2align	5
L$mulx_scatter:
	movq	(%rdi),%rdx
	call	__rsaz_512_mulx

.byte	102,72,15,126,199
.byte	102,72,15,126,205

	movq	128(%rsp),%rdx
	movq	(%rsp),%r8
	movq	8(%rsp),%r9
	movq	16(%rsp),%r10
	movq	24(%rsp),%r11
	movq	32(%rsp),%r12
	movq	40(%rsp),%r13
	movq	48(%rsp),%r14
	movq	56(%rsp),%r15

	call	__rsaz_512_reducex

L$mul_scatter_tail:
	addq	64(%rsp),%r8
	adcq	72(%rsp),%r9
	adcq	80(%rsp),%r10
	adcq	88(%rsp),%r11
	adcq	96(%rsp),%r12
	adcq	104(%rsp),%r13
	adcq	112(%rsp),%r14
	adcq	120(%rsp),%r15
.byte	102,72,15,126,214
	sbbq	%rcx,%rcx

	call	__rsaz_512_subtract

	movq	%r8,0(%rsi)
	movq	%r9,128(%rsi)
	movq	%r10,256(%rsi)
	movq	%r11,384(%rsi)
	movq	%r12,512(%rsi)
	movq	%r13,640(%rsi)
	movq	%r14,768(%rsi)
	movq	%r15,896(%rsi)

	leaq	128+24+48(%rsp),%rax

	movq	-48(%rax),%r15

	movq	-40(%rax),%r14

	movq	-32(%rax),%r13

	movq	-24(%rax),%r12

	movq	-16(%rax),%rbp

	movq	-8(%rax),%rbx

	leaq	(%rax),%rsp

L$mul_scatter4_epilogue:
	.byte	0xf3,0xc3


.globl	_rsaz_512_mul_by_one

.p2align	5
_rsaz_512_mul_by_one:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15


	subq	$128+24,%rsp

L$mul_by_one_body:
	movl	_OPENSSL_ia32cap_P+8(%rip),%eax
	movq	%rdx,%rbp
	movq	%rcx,128(%rsp)

	movq	(%rsi),%r8
	pxor	%xmm0,%xmm0
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11
	movq	32(%rsi),%r12
	movq	40(%rsi),%r13
	movq	48(%rsi),%r14
	movq	56(%rsi),%r15

	movdqa	%xmm0,(%rsp)
	movdqa	%xmm0,16(%rsp)
	movdqa	%xmm0,32(%rsp)
	movdqa	%xmm0,48(%rsp)
	movdqa	%xmm0,64(%rsp)
	movdqa	%xmm0,80(%rsp)
	movdqa	%xmm0,96(%rsp)
	andl	$0x80100,%eax
	cmpl	$0x80100,%eax
	je	L$by_one_callx
	call	__rsaz_512_reduce
	jmp	L$by_one_tail
.p2align	5
L$by_one_callx:
	movq	128(%rsp),%rdx
	call	__rsaz_512_reducex
L$by_one_tail:
	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	leaq	128+24+48(%rsp),%rax

	movq	-48(%rax),%r15

	movq	-40(%rax),%r14

	movq	-32(%rax),%r13

	movq	-24(%rax),%r12

	movq	-16(%rax),%rbp

	movq	-8(%rax),%rbx

	leaq	(%rax),%rsp

L$mul_by_one_epilogue:
	.byte	0xf3,0xc3



.p2align	5
__rsaz_512_reduce:

	movq	%r8,%rbx
	imulq	128+8(%rsp),%rbx
	movq	0(%rbp),%rax
	movl	$8,%ecx
	jmp	L$reduction_loop

.p2align	5
L$reduction_loop:
	mulq	%rbx
	movq	8(%rbp),%rax
	negq	%r8
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rbp),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rbp),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rbp),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	128+8(%rsp),%rsi


	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rbp),%rax
	adcq	$0,%rdx
	imulq	%r8,%rsi
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rbp),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rbp),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	movq	%rsi,%rbx
	addq	%rax,%r15
	movq	0(%rbp),%rax
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	%rdx,%r15
	adcq	$0,%r15

	decl	%ecx
	jne	L$reduction_loop

	.byte	0xf3,0xc3



.p2align	5
__rsaz_512_reducex:


	imulq	%r8,%rdx
	xorq	%rsi,%rsi
	movl	$8,%ecx
	jmp	L$reduction_loopx

.p2align	5
L$reduction_loopx:
	movq	%r8,%rbx
	mulxq	0(%rbp),%rax,%r8
	adcxq	%rbx,%rax
	adoxq	%r9,%r8

	mulxq	8(%rbp),%rax,%r9
	adcxq	%rax,%r8
	adoxq	%r10,%r9

	mulxq	16(%rbp),%rbx,%r10
	adcxq	%rbx,%r9
	adoxq	%r11,%r10

	mulxq	24(%rbp),%rbx,%r11
	adcxq	%rbx,%r10
	adoxq	%r12,%r11

.byte	0xc4,0x62,0xe3,0xf6,0xa5,0x20,0x00,0x00,0x00
	movq	%rdx,%rax
	movq	%r8,%rdx
	adcxq	%rbx,%r11
	adoxq	%r13,%r12

	mulxq	128+8(%rsp),%rbx,%rdx
	movq	%rax,%rdx

	mulxq	40(%rbp),%rax,%r13
	adcxq	%rax,%r12
	adoxq	%r14,%r13

.byte	0xc4,0x62,0xfb,0xf6,0xb5,0x30,0x00,0x00,0x00
	adcxq	%rax,%r13
	adoxq	%r15,%r14

	mulxq	56(%rbp),%rax,%r15
	movq	%rbx,%rdx
	adcxq	%rax,%r14
	adoxq	%rsi,%r15
	adcxq	%rsi,%r15

	decl	%ecx
	jne	L$reduction_loopx

	.byte	0xf3,0xc3



.p2align	5
__rsaz_512_subtract:

	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	movq	0(%rbp),%r8
	movq	8(%rbp),%r9
	negq	%r8
	notq	%r9
	andq	%rcx,%r8
	movq	16(%rbp),%r10
	andq	%rcx,%r9
	notq	%r10
	movq	24(%rbp),%r11
	andq	%rcx,%r10
	notq	%r11
	movq	32(%rbp),%r12
	andq	%rcx,%r11
	notq	%r12
	movq	40(%rbp),%r13
	andq	%rcx,%r12
	notq	%r13
	movq	48(%rbp),%r14
	andq	%rcx,%r13
	notq	%r14
	movq	56(%rbp),%r15
	andq	%rcx,%r14
	notq	%r15
	andq	%rcx,%r15

	addq	(%rdi),%r8
	adcq	8(%rdi),%r9
	adcq	16(%rdi),%r10
	adcq	24(%rdi),%r11
	adcq	32(%rdi),%r12
	adcq	40(%rdi),%r13
	adcq	48(%rdi),%r14
	adcq	56(%rdi),%r15

	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	.byte	0xf3,0xc3



.p2align	5
__rsaz_512_mul:

	leaq	8(%rsp),%rdi

	movq	(%rsi),%rax
	mulq	%rbx
	movq	%rax,(%rdi)
	movq	8(%rsi),%rax
	movq	%rdx,%r8

	mulq	%rbx
	addq	%rax,%r8
	movq	16(%rsi),%rax
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r9
	movq	24(%rsi),%rax
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r10
	movq	32(%rsi),%rax
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r11
	movq	40(%rsi),%rax
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r12
	movq	48(%rsi),%rax
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r13
	movq	56(%rsi),%rax
	movq	%rdx,%r14
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r14
	movq	(%rsi),%rax
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rbp),%rbp
	leaq	8(%rdi),%rdi

	movl	$7,%ecx
	jmp	L$oop_mul

.p2align	5
L$oop_mul:
	movq	(%rbp),%rbx
	mulq	%rbx
	addq	%rax,%r8
	movq	8(%rsi),%rax
	movq	%r8,(%rdi)
	movq	%rdx,%r8
	adcq	$0,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	addq	%r9,%r8
	movq	%rdx,%r9
	adcq	$0,%r9

	mulq	%rbx
	addq	%rax,%r10
	movq	24(%rsi),%rax
	adcq	$0,%rdx
	addq	%r10,%r9
	movq	%rdx,%r10
	adcq	$0,%r10

	mulq	%rbx
	addq	%rax,%r11
	movq	32(%rsi),%rax
	adcq	$0,%rdx
	addq	%r11,%r10
	movq	%rdx,%r11
	adcq	$0,%r11

	mulq	%rbx
	addq	%rax,%r12
	movq	40(%rsi),%rax
	adcq	$0,%rdx
	addq	%r12,%r11
	movq	%rdx,%r12
	adcq	$0,%r12

	mulq	%rbx
	addq	%rax,%r13
	movq	48(%rsi),%rax
	adcq	$0,%rdx
	addq	%r13,%r12
	movq	%rdx,%r13
	adcq	$0,%r13

	mulq	%rbx
	addq	%rax,%r14
	movq	56(%rsi),%rax
	adcq	$0,%rdx
	addq	%r14,%r13
	movq	%rdx,%r14
	leaq	8(%rbp),%rbp
	adcq	$0,%r14

	mulq	%rbx
	addq	%rax,%r15
	movq	(%rsi),%rax
	adcq	$0,%rdx
	addq	%r15,%r14
	movq	%rdx,%r15
	adcq	$0,%r15

	leaq	8(%rdi),%rdi

	decl	%ecx
	jnz	L$oop_mul

	movq	%r8,(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r12,32(%rdi)
	movq	%r13,40(%rdi)
	movq	%r14,48(%rdi)
	movq	%r15,56(%rdi)

	.byte	0xf3,0xc3



.p2align	5
__rsaz_512_mulx:

	mulxq	(%rsi),%rbx,%r8
	movq	$-6,%rcx

	mulxq	8(%rsi),%rax,%r9
	movq	%rbx,8(%rsp)

	mulxq	16(%rsi),%rbx,%r10
	adcq	%rax,%r8

	mulxq	24(%rsi),%rax,%r11
	adcq	%rbx,%r9

	mulxq	32(%rsi),%rbx,%r12
	adcq	%rax,%r10

	mulxq	40(%rsi),%rax,%r13
	adcq	%rbx,%r11

	mulxq	48(%rsi),%rbx,%r14
	adcq	%rax,%r12

	mulxq	56(%rsi),%rax,%r15
	movq	8(%rbp),%rdx
	adcq	%rbx,%r13
	adcq	%rax,%r14
	adcq	$0,%r15

	xorq	%rdi,%rdi
	jmp	L$oop_mulx

.p2align	5
L$oop_mulx:
	movq	%r8,%rbx
	mulxq	(%rsi),%rax,%r8
	adcxq	%rax,%rbx
	adoxq	%r9,%r8

	mulxq	8(%rsi),%rax,%r9
	adcxq	%rax,%r8
	adoxq	%r10,%r9

	mulxq	16(%rsi),%rax,%r10
	adcxq	%rax,%r9
	adoxq	%r11,%r10

	mulxq	24(%rsi),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11

.byte	0x3e,0xc4,0x62,0xfb,0xf6,0xa6,0x20,0x00,0x00,0x00
	adcxq	%rax,%r11
	adoxq	%r13,%r12

	mulxq	40(%rsi),%rax,%r13
	adcxq	%rax,%r12
	adoxq	%r14,%r13

	mulxq	48(%rsi),%rax,%r14
	adcxq	%rax,%r13
	adoxq	%r15,%r14

	mulxq	56(%rsi),%rax,%r15
	movq	64(%rbp,%rcx,8),%rdx
	movq	%rbx,8+64-8(%rsp,%rcx,8)
	adcxq	%rax,%r14
	adoxq	%rdi,%r15
	adcxq	%rdi,%r15

	incq	%rcx
	jnz	L$oop_mulx

	movq	%r8,%rbx
	mulxq	(%rsi),%rax,%r8
	adcxq	%rax,%rbx
	adoxq	%r9,%r8

.byte	0xc4,0x62,0xfb,0xf6,0x8e,0x08,0x00,0x00,0x00
	adcxq	%rax,%r8
	adoxq	%r10,%r9

.byte	0xc4,0x62,0xfb,0xf6,0x96,0x10,0x00,0x00,0x00
	adcxq	%rax,%r9
	adoxq	%r11,%r10

	mulxq	24(%rsi),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11

	mulxq	32(%rsi),%rax,%r12
	adcxq	%rax,%r11
	adoxq	%r13,%r12

	mulxq	40(%rsi),%rax,%r13
	adcxq	%rax,%r12
	adoxq	%r14,%r13

.byte	0xc4,0x62,0xfb,0xf6,0xb6,0x30,0x00,0x00,0x00
	adcxq	%rax,%r13
	adoxq	%r15,%r14

.byte	0xc4,0x62,0xfb,0xf6,0xbe,0x38,0x00,0x00,0x00
	adcxq	%rax,%r14
	adoxq	%rdi,%r15
	adcxq	%rdi,%r15

	movq	%rbx,8+64-8(%rsp)
	movq	%r8,8+64(%rsp)
	movq	%r9,8+64+8(%rsp)
	movq	%r10,8+64+16(%rsp)
	movq	%r11,8+64+24(%rsp)
	movq	%r12,8+64+32(%rsp)
	movq	%r13,8+64+40(%rsp)
	movq	%r14,8+64+48(%rsp)
	movq	%r15,8+64+56(%rsp)

	.byte	0xf3,0xc3


.globl	_rsaz_512_scatter4

.p2align	4
_rsaz_512_scatter4:

	leaq	(%rdi,%rdx,8),%rdi
	movl	$8,%r9d
	jmp	L$oop_scatter
.p2align	4
L$oop_scatter:
	movq	(%rsi),%rax
	leaq	8(%rsi),%rsi
	movq	%rax,(%rdi)
	leaq	128(%rdi),%rdi
	decl	%r9d
	jnz	L$oop_scatter
	.byte	0xf3,0xc3



.globl	_rsaz_512_gather4

.p2align	4
_rsaz_512_gather4:

	movd	%edx,%xmm8
	movdqa	L$inc+16(%rip),%xmm1
	movdqa	L$inc(%rip),%xmm0

	pshufd	$0,%xmm8,%xmm8
	movdqa	%xmm1,%xmm7
	movdqa	%xmm1,%xmm2
	paddd	%xmm0,%xmm1
	pcmpeqd	%xmm8,%xmm0
	movdqa	%xmm7,%xmm3
	paddd	%xmm1,%xmm2
	pcmpeqd	%xmm8,%xmm1
	movdqa	%xmm7,%xmm4
	paddd	%xmm2,%xmm3
	pcmpeqd	%xmm8,%xmm2
	movdqa	%xmm7,%xmm5
	paddd	%xmm3,%xmm4
	pcmpeqd	%xmm8,%xmm3
	movdqa	%xmm7,%xmm6
	paddd	%xmm4,%xmm5
	pcmpeqd	%xmm8,%xmm4
	paddd	%xmm5,%xmm6
	pcmpeqd	%xmm8,%xmm5
	paddd	%xmm6,%xmm7
	pcmpeqd	%xmm8,%xmm6
	pcmpeqd	%xmm8,%xmm7
	movl	$8,%r9d
	jmp	L$oop_gather
.p2align	4
L$oop_gather:
	movdqa	0(%rsi),%xmm8
	movdqa	16(%rsi),%xmm9
	movdqa	32(%rsi),%xmm10
	movdqa	48(%rsi),%xmm11
	pand	%xmm0,%xmm8
	movdqa	64(%rsi),%xmm12
	pand	%xmm1,%xmm9
	movdqa	80(%rsi),%xmm13
	pand	%xmm2,%xmm10
	movdqa	96(%rsi),%xmm14
	pand	%xmm3,%xmm11
	movdqa	112(%rsi),%xmm15
	leaq	128(%rsi),%rsi
	pand	%xmm4,%xmm12
	pand	%xmm5,%xmm13
	pand	%xmm6,%xmm14
	pand	%xmm7,%xmm15
	por	%xmm10,%xmm8
	por	%xmm11,%xmm9
	por	%xmm12,%xmm8
	por	%xmm13,%xmm9
	por	%xmm14,%xmm8
	por	%xmm15,%xmm9

	por	%xmm9,%xmm8
	pshufd	$0x4e,%xmm8,%xmm9
	por	%xmm9,%xmm8
	movq	%xmm8,(%rdi)
	leaq	8(%rdi),%rdi
	decl	%r9d
	jnz	L$oop_gather
	.byte	0xf3,0xc3
L$SEH_end_rsaz_512_gather4:



.p2align	6
L$inc:
.long	0,0, 1,1
.long	2,2, 2,2
