#! /usr/bin/env perl
# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# ====================================================================
# Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================
#
# This module implements Poly1305 hash for x86_64.
#
# March 2015
#
# Numbers are cycles per processed byte with poly1305_blocks alone,
# measured with rdtsc at fixed clock frequency.
#
#		IALU/gcc-4.8(*)	AVX(**)		AVX2
# P4		4.46/+120%	-
# Core 2	2.41/+90%	-
# Westmere	1.88/+120%	-
# Sandy Bridge	1.39/+140%	1.10
# Haswell	1.14/+175%	1.11		0.65
# Skylake	1.13/+120%	0.96		0.51
# Silvermont	2.83/+95%	-
# Goldmont	1.70/+180%	-
# VIA Nano	1.82/+150%	-
# Sledgehammer	1.38/+160%	-
# Bulldozer	2.30/+130%	0.97
#
# (*)	improvement coefficients relative to clang are more modest and
#	are ~50% on most processors, in both cases we are comparing to
#	__int128 code;
# (**)	SSE2 implementation was attempted, but among non-AVX processors
#	it was faster than integer-only code only on older Intel P4 and
#	Core processors, 50-30%, less newer processor is, but slower on
#	contemporary ones, for example almost 2x slower on Atom, and as
#	former are naturally disappearing, SSE2 is deemed unnecessary;

$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
		=~ /GNU assembler version ([2-9]\.[0-9]+)/) {
	$avx = ($1>=2.19) + ($1>=2.22);
}

