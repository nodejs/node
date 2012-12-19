
.hidden	OPENSSL_cpuid_setup
.section	.init
	call	OPENSSL_cpuid_setup

.hidden	OPENSSL_ia32cap_P
.comm	OPENSSL_ia32cap_P,8,4

.text


.globl	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,@function
.align	16
OPENSSL_atomic_add:
	movl	(%rdi),%eax
.Lspin:	leaq	(%rsi,%rax,1),%r8
.byte	0xf0

	cmpxchgl	%r8d,(%rdi)
	jne	.Lspin
	movl	%r8d,%eax
.byte	0x48,0x98

	.byte	0xf3,0xc3
.size	OPENSSL_atomic_add,.-OPENSSL_atomic_add

.globl	OPENSSL_rdtsc
.type	OPENSSL_rdtsc,@function
.align	16
OPENSSL_rdtsc:
	rdtsc
	shlq	$32,%rdx
	orq	%rdx,%rax
	.byte	0xf3,0xc3
.size	OPENSSL_rdtsc,.-OPENSSL_rdtsc

.globl	OPENSSL_ia32_cpuid
.type	OPENSSL_ia32_cpuid,@function
.align	16
OPENSSL_ia32_cpuid:
	movq	%rbx,%r8

	xorl	%eax,%eax
	cpuid
	movl	%eax,%r11d

	xorl	%eax,%eax
	cmpl	$1970169159,%ebx
	setne	%al
	movl	%eax,%r9d
	cmpl	$1231384169,%edx
	setne	%al
	orl	%eax,%r9d
	cmpl	$1818588270,%ecx
	setne	%al
	orl	%eax,%r9d
	jz	.Lintel

	cmpl	$1752462657,%ebx
	setne	%al
	movl	%eax,%r10d
	cmpl	$1769238117,%edx
	setne	%al
	orl	%eax,%r10d
	cmpl	$1145913699,%ecx
	setne	%al
	orl	%eax,%r10d
	jnz	.Lintel


	movl	$2147483648,%eax
	cpuid
	cmpl	$2147483649,%eax
	jb	.Lintel
	movl	%eax,%r10d
	movl	$2147483649,%eax
	cpuid
	orl	%ecx,%r9d
	andl	$2049,%r9d

	cmpl	$2147483656,%r10d
	jb	.Lintel

	movl	$2147483656,%eax
	cpuid
	movzbq	%cl,%r10
	incq	%r10

	movl	$1,%eax
	cpuid
	btl	$28,%edx
	jnc	.Lgeneric
	shrl	$16,%ebx
	cmpb	%r10b,%bl
	ja	.Lgeneric
	andl	$4026531839,%edx
	jmp	.Lgeneric

.Lintel:
	cmpl	$4,%r11d
	movl	$-1,%r10d
	jb	.Lnocacheinfo

	movl	$4,%eax
	movl	$0,%ecx
	cpuid
	movl	%eax,%r10d
	shrl	$14,%r10d
	andl	$4095,%r10d

.Lnocacheinfo:
	movl	$1,%eax
	cpuid
	andl	$3220176895,%edx
	cmpl	$0,%r9d
	jne	.Lnotintel
	orl	$1073741824,%edx
	andb	$15,%ah
	cmpb	$15,%ah
	jne	.Lnotintel
	orl	$1048576,%edx
.Lnotintel:
	btl	$28,%edx
	jnc	.Lgeneric
	andl	$4026531839,%edx
	cmpl	$0,%r10d
	je	.Lgeneric

	orl	$268435456,%edx
	shrl	$16,%ebx
	cmpb	$1,%bl
	ja	.Lgeneric
	andl	$4026531839,%edx
.Lgeneric:
	andl	$2048,%r9d
	andl	$4294965247,%ecx
	orl	%ecx,%r9d

	movl	%edx,%r10d
	btl	$27,%r9d
	jnc	.Lclear_avx
	xorl	%ecx,%ecx
.byte	0x0f,0x01,0xd0

	andl	$6,%eax
	cmpl	$6,%eax
	je	.Ldone
.Lclear_avx:
	movl	$4026525695,%eax
	andl	%eax,%r9d
.Ldone:
	shlq	$32,%r9
	movl	%r10d,%eax
	movq	%r8,%rbx
	orq	%r9,%rax
	.byte	0xf3,0xc3
.size	OPENSSL_ia32_cpuid,.-OPENSSL_ia32_cpuid

.globl	OPENSSL_cleanse
.type	OPENSSL_cleanse,@function
.align	16
OPENSSL_cleanse:
	xorq	%rax,%rax
	cmpq	$15,%rsi
	jae	.Lot
	cmpq	$0,%rsi
	je	.Lret
.Little:
	movb	%al,(%rdi)
	subq	$1,%rsi
	leaq	1(%rdi),%rdi
	jnz	.Little
.Lret:
	.byte	0xf3,0xc3
.align	16
.Lot:
	testq	$7,%rdi
	jz	.Laligned
	movb	%al,(%rdi)
	leaq	-1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	jmp	.Lot
.Laligned:
	movq	%rax,(%rdi)
	leaq	-8(%rsi),%rsi
	testq	$-8,%rsi
	leaq	8(%rdi),%rdi
	jnz	.Laligned
	cmpq	$0,%rsi
	jne	.Little
	.byte	0xf3,0xc3
.size	OPENSSL_cleanse,.-OPENSSL_cleanse
.globl	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,@function
.align	16
OPENSSL_wipe_cpu:
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
.size	OPENSSL_wipe_cpu,.-OPENSSL_wipe_cpu
.globl	OPENSSL_ia32_rdrand
.type	OPENSSL_ia32_rdrand,@function
.align	16
OPENSSL_ia32_rdrand:
	movl	$8,%ecx
.Loop_rdrand:
.byte	72,15,199,240
	jc	.Lbreak_rdrand
	loop	.Loop_rdrand
.Lbreak_rdrand:
	cmpq	$0,%rax
	cmoveq	%rcx,%rax
	.byte	0xf3,0xc3
.size	OPENSSL_ia32_rdrand,.-OPENSSL_ia32_rdrand
