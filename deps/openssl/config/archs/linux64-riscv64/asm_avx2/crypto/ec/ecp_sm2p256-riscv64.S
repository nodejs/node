.text

.section .rodata
.p2align 5
// The polynomial p
.type .Lpoly,@object
.Lpoly:
.dword	0xffffffffffffffff,0xffffffff00000000,0xffffffffffffffff,0xfffffffeffffffff

// The order of polynomial n
.type .Lord,@object
.Lord:
.dword	0x53bbf40939d54123,0x7203df6b21c6052b,0xffffffffffffffff,0xfffffffeffffffff

// (p + 1) / 2
.type .Lpoly_div_2,@object
.Lpoly_div_2:
.dword	0x8000000000000000,0xffffffff80000000,0xffffffffffffffff,0x7fffffff7fffffff

// (n + 1) / 2
.type .Lord_div_2,@object
.Lord_div_2:
.dword	0xa9ddfa049ceaa092,0xb901efb590e30295,0xffffffffffffffff,0x7fffffff7fffffff


// void bn_rshift1(BN_ULONG *a);
.globl	bn_rshift1
.type	bn_rshift1,%function
.p2align 5
bn_rshift1:
	// Load inputs
	ld x5, 0(x10)
	ld x6, 8(x10)
	ld x7, 16(x10)
	ld x13, 24(x10)

	// Right shift 1
	slli x14, x6, 63
	srli x5, x5, 1
	or x5, x5, x14
	slli x15, x7, 63
	srli x6, x6, 1
	or x6, x6, x15
	slli x16, x13, 63
	srli x7, x7, 1
	or x7, x7, x16
	srli x17, x13, 1

	// Store results
	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x17, 24(x10)

	ret
.size bn_rshift1,.-bn_rshift1

// void bn_sub(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b);
.globl	bn_sub
.type	bn_sub,%function
.p2align 5
bn_sub:
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)

	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	// Subtraction
	sltu x31, x5, x14  //borrow
	sub x5, x5, x14

	sltu x29, x6, x15
	sub x6, x6, x15
	sltu x28, x6, x31
	sub x6, x6, x31
	add x31, x29, x28

	sltu x30, x7, x16
	sub x7, x7, x16
	sltu x29, x7, x31
	sub x7, x7, x31
	add x30, x30, x29

	sub x13, x13, x17
	sub x13, x13, x30

	// Store results
	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x13, 24(x10)

	ret
.size bn_sub,.-bn_sub

// void ecp_sm2p256_mul_by_3(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_mul_by_3
.type	ecp_sm2p256_mul_by_3,%function
.p2align 5
ecp_sm2p256_mul_by_3:
// returns r = ( a * 3 ) mod p, where p is a predefined polynomial modulus .Lpoly
// Input: x11 = address of operand a
// Output: x10 = address for result storage
// Register usage: x5-x17,x9-x24: data storage registers, x28-x31: carry/borrow flags
    addi    sp,sp,-96
    sd      x9,80(sp)
    sd      x18,72(sp)
    sd      x19,64(sp)
    sd      x20,56(sp)
    sd      x21,48(sp)
    sd      x22,40(sp)
    sd      x23,32(sp)
    sd      x24,24(sp)
    sd      x25,16(sp)
    sd      x26,8(sp)
    sd      x27,0(sp)
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)

	// 2*a
	add x14, x5, x5
	sltu x28, x14, x5

	slli x15, x6, 1
	sltu x29, x15, x6
	add x15, x15, x28

	slli x16, x7, 1
	sltu x31, x16, x7
	add x16, x16, x29

	slli x17, x13, 1
	sltu x28, x17, x13
	add x17, x17, x31

	la x12, .Lpoly
	ld x9, 0(x12)
	ld x18, 8(x12)
	ld x19, 16(x12)
	ld x20, 24(x12)

	// Sub polynomial
	sltu x31, x14, x9
	sub x21, x14, x9

	sltu x29, x15, x31
	sub x22, x15, x31
	sltu x31, x22, x18
	sub x22, x22, x18
	add x31, x31, x29

	sltu x29, x16, x31
	sub x23, x16, x31
	sltu x31, x23, x19
	sub x23, x23, x19
	add x31, x31, x29

	sltu x29, x17, x31
	sub x24, x17, x31
	sltu x31, x24, x20
	sub x24, x24, x20
	add x31, x31, x29

	slt x28, x28, x31
	negw x29, x28
	addw x28, x28, -1

	and x14, x14, x29
	and x15, x15, x29
	and x16, x16, x29
	and x17, x17, x29
	and x21, x21, x28
	and x22, x22, x28
	and x23, x23, x28
	and x24, x24, x28
	or x14, x14, x21
	or x15, x15, x22
	or x16, x16, x23
	or x17, x17, x24

	// 3*a
	add x14, x14, x5
	sltu x28, x14, x5

	add x15, x15, x6
	sltu x29, x15, x6
	add x15, x15, x28
	sltu x28, x15, x28
	add x28, x28, x29

	add x16, x16, x7
	sltu x29, x16, x7
	add x16, x16, x28
	sltu x28, x16, x28
	add x28, x28, x29

	add x17, x17, x13
	sltu x29, x17, x13
	add x17, x17, x28
	sltu x28, x17, x28
	add x28, x28, x29

	// Sub polynomial
	sltu x31, x14, x9
 	sub x21, x14, x9

	sltu x29, x15, x31
	sub x22, x15, x31
	sltu x31, x22, x18
	sub x22, x22, x18
	add x31, x31, x29

	sltu x29, x16, x31
	sub x23, x16, x31
	sltu x31, x23, x19
	sub x23, x23, x19
	add x31, x31, x29

	sltu x29, x17, x31
	sub x24, x17, x31
	sltu x31, x24, x20
	sub x24, x24, x20
	add x31, x31, x29

	slt x28, x28, x31
	negw x29, x28
	addw x28, x28, -1

	and x14, x14, x29
	and x15, x15, x29
	and x16, x16, x29
	and x17, x17, x29
	and x21, x21, x28
	and x22, x22, x28
	and x23, x23, x28
	and x24, x24, x28
	or x14, x14, x21
	or x15, x15, x22
	or x16, x16, x23
	or x17, x17, x24

	// Store results
	sd x14, 0(x10)
	sd x15, 8(x10)
	sd x16, 16(x10)
	sd x17, 24(x10)
    ld      x9,80(sp)
    ld      x18,72(sp)
    ld      x19,64(sp)
    ld      x20,56(sp)
    ld      x21,48(sp)
    ld      x22,40(sp)
    ld      x23,32(sp)
    ld      x24,24(sp)
    ld      x25,16(sp)
    ld      x26,8(sp)
    ld      x27,0(sp)
    addi    sp,sp,96
	ret