if (!$avx && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
	   `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)/) {
	$avx = ($1>=2.09) + ($1>=2.10);
}

if (!$avx && $win64 && ($flavour =~ /masm/ || $ENV{ASM} =~ /ml64/) &&
	   `ml64 2>&1` =~ /Version ([0-9]+)\./) {
	$avx = ($1>=10) + ($1>=12);
}

if (!$avx && `$ENV{CC} -v 2>&1` =~ /((?:^clang|LLVM) version|.*based on LLVM) ([3-9]\.[0-9]+)/) {
	$avx = ($2>=3.0) + ($2>3.0);
}

open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\"";
*STDOUT=*OUT;

my ($ctx,$inp,$len,$padbit)=("%rdi","%rsi","%rdx","%rcx");
my ($mac,$nonce)=($inp,$len);	# *_emit arguments
my ($d1,$d2,$d3, $r0,$r1,$s1)=map("%r$_",(8..13));
my ($h0,$h1,$h2)=("%r14","%rbx","%rbp");

sub poly1305_iteration {
# input:	copy of $r1 in %rax, $h0-$h2, $r0-$r1
# output:	$h0-$h2 *= $r0-$r1
$code.=<<___;
	mulq	$h0			# h0*r1
	mov	%rax,$d2
	 mov	$r0,%rax
	mov	%rdx,$d3

	mulq	$h0			# h0*r0
	mov	%rax,$h0		# future $h0
	 mov	$r0,%rax
	mov	%rdx,$d1

	mulq	$h1			# h1*r0
	add	%rax,$d2
	 mov	$s1,%rax
	adc	%rdx,$d3

	mulq	$h1			# h1*s1
	 mov	$h2,$h1			# borrow $h1
	add	%rax,$h0
	adc	%rdx,$d1

	imulq	$s1,$h1			# h2*s1
	add	$h1,$d2
	 mov	$d1,$h1
	adc	\$0,$d3

	imulq	$r0,$h2			# h2*r0
	add	$d2,$h1
	mov	\$-4,%rax		# mask value
	adc	$h2,$d3

	and	$d3,%rax		# last reduction step
	mov	$d3,$h2
	shr	\$2,$d3
	and	\$3,$h2
	add	$d3,%rax
	add	%rax,$h0
	adc	\$0,$h1
	adc	\$0,$h2
___
}

########################################################################
# Layout of opaque area is following.
#
#	unsigned __int64 h[3];		# current hash value base 2^64
#	unsigned __int64 r[2];		# key value base 2^64

$code.=<<___;
.text

.extern	OPENSSL_ia32cap_P

.globl	poly1305_init
.hidden	poly1305_init
.globl	poly1305_blocks
.hidden	poly1305_blocks
.globl	poly1305_emit
.hidden	poly1305_emit

.type	poly1305_init,\@function,3
.align	32
poly1305_init:
	xor	%rax,%rax
	mov	%rax,0($ctx)		# initialize hash value
	mov	%rax,8($ctx)
	mov	%rax,16($ctx)

	cmp	\$0,$inp
	je	.Lno_key

	lea	poly1305_blocks(%rip),%r10
	lea	poly1305_emit(%rip),%r11
___
$code.=<<___	if ($avx);
	mov	OPENSSL_ia32cap_P+4(%rip),%r9
	lea	poly1305_blocks_avx(%rip),%rax
	lea	poly1305_emit_avx(%rip),%rcx
	bt	\$`60-32`,%r9		# AVX?
	cmovc	%rax,%r10
	cmovc	%rcx,%r11
___
$code.=<<___	if ($avx>1);
	lea	poly1305_blocks_avx2(%rip),%rax
	bt	\$`5+32`,%r9		# AVX2?
	cmovc	%rax,%r10
___
$code.=<<___;
	mov	\$0x0ffffffc0fffffff,%rax
	mov	\$0x0ffffffc0ffffffc,%rcx
	and	0($inp),%rax
	and	8($inp),%rcx
	mov	%rax,24($ctx)
	mov	%rcx,32($ctx)
___
$code.=<<___	if ($flavour !~ /elf32/);
	mov	%r10,0(%rdx)
	mov	%r11,8(%rdx)
___
$code.=<<___	if ($flavour =~ /elf32/);
	mov	%r10d,0(%rdx)
	mov	%r11d,4(%rdx)
___
$code.=<<___;
	mov	\$1,%eax
.Lno_key:
	ret
.size	poly1305_init,.-poly1305_init

.type	poly1305_blocks,\@function,4
.align	32
poly1305_blocks:
.Lblocks:
	shr	\$4,$len
	jz	.Lno_data		# too short

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lblocks_body:

	mov	$len,%r15		# reassign $len

	mov	24($ctx),$r0		# load r
	mov	32($ctx),$s1

	mov	0($ctx),$h0		# load hash value
	mov	8($ctx),$h1
	mov	16($ctx),$h2

	mov	$s1,$r1
	shr	\$2,$s1
	mov	$r1,%rax
	add	$r1,$s1			# s1 = r1 + (r1 >> 2)
	jmp	.Loop

.align	32
.Loop:
	add	0($inp),$h0		# accumulate input
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
___
	&poly1305_iteration();
$code.=<<___;
	mov	$r1,%rax
	dec	%r15			# len-=16
	jnz	.Loop

	mov	$h0,0($ctx)		# store hash value
	mov	$h1,8($ctx)
	mov	$h2,16($ctx)

	mov	0(%rsp),%r15
	mov	8(%rsp),%r14
	mov	16(%rsp),%r13
	mov	24(%rsp),%r12
	mov	32(%rsp),%rbp
	mov	40(%rsp),%rbx
	lea	48(%rsp),%rsp
.Lno_data:
.Lblocks_epilogue:
	ret
.size	poly1305_blocks,.-poly1305_blocks

.type	poly1305_emit,\@function,3
.align	32
poly1305_emit:
.Lemit:
	mov	0($ctx),%r8	# load hash value
	mov	8($ctx),%r9
	mov	16($ctx),%r10

	mov	%r8,%rax
	add	\$5,%r8		# compare to modulus
	mov	%r9,%rcx
	adc	\$0,%r9
	adc	\$0,%r10
	shr	\$2,%r10	# did 130-bit value overfow?
	cmovnz	%r8,%rax
	cmovnz	%r9,%rcx

	add	0($nonce),%rax	# accumulate nonce
	adc	8($nonce),%rcx
	mov	%rax,0($mac)	# write result
	mov	%rcx,8($mac)

	ret
.size	poly1305_emit,.-poly1305_emit
___
if ($avx) {

########################################################################
# Layout of opaque area is following.
#
#	unsigned __int32 h[5];		# current hash value base 2^26
#	unsigned __int32 is_base2_26;
#	unsigned __int64 r[2];		# key value base 2^64
#	unsigned __int64 pad;
#	struct { unsigned __int32 r^2, r^1, r^4, r^3; } r[9];
#
# where r^n are base 2^26 digits of degrees of multiplier key. There are
# 5 digits, but last four are interleaved with multiples of 5, totalling
# in 9 elements: r0, r1, 5*r1, r2, 5*r2, r3, 5*r3, r4, 5*r4.

my ($H0,$H1,$H2,$H3,$H4, $T0,$T1,$T2,$T3,$T4, $D0,$D1,$D2,$D3,$D4, $MASK) =
    map("%xmm$_",(0..15));

$code.=<<___;
.type	__poly1305_block,\@abi-omnipotent
.align	32
__poly1305_block:
___
	&poly1305_iteration();
$code.=<<___;
	ret
.size	__poly1305_block,.-__poly1305_block

.type	__poly1305_init_avx,\@abi-omnipotent
.align	32
__poly1305_init_avx:
	mov	$r0,$h0
	mov	$r1,$h1
	xor	$h2,$h2

	lea	48+64($ctx),$ctx	# size optimization

	mov	$r1,%rax
	call	__poly1305_block	# r^2

	mov	\$0x3ffffff,%eax	# save interleaved r^2 and r base 2^26
	mov	\$0x3ffffff,%edx
	mov	$h0,$d1
	and	$h0#d,%eax
	mov	$r0,$d2
	and	$r0#d,%edx
	mov	%eax,`16*0+0-64`($ctx)
	shr	\$26,$d1
	mov	%edx,`16*0+4-64`($ctx)
	shr	\$26,$d2

	mov	\$0x3ffffff,%eax
	mov	\$0x3ffffff,%edx
	and	$d1#d,%eax
	and	$d2#d,%edx
	mov	%eax,`16*1+0-64`($ctx)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%edx,`16*1+4-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	# *5
	mov	%eax,`16*2+0-64`($ctx)
	shr	\$26,$d1
	mov	%edx,`16*2+4-64`($ctx)
	shr	\$26,$d2

	mov	$h1,%rax
	mov	$r1,%rdx
	shl	\$12,%rax
	shl	\$12,%rdx
	or	$d1,%rax
	or	$d2,%rdx
	and	\$0x3ffffff,%eax
	and	\$0x3ffffff,%edx
	mov	%eax,`16*3+0-64`($ctx)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%edx,`16*3+4-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	# *5
	mov	%eax,`16*4+0-64`($ctx)
	mov	$h1,$d1
	mov	%edx,`16*4+4-64`($ctx)
	mov	$r1,$d2

	mov	\$0x3ffffff,%eax
	mov	\$0x3ffffff,%edx
	shr	\$14,$d1
	shr	\$14,$d2
	and	$d1#d,%eax
	and	$d2#d,%edx
	mov	%eax,`16*5+0-64`($ctx)
	lea	(%rax,%rax,4),%eax	# *5
	mov	%edx,`16*5+4-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	# *5
	mov	%eax,`16*6+0-64`($ctx)
	shr	\$26,$d1
	mov	%edx,`16*6+4-64`($ctx)
	shr	\$26,$d2

	mov	$h2,%rax
	shl	\$24,%rax
	or	%rax,$d1
	mov	$d1#d,`16*7+0-64`($ctx)
	lea	($d1,$d1,4),$d1		# *5
	mov	$d2#d,`16*7+4-64`($ctx)
	lea	($d2,$d2,4),$d2		# *5
	mov	$d1#d,`16*8+0-64`($ctx)
	mov	$d2#d,`16*8+4-64`($ctx)

	mov	$r1,%rax
	call	__poly1305_block	# r^3

	mov	\$0x3ffffff,%eax	# save r^3 base 2^26
	mov	$h0,$d1
	and	$h0#d,%eax
	shr	\$26,$d1
	mov	%eax,`16*0+12-64`($ctx)

	mov	\$0x3ffffff,%edx
	and	$d1#d,%edx
	mov	%edx,`16*1+12-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	\$26,$d1
	mov	%edx,`16*2+12-64`($ctx)

	mov	$h1,%rax
	shl	\$12,%rax
	or	$d1,%rax
	and	\$0x3ffffff,%eax
	mov	%eax,`16*3+12-64`($ctx)
	lea	(%rax,%rax,4),%eax	# *5
	mov	$h1,$d1
	mov	%eax,`16*4+12-64`($ctx)

	mov	\$0x3ffffff,%edx
	shr	\$14,$d1
	and	$d1#d,%edx
	mov	%edx,`16*5+12-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	\$26,$d1
	mov	%edx,`16*6+12-64`($ctx)

	mov	$h2,%rax
	shl	\$24,%rax
	or	%rax,$d1
	mov	$d1#d,`16*7+12-64`($ctx)
	lea	($d1,$d1,4),$d1		# *5
	mov	$d1#d,`16*8+12-64`($ctx)

	mov	$r1,%rax
	call	__poly1305_block	# r^4

	mov	\$0x3ffffff,%eax	# save r^4 base 2^26
	mov	$h0,$d1
	and	$h0#d,%eax
	shr	\$26,$d1
	mov	%eax,`16*0+8-64`($ctx)

	mov	\$0x3ffffff,%edx
	and	$d1#d,%edx
	mov	%edx,`16*1+8-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	\$26,$d1
	mov	%edx,`16*2+8-64`($ctx)

	mov	$h1,%rax
	shl	\$12,%rax
	or	$d1,%rax
	and	\$0x3ffffff,%eax
	mov	%eax,`16*3+8-64`($ctx)
	lea	(%rax,%rax,4),%eax	# *5
	mov	$h1,$d1
	mov	%eax,`16*4+8-64`($ctx)

	mov	\$0x3ffffff,%edx
	shr	\$14,$d1
	and	$d1#d,%edx
	mov	%edx,`16*5+8-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	# *5
	shr	\$26,$d1
	mov	%edx,`16*6+8-64`($ctx)

	mov	$h2,%rax
	shl	\$24,%rax
	or	%rax,$d1
	mov	$d1#d,`16*7+8-64`($ctx)
	lea	($d1,$d1,4),$d1		# *5
	mov	$d1#d,`16*8+8-64`($ctx)

	lea	-48-64($ctx),$ctx	# size [de-]optimization
	ret
.size	__poly1305_init_avx,.-__poly1305_init_avx

.type	poly1305_blocks_avx,\@function,4
.align	32
poly1305_blocks_avx:
	mov	20($ctx),%r8d		# is_base2_26
	cmp	\$128,$len
	jae	.Lblocks_avx
	test	%r8d,%r8d
	jz	.Lblocks

.Lblocks_avx:
	and	\$-16,$len
	jz	.Lno_data_avx

	vzeroupper

	test	%r8d,%r8d
	jz	.Lbase2_64_avx

	test	\$31,$len
	jz	.Leven_avx

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lblocks_avx_body:

	mov	$len,%r15		# reassign $len

	mov	0($ctx),$d1		# load hash value
	mov	8($ctx),$d2
	mov	16($ctx),$h2#d

	mov	24($ctx),$r0		# load r
	mov	32($ctx),$s1

	################################# base 2^26 -> base 2^64
	mov	$d1#d,$h0#d
	and	\$`-1*(1<<31)`,$d1
	mov	$d2,$r1			# borrow $r1
	mov	$d2#d,$h1#d
	and	\$`-1*(1<<31)`,$d2

	shr	\$6,$d1
	shl	\$52,$r1
	add	$d1,$h0
	shr	\$12,$h1
	shr	\$18,$d2
	add	$r1,$h0
	adc	$d2,$h1

	mov	$h2,$d1
	shl	\$40,$d1
	shr	\$24,$h2
	add	$d1,$h1
	adc	\$0,$h2			# can be partially reduced...

	mov	\$-4,$d2		# ... so reduce
	mov	$h2,$d1
	and	$h2,$d2
	shr	\$2,$d1
	and	\$3,$h2
	add	$d2,$d1			# =*5
	add	$d1,$h0
	adc	\$0,$h1
	adc	\$0,$h2

	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			# s1 = r1 + (r1 >> 2)

	add	0($inp),$h0		# accumulate input
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2

	call	__poly1305_block

	test	$padbit,$padbit		# if $padbit is zero,
	jz	.Lstore_base2_64_avx	# store hash in base 2^64 format

	################################# base 2^64 -> base 2^26
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$r0
	mov	$h1,$r1
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	# h[0]
	shl	\$12,$r0
	and	\$0x3ffffff,%rdx	# h[1]
	shr	\$14,$h1
	or	$r0,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		# h[2]
	shr	\$40,$r1
	and	\$0x3ffffff,$h1		# h[3]
	or	$r1,$h2			# h[4]

	sub	\$16,%r15
	jz	.Lstore_base2_26_avx

	vmovd	%rax#d,$H0
	vmovd	%rdx#d,$H1
	vmovd	$h0#d,$H2
	vmovd	$h1#d,$H3
	vmovd	$h2#d,$H4
	jmp	.Lproceed_avx

.align	32
.Lstore_base2_64_avx:
	mov	$h0,0($ctx)
	mov	$h1,8($ctx)
	mov	$h2,16($ctx)		# note that is_base2_26 is zeroed
	jmp	.Ldone_avx

.align	16
.Lstore_base2_26_avx:
	mov	%rax#d,0($ctx)		# store hash value base 2^26
	mov	%rdx#d,4($ctx)
	mov	$h0#d,8($ctx)
	mov	$h1#d,12($ctx)
	mov	$h2#d,16($ctx)
.align	16
.Ldone_avx:
	mov	0(%rsp),%r15
	mov	8(%rsp),%r14
	mov	16(%rsp),%r13
	mov	24(%rsp),%r12
	mov	32(%rsp),%rbp
	mov	40(%rsp),%rbx
	lea	48(%rsp),%rsp
.Lno_data_avx:
.Lblocks_avx_epilogue:
	ret

.align	32
.Lbase2_64_avx:
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lbase2_64_avx_body:

	mov	$len,%r15		# reassign $len

	mov	24($ctx),$r0		# load r
	mov	32($ctx),$s1

	mov	0($ctx),$h0		# load hash value
	mov	8($ctx),$h1
	mov	16($ctx),$h2#d

	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			# s1 = r1 + (r1 >> 2)

	test	\$31,$len
	jz	.Linit_avx

	add	0($inp),$h0		# accumulate input
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
	sub	\$16,%r15

	call	__poly1305_block

.Linit_avx:
	################################# base 2^64 -> base 2^26
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$d1
	mov	$h1,$d2
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	# h[0]
	shl	\$12,$d1
	and	\$0x3ffffff,%rdx	# h[1]
	shr	\$14,$h1
	or	$d1,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		# h[2]
	shr	\$40,$d2
	and	\$0x3ffffff,$h1		# h[3]
	or	$d2,$h2			# h[4]

	vmovd	%rax#d,$H0
	vmovd	%rdx#d,$H1
	vmovd	$h0#d,$H2
	vmovd	$h1#d,$H3
	vmovd	$h2#d,$H4
	movl	\$1,20($ctx)		# set is_base2_26

	call	__poly1305_init_avx

.Lproceed_avx:
	mov	%r15,$len

	mov	0(%rsp),%r15
	mov	8(%rsp),%r14
	mov	16(%rsp),%r13
	mov	24(%rsp),%r12
	mov	32(%rsp),%rbp
	mov	40(%rsp),%rbx
	lea	48(%rsp),%rax
	lea	48(%rsp),%rsp
.Lbase2_64_avx_epilogue:
	jmp	.Ldo_avx

.align	32
.Leven_avx:
	vmovd		4*0($ctx),$H0		# load hash value
	vmovd		4*1($ctx),$H1
	vmovd		4*2($ctx),$H2
	vmovd		4*3($ctx),$H3
	vmovd		4*4($ctx),$H4

.Ldo_avx:
___
$code.=<<___	if (!$win64);
	lea		-0x58(%rsp),%r11
	sub		\$0x178,%rsp
___
$code.=<<___	if ($win64);
	lea		-0xf8(%rsp),%r11
	sub		\$0x218,%rsp
	vmovdqa		%xmm6,0x50(%r11)
	vmovdqa		%xmm7,0x60(%r11)
	vmovdqa		%xmm8,0x70(%r11)
	vmovdqa		%xmm9,0x80(%r11)
	vmovdqa		%xmm10,0x90(%r11)
	vmovdqa		%xmm11,0xa0(%r11)
	vmovdqa		%xmm12,0xb0(%r11)
	vmovdqa		%xmm13,0xc0(%r11)
	vmovdqa		%xmm14,0xd0(%r11)
	vmovdqa		%xmm15,0xe0(%r11)
.Ldo_avx_body:
___
$code.=<<___;
	sub		\$64,$len
	lea		-32($inp),%rax
	cmovc		%rax,$inp

	vmovdqu		`16*3`($ctx),$D4	# preload r0^2
	lea		`16*3+64`($ctx),$ctx	# size optimization
	lea		.Lconst(%rip),%rcx

	################################################################
	# load input
	vmovdqu		16*2($inp),$T0
	vmovdqu		16*3($inp),$T1
	vmovdqa		64(%rcx),$MASK		# .Lmask26

	vpsrldq		\$6,$T0,$T2		# splat input
	vpsrldq		\$6,$T1,$T3
	vpunpckhqdq	$T1,$T0,$T4		# 4
	vpunpcklqdq	$T1,$T0,$T0		# 0:1
	vpunpcklqdq	$T3,$T2,$T3		# 2:3

	vpsrlq		\$40,$T4,$T4		# 4
	vpsrlq		\$26,$T0,$T1
	vpand		$MASK,$T0,$T0		# 0
	vpsrlq		\$4,$T3,$T2
	vpand		$MASK,$T1,$T1		# 1
	vpsrlq		\$30,$T3,$T3
	vpand		$MASK,$T2,$T2		# 2
	vpand		$MASK,$T3,$T3		# 3
	vpor		32(%rcx),$T4,$T4	# padbit, yes, always

	jbe		.Lskip_loop_avx

	# expand and copy pre-calculated table to stack
	vmovdqu		`16*1-64`($ctx),$D1
	vmovdqu		`16*2-64`($ctx),$D2
	vpshufd		\$0xEE,$D4,$D3		# 34xx -> 3434
	vpshufd		\$0x44,$D4,$D0		# xx12 -> 1212
	vmovdqa		$D3,-0x90(%r11)
	vmovdqa		$D0,0x00(%rsp)
	vpshufd		\$0xEE,$D1,$D4
	vmovdqu		`16*3-64`($ctx),$D0
	vpshufd		\$0x44,$D1,$D1
	vmovdqa		$D4,-0x80(%r11)
	vmovdqa		$D1,0x10(%rsp)
	vpshufd		\$0xEE,$D2,$D3
	vmovdqu		`16*4-64`($ctx),$D1
	vpshufd		\$0x44,$D2,$D2
	vmovdqa		$D3,-0x70(%r11)
	vmovdqa		$D2,0x20(%rsp)
	vpshufd		\$0xEE,$D0,$D4
	vmovdqu		`16*5-64`($ctx),$D2
	vpshufd		\$0x44,$D0,$D0
	vmovdqa		$D4,-0x60(%r11)
	vmovdqa		$D0,0x30(%rsp)
	vpshufd		\$0xEE,$D1,$D3
	vmovdqu		`16*6-64`($ctx),$D0
	vpshufd		\$0x44,$D1,$D1
	vmovdqa		$D3,-0x50(%r11)
	vmovdqa		$D1,0x40(%rsp)
	vpshufd		\$0xEE,$D2,$D4
	vmovdqu		`16*7-64`($ctx),$D1
	vpshufd		\$0x44,$D2,$D2
	vmovdqa		$D4,-0x40(%r11)
	vmovdqa		$D2,0x50(%rsp)
	vpshufd		\$0xEE,$D0,$D3
	vmovdqu		`16*8-64`($ctx),$D2
	vpshufd		\$0x44,$D0,$D0
	vmovdqa		$D3,-0x30(%r11)
	vmovdqa		$D0,0x60(%rsp)
	vpshufd		\$0xEE,$D1,$D4
	vpshufd		\$0x44,$D1,$D1
	vmovdqa		$D4,-0x20(%r11)
	vmovdqa		$D1,0x70(%rsp)
	vpshufd		\$0xEE,$D2,$D3
	 vmovdqa	0x00(%rsp),$D4		# preload r0^2
	vpshufd		\$0x44,$D2,$D2
	vmovdqa		$D3,-0x10(%r11)
	vmovdqa		$D2,0x80(%rsp)

	jmp		.Loop_avx

.align	32
.Loop_avx:
	################################################################
	# ((inp[0]*r^4+inp[2]*r^2+inp[4])*r^4+inp[6]*r^2
	# ((inp[1]*r^4+inp[3]*r^2+inp[5])*r^3+inp[7]*r
	#   \___________________/
	# ((inp[0]*r^4+inp[2]*r^2+inp[4])*r^4+inp[6]*r^2+inp[8])*r^2
	# ((inp[1]*r^4+inp[3]*r^2+inp[5])*r^4+inp[7]*r^2+inp[9])*r
	#   \___________________/ \____________________/
	#
	# Note that we start with inp[2:3]*r^2. This is because it
	# doesn't depend on reduction in previous iteration.
	################################################################
	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	#
	# though note that $Tx and $Hx are "reversed" in this section,
	# and $D4 is preloaded with r0^2...

	vpmuludq	$T0,$D4,$D0		# d0 = h0*r0
	vpmuludq	$T1,$D4,$D1		# d1 = h1*r0
	  vmovdqa	$H2,0x20(%r11)				# offload hash
	vpmuludq	$T2,$D4,$D2		# d3 = h2*r0
	 vmovdqa	0x10(%rsp),$H2		# r1^2
	vpmuludq	$T3,$D4,$D3		# d3 = h3*r0
	vpmuludq	$T4,$D4,$D4		# d4 = h4*r0

	  vmovdqa	$H0,0x00(%r11)				#
	vpmuludq	0x20(%rsp),$T4,$H0	# h4*s1
	  vmovdqa	$H1,0x10(%r11)				#
	vpmuludq	$T3,$H2,$H1		# h3*r1
	vpaddq		$H0,$D0,$D0		# d0 += h4*s1
	vpaddq		$H1,$D4,$D4		# d4 += h3*r1
	  vmovdqa	$H3,0x30(%r11)				#
	vpmuludq	$T2,$H2,$H0		# h2*r1
	vpmuludq	$T1,$H2,$H1		# h1*r1
	vpaddq		$H0,$D3,$D3		# d3 += h2*r1
	 vmovdqa	0x30(%rsp),$H3		# r2^2
	vpaddq		$H1,$D2,$D2		# d2 += h1*r1
	  vmovdqa	$H4,0x40(%r11)				#
	vpmuludq	$T0,$H2,$H2		# h0*r1
	 vpmuludq	$T2,$H3,$H0		# h2*r2
	vpaddq		$H2,$D1,$D1		# d1 += h0*r1

	 vmovdqa	0x40(%rsp),$H4		# s2^2
	vpaddq		$H0,$D4,$D4		# d4 += h2*r2
	vpmuludq	$T1,$H3,$H1		# h1*r2
	vpmuludq	$T0,$H3,$H3		# h0*r2
	vpaddq		$H1,$D3,$D3		# d3 += h1*r2
	 vmovdqa	0x50(%rsp),$H2		# r3^2
	vpaddq		$H3,$D2,$D2		# d2 += h0*r2
	vpmuludq	$T4,$H4,$H0		# h4*s2
	vpmuludq	$T3,$H4,$H4		# h3*s2
	vpaddq		$H0,$D1,$D1		# d1 += h4*s2
	 vmovdqa	0x60(%rsp),$H3		# s3^2
	vpaddq		$H4,$D0,$D0		# d0 += h3*s2

	 vmovdqa	0x80(%rsp),$H4		# s4^2
	vpmuludq	$T1,$H2,$H1		# h1*r3
	vpmuludq	$T0,$H2,$H2		# h0*r3
	vpaddq		$H1,$D4,$D4		# d4 += h1*r3
	vpaddq		$H2,$D3,$D3		# d3 += h0*r3
	vpmuludq	$T4,$H3,$H0		# h4*s3
	vpmuludq	$T3,$H3,$H1		# h3*s3
	vpaddq		$H0,$D2,$D2		# d2 += h4*s3
	 vmovdqu	16*0($inp),$H0				# load input
	vpaddq		$H1,$D1,$D1		# d1 += h3*s3
	vpmuludq	$T2,$H3,$H3		# h2*s3
	 vpmuludq	$T2,$H4,$T2		# h2*s4
	vpaddq		$H3,$D0,$D0		# d0 += h2*s3

	 vmovdqu	16*1($inp),$H1				#
	vpaddq		$T2,$D1,$D1		# d1 += h2*s4
	vpmuludq	$T3,$H4,$T3		# h3*s4
	vpmuludq	$T4,$H4,$T4		# h4*s4
	 vpsrldq	\$6,$H0,$H2				# splat input
	vpaddq		$T3,$D2,$D2		# d2 += h3*s4
	vpaddq		$T4,$D3,$D3		# d3 += h4*s4
	 vpsrldq	\$6,$H1,$H3				#
	vpmuludq	0x70(%rsp),$T0,$T4	# h0*r4
	vpmuludq	$T1,$H4,$T0		# h1*s4
	 vpunpckhqdq	$H1,$H0,$H4		# 4
	vpaddq		$T4,$D4,$D4		# d4 += h0*r4
	 vmovdqa	-0x90(%r11),$T4		# r0^4
	vpaddq		$T0,$D0,$D0		# d0 += h1*s4

	vpunpcklqdq	$H1,$H0,$H0		# 0:1
	vpunpcklqdq	$H3,$H2,$H3		# 2:3

	#vpsrlq		\$40,$H4,$H4		# 4
	vpsrldq		\$`40/8`,$H4,$H4	# 4
	vpsrlq		\$26,$H0,$H1
	vpand		$MASK,$H0,$H0		# 0
	vpsrlq		\$4,$H3,$H2
	vpand		$MASK,$H1,$H1		# 1
	vpand		0(%rcx),$H4,$H4		# .Lmask24
	vpsrlq		\$30,$H3,$H3
	vpand		$MASK,$H2,$H2		# 2
	vpand		$MASK,$H3,$H3		# 3
	vpor		32(%rcx),$H4,$H4	# padbit, yes, always

	vpaddq		0x00(%r11),$H0,$H0	# add hash value
	vpaddq		0x10(%r11),$H1,$H1
	vpaddq		0x20(%r11),$H2,$H2
	vpaddq		0x30(%r11),$H3,$H3
	vpaddq		0x40(%r11),$H4,$H4

	lea		16*2($inp),%rax
	lea		16*4($inp),$inp
	sub		\$64,$len
	cmovc		%rax,$inp

	################################################################
	# Now we accumulate (inp[0:1]+hash)*r^4
	################################################################
	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4

	vpmuludq	$H0,$T4,$T0		# h0*r0
	vpmuludq	$H1,$T4,$T1		# h1*r0
	vpaddq		$T0,$D0,$D0
	vpaddq		$T1,$D1,$D1
	 vmovdqa	-0x80(%r11),$T2		# r1^4
	vpmuludq	$H2,$T4,$T0		# h2*r0
	vpmuludq	$H3,$T4,$T1		# h3*r0
	vpaddq		$T0,$D2,$D2
	vpaddq		$T1,$D3,$D3
	vpmuludq	$H4,$T4,$T4		# h4*r0
	 vpmuludq	-0x70(%r11),$H4,$T0	# h4*s1
	vpaddq		$T4,$D4,$D4

	vpaddq		$T0,$D0,$D0		# d0 += h4*s1
	vpmuludq	$H2,$T2,$T1		# h2*r1
	vpmuludq	$H3,$T2,$T0		# h3*r1
	vpaddq		$T1,$D3,$D3		# d3 += h2*r1
	 vmovdqa	-0x60(%r11),$T3		# r2^4
	vpaddq		$T0,$D4,$D4		# d4 += h3*r1
	vpmuludq	$H1,$T2,$T1		# h1*r1
	vpmuludq	$H0,$T2,$T2		# h0*r1
	vpaddq		$T1,$D2,$D2		# d2 += h1*r1
	vpaddq		$T2,$D1,$D1		# d1 += h0*r1

	 vmovdqa	-0x50(%r11),$T4		# s2^4
	vpmuludq	$H2,$T3,$T0		# h2*r2
	vpmuludq	$H1,$T3,$T1		# h1*r2
	vpaddq		$T0,$D4,$D4		# d4 += h2*r2
	vpaddq		$T1,$D3,$D3		# d3 += h1*r2
	 vmovdqa	-0x40(%r11),$T2		# r3^4
	vpmuludq	$H0,$T3,$T3		# h0*r2
	vpmuludq	$H4,$T4,$T0		# h4*s2
	vpaddq		$T3,$D2,$D2		# d2 += h0*r2
	vpaddq		$T0,$D1,$D1		# d1 += h4*s2
	 vmovdqa	-0x30(%r11),$T3		# s3^4
	vpmuludq	$H3,$T4,$T4		# h3*s2
	 vpmuludq	$H1,$T2,$T1		# h1*r3
	vpaddq		$T4,$D0,$D0		# d0 += h3*s2

	 vmovdqa	-0x10(%r11),$T4		# s4^4
	vpaddq		$T1,$D4,$D4		# d4 += h1*r3
	vpmuludq	$H0,$T2,$T2		# h0*r3
	vpmuludq	$H4,$T3,$T0		# h4*s3
	vpaddq		$T2,$D3,$D3		# d3 += h0*r3
	vpaddq		$T0,$D2,$D2		# d2 += h4*s3
	 vmovdqu	16*2($inp),$T0				# load input
	vpmuludq	$H3,$T3,$T2		# h3*s3
	vpmuludq	$H2,$T3,$T3		# h2*s3
	vpaddq		$T2,$D1,$D1		# d1 += h3*s3
	 vmovdqu	16*3($inp),$T1				#
	vpaddq		$T3,$D0,$D0		# d0 += h2*s3

	vpmuludq	$H2,$T4,$H2		# h2*s4
	vpmuludq	$H3,$T4,$H3		# h3*s4
	 vpsrldq	\$6,$T0,$T2				# splat input
	vpaddq		$H2,$D1,$D1		# d1 += h2*s4
	vpmuludq	$H4,$T4,$H4		# h4*s4
	 vpsrldq	\$6,$T1,$T3				#
	vpaddq		$H3,$D2,$H2		# h2 = d2 + h3*s4
	vpaddq		$H4,$D3,$H3		# h3 = d3 + h4*s4
	vpmuludq	-0x20(%r11),$H0,$H4	# h0*r4
	vpmuludq	$H1,$T4,$H0
	 vpunpckhqdq	$T1,$T0,$T4		# 4
	vpaddq		$H4,$D4,$H4		# h4 = d4 + h0*r4
	vpaddq		$H0,$D0,$H0		# h0 = d0 + h1*s4

	vpunpcklqdq	$T1,$T0,$T0		# 0:1
	vpunpcklqdq	$T3,$T2,$T3		# 2:3

	#vpsrlq		\$40,$T4,$T4		# 4
	vpsrldq		\$`40/8`,$T4,$T4	# 4
	vpsrlq		\$26,$T0,$T1
	 vmovdqa	0x00(%rsp),$D4		# preload r0^2
	vpand		$MASK,$T0,$T0		# 0
	vpsrlq		\$4,$T3,$T2
	vpand		$MASK,$T1,$T1		# 1
	vpand		0(%rcx),$T4,$T4		# .Lmask24
	vpsrlq		\$30,$T3,$T3
	vpand		$MASK,$T2,$T2		# 2
	vpand		$MASK,$T3,$T3		# 3
	vpor		32(%rcx),$T4,$T4	# padbit, yes, always

	################################################################
	# lazy reduction as discussed in "NEON crypto" by D.J. Bernstein
	# and P. Schwabe

	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		# h3 -> h4

	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$D1,$H1		# h0 -> h1

	vpsrlq		\$26,$H4,$D0
	vpand		$MASK,$H4,$H4

	vpsrlq		\$26,$H1,$D1
	vpand		$MASK,$H1,$H1
	vpaddq		$D1,$H2,$H2		# h1 -> h2

	vpaddq		$D0,$H0,$H0
	vpsllq		\$2,$D0,$D0
	vpaddq		$D0,$H0,$H0		# h4 -> h0

	vpsrlq		\$26,$H2,$D2
	vpand		$MASK,$H2,$H2
	vpaddq		$D2,$H3,$H3		# h2 -> h3

	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		# h0 -> h1

	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		# h3 -> h4

	ja		.Loop_avx

.Lskip_loop_avx:
	################################################################
	# multiply (inp[0:1]+hash) or inp[2:3] by r^2:r^1

	vpshufd		\$0x10,$D4,$D4		# r0^n, xx12 -> x1x2
	add		\$32,$len
	jnz		.Long_tail_avx

	vpaddq		$H2,$T2,$T2
	vpaddq		$H0,$T0,$T0
	vpaddq		$H1,$T1,$T1
	vpaddq		$H3,$T3,$T3
	vpaddq		$H4,$T4,$T4

.Long_tail_avx:
	vmovdqa		$H2,0x20(%r11)
	vmovdqa		$H0,0x00(%r11)
	vmovdqa		$H1,0x10(%r11)
	vmovdqa		$H3,0x30(%r11)
	vmovdqa		$H4,0x40(%r11)

	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4

	vpmuludq	$T2,$D4,$D2		# d2 = h2*r0
	vpmuludq	$T0,$D4,$D0		# d0 = h0*r0
	 vpshufd	\$0x10,`16*1-64`($ctx),$H2		# r1^n
	vpmuludq	$T1,$D4,$D1		# d1 = h1*r0
	vpmuludq	$T3,$D4,$D3		# d3 = h3*r0
	vpmuludq	$T4,$D4,$D4		# d4 = h4*r0

	vpmuludq	$T3,$H2,$H0		# h3*r1
	vpaddq		$H0,$D4,$D4		# d4 += h3*r1
	 vpshufd	\$0x10,`16*2-64`($ctx),$H3		# s1^n
	vpmuludq	$T2,$H2,$H1		# h2*r1
	vpaddq		$H1,$D3,$D3		# d3 += h2*r1
	 vpshufd	\$0x10,`16*3-64`($ctx),$H4		# r2^n
	vpmuludq	$T1,$H2,$H0		# h1*r1
	vpaddq		$H0,$D2,$D2		# d2 += h1*r1
	vpmuludq	$T0,$H2,$H2		# h0*r1
	vpaddq		$H2,$D1,$D1		# d1 += h0*r1
	vpmuludq	$T4,$H3,$H3		# h4*s1
	vpaddq		$H3,$D0,$D0		# d0 += h4*s1

	 vpshufd	\$0x10,`16*4-64`($ctx),$H2		# s2^n
	vpmuludq	$T2,$H4,$H1		# h2*r2
	vpaddq		$H1,$D4,$D4		# d4 += h2*r2
	vpmuludq	$T1,$H4,$H0		# h1*r2
	vpaddq		$H0,$D3,$D3		# d3 += h1*r2
	 vpshufd	\$0x10,`16*5-64`($ctx),$H3		# r3^n
	vpmuludq	$T0,$H4,$H4		# h0*r2
	vpaddq		$H4,$D2,$D2		# d2 += h0*r2
	vpmuludq	$T4,$H2,$H1		# h4*s2
	vpaddq		$H1,$D1,$D1		# d1 += h4*s2
	 vpshufd	\$0x10,`16*6-64`($ctx),$H4		# s3^n
	vpmuludq	$T3,$H2,$H2		# h3*s2
	vpaddq		$H2,$D0,$D0		# d0 += h3*s2

	vpmuludq	$T1,$H3,$H0		# h1*r3
	vpaddq		$H0,$D4,$D4		# d4 += h1*r3
	vpmuludq	$T0,$H3,$H3		# h0*r3
	vpaddq		$H3,$D3,$D3		# d3 += h0*r3
	 vpshufd	\$0x10,`16*7-64`($ctx),$H2		# r4^n
	vpmuludq	$T4,$H4,$H1		# h4*s3
	vpaddq		$H1,$D2,$D2		# d2 += h4*s3
	 vpshufd	\$0x10,`16*8-64`($ctx),$H3		# s4^n
	vpmuludq	$T3,$H4,$H0		# h3*s3
	vpaddq		$H0,$D1,$D1		# d1 += h3*s3
	vpmuludq	$T2,$H4,$H4		# h2*s3
	vpaddq		$H4,$D0,$D0		# d0 += h2*s3

	vpmuludq	$T0,$H2,$H2		# h0*r4
	vpaddq		$H2,$D4,$D4		# h4 = d4 + h0*r4
	vpmuludq	$T4,$H3,$H1		# h4*s4
	vpaddq		$H1,$D3,$D3		# h3 = d3 + h4*s4
	vpmuludq	$T3,$H3,$H0		# h3*s4
	vpaddq		$H0,$D2,$D2		# h2 = d2 + h3*s4
	vpmuludq	$T2,$H3,$H1		# h2*s4
	vpaddq		$H1,$D1,$D1		# h1 = d1 + h2*s4
	vpmuludq	$T1,$H3,$H3		# h1*s4
	vpaddq		$H3,$D0,$D0		# h0 = d0 + h1*s4

	jz		.Lshort_tail_avx

	vmovdqu		16*0($inp),$H0		# load input
	vmovdqu		16*1($inp),$H1

	vpsrldq		\$6,$H0,$H2		# splat input
	vpsrldq		\$6,$H1,$H3
	vpunpckhqdq	$H1,$H0,$H4		# 4
	vpunpcklqdq	$H1,$H0,$H0		# 0:1
	vpunpcklqdq	$H3,$H2,$H3		# 2:3

	vpsrlq		\$40,$H4,$H4		# 4
	vpsrlq		\$26,$H0,$H1
	vpand		$MASK,$H0,$H0		# 0
	vpsrlq		\$4,$H3,$H2
	vpand		$MASK,$H1,$H1		# 1
	vpsrlq		\$30,$H3,$H3
	vpand		$MASK,$H2,$H2		# 2
	vpand		$MASK,$H3,$H3		# 3
	vpor		32(%rcx),$H4,$H4	# padbit, yes, always

	vpshufd		\$0x32,`16*0-64`($ctx),$T4	# r0^n, 34xx -> x3x4
	vpaddq		0x00(%r11),$H0,$H0
	vpaddq		0x10(%r11),$H1,$H1
	vpaddq		0x20(%r11),$H2,$H2
	vpaddq		0x30(%r11),$H3,$H3
	vpaddq		0x40(%r11),$H4,$H4

	################################################################
	# multiply (inp[0:1]+hash) by r^4:r^3 and accumulate

	vpmuludq	$H0,$T4,$T0		# h0*r0
	vpaddq		$T0,$D0,$D0		# d0 += h0*r0
	vpmuludq	$H1,$T4,$T1		# h1*r0
	vpaddq		$T1,$D1,$D1		# d1 += h1*r0
	vpmuludq	$H2,$T4,$T0		# h2*r0
	vpaddq		$T0,$D2,$D2		# d2 += h2*r0
	 vpshufd	\$0x32,`16*1-64`($ctx),$T2		# r1^n
	vpmuludq	$H3,$T4,$T1		# h3*r0
	vpaddq		$T1,$D3,$D3		# d3 += h3*r0
	vpmuludq	$H4,$T4,$T4		# h4*r0
	vpaddq		$T4,$D4,$D4		# d4 += h4*r0

	vpmuludq	$H3,$T2,$T0		# h3*r1
	vpaddq		$T0,$D4,$D4		# d4 += h3*r1
	 vpshufd	\$0x32,`16*2-64`($ctx),$T3		# s1
	vpmuludq	$H2,$T2,$T1		# h2*r1
	vpaddq		$T1,$D3,$D3		# d3 += h2*r1
	 vpshufd	\$0x32,`16*3-64`($ctx),$T4		# r2
	vpmuludq	$H1,$T2,$T0		# h1*r1
	vpaddq		$T0,$D2,$D2		# d2 += h1*r1
	vpmuludq	$H0,$T2,$T2		# h0*r1
	vpaddq		$T2,$D1,$D1		# d1 += h0*r1
	vpmuludq	$H4,$T3,$T3		# h4*s1
	vpaddq		$T3,$D0,$D0		# d0 += h4*s1

	 vpshufd	\$0x32,`16*4-64`($ctx),$T2		# s2
	vpmuludq	$H2,$T4,$T1		# h2*r2
	vpaddq		$T1,$D4,$D4		# d4 += h2*r2
	vpmuludq	$H1,$T4,$T0		# h1*r2
	vpaddq		$T0,$D3,$D3		# d3 += h1*r2
	 vpshufd	\$0x32,`16*5-64`($ctx),$T3		# r3
	vpmuludq	$H0,$T4,$T4		# h0*r2
	vpaddq		$T4,$D2,$D2		# d2 += h0*r2
	vpmuludq	$H4,$T2,$T1		# h4*s2
	vpaddq		$T1,$D1,$D1		# d1 += h4*s2
	 vpshufd	\$0x32,`16*6-64`($ctx),$T4		# s3
	vpmuludq	$H3,$T2,$T2		# h3*s2
	vpaddq		$T2,$D0,$D0		# d0 += h3*s2

	vpmuludq	$H1,$T3,$T0		# h1*r3
	vpaddq		$T0,$D4,$D4		# d4 += h1*r3
	vpmuludq	$H0,$T3,$T3		# h0*r3
	vpaddq		$T3,$D3,$D3		# d3 += h0*r3
	 vpshufd	\$0x32,`16*7-64`($ctx),$T2		# r4
	vpmuludq	$H4,$T4,$T1		# h4*s3
	vpaddq		$T1,$D2,$D2		# d2 += h4*s3
	 vpshufd	\$0x32,`16*8-64`($ctx),$T3		# s4
	vpmuludq	$H3,$T4,$T0		# h3*s3
	vpaddq		$T0,$D1,$D1		# d1 += h3*s3
	vpmuludq	$H2,$T4,$T4		# h2*s3
	vpaddq		$T4,$D0,$D0		# d0 += h2*s3

	vpmuludq	$H0,$T2,$T2		# h0*r4
	vpaddq		$T2,$D4,$D4		# d4 += h0*r4
	vpmuludq	$H4,$T3,$T1		# h4*s4
	vpaddq		$T1,$D3,$D3		# d3 += h4*s4
	vpmuludq	$H3,$T3,$T0		# h3*s4
	vpaddq		$T0,$D2,$D2		# d2 += h3*s4
	vpmuludq	$H2,$T3,$T1		# h2*s4
	vpaddq		$T1,$D1,$D1		# d1 += h2*s4
	vpmuludq	$H1,$T3,$T3		# h1*s4
	vpaddq		$T3,$D0,$D0		# d0 += h1*s4

.Lshort_tail_avx:
	################################################################
	# horizontal addition

	vpsrldq		\$8,$D4,$T4
	vpsrldq		\$8,$D3,$T3
	vpsrldq		\$8,$D1,$T1
	vpsrldq		\$8,$D0,$T0
	vpsrldq		\$8,$D2,$T2
	vpaddq		$T3,$D3,$D3
	vpaddq		$T4,$D4,$D4
	vpaddq		$T0,$D0,$D0
	vpaddq		$T1,$D1,$D1
	vpaddq		$T2,$D2,$D2

	################################################################
	# lazy reduction

	vpsrlq		\$26,$D3,$H3
	vpand		$MASK,$D3,$D3
	vpaddq		$H3,$D4,$D4		# h3 -> h4

	vpsrlq		\$26,$D0,$H0
	vpand		$MASK,$D0,$D0
	vpaddq		$H0,$D1,$D1		# h0 -> h1

	vpsrlq		\$26,$D4,$H4
	vpand		$MASK,$D4,$D4

	vpsrlq		\$26,$D1,$H1
	vpand		$MASK,$D1,$D1
	vpaddq		$H1,$D2,$D2		# h1 -> h2

	vpaddq		$H4,$D0,$D0
	vpsllq		\$2,$H4,$H4
	vpaddq		$H4,$D0,$D0		# h4 -> h0

	vpsrlq		\$26,$D2,$H2
	vpand		$MASK,$D2,$D2
	vpaddq		$H2,$D3,$D3		# h2 -> h3

	vpsrlq		\$26,$D0,$H0
	vpand		$MASK,$D0,$D0
	vpaddq		$H0,$D1,$D1		# h0 -> h1

	vpsrlq		\$26,$D3,$H3
	vpand		$MASK,$D3,$D3
	vpaddq		$H3,$D4,$D4		# h3 -> h4

	vmovd		$D0,`4*0-48-64`($ctx)	# save partially reduced
	vmovd		$D1,`4*1-48-64`($ctx)
	vmovd		$D2,`4*2-48-64`($ctx)
	vmovd		$D3,`4*3-48-64`($ctx)
	vmovd		$D4,`4*4-48-64`($ctx)
___
$code.=<<___	if ($win64);
	vmovdqa		0x50(%r11),%xmm6
	vmovdqa		0x60(%r11),%xmm7
	vmovdqa		0x70(%r11),%xmm8
	vmovdqa		0x80(%r11),%xmm9
	vmovdqa		0x90(%r11),%xmm10
	vmovdqa		0xa0(%r11),%xmm11
	vmovdqa		0xb0(%r11),%xmm12
	vmovdqa		0xc0(%r11),%xmm13
	vmovdqa		0xd0(%r11),%xmm14
	vmovdqa		0xe0(%r11),%xmm15
	lea		0xf8(%r11),%rsp
.Ldo_avx_epilogue:
___
$code.=<<___	if (!$win64);
	lea		0x58(%r11),%rsp
___
$code.=<<___;
	vzeroupper
	ret
.size	poly1305_blocks_avx,.-poly1305_blocks_avx

.type	poly1305_emit_avx,\@function,3
.align	32
poly1305_emit_avx:
	cmpl	\$0,20($ctx)	# is_base2_26?
	je	.Lemit

	mov	0($ctx),%eax	# load hash value base 2^26
	mov	4($ctx),%ecx
	mov	8($ctx),%r8d
	mov	12($ctx),%r11d
	mov	16($ctx),%r10d

	shl	\$26,%rcx	# base 2^26 -> base 2^64
	mov	%r8,%r9
	shl	\$52,%r8
	add	%rcx,%rax
	shr	\$12,%r9
	add	%rax,%r8	# h0
	adc	\$0,%r9

	shl	\$14,%r11
	mov	%r10,%rax
	shr	\$24,%r10
	add	%r11,%r9
	shl	\$40,%rax
	add	%rax,%r9	# h1
	adc	\$0,%r10	# h2

	mov	%r10,%rax	# could be partially reduced, so reduce
	mov	%r10,%rcx
	and	\$3,%r10
	shr	\$2,%rax
	and	\$-4,%rcx
	add	%rcx,%rax
	add	%rax,%r8
	adc	\$0,%r9
	adc	\$0,%r10

	mov	%r8,%rax
	add	\$5,%r8		# compare to modulus
	mov	%r9,%rcx
	adc	\$0,%r9
	adc	\$0,%r10
	shr	\$2,%r10	# did 130-bit value overfow?
	cmovnz	%r8,%rax
	cmovnz	%r9,%rcx

	add	0($nonce),%rax	# accumulate nonce
	adc	8($nonce),%rcx
	mov	%rax,0($mac)	# write result
	mov	%rcx,8($mac)

	ret
.size	poly1305_emit_avx,.-poly1305_emit_avx
___

if ($avx>1) {
my ($H0,$H1,$H2,$H3,$H4, $MASK, $T4,$T0,$T1,$T2,$T3, $D0,$D1,$D2,$D3,$D4) =
    map("%ymm$_",(0..15));
my $S4=$MASK;

$code.=<<___;
.type	poly1305_blocks_avx2,\@function,4
.align	32
poly1305_blocks_avx2:
	mov	20($ctx),%r8d		# is_base2_26
	cmp	\$128,$len
	jae	.Lblocks_avx2
	test	%r8d,%r8d
	jz	.Lblocks

.Lblocks_avx2:
	and	\$-16,$len
	jz	.Lno_data_avx2

	vzeroupper

	test	%r8d,%r8d
	jz	.Lbase2_64_avx2

	test	\$63,$len
	jz	.Leven_avx2

	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lblocks_avx2_body:

	mov	$len,%r15		# reassign $len

	mov	0($ctx),$d1		# load hash value
	mov	8($ctx),$d2
	mov	16($ctx),$h2#d

	mov	24($ctx),$r0		# load r
	mov	32($ctx),$s1

	################################# base 2^26 -> base 2^64
	mov	$d1#d,$h0#d
	and	\$`-1*(1<<31)`,$d1
	mov	$d2,$r1			# borrow $r1
	mov	$d2#d,$h1#d
	and	\$`-1*(1<<31)`,$d2

	shr	\$6,$d1
	shl	\$52,$r1
	add	$d1,$h0
	shr	\$12,$h1
	shr	\$18,$d2
	add	$r1,$h0
	adc	$d2,$h1

	mov	$h2,$d1
	shl	\$40,$d1
	shr	\$24,$h2
	add	$d1,$h1
	adc	\$0,$h2			# can be partially reduced...

	mov	\$-4,$d2		# ... so reduce
	mov	$h2,$d1
	and	$h2,$d2
	shr	\$2,$d1
	and	\$3,$h2
	add	$d2,$d1			# =*5
	add	$d1,$h0
	adc	\$0,$h1
	adc	\$0,$h2

	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			# s1 = r1 + (r1 >> 2)

.Lbase2_26_pre_avx2:
	add	0($inp),$h0		# accumulate input
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
	sub	\$16,%r15

	call	__poly1305_block
	mov	$r1,%rax

	test	\$63,%r15
	jnz	.Lbase2_26_pre_avx2

	test	$padbit,$padbit		# if $padbit is zero,
	jz	.Lstore_base2_64_avx2	# store hash in base 2^64 format

	################################# base 2^64 -> base 2^26
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$r0
	mov	$h1,$r1
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	# h[0]
	shl	\$12,$r0
	and	\$0x3ffffff,%rdx	# h[1]
	shr	\$14,$h1
	or	$r0,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		# h[2]
	shr	\$40,$r1
	and	\$0x3ffffff,$h1		# h[3]
	or	$r1,$h2			# h[4]

	test	%r15,%r15
	jz	.Lstore_base2_26_avx2

	vmovd	%rax#d,%x#$H0
	vmovd	%rdx#d,%x#$H1
	vmovd	$h0#d,%x#$H2
	vmovd	$h1#d,%x#$H3
	vmovd	$h2#d,%x#$H4
	jmp	.Lproceed_avx2

.align	32
.Lstore_base2_64_avx2:
	mov	$h0,0($ctx)
	mov	$h1,8($ctx)
	mov	$h2,16($ctx)		# note that is_base2_26 is zeroed
	jmp	.Ldone_avx2

.align	16
.Lstore_base2_26_avx2:
	mov	%rax#d,0($ctx)		# store hash value base 2^26
	mov	%rdx#d,4($ctx)
	mov	$h0#d,8($ctx)
	mov	$h1#d,12($ctx)
	mov	$h2#d,16($ctx)
.align	16
.Ldone_avx2:
	mov	0(%rsp),%r15
	mov	8(%rsp),%r14
	mov	16(%rsp),%r13
	mov	24(%rsp),%r12
	mov	32(%rsp),%rbp
	mov	40(%rsp),%rbx
	lea	48(%rsp),%rsp
.Lno_data_avx2:
.Lblocks_avx2_epilogue:
	ret

.align	32
.Lbase2_64_avx2:
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
.Lbase2_64_avx2_body:

	mov	$len,%r15		# reassign $len

	mov	24($ctx),$r0		# load r
	mov	32($ctx),$s1

	mov	0($ctx),$h0		# load hash value
	mov	8($ctx),$h1
	mov	16($ctx),$h2#d

	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			# s1 = r1 + (r1 >> 2)

	test	\$63,$len
	jz	.Linit_avx2

.Lbase2_64_pre_avx2:
	add	0($inp),$h0		# accumulate input
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
	sub	\$16,%r15

	call	__poly1305_block
	mov	$r1,%rax

	test	\$63,%r15
	jnz	.Lbase2_64_pre_avx2

.Linit_avx2:
	################################# base 2^64 -> base 2^26
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$d1
	mov	$h1,$d2
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	# h[0]
	shl	\$12,$d1
	and	\$0x3ffffff,%rdx	# h[1]
	shr	\$14,$h1
	or	$d1,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		# h[2]
	shr	\$40,$d2
	and	\$0x3ffffff,$h1		# h[3]
	or	$d2,$h2			# h[4]

	vmovd	%rax#d,%x#$H0
	vmovd	%rdx#d,%x#$H1
	vmovd	$h0#d,%x#$H2
	vmovd	$h1#d,%x#$H3
	vmovd	$h2#d,%x#$H4
	movl	\$1,20($ctx)		# set is_base2_26

	call	__poly1305_init_avx

.Lproceed_avx2:
	mov	%r15,$len

	mov	0(%rsp),%r15
	mov	8(%rsp),%r14
	mov	16(%rsp),%r13
	mov	24(%rsp),%r12
	mov	32(%rsp),%rbp
	mov	40(%rsp),%rbx
	lea	48(%rsp),%rax
	lea	48(%rsp),%rsp
.Lbase2_64_avx2_epilogue:
	jmp	.Ldo_avx2

.align	32
.Leven_avx2:
	vmovd		4*0($ctx),%x#$H0	# load hash value base 2^26
	vmovd		4*1($ctx),%x#$H1
	vmovd		4*2($ctx),%x#$H2
	vmovd		4*3($ctx),%x#$H3
	vmovd		4*4($ctx),%x#$H4

.Ldo_avx2:
___
$code.=<<___	if (!$win64);
	lea		-8(%rsp),%r11
	sub		\$0x128,%rsp
___
$code.=<<___	if ($win64);
	lea		-0xf8(%rsp),%r11
	sub		\$0x1c8,%rsp
	vmovdqa		%xmm6,0x50(%r11)
	vmovdqa		%xmm7,0x60(%r11)
	vmovdqa		%xmm8,0x70(%r11)
	vmovdqa		%xmm9,0x80(%r11)
	vmovdqa		%xmm10,0x90(%r11)
	vmovdqa		%xmm11,0xa0(%r11)
	vmovdqa		%xmm12,0xb0(%r11)
	vmovdqa		%xmm13,0xc0(%r11)
	vmovdqa		%xmm14,0xd0(%r11)
	vmovdqa		%xmm15,0xe0(%r11)
.Ldo_avx2_body:
___
$code.=<<___;
	lea		48+64($ctx),$ctx	# size optimization
	lea		.Lconst(%rip),%rcx

	# expand and copy pre-calculated table to stack
	vmovdqu		`16*0-64`($ctx),%x#$T2
	and		\$-512,%rsp
	vmovdqu		`16*1-64`($ctx),%x#$T3
	vmovdqu		`16*2-64`($ctx),%x#$T4
	vmovdqu		`16*3-64`($ctx),%x#$D0
	vmovdqu		`16*4-64`($ctx),%x#$D1
	vmovdqu		`16*5-64`($ctx),%x#$D2
	vmovdqu		`16*6-64`($ctx),%x#$D3
	vpermq		\$0x15,$T2,$T2		# 00003412 -> 12343434
	vmovdqu		`16*7-64`($ctx),%x#$D4
	vpermq		\$0x15,$T3,$T3
	vpshufd		\$0xc8,$T2,$T2		# 12343434 -> 14243444
	vmovdqu		`16*8-64`($ctx),%x#$MASK
	vpermq		\$0x15,$T4,$T4
	vpshufd		\$0xc8,$T3,$T3
	vmovdqa		$T2,0x00(%rsp)
	vpermq		\$0x15,$D0,$D0
	vpshufd		\$0xc8,$T4,$T4
	vmovdqa		$T3,0x20(%rsp)
	vpermq		\$0x15,$D1,$D1
	vpshufd		\$0xc8,$D0,$D0
	vmovdqa		$T4,0x40(%rsp)
	vpermq		\$0x15,$D2,$D2
	vpshufd		\$0xc8,$D1,$D1
	vmovdqa		$D0,0x60(%rsp)
	vpermq		\$0x15,$D3,$D3
	vpshufd		\$0xc8,$D2,$D2
	vmovdqa		$D1,0x80(%rsp)
	vpermq		\$0x15,$D4,$D4
	vpshufd		\$0xc8,$D3,$D3
	vmovdqa		$D2,0xa0(%rsp)
	vpermq		\$0x15,$MASK,$MASK
	vpshufd		\$0xc8,$D4,$D4
	vmovdqa		$D3,0xc0(%rsp)
	vpshufd		\$0xc8,$MASK,$MASK
	vmovdqa		$D4,0xe0(%rsp)
	vmovdqa		$MASK,0x100(%rsp)
	vmovdqa		64(%rcx),$MASK		# .Lmask26

	################################################################
	# load input
	vmovdqu		16*0($inp),%x#$T0
	vmovdqu		16*1($inp),%x#$T1
	vinserti128	\$1,16*2($inp),$T0,$T0
	vinserti128	\$1,16*3($inp),$T1,$T1
	lea		16*4($inp),$inp

	vpsrldq		\$6,$T0,$T2		# splat input
	vpsrldq		\$6,$T1,$T3
	vpunpckhqdq	$T1,$T0,$T4		# 4
	vpunpcklqdq	$T3,$T2,$T2		# 2:3
	vpunpcklqdq	$T1,$T0,$T0		# 0:1

	vpsrlq		\$30,$T2,$T3
	vpsrlq		\$4,$T2,$T2
	vpsrlq		\$26,$T0,$T1
	vpsrlq		\$40,$T4,$T4		# 4
	vpand		$MASK,$T2,$T2		# 2
	vpand		$MASK,$T0,$T0		# 0
	vpand		$MASK,$T1,$T1		# 1
	vpand		$MASK,$T3,$T3		# 3
	vpor		32(%rcx),$T4,$T4	# padbit, yes, always

	lea		0x90(%rsp),%rax		# size optimization
	vpaddq		$H2,$T2,$H2		# accumulate input
	sub		\$64,$len
	jz		.Ltail_avx2
	jmp		.Loop_avx2

.align	32
.Loop_avx2:
	################################################################
	# ((inp[0]*r^4+r[4])*r^4+r[8])*r^4
	# ((inp[1]*r^4+r[5])*r^4+r[9])*r^3
	# ((inp[2]*r^4+r[6])*r^4+r[10])*r^2
	# ((inp[3]*r^4+r[7])*r^4+r[11])*r^1
	#   \________/\________/
	################################################################
	#vpaddq		$H2,$T2,$H2		# accumulate input
	vpaddq		$H0,$T0,$H0
	vmovdqa		`32*0`(%rsp),$T0	# r0^4
	vpaddq		$H1,$T1,$H1
	vmovdqa		`32*1`(%rsp),$T1	# r1^4
	vpaddq		$H3,$T3,$H3
	vmovdqa		`32*3`(%rsp),$T2	# r2^4
	vpaddq		$H4,$T4,$H4
	vmovdqa		`32*6-0x90`(%rax),$T3	# s3^4
	vmovdqa		`32*8-0x90`(%rax),$S4	# s4^4

	# d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	# d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	# d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	#
	# however, as h2 is "chronologically" first one available pull
	# corresponding operations up, so it's
	#
	# d4 = h2*r2   + h4*r0 + h3*r1             + h1*r3   + h0*r4
	# d3 = h2*r1   + h3*r0           + h1*r2   + h0*r3   + h4*5*r4
	# d2 = h2*r0           + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	# d1 = h2*5*r4 + h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3
	# d0 = h2*5*r3 + h0*r0 + h4*5*r1 + h3*5*r2           + h1*5*r4

	vpmuludq	$H2,$T0,$D2		# d2 = h2*r0
	vpmuludq	$H2,$T1,$D3		# d3 = h2*r1
	vpmuludq	$H2,$T2,$D4		# d4 = h2*r2
	vpmuludq	$H2,$T3,$D0		# d0 = h2*s3
	vpmuludq	$H2,$S4,$D1		# d1 = h2*s4

	vpmuludq	$H0,$T1,$T4		# h0*r1
	vpmuludq	$H1,$T1,$H2		# h1*r1, borrow $H2 as temp
	vpaddq		$T4,$D1,$D1		# d1 += h0*r1
	vpaddq		$H2,$D2,$D2		# d2 += h1*r1
	vpmuludq	$H3,$T1,$T4		# h3*r1
	vpmuludq	`32*2`(%rsp),$H4,$H2	# h4*s1
	vpaddq		$T4,$D4,$D4		# d4 += h3*r1
	vpaddq		$H2,$D0,$D0		# d0 += h4*s1
	 vmovdqa	`32*4-0x90`(%rax),$T1	# s2

	vpmuludq	$H0,$T0,$T4		# h0*r0
	vpmuludq	$H1,$T0,$H2		# h1*r0
	vpaddq		$T4,$D0,$D0		# d0 += h0*r0
	vpaddq		$H2,$D1,$D1		# d1 += h1*r0
	vpmuludq	$H3,$T0,$T4		# h3*r0
	vpmuludq	$H4,$T0,$H2		# h4*r0
	 vmovdqu	16*0($inp),%x#$T0	# load input
	vpaddq		$T4,$D3,$D3		# d3 += h3*r0
	vpaddq		$H2,$D4,$D4		# d4 += h4*r0
	 vinserti128	\$1,16*2($inp),$T0,$T0

	vpmuludq	$H3,$T1,$T4		# h3*s2
	vpmuludq	$H4,$T1,$H2		# h4*s2
	 vmovdqu	16*1($inp),%x#$T1
	vpaddq		$T4,$D0,$D0		# d0 += h3*s2
	vpaddq		$H2,$D1,$D1		# d1 += h4*s2
	 vmovdqa	`32*5-0x90`(%rax),$H2	# r3
	vpmuludq	$H1,$T2,$T4		# h1*r2
	vpmuludq	$H0,$T2,$T2		# h0*r2
	vpaddq		$T4,$D3,$D3		# d3 += h1*r2
	vpaddq		$T2,$D2,$D2		# d2 += h0*r2
	 vinserti128	\$1,16*3($inp),$T1,$T1
	 lea		16*4($inp),$inp

	vpmuludq	$H1,$H2,$T4		# h1*r3
	vpmuludq	$H0,$H2,$H2		# h0*r3
	 vpsrldq	\$6,$T0,$T2		# splat input
	vpaddq		$T4,$D4,$D4		# d4 += h1*r3
	vpaddq		$H2,$D3,$D3		# d3 += h0*r3
	vpmuludq	$H3,$T3,$T4		# h3*s3
	vpmuludq	$H4,$T3,$H2		# h4*s3
	 vpsrldq	\$6,$T1,$T3
	vpaddq		$T4,$D1,$D1		# d1 += h3*s3
	vpaddq		$H2,$D2,$D2		# d2 += h4*s3
	 vpunpckhqdq	$T1,$T0,$T4		# 4

	vpmuludq	$H3,$S4,$H3		# h3*s4
	vpmuludq	$H4,$S4,$H4		# h4*s4
	 vpunpcklqdq	$T1,$T0,$T0		# 0:1
	vpaddq		$H3,$D2,$H2		# h2 = d2 + h3*r4
	vpaddq		$H4,$D3,$H3		# h3 = d3 + h4*r4
	 vpunpcklqdq	$T3,$T2,$T3		# 2:3
	vpmuludq	`32*7-0x90`(%rax),$H0,$H4	# h0*r4
	vpmuludq	$H1,$S4,$H0		# h1*s4
	vmovdqa		64(%rcx),$MASK		# .Lmask26
	vpaddq		$H4,$D4,$H4		# h4 = d4 + h0*r4
	vpaddq		$H0,$D0,$H0		# h0 = d0 + h1*s4

	################################################################
	# lazy reduction (interleaved with tail of input splat)

	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		# h3 -> h4

	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$D1,$H1		# h0 -> h1

	vpsrlq		\$26,$H4,$D4
	vpand		$MASK,$H4,$H4

	 vpsrlq		\$4,$T3,$T2

	vpsrlq		\$26,$H1,$D1
	vpand		$MASK,$H1,$H1
	vpaddq		$D1,$H2,$H2		# h1 -> h2

	vpaddq		$D4,$H0,$H0
	vpsllq		\$2,$D4,$D4
	vpaddq		$D4,$H0,$H0		# h4 -> h0

	 vpand		$MASK,$T2,$T2		# 2
	 vpsrlq		\$26,$T0,$T1

	vpsrlq		\$26,$H2,$D2
	vpand		$MASK,$H2,$H2
	vpaddq		$D2,$H3,$H3		# h2 -> h3

	 vpaddq		$T2,$H2,$H2		# modulo-scheduled
	 vpsrlq		\$30,$T3,$T3

	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		# h0 -> h1

	 vpsrlq		\$40,$T4,$T4		# 4

	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		# h3 -> h4

	 vpand		$MASK,$T0,$T0		# 0
	 vpand		$MASK,$T1,$T1		# 1
	 vpand		$MASK,$T3,$T3		# 3
	 vpor		32(%rcx),$T4,$T4	# padbit, yes, always

	sub		\$64,$len
	jnz		.Loop_avx2

	.byte		0x66,0x90
.Ltail_avx2:
	################################################################
	# while above multiplications were by r^4 in all lanes, in last
	# iteration we multiply least significant lane by r^4 and most
	# significant one by r, so copy of above except that references
	# to the precomputed table are displaced by 4...

	#vpaddq		$H2,$T2,$H2		# accumulate input
	vpaddq		$H0,$T0,$H0
	vmovdqu		`32*0+4`(%rsp),$T0	# r0^4
	vpaddq		$H1,$T1,$H1
	vmovdqu		`32*1+4`(%rsp),$T1	# r1^4
	vpaddq		$H3,$T3,$H3
	vmovdqu		`32*3+4`(%rsp),$T2	# r2^4
	vpaddq		$H4,$T4,$H4
	vmovdqu		`32*6+4-0x90`(%rax),$T3	# s3^4
	vmovdqu		`32*8+4-0x90`(%rax),$S4	# s4^4

	vpmuludq	$H2,$T0,$D2		# d2 = h2*r0
	vpmuludq	$H2,$T1,$D3		# d3 = h2*r1
	vpmuludq	$H2,$T2,$D4		# d4 = h2*r2
	vpmuludq	$H2,$T3,$D0		# d0 = h2*s3
	vpmuludq	$H2,$S4,$D1		# d1 = h2*s4

	vpmuludq	$H0,$T1,$T4		# h0*r1
	vpmuludq	$H1,$T1,$H2		# h1*r1
	vpaddq		$T4,$D1,$D1		# d1 += h0*r1
	vpaddq		$H2,$D2,$D2		# d2 += h1*r1
	vpmuludq	$H3,$T1,$T4		# h3*r1
	vpmuludq	`32*2+4`(%rsp),$H4,$H2	# h4*s1
	vpaddq		$T4,$D4,$D4		# d4 += h3*r1
	vpaddq		$H2,$D0,$D0		# d0 += h4*s1

	vpmuludq	$H0,$T0,$T4		# h0*r0
	vpmuludq	$H1,$T0,$H2		# h1*r0
	vpaddq		$T4,$D0,$D0		# d0 += h0*r0
	 vmovdqu	`32*4+4-0x90`(%rax),$T1	# s2
	vpaddq		$H2,$D1,$D1		# d1 += h1*r0
	vpmuludq	$H3,$T0,$T4		# h3*r0
	vpmuludq	$H4,$T0,$H2		# h4*r0
	vpaddq		$T4,$D3,$D3		# d3 += h3*r0
	vpaddq		$H2,$D4,$D4		# d4 += h4*r0

	vpmuludq	$H3,$T1,$T4		# h3*s2
	vpmuludq	$H4,$T1,$H2		# h4*s2
	vpaddq		$T4,$D0,$D0		# d0 += h3*s2
	vpaddq		$H2,$D1,$D1		# d1 += h4*s2
	 vmovdqu	`32*5+4-0x90`(%rax),$H2	# r3
	vpmuludq	$H1,$T2,$T4		# h1*r2
	vpmuludq	$H0,$T2,$T2		# h0*r2
	vpaddq		$T4,$D3,$D3		# d3 += h1*r2
	vpaddq		$T2,$D2,$D2		# d2 += h0*r2

	vpmuludq	$H1,$H2,$T4		# h1*r3
	vpmuludq	$H0,$H2,$H2		# h0*r3
	vpaddq		$T4,$D4,$D4		# d4 += h1*r3
	vpaddq		$H2,$D3,$D3		# d3 += h0*r3
	vpmuludq	$H3,$T3,$T4		# h3*s3
	vpmuludq	$H4,$T3,$H2		# h4*s3
	vpaddq		$T4,$D1,$D1		# d1 += h3*s3
	vpaddq		$H2,$D2,$D2		# d2 += h4*s3

	vpmuludq	$H3,$S4,$H3		# h3*s4
	vpmuludq	$H4,$S4,$H4		# h4*s4
	vpaddq		$H3,$D2,$H2		# h2 = d2 + h3*r4
	vpaddq		$H4,$D3,$H3		# h3 = d3 + h4*r4
	vpmuludq	`32*7+4-0x90`(%rax),$H0,$H4		# h0*r4
	vpmuludq	$H1,$S4,$H0		# h1*s4
	vmovdqa		64(%rcx),$MASK		# .Lmask26
	vpaddq		$H4,$D4,$H4		# h4 = d4 + h0*r4
	vpaddq		$H0,$D0,$H0		# h0 = d0 + h1*s4

	################################################################
	# horizontal addition

	vpsrldq		\$8,$D1,$T1
	vpsrldq		\$8,$H2,$T2
	vpsrldq		\$8,$H3,$T3
	vpsrldq		\$8,$H4,$T4
	vpsrldq		\$8,$H0,$T0
	vpaddq		$T1,$D1,$D1
	vpaddq		$T2,$H2,$H2
	vpaddq		$T3,$H3,$H3
	vpaddq		$T4,$H4,$H4
	vpaddq		$T0,$H0,$H0

	vpermq		\$0x2,$H3,$T3
	vpermq		\$0x2,$H4,$T4
	vpermq		\$0x2,$H0,$T0
	vpermq		\$0x2,$D1,$T1
	vpermq		\$0x2,$H2,$T2
	vpaddq		$T3,$H3,$H3
	vpaddq		$T4,$H4,$H4
	vpaddq		$T0,$H0,$H0
	vpaddq		$T1,$D1,$D1
	vpaddq		$T2,$H2,$H2

	################################################################
	# lazy reduction

	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		# h3 -> h4

	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$D1,$H1		# h0 -> h1

	vpsrlq		\$26,$H4,$D4
	vpand		$MASK,$H4,$H4

	vpsrlq		\$26,$H1,$D1
	vpand		$MASK,$H1,$H1
	vpaddq		$D1,$H2,$H2		# h1 -> h2

	vpaddq		$D4,$H0,$H0
	vpsllq		\$2,$D4,$D4
	vpaddq		$D4,$H0,$H0		# h4 -> h0

	vpsrlq		\$26,$H2,$D2
	vpand		$MASK,$H2,$H2
	vpaddq		$D2,$H3,$H3		# h2 -> h3

	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		# h0 -> h1

	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		# h3 -> h4

	vmovd		%x#$H0,`4*0-48-64`($ctx)# save partially reduced
	vmovd		%x#$H1,`4*1-48-64`($ctx)
	vmovd		%x#$H2,`4*2-48-64`($ctx)
	vmovd		%x#$H3,`4*3-48-64`($ctx)
	vmovd		%x#$H4,`4*4-48-64`($ctx)
___
$code.=<<___	if ($win64);
	vmovdqa		0x50(%r11),%xmm6
	vmovdqa		0x60(%r11),%xmm7
	vmovdqa		0x70(%r11),%xmm8
	vmovdqa		0x80(%r11),%xmm9
	vmovdqa		0x90(%r11),%xmm10
	vmovdqa		0xa0(%r11),%xmm11
	vmovdqa		0xb0(%r11),%xmm12
	vmovdqa		0xc0(%r11),%xmm13
	vmovdqa		0xd0(%r11),%xmm14
	vmovdqa		0xe0(%r11),%xmm15
	lea		0xf8(%r11),%rsp
.Ldo_avx2_epilogue:
___
$code.=<<___	if (!$win64);
	lea		8(%r11),%rsp
___
$code.=<<___;
	vzeroupper
	ret
.size	poly1305_blocks_avx2,.-poly1305_blocks_avx2
___
}
$code.=<<___;
.align	64
.Lconst:
.Lmask24:
.long	0x0ffffff,0,0x0ffffff,0,0x0ffffff,0,0x0ffffff,0
.L129:
.long	`1<<24`,0,`1<<24`,0,`1<<24`,0,`1<<24`,0
.Lmask26:
.long	0x3ffffff,0,0x3ffffff,0,0x3ffffff,0,0x3ffffff,0
.Lfive:
.long	5,0,5,0,5,0,5,0
___
}

