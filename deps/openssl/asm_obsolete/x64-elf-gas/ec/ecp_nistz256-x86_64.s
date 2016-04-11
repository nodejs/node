.text



.align	64
.Lpoly:
.quad	0xffffffffffffffff, 0x00000000ffffffff, 0x0000000000000000, 0xffffffff00000001


.LRR:
.quad	0x0000000000000003, 0xfffffffbffffffff, 0xfffffffffffffffe, 0x00000004fffffffd

.LOne:
.long	1,1,1,1,1,1,1,1
.LTwo:
.long	2,2,2,2,2,2,2,2
.LThree:
.long	3,3,3,3,3,3,3,3
.LONE_mont:
.quad	0x0000000000000001, 0xffffffff00000000, 0xffffffffffffffff, 0x00000000fffffffe

.globl	ecp_nistz256_mul_by_2
.type	ecp_nistz256_mul_by_2,@function
.align	64
ecp_nistz256_mul_by_2:
	pushq	%r12
	pushq	%r13

	movq	0(%rsi),%r8
	movq	8(%rsi),%r9
	addq	%r8,%r8
	movq	16(%rsi),%r10
	adcq	%r9,%r9
	movq	24(%rsi),%r11
	leaq	.Lpoly(%rip),%rsi
	movq	%r8,%rax
	adcq	%r10,%r10
	adcq	%r11,%r11
	movq	%r9,%rdx
	sbbq	%r13,%r13

	subq	0(%rsi),%r8
	movq	%r10,%rcx
	sbbq	8(%rsi),%r9
	sbbq	16(%rsi),%r10
	movq	%r11,%r12
	sbbq	24(%rsi),%r11
	testq	%r13,%r13

	cmovzq	%rax,%r8
	cmovzq	%rdx,%r9
	movq	%r8,0(%rdi)
	cmovzq	%rcx,%r10
	movq	%r9,8(%rdi)
	cmovzq	%r12,%r11
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

	popq	%r13
	popq	%r12
	.byte	0xf3,0xc3
.size	ecp_nistz256_mul_by_2,.-ecp_nistz256_mul_by_2



.globl	ecp_nistz256_div_by_2
.type	ecp_nistz256_div_by_2,@function
.align	32
ecp_nistz256_div_by_2:
	pushq	%r12
	pushq	%r13

	movq	0(%rsi),%r8
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	%r8,%rax
	movq	24(%rsi),%r11
	leaq	.Lpoly(%rip),%rsi

	movq	%r9,%rdx
	xorq	%r13,%r13
	addq	0(%rsi),%r8
	movq	%r10,%rcx
	adcq	8(%rsi),%r9
	adcq	16(%rsi),%r10
	movq	%r11,%r12
	adcq	24(%rsi),%r11
	adcq	$0,%r13
	xorq	%rsi,%rsi
	testq	$1,%rax

	cmovzq	%rax,%r8
	cmovzq	%rdx,%r9
	cmovzq	%rcx,%r10
	cmovzq	%r12,%r11
	cmovzq	%rsi,%r13

	movq	%r9,%rax
	shrq	$1,%r8
	shlq	$63,%rax
	movq	%r10,%rdx
	shrq	$1,%r9
	orq	%rax,%r8
	shlq	$63,%rdx
	movq	%r11,%rcx
	shrq	$1,%r10
	orq	%rdx,%r9
	shlq	$63,%rcx
	shrq	$1,%r11
	shlq	$63,%r13
	orq	%rcx,%r10
	orq	%r13,%r11

	movq	%r8,0(%rdi)
	movq	%r9,8(%rdi)
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

	popq	%r13
	popq	%r12
	.byte	0xf3,0xc3
.size	ecp_nistz256_div_by_2,.-ecp_nistz256_div_by_2



.globl	ecp_nistz256_mul_by_3
.type	ecp_nistz256_mul_by_3,@function
.align	32
ecp_nistz256_mul_by_3:
	pushq	%r12
	pushq	%r13

	movq	0(%rsi),%r8
	xorq	%r13,%r13
	movq	8(%rsi),%r9
	addq	%r8,%r8
	movq	16(%rsi),%r10
	adcq	%r9,%r9
	movq	24(%rsi),%r11
	movq	%r8,%rax
	adcq	%r10,%r10
	adcq	%r11,%r11
	movq	%r9,%rdx
	adcq	$0,%r13

	subq	$-1,%r8
	movq	%r10,%rcx
	sbbq	.Lpoly+8(%rip),%r9
	sbbq	$0,%r10
	movq	%r11,%r12
	sbbq	.Lpoly+24(%rip),%r11
	testq	%r13,%r13

	cmovzq	%rax,%r8
	cmovzq	%rdx,%r9
	cmovzq	%rcx,%r10
	cmovzq	%r12,%r11

	xorq	%r13,%r13
	addq	0(%rsi),%r8
	adcq	8(%rsi),%r9
	movq	%r8,%rax
	adcq	16(%rsi),%r10
	adcq	24(%rsi),%r11
	movq	%r9,%rdx
	adcq	$0,%r13

	subq	$-1,%r8
	movq	%r10,%rcx
	sbbq	.Lpoly+8(%rip),%r9
	sbbq	$0,%r10
	movq	%r11,%r12
	sbbq	.Lpoly+24(%rip),%r11
	testq	%r13,%r13

	cmovzq	%rax,%r8
	cmovzq	%rdx,%r9
	movq	%r8,0(%rdi)
	cmovzq	%rcx,%r10
	movq	%r9,8(%rdi)
	cmovzq	%r12,%r11
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

	popq	%r13
	popq	%r12
	.byte	0xf3,0xc3
.size	ecp_nistz256_mul_by_3,.-ecp_nistz256_mul_by_3



.globl	ecp_nistz256_add
.type	ecp_nistz256_add,@function
.align	32
ecp_nistz256_add:
	pushq	%r12
	pushq	%r13

	movq	0(%rsi),%r8
	xorq	%r13,%r13
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11
	leaq	.Lpoly(%rip),%rsi

	addq	0(%rdx),%r8
	adcq	8(%rdx),%r9
	movq	%r8,%rax
	adcq	16(%rdx),%r10
	adcq	24(%rdx),%r11
	movq	%r9,%rdx
	adcq	$0,%r13

	subq	0(%rsi),%r8
	movq	%r10,%rcx
	sbbq	8(%rsi),%r9
	sbbq	16(%rsi),%r10
	movq	%r11,%r12
	sbbq	24(%rsi),%r11
	testq	%r13,%r13

	cmovzq	%rax,%r8
	cmovzq	%rdx,%r9
	movq	%r8,0(%rdi)
	cmovzq	%rcx,%r10
	movq	%r9,8(%rdi)
	cmovzq	%r12,%r11
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

	popq	%r13
	popq	%r12
	.byte	0xf3,0xc3
.size	ecp_nistz256_add,.-ecp_nistz256_add



