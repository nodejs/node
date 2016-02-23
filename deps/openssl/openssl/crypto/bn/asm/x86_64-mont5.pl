#!/usr/bin/env perl

# ====================================================================
# Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================

# August 2011.
#
# Companion to x86_64-mont.pl that optimizes cache-timing attack
# countermeasures. The subroutines are produced by replacing bp[i]
# references in their x86_64-mont.pl counterparts with cache-neutral
# references to powers table computed in BN_mod_exp_mont_consttime.
# In addition subroutine that scatters elements of the powers table
# is implemented, so that scatter-/gathering can be tuned without
# bn_exp.c modifications.

# August 2013.
#
# Add MULX/AD*X code paths and additional interfaces to optimize for
# branch prediction unit. For input lengths that are multiples of 8
# the np argument is not just modulus value, but one interleaved
# with 0. This is to optimize post-condition...

$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

open OUT,"| \"$^X\" $xlate $flavour $output";
*STDOUT=*OUT;

if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
		=~ /GNU assembler version ([2-9]\.[0-9]+)/) {
	$addx = ($1>=2.23);
}

if (!$addx && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
	    `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)/) {
	$addx = ($1>=2.10);
}

if (!$addx && $win64 && ($flavour =~ /masm/ || $ENV{ASM} =~ /ml64/) &&
	    `ml64 2>&1` =~ /Version ([0-9]+)\./) {
	$addx = ($1>=12);
}

if (!$addx && `$ENV{CC} -v 2>&1` =~ /((?:^clang|LLVM) version|.*based on LLVM) ([3-9])\.([0-9]+)/) {
	my $ver = $2 + $3/100.0;	# 3.1->3.01, 3.10->3.10
	$addx = ($ver>=3.03);
}

# int bn_mul_mont_gather5(
$rp="%rdi";	# BN_ULONG *rp,
$ap="%rsi";	# const BN_ULONG *ap,
$bp="%rdx";	# const BN_ULONG *bp,
$np="%rcx";	# const BN_ULONG *np,
$n0="%r8";	# const BN_ULONG *n0,
$num="%r9";	# int num,
		# int idx);	# 0 to 2^5-1, "index" in $bp holding
				# pre-computed powers of a', interlaced
				# in such manner that b[0] is $bp[idx],
				# b[1] is [2^5+idx], etc.
$lo0="%r10";
$hi0="%r11";
$hi1="%r13";
$i="%r14";
$j="%r15";
$m0="%rbx";
$m1="%rbp";

$code=<<___;
.text

.extern	OPENSSL_ia32cap_P

.globl	bn_mul_mont_gather5
.type	bn_mul_mont_gather5,\@function,6
.align	64
bn_mul_mont_gather5:
	test	\$7,${num}d
	jnz	.Lmul_enter
___
$code.=<<___ if ($addx);
	mov	OPENSSL_ia32cap_P+8(%rip),%r11d
___
$code.=<<___;
	jmp	.Lmul4x_enter

.align	16
.Lmul_enter:
	mov	${num}d,${num}d
	mov	%rsp,%rax
	mov	`($win64?56:8)`(%rsp),%r10d	# load 7th argument
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
___
$code.=<<___ if ($win64);
	lea	-0x28(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
___
$code.=<<___;
	lea	2($num),%r11
	neg	%r11
	lea	(%rsp,%r11,8),%rsp	# tp=alloca(8*(num+2))
	and	\$-1024,%rsp		# minimize TLB usage

	mov	%rax,8(%rsp,$num,8)	# tp[num+1]=%rsp
.Lmul_body:
	mov	$bp,%r12		# reassign $bp
___
		$bp="%r12";
		$STRIDE=2**5*8;		# 5 is "window size"
		$N=$STRIDE/4;		# should match cache line size
$code.=<<___;
	mov	%r10,%r11
	shr	\$`log($N/8)/log(2)`,%r10
	and	\$`$N/8-1`,%r11
	not	%r10
	lea	.Lmagic_masks(%rip),%rax
	and	\$`2**5/($N/8)-1`,%r10	# 5 is "window size"
	lea	96($bp,%r11,8),$bp	# pointer within 1st cache line
	movq	0(%rax,%r10,8),%xmm4	# set of masks denoting which
	movq	8(%rax,%r10,8),%xmm5	# cache line contains element
	movq	16(%rax,%r10,8),%xmm6	# denoted by 7th argument
	movq	24(%rax,%r10,8),%xmm7

	movq	`0*$STRIDE/4-96`($bp),%xmm0
	movq	`1*$STRIDE/4-96`($bp),%xmm1
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-96`($bp),%xmm2
	pand	%xmm5,%xmm1
	movq	`3*$STRIDE/4-96`($bp),%xmm3
	pand	%xmm6,%xmm2
	por	%xmm1,%xmm0
	pand	%xmm7,%xmm3
	por	%xmm2,%xmm0
	lea	$STRIDE($bp),$bp
	por	%xmm3,%xmm0

	movq	%xmm0,$m0		# m0=bp[0]

	mov	($n0),$n0		# pull n0[0] value
	mov	($ap),%rax

	xor	$i,$i			# i=0
	xor	$j,$j			# j=0

	movq	`0*$STRIDE/4-96`($bp),%xmm0
	movq	`1*$STRIDE/4-96`($bp),%xmm1
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-96`($bp),%xmm2
	pand	%xmm5,%xmm1

	mov	$n0,$m1
	mulq	$m0			# ap[0]*bp[0]
	mov	%rax,$lo0
	mov	($np),%rax

	movq	`3*$STRIDE/4-96`($bp),%xmm3
	pand	%xmm6,%xmm2
	por	%xmm1,%xmm0
	pand	%xmm7,%xmm3

	imulq	$lo0,$m1		# "tp[0]"*n0
	mov	%rdx,$hi0

	por	%xmm2,%xmm0
	lea	$STRIDE($bp),$bp
	por	%xmm3,%xmm0

	mulq	$m1			# np[0]*m1
	add	%rax,$lo0		# discarded
	mov	8($ap),%rax
	adc	\$0,%rdx
	mov	%rdx,$hi1

	lea	1($j),$j		# j++
	jmp	.L1st_enter

.align	16
.L1st:
	add	%rax,$hi1
	mov	($ap,$j,8),%rax
	adc	\$0,%rdx
	add	$hi0,$hi1		# np[j]*m1+ap[j]*bp[0]
	mov	$lo0,$hi0
	adc	\$0,%rdx
	mov	$hi1,-16(%rsp,$j,8)	# tp[j-1]
	mov	%rdx,$hi1

.L1st_enter:
	mulq	$m0			# ap[j]*bp[0]
	add	%rax,$hi0
	mov	($np,$j,8),%rax
	adc	\$0,%rdx
	lea	1($j),$j		# j++
	mov	%rdx,$lo0

	mulq	$m1			# np[j]*m1
	cmp	$num,$j
	jne	.L1st

	movq	%xmm0,$m0		# bp[1]

	add	%rax,$hi1
	mov	($ap),%rax		# ap[0]
	adc	\$0,%rdx
	add	$hi0,$hi1		# np[j]*m1+ap[j]*bp[0]
	adc	\$0,%rdx
	mov	$hi1,-16(%rsp,$j,8)	# tp[j-1]
	mov	%rdx,$hi1
	mov	$lo0,$hi0

	xor	%rdx,%rdx
	add	$hi0,$hi1
	adc	\$0,%rdx
	mov	$hi1,-8(%rsp,$num,8)
	mov	%rdx,(%rsp,$num,8)	# store upmost overflow bit

	lea	1($i),$i		# i++
	jmp	.Louter
.align	16
.Louter:
	xor	$j,$j			# j=0
	mov	$n0,$m1
	mov	(%rsp),$lo0

	movq	`0*$STRIDE/4-96`($bp),%xmm0
	movq	`1*$STRIDE/4-96`($bp),%xmm1
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-96`($bp),%xmm2
	pand	%xmm5,%xmm1

	mulq	$m0			# ap[0]*bp[i]
	add	%rax,$lo0		# ap[0]*bp[i]+tp[0]
	mov	($np),%rax
	adc	\$0,%rdx

	movq	`3*$STRIDE/4-96`($bp),%xmm3
	pand	%xmm6,%xmm2
	por	%xmm1,%xmm0
	pand	%xmm7,%xmm3

	imulq	$lo0,$m1		# tp[0]*n0
	mov	%rdx,$hi0

	por	%xmm2,%xmm0
	lea	$STRIDE($bp),$bp
	por	%xmm3,%xmm0

	mulq	$m1			# np[0]*m1
	add	%rax,$lo0		# discarded
	mov	8($ap),%rax
	adc	\$0,%rdx
	mov	8(%rsp),$lo0		# tp[1]
	mov	%rdx,$hi1

	lea	1($j),$j		# j++
	jmp	.Linner_enter

.align	16
.Linner:
	add	%rax,$hi1
	mov	($ap,$j,8),%rax
	adc	\$0,%rdx
	add	$lo0,$hi1		# np[j]*m1+ap[j]*bp[i]+tp[j]
	mov	(%rsp,$j,8),$lo0
	adc	\$0,%rdx
	mov	$hi1,-16(%rsp,$j,8)	# tp[j-1]
	mov	%rdx,$hi1

.Linner_enter:
	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$hi0
	mov	($np,$j,8),%rax
	adc	\$0,%rdx
	add	$hi0,$lo0		# ap[j]*bp[i]+tp[j]
	mov	%rdx,$hi0
	adc	\$0,$hi0
	lea	1($j),$j		# j++

	mulq	$m1			# np[j]*m1
	cmp	$num,$j
	jne	.Linner

	movq	%xmm0,$m0		# bp[i+1]

	add	%rax,$hi1
	mov	($ap),%rax		# ap[0]
	adc	\$0,%rdx
	add	$lo0,$hi1		# np[j]*m1+ap[j]*bp[i]+tp[j]
	mov	(%rsp,$j,8),$lo0
	adc	\$0,%rdx
	mov	$hi1,-16(%rsp,$j,8)	# tp[j-1]
	mov	%rdx,$hi1

	xor	%rdx,%rdx
	add	$hi0,$hi1
	adc	\$0,%rdx
	add	$lo0,$hi1		# pull upmost overflow bit
	adc	\$0,%rdx
	mov	$hi1,-8(%rsp,$num,8)
	mov	%rdx,(%rsp,$num,8)	# store upmost overflow bit

	lea	1($i),$i		# i++
	cmp	$num,$i
	jb	.Louter

	xor	$i,$i			# i=0 and clear CF!
	mov	(%rsp),%rax		# tp[0]
	lea	(%rsp),$ap		# borrow ap for tp
	mov	$num,$j			# j=num
	jmp	.Lsub
.align	16
.Lsub:	sbb	($np,$i,8),%rax
	mov	%rax,($rp,$i,8)		# rp[i]=tp[i]-np[i]
	mov	8($ap,$i,8),%rax	# tp[i+1]
	lea	1($i),$i		# i++
	dec	$j			# doesnn't affect CF!
	jnz	.Lsub

	sbb	\$0,%rax		# handle upmost overflow bit
	xor	$i,$i
	and	%rax,$ap
	not	%rax
	mov	$rp,$np
	and	%rax,$np
	mov	$num,$j			# j=num
	or	$np,$ap			# ap=borrow?tp:rp
.align	16
.Lcopy:					# copy or in-place refresh
	mov	($ap,$i,8),%rax
	mov	$i,(%rsp,$i,8)		# zap temporary vector
	mov	%rax,($rp,$i,8)		# rp[i]=tp[i]
	lea	1($i),$i
	sub	\$1,$j
	jnz	.Lcopy

	mov	8(%rsp,$num,8),%rsi	# restore %rsp
	mov	\$1,%rax
___
$code.=<<___ if ($win64);
	movaps	-88(%rsi),%xmm6
	movaps	-72(%rsi),%xmm7
___
$code.=<<___;
	mov	-48(%rsi),%r15
	mov	-40(%rsi),%r14
	mov	-32(%rsi),%r13
	mov	-24(%rsi),%r12
	mov	-16(%rsi),%rbp
	mov	-8(%rsi),%rbx
	lea	(%rsi),%rsp
.Lmul_epilogue:
	ret
.size	bn_mul_mont_gather5,.-bn_mul_mont_gather5
___
{{{
my @A=("%r10","%r11");
my @N=("%r13","%rdi");
$code.=<<___;
.type	bn_mul4x_mont_gather5,\@function,6
.align	32
bn_mul4x_mont_gather5:
.Lmul4x_enter:
___
$code.=<<___ if ($addx);
	and	\$0x80100,%r11d
	cmp	\$0x80100,%r11d
	je	.Lmulx4x_enter
___
$code.=<<___;
	.byte	0x67
	mov	%rsp,%rax
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
___
$code.=<<___ if ($win64);
	lea	-0x28(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
___
$code.=<<___;
	.byte	0x67
	mov	${num}d,%r10d
	shl	\$3,${num}d
	shl	\$3+2,%r10d		# 4*$num
	neg	$num			# -$num

	##############################################################
	# ensure that stack frame doesn't alias with $aptr+4*$num
	# modulo 4096, which covers ret[num], am[num] and n[2*num]
	# (see bn_exp.c). this is done to allow memory disambiguation
	# logic do its magic. [excessive frame is allocated in order
	# to allow bn_from_mont8x to clear it.]
	#
	lea	-64(%rsp,$num,2),%r11
	sub	$ap,%r11
	and	\$4095,%r11
	cmp	%r11,%r10
	jb	.Lmul4xsp_alt
	sub	%r11,%rsp		# align with $ap
	lea	-64(%rsp,$num,2),%rsp	# alloca(128+num*8)
	jmp	.Lmul4xsp_done

.align	32
.Lmul4xsp_alt:
	lea	4096-64(,$num,2),%r10
	lea	-64(%rsp,$num,2),%rsp	# alloca(128+num*8)
	sub	%r10,%r11
	mov	\$0,%r10
	cmovc	%r10,%r11
	sub	%r11,%rsp
.Lmul4xsp_done:
	and	\$-64,%rsp
	neg	$num

	mov	%rax,40(%rsp)
.Lmul4x_body:

	call	mul4x_internal

	mov	40(%rsp),%rsi		# restore %rsp
	mov	\$1,%rax
___
$code.=<<___ if ($win64);
	movaps	-88(%rsi),%xmm6
	movaps	-72(%rsi),%xmm7
___
$code.=<<___;
	mov	-48(%rsi),%r15
	mov	-40(%rsi),%r14
	mov	-32(%rsi),%r13
	mov	-24(%rsi),%r12
	mov	-16(%rsi),%rbp
	mov	-8(%rsi),%rbx
	lea	(%rsi),%rsp
.Lmul4x_epilogue:
	ret
.size	bn_mul4x_mont_gather5,.-bn_mul4x_mont_gather5

.type	mul4x_internal,\@abi-omnipotent
.align	32
mul4x_internal:
	shl	\$5,$num
	mov	`($win64?56:8)`(%rax),%r10d	# load 7th argument
	lea	256(%rdx,$num),%r13
	shr	\$5,$num		# restore $num
___
		$bp="%r12";
		$STRIDE=2**5*8;		# 5 is "window size"
		$N=$STRIDE/4;		# should match cache line size
		$tp=$i;
$code.=<<___;
	mov	%r10,%r11
	shr	\$`log($N/8)/log(2)`,%r10
	and	\$`$N/8-1`,%r11
	not	%r10
	lea	.Lmagic_masks(%rip),%rax
	and	\$`2**5/($N/8)-1`,%r10	# 5 is "window size"
	lea	96(%rdx,%r11,8),$bp	# pointer within 1st cache line
	movq	0(%rax,%r10,8),%xmm4	# set of masks denoting which
	movq	8(%rax,%r10,8),%xmm5	# cache line contains element
	add	\$7,%r11
	movq	16(%rax,%r10,8),%xmm6	# denoted by 7th argument
	movq	24(%rax,%r10,8),%xmm7
	and	\$7,%r11

	movq	`0*$STRIDE/4-96`($bp),%xmm0
	lea	$STRIDE($bp),$tp	# borrow $tp
	movq	`1*$STRIDE/4-96`($bp),%xmm1
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-96`($bp),%xmm2
	pand	%xmm5,%xmm1
	movq	`3*$STRIDE/4-96`($bp),%xmm3
	pand	%xmm6,%xmm2
	.byte	0x67
	por	%xmm1,%xmm0
	movq	`0*$STRIDE/4-96`($tp),%xmm1
	.byte	0x67
	pand	%xmm7,%xmm3
	.byte	0x67
	por	%xmm2,%xmm0
	movq	`1*$STRIDE/4-96`($tp),%xmm2
	.byte	0x67
	pand	%xmm4,%xmm1
	.byte	0x67
	por	%xmm3,%xmm0
	movq	`2*$STRIDE/4-96`($tp),%xmm3

	movq	%xmm0,$m0		# m0=bp[0]
	movq	`3*$STRIDE/4-96`($tp),%xmm0
	mov	%r13,16+8(%rsp)		# save end of b[num]
	mov	$rp, 56+8(%rsp)		# save $rp

	mov	($n0),$n0		# pull n0[0] value
	mov	($ap),%rax
	lea	($ap,$num),$ap		# end of a[num]
	neg	$num

	mov	$n0,$m1
	mulq	$m0			# ap[0]*bp[0]
	mov	%rax,$A[0]
	mov	($np),%rax

	pand	%xmm5,%xmm2
	pand	%xmm6,%xmm3
	por	%xmm2,%xmm1

	imulq	$A[0],$m1		# "tp[0]"*n0
	##############################################################
	# $tp is chosen so that writing to top-most element of the
	# vector occurs just "above" references to powers table,
	# "above" modulo cache-line size, which effectively precludes
	# possibility of memory disambiguation logic failure when
	# accessing the table.
	# 
	lea	64+8(%rsp,%r11,8),$tp
	mov	%rdx,$A[1]

	pand	%xmm7,%xmm0
	por	%xmm3,%xmm1
	lea	2*$STRIDE($bp),$bp
	por	%xmm1,%xmm0

	mulq	$m1			# np[0]*m1
	add	%rax,$A[0]		# discarded
	mov	8($ap,$num),%rax
	adc	\$0,%rdx
	mov	%rdx,$N[1]

	mulq	$m0
	add	%rax,$A[1]
	mov	16*1($np),%rax		# interleaved with 0, therefore 16*n
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1
	add	%rax,$N[1]
	mov	16($ap,$num),%rax
	adc	\$0,%rdx
	add	$A[1],$N[1]
	lea	4*8($num),$j		# j=4
	lea	16*4($np),$np
	adc	\$0,%rdx
	mov	$N[1],($tp)
	mov	%rdx,$N[0]
	jmp	.L1st4x

.align	32
.L1st4x:
	mulq	$m0			# ap[j]*bp[0]
	add	%rax,$A[0]
	mov	-16*2($np),%rax
	lea	32($tp),$tp
	adc	\$0,%rdx
	mov	%rdx,$A[1]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[0]
	mov	-8($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[0],$N[0]		# np[j]*m1+ap[j]*bp[0]
	adc	\$0,%rdx
	mov	$N[0],-24($tp)		# tp[j-1]
	mov	%rdx,$N[1]

	mulq	$m0			# ap[j]*bp[0]
	add	%rax,$A[1]
	mov	-16*1($np),%rax
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[1]
	mov	($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[1],$N[1]		# np[j]*m1+ap[j]*bp[0]
	adc	\$0,%rdx
	mov	$N[1],-16($tp)		# tp[j-1]
	mov	%rdx,$N[0]

	mulq	$m0			# ap[j]*bp[0]
	add	%rax,$A[0]
	mov	16*0($np),%rax
	adc	\$0,%rdx
	mov	%rdx,$A[1]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[0]
	mov	8($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[0],$N[0]		# np[j]*m1+ap[j]*bp[0]
	adc	\$0,%rdx
	mov	$N[0],-8($tp)		# tp[j-1]
	mov	%rdx,$N[1]

	mulq	$m0			# ap[j]*bp[0]
	add	%rax,$A[1]
	mov	16*1($np),%rax
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[1]
	mov	16($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[1],$N[1]		# np[j]*m1+ap[j]*bp[0]
	lea	16*4($np),$np
	adc	\$0,%rdx
	mov	$N[1],($tp)		# tp[j-1]
	mov	%rdx,$N[0]

	add	\$32,$j			# j+=4
	jnz	.L1st4x

	mulq	$m0			# ap[j]*bp[0]
	add	%rax,$A[0]
	mov	-16*2($np),%rax
	lea	32($tp),$tp
	adc	\$0,%rdx
	mov	%rdx,$A[1]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[0]
	mov	-8($ap),%rax
	adc	\$0,%rdx
	add	$A[0],$N[0]		# np[j]*m1+ap[j]*bp[0]
	adc	\$0,%rdx
	mov	$N[0],-24($tp)		# tp[j-1]
	mov	%rdx,$N[1]

	mulq	$m0			# ap[j]*bp[0]
	add	%rax,$A[1]
	mov	-16*1($np),%rax
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[1]
	mov	($ap,$num),%rax		# ap[0]
	adc	\$0,%rdx
	add	$A[1],$N[1]		# np[j]*m1+ap[j]*bp[0]
	adc	\$0,%rdx
	mov	$N[1],-16($tp)		# tp[j-1]
	mov	%rdx,$N[0]

	movq	%xmm0,$m0		# bp[1]
	lea	($np,$num,2),$np	# rewind $np

	xor	$N[1],$N[1]
	add	$A[0],$N[0]
	adc	\$0,$N[1]
	mov	$N[0],-8($tp)

	jmp	.Louter4x

.align	32
.Louter4x:
	mov	($tp,$num),$A[0]
	mov	$n0,$m1
	mulq	$m0			# ap[0]*bp[i]
	add	%rax,$A[0]		# ap[0]*bp[i]+tp[0]
	mov	($np),%rax
	adc	\$0,%rdx

	movq	`0*$STRIDE/4-96`($bp),%xmm0
	movq	`1*$STRIDE/4-96`($bp),%xmm1
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-96`($bp),%xmm2
	pand	%xmm5,%xmm1
	movq	`3*$STRIDE/4-96`($bp),%xmm3

	imulq	$A[0],$m1		# tp[0]*n0
	.byte	0x67
	mov	%rdx,$A[1]
	mov	$N[1],($tp)		# store upmost overflow bit

	pand	%xmm6,%xmm2
	por	%xmm1,%xmm0
	pand	%xmm7,%xmm3
	por	%xmm2,%xmm0
	lea	($tp,$num),$tp		# rewind $tp
	lea	$STRIDE($bp),$bp
	por	%xmm3,%xmm0

	mulq	$m1			# np[0]*m1
	add	%rax,$A[0]		# "$N[0]", discarded
	mov	8($ap,$num),%rax
	adc	\$0,%rdx
	mov	%rdx,$N[1]

	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$A[1]
	mov	16*1($np),%rax		# interleaved with 0, therefore 16*n
	adc	\$0,%rdx
	add	8($tp),$A[1]		# +tp[1]
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[1]
	mov	16($ap,$num),%rax
	adc	\$0,%rdx
	add	$A[1],$N[1]		# np[j]*m1+ap[j]*bp[i]+tp[j]
	lea	4*8($num),$j		# j=4
	lea	16*4($np),$np
	adc	\$0,%rdx
	mov	%rdx,$N[0]
	jmp	.Linner4x

.align	32
.Linner4x:
	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$A[0]
	mov	-16*2($np),%rax
	adc	\$0,%rdx
	add	16($tp),$A[0]		# ap[j]*bp[i]+tp[j]
	lea	32($tp),$tp
	adc	\$0,%rdx
	mov	%rdx,$A[1]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[0]
	mov	-8($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[0],$N[0]
	adc	\$0,%rdx
	mov	$N[1],-32($tp)		# tp[j-1]
	mov	%rdx,$N[1]

	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$A[1]
	mov	-16*1($np),%rax
	adc	\$0,%rdx
	add	-8($tp),$A[1]
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[1]
	mov	($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[1],$N[1]
	adc	\$0,%rdx
	mov	$N[0],-24($tp)		# tp[j-1]
	mov	%rdx,$N[0]

	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$A[0]
	mov	16*0($np),%rax
	adc	\$0,%rdx
	add	($tp),$A[0]		# ap[j]*bp[i]+tp[j]
	adc	\$0,%rdx
	mov	%rdx,$A[1]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[0]
	mov	8($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[0],$N[0]
	adc	\$0,%rdx
	mov	$N[1],-16($tp)		# tp[j-1]
	mov	%rdx,$N[1]

	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$A[1]
	mov	16*1($np),%rax
	adc	\$0,%rdx
	add	8($tp),$A[1]
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[1]
	mov	16($ap,$j),%rax
	adc	\$0,%rdx
	add	$A[1],$N[1]
	lea	16*4($np),$np
	adc	\$0,%rdx
	mov	$N[0],-8($tp)		# tp[j-1]
	mov	%rdx,$N[0]

	add	\$32,$j			# j+=4
	jnz	.Linner4x

	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$A[0]
	mov	-16*2($np),%rax
	adc	\$0,%rdx
	add	16($tp),$A[0]		# ap[j]*bp[i]+tp[j]
	lea	32($tp),$tp
	adc	\$0,%rdx
	mov	%rdx,$A[1]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[0]
	mov	-8($ap),%rax
	adc	\$0,%rdx
	add	$A[0],$N[0]
	adc	\$0,%rdx
	mov	$N[1],-32($tp)		# tp[j-1]
	mov	%rdx,$N[1]

	mulq	$m0			# ap[j]*bp[i]
	add	%rax,$A[1]
	mov	$m1,%rax
	mov	-16*1($np),$m1
	adc	\$0,%rdx
	add	-8($tp),$A[1]
	adc	\$0,%rdx
	mov	%rdx,$A[0]

	mulq	$m1			# np[j]*m1
	add	%rax,$N[1]
	mov	($ap,$num),%rax		# ap[0]
	adc	\$0,%rdx
	add	$A[1],$N[1]
	adc	\$0,%rdx
	mov	$N[0],-24($tp)		# tp[j-1]
	mov	%rdx,$N[0]

	movq	%xmm0,$m0		# bp[i+1]
	mov	$N[1],-16($tp)		# tp[j-1]
	lea	($np,$num,2),$np	# rewind $np

	xor	$N[1],$N[1]
	add	$A[0],$N[0]
	adc	\$0,$N[1]
	add	($tp),$N[0]		# pull upmost overflow bit
	adc	\$0,$N[1]		# upmost overflow bit
	mov	$N[0],-8($tp)

	cmp	16+8(%rsp),$bp
	jb	.Louter4x
___
if (1) {
$code.=<<___;
	sub	$N[0],$m1		# compare top-most words
	adc	$j,$j			# $j is zero
	or	$j,$N[1]
	xor	\$1,$N[1]
	lea	($tp,$num),%rbx		# tptr in .sqr4x_sub
	lea	($np,$N[1],8),%rbp	# nptr in .sqr4x_sub
	mov	%r9,%rcx
	sar	\$3+2,%rcx		# cf=0
	mov	56+8(%rsp),%rdi		# rptr in .sqr4x_sub
	jmp	.Lsqr4x_sub
___
} else {
my @ri=("%rax",$bp,$m0,$m1);
my $rp="%rdx";
$code.=<<___
	xor	\$1,$N[1]
	lea	($tp,$num),$tp		# rewind $tp
	sar	\$5,$num		# cf=0
	lea	($np,$N[1],8),$np
	mov	56+8(%rsp),$rp		# restore $rp
	jmp	.Lsub4x

.align	32
.Lsub4x:
	.byte	0x66
	mov	8*0($tp),@ri[0]
	mov	8*1($tp),@ri[1]
	.byte	0x66
	sbb	16*0($np),@ri[0]
	mov	8*2($tp),@ri[2]
	sbb	16*1($np),@ri[1]
	mov	3*8($tp),@ri[3]
	lea	4*8($tp),$tp
	sbb	16*2($np),@ri[2]
	mov	@ri[0],8*0($rp)
	sbb	16*3($np),@ri[3]
	lea	16*4($np),$np
	mov	@ri[1],8*1($rp)
	mov	@ri[2],8*2($rp)
	mov	@ri[3],8*3($rp)
	lea	8*4($rp),$rp

	inc	$num
	jnz	.Lsub4x

	ret
___
}
$code.=<<___;
.size	mul4x_internal,.-mul4x_internal
___
}}}
{{{
######################################################################
# void bn_power5(
my $rptr="%rdi";	# BN_ULONG *rptr,
my $aptr="%rsi";	# const BN_ULONG *aptr,
my $bptr="%rdx";	# const void *table,
my $nptr="%rcx";	# const BN_ULONG *nptr,
my $n0  ="%r8";		# const BN_ULONG *n0);
my $num ="%r9";		# int num, has to be divisible by 8
			# int pwr 

my ($i,$j,$tptr)=("%rbp","%rcx",$rptr);
my @A0=("%r10","%r11");
my @A1=("%r12","%r13");
my ($a0,$a1,$ai)=("%r14","%r15","%rbx");

$code.=<<___;
.globl	bn_power5
.type	bn_power5,\@function,6
.align	32
bn_power5:
___
$code.=<<___ if ($addx);
	mov	OPENSSL_ia32cap_P+8(%rip),%r11d
	and	\$0x80100,%r11d
	cmp	\$0x80100,%r11d
	je	.Lpowerx5_enter
___
$code.=<<___;
	mov	%rsp,%rax
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
___
$code.=<<___ if ($win64);
	lea	-0x28(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
___
$code.=<<___;
	mov	${num}d,%r10d
	shl	\$3,${num}d		# convert $num to bytes
	shl	\$3+2,%r10d		# 4*$num
	neg	$num
	mov	($n0),$n0		# *n0

	##############################################################
	# ensure that stack frame doesn't alias with $aptr+4*$num
	# modulo 4096, which covers ret[num], am[num] and n[2*num]
	# (see bn_exp.c). this is done to allow memory disambiguation
	# logic do its magic.
	#
	lea	-64(%rsp,$num,2),%r11
	sub	$aptr,%r11
	and	\$4095,%r11
	cmp	%r11,%r10
	jb	.Lpwr_sp_alt
	sub	%r11,%rsp		# align with $aptr
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+2*$num)
	jmp	.Lpwr_sp_done

.align	32
.Lpwr_sp_alt:
	lea	4096-64(,$num,2),%r10	# 4096-frame-2*$num
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+2*$num)
	sub	%r10,%r11
	mov	\$0,%r10
	cmovc	%r10,%r11
	sub	%r11,%rsp
.Lpwr_sp_done:
	and	\$-64,%rsp
	mov	$num,%r10	
	neg	$num

	##############################################################
	# Stack layout
	#
	# +0	saved $num, used in reduction section
	# +8	&t[2*$num], used in reduction section
	# +32	saved *n0
	# +40	saved %rsp
	# +48	t[2*$num]
	#
	mov	$n0,  32(%rsp)
	mov	%rax, 40(%rsp)		# save original %rsp
.Lpower5_body:
	movq	$rptr,%xmm1		# save $rptr
	movq	$nptr,%xmm2		# save $nptr
	movq	%r10, %xmm3		# -$num
	movq	$bptr,%xmm4

	call	__bn_sqr8x_internal
	call	__bn_sqr8x_internal
	call	__bn_sqr8x_internal
	call	__bn_sqr8x_internal
	call	__bn_sqr8x_internal

	movq	%xmm2,$nptr
	movq	%xmm4,$bptr
	mov	$aptr,$rptr
	mov	40(%rsp),%rax
	lea	32(%rsp),$n0

	call	mul4x_internal

	mov	40(%rsp),%rsi		# restore %rsp
	mov	\$1,%rax
	mov	-48(%rsi),%r15
	mov	-40(%rsi),%r14
	mov	-32(%rsi),%r13
	mov	-24(%rsi),%r12
	mov	-16(%rsi),%rbp
	mov	-8(%rsi),%rbx
	lea	(%rsi),%rsp
.Lpower5_epilogue:
	ret
.size	bn_power5,.-bn_power5

.globl	bn_sqr8x_internal
.hidden	bn_sqr8x_internal
.type	bn_sqr8x_internal,\@abi-omnipotent
.align	32
bn_sqr8x_internal:
__bn_sqr8x_internal:
	##############################################################
	# Squaring part:
	#
	# a) multiply-n-add everything but a[i]*a[i];
	# b) shift result of a) by 1 to the left and accumulate
	#    a[i]*a[i] products;
	#
	##############################################################
	#                                                     a[1]a[0]
	#                                                 a[2]a[0]
	#                                             a[3]a[0]
	#                                             a[2]a[1]
	#                                         a[4]a[0]
	#                                         a[3]a[1]
	#                                     a[5]a[0]
	#                                     a[4]a[1]
	#                                     a[3]a[2]
	#                                 a[6]a[0]
	#                                 a[5]a[1]
	#                                 a[4]a[2]
	#                             a[7]a[0]
	#                             a[6]a[1]
	#                             a[5]a[2]
	#                             a[4]a[3]
	#                         a[7]a[1]
	#                         a[6]a[2]
	#                         a[5]a[3]
	#                     a[7]a[2]
	#                     a[6]a[3]
	#                     a[5]a[4]
	#                 a[7]a[3]
	#                 a[6]a[4]
	#             a[7]a[4]
	#             a[6]a[5]
	#         a[7]a[5]
	#     a[7]a[6]
	#                                                     a[1]a[0]
	#                                                 a[2]a[0]
	#                                             a[3]a[0]
	#                                         a[4]a[0]
	#                                     a[5]a[0]
	#                                 a[6]a[0]
	#                             a[7]a[0]
	#                                             a[2]a[1]
	#                                         a[3]a[1]
	#                                     a[4]a[1]
	#                                 a[5]a[1]
	#                             a[6]a[1]
	#                         a[7]a[1]
	#                                     a[3]a[2]
	#                                 a[4]a[2]
	#                             a[5]a[2]
	#                         a[6]a[2]
	#                     a[7]a[2]
	#                             a[4]a[3]
	#                         a[5]a[3]
	#                     a[6]a[3]
	#                 a[7]a[3]
	#                     a[5]a[4]
	#                 a[6]a[4]
	#             a[7]a[4]
	#             a[6]a[5]
	#         a[7]a[5]
	#     a[7]a[6]
	#                                                         a[0]a[0]
	#                                                 a[1]a[1]
	#                                         a[2]a[2]
	#                                 a[3]a[3]
	#                         a[4]a[4]
	#                 a[5]a[5]
	#         a[6]a[6]
	# a[7]a[7]

	lea	32(%r10),$i		# $i=-($num-32)
	lea	($aptr,$num),$aptr	# end of a[] buffer, ($aptr,$i)=&ap[2]

	mov	$num,$j			# $j=$num

					# comments apply to $num==8 case
	mov	-32($aptr,$i),$a0	# a[0]
	lea	48+8(%rsp,$num,2),$tptr	# end of tp[] buffer, &tp[2*$num]
	mov	-24($aptr,$i),%rax	# a[1]
	lea	-32($tptr,$i),$tptr	# end of tp[] window, &tp[2*$num-"$i"]
	mov	-16($aptr,$i),$ai	# a[2]
	mov	%rax,$a1

	mul	$a0			# a[1]*a[0]
	mov	%rax,$A0[0]		# a[1]*a[0]
	 mov	$ai,%rax		# a[2]
	mov	%rdx,$A0[1]
	mov	$A0[0],-24($tptr,$i)	# t[1]

	mul	$a0			# a[2]*a[0]
	add	%rax,$A0[1]
	 mov	$ai,%rax
	adc	\$0,%rdx
	mov	$A0[1],-16($tptr,$i)	# t[2]
	mov	%rdx,$A0[0]


	 mov	-8($aptr,$i),$ai	# a[3]
	mul	$a1			# a[2]*a[1]
	mov	%rax,$A1[0]		# a[2]*a[1]+t[3]
	 mov	$ai,%rax
	mov	%rdx,$A1[1]

	 lea	($i),$j
	mul	$a0			# a[3]*a[0]
	add	%rax,$A0[0]		# a[3]*a[0]+a[2]*a[1]+t[3]
	 mov	$ai,%rax
	mov	%rdx,$A0[1]
	adc	\$0,$A0[1]
	add	$A1[0],$A0[0]
	adc	\$0,$A0[1]
	mov	$A0[0],-8($tptr,$j)	# t[3]
	jmp	.Lsqr4x_1st

.align	32
.Lsqr4x_1st:
	 mov	($aptr,$j),$ai		# a[4]
	mul	$a1			# a[3]*a[1]
	add	%rax,$A1[1]		# a[3]*a[1]+t[4]
	 mov	$ai,%rax
	mov	%rdx,$A1[0]
	adc	\$0,$A1[0]

	mul	$a0			# a[4]*a[0]
	add	%rax,$A0[1]		# a[4]*a[0]+a[3]*a[1]+t[4]
	 mov	$ai,%rax		# a[3]
	 mov	8($aptr,$j),$ai		# a[5]
	mov	%rdx,$A0[0]
	adc	\$0,$A0[0]
	add	$A1[1],$A0[1]
	adc	\$0,$A0[0]


	mul	$a1			# a[4]*a[3]
	add	%rax,$A1[0]		# a[4]*a[3]+t[5]
	 mov	$ai,%rax
	 mov	$A0[1],($tptr,$j)	# t[4]
	mov	%rdx,$A1[1]
	adc	\$0,$A1[1]

	mul	$a0			# a[5]*a[2]
	add	%rax,$A0[0]		# a[5]*a[2]+a[4]*a[3]+t[5]
	 mov	$ai,%rax
	 mov	16($aptr,$j),$ai	# a[6]
	mov	%rdx,$A0[1]
	adc	\$0,$A0[1]
	add	$A1[0],$A0[0]
	adc	\$0,$A0[1]

	mul	$a1			# a[5]*a[3]
	add	%rax,$A1[1]		# a[5]*a[3]+t[6]
	 mov	$ai,%rax
	 mov	$A0[0],8($tptr,$j)	# t[5]
	mov	%rdx,$A1[0]
	adc	\$0,$A1[0]

	mul	$a0			# a[6]*a[2]
	add	%rax,$A0[1]		# a[6]*a[2]+a[5]*a[3]+t[6]
	 mov	$ai,%rax		# a[3]
	 mov	24($aptr,$j),$ai	# a[7]
	mov	%rdx,$A0[0]
	adc	\$0,$A0[0]
	add	$A1[1],$A0[1]
	adc	\$0,$A0[0]


	mul	$a1			# a[6]*a[5]
	add	%rax,$A1[0]		# a[6]*a[5]+t[7]
	 mov	$ai,%rax
	 mov	$A0[1],16($tptr,$j)	# t[6]
	mov	%rdx,$A1[1]
	adc	\$0,$A1[1]
	 lea	32($j),$j

	mul	$a0			# a[7]*a[4]
	add	%rax,$A0[0]		# a[7]*a[4]+a[6]*a[5]+t[6]
	 mov	$ai,%rax
	mov	%rdx,$A0[1]
	adc	\$0,$A0[1]
	add	$A1[0],$A0[0]
	adc	\$0,$A0[1]
	mov	$A0[0],-8($tptr,$j)	# t[7]

	cmp	\$0,$j
	jne	.Lsqr4x_1st

	mul	$a1			# a[7]*a[5]
	add	%rax,$A1[1]
	lea	16($i),$i
	adc	\$0,%rdx
	add	$A0[1],$A1[1]
	adc	\$0,%rdx

	mov	$A1[1],($tptr)		# t[8]
	mov	%rdx,$A1[0]
	mov	%rdx,8($tptr)		# t[9]
	jmp	.Lsqr4x_outer

.align	32
.Lsqr4x_outer:				# comments apply to $num==6 case
	mov	-32($aptr,$i),$a0	# a[0]
	lea	48+8(%rsp,$num,2),$tptr	# end of tp[] buffer, &tp[2*$num]
	mov	-24($aptr,$i),%rax	# a[1]
	lea	-32($tptr,$i),$tptr	# end of tp[] window, &tp[2*$num-"$i"]
	mov	-16($aptr,$i),$ai	# a[2]
	mov	%rax,$a1

	mul	$a0			# a[1]*a[0]
	mov	-24($tptr,$i),$A0[0]	# t[1]
	add	%rax,$A0[0]		# a[1]*a[0]+t[1]
	 mov	$ai,%rax		# a[2]
	adc	\$0,%rdx
	mov	$A0[0],-24($tptr,$i)	# t[1]
	mov	%rdx,$A0[1]

	mul	$a0			# a[2]*a[0]
	add	%rax,$A0[1]
	 mov	$ai,%rax
	adc	\$0,%rdx
	add	-16($tptr,$i),$A0[1]	# a[2]*a[0]+t[2]
	mov	%rdx,$A0[0]
	adc	\$0,$A0[0]
	mov	$A0[1],-16($tptr,$i)	# t[2]

	xor	$A1[0],$A1[0]

	 mov	-8($aptr,$i),$ai	# a[3]
	mul	$a1			# a[2]*a[1]
	add	%rax,$A1[0]		# a[2]*a[1]+t[3]
	 mov	$ai,%rax
	adc	\$0,%rdx
	add	-8($tptr,$i),$A1[0]
	mov	%rdx,$A1[1]
	adc	\$0,$A1[1]

	mul	$a0			# a[3]*a[0]
	add	%rax,$A0[0]		# a[3]*a[0]+a[2]*a[1]+t[3]
	 mov	$ai,%rax
	adc	\$0,%rdx
	add	$A1[0],$A0[0]
	mov	%rdx,$A0[1]
	adc	\$0,$A0[1]
	mov	$A0[0],-8($tptr,$i)	# t[3]

	lea	($i),$j
	jmp	.Lsqr4x_inner

.align	32
.Lsqr4x_inner:
	 mov	($aptr,$j),$ai		# a[4]
	mul	$a1			# a[3]*a[1]
	add	%rax,$A1[1]		# a[3]*a[1]+t[4]
	 mov	$ai,%rax
	mov	%rdx,$A1[0]
	adc	\$0,$A1[0]
	add	($tptr,$j),$A1[1]
	adc	\$0,$A1[0]

	.byte	0x67
	mul	$a0			# a[4]*a[0]
	add	%rax,$A0[1]		# a[4]*a[0]+a[3]*a[1]+t[4]
	 mov	$ai,%rax		# a[3]
	 mov	8($aptr,$j),$ai		# a[5]
	mov	%rdx,$A0[0]
	adc	\$0,$A0[0]
	add	$A1[1],$A0[1]
	adc	\$0,$A0[0]

	mul	$a1			# a[4]*a[3]
	add	%rax,$A1[0]		# a[4]*a[3]+t[5]
	mov	$A0[1],($tptr,$j)	# t[4]
	 mov	$ai,%rax
	mov	%rdx,$A1[1]
	adc	\$0,$A1[1]
	add	8($tptr,$j),$A1[0]
	lea	16($j),$j		# j++
	adc	\$0,$A1[1]

	mul	$a0			# a[5]*a[2]
	add	%rax,$A0[0]		# a[5]*a[2]+a[4]*a[3]+t[5]
	 mov	$ai,%rax
	adc	\$0,%rdx
	add	$A1[0],$A0[0]
	mov	%rdx,$A0[1]
	adc	\$0,$A0[1]
	mov	$A0[0],-8($tptr,$j)	# t[5], "preloaded t[1]" below

	cmp	\$0,$j
	jne	.Lsqr4x_inner

	.byte	0x67
	mul	$a1			# a[5]*a[3]
	add	%rax,$A1[1]
	adc	\$0,%rdx
	add	$A0[1],$A1[1]
	adc	\$0,%rdx

	mov	$A1[1],($tptr)		# t[6], "preloaded t[2]" below
	mov	%rdx,$A1[0]
	mov	%rdx,8($tptr)		# t[7], "preloaded t[3]" below

	add	\$16,$i
	jnz	.Lsqr4x_outer

					# comments apply to $num==4 case
	mov	-32($aptr),$a0		# a[0]
	lea	48+8(%rsp,$num,2),$tptr	# end of tp[] buffer, &tp[2*$num]
	mov	-24($aptr),%rax		# a[1]
	lea	-32($tptr,$i),$tptr	# end of tp[] window, &tp[2*$num-"$i"]
	mov	-16($aptr),$ai		# a[2]
	mov	%rax,$a1

	mul	$a0			# a[1]*a[0]
	add	%rax,$A0[0]		# a[1]*a[0]+t[1], preloaded t[1]
	 mov	$ai,%rax		# a[2]
	mov	%rdx,$A0[1]
	adc	\$0,$A0[1]

	mul	$a0			# a[2]*a[0]
	add	%rax,$A0[1]
	 mov	$ai,%rax
	 mov	$A0[0],-24($tptr)	# t[1]
	mov	%rdx,$A0[0]
	adc	\$0,$A0[0]
	add	$A1[1],$A0[1]		# a[2]*a[0]+t[2], preloaded t[2]
	 mov	-8($aptr),$ai		# a[3]
	adc	\$0,$A0[0]

	mul	$a1			# a[2]*a[1]
	add	%rax,$A1[0]		# a[2]*a[1]+t[3], preloaded t[3]
	 mov	$ai,%rax
	 mov	$A0[1],-16($tptr)	# t[2]
	mov	%rdx,$A1[1]
	adc	\$0,$A1[1]

	mul	$a0			# a[3]*a[0]
	add	%rax,$A0[0]		# a[3]*a[0]+a[2]*a[1]+t[3]
	 mov	$ai,%rax
	mov	%rdx,$A0[1]
	adc	\$0,$A0[1]
	add	$A1[0],$A0[0]
	adc	\$0,$A0[1]
	mov	$A0[0],-8($tptr)	# t[3]

	mul	$a1			# a[3]*a[1]
	add	%rax,$A1[1]
	 mov	-16($aptr),%rax		# a[2]
	adc	\$0,%rdx
	add	$A0[1],$A1[1]
	adc	\$0,%rdx

	mov	$A1[1],($tptr)		# t[4]
	mov	%rdx,$A1[0]
	mov	%rdx,8($tptr)		# t[5]

	mul	$ai			# a[2]*a[3]
___
{
my ($shift,$carry)=($a0,$a1);
my @S=(@A1,$ai,$n0);
$code.=<<___;
	 add	\$16,$i
	 xor	$shift,$shift
	 sub	$num,$i			# $i=16-$num
	 xor	$carry,$carry

	add	$A1[0],%rax		# t[5]
	adc	\$0,%rdx
	mov	%rax,8($tptr)		# t[5]
	mov	%rdx,16($tptr)		# t[6]
	mov	$carry,24($tptr)	# t[7]

	 mov	-16($aptr,$i),%rax	# a[0]
	lea	48+8(%rsp),$tptr
	 xor	$A0[0],$A0[0]		# t[0]
	 mov	8($tptr),$A0[1]		# t[1]

	lea	($shift,$A0[0],2),$S[0]	# t[2*i]<<1 | shift
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[1]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[1]		# | t[2*i]>>63
	 mov	16($tptr),$A0[0]	# t[2*i+2]	# prefetch
	mov	$A0[1],$shift		# shift=t[2*i+1]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	 mov	24($tptr),$A0[1]	# t[2*i+2+1]	# prefetch
	adc	%rax,$S[0]
	 mov	-8($aptr,$i),%rax	# a[i+1]	# prefetch
	mov	$S[0],($tptr)
	adc	%rdx,$S[1]

	lea	($shift,$A0[0],2),$S[2]	# t[2*i]<<1 | shift
	 mov	$S[1],8($tptr)
	 sbb	$carry,$carry		# mov cf,$carry
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[3]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[3]		# | t[2*i]>>63
	 mov	32($tptr),$A0[0]	# t[2*i+2]	# prefetch
	mov	$A0[1],$shift		# shift=t[2*i+1]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	 mov	40($tptr),$A0[1]	# t[2*i+2+1]	# prefetch
	adc	%rax,$S[2]
	 mov	0($aptr,$i),%rax	# a[i+1]	# prefetch
	mov	$S[2],16($tptr)
	adc	%rdx,$S[3]
	lea	16($i),$i
	mov	$S[3],24($tptr)
	sbb	$carry,$carry		# mov cf,$carry
	lea	64($tptr),$tptr
	jmp	.Lsqr4x_shift_n_add

.align	32
.Lsqr4x_shift_n_add:
	lea	($shift,$A0[0],2),$S[0]	# t[2*i]<<1 | shift
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[1]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[1]		# | t[2*i]>>63
	 mov	-16($tptr),$A0[0]	# t[2*i+2]	# prefetch
	mov	$A0[1],$shift		# shift=t[2*i+1]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	 mov	-8($tptr),$A0[1]	# t[2*i+2+1]	# prefetch
	adc	%rax,$S[0]
	 mov	-8($aptr,$i),%rax	# a[i+1]	# prefetch
	mov	$S[0],-32($tptr)
	adc	%rdx,$S[1]

	lea	($shift,$A0[0],2),$S[2]	# t[2*i]<<1 | shift
	 mov	$S[1],-24($tptr)
	 sbb	$carry,$carry		# mov cf,$carry
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[3]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[3]		# | t[2*i]>>63
	 mov	0($tptr),$A0[0]		# t[2*i+2]	# prefetch
	mov	$A0[1],$shift		# shift=t[2*i+1]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	 mov	8($tptr),$A0[1]		# t[2*i+2+1]	# prefetch
	adc	%rax,$S[2]
	 mov	0($aptr,$i),%rax	# a[i+1]	# prefetch
	mov	$S[2],-16($tptr)
	adc	%rdx,$S[3]

	lea	($shift,$A0[0],2),$S[0]	# t[2*i]<<1 | shift
	 mov	$S[3],-8($tptr)
	 sbb	$carry,$carry		# mov cf,$carry
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[1]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[1]		# | t[2*i]>>63
	 mov	16($tptr),$A0[0]	# t[2*i+2]	# prefetch
	mov	$A0[1],$shift		# shift=t[2*i+1]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	 mov	24($tptr),$A0[1]	# t[2*i+2+1]	# prefetch
	adc	%rax,$S[0]
	 mov	8($aptr,$i),%rax	# a[i+1]	# prefetch
	mov	$S[0],0($tptr)
	adc	%rdx,$S[1]

	lea	($shift,$A0[0],2),$S[2]	# t[2*i]<<1 | shift
	 mov	$S[1],8($tptr)
	 sbb	$carry,$carry		# mov cf,$carry
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[3]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[3]		# | t[2*i]>>63
	 mov	32($tptr),$A0[0]	# t[2*i+2]	# prefetch
	mov	$A0[1],$shift		# shift=t[2*i+1]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	 mov	40($tptr),$A0[1]	# t[2*i+2+1]	# prefetch
	adc	%rax,$S[2]
	 mov	16($aptr,$i),%rax	# a[i+1]	# prefetch
	mov	$S[2],16($tptr)
	adc	%rdx,$S[3]
	mov	$S[3],24($tptr)
	sbb	$carry,$carry		# mov cf,$carry
	lea	64($tptr),$tptr
	add	\$32,$i
	jnz	.Lsqr4x_shift_n_add

	lea	($shift,$A0[0],2),$S[0]	# t[2*i]<<1 | shift
	.byte	0x67
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[1]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[1]		# | t[2*i]>>63
	 mov	-16($tptr),$A0[0]	# t[2*i+2]	# prefetch
	mov	$A0[1],$shift		# shift=t[2*i+1]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	 mov	-8($tptr),$A0[1]	# t[2*i+2+1]	# prefetch
	adc	%rax,$S[0]
	 mov	-8($aptr),%rax		# a[i+1]	# prefetch
	mov	$S[0],-32($tptr)
	adc	%rdx,$S[1]

	lea	($shift,$A0[0],2),$S[2]	# t[2*i]<<1|shift
	 mov	$S[1],-24($tptr)
	 sbb	$carry,$carry		# mov cf,$carry
	shr	\$63,$A0[0]
	lea	($j,$A0[1],2),$S[3]	# t[2*i+1]<<1 |
	shr	\$63,$A0[1]
	or	$A0[0],$S[3]		# | t[2*i]>>63
	mul	%rax			# a[i]*a[i]
	neg	$carry			# mov $carry,cf
	adc	%rax,$S[2]
	adc	%rdx,$S[3]
	mov	$S[2],-16($tptr)
	mov	$S[3],-8($tptr)
___
}
######################################################################
# Montgomery reduction part, "word-by-word" algorithm.
#
# This new path is inspired by multiple submissions from Intel, by
# Shay Gueron, Vlad Krasnov, Erdinc Ozturk, James Guilford,
# Vinodh Gopal...
{
my ($nptr,$tptr,$carry,$m0)=("%rbp","%rdi","%rsi","%rbx");

$code.=<<___;
	movq	%xmm2,$nptr
sqr8x_reduction:
	xor	%rax,%rax
	lea	($nptr,$num,2),%rcx	# end of n[]
	lea	48+8(%rsp,$num,2),%rdx	# end of t[] buffer
	mov	%rcx,0+8(%rsp)
	lea	48+8(%rsp,$num),$tptr	# end of initial t[] window
	mov	%rdx,8+8(%rsp)
	neg	$num
	jmp	.L8x_reduction_loop

.align	32
.L8x_reduction_loop:
	lea	($tptr,$num),$tptr	# start of current t[] window
	.byte	0x66
	mov	8*0($tptr),$m0
	mov	8*1($tptr),%r9
	mov	8*2($tptr),%r10
	mov	8*3($tptr),%r11
	mov	8*4($tptr),%r12
	mov	8*5($tptr),%r13
	mov	8*6($tptr),%r14
	mov	8*7($tptr),%r15
	mov	%rax,(%rdx)		# store top-most carry bit
	lea	8*8($tptr),$tptr

	.byte	0x67
	mov	$m0,%r8
	imulq	32+8(%rsp),$m0		# n0*a[0]
	mov	16*0($nptr),%rax	# n[0]
	mov	\$8,%ecx
	jmp	.L8x_reduce

.align	32
.L8x_reduce:
	mulq	$m0
	 mov	16*1($nptr),%rax	# n[1]
	neg	%r8
	mov	%rdx,%r8
	adc	\$0,%r8

	mulq	$m0
	add	%rax,%r9
	 mov	16*2($nptr),%rax
	adc	\$0,%rdx
	add	%r9,%r8
	 mov	$m0,48-8+8(%rsp,%rcx,8)	# put aside n0*a[i]
	mov	%rdx,%r9
	adc	\$0,%r9

	mulq	$m0
	add	%rax,%r10
	 mov	16*3($nptr),%rax
	adc	\$0,%rdx
	add	%r10,%r9
	 mov	32+8(%rsp),$carry	# pull n0, borrow $carry
	mov	%rdx,%r10
	adc	\$0,%r10

	mulq	$m0
	add	%rax,%r11
	 mov	16*4($nptr),%rax
	adc	\$0,%rdx
	 imulq	%r8,$carry		# modulo-scheduled
	add	%r11,%r10
	mov	%rdx,%r11
	adc	\$0,%r11

	mulq	$m0
	add	%rax,%r12
	 mov	16*5($nptr),%rax
	adc	\$0,%rdx
	add	%r12,%r11
	mov	%rdx,%r12
	adc	\$0,%r12

	mulq	$m0
	add	%rax,%r13
	 mov	16*6($nptr),%rax
	adc	\$0,%rdx
	add	%r13,%r12
	mov	%rdx,%r13
	adc	\$0,%r13

	mulq	$m0
	add	%rax,%r14
	 mov	16*7($nptr),%rax
	adc	\$0,%rdx
	add	%r14,%r13
	mov	%rdx,%r14
	adc	\$0,%r14

	mulq	$m0
	 mov	$carry,$m0		# n0*a[i]
	add	%rax,%r15
	 mov	16*0($nptr),%rax	# n[0]
	adc	\$0,%rdx
	add	%r15,%r14
	mov	%rdx,%r15
	adc	\$0,%r15

	dec	%ecx
	jnz	.L8x_reduce

	lea	16*8($nptr),$nptr
	xor	%rax,%rax
	mov	8+8(%rsp),%rdx		# pull end of t[]
	cmp	0+8(%rsp),$nptr		# end of n[]?
	jae	.L8x_no_tail

	.byte	0x66
	add	8*0($tptr),%r8
	adc	8*1($tptr),%r9
	adc	8*2($tptr),%r10
	adc	8*3($tptr),%r11
	adc	8*4($tptr),%r12
	adc	8*5($tptr),%r13
	adc	8*6($tptr),%r14
	adc	8*7($tptr),%r15
	sbb	$carry,$carry		# top carry

	mov	48+56+8(%rsp),$m0	# pull n0*a[0]
	mov	\$8,%ecx
	mov	16*0($nptr),%rax
	jmp	.L8x_tail

.align	32
.L8x_tail:
	mulq	$m0
	add	%rax,%r8
	 mov	16*1($nptr),%rax
	 mov	%r8,($tptr)		# save result
	mov	%rdx,%r8
	adc	\$0,%r8

	mulq	$m0
	add	%rax,%r9
	 mov	16*2($nptr),%rax
	adc	\$0,%rdx
	add	%r9,%r8
	 lea	8($tptr),$tptr		# $tptr++
	mov	%rdx,%r9
	adc	\$0,%r9

	mulq	$m0
	add	%rax,%r10
	 mov	16*3($nptr),%rax
	adc	\$0,%rdx
	add	%r10,%r9
	mov	%rdx,%r10
	adc	\$0,%r10

	mulq	$m0
	add	%rax,%r11
	 mov	16*4($nptr),%rax
	adc	\$0,%rdx
	add	%r11,%r10
	mov	%rdx,%r11
	adc	\$0,%r11

	mulq	$m0
	add	%rax,%r12
	 mov	16*5($nptr),%rax
	adc	\$0,%rdx
	add	%r12,%r11
	mov	%rdx,%r12
	adc	\$0,%r12

	mulq	$m0
	add	%rax,%r13
	 mov	16*6($nptr),%rax
	adc	\$0,%rdx
	add	%r13,%r12
	mov	%rdx,%r13
	adc	\$0,%r13

	mulq	$m0
	add	%rax,%r14
	 mov	16*7($nptr),%rax
	adc	\$0,%rdx
	add	%r14,%r13
	mov	%rdx,%r14
	adc	\$0,%r14

	mulq	$m0
	 mov	48-16+8(%rsp,%rcx,8),$m0# pull n0*a[i]
	add	%rax,%r15
	adc	\$0,%rdx
	add	%r15,%r14
	 mov	16*0($nptr),%rax	# pull n[0]
	mov	%rdx,%r15
	adc	\$0,%r15

	dec	%ecx
	jnz	.L8x_tail

	lea	16*8($nptr),$nptr
	mov	8+8(%rsp),%rdx		# pull end of t[]
	cmp	0+8(%rsp),$nptr		# end of n[]?
	jae	.L8x_tail_done		# break out of loop

	 mov	48+56+8(%rsp),$m0	# pull n0*a[0]
	neg	$carry
	 mov	8*0($nptr),%rax		# pull n[0]
	adc	8*0($tptr),%r8
	adc	8*1($tptr),%r9
	adc	8*2($tptr),%r10
	adc	8*3($tptr),%r11
	adc	8*4($tptr),%r12
	adc	8*5($tptr),%r13
	adc	8*6($tptr),%r14
	adc	8*7($tptr),%r15
	sbb	$carry,$carry		# top carry

	mov	\$8,%ecx
	jmp	.L8x_tail

.align	32
.L8x_tail_done:
	add	(%rdx),%r8		# can this overflow?
	adc	\$0,%r9
	adc	\$0,%r10
	adc	\$0,%r11
	adc	\$0,%r12
	adc	\$0,%r13
	adc	\$0,%r14
	adc	\$0,%r15		# can't overflow, because we
					# started with "overhung" part
					# of multiplication
	xor	%rax,%rax

	neg	$carry
.L8x_no_tail:
	adc	8*0($tptr),%r8
	adc	8*1($tptr),%r9
	adc	8*2($tptr),%r10
	adc	8*3($tptr),%r11
	adc	8*4($tptr),%r12
	adc	8*5($tptr),%r13
	adc	8*6($tptr),%r14
	adc	8*7($tptr),%r15
	adc	\$0,%rax		# top-most carry
	 mov	-16($nptr),%rcx		# np[num-1]
	 xor	$carry,$carry

	movq	%xmm2,$nptr		# restore $nptr

	mov	%r8,8*0($tptr)		# store top 512 bits
	mov	%r9,8*1($tptr)
	 movq	%xmm3,$num		# $num is %r9, can't be moved upwards
	mov	%r10,8*2($tptr)
	mov	%r11,8*3($tptr)
	mov	%r12,8*4($tptr)
	mov	%r13,8*5($tptr)
	mov	%r14,8*6($tptr)
	mov	%r15,8*7($tptr)
	lea	8*8($tptr),$tptr

	cmp	%rdx,$tptr		# end of t[]?
	jb	.L8x_reduction_loop
___
}
##############################################################
# Post-condition, 4x unrolled
#
{
my ($tptr,$nptr)=("%rbx","%rbp");
$code.=<<___;
	#xor	%rsi,%rsi		# %rsi was $carry above
	sub	%r15,%rcx		# compare top-most words
	lea	(%rdi,$num),$tptr	# %rdi was $tptr above
	adc	%rsi,%rsi
	mov	$num,%rcx
	or	%rsi,%rax
	movq	%xmm1,$rptr		# restore $rptr
	xor	\$1,%rax
	movq	%xmm1,$aptr		# prepare for back-to-back call
	lea	($nptr,%rax,8),$nptr
	sar	\$3+2,%rcx		# cf=0
	jmp	.Lsqr4x_sub

.align	32
.Lsqr4x_sub:
	.byte	0x66
	mov	8*0($tptr),%r12
	mov	8*1($tptr),%r13
	sbb	16*0($nptr),%r12
	mov	8*2($tptr),%r14
	sbb	16*1($nptr),%r13
	mov	8*3($tptr),%r15
	lea	8*4($tptr),$tptr
	sbb	16*2($nptr),%r14
	mov	%r12,8*0($rptr)
	sbb	16*3($nptr),%r15
	lea	16*4($nptr),$nptr
	mov	%r13,8*1($rptr)
	mov	%r14,8*2($rptr)
	mov	%r15,8*3($rptr)
	lea	8*4($rptr),$rptr

	inc	%rcx			# pass %cf
	jnz	.Lsqr4x_sub
___
}
$code.=<<___;
	mov	$num,%r10		# prepare for back-to-back call
	neg	$num			# restore $num	
	ret
.size	bn_sqr8x_internal,.-bn_sqr8x_internal
___
{
$code.=<<___;
.globl	bn_from_montgomery
.type	bn_from_montgomery,\@abi-omnipotent
.align	32
bn_from_montgomery:
	testl	\$7,`($win64?"48(%rsp)":"%r9d")`
	jz	bn_from_mont8x
	xor	%eax,%eax
	ret
.size	bn_from_montgomery,.-bn_from_montgomery

.type	bn_from_mont8x,\@function,6
.align	32
bn_from_mont8x:
	.byte	0x67
	mov	%rsp,%rax
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
___
$code.=<<___ if ($win64);
	lea	-0x28(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
___
$code.=<<___;
	.byte	0x67
	mov	${num}d,%r10d
	shl	\$3,${num}d		# convert $num to bytes
	shl	\$3+2,%r10d		# 4*$num
	neg	$num
	mov	($n0),$n0		# *n0

	##############################################################
	# ensure that stack frame doesn't alias with $aptr+4*$num
	# modulo 4096, which covers ret[num], am[num] and n[2*num]
	# (see bn_exp.c). this is done to allow memory disambiguation
	# logic do its magic.
	#
	lea	-64(%rsp,$num,2),%r11
	sub	$aptr,%r11
	and	\$4095,%r11
	cmp	%r11,%r10
	jb	.Lfrom_sp_alt
	sub	%r11,%rsp		# align with $aptr
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+2*$num)
	jmp	.Lfrom_sp_done

.align	32
.Lfrom_sp_alt:
	lea	4096-64(,$num,2),%r10	# 4096-frame-2*$num
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+2*$num)
	sub	%r10,%r11
	mov	\$0,%r10
	cmovc	%r10,%r11
	sub	%r11,%rsp
.Lfrom_sp_done:
	and	\$-64,%rsp
	mov	$num,%r10	
	neg	$num

	##############################################################
	# Stack layout
	#
	# +0	saved $num, used in reduction section
	# +8	&t[2*$num], used in reduction section
	# +32	saved *n0
	# +40	saved %rsp
	# +48	t[2*$num]
	#
	mov	$n0,  32(%rsp)
	mov	%rax, 40(%rsp)		# save original %rsp
.Lfrom_body:
	mov	$num,%r11
	lea	48(%rsp),%rax
	pxor	%xmm0,%xmm0
	jmp	.Lmul_by_1

.align	32
.Lmul_by_1:
	movdqu	($aptr),%xmm1
	movdqu	16($aptr),%xmm2
	movdqu	32($aptr),%xmm3
	movdqa	%xmm0,(%rax,$num)
	movdqu	48($aptr),%xmm4
	movdqa	%xmm0,16(%rax,$num)
	.byte	0x48,0x8d,0xb6,0x40,0x00,0x00,0x00	# lea	64($aptr),$aptr
	movdqa	%xmm1,(%rax)
	movdqa	%xmm0,32(%rax,$num)
	movdqa	%xmm2,16(%rax)
	movdqa	%xmm0,48(%rax,$num)
	movdqa	%xmm3,32(%rax)
	movdqa	%xmm4,48(%rax)
	lea	64(%rax),%rax
	sub	\$64,%r11
	jnz	.Lmul_by_1

	movq	$rptr,%xmm1
	movq	$nptr,%xmm2
	.byte	0x67
	mov	$nptr,%rbp
	movq	%r10, %xmm3		# -num
___
$code.=<<___ if ($addx);
	mov	OPENSSL_ia32cap_P+8(%rip),%r11d
	and	\$0x80100,%r11d
	cmp	\$0x80100,%r11d
	jne	.Lfrom_mont_nox

	lea	(%rax,$num),$rptr
	call	sqrx8x_reduction

	pxor	%xmm0,%xmm0
	lea	48(%rsp),%rax
	mov	40(%rsp),%rsi		# restore %rsp
	jmp	.Lfrom_mont_zero

.align	32
.Lfrom_mont_nox:
___
$code.=<<___;
	call	sqr8x_reduction

	pxor	%xmm0,%xmm0
	lea	48(%rsp),%rax
	mov	40(%rsp),%rsi		# restore %rsp
	jmp	.Lfrom_mont_zero

.align	32
.Lfrom_mont_zero:
	movdqa	%xmm0,16*0(%rax)
	movdqa	%xmm0,16*1(%rax)
	movdqa	%xmm0,16*2(%rax)
	movdqa	%xmm0,16*3(%rax)
	lea	16*4(%rax),%rax
	sub	\$32,$num
	jnz	.Lfrom_mont_zero

	mov	\$1,%rax
	mov	-48(%rsi),%r15
	mov	-40(%rsi),%r14
	mov	-32(%rsi),%r13
	mov	-24(%rsi),%r12
	mov	-16(%rsi),%rbp
	mov	-8(%rsi),%rbx
	lea	(%rsi),%rsp
.Lfrom_epilogue:
	ret
.size	bn_from_mont8x,.-bn_from_mont8x
___
}
}}}

if ($addx) {{{
my $bp="%rdx";	# restore original value

$code.=<<___;
.type	bn_mulx4x_mont_gather5,\@function,6
.align	32
bn_mulx4x_mont_gather5:
.Lmulx4x_enter:
	.byte	0x67
	mov	%rsp,%rax
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
___
$code.=<<___ if ($win64);
	lea	-0x28(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
___
$code.=<<___;
	.byte	0x67
	mov	${num}d,%r10d
	shl	\$3,${num}d		# convert $num to bytes
	shl	\$3+2,%r10d		# 4*$num
	neg	$num			# -$num
	mov	($n0),$n0		# *n0

	##############################################################
	# ensure that stack frame doesn't alias with $aptr+4*$num
	# modulo 4096, which covers a[num], ret[num] and n[2*num]
	# (see bn_exp.c). this is done to allow memory disambiguation
	# logic do its magic. [excessive frame is allocated in order
	# to allow bn_from_mont8x to clear it.]
	#
	lea	-64(%rsp,$num,2),%r11
	sub	$ap,%r11
	and	\$4095,%r11
	cmp	%r11,%r10
	jb	.Lmulx4xsp_alt
	sub	%r11,%rsp		# align with $aptr
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+$num)
	jmp	.Lmulx4xsp_done

.align	32
.Lmulx4xsp_alt:
	lea	4096-64(,$num,2),%r10	# 4096-frame-$num
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+$num)
	sub	%r10,%r11
	mov	\$0,%r10
	cmovc	%r10,%r11
	sub	%r11,%rsp
.Lmulx4xsp_done:	
	and	\$-64,%rsp		# ensure alignment
	##############################################################
	# Stack layout
	# +0	-num
	# +8	off-loaded &b[i]
	# +16	end of b[num]
	# +24	inner counter
	# +32	saved n0
	# +40	saved %rsp
	# +48
	# +56	saved rp
	# +64	tmp[num+1]
	#
	mov	$n0, 32(%rsp)		# save *n0
	mov	%rax,40(%rsp)		# save original %rsp
.Lmulx4x_body:
	call	mulx4x_internal

	mov	40(%rsp),%rsi		# restore %rsp
	mov	\$1,%rax
___
$code.=<<___ if ($win64);
	movaps	-88(%rsi),%xmm6
	movaps	-72(%rsi),%xmm7
___
$code.=<<___;
	mov	-48(%rsi),%r15
	mov	-40(%rsi),%r14
	mov	-32(%rsi),%r13
	mov	-24(%rsi),%r12
	mov	-16(%rsi),%rbp
	mov	-8(%rsi),%rbx
	lea	(%rsi),%rsp
.Lmulx4x_epilogue:
	ret
.size	bn_mulx4x_mont_gather5,.-bn_mulx4x_mont_gather5

.type	mulx4x_internal,\@abi-omnipotent
.align	32
mulx4x_internal:
	.byte	0x4c,0x89,0x8c,0x24,0x08,0x00,0x00,0x00	# mov	$num,8(%rsp)		# save -$num
	.byte	0x67
	neg	$num			# restore $num
	shl	\$5,$num
	lea	256($bp,$num),%r13
	shr	\$5+5,$num
	mov	`($win64?56:8)`(%rax),%r10d	# load 7th argument
	sub	\$1,$num
	mov	%r13,16+8(%rsp)		# end of b[num]
	mov	$num,24+8(%rsp)		# inner counter
	mov	$rp, 56+8(%rsp)		# save $rp
___
my ($aptr, $bptr, $nptr, $tptr, $mi,  $bi,  $zero, $num)=
   ("%rsi","%rdi","%rcx","%rbx","%r8","%r9","%rbp","%rax");
my $rptr=$bptr;
my $STRIDE=2**5*8;		# 5 is "window size"
my $N=$STRIDE/4;		# should match cache line size
$code.=<<___;
	mov	%r10,%r11
	shr	\$`log($N/8)/log(2)`,%r10
	and	\$`$N/8-1`,%r11
	not	%r10
	lea	.Lmagic_masks(%rip),%rax
	and	\$`2**5/($N/8)-1`,%r10	# 5 is "window size"
	lea	96($bp,%r11,8),$bptr	# pointer within 1st cache line
	movq	0(%rax,%r10,8),%xmm4	# set of masks denoting which
	movq	8(%rax,%r10,8),%xmm5	# cache line contains element
	add	\$7,%r11
	movq	16(%rax,%r10,8),%xmm6	# denoted by 7th argument
	movq	24(%rax,%r10,8),%xmm7
	and	\$7,%r11

	movq	`0*$STRIDE/4-96`($bptr),%xmm0
	lea	$STRIDE($bptr),$tptr	# borrow $tptr
	movq	`1*$STRIDE/4-96`($bptr),%xmm1
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-96`($bptr),%xmm2
	pand	%xmm5,%xmm1
	movq	`3*$STRIDE/4-96`($bptr),%xmm3
	pand	%xmm6,%xmm2
	por	%xmm1,%xmm0
	movq	`0*$STRIDE/4-96`($tptr),%xmm1
	pand	%xmm7,%xmm3
	por	%xmm2,%xmm0
	movq	`1*$STRIDE/4-96`($tptr),%xmm2
	por	%xmm3,%xmm0
	.byte	0x67,0x67
	pand	%xmm4,%xmm1
	movq	`2*$STRIDE/4-96`($tptr),%xmm3

	movq	%xmm0,%rdx		# bp[0]
	movq	`3*$STRIDE/4-96`($tptr),%xmm0
	lea	2*$STRIDE($bptr),$bptr	# next &b[i]
	pand	%xmm5,%xmm2
	.byte	0x67,0x67
	pand	%xmm6,%xmm3
	##############################################################
	# $tptr is chosen so that writing to top-most element of the
	# vector occurs just "above" references to powers table,
	# "above" modulo cache-line size, which effectively precludes
	# possibility of memory disambiguation logic failure when
	# accessing the table.
	# 
	lea	64+8*4+8(%rsp,%r11,8),$tptr

	mov	%rdx,$bi
	mulx	0*8($aptr),$mi,%rax	# a[0]*b[0]
	mulx	1*8($aptr),%r11,%r12	# a[1]*b[0]
	add	%rax,%r11
	mulx	2*8($aptr),%rax,%r13	# ...
	adc	%rax,%r12
	adc	\$0,%r13
	mulx	3*8($aptr),%rax,%r14

	mov	$mi,%r15
	imulq	32+8(%rsp),$mi		# "t[0]"*n0
	xor	$zero,$zero		# cf=0, of=0
	mov	$mi,%rdx

	por	%xmm2,%xmm1
	pand	%xmm7,%xmm0
	por	%xmm3,%xmm1
	mov	$bptr,8+8(%rsp)		# off-load &b[i]
	por	%xmm1,%xmm0

	.byte	0x48,0x8d,0xb6,0x20,0x00,0x00,0x00	# lea	4*8($aptr),$aptr
	adcx	%rax,%r13
	adcx	$zero,%r14		# cf=0

	mulx	0*16($nptr),%rax,%r10
	adcx	%rax,%r15		# discarded
	adox	%r11,%r10
	mulx	1*16($nptr),%rax,%r11
	adcx	%rax,%r10
	adox	%r12,%r11
	mulx	2*16($nptr),%rax,%r12
	mov	24+8(%rsp),$bptr	# counter value
	.byte	0x66
	mov	%r10,-8*4($tptr)
	adcx	%rax,%r11
	adox	%r13,%r12
	mulx	3*16($nptr),%rax,%r15
	 .byte	0x67,0x67
	 mov	$bi,%rdx
	mov	%r11,-8*3($tptr)
	adcx	%rax,%r12
	adox	$zero,%r15		# of=0
	.byte	0x48,0x8d,0x89,0x40,0x00,0x00,0x00	# lea	4*16($nptr),$nptr
	mov	%r12,-8*2($tptr)
	#jmp	.Lmulx4x_1st

.align	32
.Lmulx4x_1st:
	adcx	$zero,%r15		# cf=0, modulo-scheduled
	mulx	0*8($aptr),%r10,%rax	# a[4]*b[0]
	adcx	%r14,%r10
	mulx	1*8($aptr),%r11,%r14	# a[5]*b[0]
	adcx	%rax,%r11
	mulx	2*8($aptr),%r12,%rax	# ...
	adcx	%r14,%r12
	mulx	3*8($aptr),%r13,%r14
	 .byte	0x67,0x67
	 mov	$mi,%rdx
	adcx	%rax,%r13
	adcx	$zero,%r14		# cf=0
	lea	4*8($aptr),$aptr
	lea	4*8($tptr),$tptr

	adox	%r15,%r10
	mulx	0*16($nptr),%rax,%r15
	adcx	%rax,%r10
	adox	%r15,%r11
	mulx	1*16($nptr),%rax,%r15
	adcx	%rax,%r11
	adox	%r15,%r12
	mulx	2*16($nptr),%rax,%r15
	mov	%r10,-5*8($tptr)
	adcx	%rax,%r12
	mov	%r11,-4*8($tptr)
	adox	%r15,%r13
	mulx	3*16($nptr),%rax,%r15
	 mov	$bi,%rdx
	mov	%r12,-3*8($tptr)
	adcx	%rax,%r13
	adox	$zero,%r15
	lea	4*16($nptr),$nptr
	mov	%r13,-2*8($tptr)

	dec	$bptr			# of=0, pass cf
	jnz	.Lmulx4x_1st

	mov	8(%rsp),$num		# load -num
	movq	%xmm0,%rdx		# bp[1]
	adc	$zero,%r15		# modulo-scheduled
	lea	($aptr,$num),$aptr	# rewind $aptr
	add	%r15,%r14
	mov	8+8(%rsp),$bptr		# re-load &b[i]
	adc	$zero,$zero		# top-most carry
	mov	%r14,-1*8($tptr)
	jmp	.Lmulx4x_outer

.align	32
.Lmulx4x_outer:
	mov	$zero,($tptr)		# save top-most carry
	lea	4*8($tptr,$num),$tptr	# rewind $tptr
	mulx	0*8($aptr),$mi,%r11	# a[0]*b[i]
	xor	$zero,$zero		# cf=0, of=0
	mov	%rdx,$bi
	mulx	1*8($aptr),%r14,%r12	# a[1]*b[i]
	adox	-4*8($tptr),$mi		# +t[0]
	adcx	%r14,%r11
	mulx	2*8($aptr),%r15,%r13	# ...
	adox	-3*8($tptr),%r11
	adcx	%r15,%r12
	mulx	3*8($aptr),%rdx,%r14
	adox	-2*8($tptr),%r12
	adcx	%rdx,%r13
	lea	($nptr,$num,2),$nptr	# rewind $nptr
	lea	4*8($aptr),$aptr
	adox	-1*8($tptr),%r13
	adcx	$zero,%r14
	adox	$zero,%r14

	.byte	0x67
	mov	$mi,%r15
	imulq	32+8(%rsp),$mi		# "t[0]"*n0

	movq	`0*$STRIDE/4-96`($bptr),%xmm0
	.byte	0x67,0x67
	mov	$mi,%rdx
	movq	`1*$STRIDE/4-96`($bptr),%xmm1
	.byte	0x67
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-96`($bptr),%xmm2
	.byte	0x67
	pand	%xmm5,%xmm1
	movq	`3*$STRIDE/4-96`($bptr),%xmm3
	add	\$$STRIDE,$bptr		# next &b[i]
	.byte	0x67
	pand	%xmm6,%xmm2
	por	%xmm1,%xmm0
	pand	%xmm7,%xmm3
	xor	$zero,$zero		# cf=0, of=0
	mov	$bptr,8+8(%rsp)		# off-load &b[i]

	mulx	0*16($nptr),%rax,%r10
	adcx	%rax,%r15		# discarded
	adox	%r11,%r10
	mulx	1*16($nptr),%rax,%r11
	adcx	%rax,%r10
	adox	%r12,%r11
	mulx	2*16($nptr),%rax,%r12
	adcx	%rax,%r11
	adox	%r13,%r12
	mulx	3*16($nptr),%rax,%r15
	 mov	$bi,%rdx
	 por	%xmm2,%xmm0
	mov	24+8(%rsp),$bptr	# counter value
	mov	%r10,-8*4($tptr)
	 por	%xmm3,%xmm0
	adcx	%rax,%r12
	mov	%r11,-8*3($tptr)
	adox	$zero,%r15		# of=0
	mov	%r12,-8*2($tptr)
	lea	4*16($nptr),$nptr
	jmp	.Lmulx4x_inner

.align	32
.Lmulx4x_inner:
	mulx	0*8($aptr),%r10,%rax	# a[4]*b[i]
	adcx	$zero,%r15		# cf=0, modulo-scheduled
	adox	%r14,%r10
	mulx	1*8($aptr),%r11,%r14	# a[5]*b[i]
	adcx	0*8($tptr),%r10
	adox	%rax,%r11
	mulx	2*8($aptr),%r12,%rax	# ...
	adcx	1*8($tptr),%r11
	adox	%r14,%r12
	mulx	3*8($aptr),%r13,%r14
	 mov	$mi,%rdx
	adcx	2*8($tptr),%r12
	adox	%rax,%r13
	adcx	3*8($tptr),%r13
	adox	$zero,%r14		# of=0
	lea	4*8($aptr),$aptr
	lea	4*8($tptr),$tptr
	adcx	$zero,%r14		# cf=0

	adox	%r15,%r10
	mulx	0*16($nptr),%rax,%r15
	adcx	%rax,%r10
	adox	%r15,%r11
	mulx	1*16($nptr),%rax,%r15
	adcx	%rax,%r11
	adox	%r15,%r12
	mulx	2*16($nptr),%rax,%r15
	mov	%r10,-5*8($tptr)
	adcx	%rax,%r12
	adox	%r15,%r13
	mov	%r11,-4*8($tptr)
	mulx	3*16($nptr),%rax,%r15
	 mov	$bi,%rdx
	lea	4*16($nptr),$nptr
	mov	%r12,-3*8($tptr)
	adcx	%rax,%r13
	adox	$zero,%r15
	mov	%r13,-2*8($tptr)

	dec	$bptr			# of=0, pass cf
	jnz	.Lmulx4x_inner

	mov	0+8(%rsp),$num		# load -num
	movq	%xmm0,%rdx		# bp[i+1]
	adc	$zero,%r15		# modulo-scheduled
	sub	0*8($tptr),$bptr	# pull top-most carry to %cf
	mov	8+8(%rsp),$bptr		# re-load &b[i]
	mov	16+8(%rsp),%r10
	adc	%r15,%r14
	lea	($aptr,$num),$aptr	# rewind $aptr
	adc	$zero,$zero		# top-most carry
	mov	%r14,-1*8($tptr)

	cmp	%r10,$bptr
	jb	.Lmulx4x_outer

	mov	-16($nptr),%r10
	xor	%r15,%r15
	sub	%r14,%r10		# compare top-most words
	adc	%r15,%r15
	or	%r15,$zero
	xor	\$1,$zero
	lea	($tptr,$num),%rdi	# rewind $tptr
	lea	($nptr,$num,2),$nptr	# rewind $nptr
	.byte	0x67,0x67
	sar	\$3+2,$num		# cf=0
	lea	($nptr,$zero,8),%rbp
	mov	56+8(%rsp),%rdx		# restore rp
	mov	$num,%rcx
	jmp	.Lsqrx4x_sub		# common post-condition
.size	mulx4x_internal,.-mulx4x_internal
___
}{
######################################################################
# void bn_power5(
my $rptr="%rdi";	# BN_ULONG *rptr,
my $aptr="%rsi";	# const BN_ULONG *aptr,
my $bptr="%rdx";	# const void *table,
my $nptr="%rcx";	# const BN_ULONG *nptr,
my $n0  ="%r8";		# const BN_ULONG *n0);
my $num ="%r9";		# int num, has to be divisible by 8
			# int pwr);

my ($i,$j,$tptr)=("%rbp","%rcx",$rptr);
my @A0=("%r10","%r11");
my @A1=("%r12","%r13");
my ($a0,$a1,$ai)=("%r14","%r15","%rbx");

$code.=<<___;
.type	bn_powerx5,\@function,6
.align	32
bn_powerx5:
.Lpowerx5_enter:
	.byte	0x67
	mov	%rsp,%rax
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
___
$code.=<<___ if ($win64);
	lea	-0x28(%rsp),%rsp
	movaps	%xmm6,(%rsp)
	movaps	%xmm7,0x10(%rsp)
___
$code.=<<___;
	.byte	0x67
	mov	${num}d,%r10d
	shl	\$3,${num}d		# convert $num to bytes
	shl	\$3+2,%r10d		# 4*$num
	neg	$num
	mov	($n0),$n0		# *n0

	##############################################################
	# ensure that stack frame doesn't alias with $aptr+4*$num
	# modulo 4096, which covers ret[num], am[num] and n[2*num]
	# (see bn_exp.c). this is done to allow memory disambiguation
	# logic do its magic.
	#
	lea	-64(%rsp,$num,2),%r11
	sub	$aptr,%r11
	and	\$4095,%r11
	cmp	%r11,%r10
	jb	.Lpwrx_sp_alt
	sub	%r11,%rsp		# align with $aptr
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+2*$num)
	jmp	.Lpwrx_sp_done

.align	32
.Lpwrx_sp_alt:
	lea	4096-64(,$num,2),%r10	# 4096-frame-2*$num
	lea	-64(%rsp,$num,2),%rsp	# alloca(frame+2*$num)
	sub	%r10,%r11
	mov	\$0,%r10
	cmovc	%r10,%r11
	sub	%r11,%rsp
.Lpwrx_sp_done:
	and	\$-64,%rsp
	mov	$num,%r10	
	neg	$num

	##############################################################
	# Stack layout
	#
	# +0	saved $num, used in reduction section
	# +8	&t[2*$num], used in reduction section
	# +16	intermediate carry bit
	# +24	top-most carry bit, used in reduction section
	# +32	saved *n0
	# +40	saved %rsp
	# +48	t[2*$num]
	#
	pxor	%xmm0,%xmm0
	movq	$rptr,%xmm1		# save $rptr
	movq	$nptr,%xmm2		# save $nptr
	movq	%r10, %xmm3		# -$num
	movq	$bptr,%xmm4
	mov	$n0,  32(%rsp)
	mov	%rax, 40(%rsp)		# save original %rsp
.Lpowerx5_body:

	call	__bn_sqrx8x_internal
	call	__bn_sqrx8x_internal
	call	__bn_sqrx8x_internal
	call	__bn_sqrx8x_internal
	call	__bn_sqrx8x_internal

	mov	%r10,$num		# -num
	mov	$aptr,$rptr
	movq	%xmm2,$nptr
	movq	%xmm4,$bptr
	mov	40(%rsp),%rax

	call	mulx4x_internal

	mov	40(%rsp),%rsi		# restore %rsp
	mov	\$1,%rax
___
$code.=<<___ if ($win64);
	movaps	-88(%rsi),%xmm6
	movaps	-72(%rsi),%xmm7
___
$code.=<<___;
	mov	-48(%rsi),%r15
	mov	-40(%rsi),%r14
	mov	-32(%rsi),%r13
	mov	-24(%rsi),%r12
	mov	-16(%rsi),%rbp
	mov	-8(%rsi),%rbx
	lea	(%rsi),%rsp
.Lpowerx5_epilogue:
	ret
.size	bn_powerx5,.-bn_powerx5

.globl	bn_sqrx8x_internal
.hidden	bn_sqrx8x_internal
.type	bn_sqrx8x_internal,\@abi-omnipotent
.align	32
bn_sqrx8x_internal:
__bn_sqrx8x_internal:
	##################################################################
	# Squaring part:
	#
	# a) multiply-n-add everything but a[i]*a[i];
	# b) shift result of a) by 1 to the left and accumulate
	#    a[i]*a[i] products;
	#
	##################################################################
	# a[7]a[7]a[6]a[6]a[5]a[5]a[4]a[4]a[3]a[3]a[2]a[2]a[1]a[1]a[0]a[0]
	#                                                     a[1]a[0]
	#                                                 a[2]a[0]
	#                                             a[3]a[0]
	#                                             a[2]a[1]
	#                                         a[3]a[1]
	#                                     a[3]a[2]
	#
	#                                         a[4]a[0]
	#                                     a[5]a[0]
	#                                 a[6]a[0]
	#                             a[7]a[0]
	#                                     a[4]a[1]
	#                                 a[5]a[1]
	#                             a[6]a[1]
	#                         a[7]a[1]
	#                                 a[4]a[2]
	#                             a[5]a[2]
	#                         a[6]a[2]
	#                     a[7]a[2]
	#                             a[4]a[3]
	#                         a[5]a[3]
	#                     a[6]a[3]
	#                 a[7]a[3]
	#
	#                     a[5]a[4]
	#                 a[6]a[4]
	#             a[7]a[4]
	#             a[6]a[5]
	#         a[7]a[5]
	#     a[7]a[6]
	# a[7]a[7]a[6]a[6]a[5]a[5]a[4]a[4]a[3]a[3]a[2]a[2]a[1]a[1]a[0]a[0]
___
{
my ($zero,$carry)=("%rbp","%rcx");
my $aaptr=$zero;
$code.=<<___;
	lea	48+8(%rsp),$tptr
	lea	($aptr,$num),$aaptr
	mov	$num,0+8(%rsp)			# save $num
	mov	$aaptr,8+8(%rsp)		# save end of $aptr
	jmp	.Lsqr8x_zero_start

.align	32
.byte	0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00
.Lsqrx8x_zero:
	.byte	0x3e
	movdqa	%xmm0,0*8($tptr)
	movdqa	%xmm0,2*8($tptr)
	movdqa	%xmm0,4*8($tptr)
	movdqa	%xmm0,6*8($tptr)
.Lsqr8x_zero_start:			# aligned at 32
	movdqa	%xmm0,8*8($tptr)
	movdqa	%xmm0,10*8($tptr)
	movdqa	%xmm0,12*8($tptr)
	movdqa	%xmm0,14*8($tptr)
	lea	16*8($tptr),$tptr
	sub	\$64,$num
	jnz	.Lsqrx8x_zero

	mov	0*8($aptr),%rdx		# a[0], modulo-scheduled
	#xor	%r9,%r9			# t[1], ex-$num, zero already
	xor	%r10,%r10
	xor	%r11,%r11
	xor	%r12,%r12
	xor	%r13,%r13
	xor	%r14,%r14
	xor	%r15,%r15
	lea	48+8(%rsp),$tptr
	xor	$zero,$zero		# cf=0, cf=0
	jmp	.Lsqrx8x_outer_loop

.align	32
.Lsqrx8x_outer_loop:
	mulx	1*8($aptr),%r8,%rax	# a[1]*a[0]
	adcx	%r9,%r8			# a[1]*a[0]+=t[1]
	adox	%rax,%r10
	mulx	2*8($aptr),%r9,%rax	# a[2]*a[0]
	adcx	%r10,%r9
	adox	%rax,%r11
	.byte	0xc4,0xe2,0xab,0xf6,0x86,0x18,0x00,0x00,0x00	# mulx	3*8($aptr),%r10,%rax	# ...
	adcx	%r11,%r10
	adox	%rax,%r12
	.byte	0xc4,0xe2,0xa3,0xf6,0x86,0x20,0x00,0x00,0x00	# mulx	4*8($aptr),%r11,%rax
	adcx	%r12,%r11
	adox	%rax,%r13
	mulx	5*8($aptr),%r12,%rax
	adcx	%r13,%r12
	adox	%rax,%r14
	mulx	6*8($aptr),%r13,%rax
	adcx	%r14,%r13
	adox	%r15,%rax
	mulx	7*8($aptr),%r14,%r15
	 mov	1*8($aptr),%rdx		# a[1]
	adcx	%rax,%r14
	adox	$zero,%r15
	adc	8*8($tptr),%r15
	mov	%r8,1*8($tptr)		# t[1]
	mov	%r9,2*8($tptr)		# t[2]
	sbb	$carry,$carry		# mov %cf,$carry
	xor	$zero,$zero		# cf=0, of=0


	mulx	2*8($aptr),%r8,%rbx	# a[2]*a[1]
	mulx	3*8($aptr),%r9,%rax	# a[3]*a[1]
	adcx	%r10,%r8
	adox	%rbx,%r9
	mulx	4*8($aptr),%r10,%rbx	# ...
	adcx	%r11,%r9
	adox	%rax,%r10
	.byte	0xc4,0xe2,0xa3,0xf6,0x86,0x28,0x00,0x00,0x00	# mulx	5*8($aptr),%r11,%rax
	adcx	%r12,%r10
	adox	%rbx,%r11
	.byte	0xc4,0xe2,0x9b,0xf6,0x9e,0x30,0x00,0x00,0x00	# mulx	6*8($aptr),%r12,%rbx
	adcx	%r13,%r11
	adox	%r14,%r12
	.byte	0xc4,0x62,0x93,0xf6,0xb6,0x38,0x00,0x00,0x00	# mulx	7*8($aptr),%r13,%r14
	 mov	2*8($aptr),%rdx		# a[2]
	adcx	%rax,%r12
	adox	%rbx,%r13
	adcx	%r15,%r13
	adox	$zero,%r14		# of=0
	adcx	$zero,%r14		# cf=0

	mov	%r8,3*8($tptr)		# t[3]
	mov	%r9,4*8($tptr)		# t[4]

	mulx	3*8($aptr),%r8,%rbx	# a[3]*a[2]
	mulx	4*8($aptr),%r9,%rax	# a[4]*a[2]
	adcx	%r10,%r8
	adox	%rbx,%r9
	mulx	5*8($aptr),%r10,%rbx	# ...
	adcx	%r11,%r9
	adox	%rax,%r10
	.byte	0xc4,0xe2,0xa3,0xf6,0x86,0x30,0x00,0x00,0x00	# mulx	6*8($aptr),%r11,%rax
	adcx	%r12,%r10
	adox	%r13,%r11
	.byte	0xc4,0x62,0x9b,0xf6,0xae,0x38,0x00,0x00,0x00	# mulx	7*8($aptr),%r12,%r13
	.byte	0x3e
	 mov	3*8($aptr),%rdx		# a[3]
	adcx	%rbx,%r11
	adox	%rax,%r12
	adcx	%r14,%r12
	mov	%r8,5*8($tptr)		# t[5]
	mov	%r9,6*8($tptr)		# t[6]
	 mulx	4*8($aptr),%r8,%rax	# a[4]*a[3]
	adox	$zero,%r13		# of=0
	adcx	$zero,%r13		# cf=0

	mulx	5*8($aptr),%r9,%rbx	# a[5]*a[3]
	adcx	%r10,%r8
	adox	%rax,%r9
	mulx	6*8($aptr),%r10,%rax	# ...
	adcx	%r11,%r9
	adox	%r12,%r10
	mulx	7*8($aptr),%r11,%r12
	 mov	4*8($aptr),%rdx		# a[4]
	 mov	5*8($aptr),%r14		# a[5]
	adcx	%rbx,%r10
	adox	%rax,%r11
	 mov	6*8($aptr),%r15		# a[6]
	adcx	%r13,%r11
	adox	$zero,%r12		# of=0
	adcx	$zero,%r12		# cf=0

	mov	%r8,7*8($tptr)		# t[7]
	mov	%r9,8*8($tptr)		# t[8]

	mulx	%r14,%r9,%rax		# a[5]*a[4]
	 mov	7*8($aptr),%r8		# a[7]
	adcx	%r10,%r9
	mulx	%r15,%r10,%rbx		# a[6]*a[4]
	adox	%rax,%r10
	adcx	%r11,%r10
	mulx	%r8,%r11,%rax		# a[7]*a[4]
	 mov	%r14,%rdx		# a[5]
	adox	%rbx,%r11
	adcx	%r12,%r11
	#adox	$zero,%rax		# of=0
	adcx	$zero,%rax		# cf=0

	mulx	%r15,%r14,%rbx		# a[6]*a[5]
	mulx	%r8,%r12,%r13		# a[7]*a[5]
	 mov	%r15,%rdx		# a[6]
	 lea	8*8($aptr),$aptr
	adcx	%r14,%r11
	adox	%rbx,%r12
	adcx	%rax,%r12
	adox	$zero,%r13

	.byte	0x67,0x67
	mulx	%r8,%r8,%r14		# a[7]*a[6]
	adcx	%r8,%r13
	adcx	$zero,%r14

	cmp	8+8(%rsp),$aptr
	je	.Lsqrx8x_outer_break

	neg	$carry			# mov $carry,%cf
	mov	\$-8,%rcx
	mov	$zero,%r15
	mov	8*8($tptr),%r8
	adcx	9*8($tptr),%r9		# +=t[9]
	adcx	10*8($tptr),%r10	# ...
	adcx	11*8($tptr),%r11
	adc	12*8($tptr),%r12
	adc	13*8($tptr),%r13
	adc	14*8($tptr),%r14
	adc	15*8($tptr),%r15
	lea	($aptr),$aaptr
	lea	2*64($tptr),$tptr
	sbb	%rax,%rax		# mov %cf,$carry

	mov	-64($aptr),%rdx		# a[0]
	mov	%rax,16+8(%rsp)		# offload $carry
	mov	$tptr,24+8(%rsp)

	#lea	8*8($tptr),$tptr	# see 2*8*8($tptr) above
	xor	%eax,%eax		# cf=0, of=0
	jmp	.Lsqrx8x_loop

.align	32
.Lsqrx8x_loop:
	mov	%r8,%rbx
	mulx	0*8($aaptr),%rax,%r8	# a[8]*a[i]
	adcx	%rax,%rbx		# +=t[8]
	adox	%r9,%r8

	mulx	1*8($aaptr),%rax,%r9	# ...
	adcx	%rax,%r8
	adox	%r10,%r9

	mulx	2*8($aaptr),%rax,%r10
	adcx	%rax,%r9
	adox	%r11,%r10

	mulx	3*8($aaptr),%rax,%r11
	adcx	%rax,%r10
	adox	%r12,%r11

	.byte	0xc4,0x62,0xfb,0xf6,0xa5,0x20,0x00,0x00,0x00	# mulx	4*8($aaptr),%rax,%r12
	adcx	%rax,%r11
	adox	%r13,%r12

	mulx	5*8($aaptr),%rax,%r13
	adcx	%rax,%r12
	adox	%r14,%r13

	mulx	6*8($aaptr),%rax,%r14
	 mov	%rbx,($tptr,%rcx,8)	# store t[8+i]
	 mov	\$0,%ebx
	adcx	%rax,%r13
	adox	%r15,%r14

	.byte	0xc4,0x62,0xfb,0xf6,0xbd,0x38,0x00,0x00,0x00	# mulx	7*8($aaptr),%rax,%r15
	 mov	8($aptr,%rcx,8),%rdx	# a[i]
	adcx	%rax,%r14
	adox	%rbx,%r15		# %rbx is 0, of=0
	adcx	%rbx,%r15		# cf=0

	.byte	0x67
	inc	%rcx			# of=0
	jnz	.Lsqrx8x_loop

	lea	8*8($aaptr),$aaptr
	mov	\$-8,%rcx
	cmp	8+8(%rsp),$aaptr	# done?
	je	.Lsqrx8x_break

	sub	16+8(%rsp),%rbx		# mov 16(%rsp),%cf
	.byte	0x66
	mov	-64($aptr),%rdx
	adcx	0*8($tptr),%r8
	adcx	1*8($tptr),%r9
	adc	2*8($tptr),%r10
	adc	3*8($tptr),%r11
	adc	4*8($tptr),%r12
	adc	5*8($tptr),%r13
	adc	6*8($tptr),%r14
	adc	7*8($tptr),%r15
	lea	8*8($tptr),$tptr
	.byte	0x67
	sbb	%rax,%rax		# mov %cf,%rax
	xor	%ebx,%ebx		# cf=0, of=0
	mov	%rax,16+8(%rsp)		# offload carry
	jmp	.Lsqrx8x_loop

.align	32
.Lsqrx8x_break:
	sub	16+8(%rsp),%r8		# consume last carry
	mov	24+8(%rsp),$carry	# initial $tptr, borrow $carry
	mov	0*8($aptr),%rdx		# a[8], modulo-scheduled
	xor	%ebp,%ebp		# xor	$zero,$zero
	mov	%r8,0*8($tptr)
	cmp	$carry,$tptr		# cf=0, of=0
	je	.Lsqrx8x_outer_loop

	mov	%r9,1*8($tptr)
	 mov	1*8($carry),%r9
	mov	%r10,2*8($tptr)
	 mov	2*8($carry),%r10
	mov	%r11,3*8($tptr)
	 mov	3*8($carry),%r11
	mov	%r12,4*8($tptr)
	 mov	4*8($carry),%r12
	mov	%r13,5*8($tptr)
	 mov	5*8($carry),%r13
	mov	%r14,6*8($tptr)
	 mov	6*8($carry),%r14
	mov	%r15,7*8($tptr)
	 mov	7*8($carry),%r15
	mov	$carry,$tptr
	jmp	.Lsqrx8x_outer_loop

.align	32
.Lsqrx8x_outer_break:
	mov	%r9,9*8($tptr)		# t[9]
	 movq	%xmm3,%rcx		# -$num
	mov	%r10,10*8($tptr)	# ...
	mov	%r11,11*8($tptr)
	mov	%r12,12*8($tptr)
	mov	%r13,13*8($tptr)
	mov	%r14,14*8($tptr)
___
}{
my $i="%rcx";
$code.=<<___;
	lea	48+8(%rsp),$tptr
	mov	($aptr,$i),%rdx		# a[0]

	mov	8($tptr),$A0[1]		# t[1]
	xor	$A0[0],$A0[0]		# t[0], of=0, cf=0
	mov	0+8(%rsp),$num		# restore $num
	adox	$A0[1],$A0[1]
	 mov	16($tptr),$A1[0]	# t[2]	# prefetch
	 mov	24($tptr),$A1[1]	# t[3]	# prefetch
	#jmp	.Lsqrx4x_shift_n_add	# happens to be aligned

.align	32
.Lsqrx4x_shift_n_add:
	mulx	%rdx,%rax,%rbx
	 adox	$A1[0],$A1[0]
	adcx	$A0[0],%rax
	 .byte	0x48,0x8b,0x94,0x0e,0x08,0x00,0x00,0x00	# mov	8($aptr,$i),%rdx	# a[i+1]	# prefetch
	 .byte	0x4c,0x8b,0x97,0x20,0x00,0x00,0x00	# mov	32($tptr),$A0[0]	# t[2*i+4]	# prefetch
	 adox	$A1[1],$A1[1]
	adcx	$A0[1],%rbx
	 mov	40($tptr),$A0[1]		# t[2*i+4+1]	# prefetch
	mov	%rax,0($tptr)
	mov	%rbx,8($tptr)

	mulx	%rdx,%rax,%rbx
	 adox	$A0[0],$A0[0]
	adcx	$A1[0],%rax
	 mov	16($aptr,$i),%rdx	# a[i+2]	# prefetch
	 mov	48($tptr),$A1[0]	# t[2*i+6]	# prefetch
	 adox	$A0[1],$A0[1]
	adcx	$A1[1],%rbx
	 mov	56($tptr),$A1[1]	# t[2*i+6+1]	# prefetch
	mov	%rax,16($tptr)
	mov	%rbx,24($tptr)

	mulx	%rdx,%rax,%rbx
	 adox	$A1[0],$A1[0]
	adcx	$A0[0],%rax
	 mov	24($aptr,$i),%rdx	# a[i+3]	# prefetch
	 lea	32($i),$i
	 mov	64($tptr),$A0[0]	# t[2*i+8]	# prefetch
	 adox	$A1[1],$A1[1]
	adcx	$A0[1],%rbx
	 mov	72($tptr),$A0[1]	# t[2*i+8+1]	# prefetch
	mov	%rax,32($tptr)
	mov	%rbx,40($tptr)

	mulx	%rdx,%rax,%rbx
	 adox	$A0[0],$A0[0]
	adcx	$A1[0],%rax
	jrcxz	.Lsqrx4x_shift_n_add_break
	 .byte	0x48,0x8b,0x94,0x0e,0x00,0x00,0x00,0x00	# mov	0($aptr,$i),%rdx	# a[i+4]	# prefetch
	 adox	$A0[1],$A0[1]
	adcx	$A1[1],%rbx
	 mov	80($tptr),$A1[0]	# t[2*i+10]	# prefetch
	 mov	88($tptr),$A1[1]	# t[2*i+10+1]	# prefetch
	mov	%rax,48($tptr)
	mov	%rbx,56($tptr)
	lea	64($tptr),$tptr
	nop
	jmp	.Lsqrx4x_shift_n_add

.align	32
.Lsqrx4x_shift_n_add_break:
	adcx	$A1[1],%rbx
	mov	%rax,48($tptr)
	mov	%rbx,56($tptr)
	lea	64($tptr),$tptr		# end of t[] buffer
___
}
######################################################################
# Montgomery reduction part, "word-by-word" algorithm.
#
# This new path is inspired by multiple submissions from Intel, by
# Shay Gueron, Vlad Krasnov, Erdinc Ozturk, James Guilford,
# Vinodh Gopal...
{
my ($nptr,$carry,$m0)=("%rbp","%rsi","%rdx");

$code.=<<___;
	movq	%xmm2,$nptr
sqrx8x_reduction:
	xor	%eax,%eax		# initial top-most carry bit
	mov	32+8(%rsp),%rbx		# n0
	mov	48+8(%rsp),%rdx		# "%r8", 8*0($tptr)
	lea	-128($nptr,$num,2),%rcx	# end of n[]
	#lea	48+8(%rsp,$num,2),$tptr	# end of t[] buffer
	mov	%rcx, 0+8(%rsp)		# save end of n[]
	mov	$tptr,8+8(%rsp)		# save end of t[]

	lea	48+8(%rsp),$tptr		# initial t[] window
	jmp	.Lsqrx8x_reduction_loop

.align	32
.Lsqrx8x_reduction_loop:
	mov	8*1($tptr),%r9
	mov	8*2($tptr),%r10
	mov	8*3($tptr),%r11
	mov	8*4($tptr),%r12
	mov	%rdx,%r8
	imulq	%rbx,%rdx		# n0*a[i]
	mov	8*5($tptr),%r13
	mov	8*6($tptr),%r14
	mov	8*7($tptr),%r15
	mov	%rax,24+8(%rsp)		# store top-most carry bit

	lea	8*8($tptr),$tptr
	xor	$carry,$carry		# cf=0,of=0
	mov	\$-8,%rcx
	jmp	.Lsqrx8x_reduce

.align	32
.Lsqrx8x_reduce:
	mov	%r8, %rbx
	mulx	16*0($nptr),%rax,%r8	# n[0]
	adcx	%rbx,%rax		# discarded
	adox	%r9,%r8

	mulx	16*1($nptr),%rbx,%r9	# n[1]
	adcx	%rbx,%r8
	adox	%r10,%r9

	mulx	16*2($nptr),%rbx,%r10
	adcx	%rbx,%r9
	adox	%r11,%r10

	mulx	16*3($nptr),%rbx,%r11
	adcx	%rbx,%r10
	adox	%r12,%r11

	.byte	0xc4,0x62,0xe3,0xf6,0xa5,0x40,0x00,0x00,0x00	# mulx	16*4($nptr),%rbx,%r12
	 mov	%rdx,%rax
	 mov	%r8,%rdx
	adcx	%rbx,%r11
	adox	%r13,%r12

	 mulx	32+8(%rsp),%rbx,%rdx	# %rdx discarded
	 mov	%rax,%rdx
	 mov	%rax,64+48+8(%rsp,%rcx,8)	# put aside n0*a[i]

	mulx	16*5($nptr),%rax,%r13
	adcx	%rax,%r12
	adox	%r14,%r13

	mulx	16*6($nptr),%rax,%r14
	adcx	%rax,%r13
	adox	%r15,%r14

	mulx	16*7($nptr),%rax,%r15
	 mov	%rbx,%rdx
	adcx	%rax,%r14
	adox	$carry,%r15		# $carry is 0
	adcx	$carry,%r15		# cf=0

	.byte	0x67,0x67,0x67
	inc	%rcx			# of=0
	jnz	.Lsqrx8x_reduce

	mov	$carry,%rax		# xor	%rax,%rax
	cmp	0+8(%rsp),$nptr		# end of n[]?
	jae	.Lsqrx8x_no_tail

	mov	48+8(%rsp),%rdx		# pull n0*a[0]
	add	8*0($tptr),%r8
	lea	16*8($nptr),$nptr
	mov	\$-8,%rcx
	adcx	8*1($tptr),%r9
	adcx	8*2($tptr),%r10
	adc	8*3($tptr),%r11
	adc	8*4($tptr),%r12
	adc	8*5($tptr),%r13
	adc	8*6($tptr),%r14
	adc	8*7($tptr),%r15
	lea	8*8($tptr),$tptr
	sbb	%rax,%rax		# top carry

	xor	$carry,$carry		# of=0, cf=0
	mov	%rax,16+8(%rsp)
	jmp	.Lsqrx8x_tail

.align	32
.Lsqrx8x_tail:
	mov	%r8,%rbx
	mulx	16*0($nptr),%rax,%r8
	adcx	%rax,%rbx
	adox	%r9,%r8

	mulx	16*1($nptr),%rax,%r9
	adcx	%rax,%r8
	adox	%r10,%r9

	mulx	16*2($nptr),%rax,%r10
	adcx	%rax,%r9
	adox	%r11,%r10

	mulx	16*3($nptr),%rax,%r11
	adcx	%rax,%r10
	adox	%r12,%r11

	.byte	0xc4,0x62,0xfb,0xf6,0xa5,0x40,0x00,0x00,0x00	# mulx	16*4($nptr),%rax,%r12
	adcx	%rax,%r11
	adox	%r13,%r12

	mulx	16*5($nptr),%rax,%r13
	adcx	%rax,%r12
	adox	%r14,%r13

	mulx	16*6($nptr),%rax,%r14
	adcx	%rax,%r13
	adox	%r15,%r14

	mulx	16*7($nptr),%rax,%r15
	 mov	72+48+8(%rsp,%rcx,8),%rdx	# pull n0*a[i]
	adcx	%rax,%r14
	adox	$carry,%r15
	 mov	%rbx,($tptr,%rcx,8)	# save result
	 mov	%r8,%rbx
	adcx	$carry,%r15		# cf=0

	inc	%rcx			# of=0
	jnz	.Lsqrx8x_tail

	cmp	0+8(%rsp),$nptr		# end of n[]?
	jae	.Lsqrx8x_tail_done	# break out of loop

	sub	16+8(%rsp),$carry	# mov 16(%rsp),%cf
	 mov	48+8(%rsp),%rdx		# pull n0*a[0]
	 lea	16*8($nptr),$nptr
	adc	8*0($tptr),%r8
	adc	8*1($tptr),%r9
	adc	8*2($tptr),%r10
	adc	8*3($tptr),%r11
	adc	8*4($tptr),%r12
	adc	8*5($tptr),%r13
	adc	8*6($tptr),%r14
	adc	8*7($tptr),%r15
	lea	8*8($tptr),$tptr
	sbb	%rax,%rax
	sub	\$8,%rcx		# mov	\$-8,%rcx

	xor	$carry,$carry		# of=0, cf=0
	mov	%rax,16+8(%rsp)
	jmp	.Lsqrx8x_tail

.align	32
.Lsqrx8x_tail_done:
	add	24+8(%rsp),%r8		# can this overflow?
	adc	\$0,%r9
	adc	\$0,%r10
	adc	\$0,%r11
	adc	\$0,%r12
	adc	\$0,%r13
	adc	\$0,%r14
	adc	\$0,%r15		# can't overflow, because we
					# started with "overhung" part
					# of multiplication
	mov	$carry,%rax		# xor	%rax,%rax

	sub	16+8(%rsp),$carry	# mov 16(%rsp),%cf
.Lsqrx8x_no_tail:			# %cf is 0 if jumped here
	adc	8*0($tptr),%r8
	 movq	%xmm3,%rcx
	adc	8*1($tptr),%r9
	 mov	16*7($nptr),$carry
	 movq	%xmm2,$nptr		# restore $nptr
	adc	8*2($tptr),%r10
	adc	8*3($tptr),%r11
	adc	8*4($tptr),%r12
	adc	8*5($tptr),%r13
	adc	8*6($tptr),%r14
	adc	8*7($tptr),%r15
	adc	%rax,%rax		# top-most carry

	mov	32+8(%rsp),%rbx		# n0
	mov	8*8($tptr,%rcx),%rdx	# modulo-scheduled "%r8"

	mov	%r8,8*0($tptr)		# store top 512 bits
	 lea	8*8($tptr),%r8		# borrow %r8
	mov	%r9,8*1($tptr)
	mov	%r10,8*2($tptr)
	mov	%r11,8*3($tptr)
	mov	%r12,8*4($tptr)
	mov	%r13,8*5($tptr)
	mov	%r14,8*6($tptr)
	mov	%r15,8*7($tptr)

	lea	8*8($tptr,%rcx),$tptr	# start of current t[] window
	cmp	8+8(%rsp),%r8		# end of t[]?
	jb	.Lsqrx8x_reduction_loop
___
}
##############################################################
# Post-condition, 4x unrolled
#
{
my ($rptr,$nptr)=("%rdx","%rbp");
my @ri=map("%r$_",(10..13));
my @ni=map("%r$_",(14..15));
$code.=<<___;
	xor	%ebx,%ebx
	sub	%r15,%rsi		# compare top-most words
	adc	%rbx,%rbx
	mov	%rcx,%r10		# -$num
	or	%rbx,%rax
	mov	%rcx,%r9		# -$num
	xor	\$1,%rax
	sar	\$3+2,%rcx		# cf=0
	#lea	48+8(%rsp,%r9),$tptr
	lea	($nptr,%rax,8),$nptr
	movq	%xmm1,$rptr		# restore $rptr
	movq	%xmm1,$aptr		# prepare for back-to-back call
	jmp	.Lsqrx4x_sub

.align	32
.Lsqrx4x_sub:
	.byte	0x66
	mov	8*0($tptr),%r12
	mov	8*1($tptr),%r13
	sbb	16*0($nptr),%r12
	mov	8*2($tptr),%r14
	sbb	16*1($nptr),%r13
	mov	8*3($tptr),%r15
	lea	8*4($tptr),$tptr
	sbb	16*2($nptr),%r14
	mov	%r12,8*0($rptr)
	sbb	16*3($nptr),%r15
	lea	16*4($nptr),$nptr
	mov	%r13,8*1($rptr)
	mov	%r14,8*2($rptr)
	mov	%r15,8*3($rptr)
	lea	8*4($rptr),$rptr

	inc	%rcx
	jnz	.Lsqrx4x_sub
___
}
$code.=<<___;
	neg	%r9			# restore $num

	ret
.size	bn_sqrx8x_internal,.-bn_sqrx8x_internal
___
}}}
{
my ($inp,$num,$tbl,$idx)=$win64?("%rcx","%edx","%r8", "%r9d") : # Win64 order
				("%rdi","%esi","%rdx","%ecx");  # Unix order
my $out=$inp;
my $STRIDE=2**5*8;
my $N=$STRIDE/4;

$code.=<<___;
.globl	bn_get_bits5
.type	bn_get_bits5,\@abi-omnipotent
.align	16
bn_get_bits5:
	lea	0($inp),%r10
	lea	1($inp),%r11
	mov	$num,%ecx
	shr	\$4,$num
	and	\$15,%ecx
	lea	-8(%ecx),%eax
	cmp	\$11,%ecx
	cmova	%r11,%r10
	cmova	%eax,%ecx
	movzw	(%r10,$num,2),%eax
	shrl	%cl,%eax
	and	\$31,%eax
	ret
.size	bn_get_bits5,.-bn_get_bits5

.globl	bn_scatter5
.type	bn_scatter5,\@abi-omnipotent
.align	16
bn_scatter5:
	cmp	\$0, $num
	jz	.Lscatter_epilogue
	lea	($tbl,$idx,8),$tbl
.Lscatter:
	mov	($inp),%rax
	lea	8($inp),$inp
	mov	%rax,($tbl)
	lea	32*8($tbl),$tbl
	sub	\$1,$num
	jnz	.Lscatter
.Lscatter_epilogue:
	ret
.size	bn_scatter5,.-bn_scatter5

.globl	bn_gather5
.type	bn_gather5,\@abi-omnipotent
.align	16
bn_gather5:
___
$code.=<<___ if ($win64);
.LSEH_begin_bn_gather5:
	# I can't trust assembler to use specific encoding:-(
	.byte	0x48,0x83,0xec,0x28		#sub	\$0x28,%rsp
	.byte	0x0f,0x29,0x34,0x24		#movaps	%xmm6,(%rsp)
	.byte	0x0f,0x29,0x7c,0x24,0x10	#movdqa	%xmm7,0x10(%rsp)
___
$code.=<<___;
	mov	$idx,%r11d
	shr	\$`log($N/8)/log(2)`,$idx
	and	\$`$N/8-1`,%r11
	not	$idx
	lea	.Lmagic_masks(%rip),%rax
	and	\$`2**5/($N/8)-1`,$idx	# 5 is "window size"
	lea	128($tbl,%r11,8),$tbl	# pointer within 1st cache line
	movq	0(%rax,$idx,8),%xmm4	# set of masks denoting which
	movq	8(%rax,$idx,8),%xmm5	# cache line contains element
	movq	16(%rax,$idx,8),%xmm6	# denoted by 7th argument
	movq	24(%rax,$idx,8),%xmm7
	jmp	.Lgather
.align	16
.Lgather:
	movq	`0*$STRIDE/4-128`($tbl),%xmm0
	movq	`1*$STRIDE/4-128`($tbl),%xmm1
	pand	%xmm4,%xmm0
	movq	`2*$STRIDE/4-128`($tbl),%xmm2
	pand	%xmm5,%xmm1
	movq	`3*$STRIDE/4-128`($tbl),%xmm3
	pand	%xmm6,%xmm2
	por	%xmm1,%xmm0
	pand	%xmm7,%xmm3
	.byte	0x67,0x67
	por	%xmm2,%xmm0
	lea	$STRIDE($tbl),$tbl
	por	%xmm3,%xmm0

	movq	%xmm0,($out)		# m0=bp[0]
	lea	8($out),$out
	sub	\$1,$num
	jnz	.Lgather
___
$code.=<<___ if ($win64);
	movaps	(%rsp),%xmm6
	movaps	0x10(%rsp),%xmm7
	lea	0x28(%rsp),%rsp
___
$code.=<<___;
	ret
.LSEH_end_bn_gather5:
.size	bn_gather5,.-bn_gather5
___
}
$code.=<<___;
.align	64
.Lmagic_masks:
	.long	0,0, 0,0, 0,0, -1,-1
	.long	0,0, 0,0, 0,0,  0,0
.asciz	"Montgomery Multiplication with scatter/gather for x86_64, CRYPTOGAMS by <appro\@openssl.org>"
___

# EXCEPTION_DISPOSITION handler (EXCEPTION_RECORD *rec,ULONG64 frame,
#		CONTEXT *context,DISPATCHER_CONTEXT *disp)
if ($win64) {
$rec="%rcx";
$frame="%rdx";
$context="%r8";
$disp="%r9";

$code.=<<___;
.extern	__imp_RtlVirtualUnwind
.type	mul_handler,\@abi-omnipotent
.align	16
mul_handler:
	push	%rsi
	push	%rdi
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	pushfq
	sub	\$64,%rsp

	mov	120($context),%rax	# pull context->Rax
	mov	248($context),%rbx	# pull context->Rip

	mov	8($disp),%rsi		# disp->ImageBase
	mov	56($disp),%r11		# disp->HandlerData

	mov	0(%r11),%r10d		# HandlerData[0]
	lea	(%rsi,%r10),%r10	# end of prologue label
	cmp	%r10,%rbx		# context->Rip<end of prologue label
	jb	.Lcommon_seh_tail

	mov	152($context),%rax	# pull context->Rsp

	mov	4(%r11),%r10d		# HandlerData[1]
	lea	(%rsi,%r10),%r10	# epilogue label
	cmp	%r10,%rbx		# context->Rip>=epilogue label
	jae	.Lcommon_seh_tail

	lea	.Lmul_epilogue(%rip),%r10
	cmp	%r10,%rbx
	jb	.Lbody_40

	mov	192($context),%r10	# pull $num
	mov	8(%rax,%r10,8),%rax	# pull saved stack pointer
	jmp	.Lbody_proceed

.Lbody_40:
	mov	40(%rax),%rax		# pull saved stack pointer
.Lbody_proceed:

	movaps	-88(%rax),%xmm0
	movaps	-72(%rax),%xmm1

	mov	-8(%rax),%rbx
	mov	-16(%rax),%rbp
	mov	-24(%rax),%r12
	mov	-32(%rax),%r13
	mov	-40(%rax),%r14
	mov	-48(%rax),%r15
	mov	%rbx,144($context)	# restore context->Rbx
	mov	%rbp,160($context)	# restore context->Rbp
	mov	%r12,216($context)	# restore context->R12
	mov	%r13,224($context)	# restore context->R13
	mov	%r14,232($context)	# restore context->R14
	mov	%r15,240($context)	# restore context->R15
	movups	%xmm0,512($context)	# restore context->Xmm6
	movups	%xmm1,528($context)	# restore context->Xmm7

.Lcommon_seh_tail:
	mov	8(%rax),%rdi
	mov	16(%rax),%rsi
	mov	%rax,152($context)	# restore context->Rsp
	mov	%rsi,168($context)	# restore context->Rsi
	mov	%rdi,176($context)	# restore context->Rdi

	mov	40($disp),%rdi		# disp->ContextRecord
	mov	$context,%rsi		# context
	mov	\$154,%ecx		# sizeof(CONTEXT)
	.long	0xa548f3fc		# cld; rep movsq

	mov	$disp,%rsi
	xor	%rcx,%rcx		# arg1, UNW_FLAG_NHANDLER
	mov	8(%rsi),%rdx		# arg2, disp->ImageBase
	mov	0(%rsi),%r8		# arg3, disp->ControlPc
	mov	16(%rsi),%r9		# arg4, disp->FunctionEntry
	mov	40(%rsi),%r10		# disp->ContextRecord
	lea	56(%rsi),%r11		# &disp->HandlerData
	lea	24(%rsi),%r12		# &disp->EstablisherFrame
	mov	%r10,32(%rsp)		# arg5
	mov	%r11,40(%rsp)		# arg6
	mov	%r12,48(%rsp)		# arg7
	mov	%rcx,56(%rsp)		# arg8, (NULL)
	call	*__imp_RtlVirtualUnwind(%rip)

	mov	\$1,%eax		# ExceptionContinueSearch
	add	\$64,%rsp
	popfq
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	pop	%rdi
	pop	%rsi
	ret
.size	mul_handler,.-mul_handler

.section	.pdata
.align	4
	.rva	.LSEH_begin_bn_mul_mont_gather5
	.rva	.LSEH_end_bn_mul_mont_gather5
	.rva	.LSEH_info_bn_mul_mont_gather5

	.rva	.LSEH_begin_bn_mul4x_mont_gather5
	.rva	.LSEH_end_bn_mul4x_mont_gather5
	.rva	.LSEH_info_bn_mul4x_mont_gather5

	.rva	.LSEH_begin_bn_power5
	.rva	.LSEH_end_bn_power5
	.rva	.LSEH_info_bn_power5

	.rva	.LSEH_begin_bn_from_mont8x
	.rva	.LSEH_end_bn_from_mont8x
	.rva	.LSEH_info_bn_from_mont8x
___
$code.=<<___ if ($addx);
	.rva	.LSEH_begin_bn_mulx4x_mont_gather5
	.rva	.LSEH_end_bn_mulx4x_mont_gather5
	.rva	.LSEH_info_bn_mulx4x_mont_gather5

	.rva	.LSEH_begin_bn_powerx5
	.rva	.LSEH_end_bn_powerx5
	.rva	.LSEH_info_bn_powerx5
___
$code.=<<___;
	.rva	.LSEH_begin_bn_gather5
	.rva	.LSEH_end_bn_gather5
	.rva	.LSEH_info_bn_gather5

.section	.xdata
.align	8
.LSEH_info_bn_mul_mont_gather5:
	.byte	9,0,0,0
	.rva	mul_handler
	.rva	.Lmul_body,.Lmul_epilogue		# HandlerData[]
.align	8
.LSEH_info_bn_mul4x_mont_gather5:
	.byte	9,0,0,0
	.rva	mul_handler
	.rva	.Lmul4x_body,.Lmul4x_epilogue		# HandlerData[]
.align	8
.LSEH_info_bn_power5:
	.byte	9,0,0,0
	.rva	mul_handler
	.rva	.Lpower5_body,.Lpower5_epilogue		# HandlerData[]
.align	8
.LSEH_info_bn_from_mont8x:
	.byte	9,0,0,0
	.rva	mul_handler
	.rva	.Lfrom_body,.Lfrom_epilogue		# HandlerData[]
___
$code.=<<___ if ($addx);
.align	8
.LSEH_info_bn_mulx4x_mont_gather5:
	.byte	9,0,0,0
	.rva	mul_handler
	.rva	.Lmulx4x_body,.Lmulx4x_epilogue		# HandlerData[]
.align	8
.LSEH_info_bn_powerx5:
	.byte	9,0,0,0
	.rva	mul_handler
	.rva	.Lpowerx5_body,.Lpowerx5_epilogue	# HandlerData[]
___
$code.=<<___;
.align	8
.LSEH_info_bn_gather5:
        .byte   0x01,0x0d,0x05,0x00
        .byte   0x0d,0x78,0x01,0x00	#movaps	0x10(rsp),xmm7
        .byte   0x08,0x68,0x00,0x00	#movaps	(rsp),xmm6
        .byte   0x04,0x42,0x00,0x00	#sub	rsp,0x28
.align	8
___
}

$code =~ s/\`([^\`]*)\`/eval($1)/gem;

print $code;
close STDOUT;