$code.=<<___;
.asciz	"Poly1305 for x86_64, CRYPTOGAMS by <appro\@openssl.org>"
.align	16
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
.type	se_handler,\@abi-omnipotent
.align	16
se_handler:
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
	lea	(%rsi,%r10),%r10	# prologue label
	cmp	%r10,%rbx		# context->Rip<.Lprologue
	jb	.Lcommon_seh_tail

	mov	152($context),%rax	# pull context->Rsp

	mov	4(%r11),%r10d		# HandlerData[1]
	lea	(%rsi,%r10),%r10	# epilogue label
	cmp	%r10,%rbx		# context->Rip>=.Lepilogue
	jae	.Lcommon_seh_tail

	lea	48(%rax),%rax

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
	mov	%r15,240($context)	# restore context->R14

	jmp	.Lcommon_seh_tail
.size	se_handler,.-se_handler

.type	avx_handler,\@abi-omnipotent
.align	16
avx_handler:
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
	lea	(%rsi,%r10),%r10	# prologue label
	cmp	%r10,%rbx		# context->Rip<prologue label
	jb	.Lcommon_seh_tail

	mov	152($context),%rax	# pull context->Rsp

	mov	4(%r11),%r10d		# HandlerData[1]
	lea	(%rsi,%r10),%r10	# epilogue label
	cmp	%r10,%rbx		# context->Rip>=epilogue label
	jae	.Lcommon_seh_tail

	mov	208($context),%rax	# pull context->R11

	lea	0x50(%rax),%rsi
	lea	0xf8(%rax),%rax
	lea	512($context),%rdi	# &context.Xmm6
	mov	\$20,%ecx
	.long	0xa548f3fc		# cld; rep movsq

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
.size	avx_handler,.-avx_handler

