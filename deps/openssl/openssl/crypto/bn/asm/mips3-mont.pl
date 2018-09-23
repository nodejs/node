#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@fy.chalmers.se> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================

# This module doesn't present direct interest for OpenSSL, because it
# doesn't provide better performance for longer keys. While 512-bit
# RSA private key operations are 40% faster, 1024-bit ones are hardly
# faster at all, while longer key operations are slower by up to 20%.
# It might be of interest to embedded system developers though, as
# it's smaller than 1KB, yet offers ~3x improvement over compiler
# generated code.
#
# The module targets N32 and N64 MIPS ABIs and currently is a bit
# IRIX-centric, i.e. is likely to require adaptation for other OSes.

# int bn_mul_mont(
$rp="a0";	# BN_ULONG *rp,
$ap="a1";	# const BN_ULONG *ap,
$bp="a2";	# const BN_ULONG *bp,
$np="a3";	# const BN_ULONG *np,
$n0="a4";	# const BN_ULONG *n0,
$num="a5";	# int num);

$lo0="a6";
$hi0="a7";
$lo1="v0";
$hi1="v1";
$aj="t0";
$bi="t1";
$nj="t2";
$tp="t3";
$alo="s0";
$ahi="s1";
$nlo="s2";
$nhi="s3";
$tj="s4";
$i="s5";
$j="s6";
$fp="t8";
$m1="t9";

$FRAME=8*(2+8);

$code=<<___;
#include <asm.h>
#include <regdef.h>

.text

.set	noat
.set	reorder

.align	5
.globl	bn_mul_mont
.ent	bn_mul_mont
bn_mul_mont:
	.set	noreorder
	PTR_SUB	sp,64
	move	$fp,sp
	.frame	$fp,64,ra
	slt	AT,$num,4
	li	v0,0
	beqzl	AT,.Lproceed
	nop
	jr	ra
	PTR_ADD	sp,$fp,64
	.set	reorder
.align	5
.Lproceed:
	ld	$n0,0($n0)
	ld	$bi,0($bp)	# bp[0]
	ld	$aj,0($ap)	# ap[0]
	ld	$nj,0($np)	# np[0]
	PTR_SUB	sp,16		# place for two extra words
	sll	$num,3
	li	AT,-4096
	PTR_SUB	sp,$num
	and	sp,AT

	sd	s0,0($fp)
	sd	s1,8($fp)
	sd	s2,16($fp)
	sd	s3,24($fp)
	sd	s4,32($fp)
	sd	s5,40($fp)
	sd	s6,48($fp)
	sd	s7,56($fp)

	dmultu	$aj,$bi
	ld	$alo,8($ap)
	ld	$nlo,8($np)
	mflo	$lo0
	mfhi	$hi0
	dmultu	$lo0,$n0
	mflo	$m1

	dmultu	$alo,$bi
	mflo	$alo
	mfhi	$ahi

	dmultu	$nj,$m1
	mflo	$lo1
	mfhi	$hi1
	dmultu	$nlo,$m1
	daddu	$lo1,$lo0
	sltu	AT,$lo1,$lo0
	daddu	$hi1,AT
	mflo	$nlo
	mfhi	$nhi

	move	$tp,sp
	li	$j,16
.align	4
.L1st:
	.set	noreorder
	PTR_ADD	$aj,$ap,$j
	ld	$aj,($aj)
	PTR_ADD	$nj,$np,$j
	ld	$nj,($nj)

	dmultu	$aj,$bi
	daddu	$lo0,$alo,$hi0
	daddu	$lo1,$nlo,$hi1
	sltu	AT,$lo0,$hi0
	sltu	s7,$lo1,$hi1
	daddu	$hi0,$ahi,AT
	daddu	$hi1,$nhi,s7
	mflo	$alo
	mfhi	$ahi

	daddu	$lo1,$lo0
	sltu	AT,$lo1,$lo0
	dmultu	$nj,$m1
	daddu	$hi1,AT
	addu	$j,8
	sd	$lo1,($tp)
	sltu	s7,$j,$num
	mflo	$nlo
	mfhi	$nhi

	bnez	s7,.L1st
	PTR_ADD	$tp,8
	.set	reorder

	daddu	$lo0,$alo,$hi0
	sltu	AT,$lo0,$hi0
	daddu	$hi0,$ahi,AT

	daddu	$lo1,$nlo,$hi1
	sltu	s7,$lo1,$hi1
	daddu	$hi1,$nhi,s7
	daddu	$lo1,$lo0
	sltu	AT,$lo1,$lo0
	daddu	$hi1,AT

	sd	$lo1,($tp)

	daddu	$hi1,$hi0
	sltu	AT,$hi1,$hi0
	sd	$hi1,8($tp)
	sd	AT,16($tp)

	li	$i,8
