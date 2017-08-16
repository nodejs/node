.text



.globl	_aesni_multi_cbc_encrypt

.p2align	5
_aesni_multi_cbc_encrypt:
	movq	%rsp,%rax
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15






	subq	$48,%rsp
	andq	$-64,%rsp
	movq	%rax,16(%rsp)

L$enc4x_body:
	movdqu	(%rsi),%xmm12
	leaq	120(%rsi),%rsi
	leaq	80(%rdi),%rdi

L$enc4x_loop_grande:
	movl	%edx,24(%rsp)
	xorl	%edx,%edx
	movl	-64(%rdi),%ecx
	movq	-80(%rdi),%r8
	cmpl	%edx,%ecx
	movq	-72(%rdi),%r12
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	-56(%rdi),%xmm2
	movl	%ecx,32(%rsp)
	cmovleq	%rsp,%r8
	movl	-24(%rdi),%ecx
	movq	-40(%rdi),%r9
	cmpl	%edx,%ecx
	movq	-32(%rdi),%r13
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	-16(%rdi),%xmm3
	movl	%ecx,36(%rsp)
	cmovleq	%rsp,%r9
	movl	16(%rdi),%ecx
	movq	0(%rdi),%r10
	cmpl	%edx,%ecx
	movq	8(%rdi),%r14
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	24(%rdi),%xmm4
	movl	%ecx,40(%rsp)
	cmovleq	%rsp,%r10
	movl	56(%rdi),%ecx
	movq	40(%rdi),%r11
	cmpl	%edx,%ecx
	movq	48(%rdi),%r15
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	64(%rdi),%xmm5
	movl	%ecx,44(%rsp)
	cmovleq	%rsp,%r11
	testl	%edx,%edx
	jz	L$enc4x_done

	movups	16-120(%rsi),%xmm1
	pxor	%xmm12,%xmm2
	movups	32-120(%rsi),%xmm0
	pxor	%xmm12,%xmm3
	movl	240-120(%rsi),%eax
	pxor	%xmm12,%xmm4
	movdqu	(%r8),%xmm6
	pxor	%xmm12,%xmm5
	movdqu	(%r9),%xmm7
	pxor	%xmm6,%xmm2
	movdqu	(%r10),%xmm8
	pxor	%xmm7,%xmm3
	movdqu	(%r11),%xmm9
	pxor	%xmm8,%xmm4
	pxor	%xmm9,%xmm5
	movdqa	32(%rsp),%xmm10
	xorq	%rbx,%rbx
	jmp	L$oop_enc4x

.p2align	5
L$oop_enc4x:
	addq	$16,%rbx
	leaq	16(%rsp),%rbp
	movl	$1,%ecx
	subq	%rbx,%rbp

.byte	102,15,56,220,209
	prefetcht0	31(%r8,%rbx,1)
	prefetcht0	31(%r9,%rbx,1)
.byte	102,15,56,220,217
	prefetcht0	31(%r10,%rbx,1)
	prefetcht0	31(%r10,%rbx,1)
.byte	102,15,56,220,225
.byte	102,15,56,220,233
	movups	48-120(%rsi),%xmm1
	cmpl	32(%rsp),%ecx
.byte	102,15,56,220,208
.byte	102,15,56,220,216
.byte	102,15,56,220,224
	cmovgeq	%rbp,%r8
	cmovgq	%rbp,%r12
.byte	102,15,56,220,232
	movups	-56(%rsi),%xmm0
	cmpl	36(%rsp),%ecx
.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
	cmovgeq	%rbp,%r9
	cmovgq	%rbp,%r13
.byte	102,15,56,220,233
	movups	-40(%rsi),%xmm1
	cmpl	40(%rsp),%ecx
.byte	102,15,56,220,208
.byte	102,15,56,220,216
.byte	102,15,56,220,224
	cmovgeq	%rbp,%r10
	cmovgq	%rbp,%r14
