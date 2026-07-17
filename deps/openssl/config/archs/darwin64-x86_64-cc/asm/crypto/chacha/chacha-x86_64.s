.text	



.section	__DATA,__const
.p2align	6
L$zero:
.long	0,0,0,0
L$one:
.long	1,0,0,0
L$inc:
.long	0,1,2,3
L$four:
.long	4,4,4,4
L$incy:
.long	0,2,4,6,1,3,5,7
L$eight:
.long	8,8,8,8,8,8,8,8
L$rot16:
.byte	0x2,0x3,0x0,0x1, 0x6,0x7,0x4,0x5, 0xa,0xb,0x8,0x9, 0xe,0xf,0xc,0xd
L$rot24:
.byte	0x3,0x0,0x1,0x2, 0x7,0x4,0x5,0x6, 0xb,0x8,0x9,0xa, 0xf,0xc,0xd,0xe
L$twoy:
.long	2,0,0,0, 2,0,0,0
.p2align	6
L$zeroz:
.long	0,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0
L$fourz:
.long	4,0,0,0, 4,0,0,0, 4,0,0,0, 4,0,0,0
L$incz:
.long	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
L$sixteen:
.long	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16
L$sigma:
.byte	101,120,112,97,110,100,32,51,50,45,98,121,116,101,32,107,0
.byte	67,104,97,67,104,97,50,48,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.text
.globl	_ChaCha20_ctr32

.p2align	6
_ChaCha20_ctr32:

	cmpq	$0,%rdx
	je	L$no_data
	movq	_OPENSSL_ia32cap_P+4(%rip),%r10
	btq	$48,%r10
	jc	L$ChaCha20_avx512
	testq	%r10,%r10
	js	L$ChaCha20_avx512vl
	testl	$512,%r10d
	jnz	L$ChaCha20_ssse3

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	subq	$64+24,%rsp

L$ctr32_body:


	movdqu	(%rcx),%xmm1
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	L$one(%rip),%xmm4


	movdqa	%xmm1,16(%rsp)
	movdqa	%xmm2,32(%rsp)
	movdqa	%xmm3,48(%rsp)
	movq	%rdx,%rbp
	jmp	L$oop_outer

.p2align	5
L$oop_outer:
	movl	$0x61707865,%eax
	movl	$0x3320646e,%ebx
	movl	$0x79622d32,%ecx
	movl	$0x6b206574,%edx
	movl	16(%rsp),%r8d
	movl	20(%rsp),%r9d
	movl	24(%rsp),%r10d
	movl	28(%rsp),%r11d
	movd	%xmm3,%r12d
	movl	52(%rsp),%r13d
	movl	56(%rsp),%r14d
	movl	60(%rsp),%r15d

	movq	%rbp,64+0(%rsp)
	movl	$10,%ebp
	movq	%rsi,64+8(%rsp)
.byte	102,72,15,126,214
	movq	%rdi,64+16(%rsp)
	movq	%rsi,%rdi
	shrq	$32,%rdi
	jmp	L$oop

.p2align	5
L$oop:
	addl	%r8d,%eax
	xorl	%eax,%r12d
	roll	$16,%r12d
	addl	%r9d,%ebx
	xorl	%ebx,%r13d
	roll	$16,%r13d
	addl	%r12d,%esi
	xorl	%esi,%r8d
	roll	$12,%r8d
	addl	%r13d,%edi
	xorl	%edi,%r9d
	roll	$12,%r9d
	addl	%r8d,%eax
	xorl	%eax,%r12d
	roll	$8,%r12d
	addl	%r9d,%ebx
	xorl	%ebx,%r13d
	roll	$8,%r13d
	addl	%r12d,%esi
	xorl	%esi,%r8d
	roll	$7,%r8d
	addl	%r13d,%edi
	xorl	%edi,%r9d
	roll	$7,%r9d
	movl	%esi,32(%rsp)
	movl	%edi,36(%rsp)
	movl	40(%rsp),%esi
	movl	44(%rsp),%edi
	addl	%r10d,%ecx
	xorl	%ecx,%r14d
	roll	$16,%r14d
	addl	%r11d,%edx
	xorl	%edx,%r15d
	roll	$16,%r15d
	addl	%r14d,%esi
	xorl	%esi,%r10d
	roll	$12,%r10d
	addl	%r15d,%edi
	xorl	%edi,%r11d
	roll	$12,%r11d
	addl	%r10d,%ecx
	xorl	%ecx,%r14d
	roll	$8,%r14d
	addl	%r11d,%edx
	xorl	%edx,%r15d
	roll	$8,%r15d
	addl	%r14d,%esi
	xorl	%esi,%r10d
	roll	$7,%r10d
	addl	%r15d,%edi
	xorl	%edi,%r11d
	roll	$7,%r11d
	addl	%r9d,%eax
	xorl	%eax,%r15d
	roll	$16,%r15d
	addl	%r10d,%ebx
	xorl	%ebx,%r12d
	roll	$16,%r12d
	addl	%r15d,%esi
	xorl	%esi,%r9d
	roll	$12,%r9d
	addl	%r12d,%edi
	xorl	%edi,%r10d
	roll	$12,%r10d
	addl	%r9d,%eax
	xorl	%eax,%r15d
	roll	$8,%r15d
	addl	%r10d,%ebx
	xorl	%ebx,%r12d
	roll	$8,%r12d
	addl	%r15d,%esi
	xorl	%esi,%r9d
	roll	$7,%r9d
	addl	%r12d,%edi
	xorl	%edi,%r10d
	roll	$7,%r10d
	movl	%esi,40(%rsp)
	movl	%edi,44(%rsp)
	movl	32(%rsp),%esi
	movl	36(%rsp),%edi
	addl	%r11d,%ecx
	xorl	%ecx,%r13d
	roll	$16,%r13d
	addl	%r8d,%edx
	xorl	%edx,%r14d
	roll	$16,%r14d
	addl	%r13d,%esi
	xorl	%esi,%r11d
	roll	$12,%r11d
	addl	%r14d,%edi
	xorl	%edi,%r8d
	roll	$12,%r8d
	addl	%r11d,%ecx
	xorl	%ecx,%r13d
	roll	$8,%r13d
	addl	%r8d,%edx
	xorl	%edx,%r14d
	roll	$8,%r14d
	addl	%r13d,%esi
	xorl	%esi,%r11d
	roll	$7,%r11d
	addl	%r14d,%edi
	xorl	%edi,%r8d
	roll	$7,%r8d
	decl	%ebp
	jnz	L$oop
	movl	%edi,36(%rsp)
	movl	%esi,32(%rsp)
	movq	64(%rsp),%rbp
	movdqa	%xmm2,%xmm1
	movq	64+8(%rsp),%rsi
	paddd	%xmm4,%xmm3
	movq	64+16(%rsp),%rdi

	addl	$0x61707865,%eax
	addl	$0x3320646e,%ebx
	addl	$0x79622d32,%ecx
	addl	$0x6b206574,%edx
	addl	16(%rsp),%r8d
	addl	20(%rsp),%r9d
	addl	24(%rsp),%r10d
	addl	28(%rsp),%r11d
	addl	48(%rsp),%r12d
	addl	52(%rsp),%r13d
	addl	56(%rsp),%r14d
	addl	60(%rsp),%r15d
	paddd	32(%rsp),%xmm1

	cmpq	$64,%rbp
	jb	L$tail

	xorl	0(%rsi),%eax
	xorl	4(%rsi),%ebx
	xorl	8(%rsi),%ecx
	xorl	12(%rsi),%edx
	xorl	16(%rsi),%r8d
	xorl	20(%rsi),%r9d
	xorl	24(%rsi),%r10d
	xorl	28(%rsi),%r11d
	movdqu	32(%rsi),%xmm0
	xorl	48(%rsi),%r12d
	xorl	52(%rsi),%r13d
	xorl	56(%rsi),%r14d
	xorl	60(%rsi),%r15d
	leaq	64(%rsi),%rsi
	pxor	%xmm1,%xmm0

	movdqa	%xmm2,32(%rsp)
	movd	%xmm3,48(%rsp)

	movl	%eax,0(%rdi)
	movl	%ebx,4(%rdi)
	movl	%ecx,8(%rdi)
	movl	%edx,12(%rdi)
	movl	%r8d,16(%rdi)
	movl	%r9d,20(%rdi)
	movl	%r10d,24(%rdi)
	movl	%r11d,28(%rdi)
	movdqu	%xmm0,32(%rdi)
	movl	%r12d,48(%rdi)
	movl	%r13d,52(%rdi)
	movl	%r14d,56(%rdi)
	movl	%r15d,60(%rdi)
	leaq	64(%rdi),%rdi

	subq	$64,%rbp
	jnz	L$oop_outer

	jmp	L$done

.p2align	4
L$tail:
	movl	%eax,0(%rsp)
	movl	%ebx,4(%rsp)
	xorq	%rbx,%rbx
	movl	%ecx,8(%rsp)
	movl	%edx,12(%rsp)
	movl	%r8d,16(%rsp)
	movl	%r9d,20(%rsp)
	movl	%r10d,24(%rsp)
	movl	%r11d,28(%rsp)
	movdqa	%xmm1,32(%rsp)
	movl	%r12d,48(%rsp)
	movl	%r13d,52(%rsp)
	movl	%r14d,56(%rsp)
	movl	%r15d,60(%rsp)

L$oop_tail:
	movzbl	(%rsi,%rbx,1),%eax
	movzbl	(%rsp,%rbx,1),%edx
	leaq	1(%rbx),%rbx
	xorl	%edx,%eax
	movb	%al,-1(%rdi,%rbx,1)
	decq	%rbp
	jnz	L$oop_tail

L$done:
	leaq	64+24+48(%rsp),%rsi

	movq	-48(%rsi),%r15

	movq	-40(%rsi),%r14

	movq	-32(%rsi),%r13

	movq	-24(%rsi),%r12

	movq	-16(%rsi),%rbp

	movq	-8(%rsi),%rbx

	leaq	(%rsi),%rsp

L$no_data:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_ssse3:

L$ChaCha20_ssse3:
	movq	%rsp,%r9

	testl	$2048,%r10d
	jnz	L$ChaCha20_4xop
	cmpq	$128,%rdx
	je	L$ChaCha20_128
	ja	L$ChaCha20_4x

L$do_sse3_after_all:
	subq	$64+8,%rsp
	movdqa	L$sigma(%rip),%xmm0
	movdqu	(%rcx),%xmm1
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	L$rot16(%rip),%xmm6
	movdqa	L$rot24(%rip),%xmm7

	movdqa	%xmm0,0(%rsp)
	movdqa	%xmm1,16(%rsp)
	movdqa	%xmm2,32(%rsp)
	movdqa	%xmm3,48(%rsp)
	movq	$10,%r8
	jmp	L$oop_ssse3

.p2align	5
L$oop_outer_ssse3:
	movdqa	L$one(%rip),%xmm3
	movdqa	0(%rsp),%xmm0
	movdqa	16(%rsp),%xmm1
	movdqa	32(%rsp),%xmm2
	paddd	48(%rsp),%xmm3
	movq	$10,%r8
	movdqa	%xmm3,48(%rsp)
	jmp	L$oop_ssse3

.p2align	5
L$oop_ssse3:
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,222
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,223
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$78,%xmm2,%xmm2
	pshufd	$57,%xmm1,%xmm1
	pshufd	$147,%xmm3,%xmm3
	nop
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,222
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$20,%xmm1
	pslld	$12,%xmm4
	por	%xmm4,%xmm1
	paddd	%xmm1,%xmm0
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,223
	paddd	%xmm3,%xmm2
	pxor	%xmm2,%xmm1
	movdqa	%xmm1,%xmm4
	psrld	$25,%xmm1
	pslld	$7,%xmm4
	por	%xmm4,%xmm1
	pshufd	$78,%xmm2,%xmm2
	pshufd	$147,%xmm1,%xmm1
	pshufd	$57,%xmm3,%xmm3
	decq	%r8
	jnz	L$oop_ssse3
	paddd	0(%rsp),%xmm0
	paddd	16(%rsp),%xmm1
	paddd	32(%rsp),%xmm2
	paddd	48(%rsp),%xmm3

	cmpq	$64,%rdx
	jb	L$tail_ssse3

	movdqu	0(%rsi),%xmm4
	movdqu	16(%rsi),%xmm5
	pxor	%xmm4,%xmm0
	movdqu	32(%rsi),%xmm4
	pxor	%xmm5,%xmm1
	movdqu	48(%rsi),%xmm5
	leaq	64(%rsi),%rsi
	pxor	%xmm4,%xmm2
	pxor	%xmm5,%xmm3

	movdqu	%xmm0,0(%rdi)
	movdqu	%xmm1,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm3,48(%rdi)
	leaq	64(%rdi),%rdi

	subq	$64,%rdx
	jnz	L$oop_outer_ssse3

	jmp	L$done_ssse3

.p2align	4
L$tail_ssse3:
	movdqa	%xmm0,0(%rsp)
	movdqa	%xmm1,16(%rsp)
	movdqa	%xmm2,32(%rsp)
	movdqa	%xmm3,48(%rsp)
	xorq	%r8,%r8

L$oop_tail_ssse3:
	movzbl	(%rsi,%r8,1),%eax
	movzbl	(%rsp,%r8,1),%ecx
	leaq	1(%r8),%r8
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r8,1)
	decq	%rdx
	jnz	L$oop_tail_ssse3

L$done_ssse3:
	leaq	(%r9),%rsp

L$ssse3_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_128:

L$ChaCha20_128:
	movq	%rsp,%r9

	subq	$64+8,%rsp
	movdqa	L$sigma(%rip),%xmm8
	movdqu	(%rcx),%xmm9
	movdqu	16(%rcx),%xmm2
	movdqu	(%r8),%xmm3
	movdqa	L$one(%rip),%xmm1
	movdqa	L$rot16(%rip),%xmm6
	movdqa	L$rot24(%rip),%xmm7

	movdqa	%xmm8,%xmm10
	movdqa	%xmm8,0(%rsp)
	movdqa	%xmm9,%xmm11
	movdqa	%xmm9,16(%rsp)
	movdqa	%xmm2,%xmm0
	movdqa	%xmm2,32(%rsp)
	paddd	%xmm3,%xmm1
	movdqa	%xmm3,48(%rsp)
	movq	$10,%r8
	jmp	L$oop_128

.p2align	5
L$oop_128:
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,222
.byte	102,15,56,0,206
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$20,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$12,%xmm4
	psrld	$20,%xmm11
	por	%xmm4,%xmm9
	pslld	$12,%xmm5
	por	%xmm5,%xmm11
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,223
.byte	102,15,56,0,207
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$25,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$7,%xmm4
	psrld	$25,%xmm11
	por	%xmm4,%xmm9
	pslld	$7,%xmm5
	por	%xmm5,%xmm11
	pshufd	$78,%xmm2,%xmm2
	pshufd	$57,%xmm9,%xmm9
	pshufd	$147,%xmm3,%xmm3
	pshufd	$78,%xmm0,%xmm0
	pshufd	$57,%xmm11,%xmm11
	pshufd	$147,%xmm1,%xmm1
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,222
.byte	102,15,56,0,206
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$20,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$12,%xmm4
	psrld	$20,%xmm11
	por	%xmm4,%xmm9
	pslld	$12,%xmm5
	por	%xmm5,%xmm11
	paddd	%xmm9,%xmm8
	pxor	%xmm8,%xmm3
	paddd	%xmm11,%xmm10
	pxor	%xmm10,%xmm1
.byte	102,15,56,0,223
.byte	102,15,56,0,207
	paddd	%xmm3,%xmm2
	paddd	%xmm1,%xmm0
	pxor	%xmm2,%xmm9
	pxor	%xmm0,%xmm11
	movdqa	%xmm9,%xmm4
	psrld	$25,%xmm9
	movdqa	%xmm11,%xmm5
	pslld	$7,%xmm4
	psrld	$25,%xmm11
	por	%xmm4,%xmm9
	pslld	$7,%xmm5
	por	%xmm5,%xmm11
	pshufd	$78,%xmm2,%xmm2
	pshufd	$147,%xmm9,%xmm9
	pshufd	$57,%xmm3,%xmm3
	pshufd	$78,%xmm0,%xmm0
	pshufd	$147,%xmm11,%xmm11
	pshufd	$57,%xmm1,%xmm1
	decq	%r8
	jnz	L$oop_128
	paddd	0(%rsp),%xmm8
	paddd	16(%rsp),%xmm9
	paddd	32(%rsp),%xmm2
	paddd	48(%rsp),%xmm3
	paddd	L$one(%rip),%xmm1
	paddd	0(%rsp),%xmm10
	paddd	16(%rsp),%xmm11
	paddd	32(%rsp),%xmm0
	paddd	48(%rsp),%xmm1

	movdqu	0(%rsi),%xmm4
	movdqu	16(%rsi),%xmm5
	pxor	%xmm4,%xmm8
	movdqu	32(%rsi),%xmm4
	pxor	%xmm5,%xmm9
	movdqu	48(%rsi),%xmm5
	pxor	%xmm4,%xmm2
	movdqu	64(%rsi),%xmm4
	pxor	%xmm5,%xmm3
	movdqu	80(%rsi),%xmm5
	pxor	%xmm4,%xmm10
	movdqu	96(%rsi),%xmm4
	pxor	%xmm5,%xmm11
	movdqu	112(%rsi),%xmm5
	pxor	%xmm4,%xmm0
	pxor	%xmm5,%xmm1

	movdqu	%xmm8,0(%rdi)
	movdqu	%xmm9,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm3,48(%rdi)
	movdqu	%xmm10,64(%rdi)
	movdqu	%xmm11,80(%rdi)
	movdqu	%xmm0,96(%rdi)
	movdqu	%xmm1,112(%rdi)
	leaq	(%r9),%rsp