.size ecp_sm2p256_mul_by_3,.-ecp_sm2p256_mul_by_3


// void ecp_sm2p256_add(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_add
.type	ecp_sm2p256_add,%function
.p2align 5
ecp_sm2p256_add:
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)

	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	// Addition
	add x5, x5, x14
	sltu x28, x5, x14  //carry

	add x6, x6, x15
	sltu x29, x6, x15
	add x6, x6, x28
	sltu x28, x6, x28
	add x28, x28, x29

	add x7, x7, x16
	sltu x29, x7, x16
	add x7, x7, x28
	sltu x28, x7, x28
	add x28, x28, x29

	add x13, x13, x17
	sltu x29, x13, x17
	add x13, x13, x28
	sltu x28, x13, x28
	add x28, x28, x29

	// Load polynomial
	la x12, .Lpoly
	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	// Sub polynomial
	sltu x31, x5, x14  //borrow
	sub x14, x5, x14
	sltu x29, x6, x15
	sub x15, x6, x15
	sltu x30, x15, x31
	sub x15, x15, x31
	add x29, x29, x30  //borrow
	sltu x31, x7, x16
	sub x16, x7, x16
	sltu x30, x16, x29
	sub x16, x16, x29
	add x31, x31, x30  //borrow
	sltu x29, x13, x17
	sub x17, x13, x17
	sltu x30, x17, x31
	sub x17, x17, x31
	add x31, x29, x30  //borrow

	// Select based on carry
	slt x28, x28, x31
	negw x29, x28
	addw x28, x28, -1

	and x5, x5, x29
	and x6, x6, x29
	and x7, x7, x29
	and x13, x13, x29
	and x14, x14, x28
	and x15, x15, x28
	and x16, x16, x28
	and x17, x17, x28
	or x5, x5, x14
	or x6, x6, x15
	or x7, x7, x16
	or x13, x13, x17


	// Store results
	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x13, 24(x10)
	ret
.size ecp_sm2p256_add,.-ecp_sm2p256_add

// void ecp_sm2p256_sub(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_sub
.type	ecp_sm2p256_sub,%function
.p2align 5
ecp_sm2p256_sub:
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)

	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	// Subtraction
	sltu x31, x5, x14  //borrow
	sub x5, x5, x14

	sltu x29, x6, x15
	sub x6, x6, x15
	sltu x30, x6, x31
	sub x6, x6, x31
	add x31, x30, x29

	sltu x29, x7, x16
	sub x7, x7, x16
	sltu x30, x7, x31
	sub x7, x7, x31
	add x31, x30, x29

	sltu x29, x13, x17
	sub x13, x13, x17
	sltu x30, x13, x31
	sub x13, x13, x31
	add x31, x30, x29

	// Load polynomial
	la x12, .Lpoly
	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	// Add polynomial
	add x14, x5, x14
	sltu x28, x14, x5

	add x15, x6, x15
	sltu x29, x15, x6
	add x15, x15, x28
	sltu x28, x15, x28
	add x28, x28, x29

	add x16, x7, x16
	sltu x29, x16, x7
	add x16, x16, x28
	sltu x28, x16, x28
	add x28, x28, x29

	add x17, x13, x17
	add x17, x17, x28

	negw x29, x31
	addw x31, x31, -1

	and x5, x5, x31
	and x6, x6, x31
	and x7, x7, x31
	and x13, x13, x31
	and x14, x14, x29
	and x15, x15, x29
	and x16, x16, x29
	and x17, x17, x29
	or x5, x5, x14
	or x6, x6, x15
	or x7, x7, x16
	or x13, x13, x17

	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x13, 24(x10)
	ret
