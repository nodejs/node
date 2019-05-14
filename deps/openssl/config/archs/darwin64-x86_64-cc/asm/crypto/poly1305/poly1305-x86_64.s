.text	



.globl	_poly1305_init
.private_extern	_poly1305_init
.globl	_poly1305_blocks
.private_extern	_poly1305_blocks
.globl	_poly1305_emit
.private_extern	_poly1305_emit


.p2align	5
_poly1305_init:
	xorq	%rax,%rax
	movq	%rax,0(%rdi)
	movq	%rax,8(%rdi)
	movq	%rax,16(%rdi)

	cmpq	$0,%rsi
	je	L$no_key

	leaq	_poly1305_blocks(%rip),%r10
	leaq	_poly1305_emit(%rip),%r11
	movq	_OPENSSL_ia32cap_P+4(%rip),%r9
	leaq	poly1305_blocks_avx(%rip),%rax
	leaq	poly1305_emit_avx(%rip),%rcx
	btq	$28,%r9
	cmovcq	%rax,%r10
	cmovcq	%rcx,%r11
	leaq	poly1305_blocks_avx2(%rip),%rax
	btq	$37,%r9
	cmovcq	%rax,%r10
	movq	$2149646336,%rax
	shrq	$32,%r9
	andq	%rax,%r9
	cmpq	%rax,%r9
	je	L$init_base2_44
	movq	$0x0ffffffc0fffffff,%rax
	movq	$0x0ffffffc0ffffffc,%rcx
	andq	0(%rsi),%rax
	andq	8(%rsi),%rcx
	movq	%rax,24(%rdi)
	movq	%rcx,32(%rdi)
	movq	%r10,0(%rdx)
	movq	%r11,8(%rdx)
	movl	$1,%eax
L$no_key:
	.byte	0xf3,0xc3



.p2align	5
_poly1305_blocks:

L$blocks:
	shrq	$4,%rdx
	jz	L$no_data

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$blocks_body:

	movq	%rdx,%r15

	movq	24(%rdi),%r11
	movq	32(%rdi),%r13

	movq	0(%rdi),%r14
	movq	8(%rdi),%rbx
	movq	16(%rdi),%rbp

	movq	%r13,%r12
	shrq	$2,%r13
	movq	%r12,%rax
	addq	%r12,%r13
	jmp	L$oop

.p2align	5
L$oop:
	addq	0(%rsi),%r14
	adcq	8(%rsi),%rbx
	leaq	16(%rsi),%rsi
	adcq	%rcx,%rbp
	mulq	%r14
	movq	%rax,%r9
	movq	%r11,%rax
	movq	%rdx,%r10

	mulq	%r14
	movq	%rax,%r14
	movq	%r11,%rax
	movq	%rdx,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	%r13,%rax
	adcq	%rdx,%r10

	mulq	%rbx
	movq	%rbp,%rbx
	addq	%rax,%r14
	adcq	%rdx,%r8

	imulq	%r13,%rbx
	addq	%rbx,%r9
	movq	%r8,%rbx
	adcq	$0,%r10

	imulq	%r11,%rbp
	addq	%r9,%rbx
	movq	$-4,%rax
	adcq	%rbp,%r10

	andq	%r10,%rax
	movq	%r10,%rbp
	shrq	$2,%r10
	andq	$3,%rbp
	addq	%r10,%rax
	addq	%rax,%r14
	adcq	$0,%rbx
	adcq	$0,%rbp
	movq	%r12,%rax
	decq	%r15
	jnz	L$oop

	movq	%r14,0(%rdi)
	movq	%rbx,8(%rdi)
	movq	%rbp,16(%rdi)

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbp

	movq	40(%rsp),%rbx

	leaq	48(%rsp),%rsp

L$no_data:
L$blocks_epilogue:
	.byte	0xf3,0xc3




.p2align	5
_poly1305_emit:
L$emit:
	movq	0(%rdi),%r8
	movq	8(%rdi),%r9
	movq	16(%rdi),%r10

	movq	%r8,%rax
	addq	$5,%r8
	movq	%r9,%rcx
	adcq	$0,%r9
	adcq	$0,%r10
	shrq	$2,%r10
	cmovnzq	%r8,%rax
	cmovnzq	%r9,%rcx

	addq	0(%rdx),%rax
	adcq	8(%rdx),%rcx
	movq	%rax,0(%rsi)
	movq	%rcx,8(%rsi)

	.byte	0xf3,0xc3


.p2align	5
__poly1305_block:
	mulq	%r14
	movq	%rax,%r9
	movq	%r11,%rax
	movq	%rdx,%r10

	mulq	%r14
	movq	%rax,%r14
	movq	%r11,%rax
	movq	%rdx,%r8

	mulq	%rbx
	addq	%rax,%r9
	movq	%r13,%rax
	adcq	%rdx,%r10

	mulq	%rbx
	movq	%rbp,%rbx
	addq	%rax,%r14
	adcq	%rdx,%r8

	imulq	%r13,%rbx
	addq	%rbx,%r9
	movq	%r8,%rbx
	adcq	$0,%r10

	imulq	%r11,%rbp
	addq	%r9,%rbx
	movq	$-4,%rax
	adcq	%rbp,%r10

	andq	%r10,%rax
	movq	%r10,%rbp
	shrq	$2,%r10
	andq	$3,%rbp
	addq	%r10,%rax
	addq	%rax,%r14
	adcq	$0,%rbx
	adcq	$0,%rbp
	.byte	0xf3,0xc3



.p2align	5
__poly1305_init_avx:
	movq	%r11,%r14
	movq	%r12,%rbx
	xorq	%rbp,%rbp

	leaq	48+64(%rdi),%rdi

	movq	%r12,%rax
	call	__poly1305_block

	movl	$0x3ffffff,%eax
	movl	$0x3ffffff,%edx
	movq	%r14,%r8
	andl	%r14d,%eax
	movq	%r11,%r9
	andl	%r11d,%edx
	movl	%eax,-64(%rdi)
	shrq	$26,%r8
	movl	%edx,-60(%rdi)
	shrq	$26,%r9

	movl	$0x3ffffff,%eax
	movl	$0x3ffffff,%edx
	andl	%r8d,%eax
	andl	%r9d,%edx
	movl	%eax,-48(%rdi)
	leal	(%rax,%rax,4),%eax
	movl	%edx,-44(%rdi)
	leal	(%rdx,%rdx,4),%edx
	movl	%eax,-32(%rdi)
	shrq	$26,%r8
	movl	%edx,-28(%rdi)
	shrq	$26,%r9

	movq	%rbx,%rax
	movq	%r12,%rdx
	shlq	$12,%rax
	shlq	$12,%rdx
	orq	%r8,%rax
	orq	%r9,%rdx
	andl	$0x3ffffff,%eax
	andl	$0x3ffffff,%edx
	movl	%eax,-16(%rdi)
	leal	(%rax,%rax,4),%eax
	movl	%edx,-12(%rdi)
	leal	(%rdx,%rdx,4),%edx
	movl	%eax,0(%rdi)
	movq	%rbx,%r8
	movl	%edx,4(%rdi)
	movq	%r12,%r9

	movl	$0x3ffffff,%eax
	movl	$0x3ffffff,%edx
	shrq	$14,%r8
	shrq	$14,%r9
	andl	%r8d,%eax
	andl	%r9d,%edx
	movl	%eax,16(%rdi)
	leal	(%rax,%rax,4),%eax
	movl	%edx,20(%rdi)
	leal	(%rdx,%rdx,4),%edx
	movl	%eax,32(%rdi)
	shrq	$26,%r8
	movl	%edx,36(%rdi)
	shrq	$26,%r9

	movq	%rbp,%rax
	shlq	$24,%rax
	orq	%rax,%r8
	movl	%r8d,48(%rdi)
	leaq	(%r8,%r8,4),%r8
	movl	%r9d,52(%rdi)
	leaq	(%r9,%r9,4),%r9
	movl	%r8d,64(%rdi)
	movl	%r9d,68(%rdi)

	movq	%r12,%rax
	call	__poly1305_block

	movl	$0x3ffffff,%eax
	movq	%r14,%r8
	andl	%r14d,%eax
	shrq	$26,%r8
	movl	%eax,-52(%rdi)

	movl	$0x3ffffff,%edx
	andl	%r8d,%edx
	movl	%edx,-36(%rdi)
	leal	(%rdx,%rdx,4),%edx
	shrq	$26,%r8
	movl	%edx,-20(%rdi)

	movq	%rbx,%rax
	shlq	$12,%rax
	orq	%r8,%rax
	andl	$0x3ffffff,%eax
	movl	%eax,-4(%rdi)
	leal	(%rax,%rax,4),%eax
	movq	%rbx,%r8
	movl	%eax,12(%rdi)

	movl	$0x3ffffff,%edx
	shrq	$14,%r8
	andl	%r8d,%edx
	movl	%edx,28(%rdi)
	leal	(%rdx,%rdx,4),%edx
	shrq	$26,%r8
	movl	%edx,44(%rdi)

	movq	%rbp,%rax
	shlq	$24,%rax
	orq	%rax,%r8
	movl	%r8d,60(%rdi)
	leaq	(%r8,%r8,4),%r8
	movl	%r8d,76(%rdi)

	movq	%r12,%rax
	call	__poly1305_block

	movl	$0x3ffffff,%eax
	movq	%r14,%r8
	andl	%r14d,%eax
	shrq	$26,%r8
	movl	%eax,-56(%rdi)

	movl	$0x3ffffff,%edx
	andl	%r8d,%edx
	movl	%edx,-40(%rdi)
	leal	(%rdx,%rdx,4),%edx
	shrq	$26,%r8
	movl	%edx,-24(%rdi)

	movq	%rbx,%rax
	shlq	$12,%rax
	orq	%r8,%rax
	andl	$0x3ffffff,%eax
	movl	%eax,-8(%rdi)
	leal	(%rax,%rax,4),%eax
	movq	%rbx,%r8
	movl	%eax,8(%rdi)

	movl	$0x3ffffff,%edx
	shrq	$14,%r8
	andl	%r8d,%edx
	movl	%edx,24(%rdi)
	leal	(%rdx,%rdx,4),%edx
	shrq	$26,%r8
	movl	%edx,40(%rdi)

	movq	%rbp,%rax
	shlq	$24,%rax
	orq	%rax,%r8
	movl	%r8d,56(%rdi)
	leaq	(%r8,%r8,4),%r8
	movl	%r8d,72(%rdi)

	leaq	-48-64(%rdi),%rdi
	.byte	0xf3,0xc3



.p2align	5
poly1305_blocks_avx:

	movl	20(%rdi),%r8d
	cmpq	$128,%rdx
	jae	L$blocks_avx
	testl	%r8d,%r8d
	jz	L$blocks

L$blocks_avx:
	andq	$-16,%rdx
	jz	L$no_data_avx

	vzeroupper

	testl	%r8d,%r8d
	jz	L$base2_64_avx

	testq	$31,%rdx
	jz	L$even_avx

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$blocks_avx_body:

	movq	%rdx,%r15

	movq	0(%rdi),%r8
	movq	8(%rdi),%r9
	movl	16(%rdi),%ebp

	movq	24(%rdi),%r11
	movq	32(%rdi),%r13


	movl	%r8d,%r14d
	andq	$-2147483648,%r8
	movq	%r9,%r12
	movl	%r9d,%ebx
	andq	$-2147483648,%r9

	shrq	$6,%r8
	shlq	$52,%r12
	addq	%r8,%r14
	shrq	$12,%rbx
	shrq	$18,%r9
	addq	%r12,%r14
	adcq	%r9,%rbx

	movq	%rbp,%r8
	shlq	$40,%r8
	shrq	$24,%rbp
	addq	%r8,%rbx
	adcq	$0,%rbp

	movq	$-4,%r9
	movq	%rbp,%r8
	andq	%rbp,%r9
	shrq	$2,%r8
	andq	$3,%rbp
	addq	%r9,%r8
	addq	%r8,%r14
	adcq	$0,%rbx
	adcq	$0,%rbp

	movq	%r13,%r12
	movq	%r13,%rax
	shrq	$2,%r13
	addq	%r12,%r13

	addq	0(%rsi),%r14
	adcq	8(%rsi),%rbx
	leaq	16(%rsi),%rsi
	adcq	%rcx,%rbp

	call	__poly1305_block

	testq	%rcx,%rcx
	jz	L$store_base2_64_avx


	movq	%r14,%rax
	movq	%r14,%rdx
	shrq	$52,%r14
	movq	%rbx,%r11
	movq	%rbx,%r12
	shrq	$26,%rdx
	andq	$0x3ffffff,%rax
	shlq	$12,%r11
	andq	$0x3ffffff,%rdx
	shrq	$14,%rbx
	orq	%r11,%r14
	shlq	$24,%rbp
	andq	$0x3ffffff,%r14
	shrq	$40,%r12
	andq	$0x3ffffff,%rbx
	orq	%r12,%rbp

	subq	$16,%r15
	jz	L$store_base2_26_avx

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%ebp,%xmm4
	jmp	L$proceed_avx

.p2align	5
L$store_base2_64_avx:
	movq	%r14,0(%rdi)
	movq	%rbx,8(%rdi)
	movq	%rbp,16(%rdi)
	jmp	L$done_avx

.p2align	4
L$store_base2_26_avx:
	movl	%eax,0(%rdi)
	movl	%edx,4(%rdi)
	movl	%r14d,8(%rdi)
	movl	%ebx,12(%rdi)
	movl	%ebp,16(%rdi)
.p2align	4
L$done_avx:
	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbp

	movq	40(%rsp),%rbx

	leaq	48(%rsp),%rsp

L$no_data_avx:
L$blocks_avx_epilogue:
	.byte	0xf3,0xc3


.p2align	5
L$base2_64_avx:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$base2_64_avx_body:

	movq	%rdx,%r15

	movq	24(%rdi),%r11
	movq	32(%rdi),%r13

	movq	0(%rdi),%r14
	movq	8(%rdi),%rbx
	movl	16(%rdi),%ebp

	movq	%r13,%r12
	movq	%r13,%rax
	shrq	$2,%r13
	addq	%r12,%r13

	testq	$31,%rdx
	jz	L$init_avx

	addq	0(%rsi),%r14
	adcq	8(%rsi),%rbx
	leaq	16(%rsi),%rsi
	adcq	%rcx,%rbp
	subq	$16,%r15

	call	__poly1305_block

L$init_avx:

	movq	%r14,%rax
	movq	%r14,%rdx
	shrq	$52,%r14
	movq	%rbx,%r8
	movq	%rbx,%r9
	shrq	$26,%rdx
	andq	$0x3ffffff,%rax
	shlq	$12,%r8
	andq	$0x3ffffff,%rdx
	shrq	$14,%rbx
	orq	%r8,%r14
	shlq	$24,%rbp
	andq	$0x3ffffff,%r14
	shrq	$40,%r9
	andq	$0x3ffffff,%rbx
	orq	%r9,%rbp

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%ebp,%xmm4
	movl	$1,20(%rdi)

	call	__poly1305_init_avx

L$proceed_avx:
	movq	%r15,%rdx

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbp

	movq	40(%rsp),%rbx

	leaq	48(%rsp),%rax
	leaq	48(%rsp),%rsp

L$base2_64_avx_epilogue:
	jmp	L$do_avx


.p2align	5
L$even_avx:

	vmovd	0(%rdi),%xmm0
	vmovd	4(%rdi),%xmm1
	vmovd	8(%rdi),%xmm2
	vmovd	12(%rdi),%xmm3
	vmovd	16(%rdi),%xmm4