.section	.pdata
.align	4
	.rva	.LSEH_begin_poly1305_init
	.rva	.LSEH_end_poly1305_init
	.rva	.LSEH_info_poly1305_init

	.rva	.LSEH_begin_poly1305_blocks
	.rva	.LSEH_end_poly1305_blocks
	.rva	.LSEH_info_poly1305_blocks

	.rva	.LSEH_begin_poly1305_emit
	.rva	.LSEH_end_poly1305_emit
	.rva	.LSEH_info_poly1305_emit
___
$code.=<<___ if ($avx);
	.rva	.LSEH_begin_poly1305_blocks_avx
	.rva	.Lbase2_64_avx
	.rva	.LSEH_info_poly1305_blocks_avx_1

	.rva	.Lbase2_64_avx
	.rva	.Leven_avx
	.rva	.LSEH_info_poly1305_blocks_avx_2

	.rva	.Leven_avx
	.rva	.LSEH_end_poly1305_blocks_avx
	.rva	.LSEH_info_poly1305_blocks_avx_3

	.rva	.LSEH_begin_poly1305_emit_avx
	.rva	.LSEH_end_poly1305_emit_avx
	.rva	.LSEH_info_poly1305_emit_avx
___
$code.=<<___ if ($avx>1);
	.rva	.LSEH_begin_poly1305_blocks_avx2
	.rva	.Lbase2_64_avx2
	.rva	.LSEH_info_poly1305_blocks_avx2_1

	.rva	.Lbase2_64_avx2
	.rva	.Leven_avx2
	.rva	.LSEH_info_poly1305_blocks_avx2_2

	.rva	.Leven_avx2
	.rva	.LSEH_end_poly1305_blocks_avx2
	.rva	.LSEH_info_poly1305_blocks_avx2_3
