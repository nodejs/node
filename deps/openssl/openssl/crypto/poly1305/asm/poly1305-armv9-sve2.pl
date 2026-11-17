#! /usr/bin/env perl
# Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

##############################################################################
#
# Copyright (c) 2025, Iakov Polyak <iakov.polyak@linaro.org>
# This file is an SVE2 port-and-merge of POLY1305 hash algorithm, derived from
# the OpenSSL Neon implementation and a vector length agnostic (VLA)
# RISC-V implementation from the CRYPTOGAMS project.
#
##############################################################################
#
# Copyright (c) 2006, CRYPTOGAMS by <appro@openssl.org>
# All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions
#are met:
#
#      *	Redistributions of source code must retain copyright notices,
#	this list of conditions and the following disclaimer.
#
#      *	Redistributions in binary form must reproduce the above
#	copyright notice, this list of conditions and the following
#	disclaimer in the documentation and/or other materials
#	provided with the distribution.
#
#      *	Neither the name of the CRYPTOGAMS nor the names of its
#	copyright holder and contributors may be used to endorse or
#	promote products derived from this software without specific
#	prior written permission.
#
#ALTERNATIVELY, provided that this notice is retained in full, this
#product may be distributed under the terms of the GNU General Public
#License (GPL), in which case the provisions of the GPL apply INSTEAD OF
#those given above.
#
#THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
#"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##############################################################################
#
# September 2025
#
# This is a 100% vector length agnostic implementation and has
# been tested with QEMU for the vector length of up to 2048 bits.
#
# On Graviton4, with the vector register length of 128 bits,
# it is less efficient than the Neon implementation by only 6%.
# This number has been obtained by running
# `openssl speed -evp ChaCha20-POLY1305` and
# `openssl speed -evp ChaCha20`, pinned to a single CPU,
# converting the 8192-byte result to cycles per byte
# using actual average runtime CPU frequency from `perf stat`,
# and taking the difference. On Graviton 4, this results in 
# 0.62 cpb for Neon and 0.66 for SVE2.
# 
# While Neon should probably be the default choice on a 128-bit architecture,
# speed-up is clearly expected with 256-bit and larger vector registers
# in the future.

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}arm-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/arm-xlate.pl" and -f $xlate) or
die "can't locate arm-xlate.pl";

open OUT,"| \"$^X\" $xlate $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

my ($ctx,$inp,$len,$padbit) = map("x$_",(0..3));

my ($h0,$h1,$h2,$r0,$r1,$r2,$t0,$t1,$d0,$d1,$d2) = map("x$_",(4..14));

my ($SVE_R0,$SVE_R1,$SVE_S1,$SVE_R2,$SVE_S2,$SVE_R3,$SVE_S3,$SVE_R4,$SVE_S4) = map("z$_.s",(0..8));
my ($SVE_INlo_0,$SVE_INlo_1,$SVE_INlo_2,$SVE_INlo_3,$SVE_INlo_4) = map("z$_.s",(9..13));
my ($SVE_INhi_0,$SVE_INhi_1,$SVE_INhi_2,$SVE_INhi_3,$SVE_INhi_4) = map("z$_.s",(14..18));
my ($SVE_ACC0,$SVE_ACC1,$SVE_ACC2,$SVE_ACC3,$SVE_ACC4) = map("z$_.d",(19..23));
my ($SVE_H0,$SVE_H1,$SVE_H2,$SVE_H3,$SVE_H4) = map("z$_.s",(24..28));
my ($SVE_T0,$SVE_T1,$SVE_MASK) = map("z$_",(29..31));

my ($vl,$vl0,$vl1,$vl2,$vl3,$vl4) = ("x16",$h0,$h1,$h2,$r0,$r1);
my ($cs0,$cs1,$cs2,$cs3,$cs4,$cs5) = map("x$_",(19..24));
my ($pwr,$mask) = map("x$_",(25..26));
my $is_base2_26 = "w17";

$code.=<<___;
#include "arch/arm_arch.h"

.text

.arch armv8-a

.extern poly1305_blocks