L$128_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_4x:

L$ChaCha20_4x:
	movq	%rsp,%r9

	movq	%r10,%r11
	shrq	$32,%r10
	testq	$32,%r10
	jnz	L$ChaCha20_8x
	cmpq	$192,%rdx
	ja	L$proceed4x

	andq	$71303168,%r11
	cmpq	$4194304,%r11
	je	L$do_sse3_after_all

L$proceed4x:
	subq	$0x140+8,%rsp
	movdqa	L$sigma(%rip),%xmm11
	movdqu	(%rcx),%xmm15
	movdqu	16(%rcx),%xmm7
	movdqu	(%r8),%xmm3
	leaq	256(%rsp),%rcx
	leaq	L$rot16(%rip),%r10
	leaq	L$rot24(%rip),%r11

	pshufd	$0x00,%xmm11,%xmm8
	pshufd	$0x55,%xmm11,%xmm9
	movdqa	%xmm8,64(%rsp)
	pshufd	$0xaa,%xmm11,%xmm10
	movdqa	%xmm9,80(%rsp)
	pshufd	$0xff,%xmm11,%xmm11
	movdqa	%xmm10,96(%rsp)
	movdqa	%xmm11,112(%rsp)

	pshufd	$0x00,%xmm15,%xmm12
	pshufd	$0x55,%xmm15,%xmm13
	movdqa	%xmm12,128-256(%rcx)
	pshufd	$0xaa,%xmm15,%xmm14
	movdqa	%xmm13,144-256(%rcx)
	pshufd	$0xff,%xmm15,%xmm15
	movdqa	%xmm14,160-256(%rcx)
	movdqa	%xmm15,176-256(%rcx)

	pshufd	$0x00,%xmm7,%xmm4
	pshufd	$0x55,%xmm7,%xmm5
	movdqa	%xmm4,192-256(%rcx)
	pshufd	$0xaa,%xmm7,%xmm6
	movdqa	%xmm5,208-256(%rcx)
	pshufd	$0xff,%xmm7,%xmm7
	movdqa	%xmm6,224-256(%rcx)
	movdqa	%xmm7,240-256(%rcx)

	pshufd	$0x00,%xmm3,%xmm0
	pshufd	$0x55,%xmm3,%xmm1
	paddd	L$inc(%rip),%xmm0
	pshufd	$0xaa,%xmm3,%xmm2
	movdqa	%xmm1,272-256(%rcx)
	pshufd	$0xff,%xmm3,%xmm3
	movdqa	%xmm2,288-256(%rcx)
	movdqa	%xmm3,304-256(%rcx)

	jmp	L$oop_enter4x

.p2align	5
L$oop_outer4x:
	movdqa	64(%rsp),%xmm8
	movdqa	80(%rsp),%xmm9
	movdqa	96(%rsp),%xmm10
	movdqa	112(%rsp),%xmm11
	movdqa	128-256(%rcx),%xmm12
	movdqa	144-256(%rcx),%xmm13
	movdqa	160-256(%rcx),%xmm14
	movdqa	176-256(%rcx),%xmm15
	movdqa	192-256(%rcx),%xmm4
	movdqa	208-256(%rcx),%xmm5
	movdqa	224-256(%rcx),%xmm6
	movdqa	240-256(%rcx),%xmm7
	movdqa	256-256(%rcx),%xmm0
	movdqa	272-256(%rcx),%xmm1
	movdqa	288-256(%rcx),%xmm2
	movdqa	304-256(%rcx),%xmm3
	paddd	L$four(%rip),%xmm0

L$oop_enter4x:
	movdqa	%xmm6,32(%rsp)
	movdqa	%xmm7,48(%rsp)
	movdqa	(%r10),%xmm7
	movl	$10,%eax
	movdqa	%xmm0,256-256(%rcx)
	jmp	L$oop4x

.p2align	5
L$oop4x:
	paddd	%xmm12,%xmm8
	paddd	%xmm13,%xmm9
	pxor	%xmm8,%xmm0
	pxor	%xmm9,%xmm1
.byte	102,15,56,0,199
.byte	102,15,56,0,207
	paddd	%xmm0,%xmm4
	paddd	%xmm1,%xmm5
	pxor	%xmm4,%xmm12
	pxor	%xmm5,%xmm13
	movdqa	%xmm12,%xmm6
	pslld	$12,%xmm12
	psrld	$20,%xmm6
	movdqa	%xmm13,%xmm7
	pslld	$12,%xmm13
	por	%xmm6,%xmm12
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm13
	paddd	%xmm12,%xmm8
	paddd	%xmm13,%xmm9
	pxor	%xmm8,%xmm0
	pxor	%xmm9,%xmm1
.byte	102,15,56,0,198
.byte	102,15,56,0,206
	paddd	%xmm0,%xmm4
	paddd	%xmm1,%xmm5
	pxor	%xmm4,%xmm12
	pxor	%xmm5,%xmm13
	movdqa	%xmm12,%xmm7
	pslld	$7,%xmm12
	psrld	$25,%xmm7
	movdqa	%xmm13,%xmm6
	pslld	$7,%xmm13
	por	%xmm7,%xmm12
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm13
	movdqa	%xmm4,0(%rsp)
	movdqa	%xmm5,16(%rsp)
	movdqa	32(%rsp),%xmm4
	movdqa	48(%rsp),%xmm5
	paddd	%xmm14,%xmm10
	paddd	%xmm15,%xmm11
	pxor	%xmm10,%xmm2
	pxor	%xmm11,%xmm3
.byte	102,15,56,0,215
.byte	102,15,56,0,223
	paddd	%xmm2,%xmm4
	paddd	%xmm3,%xmm5
	pxor	%xmm4,%xmm14
	pxor	%xmm5,%xmm15
	movdqa	%xmm14,%xmm6
	pslld	$12,%xmm14
	psrld	$20,%xmm6
	movdqa	%xmm15,%xmm7
	pslld	$12,%xmm15
	por	%xmm6,%xmm14
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm15
	paddd	%xmm14,%xmm10
	paddd	%xmm15,%xmm11
	pxor	%xmm10,%xmm2
	pxor	%xmm11,%xmm3
.byte	102,15,56,0,214
.byte	102,15,56,0,222
	paddd	%xmm2,%xmm4
	paddd	%xmm3,%xmm5
	pxor	%xmm4,%xmm14
	pxor	%xmm5,%xmm15
	movdqa	%xmm14,%xmm7
	pslld	$7,%xmm14
	psrld	$25,%xmm7
	movdqa	%xmm15,%xmm6
	pslld	$7,%xmm15
	por	%xmm7,%xmm14
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm15
	paddd	%xmm13,%xmm8
	paddd	%xmm14,%xmm9
	pxor	%xmm8,%xmm3
	pxor	%xmm9,%xmm0
.byte	102,15,56,0,223
.byte	102,15,56,0,199
	paddd	%xmm3,%xmm4
	paddd	%xmm0,%xmm5
	pxor	%xmm4,%xmm13
	pxor	%xmm5,%xmm14
	movdqa	%xmm13,%xmm6
	pslld	$12,%xmm13
	psrld	$20,%xmm6
	movdqa	%xmm14,%xmm7
	pslld	$12,%xmm14
	por	%xmm6,%xmm13
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm14
	paddd	%xmm13,%xmm8
	paddd	%xmm14,%xmm9
	pxor	%xmm8,%xmm3
	pxor	%xmm9,%xmm0
.byte	102,15,56,0,222
.byte	102,15,56,0,198
	paddd	%xmm3,%xmm4
	paddd	%xmm0,%xmm5
	pxor	%xmm4,%xmm13
	pxor	%xmm5,%xmm14
	movdqa	%xmm13,%xmm7
	pslld	$7,%xmm13
	psrld	$25,%xmm7
	movdqa	%xmm14,%xmm6
	pslld	$7,%xmm14
	por	%xmm7,%xmm13
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm14
	movdqa	%xmm4,32(%rsp)
	movdqa	%xmm5,48(%rsp)
	movdqa	0(%rsp),%xmm4
	movdqa	16(%rsp),%xmm5
	paddd	%xmm15,%xmm10
	paddd	%xmm12,%xmm11
	pxor	%xmm10,%xmm1
	pxor	%xmm11,%xmm2
.byte	102,15,56,0,207
.byte	102,15,56,0,215
	paddd	%xmm1,%xmm4
	paddd	%xmm2,%xmm5
	pxor	%xmm4,%xmm15
	pxor	%xmm5,%xmm12
	movdqa	%xmm15,%xmm6
	pslld	$12,%xmm15
	psrld	$20,%xmm6
	movdqa	%xmm12,%xmm7
	pslld	$12,%xmm12
	por	%xmm6,%xmm15
	psrld	$20,%xmm7
	movdqa	(%r11),%xmm6
	por	%xmm7,%xmm12
	paddd	%xmm15,%xmm10
	paddd	%xmm12,%xmm11
	pxor	%xmm10,%xmm1
	pxor	%xmm11,%xmm2
.byte	102,15,56,0,206
.byte	102,15,56,0,214
	paddd	%xmm1,%xmm4
	paddd	%xmm2,%xmm5
	pxor	%xmm4,%xmm15
	pxor	%xmm5,%xmm12
	movdqa	%xmm15,%xmm7
	pslld	$7,%xmm15
	psrld	$25,%xmm7
	movdqa	%xmm12,%xmm6
	pslld	$7,%xmm12
	por	%xmm7,%xmm15
	psrld	$25,%xmm6
	movdqa	(%r10),%xmm7
	por	%xmm6,%xmm12
	decl	%eax
	jnz	L$oop4x

	paddd	64(%rsp),%xmm8
	paddd	80(%rsp),%xmm9
	paddd	96(%rsp),%xmm10
	paddd	112(%rsp),%xmm11

	movdqa	%xmm8,%xmm6
	punpckldq	%xmm9,%xmm8
	movdqa	%xmm10,%xmm7
	punpckldq	%xmm11,%xmm10
	punpckhdq	%xmm9,%xmm6
	punpckhdq	%xmm11,%xmm7
	movdqa	%xmm8,%xmm9
	punpcklqdq	%xmm10,%xmm8
	movdqa	%xmm6,%xmm11
	punpcklqdq	%xmm7,%xmm6
	punpckhqdq	%xmm10,%xmm9
	punpckhqdq	%xmm7,%xmm11
	paddd	128-256(%rcx),%xmm12
	paddd	144-256(%rcx),%xmm13
	paddd	160-256(%rcx),%xmm14
	paddd	176-256(%rcx),%xmm15

	movdqa	%xmm8,0(%rsp)
	movdqa	%xmm9,16(%rsp)
	movdqa	32(%rsp),%xmm8
	movdqa	48(%rsp),%xmm9

	movdqa	%xmm12,%xmm10
	punpckldq	%xmm13,%xmm12
	movdqa	%xmm14,%xmm7
	punpckldq	%xmm15,%xmm14
	punpckhdq	%xmm13,%xmm10
	punpckhdq	%xmm15,%xmm7
	movdqa	%xmm12,%xmm13
	punpcklqdq	%xmm14,%xmm12
	movdqa	%xmm10,%xmm15
	punpcklqdq	%xmm7,%xmm10
	punpckhqdq	%xmm14,%xmm13
	punpckhqdq	%xmm7,%xmm15
	paddd	192-256(%rcx),%xmm4
	paddd	208-256(%rcx),%xmm5
	paddd	224-256(%rcx),%xmm8
	paddd	240-256(%rcx),%xmm9

	movdqa	%xmm6,32(%rsp)
	movdqa	%xmm11,48(%rsp)

	movdqa	%xmm4,%xmm14
	punpckldq	%xmm5,%xmm4
	movdqa	%xmm8,%xmm7
	punpckldq	%xmm9,%xmm8
	punpckhdq	%xmm5,%xmm14
	punpckhdq	%xmm9,%xmm7
	movdqa	%xmm4,%xmm5
	punpcklqdq	%xmm8,%xmm4
	movdqa	%xmm14,%xmm9
	punpcklqdq	%xmm7,%xmm14
	punpckhqdq	%xmm8,%xmm5
	punpckhqdq	%xmm7,%xmm9
	paddd	256-256(%rcx),%xmm0
	paddd	272-256(%rcx),%xmm1
	paddd	288-256(%rcx),%xmm2
	paddd	304-256(%rcx),%xmm3

	movdqa	%xmm0,%xmm8
	punpckldq	%xmm1,%xmm0
	movdqa	%xmm2,%xmm7
	punpckldq	%xmm3,%xmm2
	punpckhdq	%xmm1,%xmm8
	punpckhdq	%xmm3,%xmm7
	movdqa	%xmm0,%xmm1
	punpcklqdq	%xmm2,%xmm0
	movdqa	%xmm8,%xmm3
	punpcklqdq	%xmm7,%xmm8
	punpckhqdq	%xmm2,%xmm1
	punpckhqdq	%xmm7,%xmm3
	cmpq	$256,%rdx
	jb	L$tail4x

	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	leaq	128(%rsi),%rsi
	pxor	16(%rsp),%xmm6
	pxor	%xmm13,%xmm11
	pxor	%xmm5,%xmm2
	pxor	%xmm1,%xmm7

	movdqu	%xmm6,64(%rdi)
	movdqu	0(%rsi),%xmm6
	movdqu	%xmm11,80(%rdi)
	movdqu	16(%rsi),%xmm11
	movdqu	%xmm2,96(%rdi)
	movdqu	32(%rsi),%xmm2
	movdqu	%xmm7,112(%rdi)
	leaq	128(%rdi),%rdi
	movdqu	48(%rsi),%xmm7
	pxor	32(%rsp),%xmm6
	pxor	%xmm10,%xmm11
	pxor	%xmm14,%xmm2
	pxor	%xmm8,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	leaq	128(%rsi),%rsi
	pxor	48(%rsp),%xmm6
	pxor	%xmm15,%xmm11
	pxor	%xmm9,%xmm2
	pxor	%xmm3,%xmm7
	movdqu	%xmm6,64(%rdi)
	movdqu	%xmm11,80(%rdi)
	movdqu	%xmm2,96(%rdi)
	movdqu	%xmm7,112(%rdi)
	leaq	128(%rdi),%rdi

	subq	$256,%rdx
	jnz	L$oop_outer4x

	jmp	L$done4x

L$tail4x:
	cmpq	$192,%rdx
	jae	L$192_or_more4x
	cmpq	$128,%rdx
	jae	L$128_or_more4x
	cmpq	$64,%rdx
	jae	L$64_or_more4x


	xorq	%r10,%r10

	movdqa	%xmm12,16(%rsp)
	movdqa	%xmm4,32(%rsp)
	movdqa	%xmm0,48(%rsp)
	jmp	L$oop_tail4x

.p2align	5
L$64_or_more4x:
	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7
	movdqu	%xmm6,0(%rdi)
	movdqu	%xmm11,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm7,48(%rdi)
	je	L$done4x

	movdqa	16(%rsp),%xmm6
	leaq	64(%rsi),%rsi
	xorq	%r10,%r10
	movdqa	%xmm6,0(%rsp)
	movdqa	%xmm13,16(%rsp)
	leaq	64(%rdi),%rdi
	movdqa	%xmm5,32(%rsp)
	subq	$64,%rdx
	movdqa	%xmm1,48(%rsp)
	jmp	L$oop_tail4x

