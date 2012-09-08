.file	"x86cpuid.s"
.text
.globl	OPENSSL_ia32_cpuid
.type	OPENSSL_ia32_cpuid,@function
.align	16
OPENSSL_ia32_cpuid:
.L_OPENSSL_ia32_cpuid_begin:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	xorl	%edx,%edx
	pushfl
	popl	%eax
	movl	%eax,%ecx
	xorl	$2097152,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%eax,%ecx
	btl	$21,%ecx
	jnc	.L000done
	xorl	%eax,%eax
	.byte	0x0f,0xa2
	movl	%eax,%edi
	xorl	%eax,%eax
	cmpl	$1970169159,%ebx
	setne	%al
	movl	%eax,%ebp
	cmpl	$1231384169,%edx
	setne	%al
	orl	%eax,%ebp
	cmpl	$1818588270,%ecx
	setne	%al
	orl	%eax,%ebp
	jz	.L001intel
	cmpl	$1752462657,%ebx
	setne	%al
	movl	%eax,%esi
	cmpl	$1769238117,%edx
	setne	%al
	orl	%eax,%esi
	cmpl	$1145913699,%ecx
	setne	%al
	orl	%eax,%esi
	jnz	.L001intel
	movl	$2147483648,%eax
	.byte	0x0f,0xa2
	cmpl	$2147483656,%eax
	jb	.L001intel
	movl	$2147483656,%eax
	.byte	0x0f,0xa2
	movzbl	%cl,%esi
	incl	%esi
	movl	$1,%eax
	.byte	0x0f,0xa2
	btl	$28,%edx
	jnc	.L000done
	shrl	$16,%ebx
	andl	$255,%ebx
	cmpl	%esi,%ebx
	ja	.L000done
	andl	$4026531839,%edx
	jmp	.L000done
.L001intel:
	cmpl	$4,%edi
	movl	$-1,%edi
	jb	.L002nocacheinfo
	movl	$4,%eax
	movl	$0,%ecx
	.byte	0x0f,0xa2
	movl	%eax,%edi
	shrl	$14,%edi
	andl	$4095,%edi
.L002nocacheinfo:
	movl	$1,%eax
	.byte	0x0f,0xa2
	cmpl	$0,%ebp
	jne	.L003notP4
	andb	$15,%ah
	cmpb	$15,%ah
	jne	.L003notP4
	orl	$1048576,%edx
.L003notP4:
	btl	$28,%edx
	jnc	.L000done
	andl	$4026531839,%edx
	cmpl	$0,%edi
	je	.L000done
	orl	$268435456,%edx
	shrl	$16,%ebx
	cmpb	$1,%bl
	ja	.L000done
	andl	$4026531839,%edx
.L000done:
	movl	%edx,%eax
	movl	%ecx,%edx
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.size	OPENSSL_ia32_cpuid,.-.L_OPENSSL_ia32_cpuid_begin
.globl	OPENSSL_rdtsc
.type	OPENSSL_rdtsc,@function
.align	16
OPENSSL_rdtsc:
.L_OPENSSL_rdtsc_begin:
	xorl	%eax,%eax
	xorl	%edx,%edx
	leal	OPENSSL_ia32cap_P,%ecx
	btl	$4,(%ecx)
	jnc	.L004notsc
	.byte	0x0f,0x31
.L004notsc:
	ret
.size	OPENSSL_rdtsc,.-.L_OPENSSL_rdtsc_begin
.globl	OPENSSL_instrument_halt
.type	OPENSSL_instrument_halt,@function
.align	16
OPENSSL_instrument_halt:
.L_OPENSSL_instrument_halt_begin:
	leal	OPENSSL_ia32cap_P,%ecx
	btl	$4,(%ecx)
	jnc	.L005nohalt
.long	2421723150
	andl	$3,%eax
	jnz	.L005nohalt
	pushfl
	popl	%eax
	btl	$9,%eax
	jnc	.L005nohalt
	.byte	0x0f,0x31
	pushl	%edx
	pushl	%eax
	hlt
	.byte	0x0f,0x31
	subl	(%esp),%eax
	sbbl	4(%esp),%edx
	addl	$8,%esp
	ret
.L005nohalt:
	xorl	%eax,%eax
	xorl	%edx,%edx
	ret