.globl	ecp_nistz256_sub
.type	ecp_nistz256_sub,@function
.align	32
ecp_nistz256_sub:
	pushq	%r12
	pushq	%r13

	movq	0(%rsi),%r8
	xorq	%r13,%r13
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11
	leaq	.Lpoly(%rip),%rsi

	subq	0(%rdx),%r8
	sbbq	8(%rdx),%r9
	movq	%r8,%rax
	sbbq	16(%rdx),%r10
	sbbq	24(%rdx),%r11
	movq	%r9,%rdx
	sbbq	$0,%r13

	addq	0(%rsi),%r8
	movq	%r10,%rcx
	adcq	8(%rsi),%r9
	adcq	16(%rsi),%r10
	movq	%r11,%r12
	adcq	24(%rsi),%r11
	testq	%r13,%r13

	cmovzq	%rax,%r8
	cmovzq	%rdx,%r9
	movq	%r8,0(%rdi)
	cmovzq	%rcx,%r10
	movq	%r9,8(%rdi)
	cmovzq	%r12,%r11
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

	popq	%r13
	popq	%r12
	.byte	0xf3,0xc3
.size	ecp_nistz256_sub,.-ecp_nistz256_sub



.globl	ecp_nistz256_neg
.type	ecp_nistz256_neg,@function
.align	32
ecp_nistz256_neg:
	pushq	%r12
	pushq	%r13

	xorq	%r8,%r8
	xorq	%r9,%r9
	xorq	%r10,%r10
	xorq	%r11,%r11
	xorq	%r13,%r13

	subq	0(%rsi),%r8
	sbbq	8(%rsi),%r9
	sbbq	16(%rsi),%r10
	movq	%r8,%rax
	sbbq	24(%rsi),%r11
	leaq	.Lpoly(%rip),%rsi
	movq	%r9,%rdx
	sbbq	$0,%r13

	addq	0(%rsi),%r8
	movq	%r10,%rcx
	adcq	8(%rsi),%r9
	adcq	16(%rsi),%r10
	movq	%r11,%r12
	adcq	24(%rsi),%r11
	testq	%r13,%r13

	cmovzq	%rax,%r8
	cmovzq	%rdx,%r9
	movq	%r8,0(%rdi)
	cmovzq	%rcx,%r10
	movq	%r9,8(%rdi)
	cmovzq	%r12,%r11
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

	popq	%r13
	popq	%r12
	.byte	0xf3,0xc3
.size	ecp_nistz256_neg,.-ecp_nistz256_neg




.globl	ecp_nistz256_to_mont
.type	ecp_nistz256_to_mont,@function
.align	32
ecp_nistz256_to_mont:
	leaq	.LRR(%rip),%rdx
	jmp	.Lmul_mont
.size	ecp_nistz256_to_mont,.-ecp_nistz256_to_mont







.globl	ecp_nistz256_mul_mont
.type	ecp_nistz256_mul_mont,@function
.align	32
ecp_nistz256_mul_mont:
.Lmul_mont:
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	%rdx,%rbx
	movq	0(%rdx),%rax
	movq	0(%rsi),%r9
	movq	8(%rsi),%r10
	movq	16(%rsi),%r11
	movq	24(%rsi),%r12

	call	__ecp_nistz256_mul_montq
.Lmul_mont_done:
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3
.size	ecp_nistz256_mul_mont,.-ecp_nistz256_mul_mont

.type	__ecp_nistz256_mul_montq,@function
.align	32
__ecp_nistz256_mul_montq:


	movq	%rax,%rbp
	mulq	%r9
	movq	.Lpoly+8(%rip),%r14
	movq	%rax,%r8
	movq	%rbp,%rax
	movq	%rdx,%r9

	mulq	%r10
	movq	.Lpoly+24(%rip),%r15
	addq	%rax,%r9
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%r10

	mulq	%r11
	addq	%rax,%r10
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%r12
	addq	%rax,%r11
	movq	%r8,%rax
	adcq	$0,%rdx
	xorq	%r13,%r13
	movq	%rdx,%r12










	movq	%r8,%rbp
	shlq	$32,%r8
	mulq	%r15
	shrq	$32,%rbp
	addq	%r8,%r9
	adcq	%rbp,%r10
	adcq	%rax,%r11
	movq	8(%rbx),%rax
	adcq	%rdx,%r12
	adcq	$0,%r13
	xorq	%r8,%r8



	movq	%rax,%rbp
	mulq	0(%rsi)
	addq	%rax,%r9
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	8(%rsi)
	addq	%rcx,%r10
	adcq	$0,%rdx
	addq	%rax,%r10
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	16(%rsi)
	addq	%rcx,%r11
	adcq	$0,%rdx
	addq	%rax,%r11
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	24(%rsi)
	addq	%rcx,%r12
	adcq	$0,%rdx
	addq	%rax,%r12
	movq	%r9,%rax
	adcq	%rdx,%r13
	adcq	$0,%r8



	movq	%r9,%rbp
	shlq	$32,%r9
	mulq	%r15
	shrq	$32,%rbp
	addq	%r9,%r10
	adcq	%rbp,%r11
	adcq	%rax,%r12
	movq	16(%rbx),%rax
	adcq	%rdx,%r13
	adcq	$0,%r8
	xorq	%r9,%r9



	movq	%rax,%rbp
	mulq	0(%rsi)
	addq	%rax,%r10
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	8(%rsi)
	addq	%rcx,%r11
	adcq	$0,%rdx
	addq	%rax,%r11
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	16(%rsi)
	addq	%rcx,%r12
	adcq	$0,%rdx
	addq	%rax,%r12
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	24(%rsi)
	addq	%rcx,%r13
	adcq	$0,%rdx
	addq	%rax,%r13
	movq	%r10,%rax
	adcq	%rdx,%r8
	adcq	$0,%r9



	movq	%r10,%rbp
	shlq	$32,%r10
	mulq	%r15
	shrq	$32,%rbp
	addq	%r10,%r11
	adcq	%rbp,%r12
	adcq	%rax,%r13
	movq	24(%rbx),%rax
	adcq	%rdx,%r8
	adcq	$0,%r9
	xorq	%r10,%r10



	movq	%rax,%rbp
	mulq	0(%rsi)
	addq	%rax,%r11
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	8(%rsi)
	addq	%rcx,%r12
	adcq	$0,%rdx
	addq	%rax,%r12
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	16(%rsi)
	addq	%rcx,%r13
	adcq	$0,%rdx
	addq	%rax,%r13
	movq	%rbp,%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	24(%rsi)
	addq	%rcx,%r8
	adcq	$0,%rdx
	addq	%rax,%r8
	movq	%r11,%rax
	adcq	%rdx,%r9
	adcq	$0,%r10



	movq	%r11,%rbp
	shlq	$32,%r11
	mulq	%r15
	shrq	$32,%rbp
	addq	%r11,%r12
	adcq	%rbp,%r13
	movq	%r12,%rcx
	adcq	%rax,%r8
	adcq	%rdx,%r9
	movq	%r13,%rbp
	adcq	$0,%r10



	subq	$-1,%r12
	movq	%r8,%rbx
	sbbq	%r14,%r13
	sbbq	$0,%r8
	movq	%r9,%rdx
	sbbq	%r15,%r9
	sbbq	$0,%r10

	cmovcq	%rcx,%r12
	cmovcq	%rbp,%r13
	movq	%r12,0(%rdi)
	cmovcq	%rbx,%r8
	movq	%r13,8(%rdi)
	cmovcq	%rdx,%r9
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)

	.byte	0xf3,0xc3