___
$code.=<<___;
.section	.xdata
.align	8
.LSEH_info_poly1305_init:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.LSEH_begin_poly1305_init,.LSEH_begin_poly1305_init

.LSEH_info_poly1305_blocks:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lblocks_body,.Lblocks_epilogue

.LSEH_info_poly1305_emit:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.LSEH_begin_poly1305_emit,.LSEH_begin_poly1305_emit
___
$code.=<<___ if ($avx);
.LSEH_info_poly1305_blocks_avx_1:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lblocks_avx_body,.Lblocks_avx_epilogue		# HandlerData[]

.LSEH_info_poly1305_blocks_avx_2:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lbase2_64_avx_body,.Lbase2_64_avx_epilogue	# HandlerData[]

.LSEH_info_poly1305_blocks_avx_3:
	.byte	9,0,0,0
	.rva	avx_handler
	.rva	.Ldo_avx_body,.Ldo_avx_epilogue			# HandlerData[]

.LSEH_info_poly1305_emit_avx:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.LSEH_begin_poly1305_emit_avx,.LSEH_begin_poly1305_emit_avx
___
$code.=<<___ if ($avx>1);
.LSEH_info_poly1305_blocks_avx2_1:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lblocks_avx2_body,.Lblocks_avx2_epilogue	# HandlerData[]

.LSEH_info_poly1305_blocks_avx2_2:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lbase2_64_avx2_body,.Lbase2_64_avx2_epilogue	# HandlerData[]

.LSEH_info_poly1305_blocks_avx2_3:
	.byte	9,0,0,0
	.rva	avx_handler
	.rva	.Ldo_avx2_body,.Ldo_avx2_epilogue		# HandlerData[]
___
}

foreach (split('\n',$code)) {
	s/\`([^\`]*)\`/eval($1)/ge;
	s/%r([a-z]+)#d/%e$1/g;
	s/%r([0-9]+)#d/%r$1d/g;
	s/%x#%y/%x/g;

	print $_,"\n";
}
close STDOUT;
