.text



.globl	sha1_multi_block
.type	sha1_multi_block,@function
.align	32
sha1_multi_block:
	movq	OPENSSL_ia32cap_P+4(%rip),%rcx
	btq	$61,%rcx
	jc	_shaext_shortcut
	movq	%rsp,%rax
	pushq	%rbx
	pushq	%rbp
	subq	$288,%rsp
	andq	$-256,%rsp
	movq	%rax,272(%rsp)
.Lbody:
	leaq	K_XX_XX(%rip),%rbp
	leaq	256(%rsp),%rbx

.Loop_grande:
	movl	%edx,280(%rsp)
	xorl	%edx,%edx
	movq	0(%rsi),%r8
	movl	8(%rsi),%ecx
	cmpl	%edx,%ecx
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movl	%ecx,0(%rbx)
	cmovleq	%rbp,%r8
	movq	16(%rsi),%r9
	movl	24(%rsi),%ecx
	cmpl	%edx,%ecx
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movl	%ecx,4(%rbx)
	cmovleq	%rbp,%r9
	movq	32(%rsi),%r10
	movl	40(%rsi),%ecx
	cmpl	%edx,%ecx
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movl	%ecx,8(%rbx)
	cmovleq	%rbp,%r10
	movq	48(%rsi),%r11
	movl	56(%rsi),%ecx
	cmpl	%edx,%ecx
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movl	%ecx,12(%rbx)
	cmovleq	%rbp,%r11
	testl	%edx,%edx
	jz	.Ldone

	movdqu	0(%rdi),%xmm10
	leaq	128(%rsp),%rax
	movdqu	32(%rdi),%xmm11
	movdqu	64(%rdi),%xmm12
	movdqu	96(%rdi),%xmm13
	movdqu	128(%rdi),%xmm14
	movdqa	96(%rbp),%xmm5
	movdqa	-32(%rbp),%xmm15
	jmp	.Loop

.align	32
.Loop:
	movd	(%r8),%xmm0
	leaq	64(%r8),%r8
	movd	(%r9),%xmm2
	leaq	64(%r9),%r9
	movd	(%r10),%xmm3
	leaq	64(%r10),%r10
	movd	(%r11),%xmm4
	leaq	64(%r11),%r11
	punpckldq	%xmm3,%xmm0
	movd	-60(%r8),%xmm1
	punpckldq	%xmm4,%xmm2
	movd	-60(%r9),%xmm9
	punpckldq	%xmm2,%xmm0
	movd	-60(%r10),%xmm8
