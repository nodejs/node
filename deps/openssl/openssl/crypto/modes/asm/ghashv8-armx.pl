#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================
#
# GHASH for ARMv8 Crypto Extension, 64-bit polynomial multiplication.
#
# June 2014
#
# Initial version was developed in tight cooperation with Ard
# Biesheuvel <ard.biesheuvel@linaro.org> from bits-n-pieces from
# other assembly modules. Just like aesv8-armx.pl this module
# supports both AArch32 and AArch64 execution modes.
#
# Current performance in cycles per processed byte:
#
#		PMULL[2]	32-bit NEON(*)
# Apple A7	1.76		5.62
# Cortex-A53	1.45		8.39
# Cortex-A57	2.22		7.61
#
# (*)	presented for reference/comparison purposes;

$flavour = shift;
open STDOUT,">".shift;

$Xi="x0";	# argument block
$Htbl="x1";
$inp="x2";
$len="x3";

$inc="x12";

{
my ($Xl,$Xm,$Xh,$IN)=map("q$_",(0..3));
my ($t0,$t1,$t2,$t3,$H,$Hhl)=map("q$_",(8..14));

$code=<<___;
#include "arm_arch.h"

.text
___
$code.=".arch	armv8-a+crypto\n"	if ($flavour =~ /64/);
$code.=".fpu	neon\n.code	32\n"	if ($flavour !~ /64/);

$code.=<<___;
.global	gcm_init_v8
.type	gcm_init_v8,%function
.align	4
gcm_init_v8:
	vld1.64		{$t1},[x1]		@ load H
	vmov.i8		$t0,#0xe1
	vext.8		$IN,$t1,$t1,#8
	vshl.i64	$t0,$t0,#57
	vshr.u64	$t2,$t0,#63
	vext.8		$t0,$t2,$t0,#8		@ t0=0xc2....01
	vdup.32		$t1,${t1}[1]
	vshr.u64	$t3,$IN,#63
	vshr.s32	$t1,$t1,#31		@ broadcast carry bit
	vand		$t3,$t3,$t0
	vshl.i64	$IN,$IN,#1
	vext.8		$t3,$t3,$t3,#8
	vand		$t0,$t0,$t1
	vorr		$IN,$IN,$t3		@ H<<<=1
	veor		$IN,$IN,$t0		@ twisted H
	vst1.64		{$IN},[x0]

	ret
.size	gcm_init_v8,.-gcm_init_v8

.global	gcm_gmult_v8
.type	gcm_gmult_v8,%function
.align	4
gcm_gmult_v8:
	vld1.64		{$t1},[$Xi]		@ load Xi
	vmov.i8		$t3,#0xe1
	vld1.64		{$H},[$Htbl]		@ load twisted H
	vshl.u64	$t3,$t3,#57
#ifndef __ARMEB__
	vrev64.8	$t1,$t1
#endif
	vext.8		$Hhl,$H,$H,#8
	mov		$len,#0
	vext.8		$IN,$t1,$t1,#8
	mov		$inc,#0
	veor		$Hhl,$Hhl,$H		@ Karatsuba pre-processing
	mov		$inp,$Xi
	b		.Lgmult_v8
.size	gcm_gmult_v8,.-gcm_gmult_v8

.global	gcm_ghash_v8
.type	gcm_ghash_v8,%function
.align	4
gcm_ghash_v8:
	vld1.64		{$Xl},[$Xi]		@ load [rotated] Xi
	subs		$len,$len,#16
	vmov.i8		$t3,#0xe1
	mov		$inc,#16
	vld1.64		{$H},[$Htbl]		@ load twisted H
	cclr		$inc,eq
	vext.8		$Xl,$Xl,$Xl,#8
	vshl.u64	$t3,$t3,#57
	vld1.64		{$t1},[$inp],$inc	@ load [rotated] inp
	vext.8		$Hhl,$H,$H,#8
#ifndef __ARMEB__
	vrev64.8	$Xl,$Xl
	vrev64.8	$t1,$t1
#endif
	veor		$Hhl,$Hhl,$H		@ Karatsuba pre-processing
	vext.8		$IN,$t1,$t1,#8
	b		.Loop_v8

.align	4
.Loop_v8:
	vext.8		$t2,$Xl,$Xl,#8
	veor		$IN,$IN,$Xl		@ inp^=Xi
	veor		$t1,$t1,$t2		@ $t1 is rotated inp^Xi

.Lgmult_v8:
	vpmull.p64	$Xl,$H,$IN		@ H.lo·Xi.lo
	veor		$t1,$t1,$IN		@ Karatsuba pre-processing
	vpmull2.p64	$Xh,$H,$IN		@ H.hi·Xi.hi
	subs		$len,$len,#16
	vpmull.p64	$Xm,$Hhl,$t1		@ (H.lo+H.hi)·(Xi.lo+Xi.hi)
	cclr		$inc,eq

	vext.8		$t1,$Xl,$Xh,#8		@ Karatsuba post-processing
	veor		$t2,$Xl,$Xh
	veor		$Xm,$Xm,$t1
	 vld1.64	{$t1},[$inp],$inc	@ load [rotated] inp
	veor		$Xm,$Xm,$t2
	vpmull.p64	$t2,$Xl,$t3		@ 1st phase

	vmov		$Xh#lo,$Xm#hi		@ Xh|Xm - 256-bit result
	vmov		$Xm#hi,$Xl#lo		@ Xm is rotated Xl
#ifndef __ARMEB__
	 vrev64.8	$t1,$t1
#endif
	veor		$Xl,$Xm,$t2
	 vext.8		$IN,$t1,$t1,#8

	vext.8		$t2,$Xl,$Xl,#8		@ 2nd phase
	vpmull.p64	$Xl,$Xl,$t3
	veor		$t2,$t2,$Xh
	veor		$Xl,$Xl,$t2
	b.hs		.Loop_v8

#ifndef __ARMEB__
	vrev64.8	$Xl,$Xl
#endif
	vext.8		$Xl,$Xl,$Xl,#8
	vst1.64		{$Xl},[$Xi]		@ write out Xi

	ret
.size	gcm_ghash_v8,.-gcm_ghash_v8
___
}
$code.=<<___;
.asciz  "GHASH for ARMv8, CRYPTOGAMS by <appro\@openssl.org>"
.align  2
___