.size ecp_sm2p256_sub,.-ecp_sm2p256_sub

// void ecp_sm2p256_sub_mod_ord(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_sub_mod_ord
.type	ecp_sm2p256_sub_mod_ord,%function
.p2align 5
ecp_sm2p256_sub_mod_ord:
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)

	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	// Subtraction
	sltu x31, x5, x14  //borrow
	sub x5, x5, x14

	sltu x29, x6, x15
	sub x6, x6, x15
	sltu x30, x6, x31
	sub x6, x6, x31
	add x31, x30, x29

	sltu x29, x7, x16
	sub x7, x7, x16
	sltu x30, x7, x31
	sub x7, x7, x31
	add x31, x30, x29

	sltu x29, x13, x17
	sub x13, x13, x17
	sltu x30, x13, x31
	sub x13, x13, x31
	add x31, x30, x29

	// Load polynomial
	la x12, .Lord
	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	// Add polynomial
	add x14, x5, x14
	sltu x28, x14, x5

	add x15, x6, x15
	sltu x29, x15, x6
	add x15, x15, x28
	sltu x28, x15, x28
	add x28, x28, x29

	add x16, x7, x16
	sltu x29, x16, x7
	add x16, x16, x28
	sltu x28, x16, x28
	add x28, x28, x29

	add x17, x13, x17
	add x17, x17, x28

	negw x29, x31
	addw x31, x31, -1

	and x5, x5, x31
	and x6, x6, x31
	and x7, x7, x31
	and x13, x13, x31
	and x14, x14, x29
	and x15, x15, x29
	and x16, x16, x29
	and x17, x17, x29
	or x5, x5, x14
	or x6, x6, x15
	or x7, x7, x16
	or x13, x13, x17

	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x13, 24(x10)
	ret
.size ecp_sm2p256_sub_mod_ord,.-ecp_sm2p256_sub_mod_ord

// void ecp_sm2p256_div_by_2(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_div_by_2
.type	ecp_sm2p256_div_by_2,%function
.p2align 5
ecp_sm2p256_div_by_2:
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)

	// Save the least significant bit, is odd?
	andi x28, x5, 0x1

	// Right shift 1
	slli x14, x6, 63
	srli x5, x5, 1
	or x5, x5, x14
	slli x15, x7, 63
	srli x6, x6, 1
	or x6, x6, x15
	slli x16, x13, 63
	srli x7, x7, 1
	or x7, x7, x16
	srli x13, x13, 1

	// Load mod
	la x12, .Lpoly_div_2
	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	sub x29, zero, x28  //if even， p clear to 0

	and x14, x14, x29
	and x15, x15, x29
	and x16, x16, x29
	and x17, x17, x29

	add x5, x5, x14
	sltu x28, x5, x14

	add x6, x6, x15
	sltu x29, x6, x15
	add x6, x6, x28
	sltu x28, x6, x28
	add x28, x28, x29

	add x7, x7, x16
	sltu x29, x7, x16
	add x7, x7, x28
	sltu x28, x7, x28
	add x28, x28, x29

	add x13, x13, x17
	add x13, x13, x28

	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x13, 24(x10)
	ret
.size ecp_sm2p256_div_by_2,.-ecp_sm2p256_div_by_2

// void ecp_sm2p256_div_by_2_mod_ord(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_div_by_2_mod_ord
.type	ecp_sm2p256_div_by_2_mod_ord,%function
.p2align 5
ecp_sm2p256_div_by_2_mod_ord:
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)

	// Save the least significant bit, is odd?
	andi x28, x5, 0x1

	// Right shift 1
	slli x14, x6, 63
	srli x5, x5, 1
	or x5, x5, x14
	slli x15, x7, 63
	srli x6, x6, 1
	or x6, x6, x15
	slli x16, x13, 63
	srli x7, x7, 1
	or x7, x7, x16
	srli x13, x13, 1

	// Load mod
	la x12, .Lord_div_2
	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)

	sub x29, zero, x28  //if even， p clear to 0

	and x14, x14, x29
	and x15, x15, x29
	and x16, x16, x29
	and x17, x17, x29

	add x5, x5, x14
	sltu x28, x5, x14

	add x6, x6, x15
	sltu x29, x6, x15
	add x6, x6, x28
	sltu x28, x6, x28
	add x28, x28, x29

	add x7, x7, x16
	sltu x29, x7, x16
	add x7, x7, x28
	sltu x28, x7, x28
	add x28, x28, x29

	add x13, x13, x17
	add x13, x13, x28

	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x13, 24(x10)
	ret
