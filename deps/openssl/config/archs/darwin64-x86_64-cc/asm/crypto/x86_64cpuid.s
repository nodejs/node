
.private_extern	_OPENSSL_cpuid_setup
.mod_init_func
	.p2align	3
	.quad	_OPENSSL_cpuid_setup

.private_extern	_OPENSSL_ia32cap_P
.comm	_OPENSSL_ia32cap_P,16,2

.text	

.globl	_OPENSSL_atomic_add

.p2align	4
_OPENSSL_atomic_add:

.byte	243,15,30,250
	movl	(%rdi),%eax
L$spin:	leaq	(%rsi,%rax,1),%r8
.byte	0xf0
	cmpxchgl	%r8d,(%rdi)
	jne	L$spin
	movl	%r8d,%eax
.byte	0x48,0x98
	.byte	0xf3,0xc3



.globl	_OPENSSL_rdtsc

.p2align	4
_OPENSSL_rdtsc:

.byte	243,15,30,250
	rdtsc
	shlq	$32,%rdx
	orq	%rdx,%rax
	.byte	0xf3,0xc3



.globl	_OPENSSL_ia32_cpuid

.p2align	4
_OPENSSL_ia32_cpuid:

.byte	243,15,30,250
	movq	%rbx,%r8


	xorl	%eax,%eax
	movq	%rax,8(%rdi)
	cpuid
	movl	%eax,%r11d

	xorl	%eax,%eax
	cmpl	$0x756e6547,%ebx
	setne	%al
	movl	%eax,%r9d
	cmpl	$0x49656e69,%edx
	setne	%al
	orl	%eax,%r9d
	cmpl	$0x6c65746e,%ecx
	setne	%al
	orl	%eax,%r9d
	jz	L$intel

	cmpl	$0x68747541,%ebx
	setne	%al
	movl	%eax,%r10d
	cmpl	$0x69746E65,%edx
	setne	%al
	orl	%eax,%r10d
	cmpl	$0x444D4163,%ecx
	setne	%al
	orl	%eax,%r10d
	jnz	L$intel


	movl	$0x80000000,%eax
	cpuid
	cmpl	$0x80000001,%eax
	jb	L$intel
	movl	%eax,%r10d
	movl	$0x80000001,%eax
	cpuid
	orl	%ecx,%r9d
	andl	$0x00000801,%r9d

	cmpl	$0x80000008,%r10d
	jb	L$intel

	movl	$0x80000008,%eax
	cpuid
	movzbq	%cl,%r10
	incq	%r10

	movl	$1,%eax
	cpuid
	btl	$28,%edx
	jnc	L$generic
	shrl	$16,%ebx
	cmpb	%r10b,%bl
	ja	L$generic
	andl	$0xefffffff,%edx
	jmp	L$generic

L$intel:
	cmpl	$4,%r11d
	movl	$-1,%r10d
	jb	L$nocacheinfo

	movl	$4,%eax
	movl	$0,%ecx
	cpuid
	movl	%eax,%r10d
	shrl	$14,%r10d
	andl	$0xfff,%r10d

L$nocacheinfo:
	movl	$1,%eax
	cpuid
	movd	%eax,%xmm0
	andl	$0xbfefffff,%edx
	cmpl	$0,%r9d
	jne	L$notintel
	orl	$0x40000000,%edx
	andb	$15,%ah
	cmpb	$15,%ah
	jne	L$notP4
	orl	$0x00100000,%edx
L$notP4:
	cmpb	$6,%ah
	jne	L$notintel
	andl	$0x0fff0ff0,%eax
	cmpl	$0x00050670,%eax
	je	L$knights
	cmpl	$0x00080650,%eax
	jne	L$notintel
L$knights:
	andl	$0xfbffffff,%ecx

L$notintel:
	btl	$28,%edx
	jnc	L$generic
	andl	$0xefffffff,%edx
	cmpl	$0,%r10d
	je	L$generic

	orl	$0x10000000,%edx
	shrl	$16,%ebx
	cmpb	$1,%bl
	ja	L$generic
	andl	$0xefffffff,%edx
L$generic:
	andl	$0x00000800,%r9d
	andl	$0xfffff7ff,%ecx
	orl	%ecx,%r9d

	movl	%edx,%r10d

	cmpl	$7,%r11d
	jb	L$no_extended_info
	movl	$7,%eax
	xorl	%ecx,%ecx
	cpuid
	btl	$26,%r9d
	jc	L$notknights
	andl	$0xfff7ffff,%ebx