.byte	102,15,56,220,232
	movups	-24(%rsi),%xmm0
	cmpl	44(%rsp),%ecx
.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
	cmovgeq	%rbp,%r11
	cmovgq	%rbp,%r15
.byte	102,15,56,220,233
	movups	-8(%rsi),%xmm1
	movdqa	%xmm10,%xmm11
.byte	102,15,56,220,208
	prefetcht0	15(%r12,%rbx,1)
	prefetcht0	15(%r13,%rbx,1)
.byte	102,15,56,220,216
	prefetcht0	15(%r14,%rbx,1)
	prefetcht0	15(%r15,%rbx,1)
.byte	102,15,56,220,224
.byte	102,15,56,220,232
	movups	128-120(%rsi),%xmm0
	pxor	%xmm12,%xmm12

.byte	102,15,56,220,209
	pcmpgtd	%xmm12,%xmm11
	movdqu	-120(%rsi),%xmm12
.byte	102,15,56,220,217
	paddd	%xmm11,%xmm10
	movdqa	%xmm10,32(%rsp)
.byte	102,15,56,220,225
.byte	102,15,56,220,233
	movups	144-120(%rsi),%xmm1

	cmpl	$11,%eax

.byte	102,15,56,220,208
.byte	102,15,56,220,216
.byte	102,15,56,220,224
.byte	102,15,56,220,232
	movups	160-120(%rsi),%xmm0

	jb	L$enc4x_tail

.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
.byte	102,15,56,220,233
	movups	176-120(%rsi),%xmm1

.byte	102,15,56,220,208
.byte	102,15,56,220,216
.byte	102,15,56,220,224
.byte	102,15,56,220,232
	movups	192-120(%rsi),%xmm0

	je	L$enc4x_tail

.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
.byte	102,15,56,220,233
	movups	208-120(%rsi),%xmm1

.byte	102,15,56,220,208
.byte	102,15,56,220,216
.byte	102,15,56,220,224
.byte	102,15,56,220,232
	movups	224-120(%rsi),%xmm0
	jmp	L$enc4x_tail

.p2align	5
L$enc4x_tail:
.byte	102,15,56,220,209
.byte	102,15,56,220,217
.byte	102,15,56,220,225
.byte	102,15,56,220,233
	movdqu	(%r8,%rbx,1),%xmm6
	movdqu	16-120(%rsi),%xmm1

.byte	102,15,56,221,208
	movdqu	(%r9,%rbx,1),%xmm7
	pxor	%xmm12,%xmm6
.byte	102,15,56,221,216
	movdqu	(%r10,%rbx,1),%xmm8
	pxor	%xmm12,%xmm7
.byte	102,15,56,221,224
	movdqu	(%r11,%rbx,1),%xmm9
	pxor	%xmm12,%xmm8
.byte	102,15,56,221,232
	movdqu	32-120(%rsi),%xmm0
	pxor	%xmm12,%xmm9

	movups	%xmm2,-16(%r12,%rbx,1)
	pxor	%xmm6,%xmm2
	movups	%xmm3,-16(%r13,%rbx,1)
	pxor	%xmm7,%xmm3
	movups	%xmm4,-16(%r14,%rbx,1)
	pxor	%xmm8,%xmm4
	movups	%xmm5,-16(%r15,%rbx,1)
	pxor	%xmm9,%xmm5

	decl	%edx
	jnz	L$oop_enc4x

	movq	16(%rsp),%rax
	movl	24(%rsp),%edx










	leaq	160(%rdi),%rdi
	decl	%edx
	jnz	L$enc4x_loop_grande

L$enc4x_done:
	movq	-48(%rax),%r15
	movq	-40(%rax),%r14
	movq	-32(%rax),%r13
	movq	-24(%rax),%r12
	movq	-16(%rax),%rbp
	movq	-8(%rax),%rbx
	leaq	(%rax),%rsp
L$enc4x_epilogue:
	.byte	0xf3,0xc3


.globl	_aesni_multi_cbc_decrypt