.p2align	5
L$128_or_more4x:
	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	pxor	16(%rsp),%xmm6
	pxor	%xmm13,%xmm11
	pxor	%xmm5,%xmm2
	pxor	%xmm1,%xmm7
	movdqu	%xmm6,64(%rdi)
	movdqu	%xmm11,80(%rdi)
	movdqu	%xmm2,96(%rdi)
	movdqu	%xmm7,112(%rdi)
	je	L$done4x

	movdqa	32(%rsp),%xmm6
	leaq	128(%rsi),%rsi
	xorq	%r10,%r10
	movdqa	%xmm6,0(%rsp)
	movdqa	%xmm10,16(%rsp)
	leaq	128(%rdi),%rdi
	movdqa	%xmm14,32(%rsp)
	subq	$128,%rdx
	movdqa	%xmm8,48(%rsp)
	jmp	L$oop_tail4x

.p2align	5
L$192_or_more4x:
	movdqu	0(%rsi),%xmm6
	movdqu	16(%rsi),%xmm11
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm7
	pxor	0(%rsp),%xmm6
	pxor	%xmm12,%xmm11
	pxor	%xmm4,%xmm2
	pxor	%xmm0,%xmm7

	movdqu	%xmm6,0(%rdi)
	movdqu	64(%rsi),%xmm6
	movdqu	%xmm11,16(%rdi)
	movdqu	80(%rsi),%xmm11
	movdqu	%xmm2,32(%rdi)
	movdqu	96(%rsi),%xmm2
	movdqu	%xmm7,48(%rdi)
	movdqu	112(%rsi),%xmm7
	leaq	128(%rsi),%rsi
	pxor	16(%rsp),%xmm6
	pxor	%xmm13,%xmm11
	pxor	%xmm5,%xmm2
	pxor	%xmm1,%xmm7

	movdqu	%xmm6,64(%rdi)
	movdqu	0(%rsi),%xmm6
	movdqu	%xmm11,80(%rdi)
	movdqu	16(%rsi),%xmm11
	movdqu	%xmm2,96(%rdi)
	movdqu	32(%rsi),%xmm2
	movdqu	%xmm7,112(%rdi)
	leaq	128(%rdi),%rdi
	movdqu	48(%rsi),%xmm7
	pxor	32(%rsp),%xmm6
	pxor	%xmm10,%xmm11
	pxor	%xmm14,%xmm2
	pxor	%xmm8,%xmm7
	movdqu	%xmm6,0(%rdi)
	movdqu	%xmm11,16(%rdi)
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm7,48(%rdi)
	je	L$done4x

	movdqa	48(%rsp),%xmm6
	leaq	64(%rsi),%rsi
	xorq	%r10,%r10
	movdqa	%xmm6,0(%rsp)
	movdqa	%xmm15,16(%rsp)
	leaq	64(%rdi),%rdi
	movdqa	%xmm9,32(%rsp)
	subq	$192,%rdx
	movdqa	%xmm3,48(%rsp)

L$oop_tail4x:
	movzbl	(%rsi,%r10,1),%eax
	movzbl	(%rsp,%r10,1),%ecx
	leaq	1(%r10),%r10
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r10,1)
	decq	%rdx
	jnz	L$oop_tail4x

L$done4x:
	leaq	(%r9),%rsp

L$4x_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_4xop:

L$ChaCha20_4xop:
	movq	%rsp,%r9

	subq	$0x140+8,%rsp
	vzeroupper

	vmovdqa	L$sigma(%rip),%xmm11
	vmovdqu	(%rcx),%xmm3
	vmovdqu	16(%rcx),%xmm15
	vmovdqu	(%r8),%xmm7
	leaq	256(%rsp),%rcx

	vpshufd	$0x00,%xmm11,%xmm8
	vpshufd	$0x55,%xmm11,%xmm9
	vmovdqa	%xmm8,64(%rsp)
	vpshufd	$0xaa,%xmm11,%xmm10
	vmovdqa	%xmm9,80(%rsp)
	vpshufd	$0xff,%xmm11,%xmm11
	vmovdqa	%xmm10,96(%rsp)
	vmovdqa	%xmm11,112(%rsp)

	vpshufd	$0x00,%xmm3,%xmm0
	vpshufd	$0x55,%xmm3,%xmm1
	vmovdqa	%xmm0,128-256(%rcx)
	vpshufd	$0xaa,%xmm3,%xmm2
	vmovdqa	%xmm1,144-256(%rcx)
	vpshufd	$0xff,%xmm3,%xmm3
	vmovdqa	%xmm2,160-256(%rcx)
	vmovdqa	%xmm3,176-256(%rcx)

	vpshufd	$0x00,%xmm15,%xmm12
	vpshufd	$0x55,%xmm15,%xmm13
	vmovdqa	%xmm12,192-256(%rcx)
	vpshufd	$0xaa,%xmm15,%xmm14
	vmovdqa	%xmm13,208-256(%rcx)
	vpshufd	$0xff,%xmm15,%xmm15
	vmovdqa	%xmm14,224-256(%rcx)
	vmovdqa	%xmm15,240-256(%rcx)

	vpshufd	$0x00,%xmm7,%xmm4
	vpshufd	$0x55,%xmm7,%xmm5
	vpaddd	L$inc(%rip),%xmm4,%xmm4
	vpshufd	$0xaa,%xmm7,%xmm6
	vmovdqa	%xmm5,272-256(%rcx)
	vpshufd	$0xff,%xmm7,%xmm7
	vmovdqa	%xmm6,288-256(%rcx)
	vmovdqa	%xmm7,304-256(%rcx)

	jmp	L$oop_enter4xop

.p2align	5
L$oop_outer4xop:
	vmovdqa	64(%rsp),%xmm8
	vmovdqa	80(%rsp),%xmm9
	vmovdqa	96(%rsp),%xmm10
	vmovdqa	112(%rsp),%xmm11
	vmovdqa	128-256(%rcx),%xmm0
	vmovdqa	144-256(%rcx),%xmm1
	vmovdqa	160-256(%rcx),%xmm2
	vmovdqa	176-256(%rcx),%xmm3
	vmovdqa	192-256(%rcx),%xmm12
	vmovdqa	208-256(%rcx),%xmm13
	vmovdqa	224-256(%rcx),%xmm14
	vmovdqa	240-256(%rcx),%xmm15
	vmovdqa	256-256(%rcx),%xmm4
	vmovdqa	272-256(%rcx),%xmm5
	vmovdqa	288-256(%rcx),%xmm6
	vmovdqa	304-256(%rcx),%xmm7
	vpaddd	L$four(%rip),%xmm4,%xmm4

L$oop_enter4xop:
	movl	$10,%eax
	vmovdqa	%xmm4,256-256(%rcx)
	jmp	L$oop4xop

.p2align	5
L$oop4xop:
	vpaddd	%xmm0,%xmm8,%xmm8
	vpaddd	%xmm1,%xmm9,%xmm9
	vpaddd	%xmm2,%xmm10,%xmm10
	vpaddd	%xmm3,%xmm11,%xmm11
	vpxor	%xmm4,%xmm8,%xmm4
	vpxor	%xmm5,%xmm9,%xmm5
	vpxor	%xmm6,%xmm10,%xmm6
	vpxor	%xmm7,%xmm11,%xmm7
.byte	143,232,120,194,228,16
.byte	143,232,120,194,237,16
.byte	143,232,120,194,246,16
.byte	143,232,120,194,255,16
	vpaddd	%xmm4,%xmm12,%xmm12
	vpaddd	%xmm5,%xmm13,%xmm13
	vpaddd	%xmm6,%xmm14,%xmm14
	vpaddd	%xmm7,%xmm15,%xmm15
	vpxor	%xmm0,%xmm12,%xmm0
	vpxor	%xmm1,%xmm13,%xmm1
	vpxor	%xmm14,%xmm2,%xmm2
	vpxor	%xmm15,%xmm3,%xmm3
.byte	143,232,120,194,192,12
.byte	143,232,120,194,201,12
.byte	143,232,120,194,210,12
.byte	143,232,120,194,219,12
	vpaddd	%xmm8,%xmm0,%xmm8
	vpaddd	%xmm9,%xmm1,%xmm9
	vpaddd	%xmm2,%xmm10,%xmm10
	vpaddd	%xmm3,%xmm11,%xmm11
	vpxor	%xmm4,%xmm8,%xmm4
	vpxor	%xmm5,%xmm9,%xmm5
	vpxor	%xmm6,%xmm10,%xmm6
	vpxor	%xmm7,%xmm11,%xmm7
.byte	143,232,120,194,228,8
.byte	143,232,120,194,237,8
.byte	143,232,120,194,246,8
.byte	143,232,120,194,255,8
	vpaddd	%xmm4,%xmm12,%xmm12
	vpaddd	%xmm5,%xmm13,%xmm13
	vpaddd	%xmm6,%xmm14,%xmm14
	vpaddd	%xmm7,%xmm15,%xmm15
	vpxor	%xmm0,%xmm12,%xmm0
	vpxor	%xmm1,%xmm13,%xmm1
	vpxor	%xmm14,%xmm2,%xmm2
	vpxor	%xmm15,%xmm3,%xmm3
.byte	143,232,120,194,192,7
.byte	143,232,120,194,201,7
.byte	143,232,120,194,210,7
.byte	143,232,120,194,219,7
	vpaddd	%xmm1,%xmm8,%xmm8
	vpaddd	%xmm2,%xmm9,%xmm9
	vpaddd	%xmm3,%xmm10,%xmm10
	vpaddd	%xmm0,%xmm11,%xmm11
	vpxor	%xmm7,%xmm8,%xmm7
	vpxor	%xmm4,%xmm9,%xmm4
	vpxor	%xmm5,%xmm10,%xmm5
	vpxor	%xmm6,%xmm11,%xmm6
.byte	143,232,120,194,255,16
.byte	143,232,120,194,228,16
.byte	143,232,120,194,237,16
.byte	143,232,120,194,246,16
	vpaddd	%xmm7,%xmm14,%xmm14
	vpaddd	%xmm4,%xmm15,%xmm15
	vpaddd	%xmm5,%xmm12,%xmm12
	vpaddd	%xmm6,%xmm13,%xmm13
	vpxor	%xmm1,%xmm14,%xmm1
	vpxor	%xmm2,%xmm15,%xmm2
	vpxor	%xmm12,%xmm3,%xmm3
	vpxor	%xmm13,%xmm0,%xmm0
.byte	143,232,120,194,201,12
.byte	143,232,120,194,210,12
.byte	143,232,120,194,219,12
.byte	143,232,120,194,192,12
	vpaddd	%xmm8,%xmm1,%xmm8
	vpaddd	%xmm9,%xmm2,%xmm9
	vpaddd	%xmm3,%xmm10,%xmm10
	vpaddd	%xmm0,%xmm11,%xmm11
	vpxor	%xmm7,%xmm8,%xmm7
	vpxor	%xmm4,%xmm9,%xmm4
	vpxor	%xmm5,%xmm10,%xmm5
	vpxor	%xmm6,%xmm11,%xmm6
.byte	143,232,120,194,255,8
.byte	143,232,120,194,228,8
.byte	143,232,120,194,237,8
.byte	143,232,120,194,246,8
	vpaddd	%xmm7,%xmm14,%xmm14
	vpaddd	%xmm4,%xmm15,%xmm15
	vpaddd	%xmm5,%xmm12,%xmm12
	vpaddd	%xmm6,%xmm13,%xmm13
	vpxor	%xmm1,%xmm14,%xmm1
	vpxor	%xmm2,%xmm15,%xmm2
	vpxor	%xmm12,%xmm3,%xmm3
	vpxor	%xmm13,%xmm0,%xmm0
.byte	143,232,120,194,201,7
.byte	143,232,120,194,210,7
.byte	143,232,120,194,219,7
.byte	143,232,120,194,192,7
	decl	%eax
	jnz	L$oop4xop

	vpaddd	64(%rsp),%xmm8,%xmm8
	vpaddd	80(%rsp),%xmm9,%xmm9
	vpaddd	96(%rsp),%xmm10,%xmm10
	vpaddd	112(%rsp),%xmm11,%xmm11

	vmovdqa	%xmm14,32(%rsp)
	vmovdqa	%xmm15,48(%rsp)

	vpunpckldq	%xmm9,%xmm8,%xmm14
	vpunpckldq	%xmm11,%xmm10,%xmm15
	vpunpckhdq	%xmm9,%xmm8,%xmm8
	vpunpckhdq	%xmm11,%xmm10,%xmm10
	vpunpcklqdq	%xmm15,%xmm14,%xmm9
	vpunpckhqdq	%xmm15,%xmm14,%xmm14
	vpunpcklqdq	%xmm10,%xmm8,%xmm11
	vpunpckhqdq	%xmm10,%xmm8,%xmm8
	vpaddd	128-256(%rcx),%xmm0,%xmm0
	vpaddd	144-256(%rcx),%xmm1,%xmm1
	vpaddd	160-256(%rcx),%xmm2,%xmm2
	vpaddd	176-256(%rcx),%xmm3,%xmm3

	vmovdqa	%xmm9,0(%rsp)
	vmovdqa	%xmm14,16(%rsp)
	vmovdqa	32(%rsp),%xmm9
	vmovdqa	48(%rsp),%xmm14

	vpunpckldq	%xmm1,%xmm0,%xmm10
	vpunpckldq	%xmm3,%xmm2,%xmm15
	vpunpckhdq	%xmm1,%xmm0,%xmm0
	vpunpckhdq	%xmm3,%xmm2,%xmm2
	vpunpcklqdq	%xmm15,%xmm10,%xmm1
	vpunpckhqdq	%xmm15,%xmm10,%xmm10
	vpunpcklqdq	%xmm2,%xmm0,%xmm3
	vpunpckhqdq	%xmm2,%xmm0,%xmm0
	vpaddd	192-256(%rcx),%xmm12,%xmm12
	vpaddd	208-256(%rcx),%xmm13,%xmm13
	vpaddd	224-256(%rcx),%xmm9,%xmm9
	vpaddd	240-256(%rcx),%xmm14,%xmm14

	vpunpckldq	%xmm13,%xmm12,%xmm2
	vpunpckldq	%xmm14,%xmm9,%xmm15
	vpunpckhdq	%xmm13,%xmm12,%xmm12
	vpunpckhdq	%xmm14,%xmm9,%xmm9
	vpunpcklqdq	%xmm15,%xmm2,%xmm13
	vpunpckhqdq	%xmm15,%xmm2,%xmm2
	vpunpcklqdq	%xmm9,%xmm12,%xmm14
	vpunpckhqdq	%xmm9,%xmm12,%xmm12
	vpaddd	256-256(%rcx),%xmm4,%xmm4
	vpaddd	272-256(%rcx),%xmm5,%xmm5
	vpaddd	288-256(%rcx),%xmm6,%xmm6
	vpaddd	304-256(%rcx),%xmm7,%xmm7

	vpunpckldq	%xmm5,%xmm4,%xmm9
	vpunpckldq	%xmm7,%xmm6,%xmm15
	vpunpckhdq	%xmm5,%xmm4,%xmm4
	vpunpckhdq	%xmm7,%xmm6,%xmm6
	vpunpcklqdq	%xmm15,%xmm9,%xmm5
	vpunpckhqdq	%xmm15,%xmm9,%xmm9
	vpunpcklqdq	%xmm6,%xmm4,%xmm7
	vpunpckhqdq	%xmm6,%xmm4,%xmm4
	vmovdqa	0(%rsp),%xmm6
	vmovdqa	16(%rsp),%xmm15

	cmpq	$256,%rdx
	jb	L$tail4xop

	vpxor	0(%rsi),%xmm6,%xmm6
	vpxor	16(%rsi),%xmm1,%xmm1
	vpxor	32(%rsi),%xmm13,%xmm13
	vpxor	48(%rsi),%xmm5,%xmm5
	vpxor	64(%rsi),%xmm15,%xmm15
	vpxor	80(%rsi),%xmm10,%xmm10
	vpxor	96(%rsi),%xmm2,%xmm2
	vpxor	112(%rsi),%xmm9,%xmm9
	leaq	128(%rsi),%rsi
	vpxor	0(%rsi),%xmm11,%xmm11
	vpxor	16(%rsi),%xmm3,%xmm3
	vpxor	32(%rsi),%xmm14,%xmm14
	vpxor	48(%rsi),%xmm7,%xmm7
	vpxor	64(%rsi),%xmm8,%xmm8
	vpxor	80(%rsi),%xmm0,%xmm0
	vpxor	96(%rsi),%xmm12,%xmm12
	vpxor	112(%rsi),%xmm4,%xmm4
	leaq	128(%rsi),%rsi

	vmovdqu	%xmm6,0(%rdi)
	vmovdqu	%xmm1,16(%rdi)
	vmovdqu	%xmm13,32(%rdi)
	vmovdqu	%xmm5,48(%rdi)
	vmovdqu	%xmm15,64(%rdi)
	vmovdqu	%xmm10,80(%rdi)
	vmovdqu	%xmm2,96(%rdi)
	vmovdqu	%xmm9,112(%rdi)
	leaq	128(%rdi),%rdi
	vmovdqu	%xmm11,0(%rdi)
	vmovdqu	%xmm3,16(%rdi)
	vmovdqu	%xmm14,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	vmovdqu	%xmm8,64(%rdi)
	vmovdqu	%xmm0,80(%rdi)
	vmovdqu	%xmm12,96(%rdi)
	vmovdqu	%xmm4,112(%rdi)
	leaq	128(%rdi),%rdi

	subq	$256,%rdx
	jnz	L$oop_outer4xop

	jmp	L$done4xop