.size	__ecp_nistz256_mul_montq,.-__ecp_nistz256_mul_montq








.globl	ecp_nistz256_sqr_mont
.type	ecp_nistz256_sqr_mont,@function
.align	32
ecp_nistz256_sqr_mont:
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	movq	0(%rsi),%rax
	movq	8(%rsi),%r14
	movq	16(%rsi),%r15
	movq	24(%rsi),%r8

	call	__ecp_nistz256_sqr_montq
.Lsqr_mont_done:
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3
.size	ecp_nistz256_sqr_mont,.-ecp_nistz256_sqr_mont

.type	__ecp_nistz256_sqr_montq,@function
.align	32
__ecp_nistz256_sqr_montq:
	movq	%rax,%r13
	mulq	%r14
	movq	%rax,%r9
	movq	%r15,%rax
	movq	%rdx,%r10

	mulq	%r13
	addq	%rax,%r10
	movq	%r8,%rax
	adcq	$0,%rdx
	movq	%rdx,%r11

	mulq	%r13
	addq	%rax,%r11
	movq	%r15,%rax
	adcq	$0,%rdx
	movq	%rdx,%r12


	mulq	%r14
	addq	%rax,%r11
	movq	%r8,%rax
	adcq	$0,%rdx
	movq	%rdx,%rbp

	mulq	%r14
	addq	%rax,%r12
	movq	%r8,%rax
	adcq	$0,%rdx
	addq	%rbp,%r12
	movq	%rdx,%r13
	adcq	$0,%r13


	mulq	%r15
	xorq	%r15,%r15
	addq	%rax,%r13
	movq	0(%rsi),%rax
	movq	%rdx,%r14
	adcq	$0,%r14

	addq	%r9,%r9
	adcq	%r10,%r10
	adcq	%r11,%r11
	adcq	%r12,%r12
	adcq	%r13,%r13
	adcq	%r14,%r14
	adcq	$0,%r15

	mulq	%rax
	movq	%rax,%r8
	movq	8(%rsi),%rax
	movq	%rdx,%rcx

	mulq	%rax
	addq	%rcx,%r9
	adcq	%rax,%r10
	movq	16(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	%rax
	addq	%rcx,%r11
	adcq	%rax,%r12
	movq	24(%rsi),%rax
	adcq	$0,%rdx
	movq	%rdx,%rcx

	mulq	%rax
	addq	%rcx,%r13
	adcq	%rax,%r14
	movq	%r8,%rax
	adcq	%rdx,%r15

	movq	.Lpoly+8(%rip),%rsi
	movq	.Lpoly+24(%rip),%rbp




	movq	%r8,%rcx
	shlq	$32,%r8
	mulq	%rbp
	shrq	$32,%rcx
	addq	%r8,%r9
	adcq	%rcx,%r10
	adcq	%rax,%r11
	movq	%r9,%rax
	adcq	$0,%rdx



	movq	%r9,%rcx
	shlq	$32,%r9
	movq	%rdx,%r8
	mulq	%rbp
	shrq	$32,%rcx
	addq	%r9,%r10
	adcq	%rcx,%r11
	adcq	%rax,%r8
	movq	%r10,%rax
	adcq	$0,%rdx



	movq	%r10,%rcx
	shlq	$32,%r10
	movq	%rdx,%r9
	mulq	%rbp
	shrq	$32,%rcx
	addq	%r10,%r11
	adcq	%rcx,%r8
	adcq	%rax,%r9
	movq	%r11,%rax
	adcq	$0,%rdx



	movq	%r11,%rcx
	shlq	$32,%r11
	movq	%rdx,%r10
	mulq	%rbp
	shrq	$32,%rcx
	addq	%r11,%r8
	adcq	%rcx,%r9
	adcq	%rax,%r10
	adcq	$0,%rdx
	xorq	%r11,%r11



	addq	%r8,%r12
	adcq	%r9,%r13
	movq	%r12,%r8
	adcq	%r10,%r14
	adcq	%rdx,%r15
	movq	%r13,%r9
	adcq	$0,%r11

	subq	$-1,%r12
	movq	%r14,%r10
	sbbq	%rsi,%r13
	sbbq	$0,%r14
	movq	%r15,%rcx
	sbbq	%rbp,%r15
	sbbq	$0,%r11

	cmovcq	%r8,%r12
	cmovcq	%r9,%r13
	movq	%r12,0(%rdi)
	cmovcq	%r10,%r14
	movq	%r13,8(%rdi)
	cmovcq	%rcx,%r15
	movq	%r14,16(%rdi)
	movq	%r15,24(%rdi)

	.byte	0xf3,0xc3
.size	__ecp_nistz256_sqr_montq,.-__ecp_nistz256_sqr_montq






.globl	ecp_nistz256_from_mont
.type	ecp_nistz256_from_mont,@function
.align	32
ecp_nistz256_from_mont:
	pushq	%r12
	pushq	%r13

	movq	0(%rsi),%rax
	movq	.Lpoly+24(%rip),%r13
	movq	8(%rsi),%r9
	movq	16(%rsi),%r10
	movq	24(%rsi),%r11
	movq	%rax,%r8
	movq	.Lpoly+8(%rip),%r12



	movq	%rax,%rcx
	shlq	$32,%r8
	mulq	%r13
	shrq	$32,%rcx
	addq	%r8,%r9
	adcq	%rcx,%r10
	adcq	%rax,%r11
	movq	%r9,%rax
	adcq	$0,%rdx



	movq	%r9,%rcx
	shlq	$32,%r9
	movq	%rdx,%r8
	mulq	%r13
	shrq	$32,%rcx
	addq	%r9,%r10
	adcq	%rcx,%r11
	adcq	%rax,%r8
	movq	%r10,%rax
	adcq	$0,%rdx



	movq	%r10,%rcx
	shlq	$32,%r10
	movq	%rdx,%r9
	mulq	%r13
	shrq	$32,%rcx
	addq	%r10,%r11
	adcq	%rcx,%r8
	adcq	%rax,%r9
	movq	%r11,%rax
	adcq	$0,%rdx



	movq	%r11,%rcx
	shlq	$32,%r11
	movq	%rdx,%r10
	mulq	%r13
	shrq	$32,%rcx
	addq	%r11,%r8
	adcq	%rcx,%r9
	movq	%r8,%rcx
	adcq	%rax,%r10
	movq	%r9,%rsi
	adcq	$0,%rdx



	subq	$-1,%r8
	movq	%r10,%rax
	sbbq	%r12,%r9
	sbbq	$0,%r10
	movq	%rdx,%r11
	sbbq	%r13,%rdx
	sbbq	%r13,%r13

	cmovnzq	%rcx,%r8
	cmovnzq	%rsi,%r9
	movq	%r8,0(%rdi)
	cmovnzq	%rax,%r10
	movq	%r9,8(%rdi)
	cmovzq	%rdx,%r11
	movq	%r10,16(%rdi)
	movq	%r11,24(%rdi)

	popq	%r13
	popq	%r12
	.byte	0xf3,0xc3
.size	ecp_nistz256_from_mont,.-ecp_nistz256_from_mont


.globl	ecp_nistz256_select_w5
.type	ecp_nistz256_select_w5,@function
.align	32
ecp_nistz256_select_w5:
	movdqa	.LOne(%rip),%xmm0
	movd	%edx,%xmm1

	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7

	movdqa	%xmm0,%xmm8
	pshufd	$0,%xmm1,%xmm1

	movq	$16,%rax
.Lselect_loop_sse_w5:

	movdqa	%xmm8,%xmm15
	paddd	%xmm0,%xmm8
	pcmpeqd	%xmm1,%xmm15

	movdqa	0(%rsi),%xmm9
	movdqa	16(%rsi),%xmm10
	movdqa	32(%rsi),%xmm11
	movdqa	48(%rsi),%xmm12
	movdqa	64(%rsi),%xmm13
	movdqa	80(%rsi),%xmm14
	leaq	96(%rsi),%rsi

	pand	%xmm15,%xmm9
	pand	%xmm15,%xmm10
	por	%xmm9,%xmm2
	pand	%xmm15,%xmm11
	por	%xmm10,%xmm3
	pand	%xmm15,%xmm12
	por	%xmm11,%xmm4
	pand	%xmm15,%xmm13
	por	%xmm12,%xmm5
	pand	%xmm15,%xmm14
	por	%xmm13,%xmm6
	por	%xmm14,%xmm7

	decq	%rax
	jnz	.Lselect_loop_sse_w5

	movdqu	%xmm2,0(%rdi)
	movdqu	%xmm3,16(%rdi)
	movdqu	%xmm4,32(%rdi)
	movdqu	%xmm5,48(%rdi)
	movdqu	%xmm6,64(%rdi)
	movdqu	%xmm7,80(%rdi)
	.byte	0xf3,0xc3
.size	ecp_nistz256_select_w5,.-ecp_nistz256_select_w5



.globl	ecp_nistz256_select_w7
.type	ecp_nistz256_select_w7,@function
.align	32
ecp_nistz256_select_w7:
	movdqa	.LOne(%rip),%xmm8
	movd	%edx,%xmm1

	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5

	movdqa	%xmm8,%xmm0
	pshufd	$0,%xmm1,%xmm1
	movq	$64,%rax

.Lselect_loop_sse_w7:
	movdqa	%xmm8,%xmm15
	paddd	%xmm0,%xmm8
	movdqa	0(%rsi),%xmm9
	movdqa	16(%rsi),%xmm10
	pcmpeqd	%xmm1,%xmm15
	movdqa	32(%rsi),%xmm11
	movdqa	48(%rsi),%xmm12
	leaq	64(%rsi),%rsi

	pand	%xmm15,%xmm9
	pand	%xmm15,%xmm10
	por	%xmm9,%xmm2
	pand	%xmm15,%xmm11
	por	%xmm10,%xmm3
	pand	%xmm15,%xmm12
	por	%xmm11,%xmm4
	prefetcht0	255(%rsi)
	por	%xmm12,%xmm5

	decq	%rax
	jnz	.Lselect_loop_sse_w7

	movdqu	%xmm2,0(%rdi)
	movdqu	%xmm3,16(%rdi)
	movdqu	%xmm4,32(%rdi)
	movdqu	%xmm5,48(%rdi)
	.byte	0xf3,0xc3
.size	ecp_nistz256_select_w7,.-ecp_nistz256_select_w7
.globl	ecp_nistz256_avx2_select_w7
.type	ecp_nistz256_avx2_select_w7,@function
.align	32
ecp_nistz256_avx2_select_w7:
.byte	0x0f,0x0b
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_select_w7,.-ecp_nistz256_avx2_select_w7
.type	__ecp_nistz256_add_toq,@function
.align	32
__ecp_nistz256_add_toq:
	addq	0(%rbx),%r12
	adcq	8(%rbx),%r13
	movq	%r12,%rax
	adcq	16(%rbx),%r8
	adcq	24(%rbx),%r9
	movq	%r13,%rbp
	sbbq	%r11,%r11

	subq	$-1,%r12
	movq	%r8,%rcx
	sbbq	%r14,%r13
	sbbq	$0,%r8
	movq	%r9,%r10
	sbbq	%r15,%r9
	testq	%r11,%r11

	cmovzq	%rax,%r12
	cmovzq	%rbp,%r13
	movq	%r12,0(%rdi)
	cmovzq	%rcx,%r8
	movq	%r13,8(%rdi)
	cmovzq	%r10,%r9
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)

	.byte	0xf3,0xc3