L$notknights:
	movd	%xmm0,%eax
	andl	$0x0fff0ff0,%eax
	cmpl	$0x00050650,%eax
	jne	L$notskylakex
	andl	$0xfffeffff,%ebx

L$notskylakex:
	movl	%ebx,8(%rdi)
	movl	%ecx,12(%rdi)
L$no_extended_info:

	btl	$27,%r9d
	jnc	L$clear_avx
	xorl	%ecx,%ecx
.byte	0x0f,0x01,0xd0
	andl	$0xe6,%eax
	cmpl	$0xe6,%eax
	je	L$done
	andl	$0x3fdeffff,8(%rdi)




	andl	$6,%eax
	cmpl	$6,%eax
	je	L$done
L$clear_avx:
	movl	$0xefffe7ff,%eax
	andl	%eax,%r9d
	movl	$0x3fdeffdf,%eax
	andl	%eax,8(%rdi)
L$done:
	shlq	$32,%r9
	movl	%r10d,%eax
	movq	%r8,%rbx

	orq	%r9,%rax
	.byte	0xf3,0xc3



.globl	_OPENSSL_cleanse

.p2align	4
_OPENSSL_cleanse:

.byte	243,15,30,250
	xorq	%rax,%rax
	cmpq	$15,%rsi
	jae	L$ot
	cmpq	$0,%rsi
	je	L$ret
L$ittle:
	movb	%al,(%rdi)
	subq	$1,%rsi
	leaq	1(%rdi),%rdi
	jnz	L$ittle
L$ret:
	.byte	0xf3,0xc3
.p2align	4
L$ot:
	testq	$7,%rdi
	jz	L$aligned
	movb	%al,(%rdi)
	leaq	-1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	jmp	L$ot
L$aligned:
	movq	%rax,(%rdi)
	leaq	-8(%rsi),%rsi
	testq	$-8,%rsi
	leaq	8(%rdi),%rdi
	jnz	L$aligned
	cmpq	$0,%rsi
	jne	L$ittle
	.byte	0xf3,0xc3



.globl	_CRYPTO_memcmp

.p2align	4
_CRYPTO_memcmp:

.byte	243,15,30,250
	xorq	%rax,%rax
	xorq	%r10,%r10
	cmpq	$0,%rdx
	je	L$no_data
	cmpq	$16,%rdx
	jne	L$oop_cmp
	movq	(%rdi),%r10
	movq	8(%rdi),%r11
	movq	$1,%rdx
	xorq	(%rsi),%r10
	xorq	8(%rsi),%r11
	orq	%r11,%r10
	cmovnzq	%rdx,%rax
	.byte	0xf3,0xc3

.p2align	4
L$oop_cmp:
	movb	(%rdi),%r10b
	leaq	1(%rdi),%rdi
	xorb	(%rsi),%r10b
	leaq	1(%rsi),%rsi
	orb	%r10b,%al
	decq	%rdx
	jnz	L$oop_cmp
	negq	%rax
	shrq	$63,%rax
L$no_data:
	.byte	0xf3,0xc3


.globl	_OPENSSL_wipe_cpu

.p2align	4
_OPENSSL_wipe_cpu:

.byte	243,15,30,250
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7
	pxor	%xmm8,%xmm8
	pxor	%xmm9,%xmm9
	pxor	%xmm10,%xmm10
	pxor	%xmm11,%xmm11
	pxor	%xmm12,%xmm12
	pxor	%xmm13,%xmm13
	pxor	%xmm14,%xmm14
	pxor	%xmm15,%xmm15
	xorq	%rcx,%rcx
	xorq	%rdx,%rdx
	xorq	%rsi,%rsi
	xorq	%rdi,%rdi
	xorq	%r8,%r8
	xorq	%r9,%r9
	xorq	%r10,%r10
	xorq	%r11,%r11
	leaq	8(%rsp),%rax
	.byte	0xf3,0xc3


.globl	_OPENSSL_instrument_bus

.p2align	4
_OPENSSL_instrument_bus:

.byte	243,15,30,250
	movq	%rdi,%r10
	movq	%rsi,%rcx
	movq	%rsi,%r11

	rdtsc
	movl	%eax,%r8d
	movl	$0,%r9d
	clflush	(%r10)
