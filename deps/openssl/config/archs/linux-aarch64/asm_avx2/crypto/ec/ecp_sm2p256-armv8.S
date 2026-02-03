#include "arm_arch.h"
.arch	armv8-a
.section	.rodata

.align	5
// The polynomial p
.Lpoly:
.quad	0xffffffffffffffff,0xffffffff00000000,0xffffffffffffffff,0xfffffffeffffffff
// The order of polynomial n
.Lord:
.quad	0x53bbf40939d54123,0x7203df6b21c6052b,0xffffffffffffffff,0xfffffffeffffffff
// (p + 1) / 2
.Lpoly_div_2:
.quad	0x8000000000000000,0xffffffff80000000,0xffffffffffffffff,0x7fffffff7fffffff
// (n + 1) / 2
.Lord_div_2:
.quad	0xa9ddfa049ceaa092,0xb901efb590e30295,0xffffffffffffffff,0x7fffffff7fffffff

.text

// void bn_rshift1(BN_ULONG *a);
.globl	bn_rshift1
.type	bn_rshift1,%function
.align	5
bn_rshift1:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x0]
	ldp	x9,x10,[x0,#16]

	// Right shift
	extr	x7,x8,x7,#1
	extr	x8,x9,x8,#1
	extr	x9,x10,x9,#1
	lsr	x10,x10,#1

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]

	ret
.size	bn_rshift1,.-bn_rshift1

// void bn_sub(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b);
.globl	bn_sub
.type	bn_sub,%function
.align	5
bn_sub:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Subtraction
	subs	x7,x7,x11
	sbcs	x8,x8,x12
	sbcs	x9,x9,x13
	sbc	x10,x10,x14

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]

	ret
.size	bn_sub,.-bn_sub

// void ecp_sm2p256_div_by_2(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_div_by_2
.type	ecp_sm2p256_div_by_2,%function
.align	5
ecp_sm2p256_div_by_2:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]

	// Save the least significant bit
	mov	x3,x7

	// Right shift 1
	extr	x7,x8,x7,#1
	extr	x8,x9,x8,#1
	extr	x9,x10,x9,#1
	lsr	x10,x10,#1

	// Load mod
	adrp	x2,.Lpoly_div_2
	add	x2,x2,#:lo12:.Lpoly_div_2
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Parity check
	tst	x3,#1
	csel	x11,xzr,x11,eq
	csel	x12,xzr,x12,eq
	csel	x13,xzr,x13,eq
	csel	x14,xzr,x14,eq

	// Add
	adds	x7,x7,x11
	adcs	x8,x8,x12
	adcs	x9,x9,x13
	adc	x10,x10,x14

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]
	ret
.size	ecp_sm2p256_div_by_2,.-ecp_sm2p256_div_by_2

// void ecp_sm2p256_div_by_2_mod_ord(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_div_by_2_mod_ord
.type	ecp_sm2p256_div_by_2_mod_ord,%function
.align	5
ecp_sm2p256_div_by_2_mod_ord:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]

	// Save the least significant bit
	mov	x3,x7

	// Right shift 1
	extr	x7,x8,x7,#1
	extr	x8,x9,x8,#1
	extr	x9,x10,x9,#1
	lsr	x10,x10,#1

	// Load mod
	adrp	x2,.Lord_div_2
	add	x2,x2,#:lo12:.Lord_div_2
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Parity check
	tst	x3,#1
	csel	x11,xzr,x11,eq
	csel	x12,xzr,x12,eq
	csel	x13,xzr,x13,eq
	csel	x14,xzr,x14,eq

	// Add
	adds	x7,x7,x11
	adcs	x8,x8,x12
	adcs	x9,x9,x13
	adc	x10,x10,x14

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]
	ret
.size	ecp_sm2p256_div_by_2_mod_ord,.-ecp_sm2p256_div_by_2_mod_ord