.p2align	5
_aesni_multi_cbc_decrypt:
	movq	%rsp,%rax
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15






	subq	$48,%rsp
	andq	$-64,%rsp
	movq	%rax,16(%rsp)

L$dec4x_body:
	movdqu	(%rsi),%xmm12
	leaq	120(%rsi),%rsi
	leaq	80(%rdi),%rdi

L$dec4x_loop_grande:
	movl	%edx,24(%rsp)
	xorl	%edx,%edx
	movl	-64(%rdi),%ecx
	movq	-80(%rdi),%r8
	cmpl	%edx,%ecx
	movq	-72(%rdi),%r12
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	-56(%rdi),%xmm6
	movl	%ecx,32(%rsp)
	cmovleq	%rsp,%r8
	movl	-24(%rdi),%ecx
	movq	-40(%rdi),%r9
	cmpl	%edx,%ecx
	movq	-32(%rdi),%r13
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	-16(%rdi),%xmm7
	movl	%ecx,36(%rsp)
	cmovleq	%rsp,%r9
	movl	16(%rdi),%ecx
	movq	0(%rdi),%r10
	cmpl	%edx,%ecx
	movq	8(%rdi),%r14
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	24(%rdi),%xmm8
	movl	%ecx,40(%rsp)
	cmovleq	%rsp,%r10
	movl	56(%rdi),%ecx
	movq	40(%rdi),%r11
	cmpl	%edx,%ecx
	movq	48(%rdi),%r15
	cmovgl	%ecx,%edx
	testl	%ecx,%ecx
	movdqu	64(%rdi),%xmm9
	movl	%ecx,44(%rsp)
	cmovleq	%rsp,%r11
	testl	%edx,%edx
	jz	L$dec4x_done

	movups	16-120(%rsi),%xmm1
	movups	32-120(%rsi),%xmm0
	movl	240-120(%rsi),%eax
	movdqu	(%r8),%xmm2
	movdqu	(%r9),%xmm3
	pxor	%xmm12,%xmm2
	movdqu	(%r10),%xmm4
	pxor	%xmm12,%xmm3
	movdqu	(%r11),%xmm5
	pxor	%xmm12,%xmm4
	pxor	%xmm12,%xmm5
	movdqa	32(%rsp),%xmm10
	xorq	%rbx,%rbx
	jmp	L$oop_dec4x

.p2align	5
L$oop_dec4x:
	addq	$16,%rbx
	leaq	16(%rsp),%rbp
	movl	$1,%ecx
	subq	%rbx,%rbp

.byte	102,15,56,222,209
	prefetcht0	31(%r8,%rbx,1)
	prefetcht0	31(%r9,%rbx,1)
.byte	102,15,56,222,217
	prefetcht0	31(%r10,%rbx,1)
	prefetcht0	31(%r11,%rbx,1)
.byte	102,15,56,222,225
.byte	102,15,56,222,233
	movups	48-120(%rsi),%xmm1
	cmpl	32(%rsp),%ecx
.byte	102,15,56,222,208
.byte	102,15,56,222,216
.byte	102,15,56,222,224
	cmovgeq	%rbp,%r8
	cmovgq	%rbp,%r12
.byte	102,15,56,222,232
	movups	-56(%rsi),%xmm0
	cmpl	36(%rsp),%ecx
.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
	cmovgeq	%rbp,%r9
	cmovgq	%rbp,%r13
.byte	102,15,56,222,233
	movups	-40(%rsi),%xmm1
	cmpl	40(%rsp),%ecx
.byte	102,15,56,222,208
.byte	102,15,56,222,216
.byte	102,15,56,222,224
	cmovgeq	%rbp,%r10
	cmovgq	%rbp,%r14
.byte	102,15,56,222,232
	movups	-24(%rsi),%xmm0
	cmpl	44(%rsp),%ecx
.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
	cmovgeq	%rbp,%r11
	cmovgq	%rbp,%r15