.size	__ecp_nistz256_add_toq,.-__ecp_nistz256_add_toq

.type	__ecp_nistz256_sub_fromq,@function
.align	32
__ecp_nistz256_sub_fromq:
	subq	0(%rbx),%r12
	sbbq	8(%rbx),%r13
	movq	%r12,%rax
	sbbq	16(%rbx),%r8
	sbbq	24(%rbx),%r9
	movq	%r13,%rbp
	sbbq	%r11,%r11

	addq	$-1,%r12
	movq	%r8,%rcx
	adcq	%r14,%r13
	adcq	$0,%r8
	movq	%r9,%r10
	adcq	%r15,%r9
	testq	%r11,%r11

	cmovzq	%rax,%r12
	cmovzq	%rbp,%r13
	movq	%r12,0(%rdi)
	cmovzq	%rcx,%r8
	movq	%r13,8(%rdi)
	cmovzq	%r10,%r9
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)

	.byte	0xf3,0xc3
.size	__ecp_nistz256_sub_fromq,.-__ecp_nistz256_sub_fromq

.type	__ecp_nistz256_subq,@function
.align	32
__ecp_nistz256_subq:
	subq	%r12,%rax
	sbbq	%r13,%rbp
	movq	%rax,%r12
	sbbq	%r8,%rcx
	sbbq	%r9,%r10
	movq	%rbp,%r13
	sbbq	%r11,%r11

	addq	$-1,%rax
	movq	%rcx,%r8
	adcq	%r14,%rbp
	adcq	$0,%rcx
	movq	%r10,%r9
	adcq	%r15,%r10
	testq	%r11,%r11

	cmovnzq	%rax,%r12
	cmovnzq	%rbp,%r13
	cmovnzq	%rcx,%r8
	cmovnzq	%r10,%r9

	.byte	0xf3,0xc3
.size	__ecp_nistz256_subq,.-__ecp_nistz256_subq