L$do_avx:
	leaq	-88(%rsp),%r11

	subq	$0x178,%rsp
	subq	$64,%rdx
	leaq	-32(%rsi),%rax
	cmovcq	%rax,%rsi

	vmovdqu	48(%rdi),%xmm14
	leaq	112(%rdi),%rdi
	leaq	L$const(%rip),%rcx



	vmovdqu	32(%rsi),%xmm5
	vmovdqu	48(%rsi),%xmm6
	vmovdqa	64(%rcx),%xmm15

	vpsrldq	$6,%xmm5,%xmm7
	vpsrldq	$6,%xmm6,%xmm8
	vpunpckhqdq	%xmm6,%xmm5,%xmm9
	vpunpcklqdq	%xmm6,%xmm5,%xmm5
	vpunpcklqdq	%xmm8,%xmm7,%xmm8

	vpsrlq	$40,%xmm9,%xmm9
	vpsrlq	$26,%xmm5,%xmm6
	vpand	%xmm15,%xmm5,%xmm5
	vpsrlq	$4,%xmm8,%xmm7
	vpand	%xmm15,%xmm6,%xmm6
	vpsrlq	$30,%xmm8,%xmm8
	vpand	%xmm15,%xmm7,%xmm7
	vpand	%xmm15,%xmm8,%xmm8
	vpor	32(%rcx),%xmm9,%xmm9

	jbe	L$skip_loop_avx


	vmovdqu	-48(%rdi),%xmm11
	vmovdqu	-32(%rdi),%xmm12
	vpshufd	$0xEE,%xmm14,%xmm13
	vpshufd	$0x44,%xmm14,%xmm10
	vmovdqa	%xmm13,-144(%r11)
	vmovdqa	%xmm10,0(%rsp)
	vpshufd	$0xEE,%xmm11,%xmm14
	vmovdqu	-16(%rdi),%xmm10
	vpshufd	$0x44,%xmm11,%xmm11
	vmovdqa	%xmm14,-128(%r11)
	vmovdqa	%xmm11,16(%rsp)
	vpshufd	$0xEE,%xmm12,%xmm13
	vmovdqu	0(%rdi),%xmm11
	vpshufd	$0x44,%xmm12,%xmm12
	vmovdqa	%xmm13,-112(%r11)
	vmovdqa	%xmm12,32(%rsp)
	vpshufd	$0xEE,%xmm10,%xmm14
	vmovdqu	16(%rdi),%xmm12
	vpshufd	$0x44,%xmm10,%xmm10
	vmovdqa	%xmm14,-96(%r11)
	vmovdqa	%xmm10,48(%rsp)
	vpshufd	$0xEE,%xmm11,%xmm13
	vmovdqu	32(%rdi),%xmm10
	vpshufd	$0x44,%xmm11,%xmm11
	vmovdqa	%xmm13,-80(%r11)
	vmovdqa	%xmm11,64(%rsp)
	vpshufd	$0xEE,%xmm12,%xmm14
	vmovdqu	48(%rdi),%xmm11
	vpshufd	$0x44,%xmm12,%xmm12
	vmovdqa	%xmm14,-64(%r11)
	vmovdqa	%xmm12,80(%rsp)
	vpshufd	$0xEE,%xmm10,%xmm13
	vmovdqu	64(%rdi),%xmm12
	vpshufd	$0x44,%xmm10,%xmm10
	vmovdqa	%xmm13,-48(%r11)
	vmovdqa	%xmm10,96(%rsp)
	vpshufd	$0xEE,%xmm11,%xmm14
	vpshufd	$0x44,%xmm11,%xmm11
	vmovdqa	%xmm14,-32(%r11)
	vmovdqa	%xmm11,112(%rsp)
	vpshufd	$0xEE,%xmm12,%xmm13
	vmovdqa	0(%rsp),%xmm14
	vpshufd	$0x44,%xmm12,%xmm12
	vmovdqa	%xmm13,-16(%r11)
	vmovdqa	%xmm12,128(%rsp)

	jmp	L$oop_avx

.p2align	5
L$oop_avx:




















	vpmuludq	%xmm5,%xmm14,%xmm10
	vpmuludq	%xmm6,%xmm14,%xmm11
	vmovdqa	%xmm2,32(%r11)
	vpmuludq	%xmm7,%xmm14,%xmm12
	vmovdqa	16(%rsp),%xmm2
	vpmuludq	%xmm8,%xmm14,%xmm13
	vpmuludq	%xmm9,%xmm14,%xmm14

	vmovdqa	%xmm0,0(%r11)
	vpmuludq	32(%rsp),%xmm9,%xmm0
	vmovdqa	%xmm1,16(%r11)
	vpmuludq	%xmm8,%xmm2,%xmm1
	vpaddq	%xmm0,%xmm10,%xmm10
	vpaddq	%xmm1,%xmm14,%xmm14
	vmovdqa	%xmm3,48(%r11)
	vpmuludq	%xmm7,%xmm2,%xmm0
	vpmuludq	%xmm6,%xmm2,%xmm1
	vpaddq	%xmm0,%xmm13,%xmm13
	vmovdqa	48(%rsp),%xmm3
	vpaddq	%xmm1,%xmm12,%xmm12
	vmovdqa	%xmm4,64(%r11)
	vpmuludq	%xmm5,%xmm2,%xmm2
	vpmuludq	%xmm7,%xmm3,%xmm0
	vpaddq	%xmm2,%xmm11,%xmm11

	vmovdqa	64(%rsp),%xmm4
	vpaddq	%xmm0,%xmm14,%xmm14
	vpmuludq	%xmm6,%xmm3,%xmm1
	vpmuludq	%xmm5,%xmm3,%xmm3
	vpaddq	%xmm1,%xmm13,%xmm13
	vmovdqa	80(%rsp),%xmm2
	vpaddq	%xmm3,%xmm12,%xmm12
	vpmuludq	%xmm9,%xmm4,%xmm0
	vpmuludq	%xmm8,%xmm4,%xmm4
	vpaddq	%xmm0,%xmm11,%xmm11
	vmovdqa	96(%rsp),%xmm3
	vpaddq	%xmm4,%xmm10,%xmm10

	vmovdqa	128(%rsp),%xmm4
	vpmuludq	%xmm6,%xmm2,%xmm1
	vpmuludq	%xmm5,%xmm2,%xmm2
	vpaddq	%xmm1,%xmm14,%xmm14
	vpaddq	%xmm2,%xmm13,%xmm13
	vpmuludq	%xmm9,%xmm3,%xmm0
	vpmuludq	%xmm8,%xmm3,%xmm1
	vpaddq	%xmm0,%xmm12,%xmm12
	vmovdqu	0(%rsi),%xmm0
	vpaddq	%xmm1,%xmm11,%xmm11
	vpmuludq	%xmm7,%xmm3,%xmm3
	vpmuludq	%xmm7,%xmm4,%xmm7
	vpaddq	%xmm3,%xmm10,%xmm10

	vmovdqu	16(%rsi),%xmm1
	vpaddq	%xmm7,%xmm11,%xmm11
	vpmuludq	%xmm8,%xmm4,%xmm8
	vpmuludq	%xmm9,%xmm4,%xmm9
	vpsrldq	$6,%xmm0,%xmm2
	vpaddq	%xmm8,%xmm12,%xmm12
	vpaddq	%xmm9,%xmm13,%xmm13
	vpsrldq	$6,%xmm1,%xmm3
	vpmuludq	112(%rsp),%xmm5,%xmm9
	vpmuludq	%xmm6,%xmm4,%xmm5
	vpunpckhqdq	%xmm1,%xmm0,%xmm4
	vpaddq	%xmm9,%xmm14,%xmm14
	vmovdqa	-144(%r11),%xmm9
	vpaddq	%xmm5,%xmm10,%xmm10

	vpunpcklqdq	%xmm1,%xmm0,%xmm0
	vpunpcklqdq	%xmm3,%xmm2,%xmm3


	vpsrldq	$5,%xmm4,%xmm4
	vpsrlq	$26,%xmm0,%xmm1
	vpand	%xmm15,%xmm0,%xmm0
	vpsrlq	$4,%xmm3,%xmm2
	vpand	%xmm15,%xmm1,%xmm1
	vpand	0(%rcx),%xmm4,%xmm4
	vpsrlq	$30,%xmm3,%xmm3
	vpand	%xmm15,%xmm2,%xmm2
	vpand	%xmm15,%xmm3,%xmm3
	vpor	32(%rcx),%xmm4,%xmm4

	vpaddq	0(%r11),%xmm0,%xmm0
	vpaddq	16(%r11),%xmm1,%xmm1
	vpaddq	32(%r11),%xmm2,%xmm2
	vpaddq	48(%r11),%xmm3,%xmm3
	vpaddq	64(%r11),%xmm4,%xmm4

	leaq	32(%rsi),%rax
	leaq	64(%rsi),%rsi
	subq	$64,%rdx
	cmovcq	%rax,%rsi










	vpmuludq	%xmm0,%xmm9,%xmm5
	vpmuludq	%xmm1,%xmm9,%xmm6
	vpaddq	%xmm5,%xmm10,%xmm10
	vpaddq	%xmm6,%xmm11,%xmm11
	vmovdqa	-128(%r11),%xmm7
	vpmuludq	%xmm2,%xmm9,%xmm5
	vpmuludq	%xmm3,%xmm9,%xmm6
	vpaddq	%xmm5,%xmm12,%xmm12
	vpaddq	%xmm6,%xmm13,%xmm13
	vpmuludq	%xmm4,%xmm9,%xmm9
	vpmuludq	-112(%r11),%xmm4,%xmm5
	vpaddq	%xmm9,%xmm14,%xmm14

	vpaddq	%xmm5,%xmm10,%xmm10
	vpmuludq	%xmm2,%xmm7,%xmm6
	vpmuludq	%xmm3,%xmm7,%xmm5
	vpaddq	%xmm6,%xmm13,%xmm13
	vmovdqa	-96(%r11),%xmm8
	vpaddq	%xmm5,%xmm14,%xmm14
	vpmuludq	%xmm1,%xmm7,%xmm6
	vpmuludq	%xmm0,%xmm7,%xmm7
	vpaddq	%xmm6,%xmm12,%xmm12
	vpaddq	%xmm7,%xmm11,%xmm11

	vmovdqa	-80(%r11),%xmm9
	vpmuludq	%xmm2,%xmm8,%xmm5
	vpmuludq	%xmm1,%xmm8,%xmm6
	vpaddq	%xmm5,%xmm14,%xmm14
	vpaddq	%xmm6,%xmm13,%xmm13
	vmovdqa	-64(%r11),%xmm7
	vpmuludq	%xmm0,%xmm8,%xmm8
	vpmuludq	%xmm4,%xmm9,%xmm5
	vpaddq	%xmm8,%xmm12,%xmm12
	vpaddq	%xmm5,%xmm11,%xmm11
	vmovdqa	-48(%r11),%xmm8
	vpmuludq	%xmm3,%xmm9,%xmm9
	vpmuludq	%xmm1,%xmm7,%xmm6
	vpaddq	%xmm9,%xmm10,%xmm10

	vmovdqa	-16(%r11),%xmm9
	vpaddq	%xmm6,%xmm14,%xmm14
	vpmuludq	%xmm0,%xmm7,%xmm7
	vpmuludq	%xmm4,%xmm8,%xmm5
	vpaddq	%xmm7,%xmm13,%xmm13
	vpaddq	%xmm5,%xmm12,%xmm12
	vmovdqu	32(%rsi),%xmm5
	vpmuludq	%xmm3,%xmm8,%xmm7
	vpmuludq	%xmm2,%xmm8,%xmm8
	vpaddq	%xmm7,%xmm11,%xmm11
	vmovdqu	48(%rsi),%xmm6
	vpaddq	%xmm8,%xmm10,%xmm10

	vpmuludq	%xmm2,%xmm9,%xmm2
	vpmuludq	%xmm3,%xmm9,%xmm3
	vpsrldq	$6,%xmm5,%xmm7
	vpaddq	%xmm2,%xmm11,%xmm11
	vpmuludq	%xmm4,%xmm9,%xmm4
	vpsrldq	$6,%xmm6,%xmm8
	vpaddq	%xmm3,%xmm12,%xmm2
	vpaddq	%xmm4,%xmm13,%xmm3
	vpmuludq	-32(%r11),%xmm0,%xmm4
	vpmuludq	%xmm1,%xmm9,%xmm0
	vpunpckhqdq	%xmm6,%xmm5,%xmm9
	vpaddq	%xmm4,%xmm14,%xmm4
	vpaddq	%xmm0,%xmm10,%xmm0

	vpunpcklqdq	%xmm6,%xmm5,%xmm5
	vpunpcklqdq	%xmm8,%xmm7,%xmm8


	vpsrldq	$5,%xmm9,%xmm9
	vpsrlq	$26,%xmm5,%xmm6
	vmovdqa	0(%rsp),%xmm14
	vpand	%xmm15,%xmm5,%xmm5
	vpsrlq	$4,%xmm8,%xmm7
	vpand	%xmm15,%xmm6,%xmm6
	vpand	0(%rcx),%xmm9,%xmm9
	vpsrlq	$30,%xmm8,%xmm8
	vpand	%xmm15,%xmm7,%xmm7
	vpand	%xmm15,%xmm8,%xmm8
	vpor	32(%rcx),%xmm9,%xmm9





	vpsrlq	$26,%xmm3,%xmm13
	vpand	%xmm15,%xmm3,%xmm3
	vpaddq	%xmm13,%xmm4,%xmm4

	vpsrlq	$26,%xmm0,%xmm10
	vpand	%xmm15,%xmm0,%xmm0
	vpaddq	%xmm10,%xmm11,%xmm1

	vpsrlq	$26,%xmm4,%xmm10
	vpand	%xmm15,%xmm4,%xmm4

	vpsrlq	$26,%xmm1,%xmm11
	vpand	%xmm15,%xmm1,%xmm1
	vpaddq	%xmm11,%xmm2,%xmm2

	vpaddq	%xmm10,%xmm0,%xmm0
	vpsllq	$2,%xmm10,%xmm10
	vpaddq	%xmm10,%xmm0,%xmm0

	vpsrlq	$26,%xmm2,%xmm12
	vpand	%xmm15,%xmm2,%xmm2
	vpaddq	%xmm12,%xmm3,%xmm3

	vpsrlq	$26,%xmm0,%xmm10
	vpand	%xmm15,%xmm0,%xmm0
	vpaddq	%xmm10,%xmm1,%xmm1

	vpsrlq	$26,%xmm3,%xmm13
	vpand	%xmm15,%xmm3,%xmm3
	vpaddq	%xmm13,%xmm4,%xmm4

	ja	L$oop_avx

L$skip_loop_avx:



	vpshufd	$0x10,%xmm14,%xmm14
	addq	$32,%rdx
	jnz	L$ong_tail_avx

	vpaddq	%xmm2,%xmm7,%xmm7
	vpaddq	%xmm0,%xmm5,%xmm5
	vpaddq	%xmm1,%xmm6,%xmm6
	vpaddq	%xmm3,%xmm8,%xmm8
	vpaddq	%xmm4,%xmm9,%xmm9

