#!/usr/bin/env perl

$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

open OUT,"| \"$^X\" $xlate $flavour $output";
*STDOUT=*OUT;

($arg1,$arg2,$arg3,$arg4)=$win64?("%rcx","%rdx","%r8", "%r9") :	# Win64 order
				 ("%rdi","%rsi","%rdx","%rcx");	# Unix order

print<<___;
.extern		OPENSSL_cpuid_setup
.hidden		OPENSSL_cpuid_setup
.section	.init
	call	OPENSSL_cpuid_setup

.hidden	OPENSSL_ia32cap_P
.comm	OPENSSL_ia32cap_P,16,4

.text

.globl	OPENSSL_atomic_add
.type	OPENSSL_atomic_add,\@abi-omnipotent
.align	16
OPENSSL_atomic_add:
	movl	($arg1),%eax
.Lspin:	leaq	($arg2,%rax),%r8
	.byte	0xf0		# lock
	cmpxchgl	%r8d,($arg1)
	jne	.Lspin
	movl	%r8d,%eax
	.byte	0x48,0x98	# cltq/cdqe
	ret
.size	OPENSSL_atomic_add,.-OPENSSL_atomic_add

.globl	OPENSSL_rdtsc
.type	OPENSSL_rdtsc,\@abi-omnipotent
.align	16
OPENSSL_rdtsc:
	rdtsc
	shl	\$32,%rdx
	or	%rdx,%rax
	ret
.size	OPENSSL_rdtsc,.-OPENSSL_rdtsc

.globl	OPENSSL_ia32_cpuid
.type	OPENSSL_ia32_cpuid,\@function,1
.align	16
OPENSSL_ia32_cpuid:
	mov	%rbx,%r8		# save %rbx

	xor	%eax,%eax
	mov	%eax,8(%rdi)		# clear extended feature flags
	cpuid
	mov	%eax,%r11d		# max value for standard query level

	xor	%eax,%eax
	cmp	\$0x756e6547,%ebx	# "Genu"
	setne	%al
	mov	%eax,%r9d
	cmp	\$0x49656e69,%edx	# "ineI"
	setne	%al
	or	%eax,%r9d
	cmp	\$0x6c65746e,%ecx	# "ntel"
	setne	%al
	or	%eax,%r9d		# 0 indicates Intel CPU
	jz	.Lintel

	cmp	\$0x68747541,%ebx	# "Auth"
	setne	%al
	mov	%eax,%r10d
	cmp	\$0x69746E65,%edx	# "enti"
	setne	%al
	or	%eax,%r10d
	cmp	\$0x444D4163,%ecx	# "cAMD"
	setne	%al
	or	%eax,%r10d		# 0 indicates AMD CPU
	jnz	.Lintel

	# AMD specific
	mov	\$0x80000000,%eax
	cpuid
	cmp	\$0x80000001,%eax
	jb	.Lintel
	mov	%eax,%r10d
	mov	\$0x80000001,%eax
	cpuid
	or	%ecx,%r9d
	and	\$0x00000801,%r9d	# isolate AMD XOP bit, 1<<11

	cmp	\$0x80000008,%r10d
	jb	.Lintel

	mov	\$0x80000008,%eax
	cpuid
	movzb	%cl,%r10		# number of cores - 1
	inc	%r10			# number of cores

	mov	\$1,%eax
	cpuid
	bt	\$28,%edx		# test hyper-threading bit
	jnc	.Lgeneric
	shr	\$16,%ebx		# number of logical processors
	cmp	%r10b,%bl
	ja	.Lgeneric
	and	\$0xefffffff,%edx	# ~(1<<28)
	jmp	.Lgeneric

.Lintel:
	cmp	\$4,%r11d
	mov	\$-1,%r10d
	jb	.Lnocacheinfo

	mov	\$4,%eax
	mov	\$0,%ecx		# query L1D
	cpuid
	mov	%eax,%r10d
	shr	\$14,%r10d
	and	\$0xfff,%r10d		# number of cores -1 per L1D

.Lnocacheinfo:
	mov	\$1,%eax
	cpuid
	and	\$0xbfefffff,%edx	# force reserved bits to 0
	cmp	\$0,%r9d
	jne	.Lnotintel
	or	\$0x40000000,%edx	# set reserved bit#30 on Intel CPUs
	and	\$15,%ah
	cmp	\$15,%ah		# examine Family ID
	jne	.LnotP4
	or	\$0x00100000,%edx	# set reserved bit#20 to engage RC4_CHAR
.LnotP4:
	cmp	\$6,%ah
	jne	.Lnotintel
	and	\$0x0fff0ff0,%eax
	cmp	\$0x00050670,%eax	# Knights Landing
	je	.Lknights
	cmp	\$0x00080650,%eax	# Knights Mill (according to sde)
	jne	.Lnotintel
.Lknights:
	and	\$0xfbffffff,%ecx	# clear XSAVE flag to mimic Silvermont

