.text	

.globl	_x25519_fe51_mul

.p2align	5
_x25519_fe51_mul:

	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	leaq	-40(%rsp),%rsp

L$fe51_mul_body:

	movq	0(%rsi),%rax
	movq	0(%rdx),%r11
	movq	8(%rdx),%r12
	movq	16(%rdx),%r13
	movq	24(%rdx),%rbp
	movq	32(%rdx),%r14

	movq	%rdi,32(%rsp)
	movq	%rax,%rdi
	mulq	%r11
	movq	%r11,0(%rsp)
	movq	%rax,%rbx
	movq	%rdi,%rax
	movq	%rdx,%rcx
	mulq	%r12
	movq	%r12,8(%rsp)
	movq	%rax,%r8
	movq	%rdi,%rax
	leaq	(%r14,%r14,8),%r15
	movq	%rdx,%r9
	mulq	%r13
	movq	%r13,16(%rsp)
	movq	%rax,%r10
	movq	%rdi,%rax
	leaq	(%r14,%r15,2),%rdi
	movq	%rdx,%r11
	mulq	%rbp
	movq	%rax,%r12
	movq	0(%rsi),%rax
	movq	%rdx,%r13
	mulq	%r14
	movq	%rax,%r14
	movq	8(%rsi),%rax
	movq	%rdx,%r15

	mulq	%rdi
	addq	%rax,%rbx
	movq	16(%rsi),%rax
	adcq	%rdx,%rcx
	mulq	%rdi
	addq	%rax,%r8
	movq	24(%rsi),%rax
	adcq	%rdx,%r9
	mulq	%rdi
	addq	%rax,%r10
	movq	32(%rsi),%rax
	adcq	%rdx,%r11
	mulq	%rdi
	imulq	$19,%rbp,%rdi
	addq	%rax,%r12
	movq	8(%rsi),%rax
	adcq	%rdx,%r13
	mulq	%rbp
	movq	16(%rsp),%rbp
	addq	%rax,%r14
	movq	16(%rsi),%rax
	adcq	%rdx,%r15

	mulq	%rdi
	addq	%rax,%rbx
	movq	24(%rsi),%rax
	adcq	%rdx,%rcx
	mulq	%rdi
	addq	%rax,%r8
	movq	32(%rsi),%rax
	adcq	%rdx,%r9
	mulq	%rdi
	imulq	$19,%rbp,%rdi
	addq	%rax,%r10
	movq	8(%rsi),%rax
	adcq	%rdx,%r11
	mulq	%rbp
	addq	%rax,%r12
	movq	16(%rsi),%rax
	adcq	%rdx,%r13
	mulq	%rbp
	movq	8(%rsp),%rbp
	addq	%rax,%r14
	movq	24(%rsi),%rax
	adcq	%rdx,%r15

	mulq	%rdi
	addq	%rax,%rbx
	movq	32(%rsi),%rax
	adcq	%rdx,%rcx
	mulq	%rdi
	addq	%rax,%r8
	movq	8(%rsi),%rax
	adcq	%rdx,%r9
	mulq	%rbp
	imulq	$19,%rbp,%rdi
	addq	%rax,%r10
	movq	16(%rsi),%rax
	adcq	%rdx,%r11
	mulq	%rbp
	addq	%rax,%r12
	movq	24(%rsi),%rax
	adcq	%rdx,%r13
	mulq	%rbp
	movq	0(%rsp),%rbp
	addq	%rax,%r14
	movq	32(%rsi),%rax
	adcq	%rdx,%r15

	mulq	%rdi
	addq	%rax,%rbx
	movq	8(%rsi),%rax
	adcq	%rdx,%rcx
	mulq	%rbp
	addq	%rax,%r8
	movq	16(%rsi),%rax
	adcq	%rdx,%r9
	mulq	%rbp
	addq	%rax,%r10
	movq	24(%rsi),%rax
	adcq	%rdx,%r11
	mulq	%rbp
	addq	%rax,%r12
	movq	32(%rsi),%rax
	adcq	%rdx,%r13
	mulq	%rbp
	addq	%rax,%r14
	adcq	%rdx,%r15

	movq	32(%rsp),%rdi
	jmp	L$reduce51