L$ong_tail_avx:
	vmovdqa	%xmm2,32(%r11)
	vmovdqa	%xmm0,0(%r11)
	vmovdqa	%xmm1,16(%r11)
	vmovdqa	%xmm3,48(%r11)
	vmovdqa	%xmm4,64(%r11)







	vpmuludq	%xmm7,%xmm14,%xmm12
	vpmuludq	%xmm5,%xmm14,%xmm10
	vpshufd	$0x10,-48(%rdi),%xmm2
	vpmuludq	%xmm6,%xmm14,%xmm11
	vpmuludq	%xmm8,%xmm14,%xmm13
	vpmuludq	%xmm9,%xmm14,%xmm14

	vpmuludq	%xmm8,%xmm2,%xmm0
	vpaddq	%xmm0,%xmm14,%xmm14
	vpshufd	$0x10,-32(%rdi),%xmm3
	vpmuludq	%xmm7,%xmm2,%xmm1
	vpaddq	%xmm1,%xmm13,%xmm13
	vpshufd	$0x10,-16(%rdi),%xmm4
	vpmuludq	%xmm6,%xmm2,%xmm0
	vpaddq	%xmm0,%xmm12,%xmm12
	vpmuludq	%xmm5,%xmm2,%xmm2
	vpaddq	%xmm2,%xmm11,%xmm11
	vpmuludq	%xmm9,%xmm3,%xmm3
	vpaddq	%xmm3,%xmm10,%xmm10

	vpshufd	$0x10,0(%rdi),%xmm2
	vpmuludq	%xmm7,%xmm4,%xmm1
	vpaddq	%xmm1,%xmm14,%xmm14
	vpmuludq	%xmm6,%xmm4,%xmm0
	vpaddq	%xmm0,%xmm13,%xmm13
	vpshufd	$0x10,16(%rdi),%xmm3
	vpmuludq	%xmm5,%xmm4,%xmm4
	vpaddq	%xmm4,%xmm12,%xmm12
	vpmuludq	%xmm9,%xmm2,%xmm1
	vpaddq	%xmm1,%xmm11,%xmm11
	vpshufd	$0x10,32(%rdi),%xmm4
	vpmuludq	%xmm8,%xmm2,%xmm2
	vpaddq	%xmm2,%xmm10,%xmm10

	vpmuludq	%xmm6,%xmm3,%xmm0
	vpaddq	%xmm0,%xmm14,%xmm14
	vpmuludq	%xmm5,%xmm3,%xmm3
	vpaddq	%xmm3,%xmm13,%xmm13
	vpshufd	$0x10,48(%rdi),%xmm2
	vpmuludq	%xmm9,%xmm4,%xmm1
	vpaddq	%xmm1,%xmm12,%xmm12
	vpshufd	$0x10,64(%rdi),%xmm3
	vpmuludq	%xmm8,%xmm4,%xmm0
	vpaddq	%xmm0,%xmm11,%xmm11
	vpmuludq	%xmm7,%xmm4,%xmm4
	vpaddq	%xmm4,%xmm10,%xmm10

	vpmuludq	%xmm5,%xmm2,%xmm2
	vpaddq	%xmm2,%xmm14,%xmm14
	vpmuludq	%xmm9,%xmm3,%xmm1
	vpaddq	%xmm1,%xmm13,%xmm13
	vpmuludq	%xmm8,%xmm3,%xmm0
	vpaddq	%xmm0,%xmm12,%xmm12
	vpmuludq	%xmm7,%xmm3,%xmm1
	vpaddq	%xmm1,%xmm11,%xmm11
	vpmuludq	%xmm6,%xmm3,%xmm3
	vpaddq	%xmm3,%xmm10,%xmm10

	jz	L$short_tail_avx

	vmovdqu	0(%rsi),%xmm0
	vmovdqu	16(%rsi),%xmm1

	vpsrldq	$6,%xmm0,%xmm2
	vpsrldq	$6,%xmm1,%xmm3
	vpunpckhqdq	%xmm1,%xmm0,%xmm4
	vpunpcklqdq	%xmm1,%xmm0,%xmm0
	vpunpcklqdq	%xmm3,%xmm2,%xmm3

	vpsrlq	$40,%xmm4,%xmm4
	vpsrlq	$26,%xmm0,%xmm1
	vpand	%xmm15,%xmm0,%xmm0
	vpsrlq	$4,%xmm3,%xmm2
	vpand	%xmm15,%xmm1,%xmm1
	vpsrlq	$30,%xmm3,%xmm3
	vpand	%xmm15,%xmm2,%xmm2
	vpand	%xmm15,%xmm3,%xmm3
	vpor	32(%rcx),%xmm4,%xmm4

	vpshufd	$0x32,-64(%rdi),%xmm9
	vpaddq	0(%r11),%xmm0,%xmm0
	vpaddq	16(%r11),%xmm1,%xmm1
	vpaddq	32(%r11),%xmm2,%xmm2
	vpaddq	48(%r11),%xmm3,%xmm3
	vpaddq	64(%r11),%xmm4,%xmm4




	vpmuludq	%xmm0,%xmm9,%xmm5
	vpaddq	%xmm5,%xmm10,%xmm10
	vpmuludq	%xmm1,%xmm9,%xmm6
	vpaddq	%xmm6,%xmm11,%xmm11
	vpmuludq	%xmm2,%xmm9,%xmm5
	vpaddq	%xmm5,%xmm12,%xmm12
	vpshufd	$0x32,-48(%rdi),%xmm7
	vpmuludq	%xmm3,%xmm9,%xmm6
	vpaddq	%xmm6,%xmm13,%xmm13
	vpmuludq	%xmm4,%xmm9,%xmm9
	vpaddq	%xmm9,%xmm14,%xmm14

	vpmuludq	%xmm3,%xmm7,%xmm5
	vpaddq	%xmm5,%xmm14,%xmm14
	vpshufd	$0x32,-32(%rdi),%xmm8
	vpmuludq	%xmm2,%xmm7,%xmm6
	vpaddq	%xmm6,%xmm13,%xmm13
	vpshufd	$0x32,-16(%rdi),%xmm9
	vpmuludq	%xmm1,%xmm7,%xmm5
	vpaddq	%xmm5,%xmm12,%xmm12
	vpmuludq	%xmm0,%xmm7,%xmm7
	vpaddq	%xmm7,%xmm11,%xmm11
	vpmuludq	%xmm4,%xmm8,%xmm8
	vpaddq	%xmm8,%xmm10,%xmm10

	vpshufd	$0x32,0(%rdi),%xmm7
	vpmuludq	%xmm2,%xmm9,%xmm6
	vpaddq	%xmm6,%xmm14,%xmm14
	vpmuludq	%xmm1,%xmm9,%xmm5
	vpaddq	%xmm5,%xmm13,%xmm13
	vpshufd	$0x32,16(%rdi),%xmm8
	vpmuludq	%xmm0,%xmm9,%xmm9
	vpaddq	%xmm9,%xmm12,%xmm12
	vpmuludq	%xmm4,%xmm7,%xmm6
	vpaddq	%xmm6,%xmm11,%xmm11
	vpshufd	$0x32,32(%rdi),%xmm9
	vpmuludq	%xmm3,%xmm7,%xmm7
	vpaddq	%xmm7,%xmm10,%xmm10

	vpmuludq	%xmm1,%xmm8,%xmm5
	vpaddq	%xmm5,%xmm14,%xmm14
	vpmuludq	%xmm0,%xmm8,%xmm8
	vpaddq	%xmm8,%xmm13,%xmm13
	vpshufd	$0x32,48(%rdi),%xmm7
	vpmuludq	%xmm4,%xmm9,%xmm6
	vpaddq	%xmm6,%xmm12,%xmm12
	vpshufd	$0x32,64(%rdi),%xmm8
	vpmuludq	%xmm3,%xmm9,%xmm5
	vpaddq	%xmm5,%xmm11,%xmm11
	vpmuludq	%xmm2,%xmm9,%xmm9
	vpaddq	%xmm9,%xmm10,%xmm10

	vpmuludq	%xmm0,%xmm7,%xmm7
	vpaddq	%xmm7,%xmm14,%xmm14
	vpmuludq	%xmm4,%xmm8,%xmm6
	vpaddq	%xmm6,%xmm13,%xmm13
	vpmuludq	%xmm3,%xmm8,%xmm5
	vpaddq	%xmm5,%xmm12,%xmm12
	vpmuludq	%xmm2,%xmm8,%xmm6
	vpaddq	%xmm6,%xmm11,%xmm11
	vpmuludq	%xmm1,%xmm8,%xmm8
	vpaddq	%xmm8,%xmm10,%xmm10

L$short_tail_avx:



	vpsrldq	$8,%xmm14,%xmm9
	vpsrldq	$8,%xmm13,%xmm8
	vpsrldq	$8,%xmm11,%xmm6
	vpsrldq	$8,%xmm10,%xmm5
	vpsrldq	$8,%xmm12,%xmm7
	vpaddq	%xmm8,%xmm13,%xmm13
	vpaddq	%xmm9,%xmm14,%xmm14
	vpaddq	%xmm5,%xmm10,%xmm10
	vpaddq	%xmm6,%xmm11,%xmm11
	vpaddq	%xmm7,%xmm12,%xmm12




	vpsrlq	$26,%xmm13,%xmm3
	vpand	%xmm15,%xmm13,%xmm13
	vpaddq	%xmm3,%xmm14,%xmm14

	vpsrlq	$26,%xmm10,%xmm0
	vpand	%xmm15,%xmm10,%xmm10
	vpaddq	%xmm0,%xmm11,%xmm11

	vpsrlq	$26,%xmm14,%xmm4
	vpand	%xmm15,%xmm14,%xmm14

	vpsrlq	$26,%xmm11,%xmm1
	vpand	%xmm15,%xmm11,%xmm11
	vpaddq	%xmm1,%xmm12,%xmm12

	vpaddq	%xmm4,%xmm10,%xmm10
	vpsllq	$2,%xmm4,%xmm4
	vpaddq	%xmm4,%xmm10,%xmm10

	vpsrlq	$26,%xmm12,%xmm2
	vpand	%xmm15,%xmm12,%xmm12
	vpaddq	%xmm2,%xmm13,%xmm13

	vpsrlq	$26,%xmm10,%xmm0
	vpand	%xmm15,%xmm10,%xmm10
	vpaddq	%xmm0,%xmm11,%xmm11

	vpsrlq	$26,%xmm13,%xmm3
	vpand	%xmm15,%xmm13,%xmm13
	vpaddq	%xmm3,%xmm14,%xmm14

	vmovd	%xmm10,-112(%rdi)
	vmovd	%xmm11,-108(%rdi)
	vmovd	%xmm12,-104(%rdi)
	vmovd	%xmm13,-100(%rdi)
	vmovd	%xmm14,-96(%rdi)
	leaq	88(%r11),%rsp

	vzeroupper
	.byte	0xf3,0xc3




.p2align	5
poly1305_emit_avx:
	cmpl	$0,20(%rdi)
	je	L$emit

	movl	0(%rdi),%eax
	movl	4(%rdi),%ecx
	movl	8(%rdi),%r8d
	movl	12(%rdi),%r11d
	movl	16(%rdi),%r10d

	shlq	$26,%rcx
	movq	%r8,%r9
	shlq	$52,%r8
	addq	%rcx,%rax
	shrq	$12,%r9
	addq	%rax,%r8
	adcq	$0,%r9

	shlq	$14,%r11
	movq	%r10,%rax
	shrq	$24,%r10
	addq	%r11,%r9
	shlq	$40,%rax
	addq	%rax,%r9
	adcq	$0,%r10

	movq	%r10,%rax
	movq	%r10,%rcx
	andq	$3,%r10
	shrq	$2,%rax
	andq	$-4,%rcx
	addq	%rcx,%rax
	addq	%rax,%r8
	adcq	$0,%r9
	adcq	$0,%r10

	movq	%r8,%rax
	addq	$5,%r8
	movq	%r9,%rcx
	adcq	$0,%r9
	adcq	$0,%r10
	shrq	$2,%r10
	cmovnzq	%r8,%rax
	cmovnzq	%r9,%rcx

	addq	0(%rdx),%rax
	adcq	8(%rdx),%rcx
	movq	%rax,0(%rsi)
	movq	%rcx,8(%rsi)

	.byte	0xf3,0xc3


.p2align	5
poly1305_blocks_avx2:

	movl	20(%rdi),%r8d
	cmpq	$128,%rdx
	jae	L$blocks_avx2
	testl	%r8d,%r8d
	jz	L$blocks

L$blocks_avx2:
	andq	$-16,%rdx
	jz	L$no_data_avx2

	vzeroupper

	testl	%r8d,%r8d
	jz	L$base2_64_avx2

	testq	$63,%rdx
	jz	L$even_avx2

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$blocks_avx2_body:

	movq	%rdx,%r15

	movq	0(%rdi),%r8
	movq	8(%rdi),%r9
	movl	16(%rdi),%ebp

	movq	24(%rdi),%r11
	movq	32(%rdi),%r13


	movl	%r8d,%r14d
	andq	$-2147483648,%r8
	movq	%r9,%r12
	movl	%r9d,%ebx
	andq	$-2147483648,%r9

	shrq	$6,%r8
	shlq	$52,%r12
	addq	%r8,%r14
	shrq	$12,%rbx
	shrq	$18,%r9
	addq	%r12,%r14
	adcq	%r9,%rbx

	movq	%rbp,%r8
	shlq	$40,%r8
	shrq	$24,%rbp
	addq	%r8,%rbx
	adcq	$0,%rbp

	movq	$-4,%r9
	movq	%rbp,%r8
	andq	%rbp,%r9
	shrq	$2,%r8
	andq	$3,%rbp
	addq	%r9,%r8
	addq	%r8,%r14
	adcq	$0,%rbx
	adcq	$0,%rbp

	movq	%r13,%r12
	movq	%r13,%rax
	shrq	$2,%r13
	addq	%r12,%r13

L$base2_26_pre_avx2:
	addq	0(%rsi),%r14
	adcq	8(%rsi),%rbx
	leaq	16(%rsi),%rsi
	adcq	%rcx,%rbp
	subq	$16,%r15

	call	__poly1305_block
	movq	%r12,%rax

	testq	$63,%r15
	jnz	L$base2_26_pre_avx2

	testq	%rcx,%rcx
	jz	L$store_base2_64_avx2


	movq	%r14,%rax
	movq	%r14,%rdx
	shrq	$52,%r14
	movq	%rbx,%r11
	movq	%rbx,%r12
	shrq	$26,%rdx
	andq	$0x3ffffff,%rax
	shlq	$12,%r11
	andq	$0x3ffffff,%rdx
	shrq	$14,%rbx
	orq	%r11,%r14
	shlq	$24,%rbp
	andq	$0x3ffffff,%r14
	shrq	$40,%r12
	andq	$0x3ffffff,%rbx
	orq	%r12,%rbp

	testq	%r15,%r15
	jz	L$store_base2_26_avx2

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%ebp,%xmm4
	jmp	L$proceed_avx2

.p2align	5
L$store_base2_64_avx2:
	movq	%r14,0(%rdi)
	movq	%rbx,8(%rdi)
	movq	%rbp,16(%rdi)
	jmp	L$done_avx2

.p2align	4
L$store_base2_26_avx2:
	movl	%eax,0(%rdi)
	movl	%edx,4(%rdi)
	movl	%r14d,8(%rdi)
	movl	%ebx,12(%rdi)
	movl	%ebp,16(%rdi)
.p2align	4
L$done_avx2:
	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbp

	movq	40(%rsp),%rbx

	leaq	48(%rsp),%rsp

L$no_data_avx2:
L$blocks_avx2_epilogue:
	.byte	0xf3,0xc3


.p2align	5
L$base2_64_avx2:

	pushq	%rbx

	pushq	%rbp

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

L$base2_64_avx2_body:

	movq	%rdx,%r15

	movq	24(%rdi),%r11
	movq	32(%rdi),%r13

	movq	0(%rdi),%r14
	movq	8(%rdi),%rbx
	movl	16(%rdi),%ebp

	movq	%r13,%r12
	movq	%r13,%rax
	shrq	$2,%r13
	addq	%r12,%r13

	testq	$63,%rdx
	jz	L$init_avx2

L$base2_64_pre_avx2:
	addq	0(%rsi),%r14
	adcq	8(%rsi),%rbx
	leaq	16(%rsi),%rsi
	adcq	%rcx,%rbp
	subq	$16,%r15

	call	__poly1305_block
	movq	%r12,%rax

	testq	$63,%r15
	jnz	L$base2_64_pre_avx2

L$init_avx2:

	movq	%r14,%rax
	movq	%r14,%rdx
	shrq	$52,%r14
	movq	%rbx,%r8
	movq	%rbx,%r9
	shrq	$26,%rdx
	andq	$0x3ffffff,%rax
	shlq	$12,%r8
	andq	$0x3ffffff,%rdx
	shrq	$14,%rbx
	orq	%r8,%r14
	shlq	$24,%rbp
	andq	$0x3ffffff,%r14
	shrq	$40,%r9
	andq	$0x3ffffff,%rbx
	orq	%r9,%rbp

	vmovd	%eax,%xmm0
	vmovd	%edx,%xmm1
	vmovd	%r14d,%xmm2
	vmovd	%ebx,%xmm3
	vmovd	%ebp,%xmm4
	movl	$1,20(%rdi)

	call	__poly1305_init_avx

