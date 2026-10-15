// Copyright 2016-2026 The OpenSSL Project Authors. All Rights Reserved.
//
// Licensed under the Apache License 2.0 (the "License").  You may not use
// this file except in compliance with the License.  You can obtain a copy
// in the file LICENSE in the source distribution or at
// https://www.openssl.org/source/license.html

//#############################################################################
//
// Copyright (c) 2025, Iakov Polyak <iakov.polyak@linaro.org>
// This file is an SVE2 port-and-merge of POLY1305 hash algorithm, derived from
// the OpenSSL Neon implementation and a vector length agnostic (VLA)
// RISC-V implementation from the CRYPTOGAMS project.
//
//#############################################################################
//
// Copyright (c) 2006, CRYPTOGAMS by <appro@openssl.org>
// All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//      *	Redistributions of source code must retain copyright notices,
//	this list of conditions and the following disclaimer.
//
//      *	Redistributions in binary form must reproduce the above
//	copyright notice, this list of conditions and the following
//	disclaimer in the documentation and/or other materials
//	provided with the distribution.
//
//      *	Neither the name of the CRYPTOGAMS nor the names of its
//	copyright holder and contributors may be used to endorse or
//	promote products derived from this software without specific
//	prior written permission.
//
//ALTERNATIVELY, provided that this notice is retained in full, this
//product may be distributed under the terms of the GNU General Public
//License (GPL), in which case the provisions of the GPL apply INSTEAD OF
//those given above.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//#############################################################################
//
// September 2025
//
// This is a 100% vector length agnostic implementation and has
// been tested with QEMU for the vector length of up to 2048 bits.
//
// On Graviton4, with the vector register length of 128 bits,
// it is less efficient than the Neon implementation by only 6%.
// This number has been obtained by running
// `openssl speed -evp ChaCha20-POLY1305` and
// `openssl speed -evp ChaCha20`, pinned to a single CPU,
// converting the 8192-byte result to cycles per byte
// using actual average runtime CPU frequency from `perf stat`,
// and taking the difference. On Graviton 4, this results in 
// 0.62 cpb for Neon and 0.66 for SVE2.
// 
// While Neon should probably be the default choice on a 128-bit architecture,
// speed-up is clearly expected with 256-bit and larger vector registers
// in the future.

// $output is the last argument if it looks like a file (it has an extension)
// $flavour is the first argument if it doesn't look like a file
#include "arch/arm_arch.h"

.text





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

