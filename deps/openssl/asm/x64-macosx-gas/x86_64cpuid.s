
.private_extern	_OPENSSL_cpuid_setup
.mod_init_func
	.p2align	3
	.quad	_OPENSSL_cpuid_setup

.private_extern	_OPENSSL_ia32cap_P
.comm	_OPENSSL_ia32cap_P,8,2

.text


.globl	_OPENSSL_atomic_add

.p2align	4
_OPENSSL_atomic_add:
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
	rdtsc
	shlq	$32,%rdx
	orq	%rdx,%rax
	.byte	0xf3,0xc3


.globl	_OPENSSL_ia32_cpuid

.p2align	4
_OPENSSL_ia32_cpuid:
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
	jz	L$intel

	cmpl	$1752462657,%ebx
	setne	%al
	movl	%eax,%r10d
	cmpl	$1769238117,%edx
	setne	%al
	orl	%eax,%r10d
	cmpl	$1145913699,%ecx
	setne	%al
	orl	%eax,%r10d
	jnz	L$intel


	movl	$2147483648,%eax
	cpuid
	cmpl	$2147483649,%eax
	jb	L$intel
	movl	%eax,%r10d
	movl	$2147483649,%eax
	cpuid
	orl	%ecx,%r9d
	andl	$2049,%r9d

	cmpl	$2147483656,%r10d
	jb	L$intel

	movl	$2147483656,%eax
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
	andl	$4026531839,%edx
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
	andl	$4095,%r10d

L$nocacheinfo:
	movl	$1,%eax
	cpuid
	andl	$3220176895,%edx
	cmpl	$0,%r9d
	jne	L$notintel
	orl	$1073741824,%edx
	andb	$15,%ah
	cmpb	$15,%ah
	jne	L$notintel
	orl	$1048576,%edx
L$notintel:
	btl	$28,%edx
	jnc	L$generic
	andl	$4026531839,%edx
	cmpl	$0,%r10d
	je	L$generic

	orl	$268435456,%edx
	shrl	$16,%ebx
	cmpb	$1,%bl
	ja	L$generic
	andl	$4026531839,%edx
L$generic:
	andl	$2048,%r9d
	andl	$4294965247,%ecx
	orl	%ecx,%r9d

	movl	%edx,%r10d
	btl	$27,%r9d
	jnc	L$clear_avx
	xorl	%ecx,%ecx
.byte	0x0f,0x01,0xd0

	andl	$6,%eax
	cmpl	$6,%eax
	je	L$done
L$clear_avx:
	movl	$4026525695,%eax
	andl	%eax,%r9d
L$done:
	shlq	$32,%r9
	movl	%r10d,%eax
	movq	%r8,%rbx
	orq	%r9,%rax
	.byte	0xf3,0xc3


.globl	_OPENSSL_cleanse

.p2align	4
_OPENSSL_cleanse:
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

.globl	_OPENSSL_wipe_cpu

.p2align	4
_OPENSSL_wipe_cpu:
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

.globl	_OPENSSL_ia32_rdrand

.p2align	4
_OPENSSL_ia32_rdrand:
	movl	$8,%ecx
L$oop_rdrand:
.byte	72,15,199,240
	jc	L$break_rdrand
	loop	L$oop_rdrand
L$break_rdrand:
	cmpq	$0,%rax
	cmoveq	%rcx,%rax
	.byte	0xf3,0xc3