.size ecp_sm2p256_div_by_2_mod_ord,.-ecp_sm2p256_div_by_2_mod_ord

.macro RDC
// Fast Modular reduction of 512 bits in t7..t0 mod p
// See https://en.wikipedia.org/wiki/Solinas_prime#Modular_reduction_algorithm
//
// For SM2 p = 2^256 -2^224 - 2^96 + 2^64 - 1
//
// p is a Generalized Mersenne prime of the form
// p = 2^(32*8) - (2^(32*7) + 2^(32*3) - 2^(32*2) + 2^(32*0))
//
// Giving f(x) = x^7 + x^3 - x^2 + 1 (for x = 2^32)
//
// For j = 0 to 7
//  sum(a(j) * x^j) = (s0 s1 .. s7) + ((s8 s9 .. s15))[matrix of coefficients][1 x x^2.. x^7]
//
// where the higher order terms can be expressed as
//    x^8 ~= x^7 + x^3 - x^2 + 1
//    x^9 ~= x * x^8 ~= x * (x^7 + x^3 - x^2 + 1)
//       ~= x^8 + x^4 - x^3 + x mod p ~= x^7 + x^3 - x^2 + 1 + x^4 - x^3 + x
//       ~= x^7 + x^4 - x^2 + x + 1
//    Similarly:
//    x^10 ~= x * x^9 ~= x * (x^7 + x^4 - x^2 + x + 1) ~= (x^8 + x^5 - x^3 + x2 + x)
//         ~= x^7 + x^5 + x + 1
//    x^11 ~= x^8 + x^6 + x^2 + x
//         ~= x^7 + x^6 + x^3 + x + 1
//    x^12 ~= x^8 + x^7 + x^4 + x^2 + x
//         ~= 2x^7 + x^4 + x^3 + x + 1
//    x^13 ~= 2x^8 + x^5 + x^4 + x^2 + x
//         ~= 2x^7 + x^5 + x^4 + 2x^3 - x^2 + x + 2
//    x^14 ~= 2x^8 + x^6 + x^5 + 2x^4 - x^3 + x^2 + 2x
//         ~= 2x^7 + x^6 + x^5 + 2x^4 + x^3 - x^2 + 2x + 2
//    x^15 ~= 2x^8 + x^7 + x^6 + 2x^5 + x^4 - x^3 + 2x^2 + 2x
//         ~= 3x^7 + x^6 + 2x^5 + x^4 + x^3 + 2x + 2
// ====>
//      x^0     x^1     x^2     x^3     x^4     x^5     x^6     x^7
//------------------------------------------------------------------
// x^8      1       0      -1       1       0       0       0       1
// x^9      1       1      -1       0       1       0       0       1
// x^10     1       1       0       0       0       1       0       1
// x^11     1       1       0       1       0       0       1       1
// x^12     1       1       0       1       1       0       0       2
// x^13     2       1      -1       2       1       1       0       2
// x^14     2       2      -1       1       2       1       1       2
// x^15     2       2       0       1       1       2       1       3
// ====>
// c0 = x0 + x8 + x9 + x10 + x11 + x12 + 2*x13 + 2*x14 + 2*x15
// c1 = x1 + 0 + x9 + x10 + x11 + x12 + x13 + 2*x14 + 2*x15
// c2 = x2 - x8 - x9 + 0 + 0 + 0 - x13 - x14 + 0
// c3 = x3 + x8 + 0 + 0 + x11 + x12 + 2*x13 + x14 + x15
// c4 = x4 + 0 + x9 + 0 + 0 + x12 + x13 + 2*x14 + x15
// c5 = x5 + 0 + 0 + x10 + 0 + 0 + x13 + x14 + 2*x15
// c6 = x6 + 0 + 0 + 0 + x11 + 0 + 0 + x14 + x15
// c7 = x7 + x8 + x9 + x10 + x11 + 2*x12 + 2*x13 + 2*x14 + 3*x15
// ===>
// The input values are 8 64 bit values t0..t7
// t4-t7 can be represented as 16*32 bit values s0..s7
// a = |  t7   | ... | t0  |, where ti are 64-bit quantitiet
//   = | s7| s6| ... |--|--|, where ai are 32-bit quantitiet
// |    t7     |    t6     |    t5     |    t4     |
// |  s7 |  s6 |  s5 |  s4 |  s3 |  s2 | s1  | s0  |
// |    t3     |    t2     |    t1     |    t0     |
// |           |           |           |           |
// =================================================
// |  s0 |  s3 |  s2 | s1  |  s0 |   0 |    t4     | (+)
// |  s1 |  s7 |    t6     |  s3 |   0 |  s2 |  s1 | (+)
// |  s2 |   0 |  s6 |  s5 |  s4 |   0 |    t5     | (+)
// |  s3 |   0 |    t7     |  s5 |   0 |  s4 |  s3 | (+)
// |  s4 |   0 |    t7     |  s5 |   0 |    t6     | (+)
// |  s4 |   0 |   0 |  s7 |  s6 |   0 |  s6 |  s5 | (+)
// |  s5 |   0 |   0 |   0 |  s7 |   0 |  s6 |  s5 | (+)
// |  s5 |   0 |   0 |   0 |   0 |   0 |    t7     | (+)
// |  s6 |   0 |   0 |   0 |   0 |   0 |    t7     | (+)
// |  s6 |   0 |   0 |   0 |   0 |   0 |   0 |  s7 | (+)
// |  s7 |   0 |   0 |   0 |   0 |   0 |   0 |  s7 | (+)
// |  s7 |   0 |   0 |   0 |   0 |   0 |   0 |   0 | (+)
// |    t7     |   0 |   0 |   0 |   0 |   0 |   0 | (+)
// |   0 |   0 |   0 |   0 |   0 | s0  |   0 |   0 | (-)
// |   0 |   0 |   0 |   0 |   0 | s1  |   0 |   0 | (-)
// |   0 |   0 |   0 |   0 |   0 | s5  |   0 |   0 | (-)
// |   0 |   0 |   0 |   0 |   0 | s6  |   0 |   0 | (-)
// | U[7]| U[6]| U[5]| U[4]| U[3]| U[2]| U[1]| U[0]|
// |  t7 |  t6 |  t5 |  t4 |  s3 |  s2 |   s1 | s0 | (+)
// |    V[3]   |    V[2]   |    V[1]   |    V[0]   |

	// 1. 64-bit addition
	// s0=t6+t7+t7
	add x9, x16, x17
	sltu x28, x9, x16
	add x9, x9, x17
	sltu x29, x9, x17
	add x27, x28, x29  //t6+t7+t7 carry

	// s2=t4+t5+s0
	add x19, x14, x15
	sltu x30, x19, x14
	add x28, x30, x27
	add x19, x19, x9
	sltu x31, x19, x9
	add x28, x28, x31  //t4+t5+t6+t7+t7 carry

	// sum
	add x5, x5, x19 // t0 += (t4 + t5 + t6 + 2 * t7)
	sltu x29, x5, x19
	add x6, x6, x28
	sltu x28, x6, x28
	add x6, x6, x29
	sltu x29, x6, x29
	add x28, x28, x29
	add x7, x7, x9  //t2 += t6 + 2 * t7
	sltu x30, x7, x9
	add x7, x7, x28
	sltu x28, x7, x28
	add x28, x28, x30
	add x13, x13, x17 // t3 += t7
	sltu x29, x13, x17
	add x13, x13, x28
	sltu x28, x13, x28
	add x28, x28, x29
	add x13, x13, x27
	sltu x29, x13, x27
	add x25, x28, x29  // s8 = carry from t3

	// 2. 64-bit to 32-bit spread
	zext.w x9, x14
	zext.w x19, x15
	zext.w x21, x16
	zext.w x23, x17

	srli x18, x14, 32
	srli x20, x15, 32
	srli x22, x16, 32
	srli x24, x17, 32

	// 3. 32-bit addition
	add x28, x21, x23
	add x29, x22, x24
	add x30, x9, x18
	add x15, x23, x19  // t5 :s2 + s6
	add x16, x24, x20  // t6: s3 + s7
	add x21, x29, x28  // s4 + s5 + s6 + s7
	add x19, x19, x21
	add x19, x19, x21
	add x31, x20, x30
	add x17, x19, x31  //  t7: s0 + s1 + s2 + s3 + 2*(s4 + s5 + s6 + s7)
	add x21, x21, x22
	add x21, x21, x20
	add x21, x21, x9  // s0 + s3 + s4 + s5*2 + s6 + s7
	add x30, x30, x23
	add x19, x30, x22  // s2: s0 + s1 + s5 + s6
	add x14, x18, x29  // t4: s1 +s5 +s7
	add x20, x20, x14
	add x9, x20, x29  // s0: s1 + s3 + (s5 + s7)*2
	add x18, x28, x15  // s1: s2 + s4 + s6*2
	mv x20, x21   // s3: s4 + s5 + s6 + s7

	// 4. 32-bit to 64-bit
	slli x26, x18, 32
	slli x28, x20, 32
	srli x29, x18, 32
	or x18, x29, x28
	slli x30, x15, 32
	srli x31, x20, 32
	or x20, x31, x30
	slli x28, x17, 32
	srli x29, x15, 32
	or x15, x28, x29
	srli x17, x17, 32

	// 5. 64-bit addition
	add x9, x9, x26
	sltu x28, x9, x26
	add x18, x18, x28
	sltu x29, x18, x28
	add x20, x14, x20
	sltu x30, x20, x14
	add x20, x20, x29
	sltu x31, x20, x29
	add x28, x30, x31
	add x21, x16, x15
	sltu x29, x21, x16
	add x21, x21, x28
	sltu x30, x21, x28
	add x28, x29, x30
	add x28, x28, x25
	add x28, x28, x17

	// V[0]	s0
	// V[1]	s1
	// V[2]	s3
	// V[3]	s4
	// carry	c0
	// sub	s2

	// add with V0-V3
	add x5, x5, x9
	sltu x29, x5, x9
	add x6, x6, x18
	sltu x30, x6, x18
	add x6, x6, x29
	sltu x29, x6, x29
	add x29, x29, x30
	add x7, x7, x20
	sltu x30, x7, x20
	add x7, x7, x29
	sltu x29, x7, x29
	add x29, x29, x30
	add x13, x13, x21
	sltu x30, x13, x21
	add x13, x13, x29
	sltu x29, x13, x29
	add x29, x29, x30
	add x28, x28, x29
	// sub with s2
	sltu x30, x6, x19
	sub x6, x6, x19
	sltu x31, x7, x30
	sub x7, x7, x30
	sltu x29, x13, x31
	sub x13, x13, x31
	sub x28, x28, x29

	// 6. MOD
	// First Mod
	slli x22, x28, 32
	sub x23, x22, x28

	add x5, x5, x28
	sltu x29, x5, x28
	add x6, x6, x23
	sltu x30, x6, x23
	add x6, x6, x29
	sltu x29, x6, x29
	add x29, x29, x30
	add x7, x7, x29
	sltu x29, x7, x29
	add x13, x13, x22
	sltu x30, x13, x22
	add x13, x13, x29
	sltu x29, x13, x29
	add x28, x30, x29

	// Last Mod
	// return y - p if y > p else y
	la x12, .Lpoly
	ld x9, 0(x12)
	ld x18, 8(x12)
	ld x19, 16(x12)
	ld x20, 24(x12)

	sltu x29, x5, x9
	sub x14, x5, x9
	sltu x30, x6, x18
	sub x15, x6, x18
	sltu x31, x15, x29
	sub x15, x15, x29
	add x29, x30, x31
	sltu x30, x7, x19
	sub x16, x7, x19
	sltu x31, x16, x29
	sub x16, x16, x29
	add x29, x30, x31
	sltu x30, x13, x20
	sub x17, x13, x20
	sltu x31, x17, x29
	sub x17, x17, x29
	add x29, x30, x31

	slt x28, x28, x29
	negw x29, x28
	addw x28, x28, -1

	and x5, x5, x29
	and x6, x6, x29
	and x7, x7, x29
	and x13, x13, x29
	and x14, x14, x28
	and x15, x15, x28
	and x16, x16, x28
	and x17, x17, x28
	or x5, x5, x14
	or x6, x6, x15
	or x7, x7, x16
	or x13, x13, x17

	sd x5, 0(x10)
	sd x6, 8(x10)
	sd x7, 16(x10)
	sd x13, 24(x10)