.type	__ecp_nistz256_mul_by_2q,@function
.align	32
__ecp_nistz256_mul_by_2q:
	addq	%r12,%r12
	adcq	%r13,%r13
	movq	%r12,%rax
	adcq	%r8,%r8
	adcq	%r9,%r9
	movq	%r13,%rbp
	sbbq	%r11,%r11

	subq	$-1,%r12
	movq	%r8,%rcx
	sbbq	%r14,%r13
	sbbq	$0,%r8
	movq	%r9,%r10
	sbbq	%r15,%r9
	testq	%r11,%r11

	cmovzq	%rax,%r12
	cmovzq	%rbp,%r13
	movq	%r12,0(%rdi)
	cmovzq	%rcx,%r8
	movq	%r13,8(%rdi)
	cmovzq	%r10,%r9
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)

	.byte	0xf3,0xc3
.size	__ecp_nistz256_mul_by_2q,.-__ecp_nistz256_mul_by_2q
.globl	ecp_nistz256_point_double
.type	ecp_nistz256_point_double,@function
.align	32
ecp_nistz256_point_double:
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$160+8,%rsp

.Lpoint_double_shortcutq:
	movdqu	0(%rsi),%xmm0
	movq	%rsi,%rbx
	movdqu	16(%rsi),%xmm1
	movq	32+0(%rsi),%r12
	movq	32+8(%rsi),%r13
	movq	32+16(%rsi),%r8
	movq	32+24(%rsi),%r9
	movq	.Lpoly+8(%rip),%r14
	movq	.Lpoly+24(%rip),%r15
	movdqa	%xmm0,96(%rsp)
	movdqa	%xmm1,96+16(%rsp)
	leaq	32(%rdi),%r10
	leaq	64(%rdi),%r11