.byte	102,15,56,0,197
	movd	-60(%r11),%xmm7
	punpckldq	%xmm8,%xmm1
	movdqa	%xmm10,%xmm8
	paddd	%xmm15,%xmm14
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm11,%xmm7
	movdqa	%xmm11,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm13,%xmm7
	pand	%xmm12,%xmm6
	punpckldq	%xmm9,%xmm1
	movdqa	%xmm10,%xmm9

	movdqa	%xmm0,0-128(%rax)
	paddd	%xmm0,%xmm14
	movd	-56(%r8),%xmm2
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm11,%xmm7

	por	%xmm9,%xmm8
	movd	-56(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
.byte	102,15,56,0,205
	movd	-56(%r10),%xmm8
	por	%xmm7,%xmm11
	movd	-56(%r11),%xmm7
	punpckldq	%xmm8,%xmm2
	movdqa	%xmm14,%xmm8
	paddd	%xmm15,%xmm13
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm10,%xmm7
	movdqa	%xmm10,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm12,%xmm7
	pand	%xmm11,%xmm6
	punpckldq	%xmm9,%xmm2
	movdqa	%xmm14,%xmm9

	movdqa	%xmm1,16-128(%rax)
	paddd	%xmm1,%xmm13
	movd	-52(%r8),%xmm3
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm10,%xmm7

	por	%xmm9,%xmm8
	movd	-52(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
.byte	102,15,56,0,213
	movd	-52(%r10),%xmm8
	por	%xmm7,%xmm10
	movd	-52(%r11),%xmm7
	punpckldq	%xmm8,%xmm3
	movdqa	%xmm13,%xmm8
	paddd	%xmm15,%xmm12
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm14,%xmm7
	movdqa	%xmm14,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm11,%xmm7
	pand	%xmm10,%xmm6
	punpckldq	%xmm9,%xmm3
	movdqa	%xmm13,%xmm9

	movdqa	%xmm2,32-128(%rax)
	paddd	%xmm2,%xmm12
	movd	-48(%r8),%xmm4
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm14,%xmm7

	por	%xmm9,%xmm8
	movd	-48(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
.byte	102,15,56,0,221
	movd	-48(%r10),%xmm8
	por	%xmm7,%xmm14
	movd	-48(%r11),%xmm7
	punpckldq	%xmm8,%xmm4
	movdqa	%xmm12,%xmm8
	paddd	%xmm15,%xmm11
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm13,%xmm7
	movdqa	%xmm13,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm10,%xmm7
	pand	%xmm14,%xmm6
	punpckldq	%xmm9,%xmm4
	movdqa	%xmm12,%xmm9

	movdqa	%xmm3,48-128(%rax)
	paddd	%xmm3,%xmm11
	movd	-44(%r8),%xmm0
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm13,%xmm7

	por	%xmm9,%xmm8
	movd	-44(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
.byte	102,15,56,0,229
	movd	-44(%r10),%xmm8
	por	%xmm7,%xmm13
	movd	-44(%r11),%xmm7
	punpckldq	%xmm8,%xmm0
	movdqa	%xmm11,%xmm8
	paddd	%xmm15,%xmm10
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm12,%xmm7
	movdqa	%xmm12,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm14,%xmm7
	pand	%xmm13,%xmm6
	punpckldq	%xmm9,%xmm0
	movdqa	%xmm11,%xmm9

	movdqa	%xmm4,64-128(%rax)
	paddd	%xmm4,%xmm10
	movd	-40(%r8),%xmm1
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm12,%xmm7

	por	%xmm9,%xmm8
	movd	-40(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
.byte	102,15,56,0,197
	movd	-40(%r10),%xmm8
	por	%xmm7,%xmm12
	movd	-40(%r11),%xmm7
	punpckldq	%xmm8,%xmm1
	movdqa	%xmm10,%xmm8
	paddd	%xmm15,%xmm14
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm11,%xmm7
	movdqa	%xmm11,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm13,%xmm7
	pand	%xmm12,%xmm6
	punpckldq	%xmm9,%xmm1
	movdqa	%xmm10,%xmm9

	movdqa	%xmm0,80-128(%rax)
	paddd	%xmm0,%xmm14
	movd	-36(%r8),%xmm2
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm11,%xmm7

	por	%xmm9,%xmm8
	movd	-36(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
.byte	102,15,56,0,205
	movd	-36(%r10),%xmm8
	por	%xmm7,%xmm11
	movd	-36(%r11),%xmm7
	punpckldq	%xmm8,%xmm2
	movdqa	%xmm14,%xmm8
	paddd	%xmm15,%xmm13
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm10,%xmm7
	movdqa	%xmm10,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm12,%xmm7
	pand	%xmm11,%xmm6
	punpckldq	%xmm9,%xmm2
	movdqa	%xmm14,%xmm9

	movdqa	%xmm1,96-128(%rax)
	paddd	%xmm1,%xmm13
	movd	-32(%r8),%xmm3
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm10,%xmm7

	por	%xmm9,%xmm8
	movd	-32(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
.byte	102,15,56,0,213
	movd	-32(%r10),%xmm8
	por	%xmm7,%xmm10
	movd	-32(%r11),%xmm7
	punpckldq	%xmm8,%xmm3
	movdqa	%xmm13,%xmm8
	paddd	%xmm15,%xmm12
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm14,%xmm7
	movdqa	%xmm14,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm11,%xmm7
	pand	%xmm10,%xmm6
	punpckldq	%xmm9,%xmm3
	movdqa	%xmm13,%xmm9

	movdqa	%xmm2,112-128(%rax)
	paddd	%xmm2,%xmm12
	movd	-28(%r8),%xmm4
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm14,%xmm7

	por	%xmm9,%xmm8
	movd	-28(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
.byte	102,15,56,0,221
	movd	-28(%r10),%xmm8
	por	%xmm7,%xmm14
	movd	-28(%r11),%xmm7
	punpckldq	%xmm8,%xmm4
	movdqa	%xmm12,%xmm8
	paddd	%xmm15,%xmm11
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm13,%xmm7
	movdqa	%xmm13,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm10,%xmm7
	pand	%xmm14,%xmm6
	punpckldq	%xmm9,%xmm4
	movdqa	%xmm12,%xmm9

	movdqa	%xmm3,128-128(%rax)
	paddd	%xmm3,%xmm11
	movd	-24(%r8),%xmm0
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm13,%xmm7

	por	%xmm9,%xmm8
	movd	-24(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
.byte	102,15,56,0,229
	movd	-24(%r10),%xmm8
	por	%xmm7,%xmm13
	movd	-24(%r11),%xmm7
	punpckldq	%xmm8,%xmm0
	movdqa	%xmm11,%xmm8
	paddd	%xmm15,%xmm10
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm12,%xmm7
	movdqa	%xmm12,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm14,%xmm7
	pand	%xmm13,%xmm6
	punpckldq	%xmm9,%xmm0
	movdqa	%xmm11,%xmm9

	movdqa	%xmm4,144-128(%rax)
	paddd	%xmm4,%xmm10
	movd	-20(%r8),%xmm1
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm12,%xmm7

	por	%xmm9,%xmm8
	movd	-20(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
.byte	102,15,56,0,197
	movd	-20(%r10),%xmm8
	por	%xmm7,%xmm12
	movd	-20(%r11),%xmm7
	punpckldq	%xmm8,%xmm1
	movdqa	%xmm10,%xmm8
	paddd	%xmm15,%xmm14
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm11,%xmm7
	movdqa	%xmm11,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm13,%xmm7
	pand	%xmm12,%xmm6
	punpckldq	%xmm9,%xmm1
	movdqa	%xmm10,%xmm9

	movdqa	%xmm0,160-128(%rax)
	paddd	%xmm0,%xmm14
	movd	-16(%r8),%xmm2
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm11,%xmm7

	por	%xmm9,%xmm8
	movd	-16(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
.byte	102,15,56,0,205
	movd	-16(%r10),%xmm8
	por	%xmm7,%xmm11
	movd	-16(%r11),%xmm7
	punpckldq	%xmm8,%xmm2
	movdqa	%xmm14,%xmm8
	paddd	%xmm15,%xmm13
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm10,%xmm7
	movdqa	%xmm10,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm12,%xmm7
	pand	%xmm11,%xmm6
	punpckldq	%xmm9,%xmm2
	movdqa	%xmm14,%xmm9

	movdqa	%xmm1,176-128(%rax)
	paddd	%xmm1,%xmm13
	movd	-12(%r8),%xmm3
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm10,%xmm7

	por	%xmm9,%xmm8
	movd	-12(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
.byte	102,15,56,0,213
	movd	-12(%r10),%xmm8
	por	%xmm7,%xmm10
	movd	-12(%r11),%xmm7
	punpckldq	%xmm8,%xmm3
	movdqa	%xmm13,%xmm8
	paddd	%xmm15,%xmm12
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm14,%xmm7
	movdqa	%xmm14,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm11,%xmm7
	pand	%xmm10,%xmm6
	punpckldq	%xmm9,%xmm3
	movdqa	%xmm13,%xmm9

	movdqa	%xmm2,192-128(%rax)
	paddd	%xmm2,%xmm12
	movd	-8(%r8),%xmm4
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm14,%xmm7

	por	%xmm9,%xmm8
	movd	-8(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
.byte	102,15,56,0,221
	movd	-8(%r10),%xmm8
	por	%xmm7,%xmm14
	movd	-8(%r11),%xmm7
	punpckldq	%xmm8,%xmm4
	movdqa	%xmm12,%xmm8
	paddd	%xmm15,%xmm11
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm13,%xmm7
	movdqa	%xmm13,%xmm6
	pslld	$5,%xmm8
	pandn	%xmm10,%xmm7
	pand	%xmm14,%xmm6
	punpckldq	%xmm9,%xmm4
	movdqa	%xmm12,%xmm9

	movdqa	%xmm3,208-128(%rax)
	paddd	%xmm3,%xmm11
	movd	-4(%r8),%xmm0
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm13,%xmm7

	por	%xmm9,%xmm8
	movd	-4(%r9),%xmm9
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
.byte	102,15,56,0,229
	movd	-4(%r10),%xmm8
	por	%xmm7,%xmm13
	movdqa	0-128(%rax),%xmm1
	movd	-4(%r11),%xmm7
	punpckldq	%xmm8,%xmm0
	movdqa	%xmm11,%xmm8
	paddd	%xmm15,%xmm10
	punpckldq	%xmm7,%xmm9
	movdqa	%xmm12,%xmm7
	movdqa	%xmm12,%xmm6
	pslld	$5,%xmm8
	prefetcht0	63(%r8)
	pandn	%xmm14,%xmm7
	pand	%xmm13,%xmm6
	punpckldq	%xmm9,%xmm0
	movdqa	%xmm11,%xmm9

	movdqa	%xmm4,224-128(%rax)
	paddd	%xmm4,%xmm10
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6
	movdqa	%xmm12,%xmm7
	prefetcht0	63(%r9)

	por	%xmm9,%xmm8
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm10
	prefetcht0	63(%r10)

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
.byte	102,15,56,0,197
	prefetcht0	63(%r11)
	por	%xmm7,%xmm12
	movdqa	16-128(%rax),%xmm2
	pxor	%xmm3,%xmm1
	movdqa	32-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	pxor	128-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	movdqa	%xmm11,%xmm7
	pslld	$5,%xmm8
	pxor	%xmm3,%xmm1
	movdqa	%xmm11,%xmm6
	pandn	%xmm13,%xmm7
	movdqa	%xmm1,%xmm5
	pand	%xmm12,%xmm6
	movdqa	%xmm10,%xmm9
	psrld	$31,%xmm5
	paddd	%xmm1,%xmm1

	movdqa	%xmm0,240-128(%rax)
	paddd	%xmm0,%xmm14
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6

	movdqa	%xmm11,%xmm7
	por	%xmm9,%xmm8
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	48-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	pxor	144-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	movdqa	%xmm10,%xmm7
	pslld	$5,%xmm8
	pxor	%xmm4,%xmm2
	movdqa	%xmm10,%xmm6
	pandn	%xmm12,%xmm7
	movdqa	%xmm2,%xmm5
	pand	%xmm11,%xmm6
	movdqa	%xmm14,%xmm9
	psrld	$31,%xmm5
	paddd	%xmm2,%xmm2

	movdqa	%xmm1,0-128(%rax)
	paddd	%xmm1,%xmm13
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6

	movdqa	%xmm10,%xmm7
	por	%xmm9,%xmm8
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	64-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	pxor	160-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	movdqa	%xmm14,%xmm7
	pslld	$5,%xmm8
	pxor	%xmm0,%xmm3
	movdqa	%xmm14,%xmm6
	pandn	%xmm11,%xmm7
	movdqa	%xmm3,%xmm5
	pand	%xmm10,%xmm6
	movdqa	%xmm13,%xmm9
	psrld	$31,%xmm5
	paddd	%xmm3,%xmm3

	movdqa	%xmm2,16-128(%rax)
	paddd	%xmm2,%xmm12
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6

	movdqa	%xmm14,%xmm7
	por	%xmm9,%xmm8
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	80-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	pxor	176-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	movdqa	%xmm13,%xmm7
	pslld	$5,%xmm8
	pxor	%xmm1,%xmm4
	movdqa	%xmm13,%xmm6
	pandn	%xmm10,%xmm7
	movdqa	%xmm4,%xmm5
	pand	%xmm14,%xmm6
	movdqa	%xmm12,%xmm9
	psrld	$31,%xmm5
	paddd	%xmm4,%xmm4

	movdqa	%xmm3,32-128(%rax)
	paddd	%xmm3,%xmm11
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6

	movdqa	%xmm13,%xmm7
	por	%xmm9,%xmm8
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	96-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	pxor	192-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	movdqa	%xmm12,%xmm7
	pslld	$5,%xmm8
	pxor	%xmm2,%xmm0
	movdqa	%xmm12,%xmm6
	pandn	%xmm14,%xmm7
	movdqa	%xmm0,%xmm5
	pand	%xmm13,%xmm6
	movdqa	%xmm11,%xmm9
	psrld	$31,%xmm5
	paddd	%xmm0,%xmm0

	movdqa	%xmm4,48-128(%rax)
	paddd	%xmm4,%xmm10
	psrld	$27,%xmm9
	pxor	%xmm7,%xmm6

	movdqa	%xmm12,%xmm7
	por	%xmm9,%xmm8
	pslld	$30,%xmm7
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	movdqa	0(%rbp),%xmm15
	pxor	%xmm3,%xmm1
	movdqa	112-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	208-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	movdqa	%xmm0,64-128(%rax)
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	128-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	224-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	movdqa	%xmm1,80-128(%rax)
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	144-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	240-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	movdqa	%xmm2,96-128(%rax)
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	160-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	0-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	movdqa	%xmm3,112-128(%rax)
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	176-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm6
	pxor	16-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	movdqa	%xmm4,128-128(%rax)
	paddd	%xmm4,%xmm10
	pxor	%xmm2,%xmm0
	psrld	$27,%xmm9
	pxor	%xmm13,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm0,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm10
	paddd	%xmm0,%xmm0

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	192-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	32-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	movdqa	%xmm0,144-128(%rax)
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	208-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	48-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	movdqa	%xmm1,160-128(%rax)
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	224-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	64-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	movdqa	%xmm2,176-128(%rax)
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	240-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	80-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	movdqa	%xmm3,192-128(%rax)
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	0-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm6
	pxor	96-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	movdqa	%xmm4,208-128(%rax)
	paddd	%xmm4,%xmm10
	pxor	%xmm2,%xmm0
	psrld	$27,%xmm9
	pxor	%xmm13,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm0,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm10
	paddd	%xmm0,%xmm0

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	16-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	112-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	movdqa	%xmm0,224-128(%rax)
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	32-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	128-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	movdqa	%xmm1,240-128(%rax)
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	48-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	144-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	movdqa	%xmm2,0-128(%rax)
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	64-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	160-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	movdqa	%xmm3,16-128(%rax)
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	80-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm6
	pxor	176-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	movdqa	%xmm4,32-128(%rax)
	paddd	%xmm4,%xmm10
	pxor	%xmm2,%xmm0
	psrld	$27,%xmm9
	pxor	%xmm13,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm0,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm10
	paddd	%xmm0,%xmm0

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	96-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	192-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	movdqa	%xmm0,48-128(%rax)
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	112-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	208-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	movdqa	%xmm1,64-128(%rax)
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	128-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	224-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	movdqa	%xmm2,80-128(%rax)
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	144-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	240-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	movdqa	%xmm3,96-128(%rax)
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	160-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm6
	pxor	0-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	movdqa	%xmm4,112-128(%rax)
	paddd	%xmm4,%xmm10
	pxor	%xmm2,%xmm0
	psrld	$27,%xmm9
	pxor	%xmm13,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm0,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm10
	paddd	%xmm0,%xmm0

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	movdqa	32(%rbp),%xmm15
	pxor	%xmm3,%xmm1
	movdqa	176-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm7
	pxor	16-128(%rax),%xmm1
	pxor	%xmm3,%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	movdqa	%xmm10,%xmm9
	pand	%xmm12,%xmm7

	movdqa	%xmm13,%xmm6
	movdqa	%xmm1,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm14
	pxor	%xmm12,%xmm6

	movdqa	%xmm0,128-128(%rax)
	paddd	%xmm0,%xmm14
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm11,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm1,%xmm1
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	192-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm7
	pxor	32-128(%rax),%xmm2
	pxor	%xmm4,%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	movdqa	%xmm14,%xmm9
	pand	%xmm11,%xmm7

	movdqa	%xmm12,%xmm6
	movdqa	%xmm2,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm13
	pxor	%xmm11,%xmm6

	movdqa	%xmm1,144-128(%rax)
	paddd	%xmm1,%xmm13
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm10,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm2,%xmm2
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	208-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm7
	pxor	48-128(%rax),%xmm3
	pxor	%xmm0,%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	movdqa	%xmm13,%xmm9
	pand	%xmm10,%xmm7

	movdqa	%xmm11,%xmm6
	movdqa	%xmm3,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm12
	pxor	%xmm10,%xmm6

	movdqa	%xmm2,160-128(%rax)
	paddd	%xmm2,%xmm12
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm14,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm3,%xmm3
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	224-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm7
	pxor	64-128(%rax),%xmm4
	pxor	%xmm1,%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	movdqa	%xmm12,%xmm9
	pand	%xmm14,%xmm7

	movdqa	%xmm10,%xmm6
	movdqa	%xmm4,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm11
	pxor	%xmm14,%xmm6

	movdqa	%xmm3,176-128(%rax)
	paddd	%xmm3,%xmm11
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm13,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm4,%xmm4
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	240-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm7
	pxor	80-128(%rax),%xmm0
	pxor	%xmm2,%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	movdqa	%xmm11,%xmm9
	pand	%xmm13,%xmm7

	movdqa	%xmm14,%xmm6
	movdqa	%xmm0,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm10
	pxor	%xmm13,%xmm6

	movdqa	%xmm4,192-128(%rax)
	paddd	%xmm4,%xmm10
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm12,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm0,%xmm0
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	0-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm7
	pxor	96-128(%rax),%xmm1
	pxor	%xmm3,%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	movdqa	%xmm10,%xmm9
	pand	%xmm12,%xmm7

	movdqa	%xmm13,%xmm6
	movdqa	%xmm1,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm14
	pxor	%xmm12,%xmm6

	movdqa	%xmm0,208-128(%rax)
	paddd	%xmm0,%xmm14
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm11,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm1,%xmm1
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	16-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm7
	pxor	112-128(%rax),%xmm2
	pxor	%xmm4,%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	movdqa	%xmm14,%xmm9
	pand	%xmm11,%xmm7

	movdqa	%xmm12,%xmm6
	movdqa	%xmm2,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm13
	pxor	%xmm11,%xmm6

	movdqa	%xmm1,224-128(%rax)
	paddd	%xmm1,%xmm13
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm10,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm2,%xmm2
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	32-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm7
	pxor	128-128(%rax),%xmm3
	pxor	%xmm0,%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	movdqa	%xmm13,%xmm9
	pand	%xmm10,%xmm7

	movdqa	%xmm11,%xmm6
	movdqa	%xmm3,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm12
	pxor	%xmm10,%xmm6

	movdqa	%xmm2,240-128(%rax)
	paddd	%xmm2,%xmm12
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm14,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm3,%xmm3
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	48-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm7
	pxor	144-128(%rax),%xmm4
	pxor	%xmm1,%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	movdqa	%xmm12,%xmm9
	pand	%xmm14,%xmm7

	movdqa	%xmm10,%xmm6
	movdqa	%xmm4,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm11
	pxor	%xmm14,%xmm6

	movdqa	%xmm3,0-128(%rax)
	paddd	%xmm3,%xmm11
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm13,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm4,%xmm4
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	64-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm7
	pxor	160-128(%rax),%xmm0
	pxor	%xmm2,%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	movdqa	%xmm11,%xmm9
	pand	%xmm13,%xmm7

	movdqa	%xmm14,%xmm6
	movdqa	%xmm0,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm10
	pxor	%xmm13,%xmm6

	movdqa	%xmm4,16-128(%rax)
	paddd	%xmm4,%xmm10
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm12,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm0,%xmm0
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	80-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm7
	pxor	176-128(%rax),%xmm1
	pxor	%xmm3,%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	movdqa	%xmm10,%xmm9
	pand	%xmm12,%xmm7

	movdqa	%xmm13,%xmm6
	movdqa	%xmm1,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm14
	pxor	%xmm12,%xmm6

	movdqa	%xmm0,32-128(%rax)
	paddd	%xmm0,%xmm14
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm11,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm1,%xmm1
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	96-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm7
	pxor	192-128(%rax),%xmm2
	pxor	%xmm4,%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	movdqa	%xmm14,%xmm9
	pand	%xmm11,%xmm7

	movdqa	%xmm12,%xmm6
	movdqa	%xmm2,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm13
	pxor	%xmm11,%xmm6

	movdqa	%xmm1,48-128(%rax)
	paddd	%xmm1,%xmm13
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm10,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm2,%xmm2
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	112-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm7
	pxor	208-128(%rax),%xmm3
	pxor	%xmm0,%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	movdqa	%xmm13,%xmm9
	pand	%xmm10,%xmm7

	movdqa	%xmm11,%xmm6
	movdqa	%xmm3,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm12
	pxor	%xmm10,%xmm6

	movdqa	%xmm2,64-128(%rax)
	paddd	%xmm2,%xmm12
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm14,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm3,%xmm3
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	128-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm7
	pxor	224-128(%rax),%xmm4
	pxor	%xmm1,%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	movdqa	%xmm12,%xmm9
	pand	%xmm14,%xmm7

	movdqa	%xmm10,%xmm6
	movdqa	%xmm4,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm11
	pxor	%xmm14,%xmm6

	movdqa	%xmm3,80-128(%rax)
	paddd	%xmm3,%xmm11
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm13,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm4,%xmm4
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	144-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm7
	pxor	240-128(%rax),%xmm0
	pxor	%xmm2,%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	movdqa	%xmm11,%xmm9
	pand	%xmm13,%xmm7

	movdqa	%xmm14,%xmm6
	movdqa	%xmm0,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm10
	pxor	%xmm13,%xmm6

	movdqa	%xmm4,96-128(%rax)
	paddd	%xmm4,%xmm10
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm12,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm0,%xmm0
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	160-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm7
	pxor	0-128(%rax),%xmm1
	pxor	%xmm3,%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	movdqa	%xmm10,%xmm9
	pand	%xmm12,%xmm7

	movdqa	%xmm13,%xmm6
	movdqa	%xmm1,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm14
	pxor	%xmm12,%xmm6

	movdqa	%xmm0,112-128(%rax)
	paddd	%xmm0,%xmm14
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm11,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm1,%xmm1
	paddd	%xmm6,%xmm14

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	176-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm7
	pxor	16-128(%rax),%xmm2
	pxor	%xmm4,%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	movdqa	%xmm14,%xmm9
	pand	%xmm11,%xmm7

	movdqa	%xmm12,%xmm6
	movdqa	%xmm2,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm13
	pxor	%xmm11,%xmm6

	movdqa	%xmm1,128-128(%rax)
	paddd	%xmm1,%xmm13
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm10,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm2,%xmm2
	paddd	%xmm6,%xmm13

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	192-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm7
	pxor	32-128(%rax),%xmm3
	pxor	%xmm0,%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	movdqa	%xmm13,%xmm9
	pand	%xmm10,%xmm7

	movdqa	%xmm11,%xmm6
	movdqa	%xmm3,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm12
	pxor	%xmm10,%xmm6

	movdqa	%xmm2,144-128(%rax)
	paddd	%xmm2,%xmm12
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm14,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm3,%xmm3
	paddd	%xmm6,%xmm12

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	208-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm7
	pxor	48-128(%rax),%xmm4
	pxor	%xmm1,%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	movdqa	%xmm12,%xmm9
	pand	%xmm14,%xmm7

	movdqa	%xmm10,%xmm6
	movdqa	%xmm4,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm11
	pxor	%xmm14,%xmm6

	movdqa	%xmm3,160-128(%rax)
	paddd	%xmm3,%xmm11
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm13,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm4,%xmm4
	paddd	%xmm6,%xmm11

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	224-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm7
	pxor	64-128(%rax),%xmm0
	pxor	%xmm2,%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	movdqa	%xmm11,%xmm9
	pand	%xmm13,%xmm7

	movdqa	%xmm14,%xmm6
	movdqa	%xmm0,%xmm5
	psrld	$27,%xmm9
	paddd	%xmm7,%xmm10
	pxor	%xmm13,%xmm6

	movdqa	%xmm4,176-128(%rax)
	paddd	%xmm4,%xmm10
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	pand	%xmm12,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	paddd	%xmm0,%xmm0
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	movdqa	64(%rbp),%xmm15
	pxor	%xmm3,%xmm1
	movdqa	240-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	80-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	movdqa	%xmm0,192-128(%rax)
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	0-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	96-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	movdqa	%xmm1,208-128(%rax)
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	16-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	112-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	movdqa	%xmm2,224-128(%rax)
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	32-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	128-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	movdqa	%xmm3,240-128(%rax)
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	48-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm6
	pxor	144-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	movdqa	%xmm4,0-128(%rax)
	paddd	%xmm4,%xmm10
	pxor	%xmm2,%xmm0
	psrld	$27,%xmm9
	pxor	%xmm13,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm0,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm10
	paddd	%xmm0,%xmm0

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	64-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	160-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	movdqa	%xmm0,16-128(%rax)
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	80-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	176-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	movdqa	%xmm1,32-128(%rax)
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	96-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	192-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	movdqa	%xmm2,48-128(%rax)
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	112-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	208-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	movdqa	%xmm3,64-128(%rax)
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	128-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm6
	pxor	224-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	movdqa	%xmm4,80-128(%rax)
	paddd	%xmm4,%xmm10
	pxor	%xmm2,%xmm0
	psrld	$27,%xmm9
	pxor	%xmm13,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm0,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm10
	paddd	%xmm0,%xmm0

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	144-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	240-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	movdqa	%xmm0,96-128(%rax)
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	160-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	0-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	movdqa	%xmm1,112-128(%rax)
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	176-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	16-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	192-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	32-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	pxor	%xmm2,%xmm0
	movdqa	208-128(%rax),%xmm2

	movdqa	%xmm11,%xmm8
	movdqa	%xmm14,%xmm6
	pxor	48-128(%rax),%xmm0
	paddd	%xmm15,%xmm10
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	paddd	%xmm4,%xmm10
	pxor	%xmm2,%xmm0
	psrld	$27,%xmm9
	pxor	%xmm13,%xmm6
	movdqa	%xmm12,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm0,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm10
	paddd	%xmm0,%xmm0

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm5,%xmm0
	por	%xmm7,%xmm12
	pxor	%xmm3,%xmm1
	movdqa	224-128(%rax),%xmm3

	movdqa	%xmm10,%xmm8
	movdqa	%xmm13,%xmm6
	pxor	64-128(%rax),%xmm1
	paddd	%xmm15,%xmm14
	pslld	$5,%xmm8
	pxor	%xmm11,%xmm6

	movdqa	%xmm10,%xmm9
	paddd	%xmm0,%xmm14
	pxor	%xmm3,%xmm1
	psrld	$27,%xmm9
	pxor	%xmm12,%xmm6
	movdqa	%xmm11,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm1,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm14
	paddd	%xmm1,%xmm1

	psrld	$2,%xmm11
	paddd	%xmm8,%xmm14
	por	%xmm5,%xmm1
	por	%xmm7,%xmm11
	pxor	%xmm4,%xmm2
	movdqa	240-128(%rax),%xmm4

	movdqa	%xmm14,%xmm8
	movdqa	%xmm12,%xmm6
	pxor	80-128(%rax),%xmm2
	paddd	%xmm15,%xmm13
	pslld	$5,%xmm8
	pxor	%xmm10,%xmm6

	movdqa	%xmm14,%xmm9
	paddd	%xmm1,%xmm13
	pxor	%xmm4,%xmm2
	psrld	$27,%xmm9
	pxor	%xmm11,%xmm6
	movdqa	%xmm10,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm2,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm13
	paddd	%xmm2,%xmm2

	psrld	$2,%xmm10
	paddd	%xmm8,%xmm13
	por	%xmm5,%xmm2
	por	%xmm7,%xmm10
	pxor	%xmm0,%xmm3
	movdqa	0-128(%rax),%xmm0

	movdqa	%xmm13,%xmm8
	movdqa	%xmm11,%xmm6
	pxor	96-128(%rax),%xmm3
	paddd	%xmm15,%xmm12
	pslld	$5,%xmm8
	pxor	%xmm14,%xmm6

	movdqa	%xmm13,%xmm9
	paddd	%xmm2,%xmm12
	pxor	%xmm0,%xmm3
	psrld	$27,%xmm9
	pxor	%xmm10,%xmm6
	movdqa	%xmm14,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm3,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm12
	paddd	%xmm3,%xmm3

	psrld	$2,%xmm14
	paddd	%xmm8,%xmm12
	por	%xmm5,%xmm3
	por	%xmm7,%xmm14
	pxor	%xmm1,%xmm4
	movdqa	16-128(%rax),%xmm1

	movdqa	%xmm12,%xmm8
	movdqa	%xmm10,%xmm6
	pxor	112-128(%rax),%xmm4
	paddd	%xmm15,%xmm11
	pslld	$5,%xmm8
	pxor	%xmm13,%xmm6

	movdqa	%xmm12,%xmm9
	paddd	%xmm3,%xmm11
	pxor	%xmm1,%xmm4
	psrld	$27,%xmm9
	pxor	%xmm14,%xmm6
	movdqa	%xmm13,%xmm7

	pslld	$30,%xmm7
	movdqa	%xmm4,%xmm5
	por	%xmm9,%xmm8
	psrld	$31,%xmm5
	paddd	%xmm6,%xmm11
	paddd	%xmm4,%xmm4

	psrld	$2,%xmm13
	paddd	%xmm8,%xmm11
	por	%xmm5,%xmm4
	por	%xmm7,%xmm13
	movdqa	%xmm11,%xmm8
	paddd	%xmm15,%xmm10
	movdqa	%xmm14,%xmm6
	pslld	$5,%xmm8
	pxor	%xmm12,%xmm6

	movdqa	%xmm11,%xmm9
	paddd	%xmm4,%xmm10
	psrld	$27,%xmm9
	movdqa	%xmm12,%xmm7
	pxor	%xmm13,%xmm6

	pslld	$30,%xmm7
	por	%xmm9,%xmm8
	paddd	%xmm6,%xmm10

	psrld	$2,%xmm12
	paddd	%xmm8,%xmm10
	por	%xmm7,%xmm12
	movdqa	(%rbx),%xmm0
	movl	$1,%ecx
	cmpl	0(%rbx),%ecx
	pxor	%xmm8,%xmm8
	cmovgeq	%rbp,%r8
	cmpl	4(%rbx),%ecx
	movdqa	%xmm0,%xmm1
	cmovgeq	%rbp,%r9
	cmpl	8(%rbx),%ecx
	pcmpgtd	%xmm8,%xmm1
	cmovgeq	%rbp,%r10
	cmpl	12(%rbx),%ecx
	paddd	%xmm1,%xmm0
	cmovgeq	%rbp,%r11

	movdqu	0(%rdi),%xmm6
	pand	%xmm1,%xmm10
	movdqu	32(%rdi),%xmm7
	pand	%xmm1,%xmm11
	paddd	%xmm6,%xmm10
	movdqu	64(%rdi),%xmm8
	pand	%xmm1,%xmm12
	paddd	%xmm7,%xmm11
	movdqu	96(%rdi),%xmm9
	pand	%xmm1,%xmm13
	paddd	%xmm8,%xmm12
	movdqu	128(%rdi),%xmm5
	pand	%xmm1,%xmm14
	movdqu	%xmm10,0(%rdi)
	paddd	%xmm9,%xmm13
	movdqu	%xmm11,32(%rdi)
	paddd	%xmm5,%xmm14
	movdqu	%xmm12,64(%rdi)
	movdqu	%xmm13,96(%rdi)
	movdqu	%xmm14,128(%rdi)

	movdqa	%xmm0,(%rbx)
	movdqa	96(%rbp),%xmm5
	movdqa	-32(%rbp),%xmm15
	decl	%edx
	jnz	.Loop

	movl	280(%rsp),%edx
	leaq	16(%rdi),%rdi
	leaq	64(%rsi),%rsi
	decl	%edx
	jnz	.Loop_grande

.Ldone:
	movq	272(%rsp),%rax
	movq	-16(%rax),%rbp
	movq	-8(%rax),%rbx
	leaq	(%rax),%rsp
.Lepilogue:
	.byte	0xf3,0xc3
.size	sha1_multi_block,.-sha1_multi_block
.type	sha1_multi_block_shaext,@function
.align	32
sha1_multi_block_shaext:
_shaext_shortcut:
	movq	%rsp,%rax
	pushq	%rbx
	pushq	%rbp
	subq	$288,%rsp
	shll	$1,%edx
	andq	$-256,%rsp
	leaq	64(%rdi),%rdi
	movq	%rax,272(%rsp)
.Lbody_shaext:
	leaq	256(%rsp),%rbx
	movdqa	K_XX_XX+128(%rip),%xmm3

.Loop_grande_shaext:
	movl	%edx,280(%rsp)
	xorl	%edx,%edx
	movq	0(%rsi),%r8
	movl	8(%rsi),%ecx
	cmpl	%edx,%ecx
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movl	%ecx,0(%rbx)
	cmovleq	%rsp,%r8
	movq	16(%rsi),%r9
	movl	24(%rsi),%ecx
	cmpl	%edx,%ecx
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movl	%ecx,4(%rbx)
	cmovleq	%rsp,%r9
	testl	%edx,%edx
	jz	.Ldone_shaext

	movq	0-64(%rdi),%xmm0
	movq	32-64(%rdi),%xmm4
	movq	64-64(%rdi),%xmm5
	movq	96-64(%rdi),%xmm6
	movq	128-64(%rdi),%xmm7

	punpckldq	%xmm4,%xmm0
	punpckldq	%xmm6,%xmm5

	movdqa	%xmm0,%xmm8
	punpcklqdq	%xmm5,%xmm0
	punpckhqdq	%xmm5,%xmm8

	pshufd	$63,%xmm7,%xmm1
	pshufd	$127,%xmm7,%xmm9
	pshufd	$27,%xmm0,%xmm0
	pshufd	$27,%xmm8,%xmm8
	jmp	.Loop_shaext

.align	32
.Loop_shaext:
	movdqu	0(%r8),%xmm4
	movdqu	0(%r9),%xmm11
	movdqu	16(%r8),%xmm5
	movdqu	16(%r9),%xmm12
	movdqu	32(%r8),%xmm6
.byte	102,15,56,0,227
	movdqu	32(%r9),%xmm13
.byte	102,68,15,56,0,219
	movdqu	48(%r8),%xmm7
	leaq	64(%r8),%r8
.byte	102,15,56,0,235
	movdqu	48(%r9),%xmm14
	leaq	64(%r9),%r9
.byte	102,68,15,56,0,227

	movdqa	%xmm1,80(%rsp)
	paddd	%xmm4,%xmm1
	movdqa	%xmm9,112(%rsp)
	paddd	%xmm11,%xmm9
	movdqa	%xmm0,64(%rsp)
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,96(%rsp)
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,0
.byte	15,56,200,213
.byte	69,15,58,204,193,0
.byte	69,15,56,200,212
.byte	102,15,56,0,243
	prefetcht0	127(%r8)
.byte	15,56,201,229
.byte	102,68,15,56,0,235
	prefetcht0	127(%r9)
.byte	69,15,56,201,220

.byte	102,15,56,0,251
	movdqa	%xmm0,%xmm1
.byte	102,68,15,56,0,243
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,0
.byte	15,56,200,206
.byte	69,15,58,204,194,0
.byte	69,15,56,200,205
	pxor	%xmm6,%xmm4
.byte	15,56,201,238
	pxor	%xmm13,%xmm11
.byte	69,15,56,201,229
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,0
.byte	15,56,200,215
.byte	69,15,58,204,193,0
.byte	69,15,56,200,214
.byte	15,56,202,231
.byte	69,15,56,202,222
	pxor	%xmm7,%xmm5
.byte	15,56,201,247
	pxor	%xmm14,%xmm12
.byte	69,15,56,201,238
	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,0
.byte	15,56,200,204
.byte	69,15,58,204,194,0
.byte	69,15,56,200,203
.byte	15,56,202,236
.byte	69,15,56,202,227
	pxor	%xmm4,%xmm6
.byte	15,56,201,252
	pxor	%xmm11,%xmm13
.byte	69,15,56,201,243
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,0
.byte	15,56,200,213
.byte	69,15,58,204,193,0
.byte	69,15,56,200,212
.byte	15,56,202,245
.byte	69,15,56,202,236
	pxor	%xmm5,%xmm7
.byte	15,56,201,229
	pxor	%xmm12,%xmm14
.byte	69,15,56,201,220
	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,1
.byte	15,56,200,206
.byte	69,15,58,204,194,1
.byte	69,15,56,200,205
.byte	15,56,202,254
.byte	69,15,56,202,245
	pxor	%xmm6,%xmm4
.byte	15,56,201,238
	pxor	%xmm13,%xmm11
.byte	69,15,56,201,229
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,1
.byte	15,56,200,215
.byte	69,15,58,204,193,1
.byte	69,15,56,200,214
.byte	15,56,202,231
.byte	69,15,56,202,222
	pxor	%xmm7,%xmm5
.byte	15,56,201,247
	pxor	%xmm14,%xmm12
.byte	69,15,56,201,238
	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,1
.byte	15,56,200,204
.byte	69,15,58,204,194,1
.byte	69,15,56,200,203
.byte	15,56,202,236
.byte	69,15,56,202,227
	pxor	%xmm4,%xmm6
.byte	15,56,201,252
	pxor	%xmm11,%xmm13
.byte	69,15,56,201,243
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,1
.byte	15,56,200,213
.byte	69,15,58,204,193,1
.byte	69,15,56,200,212
.byte	15,56,202,245
.byte	69,15,56,202,236
	pxor	%xmm5,%xmm7
.byte	15,56,201,229
	pxor	%xmm12,%xmm14
.byte	69,15,56,201,220
	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,1
.byte	15,56,200,206
.byte	69,15,58,204,194,1
.byte	69,15,56,200,205
.byte	15,56,202,254
.byte	69,15,56,202,245
	pxor	%xmm6,%xmm4
.byte	15,56,201,238
	pxor	%xmm13,%xmm11
.byte	69,15,56,201,229
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,2
.byte	15,56,200,215
.byte	69,15,58,204,193,2
.byte	69,15,56,200,214
.byte	15,56,202,231
.byte	69,15,56,202,222
	pxor	%xmm7,%xmm5
.byte	15,56,201,247
	pxor	%xmm14,%xmm12
.byte	69,15,56,201,238
	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,2
.byte	15,56,200,204
.byte	69,15,58,204,194,2
.byte	69,15,56,200,203
.byte	15,56,202,236
.byte	69,15,56,202,227
	pxor	%xmm4,%xmm6
.byte	15,56,201,252
	pxor	%xmm11,%xmm13
.byte	69,15,56,201,243
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,2
.byte	15,56,200,213
.byte	69,15,58,204,193,2
.byte	69,15,56,200,212
.byte	15,56,202,245
.byte	69,15,56,202,236
	pxor	%xmm5,%xmm7
.byte	15,56,201,229
	pxor	%xmm12,%xmm14
.byte	69,15,56,201,220
	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,2
.byte	15,56,200,206
.byte	69,15,58,204,194,2
.byte	69,15,56,200,205
.byte	15,56,202,254
.byte	69,15,56,202,245
	pxor	%xmm6,%xmm4
.byte	15,56,201,238
	pxor	%xmm13,%xmm11
.byte	69,15,56,201,229
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,2
.byte	15,56,200,215
.byte	69,15,58,204,193,2
.byte	69,15,56,200,214
.byte	15,56,202,231
.byte	69,15,56,202,222
	pxor	%xmm7,%xmm5
.byte	15,56,201,247
	pxor	%xmm14,%xmm12
.byte	69,15,56,201,238
	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,3
.byte	15,56,200,204
.byte	69,15,58,204,194,3
.byte	69,15,56,200,203
.byte	15,56,202,236
.byte	69,15,56,202,227
	pxor	%xmm4,%xmm6
.byte	15,56,201,252
	pxor	%xmm11,%xmm13
.byte	69,15,56,201,243
	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,3
.byte	15,56,200,213
.byte	69,15,58,204,193,3
.byte	69,15,56,200,212
.byte	15,56,202,245
.byte	69,15,56,202,236
	pxor	%xmm5,%xmm7
	pxor	%xmm12,%xmm14

	movl	$1,%ecx
	pxor	%xmm4,%xmm4
	cmpl	0(%rbx),%ecx
	cmovgeq	%rsp,%r8

	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,3
.byte	15,56,200,206
.byte	69,15,58,204,194,3
.byte	69,15,56,200,205
.byte	15,56,202,254
.byte	69,15,56,202,245

	cmpl	4(%rbx),%ecx
	cmovgeq	%rsp,%r9
	movq	(%rbx),%xmm6

	movdqa	%xmm0,%xmm2
	movdqa	%xmm8,%xmm10
.byte	15,58,204,193,3
.byte	15,56,200,215
.byte	69,15,58,204,193,3
.byte	69,15,56,200,214

	pshufd	$0x00,%xmm6,%xmm11
	pshufd	$0x55,%xmm6,%xmm12
	movdqa	%xmm6,%xmm7
	pcmpgtd	%xmm4,%xmm11
	pcmpgtd	%xmm4,%xmm12

	movdqa	%xmm0,%xmm1
	movdqa	%xmm8,%xmm9
.byte	15,58,204,194,3
.byte	15,56,200,204
.byte	69,15,58,204,194,3
.byte	68,15,56,200,204

	pcmpgtd	%xmm4,%xmm7
	pand	%xmm11,%xmm0
	pand	%xmm11,%xmm1
	pand	%xmm12,%xmm8
	pand	%xmm12,%xmm9
	paddd	%xmm7,%xmm6

	paddd	64(%rsp),%xmm0
	paddd	80(%rsp),%xmm1
	paddd	96(%rsp),%xmm8
	paddd	112(%rsp),%xmm9

	movq	%xmm6,(%rbx)
	decl	%edx
	jnz	.Loop_shaext

	movl	280(%rsp),%edx

	pshufd	$27,%xmm0,%xmm0
	pshufd	$27,%xmm8,%xmm8

	movdqa	%xmm0,%xmm6
	punpckldq	%xmm8,%xmm0
	punpckhdq	%xmm8,%xmm6
	punpckhdq	%xmm9,%xmm1
	movq	%xmm0,0-64(%rdi)
	psrldq	$8,%xmm0
	movq	%xmm6,64-64(%rdi)
	psrldq	$8,%xmm6
	movq	%xmm0,32-64(%rdi)
	psrldq	$8,%xmm1
	movq	%xmm6,96-64(%rdi)
	movq	%xmm1,128-64(%rdi)

	leaq	8(%rdi),%rdi
	leaq	32(%rsi),%rsi
	decl	%edx
	jnz	.Loop_grande_shaext

.Ldone_shaext:

	movq	-16(%rax),%rbp
	movq	-8(%rax),%rbx
	leaq	(%rax),%rsp
.Lepilogue_shaext:
	.byte	0xf3,0xc3
.size	sha1_multi_block_shaext,.-sha1_multi_block_shaext

.align	256
.long	0x5a827999,0x5a827999,0x5a827999,0x5a827999
.long	0x5a827999,0x5a827999,0x5a827999,0x5a827999
K_XX_XX:
.long	0x6ed9eba1,0x6ed9eba1,0x6ed9eba1,0x6ed9eba1
.long	0x6ed9eba1,0x6ed9eba1,0x6ed9eba1,0x6ed9eba1
.long	0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc
.long	0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc,0x8f1bbcdc
.long	0xca62c1d6,0xca62c1d6,0xca62c1d6,0xca62c1d6
.long	0xca62c1d6,0xca62c1d6,0xca62c1d6,0xca62c1d6
.long	0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f
.long	0x00010203,0x04050607,0x08090a0b,0x0c0d0e0f
.byte	0xf,0xe,0xd,0xc,0xb,0xa,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0
.byte	83,72,65,49,32,109,117,108,116,105,45,98,108,111,99,107,32,116,114,97,110,115,102,111,114,109,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