.endm


// void ecp_sm2p256_mul(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b);
.globl	ecp_sm2p256_mul
.type	ecp_sm2p256_mul,%function
.p2align 5
ecp_sm2p256_mul:
    addi    sp,sp,-96
    sd      x9,80(sp)
    sd      x18,72(sp)
    sd      x19,64(sp)
    sd      x20,56(sp)
    sd      x21,48(sp)
    sd      x22,40(sp)
    sd      x23,32(sp)
    sd      x24,24(sp)
    sd      x25,16(sp)
    sd      x26,8(sp)
    sd      x27,0(sp)
	// Load inputs
	ld x5, 0(x11)
	ld x6, 8(x11)
	ld x7, 16(x11)
	ld x13, 24(x11)
	ld x14, 0(x12)
	ld x15, 8(x12)
	ld x16, 16(x12)
	ld x17, 24(x12)
// ### multiplication ###
	// ========================
	//             t3 t2 t1 t0
	// *           t7 t6 t5 t4
	// ------------------------
	// +           t0 t0 t0 t0
	//              *  *  *  *
	//             t7 t6 t5 t4
	//          t1 t1 t1 t1
	//           *  *  *  *
	//          t7 t6 t5 t4
	//       t2 t2 t2 t2
	//        *  *  *  *
	//       t7 t6 t5 t4
	//    t3 t3 t3 t3
	//     *  *  *  *
	//    t7 t6 t5 t4
	// ------------------------
	// t7 t6 t5 t4 t3 t2 t1 t0
	// ========================