// void ecp_sm2p256_mul_by_3(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_mul_by_3
.type	ecp_sm2p256_mul_by_3,%function
.align	5
ecp_sm2p256_mul_by_3:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]

	// 2*a
	adds	x7,x7,x7
	adcs	x8,x8,x8
	adcs	x9,x9,x9
	adcs	x10,x10,x10
	adcs	x15,xzr,xzr

	mov	x3,x7
	mov	x4,x8
	mov	x5,x9
	mov	x6,x10

	// Sub polynomial
	adrp	x2,.Lpoly
	add	x2,x2,#:lo12:.Lpoly
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]
	subs	x7,x7,x11
	sbcs	x8,x8,x12
	sbcs	x9,x9,x13
	sbcs	x10,x10,x14
	sbcs	x15,x15,xzr

	csel	x7,x7,x3,cs
	csel	x8,x8,x4,cs
	csel	x9,x9,x5,cs
	csel	x10,x10,x6,cs
	eor	x15,x15,x15

	// 3*a
	ldp	x11,x12,[x1]
	ldp	x13,x14,[x1,#16]
	adds	x7,x7,x11
	adcs	x8,x8,x12
	adcs	x9,x9,x13
	adcs	x10,x10,x14
	adcs	x15,xzr,xzr

	mov	x3,x7
	mov	x4,x8
	mov	x5,x9
	mov	x6,x10

	// Sub polynomial
	adrp	x2,.Lpoly
	add	x2,x2,#:lo12:.Lpoly
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]
	subs	x7,x7,x11
	sbcs	x8,x8,x12
	sbcs	x9,x9,x13
	sbcs	x10,x10,x14
	sbcs	x15,x15,xzr

	csel	x7,x7,x3,cs
	csel	x8,x8,x4,cs
	csel	x9,x9,x5,cs
	csel	x10,x10,x6,cs

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]

	ret
.size	ecp_sm2p256_mul_by_3,.-ecp_sm2p256_mul_by_3

// void ecp_sm2p256_add(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_add
.type	ecp_sm2p256_add,%function
.align	5
ecp_sm2p256_add:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Addition
	adds	x7,x7,x11
	adcs	x8,x8,x12
	adcs	x9,x9,x13
	adcs	x10,x10,x14
	adc	x15,xzr,xzr

	// Load polynomial
	adrp	x2,.Lpoly
	add	x2,x2,#:lo12:.Lpoly
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Backup Addition
	mov	x3,x7
	mov	x4,x8
	mov	x5,x9
	mov	x6,x10

	// Sub polynomial
	subs	x3,x3,x11
	sbcs	x4,x4,x12
	sbcs	x5,x5,x13
	sbcs	x6,x6,x14
	sbcs	x15,x15,xzr

	// Select based on carry
	csel	x7,x7,x3,cc
	csel	x8,x8,x4,cc
	csel	x9,x9,x5,cc
	csel	x10,x10,x6,cc

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]
	ret
.size	ecp_sm2p256_add,.-ecp_sm2p256_add

// void ecp_sm2p256_sub(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_sub
.type	ecp_sm2p256_sub,%function
.align	5
ecp_sm2p256_sub:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Subtraction
	subs	x7,x7,x11
	sbcs	x8,x8,x12
	sbcs	x9,x9,x13
	sbcs	x10,x10,x14
	sbc	x15,xzr,xzr

	// Load polynomial
	adrp	x2,.Lpoly
	add	x2,x2,#:lo12:.Lpoly
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Backup subtraction
	mov	x3,x7
	mov	x4,x8
	mov	x5,x9
	mov	x6,x10

	// Add polynomial
	adds	x3,x3,x11
	adcs	x4,x4,x12
	adcs	x5,x5,x13
	adcs	x6,x6,x14
	tst	x15,x15

	// Select based on carry
	csel	x7,x7,x3,eq
	csel	x8,x8,x4,eq
	csel	x9,x9,x5,eq
	csel	x10,x10,x6,eq

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]
	ret
.size	ecp_sm2p256_sub,.-ecp_sm2p256_sub

// void ecp_sm2p256_sub_mod_ord(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_sub_mod_ord
.type	ecp_sm2p256_sub_mod_ord,%function
.align	5
ecp_sm2p256_sub_mod_ord:
	AARCH64_VALID_CALL_TARGET
	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Subtraction
	subs	x7,x7,x11
	sbcs	x8,x8,x12
	sbcs	x9,x9,x13
	sbcs	x10,x10,x14
	sbc	x15,xzr,xzr

	// Load polynomial
	adrp	x2,.Lord
	add	x2,x2,#:lo12:.Lord
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

	// Backup subtraction
	mov	x3,x7
	mov	x4,x8
	mov	x5,x9
	mov	x6,x10

	// Add polynomial
	adds	x3,x3,x11
	adcs	x4,x4,x12
	adcs	x5,x5,x13
	adcs	x6,x6,x14
	tst	x15,x15

	// Select based on carry
	csel	x7,x7,x3,eq
	csel	x8,x8,x4,eq
	csel	x9,x9,x5,eq
	csel	x10,x10,x6,eq

	// Store results
	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]
	ret
