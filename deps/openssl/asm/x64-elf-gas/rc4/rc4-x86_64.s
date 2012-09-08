.text


.globl	RC4
.type	RC4,@function
.align	16
RC4:	orq	%rsi,%rsi
	jne	.Lentry
	.byte	0xf3,0xc3
.Lentry:
	pushq	%rbx
	pushq	%r12
	pushq	%r13
.Lprologue:

	addq	$8,%rdi
	movl	-8(%rdi),%r8d
	movl	-4(%rdi),%r12d
	cmpl	$-1,256(%rdi)
	je	.LRC4_CHAR
	incb	%r8b
	movl	(%rdi,%r8,4),%r9d
	testq	$-8,%rsi
	jz	.Lloop1
	jmp	.Lloop8
.align	16
.Lloop8:
	addb	%r9b,%r12b
	movq	%r8,%r10
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r10b
	movl	(%rdi,%r10,4),%r11d
	cmpq	%r10,%r12
	movl	%r9d,(%rdi,%r12,4)
	cmoveq	%r9,%r11
	movl	%r13d,(%rdi,%r8,4)
	addb	%r9b,%r13b
	movb	(%rdi,%r13,4),%al
	addb	%r11b,%r12b
	movq	%r10,%r8
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r8b
	movl	(%rdi,%r8,4),%r9d
	cmpq	%r8,%r12
	movl	%r11d,(%rdi,%r12,4)
	cmoveq	%r11,%r9
	movl	%r13d,(%rdi,%r10,4)
	addb	%r11b,%r13b
	movb	(%rdi,%r13,4),%al
	addb	%r9b,%r12b
	movq	%r8,%r10
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r10b
	movl	(%rdi,%r10,4),%r11d
	cmpq	%r10,%r12
	movl	%r9d,(%rdi,%r12,4)
	cmoveq	%r9,%r11
	movl	%r13d,(%rdi,%r8,4)
	addb	%r9b,%r13b
	movb	(%rdi,%r13,4),%al
	addb	%r11b,%r12b
	movq	%r10,%r8
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r8b
	movl	(%rdi,%r8,4),%r9d
	cmpq	%r8,%r12
	movl	%r11d,(%rdi,%r12,4)
	cmoveq	%r11,%r9
	movl	%r13d,(%rdi,%r10,4)
	addb	%r11b,%r13b
	movb	(%rdi,%r13,4),%al
	addb	%r9b,%r12b
	movq	%r8,%r10
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r10b
	movl	(%rdi,%r10,4),%r11d
	cmpq	%r10,%r12
	movl	%r9d,(%rdi,%r12,4)
	cmoveq	%r9,%r11
	movl	%r13d,(%rdi,%r8,4)
	addb	%r9b,%r13b
	movb	(%rdi,%r13,4),%al
	addb	%r11b,%r12b
	movq	%r10,%r8
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r8b
	movl	(%rdi,%r8,4),%r9d
	cmpq	%r8,%r12
	movl	%r11d,(%rdi,%r12,4)
	cmoveq	%r11,%r9
	movl	%r13d,(%rdi,%r10,4)
	addb	%r11b,%r13b
	movb	(%rdi,%r13,4),%al
	addb	%r9b,%r12b
	movq	%r8,%r10
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r10b
	movl	(%rdi,%r10,4),%r11d
	cmpq	%r10,%r12
	movl	%r9d,(%rdi,%r12,4)
	cmoveq	%r9,%r11
	movl	%r13d,(%rdi,%r8,4)
	addb	%r9b,%r13b
	movb	(%rdi,%r13,4),%al
	addb	%r11b,%r12b
	movq	%r10,%r8
	movl	(%rdi,%r12,4),%r13d
	rorq	$8,%rax
	incb	%r8b
	movl	(%rdi,%r8,4),%r9d
	cmpq	%r8,%r12
	movl	%r11d,(%rdi,%r12,4)
	cmoveq	%r11,%r9
	movl	%r13d,(%rdi,%r10,4)
	addb	%r11b,%r13b
	movb	(%rdi,%r13,4),%al
	rorq	$8,%rax
	subq	$8,%rsi

	xorq	(%rdx),%rax
	addq	$8,%rdx
	movq	%rax,(%rcx)
	addq	$8,%rcx

	testq	$-8,%rsi
	jnz	.Lloop8
	cmpq	$0,%rsi
	jne	.Lloop1
	jmp	.Lexit

.align	16
.Lloop1:
	addb	%r9b,%r12b
	movl	(%rdi,%r12,4),%r13d
	movl	%r9d,(%rdi,%r12,4)
	movl	%r13d,(%rdi,%r8,4)
	addb	%r13b,%r9b
	incb	%r8b
	movl	(%rdi,%r9,4),%r13d
	movl	(%rdi,%r8,4),%r9d
	xorb	(%rdx),%r13b
	incq	%rdx
	movb	%r13b,(%rcx)
	incq	%rcx
	decq	%rsi
	jnz	.Lloop1
	jmp	.Lexit