// ### t0*t4 ###
	mul x9, x5, x14
	mulhu x18, x5, x14

//	### t0*t5+t1*t4 ###
	mul x19, x5, x15
	mulhu x20, x5, x15
	mul x21, x6, x14
	mulhu x22, x6, x14

	add x18, x18, x19
	sltu x28, x18, x19
	add x20, x20, x22
	sltu x29, x20, x22

	add x18, x18, x21
	sltu x30, x18, x21
	add x28, x28, x30
	add x20, x20, x28
	sltu x28, x20, x28
	add x28, x28, x29

//	### t0*t6+t1*t5+t2*t4 ###
	mul x19, x5, x16
	mulhu x21, x5, x16
	mul x22, x6, x15
	mulhu x23, x6, x15
	mul x24, x7, x14
	mulhu x25, x7, x14

	add x19, x19, x20
	sltu x29, x19, x20
	add x22, x22, x24
	sltu x31, x22, x24
	add x19, x19, x22
	sltu x30, x19, x22
	add x29, x29, x31
	add x28, x28, x30
	add x28, x28, x29

	add x21, x21, x23
	sltu x30, x21, x23
	add x25, x25, x28
	sltu x28, x25, x28
	add x21, x21, x25
	sltu x31, x21, x25
	add x28, x30, x28
	add x28, x28, x31