.align	5
poly1305_sw_2_26:
	// Converts 3 base2_44 -> 5 base2_26 values and stores
	mov	x15,#0x3ffffff			// w15  : 2^26-1 mask
	and	x10,x7,x15				// w10 -> r0
	lsr	x11,x7,#26				// w11 : top 18 bits of r0
	str	w10,[x5]				// Store r0
	bfi	x11,x8,#18,#8			// w11 -> r1
	ubfx	x12,x8,#8,#26			// w12 -> r2
	str	w11,[x5,#28]			// Store r1
	lsr	x13,x8,#34				// w13 : top 10 bits of r1
	str	w12,[x5,#56]			// Store r2
	bfi	x13,x9,#10,#16			// w13 -> r3
	lsr	x14,x9,#16				// w14 -> r4
	str	w13,[x5,#84]			// Store r3
	str	w14,[x5,#112]			// Store r4
	ret


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

.align	5
poly1305_sqr_2_44:

    // Pre-calculate constants and doubled terms.
	mov	x12,#20
	lsl	x13,x8,#1		// x13 = r1 * 2
	mul	x12,x9,x12		// x12 = r2 * 20
	lsl	x10,x7,#1		// x10 = r0 * 2

    // --- Calculate d2 = r1*r1 + 2*r0*r2 ---
	umulh	x24,x8,x8	// high part of r1*r1
	mul	x23,x8,x8	// low part of r1*r1
	umulh	x15,x10,x9		// high part of (r0*2)*r2
	mul	x14,x10,x9		// low part of (r0*2)*r2

    // --- Calculate d0 = r0*r0 + 20*(2*r1*r2) ---
	umulh	x20,x7,x7	// high part of r0*r0
	mul	x19,x7,x7	// low part of r0*r0
	umulh	x11,x13,x12		// high part of (r1*2)*(r2*20)
	mul	x10,x13,x12		// low part of (r1*2)*(r2*20)

	adds	x23,x23,x14	// d2_lo
	adc	x24,x24,x15	// d2_hi

    // --- Calculate d1 = 2*r0*r1 + 20*r2*r2 ---
    // d1 is a 128-bit result stored in x7:x6 (hi:lo)
	umulh	x22,x7,x13	// high part of r0*(r1*2)
	mul	x21,x7,x13	// low part of r0*(r1*2)
	umulh	x13,x9,x12		// high part of r2*(r2*20)
	mul	x12,x9,x12		// low part of r2*(r2*20)

	adds	x19,x19,x10	// d0_lo
	adc	x20,x20,x11	// d0_hi

	adds	x21,x21,x12	// d1_lo
	adc	x22,x22,x13	// d1_hi

    // --- Reduction and Carry Propagation ---
    // Reduce the 128-bit d0, d1, d2 back to three 44-bit limbs in x0, x1, x2
	lsr	x10,x19,#44	// (d0_lo >> 44)
	lsl	x11,x20,#20	// (d0_hi << 20) - high 20 bits are zero
	and	x7,x19,x26	// r0 -> d0_lo & mask
	orr	x10,x10,x11		// x10 -> 64-bit carry from d0

	lsr	x12,x21,#44	// (d1_lo >> 44)
	lsl	x13,x22,#20	// (d1_hi << 20)
	and	x8,x21,x26	// r1 -> d1_lo & mask
	orr	x12,x12,x13		// x12 -> 64-bit carry from d1
	add	x8,x8,x10		// r1 += carry from d0

	lsr	x11,x26,#2	// x11 -> 2^42-1 mask for d2 reduction
	lsr	x10,x23,#42	// (d2_lo >> 42)
	lsl	x13,x24,#22	// (d2_hi << 22)
	and	x9,x23,x11	// r2 -> d2_lo & 2^42-1 mask
	orr	x10,x10,x13		// x10 -> final carry from d2
	add	x9,x9,x12		// r2 += carry from d1

    // Handle ripple-carry from r2 and apply the *5 reduction.
	lsr	x13,x9,#42		// Get carry from r2 (if r2 >= 2^42)
	and	x9,x9,x11		// Mask r2 back down to 42 bits
	add	x10,x10,x13		// Add this ripple-carry to the final carry

	add	x11,x10,x10,lsl #2	// x11 -> final_carry * 5
	add	x7,x7,x11			// r0 += final_carry * 5

    // Final ripple-carry chain to ensure all limbs are 44 bits.
	lsr	x11,x8,#44		// Get carry from r1
	and	x8,x8,x26	// Mask r1 to 44 bits
	add	x9,x9,x11		// r2 += carry from r1

	lsr	x10,x7,#44		// Get carry from r0
	and	x7,x7,x26	// Mask r0 to 44 bits
	add	x8,x8,x10		// r1 += carry from r0

	ret


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


.align	5
poly1305_lazy_reduce_sve2:
.long	0x25f8dfff	//dup z31.d,#-1
.long	0x04e696dd	//lsr z29.d,z22.d,#26
.long	0x05b872db	//trn1 z27.s,z22.s,z24.s					// reproducing Neon's xtn - treat ACC3 as a .s vector
.long	0x04ba97ff	//lsr z31.d,z31.d,#38
.long	0x04e6967e	//lsr z30.d,z19.d,#26
.long	0x043f3273	//and z19.d,z19.d,z31.d
.long	0x04fd02f7	//add z23.d,z23.d,z29.d	    // h3 -> h4
	// Neon's bic is replaced with &=z31 (because of using even-indexed elements)
.long	0x043f337b	//and z27.d,z27.d,z31.d			// refer to SVE_H3 as .d
.long	0x04fe0294	//add z20.d,z20.d,z30.d	    // h0 -> h1

.long	0x04e696fd	//lsr z29.d,z23.d,#26
.long	0x05b872fc	//trn1 z28.s,z23.s,z24.s					// reproducing Neon's xtn - treat ACC4 as a .s vector
.long	0x04e6969e	//lsr z30.d,z20.d,#26
.long	0x05b87299	//trn1 z25.s,z20.s,z24.s					// reproducing Neon's xtn - treat ACC1 as a .s vector
.long	0x043f339c	//and z28.d,z28.d,z31.d			// refer to SVE_H4 as .d
.long	0x04fe02b5	//add z21.d,z21.d,z30.d	    // h1 -> h2

.long	0x04fd0273	//add z19.d,z19.d,z29.d
.long	0x04a29fbd	//lsl z29.d,z29.d,#2
.long	0x456612be	//shrnb z30.s,z21.d,#26			// check it's OK
.long	0x05b872ba	//trn1 z26.s,z21.s,z24.s					// reproducing Neon's xtn - treat ACC2 as a .s vector
.long	0x04fd0273	//add z19.d,z19.d,z29.d		// h4 -> h0
.long	0x043f3339	//and z25.d,z25.d,z31.d			// refer to SVE_H1 as .d
.long	0x04be037b	//add z27.s,z27.s,z30.s			// h2 -> h3
.long	0x043f335a	//and z26.d,z26.d,z31.d			// refer to SVE_H2 as .d

.long	0x4566127d	//shrnb z29.s,z19.d,#26
.long	0x05b87278	//trn1 z24.s,z19.s,z24.s					// reproducing Neon's xtn - treat ACC0 as a .s vector - re-writing H0 here...
.long	0x0466977e	//lsr z30.s,z27.s,#26
.long	0x043f337b	//and z27.d,z27.d,z31.d			// refer to SVE_H3 as .d
.long	0x04bd0339	//add z25.s,z25.s,z29.s			// h0 -> h1
.long	0x043f3318	//and z24.d,z24.d,z31.d			// refer to SVE_H0 as .d
.long	0x04be039c	//add z28.s,z28.s,z30.s			// h3 -> h4

.long	0x04bf33ff	//eor z31.d,z31.d,z31.d	// reset zero mask

	ret


// --- poly1305_blocks_sve2 ---
// Main function, implementing POLY1305 algorithm as discussed
// in "NEON crypto" by D.J. Bernstein and P. Schwabe, in a VLA fashion,
// using SVE2.
//
// It is mostly a port-and-merge of the 128-bit Neon implementation herein and
//  a VLA risc-v implementation in https://github.com/dot-asm/cryptogams.
//
.globl	_poly1305_blocks_sve2
.private_extern	_poly1305_blocks_sve2

.align	5
_poly1305_blocks_sve2:
Lpoly1305_blocks_sve2:
	AARCH64_VALID_CALL_TARGET
	ldr	w17,[x0,#24]
	// Estimate vector width and branch to scalar if input too short
.long	0x04e0e3f0	//cntd x16					// vector width in 64-bit lanes (vl)
	lsl	x4,x16,#4				// vl * 16 (bytes per vector input blocks)
	add	x5,x4,x4,lsl #1	// 3 * vl * 16 - new threshold.
	cmp	x2,x5
	b.hs	Lblocks_sve2
	cbz	w17,Lshort_blocks	// Call scalar f-n if short; if in base 2^26 - proceed

Lblocks_sve2:
	AARCH64_SIGN_LINK_REGISTER
	stp	x29,x30,[sp,#-144]!		// Allowing for callee-saved reg-s
	add	x29,sp,#0

	//Store some callee-saved GPRs
	stp	x19,x20,[sp,#16]
	stp	x21,x22,[sp,#32]
	stp	x23,x24,[sp,#48]
	stp	x25,x26,[sp,#64]

	ands	x2,x2,#-16
	b.eq	Lno_data_sve2

	cbz	w17,Lbase2_64_sve2

	ldp	w10,w11,[x0]			// load hash value base 2^26
	ldp	w12,w13,[x0,#8]
	ldr	w14,[x0,#16]

	neg	x5,x4				// - (vl * 16)
	sub	x4,x4,#1			// (vl * 16) - 1
	and	x6,x2,x5			// x2 - (x2 % (vl * 16)) -> VLA length
	and	x8,x2,x4			// x2 % (vl * 16) -> scalar remainder
	cbz	x8,Leven_sve2		// If no scalar "head", proceed to VLA
	add	x7,x1,x8			// Pointer to the start of the VLA data
	stp	x6,x7,[sp,#-16]!	// Backup VLA length and ptr
	mov	x2,x8				// So that scalar part knows it's length

	add	x4,x10,x11,lsl#26		// base 2^26 -> base 2^64
	lsr	x5,x12,#12
	adds	x4,x4,x12,lsl#52
	add	x5,x5,x13,lsl#14
	adc	x5,x5,xzr
	lsr	x6,x14,#24
	adds	x5,x5,x14,lsl#40
	adc	x14,x6,xzr				// can be partially reduced...

	and	x10,x14,#-4				// ... so reduce
	and	x6,x14,#3
	add	x10,x10,x14,lsr#2
	adds	x4,x4,x10
	adcs	x5,x5,xzr
	adc	x6,x6,xzr

	stp	x4,x5,[x0]			// store hash value base 2^64
	str	x6,[x0,#16]

	bl	_poly1305_blocks			// Calculate the scalar "head"
	ldp	x2,x1,[sp],#16		// Recover updated length and input ptr
	ldr	x30,[sp,#8]

	cbz	x3,Lzero_padbit_sve2	// hash already stored in _poly1305_blocks

	ldp	x4,x5,[x0]			// load hash value base 2^64
	ldr	x6,[x0,#16]

	and	x10,x4,#0x03ffffff		// base 2^64 -> base 2^26
	ubfx	x11,x4,#26,#26
	extr	x12,x5,x4,#52
	and	x12,x12,#0x03ffffff
	ubfx	x13,x5,#14,#26
	extr	x14,x6,x5,#40

	cbnz	x2,Leven_sve2

	stp	w10,w11,[x0]			// store hash value base 2^26
	stp	w12,w13,[x0,#8]
	str	w14,[x0,#16]
	b	Lno_data_sve2

.align	4
Lzero_padbit_sve2:
	str	xzr,[x0,#24]
	b	Lno_data_sve2

.align	4
Lbase2_64_sve2:
	neg	x5,x4				// - (vl * 16)
	sub	x4,x4,#1			// (vl * 16) - 1
	and	x6,x2,x5			// x2 - (x2 % (vl * 16)) -> VLA length
	and	x8,x2,x4			// x2 % (vl * 16) -> scalar remainder
	cbz	x8,Linit_sve2		// If no scalar "head", proceed to VLA
	add	x7,x1,x8			// Pointer to the start of the VLA data
	stp	x6,x7,[sp,#-16]!	// Backup VLA length and ptr
	mov	x2,x8				// So that scalar part knows it's length
	bl	_poly1305_blocks			// Calculate the scalar "head"
	ldp	x2,x1,[sp],#16		// Recover updated length and input ptr

Linit_sve2:
	// Calculating and storing r-powers (powers of a key).
	// The layout of how r-powers are stored in memory:
	//////////////////////////////////////////////////////////////////////////////////////
	//                   lobe 1                           lobe 2                   etc. //
	//      | .. r^{max},r^{max/2},...,r^2,r | .. r^{max},r^{max/2},...,r^2,r | ..      //
	//     /                               /                               /         //
	//  [x0,48]                       [x0,48+28]                     [x0,48+56]   //
	//////////////////////////////////////////////////////////////////////////////////////

	ldr	w5,[x0,#28]		// Load top power (if exists - 0 by default)
	add	x25,x0,#48+28	// Point to the end of powers allocation (1st lobe)

	mov	x26,#-1
	lsr	x26,x26,#20		//2^44-1

	cbnz	w5,Lpwrs_precomputed

	ldp	x7,x8,[x0,#32]	// load key value

	lsr	x9,x8,#24			// base2_64 -> base2_44
	extr	x8,x8,x7,#44
	and	x7,x7,x26
	and	x8,x8,x26

	mov	x4,x16
	add	x5,x25,#-4
	bl	poly1305_sw_2_26

Loop_pwrs_sqr:
	lsr	x4,x4,#1
	add	x5,x5,#-4
	bl	poly1305_sqr_2_44
	bl	poly1305_sw_2_26
	cbnz	x4,Loop_pwrs_sqr

	sub	x5,x5,x25
	str	w5,[x0,#28]

Lpwrs_precomputed:
	ldp	x4,x5,[x0]		// load hash value base 2^64
	ldr	x6,[x0,#16]

	and	x10,x4,#0x03ffffff	// base 2^64 -> base 2^26
	ubfx	x11,x4,#26,#26
	extr	x12,x5,x4,#52
	and	x12,x12,#0x03ffffff
	ubfx	x13,x5,#14,#26
	extr	x14,x6,x5,#40

	stp	d8,d9,[sp,#80]		// meet ABI requirements
	stp	d10,d11,[sp,#96]
	stp	d12,d13,[sp,#112]
	stp	d14,d15,[sp,#128]

    // Zeroing H0-H4 registers
.long	0x04b83318	//eor z24.d,z24.d,z24.d  // H0
.long	0x04b93339	//eor z25.d,z25.d,z25.d  // H1
.long	0x04ba335a	//eor z26.d,z26.d,z26.d  // H2
.long	0x04bb337b	//eor z27.d,z27.d,z27.d  // H3
.long	0x04bc339c	//eor z28.d,z28.d,z28.d  // H4

	// Using Neon's fmov here for speed.
	//  We only need the low 26 bits in the first step so no need for post-mov reshuffle.
	fmov	d24,x10		// H0
	fmov	d25,x11		// H1
	fmov	d26,x12		// H2
	fmov	d27,x13		// H3
	fmov	d28,x14		// H4

	ldr	x30,[sp,#8]

	mov	x4,#1
	stur	w4,[x0,#24]		// set is_base2_26
	b	Ldo_sve2

.align	4
Leven_sve2:
	// In principle all this could be moved to Ldo_sve2
	stp	d8,d9,[sp,#80]		// meet ABI requirements
	stp	d10,d11,[sp,#96]
	stp	d12,d13,[sp,#112]
	stp	d14,d15,[sp,#128]

.long	0x04b83318	//eor z24.d,z24.d,z24.d  // H0
.long	0x04b93339	//eor z25.d,z25.d,z25.d  // H1
.long	0x04ba335a	//eor z26.d,z26.d,z26.d  // H2
.long	0x04bb337b	//eor z27.d,z27.d,z27.d  // H3
.long	0x04bc339c	//eor z28.d,z28.d,z28.d  // H4

	fmov	d24,x10		// H0
	fmov	d25,x11		// H1
	fmov	d26,x12		// H2
	fmov	d27,x13		// H3
	fmov	d28,x14		// H4

Ldo_sve2:
.long	0x2518e3e0	//ptrue p0.b, ALL               		// Set all-true predicate

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

	ldr	w5,[x0,#28]
	sxtw	x5,w5				// Zero-extend
	add	x25,x0,#48+28	// Pointer to the end of the r-powers 1st lobe
	add	x10,x0,#48+20		// Pointer to r^2.
	add	x25,x25,x5		// Pointer to the r^{max}

	mov	x15,#2
.long	0x25af1fe1	//whilelo p1.s,xzr,x15

	// If wouldn't need to load in two chunks, could use ld1rqw - 
	//  optimisation potential for 256-bit vector.
.long	0xa540a720	//ld1w { z0.s },p1/z,[x25]
.long	0xa540a55d	//ld1w { z29.s },p1/z,[x10]
	add	x25,x25,#28
	add	x10,x10,#28
.long	0x05bd6000	//zip1 z0.s,z0.s,z29.s

.long	0xa540a721	//ld1w { z1.s },p1/z,[x25]
.long	0xa540a55e	//ld1w { z30.s },p1/z,[x10]
	add	x25,x25,#28
	add	x10,x10,#28
.long	0x05be6021	//zip1 z1.s,z1.s,z30.s

.long	0xa540a723	//ld1w { z3.s },p1/z,[x25]
.long	0xa540a55d	//ld1w { z29.s },p1/z,[x10]
	add	x25,x25,#28
	add	x10,x10,#28
.long	0x05bd6063	//zip1 z3.s,z3.s,z29.s

.long	0xa540a725	//ld1w { z5.s },p1/z,[x25]
.long	0xa540a55e	//ld1w { z30.s },p1/z,[x10]
	add	x25,x25,#28
	add	x10,x10,#28
.long	0x05be60a5	//zip1 z5.s,z5.s,z30.s

.long	0xa540a727	//ld1w { z7.s },p1/z,[x25]
.long	0xa540a55d	//ld1w { z29.s },p1/z,[x10]
	sub	x25,x25,#104				// Adjust to 1st lobe, 3d power
.long	0x05bd60e7	//zip1 z7.s,z7.s,z29.s

	// Broadcast r-powers loaded above to higher parts of the R-vectors.
	cmp	x16,#2
	b.eq	L_skip_dup_broadcast
.long	0x05302000	//dup z0.q,z0.q[0]
.long	0x05302021	//dup z1.q,z1.q[0]
.long	0x05302063	//dup z3.q,z3.q[0]
.long	0x053020a5	//dup z5.q,z5.q[0]
.long	0x053020e7	//dup z7.q,z7.q[0]

L_skip_dup_broadcast:
	// Calculate S-vectors (r^x*5)
.long	0x04a1a822	//adr z2.s,[z1.s,z1.s,lsl #2]
.long	0x04a3a864	//adr z4.s,[z3.s,z3.s,lsl #2]
.long	0x04a5a8a6	//adr z6.s,[z5.s,z5.s,lsl #2]
.long	0x04a7a8e8	//adr z8.s,[z7.s,z7.s,lsl #2]

	// Load initial input blocks
	lsr	x15,x2,#4
.long	0x25af1fe1	//whilelo p1.s,xzr,x15					// Set predicate for blocks loading
	lsl	x3,x3,#24
.long	0xa560e429	//ld4w { z9.s-z12.s },p1/z,[x1]		// Loading all blocks at once

#ifdef  __AARCH64EB__
.long	0x05a48129	//revb z9.s,  p0/m, z9.s
.long	0x05a4814a	//revb z10.s, p0/m, z10.s
.long	0x05a4816b	//revb z11.s, p0/m, z11.s
.long	0x05a4818c	//revb z12.s, p0/m, z12.s
#endif

	// In-vector (VLA) conversion base2_64 -> base2_26.
.long	0x25b8dfff	//dup z31.s,#-1
.long	0x047a97ff	//lsr z31.s,z31.s,#6

.long	0x0472957d	//lsr z29.s,z11.s,#14		// T0 -> z11 >> 14
.long	0x0478958d	//lsr z13.s,z12.s,#8				// z13 -> l4
.long	0x046c9d6b	//lsl z11.s,z11.s,#12				// z11 -> upper part of l2
.long	0x04729d8c	//lsl z12.s,z12.s,#18				// z12 -> upper part of l3
.long	0x046c955e	//lsr z30.s,z10.s,#20		// T1 -> z10 >> 20
.long	0x047d318c	//orr z12.d,z12.d,z29.d		// z12 -> final l3
.long	0x04669d4a	//lsl z10.s,z10.s,#6				// z10 -> upper part of l1
.long	0x0466953d	//lsr z29.s,z9.s,#26		// T0 -> z9 >> 26
.long	0x043f3129	//and z9.d,z9.d,z31.d		// z9 is now final l0
.long	0x047e316b	//orr z11.d,z11.d,z30.d		// z11 -> final l2
.long	0x047d314a	//orr z10.d,z10.d,z29.d		// z10 -> final l1
.long	0x05a0387e	//dup z30.s,w3
.long	0x04bd33bd	//eor z29.d,z29.d,z29.d	// set zero mask
.long	0x047e31ad	//orr z13.d,z13.d,z30.d		// l4 += padbit
.long	0x043f318c	//and z12.d,z12.d,z31.d	// Mask l3
.long	0x043f316b	//and z11.d,z11.d,z31.d	// Mask l2
.long	0x043f314a	//and z10.d,z10.d,z31.d	// Mask l1


	// Move high blocks from INlo -> INhi and sparcify (put in even lanes)
.long	0x05bd652e	//zip2 z14.s,z9.s,z29.s
.long	0x05bd65b2	//zip2 z18.s,z13.s,z29.s
.long	0x05bd6591	//zip2 z17.s,z12.s,z29.s
.long	0x05bd6570	//zip2 z16.s,z11.s,z29.s
.long	0x05bd654f	//zip2 z15.s,z10.s,z29.s

	// Sparcify blocks to even lanes in INlo
.long	0x05bd6129	//zip1 z9.s,z9.s,z29.s
.long	0x05bd61ad	//zip1 z13.s,z13.s,z29.s
.long	0x05bd618c	//zip1 z12.s,z12.s,z29.s
.long	0x05bd616b	//zip1 z11.s,z11.s,z29.s
.long	0x05bd614a	//zip1 z10.s,z10.s,z29.s

	subs	x2,x2,x16,lsl #5		// By half vector width * 32

	b.ls	Lskip_loop_sve2

.align	4
Loop_sve2:
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ((inp[0]*r^{vl*2} + inp[vl]  *r^{vl} + inp[2*vl]  )*r^{vl} + inp[3*vl]  )*r^{vl}
	//+((inp[1]*r^{vl*2} + inp[vl+1]*r^{vl} + inp[2*vl+1])*r^{vl} + inp[3*vl+1])*r^{vl-1}
	//+...
	//   _______________________________/    _________________________________________/ 
	//      first main loop iteration                       long tail
	//
	// ((inp[0]*r^{vl*2} + inp[vl]  *r^{vl} + inp[2*vl]  )*r^{vl*2} + inp[3*vl]  *r^{vl} + inp[4*vl]  )*r^{vl}
	//+((inp[1]*r^{vl*2} + inp[vl+1]*r^{vl} + inp[2*vl+1])*r^{vl*2} + inp[3*vl+1]*r^{vl} + inp[4*vl+1])*r^{vl-1}
	//+...
	//   _______________________________/    ________________________________________/   ___________________/
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

	add	x1,x1,x16,lsl #5

.long	0x44f7d1d7	//umullb z23.d,z14.s,z7.s[2]
.long	0x44f5d1d6	//umullb z22.d,z14.s,z5.s[2]
.long	0x44f3d1d5	//umullb z21.d,z14.s,z3.s[2]
.long	0x44f1d1d4	//umullb z20.d,z14.s,z1.s[2]
.long	0x44f0d1d3	//umullb z19.d,z14.s,z0.s[2]

.long	0x44f591f7	//umlalb z23.d,z15.s,z5.s[2]
.long	0x44f391f6	//umlalb z22.d,z15.s,z3.s[2]
.long	0x44f191f5	//umlalb z21.d,z15.s,z1.s[2]
.long	0x44f091f4	//umlalb z20.d,z15.s,z0.s[2]
.long	0x44f891f3	//umlalb z19.d,z15.s,z8.s[2]

.long	0x44f39217	//umlalb z23.d,z16.s,z3.s[2]
.long	0x44f19216	//umlalb z22.d,z16.s,z1.s[2]
.long	0x44f09215	//umlalb z21.d,z16.s,z0.s[2]
.long	0x44f89214	//umlalb z20.d,z16.s,z8.s[2]
.long	0x44f69213	//umlalb z19.d,z16.s,z6.s[2]

.long	0x44f19237	//umlalb z23.d,z17.s,z1.s[2]
.long	0x44f09236	//umlalb z22.d,z17.s,z0.s[2]
.long	0x44f89235	//umlalb z21.d,z17.s,z8.s[2]
.long	0x44f69234	//umlalb z20.d,z17.s,z6.s[2]
.long	0x44f49233	//umlalb z19.d,z17.s,z4.s[2]

.long	0x04ba016b	//add z11.s,z11.s,z26.s
.long	0x44f09257	//umlalb z23.d,z18.s,z0.s[2]
.long	0x44f89256	//umlalb z22.d,z18.s,z8.s[2]
.long	0x44f69255	//umlalb z21.d,z18.s,z6.s[2]
.long	0x44f49254	//umlalb z20.d,z18.s,z4.s[2]
.long	0x44f29253	//umlalb z19.d,z18.s,z2.s[2]

	//////////////////////////////////////////////////////////////////////
	// (hash+inp[0:vl])*r^{vl*2} and accumulate
	// Interleave add+mul with loading and converting the next input batch

.long	0x04b80129	//add z9.s,z9.s,z24.s
	lsr	x15,x2,#4
.long	0x44e19176	//umlalb z22.d,z11.s,z1.s[0]
.long	0x25af1fe1	//whilelo p1.s,xzr,x15
.long	0x44e69173	//umlalb z19.d,z11.s,z6.s[0]
.long	0xa560e42e	//ld4w { z14.s-z17.s }, p1/z, [x1]
.long	0x44e39177	//umlalb z23.d,z11.s,z3.s[0]
.long	0x44e89174	//umlalb z20.d,z11.s,z8.s[0]
.long	0x44e09175	//umlalb z21.d,z11.s,z0.s[0]

#ifdef  __AARCH64EB__
.long	0x05a481ce	//revb z14.s, p0/m, z14.s
.long	0x05a481ef	//revb z15.s, p0/m, z15.s
.long	0x05a48210	//revb z16.s, p0/m, z16.s
.long	0x05a48231	//revb z17.s, p0/m, z17.s
#endif

.long	0x04b9014a	//add z10.s,z10.s,z25.s
.long	0x25b8dfff	//dup z31.s,#-1
.long	0x44e59136	//umlalb z22.d,z9.s,z5.s[0]
.long	0x047a97ff	//lsr z31.s,z31.s,#6
.long	0x44e79137	//umlalb z23.d,z9.s,z7.s[0]
.long	0x0472961d	//lsr z29.s,z16.s,#14		// T0 -> z16 >> 14
.long	0x44e39135	//umlalb z21.d,z9.s,z3.s[0]
.long	0x04789632	//lsr z18.s,z17.s,#8				// z18 -> l4
.long	0x44e09133	//umlalb z19.d,z9.s,z0.s[0]
.long	0x046c9e10	//lsl z16.s,z16.s,#12				// z16 -> upper part of l2
.long	0x44e19134	//umlalb z20.d,z9.s,z1.s[0]
.long	0x04729e31	//lsl z17.s,z17.s,#18				// z17 -> upper part of l3

.long	0x04bb018c	//add z12.s,z12.s,z27.s
.long	0x046c95fe	//lsr z30.s,z15.s,#20		// T1 -> z15 >> 20
.long	0x44e39156	//umlalb z22.d,z10.s,z3.s[0]
.long	0x047d3231	//orr z17.d,z17.d,z29.d		// z17 -> final l3
.long	0x44e59157	//umlalb z23.d,z10.s,z5.s[0]
.long	0x04669def	//lsl z15.s,z15.s,#6				// z15 -> upper part of l1
.long	0x44e89153	//umlalb z19.d,z10.s,z8.s[0]
.long	0x046695dd	//lsr z29.s,z14.s,#26		// T0 -> z14 >> 26
.long	0x44e19155	//umlalb z21.d,z10.s,z1.s[0]
.long	0x043f31ce	//and z14.d,z14.d,z31.d	// z14 is now final l0
.long	0x44e09154	//umlalb z20.d,z10.s,z0.s[0]
.long	0x047e3210	//orr z16.d,z16.d,z30.d		// z16 -> final l2

.long	0x04bc01ad	//add z13.s,z13.s,z28.s
.long	0x047d31ef	//orr z15.d,z15.d,z29.d		// z15 -> final l1
.long	0x44e09196	//umlalb z22.d,z12.s,z0.s[0]
.long	0x05a0387e	//dup z30.s,w3
.long	0x44e49193	//umlalb z19.d,z12.s,z4.s[0]
.long	0x04bd33bd	//eor z29.d,z29.d,z29.d	// set zero mask
.long	0x44e19197	//umlalb z23.d,z12.s,z1.s[0]
.long	0x047e3252	//orr z18.d,z18.d,z30.d		// l4 += padbit
.long	0x44e69194	//umlalb z20.d,z12.s,z6.s[0]
.long	0x043f3231	//and z17.d,z17.d,z31.d	// Mask l3
.long	0x44e89195	//umlalb z21.d,z12.s,z8.s[0]
.long	0x043f3210	//and z16.d,z16.d,z31.d	// Mask l2

.long	0x44e891b6	//umlalb z22.d,z13.s,z8.s[0]
.long	0x043f31ef	//and z15.d,z15.d,z31.d	// Mask l1
.long	0x44e291b3	//umlalb z19.d,z13.s,z2.s[0]
.long	0x05bd61c9	//zip1 z9.s,z14.s,z29.s
.long	0x44e091b7	//umlalb z23.d,z13.s,z0.s[0]
.long	0x05bd61ea	//zip1 z10.s,z15.s,z29.s
.long	0x44e491b4	//umlalb z20.d,z13.s,z4.s[0]
.long	0x05bd620b	//zip1 z11.s,z16.s,z29.s
.long	0x44e691b5	//umlalb z21.d,z13.s,z6.s[0]
.long	0x05bd622c	//zip1 z12.s,z17.s,z29.s
.long	0x05bd624d	//zip1 z13.s,z18.s,z29.s

	// Sparcify blocks to even lanes in INlo
.long	0x05bd65ce	//zip2 z14.s,z14.s,z29.s
.long	0x05bd65ef	//zip2 z15.s,z15.s,z29.s
.long	0x05bd6610	//zip2 z16.s,z16.s,z29.s
.long	0x05bd6631	//zip2 z17.s,z17.s,z29.s
.long	0x05bd6652	//zip2 z18.s,z18.s,z29.s

	subs	x2,x2,x16,lsl #5

	// Lazy reduction
	bl	poly1305_lazy_reduce_sve2
	ldr	x30,[sp,#8]

	b.hi	Loop_sve2

Lskip_loop_sve2:

	adds	x2,x2,x16,lsl #4		// By half the usual input size
	b.eq	Lshort_tail_sve2

Long_tail_sve2:
	////////////////////////////////////////////////////////////////
	// (hash + inp[lo])*r^{vl} + inp[hi])*r^{vl..1}               //
	//  ____________________/                                    //
	//  first part of long tail                                   //
	////////////////////////////////////////////////////////////////
	//NB `vl` here (and in the code) is the vector length in double words.
	// Intereaving algebra with copying INhi -> INlo for the next steps.

.long	0x04ba016b	//add z11.s,z11.s,z26.s
.long	0x04b80129	//add z9.s,z9.s,z24.s
.long	0x04b9014a	//add z10.s,z10.s,z25.s
.long	0x04bb018c	//add z12.s,z12.s,z27.s
.long	0x04bc01ad	//add z13.s,z13.s,z28.s

.long	0x44f1d176	//umullb z22.d,z11.s,z1.s[2]
.long	0x44f6d173	//umullb z19.d,z11.s,z6.s[2]
.long	0x44f3d177	//umullb z23.d,z11.s,z3.s[2]
.long	0x44f8d174	//umullb z20.d,z11.s,z8.s[2]
.long	0x44f0d175	//umullb z21.d,z11.s,z0.s[2]

.long	0x44f59136	//umlalb z22.d,z9.s,z5.s[2]
.long	0x44f79137	//umlalb z23.d,z9.s,z7.s[2]
.long	0x44f39135	//umlalb z21.d,z9.s,z3.s[2]
.long	0x44f09133	//umlalb z19.d,z9.s,z0.s[2]
.long	0x44f19134	//umlalb z20.d,z9.s,z1.s[2]
.long	0x0470320b	//mov z11.d,z16.d

.long	0x44f39156	//umlalb z22.d,z10.s,z3.s[2]
.long	0x44f59157	//umlalb z23.d,z10.s,z5.s[2]
.long	0x44f89153	//umlalb z19.d,z10.s,z8.s[2]
.long	0x44f19155	//umlalb z21.d,z10.s,z1.s[2]
.long	0x44f09154	//umlalb z20.d,z10.s,z0.s[2]
.long	0x046e31c9	//mov z9.d,z14.d

.long	0x44f09196	//umlalb z22.d,z12.s,z0.s[2]
.long	0x44f49193	//umlalb z19.d,z12.s,z4.s[2]
.long	0x44f19197	//umlalb z23.d,z12.s,z1.s[2]
.long	0x44f69194	//umlalb z20.d,z12.s,z6.s[2]
.long	0x44f89195	//umlalb z21.d,z12.s,z8.s[2]
.long	0x046f31ea	//mov z10.d,z15.d

.long	0x44f891b6	//umlalb z22.d,z13.s,z8.s[2]
.long	0x44f291b3	//umlalb z19.d,z13.s,z2.s[2]
.long	0x44f091b7	//umlalb z23.d,z13.s,z0.s[2]
.long	0x44f491b4	//umlalb z20.d,z13.s,z4.s[2]
.long	0x44f691b5	//umlalb z21.d,z13.s,z6.s[2]
.long	0x0471322c	//mov z12.d,z17.d

	// Lazy reduction
	bl	poly1305_lazy_reduce_sve2
	ldr	x30,[sp,#8]

.long	0x0472324d	//mov z13.d,z18.d

Lshort_tail_sve2:

	cmp	x16, #2
	b.ls	Last_reduce_sve2

	mov	x15,#1
.long	0x25af1fe1	//whilelo p1.s,xzr,x15

Loop_reduce_sve2:
	////////////////////////////////////////////////////////////////
	// (hash + inp[hi])*r^{vl/2..2}                               //
	//       ____________________/                               //
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
	ldr	w10,[x25]
	ldr	w11,[x25,#28]
	ldr	w12,[x25,#56]
.long	0x05a8a540	//cpy z0.s,p1/m,w10
	ldr	w13,[x25,#84]
.long	0x05a8a561	//cpy z1.s,p1/m,w11
.long	0x05302000	//dup z0.q,z0.q[0]
	ldr	w14,[x25,#112]
.long	0x05a8a583	//cpy z3.s,p1/m,w12
.long	0x05302021	//dup z1.q,z1.q[0]
.long	0x05a8a5a5	//cpy z5.s,p1/m,w13
.long	0x05302063	//dup z3.q,z3.q[0]
.long	0x05a8a5c7	//cpy z7.s,p1/m,w14
	add	x25,x25,#4			// Increment pointer for the next iteration
.long	0x053020a5	//dup z5.q,z5.q[0]
.long	0x053020e7	//dup z7.q,z7.q[0]

	// Interleaved hash contraction and S-vector calc.
.long	0x04ba016b	//add z11.s,z11.s,z26.s
.long	0x04a1a822	//adr z2.s,[z1.s,z1.s,lsl #2]
.long	0x04b80129	//add z9.s,z9.s,z24.s
.long	0x04a3a864	//adr z4.s,[z3.s,z3.s,lsl #2]
.long	0x04b9014a	//add z10.s,z10.s,z25.s
.long	0x04a5a8a6	//adr z6.s,[z5.s,z5.s,lsl #2]
.long	0x04bb018c	//add z12.s,z12.s,z27.s
.long	0x04a7a8e8	//adr z8.s,[z7.s,z7.s,lsl #2]
.long	0x04bc01ad	//add z13.s,z13.s,z28.s

.long	0x44e5d136	//umullb z22.d,z9.s,z5.s[0]
.long	0x44e7d137	//umullb z23.d,z9.s,z7.s[0]
.long	0x44e3d135	//umullb z21.d,z9.s,z3.s[0]
.long	0x44e0d133	//umullb z19.d,z9.s,z0.s[0]
.long	0x44e1d134	//umullb z20.d,z9.s,z1.s[0]

.long	0x44e39156	//umlalb z22.d,z10.s,z3.s[0]
.long	0x44e59157	//umlalb z23.d,z10.s,z5.s[0]
.long	0x44e89153	//umlalb z19.d,z10.s,z8.s[0]
.long	0x44e19155	//umlalb z21.d,z10.s,z1.s[0]
.long	0x44e09154	//umlalb z20.d,z10.s,z0.s[0]

.long	0x44e19176	//umlalb z22.d,z11.s,z1.s[0]
.long	0x44e69173	//umlalb z19.d,z11.s,z6.s[0]
.long	0x44e39177	//umlalb z23.d,z11.s,z3.s[0]
.long	0x44e89174	//umlalb z20.d,z11.s,z8.s[0]
.long	0x44e09175	//umlalb z21.d,z11.s,z0.s[0]

.long	0x44e09196	//umlalb z22.d,z12.s,z0.s[0]
.long	0x44e49193	//umlalb z19.d,z12.s,z4.s[0]
.long	0x44e19197	//umlalb z23.d,z12.s,z1.s[0]
.long	0x44e69194	//umlalb z20.d,z12.s,z6.s[0]
.long	0x44e89195	//umlalb z21.d,z12.s,z8.s[0]

.long	0x44e891b6	//umlalb z22.d,z13.s,z8.s[0]
.long	0x44e291b3	//umlalb z19.d,z13.s,z2.s[0]
.long	0x44e091b7	//umlalb z23.d,z13.s,z0.s[0]
.long	0x44e491b4	//umlalb z20.d,z13.s,z4.s[0]
.long	0x44e691b5	//umlalb z21.d,z13.s,z6.s[0]

	// Lazy reduction
	bl	poly1305_lazy_reduce_sve2
	ldr	x30,[sp,#8]

	// Move higher part of vectors to lower part, depending on current vl
	// NB look-up is done in terms of single-word lanes, hence indices
	//  start from vl (refer to as w16) and not vl/2
	// Higher part now contains "junk"
.long	0x04a1461d	//index z29.s,w16,#1
.long	0x05bd3129	//tbl z9.s,z9.s,z29.s
.long	0x05bd314a	//tbl z10.s,z10.s,z29.s
.long	0x05bd316b	//tbl z11.s,z11.s,z29.s
.long	0x05bd318c	//tbl z12.s,z12.s,z29.s
.long	0x05bd31ad	//tbl z13.s,z13.s,z29.s
	lsr	x16,x16,#1		// vl /= 2
	cmp	x16,#2
	b.hi	Loop_reduce_sve2

Last_reduce_sve2:
	////////////////////////////////////////////////////////////////
	// (hash + inp[n-1])*r^2                                      //
	//+(hash + inp[n]  )*r                                        //
	//       _____________/                                      //
	//  Final part of the short tail                              //
	////////////////////////////////////////////////////////////////

	//Last hash addition - now everything stored in SVE_Hx
.long	0x04ab035a	//add z26.s,z26.s,z11.s
.long	0x04a90318	//add z24.s,z24.s,z9.s
.long	0x04aa0339	//add z25.s,z25.s,z10.s
.long	0x04ac037b	//add z27.s,z27.s,z12.s
.long	0x04ad039c	//add z28.s,z28.s,z13.s

	// Shift even lanes to odd lanes and set even to zero
	//  because r^2 and r^1 are in lanes 1 and 3 of R-vectors
.long	0x05ba73fa	//trn1 z26.s,z31.s,z26.s
.long	0x05b873f8	//trn1 z24.s,z31.s,z24.s
.long	0x05b973f9	//trn1 z25.s,z31.s,z25.s
.long	0x05bb73fb	//trn1 z27.s,z31.s,z27.s
.long	0x05bc73fc	//trn1 z28.s,z31.s,z28.s

.long	0x45c17f56	//umullt z22.d,z26.s,z1.s
.long	0x45c67f53	//umullt z19.d,z26.s,z6.s
.long	0x45c37f57	//umullt z23.d,z26.s,z3.s
.long	0x45c87f54	//umullt z20.d,z26.s,z8.s
.long	0x45c07f55	//umullt z21.d,z26.s,z0.s

.long	0x44c54f16	//umlalt z22.d,z24.s,z5.s
.long	0x44c74f17	//umlalt z23.d,z24.s,z7.s
.long	0x44c34f15	//umlalt z21.d,z24.s,z3.s
.long	0x44c04f13	//umlalt z19.d,z24.s,z0.s
.long	0x44c14f14	//umlalt z20.d,z24.s,z1.s

.long	0x44c34f36	//umlalt z22.d,z25.s,z3.s
.long	0x44c54f37	//umlalt z23.d,z25.s,z5.s
.long	0x44c84f33	//umlalt z19.d,z25.s,z8.s
.long	0x44c14f35	//umlalt z21.d,z25.s,z1.s
.long	0x44c04f34	//umlalt z20.d,z25.s,z0.s

.long	0x44c04f76	//umlalt z22.d,z27.s,z0.s
.long	0x44c44f73	//umlalt z19.d,z27.s,z4.s
.long	0x44c14f77	//umlalt z23.d,z27.s,z1.s
.long	0x44c64f74	//umlalt z20.d,z27.s,z6.s
.long	0x44c84f75	//umlalt z21.d,z27.s,z8.s

.long	0x44c84f96	//umlalt z22.d,z28.s,z8.s
.long	0x44c24f93	//umlalt z19.d,z28.s,z2.s
.long	0x44c04f97	//umlalt z23.d,z28.s,z0.s
.long	0x44c44f94	//umlalt z20.d,z28.s,z4.s
.long	0x44c64f95	//umlalt z21.d,z28.s,z6.s

	// Generate predicate for the last two double words
	mov	x15,#2
.long	0x25ef1fe2	//whilelo p2.d,xzr,x15

.long	0x25f8dfff	//dup z31.d,#-1
.long	0x04ba97ff	//lsr z31.d,z31.d,#38

	////////////////////////////////////////////////////////////////
	// horizontal add

	//In Neon implementation, one effectively using lower 64 bits of vector registers here.
	//Here and below I use hard-coded FP registers.

.long	0x04c12ad6	//uaddv d22,p2,z22.d
	ldp	d8,d9,[sp,#80]		// meet ABI requirements
.long	0x04c12a73	//uaddv d19,p2,z19.d
	ldp	d10,d11,[sp,#96]
.long	0x04c12af7	//uaddv d23,p2,z23.d
	ldp	d12,d13,[sp,#112]
.long	0x04c12a94	//uaddv d20,p2,z20.d
	ldp	d14,d15,[sp,#128]
.long	0x04c12ab5	//uaddv d21,p2,z21.d

	////////////////////////////////////////////////////////////////
	// Lazy reduction, but without narrowing

	// Since results were accumulated in the lower 64 bits,
	//  one can refer to them as FP/aSIMD reg-s.

	ushr	d29,d22,#26
	and	v22.8b,v22.8b,v31.8b
	ushr	d30,d19,#26
	and	v19.8b,v19.8b,v31.8b

	add	d23,d23,d29				// h3 -> h4
	add	d20,d20,d30				// h0 -> h1

	ushr	d29,d23,#26
	and	v23.8b,v23.8b,v31.8b
	ushr	d30,d20,#26
	and	v20.8b,v20.8b,v31.8b
	add	d21,d21,d30				// h1 -> h2

	add	d19,d19,d29
	shl	d29,d29,#2
	ushr	d30,d21,#26
	and	v21.8b,v21.8b,v31.8b
	add	d19,d19,d29				// h4 -> h0
	add	d22,d22,d30				// h2 -> h3

	ushr	d29,d19,#26
	and	v19.8b,v19.8b,v31.8b
	ushr	d30,d22,#26
	and	v22.8b,v22.8b,v31.8b
	add	d20,d20,d29				// h0 -> h1
	add	d23,d23,d30				// h3 -> h4

	////////////////////////////////////////////////////////////////
	// write the result, can be partially reduced

	stp	s19,s20,[x0],#8
	stp	s21,s22,[x0],#8
	str	s23,[x0]

Lno_data_sve2:
	// Restore the callee-saved GPRs
	ldp	x19,x20,[sp,#16]
	ldp	x21,x22,[sp,#32]
	ldp	x23,x24,[sp,#48]
	ldp	x25,x26,[sp,#64]
	ldr	x29,[sp],#144
	AARCH64_VALIDATE_LINK_REGISTER
	ret

Lshort_blocks:
	b	_poly1305_blocks