.p2align	5
L$tail4xop:
	cmpq	$192,%rdx
	jae	L$192_or_more4xop
	cmpq	$128,%rdx
	jae	L$128_or_more4xop
	cmpq	$64,%rdx
	jae	L$64_or_more4xop

	xorq	%r10,%r10
	vmovdqa	%xmm6,0(%rsp)
	vmovdqa	%xmm1,16(%rsp)
	vmovdqa	%xmm13,32(%rsp)
	vmovdqa	%xmm5,48(%rsp)
	jmp	L$oop_tail4xop

.p2align	5
L$64_or_more4xop:
	vpxor	0(%rsi),%xmm6,%xmm6
	vpxor	16(%rsi),%xmm1,%xmm1
	vpxor	32(%rsi),%xmm13,%xmm13
	vpxor	48(%rsi),%xmm5,%xmm5
	vmovdqu	%xmm6,0(%rdi)
	vmovdqu	%xmm1,16(%rdi)
	vmovdqu	%xmm13,32(%rdi)
	vmovdqu	%xmm5,48(%rdi)
	je	L$done4xop

	leaq	64(%rsi),%rsi
	vmovdqa	%xmm15,0(%rsp)
	xorq	%r10,%r10
	vmovdqa	%xmm10,16(%rsp)
	leaq	64(%rdi),%rdi
	vmovdqa	%xmm2,32(%rsp)
	subq	$64,%rdx
	vmovdqa	%xmm9,48(%rsp)
	jmp	L$oop_tail4xop

.p2align	5
L$128_or_more4xop:
	vpxor	0(%rsi),%xmm6,%xmm6
	vpxor	16(%rsi),%xmm1,%xmm1
	vpxor	32(%rsi),%xmm13,%xmm13
	vpxor	48(%rsi),%xmm5,%xmm5
	vpxor	64(%rsi),%xmm15,%xmm15
	vpxor	80(%rsi),%xmm10,%xmm10
	vpxor	96(%rsi),%xmm2,%xmm2
	vpxor	112(%rsi),%xmm9,%xmm9

	vmovdqu	%xmm6,0(%rdi)
	vmovdqu	%xmm1,16(%rdi)
	vmovdqu	%xmm13,32(%rdi)
	vmovdqu	%xmm5,48(%rdi)
	vmovdqu	%xmm15,64(%rdi)
	vmovdqu	%xmm10,80(%rdi)
	vmovdqu	%xmm2,96(%rdi)
	vmovdqu	%xmm9,112(%rdi)
	je	L$done4xop

	leaq	128(%rsi),%rsi
	vmovdqa	%xmm11,0(%rsp)
	xorq	%r10,%r10
	vmovdqa	%xmm3,16(%rsp)
	leaq	128(%rdi),%rdi
	vmovdqa	%xmm14,32(%rsp)
	subq	$128,%rdx
	vmovdqa	%xmm7,48(%rsp)
	jmp	L$oop_tail4xop

.p2align	5
L$192_or_more4xop:
	vpxor	0(%rsi),%xmm6,%xmm6
	vpxor	16(%rsi),%xmm1,%xmm1
	vpxor	32(%rsi),%xmm13,%xmm13
	vpxor	48(%rsi),%xmm5,%xmm5
	vpxor	64(%rsi),%xmm15,%xmm15
	vpxor	80(%rsi),%xmm10,%xmm10
	vpxor	96(%rsi),%xmm2,%xmm2
	vpxor	112(%rsi),%xmm9,%xmm9
	leaq	128(%rsi),%rsi
	vpxor	0(%rsi),%xmm11,%xmm11
	vpxor	16(%rsi),%xmm3,%xmm3
	vpxor	32(%rsi),%xmm14,%xmm14
	vpxor	48(%rsi),%xmm7,%xmm7

	vmovdqu	%xmm6,0(%rdi)
	vmovdqu	%xmm1,16(%rdi)
	vmovdqu	%xmm13,32(%rdi)
	vmovdqu	%xmm5,48(%rdi)
	vmovdqu	%xmm15,64(%rdi)
	vmovdqu	%xmm10,80(%rdi)
	vmovdqu	%xmm2,96(%rdi)
	vmovdqu	%xmm9,112(%rdi)
	leaq	128(%rdi),%rdi
	vmovdqu	%xmm11,0(%rdi)
	vmovdqu	%xmm3,16(%rdi)
	vmovdqu	%xmm14,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	je	L$done4xop

	leaq	64(%rsi),%rsi
	vmovdqa	%xmm8,0(%rsp)
	xorq	%r10,%r10
	vmovdqa	%xmm0,16(%rsp)
	leaq	64(%rdi),%rdi
	vmovdqa	%xmm12,32(%rsp)
	subq	$192,%rdx
	vmovdqa	%xmm4,48(%rsp)

L$oop_tail4xop:
	movzbl	(%rsi,%r10,1),%eax
	movzbl	(%rsp,%r10,1),%ecx
	leaq	1(%r10),%r10
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r10,1)
	decq	%rdx
	jnz	L$oop_tail4xop

L$done4xop:
	vzeroupper
	leaq	(%r9),%rsp

L$4xop_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_8x:

L$ChaCha20_8x:
	movq	%rsp,%r9

	subq	$0x280+8,%rsp
	andq	$-32,%rsp
	vzeroupper










	vbroadcasti128	L$sigma(%rip),%ymm11
	vbroadcasti128	(%rcx),%ymm3
	vbroadcasti128	16(%rcx),%ymm15
	vbroadcasti128	(%r8),%ymm7
	leaq	256(%rsp),%rcx
	leaq	512(%rsp),%rax
	leaq	L$rot16(%rip),%r10
	leaq	L$rot24(%rip),%r11

	vpshufd	$0x00,%ymm11,%ymm8
	vpshufd	$0x55,%ymm11,%ymm9
	vmovdqa	%ymm8,128-256(%rcx)
	vpshufd	$0xaa,%ymm11,%ymm10
	vmovdqa	%ymm9,160-256(%rcx)
	vpshufd	$0xff,%ymm11,%ymm11
	vmovdqa	%ymm10,192-256(%rcx)
	vmovdqa	%ymm11,224-256(%rcx)

	vpshufd	$0x00,%ymm3,%ymm0
	vpshufd	$0x55,%ymm3,%ymm1
	vmovdqa	%ymm0,256-256(%rcx)
	vpshufd	$0xaa,%ymm3,%ymm2
	vmovdqa	%ymm1,288-256(%rcx)
	vpshufd	$0xff,%ymm3,%ymm3
	vmovdqa	%ymm2,320-256(%rcx)
	vmovdqa	%ymm3,352-256(%rcx)

	vpshufd	$0x00,%ymm15,%ymm12
	vpshufd	$0x55,%ymm15,%ymm13
	vmovdqa	%ymm12,384-512(%rax)
	vpshufd	$0xaa,%ymm15,%ymm14
	vmovdqa	%ymm13,416-512(%rax)
	vpshufd	$0xff,%ymm15,%ymm15
	vmovdqa	%ymm14,448-512(%rax)
	vmovdqa	%ymm15,480-512(%rax)

	vpshufd	$0x00,%ymm7,%ymm4
	vpshufd	$0x55,%ymm7,%ymm5
	vpaddd	L$incy(%rip),%ymm4,%ymm4
	vpshufd	$0xaa,%ymm7,%ymm6
	vmovdqa	%ymm5,544-512(%rax)
	vpshufd	$0xff,%ymm7,%ymm7
	vmovdqa	%ymm6,576-512(%rax)
	vmovdqa	%ymm7,608-512(%rax)

	jmp	L$oop_enter8x

.p2align	5
L$oop_outer8x:
	vmovdqa	128-256(%rcx),%ymm8
	vmovdqa	160-256(%rcx),%ymm9
	vmovdqa	192-256(%rcx),%ymm10
	vmovdqa	224-256(%rcx),%ymm11
	vmovdqa	256-256(%rcx),%ymm0
	vmovdqa	288-256(%rcx),%ymm1
	vmovdqa	320-256(%rcx),%ymm2
	vmovdqa	352-256(%rcx),%ymm3
	vmovdqa	384-512(%rax),%ymm12
	vmovdqa	416-512(%rax),%ymm13
	vmovdqa	448-512(%rax),%ymm14
	vmovdqa	480-512(%rax),%ymm15
	vmovdqa	512-512(%rax),%ymm4
	vmovdqa	544-512(%rax),%ymm5
	vmovdqa	576-512(%rax),%ymm6
	vmovdqa	608-512(%rax),%ymm7
	vpaddd	L$eight(%rip),%ymm4,%ymm4

L$oop_enter8x:
	vmovdqa	%ymm14,64(%rsp)
	vmovdqa	%ymm15,96(%rsp)
	vbroadcasti128	(%r10),%ymm15
	vmovdqa	%ymm4,512-512(%rax)
	movl	$10,%eax
	jmp	L$oop8x

