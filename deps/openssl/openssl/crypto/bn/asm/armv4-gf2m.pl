#!/usr/bin/env perl
#
# ====================================================================
# Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
# project. The module is, however, dual licensed under OpenSSL and
# CRYPTOGAMS licenses depending on where you obtain it. For further
# details see http://www.openssl.org/~appro/cryptogams/.
# ====================================================================
#
# May 2011
#
# The module implements bn_GF2m_mul_2x2 polynomial multiplication
# used in bn_gf2m.c. It's kind of low-hanging mechanical port from
# C for the time being... Except that it has two code paths: pure
# integer code suitable for any ARMv4 and later CPU and NEON code
# suitable for ARMv7. Pure integer 1x1 multiplication subroutine runs
# in ~45 cycles on dual-issue core such as Cortex A8, which is ~50%
# faster than compiler-generated code. For ECDH and ECDSA verify (but
# not for ECDSA sign) it means 25%-45% improvement depending on key
# length, more for longer keys. Even though NEON 1x1 multiplication
# runs in even less cycles, ~30, improvement is measurable only on
# longer keys. One has to optimize code elsewhere to get NEON glow...

while (($output=shift) && ($output!~/^\w[\w\-]*\.\w+$/)) {}
open STDOUT,">$output";

sub Dlo()   { shift=~m|q([1]?[0-9])|?"d".($1*2):"";     }
sub Dhi()   { shift=~m|q([1]?[0-9])|?"d".($1*2+1):"";   }
sub Q()     { shift=~m|d([1-3]?[02468])|?"q".($1/2):""; }

$code=<<___;
#include "arm_arch.h"

.text
.code	32

#if __ARM_ARCH__>=7
.fpu	neon

.type	mul_1x1_neon,%function
.align	5
mul_1x1_neon:
	vshl.u64	`&Dlo("q1")`,d16,#8	@ q1-q3 are slided $a
	vmull.p8	`&Q("d0")`,d16,d17	@ a·bb
	vshl.u64	`&Dlo("q2")`,d16,#16
	vmull.p8	q1,`&Dlo("q1")`,d17	@ a<<8·bb
	vshl.u64	`&Dlo("q3")`,d16,#24
	vmull.p8	q2,`&Dlo("q2")`,d17	@ a<<16·bb
	vshr.u64	`&Dlo("q1")`,#8
	vmull.p8	q3,`&Dlo("q3")`,d17	@ a<<24·bb
	vshl.u64	`&Dhi("q1")`,#24
	veor		d0,`&Dlo("q1")`
	vshr.u64	`&Dlo("q2")`,#16
	veor		d0,`&Dhi("q1")`
	vshl.u64	`&Dhi("q2")`,#16
	veor		d0,`&Dlo("q2")`
	vshr.u64	`&Dlo("q3")`,#24
	veor		d0,`&Dhi("q2")`
	vshl.u64	`&Dhi("q3")`,#8
	veor		d0,`&Dlo("q3")`
	veor		d0,`&Dhi("q3")`
	bx	lr
.size	mul_1x1_neon,.-mul_1x1_neon
#endif
___
################
# private interface to mul_1x1_ialu
#
$a="r1";
$b="r0";

($a0,$a1,$a2,$a12,$a4,$a14)=
($hi,$lo,$t0,$t1, $i0,$i1 )=map("r$_",(4..9),12);

$mask="r12";