L$fe51_mul_epilogue:



.globl	_x25519_fe51_sqr

.p2align	5
_x25519_fe51_sqr:

	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	leaq	-40(%rsp),%rsp

L$fe51_sqr_body:

	movq	0(%rsi),%rax
	movq	16(%rsi),%r15
	movq	32(%rsi),%rbp

	movq	%rdi,32(%rsp)
	leaq	(%rax,%rax,1),%r14
	mulq	%rax
	movq	%rax,%rbx
	movq	8(%rsi),%rax
	movq	%rdx,%rcx
	mulq	%r14
	movq	%rax,%r8
	movq	%r15,%rax
	movq	%r15,0(%rsp)
	movq	%rdx,%r9
	mulq	%r14
	movq	%rax,%r10
	movq	24(%rsi),%rax
	movq	%rdx,%r11
	imulq	$19,%rbp,%rdi
	mulq	%r14
	movq	%rax,%r12
	movq	%rbp,%rax
	movq	%rdx,%r13
	mulq	%r14
	movq	%rax,%r14
	movq	%rbp,%rax
	movq	%rdx,%r15

	mulq	%rdi
	addq	%rax,%r12
	movq	8(%rsi),%rax
	adcq	%rdx,%r13

	movq	24(%rsi),%rsi
	leaq	(%rax,%rax,1),%rbp
	mulq	%rax
	addq	%rax,%r10
	movq	0(%rsp),%rax
	adcq	%rdx,%r11
	mulq	%rbp
	addq	%rax,%r12
	movq	%rbp,%rax
	adcq	%rdx,%r13
	mulq	%rsi
	addq	%rax,%r14
	movq	%rbp,%rax
	adcq	%rdx,%r15
	imulq	$19,%rsi,%rbp
	mulq	%rdi
	addq	%rax,%rbx
	leaq	(%rsi,%rsi,1),%rax
	adcq	%rdx,%rcx

	mulq	%rdi
	addq	%rax,%r10
	movq	%rsi,%rax
	adcq	%rdx,%r11
	mulq	%rbp
	addq	%rax,%r8
	movq	0(%rsp),%rax
	adcq	%rdx,%r9

	leaq	(%rax,%rax,1),%rsi
	mulq	%rax
	addq	%rax,%r14
	movq	%rbp,%rax
	adcq	%rdx,%r15
	mulq	%rsi
	addq	%rax,%rbx
	movq	%rsi,%rax
	adcq	%rdx,%rcx
	mulq	%rdi
	addq	%rax,%r8
	adcq	%rdx,%r9

	movq	32(%rsp),%rdi
	jmp	L$reduce51

.p2align	5
L$reduce51:
	movq	$0x7ffffffffffff,%rbp

	movq	%r10,%rdx
	shrq	$51,%r10
	shlq	$13,%r11
	andq	%rbp,%rdx
	orq	%r10,%r11
	addq	%r11,%r12
	adcq	$0,%r13

	movq	%rbx,%rax
	shrq	$51,%rbx
	shlq	$13,%rcx
	andq	%rbp,%rax
	orq	%rbx,%rcx
	addq	%rcx,%r8
	adcq	$0,%r9

	movq	%r12,%rbx
	shrq	$51,%r12
	shlq	$13,%r13
	andq	%rbp,%rbx
	orq	%r12,%r13
	addq	%r13,%r14
	adcq	$0,%r15

	movq	%r8,%rcx
	shrq	$51,%r8
	shlq	$13,%r9
	andq	%rbp,%rcx
	orq	%r8,%r9
	addq	%r9,%rdx

	movq	%r14,%r10
	shrq	$51,%r14
	shlq	$13,%r15
	andq	%rbp,%r10
	orq	%r14,%r15

	leaq	(%r15,%r15,8),%r14
	leaq	(%r15,%r14,2),%r15
	addq	%r15,%rax

	movq	%rdx,%r8
	andq	%rbp,%rdx
	shrq	$51,%r8
	addq	%r8,%rbx

	movq	%rax,%r9
	andq	%rbp,%rax
	shrq	$51,%r9
	addq	%r9,%rcx

	movq	%rax,0(%rdi)
	movq	%rcx,8(%rdi)
	movq	%rdx,16(%rdi)
	movq	%rbx,24(%rdi)
	movq	%r10,32(%rdi)

	movq	40(%rsp),%r15

	movq	48(%rsp),%r14

	movq	56(%rsp),%r13

	movq	64(%rsp),%r12

	movq	72(%rsp),%rbx

	movq	80(%rsp),%rbp

	leaq	88(%rsp),%rsp

