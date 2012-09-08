.text


.globl	_RC4

.p2align	4
_RC4:	orq	%rsi,%rsi
	jne	L$entry
	.byte	0xf3,0xc3
L$entry:
	pushq	%rbx
	pushq	%r12
	pushq	%r13
L$prologue:

	addq	$8,%rdi
	movl	-8(%rdi),%r8d
	movl	-4(%rdi),%r12d
	cmpl	$-1,256(%rdi)
	je	L$RC4_CHAR
	incb	%r8b
	movl	(%rdi,%r8,4),%r9d
	testq	$-8,%rsi
	jz	L$loop1
	jmp	L$loop8
.p2align	4
L$loop8:
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
	jnz	L$loop8
	cmpq	$0,%rsi
	jne	L$loop1
	jmp	L$exit

.p2align	4
L$loop1:
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
	jnz	L$loop1
	jmp	L$exit

.p2align	4
L$RC4_CHAR:
	addb	$1,%r8b
	movzbl	(%rdi,%r8,1),%r9d
	testq	$-8,%rsi
	jz	L$cloop1
	cmpl	$0,260(%rdi)
	jnz	L$cloop1
	jmp	L$cloop8
.p2align	4
L$cloop8:
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
	jne	L$cmov0

	movq	%r9,%r11
L$cmov0:
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
	jne	L$cmov1

	movq	%r11,%r9
L$cmov1:
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
	jne	L$cmov2

	movq	%r9,%r11
L$cmov2:
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
	jne	L$cmov3

	movq	%r11,%r9
L$cmov3:
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
	jne	L$cmov4

	movq	%r9,%r11
L$cmov4:
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
	jne	L$cmov5

	movq	%r11,%r9
L$cmov5:
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
	jne	L$cmov6

	movq	%r9,%r11
L$cmov6:
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
	jne	L$cmov7

	movq	%r11,%r9
L$cmov7:
	addb	%r11b,%r13b
	xorb	(%rdi,%r13,1),%bl
	rorl	$8,%ebx
	leaq	-8(%rsi),%rsi
	movl	%eax,(%rcx)
	leaq	8(%rdx),%rdx
	movl	%ebx,4(%rcx)
	leaq	8(%rcx),%rcx

	testq	$-8,%rsi
	jnz	L$cloop8
	cmpq	$0,%rsi
	jne	L$cloop1
	jmp	L$exit
.p2align	4
L$cloop1:
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
	jnz	L$cloop1
	jmp	L$exit

.p2align	4
L$exit:
	subb	$1,%r8b
	movl	%r8d,-8(%rdi)
	movl	%r12d,-4(%rdi)

	movq	(%rsp),%r13
	movq	8(%rsp),%r12
	movq	16(%rsp),%rbx
	addq	$24,%rsp
L$epilogue:
	.byte	0xf3,0xc3


.globl	_RC4_set_key

.p2align	4
_RC4_set_key:
	leaq	8(%rdi),%rdi
	leaq	(%rdx,%rsi,1),%rdx
	negq	%rsi
	movq	%rsi,%rcx
	xorl	%eax,%eax
	xorq	%r9,%r9
	xorq	%r10,%r10
	xorq	%r11,%r11

	movl	_OPENSSL_ia32cap_P(%rip),%r8d
	btl	$20,%r8d
	jnc	L$w1stloop
	btl	$30,%r8d
	setc	%r9b
	movl	%r9d,260(%rdi)
	jmp	L$c1stloop

.p2align	4
L$w1stloop:
	movl	%eax,(%rdi,%rax,4)
	addb	$1,%al
	jnc	L$w1stloop

	xorq	%r9,%r9
	xorq	%r8,%r8
.p2align	4
L$w2ndloop:
	movl	(%rdi,%r9,4),%r10d
	addb	(%rdx,%rsi,1),%r8b
	addb	%r10b,%r8b
	addq	$1,%rsi
	movl	(%rdi,%r8,4),%r11d
	cmovzq	%rcx,%rsi
	movl	%r10d,(%rdi,%r8,4)
	movl	%r11d,(%rdi,%r9,4)
	addb	$1,%r9b
	jnc	L$w2ndloop
	jmp	L$exit_key

.p2align	4
L$c1stloop:
	movb	%al,(%rdi,%rax,1)
	addb	$1,%al
	jnc	L$c1stloop

	xorq	%r9,%r9
	xorq	%r8,%r8
.p2align	4
L$c2ndloop:
	movb	(%rdi,%r9,1),%r10b
	addb	(%rdx,%rsi,1),%r8b
	addb	%r10b,%r8b
	addq	$1,%rsi
	movb	(%rdi,%r8,1),%r11b
	jnz	L$cnowrap
	movq	%rcx,%rsi
L$cnowrap:
	movb	%r10b,(%rdi,%r8,1)
	movb	%r11b,(%rdi,%r9,1)
	addb	$1,%r9b
	jnc	L$c2ndloop
	movl	$-1,256(%rdi)

.p2align	4
L$exit_key:
	xorl	%eax,%eax
	movl	%eax,-8(%rdi)
	movl	%eax,-4(%rdi)
	.byte	0xf3,0xc3


.globl	_RC4_options

.p2align	4
_RC4_options:
	leaq	L$opts(%rip),%rax
	movl	_OPENSSL_ia32cap_P(%rip),%edx
	btl	$20,%edx
	jnc	L$done
	addq	$12,%rax
	btl	$30,%edx
	jnc	L$done
	addq	$13,%rax
L$done:
	.byte	0xf3,0xc3
.p2align	6
L$opts:
.byte	114,99,52,40,56,120,44,105,110,116,41,0
.byte	114,99,52,40,56,120,44,99,104,97,114,41,0
.byte	114,99,52,40,49,120,44,99,104,97,114,41,0
.byte	82,67,52,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	6