L$proceed_avx2:
	movq	%r15,%rdx
	movl	_OPENSSL_ia32cap_P+8(%rip),%r10d
	movl	$3221291008,%r11d

	movq	0(%rsp),%r15

	movq	8(%rsp),%r14

	movq	16(%rsp),%r13

	movq	24(%rsp),%r12

	movq	32(%rsp),%rbp

	movq	40(%rsp),%rbx

	leaq	48(%rsp),%rax
	leaq	48(%rsp),%rsp

L$base2_64_avx2_epilogue:
	jmp	L$do_avx2


.p2align	5
L$even_avx2:

	movl	_OPENSSL_ia32cap_P+8(%rip),%r10d
	vmovd	0(%rdi),%xmm0
	vmovd	4(%rdi),%xmm1
	vmovd	8(%rdi),%xmm2
	vmovd	12(%rdi),%xmm3
	vmovd	16(%rdi),%xmm4

L$do_avx2:
	cmpq	$512,%rdx
	jb	L$skip_avx512
	andl	%r11d,%r10d
	testl	$65536,%r10d
	jnz	L$blocks_avx512
L$skip_avx512:
	leaq	-8(%rsp),%r11

	subq	$0x128,%rsp
	leaq	L$const(%rip),%rcx
	leaq	48+64(%rdi),%rdi
	vmovdqa	96(%rcx),%ymm7


	vmovdqu	-64(%rdi),%xmm9
	andq	$-512,%rsp
	vmovdqu	-48(%rdi),%xmm10
	vmovdqu	-32(%rdi),%xmm6
	vmovdqu	-16(%rdi),%xmm11
	vmovdqu	0(%rdi),%xmm12
	vmovdqu	16(%rdi),%xmm13
	leaq	144(%rsp),%rax
	vmovdqu	32(%rdi),%xmm14
	vpermd	%ymm9,%ymm7,%ymm9
	vmovdqu	48(%rdi),%xmm15
	vpermd	%ymm10,%ymm7,%ymm10
	vmovdqu	64(%rdi),%xmm5
	vpermd	%ymm6,%ymm7,%ymm6
	vmovdqa	%ymm9,0(%rsp)
	vpermd	%ymm11,%ymm7,%ymm11
	vmovdqa	%ymm10,32-144(%rax)
	vpermd	%ymm12,%ymm7,%ymm12
	vmovdqa	%ymm6,64-144(%rax)
	vpermd	%ymm13,%ymm7,%ymm13
	vmovdqa	%ymm11,96-144(%rax)
	vpermd	%ymm14,%ymm7,%ymm14
	vmovdqa	%ymm12,128-144(%rax)
	vpermd	%ymm15,%ymm7,%ymm15
	vmovdqa	%ymm13,160-144(%rax)
	vpermd	%ymm5,%ymm7,%ymm5
	vmovdqa	%ymm14,192-144(%rax)
	vmovdqa	%ymm15,224-144(%rax)
	vmovdqa	%ymm5,256-144(%rax)
	vmovdqa	64(%rcx),%ymm5



	vmovdqu	0(%rsi),%xmm7
	vmovdqu	16(%rsi),%xmm8
	vinserti128	$1,32(%rsi),%ymm7,%ymm7
	vinserti128	$1,48(%rsi),%ymm8,%ymm8
	leaq	64(%rsi),%rsi

	vpsrldq	$6,%ymm7,%ymm9
	vpsrldq	$6,%ymm8,%ymm10
	vpunpckhqdq	%ymm8,%ymm7,%ymm6
	vpunpcklqdq	%ymm10,%ymm9,%ymm9
	vpunpcklqdq	%ymm8,%ymm7,%ymm7

	vpsrlq	$30,%ymm9,%ymm10
	vpsrlq	$4,%ymm9,%ymm9
	vpsrlq	$26,%ymm7,%ymm8
	vpsrlq	$40,%ymm6,%ymm6
	vpand	%ymm5,%ymm9,%ymm9
	vpand	%ymm5,%ymm7,%ymm7
	vpand	%ymm5,%ymm8,%ymm8
	vpand	%ymm5,%ymm10,%ymm10
	vpor	32(%rcx),%ymm6,%ymm6

	vpaddq	%ymm2,%ymm9,%ymm2
	subq	$64,%rdx
	jz	L$tail_avx2
	jmp	L$oop_avx2

.p2align	5
L$oop_avx2:








	vpaddq	%ymm0,%ymm7,%ymm0
	vmovdqa	0(%rsp),%ymm7
	vpaddq	%ymm1,%ymm8,%ymm1
	vmovdqa	32(%rsp),%ymm8
	vpaddq	%ymm3,%ymm10,%ymm3
	vmovdqa	96(%rsp),%ymm9
	vpaddq	%ymm4,%ymm6,%ymm4
	vmovdqa	48(%rax),%ymm10
	vmovdqa	112(%rax),%ymm5
















	vpmuludq	%ymm2,%ymm7,%ymm13
	vpmuludq	%ymm2,%ymm8,%ymm14
	vpmuludq	%ymm2,%ymm9,%ymm15
	vpmuludq	%ymm2,%ymm10,%ymm11
	vpmuludq	%ymm2,%ymm5,%ymm12

	vpmuludq	%ymm0,%ymm8,%ymm6
	vpmuludq	%ymm1,%ymm8,%ymm2
	vpaddq	%ymm6,%ymm12,%ymm12
	vpaddq	%ymm2,%ymm13,%ymm13
	vpmuludq	%ymm3,%ymm8,%ymm6
	vpmuludq	64(%rsp),%ymm4,%ymm2
	vpaddq	%ymm6,%ymm15,%ymm15
	vpaddq	%ymm2,%ymm11,%ymm11
	vmovdqa	-16(%rax),%ymm8

	vpmuludq	%ymm0,%ymm7,%ymm6
	vpmuludq	%ymm1,%ymm7,%ymm2
	vpaddq	%ymm6,%ymm11,%ymm11
	vpaddq	%ymm2,%ymm12,%ymm12
	vpmuludq	%ymm3,%ymm7,%ymm6
	vpmuludq	%ymm4,%ymm7,%ymm2
	vmovdqu	0(%rsi),%xmm7
	vpaddq	%ymm6,%ymm14,%ymm14
	vpaddq	%ymm2,%ymm15,%ymm15
	vinserti128	$1,32(%rsi),%ymm7,%ymm7

	vpmuludq	%ymm3,%ymm8,%ymm6
	vpmuludq	%ymm4,%ymm8,%ymm2
	vmovdqu	16(%rsi),%xmm8
	vpaddq	%ymm6,%ymm11,%ymm11
	vpaddq	%ymm2,%ymm12,%ymm12
	vmovdqa	16(%rax),%ymm2
	vpmuludq	%ymm1,%ymm9,%ymm6
	vpmuludq	%ymm0,%ymm9,%ymm9
	vpaddq	%ymm6,%ymm14,%ymm14
	vpaddq	%ymm9,%ymm13,%ymm13
	vinserti128	$1,48(%rsi),%ymm8,%ymm8
	leaq	64(%rsi),%rsi

	vpmuludq	%ymm1,%ymm2,%ymm6
	vpmuludq	%ymm0,%ymm2,%ymm2
	vpsrldq	$6,%ymm7,%ymm9
	vpaddq	%ymm6,%ymm15,%ymm15
	vpaddq	%ymm2,%ymm14,%ymm14
	vpmuludq	%ymm3,%ymm10,%ymm6
	vpmuludq	%ymm4,%ymm10,%ymm2
	vpsrldq	$6,%ymm8,%ymm10
	vpaddq	%ymm6,%ymm12,%ymm12
	vpaddq	%ymm2,%ymm13,%ymm13
	vpunpckhqdq	%ymm8,%ymm7,%ymm6

	vpmuludq	%ymm3,%ymm5,%ymm3
	vpmuludq	%ymm4,%ymm5,%ymm4
	vpunpcklqdq	%ymm8,%ymm7,%ymm7
	vpaddq	%ymm3,%ymm13,%ymm2
	vpaddq	%ymm4,%ymm14,%ymm3
	vpunpcklqdq	%ymm10,%ymm9,%ymm10
	vpmuludq	80(%rax),%ymm0,%ymm4
	vpmuludq	%ymm1,%ymm5,%ymm0
	vmovdqa	64(%rcx),%ymm5
	vpaddq	%ymm4,%ymm15,%ymm4
	vpaddq	%ymm0,%ymm11,%ymm0




	vpsrlq	$26,%ymm3,%ymm14
	vpand	%ymm5,%ymm3,%ymm3
	vpaddq	%ymm14,%ymm4,%ymm4

	vpsrlq	$26,%ymm0,%ymm11
	vpand	%ymm5,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm12,%ymm1

	vpsrlq	$26,%ymm4,%ymm15
	vpand	%ymm5,%ymm4,%ymm4

	vpsrlq	$4,%ymm10,%ymm9

	vpsrlq	$26,%ymm1,%ymm12
	vpand	%ymm5,%ymm1,%ymm1
	vpaddq	%ymm12,%ymm2,%ymm2

	vpaddq	%ymm15,%ymm0,%ymm0
	vpsllq	$2,%ymm15,%ymm15
	vpaddq	%ymm15,%ymm0,%ymm0

	vpand	%ymm5,%ymm9,%ymm9
	vpsrlq	$26,%ymm7,%ymm8

	vpsrlq	$26,%ymm2,%ymm13
	vpand	%ymm5,%ymm2,%ymm2
	vpaddq	%ymm13,%ymm3,%ymm3

	vpaddq	%ymm9,%ymm2,%ymm2
	vpsrlq	$30,%ymm10,%ymm10

	vpsrlq	$26,%ymm0,%ymm11
	vpand	%ymm5,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm1,%ymm1

	vpsrlq	$40,%ymm6,%ymm6

	vpsrlq	$26,%ymm3,%ymm14
	vpand	%ymm5,%ymm3,%ymm3
	vpaddq	%ymm14,%ymm4,%ymm4

	vpand	%ymm5,%ymm7,%ymm7
	vpand	%ymm5,%ymm8,%ymm8
	vpand	%ymm5,%ymm10,%ymm10
	vpor	32(%rcx),%ymm6,%ymm6

	subq	$64,%rdx
	jnz	L$oop_avx2

.byte	0x66,0x90
L$tail_avx2:







	vpaddq	%ymm0,%ymm7,%ymm0
	vmovdqu	4(%rsp),%ymm7
	vpaddq	%ymm1,%ymm8,%ymm1
	vmovdqu	36(%rsp),%ymm8
	vpaddq	%ymm3,%ymm10,%ymm3
	vmovdqu	100(%rsp),%ymm9
	vpaddq	%ymm4,%ymm6,%ymm4
	vmovdqu	52(%rax),%ymm10
	vmovdqu	116(%rax),%ymm5

	vpmuludq	%ymm2,%ymm7,%ymm13
	vpmuludq	%ymm2,%ymm8,%ymm14
	vpmuludq	%ymm2,%ymm9,%ymm15
	vpmuludq	%ymm2,%ymm10,%ymm11
	vpmuludq	%ymm2,%ymm5,%ymm12

	vpmuludq	%ymm0,%ymm8,%ymm6
	vpmuludq	%ymm1,%ymm8,%ymm2
	vpaddq	%ymm6,%ymm12,%ymm12
	vpaddq	%ymm2,%ymm13,%ymm13
	vpmuludq	%ymm3,%ymm8,%ymm6
	vpmuludq	68(%rsp),%ymm4,%ymm2
	vpaddq	%ymm6,%ymm15,%ymm15
	vpaddq	%ymm2,%ymm11,%ymm11

	vpmuludq	%ymm0,%ymm7,%ymm6
	vpmuludq	%ymm1,%ymm7,%ymm2
	vpaddq	%ymm6,%ymm11,%ymm11
	vmovdqu	-12(%rax),%ymm8
	vpaddq	%ymm2,%ymm12,%ymm12
	vpmuludq	%ymm3,%ymm7,%ymm6
	vpmuludq	%ymm4,%ymm7,%ymm2
	vpaddq	%ymm6,%ymm14,%ymm14
	vpaddq	%ymm2,%ymm15,%ymm15

	vpmuludq	%ymm3,%ymm8,%ymm6
	vpmuludq	%ymm4,%ymm8,%ymm2
	vpaddq	%ymm6,%ymm11,%ymm11
	vpaddq	%ymm2,%ymm12,%ymm12
	vmovdqu	20(%rax),%ymm2
	vpmuludq	%ymm1,%ymm9,%ymm6
	vpmuludq	%ymm0,%ymm9,%ymm9
	vpaddq	%ymm6,%ymm14,%ymm14
	vpaddq	%ymm9,%ymm13,%ymm13

	vpmuludq	%ymm1,%ymm2,%ymm6
	vpmuludq	%ymm0,%ymm2,%ymm2
	vpaddq	%ymm6,%ymm15,%ymm15
	vpaddq	%ymm2,%ymm14,%ymm14
	vpmuludq	%ymm3,%ymm10,%ymm6
	vpmuludq	%ymm4,%ymm10,%ymm2
	vpaddq	%ymm6,%ymm12,%ymm12
	vpaddq	%ymm2,%ymm13,%ymm13

	vpmuludq	%ymm3,%ymm5,%ymm3
	vpmuludq	%ymm4,%ymm5,%ymm4
	vpaddq	%ymm3,%ymm13,%ymm2
	vpaddq	%ymm4,%ymm14,%ymm3
	vpmuludq	84(%rax),%ymm0,%ymm4
	vpmuludq	%ymm1,%ymm5,%ymm0
	vmovdqa	64(%rcx),%ymm5
	vpaddq	%ymm4,%ymm15,%ymm4
	vpaddq	%ymm0,%ymm11,%ymm0




	vpsrldq	$8,%ymm12,%ymm8
	vpsrldq	$8,%ymm2,%ymm9
	vpsrldq	$8,%ymm3,%ymm10
	vpsrldq	$8,%ymm4,%ymm6
	vpsrldq	$8,%ymm0,%ymm7
	vpaddq	%ymm8,%ymm12,%ymm12
	vpaddq	%ymm9,%ymm2,%ymm2
	vpaddq	%ymm10,%ymm3,%ymm3
	vpaddq	%ymm6,%ymm4,%ymm4
	vpaddq	%ymm7,%ymm0,%ymm0

	vpermq	$0x2,%ymm3,%ymm10
	vpermq	$0x2,%ymm4,%ymm6
	vpermq	$0x2,%ymm0,%ymm7
	vpermq	$0x2,%ymm12,%ymm8
	vpermq	$0x2,%ymm2,%ymm9
	vpaddq	%ymm10,%ymm3,%ymm3
	vpaddq	%ymm6,%ymm4,%ymm4
	vpaddq	%ymm7,%ymm0,%ymm0
	vpaddq	%ymm8,%ymm12,%ymm12
	vpaddq	%ymm9,%ymm2,%ymm2




	vpsrlq	$26,%ymm3,%ymm14
	vpand	%ymm5,%ymm3,%ymm3
	vpaddq	%ymm14,%ymm4,%ymm4

	vpsrlq	$26,%ymm0,%ymm11
	vpand	%ymm5,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm12,%ymm1

	vpsrlq	$26,%ymm4,%ymm15
	vpand	%ymm5,%ymm4,%ymm4

	vpsrlq	$26,%ymm1,%ymm12
	vpand	%ymm5,%ymm1,%ymm1
	vpaddq	%ymm12,%ymm2,%ymm2

	vpaddq	%ymm15,%ymm0,%ymm0
	vpsllq	$2,%ymm15,%ymm15
	vpaddq	%ymm15,%ymm0,%ymm0

	vpsrlq	$26,%ymm2,%ymm13
	vpand	%ymm5,%ymm2,%ymm2
	vpaddq	%ymm13,%ymm3,%ymm3

	vpsrlq	$26,%ymm0,%ymm11
	vpand	%ymm5,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm1,%ymm1

	vpsrlq	$26,%ymm3,%ymm14
	vpand	%ymm5,%ymm3,%ymm3
	vpaddq	%ymm14,%ymm4,%ymm4

	vmovd	%xmm0,-112(%rdi)
	vmovd	%xmm1,-108(%rdi)
	vmovd	%xmm2,-104(%rdi)
	vmovd	%xmm3,-100(%rdi)
	vmovd	%xmm4,-96(%rdi)
	leaq	8(%r11),%rsp

	vzeroupper
	.byte	0xf3,0xc3