if ($flavour =~ /64/) {			######## 64-bit code
    sub unvmov {
	my $arg=shift;

	$arg =~ m/q([0-9]+)#(lo|hi),\s*q([0-9]+)#(lo|hi)/o &&
	sprintf	"ins	v%d.d[%d],v%d.d[%d]",$1,($2 eq "lo")?0:1,$3,($4 eq "lo")?0:1;
    }
    foreach(split("\n",$code)) {
	s/cclr\s+([wx])([^,]+),\s*([a-z]+)/csel	$1$2,$1zr,$1$2,$3/o	or
	s/vmov\.i8/movi/o		or	# fix up legacy mnemonics
	s/vmov\s+(.*)/unvmov($1)/geo	or
	s/vext\.8/ext/o			or
	s/vshr\.s/sshr\.s/o		or
	s/vshr/ushr/o			or
	s/^(\s+)v/$1/o			or	# strip off v prefix
	s/\bbx\s+lr\b/ret/o;

	s/\bq([0-9]+)\b/"v".($1<8?$1:$1+8).".16b"/geo;	# old->new registers
	s/@\s/\/\//o;				# old->new style commentary

	# fix up remainig legacy suffixes
	s/\.[ui]?8(\s)/$1/o;
	s/\.[uis]?32//o and s/\.16b/\.4s/go;
	m/\.p64/o and s/\.16b/\.1q/o;		# 1st pmull argument
	m/l\.p64/o and s/\.16b/\.1d/go;		# 2nd and 3rd pmull arguments
	s/\.[uisp]?64//o and s/\.16b/\.2d/go;
	s/\.[42]([sd])\[([0-3])\]/\.$1\[$2\]/o;

	print $_,"\n";
    }
} else {				######## 32-bit code
    sub unvdup32 {
	my $arg=shift;

	$arg =~ m/q([0-9]+),\s*q([0-9]+)\[([0-3])\]/o &&
	sprintf	"vdup.32	q%d,d%d[%d]",$1,2*$2+($3>>1),$3&1;
    }
    sub unvpmullp64 {
	my ($mnemonic,$arg)=@_;

	if ($arg =~ m/q([0-9]+),\s*q([0-9]+),\s*q([0-9]+)/o) {
	    my $word = 0xf2a00e00|(($1&7)<<13)|(($1&8)<<19)
				 |(($2&7)<<17)|(($2&8)<<4)
				 |(($3&7)<<1) |(($3&8)<<2);
	    $word |= 0x00010001	 if ($mnemonic =~ "2");
	    # since ARMv7 instructions are always encoded little-endian.
	    # correct solution is to use .inst directive, but older
	    # assemblers don't implement it:-(
	    sprintf ".byte\t0x%02x,0x%02x,0x%02x,0x%02x\t@ %s %s",
			$word&0xff,($word>>8)&0xff,
			($word>>16)&0xff,($word>>24)&0xff,
			$mnemonic,$arg;
	}
    }

    foreach(split("\n",$code)) {
	s/\b[wx]([0-9]+)\b/r$1/go;		# new->old registers
	s/\bv([0-9])\.[12468]+[bsd]\b/q$1/go;	# new->old registers
        s/\/\/\s?/@ /o;				# new->old style commentary

	# fix up remainig new-style suffixes
	s/\],#[0-9]+/]!/o;

	s/cclr\s+([^,]+),\s*([a-z]+)/mov$2	$1,#0/o			or
	s/vdup\.32\s+(.*)/unvdup32($1)/geo				or
	s/v?(pmull2?)\.p64\s+(.*)/unvpmullp64($1,$2)/geo		or
	s/\bq([0-9]+)#(lo|hi)/sprintf "d%d",2*$1+($2 eq "hi")/geo	or
	s/^(\s+)b\./$1b/o						or
	s/^(\s+)ret/$1bx\tlr/o;

        print $_,"\n";
    }
}

close STDOUT; # enforce flush