.Lnotintel:
	bt	\$28,%edx		# test hyper-threading bit
	jnc	.Lgeneric
	and	\$0xefffffff,%edx	# ~(1<<28)
	cmp	\$0,%r10d
	je	.Lgeneric

	or	\$0x10000000,%edx	# 1<<28
	shr	\$16,%ebx
	cmp	\$1,%bl			# see if cache is shared
	ja	.Lgeneric
	and	\$0xefffffff,%edx	# ~(1<<28)
.Lgeneric:
	and	\$0x00000800,%r9d	# isolate AMD XOP flag
	and	\$0xfffff7ff,%ecx
	or	%ecx,%r9d		# merge AMD XOP flag

	mov	%edx,%r10d		# %r9d:%r10d is copy of %ecx:%edx

	cmp	\$7,%r11d
	jb	.Lno_extended_info
	mov	\$7,%eax
	xor	%ecx,%ecx
	cpuid
	bt	\$26,%r9d		# check XSAVE bit, cleared on Knights
	jc	.Lnotknights
	and	\$0xfff7ffff,%ebx	# clear ADCX/ADOX flag
.Lnotknights:
	mov	%ebx,8(%rdi)		# save extended feature flags
.Lno_extended_info:

	bt	\$27,%r9d		# check OSXSAVE bit
	jnc	.Lclear_avx
	xor	%ecx,%ecx		# XCR0
	.byte	0x0f,0x01,0xd0		# xgetbv
	and	\$6,%eax		# isolate XMM and YMM state support
	cmp	\$6,%eax
	je	.Ldone
.Lclear_avx:
	mov	\$0xefffe7ff,%eax	# ~(1<<28|1<<12|1<<11)
	and	%eax,%r9d		# clear AVX, FMA and AMD XOP bits
	andl	\$0xffffffdf,8(%rdi)	# clear AVX2, ~(1<<5)
.Ldone:
	shl	\$32,%r9
	mov	%r10d,%eax
	mov	%r8,%rbx		# restore %rbx
	or	%r9,%rax
	ret
.size	OPENSSL_ia32_cpuid,.-OPENSSL_ia32_cpuid

.globl  OPENSSL_cleanse
.type   OPENSSL_cleanse,\@abi-omnipotent
.align  16
OPENSSL_cleanse:
	xor	%rax,%rax
	cmp	\$15,$arg2
	jae	.Lot
	cmp	\$0,$arg2
	je	.Lret
.Little:
	mov	%al,($arg1)
	sub	\$1,$arg2
	lea	1($arg1),$arg1
	jnz	.Little
.Lret:
	ret
.align	16
.Lot:
	test	\$7,$arg1
	jz	.Laligned
	mov	%al,($arg1)
	lea	-1($arg2),$arg2
	lea	1($arg1),$arg1
	jmp	.Lot
.Laligned:
	mov	%rax,($arg1)
	lea	-8($arg2),$arg2
	test	\$-8,$arg2
	lea	8($arg1),$arg1
	jnz	.Laligned
	cmp	\$0,$arg2
	jne	.Little
	ret
.size	OPENSSL_cleanse,.-OPENSSL_cleanse
___

print<<___ if (!$win64);
.globl	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,\@abi-omnipotent
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
	ret
.size	OPENSSL_wipe_cpu,.-OPENSSL_wipe_cpu
___
print<<___ if ($win64);
.globl	OPENSSL_wipe_cpu
.type	OPENSSL_wipe_cpu,\@abi-omnipotent
.align	16
OPENSSL_wipe_cpu:
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	xorq	%rcx,%rcx
	xorq	%rdx,%rdx
	xorq	%r8,%r8
	xorq	%r9,%r9
	xorq	%r10,%r10
	xorq	%r11,%r11
	leaq	8(%rsp),%rax
	ret
.size	OPENSSL_wipe_cpu,.-OPENSSL_wipe_cpu
___

print<<___;
.globl	OPENSSL_ia32_rdrand
.type	OPENSSL_ia32_rdrand,\@abi-omnipotent
.align	16
OPENSSL_ia32_rdrand:
	mov	\$8,%ecx
.Loop_rdrand:
	rdrand	%rax
	jc	.Lbreak_rdrand
	loop	.Loop_rdrand
.Lbreak_rdrand:
	cmp	\$0,%rax
	cmove	%rcx,%rax
	ret
.size	OPENSSL_ia32_rdrand,.-OPENSSL_ia32_rdrand

.globl	OPENSSL_ia32_rdseed
.type	OPENSSL_ia32_rdseed,\@abi-omnipotent
.align	16
OPENSSL_ia32_rdseed:
	mov	\$8,%ecx
.Loop_rdseed:
	rdseed	%rax
	jc	.Lbreak_rdseed
	loop	.Loop_rdseed
.Lbreak_rdseed:
	cmp	\$0,%rax
	cmove	%rcx,%rax
	ret
.size	OPENSSL_ia32_rdseed,.-OPENSSL_ia32_rdseed
___

close STDOUT;	# flush
