.text


.type	MULADD_128x512,@function
.align	16
MULADD_128x512:
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	movq	%r8,0(%rcx)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%r8
	movq	8(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	movq	%r9,8(%rcx)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%r9
	.byte	0xf3,0xc3
.size	MULADD_128x512,.-MULADD_128x512
.type	mont_reduce,@function
.align	16
mont_reduce:
	leaq	192(%rsp),%rdi
	movq	32(%rsp),%rsi
	addq	$576,%rsi
	leaq	520(%rsp),%rcx

	movq	96(%rcx),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	movq	(%rcx),%r8
	addq	%rax,%r8
	adcq	$0,%rdx
	movq	%r8,0(%rdi)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	movq	8(%rcx),%r9
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	movq	16(%rcx),%r10
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	movq	24(%rcx),%r11
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	movq	32(%rcx),%r12
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	movq	40(%rcx),%r13
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	movq	48(%rcx),%r14
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	movq	56(%rcx),%r15
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%r8
	movq	104(%rcx),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	movq	%r9,8(%rdi)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%r9
	movq	112(%rcx),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	movq	%r10,16(%rdi)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%r10
	movq	120(%rcx),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	movq	%r11,24(%rdi)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%r11
	xorq	%rax,%rax

	addq	64(%rcx),%r8
	adcq	72(%rcx),%r9
	adcq	80(%rcx),%r10
	adcq	88(%rcx),%r11
	adcq	$0,%rax




	movq	%r8,64(%rdi)
	movq	%r9,72(%rdi)
	movq	%r10,%rbp
	movq	%r11,88(%rdi)

	movq	%rax,384(%rsp)

	movq	0(%rdi),%r8
	movq	8(%rdi),%r9
	movq	16(%rdi),%r10
	movq	24(%rdi),%r11








	addq	$80,%rdi

	addq	$64,%rsi
	leaq	296(%rsp),%rcx

	call	MULADD_128x512


	movq	384(%rsp),%rax


	addq	-16(%rdi),%r8
	adcq	-8(%rdi),%r9
	movq	%r8,64(%rcx)
	movq	%r9,72(%rcx)

	adcq	%rax,%rax
	movq	%rax,384(%rsp)

	leaq	192(%rsp),%rdi
	addq	$64,%rsi





	movq	(%rsi),%r8
	movq	8(%rsi),%rbx

	movq	(%rcx),%rax
	mulq	%r8
	movq	%rax,%rbp
	movq	%rdx,%r9

	movq	8(%rcx),%rax
	mulq	%r8
	addq	%rax,%r9

	movq	(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r9

	movq	%r9,8(%rdi)


	subq	$192,%rsi

	movq	(%rcx),%r8
	movq	8(%rcx),%r9

	call	MULADD_128x512





	movq	0(%rsi),%rax
	movq	8(%rsi),%rbx
	movq	16(%rsi),%rdi
	movq	24(%rsi),%rdx


	movq	384(%rsp),%rbp

	addq	64(%rcx),%r8
	adcq	72(%rcx),%r9


	adcq	%rbp,%rbp



	shlq	$3,%rbp
	movq	32(%rsp),%rcx
	addq	%rcx,%rbp


	xorq	%rsi,%rsi

	addq	0(%rbp),%r10
	adcq	64(%rbp),%r11
	adcq	128(%rbp),%r12
	adcq	192(%rbp),%r13
	adcq	256(%rbp),%r14
	adcq	320(%rbp),%r15
	adcq	384(%rbp),%r8
	adcq	448(%rbp),%r9



	sbbq	$0,%rsi


	andq	%rsi,%rax
	andq	%rsi,%rbx
	andq	%rsi,%rdi
	andq	%rsi,%rdx

	movq	$1,%rbp
	subq	%rax,%r10
	sbbq	%rbx,%r11
	sbbq	%rdi,%r12
	sbbq	%rdx,%r13




	sbbq	$0,%rbp



	addq	$512,%rcx
	movq	32(%rcx),%rax
	movq	40(%rcx),%rbx
	movq	48(%rcx),%rdi
	movq	56(%rcx),%rdx



	andq	%rsi,%rax
	andq	%rsi,%rbx
	andq	%rsi,%rdi
	andq	%rsi,%rdx



	subq	$1,%rbp

	sbbq	%rax,%r14
	sbbq	%rbx,%r15
	sbbq	%rdi,%r8
	sbbq	%rdx,%r9



	movq	144(%rsp),%rsi
	movq	%r10,0(%rsi)
	movq	%r11,8(%rsi)
	movq	%r12,16(%rsi)
	movq	%r13,24(%rsi)
	movq	%r14,32(%rsi)
	movq	%r15,40(%rsi)
	movq	%r8,48(%rsi)
	movq	%r9,56(%rsi)

	.byte	0xf3,0xc3
.size	mont_reduce,.-mont_reduce
.type	mont_mul_a3b,@function
.align	16
mont_mul_a3b:




	movq	0(%rdi),%rbp

	movq	%r10,%rax
	mulq	%rbp
	movq	%rax,520(%rsp)
	movq	%rdx,%r10
	movq	%r11,%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	movq	%rdx,%r11
	movq	%r12,%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	movq	%rdx,%r12
	movq	%r13,%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	movq	%rdx,%r13
	movq	%r14,%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	movq	%rdx,%r14
	movq	%r15,%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	movq	%rdx,%r15
	movq	%r8,%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	movq	%rdx,%r8
	movq	%r9,%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	movq	%rdx,%r9
	movq	8(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	movq	%r10,528(%rsp)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%r10
	movq	16(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	movq	%r11,536(%rsp)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%r11
	movq	24(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	movq	%r12,544(%rsp)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%r12
	movq	32(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	movq	%r13,552(%rsp)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%r13
	movq	40(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	movq	%r14,560(%rsp)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%r14
	movq	48(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	movq	%r15,568(%rsp)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	addq	%rbx,%r8
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%r15
	movq	56(%rdi),%rbp
	movq	0(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r8
	adcq	$0,%rdx
	movq	%r8,576(%rsp)
	movq	%rdx,%rbx

	movq	8(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r9
	adcq	$0,%rdx
	addq	%rbx,%r9
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	16(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r10
	adcq	$0,%rdx
	addq	%rbx,%r10
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	24(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%rbx,%r11
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	32(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%rbx,%r12
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	40(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%rbx,%r13
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	48(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%rbx,%r14
	adcq	$0,%rdx
	movq	%rdx,%rbx

	movq	56(%rsi),%rax
	mulq	%rbp
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%rbx,%r15
	adcq	$0,%rdx
	movq	%rdx,%r8
	movq	%r9,584(%rsp)
	movq	%r10,592(%rsp)
	movq	%r11,600(%rsp)
	movq	%r12,608(%rsp)
	movq	%r13,616(%rsp)
	movq	%r14,624(%rsp)
	movq	%r15,632(%rsp)
	movq	%r8,640(%rsp)





	jmp	mont_reduce


.size	mont_mul_a3b,.-mont_mul_a3b
.type	sqr_reduce,@function
.align	16
sqr_reduce:
	movq	16(%rsp),%rcx



	movq	%r10,%rbx

	movq	%r11,%rax
	mulq	%rbx
	movq	%rax,528(%rsp)
	movq	%rdx,%r10
	movq	%r12,%rax
	mulq	%rbx
	addq	%rax,%r10
	adcq	$0,%rdx
	movq	%rdx,%r11
	movq	%r13,%rax
	mulq	%rbx
	addq	%rax,%r11
	adcq	$0,%rdx
	movq	%rdx,%r12
	movq	%r14,%rax
	mulq	%rbx
	addq	%rax,%r12
	adcq	$0,%rdx
	movq	%rdx,%r13
	movq	%r15,%rax
	mulq	%rbx
	addq	%rax,%r13
	adcq	$0,%rdx
	movq	%rdx,%r14
	movq	%r8,%rax
	mulq	%rbx
	addq	%rax,%r14
	adcq	$0,%rdx
	movq	%rdx,%r15
	movq	%r9,%rax
	mulq	%rbx
	addq	%rax,%r15
	adcq	$0,%rdx
	movq	%rdx,%rsi

	movq	%r10,536(%rsp)





	movq	8(%rcx),%rbx

	movq	16(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r11
	adcq	$0,%rdx
	movq	%r11,544(%rsp)

	movq	%rdx,%r10
	movq	24(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%r10,%r12
	adcq	$0,%rdx
	movq	%r12,552(%rsp)

	movq	%rdx,%r10
	movq	32(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r13
	adcq	$0,%rdx
	addq	%r10,%r13
	adcq	$0,%rdx

	movq	%rdx,%r10
	movq	40(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%r10,%r14
	adcq	$0,%rdx

	movq	%rdx,%r10
	movq	%r8,%rax
	mulq	%rbx
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%r10,%r15
	adcq	$0,%rdx

	movq	%rdx,%r10
	movq	%r9,%rax
	mulq	%rbx
	addq	%rax,%rsi
	adcq	$0,%rdx
	addq	%r10,%rsi
	adcq	$0,%rdx

	movq	%rdx,%r11




	movq	16(%rcx),%rbx

	movq	24(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r13
	adcq	$0,%rdx
	movq	%r13,560(%rsp)

	movq	%rdx,%r10
	movq	32(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r14
	adcq	$0,%rdx
	addq	%r10,%r14
	adcq	$0,%rdx
	movq	%r14,568(%rsp)

	movq	%rdx,%r10
	movq	40(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%r10,%r15
	adcq	$0,%rdx

	movq	%rdx,%r10
	movq	%r8,%rax
	mulq	%rbx
	addq	%rax,%rsi
	adcq	$0,%rdx
	addq	%r10,%rsi
	adcq	$0,%rdx

	movq	%rdx,%r10
	movq	%r9,%rax
	mulq	%rbx
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%r10,%r11
	adcq	$0,%rdx

	movq	%rdx,%r12





	movq	24(%rcx),%rbx

	movq	32(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r15
	adcq	$0,%rdx
	movq	%r15,576(%rsp)

	movq	%rdx,%r10
	movq	40(%rcx),%rax
	mulq	%rbx
	addq	%rax,%rsi
	adcq	$0,%rdx
	addq	%r10,%rsi
	adcq	$0,%rdx
	movq	%rsi,584(%rsp)

	movq	%rdx,%r10
	movq	%r8,%rax
	mulq	%rbx
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%r10,%r11
	adcq	$0,%rdx

	movq	%rdx,%r10
	movq	%r9,%rax
	mulq	%rbx
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%r10,%r12
	adcq	$0,%rdx

	movq	%rdx,%r15




	movq	32(%rcx),%rbx

	movq	40(%rcx),%rax
	mulq	%rbx
	addq	%rax,%r11
	adcq	$0,%rdx
	movq	%r11,592(%rsp)

	movq	%rdx,%r10
	movq	%r8,%rax
	mulq	%rbx
	addq	%rax,%r12
	adcq	$0,%rdx
	addq	%r10,%r12
	adcq	$0,%rdx
	movq	%r12,600(%rsp)

	movq	%rdx,%r10
	movq	%r9,%rax
	mulq	%rbx
	addq	%rax,%r15
	adcq	$0,%rdx
	addq	%r10,%r15
	adcq	$0,%rdx

	movq	%rdx,%r11




	movq	40(%rcx),%rbx

	movq	%r8,%rax
	mulq	%rbx
	addq	%rax,%r15
	adcq	$0,%rdx
	movq	%r15,608(%rsp)

	movq	%rdx,%r10
	movq	%r9,%rax
	mulq	%rbx
	addq	%rax,%r11
	adcq	$0,%rdx
	addq	%r10,%r11
	adcq	$0,%rdx
	movq	%r11,616(%rsp)

	movq	%rdx,%r12




	movq	%r8,%rbx

	movq	%r9,%rax
	mulq	%rbx
	addq	%rax,%r12
	adcq	$0,%rdx
	movq	%r12,624(%rsp)

	movq	%rdx,632(%rsp)


	movq	528(%rsp),%r10
	movq	536(%rsp),%r11
	movq	544(%rsp),%r12
	movq	552(%rsp),%r13
	movq	560(%rsp),%r14
	movq	568(%rsp),%r15

	movq	24(%rcx),%rax
	mulq	%rax
	movq	%rax,%rdi
	movq	%rdx,%r8

	addq	%r10,%r10
	adcq	%r11,%r11
	adcq	%r12,%r12
	adcq	%r13,%r13
	adcq	%r14,%r14
	adcq	%r15,%r15
	adcq	$0,%r8

	movq	0(%rcx),%rax
	mulq	%rax
	movq	%rax,520(%rsp)
	movq	%rdx,%rbx

	movq	8(%rcx),%rax
	mulq	%rax

	addq	%rbx,%r10
	adcq	%rax,%r11
	adcq	$0,%rdx

	movq	%rdx,%rbx
	movq	%r10,528(%rsp)
	movq	%r11,536(%rsp)

	movq	16(%rcx),%rax
	mulq	%rax

	addq	%rbx,%r12
	adcq	%rax,%r13
	adcq	$0,%rdx

	movq	%rdx,%rbx

	movq	%r12,544(%rsp)
	movq	%r13,552(%rsp)

	xorq	%rbp,%rbp
	addq	%rbx,%r14
	adcq	%rdi,%r15
	adcq	$0,%rbp

	movq	%r14,560(%rsp)
	movq	%r15,568(%rsp)




	movq	576(%rsp),%r10
	movq	584(%rsp),%r11
	movq	592(%rsp),%r12
	movq	600(%rsp),%r13
	movq	608(%rsp),%r14
	movq	616(%rsp),%r15
	movq	624(%rsp),%rdi
	movq	632(%rsp),%rsi

	movq	%r9,%rax
	mulq	%rax
	movq	%rax,%r9
	movq	%rdx,%rbx

	addq	%r10,%r10
	adcq	%r11,%r11
	adcq	%r12,%r12
	adcq	%r13,%r13
	adcq	%r14,%r14
	adcq	%r15,%r15
	adcq	%rdi,%rdi
	adcq	%rsi,%rsi
	adcq	$0,%rbx

	addq	%rbp,%r10

	movq	32(%rcx),%rax
	mulq	%rax

	addq	%r8,%r10
	adcq	%rax,%r11
	adcq	$0,%rdx

	movq	%rdx,%rbp

	movq	%r10,576(%rsp)
	movq	%r11,584(%rsp)

	movq	40(%rcx),%rax
	mulq	%rax

	addq	%rbp,%r12
	adcq	%rax,%r13
	adcq	$0,%rdx

	movq	%rdx,%rbp

	movq	%r12,592(%rsp)
	movq	%r13,600(%rsp)

	movq	48(%rcx),%rax
	mulq	%rax

	addq	%rbp,%r14
	adcq	%rax,%r15
	adcq	$0,%rdx

	movq	%r14,608(%rsp)
	movq	%r15,616(%rsp)

	addq	%rdx,%rdi
	adcq	%r9,%rsi
	adcq	$0,%rbx

	movq	%rdi,624(%rsp)
	movq	%rsi,632(%rsp)
	movq	%rbx,640(%rsp)

	jmp	mont_reduce


.size	sqr_reduce,.-sqr_reduce
.globl	mod_exp_512
.type	mod_exp_512,@function
mod_exp_512:
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15


	movq	%rsp,%r8
	subq	$2688,%rsp
	andq	$-64,%rsp


	movq	%r8,0(%rsp)
	movq	%rdi,8(%rsp)
	movq	%rsi,16(%rsp)
	movq	%rcx,24(%rsp)
.Lbody:



	pxor	%xmm4,%xmm4
	movdqu	0(%rsi),%xmm0
	movdqu	16(%rsi),%xmm1
	movdqu	32(%rsi),%xmm2
	movdqu	48(%rsi),%xmm3
	movdqa	%xmm4,512(%rsp)
	movdqa	%xmm4,528(%rsp)
	movdqa	%xmm4,608(%rsp)
	movdqa	%xmm4,624(%rsp)
	movdqa	%xmm0,544(%rsp)
	movdqa	%xmm1,560(%rsp)
	movdqa	%xmm2,576(%rsp)
	movdqa	%xmm3,592(%rsp)


	movdqu	0(%rdx),%xmm0
	movdqu	16(%rdx),%xmm1
	movdqu	32(%rdx),%xmm2
	movdqu	48(%rdx),%xmm3

	leaq	384(%rsp),%rbx
	movq	%rbx,136(%rsp)
	call	mont_reduce


	leaq	448(%rsp),%rcx
	xorq	%rax,%rax
	movq	%rax,0(%rcx)
	movq	%rax,8(%rcx)
	movq	%rax,24(%rcx)
	movq	%rax,32(%rcx)
	movq	%rax,40(%rcx)
	movq	%rax,48(%rcx)
	movq	%rax,56(%rcx)
	movq	%rax,128(%rsp)
	movq	$1,16(%rcx)

	leaq	640(%rsp),%rbp
	movq	%rcx,%rsi
	movq	%rbp,%rdi
	movq	$8,%rax
loop_0:
	movq	(%rcx),%rbx
	movw	%bx,(%rdi)
	shrq	$16,%rbx
	movw	%bx,64(%rdi)
	shrq	$16,%rbx
	movw	%bx,128(%rdi)
	shrq	$16,%rbx
	movw	%bx,192(%rdi)
	leaq	8(%rcx),%rcx
	leaq	256(%rdi),%rdi
	decq	%rax
	jnz	loop_0
	movq	$31,%rax
	movq	%rax,32(%rsp)
	movq	%rbp,40(%rsp)

	movq	%rsi,136(%rsp)
	movq	0(%rsi),%r10
	movq	8(%rsi),%r11
	movq	16(%rsi),%r12
	movq	24(%rsi),%r13
	movq	32(%rsi),%r14
	movq	40(%rsi),%r15
	movq	48(%rsi),%r8
	movq	56(%rsi),%r9
init_loop:
	leaq	384(%rsp),%rdi
	call	mont_mul_a3b
	leaq	448(%rsp),%rsi
	movq	40(%rsp),%rbp
	addq	$2,%rbp
	movq	%rbp,40(%rsp)
	movq	%rsi,%rcx
	movq	$8,%rax
loop_1:
	movq	(%rcx),%rbx
	movw	%bx,(%rbp)
	shrq	$16,%rbx
	movw	%bx,64(%rbp)
	shrq	$16,%rbx
	movw	%bx,128(%rbp)
	shrq	$16,%rbx
	movw	%bx,192(%rbp)
	leaq	8(%rcx),%rcx
	leaq	256(%rbp),%rbp
	decq	%rax
	jnz	loop_1
	movq	32(%rsp),%rax
	subq	$1,%rax
	movq	%rax,32(%rsp)
	jne	init_loop



	movdqa	%xmm0,64(%rsp)
	movdqa	%xmm1,80(%rsp)
	movdqa	%xmm2,96(%rsp)
	movdqa	%xmm3,112(%rsp)





	movl	126(%rsp),%eax
	movq	%rax,%rdx
	shrq	$11,%rax
	andl	$2047,%edx
	movl	%edx,126(%rsp)
	leaq	640(%rsp,%rax,2),%rsi
	movq	8(%rsp),%rdx
	movq	$4,%rbp
loop_2:
	movzwq	192(%rsi),%rbx
	movzwq	448(%rsi),%rax
	shlq	$16,%rbx
	shlq	$16,%rax
	movw	128(%rsi),%bx
	movw	384(%rsi),%ax
	shlq	$16,%rbx
	shlq	$16,%rax
	movw	64(%rsi),%bx
	movw	320(%rsi),%ax
	shlq	$16,%rbx
	shlq	$16,%rax
	movw	0(%rsi),%bx
	movw	256(%rsi),%ax
	movq	%rbx,0(%rdx)
	movq	%rax,8(%rdx)
	leaq	512(%rsi),%rsi
	leaq	16(%rdx),%rdx
	subq	$1,%rbp
	jnz	loop_2
	movq	$505,48(%rsp)

	movq	8(%rsp),%rcx
	movq	%rcx,136(%rsp)
	movq	0(%rcx),%r10
	movq	8(%rcx),%r11
	movq	16(%rcx),%r12
	movq	24(%rcx),%r13
	movq	32(%rcx),%r14
	movq	40(%rcx),%r15
	movq	48(%rcx),%r8
	movq	56(%rcx),%r9
	jmp	sqr_2

main_loop_a3b:
	call	sqr_reduce
	call	sqr_reduce
	call	sqr_reduce
sqr_2:
	call	sqr_reduce
	call	sqr_reduce



	movq	48(%rsp),%rcx
	movq	%rcx,%rax
	shrq	$4,%rax
	movl	64(%rsp,%rax,2),%edx
	andq	$15,%rcx
	shrq	%cl,%rdx
	andq	$31,%rdx

	leaq	640(%rsp,%rdx,2),%rsi
	leaq	448(%rsp),%rdx
	movq	%rdx,%rdi
	movq	$4,%rbp
loop_3:
	movzwq	192(%rsi),%rbx
	movzwq	448(%rsi),%rax
	shlq	$16,%rbx
	shlq	$16,%rax
	movw	128(%rsi),%bx
	movw	384(%rsi),%ax
	shlq	$16,%rbx
	shlq	$16,%rax
	movw	64(%rsi),%bx
	movw	320(%rsi),%ax
	shlq	$16,%rbx
	shlq	$16,%rax
	movw	0(%rsi),%bx
	movw	256(%rsi),%ax
	movq	%rbx,0(%rdx)
	movq	%rax,8(%rdx)
	leaq	512(%rsi),%rsi
	leaq	16(%rdx),%rdx
	subq	$1,%rbp
	jnz	loop_3
	movq	8(%rsp),%rsi
	call	mont_mul_a3b



	movq	48(%rsp),%rcx
	subq	$5,%rcx
	movq	%rcx,48(%rsp)
	jge	main_loop_a3b



end_main_loop_a3b:


	movq	8(%rsp),%rdx
	pxor	%xmm4,%xmm4
	movdqu	0(%rdx),%xmm0
	movdqu	16(%rdx),%xmm1
	movdqu	32(%rdx),%xmm2
	movdqu	48(%rdx),%xmm3
	movdqa	%xmm4,576(%rsp)
	movdqa	%xmm4,592(%rsp)
	movdqa	%xmm4,608(%rsp)
	movdqa	%xmm4,624(%rsp)
	movdqa	%xmm0,512(%rsp)
	movdqa	%xmm1,528(%rsp)
	movdqa	%xmm2,544(%rsp)
	movdqa	%xmm3,560(%rsp)
	call	mont_reduce



	movq	8(%rsp),%rax
	movq	0(%rax),%r8
	movq	8(%rax),%r9
	movq	16(%rax),%r10
	movq	24(%rax),%r11
	movq	32(%rax),%r12
	movq	40(%rax),%r13
	movq	48(%rax),%r14
	movq	56(%rax),%r15


	movq	24(%rsp),%rbx
	addq	$512,%rbx

	subq	0(%rbx),%r8
	sbbq	8(%rbx),%r9
	sbbq	16(%rbx),%r10
	sbbq	24(%rbx),%r11
	sbbq	32(%rbx),%r12
	sbbq	40(%rbx),%r13
	sbbq	48(%rbx),%r14
	sbbq	56(%rbx),%r15


	movq	0(%rax),%rsi
	movq	8(%rax),%rdi
	movq	16(%rax),%rcx
	movq	24(%rax),%rdx
	cmovncq	%r8,%rsi
	cmovncq	%r9,%rdi
	cmovncq	%r10,%rcx
	cmovncq	%r11,%rdx
	movq	%rsi,0(%rax)
	movq	%rdi,8(%rax)
	movq	%rcx,16(%rax)
	movq	%rdx,24(%rax)

	movq	32(%rax),%rsi
	movq	40(%rax),%rdi
	movq	48(%rax),%rcx
	movq	56(%rax),%rdx
	cmovncq	%r12,%rsi
	cmovncq	%r13,%rdi
	cmovncq	%r14,%rcx
	cmovncq	%r15,%rdx
	movq	%rsi,32(%rax)
	movq	%rdi,40(%rax)
	movq	%rcx,48(%rax)
	movq	%rdx,56(%rax)

	movq	0(%rsp),%rsi
	movq	0(%rsi),%r15
	movq	8(%rsi),%r14
	movq	16(%rsi),%r13
	movq	24(%rsi),%r12
	movq	32(%rsi),%rbx
	movq	40(%rsi),%rbp
	leaq	48(%rsi),%rsp
.Lepilogue:
	.byte	0xf3,0xc3
.size	mod_exp_512, . - mod_exp_512