.byte	102,15,56,222,233
	movups	-8(%rsi),%xmm1
	movdqa	%xmm10,%xmm11
.byte	102,15,56,222,208
	prefetcht0	15(%r12,%rbx,1)
	prefetcht0	15(%r13,%rbx,1)
.byte	102,15,56,222,216
	prefetcht0	15(%r14,%rbx,1)
	prefetcht0	15(%r15,%rbx,1)
.byte	102,15,56,222,224
.byte	102,15,56,222,232
	movups	128-120(%rsi),%xmm0
	pxor	%xmm12,%xmm12

.byte	102,15,56,222,209
	pcmpgtd	%xmm12,%xmm11
	movdqu	-120(%rsi),%xmm12
.byte	102,15,56,222,217
	paddd	%xmm11,%xmm10
	movdqa	%xmm10,32(%rsp)
.byte	102,15,56,222,225
.byte	102,15,56,222,233
	movups	144-120(%rsi),%xmm1

	cmpl	$11,%eax

.byte	102,15,56,222,208
.byte	102,15,56,222,216
.byte	102,15,56,222,224
.byte	102,15,56,222,232
	movups	160-120(%rsi),%xmm0

	jb	L$dec4x_tail

.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
.byte	102,15,56,222,233
	movups	176-120(%rsi),%xmm1

.byte	102,15,56,222,208
.byte	102,15,56,222,216
.byte	102,15,56,222,224
.byte	102,15,56,222,232
	movups	192-120(%rsi),%xmm0

	je	L$dec4x_tail

.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
.byte	102,15,56,222,233
	movups	208-120(%rsi),%xmm1

.byte	102,15,56,222,208
.byte	102,15,56,222,216
.byte	102,15,56,222,224
.byte	102,15,56,222,232
	movups	224-120(%rsi),%xmm0
	jmp	L$dec4x_tail

.p2align	5
L$dec4x_tail:
.byte	102,15,56,222,209
.byte	102,15,56,222,217
.byte	102,15,56,222,225
	pxor	%xmm0,%xmm6
	pxor	%xmm0,%xmm7
.byte	102,15,56,222,233
	movdqu	16-120(%rsi),%xmm1
	pxor	%xmm0,%xmm8
	pxor	%xmm0,%xmm9
	movdqu	32-120(%rsi),%xmm0

.byte	102,15,56,223,214
.byte	102,15,56,223,223
	movdqu	-16(%r8,%rbx,1),%xmm6
	movdqu	-16(%r9,%rbx,1),%xmm7
.byte	102,65,15,56,223,224
.byte	102,65,15,56,223,233
	movdqu	-16(%r10,%rbx,1),%xmm8
	movdqu	-16(%r11,%rbx,1),%xmm9

	movups	%xmm2,-16(%r12,%rbx,1)
	movdqu	(%r8,%rbx,1),%xmm2
	movups	%xmm3,-16(%r13,%rbx,1)
	movdqu	(%r9,%rbx,1),%xmm3
	pxor	%xmm12,%xmm2
	movups	%xmm4,-16(%r14,%rbx,1)
	movdqu	(%r10,%rbx,1),%xmm4
	pxor	%xmm12,%xmm3
	movups	%xmm5,-16(%r15,%rbx,1)
	movdqu	(%r11,%rbx,1),%xmm5
	pxor	%xmm12,%xmm4
	pxor	%xmm12,%xmm5

	decl	%edx
	jnz	L$oop_dec4x

	movq	16(%rsp),%rax
	movl	24(%rsp),%edx

	leaq	160(%rdi),%rdi
	decl	%edx
	jnz	L$dec4x_loop_grande

L$dec4x_done:
	movq	-48(%rax),%r15
	movq	-40(%rax),%r14
	movq	-32(%rax),%r13
	movq	-24(%rax),%r12
	movq	-16(%rax),%rbp
	movq	-8(%rax),%rbx
	leaq	(%rax),%rsp
L$dec4x_epilogue:
	.byte	0xf3,0xc3