.align	16
.LRC4_CHAR:
	addb	$1,%r8b
	movzbl	(%rdi,%r8,1),%r9d
	testq	$-8,%rsi
	jz	.Lcloop1
	cmpl	$0,260(%rdi)
	jnz	.Lcloop1
	jmp	.Lcloop8
.align	16
.Lcloop8:
	movl	(%rdx),%eax
	movl	4(%rdx),%ebx
	addb	%r9b,%r12b
	leaq	1(%r8),%r10
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r10b,%r10d
	movzbl	(%rdi,%r10,1),%r11d
	movb	%r9b,(%rdi,%r12,1)
	cmpq	%r10,%r12
	movb	%r13b,(%rdi,%r8,1)
	jne	.Lcmov0

	movq	%r9,%r11
.Lcmov0:
	addb	%r9b,%r13b
	xorb	(%rdi,%r13,1),%al
	rorl	$8,%eax
	addb	%r11b,%r12b
	leaq	1(%r10),%r8
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r8b,%r8d
	movzbl	(%rdi,%r8,1),%r9d
	movb	%r11b,(%rdi,%r12,1)
	cmpq	%r8,%r12
	movb	%r13b,(%rdi,%r10,1)
	jne	.Lcmov1

	movq	%r11,%r9
.Lcmov1:
	addb	%r11b,%r13b
	xorb	(%rdi,%r13,1),%al
	rorl	$8,%eax
	addb	%r9b,%r12b
	leaq	1(%r8),%r10
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r10b,%r10d
	movzbl	(%rdi,%r10,1),%r11d
	movb	%r9b,(%rdi,%r12,1)
	cmpq	%r10,%r12
	movb	%r13b,(%rdi,%r8,1)
	jne	.Lcmov2

	movq	%r9,%r11
.Lcmov2:
	addb	%r9b,%r13b
	xorb	(%rdi,%r13,1),%al
	rorl	$8,%eax
	addb	%r11b,%r12b
	leaq	1(%r10),%r8
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r8b,%r8d
	movzbl	(%rdi,%r8,1),%r9d
	movb	%r11b,(%rdi,%r12,1)
	cmpq	%r8,%r12
	movb	%r13b,(%rdi,%r10,1)
	jne	.Lcmov3

	movq	%r11,%r9
.Lcmov3:
	addb	%r11b,%r13b
	xorb	(%rdi,%r13,1),%al
	rorl	$8,%eax
	addb	%r9b,%r12b
	leaq	1(%r8),%r10
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r10b,%r10d
	movzbl	(%rdi,%r10,1),%r11d
	movb	%r9b,(%rdi,%r12,1)
	cmpq	%r10,%r12
	movb	%r13b,(%rdi,%r8,1)
	jne	.Lcmov4

	movq	%r9,%r11
.Lcmov4:
	addb	%r9b,%r13b
	xorb	(%rdi,%r13,1),%bl
	rorl	$8,%ebx
	addb	%r11b,%r12b
	leaq	1(%r10),%r8
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r8b,%r8d
	movzbl	(%rdi,%r8,1),%r9d
	movb	%r11b,(%rdi,%r12,1)
	cmpq	%r8,%r12
	movb	%r13b,(%rdi,%r10,1)
	jne	.Lcmov5

	movq	%r11,%r9
.Lcmov5:
	addb	%r11b,%r13b
	xorb	(%rdi,%r13,1),%bl
	rorl	$8,%ebx
	addb	%r9b,%r12b
	leaq	1(%r8),%r10
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r10b,%r10d
	movzbl	(%rdi,%r10,1),%r11d
	movb	%r9b,(%rdi,%r12,1)
	cmpq	%r10,%r12
	movb	%r13b,(%rdi,%r8,1)
	jne	.Lcmov6

	movq	%r9,%r11
.Lcmov6:
	addb	%r9b,%r13b
	xorb	(%rdi,%r13,1),%bl
	rorl	$8,%ebx
	addb	%r11b,%r12b
	leaq	1(%r10),%r8
	movzbl	(%rdi,%r12,1),%r13d
	movzbl	%r8b,%r8d
	movzbl	(%rdi,%r8,1),%r9d
	movb	%r11b,(%rdi,%r12,1)
	cmpq	%r8,%r12
	movb	%r13b,(%rdi,%r10,1)
	jne	.Lcmov7

	movq	%r11,%r9