$code.=<<___;
.type	mul_1x1_ialu,%function
.align	5
mul_1x1_ialu:
	mov	$a0,#0
	bic	$a1,$a,#3<<30		@ a1=a&0x3fffffff
	str	$a0,[sp,#0]		@ tab[0]=0
	add	$a2,$a1,$a1		@ a2=a1<<1
	str	$a1,[sp,#4]		@ tab[1]=a1
	eor	$a12,$a1,$a2		@ a1^a2
	str	$a2,[sp,#8]		@ tab[2]=a2
	mov	$a4,$a1,lsl#2		@ a4=a1<<2
	str	$a12,[sp,#12]		@ tab[3]=a1^a2
	eor	$a14,$a1,$a4		@ a1^a4
	str	$a4,[sp,#16]		@ tab[4]=a4
	eor	$a0,$a2,$a4		@ a2^a4
	str	$a14,[sp,#20]		@ tab[5]=a1^a4
	eor	$a12,$a12,$a4		@ a1^a2^a4
	str	$a0,[sp,#24]		@ tab[6]=a2^a4
	and	$i0,$mask,$b,lsl#2
	str	$a12,[sp,#28]		@ tab[7]=a1^a2^a4

	and	$i1,$mask,$b,lsr#1
	ldr	$lo,[sp,$i0]		@ tab[b       & 0x7]
	and	$i0,$mask,$b,lsr#4
	ldr	$t1,[sp,$i1]		@ tab[b >>  3 & 0x7]
	and	$i1,$mask,$b,lsr#7
	ldr	$t0,[sp,$i0]		@ tab[b >>  6 & 0x7]
	eor	$lo,$lo,$t1,lsl#3	@ stall
	mov	$hi,$t1,lsr#29
	ldr	$t1,[sp,$i1]		@ tab[b >>  9 & 0x7]

	and	$i0,$mask,$b,lsr#10
	eor	$lo,$lo,$t0,lsl#6
	eor	$hi,$hi,$t0,lsr#26
	ldr	$t0,[sp,$i0]		@ tab[b >> 12 & 0x7]

	and	$i1,$mask,$b,lsr#13
	eor	$lo,$lo,$t1,lsl#9
	eor	$hi,$hi,$t1,lsr#23
	ldr	$t1,[sp,$i1]		@ tab[b >> 15 & 0x7]

	and	$i0,$mask,$b,lsr#16
	eor	$lo,$lo,$t0,lsl#12
	eor	$hi,$hi,$t0,lsr#20
	ldr	$t0,[sp,$i0]		@ tab[b >> 18 & 0x7]

	and	$i1,$mask,$b,lsr#19
	eor	$lo,$lo,$t1,lsl#15
	eor	$hi,$hi,$t1,lsr#17
	ldr	$t1,[sp,$i1]		@ tab[b >> 21 & 0x7]

	and	$i0,$mask,$b,lsr#22
	eor	$lo,$lo,$t0,lsl#18
	eor	$hi,$hi,$t0,lsr#14
	ldr	$t0,[sp,$i0]		@ tab[b >> 24 & 0x7]

	and	$i1,$mask,$b,lsr#25
	eor	$lo,$lo,$t1,lsl#21
	eor	$hi,$hi,$t1,lsr#11
	ldr	$t1,[sp,$i1]		@ tab[b >> 27 & 0x7]

	tst	$a,#1<<30
	and	$i0,$mask,$b,lsr#28
	eor	$lo,$lo,$t0,lsl#24
	eor	$hi,$hi,$t0,lsr#8
	ldr	$t0,[sp,$i0]		@ tab[b >> 30      ]

	eorne	$lo,$lo,$b,lsl#30
	eorne	$hi,$hi,$b,lsr#2
	tst	$a,#1<<31
	eor	$lo,$lo,$t1,lsl#27
	eor	$hi,$hi,$t1,lsr#5
	eorne	$lo,$lo,$b,lsl#31
	eorne	$hi,$hi,$b,lsr#1
	eor	$lo,$lo,$t0,lsl#30
	eor	$hi,$hi,$t0,lsr#2

	mov	pc,lr
.size	mul_1x1_ialu,.-mul_1x1_ialu
___
################
# void	bn_GF2m_mul_2x2(BN_ULONG *r,
#	BN_ULONG a1,BN_ULONG a0,
#	BN_ULONG b1,BN_ULONG b0);	# r[3..0]=a1a0·b1b0

($A1,$B1,$A0,$B0,$A1B1,$A0B0)=map("d$_",(18..23));

$code.=<<___;
.global	bn_GF2m_mul_2x2
.type	bn_GF2m_mul_2x2,%function
.align	5
bn_GF2m_mul_2x2:
#if __ARM_ARCH__>=7
	ldr	r12,.LOPENSSL_armcap
.Lpic:	ldr	r12,[pc,r12]
	tst	r12,#1
	beq	.Lialu

	veor	$A1,$A1
	vmov.32	$B1,r3,r3		@ two copies of b1
	vmov.32	${A1}[0],r1		@ a1

	veor	$A0,$A0
	vld1.32	${B0}[],[sp,:32]	@ two copies of b0
	vmov.32	${A0}[0],r2		@ a0
	mov	r12,lr

	vmov	d16,$A1
	vmov	d17,$B1
	bl	mul_1x1_neon		@ a1·b1
	vmov	$A1B1,d0

	vmov	d16,$A0
	vmov	d17,$B0
	bl	mul_1x1_neon		@ a0·b0
	vmov	$A0B0,d0

	veor	d16,$A0,$A1
	veor	d17,$B0,$B1
	veor	$A0,$A0B0,$A1B1
	bl	mul_1x1_neon		@ (a0+a1)·(b0+b1)

	veor	d0,$A0			@ (a0+a1)·(b0+b1)-a0·b0-a1·b1
	vshl.u64 d1,d0,#32
	vshr.u64 d0,d0,#32
	veor	$A0B0,d1
	veor	$A1B1,d0
	vst1.32	{${A0B0}[0]},[r0,:32]!
	vst1.32	{${A0B0}[1]},[r0,:32]!
	vst1.32	{${A1B1}[0]},[r0,:32]!
	vst1.32	{${A1B1}[1]},[r0,:32]
	bx	r12
.align	4
.Lialu:
#endif
___
$ret="r10";	# reassigned 1st argument
$code.=<<___;
	stmdb	sp!,{r4-r10,lr}
	mov	$ret,r0			@ reassign 1st argument
	mov	$b,r3			@ $b=b1
	ldr	r3,[sp,#32]		@ load b0
	mov	$mask,#7<<2
	sub	sp,sp,#32		@ allocate tab[8]

	bl	mul_1x1_ialu		@ a1·b1
	str	$lo,[$ret,#8]
	str	$hi,[$ret,#12]

	eor	$b,$b,r3		@ flip b0 and b1
	 eor	$a,$a,r2		@ flip a0 and a1
	eor	r3,r3,$b
	 eor	r2,r2,$a
	eor	$b,$b,r3
	 eor	$a,$a,r2
	bl	mul_1x1_ialu		@ a0·b0
	str	$lo,[$ret]
	str	$hi,[$ret,#4]

	eor	$a,$a,r2
	eor	$b,$b,r3
	bl	mul_1x1_ialu		@ (a1+a0)·(b1+b0)
___
@r=map("r$_",(6..9));
$code.=<<___;
	ldmia	$ret,{@r[0]-@r[3]}
	eor	$lo,$lo,$hi
	eor	$hi,$hi,@r[1]
	eor	$lo,$lo,@r[0]
	eor	$hi,$hi,@r[2]
	eor	$lo,$lo,@r[3]
	eor	$hi,$hi,@r[3]
	str	$hi,[$ret,#8]
	eor	$lo,$lo,$hi
	add	sp,sp,#32		@ destroy tab[8]
	str	$lo,[$ret,#4]

#if __ARM_ARCH__>=5
	ldmia	sp!,{r4-r10,pc}
#else
	ldmia	sp!,{r4-r10,lr}
	tst	lr,#1
	moveq	pc,lr			@ be binary compatible with V4, yet
	bx	lr			@ interoperable with Thumb ISA:-)
#endif
.size	bn_GF2m_mul_2x2,.-bn_GF2m_mul_2x2
#if __ARM_ARCH__>=7
.align	5
.LOPENSSL_armcap:
.word	OPENSSL_armcap_P-(.Lpic+8)
#endif
.asciz	"GF(2^m) Multiplication for ARMv4/NEON, CRYPTOGAMS by <appro\@openssl.org>"
.align	5

.comm	OPENSSL_armcap_P,4,4
___

$code =~ s/\`([^\`]*)\`/eval $1/gem;
$code =~ s/\bbx\s+lr\b/.word\t0xe12fff1e/gm;    # make it possible to compile with -march=armv4
print $code;
close STDOUT;   # enforce flush
