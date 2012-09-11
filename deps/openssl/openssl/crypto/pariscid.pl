#!/usr/bin/env perl

$flavour = shift;
$output = shift;
open STDOUT,">$output";

if ($flavour =~ /64/) {
	$LEVEL		="2.0W";
	$SIZE_T		=8;
	$ST		="std";
} else {
	$LEVEL		="1.1";
	$SIZE_T		=4;
	$ST		="stw";
}

$rp="%r2";
$sp="%r30";
$rv="%r28";

$code=<<___;
	.LEVEL	$LEVEL
	.SPACE	\$TEXT\$
	.SUBSPA	\$CODE\$,QUAD=0,ALIGN=8,ACCESS=0x2C,CODE_ONLY

	.EXPORT	OPENSSL_cpuid_setup,ENTRY
	.ALIGN	8
OPENSSL_cpuid_setup
	.PROC
	.CALLINFO	NO_CALLS
	.ENTRY
	bv	($rp)
	.EXIT
	nop
	.PROCEND

	.EXPORT	OPENSSL_rdtsc,ENTRY
	.ALIGN	8
OPENSSL_rdtsc
	.PROC
	.CALLINFO	NO_CALLS
	.ENTRY
	mfctl	%cr16,$rv
	bv	($rp)
	.EXIT
	nop
	.PROCEND

	.EXPORT	OPENSSL_wipe_cpu,ENTRY
	.ALIGN	8
OPENSSL_wipe_cpu
	.PROC
	.CALLINFO	NO_CALLS
	.ENTRY
	xor		%r0,%r0,%r1
	fcpy,dbl	%fr0,%fr4
	xor		%r0,%r0,%r19
	fcpy,dbl	%fr0,%fr5
	xor		%r0,%r0,%r20
	fcpy,dbl	%fr0,%fr6
	xor		%r0,%r0,%r21
	fcpy,dbl	%fr0,%fr7
	xor		%r0,%r0,%r22
	fcpy,dbl	%fr0,%fr8
	xor		%r0,%r0,%r23
	fcpy,dbl	%fr0,%fr9
	xor		%r0,%r0,%r24
	fcpy,dbl	%fr0,%fr10
	xor		%r0,%r0,%r25
	fcpy,dbl	%fr0,%fr11
	xor		%r0,%r0,%r26
	fcpy,dbl	%fr0,%fr22
	xor		%r0,%r0,%r29
	fcpy,dbl	%fr0,%fr23
	xor		%r0,%r0,%r31
	fcpy,dbl	%fr0,%fr24
	fcpy,dbl	%fr0,%fr25
	fcpy,dbl	%fr0,%fr26
	fcpy,dbl	%fr0,%fr27
	fcpy,dbl	%fr0,%fr28
	fcpy,dbl	%fr0,%fr29
	fcpy,dbl	%fr0,%fr30
	fcpy,dbl	%fr0,%fr31
	bv		($rp)
	.EXIT
	ldo		0($sp),$rv
	.PROCEND
___
{
my $inp="%r26";
my $len="%r25";

$code.=<<___;
	.EXPORT	OPENSSL_cleanse,ENTRY,ARGW0=GR,ARGW1=GR
	.ALIGN	8
OPENSSL_cleanse
	.PROC
	.CALLINFO	NO_CALLS
	.ENTRY
	cmpib,*=	0,$len,Ldone
	nop
	cmpib,*>>=	15,$len,Little
	ldi		$SIZE_T-1,%r1

Lalign
	and,*<>		$inp,%r1,%r28
	b,n		Laligned
	stb		%r0,0($inp)
	ldo		-1($len),$len
	b		Lalign
	ldo		1($inp),$inp

Laligned
	andcm		$len,%r1,%r28
Lot
	$ST		%r0,0($inp)
	addib,*<>	-$SIZE_T,%r28,Lot
	ldo		$SIZE_T($inp),$inp

	and,*<>		$len,%r1,$len
	b,n		Ldone
Little
	stb		%r0,0($inp)
	addib,*<>	-1,$len,Little
	ldo		1($inp),$inp
Ldone
	bv		($rp)
	.EXIT
	nop
	.PROCEND
___
}
{
my ($out,$cnt,$max)=("%r26","%r25","%r24");
my ($tick,$lasttick)=("%r23","%r22");
my ($diff,$lastdiff)=("%r21","%r20");

$code.=<<___;
	.EXPORT	OPENSSL_instrument_bus,ENTRY,ARGW0=GR,ARGW1=GR
	.ALIGN	8
OPENSSL_instrument_bus
	.PROC
	.CALLINFO	NO_CALLS
	.ENTRY
	copy		$cnt,$rv
	mfctl		%cr16,$tick
	copy		$tick,$lasttick
	ldi		0,$diff

	fdc		0($out)
	ldw		0($out),$tick
	add		$diff,$tick,$tick
	stw		$tick,0($out)
Loop
	mfctl		%cr16,$tick
	sub		$tick,$lasttick,$diff
	copy		$tick,$lasttick

	fdc		0($out)
	ldw		0($out),$tick
	add		$diff,$tick,$tick
	stw		$tick,0($out)

	addib,<>	-1,$cnt,Loop
	addi		4,$out,$out

	bv		($rp)
	.EXIT
	sub		$rv,$cnt,$rv
	.PROCEND

	.EXPORT	OPENSSL_instrument_bus2,ENTRY,ARGW0=GR,ARGW1=GR
	.ALIGN	8
OPENSSL_instrument_bus2
	.PROC
	.CALLINFO	NO_CALLS
	.ENTRY
	copy		$cnt,$rv
	sub		%r0,$cnt,$cnt

	mfctl		%cr16,$tick
	copy		$tick,$lasttick
	ldi		0,$diff

	fdc		0($out)
	ldw		0($out),$tick
	add		$diff,$tick,$tick
	stw		$tick,0($out)

	mfctl		%cr16,$tick
	sub		$tick,$lasttick,$diff
	copy		$tick,$lasttick
Loop2
	copy		$diff,$lastdiff
	fdc		0($out)
	ldw		0($out),$tick
	add		$diff,$tick,$tick
	stw		$tick,0($out)

	addib,=		-1,$max,Ldone2
	nop

	mfctl		%cr16,$tick
	sub		$tick,$lasttick,$diff
	copy		$tick,$lasttick
	cmpclr,<>	$lastdiff,$diff,$tick
	ldi		1,$tick

	ldi		1,%r1
	xor		%r1,$tick,$tick
	addb,<>		$tick,$cnt,Loop2
	shladd,l	$tick,2,$out,$out
Ldone2
	bv		($rp)
	.EXIT
	add		$rv,$cnt,$rv
	.PROCEND
___
}
$code =~ s/cmpib,\*/comib,/gm if ($SIZE_T==4);
$code =~ s/,\*/,/gm if ($SIZE_T==4);
print $code;
close STDOUT;