.p2align	5
poly1305_blocks_avx512:

L$blocks_avx512:
	movl	$15,%eax
	kmovw	%eax,%k2
	leaq	-8(%rsp),%r11

	subq	$0x128,%rsp
	leaq	L$const(%rip),%rcx
	leaq	48+64(%rdi),%rdi
	vmovdqa	96(%rcx),%ymm9


	vmovdqu	-64(%rdi),%xmm11
	andq	$-512,%rsp
	vmovdqu	-48(%rdi),%xmm12
	movq	$0x20,%rax
	vmovdqu	-32(%rdi),%xmm7
	vmovdqu	-16(%rdi),%xmm13
	vmovdqu	0(%rdi),%xmm8
	vmovdqu	16(%rdi),%xmm14
	vmovdqu	32(%rdi),%xmm10
	vmovdqu	48(%rdi),%xmm15
	vmovdqu	64(%rdi),%xmm6
	vpermd	%zmm11,%zmm9,%zmm16
	vpbroadcastq	64(%rcx),%zmm5
	vpermd	%zmm12,%zmm9,%zmm17
	vpermd	%zmm7,%zmm9,%zmm21
	vpermd	%zmm13,%zmm9,%zmm18
	vmovdqa64	%zmm16,0(%rsp){%k2}
	vpsrlq	$32,%zmm16,%zmm7
	vpermd	%zmm8,%zmm9,%zmm22
	vmovdqu64	%zmm17,0(%rsp,%rax,1){%k2}
	vpsrlq	$32,%zmm17,%zmm8
	vpermd	%zmm14,%zmm9,%zmm19
	vmovdqa64	%zmm21,64(%rsp){%k2}
	vpermd	%zmm10,%zmm9,%zmm23
	vpermd	%zmm15,%zmm9,%zmm20
	vmovdqu64	%zmm18,64(%rsp,%rax,1){%k2}
	vpermd	%zmm6,%zmm9,%zmm24
	vmovdqa64	%zmm22,128(%rsp){%k2}
	vmovdqu64	%zmm19,128(%rsp,%rax,1){%k2}
	vmovdqa64	%zmm23,192(%rsp){%k2}
	vmovdqu64	%zmm20,192(%rsp,%rax,1){%k2}
	vmovdqa64	%zmm24,256(%rsp){%k2}










	vpmuludq	%zmm7,%zmm16,%zmm11
	vpmuludq	%zmm7,%zmm17,%zmm12
	vpmuludq	%zmm7,%zmm18,%zmm13
	vpmuludq	%zmm7,%zmm19,%zmm14
	vpmuludq	%zmm7,%zmm20,%zmm15
	vpsrlq	$32,%zmm18,%zmm9

	vpmuludq	%zmm8,%zmm24,%zmm25
	vpmuludq	%zmm8,%zmm16,%zmm26
	vpmuludq	%zmm8,%zmm17,%zmm27
	vpmuludq	%zmm8,%zmm18,%zmm28
	vpmuludq	%zmm8,%zmm19,%zmm29
	vpsrlq	$32,%zmm19,%zmm10
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm27,%zmm13,%zmm13
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15

	vpmuludq	%zmm9,%zmm23,%zmm25
	vpmuludq	%zmm9,%zmm24,%zmm26
	vpmuludq	%zmm9,%zmm17,%zmm28
	vpmuludq	%zmm9,%zmm18,%zmm29
	vpmuludq	%zmm9,%zmm16,%zmm27
	vpsrlq	$32,%zmm20,%zmm6
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm27,%zmm13,%zmm13

	vpmuludq	%zmm10,%zmm22,%zmm25
	vpmuludq	%zmm10,%zmm16,%zmm28
	vpmuludq	%zmm10,%zmm17,%zmm29
	vpmuludq	%zmm10,%zmm23,%zmm26
	vpmuludq	%zmm10,%zmm24,%zmm27
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm27,%zmm13,%zmm13

	vpmuludq	%zmm6,%zmm24,%zmm28
	vpmuludq	%zmm6,%zmm16,%zmm29
	vpmuludq	%zmm6,%zmm21,%zmm25
	vpmuludq	%zmm6,%zmm22,%zmm26
	vpmuludq	%zmm6,%zmm23,%zmm27
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm27,%zmm13,%zmm13



	vmovdqu64	0(%rsi),%zmm10
	vmovdqu64	64(%rsi),%zmm6
	leaq	128(%rsi),%rsi




	vpsrlq	$26,%zmm14,%zmm28
	vpandq	%zmm5,%zmm14,%zmm14
	vpaddq	%zmm28,%zmm15,%zmm15

	vpsrlq	$26,%zmm11,%zmm25
	vpandq	%zmm5,%zmm11,%zmm11
	vpaddq	%zmm25,%zmm12,%zmm12

	vpsrlq	$26,%zmm15,%zmm29
	vpandq	%zmm5,%zmm15,%zmm15

	vpsrlq	$26,%zmm12,%zmm26
	vpandq	%zmm5,%zmm12,%zmm12
	vpaddq	%zmm26,%zmm13,%zmm13

	vpaddq	%zmm29,%zmm11,%zmm11
	vpsllq	$2,%zmm29,%zmm29
	vpaddq	%zmm29,%zmm11,%zmm11

	vpsrlq	$26,%zmm13,%zmm27
	vpandq	%zmm5,%zmm13,%zmm13
	vpaddq	%zmm27,%zmm14,%zmm14

	vpsrlq	$26,%zmm11,%zmm25
	vpandq	%zmm5,%zmm11,%zmm11
	vpaddq	%zmm25,%zmm12,%zmm12

	vpsrlq	$26,%zmm14,%zmm28
	vpandq	%zmm5,%zmm14,%zmm14
	vpaddq	%zmm28,%zmm15,%zmm15





	vpunpcklqdq	%zmm6,%zmm10,%zmm7
	vpunpckhqdq	%zmm6,%zmm10,%zmm6






	vmovdqa32	128(%rcx),%zmm25
	movl	$0x7777,%eax
	kmovw	%eax,%k1

	vpermd	%zmm16,%zmm25,%zmm16
	vpermd	%zmm17,%zmm25,%zmm17
	vpermd	%zmm18,%zmm25,%zmm18
	vpermd	%zmm19,%zmm25,%zmm19
	vpermd	%zmm20,%zmm25,%zmm20

	vpermd	%zmm11,%zmm25,%zmm16{%k1}
	vpermd	%zmm12,%zmm25,%zmm17{%k1}
	vpermd	%zmm13,%zmm25,%zmm18{%k1}
	vpermd	%zmm14,%zmm25,%zmm19{%k1}
	vpermd	%zmm15,%zmm25,%zmm20{%k1}

	vpslld	$2,%zmm17,%zmm21
	vpslld	$2,%zmm18,%zmm22
	vpslld	$2,%zmm19,%zmm23
	vpslld	$2,%zmm20,%zmm24
	vpaddd	%zmm17,%zmm21,%zmm21
	vpaddd	%zmm18,%zmm22,%zmm22
	vpaddd	%zmm19,%zmm23,%zmm23
	vpaddd	%zmm20,%zmm24,%zmm24

	vpbroadcastq	32(%rcx),%zmm30

	vpsrlq	$52,%zmm7,%zmm9
	vpsllq	$12,%zmm6,%zmm10
	vporq	%zmm10,%zmm9,%zmm9
	vpsrlq	$26,%zmm7,%zmm8
	vpsrlq	$14,%zmm6,%zmm10
	vpsrlq	$40,%zmm6,%zmm6
	vpandq	%zmm5,%zmm9,%zmm9
	vpandq	%zmm5,%zmm7,%zmm7




	vpaddq	%zmm2,%zmm9,%zmm2
	subq	$192,%rdx
	jbe	L$tail_avx512
	jmp	L$oop_avx512

.p2align	5
L$oop_avx512:




























	vpmuludq	%zmm2,%zmm17,%zmm14
	vpaddq	%zmm0,%zmm7,%zmm0
	vpmuludq	%zmm2,%zmm18,%zmm15
	vpandq	%zmm5,%zmm8,%zmm8
	vpmuludq	%zmm2,%zmm23,%zmm11
	vpandq	%zmm5,%zmm10,%zmm10
	vpmuludq	%zmm2,%zmm24,%zmm12
	vporq	%zmm30,%zmm6,%zmm6
	vpmuludq	%zmm2,%zmm16,%zmm13
	vpaddq	%zmm1,%zmm8,%zmm1
	vpaddq	%zmm3,%zmm10,%zmm3
	vpaddq	%zmm4,%zmm6,%zmm4

	vmovdqu64	0(%rsi),%zmm10
	vmovdqu64	64(%rsi),%zmm6
	leaq	128(%rsi),%rsi
	vpmuludq	%zmm0,%zmm19,%zmm28
	vpmuludq	%zmm0,%zmm20,%zmm29
	vpmuludq	%zmm0,%zmm16,%zmm25
	vpmuludq	%zmm0,%zmm17,%zmm26
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm26,%zmm12,%zmm12

	vpmuludq	%zmm1,%zmm18,%zmm28
	vpmuludq	%zmm1,%zmm19,%zmm29
	vpmuludq	%zmm1,%zmm24,%zmm25
	vpmuludq	%zmm0,%zmm18,%zmm27
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm27,%zmm13,%zmm13

	vpunpcklqdq	%zmm6,%zmm10,%zmm7
	vpunpckhqdq	%zmm6,%zmm10,%zmm6

	vpmuludq	%zmm3,%zmm16,%zmm28
	vpmuludq	%zmm3,%zmm17,%zmm29
	vpmuludq	%zmm1,%zmm16,%zmm26
	vpmuludq	%zmm1,%zmm17,%zmm27
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm27,%zmm13,%zmm13

	vpmuludq	%zmm4,%zmm24,%zmm28
	vpmuludq	%zmm4,%zmm16,%zmm29
	vpmuludq	%zmm3,%zmm22,%zmm25
	vpmuludq	%zmm3,%zmm23,%zmm26
	vpaddq	%zmm28,%zmm14,%zmm14
	vpmuludq	%zmm3,%zmm24,%zmm27
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm27,%zmm13,%zmm13

	vpmuludq	%zmm4,%zmm21,%zmm25
	vpmuludq	%zmm4,%zmm22,%zmm26
	vpmuludq	%zmm4,%zmm23,%zmm27
	vpaddq	%zmm25,%zmm11,%zmm0
	vpaddq	%zmm26,%zmm12,%zmm1
	vpaddq	%zmm27,%zmm13,%zmm2




	vpsrlq	$52,%zmm7,%zmm9
	vpsllq	$12,%zmm6,%zmm10

	vpsrlq	$26,%zmm14,%zmm3
	vpandq	%zmm5,%zmm14,%zmm14
	vpaddq	%zmm3,%zmm15,%zmm4

	vporq	%zmm10,%zmm9,%zmm9

	vpsrlq	$26,%zmm0,%zmm11
	vpandq	%zmm5,%zmm0,%zmm0
	vpaddq	%zmm11,%zmm1,%zmm1

	vpandq	%zmm5,%zmm9,%zmm9

	vpsrlq	$26,%zmm4,%zmm15
	vpandq	%zmm5,%zmm4,%zmm4

	vpsrlq	$26,%zmm1,%zmm12
	vpandq	%zmm5,%zmm1,%zmm1
	vpaddq	%zmm12,%zmm2,%zmm2

	vpaddq	%zmm15,%zmm0,%zmm0
	vpsllq	$2,%zmm15,%zmm15
	vpaddq	%zmm15,%zmm0,%zmm0

	vpaddq	%zmm9,%zmm2,%zmm2
	vpsrlq	$26,%zmm7,%zmm8

	vpsrlq	$26,%zmm2,%zmm13
	vpandq	%zmm5,%zmm2,%zmm2
	vpaddq	%zmm13,%zmm14,%zmm3

	vpsrlq	$14,%zmm6,%zmm10

	vpsrlq	$26,%zmm0,%zmm11
	vpandq	%zmm5,%zmm0,%zmm0
	vpaddq	%zmm11,%zmm1,%zmm1

	vpsrlq	$40,%zmm6,%zmm6

	vpsrlq	$26,%zmm3,%zmm14
	vpandq	%zmm5,%zmm3,%zmm3
	vpaddq	%zmm14,%zmm4,%zmm4

	vpandq	%zmm5,%zmm7,%zmm7




	subq	$128,%rdx
	ja	L$oop_avx512

