#!/usr/bin/env perl

$flavour = shift;

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}ppc-xlate.pl" and -f $xlate ) or
( $xlate="${dir}perlasm/ppc-xlate.pl" and -f $xlate) or
die "can't locate ppc-xlate.pl";

open STDOUT,"| $^X $xlate $flavour ".shift || die "can't call $xlate: $!";

if ($flavour=~/64/) {
    $CMPLI="cmpldi";
    $SHRLI="srdi";
    $SIGNX="extsw";
} else {
    $CMPLI="cmplwi";
    $SHRLI="srwi";
    $SIGNX="mr";
}

$code=<<___;
.machine	"any"
.text

.globl	.OPENSSL_cpuid_setup
.align	4
.OPENSSL_cpuid_setup:
	blr

.globl	.OPENSSL_wipe_cpu
.align	4
.OPENSSL_wipe_cpu:
	xor	r0,r0,r0
	mr	r3,r1
	xor	r4,r4,r4
	xor	r5,r5,r5
	xor	r6,r6,r6
	xor	r7,r7,r7
	xor	r8,r8,r8
	xor	r9,r9,r9
	xor	r10,r10,r10
	xor	r11,r11,r11
	xor	r12,r12,r12
	blr

.globl	.OPENSSL_atomic_add
.align	4
.OPENSSL_atomic_add:
Loop:	lwarx	r5,0,r3
	add	r0,r4,r5
	stwcx.	r0,0,r3
	bne-	Loop
	$SIGNX	r3,r0
	blr

.globl	.OPENSSL_rdtsc
.align	4
.OPENSSL_rdtsc:
	mftb	r3
	mftbu	r4
	blr

.globl	.OPENSSL_cleanse
.align	4
.OPENSSL_cleanse:
	$CMPLI	r4,7
	li	r0,0
	bge	Lot
	$CMPLI	r4,0
	beqlr-
Little:	mtctr	r4
	stb	r0,0(r3)
	addi	r3,r3,1
	bdnz-	\$-8
	blr
Lot:	andi.	r5,r3,3
	beq	Laligned
	stb	r0,0(r3)
	subi	r4,r4,1
	addi	r3,r3,1
	b	Lot
Laligned:
	$SHRLI	r5,r4,2
	mtctr	r5
	stw	r0,0(r3)
	addi	r3,r3,4
	bdnz-	\$-8
	andi.	r4,r4,3
	bne	Little
	blr
___

$code =~ s/\`([^\`]*)\`/eval $1/gem;
print $code;
close STDOUT;