.Lcmov7:
	addb	%r11b,%r13b
	xorb	(%rdi,%r13,1),%bl
	rorl	$8,%ebx
	leaq	-8(%rsi),%rsi
	movl	%eax,(%rcx)
	leaq	8(%rdx),%rdx
	movl	%ebx,4(%rcx)
	leaq	8(%rcx),%rcx

	testq	$-8,%rsi
	jnz	.Lcloop8
	cmpq	$0,%rsi
	jne	.Lcloop1
	jmp	.Lexit
.align	16
.Lcloop1:
	addb	%r9b,%r12b
	movzbl	(%rdi,%r12,1),%r13d
	movb	%r9b,(%rdi,%r12,1)
	movb	%r13b,(%rdi,%r8,1)
	addb	%r9b,%r13b
	addb	$1,%r8b
	movzbl	%r13b,%r13d
	movzbl	%r8b,%r8d
	movzbl	(%rdi,%r13,1),%r13d
	movzbl	(%rdi,%r8,1),%r9d
	xorb	(%rdx),%r13b
	leaq	1(%rdx),%rdx
	movb	%r13b,(%rcx)
	leaq	1(%rcx),%rcx
	subq	$1,%rsi
	jnz	.Lcloop1
	jmp	.Lexit

.align	16
.Lexit:
	subb	$1,%r8b
	movl	%r8d,-8(%rdi)
	movl	%r12d,-4(%rdi)

	movq	(%rsp),%r13
	movq	8(%rsp),%r12
	movq	16(%rsp),%rbx
	addq	$24,%rsp
.Lepilogue:
	.byte	0xf3,0xc3
.size	RC4,.-RC4

.globl	RC4_set_key
.type	RC4_set_key,@function
.align	16
RC4_set_key:
	leaq	8(%rdi),%rdi
	leaq	(%rdx,%rsi,1),%rdx
	negq	%rsi
	movq	%rsi,%rcx
	xorl	%eax,%eax
	xorq	%r9,%r9
	xorq	%r10,%r10
	xorq	%r11,%r11

	movl	OPENSSL_ia32cap_P(%rip),%r8d
	btl	$20,%r8d
	jnc	.Lw1stloop
	btl	$30,%r8d
	setc	%r9b
	movl	%r9d,260(%rdi)
	jmp	.Lc1stloop

.align	16
.Lw1stloop:
	movl	%eax,(%rdi,%rax,4)
	addb	$1,%al
	jnc	.Lw1stloop

	xorq	%r9,%r9
	xorq	%r8,%r8
.align	16
.Lw2ndloop:
	movl	(%rdi,%r9,4),%r10d
	addb	(%rdx,%rsi,1),%r8b
	addb	%r10b,%r8b
	addq	$1,%rsi
	movl	(%rdi,%r8,4),%r11d
	cmovzq	%rcx,%rsi
	movl	%r10d,(%rdi,%r8,4)
	movl	%r11d,(%rdi,%r9,4)
	addb	$1,%r9b
	jnc	.Lw2ndloop
	jmp	.Lexit_key

.align	16
.Lc1stloop:
	movb	%al,(%rdi,%rax,1)
	addb	$1,%al
	jnc	.Lc1stloop

	xorq	%r9,%r9
	xorq	%r8,%r8
.align	16
.Lc2ndloop:
	movb	(%rdi,%r9,1),%r10b
	addb	(%rdx,%rsi,1),%r8b
	addb	%r10b,%r8b
	addq	$1,%rsi
	movb	(%rdi,%r8,1),%r11b
	jnz	.Lcnowrap
	movq	%rcx,%rsi
.Lcnowrap:
	movb	%r10b,(%rdi,%r8,1)
	movb	%r11b,(%rdi,%r9,1)
	addb	$1,%r9b
	jnc	.Lc2ndloop
	movl	$-1,256(%rdi)

.align	16
.Lexit_key:
	xorl	%eax,%eax
	movl	%eax,-8(%rdi)
	movl	%eax,-4(%rdi)
	.byte	0xf3,0xc3
.size	RC4_set_key,.-RC4_set_key

.globl	RC4_options
.type	RC4_options,@function
.align	16
RC4_options:
	leaq	.Lopts(%rip),%rax
	movl	OPENSSL_ia32cap_P(%rip),%edx
	btl	$20,%edx
	jnc	.Ldone
	addq	$12,%rax
	btl	$30,%edx
	jnc	.Ldone
	addq	$13,%rax
.Ldone:
	.byte	0xf3,0xc3
.align	64
.Lopts:
.byte	114,99,52,40,56,120,44,105,110,116,41,0
.byte	114,99,52,40,56,120,44,99,104,97,114,41,0
.byte	114,99,52,40,49,120,44,99,104,97,114,41,0
.byte	82,67,52,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.align	64
.size	RC4_options,.-RC4_options