//	### t0*t7+t1*t6+t2*t5+t3*t4 ###
	mul x20, x5, x17
	mulhu x22, x5, x17
	mv x5, x9
	mul x26, x6, x16
	mulhu x27, x6, x16
	mul x9, x7, x15
	mulhu x23, x7, x15
	mul x24, x13, x14
	mulhu x25, x13, x14

	add x20, x20, x26
	sltu x29, x20, x26
	add x9, x9, x21
	sltu x30, x9, x21
	add x29, x29, x30
	add x20, x20, x24
	sltu x31, x20, x24
	add x20, x20, x9
	sltu x30, x20, x9
	add x31, x31, x30
	add x28, x28, x29
	add x28, x28, x31

	add x22, x22, x27
	sltu x29, x22, x27
	add x23, x23, x25
	sltu x30, x23, x25
	add x29, x29, x30
	add x22, x22, x28
	sltu x28, x22, x28
	add x22, x22, x23
	sltu x31, x22, x23
	add x28, x28, x29
	add x28, x28, x31

//	### t1*t7+t2*t6+t3*t5 ###
	mul x21, x6, x17
	mulhu x23, x6, x17
	mv x6, x18
	mul x9, x7, x16
	mulhu x18, x7, x16
	mul x24, x13, x15
	mulhu x25, x13, x15

	add x21, x21, x22
	sltu x29, x21, x22
	add x9, x9, x24
	sltu x30, x9, x24
	add x29, x29, x30
	add x14, x21, x9
	sltu x31, x14, x9
	add x29, x29, x31
	add x28, x28, x29

	add x23, x23, x18
	sltu x30, x23, x18
	add x25, x25, x28
	sltu x28, x25, x28
	add x28, x28, x30
	add x23, x23, x25
	sltu x31, x23, x25
	add x28, x28, x31

//	### t2*t7+t3*t6 ###
	mul x9, x7, x17
	mulhu x18, x7, x17
	mv x7, x19
	mul x22, x13, x16
	mulhu x16, x13, x16

	add x9, x9, x22
	sltu x29, x9, x22
	add x15, x9, x23
	sltu x30, x15, x23
	add x28, x28, x29
	add x28, x28, x30

	add x16, x16, x18
	sltu x29, x16, x18
	add x16, x16, x28
	sltu x28, x16, x28
	add x28, x28, x29

//	### t3*t7 ###
	mul x19, x13, x17
	mulhu x17, x13, x17
	mv x13, x20

	add x16, x16, x19
	sltu x29, x16, x19
	add x17, x17, x28
	add x17, x17, x29

	// ### Reduction ###
	RDC
    ld      x9,80(sp)
    ld      x18,72(sp)
    ld      x19,64(sp)
    ld      x20,56(sp)
    ld      x21,48(sp)
    ld      x22,40(sp)
    ld      x23,32(sp)
    ld      x24,24(sp)
    ld      x25,16(sp)
    ld      x26,8(sp)
    ld      x27,0(sp)
    addi    sp,sp,96
	ret
.size ecp_sm2p256_mul,.-ecp_sm2p256_mul