// --- poly1305_sw_2_26 ---
// Performs conversion of 3 base2_44 to 5 base2_26 scalars and
//  stores them in memory at addresses [x5], [x5,#28], [x5,#56],
//  [x5,#84] and [x5,#112].
//
// This is a leaf function and does not modify stack.
//
// Calling Convention:
//   Inputs:
//     x5: Pointer into memory where 1st value should be stored.
//     x7-x9: The three base2_44 scalar values (r0-r2)
//   Clobbers (uses as temporaries):
//     x10-x15
.type	poly1305_sw_2_26,%function
.align	5
poly1305_sw_2_26:
	// Converts 3 base2_44 -> 5 base2_26 values and stores
	mov		x15,#0x3ffffff			// w15  : 2^26-1 mask
	and		x10,$r0,x15				// w10 -> r0
	lsr		x11,$r0,#26				// w11 : top 18 bits of r0
	str		w10,[x5]				// Store r0
	bfi		x11,$r1,#18,#8			// w11 -> r1
	ubfx	x12,$r1,#8,#26			// w12 -> r2
	str		w11,[x5,#28]			// Store r1
	lsr		x13,$r1,#34				// w13 : top 10 bits of r1
	str		w12,[x5,#56]			// Store r2
	bfi		x13,$r2,#10,#16			// w13 -> r3
	lsr		x14,$r2,#16				// w14 -> r4
	str		w13,[x5,#84]			// Store r3
	str		w14,[x5,#112]			// Store r4
	ret
.size   poly1305_sw_2_26,.-poly1305_sw_2_26

// --- poly1305_sqr_2_44 ---
// Calculates base2_44 squaring operation.
//
// This is a leaf function and does not modify stack.
// It however uses callee-saved registers as scratch, so those must be
//  saved on stack prior to calling.
//
// Calling Convention:
//   Inputs:
//     x7-x9: The three base2_44 scalar values (r0-r2)
//   Outputs:
//     x7-x9: The three base2_44 scalar values, squared (r0-r2)
//   Clobbers (uses as temporaries):
//     x10-x15, x19-x24, x26
.type	poly1305_sqr_2_44,%function
.align	5
poly1305_sqr_2_44:

    // Pre-calculate constants and doubled terms.
	mov		x12,#20
	lsl		x13,$r1,#1		// x13 = r1 * 2
	mul		x12,$r2,x12		// x12 = r2 * 20
	lsl		x10,$r0,#1		// x10 = r0 * 2

    // --- Calculate d2 = r1*r1 + 2*r0*r2 ---
	umulh	$cs5,$r1,$r1	// high part of r1*r1
	mul		$cs4,$r1,$r1	// low part of r1*r1
	umulh	x15,x10,$r2		// high part of (r0*2)*r2
	mul		x14,x10,$r2		// low part of (r0*2)*r2

    // --- Calculate d0 = r0*r0 + 20*(2*r1*r2) ---
	umulh	$cs1,$r0,$r0	// high part of r0*r0
	mul		$cs0,$r0,$r0	// low part of r0*r0
	umulh	x11,x13,x12		// high part of (r1*2)*(r2*20)
	mul		x10,x13,x12		// low part of (r1*2)*(r2*20)

	adds	$cs4,$cs4,x14	// d2_lo
	adc		$cs5,$cs5,x15	// d2_hi

    // --- Calculate d1 = 2*r0*r1 + 20*r2*r2 ---
    // d1 is a 128-bit result stored in x7:x6 (hi:lo)
	umulh	$cs3,$r0,x13	// high part of r0*(r1*2)
	mul		$cs2,$r0,x13	// low part of r0*(r1*2)
	umulh	x13,$r2,x12		// high part of r2*(r2*20)
	mul		x12,$r2,x12		// low part of r2*(r2*20)

	adds	$cs0,$cs0,x10	// d0_lo
	adc		$cs1,$cs1,x11	// d0_hi

	adds	$cs2,$cs2,x12	// d1_lo
	adc		$cs3,$cs3,x13	// d1_hi

    // --- Reduction and Carry Propagation ---
    // Reduce the 128-bit d0, d1, d2 back to three 44-bit limbs in x0, x1, x2
	lsr		x10,$cs0,#44	// (d0_lo >> 44)
	lsl		x11,$cs1,#20	// (d0_hi << 20) - high 20 bits are zero
	and		$r0,$cs0,$mask	// r0 -> d0_lo & mask
	orr		x10,x10,x11		// x10 -> 64-bit carry from d0
    
	lsr		x12,$cs2,#44	// (d1_lo >> 44)
	lsl		x13,$cs3,#20	// (d1_hi << 20)
	and		$r1,$cs2,$mask	// r1 -> d1_lo & mask
	orr		x12,x12,x13		// x12 -> 64-bit carry from d1
	add		$r1,$r1,x10		// r1 += carry from d0

	lsr		x11,$mask,#2	// x11 -> 2^42-1 mask for d2 reduction
	lsr		x10,$cs4,#42	// (d2_lo >> 42)
	lsl		x13,$cs5,#22	// (d2_hi << 22)
	and		$r2,$cs4,x11	// r2 -> d2_lo & 2^42-1 mask
	orr		x10,x10,x13		// x10 -> final carry from d2
	add		$r2,$r2,x12		// r2 += carry from d1

    // Handle ripple-carry from r2 and apply the *5 reduction.
	lsr		x13,$r2,#42		// Get carry from r2 (if r2 >= 2^42)
	and		$r2,$r2,x11		// Mask r2 back down to 42 bits
	add		x10,x10,x13		// Add this ripple-carry to the final carry

	add		x11,x10,x10,lsl #2	// x11 -> final_carry * 5
	add		$r0,$r0,x11			// r0 += final_carry * 5

    // Final ripple-carry chain to ensure all limbs are 44 bits.
	lsr		x11,$r1,#44		// Get carry from r1
	and		$r1,$r1,$mask	// Mask r1 to 44 bits
	add		$r2,$r2,x11		// r2 += carry from r1
    
	lsr		x10,$r0,#44		// Get carry from r0
	and		$r0,$r0,$mask	// Mask r0 to 44 bits
	add		$r1,$r1,x10		// r1 += carry from r0

    ret
.size	poly1305_sqr_2_44,.-poly1305_sqr_2_44

// --- poly1305_lazy_reduce_sve2 ---
// Performs lazy reduction on five accumulator vectors as discussed
// in "NEON crypto" by D.J. Bernstein and P. Schwabe.
//
// This is a leaf function and does not modify GPRs or the stack.
//
// Calling Convention:
//   Inputs:
//     z19-z23: The five 64-bit .d accumulator vectors (ACC0-ACC4)
//   Outputs:
//     z24-z28: The five 32-bit .s final limb vectors (H0-H4)
//     z31: All-zeros (resets mask)
//   Clobbers (uses as temporaries):
//     z29, z30

.type	poly1305_lazy_reduce_sve2,%function
.align	5
poly1305_lazy_reduce_sve2:
	dup 	${SVE_MASK}.d,#-1
	lsr 	${SVE_T0}.d,$SVE_ACC3,#26
	trn1	$SVE_H3,z22.s,z24.s					// reproducing Neon's `xtn` - treat ACC3 as a .s vector
	lsr 	${SVE_MASK}.d,${SVE_MASK}.d,#38
	lsr 	${SVE_T1}.d,$SVE_ACC0,#26
	and 	$SVE_ACC0,$SVE_ACC0,${SVE_MASK}.d
	add 	$SVE_ACC4,$SVE_ACC4,${SVE_T0}.d	    // h3 -> h4
	// Neon's bic is replaced with &=$SVE_MASK (because of using even-indexed elements)
	and 	z27.d,z27.d,${SVE_MASK}.d			// refer to SVE_H3 as .d
	add 	$SVE_ACC1,$SVE_ACC1,${SVE_T1}.d	    // h0 -> h1

	lsr 	${SVE_T0}.d,$SVE_ACC4,#26
	trn1	$SVE_H4,z23.s,z24.s					// reproducing Neon's `xtn` - treat ACC4 as a .s vector
	lsr 	${SVE_T1}.d,$SVE_ACC1,#26
	trn1	$SVE_H1,z20.s,z24.s					// reproducing Neon's `xtn` - treat ACC1 as a .s vector
	and 	z28.d,z28.d,${SVE_MASK}.d			// refer to SVE_H4 as .d
	add 	$SVE_ACC2,$SVE_ACC2,${SVE_T1}.d	    // h1 -> h2

	add 	$SVE_ACC0,$SVE_ACC0,${SVE_T0}.d
	lsl 	${SVE_T0}.d,${SVE_T0}.d,#2
	shrnb	${SVE_T1}.s,$SVE_ACC2,#26			// check it's OK
	trn1	$SVE_H2,z21.s,z24.s					// reproducing Neon's `xtn` - treat ACC2 as a .s vector
	add 	$SVE_ACC0,$SVE_ACC0,${SVE_T0}.d		// h4 -> h0
	and 	z25.d,z25.d,${SVE_MASK}.d			// refer to SVE_H1 as .d
	add 	$SVE_H3,$SVE_H3,${SVE_T1}.s			// h2 -> h3
	and 	z26.d,z26.d,${SVE_MASK}.d			// refer to SVE_H2 as .d

	shrnb	${SVE_T0}.s,$SVE_ACC0,#26
	trn1	$SVE_H0,z19.s,z24.s					// reproducing Neon's `xtn` - treat ACC0 as a .s vector - re-writing H0 here...
	lsr 	${SVE_T1}.s,$SVE_H3,#26
	and 	z27.d,z27.d,${SVE_MASK}.d			// refer to SVE_H3 as .d
	add 	$SVE_H1,$SVE_H1,${SVE_T0}.s			// h0 -> h1
	and 	z24.d,z24.d,${SVE_MASK}.d			// refer to SVE_H0 as .d
	add 	$SVE_H4,$SVE_H4,${SVE_T1}.s			// h3 -> h4

	eor 	${SVE_MASK}.d,${SVE_MASK}.d,${SVE_MASK}.d	// reset zero mask

    ret
.size	poly1305_lazy_reduce_sve2,.-poly1305_lazy_reduce_sve2

// --- poly1305_blocks_sve2 ---
// Main function, implementing POLY1305 algorithm as discussed
// in "NEON crypto" by D.J. Bernstein and P. Schwabe, in a VLA fashion,
// using SVE2.
//
// It is mostly a port-and-merge of the 128-bit Neon implementation herein and
//  a VLA risc-v implementation in https://github.com/dot-asm/cryptogams.
//
.globl	poly1305_blocks_sve2
.hidden	poly1305_blocks_sve2
.type	poly1305_blocks_sve2,%function
.align	5
poly1305_blocks_sve2:
.Lpoly1305_blocks_sve2:
	AARCH64_VALID_CALL_TARGET
	ldr	$is_base2_26,[$ctx,#24]
	// Estimate vector width and branch to scalar if input too short
	cntd	$vl					// vector width in 64-bit lanes (vl)
	lsl	$vl0,$vl,#4				// vl * 16 (bytes per vector input blocks) 
	add $vl1,$vl0,$vl0,lsl #1	// 3 * vl * 16 - new threshold.
	cmp	$len,$vl1
	b.hs	.Lblocks_sve2
	cbz	$is_base2_26,.Lshort_blocks	// Call scalar f-n if short; if in base 2^26 - proceed

.Lblocks_sve2:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-144]!		// Allowing for callee-saved reg-s
	add	x29,sp,#0

	//Store some callee-saved GPRs
	stp	x19,x20,[sp,#16]
 	stp	x21,x22,[sp,#32]
 	stp	x23,x24,[sp,#48]
	stp	x25,x26,[sp,#64]

	ands	$len,$len,#-16
	b.eq	.Lno_data_sve2

	cbz	$is_base2_26,.Lbase2_64_sve2

	ldp	w10,w11,[$ctx]			// load hash value base 2^26
	ldp	w12,w13,[$ctx,#8]
	ldr	w14,[$ctx,#16]

	neg	$vl1,$vl0				// - (vl * 16)
	sub	$vl0,$vl0,#1			// (vl * 16) - 1
	and	$vl2,$len,$vl1			// $len - ($len % (vl * 16)) -> VLA length
	and	$vl4,$len,$vl0			// $len % (vl * 16) -> scalar remainder
	cbz	$vl4,.Leven_sve2		// If no scalar "head", proceed to VLA
	add	$vl3,$inp,$vl4			// Pointer to the start of the VLA data
	stp	$vl2,$vl3,[sp,#-16]!	// Backup VLA length and ptr
	mov	$len,$vl4				// So that scalar part knows it's length

	add	$h0,x10,x11,lsl#26		// base 2^26 -> base 2^64
	lsr	$h1,x12,#12
	adds	$h0,$h0,x12,lsl#52
	add	$h1,$h1,x13,lsl#14
	adc	$h1,$h1,xzr
	lsr	$h2,x14,#24
	adds	$h1,$h1,x14,lsl#40
	adc	$d2,$h2,xzr				// can be partially reduced...

	and	$t0,$d2,#-4				// ... so reduce
	and	$h2,$d2,#3
	add	$t0,$t0,$d2,lsr#2
	adds	$h0,$h0,$t0
	adcs	$h1,$h1,xzr
	adc	$h2,$h2,xzr

	stp	$h0,$h1,[$ctx]			// store hash value base 2^64
	str	$h2,[$ctx,#16]

	bl	poly1305_blocks			// Calculate the scalar "head"
	ldp	$len,$inp,[sp],#16		// Recover updated length and input ptr
	ldr	x30,[sp,#8]

	cbz	$padbit,.Lzero_padbit_sve2	// hash already stored in poly1305_blocks

	ldp	$h0,$h1,[$ctx]			// load hash value base 2^64
	ldr $h2,[$ctx,#16]

	and	x10,$h0,#0x03ffffff		// base 2^64 -> base 2^26
	ubfx	x11,$h0,#26,#26
	extr	x12,$h1,$h0,#52
	and	x12,x12,#0x03ffffff
	ubfx	x13,$h1,#14,#26
	extr	x14,$h2,$h1,#40

	cbnz	$len,.Leven_sve2

	stp	w10,w11,[$ctx]			// store hash value base 2^26
	stp	w12,w13,[$ctx,#8]
	str	w14,[$ctx,#16]
	b	.Lno_data_sve2

.align	4
.Lzero_padbit_sve2:
	str	xzr,[$ctx,#24]
	b	.Lno_data_sve2

.align	4
.Lbase2_64_sve2:
	neg	$vl1,$vl0				// - (vl * 16)
	sub	$vl0,$vl0,#1			// (vl * 16) - 1
	and	$vl2,$len,$vl1			// $len - ($len % (vl * 16)) -> VLA length
	and	$vl4,$len,$vl0			// $len % (vl * 16) -> scalar remainder
	cbz	$vl4,.Linit_sve2		// If no scalar "head", proceed to VLA
	add	$vl3,$inp,$vl4			// Pointer to the start of the VLA data
	stp	$vl2,$vl3,[sp,#-16]!	// Backup VLA length and ptr
	mov	$len,$vl4				// So that scalar part knows it's length
	bl	poly1305_blocks			// Calculate the scalar "head"
	ldp	$len,$inp,[sp],#16		// Recover updated length and input ptr

.Linit_sve2:
	// Calculating and storing r-powers (powers of a key).
	// The layout of how r-powers are stored in memory:
	//////////////////////////////////////////////////////////////////////////////////////
	//                   lobe 1                           lobe 2                   etc. //
	//      | .. r^{max},r^{max/2},...,r^2,r | .. r^{max},r^{max/2},...,r^2,r | ..      //
	//     / \                              / \                              / \        //
	//  [$ctx,48]                       [$ctx,48+28]                     [$ctx,48+56]   //
	//////////////////////////////////////////////////////////////////////////////////////

	ldr w5,[$ctx,#28]		// Load top power (if exists - 0 by default)
	add $pwr,$ctx,#48+28	// Point to the end of powers allocation (1st lobe)

	mov $mask,#-1
	lsr $mask,$mask,#20		//2^44-1

	cbnz	w5,.Lpwrs_precomputed

	ldp	$r0,$r1,[$ctx,#32]	// load key value

	lsr	$r2,$r1,#24			// base2_64 -> base2_44
	extr	$r1,$r1,$r0,#44
	and	$r0,$r0,$mask
	and	$r1,$r1,$mask

	mov	x4,$vl
	add	x5,$pwr,#-4
	bl	poly1305_sw_2_26

.Loop_pwrs_sqr:
	lsr	x4,x4,#1
	add	x5,x5,#-4
	bl	poly1305_sqr_2_44
	bl	poly1305_sw_2_26
	cbnz	 x4,.Loop_pwrs_sqr

	sub	x5,x5,$pwr
	str	w5,[$ctx,#28]

.Lpwrs_precomputed:
	ldp	$h0,$h1,[$ctx]		// load hash value base 2^64
	ldr $h2,[$ctx,#16]

	and	x10,$h0,#0x03ffffff	// base 2^64 -> base 2^26
	ubfx	x11,$h0,#26,#26
	extr	x12,$h1,$h0,#52
	and	x12,x12,#0x03ffffff
	ubfx	x13,$h1,#14,#26
	extr	x14,$h2,$h1,#40

	stp	d8,d9,[sp,#80]		// meet ABI requirements
	stp	d10,d11,[sp,#96]
	stp	d12,d13,[sp,#112]
	stp	d14,d15,[sp,#128]

    // Zeroing H0-H4 registers
	eor 	z24.d,z24.d,z24.d  // H0
	eor 	z25.d,z25.d,z25.d  // H1
	eor 	z26.d,z26.d,z26.d  // H2
	eor 	z27.d,z27.d,z27.d  // H3
	eor 	z28.d,z28.d,z28.d  // H4

	// Using Neon's fmov here for speed.
	//  We only need the low 26 bits in the first step so no need for post-mov reshuffle.
	fmov	d24,x10		// H0
	fmov	d25,x11		// H1
	fmov	d26,x12		// H2
	fmov	d27,x13		// H3
	fmov	d28,x14		// H4

	ldr	x30,[sp,#8]

	mov	x4,#1
	stur	w4,[$ctx,#24]		// set is_base2_26
	b	.Ldo_sve2

.align	4
.Leven_sve2:
	// In principle all this could be moved to Ldo_sve2
	stp	d8,d9,[sp,#80]		// meet ABI requirements
	stp	d10,d11,[sp,#96]
	stp	d12,d13,[sp,#112]
	stp	d14,d15,[sp,#128]

	eor 	z24.d,z24.d,z24.d  // H0
	eor 	z25.d,z25.d,z25.d  // H1
	eor 	z26.d,z26.d,z26.d  // H2
	eor 	z27.d,z27.d,z27.d  // H3
	eor 	z28.d,z28.d,z28.d  // H4

	fmov	d24,x10		// H0
	fmov	d25,x11		// H1
	fmov	d26,x12		// H2
	fmov	d27,x13		// H3
	fmov	d28,x14		// H4

.Ldo_sve2:
    ptrue   p0.b, ALL               		// Set all-true predicate

	// Load r-powers.
	// They are stored in five lobes, in the order r^{max},...,r^2,r^1 each.
	// We need specific powers to be at specific R- and S-vector indices.
	// Hence we can't load all of them, an arbitrary amount, dependent on VL.
	// Instead we load {r^{max},r^{max/2}} and {r^2,r^1} in batches,
	//  and then interleave them using zip1 as {r^{max},r^2,r^{max/2},r}.
	// We don't really care where r^{max} and r^{max/2} are, but we want
	//  r^2 and r to be in either even or odd lanes. We chose lanes 1 and 3.
	// Intermediate r-powers (r^{max/4},..,r^4), if applicable, will be
	//  reloaded into lane 0 iteratively in Loop_reduce_sve2.

	ldr 	w5,[$ctx,#28]
	sxtw	x5,w5				// Zero-extend
	add 	$pwr,$ctx,#48+28	// Pointer to the end of the r-powers 1st lobe
	add		x10,$ctx,#48+20		// Pointer to r^2.
	add		$pwr,$pwr,x5		// Pointer to the r^{max}

	mov		x15,#2
	whilelo	p1.s,xzr,x15

	// If wouldn't need to load in two chunks, could use ld1rqw - 
	//  optimisation potential for 256-bit vector.
	ld1w	{ $SVE_R0 },p1/z,[$pwr]
	ld1w	{ $SVE_T0.s },p1/z,[x10]
	add		$pwr,$pwr,#28
	add		x10,x10,#28
	zip1	$SVE_R0,$SVE_R0,$SVE_T0.s

	ld1w	{ $SVE_R1 },p1/z,[$pwr]
	ld1w	{ $SVE_T1.s },p1/z,[x10]
	add		$pwr,$pwr,#28
	add		x10,x10,#28
	zip1	$SVE_R1,$SVE_R1,$SVE_T1.s

	ld1w	{ $SVE_R2 },p1/z,[$pwr]
	ld1w	{ $SVE_T0.s },p1/z,[x10]
	add		$pwr,$pwr,#28
	add		x10,x10,#28
	zip1	$SVE_R2,$SVE_R2,$SVE_T0.s

	ld1w	{ $SVE_R3 },p1/z,[$pwr]
	ld1w	{ $SVE_T1.s },p1/z,[x10]
	add		$pwr,$pwr,#28
	add		x10,x10,#28
	zip1	$SVE_R3,$SVE_R3,$SVE_T1.s

	ld1w	{ $SVE_R4 },p1/z,[$pwr]
	ld1w	{ $SVE_T0.s },p1/z,[x10]
	sub		$pwr,$pwr,#104				// Adjust to 1st lobe, 3d power
	zip1	$SVE_R4,$SVE_R4,$SVE_T0.s

	// Broadcast r-powers loaded above to higher parts of the R-vectors.
	cmp		$vl,#2
	b.eq	.L_skip_dup_broadcast
	dup		z0.q,z0.q[0]
	dup		z1.q,z1.q[0]
	dup		z3.q,z3.q[0]
	dup		z5.q,z5.q[0]
	dup		z7.q,z7.q[0]

.L_skip_dup_broadcast:
	// Calculate S-vectors (r^x*5)
	adr     $SVE_S1,[$SVE_R1,$SVE_R1,lsl #2]
	adr     $SVE_S2,[$SVE_R2,$SVE_R2,lsl #2]
	adr     $SVE_S3,[$SVE_R3,$SVE_R3,lsl #2]
	adr     $SVE_S4,[$SVE_R4,$SVE_R4,lsl #2]

	// Load initial input blocks
	lsr		x15,$len,#4
	whilelo	p1.s,xzr,x15					// Set predicate for blocks loading
	lsl	$padbit,$padbit,#24
	ld4w	{ z9.s-z12.s },p1/z,[$inp]		// Loading all blocks at once

#ifdef  __AARCH64EB__
	revb	z9.s,  p0/m, z9.s
	revb	z10.s, p0/m, z10.s
	revb	z11.s, p0/m, z11.s
	revb	z12.s, p0/m, z12.s
#endif

	// In-vector (VLA) conversion base2_64 -> base2_26.
	dup 	${SVE_MASK}.s,#-1
	lsr 	${SVE_MASK}.s,${SVE_MASK}.s,#6

	lsr		${SVE_T0}.s,z11.s,#14		// T0 -> z11 >> 14
	lsr		z13.s,z12.s,#8				// z13 -> l4
	lsl		z11.s,z11.s,#12				// z11 -> upper part of l2
	lsl		z12.s,z12.s,#18				// z12 -> upper part of l3
	lsr		${SVE_T1}.s,z10.s,#20		// T1 -> z10 >> 20
	orr		z12.d,z12.d,${SVE_T0}.d		// z12 -> final l3
	lsl		z10.s,z10.s,#6				// z10 -> upper part of l1
	lsr		${SVE_T0}.s,z9.s,#26		// T0 -> z9 >> 26
	and		z9.d,z9.d,${SVE_MASK}.d		// z9 is now final l0
	orr		z11.d,z11.d,${SVE_T1}.d		// z11 -> final l2
	orr		z10.d,z10.d,${SVE_T0}.d		// z10 -> final l1
	dup		${SVE_T1}.s,w3				// x3 -> $padbit but need it as a word
	eor 	${SVE_T0}.d,${SVE_T0}.d,${SVE_T0}.d	// set zero mask
	orr		z13.d,z13.d,${SVE_T1}.d		// l4 += padbit
	and		z12.d,z12.d,${SVE_MASK}.d	// Mask l3
	and		z11.d,z11.d,${SVE_MASK}.d	// Mask l2
	and		z10.d,z10.d,${SVE_MASK}.d	// Mask l1


	// Move high blocks from INlo -> INhi and sparcify (put in even lanes)
	zip2	z14.s,z9.s,${SVE_T0}.s
	zip2	z18.s,z13.s,${SVE_T0}.s
	zip2	z17.s,z12.s,${SVE_T0}.s
	zip2	z16.s,z11.s,${SVE_T0}.s
	zip2	z15.s,z10.s,${SVE_T0}.s

	// Sparcify blocks to even lanes in INlo
	zip1	z9.s,z9.s,${SVE_T0}.s
	zip1	z13.s,z13.s,${SVE_T0}.s
	zip1	z12.s,z12.s,${SVE_T0}.s
	zip1	z11.s,z11.s,${SVE_T0}.s
	zip1	z10.s,z10.s,${SVE_T0}.s

	subs	$len,$len,$vl,lsl #5		// By half vector width * 32

	b.ls	.Lskip_loop_sve2

.align	4
.Loop_sve2:
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ((inp[0]*r^{vl*2} + inp[vl]  *r^{vl} + inp[2*vl]  )*r^{vl} + inp[3*vl]  )*r^{vl}
	//+((inp[1]*r^{vl*2} + inp[vl+1]*r^{vl} + inp[2*vl+1])*r^{vl} + inp[3*vl+1])*r^{vl-1}
	//+...
	//   \_______________________________/    \_________________________________________/ 
	//      first main loop iteration                       long tail
	//
	// ((inp[0]*r^{vl*2} + inp[vl]  *r^{vl} + inp[2*vl]  )*r^{vl*2} + inp[3*vl]  *r^{vl} + inp[4*vl]  )*r^{vl}
	//+((inp[1]*r^{vl*2} + inp[vl+1]*r^{vl} + inp[2*vl+1])*r^{vl*2} + inp[3*vl+1]*r^{vl} + inp[4*vl+1])*r^{vl-1}
	//+...
	//   \_______________________________/    \________________________________________/   \___________________/
	//      first main loop iteration             second main loop iteration                    short tail
	//
	// Note that we start with inp[vl:vl*2]*r^{vl}, as it
	// doesn't depend on reduction in previous iteration.
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Hash-key power product f-la for the 5 limbs in base2^26 representation:
	// d4 = h0*r4 + h1*r3   + h2*r2   + h3*r1   + h4*r0
	// d3 = h0*r3 + h1*r2   + h2*r1   + h3*r0   + h4*5*r4
	// d2 = h0*r2 + h1*r1   + h2*r0   + h3*5*r4 + h4*5*r3
	// d1 = h0*r1 + h1*r0   + h2*5*r4 + h3*5*r3 + h4*5*r2
	// d0 = h0*r0 + h1*5*r4 + h2*5*r3 + h3*5*r2 + h4*5*r1

	add		$inp,$inp,$vl,lsl #5

	umullb	$SVE_ACC4,$SVE_INhi_0,${SVE_R4}[2]
	umullb	$SVE_ACC3,$SVE_INhi_0,${SVE_R3}[2]
	umullb	$SVE_ACC2,$SVE_INhi_0,${SVE_R2}[2]
	umullb	$SVE_ACC1,$SVE_INhi_0,${SVE_R1}[2]
	umullb	$SVE_ACC0,$SVE_INhi_0,${SVE_R0}[2]

	umlalb	$SVE_ACC4,$SVE_INhi_1,${SVE_R3}[2]
	umlalb	$SVE_ACC3,$SVE_INhi_1,${SVE_R2}[2]
	umlalb	$SVE_ACC2,$SVE_INhi_1,${SVE_R1}[2]
	umlalb	$SVE_ACC1,$SVE_INhi_1,${SVE_R0}[2]
	umlalb	$SVE_ACC0,$SVE_INhi_1,${SVE_S4}[2]

	umlalb	$SVE_ACC4,$SVE_INhi_2,${SVE_R2}[2]
	umlalb	$SVE_ACC3,$SVE_INhi_2,${SVE_R1}[2]
	umlalb	$SVE_ACC2,$SVE_INhi_2,${SVE_R0}[2]
	umlalb	$SVE_ACC1,$SVE_INhi_2,${SVE_S4}[2]
	umlalb	$SVE_ACC0,$SVE_INhi_2,${SVE_S3}[2]

	umlalb	$SVE_ACC4,$SVE_INhi_3,${SVE_R1}[2]
	umlalb	$SVE_ACC3,$SVE_INhi_3,${SVE_R0}[2]
	umlalb	$SVE_ACC2,$SVE_INhi_3,${SVE_S4}[2]
	umlalb	$SVE_ACC1,$SVE_INhi_3,${SVE_S3}[2]
	umlalb	$SVE_ACC0,$SVE_INhi_3,${SVE_S2}[2]

	add 	$SVE_INlo_2,$SVE_INlo_2,$SVE_H2
	umlalb	$SVE_ACC4,$SVE_INhi_4,${SVE_R0}[2]
	umlalb	$SVE_ACC3,$SVE_INhi_4,${SVE_S4}[2]
	umlalb	$SVE_ACC2,$SVE_INhi_4,${SVE_S3}[2]
	umlalb	$SVE_ACC1,$SVE_INhi_4,${SVE_S2}[2]
	umlalb	$SVE_ACC0,$SVE_INhi_4,${SVE_S1}[2]

	//////////////////////////////////////////////////////////////////////
	// (hash+inp[0:vl])*r^{vl*2} and accumulate
	// Interleave add+mul with loading and converting the next input batch

	add 	$SVE_INlo_0,$SVE_INlo_0,$SVE_H0
	 lsr	x15,$len,#4
	umlalb	$SVE_ACC3,$SVE_INlo_2,${SVE_R1}[0]
	 whilelo	p1.s,xzr,x15
	umlalb	$SVE_ACC0,$SVE_INlo_2,${SVE_S3}[0]
	 ld4w	{ z14.s-z17.s }, p1/z, [$inp]
	umlalb	$SVE_ACC4,$SVE_INlo_2,${SVE_R2}[0]
	umlalb	$SVE_ACC1,$SVE_INlo_2,${SVE_S4}[0]
	umlalb	$SVE_ACC2,$SVE_INlo_2,${SVE_R0}[0]

#ifdef  __AARCH64EB__
	revb	z14.s, p0/m, z14.s
	revb	z15.s, p0/m, z15.s
	revb	z16.s, p0/m, z16.s
	revb	z17.s, p0/m, z17.s
#endif

	add 	$SVE_INlo_1,$SVE_INlo_1,$SVE_H1
	 dup 	${SVE_MASK}.s,#-1
	umlalb	$SVE_ACC3,$SVE_INlo_0,${SVE_R3}[0]
	 lsr 	${SVE_MASK}.s,${SVE_MASK}.s,#6
	umlalb	$SVE_ACC4,$SVE_INlo_0,${SVE_R4}[0]
	 lsr	${SVE_T0}.s,z16.s,#14		// T0 -> z16 >> 14
	umlalb	$SVE_ACC2,$SVE_INlo_0,${SVE_R2}[0]
	 lsr	z18.s,z17.s,#8				// z18 -> l4
	umlalb	$SVE_ACC0,$SVE_INlo_0,${SVE_R0}[0]
	 lsl	z16.s,z16.s,#12				// z16 -> upper part of l2
	umlalb	$SVE_ACC1,$SVE_INlo_0,${SVE_R1}[0]
	 lsl	z17.s,z17.s,#18				// z17 -> upper part of l3

	add 	$SVE_INlo_3,$SVE_INlo_3,$SVE_H3
	 lsr	${SVE_T1}.s,z15.s,#20		// T1 -> z15 >> 20
	umlalb	$SVE_ACC3,$SVE_INlo_1,${SVE_R2}[0]
	 orr	z17.d,z17.d,${SVE_T0}.d		// z17 -> final l3
	umlalb	$SVE_ACC4,$SVE_INlo_1,${SVE_R3}[0]
	 lsl	z15.s,z15.s,#6				// z15 -> upper part of l1
	umlalb	$SVE_ACC0,$SVE_INlo_1,${SVE_S4}[0]
	 lsr	${SVE_T0}.s,z14.s,#26		// T0 -> z14 >> 26
	umlalb	$SVE_ACC2,$SVE_INlo_1,${SVE_R1}[0]
	 and	z14.d,z14.d,${SVE_MASK}.d	// z14 is now final l0
	umlalb	$SVE_ACC1,$SVE_INlo_1,${SVE_R0}[0]
	 orr	z16.d,z16.d,${SVE_T1}.d		// z16 -> final l2

	add 	$SVE_INlo_4,$SVE_INlo_4,$SVE_H4
	 orr	z15.d,z15.d,${SVE_T0}.d		// z15 -> final l1
	umlalb	$SVE_ACC3,$SVE_INlo_3,${SVE_R0}[0]
	 dup	${SVE_T1}.s,w3
	umlalb	$SVE_ACC0,$SVE_INlo_3,${SVE_S2}[0]
	 eor 	${SVE_T0}.d,${SVE_T0}.d,${SVE_T0}.d	// set zero mask
	umlalb	$SVE_ACC4,$SVE_INlo_3,${SVE_R1}[0]
	 orr	z18.d,z18.d,${SVE_T1}.d		// l4 += padbit
	umlalb	$SVE_ACC1,$SVE_INlo_3,${SVE_S3}[0]
	 and	z17.d,z17.d,${SVE_MASK}.d	// Mask l3
	umlalb	$SVE_ACC2,$SVE_INlo_3,${SVE_S4}[0]
	 and	z16.d,z16.d,${SVE_MASK}.d	// Mask l2

	umlalb	$SVE_ACC3,$SVE_INlo_4,${SVE_S4}[0]
	 and	z15.d,z15.d,${SVE_MASK}.d	// Mask l1
	umlalb	$SVE_ACC0,$SVE_INlo_4,${SVE_S1}[0]
	 zip1	z9.s,z14.s,${SVE_T0}.s
	umlalb	$SVE_ACC4,$SVE_INlo_4,${SVE_R0}[0]
	 zip1	z10.s,z15.s,${SVE_T0}.s
	umlalb	$SVE_ACC1,$SVE_INlo_4,${SVE_S2}[0]
	 zip1	z11.s,z16.s,${SVE_T0}.s
	umlalb	$SVE_ACC2,$SVE_INlo_4,${SVE_S3}[0]
	 zip1	z12.s,z17.s,${SVE_T0}.s
	 zip1	z13.s,z18.s,${SVE_T0}.s

	// Sparcify blocks to even lanes in INlo
	zip2	z14.s,z14.s,${SVE_T0}.s
	zip2	z15.s,z15.s,${SVE_T0}.s
	zip2	z16.s,z16.s,${SVE_T0}.s
	zip2	z17.s,z17.s,${SVE_T0}.s
	zip2	z18.s,z18.s,${SVE_T0}.s

	subs	$len,$len,$vl,lsl #5

	// Lazy reduction
	bl		poly1305_lazy_reduce_sve2
	ldr	x30,[sp,#8]

	b.hi	.Loop_sve2

.Lskip_loop_sve2:

	adds	$len,$len,$vl,lsl #4		// By half the usual input size
	b.eq	.Lshort_tail_sve2

.Long_tail_sve2:
	////////////////////////////////////////////////////////////////
	// (hash + inp[lo])*r^{vl} + inp[hi])*r^{vl..1}               //
	//  \____________________/                                    //
	//  first part of long tail                                   //
	////////////////////////////////////////////////////////////////
	//NB `vl` here (and in the code) is the vector length in double words.
	// Intereaving algebra with copying INhi -> INlo for the next steps.

	add 	$SVE_INlo_2,$SVE_INlo_2,$SVE_H2
	add 	$SVE_INlo_0,$SVE_INlo_0,$SVE_H0
	add 	$SVE_INlo_1,$SVE_INlo_1,$SVE_H1
	add 	$SVE_INlo_3,$SVE_INlo_3,$SVE_H3
	add 	$SVE_INlo_4,$SVE_INlo_4,$SVE_H4

	umullb	$SVE_ACC3,$SVE_INlo_2,${SVE_R1}[2]
	umullb	$SVE_ACC0,$SVE_INlo_2,${SVE_S3}[2]
	umullb	$SVE_ACC4,$SVE_INlo_2,${SVE_R2}[2]
	umullb	$SVE_ACC1,$SVE_INlo_2,${SVE_S4}[2]
	umullb	$SVE_ACC2,$SVE_INlo_2,${SVE_R0}[2]

	umlalb	$SVE_ACC3,$SVE_INlo_0,${SVE_R3}[2]
	umlalb	$SVE_ACC4,$SVE_INlo_0,${SVE_R4}[2]
	umlalb	$SVE_ACC2,$SVE_INlo_0,${SVE_R2}[2]
	umlalb	$SVE_ACC0,$SVE_INlo_0,${SVE_R0}[2]
	umlalb	$SVE_ACC1,$SVE_INlo_0,${SVE_R1}[2]
	mov		z11.d,z16.d

	umlalb	$SVE_ACC3,$SVE_INlo_1,${SVE_R2}[2]
	umlalb	$SVE_ACC4,$SVE_INlo_1,${SVE_R3}[2]
	umlalb	$SVE_ACC0,$SVE_INlo_1,${SVE_S4}[2]
	umlalb	$SVE_ACC2,$SVE_INlo_1,${SVE_R1}[2]
	umlalb	$SVE_ACC1,$SVE_INlo_1,${SVE_R0}[2]
	mov		z9.d,z14.d	

	umlalb	$SVE_ACC3,$SVE_INlo_3,${SVE_R0}[2]
	umlalb	$SVE_ACC0,$SVE_INlo_3,${SVE_S2}[2]
	umlalb	$SVE_ACC4,$SVE_INlo_3,${SVE_R1}[2]
	umlalb	$SVE_ACC1,$SVE_INlo_3,${SVE_S3}[2]
	umlalb	$SVE_ACC2,$SVE_INlo_3,${SVE_S4}[2]
	mov		z10.d,z15.d

	umlalb	$SVE_ACC3,$SVE_INlo_4,${SVE_S4}[2]
	umlalb	$SVE_ACC0,$SVE_INlo_4,${SVE_S1}[2]
	umlalb	$SVE_ACC4,$SVE_INlo_4,${SVE_R0}[2]
	umlalb	$SVE_ACC1,$SVE_INlo_4,${SVE_S2}[2]
	umlalb	$SVE_ACC2,$SVE_INlo_4,${SVE_S3}[2]
	mov		z12.d,z17.d

	// Lazy reduction
	bl		poly1305_lazy_reduce_sve2
	ldr	x30,[sp,#8]

	mov		z13.d,z18.d

.Lshort_tail_sve2:

	cmp     $vl, #2
    b.ls    .Last_reduce_sve2

	mov		x15,#1
	whilelo p1.s,xzr,x15

.Loop_reduce_sve2:
	////////////////////////////////////////////////////////////////
	// (hash + inp[hi])*r^{vl/2..2}                               //
	//       \____________________/                               //
	//  iterative reduction part of the short tail                //
	////////////////////////////////////////////////////////////////
	// Last column of products is calculated by iteratively "folding" vectors:
	// 1. If vl==2 - skip to Last_reduce_sve2
	// 2. calculate product with r^{vl/2} -> ACC{0-4}
	// 3. lazy reduction -> H{0-4}
	// 4. upper half of vectors (INlo{0-4}) is copied to lower halves
	// 5. If vl/2==2 - go to Last_reduce_sve2
	// 6. continue with 2.
	// NB: this part is skipped for 128-bit case (vl==2)
	// For 256-bit, no intermediate loading is necessary - r^2 is already in [1].
	//  So a special case can be easily implemented, when corresponding hardware is available.

	// Load the intermediate r-power into the 0th lanes of vectors
	// Interleave with broadcasting and S-vector calculation.
	ldr		w10,[$pwr]
	ldr		w11,[$pwr,#28]
	ldr		w12,[$pwr,#56]
	cpy		$SVE_R0,p1/m,w10
	ldr		w13,[$pwr,#84]
	cpy		$SVE_R1,p1/m,w11
	dup		z0.q,z0.q[0]
	ldr		w14,[$pwr,#112]
	cpy		$SVE_R2,p1/m,w12
	dup		z1.q,z1.q[0]
	cpy		$SVE_R3,p1/m,w13
	dup		z3.q,z3.q[0]
	cpy		$SVE_R4,p1/m,w14
	add		$pwr,$pwr,#4			// Increment pointer for the next iteration
	dup		z5.q,z5.q[0]
	dup		z7.q,z7.q[0]

	// Interleaved hash contraction and S-vector calc.
	add 	$SVE_INlo_2,$SVE_INlo_2,$SVE_H2
	adr     $SVE_S1,[$SVE_R1,$SVE_R1,lsl #2]
	add 	$SVE_INlo_0,$SVE_INlo_0,$SVE_H0
	adr     $SVE_S2,[$SVE_R2,$SVE_R2,lsl #2]
	add 	$SVE_INlo_1,$SVE_INlo_1,$SVE_H1
	adr     $SVE_S3,[$SVE_R3,$SVE_R3,lsl #2]
	add 	$SVE_INlo_3,$SVE_INlo_3,$SVE_H3
	adr     $SVE_S4,[$SVE_R4,$SVE_R4,lsl #2]
	add 	$SVE_INlo_4,$SVE_INlo_4,$SVE_H4

	umullb	$SVE_ACC3,$SVE_INlo_0,${SVE_R3}[0]
	umullb	$SVE_ACC4,$SVE_INlo_0,${SVE_R4}[0]
	umullb	$SVE_ACC2,$SVE_INlo_0,${SVE_R2}[0]
	umullb	$SVE_ACC0,$SVE_INlo_0,${SVE_R0}[0]
	umullb	$SVE_ACC1,$SVE_INlo_0,${SVE_R1}[0]

	umlalb	$SVE_ACC3,$SVE_INlo_1,${SVE_R2}[0]
	umlalb	$SVE_ACC4,$SVE_INlo_1,${SVE_R3}[0]
	umlalb	$SVE_ACC0,$SVE_INlo_1,${SVE_S4}[0]
	umlalb	$SVE_ACC2,$SVE_INlo_1,${SVE_R1}[0]
	umlalb	$SVE_ACC1,$SVE_INlo_1,${SVE_R0}[0]

	umlalb	$SVE_ACC3,$SVE_INlo_2,${SVE_R1}[0]
	umlalb	$SVE_ACC0,$SVE_INlo_2,${SVE_S3}[0]
	umlalb	$SVE_ACC4,$SVE_INlo_2,${SVE_R2}[0]
	umlalb	$SVE_ACC1,$SVE_INlo_2,${SVE_S4}[0]
	umlalb	$SVE_ACC2,$SVE_INlo_2,${SVE_R0}[0]

	umlalb	$SVE_ACC3,$SVE_INlo_3,${SVE_R0}[0]
	umlalb	$SVE_ACC0,$SVE_INlo_3,${SVE_S2}[0]
	umlalb	$SVE_ACC4,$SVE_INlo_3,${SVE_R1}[0]
	umlalb	$SVE_ACC1,$SVE_INlo_3,${SVE_S3}[0]
	umlalb	$SVE_ACC2,$SVE_INlo_3,${SVE_S4}[0]

	umlalb	$SVE_ACC3,$SVE_INlo_4,${SVE_S4}[0]
	umlalb	$SVE_ACC0,$SVE_INlo_4,${SVE_S1}[0]
	umlalb	$SVE_ACC4,$SVE_INlo_4,${SVE_R0}[0]
	umlalb	$SVE_ACC1,$SVE_INlo_4,${SVE_S2}[0]
	umlalb	$SVE_ACC2,$SVE_INlo_4,${SVE_S3}[0]

	// Lazy reduction
	bl		poly1305_lazy_reduce_sve2
	ldr	x30,[sp,#8]

	// Move higher part of vectors to lower part, depending on current vl
	// NB look-up is done in terms of single-word lanes, hence indices
	//  start from vl (refer to as w16) and not vl/2
	// Higher part now contains "junk"
	index	${SVE_T0}.s,w16,#1
	tbl		${SVE_INlo_0},${SVE_INlo_0},${SVE_T0}.s
	tbl		${SVE_INlo_1},${SVE_INlo_1},${SVE_T0}.s
	tbl		${SVE_INlo_2},${SVE_INlo_2},${SVE_T0}.s
	tbl		${SVE_INlo_3},${SVE_INlo_3},${SVE_T0}.s
	tbl		${SVE_INlo_4},${SVE_INlo_4},${SVE_T0}.s
	lsr		$vl,$vl,#1		// vl /= 2
	cmp 	$vl,#2
	b.hi	.Loop_reduce_sve2

.Last_reduce_sve2:
	////////////////////////////////////////////////////////////////
	// (hash + inp[n-1])*r^2                                      //
	//+(hash + inp[n]  )*r                                        //
	//       \_____________/                                      //
	//  Final part of the short tail                              //
	////////////////////////////////////////////////////////////////

	//Last hash addition - now everything stored in SVE_Hx
	add 	$SVE_H2,$SVE_H2,$SVE_INlo_2
	add 	$SVE_H0,$SVE_H0,$SVE_INlo_0
	add 	$SVE_H1,$SVE_H1,$SVE_INlo_1
	add 	$SVE_H3,$SVE_H3,$SVE_INlo_3
	add 	$SVE_H4,$SVE_H4,$SVE_INlo_4

	// Shift even lanes to odd lanes and set even to zero
	//  because r^2 and r^1 are in lanes 1 and 3 of R-vectors
	trn1	$SVE_H2,${SVE_MASK}.s,$SVE_H2
	trn1	$SVE_H0,${SVE_MASK}.s,$SVE_H0
	trn1	$SVE_H1,${SVE_MASK}.s,$SVE_H1
	trn1	$SVE_H3,${SVE_MASK}.s,$SVE_H3
	trn1	$SVE_H4,${SVE_MASK}.s,$SVE_H4

	umullt	$SVE_ACC3,$SVE_H2,${SVE_R1}
	umullt	$SVE_ACC0,$SVE_H2,${SVE_S3}
	umullt	$SVE_ACC4,$SVE_H2,${SVE_R2}
	umullt	$SVE_ACC1,$SVE_H2,${SVE_S4}
	umullt	$SVE_ACC2,$SVE_H2,${SVE_R0}

	umlalt	$SVE_ACC3,$SVE_H0,${SVE_R3}
	umlalt	$SVE_ACC4,$SVE_H0,${SVE_R4}
	umlalt	$SVE_ACC2,$SVE_H0,${SVE_R2}
	umlalt	$SVE_ACC0,$SVE_H0,${SVE_R0}
	umlalt	$SVE_ACC1,$SVE_H0,${SVE_R1}

	umlalt	$SVE_ACC3,$SVE_H1,${SVE_R2}
	umlalt	$SVE_ACC4,$SVE_H1,${SVE_R3}
	umlalt	$SVE_ACC0,$SVE_H1,${SVE_S4}
	umlalt	$SVE_ACC2,$SVE_H1,${SVE_R1}
	umlalt	$SVE_ACC1,$SVE_H1,${SVE_R0}

	umlalt	$SVE_ACC3,$SVE_H3,${SVE_R0}
	umlalt	$SVE_ACC0,$SVE_H3,${SVE_S2}
	umlalt	$SVE_ACC4,$SVE_H3,${SVE_R1}
	umlalt	$SVE_ACC1,$SVE_H3,${SVE_S3}
	umlalt	$SVE_ACC2,$SVE_H3,${SVE_S4}

	umlalt	$SVE_ACC3,$SVE_H4,${SVE_S4}
	umlalt	$SVE_ACC0,$SVE_H4,${SVE_S1}
	umlalt	$SVE_ACC4,$SVE_H4,${SVE_R0}
	umlalt	$SVE_ACC1,$SVE_H4,${SVE_S2}
	umlalt	$SVE_ACC2,$SVE_H4,${SVE_S3}

	// Generate predicate for the last two double words
	mov		x15,#2
	whilelo p2.d,xzr,x15

	dup 	${SVE_MASK}.d,#-1
	lsr 	${SVE_MASK}.d,${SVE_MASK}.d,#38

	////////////////////////////////////////////////////////////////
	// horizontal add

	//In Neon implementation, one effectively using lower 64 bits of vector registers here.
	//Here and below I use hard-coded FP registers.

	uaddv	d22,p2,$SVE_ACC3
	 ldp	d8,d9,[sp,#80]		// meet ABI requirements
	uaddv	d19,p2,$SVE_ACC0
	 ldp	d10,d11,[sp,#96]
	uaddv	d23,p2,$SVE_ACC4
	 ldp	d12,d13,[sp,#112]
	uaddv	d20,p2,$SVE_ACC1
	 ldp	d14,d15,[sp,#128]
	uaddv	d21,p2,$SVE_ACC2

	////////////////////////////////////////////////////////////////
	// Lazy reduction, but without narrowing

	// Since results were accumulated in the lower 64 bits,
	//  one can refer to them as FP/aSIMD reg-s.

	ushr	d29,d22,#26
	and 	v22.8b,v22.8b,v31.8b
	ushr	d30,d19,#26
	and 	v19.8b,v19.8b,v31.8b

	add 	d23,d23,d29				// h3 -> h4
	add 	d20,d20,d30				// h0 -> h1

	ushr	d29,d23,#26
	and 	v23.8b,v23.8b,v31.8b
	ushr	d30,d20,#26
	and 	v20.8b,v20.8b,v31.8b
	add 	d21,d21,d30				// h1 -> h2

	add 	d19,d19,d29
	shl 	d29,d29,#2
	ushr	d30,d21,#26
	and 	v21.8b,v21.8b,v31.8b
	add 	d19,d19,d29				// h4 -> h0
	add 	d22,d22,d30				// h2 -> h3

	ushr	d29,d19,#26
	and 	v19.8b,v19.8b,v31.8b
	ushr 	d30,d22,#26
	and 	v22.8b,v22.8b,v31.8b
	add 	d20,d20,d29				// h0 -> h1
	add 	d23,d23,d30				// h3 -> h4

	////////////////////////////////////////////////////////////////
	// write the result, can be partially reduced

	stp 	s19,s20,[$ctx],#8
	stp 	s21,s22,[$ctx],#8
	str 	s23,[$ctx]
	
.Lno_data_sve2:
	// Restore the callee-saved GPRs
	ldp	x19,x20,[sp,#16]
	ldp	x21,x22,[sp,#32]
	ldp	x23,x24,[sp,#48]
	ldp	x25,x26,[sp,#64]
	ldr	x29,[sp],#144
	AARCH64_VALIDATE_LINK_REGISTER
	ret

.Lshort_blocks:
	b	poly1305_blocks

.size	poly1305_blocks_sve2,.-poly1305_blocks_sve2
___

##############################################################################
#
# SVE instruction encoder, adapted from chacha20-sve.pl
#
##############################################################################

my $debug_encoder = 0;

{
my  %opcode_unpred = (
	"eor"          => 0x04a03000,
	"add"          => 0x04200000,
	"orr"          => 0x04603000,
	"mov"          => 0x04603000, # Alias for ORR
	"and"          => 0x04203000,
	"lsl"          => 0x04209C00,
	"lsr"          => 0x04209400,
	"zip1"         => 0x05206000,
	"zip2"         => 0x05206400,
	"trn1"         => 0x05207000,
	"dup_gpr"      => 0x05203800,
	"dup_elem"     => 0x05302000,
	"cntd"         => 0x04e0e000,
	"tbl"          => 0x05203000,
	"adr"          => 0x04a0a000,
	"umullb"       => 0x44e0d000,
    "umullt"       => 0x45c07c00,
    "umlalb"       => 0x44e09000,
    "umlalt"       => 0x44c04c00,
	"shrnb"        => 0x45201000);

my  %opcode_imm_unpred = (
	"dup"          => 0x2538C000,
	"index"        => 0x04204400);

my %opcode_scalar_pred = (
	"cpy"          => 0x0528A000);

my  %opcode_pred = (
	"whilelo"      => 0x25200C00,
	"ptrue"        => 0x2518E000,
	"ld4w"         => 0xA560E000,
	"ld1w"         => 0xA540A000,
	"revb"         => 0x05248000,
    "uaddv"        => 0x04012000);

my  %tsize = (
	'b'          => 0,
	'h'          => 1,
	's'          => 2,
	'd'          => 3,
	'q'          => 3); # To handle dup zx.q,zx.q[i] case

my %sf = (
	"w"          => 0,
	"x"          => 1);

my %pattern = ("ALL" => 31);

sub create_verifier {
	my $filename="./compile_sve.sh";

$scripts = <<'___';
#! /bin/bash
set -e
CROSS_COMPILE=${CROSS_COMPILE:-'aarch64-linux-gnu-'}

[ -z "$1" ] && exit 1
INST_TO_COMPILE="$1"
FILENAME_BASE=${1%% *}
TMPFILE="/tmp/${FILENAME_BASE}_test"
OBJDUMP_LOG="/tmp/${FILENAME_BASE}_objdump.log"

echo "--- DEBUG INFO ---" >&2
echo "Received \$1 (Instruction): '$1'" >&2
echo "Using Filename Base: '$FILENAME_BASE'" >&2
echo "------------------" >&2

ARCH=`uname -p | xargs echo -n`

if [ $ARCH == 'aarch64' ]; then
    CC=gcc-11
    AS=as
    OBJDUMP=objdump
else
    CC=${CROSS_COMPILE}gcc
    AS=${CROSS_COMPILE}as
    OBJDUMP=${CROSS_COMPILE}objdump
fi

cat > "${TMPFILE}.c" << EOF
extern __attribute__((noinline, section("disasm_output"))) void dummy_func()
{
    asm("$INST_TO_COMPILE");
}
int main(int argc, char *argv[])
{
}
EOF

$CC -march=armv8.2-a+sve+sve2 -S -o "${TMPFILE}.s" "${TMPFILE}.c"

$AS -march=armv8-a+sve2 -o "${TMPFILE}.o" "${TMPFILE}.s"

#$OBJDUMP -d "${TMPFILE}.o" > "$OBJDUMP_LOG"

#cat "$OBJDUMP_LOG" | awk -F"\n" -v RS="\n\n" '$1 ~ /dummy_func/' | awk 'FNR == 2 {printf "%s",$2}'
$OBJDUMP -d "${TMPFILE}.o" | awk -F"\n" -v RS="\n\n" '$1 ~ /dummy_func/' | awk 'FNR == 2 {printf "%s",$2}'

rm "${TMPFILE}.c" "${TMPFILE}.s" "${TMPFILE}.o"
___
	open(FH, '>', $filename) or die $!;
	print FH $scripts;
	close(FH);
	system("chmod a+x ./compile_sve.sh");
}

sub compile_sve {
	my $inst = shift;
    return `./compile_sve.sh "$inst"`;
}

sub verify_inst {
	my ($code,$inst)=@_;
	my $hexcode = (sprintf "%08x", $code);

	if ($debug_encoder == 1) {
		my $expect=&compile_sve($inst);
		if ($expect ne $hexcode) {
			return (sprintf "%s // Encode Error! expect [%s] actual [%s]", $inst, $expect, $hexcode);
		}
	}
	return (sprintf ".inst\t0x%s\t//%s", $hexcode, $inst);
}

sub reg_code {
	my $code = shift;

	if ($code == "zr") {
		return "31";
	}
	return $code;
}

sub encode_size_imm() {
	my ($mnemonic, $isize, $const)=@_;
	my $esize = (8<<$tsize{$isize});
	my $tsize_imm;
	if ($mnemonic eq "shrnb") {
        # Formula for narrowing shifts
        $tsize_imm = $esize - $const;
    } elsif ($mnemonic eq "lsr") {
        # Formula for logical right shifts
        $tsize_imm = 2*$esize - $const;
    } else {
        # Default formula for logical left shifts (lsl)
        $tsize_imm = $esize + $const;
    }
	return (($tsize_imm>>5)<<22)|(($tsize_imm&0x1f)<<16);
}

sub sve_unpred {
    my ($mnemonic,$arg)=@_;
    my $inst = (sprintf "%s %s", $mnemonic,$arg);
    # Special case: Widening multiplies (indexed and vector)
    if (($mnemonic =~ /^(umull[bt]|umlal[bt])/) && $arg =~ m/z([0-9]+)\.d,\s*z([0-9]+)\.s,\s*z([0-9]+)\.s(\[([0-9]+)\])?/o) {
        my ($zd, $zn, $zm, $indexed, $imm) = ($1, $2, $3, $4, $5);
        my $opcode = $opcode_unpred{$mnemonic};
        if ($indexed) {
			# Split the 2-bit immediate index into its parts.
            my $i2h = ($imm >> 1) & 0x1; # High bit of index
            my $i2l = $imm & 0x1;       # Low bit of index
            # Get the low 4 bits of the Zm register.
            my $zm_low = $zm & 0xF;
            return &verify_inst($opcode|($i2h << 20)|($zm_low << 16)|($i2l << 11)|($zn << 5)|$zd,$inst);
        } else {
            return &verify_inst($opcode|$zd|($zn<<5)|($zm<<16), $inst);
        }
    # Special case: 3-register vector ADR with lsl #2
    } elsif ($mnemonic eq "adr" && $arg =~ m/z([0-9]+)\.s,\s*\[z([0-9]+)\.s,\s*z([0-9]+)\.s,\s*lsl\s*#2\]/o) {
        my ($zd, $zn, $zm) = ($1, $2, $3);
        my $opcode = $opcode_unpred{"adr"};
        # Per the manual, the 'sz' bit (22) must be 0 for .s size.
        # It is already 0 in our base, so we do nothing.
        # The 'msz' field (bits 11-10) must be '10'. We achieve this by setting bit 11.
        $opcode |= (1<<11);
        return &verify_inst($opcode|$zd|($zn<<5)|($zm<<16), $inst);
    # Special case: 'cntd xd' alias
    } elsif ($mnemonic eq "cntd" && $arg =~ m/x([0-9]+)/o) {
        my ($xd) = ($1);
        my $opcode = $opcode_unpred{$mnemonic};
        my $pattern_all = $pattern{"ALL"} << 5;
        return &verify_inst($opcode|$xd|$pattern_all, $inst);
    # Special parser for SHRNB's unique syntax (Zd.s, Zn.d, #imm)
    } elsif ($mnemonic eq "shrnb" && $arg =~ m/z([0-9]+)\.[bhsd],\s*z([0-9]+)\.([bhsd]),\s*#([0-9]+)/o) {
        my ($zd, $zn, $size_src, $imm) = ($1, $2, $3, $4);
        my $opcode = $opcode_unpred{$mnemonic};
        return &verify_inst($opcode|&encode_size_imm($mnemonic,$size_src,$imm)|($zn << 5)|$zd, $inst);
	} elsif ($mnemonic eq "dup" && $arg =~ m/z([0-9]+)\.q,\s*z([0-9]+)\.q\[0\]/o) { # DUP from element
        my ($zd, $zn) = ($1, $2);
        my $opcode = $opcode_unpred{"dup_elem"};
        return &verify_inst($opcode | ($zn << 5) | $zd, $inst);
	} elsif ($mnemonic eq "dup" && $arg =~ m/z([0-9]+)\.([bhsdq]),\s*w([0-9]+)/o) { # DUP from GPR (wX/xX)
        my ($zd, $size, $rn) = ($1, $2, $3);
        my $opcode = $opcode_unpred{"dup_gpr"};
        $opcode |= ($tsize{$size}<<22);
        return &verify_inst($opcode|$zd|($rn<<5), $inst);
	# Generic argument patterns
    } elsif ($arg =~ m/z([0-9]+)\.([bhsdq]),\s*(.*)/o) {
        my ($zd, $size, $regs) = ($1, $2, $3);
        my $opcode = $opcode_unpred{$mnemonic};
		# Handle shift-by-immediate separately due to its unique encoding.
        if ($mnemonic eq "lsl" || $mnemonic eq "lsr") {
            if ($regs =~ m/z([0-9]+)\.[bhsd],\s*#([0-9]+)/o) {
                my ($zn, $imm) = ($1, $2);
                return &verify_inst($opcode|$zd|($zn<<5)|&encode_size_imm($mnemonic,$size,$imm), $inst);
            }
        }
		if ($mnemonic !~ /^(and|orr|eor|mov)$/) {
        	$opcode |= ($tsize{$size}<<22);
    	}
        if ($regs =~ m/z([0-9]+)\.[bhsdq],\s*z([0-9]+)\.[bhsdq]/o) { # 3-operand vector
            my ($zn, $zm) = ($1, $2);
            return &verify_inst($opcode|$zd|($zn<<5)|($zm<<16), $inst);
        } elsif ($regs =~ m/z([0-9]+)\.[bhsdq]/o) { # 2-operand vector (mov)
            my $zn = $1;
            my $zm = ($mnemonic eq "mov") ? $zn : 0;
            return &verify_inst($opcode|$zd|($zn<<5)|($zm<<16), $inst);
        } elsif ($regs =~ m/w([0-9]+),\s*#1/o) { # index
            my ($rn, $rm) = ($1, 1);
            $opcode = $opcode_imm_unpred{"index"};
			$opcode |= ($tsize{$size}<<22);
            return &verify_inst($opcode|$zd|($rn<<5)|($rm<<16), $inst);
        } elsif ($regs =~ m/#(-?[0-9]+)/o) { # dup from immediate
            my $imm = $1;
            $opcode = $opcode_imm_unpred{"dup"};
			$opcode |= ($tsize{$size}<<22);
            my $imm_val = $imm & 0xff; # Only accounting for a simple case with zero shift.
            return &verify_inst($opcode|$zd|($imm_val<<5), $inst);
        }
    }
    sprintf "%s // fail to parse: %s", $mnemonic, $arg;
}

sub sve_pred {
    my ($mnemonic, $arg)=@_;
    my $inst = (sprintf "%s %s", $mnemonic,$arg);
    # Special case: Multi-register loads (ld4w)
    if ($arg =~ m/\{\s*z([0-9]+)\.s-z([0-9]+)\.s\s*\},\s*p([0-9]+)\/z,\s*\[(x[0-9]+)\]/o) {
        my ($zt, $pg, $xn) = ($1, $3, $4);
        $xn =~ s/x//;
        my $opcode = $opcode_pred{$mnemonic};
        return &verify_inst($opcode|$zt|($pg<<10)|($xn<<5), $inst);
    # Special case: Single-register loads (ld1w)
    } elsif ($arg =~ m/\{\s*z([0-9]+)\.s\s*\},\s*p([0-9]+)\/z,\s*\[(x[0-9]+)\]/o) {
        my ($zt, $pg, $xn) = ($1, $2, $3);
        $xn =~ s/x//;
        my $opcode = $opcode_pred{$mnemonic};
        return &verify_inst($opcode|$zt|($pg<<10)|($xn<<5), $inst);
    # Special case: uaddv (scalar destination)
    } elsif ($mnemonic eq "uaddv" && $arg =~ m/d([0-9]+),\s*p([0-9]+),\s*z([0-9]+)\.([bhsd])/o) {
        my ($vd, $pg, $zn, $size) = ($1, $2, $3, $4);
        my $opcode = $opcode_pred{$mnemonic};
        return &verify_inst($opcode|($tsize{$size}<<22)|$vd|($pg<<10)|($zn<<5), $inst);
    # Generic pattern: Starts with a predicate register (whilelo, ptrue)
    } elsif ($arg =~ m/p([0-9]+)\.([bhsd]),\s*(.*)/o) {
        my ($pd, $size, $regs) = ($1, $2, $3);
        my $opcode = $opcode_pred{$mnemonic};
        if ($regs =~ m/([wx])(zr|[0-9]+),\s*[wx](zr|[0-9]+)/o) { # whilelo
            my ($sf_char, $rn, $rm) = ($1, $2, $3);
            return &verify_inst($opcode|($tsize{$size}<<22)|$pd|($sf{$sf_char}<<12)|(&reg_code($rn)<<5)|(&reg_code($rm)<<16), $inst);
        } elsif ($regs =~ m/(\w+)/o) { # ptrue
            my $pat = $1;
            return &verify_inst($opcode|($tsize{$size}<<22)|$pd|($pattern{$pat}<<5), $inst);
        }
    # Generic pattern: Starts with a vector register (cpy, revb)
    } elsif ($arg =~ m/z([0-9]+)\.([bhsd]),\s*p([0-9]+)\/m,\s*(.*)/o) {
        my ($zd, $size, $pg, $regs) = ($1, $2, $3, $4);
        if ($regs =~ m/w([0-9]+)/o) { # CPY from GPR
            my $wn = $1;
            my $opcode = $opcode_scalar_pred{"cpy"};
            return &verify_inst($opcode|($tsize{$size}<<22)|$zd|($pg<<10)|($wn<<5), $inst);
        } elsif ($regs =~ m/z([0-9]+)\.([bhsd])/o) { # 2-operand predicated (revb)
            my ($zn) = ($1);
            my $opcode = $opcode_pred{$mnemonic};
            return &verify_inst($opcode|($tsize{$size}<<22)|$zd|($pg<<10)|($zn<<5), $inst);
        }
    }
    sprintf "%s // fail to parse: %s", $mnemonic, $arg;
}

open SELF,$0;
while(<SELF>) {
	next if (/^#!/);
	last if (!s/^#/\/\// and !/^$/);
	print;
}
close SELF;

if ($debug_encoder == 1) {
	&create_verifier();
}

foreach my $line (split("\n",$code)) {
    my $original_line = $line;
    my $encoded_line = "";
    # Perform variable substitution
    $line =~ s/\`([^\`]*)\`/eval($1)/ge;
    # Predicated instructions
    if ($line =~ /^\s*(\w+)\s+(z[0-9]+\.[bhsd],\s*p[0-9].*)/) {
        $encoded_line = sve_pred($1, $2);
    }
	elsif ($line =~ /^\s*(\w+)\s+(d[0-9]+,\s*p[0-9].*)/) {
        $encoded_line = sve_pred($1, $2);
    }
    elsif ($line =~ /^\s*(\w+[1-4][bhwd])\s+(\{\s*z[0-9]+.*\},\s*p[0-9]+.*)/) {
        $encoded_line = sve_pred($1, $2);
    }
    elsif ($line =~ /^\s*(\w+)\s+(p[0-9]+\.[bhsd].*)/) {
        $encoded_line = sve_pred($1, $2);
    }
    # Specific unpredicated instructions
    elsif ($line =~ /^\s*(dup)\s+(z[0-9]+\.q,\s*z[0-9]+\.q\[0\])/) {
        $encoded_line = sve_unpred($1, $2);
    }
    elsif ($line =~ /^\s*(dup)\s+(z[0-9]+\.[bhsdq],\s*(?:w|x)[0-9]+)/) {
        $encoded_line = sve_unpred($1, $2);
    }
    elsif ($line =~ /^\s*(mov)\s+(z[0-9]+\.d,\s*z[0-9]+\.d)/) {
        $encoded_line = sve_unpred("mov", $2);
    }
    elsif ($line =~ /^\s*(umull[bt]|umlal[bt])\s+(z[0-9]+\.d,\s*z[0-9]+\.s,\s*z[0-9]+\.s(?:\[[0-9]+\])?)/) {
        $encoded_line = sve_unpred($1, $2);
    }
    elsif ($line =~ /^\s*(cntd)\s+((x|w)[0-9]+.*)/) {
        $encoded_line = sve_unpred($1, $2);
    }
    # 3. Generic Unpredicated "catch-all"
    elsif ($line =~ /^\s*(\w+)\s+(z[0-9]+\.[bhsdq].*)/) {
        $encoded_line = sve_unpred($1, $2);
    }
    if ($encoded_line) {
        print $encoded_line, "\n";
    } else {
        print $original_line, "\n";
    }
}

}
 STDOUT or die "error closing STDOUT: $!";
