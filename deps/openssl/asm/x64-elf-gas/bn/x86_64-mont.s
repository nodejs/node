.text



.globl	bn_mul_mont
.type	bn_mul_mont,@function
.align	16
bn_mul_mont:
	testl	$3,%r9d
	jnz	.Lmul_enter
	cmpl	$8,%r9d
	jb	.Lmul_enter
	movl	OPENSSL_ia32cap_P+8(%rip),%r11d
	cmpq	%rsi,%rdx
	jne	.Lmul4x_enter
	testl	$7,%r9d
	jz	.Lsqr8x_enter
	jmp	.Lmul4x_enter

.align	16
.Lmul_enter:
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
.Lmul_body:
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
	jmp	.L1st_enter

.align	16
.L1st:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r11,%r13
	movq	%r10,%r11
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

.L1st_enter:
	mulq	%rbx
	addq	%rax,%r11
	movq	(%rcx,%r15,8),%rax
	adcq	$0,%rdx
	leaq	1(%r15),%r15
	movq	%rdx,%r10

	mulq	%rbp
	cmpq	%r9,%r15
	jne	.L1st

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
	jmp	.Louter
.align	16
.Louter:
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
	jmp	.Linner_enter

.align	16
.Linner:
	addq	%rax,%r13
	movq	(%rsi,%r15,8),%rax
	adcq	$0,%rdx
	addq	%r10,%r13
	movq	(%rsp,%r15,8),%r10
	adcq	$0,%rdx
	movq	%r13,-16(%rsp,%r15,8)
	movq	%rdx,%r13

.Linner_enter:
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
	jne	.Linner

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
	jb	.Louter

	xorq	%r14,%r14
	movq	(%rsp),%rax
	leaq	(%rsp),%rsi
	movq	%r9,%r15
	jmp	.Lsub
.align	16
.Lsub:	sbbq	(%rcx,%r14,8),%rax
	movq	%rax,(%rdi,%r14,8)
	movq	8(%rsi,%r14,8),%rax
	leaq	1(%r14),%r14
	decq	%r15
	jnz	.Lsub

	sbbq	$0,%rax
	xorq	%r14,%r14
	andq	%rax,%rsi
	notq	%rax
	movq	%rdi,%rcx
	andq	%rax,%rcx
	movq	%r9,%r15
	orq	%rcx,%rsi
.align	16
.Lcopy:
	movq	(%rsi,%r14,8),%rax
	movq	%r14,(%rsp,%r14,8)
	movq	%rax,(%rdi,%r14,8)
	leaq	1(%r14),%r14
	subq	$1,%r15
	jnz	.Lcopy

	movq	8(%rsp,%r9,8),%rsi
	movq	$1,%rax
	movq	(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbp
	movq	40(%rsi),%rbx
	leaq	48(%rsi),%rsp
.Lmul_epilogue:
	.byte	0xf3,0xc3
.size	bn_mul_mont,.-bn_mul_mont
.type	bn_mul4x_mont,@function
.align	16
bn_mul4x_mont:
.Lmul4x_enter:
	andl	$524544,%r11d
	cmpl	$524544,%r11d
	je	.Lmulx4x_enter
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
.Lmul4x_body:
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
	jmp	.L1st4x
.align	16
.L1st4x:
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
	jb	.L1st4x

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
.align	4
.Louter4x:
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
	jmp	.Linner4x
.align	16
.Linner4x:
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
	jb	.Linner4x

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
	jb	.Louter4x
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
	jmp	.Lsub4x
.align	16
.Lsub4x:
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
	jnz	.Lsub4x

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
	jmp	.Lcopy4x
.align	16
.Lcopy4x:
	movdqu	16(%rsi,%r14,1),%xmm2
	movdqu	32(%rsi,%r14,1),%xmm1
	movdqa	%xmm0,16(%rsp,%r14,1)
	movdqu	%xmm2,16(%rdi,%r14,1)
	movdqa	%xmm0,32(%rsp,%r14,1)
	movdqu	%xmm1,32(%rdi,%r14,1)
	leaq	32(%r14),%r14
	decq	%r15
	jnz	.Lcopy4x

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
.Lmul4x_epilogue:
	.byte	0xf3,0xc3
.size	bn_mul4x_mont,.-bn_mul4x_mont



.type	bn_sqr8x_mont,@function
.align	32
bn_sqr8x_mont:
.Lsqr8x_enter:
	movq	%rsp,%rax
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movl	%r9d,%r10d
	shll	$3,%r9d
	shlq	$3+2,%r10
	negq	%r9






	leaq	-64(%rsp,%r9,4),%r11
	movq	(%r8),%r8
	subq	%rsi,%r11
	andq	$4095,%r11
	cmpq	%r11,%r10
	jb	.Lsqr8x_sp_alt
	subq	%r11,%rsp
	leaq	-64(%rsp,%r9,4),%rsp
	jmp	.Lsqr8x_sp_done

.align	32
.Lsqr8x_sp_alt:
	leaq	4096-64(,%r9,4),%r10
	leaq	-64(%rsp,%r9,4),%rsp
	subq	%r10,%r11
	movq	$0,%r10
	cmovcq	%r10,%r11
	subq	%r11,%rsp
.Lsqr8x_sp_done:
	andq	$-64,%rsp
	movq	%r9,%r10
	negq	%r9

	leaq	64(%rsp,%r9,2),%r11
	movq	%r8,32(%rsp)
	movq	%rax,40(%rsp)
.Lsqr8x_body:

	movq	%r9,%rbp
.byte	102,73,15,110,211
	shrq	$3+2,%rbp
	movl	OPENSSL_ia32cap_P+8(%rip),%eax
	jmp	.Lsqr8x_copy_n

.align	32
.Lsqr8x_copy_n:
	movq	0(%rcx),%xmm0
	movq	8(%rcx),%xmm1
	movq	16(%rcx),%xmm3
	movq	24(%rcx),%xmm4
	leaq	32(%rcx),%rcx
	movdqa	%xmm0,0(%r11)
	movdqa	%xmm1,16(%r11)
	movdqa	%xmm3,32(%r11)
	movdqa	%xmm4,48(%r11)
	leaq	64(%r11),%r11
	decq	%rbp
	jnz	.Lsqr8x_copy_n

	pxor	%xmm0,%xmm0
.byte	102,72,15,110,207
.byte	102,73,15,110,218
	andl	$524544,%eax
	cmpl	$524544,%eax
	jne	.Lsqr8x_nox

	call	bn_sqrx8x_internal

	pxor	%xmm0,%xmm0
	leaq	48(%rsp),%rax
	leaq	64(%rsp,%r9,2),%rdx
	shrq	$3+2,%r9
	movq	40(%rsp),%rsi
	jmp	.Lsqr8x_zero

.align	32
.Lsqr8x_nox:
	call	bn_sqr8x_internal

	pxor	%xmm0,%xmm0
	leaq	48(%rsp),%rax
	leaq	64(%rsp,%r9,2),%rdx
	shrq	$3+2,%r9
	movq	40(%rsp),%rsi
	jmp	.Lsqr8x_zero

.align	32
.Lsqr8x_zero:
	movdqa	%xmm0,0(%rax)
	movdqa	%xmm0,16(%rax)
	movdqa	%xmm0,32(%rax)
	movdqa	%xmm0,48(%rax)
	leaq	64(%rax),%rax
	movdqa	%xmm0,0(%rdx)
	movdqa	%xmm0,16(%rdx)
	movdqa	%xmm0,32(%rdx)
	movdqa	%xmm0,48(%rdx)
	leaq	64(%rdx),%rdx
	decq	%r9
	jnz	.Lsqr8x_zero

	movq	$1,%rax
	movq	-48(%rsi),%r15
	movq	-40(%rsi),%r14
	movq	-32(%rsi),%r13
	movq	-24(%rsi),%r12
	movq	-16(%rsi),%rbp
	movq	-8(%rsi),%rbx
	leaq	(%rsi),%rsp
.Lsqr8x_epilogue:
	.byte	0xf3,0xc3
.size	bn_sqr8x_mont,.-bn_sqr8x_mont
.type	bn_mulx4x_mont,@function
.align	32
bn_mulx4x_mont:
.Lmulx4x_enter:
	movq	%rsp,%rax
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	shll	$3,%r9d
.byte	0x67
	xorq	%r10,%r10
	subq	%r9,%r10
	movq	(%r8),%r8
	leaq	-72(%rsp,%r10,1),%rsp
	leaq	(%rdx,%r9,1),%r10
	andq	$-128,%rsp












	movq	%r9,0(%rsp)
	shrq	$5,%r9
	movq	%r10,16(%rsp)
	subq	$1,%r9
	movq	%r8,24(%rsp)
	movq	%rdi,32(%rsp)
	movq	%rax,40(%rsp)
	movq	%r9,48(%rsp)
	jmp	.Lmulx4x_body

.align	32
.Lmulx4x_body:
	leaq	8(%rdx),%rdi
	movq	(%rdx),%rdx
	leaq	64+32(%rsp),%rbx
	movq	%rdx,%r9

	mulxq	0(%rsi),%r8,%rax
	mulxq	8(%rsi),%r11,%r14
	addq	%rax,%r11
	movq	%rdi,8(%rsp)
	mulxq	16(%rsi),%r12,%r13
	adcq	%r14,%r12
	adcq	$0,%r13

	movq	%r8,%rdi
	imulq	24(%rsp),%r8
	xorq	%rbp,%rbp

	mulxq	24(%rsi),%rax,%r14
	movq	%r8,%rdx
	leaq	32(%rsi),%rsi
	adcxq	%rax,%r13
	adcxq	%rbp,%r14

	mulxq	0(%rcx),%rax,%r10
	adcxq	%rax,%rdi
	adoxq	%r11,%r10
	mulxq	8(%rcx),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11
.byte	0xc4,0x62,0xfb,0xf6,0xa1,0x10,0x00,0x00,0x00
	movq	48(%rsp),%rdi
	movq	%r10,-32(%rbx)
	adcxq	%rax,%r11
	adoxq	%r13,%r12
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r11,-24(%rbx)
	adcxq	%rax,%r12
	adoxq	%rbp,%r15
	leaq	32(%rcx),%rcx
	movq	%r12,-16(%rbx)

	jmp	.Lmulx4x_1st

.align	32
.Lmulx4x_1st:
	adcxq	%rbp,%r15
	mulxq	0(%rsi),%r10,%rax
	adcxq	%r14,%r10
	mulxq	8(%rsi),%r11,%r14
	adcxq	%rax,%r11
	mulxq	16(%rsi),%r12,%rax
	adcxq	%r14,%r12
	mulxq	24(%rsi),%r13,%r14
.byte	0x67,0x67
	movq	%r8,%rdx
	adcxq	%rax,%r13
	adcxq	%rbp,%r14
	leaq	32(%rsi),%rsi
	leaq	32(%rbx),%rbx

	adoxq	%r15,%r10
	mulxq	0(%rcx),%rax,%r15
	adcxq	%rax,%r10
	adoxq	%r15,%r11
	mulxq	8(%rcx),%rax,%r15
	adcxq	%rax,%r11
	adoxq	%r15,%r12
	mulxq	16(%rcx),%rax,%r15
	movq	%r10,-40(%rbx)
	adcxq	%rax,%r12
	movq	%r11,-32(%rbx)
	adoxq	%r15,%r13
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r12,-24(%rbx)
	adcxq	%rax,%r13
	adoxq	%rbp,%r15
	leaq	32(%rcx),%rcx
	movq	%r13,-16(%rbx)

	decq	%rdi
	jnz	.Lmulx4x_1st

	movq	0(%rsp),%rax
	movq	8(%rsp),%rdi
	adcq	%rbp,%r15
	addq	%r15,%r14
	sbbq	%r15,%r15
	movq	%r14,-8(%rbx)
	jmp	.Lmulx4x_outer

.align	32
.Lmulx4x_outer:
	movq	(%rdi),%rdx
	leaq	8(%rdi),%rdi
	subq	%rax,%rsi
	movq	%r15,(%rbx)
	leaq	64+32(%rsp),%rbx
	subq	%rax,%rcx

	mulxq	0(%rsi),%r8,%r11
	xorl	%ebp,%ebp
	movq	%rdx,%r9
	mulxq	8(%rsi),%r14,%r12
	adoxq	-32(%rbx),%r8
	adcxq	%r14,%r11
	mulxq	16(%rsi),%r15,%r13
	adoxq	-24(%rbx),%r11
	adcxq	%r15,%r12
	adoxq	%rbp,%r12
	adcxq	%rbp,%r13

	movq	%rdi,8(%rsp)
.byte	0x67
	movq	%r8,%r15
	imulq	24(%rsp),%r8
	xorl	%ebp,%ebp

	mulxq	24(%rsi),%rax,%r14
	movq	%r8,%rdx
	adoxq	-16(%rbx),%r12
	adcxq	%rax,%r13
	adoxq	-8(%rbx),%r13
	adcxq	%rbp,%r14
	leaq	32(%rsi),%rsi
	adoxq	%rbp,%r14

	mulxq	0(%rcx),%rax,%r10
	adcxq	%rax,%r15
	adoxq	%r11,%r10
	mulxq	8(%rcx),%rax,%r11
	adcxq	%rax,%r10
	adoxq	%r12,%r11
	mulxq	16(%rcx),%rax,%r12
	movq	%r10,-32(%rbx)
	adcxq	%rax,%r11
	adoxq	%r13,%r12
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r11,-24(%rbx)
	leaq	32(%rcx),%rcx
	adcxq	%rax,%r12
	adoxq	%rbp,%r15
	movq	48(%rsp),%rdi
	movq	%r12,-16(%rbx)

	jmp	.Lmulx4x_inner

.align	32
.Lmulx4x_inner:
	mulxq	0(%rsi),%r10,%rax
	adcxq	%rbp,%r15
	adoxq	%r14,%r10
	mulxq	8(%rsi),%r11,%r14
	adcxq	0(%rbx),%r10
	adoxq	%rax,%r11
	mulxq	16(%rsi),%r12,%rax
	adcxq	8(%rbx),%r11
	adoxq	%r14,%r12
	mulxq	24(%rsi),%r13,%r14
	movq	%r8,%rdx
	adcxq	16(%rbx),%r12
	adoxq	%rax,%r13
	adcxq	24(%rbx),%r13
	adoxq	%rbp,%r14
	leaq	32(%rsi),%rsi
	leaq	32(%rbx),%rbx
	adcxq	%rbp,%r14

	adoxq	%r15,%r10
	mulxq	0(%rcx),%rax,%r15
	adcxq	%rax,%r10
	adoxq	%r15,%r11
	mulxq	8(%rcx),%rax,%r15
	adcxq	%rax,%r11
	adoxq	%r15,%r12
	mulxq	16(%rcx),%rax,%r15
	movq	%r10,-40(%rbx)
	adcxq	%rax,%r12
	adoxq	%r15,%r13
	mulxq	24(%rcx),%rax,%r15
	movq	%r9,%rdx
	movq	%r11,-32(%rbx)
	movq	%r12,-24(%rbx)
	adcxq	%rax,%r13
	adoxq	%rbp,%r15
	leaq	32(%rcx),%rcx
	movq	%r13,-16(%rbx)

	decq	%rdi
	jnz	.Lmulx4x_inner

	movq	0(%rsp),%rax
	movq	8(%rsp),%rdi
	adcq	%rbp,%r15
	subq	0(%rbx),%rbp
	adcq	%r15,%r14
	movq	-8(%rcx),%r8
	sbbq	%r15,%r15
	movq	%r14,-8(%rbx)

	cmpq	16(%rsp),%rdi
	jne	.Lmulx4x_outer

	subq	%r14,%r8
	sbbq	%r8,%r8
	orq	%r8,%r15

	negq	%rax
	xorq	%rdx,%rdx
	movq	32(%rsp),%rdi
	leaq	64(%rsp),%rbx

	pxor	%xmm0,%xmm0
	movq	0(%rcx,%rax,1),%r8
	movq	8(%rcx,%rax,1),%r9
	negq	%r8
	jmp	.Lmulx4x_sub_entry

.align	32
.Lmulx4x_sub:
	movq	0(%rcx,%rax,1),%r8
	movq	8(%rcx,%rax,1),%r9
	notq	%r8
.Lmulx4x_sub_entry:
	movq	16(%rcx,%rax,1),%r10
	notq	%r9
	andq	%r15,%r8
	movq	24(%rcx,%rax,1),%r11
	notq	%r10
	andq	%r15,%r9
	notq	%r11
	andq	%r15,%r10
	andq	%r15,%r11

	negq	%rdx
	adcq	0(%rbx),%r8
	adcq	8(%rbx),%r9
	movdqa	%xmm0,(%rbx)
	adcq	16(%rbx),%r10
	adcq	24(%rbx),%r11
	movdqa	%xmm0,16(%rbx)
	leaq	32(%rbx),%rbx
	sbbq	%rdx,%rdx

	movq	%r8,0(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	leaq	32(%rdi),%rdi

	addq	$32,%rax
	jnz	.Lmulx4x_sub

	movq	40(%rsp),%rsi
	movq	$1,%rax
	movq	-48(%rsi),%r15
	movq	-40(%rsi),%r14
	movq	-32(%rsi),%r13
	movq	-24(%rsi),%r12
	movq	-16(%rsi),%rbp
	movq	-8(%rsi),%rbx
	leaq	(%rsi),%rsp
.Lmulx4x_epilogue:
	.byte	0xf3,0xc3
.size	bn_mulx4x_mont,.-bn_mulx4x_mont
.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	16