.p2align	5
L$oop8x:
	vpaddd	%ymm0,%ymm8,%ymm8
	vpxor	%ymm4,%ymm8,%ymm4
	vpshufb	%ymm15,%ymm4,%ymm4
	vpaddd	%ymm1,%ymm9,%ymm9
	vpxor	%ymm5,%ymm9,%ymm5
	vpshufb	%ymm15,%ymm5,%ymm5
	vpaddd	%ymm4,%ymm12,%ymm12
	vpxor	%ymm0,%ymm12,%ymm0
	vpslld	$12,%ymm0,%ymm14
	vpsrld	$20,%ymm0,%ymm0
	vpor	%ymm0,%ymm14,%ymm0
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm5,%ymm13,%ymm13
	vpxor	%ymm1,%ymm13,%ymm1
	vpslld	$12,%ymm1,%ymm15
	vpsrld	$20,%ymm1,%ymm1
	vpor	%ymm1,%ymm15,%ymm1
	vpaddd	%ymm0,%ymm8,%ymm8
	vpxor	%ymm4,%ymm8,%ymm4
	vpshufb	%ymm14,%ymm4,%ymm4
	vpaddd	%ymm1,%ymm9,%ymm9
	vpxor	%ymm5,%ymm9,%ymm5
	vpshufb	%ymm14,%ymm5,%ymm5
	vpaddd	%ymm4,%ymm12,%ymm12
	vpxor	%ymm0,%ymm12,%ymm0
	vpslld	$7,%ymm0,%ymm15
	vpsrld	$25,%ymm0,%ymm0
	vpor	%ymm0,%ymm15,%ymm0
	vbroadcasti128	(%r10),%ymm15
	vpaddd	%ymm5,%ymm13,%ymm13
	vpxor	%ymm1,%ymm13,%ymm1
	vpslld	$7,%ymm1,%ymm14
	vpsrld	$25,%ymm1,%ymm1
	vpor	%ymm1,%ymm14,%ymm1
	vmovdqa	%ymm12,0(%rsp)
	vmovdqa	%ymm13,32(%rsp)
	vmovdqa	64(%rsp),%ymm12
	vmovdqa	96(%rsp),%ymm13
	vpaddd	%ymm2,%ymm10,%ymm10
	vpxor	%ymm6,%ymm10,%ymm6
	vpshufb	%ymm15,%ymm6,%ymm6
	vpaddd	%ymm3,%ymm11,%ymm11
	vpxor	%ymm7,%ymm11,%ymm7
	vpshufb	%ymm15,%ymm7,%ymm7
	vpaddd	%ymm6,%ymm12,%ymm12
	vpxor	%ymm2,%ymm12,%ymm2
	vpslld	$12,%ymm2,%ymm14
	vpsrld	$20,%ymm2,%ymm2
	vpor	%ymm2,%ymm14,%ymm2
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm7,%ymm13,%ymm13
	vpxor	%ymm3,%ymm13,%ymm3
	vpslld	$12,%ymm3,%ymm15
	vpsrld	$20,%ymm3,%ymm3
	vpor	%ymm3,%ymm15,%ymm3
	vpaddd	%ymm2,%ymm10,%ymm10
	vpxor	%ymm6,%ymm10,%ymm6
	vpshufb	%ymm14,%ymm6,%ymm6
	vpaddd	%ymm3,%ymm11,%ymm11
	vpxor	%ymm7,%ymm11,%ymm7
	vpshufb	%ymm14,%ymm7,%ymm7
	vpaddd	%ymm6,%ymm12,%ymm12
	vpxor	%ymm2,%ymm12,%ymm2
	vpslld	$7,%ymm2,%ymm15
	vpsrld	$25,%ymm2,%ymm2
	vpor	%ymm2,%ymm15,%ymm2
	vbroadcasti128	(%r10),%ymm15
	vpaddd	%ymm7,%ymm13,%ymm13
	vpxor	%ymm3,%ymm13,%ymm3
	vpslld	$7,%ymm3,%ymm14
	vpsrld	$25,%ymm3,%ymm3
	vpor	%ymm3,%ymm14,%ymm3
	vpaddd	%ymm1,%ymm8,%ymm8
	vpxor	%ymm7,%ymm8,%ymm7
	vpshufb	%ymm15,%ymm7,%ymm7
	vpaddd	%ymm2,%ymm9,%ymm9
	vpxor	%ymm4,%ymm9,%ymm4
	vpshufb	%ymm15,%ymm4,%ymm4
	vpaddd	%ymm7,%ymm12,%ymm12
	vpxor	%ymm1,%ymm12,%ymm1
	vpslld	$12,%ymm1,%ymm14
	vpsrld	$20,%ymm1,%ymm1
	vpor	%ymm1,%ymm14,%ymm1
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm4,%ymm13,%ymm13
	vpxor	%ymm2,%ymm13,%ymm2
	vpslld	$12,%ymm2,%ymm15
	vpsrld	$20,%ymm2,%ymm2
	vpor	%ymm2,%ymm15,%ymm2
	vpaddd	%ymm1,%ymm8,%ymm8
	vpxor	%ymm7,%ymm8,%ymm7
	vpshufb	%ymm14,%ymm7,%ymm7
	vpaddd	%ymm2,%ymm9,%ymm9
	vpxor	%ymm4,%ymm9,%ymm4
	vpshufb	%ymm14,%ymm4,%ymm4
	vpaddd	%ymm7,%ymm12,%ymm12
	vpxor	%ymm1,%ymm12,%ymm1
	vpslld	$7,%ymm1,%ymm15
	vpsrld	$25,%ymm1,%ymm1
	vpor	%ymm1,%ymm15,%ymm1
	vbroadcasti128	(%r10),%ymm15
	vpaddd	%ymm4,%ymm13,%ymm13
	vpxor	%ymm2,%ymm13,%ymm2
	vpslld	$7,%ymm2,%ymm14
	vpsrld	$25,%ymm2,%ymm2
	vpor	%ymm2,%ymm14,%ymm2
	vmovdqa	%ymm12,64(%rsp)
	vmovdqa	%ymm13,96(%rsp)
	vmovdqa	0(%rsp),%ymm12
	vmovdqa	32(%rsp),%ymm13
	vpaddd	%ymm3,%ymm10,%ymm10
	vpxor	%ymm5,%ymm10,%ymm5
	vpshufb	%ymm15,%ymm5,%ymm5
	vpaddd	%ymm0,%ymm11,%ymm11
	vpxor	%ymm6,%ymm11,%ymm6
	vpshufb	%ymm15,%ymm6,%ymm6
	vpaddd	%ymm5,%ymm12,%ymm12
	vpxor	%ymm3,%ymm12,%ymm3
	vpslld	$12,%ymm3,%ymm14
	vpsrld	$20,%ymm3,%ymm3
	vpor	%ymm3,%ymm14,%ymm3
	vbroadcasti128	(%r11),%ymm14
	vpaddd	%ymm6,%ymm13,%ymm13
	vpxor	%ymm0,%ymm13,%ymm0
	vpslld	$12,%ymm0,%ymm15
	vpsrld	$20,%ymm0,%ymm0
	vpor	%ymm0,%ymm15,%ymm0
	vpaddd	%ymm3,%ymm10,%ymm10
	vpxor	%ymm5,%ymm10,%ymm5
	vpshufb	%ymm14,%ymm5,%ymm5
	vpaddd	%ymm0,%ymm11,%ymm11
	vpxor	%ymm6,%ymm11,%ymm6
	vpshufb	%ymm14,%ymm6,%ymm6
	vpaddd	%ymm5,%ymm12,%ymm12
	vpxor	%ymm3,%ymm12,%ymm3
	vpslld	$7,%ymm3,%ymm15
	vpsrld	$25,%ymm3,%ymm3
	vpor	%ymm3,%ymm15,%ymm3
	vbroadcasti128	(%r10),%ymm15
	vpaddd	%ymm6,%ymm13,%ymm13
	vpxor	%ymm0,%ymm13,%ymm0
	vpslld	$7,%ymm0,%ymm14
	vpsrld	$25,%ymm0,%ymm0
	vpor	%ymm0,%ymm14,%ymm0
	decl	%eax
	jnz	L$oop8x

	leaq	512(%rsp),%rax
	vpaddd	128-256(%rcx),%ymm8,%ymm8
	vpaddd	160-256(%rcx),%ymm9,%ymm9
	vpaddd	192-256(%rcx),%ymm10,%ymm10
	vpaddd	224-256(%rcx),%ymm11,%ymm11

	vpunpckldq	%ymm9,%ymm8,%ymm14
	vpunpckldq	%ymm11,%ymm10,%ymm15
	vpunpckhdq	%ymm9,%ymm8,%ymm8
	vpunpckhdq	%ymm11,%ymm10,%ymm10
	vpunpcklqdq	%ymm15,%ymm14,%ymm9
	vpunpckhqdq	%ymm15,%ymm14,%ymm14
	vpunpcklqdq	%ymm10,%ymm8,%ymm11
	vpunpckhqdq	%ymm10,%ymm8,%ymm8
	vpaddd	256-256(%rcx),%ymm0,%ymm0
	vpaddd	288-256(%rcx),%ymm1,%ymm1
	vpaddd	320-256(%rcx),%ymm2,%ymm2
	vpaddd	352-256(%rcx),%ymm3,%ymm3

	vpunpckldq	%ymm1,%ymm0,%ymm10
	vpunpckldq	%ymm3,%ymm2,%ymm15
	vpunpckhdq	%ymm1,%ymm0,%ymm0
	vpunpckhdq	%ymm3,%ymm2,%ymm2
	vpunpcklqdq	%ymm15,%ymm10,%ymm1
	vpunpckhqdq	%ymm15,%ymm10,%ymm10
	vpunpcklqdq	%ymm2,%ymm0,%ymm3
	vpunpckhqdq	%ymm2,%ymm0,%ymm0
	vperm2i128	$0x20,%ymm1,%ymm9,%ymm15
	vperm2i128	$0x31,%ymm1,%ymm9,%ymm1
	vperm2i128	$0x20,%ymm10,%ymm14,%ymm9
	vperm2i128	$0x31,%ymm10,%ymm14,%ymm10
	vperm2i128	$0x20,%ymm3,%ymm11,%ymm14
	vperm2i128	$0x31,%ymm3,%ymm11,%ymm3
	vperm2i128	$0x20,%ymm0,%ymm8,%ymm11
	vperm2i128	$0x31,%ymm0,%ymm8,%ymm0
	vmovdqa	%ymm15,0(%rsp)
	vmovdqa	%ymm9,32(%rsp)
	vmovdqa	64(%rsp),%ymm15
	vmovdqa	96(%rsp),%ymm9

	vpaddd	384-512(%rax),%ymm12,%ymm12
	vpaddd	416-512(%rax),%ymm13,%ymm13
	vpaddd	448-512(%rax),%ymm15,%ymm15
	vpaddd	480-512(%rax),%ymm9,%ymm9

	vpunpckldq	%ymm13,%ymm12,%ymm2
	vpunpckldq	%ymm9,%ymm15,%ymm8
	vpunpckhdq	%ymm13,%ymm12,%ymm12
	vpunpckhdq	%ymm9,%ymm15,%ymm15
	vpunpcklqdq	%ymm8,%ymm2,%ymm13
	vpunpckhqdq	%ymm8,%ymm2,%ymm2
	vpunpcklqdq	%ymm15,%ymm12,%ymm9
	vpunpckhqdq	%ymm15,%ymm12,%ymm12
	vpaddd	512-512(%rax),%ymm4,%ymm4
	vpaddd	544-512(%rax),%ymm5,%ymm5
	vpaddd	576-512(%rax),%ymm6,%ymm6
	vpaddd	608-512(%rax),%ymm7,%ymm7

	vpunpckldq	%ymm5,%ymm4,%ymm15
	vpunpckldq	%ymm7,%ymm6,%ymm8
	vpunpckhdq	%ymm5,%ymm4,%ymm4
	vpunpckhdq	%ymm7,%ymm6,%ymm6
	vpunpcklqdq	%ymm8,%ymm15,%ymm5
	vpunpckhqdq	%ymm8,%ymm15,%ymm15
	vpunpcklqdq	%ymm6,%ymm4,%ymm7
	vpunpckhqdq	%ymm6,%ymm4,%ymm4
	vperm2i128	$0x20,%ymm5,%ymm13,%ymm8
	vperm2i128	$0x31,%ymm5,%ymm13,%ymm5
	vperm2i128	$0x20,%ymm15,%ymm2,%ymm13
	vperm2i128	$0x31,%ymm15,%ymm2,%ymm15
	vperm2i128	$0x20,%ymm7,%ymm9,%ymm2
	vperm2i128	$0x31,%ymm7,%ymm9,%ymm7
	vperm2i128	$0x20,%ymm4,%ymm12,%ymm9
	vperm2i128	$0x31,%ymm4,%ymm12,%ymm4
	vmovdqa	0(%rsp),%ymm6
	vmovdqa	32(%rsp),%ymm12

	cmpq	$512,%rdx
	jb	L$tail8x

	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vpxor	64(%rsi),%ymm1,%ymm1
	vpxor	96(%rsi),%ymm5,%ymm5
	leaq	128(%rsi),%rsi
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	vmovdqu	%ymm1,64(%rdi)
	vmovdqu	%ymm5,96(%rdi)
	leaq	128(%rdi),%rdi

	vpxor	0(%rsi),%ymm12,%ymm12
	vpxor	32(%rsi),%ymm13,%ymm13
	vpxor	64(%rsi),%ymm10,%ymm10
	vpxor	96(%rsi),%ymm15,%ymm15
	leaq	128(%rsi),%rsi
	vmovdqu	%ymm12,0(%rdi)
	vmovdqu	%ymm13,32(%rdi)
	vmovdqu	%ymm10,64(%rdi)
	vmovdqu	%ymm15,96(%rdi)
	leaq	128(%rdi),%rdi

	vpxor	0(%rsi),%ymm14,%ymm14
	vpxor	32(%rsi),%ymm2,%ymm2
	vpxor	64(%rsi),%ymm3,%ymm3
	vpxor	96(%rsi),%ymm7,%ymm7
	leaq	128(%rsi),%rsi
	vmovdqu	%ymm14,0(%rdi)
	vmovdqu	%ymm2,32(%rdi)
	vmovdqu	%ymm3,64(%rdi)
	vmovdqu	%ymm7,96(%rdi)
	leaq	128(%rdi),%rdi

	vpxor	0(%rsi),%ymm11,%ymm11
	vpxor	32(%rsi),%ymm9,%ymm9
	vpxor	64(%rsi),%ymm0,%ymm0
	vpxor	96(%rsi),%ymm4,%ymm4
	leaq	128(%rsi),%rsi
	vmovdqu	%ymm11,0(%rdi)
	vmovdqu	%ymm9,32(%rdi)
	vmovdqu	%ymm0,64(%rdi)
	vmovdqu	%ymm4,96(%rdi)
	leaq	128(%rdi),%rdi

	subq	$512,%rdx
	jnz	L$oop_outer8x

	jmp	L$done8x

L$tail8x:
	cmpq	$448,%rdx
	jae	L$448_or_more8x
	cmpq	$384,%rdx
	jae	L$384_or_more8x
	cmpq	$320,%rdx
	jae	L$320_or_more8x
	cmpq	$256,%rdx
	jae	L$256_or_more8x
	cmpq	$192,%rdx
	jae	L$192_or_more8x
	cmpq	$128,%rdx
	jae	L$128_or_more8x
	cmpq	$64,%rdx
	jae	L$64_or_more8x

	xorq	%r10,%r10
	vmovdqa	%ymm6,0(%rsp)
	vmovdqa	%ymm8,32(%rsp)
	jmp	L$oop_tail8x

.p2align	5
L$64_or_more8x:
	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	je	L$done8x

	leaq	64(%rsi),%rsi
	xorq	%r10,%r10
	vmovdqa	%ymm1,0(%rsp)
	leaq	64(%rdi),%rdi
	subq	$64,%rdx
	vmovdqa	%ymm5,32(%rsp)
	jmp	L$oop_tail8x

.p2align	5
L$128_or_more8x:
	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vpxor	64(%rsi),%ymm1,%ymm1
	vpxor	96(%rsi),%ymm5,%ymm5
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	vmovdqu	%ymm1,64(%rdi)
	vmovdqu	%ymm5,96(%rdi)
	je	L$done8x

	leaq	128(%rsi),%rsi
	xorq	%r10,%r10
	vmovdqa	%ymm12,0(%rsp)
	leaq	128(%rdi),%rdi
	subq	$128,%rdx
	vmovdqa	%ymm13,32(%rsp)
	jmp	L$oop_tail8x

.p2align	5
L$192_or_more8x:
	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vpxor	64(%rsi),%ymm1,%ymm1
	vpxor	96(%rsi),%ymm5,%ymm5
	vpxor	128(%rsi),%ymm12,%ymm12
	vpxor	160(%rsi),%ymm13,%ymm13
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	vmovdqu	%ymm1,64(%rdi)
	vmovdqu	%ymm5,96(%rdi)
	vmovdqu	%ymm12,128(%rdi)
	vmovdqu	%ymm13,160(%rdi)
	je	L$done8x

	leaq	192(%rsi),%rsi
	xorq	%r10,%r10
	vmovdqa	%ymm10,0(%rsp)
	leaq	192(%rdi),%rdi
	subq	$192,%rdx
	vmovdqa	%ymm15,32(%rsp)
	jmp	L$oop_tail8x

.p2align	5
L$256_or_more8x:
	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vpxor	64(%rsi),%ymm1,%ymm1
	vpxor	96(%rsi),%ymm5,%ymm5
	vpxor	128(%rsi),%ymm12,%ymm12
	vpxor	160(%rsi),%ymm13,%ymm13
	vpxor	192(%rsi),%ymm10,%ymm10
	vpxor	224(%rsi),%ymm15,%ymm15
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	vmovdqu	%ymm1,64(%rdi)
	vmovdqu	%ymm5,96(%rdi)
	vmovdqu	%ymm12,128(%rdi)
	vmovdqu	%ymm13,160(%rdi)
	vmovdqu	%ymm10,192(%rdi)
	vmovdqu	%ymm15,224(%rdi)
	je	L$done8x

	leaq	256(%rsi),%rsi
	xorq	%r10,%r10
	vmovdqa	%ymm14,0(%rsp)
	leaq	256(%rdi),%rdi
	subq	$256,%rdx
	vmovdqa	%ymm2,32(%rsp)
	jmp	L$oop_tail8x

.p2align	5
L$320_or_more8x:
	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vpxor	64(%rsi),%ymm1,%ymm1
	vpxor	96(%rsi),%ymm5,%ymm5
	vpxor	128(%rsi),%ymm12,%ymm12
	vpxor	160(%rsi),%ymm13,%ymm13
	vpxor	192(%rsi),%ymm10,%ymm10
	vpxor	224(%rsi),%ymm15,%ymm15
	vpxor	256(%rsi),%ymm14,%ymm14
	vpxor	288(%rsi),%ymm2,%ymm2
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	vmovdqu	%ymm1,64(%rdi)
	vmovdqu	%ymm5,96(%rdi)
	vmovdqu	%ymm12,128(%rdi)
	vmovdqu	%ymm13,160(%rdi)
	vmovdqu	%ymm10,192(%rdi)
	vmovdqu	%ymm15,224(%rdi)
	vmovdqu	%ymm14,256(%rdi)
	vmovdqu	%ymm2,288(%rdi)
	je	L$done8x

	leaq	320(%rsi),%rsi
	xorq	%r10,%r10
	vmovdqa	%ymm3,0(%rsp)
	leaq	320(%rdi),%rdi
	subq	$320,%rdx
	vmovdqa	%ymm7,32(%rsp)
	jmp	L$oop_tail8x

.p2align	5
L$384_or_more8x:
	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vpxor	64(%rsi),%ymm1,%ymm1
	vpxor	96(%rsi),%ymm5,%ymm5
	vpxor	128(%rsi),%ymm12,%ymm12
	vpxor	160(%rsi),%ymm13,%ymm13
	vpxor	192(%rsi),%ymm10,%ymm10
	vpxor	224(%rsi),%ymm15,%ymm15
	vpxor	256(%rsi),%ymm14,%ymm14
	vpxor	288(%rsi),%ymm2,%ymm2
	vpxor	320(%rsi),%ymm3,%ymm3
	vpxor	352(%rsi),%ymm7,%ymm7
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	vmovdqu	%ymm1,64(%rdi)
	vmovdqu	%ymm5,96(%rdi)
	vmovdqu	%ymm12,128(%rdi)
	vmovdqu	%ymm13,160(%rdi)
	vmovdqu	%ymm10,192(%rdi)
	vmovdqu	%ymm15,224(%rdi)
	vmovdqu	%ymm14,256(%rdi)
	vmovdqu	%ymm2,288(%rdi)
	vmovdqu	%ymm3,320(%rdi)
	vmovdqu	%ymm7,352(%rdi)
	je	L$done8x

	leaq	384(%rsi),%rsi
	xorq	%r10,%r10
	vmovdqa	%ymm11,0(%rsp)
	leaq	384(%rdi),%rdi
	subq	$384,%rdx
	vmovdqa	%ymm9,32(%rsp)
	jmp	L$oop_tail8x

.p2align	5
L$448_or_more8x:
	vpxor	0(%rsi),%ymm6,%ymm6
	vpxor	32(%rsi),%ymm8,%ymm8
	vpxor	64(%rsi),%ymm1,%ymm1
	vpxor	96(%rsi),%ymm5,%ymm5
	vpxor	128(%rsi),%ymm12,%ymm12
	vpxor	160(%rsi),%ymm13,%ymm13
	vpxor	192(%rsi),%ymm10,%ymm10
	vpxor	224(%rsi),%ymm15,%ymm15
	vpxor	256(%rsi),%ymm14,%ymm14
	vpxor	288(%rsi),%ymm2,%ymm2
	vpxor	320(%rsi),%ymm3,%ymm3
	vpxor	352(%rsi),%ymm7,%ymm7
	vpxor	384(%rsi),%ymm11,%ymm11
	vpxor	416(%rsi),%ymm9,%ymm9
	vmovdqu	%ymm6,0(%rdi)
	vmovdqu	%ymm8,32(%rdi)
	vmovdqu	%ymm1,64(%rdi)
	vmovdqu	%ymm5,96(%rdi)
	vmovdqu	%ymm12,128(%rdi)
	vmovdqu	%ymm13,160(%rdi)
	vmovdqu	%ymm10,192(%rdi)
	vmovdqu	%ymm15,224(%rdi)
	vmovdqu	%ymm14,256(%rdi)
	vmovdqu	%ymm2,288(%rdi)
	vmovdqu	%ymm3,320(%rdi)
	vmovdqu	%ymm7,352(%rdi)
	vmovdqu	%ymm11,384(%rdi)
	vmovdqu	%ymm9,416(%rdi)
	je	L$done8x

	leaq	448(%rsi),%rsi
	xorq	%r10,%r10
	vmovdqa	%ymm0,0(%rsp)
	leaq	448(%rdi),%rdi
	subq	$448,%rdx
	vmovdqa	%ymm4,32(%rsp)

L$oop_tail8x:
	movzbl	(%rsi,%r10,1),%eax
	movzbl	(%rsp,%r10,1),%ecx
	leaq	1(%r10),%r10
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r10,1)
	decq	%rdx
	jnz	L$oop_tail8x

L$done8x:
	vzeroall
	leaq	(%r9),%rsp

L$8x_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_avx512:

L$ChaCha20_avx512:
	movq	%rsp,%r9

	cmpq	$512,%rdx
	ja	L$ChaCha20_16x

	subq	$64+8,%rsp
	vbroadcasti32x4	L$sigma(%rip),%zmm0
	vbroadcasti32x4	(%rcx),%zmm1
	vbroadcasti32x4	16(%rcx),%zmm2
	vbroadcasti32x4	(%r8),%zmm3

	vmovdqa32	%zmm0,%zmm16
	vmovdqa32	%zmm1,%zmm17
	vmovdqa32	%zmm2,%zmm18
	vpaddd	L$zeroz(%rip),%zmm3,%zmm3
	vmovdqa32	L$fourz(%rip),%zmm20
	movq	$10,%r8
	vmovdqa32	%zmm3,%zmm19
	jmp	L$oop_avx512