L$tail_avx512:





	vpsrlq	$32,%zmm16,%zmm16
	vpsrlq	$32,%zmm17,%zmm17
	vpsrlq	$32,%zmm18,%zmm18
	vpsrlq	$32,%zmm23,%zmm23
	vpsrlq	$32,%zmm24,%zmm24
	vpsrlq	$32,%zmm19,%zmm19
	vpsrlq	$32,%zmm20,%zmm20
	vpsrlq	$32,%zmm21,%zmm21
	vpsrlq	$32,%zmm22,%zmm22



	leaq	(%rsi,%rdx,1),%rsi


	vpaddq	%zmm0,%zmm7,%zmm0

	vpmuludq	%zmm2,%zmm17,%zmm14
	vpmuludq	%zmm2,%zmm18,%zmm15
	vpmuludq	%zmm2,%zmm23,%zmm11
	vpandq	%zmm5,%zmm8,%zmm8
	vpmuludq	%zmm2,%zmm24,%zmm12
	vpandq	%zmm5,%zmm10,%zmm10
	vpmuludq	%zmm2,%zmm16,%zmm13
	vporq	%zmm30,%zmm6,%zmm6
	vpaddq	%zmm1,%zmm8,%zmm1
	vpaddq	%zmm3,%zmm10,%zmm3
	vpaddq	%zmm4,%zmm6,%zmm4

	vmovdqu	0(%rsi),%xmm7
	vpmuludq	%zmm0,%zmm19,%zmm28
	vpmuludq	%zmm0,%zmm20,%zmm29
	vpmuludq	%zmm0,%zmm16,%zmm25
	vpmuludq	%zmm0,%zmm17,%zmm26
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm26,%zmm12,%zmm12

	vmovdqu	16(%rsi),%xmm8
	vpmuludq	%zmm1,%zmm18,%zmm28
	vpmuludq	%zmm1,%zmm19,%zmm29
	vpmuludq	%zmm1,%zmm24,%zmm25
	vpmuludq	%zmm0,%zmm18,%zmm27
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm27,%zmm13,%zmm13

	vinserti128	$1,32(%rsi),%ymm7,%ymm7
	vpmuludq	%zmm3,%zmm16,%zmm28
	vpmuludq	%zmm3,%zmm17,%zmm29
	vpmuludq	%zmm1,%zmm16,%zmm26
	vpmuludq	%zmm1,%zmm17,%zmm27
	vpaddq	%zmm28,%zmm14,%zmm14
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm27,%zmm13,%zmm13

	vinserti128	$1,48(%rsi),%ymm8,%ymm8
	vpmuludq	%zmm4,%zmm24,%zmm28
	vpmuludq	%zmm4,%zmm16,%zmm29
	vpmuludq	%zmm3,%zmm22,%zmm25
	vpmuludq	%zmm3,%zmm23,%zmm26
	vpmuludq	%zmm3,%zmm24,%zmm27
	vpaddq	%zmm28,%zmm14,%zmm3
	vpaddq	%zmm29,%zmm15,%zmm15
	vpaddq	%zmm25,%zmm11,%zmm11
	vpaddq	%zmm26,%zmm12,%zmm12
	vpaddq	%zmm27,%zmm13,%zmm13

	vpmuludq	%zmm4,%zmm21,%zmm25
	vpmuludq	%zmm4,%zmm22,%zmm26
	vpmuludq	%zmm4,%zmm23,%zmm27
	vpaddq	%zmm25,%zmm11,%zmm0
	vpaddq	%zmm26,%zmm12,%zmm1
	vpaddq	%zmm27,%zmm13,%zmm2




	movl	$1,%eax
	vpermq	$0xb1,%zmm3,%zmm14
	vpermq	$0xb1,%zmm15,%zmm4
	vpermq	$0xb1,%zmm0,%zmm11
	vpermq	$0xb1,%zmm1,%zmm12
	vpermq	$0xb1,%zmm2,%zmm13
	vpaddq	%zmm14,%zmm3,%zmm3
	vpaddq	%zmm15,%zmm4,%zmm4
	vpaddq	%zmm11,%zmm0,%zmm0
	vpaddq	%zmm12,%zmm1,%zmm1
	vpaddq	%zmm13,%zmm2,%zmm2

	kmovw	%eax,%k3
	vpermq	$0x2,%zmm3,%zmm14
	vpermq	$0x2,%zmm4,%zmm15
	vpermq	$0x2,%zmm0,%zmm11
	vpermq	$0x2,%zmm1,%zmm12
	vpermq	$0x2,%zmm2,%zmm13
	vpaddq	%zmm14,%zmm3,%zmm3
	vpaddq	%zmm15,%zmm4,%zmm4
	vpaddq	%zmm11,%zmm0,%zmm0
	vpaddq	%zmm12,%zmm1,%zmm1
	vpaddq	%zmm13,%zmm2,%zmm2

	vextracti64x4	$0x1,%zmm3,%ymm14
	vextracti64x4	$0x1,%zmm4,%ymm15
	vextracti64x4	$0x1,%zmm0,%ymm11
	vextracti64x4	$0x1,%zmm1,%ymm12
	vextracti64x4	$0x1,%zmm2,%ymm13
	vpaddq	%zmm14,%zmm3,%zmm3{%k3}{z}
	vpaddq	%zmm15,%zmm4,%zmm4{%k3}{z}
	vpaddq	%zmm11,%zmm0,%zmm0{%k3}{z}
	vpaddq	%zmm12,%zmm1,%zmm1{%k3}{z}
	vpaddq	%zmm13,%zmm2,%zmm2{%k3}{z}



	vpsrlq	$26,%ymm3,%ymm14
	vpand	%ymm5,%ymm3,%ymm3
	vpsrldq	$6,%ymm7,%ymm9
	vpsrldq	$6,%ymm8,%ymm10
	vpunpckhqdq	%ymm8,%ymm7,%ymm6
	vpaddq	%ymm14,%ymm4,%ymm4

	vpsrlq	$26,%ymm0,%ymm11
	vpand	%ymm5,%ymm0,%ymm0
	vpunpcklqdq	%ymm10,%ymm9,%ymm9
	vpunpcklqdq	%ymm8,%ymm7,%ymm7
	vpaddq	%ymm11,%ymm1,%ymm1

	vpsrlq	$26,%ymm4,%ymm15
	vpand	%ymm5,%ymm4,%ymm4

	vpsrlq	$26,%ymm1,%ymm12
	vpand	%ymm5,%ymm1,%ymm1
	vpsrlq	$30,%ymm9,%ymm10
	vpsrlq	$4,%ymm9,%ymm9
	vpaddq	%ymm12,%ymm2,%ymm2

	vpaddq	%ymm15,%ymm0,%ymm0
	vpsllq	$2,%ymm15,%ymm15
	vpsrlq	$26,%ymm7,%ymm8
	vpsrlq	$40,%ymm6,%ymm6
	vpaddq	%ymm15,%ymm0,%ymm0

	vpsrlq	$26,%ymm2,%ymm13
	vpand	%ymm5,%ymm2,%ymm2
	vpand	%ymm5,%ymm9,%ymm9
	vpand	%ymm5,%ymm7,%ymm7
	vpaddq	%ymm13,%ymm3,%ymm3

	vpsrlq	$26,%ymm0,%ymm11
	vpand	%ymm5,%ymm0,%ymm0
	vpaddq	%ymm2,%ymm9,%ymm2
	vpand	%ymm5,%ymm8,%ymm8
	vpaddq	%ymm11,%ymm1,%ymm1

	vpsrlq	$26,%ymm3,%ymm14
	vpand	%ymm5,%ymm3,%ymm3
	vpand	%ymm5,%ymm10,%ymm10
	vpor	32(%rcx),%ymm6,%ymm6
	vpaddq	%ymm14,%ymm4,%ymm4

	leaq	144(%rsp),%rax
	addq	$64,%rdx
	jnz	L$tail_avx2

	vpsubq	%ymm9,%ymm2,%ymm2
	vmovd	%xmm0,-112(%rdi)
	vmovd	%xmm1,-108(%rdi)
	vmovd	%xmm2,-104(%rdi)
	vmovd	%xmm3,-100(%rdi)
	vmovd	%xmm4,-96(%rdi)
	vzeroall
	leaq	8(%r11),%rsp

	.byte	0xf3,0xc3



.p2align	5
poly1305_init_base2_44:
	xorq	%rax,%rax
	movq	%rax,0(%rdi)
	movq	%rax,8(%rdi)
	movq	%rax,16(%rdi)

L$init_base2_44:
	leaq	poly1305_blocks_vpmadd52(%rip),%r10
	leaq	poly1305_emit_base2_44(%rip),%r11

	movq	$0x0ffffffc0fffffff,%rax
	movq	$0x0ffffffc0ffffffc,%rcx
	andq	0(%rsi),%rax
	movq	$0x00000fffffffffff,%r8
	andq	8(%rsi),%rcx
	movq	$0x00000fffffffffff,%r9
	andq	%rax,%r8
	shrdq	$44,%rcx,%rax
	movq	%r8,40(%rdi)
	andq	%r9,%rax
	shrq	$24,%rcx
	movq	%rax,48(%rdi)
	leaq	(%rax,%rax,4),%rax
	movq	%rcx,56(%rdi)
	shlq	$2,%rax
	leaq	(%rcx,%rcx,4),%rcx
	shlq	$2,%rcx
	movq	%rax,24(%rdi)
	movq	%rcx,32(%rdi)
	movq	$-1,64(%rdi)
	movq	%r10,0(%rdx)
	movq	%r11,8(%rdx)
	movl	$1,%eax
	.byte	0xf3,0xc3


.p2align	5
poly1305_blocks_vpmadd52:
	shrq	$4,%rdx
	jz	L$no_data_vpmadd52

	shlq	$40,%rcx
	movq	64(%rdi),%r8






	movq	$3,%rax
	movq	$1,%r10
	cmpq	$4,%rdx
	cmovaeq	%r10,%rax
	testq	%r8,%r8
	cmovnsq	%r10,%rax

	andq	%rdx,%rax
	jz	L$blocks_vpmadd52_4x

	subq	%rax,%rdx
	movl	$7,%r10d
	movl	$1,%r11d
	kmovw	%r10d,%k7
	leaq	L$2_44_inp_permd(%rip),%r10
	kmovw	%r11d,%k1

	vmovq	%rcx,%xmm21
	vmovdqa64	0(%r10),%ymm19
	vmovdqa64	32(%r10),%ymm20
	vpermq	$0xcf,%ymm21,%ymm21
	vmovdqa64	64(%r10),%ymm22

	vmovdqu64	0(%rdi),%ymm16{%k7}{z}
	vmovdqu64	40(%rdi),%ymm3{%k7}{z}
	vmovdqu64	32(%rdi),%ymm4{%k7}{z}
	vmovdqu64	24(%rdi),%ymm5{%k7}{z}

	vmovdqa64	96(%r10),%ymm23
	vmovdqa64	128(%r10),%ymm24

	jmp	L$oop_vpmadd52

.p2align	5
L$oop_vpmadd52:
	vmovdqu32	0(%rsi),%xmm18
	leaq	16(%rsi),%rsi

	vpermd	%ymm18,%ymm19,%ymm18
	vpsrlvq	%ymm20,%ymm18,%ymm18
	vpandq	%ymm22,%ymm18,%ymm18
	vporq	%ymm21,%ymm18,%ymm18

	vpaddq	%ymm18,%ymm16,%ymm16

	vpermq	$0,%ymm16,%ymm0{%k7}{z}
	vpermq	$85,%ymm16,%ymm1{%k7}{z}
	vpermq	$170,%ymm16,%ymm2{%k7}{z}

	vpxord	%ymm16,%ymm16,%ymm16
	vpxord	%ymm17,%ymm17,%ymm17

	vpmadd52luq	%ymm3,%ymm0,%ymm16
	vpmadd52huq	%ymm3,%ymm0,%ymm17

	vpmadd52luq	%ymm4,%ymm1,%ymm16
	vpmadd52huq	%ymm4,%ymm1,%ymm17

	vpmadd52luq	%ymm5,%ymm2,%ymm16
	vpmadd52huq	%ymm5,%ymm2,%ymm17

	vpsrlvq	%ymm23,%ymm16,%ymm18
	vpsllvq	%ymm24,%ymm17,%ymm17
	vpandq	%ymm22,%ymm16,%ymm16

	vpaddq	%ymm18,%ymm17,%ymm17

	vpermq	$147,%ymm17,%ymm17

	vpaddq	%ymm17,%ymm16,%ymm16

	vpsrlvq	%ymm23,%ymm16,%ymm18
	vpandq	%ymm22,%ymm16,%ymm16

	vpermq	$147,%ymm18,%ymm18

	vpaddq	%ymm18,%ymm16,%ymm16

	vpermq	$147,%ymm16,%ymm18{%k1}{z}

	vpaddq	%ymm18,%ymm16,%ymm16
	vpsllq	$2,%ymm18,%ymm18

	vpaddq	%ymm18,%ymm16,%ymm16

	decq	%rax
	jnz	L$oop_vpmadd52

	vmovdqu64	%ymm16,0(%rdi){%k7}

	testq	%rdx,%rdx
	jnz	L$blocks_vpmadd52_4x

L$no_data_vpmadd52:
	.byte	0xf3,0xc3


.p2align	5
poly1305_blocks_vpmadd52_4x:
	shrq	$4,%rdx
	jz	L$no_data_vpmadd52_4x

	shlq	$40,%rcx
	movq	64(%rdi),%r8

L$blocks_vpmadd52_4x:
	vpbroadcastq	%rcx,%ymm31

	vmovdqa64	L$x_mask44(%rip),%ymm28
	movl	$5,%eax
	vmovdqa64	L$x_mask42(%rip),%ymm29
	kmovw	%eax,%k1

	testq	%r8,%r8
	js	L$init_vpmadd52

	vmovq	0(%rdi),%xmm0
	vmovq	8(%rdi),%xmm1
	vmovq	16(%rdi),%xmm2

	testq	$3,%rdx
	jnz	L$blocks_vpmadd52_2x_do

L$blocks_vpmadd52_4x_do:
	vpbroadcastq	64(%rdi),%ymm3
	vpbroadcastq	96(%rdi),%ymm4
	vpbroadcastq	128(%rdi),%ymm5
	vpbroadcastq	160(%rdi),%ymm16

L$blocks_vpmadd52_4x_key_loaded:
	vpsllq	$2,%ymm5,%ymm17
	vpaddq	%ymm5,%ymm17,%ymm17
	vpsllq	$2,%ymm17,%ymm17

	testq	$7,%rdx
	jz	L$blocks_vpmadd52_8x

	vmovdqu64	0(%rsi),%ymm26
	vmovdqu64	32(%rsi),%ymm27
	leaq	64(%rsi),%rsi

	vpunpcklqdq	%ymm27,%ymm26,%ymm25
	vpunpckhqdq	%ymm27,%ymm26,%ymm27



	vpsrlq	$24,%ymm27,%ymm26
	vporq	%ymm31,%ymm26,%ymm26
	vpaddq	%ymm26,%ymm2,%ymm2
	vpandq	%ymm28,%ymm25,%ymm24
	vpsrlq	$44,%ymm25,%ymm25
	vpsllq	$20,%ymm27,%ymm27
	vporq	%ymm27,%ymm25,%ymm25
	vpandq	%ymm28,%ymm25,%ymm25

	subq	$4,%rdx
	jz	L$tail_vpmadd52_4x
	jmp	L$oop_vpmadd52_4x
	ud2

.p2align	5
L$init_vpmadd52:
	vmovq	24(%rdi),%xmm16
	vmovq	56(%rdi),%xmm2
	vmovq	32(%rdi),%xmm17
	vmovq	40(%rdi),%xmm3
	vmovq	48(%rdi),%xmm4

	vmovdqa	%ymm3,%ymm0
	vmovdqa	%ymm4,%ymm1
	vmovdqa	%ymm2,%ymm5

	movl	$2,%eax

L$mul_init_vpmadd52:
	vpxorq	%ymm18,%ymm18,%ymm18
	vpmadd52luq	%ymm2,%ymm16,%ymm18
	vpxorq	%ymm19,%ymm19,%ymm19
	vpmadd52huq	%ymm2,%ymm16,%ymm19
	vpxorq	%ymm20,%ymm20,%ymm20
	vpmadd52luq	%ymm2,%ymm17,%ymm20
	vpxorq	%ymm21,%ymm21,%ymm21
	vpmadd52huq	%ymm2,%ymm17,%ymm21
	vpxorq	%ymm22,%ymm22,%ymm22
	vpmadd52luq	%ymm2,%ymm3,%ymm22
	vpxorq	%ymm23,%ymm23,%ymm23
	vpmadd52huq	%ymm2,%ymm3,%ymm23

	vpmadd52luq	%ymm0,%ymm3,%ymm18
	vpmadd52huq	%ymm0,%ymm3,%ymm19
	vpmadd52luq	%ymm0,%ymm4,%ymm20
	vpmadd52huq	%ymm0,%ymm4,%ymm21
	vpmadd52luq	%ymm0,%ymm5,%ymm22
	vpmadd52huq	%ymm0,%ymm5,%ymm23

	vpmadd52luq	%ymm1,%ymm17,%ymm18
	vpmadd52huq	%ymm1,%ymm17,%ymm19
	vpmadd52luq	%ymm1,%ymm3,%ymm20
	vpmadd52huq	%ymm1,%ymm3,%ymm21
	vpmadd52luq	%ymm1,%ymm4,%ymm22
	vpmadd52huq	%ymm1,%ymm4,%ymm23



	vpsrlq	$44,%ymm18,%ymm30
	vpsllq	$8,%ymm19,%ymm19
	vpandq	%ymm28,%ymm18,%ymm0
	vpaddq	%ymm30,%ymm19,%ymm19

	vpaddq	%ymm19,%ymm20,%ymm20

	vpsrlq	$44,%ymm20,%ymm30
	vpsllq	$8,%ymm21,%ymm21
	vpandq	%ymm28,%ymm20,%ymm1
	vpaddq	%ymm30,%ymm21,%ymm21

	vpaddq	%ymm21,%ymm22,%ymm22

	vpsrlq	$42,%ymm22,%ymm30
	vpsllq	$10,%ymm23,%ymm23
	vpandq	%ymm29,%ymm22,%ymm2
	vpaddq	%ymm30,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm0,%ymm0
	vpsllq	$2,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm0,%ymm0

	vpsrlq	$44,%ymm0,%ymm30
	vpandq	%ymm28,%ymm0,%ymm0

	vpaddq	%ymm30,%ymm1,%ymm1

	decl	%eax
	jz	L$done_init_vpmadd52

	vpunpcklqdq	%ymm4,%ymm1,%ymm4
	vpbroadcastq	%xmm1,%xmm1
	vpunpcklqdq	%ymm5,%ymm2,%ymm5
	vpbroadcastq	%xmm2,%xmm2
	vpunpcklqdq	%ymm3,%ymm0,%ymm3
	vpbroadcastq	%xmm0,%xmm0

	vpsllq	$2,%ymm4,%ymm16
	vpsllq	$2,%ymm5,%ymm17
	vpaddq	%ymm4,%ymm16,%ymm16
	vpaddq	%ymm5,%ymm17,%ymm17
	vpsllq	$2,%ymm16,%ymm16
	vpsllq	$2,%ymm17,%ymm17

	jmp	L$mul_init_vpmadd52
	ud2