L$fe51_sqr_epilogue:
	.byte	0xf3,0xc3



.globl	_x25519_fe51_mul121666

.p2align	5
_x25519_fe51_mul121666:

	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	leaq	-40(%rsp),%rsp

L$fe51_mul121666_body:
	movl	$121666,%eax

	mulq	0(%rsi)
	movq	%rax,%rbx
	movl	$121666,%eax
	movq	%rdx,%rcx
	mulq	8(%rsi)
	movq	%rax,%r8
	movl	$121666,%eax
	movq	%rdx,%r9
	mulq	16(%rsi)
	movq	%rax,%r10
	movl	$121666,%eax
	movq	%rdx,%r11
	mulq	24(%rsi)
	movq	%rax,%r12
	movl	$121666,%eax
	movq	%rdx,%r13
	mulq	32(%rsi)
	movq	%rax,%r14
	movq	%rdx,%r15

	jmp	L$reduce51
L$fe51_mul121666_epilogue:



.globl	_x25519_fe64_eligible

.p2align	5
_x25519_fe64_eligible:

	movl	_OPENSSL_ia32cap_P+8(%rip),%ecx
	xorl	%eax,%eax
	andl	$0x80100,%ecx
	cmpl	$0x80100,%ecx
	cmovel	%ecx,%eax
	.byte	0xf3,0xc3



.globl	_x25519_fe64_mul

.p2align	5
_x25519_fe64_mul:

	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	pushq	%rdi

	leaq	-16(%rsp),%rsp

L$fe64_mul_body:

	movq	%rdx,%rax
	movq	0(%rdx),%rbp
	movq	0(%rsi),%rdx
	movq	8(%rax),%rcx
	movq	16(%rax),%r14
	movq	24(%rax),%r15

	mulxq	%rbp,%r8,%rax
	xorl	%edi,%edi
	mulxq	%rcx,%r9,%rbx
	adcxq	%rax,%r9
	mulxq	%r14,%r10,%rax
	adcxq	%rbx,%r10
	mulxq	%r15,%r11,%r12
	movq	8(%rsi),%rdx
	adcxq	%rax,%r11
	movq	%r14,(%rsp)
	adcxq	%rdi,%r12

	mulxq	%rbp,%rax,%rbx
	adoxq	%rax,%r9
	adcxq	%rbx,%r10
	mulxq	%rcx,%rax,%rbx
	adoxq	%rax,%r10
	adcxq	%rbx,%r11
	mulxq	%r14,%rax,%rbx
	adoxq	%rax,%r11
	adcxq	%rbx,%r12
	mulxq	%r15,%rax,%r13
	movq	16(%rsi),%rdx
	adoxq	%rax,%r12
	adcxq	%rdi,%r13
	adoxq	%rdi,%r13

	mulxq	%rbp,%rax,%rbx
	adcxq	%rax,%r10
	adoxq	%rbx,%r11
	mulxq	%rcx,%rax,%rbx
	adcxq	%rax,%r11
	adoxq	%rbx,%r12
	mulxq	%r14,%rax,%rbx
	adcxq	%rax,%r12
	adoxq	%rbx,%r13
	mulxq	%r15,%rax,%r14
	movq	24(%rsi),%rdx
	adcxq	%rax,%r13
	adoxq	%rdi,%r14
	adcxq	%rdi,%r14

	mulxq	%rbp,%rax,%rbx
	adoxq	%rax,%r11
	adcxq	%rbx,%r12
	mulxq	%rcx,%rax,%rbx
	adoxq	%rax,%r12
	adcxq	%rbx,%r13
	mulxq	(%rsp),%rax,%rbx
	adoxq	%rax,%r13
	adcxq	%rbx,%r14
	mulxq	%r15,%rax,%r15
	movl	$38,%edx
	adoxq	%rax,%r14
	adcxq	%rdi,%r15
	adoxq	%rdi,%r15

	jmp	L$reduce64