.p2align	4
L$oop_outer_avx512:
	vmovdqa32	%zmm16,%zmm0
	vmovdqa32	%zmm17,%zmm1
	vmovdqa32	%zmm18,%zmm2
	vpaddd	%zmm20,%zmm19,%zmm3
	movq	$10,%r8
	vmovdqa32	%zmm3,%zmm19
	jmp	L$oop_avx512

.p2align	5
L$oop_avx512:
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$16,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$12,%zmm1,%zmm1
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$8,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$7,%zmm1,%zmm1
	vpshufd	$78,%zmm2,%zmm2
	vpshufd	$57,%zmm1,%zmm1
	vpshufd	$147,%zmm3,%zmm3
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$16,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$12,%zmm1,%zmm1
	vpaddd	%zmm1,%zmm0,%zmm0
	vpxord	%zmm0,%zmm3,%zmm3
	vprold	$8,%zmm3,%zmm3
	vpaddd	%zmm3,%zmm2,%zmm2
	vpxord	%zmm2,%zmm1,%zmm1
	vprold	$7,%zmm1,%zmm1
	vpshufd	$78,%zmm2,%zmm2
	vpshufd	$147,%zmm1,%zmm1
	vpshufd	$57,%zmm3,%zmm3
	decq	%r8
	jnz	L$oop_avx512
	vpaddd	%zmm16,%zmm0,%zmm0
	vpaddd	%zmm17,%zmm1,%zmm1
	vpaddd	%zmm18,%zmm2,%zmm2
	vpaddd	%zmm19,%zmm3,%zmm3

	subq	$64,%rdx
	jb	L$tail64_avx512

	vpxor	0(%rsi),%xmm0,%xmm4
	vpxor	16(%rsi),%xmm1,%xmm5
	vpxor	32(%rsi),%xmm2,%xmm6
	vpxor	48(%rsi),%xmm3,%xmm7
	leaq	64(%rsi),%rsi

	vmovdqu	%xmm4,0(%rdi)
	vmovdqu	%xmm5,16(%rdi)
	vmovdqu	%xmm6,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	leaq	64(%rdi),%rdi

	jz	L$done_avx512

	vextracti32x4	$1,%zmm0,%xmm4
	vextracti32x4	$1,%zmm1,%xmm5
	vextracti32x4	$1,%zmm2,%xmm6
	vextracti32x4	$1,%zmm3,%xmm7

	subq	$64,%rdx
	jb	L$tail_avx512

	vpxor	0(%rsi),%xmm4,%xmm4
	vpxor	16(%rsi),%xmm5,%xmm5
	vpxor	32(%rsi),%xmm6,%xmm6
	vpxor	48(%rsi),%xmm7,%xmm7
	leaq	64(%rsi),%rsi

	vmovdqu	%xmm4,0(%rdi)
	vmovdqu	%xmm5,16(%rdi)
	vmovdqu	%xmm6,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	leaq	64(%rdi),%rdi

	jz	L$done_avx512

	vextracti32x4	$2,%zmm0,%xmm4
	vextracti32x4	$2,%zmm1,%xmm5
	vextracti32x4	$2,%zmm2,%xmm6
	vextracti32x4	$2,%zmm3,%xmm7

	subq	$64,%rdx
	jb	L$tail_avx512

	vpxor	0(%rsi),%xmm4,%xmm4
	vpxor	16(%rsi),%xmm5,%xmm5
	vpxor	32(%rsi),%xmm6,%xmm6
	vpxor	48(%rsi),%xmm7,%xmm7
	leaq	64(%rsi),%rsi

	vmovdqu	%xmm4,0(%rdi)
	vmovdqu	%xmm5,16(%rdi)
	vmovdqu	%xmm6,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	leaq	64(%rdi),%rdi

	jz	L$done_avx512

	vextracti32x4	$3,%zmm0,%xmm4
	vextracti32x4	$3,%zmm1,%xmm5
	vextracti32x4	$3,%zmm2,%xmm6
	vextracti32x4	$3,%zmm3,%xmm7

	subq	$64,%rdx
	jb	L$tail_avx512

	vpxor	0(%rsi),%xmm4,%xmm4
	vpxor	16(%rsi),%xmm5,%xmm5
	vpxor	32(%rsi),%xmm6,%xmm6
	vpxor	48(%rsi),%xmm7,%xmm7
	leaq	64(%rsi),%rsi

	vmovdqu	%xmm4,0(%rdi)
	vmovdqu	%xmm5,16(%rdi)
	vmovdqu	%xmm6,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	leaq	64(%rdi),%rdi

	jnz	L$oop_outer_avx512

	jmp	L$done_avx512

.p2align	4
L$tail64_avx512:
	vmovdqa	%xmm0,0(%rsp)
	vmovdqa	%xmm1,16(%rsp)
	vmovdqa	%xmm2,32(%rsp)
	vmovdqa	%xmm3,48(%rsp)
	addq	$64,%rdx
	jmp	L$oop_tail_avx512

.p2align	4
L$tail_avx512:
	vmovdqa	%xmm4,0(%rsp)
	vmovdqa	%xmm5,16(%rsp)
	vmovdqa	%xmm6,32(%rsp)
	vmovdqa	%xmm7,48(%rsp)
	addq	$64,%rdx

L$oop_tail_avx512:
	movzbl	(%rsi,%r8,1),%eax
	movzbl	(%rsp,%r8,1),%ecx
	leaq	1(%r8),%r8
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r8,1)
	decq	%rdx
	jnz	L$oop_tail_avx512

	vmovdqu32	%zmm16,0(%rsp)

L$done_avx512:
	vzeroall
	leaq	(%r9),%rsp

L$avx512_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_avx512vl:

L$ChaCha20_avx512vl:
	movq	%rsp,%r9

	cmpq	$128,%rdx
	ja	L$ChaCha20_8xvl

	subq	$64+8,%rsp
	vbroadcasti128	L$sigma(%rip),%ymm0
	vbroadcasti128	(%rcx),%ymm1
	vbroadcasti128	16(%rcx),%ymm2
	vbroadcasti128	(%r8),%ymm3

	vmovdqa32	%ymm0,%ymm16
	vmovdqa32	%ymm1,%ymm17
	vmovdqa32	%ymm2,%ymm18
	vpaddd	L$zeroz(%rip),%ymm3,%ymm3
	vmovdqa32	L$twoy(%rip),%ymm20
	movq	$10,%r8
	vmovdqa32	%ymm3,%ymm19
	jmp	L$oop_avx512vl

.p2align	4
L$oop_outer_avx512vl:
	vmovdqa32	%ymm18,%ymm2
	vpaddd	%ymm20,%ymm19,%ymm3
	movq	$10,%r8
	vmovdqa32	%ymm3,%ymm19
	jmp	L$oop_avx512vl

.p2align	5
L$oop_avx512vl:
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$16,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$12,%ymm1,%ymm1
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$8,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$7,%ymm1,%ymm1
	vpshufd	$78,%ymm2,%ymm2
	vpshufd	$57,%ymm1,%ymm1
	vpshufd	$147,%ymm3,%ymm3
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$16,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$12,%ymm1,%ymm1
	vpaddd	%ymm1,%ymm0,%ymm0
	vpxor	%ymm0,%ymm3,%ymm3
	vprold	$8,%ymm3,%ymm3
	vpaddd	%ymm3,%ymm2,%ymm2
	vpxor	%ymm2,%ymm1,%ymm1
	vprold	$7,%ymm1,%ymm1
	vpshufd	$78,%ymm2,%ymm2
	vpshufd	$147,%ymm1,%ymm1
	vpshufd	$57,%ymm3,%ymm3
	decq	%r8
	jnz	L$oop_avx512vl
	vpaddd	%ymm16,%ymm0,%ymm0
	vpaddd	%ymm17,%ymm1,%ymm1
	vpaddd	%ymm18,%ymm2,%ymm2
	vpaddd	%ymm19,%ymm3,%ymm3

	subq	$64,%rdx
	jb	L$tail64_avx512vl

	vpxor	0(%rsi),%xmm0,%xmm4
	vpxor	16(%rsi),%xmm1,%xmm5
	vpxor	32(%rsi),%xmm2,%xmm6
	vpxor	48(%rsi),%xmm3,%xmm7
	leaq	64(%rsi),%rsi

	vmovdqu	%xmm4,0(%rdi)
	vmovdqu	%xmm5,16(%rdi)
	vmovdqu	%xmm6,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	leaq	64(%rdi),%rdi

	jz	L$done_avx512vl

	vextracti128	$1,%ymm0,%xmm4
	vextracti128	$1,%ymm1,%xmm5
	vextracti128	$1,%ymm2,%xmm6
	vextracti128	$1,%ymm3,%xmm7

	subq	$64,%rdx
	jb	L$tail_avx512vl

	vpxor	0(%rsi),%xmm4,%xmm4
	vpxor	16(%rsi),%xmm5,%xmm5
	vpxor	32(%rsi),%xmm6,%xmm6
	vpxor	48(%rsi),%xmm7,%xmm7
	leaq	64(%rsi),%rsi

	vmovdqu	%xmm4,0(%rdi)
	vmovdqu	%xmm5,16(%rdi)
	vmovdqu	%xmm6,32(%rdi)
	vmovdqu	%xmm7,48(%rdi)
	leaq	64(%rdi),%rdi

	vmovdqa32	%ymm16,%ymm0
	vmovdqa32	%ymm17,%ymm1
	jnz	L$oop_outer_avx512vl

	jmp	L$done_avx512vl

.p2align	4
L$tail64_avx512vl:
	vmovdqa	%xmm0,0(%rsp)
	vmovdqa	%xmm1,16(%rsp)
	vmovdqa	%xmm2,32(%rsp)
	vmovdqa	%xmm3,48(%rsp)
	addq	$64,%rdx
	jmp	L$oop_tail_avx512vl

.p2align	4
L$tail_avx512vl:
	vmovdqa	%xmm4,0(%rsp)
	vmovdqa	%xmm5,16(%rsp)
	vmovdqa	%xmm6,32(%rsp)
	vmovdqa	%xmm7,48(%rsp)
	addq	$64,%rdx

L$oop_tail_avx512vl:
	movzbl	(%rsi,%r8,1),%eax
	movzbl	(%rsp,%r8,1),%ecx
	leaq	1(%r8),%r8
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r8,1)
	decq	%rdx
	jnz	L$oop_tail_avx512vl

	vmovdqu32	%ymm16,0(%rsp)
	vmovdqu32	%ymm16,32(%rsp)

L$done_avx512vl:
	vzeroall
	leaq	(%r9),%rsp

L$avx512vl_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_16x:

L$ChaCha20_16x:
	movq	%rsp,%r9

	subq	$64+8,%rsp
	andq	$-64,%rsp
	vzeroupper

	leaq	L$sigma(%rip),%r10
	vbroadcasti32x4	(%r10),%zmm3
	vbroadcasti32x4	(%rcx),%zmm7
	vbroadcasti32x4	16(%rcx),%zmm11
	vbroadcasti32x4	(%r8),%zmm15

	vpshufd	$0x00,%zmm3,%zmm0
	vpshufd	$0x55,%zmm3,%zmm1
	vpshufd	$0xaa,%zmm3,%zmm2
	vpshufd	$0xff,%zmm3,%zmm3
	vmovdqa64	%zmm0,%zmm16
	vmovdqa64	%zmm1,%zmm17
	vmovdqa64	%zmm2,%zmm18
	vmovdqa64	%zmm3,%zmm19

	vpshufd	$0x00,%zmm7,%zmm4
	vpshufd	$0x55,%zmm7,%zmm5
	vpshufd	$0xaa,%zmm7,%zmm6
	vpshufd	$0xff,%zmm7,%zmm7
	vmovdqa64	%zmm4,%zmm20
	vmovdqa64	%zmm5,%zmm21
	vmovdqa64	%zmm6,%zmm22
	vmovdqa64	%zmm7,%zmm23

	vpshufd	$0x00,%zmm11,%zmm8
	vpshufd	$0x55,%zmm11,%zmm9
	vpshufd	$0xaa,%zmm11,%zmm10
	vpshufd	$0xff,%zmm11,%zmm11
	vmovdqa64	%zmm8,%zmm24
	vmovdqa64	%zmm9,%zmm25
	vmovdqa64	%zmm10,%zmm26
	vmovdqa64	%zmm11,%zmm27

	vpshufd	$0x00,%zmm15,%zmm12
	vpshufd	$0x55,%zmm15,%zmm13
	vpshufd	$0xaa,%zmm15,%zmm14
	vpshufd	$0xff,%zmm15,%zmm15
	vpaddd	L$incz(%rip),%zmm12,%zmm12
	vmovdqa64	%zmm12,%zmm28
	vmovdqa64	%zmm13,%zmm29
	vmovdqa64	%zmm14,%zmm30
	vmovdqa64	%zmm15,%zmm31

	movl	$10,%eax
	jmp	L$oop16x

.p2align	5
L$oop_outer16x:
	vpbroadcastd	0(%r10),%zmm0
	vpbroadcastd	4(%r10),%zmm1
	vpbroadcastd	8(%r10),%zmm2
	vpbroadcastd	12(%r10),%zmm3
	vpaddd	L$sixteen(%rip),%zmm28,%zmm28
	vmovdqa64	%zmm20,%zmm4
	vmovdqa64	%zmm21,%zmm5
	vmovdqa64	%zmm22,%zmm6
	vmovdqa64	%zmm23,%zmm7
	vmovdqa64	%zmm24,%zmm8
	vmovdqa64	%zmm25,%zmm9
	vmovdqa64	%zmm26,%zmm10
	vmovdqa64	%zmm27,%zmm11
	vmovdqa64	%zmm28,%zmm12
	vmovdqa64	%zmm29,%zmm13
	vmovdqa64	%zmm30,%zmm14
	vmovdqa64	%zmm31,%zmm15

	vmovdqa64	%zmm0,%zmm16
	vmovdqa64	%zmm1,%zmm17
	vmovdqa64	%zmm2,%zmm18
	vmovdqa64	%zmm3,%zmm19

	movl	$10,%eax
	jmp	L$oop16x