.p2align	5
L$done_init_vpmadd52:
	vinserti128	$1,%xmm4,%ymm1,%ymm4
	vinserti128	$1,%xmm5,%ymm2,%ymm5
	vinserti128	$1,%xmm3,%ymm0,%ymm3

	vpermq	$216,%ymm4,%ymm4
	vpermq	$216,%ymm5,%ymm5
	vpermq	$216,%ymm3,%ymm3

	vpsllq	$2,%ymm4,%ymm16
	vpaddq	%ymm4,%ymm16,%ymm16
	vpsllq	$2,%ymm16,%ymm16

	vmovq	0(%rdi),%xmm0
	vmovq	8(%rdi),%xmm1
	vmovq	16(%rdi),%xmm2

	testq	$3,%rdx
	jnz	L$done_init_vpmadd52_2x

	vmovdqu64	%ymm3,64(%rdi)
	vpbroadcastq	%xmm3,%ymm3
	vmovdqu64	%ymm4,96(%rdi)
	vpbroadcastq	%xmm4,%ymm4
	vmovdqu64	%ymm5,128(%rdi)
	vpbroadcastq	%xmm5,%ymm5
	vmovdqu64	%ymm16,160(%rdi)
	vpbroadcastq	%xmm16,%ymm16

	jmp	L$blocks_vpmadd52_4x_key_loaded
	ud2

.p2align	5
L$done_init_vpmadd52_2x:
	vmovdqu64	%ymm3,64(%rdi)
	vpsrldq	$8,%ymm3,%ymm3
	vmovdqu64	%ymm4,96(%rdi)
	vpsrldq	$8,%ymm4,%ymm4
	vmovdqu64	%ymm5,128(%rdi)
	vpsrldq	$8,%ymm5,%ymm5
	vmovdqu64	%ymm16,160(%rdi)
	vpsrldq	$8,%ymm16,%ymm16
	jmp	L$blocks_vpmadd52_2x_key_loaded
	ud2

.p2align	5
L$blocks_vpmadd52_2x_do:
	vmovdqu64	128+8(%rdi),%ymm5{%k1}{z}
	vmovdqu64	160+8(%rdi),%ymm16{%k1}{z}
	vmovdqu64	64+8(%rdi),%ymm3{%k1}{z}
	vmovdqu64	96+8(%rdi),%ymm4{%k1}{z}

L$blocks_vpmadd52_2x_key_loaded:
	vmovdqu64	0(%rsi),%ymm26
	vpxorq	%ymm27,%ymm27,%ymm27
	leaq	32(%rsi),%rsi

	vpunpcklqdq	%ymm27,%ymm26,%ymm25
	vpunpckhqdq	%ymm27,%ymm26,%ymm27



	vpsrlq	$24,%ymm27,%ymm26
	vporq	%ymm31,%ymm26,%ymm26
	vpaddq	%ymm26,%ymm2,%ymm2
	vpandq	%ymm28,%ymm25,%ymm24
	vpsrlq	$44,%ymm25,%ymm25
	vpsllq	$20,%ymm27,%ymm27
	vporq	%ymm27,%ymm25,%ymm25
	vpandq	%ymm28,%ymm25,%ymm25

	jmp	L$tail_vpmadd52_2x
	ud2

.p2align	5
L$oop_vpmadd52_4x:

	vpaddq	%ymm24,%ymm0,%ymm0
	vpaddq	%ymm25,%ymm1,%ymm1

	vpxorq	%ymm18,%ymm18,%ymm18
	vpmadd52luq	%ymm2,%ymm16,%ymm18
	vpxorq	%ymm19,%ymm19,%ymm19
	vpmadd52huq	%ymm2,%ymm16,%ymm19
	vpxorq	%ymm20,%ymm20,%ymm20
	vpmadd52luq	%ymm2,%ymm17,%ymm20
	vpxorq	%ymm21,%ymm21,%ymm21
	vpmadd52huq	%ymm2,%ymm17,%ymm21
	vpxorq	%ymm22,%ymm22,%ymm22
	vpmadd52luq	%ymm2,%ymm3,%ymm22
	vpxorq	%ymm23,%ymm23,%ymm23
	vpmadd52huq	%ymm2,%ymm3,%ymm23

	vmovdqu64	0(%rsi),%ymm26
	vmovdqu64	32(%rsi),%ymm27
	leaq	64(%rsi),%rsi
	vpmadd52luq	%ymm0,%ymm3,%ymm18
	vpmadd52huq	%ymm0,%ymm3,%ymm19
	vpmadd52luq	%ymm0,%ymm4,%ymm20
	vpmadd52huq	%ymm0,%ymm4,%ymm21
	vpmadd52luq	%ymm0,%ymm5,%ymm22
	vpmadd52huq	%ymm0,%ymm5,%ymm23

	vpunpcklqdq	%ymm27,%ymm26,%ymm25
	vpunpckhqdq	%ymm27,%ymm26,%ymm27
	vpmadd52luq	%ymm1,%ymm17,%ymm18
	vpmadd52huq	%ymm1,%ymm17,%ymm19
	vpmadd52luq	%ymm1,%ymm3,%ymm20
	vpmadd52huq	%ymm1,%ymm3,%ymm21
	vpmadd52luq	%ymm1,%ymm4,%ymm22
	vpmadd52huq	%ymm1,%ymm4,%ymm23



	vpsrlq	$44,%ymm18,%ymm30
	vpsllq	$8,%ymm19,%ymm19
	vpandq	%ymm28,%ymm18,%ymm0
	vpaddq	%ymm30,%ymm19,%ymm19

	vpsrlq	$24,%ymm27,%ymm26
	vporq	%ymm31,%ymm26,%ymm26
	vpaddq	%ymm19,%ymm20,%ymm20

	vpsrlq	$44,%ymm20,%ymm30
	vpsllq	$8,%ymm21,%ymm21
	vpandq	%ymm28,%ymm20,%ymm1
	vpaddq	%ymm30,%ymm21,%ymm21

	vpandq	%ymm28,%ymm25,%ymm24
	vpsrlq	$44,%ymm25,%ymm25
	vpsllq	$20,%ymm27,%ymm27
	vpaddq	%ymm21,%ymm22,%ymm22

	vpsrlq	$42,%ymm22,%ymm30
	vpsllq	$10,%ymm23,%ymm23
	vpandq	%ymm29,%ymm22,%ymm2
	vpaddq	%ymm30,%ymm23,%ymm23

	vpaddq	%ymm26,%ymm2,%ymm2
	vpaddq	%ymm23,%ymm0,%ymm0
	vpsllq	$2,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm0,%ymm0
	vporq	%ymm27,%ymm25,%ymm25
	vpandq	%ymm28,%ymm25,%ymm25

	vpsrlq	$44,%ymm0,%ymm30
	vpandq	%ymm28,%ymm0,%ymm0

	vpaddq	%ymm30,%ymm1,%ymm1

	subq	$4,%rdx
	jnz	L$oop_vpmadd52_4x

L$tail_vpmadd52_4x:
	vmovdqu64	128(%rdi),%ymm5
	vmovdqu64	160(%rdi),%ymm16
	vmovdqu64	64(%rdi),%ymm3
	vmovdqu64	96(%rdi),%ymm4

L$tail_vpmadd52_2x:
	vpsllq	$2,%ymm5,%ymm17
	vpaddq	%ymm5,%ymm17,%ymm17
	vpsllq	$2,%ymm17,%ymm17


	vpaddq	%ymm24,%ymm0,%ymm0
	vpaddq	%ymm25,%ymm1,%ymm1

	vpxorq	%ymm18,%ymm18,%ymm18
	vpmadd52luq	%ymm2,%ymm16,%ymm18
	vpxorq	%ymm19,%ymm19,%ymm19
	vpmadd52huq	%ymm2,%ymm16,%ymm19
	vpxorq	%ymm20,%ymm20,%ymm20
	vpmadd52luq	%ymm2,%ymm17,%ymm20
	vpxorq	%ymm21,%ymm21,%ymm21
	vpmadd52huq	%ymm2,%ymm17,%ymm21
	vpxorq	%ymm22,%ymm22,%ymm22
	vpmadd52luq	%ymm2,%ymm3,%ymm22
	vpxorq	%ymm23,%ymm23,%ymm23
	vpmadd52huq	%ymm2,%ymm3,%ymm23

	vpmadd52luq	%ymm0,%ymm3,%ymm18
	vpmadd52huq	%ymm0,%ymm3,%ymm19
	vpmadd52luq	%ymm0,%ymm4,%ymm20
	vpmadd52huq	%ymm0,%ymm4,%ymm21
	vpmadd52luq	%ymm0,%ymm5,%ymm22
	vpmadd52huq	%ymm0,%ymm5,%ymm23

	vpmadd52luq	%ymm1,%ymm17,%ymm18
	vpmadd52huq	%ymm1,%ymm17,%ymm19
	vpmadd52luq	%ymm1,%ymm3,%ymm20
	vpmadd52huq	%ymm1,%ymm3,%ymm21
	vpmadd52luq	%ymm1,%ymm4,%ymm22
	vpmadd52huq	%ymm1,%ymm4,%ymm23




	movl	$1,%eax
	kmovw	%eax,%k1
	vpsrldq	$8,%ymm18,%ymm24
	vpsrldq	$8,%ymm19,%ymm0
	vpsrldq	$8,%ymm20,%ymm25
	vpsrldq	$8,%ymm21,%ymm1
	vpaddq	%ymm24,%ymm18,%ymm18
	vpaddq	%ymm0,%ymm19,%ymm19
	vpsrldq	$8,%ymm22,%ymm26
	vpsrldq	$8,%ymm23,%ymm2
	vpaddq	%ymm25,%ymm20,%ymm20
	vpaddq	%ymm1,%ymm21,%ymm21
	vpermq	$0x2,%ymm18,%ymm24
	vpermq	$0x2,%ymm19,%ymm0
	vpaddq	%ymm26,%ymm22,%ymm22
	vpaddq	%ymm2,%ymm23,%ymm23

	vpermq	$0x2,%ymm20,%ymm25
	vpermq	$0x2,%ymm21,%ymm1
	vpaddq	%ymm24,%ymm18,%ymm18{%k1}{z}
	vpaddq	%ymm0,%ymm19,%ymm19{%k1}{z}
	vpermq	$0x2,%ymm22,%ymm26
	vpermq	$0x2,%ymm23,%ymm2
	vpaddq	%ymm25,%ymm20,%ymm20{%k1}{z}
	vpaddq	%ymm1,%ymm21,%ymm21{%k1}{z}
	vpaddq	%ymm26,%ymm22,%ymm22{%k1}{z}
	vpaddq	%ymm2,%ymm23,%ymm23{%k1}{z}



	vpsrlq	$44,%ymm18,%ymm30
	vpsllq	$8,%ymm19,%ymm19
	vpandq	%ymm28,%ymm18,%ymm0
	vpaddq	%ymm30,%ymm19,%ymm19

	vpaddq	%ymm19,%ymm20,%ymm20

	vpsrlq	$44,%ymm20,%ymm30
	vpsllq	$8,%ymm21,%ymm21
	vpandq	%ymm28,%ymm20,%ymm1
	vpaddq	%ymm30,%ymm21,%ymm21

	vpaddq	%ymm21,%ymm22,%ymm22

	vpsrlq	$42,%ymm22,%ymm30
	vpsllq	$10,%ymm23,%ymm23
	vpandq	%ymm29,%ymm22,%ymm2
	vpaddq	%ymm30,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm0,%ymm0
	vpsllq	$2,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm0,%ymm0

	vpsrlq	$44,%ymm0,%ymm30
	vpandq	%ymm28,%ymm0,%ymm0

	vpaddq	%ymm30,%ymm1,%ymm1


	subq	$2,%rdx
	ja	L$blocks_vpmadd52_4x_do

	vmovq	%xmm0,0(%rdi)
	vmovq	%xmm1,8(%rdi)
	vmovq	%xmm2,16(%rdi)
	vzeroall

L$no_data_vpmadd52_4x:
	.byte	0xf3,0xc3


.p2align	5
poly1305_blocks_vpmadd52_8x:
	shrq	$4,%rdx
	jz	L$no_data_vpmadd52_8x

	shlq	$40,%rcx
	movq	64(%rdi),%r8

	vmovdqa64	L$x_mask44(%rip),%ymm28
	vmovdqa64	L$x_mask42(%rip),%ymm29

	testq	%r8,%r8
	js	L$init_vpmadd52

	vmovq	0(%rdi),%xmm0
	vmovq	8(%rdi),%xmm1
	vmovq	16(%rdi),%xmm2

L$blocks_vpmadd52_8x:



	vmovdqu64	128(%rdi),%ymm5
	vmovdqu64	160(%rdi),%ymm16
	vmovdqu64	64(%rdi),%ymm3
	vmovdqu64	96(%rdi),%ymm4

	vpsllq	$2,%ymm5,%ymm17
	vpaddq	%ymm5,%ymm17,%ymm17
	vpsllq	$2,%ymm17,%ymm17

	vpbroadcastq	%xmm5,%ymm8
	vpbroadcastq	%xmm3,%ymm6
	vpbroadcastq	%xmm4,%ymm7

	vpxorq	%ymm18,%ymm18,%ymm18
	vpmadd52luq	%ymm8,%ymm16,%ymm18
	vpxorq	%ymm19,%ymm19,%ymm19
	vpmadd52huq	%ymm8,%ymm16,%ymm19
	vpxorq	%ymm20,%ymm20,%ymm20
	vpmadd52luq	%ymm8,%ymm17,%ymm20
	vpxorq	%ymm21,%ymm21,%ymm21
	vpmadd52huq	%ymm8,%ymm17,%ymm21
	vpxorq	%ymm22,%ymm22,%ymm22
	vpmadd52luq	%ymm8,%ymm3,%ymm22
	vpxorq	%ymm23,%ymm23,%ymm23
	vpmadd52huq	%ymm8,%ymm3,%ymm23

	vpmadd52luq	%ymm6,%ymm3,%ymm18
	vpmadd52huq	%ymm6,%ymm3,%ymm19
	vpmadd52luq	%ymm6,%ymm4,%ymm20
	vpmadd52huq	%ymm6,%ymm4,%ymm21
	vpmadd52luq	%ymm6,%ymm5,%ymm22
	vpmadd52huq	%ymm6,%ymm5,%ymm23

	vpmadd52luq	%ymm7,%ymm17,%ymm18
	vpmadd52huq	%ymm7,%ymm17,%ymm19
	vpmadd52luq	%ymm7,%ymm3,%ymm20
	vpmadd52huq	%ymm7,%ymm3,%ymm21
	vpmadd52luq	%ymm7,%ymm4,%ymm22
	vpmadd52huq	%ymm7,%ymm4,%ymm23



	vpsrlq	$44,%ymm18,%ymm30
	vpsllq	$8,%ymm19,%ymm19
	vpandq	%ymm28,%ymm18,%ymm6
	vpaddq	%ymm30,%ymm19,%ymm19

	vpaddq	%ymm19,%ymm20,%ymm20

	vpsrlq	$44,%ymm20,%ymm30
	vpsllq	$8,%ymm21,%ymm21
	vpandq	%ymm28,%ymm20,%ymm7
	vpaddq	%ymm30,%ymm21,%ymm21

	vpaddq	%ymm21,%ymm22,%ymm22

	vpsrlq	$42,%ymm22,%ymm30
	vpsllq	$10,%ymm23,%ymm23
	vpandq	%ymm29,%ymm22,%ymm8
	vpaddq	%ymm30,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm6,%ymm6
	vpsllq	$2,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm6,%ymm6

	vpsrlq	$44,%ymm6,%ymm30
	vpandq	%ymm28,%ymm6,%ymm6

	vpaddq	%ymm30,%ymm7,%ymm7





	vpunpcklqdq	%ymm5,%ymm8,%ymm26
	vpunpckhqdq	%ymm5,%ymm8,%ymm5
	vpunpcklqdq	%ymm3,%ymm6,%ymm24
	vpunpckhqdq	%ymm3,%ymm6,%ymm3
	vpunpcklqdq	%ymm4,%ymm7,%ymm25
	vpunpckhqdq	%ymm4,%ymm7,%ymm4
	vshufi64x2	$0x44,%zmm5,%zmm26,%zmm8
	vshufi64x2	$0x44,%zmm3,%zmm24,%zmm6
	vshufi64x2	$0x44,%zmm4,%zmm25,%zmm7

	vmovdqu64	0(%rsi),%zmm26
	vmovdqu64	64(%rsi),%zmm27
	leaq	128(%rsi),%rsi

	vpsllq	$2,%zmm8,%zmm10
	vpsllq	$2,%zmm7,%zmm9
	vpaddq	%zmm8,%zmm10,%zmm10
	vpaddq	%zmm7,%zmm9,%zmm9
	vpsllq	$2,%zmm10,%zmm10
	vpsllq	$2,%zmm9,%zmm9

	vpbroadcastq	%rcx,%zmm31
	vpbroadcastq	%xmm28,%zmm28
	vpbroadcastq	%xmm29,%zmm29

	vpbroadcastq	%xmm9,%zmm16
	vpbroadcastq	%xmm10,%zmm17
	vpbroadcastq	%xmm6,%zmm3
	vpbroadcastq	%xmm7,%zmm4
	vpbroadcastq	%xmm8,%zmm5

	vpunpcklqdq	%zmm27,%zmm26,%zmm25
	vpunpckhqdq	%zmm27,%zmm26,%zmm27



	vpsrlq	$24,%zmm27,%zmm26
	vporq	%zmm31,%zmm26,%zmm26
	vpaddq	%zmm26,%zmm2,%zmm2
	vpandq	%zmm28,%zmm25,%zmm24
	vpsrlq	$44,%zmm25,%zmm25
	vpsllq	$20,%zmm27,%zmm27
	vporq	%zmm27,%zmm25,%zmm25
	vpandq	%zmm28,%zmm25,%zmm25

	subq	$8,%rdx
	jz	L$tail_vpmadd52_8x
	jmp	L$oop_vpmadd52_8x