L$fe64_mul_epilogue:



.globl	_x25519_fe64_sqr

.p2align	5
_x25519_fe64_sqr:

	pushq	%rbp

	pushq	%rbx

	pushq	%r12

	pushq	%r13

	pushq	%r14

	pushq	%r15

	pushq	%rdi

	leaq	-16(%rsp),%rsp

L$fe64_sqr_body:

	movq	0(%rsi),%rdx
	movq	8(%rsi),%rcx
	movq	16(%rsi),%rbp
	movq	24(%rsi),%rsi


	mulxq	%rdx,%r8,%r15
	mulxq	%rcx,%r9,%rax
	xorl	%edi,%edi
	mulxq	%rbp,%r10,%rbx
	adcxq	%rax,%r10
	mulxq	%rsi,%r11,%r12
	movq	%rcx,%rdx
	adcxq	%rbx,%r11
	adcxq	%rdi,%r12


	mulxq	%rbp,%rax,%rbx
	adoxq	%rax,%r11
	adcxq	%rbx,%r12
	mulxq	%rsi,%rax,%r13
	movq	%rbp,%rdx
	adoxq	%rax,%r12
	adcxq	%rdi,%r13


	mulxq	%rsi,%rax,%r14
	movq	%rcx,%rdx
	adoxq	%rax,%r13
	adcxq	%rdi,%r14
	adoxq	%rdi,%r14

	adcxq	%r9,%r9
	adoxq	%r15,%r9
	adcxq	%r10,%r10
	mulxq	%rdx,%rax,%rbx
	movq	%rbp,%rdx
	adcxq	%r11,%r11
	adoxq	%rax,%r10
	adcxq	%r12,%r12
	adoxq	%rbx,%r11
	mulxq	%rdx,%rax,%rbx
	movq	%rsi,%rdx
	adcxq	%r13,%r13
	adoxq	%rax,%r12
	adcxq	%r14,%r14
	adoxq	%rbx,%r13
	mulxq	%rdx,%rax,%r15
	movl	$38,%edx
	adoxq	%rax,%r14
	adcxq	%rdi,%r15
	adoxq	%rdi,%r15
	jmp	L$reduce64

.p2align	5
L$reduce64:
	mulxq	%r12,%rax,%rbx
	adcxq	%rax,%r8
	adoxq	%rbx,%r9
	mulxq	%r13,%rax,%rbx
	adcxq	%rax,%r9
	adoxq	%rbx,%r10
	mulxq	%r14,%rax,%rbx
	adcxq	%rax,%r10
	adoxq	%rbx,%r11
	mulxq	%r15,%rax,%r12
	adcxq	%rax,%r11
	adoxq	%rdi,%r12
	adcxq	%rdi,%r12

	movq	16(%rsp),%rdi
	imulq	%rdx,%r12

	addq	%r12,%r8
	adcq	$0,%r9
	adcq	$0,%r10
	adcq	$0,%r11

	sbbq	%rax,%rax
	andq	$38,%rax

	addq	%rax,%r8
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r8,0(%rdi)

	movq	24(%rsp),%r15

	movq	32(%rsp),%r14

	movq	40(%rsp),%r13

	movq	48(%rsp),%r12

	movq	56(%rsp),%rbx

	movq	64(%rsp),%rbp

	leaq	72(%rsp),%rsp