// void ecp_sm2p256_sqr(BN_ULONG *r, const BN_ULONG *a);
.globl	ecp_sm2p256_sqr
.type	ecp_sm2p256_sqr,%function
.p2align 5
ecp_sm2p256_sqr:
    addi    sp,sp,-96
    sd      x9,80(sp)
    sd      x18,72(sp)
    sd      x19,64(sp)
    sd      x20,56(sp)
    sd      x21,48(sp)
    sd      x22,40(sp)
    sd      x23,32(sp)
    sd      x24,24(sp)
    sd      x25,16(sp)
    sd      x26,8(sp)
    sd      x27,0(sp)
	// Load inputs
	ld x9, 0(x11)
	ld x18, 8(x11)
	ld x19, 16(x11)
	ld x20, 24(x11)

	// ========================
	//             s3 s2 s1 s0
	// *           s3 s2 s1 s0
	// ------------------------
	// +           s0 s0 s0 s0
	//              *  *  *  *
	//             s3 s2 s1 s0
	//          s1 s1 s1 s1
	//           *  *  *  *
	//          s3 s2 s1 s0
	//       s2 s2 s2 s2
	//        *  *  *  *
	//       s3 s2 s1 s0
	//    s3 s3 s3 s3
	//     *  *  *  *
	//    s3 s2 s1 s0
	// ------------------------
	// t7 t6 t5 t4 t3 t2 t1 t0
	// ========================

// ### s0*s0 ###
	mul x5, x9, x9
	mulhu x6, x9, x9

// ### s0*s1*2 ###
	mul x21, x9, x18
	mulhu x22, x9, x18

	slli x23, x21, 1
	sltu x28, x23, x21
	add x6, x6, x23
	sltu x29, x6, x23
	add x28, x28, x29

	slli x7, x22, 1
	sltu x30, x7, x22
	add x7, x7, x28
	sltu x31, x7, x28
	add x28, x30, x31

// ### s0*s2*2+s1*s1 ###
	mul x24, x9, x19
	mulhu x25, x9, x19
	mul x26, x18, x18
	mulhu x27, x18, x18

	slli x14, x24, 1
	sltu x29, x14, x24
	add x7, x7, x26
	sltu x30, x7, x26
	add x28, x28, x29
	add x7, x7, x14
	sltu x31, x7, x14
	add x28, x28, x30
	add x28, x28, x31

	slli x13, x25, 1
	sltu x29, x13, x25
	add x27, x27, x28
	sltu x28, x27, x28
	add x13, x13, x27
	sltu x30, x13, x27
	add x28, x28, x29
	add x28, x28, x30

// ### (s0*s3+s1*s2)*2 ###
	mul x21, x9, x20
	mulhu x22, x9, x20
	mul x23, x18, x19
	mulhu x24, x18, x19

	add x21, x21, x23
	sltu x29, x21, x23
	slli x29, x29, 1
	slli x23, x21, 1
	sltu x30, x23, x21
	add x29, x29, x30
	add x13, x13, x23
	sltu x31, x13, x23
	add x28, x28, x29
	add x28, x28, x31

	add x22, x22, x24
	sltu x29, x22, x24
	slli x29, x29, 1
	slli x25, x22, 1
	sltu x30, x25, x22
	add x29, x29, x30
	add x14, x28, x25
	sltu x31, x14, x25
	add x28, x29, x31

// ### s1*s3*2+s2*s2 ###
	mul x21, x18, x20
	mulhu x22, x18, x20
	mul x23, x19, x19
	mulhu x24, x19, x19

	slli x25, x21, 1
	sltu x29, x25, x21
	add x28, x28, x29
	add x14, x14, x23
	sltu x30, x14, x23
	add x28, x28, x30
	add x14, x14, x25
	sltu x31, x14, x25
	add x28, x28, x31

	slli x26, x22, 1
	sltu x29, x26, x22
	add x15, x28, x24
	sltu x30, x15, x24
	add x28, x29, x30
	add x15, x15, x26
	sltu x31, x15, x26
	add x28, x28, x31

// ### s2*s3*2 ###
	mul x21, x19, x20
	mulhu x22, x19, x20

	slli x23, x21, 1
	sltu x29, x23, x21
	add x15, x15, x23
	sltu x30, x15, x23
	add x28, x28, x29
	add x28, x28, x30

	slli x16, x22, 1
	sltu x31, x16, x22
	add x16, x16, x28
	sltu x28, x16, x28
	add x28, x28, x31

// ### s3*s3 ###
	mul x25, x20, x20
	mulhu x26, x20, x20

	add x16, x16, x25
	sltu x29, x16, x25

	add x17, x26, x28
	add x17, x17, x29

	// ### Reduction ###
	RDC
    ld      x9,80(sp)
    ld      x18,72(sp)
    ld      x19,64(sp)
    ld      x20,56(sp)
    ld      x21,48(sp)
    ld      x22,40(sp)
    ld      x23,32(sp)
    ld      x24,24(sp)
    ld      x25,16(sp)
    ld      x26,8(sp)
    ld      x27,0(sp)
    addi    sp,sp,96
	ret
.size ecp_sm2p256_sqr,.-ecp_sm2p256_sqr