.p2align	5
L$oop_vpmadd52_8x:

	vpaddq	%zmm24,%zmm0,%zmm0
	vpaddq	%zmm25,%zmm1,%zmm1

	vpxorq	%zmm18,%zmm18,%zmm18
	vpmadd52luq	%zmm2,%zmm16,%zmm18
	vpxorq	%zmm19,%zmm19,%zmm19
	vpmadd52huq	%zmm2,%zmm16,%zmm19
	vpxorq	%zmm20,%zmm20,%zmm20
	vpmadd52luq	%zmm2,%zmm17,%zmm20
	vpxorq	%zmm21,%zmm21,%zmm21
	vpmadd52huq	%zmm2,%zmm17,%zmm21
	vpxorq	%zmm22,%zmm22,%zmm22
	vpmadd52luq	%zmm2,%zmm3,%zmm22
	vpxorq	%zmm23,%zmm23,%zmm23
	vpmadd52huq	%zmm2,%zmm3,%zmm23

	vmovdqu64	0(%rsi),%zmm26
	vmovdqu64	64(%rsi),%zmm27
	leaq	128(%rsi),%rsi
	vpmadd52luq	%zmm0,%zmm3,%zmm18
	vpmadd52huq	%zmm0,%zmm3,%zmm19
	vpmadd52luq	%zmm0,%zmm4,%zmm20
	vpmadd52huq	%zmm0,%zmm4,%zmm21
	vpmadd52luq	%zmm0,%zmm5,%zmm22
	vpmadd52huq	%zmm0,%zmm5,%zmm23

	vpunpcklqdq	%zmm27,%zmm26,%zmm25
	vpunpckhqdq	%zmm27,%zmm26,%zmm27
	vpmadd52luq	%zmm1,%zmm17,%zmm18
	vpmadd52huq	%zmm1,%zmm17,%zmm19
	vpmadd52luq	%zmm1,%zmm3,%zmm20
	vpmadd52huq	%zmm1,%zmm3,%zmm21
	vpmadd52luq	%zmm1,%zmm4,%zmm22
	vpmadd52huq	%zmm1,%zmm4,%zmm23



	vpsrlq	$44,%zmm18,%zmm30
	vpsllq	$8,%zmm19,%zmm19
	vpandq	%zmm28,%zmm18,%zmm0
	vpaddq	%zmm30,%zmm19,%zmm19

	vpsrlq	$24,%zmm27,%zmm26
	vporq	%zmm31,%zmm26,%zmm26
	vpaddq	%zmm19,%zmm20,%zmm20

	vpsrlq	$44,%zmm20,%zmm30
	vpsllq	$8,%zmm21,%zmm21
	vpandq	%zmm28,%zmm20,%zmm1
	vpaddq	%zmm30,%zmm21,%zmm21

	vpandq	%zmm28,%zmm25,%zmm24
	vpsrlq	$44,%zmm25,%zmm25
	vpsllq	$20,%zmm27,%zmm27
	vpaddq	%zmm21,%zmm22,%zmm22

	vpsrlq	$42,%zmm22,%zmm30
	vpsllq	$10,%zmm23,%zmm23
	vpandq	%zmm29,%zmm22,%zmm2
	vpaddq	%zmm30,%zmm23,%zmm23

	vpaddq	%zmm26,%zmm2,%zmm2
	vpaddq	%zmm23,%zmm0,%zmm0
	vpsllq	$2,%zmm23,%zmm23

	vpaddq	%zmm23,%zmm0,%zmm0
	vporq	%zmm27,%zmm25,%zmm25
	vpandq	%zmm28,%zmm25,%zmm25

	vpsrlq	$44,%zmm0,%zmm30
	vpandq	%zmm28,%zmm0,%zmm0

	vpaddq	%zmm30,%zmm1,%zmm1

	subq	$8,%rdx
	jnz	L$oop_vpmadd52_8x

L$tail_vpmadd52_8x:

	vpaddq	%zmm24,%zmm0,%zmm0
	vpaddq	%zmm25,%zmm1,%zmm1

	vpxorq	%zmm18,%zmm18,%zmm18
	vpmadd52luq	%zmm2,%zmm9,%zmm18
	vpxorq	%zmm19,%zmm19,%zmm19
	vpmadd52huq	%zmm2,%zmm9,%zmm19
	vpxorq	%zmm20,%zmm20,%zmm20
	vpmadd52luq	%zmm2,%zmm10,%zmm20
	vpxorq	%zmm21,%zmm21,%zmm21
	vpmadd52huq	%zmm2,%zmm10,%zmm21
	vpxorq	%zmm22,%zmm22,%zmm22
	vpmadd52luq	%zmm2,%zmm6,%zmm22
	vpxorq	%zmm23,%zmm23,%zmm23
	vpmadd52huq	%zmm2,%zmm6,%zmm23

	vpmadd52luq	%zmm0,%zmm6,%zmm18
	vpmadd52huq	%zmm0,%zmm6,%zmm19
	vpmadd52luq	%zmm0,%zmm7,%zmm20
	vpmadd52huq	%zmm0,%zmm7,%zmm21
	vpmadd52luq	%zmm0,%zmm8,%zmm22
	vpmadd52huq	%zmm0,%zmm8,%zmm23

	vpmadd52luq	%zmm1,%zmm10,%zmm18
	vpmadd52huq	%zmm1,%zmm10,%zmm19
	vpmadd52luq	%zmm1,%zmm6,%zmm20
	vpmadd52huq	%zmm1,%zmm6,%zmm21
	vpmadd52luq	%zmm1,%zmm7,%zmm22
	vpmadd52huq	%zmm1,%zmm7,%zmm23




	movl	$1,%eax
	kmovw	%eax,%k1
	vpsrldq	$8,%zmm18,%zmm24
	vpsrldq	$8,%zmm19,%zmm0
	vpsrldq	$8,%zmm20,%zmm25
	vpsrldq	$8,%zmm21,%zmm1
	vpaddq	%zmm24,%zmm18,%zmm18
	vpaddq	%zmm0,%zmm19,%zmm19
	vpsrldq	$8,%zmm22,%zmm26
	vpsrldq	$8,%zmm23,%zmm2
	vpaddq	%zmm25,%zmm20,%zmm20
	vpaddq	%zmm1,%zmm21,%zmm21
	vpermq	$0x2,%zmm18,%zmm24
	vpermq	$0x2,%zmm19,%zmm0
	vpaddq	%zmm26,%zmm22,%zmm22
	vpaddq	%zmm2,%zmm23,%zmm23

	vpermq	$0x2,%zmm20,%zmm25
	vpermq	$0x2,%zmm21,%zmm1
	vpaddq	%zmm24,%zmm18,%zmm18
	vpaddq	%zmm0,%zmm19,%zmm19
	vpermq	$0x2,%zmm22,%zmm26
	vpermq	$0x2,%zmm23,%zmm2
	vpaddq	%zmm25,%zmm20,%zmm20
	vpaddq	%zmm1,%zmm21,%zmm21
	vextracti64x4	$1,%zmm18,%ymm24
	vextracti64x4	$1,%zmm19,%ymm0
	vpaddq	%zmm26,%zmm22,%zmm22
	vpaddq	%zmm2,%zmm23,%zmm23

	vextracti64x4	$1,%zmm20,%ymm25
	vextracti64x4	$1,%zmm21,%ymm1
	vextracti64x4	$1,%zmm22,%ymm26
	vextracti64x4	$1,%zmm23,%ymm2
	vpaddq	%ymm24,%ymm18,%ymm18{%k1}{z}
	vpaddq	%ymm0,%ymm19,%ymm19{%k1}{z}
	vpaddq	%ymm25,%ymm20,%ymm20{%k1}{z}
	vpaddq	%ymm1,%ymm21,%ymm21{%k1}{z}
	vpaddq	%ymm26,%ymm22,%ymm22{%k1}{z}
	vpaddq	%ymm2,%ymm23,%ymm23{%k1}{z}



	vpsrlq	$44,%ymm18,%ymm30
	vpsllq	$8,%ymm19,%ymm19
	vpandq	%ymm28,%ymm18,%ymm0
	vpaddq	%ymm30,%ymm19,%ymm19

	vpaddq	%ymm19,%ymm20,%ymm20

	vpsrlq	$44,%ymm20,%ymm30
	vpsllq	$8,%ymm21,%ymm21
	vpandq	%ymm28,%ymm20,%ymm1
	vpaddq	%ymm30,%ymm21,%ymm21

	vpaddq	%ymm21,%ymm22,%ymm22

	vpsrlq	$42,%ymm22,%ymm30
	vpsllq	$10,%ymm23,%ymm23
	vpandq	%ymm29,%ymm22,%ymm2
	vpaddq	%ymm30,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm0,%ymm0
	vpsllq	$2,%ymm23,%ymm23

	vpaddq	%ymm23,%ymm0,%ymm0

	vpsrlq	$44,%ymm0,%ymm30
	vpandq	%ymm28,%ymm0,%ymm0

	vpaddq	%ymm30,%ymm1,%ymm1



	vmovq	%xmm0,0(%rdi)
	vmovq	%xmm1,8(%rdi)
	vmovq	%xmm2,16(%rdi)
	vzeroall

L$no_data_vpmadd52_8x:
	.byte	0xf3,0xc3


.p2align	5
poly1305_emit_base2_44:
	movq	0(%rdi),%r8
	movq	8(%rdi),%r9
	movq	16(%rdi),%r10

	movq	%r9,%rax
	shrq	$20,%r9
	shlq	$44,%rax
	movq	%r10,%rcx
	shrq	$40,%r10
	shlq	$24,%rcx

	addq	%rax,%r8
	adcq	%rcx,%r9
	adcq	$0,%r10

	movq	%r8,%rax
	addq	$5,%r8
	movq	%r9,%rcx
	adcq	$0,%r9
	adcq	$0,%r10
	shrq	$2,%r10
	cmovnzq	%r8,%rax
	cmovnzq	%r9,%rcx

	addq	0(%rdx),%rax
	adcq	8(%rdx),%rcx
	movq	%rax,0(%rsi)
	movq	%rcx,8(%rsi)

	.byte	0xf3,0xc3

.p2align	6
L$const:
L$mask24:
.long	0x0ffffff,0,0x0ffffff,0,0x0ffffff,0,0x0ffffff,0
L$129:
.long	16777216,0,16777216,0,16777216,0,16777216,0
L$mask26:
.long	0x3ffffff,0,0x3ffffff,0,0x3ffffff,0,0x3ffffff,0
L$permd_avx2:
.long	2,2,2,3,2,0,2,1
L$permd_avx512:
.long	0,0,0,1, 0,2,0,3, 0,4,0,5, 0,6,0,7

L$2_44_inp_permd:
.long	0,1,1,2,2,3,7,7
L$2_44_inp_shift:
.quad	0,12,24,64
L$2_44_mask:
.quad	0xfffffffffff,0xfffffffffff,0x3ffffffffff,0xffffffffffffffff
L$2_44_shift_rgt:
.quad	44,44,42,64
L$2_44_shift_lft:
.quad	8,8,10,64

.p2align	6
L$x_mask44:
.quad	0xfffffffffff,0xfffffffffff,0xfffffffffff,0xfffffffffff
.quad	0xfffffffffff,0xfffffffffff,0xfffffffffff,0xfffffffffff
L$x_mask42:
.quad	0x3ffffffffff,0x3ffffffffff,0x3ffffffffff,0x3ffffffffff
.quad	0x3ffffffffff,0x3ffffffffff,0x3ffffffffff,0x3ffffffffff
.byte	80,111,108,121,49,51,48,53,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
.p2align	4
.globl	_xor128_encrypt_n_pad

.p2align	4
_xor128_encrypt_n_pad:
	subq	%rdx,%rsi
	subq	%rdx,%rdi
	movq	%rcx,%r10
	shrq	$4,%rcx
	jz	L$tail_enc
	nop
L$oop_enc_xmm:
	movdqu	(%rsi,%rdx,1),%xmm0
	pxor	(%rdx),%xmm0
	movdqu	%xmm0,(%rdi,%rdx,1)
	movdqa	%xmm0,(%rdx)
	leaq	16(%rdx),%rdx
	decq	%rcx
	jnz	L$oop_enc_xmm

	andq	$15,%r10
	jz	L$done_enc

L$tail_enc:
	movq	$16,%rcx
	subq	%r10,%rcx
	xorl	%eax,%eax
L$oop_enc_byte:
	movb	(%rsi,%rdx,1),%al
	xorb	(%rdx),%al
	movb	%al,(%rdi,%rdx,1)
	movb	%al,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%r10
	jnz	L$oop_enc_byte

	xorl	%eax,%eax
L$oop_enc_pad:
	movb	%al,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%rcx
	jnz	L$oop_enc_pad

L$done_enc:
	movq	%rdx,%rax
	.byte	0xf3,0xc3


.globl	_xor128_decrypt_n_pad

.p2align	4
_xor128_decrypt_n_pad:
	subq	%rdx,%rsi
	subq	%rdx,%rdi
	movq	%rcx,%r10
	shrq	$4,%rcx
	jz	L$tail_dec
	nop
L$oop_dec_xmm:
	movdqu	(%rsi,%rdx,1),%xmm0
	movdqa	(%rdx),%xmm1
	pxor	%xmm0,%xmm1
	movdqu	%xmm1,(%rdi,%rdx,1)
	movdqa	%xmm0,(%rdx)
	leaq	16(%rdx),%rdx
	decq	%rcx
	jnz	L$oop_dec_xmm

	pxor	%xmm1,%xmm1
	andq	$15,%r10
	jz	L$done_dec

L$tail_dec:
	movq	$16,%rcx
	subq	%r10,%rcx
	xorl	%eax,%eax
	xorq	%r11,%r11
L$oop_dec_byte:
	movb	(%rsi,%rdx,1),%r11b
	movb	(%rdx),%al
	xorb	%r11b,%al
	movb	%al,(%rdi,%rdx,1)
	movb	%r11b,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%r10
	jnz	L$oop_dec_byte

	xorl	%eax,%eax
L$oop_dec_pad:
	movb	%al,(%rdx)
	leaq	1(%rdx),%rdx
	decq	%rcx
	jnz	L$oop_dec_pad

L$done_dec:
	movq	%rdx,%rax
	.byte	0xf3,0xc3