.p2align	5
L$oop16x:
	vpaddd	%zmm4,%zmm0,%zmm0
	vpaddd	%zmm5,%zmm1,%zmm1
	vpaddd	%zmm6,%zmm2,%zmm2
	vpaddd	%zmm7,%zmm3,%zmm3
	vpxord	%zmm0,%zmm12,%zmm12
	vpxord	%zmm1,%zmm13,%zmm13
	vpxord	%zmm2,%zmm14,%zmm14
	vpxord	%zmm3,%zmm15,%zmm15
	vprold	$16,%zmm12,%zmm12
	vprold	$16,%zmm13,%zmm13
	vprold	$16,%zmm14,%zmm14
	vprold	$16,%zmm15,%zmm15
	vpaddd	%zmm12,%zmm8,%zmm8
	vpaddd	%zmm13,%zmm9,%zmm9
	vpaddd	%zmm14,%zmm10,%zmm10
	vpaddd	%zmm15,%zmm11,%zmm11
	vpxord	%zmm8,%zmm4,%zmm4
	vpxord	%zmm9,%zmm5,%zmm5
	vpxord	%zmm10,%zmm6,%zmm6
	vpxord	%zmm11,%zmm7,%zmm7
	vprold	$12,%zmm4,%zmm4
	vprold	$12,%zmm5,%zmm5
	vprold	$12,%zmm6,%zmm6
	vprold	$12,%zmm7,%zmm7
	vpaddd	%zmm4,%zmm0,%zmm0
	vpaddd	%zmm5,%zmm1,%zmm1
	vpaddd	%zmm6,%zmm2,%zmm2
	vpaddd	%zmm7,%zmm3,%zmm3
	vpxord	%zmm0,%zmm12,%zmm12
	vpxord	%zmm1,%zmm13,%zmm13
	vpxord	%zmm2,%zmm14,%zmm14
	vpxord	%zmm3,%zmm15,%zmm15
	vprold	$8,%zmm12,%zmm12
	vprold	$8,%zmm13,%zmm13
	vprold	$8,%zmm14,%zmm14
	vprold	$8,%zmm15,%zmm15
	vpaddd	%zmm12,%zmm8,%zmm8
	vpaddd	%zmm13,%zmm9,%zmm9
	vpaddd	%zmm14,%zmm10,%zmm10
	vpaddd	%zmm15,%zmm11,%zmm11
	vpxord	%zmm8,%zmm4,%zmm4
	vpxord	%zmm9,%zmm5,%zmm5
	vpxord	%zmm10,%zmm6,%zmm6
	vpxord	%zmm11,%zmm7,%zmm7
	vprold	$7,%zmm4,%zmm4
	vprold	$7,%zmm5,%zmm5
	vprold	$7,%zmm6,%zmm6
	vprold	$7,%zmm7,%zmm7
	vpaddd	%zmm5,%zmm0,%zmm0
	vpaddd	%zmm6,%zmm1,%zmm1
	vpaddd	%zmm7,%zmm2,%zmm2
	vpaddd	%zmm4,%zmm3,%zmm3
	vpxord	%zmm0,%zmm15,%zmm15
	vpxord	%zmm1,%zmm12,%zmm12
	vpxord	%zmm2,%zmm13,%zmm13
	vpxord	%zmm3,%zmm14,%zmm14
	vprold	$16,%zmm15,%zmm15
	vprold	$16,%zmm12,%zmm12
	vprold	$16,%zmm13,%zmm13
	vprold	$16,%zmm14,%zmm14
	vpaddd	%zmm15,%zmm10,%zmm10
	vpaddd	%zmm12,%zmm11,%zmm11
	vpaddd	%zmm13,%zmm8,%zmm8
	vpaddd	%zmm14,%zmm9,%zmm9
	vpxord	%zmm10,%zmm5,%zmm5
	vpxord	%zmm11,%zmm6,%zmm6
	vpxord	%zmm8,%zmm7,%zmm7
	vpxord	%zmm9,%zmm4,%zmm4
	vprold	$12,%zmm5,%zmm5
	vprold	$12,%zmm6,%zmm6
	vprold	$12,%zmm7,%zmm7
	vprold	$12,%zmm4,%zmm4
	vpaddd	%zmm5,%zmm0,%zmm0
	vpaddd	%zmm6,%zmm1,%zmm1
	vpaddd	%zmm7,%zmm2,%zmm2
	vpaddd	%zmm4,%zmm3,%zmm3
	vpxord	%zmm0,%zmm15,%zmm15
	vpxord	%zmm1,%zmm12,%zmm12
	vpxord	%zmm2,%zmm13,%zmm13
	vpxord	%zmm3,%zmm14,%zmm14
	vprold	$8,%zmm15,%zmm15
	vprold	$8,%zmm12,%zmm12
	vprold	$8,%zmm13,%zmm13
	vprold	$8,%zmm14,%zmm14
	vpaddd	%zmm15,%zmm10,%zmm10
	vpaddd	%zmm12,%zmm11,%zmm11
	vpaddd	%zmm13,%zmm8,%zmm8
	vpaddd	%zmm14,%zmm9,%zmm9
	vpxord	%zmm10,%zmm5,%zmm5
	vpxord	%zmm11,%zmm6,%zmm6
	vpxord	%zmm8,%zmm7,%zmm7
	vpxord	%zmm9,%zmm4,%zmm4
	vprold	$7,%zmm5,%zmm5
	vprold	$7,%zmm6,%zmm6
	vprold	$7,%zmm7,%zmm7
	vprold	$7,%zmm4,%zmm4
	decl	%eax
	jnz	L$oop16x

	vpaddd	%zmm16,%zmm0,%zmm0
	vpaddd	%zmm17,%zmm1,%zmm1
	vpaddd	%zmm18,%zmm2,%zmm2
	vpaddd	%zmm19,%zmm3,%zmm3

	vpunpckldq	%zmm1,%zmm0,%zmm18
	vpunpckldq	%zmm3,%zmm2,%zmm19
	vpunpckhdq	%zmm1,%zmm0,%zmm0
	vpunpckhdq	%zmm3,%zmm2,%zmm2
	vpunpcklqdq	%zmm19,%zmm18,%zmm1
	vpunpckhqdq	%zmm19,%zmm18,%zmm18
	vpunpcklqdq	%zmm2,%zmm0,%zmm3
	vpunpckhqdq	%zmm2,%zmm0,%zmm0
	vpaddd	%zmm20,%zmm4,%zmm4
	vpaddd	%zmm21,%zmm5,%zmm5
	vpaddd	%zmm22,%zmm6,%zmm6
	vpaddd	%zmm23,%zmm7,%zmm7

	vpunpckldq	%zmm5,%zmm4,%zmm2
	vpunpckldq	%zmm7,%zmm6,%zmm19
	vpunpckhdq	%zmm5,%zmm4,%zmm4
	vpunpckhdq	%zmm7,%zmm6,%zmm6
	vpunpcklqdq	%zmm19,%zmm2,%zmm5
	vpunpckhqdq	%zmm19,%zmm2,%zmm2
	vpunpcklqdq	%zmm6,%zmm4,%zmm7
	vpunpckhqdq	%zmm6,%zmm4,%zmm4
	vshufi32x4	$0x44,%zmm5,%zmm1,%zmm19
	vshufi32x4	$0xee,%zmm5,%zmm1,%zmm5
	vshufi32x4	$0x44,%zmm2,%zmm18,%zmm1
	vshufi32x4	$0xee,%zmm2,%zmm18,%zmm2
	vshufi32x4	$0x44,%zmm7,%zmm3,%zmm18
	vshufi32x4	$0xee,%zmm7,%zmm3,%zmm7
	vshufi32x4	$0x44,%zmm4,%zmm0,%zmm3
	vshufi32x4	$0xee,%zmm4,%zmm0,%zmm4
	vpaddd	%zmm24,%zmm8,%zmm8
	vpaddd	%zmm25,%zmm9,%zmm9
	vpaddd	%zmm26,%zmm10,%zmm10
	vpaddd	%zmm27,%zmm11,%zmm11

	vpunpckldq	%zmm9,%zmm8,%zmm6
	vpunpckldq	%zmm11,%zmm10,%zmm0
	vpunpckhdq	%zmm9,%zmm8,%zmm8
	vpunpckhdq	%zmm11,%zmm10,%zmm10
	vpunpcklqdq	%zmm0,%zmm6,%zmm9
	vpunpckhqdq	%zmm0,%zmm6,%zmm6
	vpunpcklqdq	%zmm10,%zmm8,%zmm11
	vpunpckhqdq	%zmm10,%zmm8,%zmm8
	vpaddd	%zmm28,%zmm12,%zmm12
	vpaddd	%zmm29,%zmm13,%zmm13
	vpaddd	%zmm30,%zmm14,%zmm14
	vpaddd	%zmm31,%zmm15,%zmm15

	vpunpckldq	%zmm13,%zmm12,%zmm10
	vpunpckldq	%zmm15,%zmm14,%zmm0
	vpunpckhdq	%zmm13,%zmm12,%zmm12
	vpunpckhdq	%zmm15,%zmm14,%zmm14
	vpunpcklqdq	%zmm0,%zmm10,%zmm13
	vpunpckhqdq	%zmm0,%zmm10,%zmm10
	vpunpcklqdq	%zmm14,%zmm12,%zmm15
	vpunpckhqdq	%zmm14,%zmm12,%zmm12
	vshufi32x4	$0x44,%zmm13,%zmm9,%zmm0
	vshufi32x4	$0xee,%zmm13,%zmm9,%zmm13
	vshufi32x4	$0x44,%zmm10,%zmm6,%zmm9
	vshufi32x4	$0xee,%zmm10,%zmm6,%zmm10
	vshufi32x4	$0x44,%zmm15,%zmm11,%zmm6
	vshufi32x4	$0xee,%zmm15,%zmm11,%zmm15
	vshufi32x4	$0x44,%zmm12,%zmm8,%zmm11
	vshufi32x4	$0xee,%zmm12,%zmm8,%zmm12
	vshufi32x4	$0x88,%zmm0,%zmm19,%zmm16
	vshufi32x4	$0xdd,%zmm0,%zmm19,%zmm19
	vshufi32x4	$0x88,%zmm13,%zmm5,%zmm0
	vshufi32x4	$0xdd,%zmm13,%zmm5,%zmm13
	vshufi32x4	$0x88,%zmm9,%zmm1,%zmm17
	vshufi32x4	$0xdd,%zmm9,%zmm1,%zmm1
	vshufi32x4	$0x88,%zmm10,%zmm2,%zmm9
	vshufi32x4	$0xdd,%zmm10,%zmm2,%zmm10
	vshufi32x4	$0x88,%zmm6,%zmm18,%zmm14
	vshufi32x4	$0xdd,%zmm6,%zmm18,%zmm18
	vshufi32x4	$0x88,%zmm15,%zmm7,%zmm6
	vshufi32x4	$0xdd,%zmm15,%zmm7,%zmm15
	vshufi32x4	$0x88,%zmm11,%zmm3,%zmm8
	vshufi32x4	$0xdd,%zmm11,%zmm3,%zmm3
	vshufi32x4	$0x88,%zmm12,%zmm4,%zmm11
	vshufi32x4	$0xdd,%zmm12,%zmm4,%zmm12
	cmpq	$1024,%rdx
	jb	L$tail16x

	vpxord	0(%rsi),%zmm16,%zmm16
	vpxord	64(%rsi),%zmm17,%zmm17
	vpxord	128(%rsi),%zmm14,%zmm14
	vpxord	192(%rsi),%zmm8,%zmm8
	vmovdqu32	%zmm16,0(%rdi)
	vmovdqu32	%zmm17,64(%rdi)
	vmovdqu32	%zmm14,128(%rdi)
	vmovdqu32	%zmm8,192(%rdi)

	vpxord	256(%rsi),%zmm19,%zmm19
	vpxord	320(%rsi),%zmm1,%zmm1
	vpxord	384(%rsi),%zmm18,%zmm18
	vpxord	448(%rsi),%zmm3,%zmm3
	vmovdqu32	%zmm19,256(%rdi)
	vmovdqu32	%zmm1,320(%rdi)
	vmovdqu32	%zmm18,384(%rdi)
	vmovdqu32	%zmm3,448(%rdi)

	vpxord	512(%rsi),%zmm0,%zmm0
	vpxord	576(%rsi),%zmm9,%zmm9
	vpxord	640(%rsi),%zmm6,%zmm6
	vpxord	704(%rsi),%zmm11,%zmm11
	vmovdqu32	%zmm0,512(%rdi)
	vmovdqu32	%zmm9,576(%rdi)
	vmovdqu32	%zmm6,640(%rdi)
	vmovdqu32	%zmm11,704(%rdi)

	vpxord	768(%rsi),%zmm13,%zmm13
	vpxord	832(%rsi),%zmm10,%zmm10
	vpxord	896(%rsi),%zmm15,%zmm15
	vpxord	960(%rsi),%zmm12,%zmm12
	leaq	1024(%rsi),%rsi
	vmovdqu32	%zmm13,768(%rdi)
	vmovdqu32	%zmm10,832(%rdi)
	vmovdqu32	%zmm15,896(%rdi)
	vmovdqu32	%zmm12,960(%rdi)
	leaq	1024(%rdi),%rdi

	subq	$1024,%rdx
	jnz	L$oop_outer16x

	jmp	L$done16x

.p2align	5
L$tail16x:
	xorq	%r10,%r10
	subq	%rsi,%rdi
	cmpq	$64,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm16,%zmm16
	vmovdqu32	%zmm16,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm17,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$128,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm17,%zmm17
	vmovdqu32	%zmm17,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm14,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$192,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm14,%zmm14
	vmovdqu32	%zmm14,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm8,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$256,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm8,%zmm8
	vmovdqu32	%zmm8,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm19,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$320,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm19,%zmm19
	vmovdqu32	%zmm19,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm1,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$384,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm1,%zmm1
	vmovdqu32	%zmm1,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm18,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$448,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm18,%zmm18
	vmovdqu32	%zmm18,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm3,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$512,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm3,%zmm3
	vmovdqu32	%zmm3,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm0,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$576,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm0,%zmm0
	vmovdqu32	%zmm0,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm9,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$640,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm9,%zmm9
	vmovdqu32	%zmm9,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm6,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$704,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm6,%zmm6
	vmovdqu32	%zmm6,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm11,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$768,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm11,%zmm11
	vmovdqu32	%zmm11,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm13,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$832,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm13,%zmm13
	vmovdqu32	%zmm13,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm10,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$896,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm10,%zmm10
	vmovdqu32	%zmm10,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm15,%zmm16
	leaq	64(%rsi),%rsi

	cmpq	$960,%rdx
	jb	L$ess_than_64_16x
	vpxord	(%rsi),%zmm15,%zmm15
	vmovdqu32	%zmm15,(%rdi,%rsi,1)
	je	L$done16x
	vmovdqa32	%zmm12,%zmm16
	leaq	64(%rsi),%rsi

L$ess_than_64_16x:
	vmovdqa32	%zmm16,0(%rsp)
	leaq	(%rdi,%rsi,1),%rdi
	andq	$63,%rdx

L$oop_tail16x:
	movzbl	(%rsi,%r10,1),%eax
	movzbl	(%rsp,%r10,1),%ecx
	leaq	1(%r10),%r10
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r10,1)
	decq	%rdx
	jnz	L$oop_tail16x

	vpxord	%zmm16,%zmm16,%zmm16
	vmovdqa32	%zmm16,0(%rsp)

L$done16x:
	vzeroall
	leaq	(%r9),%rsp

L$16x_epilogue:
	.byte	0xf3,0xc3



.p2align	5
ChaCha20_8xvl:

L$ChaCha20_8xvl:
	movq	%rsp,%r9

	subq	$64+8,%rsp
	andq	$-64,%rsp
	vzeroupper

	leaq	L$sigma(%rip),%r10
	vbroadcasti128	(%r10),%ymm3
	vbroadcasti128	(%rcx),%ymm7
	vbroadcasti128	16(%rcx),%ymm11
	vbroadcasti128	(%r8),%ymm15

	vpshufd	$0x00,%ymm3,%ymm0
	vpshufd	$0x55,%ymm3,%ymm1
	vpshufd	$0xaa,%ymm3,%ymm2
	vpshufd	$0xff,%ymm3,%ymm3
	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm1,%ymm17
	vmovdqa64	%ymm2,%ymm18
	vmovdqa64	%ymm3,%ymm19

	vpshufd	$0x00,%ymm7,%ymm4
	vpshufd	$0x55,%ymm7,%ymm5
	vpshufd	$0xaa,%ymm7,%ymm6
	vpshufd	$0xff,%ymm7,%ymm7
	vmovdqa64	%ymm4,%ymm20
	vmovdqa64	%ymm5,%ymm21
	vmovdqa64	%ymm6,%ymm22
	vmovdqa64	%ymm7,%ymm23

	vpshufd	$0x00,%ymm11,%ymm8
	vpshufd	$0x55,%ymm11,%ymm9
	vpshufd	$0xaa,%ymm11,%ymm10
	vpshufd	$0xff,%ymm11,%ymm11
	vmovdqa64	%ymm8,%ymm24
	vmovdqa64	%ymm9,%ymm25
	vmovdqa64	%ymm10,%ymm26
	vmovdqa64	%ymm11,%ymm27

	vpshufd	$0x00,%ymm15,%ymm12
	vpshufd	$0x55,%ymm15,%ymm13
	vpshufd	$0xaa,%ymm15,%ymm14
	vpshufd	$0xff,%ymm15,%ymm15
	vpaddd	L$incy(%rip),%ymm12,%ymm12
	vmovdqa64	%ymm12,%ymm28
	vmovdqa64	%ymm13,%ymm29
	vmovdqa64	%ymm14,%ymm30
	vmovdqa64	%ymm15,%ymm31

	movl	$10,%eax
	jmp	L$oop8xvl

.p2align	5
L$oop_outer8xvl:


	vpbroadcastd	8(%r10),%ymm2
	vpbroadcastd	12(%r10),%ymm3
	vpaddd	L$eight(%rip),%ymm28,%ymm28
	vmovdqa64	%ymm20,%ymm4
	vmovdqa64	%ymm21,%ymm5
	vmovdqa64	%ymm22,%ymm6
	vmovdqa64	%ymm23,%ymm7
	vmovdqa64	%ymm24,%ymm8
	vmovdqa64	%ymm25,%ymm9
	vmovdqa64	%ymm26,%ymm10
	vmovdqa64	%ymm27,%ymm11
	vmovdqa64	%ymm28,%ymm12
	vmovdqa64	%ymm29,%ymm13
	vmovdqa64	%ymm30,%ymm14
	vmovdqa64	%ymm31,%ymm15

	vmovdqa64	%ymm0,%ymm16
	vmovdqa64	%ymm1,%ymm17
	vmovdqa64	%ymm2,%ymm18
	vmovdqa64	%ymm3,%ymm19

	movl	$10,%eax
	jmp	L$oop8xvl