.byte	102,72,15,110,199
.byte	102,73,15,110,202
.byte	102,73,15,110,211

	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_mul_by_2q

	movq	64+0(%rsi),%rax
	movq	64+8(%rsi),%r14
	movq	64+16(%rsi),%r15
	movq	64+24(%rsi),%r8
	leaq	64-0(%rsi),%rsi
	leaq	64(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	movq	0+0(%rsp),%rax
	movq	8+0(%rsp),%r14
	leaq	0+0(%rsp),%rsi
	movq	16+0(%rsp),%r15
	movq	24+0(%rsp),%r8
	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	movq	32(%rbx),%rax
	movq	64+0(%rbx),%r9
	movq	64+8(%rbx),%r10
	movq	64+16(%rbx),%r11
	movq	64+24(%rbx),%r12
	leaq	64-0(%rbx),%rsi
	leaq	32(%rbx),%rbx
.byte	102,72,15,126,215
	call	__ecp_nistz256_mul_montq
	call	__ecp_nistz256_mul_by_2q

	movq	96+0(%rsp),%r12
	movq	96+8(%rsp),%r13
	leaq	64(%rsp),%rbx
	movq	96+16(%rsp),%r8
	movq	96+24(%rsp),%r9
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_add_toq

	movq	96+0(%rsp),%r12
	movq	96+8(%rsp),%r13
	leaq	64(%rsp),%rbx
	movq	96+16(%rsp),%r8
	movq	96+24(%rsp),%r9
	leaq	64(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

	movq	0+0(%rsp),%rax
	movq	8+0(%rsp),%r14
	leaq	0+0(%rsp),%rsi
	movq	16+0(%rsp),%r15
	movq	24+0(%rsp),%r8
.byte	102,72,15,126,207
	call	__ecp_nistz256_sqr_montq
	xorq	%r9,%r9
	movq	%r12,%rax
	addq	$-1,%r12
	movq	%r13,%r10
	adcq	%rsi,%r13
	movq	%r14,%rcx
	adcq	$0,%r14
	movq	%r15,%r8
	adcq	%rbp,%r15
	adcq	$0,%r9
	xorq	%rsi,%rsi
	testq	$1,%rax

	cmovzq	%rax,%r12
	cmovzq	%r10,%r13
	cmovzq	%rcx,%r14
	cmovzq	%r8,%r15
	cmovzq	%rsi,%r9

	movq	%r13,%rax
	shrq	$1,%r12
	shlq	$63,%rax
	movq	%r14,%r10
	shrq	$1,%r13
	orq	%rax,%r12
	shlq	$63,%r10
	movq	%r15,%rcx
	shrq	$1,%r14
	orq	%r10,%r13
	shlq	$63,%rcx
	movq	%r12,0(%rdi)
	shrq	$1,%r15
	movq	%r13,8(%rdi)
	shlq	$63,%r9
	orq	%rcx,%r14
	orq	%r9,%r15
	movq	%r14,16(%rdi)
	movq	%r15,24(%rdi)
	movq	64(%rsp),%rax
	leaq	64(%rsp),%rbx
	movq	0+32(%rsp),%r9
	movq	8+32(%rsp),%r10
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r11
	movq	24+32(%rsp),%r12
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	128(%rsp),%rdi
	call	__ecp_nistz256_mul_by_2q

	leaq	32(%rsp),%rbx
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_add_toq

	movq	96(%rsp),%rax
	leaq	96(%rsp),%rbx
	movq	0+0(%rsp),%r9
	movq	8+0(%rsp),%r10
	leaq	0+0(%rsp),%rsi
	movq	16+0(%rsp),%r11
	movq	24+0(%rsp),%r12
	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	128(%rsp),%rdi
	call	__ecp_nistz256_mul_by_2q

	movq	0+32(%rsp),%rax
	movq	8+32(%rsp),%r14
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r15
	movq	24+32(%rsp),%r8
.byte	102,72,15,126,199
	call	__ecp_nistz256_sqr_montq

	leaq	128(%rsp),%rbx
	movq	%r14,%r8
	movq	%r15,%r9
	movq	%rsi,%r14
	movq	%rbp,%r15
	call	__ecp_nistz256_sub_fromq

	movq	0+0(%rsp),%rax
	movq	0+8(%rsp),%rbp
	movq	0+16(%rsp),%rcx
	movq	0+24(%rsp),%r10
	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_subq

	movq	32(%rsp),%rax
	leaq	32(%rsp),%rbx
	movq	%r12,%r14
	xorl	%ecx,%ecx
	movq	%r12,0+0(%rsp)
	movq	%r13,%r10
	movq	%r13,0+8(%rsp)
	cmovzq	%r8,%r11
	movq	%r8,0+16(%rsp)
	leaq	0-0(%rsp),%rsi
	cmovzq	%r9,%r12
	movq	%r9,0+24(%rsp)
	movq	%r14,%r9
	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

.byte	102,72,15,126,203
.byte	102,72,15,126,207
	call	__ecp_nistz256_sub_fromq

	addq	$160+8,%rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3
.size	ecp_nistz256_point_double,.-ecp_nistz256_point_double
.globl	ecp_nistz256_point_add
.type	ecp_nistz256_point_add,@function
.align	32
ecp_nistz256_point_add:
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$576+8,%rsp

	movdqu	0(%rsi),%xmm0
	movdqu	16(%rsi),%xmm1
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm3
	movdqu	64(%rsi),%xmm4
	movdqu	80(%rsi),%xmm5
	movq	%rsi,%rbx
	movq	%rdx,%rsi
	movdqa	%xmm0,384(%rsp)
	movdqa	%xmm1,384+16(%rsp)
	por	%xmm0,%xmm1
	movdqa	%xmm2,416(%rsp)
	movdqa	%xmm3,416+16(%rsp)
	por	%xmm2,%xmm3
	movdqa	%xmm4,448(%rsp)
	movdqa	%xmm5,448+16(%rsp)
	por	%xmm1,%xmm3

	movdqu	0(%rsi),%xmm0
	pshufd	$0xb1,%xmm3,%xmm5
	movdqu	16(%rsi),%xmm1
	movdqu	32(%rsi),%xmm2
	por	%xmm3,%xmm5
	movdqu	48(%rsi),%xmm3
	movq	64+0(%rsi),%rax
	movq	64+8(%rsi),%r14
	movq	64+16(%rsi),%r15
	movq	64+24(%rsi),%r8
	movdqa	%xmm0,480(%rsp)
	pshufd	$0x1e,%xmm5,%xmm4
	movdqa	%xmm1,480+16(%rsp)
	por	%xmm0,%xmm1
.byte	102,72,15,110,199
	movdqa	%xmm2,512(%rsp)
	movdqa	%xmm3,512+16(%rsp)
	por	%xmm2,%xmm3
	por	%xmm4,%xmm5
	pxor	%xmm4,%xmm4
	por	%xmm1,%xmm3

	leaq	64-0(%rsi),%rsi
	movq	%rax,544+0(%rsp)
	movq	%r14,544+8(%rsp)
	movq	%r15,544+16(%rsp)
	movq	%r8,544+24(%rsp)
	leaq	96(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	pcmpeqd	%xmm4,%xmm5
	pshufd	$0xb1,%xmm3,%xmm4
	por	%xmm3,%xmm4
	pshufd	$0,%xmm5,%xmm5
	pshufd	$0x1e,%xmm4,%xmm3
	por	%xmm3,%xmm4
	pxor	%xmm3,%xmm3
	pcmpeqd	%xmm3,%xmm4
	pshufd	$0,%xmm4,%xmm4
	movq	64+0(%rbx),%rax
	movq	64+8(%rbx),%r14
	movq	64+16(%rbx),%r15
	movq	64+24(%rbx),%r8
.byte	102,72,15,110,203

	leaq	64-0(%rbx),%rsi
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	movq	544(%rsp),%rax
	leaq	544(%rsp),%rbx
	movq	0+96(%rsp),%r9
	movq	8+96(%rsp),%r10
	leaq	0+96(%rsp),%rsi
	movq	16+96(%rsp),%r11
	movq	24+96(%rsp),%r12
	leaq	224(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	448(%rsp),%rax
	leaq	448(%rsp),%rbx
	movq	0+32(%rsp),%r9
	movq	8+32(%rsp),%r10
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r11
	movq	24+32(%rsp),%r12
	leaq	256(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	416(%rsp),%rax
	leaq	416(%rsp),%rbx
	movq	0+224(%rsp),%r9
	movq	8+224(%rsp),%r10
	leaq	0+224(%rsp),%rsi
	movq	16+224(%rsp),%r11
	movq	24+224(%rsp),%r12
	leaq	224(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	512(%rsp),%rax
	leaq	512(%rsp),%rbx
	movq	0+256(%rsp),%r9
	movq	8+256(%rsp),%r10
	leaq	0+256(%rsp),%rsi
	movq	16+256(%rsp),%r11
	movq	24+256(%rsp),%r12
	leaq	256(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	224(%rsp),%rbx
	leaq	64(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

	orq	%r13,%r12
	movdqa	%xmm4,%xmm2
	orq	%r8,%r12
	orq	%r9,%r12
	por	%xmm5,%xmm2
.byte	102,73,15,110,220

	movq	384(%rsp),%rax
	leaq	384(%rsp),%rbx
	movq	0+96(%rsp),%r9
	movq	8+96(%rsp),%r10
	leaq	0+96(%rsp),%rsi
	movq	16+96(%rsp),%r11
	movq	24+96(%rsp),%r12
	leaq	160(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	480(%rsp),%rax
	leaq	480(%rsp),%rbx
	movq	0+32(%rsp),%r9
	movq	8+32(%rsp),%r10
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r11
	movq	24+32(%rsp),%r12
	leaq	192(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	160(%rsp),%rbx
	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

	orq	%r13,%r12
	orq	%r8,%r12
	orq	%r9,%r12

.byte	0x3e
	jnz	.Ladd_proceedq
.byte	102,73,15,126,208
.byte	102,73,15,126,217
	testq	%r8,%r8
	jnz	.Ladd_proceedq
	testq	%r9,%r9
	jz	.Ladd_doubleq

.byte	102,72,15,126,199
	pxor	%xmm0,%xmm0
	movdqu	%xmm0,0(%rdi)
	movdqu	%xmm0,16(%rdi)
	movdqu	%xmm0,32(%rdi)
	movdqu	%xmm0,48(%rdi)
	movdqu	%xmm0,64(%rdi)
	movdqu	%xmm0,80(%rdi)
	jmp	.Ladd_doneq

.align	32
.Ladd_doubleq:
.byte	102,72,15,126,206
.byte	102,72,15,126,199
	addq	$416,%rsp
	jmp	.Lpoint_double_shortcutq

.align	32
.Ladd_proceedq:
	movq	0+64(%rsp),%rax
	movq	8+64(%rsp),%r14
	leaq	0+64(%rsp),%rsi
	movq	16+64(%rsp),%r15
	movq	24+64(%rsp),%r8
	leaq	96(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	movq	448(%rsp),%rax
	leaq	448(%rsp),%rbx
	movq	0+0(%rsp),%r9
	movq	8+0(%rsp),%r10
	leaq	0+0(%rsp),%rsi
	movq	16+0(%rsp),%r11
	movq	24+0(%rsp),%r12
	leaq	352(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	0+0(%rsp),%rax
	movq	8+0(%rsp),%r14
	leaq	0+0(%rsp),%rsi
	movq	16+0(%rsp),%r15
	movq	24+0(%rsp),%r8
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	movq	544(%rsp),%rax
	leaq	544(%rsp),%rbx
	movq	0+352(%rsp),%r9
	movq	8+352(%rsp),%r10
	leaq	0+352(%rsp),%rsi
	movq	16+352(%rsp),%r11
	movq	24+352(%rsp),%r12
	leaq	352(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	0(%rsp),%rax
	leaq	0(%rsp),%rbx
	movq	0+32(%rsp),%r9
	movq	8+32(%rsp),%r10
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r11
	movq	24+32(%rsp),%r12
	leaq	128(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	160(%rsp),%rax
	leaq	160(%rsp),%rbx
	movq	0+32(%rsp),%r9
	movq	8+32(%rsp),%r10
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r11
	movq	24+32(%rsp),%r12
	leaq	192(%rsp),%rdi
	call	__ecp_nistz256_mul_montq




	addq	%r12,%r12
	leaq	96(%rsp),%rsi
	adcq	%r13,%r13
	movq	%r12,%rax
	adcq	%r8,%r8
	adcq	%r9,%r9
	movq	%r13,%rbp
	sbbq	%r11,%r11

	subq	$-1,%r12
	movq	%r8,%rcx
	sbbq	%r14,%r13
	sbbq	$0,%r8
	movq	%r9,%r10
	sbbq	%r15,%r9
	testq	%r11,%r11

	cmovzq	%rax,%r12
	movq	0(%rsi),%rax
	cmovzq	%rbp,%r13
	movq	8(%rsi),%rbp
	cmovzq	%rcx,%r8
	movq	16(%rsi),%rcx
	cmovzq	%r10,%r9
	movq	24(%rsi),%r10

	call	__ecp_nistz256_subq

	leaq	128(%rsp),%rbx
	leaq	288(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

	movq	192+0(%rsp),%rax
	movq	192+8(%rsp),%rbp
	movq	192+16(%rsp),%rcx
	movq	192+24(%rsp),%r10
	leaq	320(%rsp),%rdi

	call	__ecp_nistz256_subq

	movq	%r12,0(%rdi)
	movq	%r13,8(%rdi)
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)
	movq	128(%rsp),%rax
	leaq	128(%rsp),%rbx
	movq	0+224(%rsp),%r9
	movq	8+224(%rsp),%r10
	leaq	0+224(%rsp),%rsi
	movq	16+224(%rsp),%r11
	movq	24+224(%rsp),%r12
	leaq	256(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	320(%rsp),%rax
	leaq	320(%rsp),%rbx
	movq	0+64(%rsp),%r9
	movq	8+64(%rsp),%r10
	leaq	0+64(%rsp),%rsi
	movq	16+64(%rsp),%r11
	movq	24+64(%rsp),%r12
	leaq	320(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	256(%rsp),%rbx
	leaq	320(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

.byte	102,72,15,126,199

	movdqa	%xmm5,%xmm0
	movdqa	%xmm5,%xmm1
	pandn	352(%rsp),%xmm0
	movdqa	%xmm5,%xmm2
	pandn	352+16(%rsp),%xmm1
	movdqa	%xmm5,%xmm3
	pand	544(%rsp),%xmm2
	pand	544+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3

	movdqa	%xmm4,%xmm0
	movdqa	%xmm4,%xmm1
	pandn	%xmm2,%xmm0
	movdqa	%xmm4,%xmm2
	pandn	%xmm3,%xmm1
	movdqa	%xmm4,%xmm3
	pand	448(%rsp),%xmm2
	pand	448+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3
	movdqu	%xmm2,64(%rdi)
	movdqu	%xmm3,80(%rdi)

	movdqa	%xmm5,%xmm0
	movdqa	%xmm5,%xmm1
	pandn	288(%rsp),%xmm0
	movdqa	%xmm5,%xmm2
	pandn	288+16(%rsp),%xmm1
	movdqa	%xmm5,%xmm3
	pand	480(%rsp),%xmm2
	pand	480+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3

	movdqa	%xmm4,%xmm0
	movdqa	%xmm4,%xmm1
	pandn	%xmm2,%xmm0
	movdqa	%xmm4,%xmm2
	pandn	%xmm3,%xmm1
	movdqa	%xmm4,%xmm3
	pand	384(%rsp),%xmm2
	pand	384+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3
	movdqu	%xmm2,0(%rdi)
	movdqu	%xmm3,16(%rdi)

	movdqa	%xmm5,%xmm0
	movdqa	%xmm5,%xmm1
	pandn	320(%rsp),%xmm0
	movdqa	%xmm5,%xmm2
	pandn	320+16(%rsp),%xmm1
	movdqa	%xmm5,%xmm3
	pand	512(%rsp),%xmm2
	pand	512+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3

	movdqa	%xmm4,%xmm0
	movdqa	%xmm4,%xmm1
	pandn	%xmm2,%xmm0
	movdqa	%xmm4,%xmm2
	pandn	%xmm3,%xmm1
	movdqa	%xmm4,%xmm3
	pand	416(%rsp),%xmm2
	pand	416+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm3,48(%rdi)

.Ladd_doneq:
	addq	$576+8,%rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3
.size	ecp_nistz256_point_add,.-ecp_nistz256_point_add
.globl	ecp_nistz256_point_add_affine
.type	ecp_nistz256_point_add_affine,@function
.align	32
ecp_nistz256_point_add_affine:
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$480+8,%rsp

	movdqu	0(%rsi),%xmm0
	movq	%rdx,%rbx
	movdqu	16(%rsi),%xmm1
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm3
	movdqu	64(%rsi),%xmm4
	movdqu	80(%rsi),%xmm5
	movq	64+0(%rsi),%rax
	movq	64+8(%rsi),%r14
	movq	64+16(%rsi),%r15
	movq	64+24(%rsi),%r8
	movdqa	%xmm0,320(%rsp)
	movdqa	%xmm1,320+16(%rsp)
	por	%xmm0,%xmm1
	movdqa	%xmm2,352(%rsp)
	movdqa	%xmm3,352+16(%rsp)
	por	%xmm2,%xmm3
	movdqa	%xmm4,384(%rsp)
	movdqa	%xmm5,384+16(%rsp)
	por	%xmm1,%xmm3

	movdqu	0(%rbx),%xmm0
	pshufd	$0xb1,%xmm3,%xmm5
	movdqu	16(%rbx),%xmm1
	movdqu	32(%rbx),%xmm2
	por	%xmm3,%xmm5
	movdqu	48(%rbx),%xmm3
	movdqa	%xmm0,416(%rsp)
	pshufd	$0x1e,%xmm5,%xmm4
	movdqa	%xmm1,416+16(%rsp)
	por	%xmm0,%xmm1
.byte	102,72,15,110,199
	movdqa	%xmm2,448(%rsp)
	movdqa	%xmm3,448+16(%rsp)
	por	%xmm2,%xmm3
	por	%xmm4,%xmm5
	pxor	%xmm4,%xmm4
	por	%xmm1,%xmm3

	leaq	64-0(%rsi),%rsi
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	pcmpeqd	%xmm4,%xmm5
	pshufd	$0xb1,%xmm3,%xmm4
	movq	0(%rbx),%rax

	movq	%r12,%r9
	por	%xmm3,%xmm4
	pshufd	$0,%xmm5,%xmm5
	pshufd	$0x1e,%xmm4,%xmm3
	movq	%r13,%r10
	por	%xmm3,%xmm4
	pxor	%xmm3,%xmm3
	movq	%r14,%r11
	pcmpeqd	%xmm3,%xmm4
	pshufd	$0,%xmm4,%xmm4

	leaq	32-0(%rsp),%rsi
	movq	%r15,%r12
	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	320(%rsp),%rbx
	leaq	64(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

	movq	384(%rsp),%rax
	leaq	384(%rsp),%rbx
	movq	0+32(%rsp),%r9
	movq	8+32(%rsp),%r10
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r11
	movq	24+32(%rsp),%r12
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	384(%rsp),%rax
	leaq	384(%rsp),%rbx
	movq	0+64(%rsp),%r9
	movq	8+64(%rsp),%r10
	leaq	0+64(%rsp),%rsi
	movq	16+64(%rsp),%r11
	movq	24+64(%rsp),%r12
	leaq	288(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	448(%rsp),%rax
	leaq	448(%rsp),%rbx
	movq	0+32(%rsp),%r9
	movq	8+32(%rsp),%r10
	leaq	0+32(%rsp),%rsi
	movq	16+32(%rsp),%r11
	movq	24+32(%rsp),%r12
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	352(%rsp),%rbx
	leaq	96(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

	movq	0+64(%rsp),%rax
	movq	8+64(%rsp),%r14
	leaq	0+64(%rsp),%rsi
	movq	16+64(%rsp),%r15
	movq	24+64(%rsp),%r8
	leaq	128(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	movq	0+96(%rsp),%rax
	movq	8+96(%rsp),%r14
	leaq	0+96(%rsp),%rsi
	movq	16+96(%rsp),%r15
	movq	24+96(%rsp),%r8
	leaq	192(%rsp),%rdi
	call	__ecp_nistz256_sqr_montq

	movq	128(%rsp),%rax
	leaq	128(%rsp),%rbx
	movq	0+64(%rsp),%r9
	movq	8+64(%rsp),%r10
	leaq	0+64(%rsp),%rsi
	movq	16+64(%rsp),%r11
	movq	24+64(%rsp),%r12
	leaq	160(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	320(%rsp),%rax
	leaq	320(%rsp),%rbx
	movq	0+128(%rsp),%r9
	movq	8+128(%rsp),%r10
	leaq	0+128(%rsp),%rsi
	movq	16+128(%rsp),%r11
	movq	24+128(%rsp),%r12
	leaq	0(%rsp),%rdi
	call	__ecp_nistz256_mul_montq




	addq	%r12,%r12
	leaq	192(%rsp),%rsi
	adcq	%r13,%r13
	movq	%r12,%rax
	adcq	%r8,%r8
	adcq	%r9,%r9
	movq	%r13,%rbp
	sbbq	%r11,%r11

	subq	$-1,%r12
	movq	%r8,%rcx
	sbbq	%r14,%r13
	sbbq	$0,%r8
	movq	%r9,%r10
	sbbq	%r15,%r9
	testq	%r11,%r11

	cmovzq	%rax,%r12
	movq	0(%rsi),%rax
	cmovzq	%rbp,%r13
	movq	8(%rsi),%rbp
	cmovzq	%rcx,%r8
	movq	16(%rsi),%rcx
	cmovzq	%r10,%r9
	movq	24(%rsi),%r10

	call	__ecp_nistz256_subq

	leaq	160(%rsp),%rbx
	leaq	224(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

	movq	0+0(%rsp),%rax
	movq	0+8(%rsp),%rbp
	movq	0+16(%rsp),%rcx
	movq	0+24(%rsp),%r10
	leaq	64(%rsp),%rdi

	call	__ecp_nistz256_subq

	movq	%r12,0(%rdi)
	movq	%r13,8(%rdi)
	movq	%r8,16(%rdi)
	movq	%r9,24(%rdi)
	movq	352(%rsp),%rax
	leaq	352(%rsp),%rbx
	movq	0+160(%rsp),%r9
	movq	8+160(%rsp),%r10
	leaq	0+160(%rsp),%rsi
	movq	16+160(%rsp),%r11
	movq	24+160(%rsp),%r12
	leaq	32(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	movq	96(%rsp),%rax
	leaq	96(%rsp),%rbx
	movq	0+64(%rsp),%r9
	movq	8+64(%rsp),%r10
	leaq	0+64(%rsp),%rsi
	movq	16+64(%rsp),%r11
	movq	24+64(%rsp),%r12
	leaq	64(%rsp),%rdi
	call	__ecp_nistz256_mul_montq

	leaq	32(%rsp),%rbx
	leaq	256(%rsp),%rdi
	call	__ecp_nistz256_sub_fromq

.byte	102,72,15,126,199

	movdqa	%xmm5,%xmm0
	movdqa	%xmm5,%xmm1
	pandn	288(%rsp),%xmm0
	movdqa	%xmm5,%xmm2
	pandn	288+16(%rsp),%xmm1
	movdqa	%xmm5,%xmm3
	pand	.LONE_mont(%rip),%xmm2
	pand	.LONE_mont+16(%rip),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3

	movdqa	%xmm4,%xmm0
	movdqa	%xmm4,%xmm1
	pandn	%xmm2,%xmm0
	movdqa	%xmm4,%xmm2
	pandn	%xmm3,%xmm1
	movdqa	%xmm4,%xmm3
	pand	384(%rsp),%xmm2
	pand	384+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3
	movdqu	%xmm2,64(%rdi)
	movdqu	%xmm3,80(%rdi)

	movdqa	%xmm5,%xmm0
	movdqa	%xmm5,%xmm1
	pandn	224(%rsp),%xmm0
	movdqa	%xmm5,%xmm2
	pandn	224+16(%rsp),%xmm1
	movdqa	%xmm5,%xmm3
	pand	416(%rsp),%xmm2
	pand	416+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3

	movdqa	%xmm4,%xmm0
	movdqa	%xmm4,%xmm1
	pandn	%xmm2,%xmm0
	movdqa	%xmm4,%xmm2
	pandn	%xmm3,%xmm1
	movdqa	%xmm4,%xmm3
	pand	320(%rsp),%xmm2
	pand	320+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3
	movdqu	%xmm2,0(%rdi)
	movdqu	%xmm3,16(%rdi)

	movdqa	%xmm5,%xmm0
	movdqa	%xmm5,%xmm1
	pandn	256(%rsp),%xmm0
	movdqa	%xmm5,%xmm2
	pandn	256+16(%rsp),%xmm1
	movdqa	%xmm5,%xmm3
	pand	448(%rsp),%xmm2
	pand	448+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3

	movdqa	%xmm4,%xmm0
	movdqa	%xmm4,%xmm1
	pandn	%xmm2,%xmm0
	movdqa	%xmm4,%xmm2
	pandn	%xmm3,%xmm1
	movdqa	%xmm4,%xmm3
	pand	352(%rsp),%xmm2
	pand	352+16(%rsp),%xmm3
	por	%xmm0,%xmm2
	por	%xmm1,%xmm3
	movdqu	%xmm2,32(%rdi)
	movdqu	%xmm3,48(%rdi)

	addq	$480+8,%rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp
	.byte	0xf3,0xc3
.size	ecp_nistz256_point_add_affine,.-ecp_nistz256_point_add_affine