.align	4
.Louter:
	PTR_ADD	$bi,$bp,$i
	ld	$bi,($bi)
	ld	$aj,($ap)
	ld	$alo,8($ap)
	ld	$tj,(sp)

	dmultu	$aj,$bi
	ld	$nj,($np)
	ld	$nlo,8($np)
	mflo	$lo0
	mfhi	$hi0
	daddu	$lo0,$tj
	dmultu	$lo0,$n0
	sltu	AT,$lo0,$tj
	daddu	$hi0,AT
	mflo	$m1

	dmultu	$alo,$bi
	mflo	$alo
	mfhi	$ahi

	dmultu	$nj,$m1
	mflo	$lo1
	mfhi	$hi1

	dmultu	$nlo,$m1
	daddu	$lo1,$lo0
	sltu	AT,$lo1,$lo0
	daddu	$hi1,AT
	mflo	$nlo
	mfhi	$nhi

	move	$tp,sp
	li	$j,16
	ld	$tj,8($tp)
.align	4
.Linner:
	.set	noreorder
	PTR_ADD	$aj,$ap,$j
	ld	$aj,($aj)
	PTR_ADD	$nj,$np,$j
	ld	$nj,($nj)

	dmultu	$aj,$bi
	daddu	$lo0,$alo,$hi0
	daddu	$lo1,$nlo,$hi1
	sltu	AT,$lo0,$hi0
	sltu	s7,$lo1,$hi1
	daddu	$hi0,$ahi,AT
	daddu	$hi1,$nhi,s7
	mflo	$alo
	mfhi	$ahi

	daddu	$lo0,$tj
	addu	$j,8
	dmultu	$nj,$m1
	sltu	AT,$lo0,$tj
	daddu	$lo1,$lo0
	daddu	$hi0,AT
	sltu	s7,$lo1,$lo0
	ld	$tj,16($tp)
	daddu	$hi1,s7
	sltu	AT,$j,$num
	mflo	$nlo
	mfhi	$nhi
	sd	$lo1,($tp)
	bnez	AT,.Linner
	PTR_ADD	$tp,8
	.set	reorder

	daddu	$lo0,$alo,$hi0
	sltu	AT,$lo0,$hi0
	daddu	$hi0,$ahi,AT
	daddu	$lo0,$tj
	sltu	s7,$lo0,$tj
	daddu	$hi0,s7

	ld	$tj,16($tp)
	daddu	$lo1,$nlo,$hi1
	sltu	AT,$lo1,$hi1
	daddu	$hi1,$nhi,AT
	daddu	$lo1,$lo0
	sltu	s7,$lo1,$lo0
	daddu	$hi1,s7
	sd	$lo1,($tp)

	daddu	$lo1,$hi1,$hi0
	sltu	$hi1,$lo1,$hi0
	daddu	$lo1,$tj
	sltu	AT,$lo1,$tj
	daddu	$hi1,AT
	sd	$lo1,8($tp)
	sd	$hi1,16($tp)

	addu	$i,8
	sltu	s7,$i,$num
	bnez	s7,.Louter

	.set	noreorder
	PTR_ADD	$tj,sp,$num	# &tp[num]
	move	$tp,sp
	move	$ap,sp
	li	$hi0,0		# clear borrow bit

.align	4
.Lsub:	ld	$lo0,($tp)
	ld	$lo1,($np)
	PTR_ADD	$tp,8
	PTR_ADD	$np,8
	dsubu	$lo1,$lo0,$lo1	# tp[i]-np[i]
	sgtu	AT,$lo1,$lo0
	dsubu	$lo0,$lo1,$hi0
	sgtu	$hi0,$lo0,$lo1
	sd	$lo0,($rp)
	or	$hi0,AT
	sltu	AT,$tp,$tj
	bnez	AT,.Lsub
	PTR_ADD	$rp,8

	dsubu	$hi0,$hi1,$hi0	# handle upmost overflow bit
	move	$tp,sp
	PTR_SUB	$rp,$num	# restore rp
	not	$hi1,$hi0

	and	$ap,$hi0,sp
	and	$bp,$hi1,$rp
	or	$ap,$ap,$bp	# ap=borrow?tp:rp

.align	4
.Lcopy:	ld	$aj,($ap)
	PTR_ADD	$ap,8
	PTR_ADD	$tp,8
	sd	zero,-8($tp)
	sltu	AT,$tp,$tj
	sd	$aj,($rp)
	bnez	AT,.Lcopy
	PTR_ADD	$rp,8

	ld	s0,0($fp)
	ld	s1,8($fp)
	ld	s2,16($fp)
	ld	s3,24($fp)
	ld	s4,32($fp)
	ld	s5,40($fp)
	ld	s6,48($fp)
	ld	s7,56($fp)
	li	v0,1
	jr	ra
	PTR_ADD	sp,$fp,64
	.set	reorder
END(bn_mul_mont)
.rdata
.asciiz	"Montgomery Multiplication for MIPS III/IV, CRYPTOGAMS by <appro\@openssl.org>"
___

print $code;
close STDOUT;