.p2align	5
L$oop8xvl:
	vpaddd	%ymm4,%ymm0,%ymm0
	vpaddd	%ymm5,%ymm1,%ymm1
	vpaddd	%ymm6,%ymm2,%ymm2
	vpaddd	%ymm7,%ymm3,%ymm3
	vpxor	%ymm0,%ymm12,%ymm12
	vpxor	%ymm1,%ymm13,%ymm13
	vpxor	%ymm2,%ymm14,%ymm14
	vpxor	%ymm3,%ymm15,%ymm15
	vprold	$16,%ymm12,%ymm12
	vprold	$16,%ymm13,%ymm13
	vprold	$16,%ymm14,%ymm14
	vprold	$16,%ymm15,%ymm15
	vpaddd	%ymm12,%ymm8,%ymm8
	vpaddd	%ymm13,%ymm9,%ymm9
	vpaddd	%ymm14,%ymm10,%ymm10
	vpaddd	%ymm15,%ymm11,%ymm11
	vpxor	%ymm8,%ymm4,%ymm4
	vpxor	%ymm9,%ymm5,%ymm5
	vpxor	%ymm10,%ymm6,%ymm6
	vpxor	%ymm11,%ymm7,%ymm7
	vprold	$12,%ymm4,%ymm4
	vprold	$12,%ymm5,%ymm5
	vprold	$12,%ymm6,%ymm6
	vprold	$12,%ymm7,%ymm7
	vpaddd	%ymm4,%ymm0,%ymm0
	vpaddd	%ymm5,%ymm1,%ymm1
	vpaddd	%ymm6,%ymm2,%ymm2
	vpaddd	%ymm7,%ymm3,%ymm3
	vpxor	%ymm0,%ymm12,%ymm12
	vpxor	%ymm1,%ymm13,%ymm13
	vpxor	%ymm2,%ymm14,%ymm14
	vpxor	%ymm3,%ymm15,%ymm15
	vprold	$8,%ymm12,%ymm12
	vprold	$8,%ymm13,%ymm13
	vprold	$8,%ymm14,%ymm14
	vprold	$8,%ymm15,%ymm15
	vpaddd	%ymm12,%ymm8,%ymm8
	vpaddd	%ymm13,%ymm9,%ymm9
	vpaddd	%ymm14,%ymm10,%ymm10
	vpaddd	%ymm15,%ymm11,%ymm11
	vpxor	%ymm8,%ymm4,%ymm4
	vpxor	%ymm9,%ymm5,%ymm5
	vpxor	%ymm10,%ymm6,%ymm6
	vpxor	%ymm11,%ymm7,%ymm7
	vprold	$7,%ymm4,%ymm4
	vprold	$7,%ymm5,%ymm5
	vprold	$7,%ymm6,%ymm6
	vprold	$7,%ymm7,%ymm7
	vpaddd	%ymm5,%ymm0,%ymm0
	vpaddd	%ymm6,%ymm1,%ymm1
	vpaddd	%ymm7,%ymm2,%ymm2
	vpaddd	%ymm4,%ymm3,%ymm3
	vpxor	%ymm0,%ymm15,%ymm15
	vpxor	%ymm1,%ymm12,%ymm12
	vpxor	%ymm2,%ymm13,%ymm13
	vpxor	%ymm3,%ymm14,%ymm14
	vprold	$16,%ymm15,%ymm15
	vprold	$16,%ymm12,%ymm12
	vprold	$16,%ymm13,%ymm13
	vprold	$16,%ymm14,%ymm14
	vpaddd	%ymm15,%ymm10,%ymm10
	vpaddd	%ymm12,%ymm11,%ymm11
	vpaddd	%ymm13,%ymm8,%ymm8
	vpaddd	%ymm14,%ymm9,%ymm9
	vpxor	%ymm10,%ymm5,%ymm5
	vpxor	%ymm11,%ymm6,%ymm6
	vpxor	%ymm8,%ymm7,%ymm7
	vpxor	%ymm9,%ymm4,%ymm4
	vprold	$12,%ymm5,%ymm5
	vprold	$12,%ymm6,%ymm6
	vprold	$12,%ymm7,%ymm7
	vprold	$12,%ymm4,%ymm4
	vpaddd	%ymm5,%ymm0,%ymm0
	vpaddd	%ymm6,%ymm1,%ymm1
	vpaddd	%ymm7,%ymm2,%ymm2
	vpaddd	%ymm4,%ymm3,%ymm3
	vpxor	%ymm0,%ymm15,%ymm15
	vpxor	%ymm1,%ymm12,%ymm12
	vpxor	%ymm2,%ymm13,%ymm13
	vpxor	%ymm3,%ymm14,%ymm14
	vprold	$8,%ymm15,%ymm15
	vprold	$8,%ymm12,%ymm12
	vprold	$8,%ymm13,%ymm13
	vprold	$8,%ymm14,%ymm14
	vpaddd	%ymm15,%ymm10,%ymm10
	vpaddd	%ymm12,%ymm11,%ymm11
	vpaddd	%ymm13,%ymm8,%ymm8
	vpaddd	%ymm14,%ymm9,%ymm9
	vpxor	%ymm10,%ymm5,%ymm5
	vpxor	%ymm11,%ymm6,%ymm6
	vpxor	%ymm8,%ymm7,%ymm7
	vpxor	%ymm9,%ymm4,%ymm4
	vprold	$7,%ymm5,%ymm5
	vprold	$7,%ymm6,%ymm6
	vprold	$7,%ymm7,%ymm7
	vprold	$7,%ymm4,%ymm4
	decl	%eax
	jnz	L$oop8xvl

	vpaddd	%ymm16,%ymm0,%ymm0
	vpaddd	%ymm17,%ymm1,%ymm1
	vpaddd	%ymm18,%ymm2,%ymm2
	vpaddd	%ymm19,%ymm3,%ymm3

	vpunpckldq	%ymm1,%ymm0,%ymm18
	vpunpckldq	%ymm3,%ymm2,%ymm19
	vpunpckhdq	%ymm1,%ymm0,%ymm0
	vpunpckhdq	%ymm3,%ymm2,%ymm2
	vpunpcklqdq	%ymm19,%ymm18,%ymm1
	vpunpckhqdq	%ymm19,%ymm18,%ymm18
	vpunpcklqdq	%ymm2,%ymm0,%ymm3
	vpunpckhqdq	%ymm2,%ymm0,%ymm0
	vpaddd	%ymm20,%ymm4,%ymm4
	vpaddd	%ymm21,%ymm5,%ymm5
	vpaddd	%ymm22,%ymm6,%ymm6
	vpaddd	%ymm23,%ymm7,%ymm7

	vpunpckldq	%ymm5,%ymm4,%ymm2
	vpunpckldq	%ymm7,%ymm6,%ymm19
	vpunpckhdq	%ymm5,%ymm4,%ymm4
	vpunpckhdq	%ymm7,%ymm6,%ymm6
	vpunpcklqdq	%ymm19,%ymm2,%ymm5
	vpunpckhqdq	%ymm19,%ymm2,%ymm2
	vpunpcklqdq	%ymm6,%ymm4,%ymm7
	vpunpckhqdq	%ymm6,%ymm4,%ymm4
	vshufi32x4	$0,%ymm5,%ymm1,%ymm19
	vshufi32x4	$3,%ymm5,%ymm1,%ymm5
	vshufi32x4	$0,%ymm2,%ymm18,%ymm1
	vshufi32x4	$3,%ymm2,%ymm18,%ymm2
	vshufi32x4	$0,%ymm7,%ymm3,%ymm18
	vshufi32x4	$3,%ymm7,%ymm3,%ymm7
	vshufi32x4	$0,%ymm4,%ymm0,%ymm3
	vshufi32x4	$3,%ymm4,%ymm0,%ymm4
	vpaddd	%ymm24,%ymm8,%ymm8
	vpaddd	%ymm25,%ymm9,%ymm9
	vpaddd	%ymm26,%ymm10,%ymm10
	vpaddd	%ymm27,%ymm11,%ymm11

	vpunpckldq	%ymm9,%ymm8,%ymm6
	vpunpckldq	%ymm11,%ymm10,%ymm0
	vpunpckhdq	%ymm9,%ymm8,%ymm8
	vpunpckhdq	%ymm11,%ymm10,%ymm10
	vpunpcklqdq	%ymm0,%ymm6,%ymm9
	vpunpckhqdq	%ymm0,%ymm6,%ymm6
	vpunpcklqdq	%ymm10,%ymm8,%ymm11
	vpunpckhqdq	%ymm10,%ymm8,%ymm8
	vpaddd	%ymm28,%ymm12,%ymm12
	vpaddd	%ymm29,%ymm13,%ymm13
	vpaddd	%ymm30,%ymm14,%ymm14
	vpaddd	%ymm31,%ymm15,%ymm15

	vpunpckldq	%ymm13,%ymm12,%ymm10
	vpunpckldq	%ymm15,%ymm14,%ymm0
	vpunpckhdq	%ymm13,%ymm12,%ymm12
	vpunpckhdq	%ymm15,%ymm14,%ymm14
	vpunpcklqdq	%ymm0,%ymm10,%ymm13
	vpunpckhqdq	%ymm0,%ymm10,%ymm10
	vpunpcklqdq	%ymm14,%ymm12,%ymm15
	vpunpckhqdq	%ymm14,%ymm12,%ymm12
	vperm2i128	$0x20,%ymm13,%ymm9,%ymm0
	vperm2i128	$0x31,%ymm13,%ymm9,%ymm13
	vperm2i128	$0x20,%ymm10,%ymm6,%ymm9
	vperm2i128	$0x31,%ymm10,%ymm6,%ymm10
	vperm2i128	$0x20,%ymm15,%ymm11,%ymm6
	vperm2i128	$0x31,%ymm15,%ymm11,%ymm15
	vperm2i128	$0x20,%ymm12,%ymm8,%ymm11
	vperm2i128	$0x31,%ymm12,%ymm8,%ymm12
	cmpq	$512,%rdx
	jb	L$tail8xvl

	movl	$0x80,%eax
	vpxord	0(%rsi),%ymm19,%ymm19
	vpxor	32(%rsi),%ymm0,%ymm0
	vpxor	64(%rsi),%ymm5,%ymm5
	vpxor	96(%rsi),%ymm13,%ymm13
	leaq	(%rsi,%rax,1),%rsi
	vmovdqu32	%ymm19,0(%rdi)
	vmovdqu	%ymm0,32(%rdi)
	vmovdqu	%ymm5,64(%rdi)
	vmovdqu	%ymm13,96(%rdi)
	leaq	(%rdi,%rax,1),%rdi

	vpxor	0(%rsi),%ymm1,%ymm1
	vpxor	32(%rsi),%ymm9,%ymm9
	vpxor	64(%rsi),%ymm2,%ymm2
	vpxor	96(%rsi),%ymm10,%ymm10
	leaq	(%rsi,%rax,1),%rsi
	vmovdqu	%ymm1,0(%rdi)
	vmovdqu	%ymm9,32(%rdi)
	vmovdqu	%ymm2,64(%rdi)
	vmovdqu	%ymm10,96(%rdi)
	leaq	(%rdi,%rax,1),%rdi

	vpxord	0(%rsi),%ymm18,%ymm18
	vpxor	32(%rsi),%ymm6,%ymm6
	vpxor	64(%rsi),%ymm7,%ymm7
	vpxor	96(%rsi),%ymm15,%ymm15
	leaq	(%rsi,%rax,1),%rsi
	vmovdqu32	%ymm18,0(%rdi)
	vmovdqu	%ymm6,32(%rdi)
	vmovdqu	%ymm7,64(%rdi)
	vmovdqu	%ymm15,96(%rdi)
	leaq	(%rdi,%rax,1),%rdi

	vpxor	0(%rsi),%ymm3,%ymm3
	vpxor	32(%rsi),%ymm11,%ymm11
	vpxor	64(%rsi),%ymm4,%ymm4
	vpxor	96(%rsi),%ymm12,%ymm12
	leaq	(%rsi,%rax,1),%rsi
	vmovdqu	%ymm3,0(%rdi)
	vmovdqu	%ymm11,32(%rdi)
	vmovdqu	%ymm4,64(%rdi)
	vmovdqu	%ymm12,96(%rdi)
	leaq	(%rdi,%rax,1),%rdi

	vpbroadcastd	0(%r10),%ymm0
	vpbroadcastd	4(%r10),%ymm1

	subq	$512,%rdx
	jnz	L$oop_outer8xvl

	jmp	L$done8xvl

.p2align	5
L$tail8xvl:
	vmovdqa64	%ymm19,%ymm8
	xorq	%r10,%r10
	subq	%rsi,%rdi
	cmpq	$64,%rdx
	jb	L$ess_than_64_8xvl
	vpxor	0(%rsi),%ymm8,%ymm8
	vpxor	32(%rsi),%ymm0,%ymm0
	vmovdqu	%ymm8,0(%rdi,%rsi,1)
	vmovdqu	%ymm0,32(%rdi,%rsi,1)
	je	L$done8xvl
	vmovdqa	%ymm5,%ymm8
	vmovdqa	%ymm13,%ymm0
	leaq	64(%rsi),%rsi

	cmpq	$128,%rdx
	jb	L$ess_than_64_8xvl
	vpxor	0(%rsi),%ymm5,%ymm5
	vpxor	32(%rsi),%ymm13,%ymm13
	vmovdqu	%ymm5,0(%rdi,%rsi,1)
	vmovdqu	%ymm13,32(%rdi,%rsi,1)
	je	L$done8xvl
	vmovdqa	%ymm1,%ymm8
	vmovdqa	%ymm9,%ymm0
	leaq	64(%rsi),%rsi

	cmpq	$192,%rdx
	jb	L$ess_than_64_8xvl
	vpxor	0(%rsi),%ymm1,%ymm1
	vpxor	32(%rsi),%ymm9,%ymm9
	vmovdqu	%ymm1,0(%rdi,%rsi,1)
	vmovdqu	%ymm9,32(%rdi,%rsi,1)
	je	L$done8xvl
	vmovdqa	%ymm2,%ymm8
	vmovdqa	%ymm10,%ymm0
	leaq	64(%rsi),%rsi

	cmpq	$256,%rdx
	jb	L$ess_than_64_8xvl
	vpxor	0(%rsi),%ymm2,%ymm2
	vpxor	32(%rsi),%ymm10,%ymm10
	vmovdqu	%ymm2,0(%rdi,%rsi,1)
	vmovdqu	%ymm10,32(%rdi,%rsi,1)
	je	L$done8xvl
	vmovdqa32	%ymm18,%ymm8
	vmovdqa	%ymm6,%ymm0
	leaq	64(%rsi),%rsi

	cmpq	$320,%rdx
	jb	L$ess_than_64_8xvl
	vpxord	0(%rsi),%ymm18,%ymm18
	vpxor	32(%rsi),%ymm6,%ymm6
	vmovdqu32	%ymm18,0(%rdi,%rsi,1)
	vmovdqu	%ymm6,32(%rdi,%rsi,1)
	je	L$done8xvl
	vmovdqa	%ymm7,%ymm8
	vmovdqa	%ymm15,%ymm0
	leaq	64(%rsi),%rsi

	cmpq	$384,%rdx
	jb	L$ess_than_64_8xvl
	vpxor	0(%rsi),%ymm7,%ymm7
	vpxor	32(%rsi),%ymm15,%ymm15
	vmovdqu	%ymm7,0(%rdi,%rsi,1)
	vmovdqu	%ymm15,32(%rdi,%rsi,1)
	je	L$done8xvl
	vmovdqa	%ymm3,%ymm8
	vmovdqa	%ymm11,%ymm0
	leaq	64(%rsi),%rsi

	cmpq	$448,%rdx
	jb	L$ess_than_64_8xvl
	vpxor	0(%rsi),%ymm3,%ymm3
	vpxor	32(%rsi),%ymm11,%ymm11
	vmovdqu	%ymm3,0(%rdi,%rsi,1)
	vmovdqu	%ymm11,32(%rdi,%rsi,1)
	je	L$done8xvl
	vmovdqa	%ymm4,%ymm8
	vmovdqa	%ymm12,%ymm0
	leaq	64(%rsi),%rsi

L$ess_than_64_8xvl:
	vmovdqa	%ymm8,0(%rsp)
	vmovdqa	%ymm0,32(%rsp)
	leaq	(%rdi,%rsi,1),%rdi
	andq	$63,%rdx

L$oop_tail8xvl:
	movzbl	(%rsi,%r10,1),%eax
	movzbl	(%rsp,%r10,1),%ecx
	leaq	1(%r10),%r10
	xorl	%ecx,%eax
	movb	%al,-1(%rdi,%r10,1)
	decq	%rdx
	jnz	L$oop_tail8xvl

	vpxor	%ymm8,%ymm8,%ymm8
	vmovdqa	%ymm8,0(%rsp)
	vmovdqa	%ymm8,32(%rsp)

L$done8xvl:
	vzeroall
	leaq	(%r9),%rsp

L$8xvl_epilogue:
	.byte	0xf3,0xc3