.size	OPENSSL_instrument_halt,.-.L_OPENSSL_instrument_halt_begin
.globl	OPENSSL_far_spin
.type	OPENSSL_far_spin,@function
.align	16
OPENSSL_far_spin:
.L_OPENSSL_far_spin_begin:
	pushfl
	popl	%eax
	btl	$9,%eax
	jnc	.L006nospin
	movl	4(%esp),%eax
	movl	8(%esp),%ecx
.long	2430111262
	xorl	%eax,%eax
	movl	(%ecx),%edx
	jmp	.L007spin
.align	16
.L007spin:
	incl	%eax
	cmpl	(%ecx),%edx
	je	.L007spin
.long	529567888
	ret
.L006nospin:
	xorl	%eax,%eax
	xorl	%edx,%edx
	ret
.size	OPENSSL_far_spin,.-.L_OPENSSL_far_spin_begin
.globl	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,@function
.align	16
OPENSSL_wipe_cpu:
.L_OPENSSL_wipe_cpu_begin:
	xorl	%eax,%eax
	xorl	%edx,%edx
	leal	OPENSSL_ia32cap_P,%ecx
	movl	(%ecx),%ecx
	btl	$1,(%ecx)
	jnc	.L008no_x87
.long	4007259865,4007259865,4007259865,4007259865,2430851995
.L008no_x87:
	leal	4(%esp),%eax
	ret
.size	OPENSSL_wipe_cpu,.-.L_OPENSSL_wipe_cpu_begin
.globl	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,@function
.align	16
OPENSSL_atomic_add:
.L_OPENSSL_atomic_add_begin:
	movl	4(%esp),%edx
	movl	8(%esp),%ecx
	pushl	%ebx
	nop
	movl	(%edx),%eax
.L009spin:
	leal	(%eax,%ecx,1),%ebx
	nop
.long	447811568
	jne	.L009spin
	movl	%ebx,%eax
	popl	%ebx
	ret
.size	OPENSSL_atomic_add,.-.L_OPENSSL_atomic_add_begin
.globl	OPENSSL_indirect_call
.type	OPENSSL_indirect_call,@function
.align	16
OPENSSL_indirect_call:
.L_OPENSSL_indirect_call_begin:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$28,%esp
	movl	12(%ebp),%ecx
	movl	%ecx,(%esp)
	movl	16(%ebp),%edx
	movl	%edx,4(%esp)
	movl	20(%ebp),%eax
	movl	%eax,8(%esp)
	movl	24(%ebp),%eax
	movl	%eax,12(%esp)
	movl	28(%ebp),%eax
	movl	%eax,16(%esp)
	movl	32(%ebp),%eax
	movl	%eax,20(%esp)
	movl	36(%ebp),%eax
	movl	%eax,24(%esp)
	call	*8(%ebp)
	movl	%ebp,%esp
	popl	%ebp
	ret
.size	OPENSSL_indirect_call,.-.L_OPENSSL_indirect_call_begin
.globl	OPENSSL_cleanse
.type	OPENSSL_cleanse,@function
.align	16
OPENSSL_cleanse:
.L_OPENSSL_cleanse_begin:
	movl	4(%esp),%edx
	movl	8(%esp),%ecx
	xorl	%eax,%eax
	cmpl	$7,%ecx
	jae	.L010lot
	cmpl	$0,%ecx
	je	.L011ret
.L012little:
	movb	%al,(%edx)
	subl	$1,%ecx
	leal	1(%edx),%edx
	jnz	.L012little
.L011ret:
	ret
.align	16
.L010lot:
	testl	$3,%edx
	jz	.L013aligned
	movb	%al,(%edx)
	leal	-1(%ecx),%ecx
	leal	1(%edx),%edx
	jmp	.L010lot
.L013aligned:
	movl	%eax,(%edx)
	leal	-4(%ecx),%ecx
	testl	$-4,%ecx
	leal	4(%edx),%edx
	jnz	.L013aligned
	cmpl	$0,%ecx
	jne	.L012little
	ret
.size	OPENSSL_cleanse,.-.L_OPENSSL_cleanse_begin
.comm	OPENSSL_ia32cap_P,4,4
.section	.init
	call	OPENSSL_cpuid_setup
	jmp	.Linitalign
.align	16
.Linitalign:
