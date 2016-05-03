.text



.globl	bn_mul_mont
.type	bn_mul_mont,@function
.align	16
bn_mul_mont:
	testl	$3,%r9d
	jnz	.Lmul_enter
	cmpl	$8,%r9d
	jb	.Lmul_enter
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






	subq	%rsp,%r11
	andq	$-4096,%r11
.Lmul_page_walk:
	movq	(%rsp,%r11,1),%r10
	subq	$4096,%r11
.byte	0x66,0x2e
	jnc	.Lmul_page_walk

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
	subq	%rsp,%r11
	andq	$-4096,%r11
.Lmul4x_page_walk:
	movq	(%rsp,%r11,1),%r10
	subq	$4096,%r11
.byte	0x2e
	jnc	.Lmul4x_page_walk

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






	leaq	-64(%rsp,%r9,2),%r11
	movq	(%r8),%r8
	subq	%rsi,%r11
	andq	$4095,%r11
	cmpq	%r11,%r10
	jb	.Lsqr8x_sp_alt
	subq	%r11,%rsp
	leaq	-64(%rsp,%r9,2),%rsp
	jmp	.Lsqr8x_sp_done

.align	32
.Lsqr8x_sp_alt:
	leaq	4096-64(,%r9,2),%r10
	leaq	-64(%rsp,%r9,2),%rsp
	subq	%r10,%r11
	movq	$0,%r10
	cmovcq	%r10,%r11
	subq	%r11,%rsp
.Lsqr8x_sp_done:
	andq	$-64,%rsp
	movq	%rax,%r11
	subq	%rsp,%r11
	andq	$-4096,%r11
.Lsqr8x_page_walk:
	movq	(%rsp,%r11,1),%r10
	subq	$4096,%r11
.byte	0x2e
	jnc	.Lsqr8x_page_walk

	movq	%r9,%r10
	negq	%r9

	movq	%r8,32(%rsp)
	movq	%rax,40(%rsp)
.Lsqr8x_body:

.byte	102,72,15,110,209
	pxor	%xmm0,%xmm0
.byte	102,72,15,110,207
.byte	102,73,15,110,218
	call	bn_sqr8x_internal




	leaq	(%rdi,%r9,1),%rbx
	movq	%r9,%rcx
	movq	%r9,%rdx
.byte	102,72,15,126,207
	sarq	$3+2,%rcx
	jmp	.Lsqr8x_sub

.align	32
.Lsqr8x_sub:
	movq	0(%rbx),%r12
	movq	8(%rbx),%r13
	movq	16(%rbx),%r14
	movq	24(%rbx),%r15
	leaq	32(%rbx),%rbx
	sbbq	0(%rbp),%r12
	sbbq	8(%rbp),%r13
	sbbq	16(%rbp),%r14
	sbbq	24(%rbp),%r15
	leaq	32(%rbp),%rbp
	movq	%r12,0(%rdi)
	movq	%r13,8(%rdi)
	movq	%r14,16(%rdi)
	movq	%r15,24(%rdi)
	leaq	32(%rdi),%rdi
	incq	%rcx
	jnz	.Lsqr8x_sub

	sbbq	$0,%rax
	leaq	(%rbx,%r9,1),%rbx
	leaq	(%rdi,%r9,1),%rdi

.byte	102,72,15,110,200
	pxor	%xmm0,%xmm0
	pshufd	$0,%xmm1,%xmm1
	movq	40(%rsp),%rsi
	jmp	.Lsqr8x_cond_copy

.align	32
.Lsqr8x_cond_copy:
	movdqa	0(%rbx),%xmm2
	movdqa	16(%rbx),%xmm3
	leaq	32(%rbx),%rbx
	movdqu	0(%rdi),%xmm4
	movdqu	16(%rdi),%xmm5
	leaq	32(%rdi),%rdi
	movdqa	%xmm0,-32(%rbx)
	movdqa	%xmm0,-16(%rbx)
	movdqa	%xmm0,-32(%rbx,%rdx,1)
	movdqa	%xmm0,-16(%rbx,%rdx,1)
	pcmpeqd	%xmm1,%xmm0
	pand	%xmm1,%xmm2
	pand	%xmm1,%xmm3
	pand	%xmm0,%xmm4
	pand	%xmm0,%xmm5
	pxor	%xmm0,%xmm0
	por	%xmm2,%xmm4
	por	%xmm3,%xmm5
	movdqu	%xmm4,-32(%rdi)
	movdqu	%xmm5,-16(%rdi)
	addq	$32,%r9
	jnz	.Lsqr8x_cond_copy

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
.byte	77,111,110,116,103,111,109,101,114,121,32,77,117,108,116,105,112,108,105,99,97,116,105,111,110,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	16