L$fe64_sqr_epilogue:
	.byte	0xf3,0xc3



.globl	_x25519_fe64_mul121666

.p2align	5
_x25519_fe64_mul121666:
L$fe64_mul121666_body:

	movl	$121666,%edx
	mulxq	0(%rsi),%r8,%rcx
	mulxq	8(%rsi),%r9,%rax
	addq	%rcx,%r9
	mulxq	16(%rsi),%r10,%rcx
	adcq	%rax,%r10
	mulxq	24(%rsi),%r11,%rax
	adcq	%rcx,%r11
	adcq	$0,%rax

	imulq	$38,%rax,%rax

	addq	%rax,%r8
	adcq	$0,%r9
	adcq	$0,%r10
	adcq	$0,%r11

	sbbq	%rax,%rax
	andq	$38,%rax

	addq	%rax,%r8
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)
	movq	%r8,0(%rdi)

L$fe64_mul121666_epilogue:
	.byte	0xf3,0xc3



.globl	_x25519_fe64_add

.p2align	5
_x25519_fe64_add:
L$fe64_add_body:

	movq	0(%rsi),%r8
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11

	addq	0(%rdx),%r8
	adcq	8(%rdx),%r9
	adcq	16(%rdx),%r10
	adcq	24(%rdx),%r11

	sbbq	%rax,%rax
	andq	$38,%rax

	addq	%rax,%r8
	adcq	$0,%r9
	adcq	$0,%r10
	movq	%r9,8(%rdi)
	adcq	$0,%r11
	movq	%r10,16(%rdi)
	sbbq	%rax,%rax
	movq	%r11,24(%rdi)
	andq	$38,%rax

	addq	%rax,%r8
	movq	%r8,0(%rdi)

L$fe64_add_epilogue:
	.byte	0xf3,0xc3



.globl	_x25519_fe64_sub

.p2align	5
_x25519_fe64_sub:
L$fe64_sub_body:

	movq	0(%rsi),%r8
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11

	subq	0(%rdx),%r8
	sbbq	8(%rdx),%r9
	sbbq	16(%rdx),%r10
	sbbq	24(%rdx),%r11

	sbbq	%rax,%rax
	andq	$38,%rax

	subq	%rax,%r8
	sbbq	$0,%r9
	sbbq	$0,%r10
	movq	%r9,8(%rdi)
	sbbq	$0,%r11
	movq	%r10,16(%rdi)
	sbbq	%rax,%rax
	movq	%r11,24(%rdi)
	andq	$38,%rax

	subq	%rax,%r8
	movq	%r8,0(%rdi)

L$fe64_sub_epilogue:
	.byte	0xf3,0xc3



.globl	_x25519_fe64_tobytes

.p2align	5
_x25519_fe64_tobytes:
L$fe64_to_body:

	movq	0(%rsi),%r8
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11


	leaq	(%r11,%r11,1),%rax
	sarq	$63,%r11
	shrq	$1,%rax
	andq	$19,%r11
	addq	$19,%r11

	addq	%r11,%r8
	adcq	$0,%r9
	adcq	$0,%r10
	adcq	$0,%rax

	leaq	(%rax,%rax,1),%r11
	sarq	$63,%rax
	shrq	$1,%r11
	notq	%rax
	andq	$19,%rax

	subq	%rax,%r8
	sbbq	$0,%r9
	sbbq	$0,%r10
	sbbq	$0,%r11

	movq	%r8,0(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

L$fe64_to_epilogue:
	.byte	0xf3,0xc3


.byte	88,50,53,53,49,57,32,112,114,105,109,105,116,105,118,101,115,32,102,111,114,32,120,56,54,95,54,52,44,32,67,82,89,80,84,79,71,65,77,83,32,98,121,32,60,97,112,112,114,111,64,111,112,101,110,115,115,108,46,111,114,103,62,0