.size	ecp_sm2p256_sub_mod_ord,.-ecp_sm2p256_sub_mod_ord

.macro	RDC
	// a = |  s7   | ... | s0  |, where si are 64-bit quantities
	//   = |a15|a14| ... |a1|a0|, where ai are 32-bit quantities
	// |    s7     |    s6     |    s5     |    s4     |
	// | a15 | a14 | a13 | a12 | a11 | a10 | a9  | a8  |
	// |    s3     |    s2     |    s1     |    s0     |
	// | a7  | a6  | a5  | a4  | a3  | a2  | a1  | a0  |
	// =================================================
	// | a8  | a11 | a10 | a9  | a8  |   0 |    s4     | (+)
	// | a9  | a15 |    s6     | a11 |   0 | a10 | a9  | (+)
	// | a10 |   0 | a14 | a13 | a12 |   0 |    s5     | (+)
	// | a11 |   0 |    s7     | a13 |   0 | a12 | a11 | (+)
	// | a12 |   0 |    s7     | a13 |   0 |    s6     | (+)
	// | a12 |   0 |   0 | a15 | a14 |   0 | a14 | a13 | (+)
	// | a13 |   0 |   0 |   0 | a15 |   0 | a14 | a13 | (+)
	// | a13 |   0 |   0 |   0 |   0 |   0 |    s7     | (+)
	// | a14 |   0 |   0 |   0 |   0 |   0 |    s7     | (+)
	// | a14 |   0 |   0 |   0 |   0 |   0 |   0 | a15 | (+)
	// | a15 |   0 |   0 |   0 |   0 |   0 |   0 | a15 | (+)
	// | a15 |   0 |   0 |   0 |   0 |   0 |   0 |   0 | (+)
	// |    s7     |   0 |   0 |   0 |   0 |   0 |   0 | (+)
	// |   0 |   0 |   0 |   0 |   0 | a8  |   0 |   0 | (-)
	// |   0 |   0 |   0 |   0 |   0 | a9  |   0 |   0 | (-)
	// |   0 |   0 |   0 |   0 |   0 | a13 |   0 |   0 | (-)
	// |   0 |   0 |   0 |   0 |   0 | a14 |   0 |   0 | (-)
	// | U[7]| U[6]| U[5]| U[4]| U[3]| U[2]| U[1]| U[0]|
	// |    V[3]   |    V[2]   |    V[1]   |    V[0]   |

	// 1. 64-bit addition
	// t2=s6+s7+s7
	adds	x5,x13,x14
	adcs	x4,xzr,xzr
	adds	x5,x5,x14
	adcs	x4,x4,xzr
	// t3=s4+s5+t2
	adds	x6,x11,x5
	adcs	x15,x4,xzr
	adds	x6,x6,x12
	adcs	x15,x15,xzr
	// sum
	adds	x7,x7,x6
	adcs	x8,x8,x15
	adcs	x9,x9,x5
	adcs	x10,x10,x14
	adcs	x3,xzr,xzr
	adds	x10,x10,x4
	adcs	x3,x3,xzr

	stp	x7,x8,[sp,#32]
	stp	x9,x10,[sp,#48]

	// 2. 64-bit to 32-bit spread
	mov	x4,#0xffffffff
	mov	x7,x11
	mov	x8,x12
	mov	x9,x13
	mov	x10,x14
	and	x7,x7,x4 // a8
	and	x8,x8,x4 // a10
	and	x9,x9,x4 // a12
	and	x10,x10,x4 // a14
	lsr	x11,x11,#32 // a9
	lsr	x12,x12,#32 // a11
	lsr	x13,x13,#32 // a13
	lsr	x14,x14,#32 // a15

	// 3. 32-bit addition
	add	x4,x10,x9  // t1 <- a12 + a14
	add	x5,x14,x13  // t2 <- a13 + a15
	add	x6,x7,x11    // t3 <- a8 + a9
	add	x15,x10,x8  // t4 <- a10 + a14
	add	x14,x14,x12 // a15 <- a11 + a15
	add	x9,x5,x4   // a12 <- a12 + a13 + a14 + a15
	add	x8,x8,x9 // a10 <- a10 + a12 + a13 + a14 + a15
	add	x8,x8,x9 // a10 <- a10 + 2*(a12 + a13 + a14 + a15)
	add	x8,x8,x6  // a10 <- a8 + a9 + a10 + 2*(a12 + a13 + a14 + a15)
	add	x8,x8,x12 // a10 <- a8 + a9 + a10 + a11 + 2*(a12 + a13 + a14 + a15)
	add	x9,x9,x13 // a12 <- a12 + 2*a13 + a14 + a15
	add	x9,x9,x12 // a12 <- a11 + a12 + 2*a13 + a14 + a15
	add	x9,x9,x7  // a12 <- a8 + a11 + a12 + 2*a13 + a14 + a15
	add	x6,x6,x10   // t3 <- a8 + a9 + a14
	add	x6,x6,x13   // t3 <- a8 + a9 + a13 + a14
	add	x11,x11,x5    // a9 <- a9 + a13 + a15
	add	x12,x12,x11  // a11 <- a9 + a11 + a13 + a15
	add	x12,x12,x5  // a11 <- a9 + a11 + 2*(a13 + a15)
	add	x4,x4,x15    // t1 <- a10 + a12 + 2*a14

	// U[0]  s5	a9 + a11 + 2*(a13 + a15)
	// U[1]  t1	a10 + a12 + 2*a14
	// U[2] -t3	a8 + a9 + a13 + a14
	// U[3]  s2	a8 + a11 + a12 + 2*a13 + a14 + a15
	// U[4]  s4	a9 + a13 + a15
	// U[5]  t4	a10 + a14
	// U[6]  s7	a11 + a15
	// U[7]  s1	a8 + a9 + a10 + a11 + 2*(a12 + a13 + a14 + a15)

	// 4. 32-bit to 64-bit
	lsl	x7,x4,#32
	extr	x4,x9,x4,#32
	extr	x9,x15,x9,#32
	extr	x15,x8,x15,#32
	lsr	x8,x8,#32

	// 5. 64-bit addition
	adds	x12,x12,x7
	adcs	x4,x4,xzr
	adcs	x11,x11,x9
	adcs	x14,x14,x15
	adcs	x3,x3,x8

	// V[0]	s5
	// V[1]	t1
	// V[2]	s4
	// V[3]	s7
	// carry	t0
	// sub	t3

	// 5. Process s0-s3
	ldp	x7,x8,[sp,#32]
	ldp	x9,x10,[sp,#48]
	// add with V0-V3
	adds	x7,x7,x12
	adcs	x8,x8,x4
	adcs	x9,x9,x11
	adcs	x10,x10,x14
	adcs	x3,x3,xzr
	// sub with t3
	subs	x8,x8,x6
	sbcs	x9,x9,xzr
	sbcs	x10,x10,xzr
	sbcs	x3,x3,xzr

	// 6. MOD
	// First Mod
	lsl	x4,x3,#32
	subs	x5,x4,x3

	adds	x7,x7,x3
	adcs	x8,x8,x5
	adcs	x9,x9,xzr
	adcs	x10,x10,x4

	// Last Mod
	// return y - p if y > p else y
	mov	x11,x7
	mov	x12,x8
	mov	x13,x9
	mov	x14,x10

	adrp	x3,.Lpoly
	add	x3,x3,#:lo12:.Lpoly
	ldp	x4,x5,[x3]
	ldp	x6,x15,[x3,#16]

	adcs	x16,xzr,xzr

	subs	x7,x7,x4
	sbcs	x8,x8,x5
	sbcs	x9,x9,x6
	sbcs	x10,x10,x15
	sbcs	x16,x16,xzr

	csel	x7,x7,x11,cs
	csel	x8,x8,x12,cs
	csel	x9,x9,x13,cs
	csel	x10,x10,x14,cs

.endm

// void ecp_sm2p256_mul(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b);
.globl	ecp_sm2p256_mul
.type	ecp_sm2p256_mul,%function
.align	5
ecp_sm2p256_mul:
	AARCH64_SIGN_LINK_REGISTER
	// Store scalar registers
	stp	x29,x30,[sp,#-80]!
	add	x29,sp,#0
	stp	x16,x17,[sp,#16]
	stp	x19,x20,[sp,#64]

	// Load inputs
	ldp	x7,x8,[x1]
	ldp	x9,x10,[x1,#16]
	ldp	x11,x12,[x2]
	ldp	x13,x14,[x2,#16]

// ### multiplication ###
	// ========================
	//             s3 s2 s1 s0
	// *           s7 s6 s5 s4
	// ------------------------
	// +           s0 s0 s0 s0
	//              *  *  *  *
	//             s7 s6 s5 s4
	//          s1 s1 s1 s1
	//           *  *  *  *
	//          s7 s6 s5 s4
	//       s2 s2 s2 s2
	//        *  *  *  *
	//       s7 s6 s5 s4
	//    s3 s3 s3 s3
	//     *  *  *  *
	//    s7 s6 s5 s4
	// ------------------------
	// s7 s6 s5 s4 s3 s2 s1 s0
	// ========================

// ### s0*s4 ###
	mul	x16,x7,x11
	umulh	x5,x7,x11

// ### s1*s4 + s0*s5 ###
	mul	x3,x8,x11
	umulh	x4,x8,x11
	adds	x5,x5,x3
	adcs	x6,x4,xzr

	mul	x3,x7,x12
	umulh	x4,x7,x12
	adds	x5,x5,x3
	adcs	x6,x6,x4
	adcs	x15,xzr,xzr

// ### s2*s4 + s1*s5 + s0*s6 ###
	mul	x3,x9,x11
	umulh	x4,x9,x11
	adds	x6,x6,x3
	adcs	x15,x15,x4

	mul	x3,x8,x12
	umulh	x4,x8,x12
	adds	x6,x6,x3
	adcs	x15,x15,x4
	adcs	x17,xzr,xzr

	mul	x3,x7,x13
	umulh	x4,x7,x13
	adds	x6,x6,x3
	adcs	x15,x15,x4
	adcs	x17,x17,xzr

// ### s3*s4 + s2*s5 + s1*s6 + s0*s7 ###
	mul	x3,x10,x11
	umulh	x4,x10,x11
	adds	x15,x15,x3
	adcs	x17,x17,x4
	adcs	x19,xzr,xzr

	mul	x3,x9,x12
	umulh	x4,x9,x12
	adds	x15,x15,x3
	adcs	x17,x17,x4
	adcs	x19,x19,xzr

	mul	x3,x8,x13
	umulh	x4,x8,x13
	adds	x15,x15,x3
	adcs	x17,x17,x4
	adcs	x19,x19,xzr

	mul	x3,x7,x14
	umulh	x4,x7,x14
	adds	x15,x15,x3
	adcs	x17,x17,x4
	adcs	x19,x19,xzr

// ### s3*s5 + s2*s6 + s1*s7 ###
	mul	x3,x10,x12
	umulh	x4,x10,x12
	adds	x17,x17,x3
	adcs	x19,x19,x4
	adcs	x20,xzr,xzr

	mul	x3,x9,x13
	umulh	x4,x9,x13
	adds	x17,x17,x3
	adcs	x19,x19,x4
	adcs	x20,x20,xzr

	mul	x3,x8,x14
	umulh	x4,x8,x14
	adds	x11,x17,x3
	adcs	x19,x19,x4
	adcs	x20,x20,xzr

// ### s3*s6 + s2*s7 ###
	mul	x3,x10,x13
	umulh	x4,x10,x13
	adds	x19,x19,x3
	adcs	x20,x20,x4
	adcs	x17,xzr,xzr

	mul	x3,x9,x14
	umulh	x4,x9,x14
	adds	x12,x19,x3
	adcs	x20,x20,x4
	adcs	x17,x17,xzr

// ### s3*s7 ###
	mul	x3,x10,x14
	umulh	x4,x10,x14
	adds	x13,x20,x3
	adcs	x14,x17,x4

	mov	x7,x16
	mov	x8,x5
	mov	x9,x6
	mov	x10,x15

	// result of mul: s7 s6 s5 s4 s3 s2 s1 s0

// ### Reduction ###
	RDC

	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]

	// Restore scalar registers
	ldp	x16,x17,[sp,#16]
	ldp	x19,x20,[sp,#64]
	ldp	x29,x30,[sp],#80

	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	ecp_sm2p256_mul,.-ecp_sm2p256_mul

// void ecp_sm2p256_sqr(BN_ULONG *r, const BN_ULONG *a);
.globl	ecp_sm2p256_sqr
.type	ecp_sm2p256_sqr,%function
.align	5

ecp_sm2p256_sqr:
	AARCH64_SIGN_LINK_REGISTER
	// Store scalar registers
	stp	x29,x30,[sp,#-80]!
	add	x29,sp,#0
	stp	x16,x17,[sp,#16]
	stp	x19,x20,[sp,#64]

	// Load inputs
	ldp	x11,x12,[x1]
	ldp	x13,x14,[x1,#16]

// ### square ###
	// ========================
	//             s7 s6 s5 s4
	// *           s7 s6 s5 s4
	// ------------------------
	// +           s4 s4 s4 s4
	//              *  *  *  *
	//             s7 s6 s5 s4
	//          s5 s5 s5 s5
	//           *  *  *  *
	//          s7 s6 s5 s4
	//       s6 s6 s6 s6
	//        *  *  *  *
	//       s7 s6 s5 s4
	//    s7 s7 s7 s7
	//     *  *  *  *
	//    s7 s6 s5 s4
	// ------------------------
	// s7 s6 s5 s4 s3 s2 s1 s0
	// ========================

// ### s4*s5 ###
	mul	x8,x11,x12
	umulh	x9,x11,x12

// ### s4*s6 ###
	mul	x3,x13,x11
	umulh	x10,x13,x11
	adds	x9,x9,x3
	adcs	x10,x10,xzr

// ### s4*s7 + s5*s6 ###
	mul	x3,x14,x11
	umulh	x4,x14,x11
	adds	x10,x10,x3
	adcs	x7,x4,xzr

	mul	x3,x13,x12
	umulh	x4,x13,x12
	adds	x10,x10,x3
	adcs	x7,x7,x4
	adcs	x5,xzr,xzr

// ### s5*s7 ###
	mul	x3,x14,x12
	umulh	x4,x14,x12
	adds	x7,x7,x3
	adcs	x5,x5,x4

// ### s6*s7 ###
	mul	x3,x14,x13
	umulh	x4,x14,x13
	adds	x5,x5,x3
	adcs	x6,x4,xzr

// ### 2*(t3,t2,s0,s3,s2,s1) ###
	adds	x8,x8,x8
	adcs	x9,x9,x9
	adcs	x10,x10,x10
	adcs	x7,x7,x7
	adcs	x5,x5,x5
	adcs	x6,x6,x6
	adcs	x15,xzr,xzr

// ### s4*s4 ###
	mul	x16,x11,x11
	umulh	x17,x11,x11

// ### s5*s5 ###
	mul	x11,x12,x12
	umulh	x12,x12,x12

// ### s6*s6 ###
	mul	x3,x13,x13
	umulh	x4,x13,x13

// ### s7*s7 ###
	mul	x19,x14,x14
	umulh	x20,x14,x14

	adds	x8,x8,x17
	adcs	x9,x9,x11
	adcs	x10,x10,x12
	adcs	x7,x7,x3
	adcs	x5,x5,x4
	adcs	x6,x6,x19
	adcs	x15,x15,x20

	mov	x11,x7
	mov	x7,x16
	mov	x12,x5
	mov	x13,x6
	mov	x14,x15

	// result of mul: s7 s6 s5 s4 s3 s2 s1 s0

// ### Reduction ###
	RDC

	stp	x7,x8,[x0]
	stp	x9,x10,[x0,#16]

	// Restore scalar registers
	ldp	x16,x17,[sp,#16]
	ldp	x19,x20,[sp,#64]
	ldp	x29,x30,[sp],#80

	AARCH64_VALIDATE_LINK_REGISTER
	ret
.size	ecp_sm2p256_sqr,.-ecp_sm2p256_sqr