.byte	0xf0
	addl	%r9d,(%r10)
	jmp	L$oop
.p2align	4
L$oop:	rdtsc
	movl	%eax,%edx
	subl	%r8d,%eax
	movl	%edx,%r8d
	movl	%eax,%r9d
	clflush	(%r10)
.byte	0xf0
	addl	%eax,(%r10)
	leaq	4(%r10),%r10
	subq	$1,%rcx
	jnz	L$oop

	movq	%r11,%rax
	.byte	0xf3,0xc3



.globl	_OPENSSL_instrument_bus2

.p2align	4
_OPENSSL_instrument_bus2:

.byte	243,15,30,250
	movq	%rdi,%r10
	movq	%rsi,%rcx
	movq	%rdx,%r11
	movq	%rcx,8(%rsp)

	rdtsc
	movl	%eax,%r8d
	movl	$0,%r9d

	clflush	(%r10)
.byte	0xf0
	addl	%r9d,(%r10)

	rdtsc
	movl	%eax,%edx
	subl	%r8d,%eax
	movl	%edx,%r8d
	movl	%eax,%r9d
L$oop2:
	clflush	(%r10)
.byte	0xf0
	addl	%eax,(%r10)

	subq	$1,%r11
	jz	L$done2

	rdtsc
	movl	%eax,%edx
	subl	%r8d,%eax
	movl	%edx,%r8d
	cmpl	%r9d,%eax
	movl	%eax,%r9d
	movl	$0,%edx
	setne	%dl
	subq	%rdx,%rcx
	leaq	(%r10,%rdx,4),%r10
	jnz	L$oop2

L$done2:
	movq	8(%rsp),%rax
	subq	%rcx,%rax
	.byte	0xf3,0xc3


.globl	_OPENSSL_ia32_rdrand_bytes

.p2align	4
_OPENSSL_ia32_rdrand_bytes:

.byte	243,15,30,250
	xorq	%rax,%rax
	cmpq	$0,%rsi
	je	L$done_rdrand_bytes

	movq	$8,%r11
L$oop_rdrand_bytes:
.byte	73,15,199,242
	jc	L$break_rdrand_bytes
	decq	%r11
	jnz	L$oop_rdrand_bytes
	jmp	L$done_rdrand_bytes

.p2align	4
L$break_rdrand_bytes:
	cmpq	$8,%rsi
	jb	L$tail_rdrand_bytes
	movq	%r10,(%rdi)
	leaq	8(%rdi),%rdi
	addq	$8,%rax
	subq	$8,%rsi
	jz	L$done_rdrand_bytes
	movq	$8,%r11
	jmp	L$oop_rdrand_bytes

.p2align	4
L$tail_rdrand_bytes:
	movb	%r10b,(%rdi)
	leaq	1(%rdi),%rdi
	incq	%rax
	shrq	$8,%r10
	decq	%rsi
	jnz	L$tail_rdrand_bytes

L$done_rdrand_bytes:
	xorq	%r10,%r10
	.byte	0xf3,0xc3


.globl	_OPENSSL_ia32_rdseed_bytes

.p2align	4
_OPENSSL_ia32_rdseed_bytes:

.byte	243,15,30,250
	xorq	%rax,%rax
	cmpq	$0,%rsi
	je	L$done_rdseed_bytes

	movq	$8,%r11
L$oop_rdseed_bytes:
.byte	73,15,199,250
	jc	L$break_rdseed_bytes
	decq	%r11
	jnz	L$oop_rdseed_bytes
	jmp	L$done_rdseed_bytes

.p2align	4
L$break_rdseed_bytes:
	cmpq	$8,%rsi
	jb	L$tail_rdseed_bytes
	movq	%r10,(%rdi)
	leaq	8(%rdi),%rdi
	addq	$8,%rax
	subq	$8,%rsi
	jz	L$done_rdseed_bytes
	movq	$8,%r11
	jmp	L$oop_rdseed_bytes

.p2align	4
L$tail_rdseed_bytes:
	movb	%r10b,(%rdi)
	leaq	1(%rdi),%rdi
	incq	%rax
	shrq	$8,%r10
	decq	%rsi
	jnz	L$tail_rdseed_bytes

L$done_rdseed_bytes:
	xorq	%r10,%r10
	.byte	0xf3,0xc3


