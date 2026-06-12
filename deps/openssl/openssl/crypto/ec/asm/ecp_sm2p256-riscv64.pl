#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
use strict;
use warnings;

use FindBin qw($Bin);
use lib "$Bin";
use lib "$Bin/../../perlasm";
use riscv;

my $output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
my $flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$output and open STDOUT,">$output";

################################################################################
# Utility functions to help with keeping track of which registers to stack/
# unstack when entering / exiting routines.
################################################################################
{
    # Callee-saved registers
    my @callee_saved = map("x$_",(2,8,9,18..27));
    # Caller-saved registers
    my @caller_saved = map("x$_",(1,5..7,10..17,28..31));
    my @must_save;
    sub use_reg {
        my $reg = shift;
        if (grep(/^$reg$/, @callee_saved)) {
            push(@must_save, $reg);
        } elsif (!grep(/^$reg$/, @caller_saved)) {
            # Register is not usable!
            die("Unusable register ".$reg);
        }
        return $reg;
    }
    sub use_regs {
        return map(use_reg("x$_"), @_);
    }
    sub save_regs {
        my $ret = '';
        my $stack_reservation = ($#must_save + 1) * 8;
        my $stack_offset = $stack_reservation;
        if ($stack_reservation % 16) {
            $stack_reservation += 8;
        }
        $ret.="    addi    sp,sp,-$stack_reservation\n";
        foreach (@must_save) {
            $stack_offset -= 8;
            $ret.="    sd      $_,$stack_offset(sp)\n";
        }
        return $ret;
    }
    sub load_regs {
        my $ret = '';
        my $stack_reservation = ($#must_save + 1) * 8;
        my $stack_offset = $stack_reservation;
        if ($stack_reservation % 16) {
            $stack_reservation += 8;
        }
        foreach (@must_save) {
            $stack_offset -= 8;
            $ret.="    ld      $_,$stack_offset(sp)\n";
        }
        $ret.="    addi    sp,sp,$stack_reservation\n";
        return $ret;
    }
    sub clear_regs {
        @must_save = ();
    }
}

my $code=<<___;
.text
___

# Function arguments
# Input block pointer, output block pointer, key pointer
my ($i0, $i1, $i2) = use_regs(10 .. 12);
my ($t0, $t1, $t2, $t3, $t4, $t5, $t6, $t7, $c0, $c1, $c2, $c3) = use_regs(5 .. 7, 13 ... 17, 28 .. 31);
my ($s0, $s1, $s2, $s3, $s4, $s5, $s6, $s7, $s8, $s9, $s10) = use_regs(9, 18 .. 27);

sub bn_mod_add() {
# returns r = ( a + b ) mod p, where p is a predefined polynomial modulus
# Input: $i1 = address of operand a, $i2 = address of operand b
# Output: $i0 = address for result storage
# Dependencies: $mod = address of modulus p (passed via parameter)
# Register usage: $t0-$t7: data storage registers, $c0-$c3: carry/borrow flags
    my $mod = shift;
$code.=<<___;
	// Load inputs
	ld $t0, 0($i1)
	ld $t1, 8($i1)
	ld $t2, 16($i1)
	ld $t3, 24($i1)

	ld $t4, 0($i2)
	ld $t5, 8($i2)
	ld $t6, 16($i2)
	ld $t7, 24($i2)

	// Addition
	add $t0, $t0, $t4
	sltu $c0, $t0, $t4  //carry

	add $t1, $t1, $t5
	sltu $c1, $t1, $t5
	add $t1, $t1, $c0
	sltu $c0, $t1, $c0
	add $c0, $c0, $c1

	add $t2, $t2, $t6
	sltu $c1, $t2, $t6
	add $t2, $t2, $c0
	sltu $c0, $t2, $c0
	add $c0, $c0, $c1

	add $t3, $t3, $t7
	sltu $c1, $t3, $t7
	add $t3, $t3, $c0
	sltu $c0, $t3, $c0
	add $c0, $c0, $c1

	// Load polynomial
	la $i2, $mod
	ld $t4, 0($i2)
	ld $t5, 8($i2)
	ld $t6, 16($i2)
	ld $t7, 24($i2)

	// Sub polynomial
	sltu $c3, $t0, $t4  //borrow
	sub $t4, $t0, $t4
	sltu $c1, $t1, $t5
	sub $t5, $t1, $t5
	sltu $c2, $t5, $c3
	sub $t5, $t5, $c3
	add $c1, $c1, $c2  //borrow
	sltu $c3, $t2, $t6
	sub $t6, $t2, $t6
	sltu $c2, $t6, $c1
	sub $t6, $t6, $c1
	add $c3, $c3, $c2  //borrow
	sltu $c1, $t3, $t7
	sub $t7, $t3, $t7
	sltu $c2, $t7, $c3
	sub $t7, $t7, $c3
	add $c3, $c1, $c2  //borrow

	// Select based on carry
	slt $c0, $c0, $c3
	negw $c1, $c0
	addw $c0, $c0, -1

	and $t0, $t0, $c1
	and $t1, $t1, $c1
	and $t2, $t2, $c1
	and $t3, $t3, $c1
	and $t4, $t4, $c0
	and $t5, $t5, $c0
	and $t6, $t6, $c0
	and $t7, $t7, $c0
	or $t0, $t0, $t4
	or $t1, $t1, $t5
	or $t2, $t2, $t6
	or $t3, $t3, $t7


	// Store results
	sd $t0, 0($i0)
	sd $t1, 8($i0)
	sd $t2, 16($i0)
	sd $t3, 24($i0)
___
}

sub bn_mod_sub() {
# returns r = ( a - b ) mod p, where p is a predefined polynomial modulus
# Input: $i1 = address of operand a, $i2 = address of operand b
# Output: $i0 = address for result storage
# Dependencies: $mod = address of modulus p (passed via parameter)
# Register usage: $t0-$t7: data storage registers, $c0-$c3: carry/borrow flags
    my $mod = shift;
$code.=<<___;
	// Load inputs
	ld $t0, 0($i1)
	ld $t1, 8($i1)
	ld $t2, 16($i1)
	ld $t3, 24($i1)

	ld $t4, 0($i2)
	ld $t5, 8($i2)
	ld $t6, 16($i2)
	ld $t7, 24($i2)

	// Subtraction
	sltu $c3, $t0, $t4  //borrow
	sub $t0, $t0, $t4

	sltu $c1, $t1, $t5
	sub $t1, $t1, $t5
	sltu $c2, $t1, $c3
	sub $t1, $t1, $c3
	add $c3, $c2, $c1

	sltu $c1, $t2, $t6
	sub $t2, $t2, $t6
	sltu $c2, $t2, $c3
	sub $t2, $t2, $c3
	add $c3, $c2, $c1

	sltu $c1, $t3, $t7
	sub $t3, $t3, $t7
	sltu $c2, $t3, $c3
	sub $t3, $t3, $c3
	add $c3, $c2, $c1

	// Load polynomial
	la $i2, $mod
	ld $t4, 0($i2)
	ld $t5, 8($i2)
	ld $t6, 16($i2)
	ld $t7, 24($i2)

	// Add polynomial
	add $t4, $t0, $t4
	sltu $c0, $t4, $t0

	add $t5, $t1, $t5
	sltu $c1, $t5, $t1
	add $t5, $t5, $c0
	sltu $c0, $t5, $c0
	add $c0, $c0, $c1

	add $t6, $t2, $t6
	sltu $c1, $t6, $t2
	add $t6, $t6, $c0
	sltu $c0, $t6, $c0
	add $c0, $c0, $c1

	add $t7, $t3, $t7
	add $t7, $t7, $c0

	negw $c1, $c3
	addw $c3, $c3, -1

	and $t0, $t0, $c3
	and $t1, $t1, $c3
	and $t2, $t2, $c3
	and $t3, $t3, $c3
	and $t4, $t4, $c1
	and $t5, $t5, $c1
	and $t6, $t6, $c1
	and $t7, $t7, $c1
	or $t0, $t0, $t4
	or $t1, $t1, $t5
	or $t2, $t2, $t6
	or $t3, $t3, $t7

	sd $t0, 0($i0)
	sd $t1, 8($i0)
	sd $t2, 16($i0)
	sd $t3, 24($i0)
___
}

sub bn_mod_div_by_2() {
# returns r = ( a / 2 ) mod p, (if a_is_odd ? a + p : a) >> 1， where p is a predefined polynomial modulus
# Input: $i1 = address of operand a
# Output: $i0 = address for result storage
# Dependencies: $mod = address of modulus k = (p + 1) / 2 (passed via parameter)
# Register usage: $t0-$t7: data storage registers, $c0-$c3: carry/borrow flags
# Principle:
#     if a is even: 0 <= a / 2 < p, ( a / 2 ) mod p = a / 2
#     if a is odd: ( a / 2 ) mod p = ( a + p ) >> 1, k = (p + 1) / 2, ( a / 2 ) mod p = a >> 1 + k
    my $mod = shift;
$code.=<<___;
	// Load inputs
	ld $t0, 0($i1)
	ld $t1, 8($i1)
	ld $t2, 16($i1)
	ld $t3, 24($i1)

	// Save the least significant bit, is odd?
	andi $c0, $t0, 0x1

	// Right shift 1
	slli $t4, $t1, 63
	srli $t0, $t0, 1
	or $t0, $t0, $t4
	slli $t5, $t2, 63
	srli $t1, $t1, 1
	or $t1, $t1, $t5
	slli $t6, $t3, 63
	srli $t2, $t2, 1
	or $t2, $t2, $t6
	srli $t3, $t3, 1

	// Load mod
	la $i2, $mod
	ld $t4, 0($i2)
	ld $t5, 8($i2)
	ld $t6, 16($i2)
	ld $t7, 24($i2)

	sub $c1, zero, $c0  //if even， p clear to 0

	and $t4, $t4, $c1
	and $t5, $t5, $c1
	and $t6, $t6, $c1
	and $t7, $t7, $c1

	add $t0, $t0, $t4
	sltu $c0, $t0, $t4

	add $t1, $t1, $t5
	sltu $c1, $t1, $t5
	add $t1, $t1, $c0
	sltu $c0, $t1, $c0
	add $c0, $c0, $c1

	add $t2, $t2, $t6
	sltu $c1, $t2, $t6
	add $t2, $t2, $c0
	sltu $c0, $t2, $c0
	add $c0, $c0, $c1

	add $t3, $t3, $t7
	add $t3, $t3, $c0

	sd $t0, 0($i0)
	sd $t1, 8($i0)
	sd $t2, 16($i0)
	sd $t3, 24($i0)
___
}

{
$code.=<<___;

.section .rodata
.p2align 5
// The polynomial p
.type .Lpoly,\@object
.Lpoly:
.dword	0xffffffffffffffff,0xffffffff00000000,0xffffffffffffffff,0xfffffffeffffffff

// The order of polynomial n
.type .Lord,\@object
.Lord:
.dword	0x53bbf40939d54123,0x7203df6b21c6052b,0xffffffffffffffff,0xfffffffeffffffff

// (p + 1) / 2
.type .Lpoly_div_2,\@object
.Lpoly_div_2:
.dword	0x8000000000000000,0xffffffff80000000,0xffffffffffffffff,0x7fffffff7fffffff

// (n + 1) / 2
.type .Lord_div_2,\@object
.Lord_div_2:
.dword	0xa9ddfa049ceaa092,0xb901efb590e30295,0xffffffffffffffff,0x7fffffff7fffffff


// void bn_rshift1(BN_ULONG *a);
.globl	bn_rshift1
.type	bn_rshift1,%function
.p2align 5
bn_rshift1:
	// Load inputs
	ld $t0, 0($i0)
	ld $t1, 8($i0)
	ld $t2, 16($i0)
	ld $t3, 24($i0)

	// Right shift 1
	slli $t4, $t1, 63
	srli $t0, $t0, 1
	or $t0, $t0, $t4
	slli $t5, $t2, 63
	srli $t1, $t1, 1
	or $t1, $t1, $t5
	slli $t6, $t3, 63
	srli $t2, $t2, 1
	or $t2, $t2, $t6
	srli $t7, $t3, 1

	// Store results
	sd $t0, 0($i0)
	sd $t1, 8($i0)
	sd $t2, 16($i0)
	sd $t7, 24($i0)

	ret
.size bn_rshift1,.-bn_rshift1

// void bn_sub(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b);
.globl	bn_sub
.type	bn_sub,%function
.p2align 5
bn_sub:
	// Load inputs
	ld $t0, 0($i1)
	ld $t1, 8($i1)
	ld $t2, 16($i1)
	ld $t3, 24($i1)

	ld $t4, 0($i2)
	ld $t5, 8($i2)
	ld $t6, 16($i2)
	ld $t7, 24($i2)

	// Subtraction
	sltu $c3, $t0, $t4  //borrow
	sub $t0, $t0, $t4

	sltu $c1, $t1, $t5
	sub $t1, $t1, $t5
	sltu $c0, $t1, $c3
	sub $t1, $t1, $c3
	add $c3, $c1, $c0

	sltu $c2, $t2, $t6
	sub $t2, $t2, $t6
	sltu $c1, $t2, $c3
	sub $t2, $t2, $c3
	add $c2, $c2, $c1

	sub $t3, $t3, $t7
	sub $t3, $t3, $c2

	// Store results
	sd $t0, 0($i0)
	sd $t1, 8($i0)
	sd $t2, 16($i0)
	sd $t3, 24($i0)

	ret
.size bn_sub,.-bn_sub

// void ecp_sm2p256_mul_by_3(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_mul_by_3
.type	ecp_sm2p256_mul_by_3,%function
.p2align 5
ecp_sm2p256_mul_by_3:
// returns r = ( a * 3 ) mod p, where p is a predefined polynomial modulus .Lpoly
// Input: $i1 = address of operand a
// Output: $i0 = address for result storage
// Register usage: $t0-$t7,$s0-$s7: data storage registers, $c0-$c3: carry/borrow flags
___
$code.= save_regs();
$code.=<<___;
	// Load inputs
	ld $t0, 0($i1)
	ld $t1, 8($i1)
	ld $t2, 16($i1)
	ld $t3, 24($i1)

	// 2*a
	add $t4, $t0, $t0
	sltu $c0, $t4, $t0

	slli $t5, $t1, 1
	sltu $c1, $t5, $t1
	add $t5, $t5, $c0

	slli $t6, $t2, 1
	sltu $c3, $t6, $t2
	add $t6, $t6, $c1

	slli $t7, $t3, 1
	sltu $c0, $t7, $t3
	add $t7, $t7, $c3

	la $i2, .Lpoly
	ld $s0, 0($i2)
	ld $s1, 8($i2)
	ld $s2, 16($i2)
	ld $s3, 24($i2)

	// Sub polynomial
	sltu $c3, $t4, $s0
	sub $s4, $t4, $s0

	sltu $c1, $t5, $c3
	sub $s5, $t5, $c3
	sltu $c3, $s5, $s1
	sub $s5, $s5, $s1
	add $c3, $c3, $c1

	sltu $c1, $t6, $c3
	sub $s6, $t6, $c3
	sltu $c3, $s6, $s2
	sub $s6, $s6, $s2
	add $c3, $c3, $c1

	sltu $c1, $t7, $c3
	sub $s7, $t7, $c3
	sltu $c3, $s7, $s3
	sub $s7, $s7, $s3
	add $c3, $c3, $c1

	slt $c0, $c0, $c3
	negw $c1, $c0
	addw $c0, $c0, -1

	and $t4, $t4, $c1
	and $t5, $t5, $c1
	and $t6, $t6, $c1
	and $t7, $t7, $c1
	and $s4, $s4, $c0
	and $s5, $s5, $c0
	and $s6, $s6, $c0
	and $s7, $s7, $c0
	or $t4, $t4, $s4
	or $t5, $t5, $s5
	or $t6, $t6, $s6
	or $t7, $t7, $s7

	// 3*a
	add $t4, $t4, $t0
	sltu $c0, $t4, $t0

	add $t5, $t5, $t1
	sltu $c1, $t5, $t1
	add $t5, $t5, $c0
	sltu $c0, $t5, $c0
	add $c0, $c0, $c1

	add $t6, $t6, $t2
	sltu $c1, $t6, $t2
	add $t6, $t6, $c0
	sltu $c0, $t6, $c0
	add $c0, $c0, $c1

	add $t7, $t7, $t3
	sltu $c1, $t7, $t3
	add $t7, $t7, $c0
	sltu $c0, $t7, $c0
	add $c0, $c0, $c1

	// Sub polynomial
	sltu $c3, $t4, $s0
 	sub $s4, $t4, $s0

	sltu $c1, $t5, $c3
	sub $s5, $t5, $c3
	sltu $c3, $s5, $s1
	sub $s5, $s5, $s1
	add $c3, $c3, $c1

	sltu $c1, $t6, $c3
	sub $s6, $t6, $c3
	sltu $c3, $s6, $s2
	sub $s6, $s6, $s2
	add $c3, $c3, $c1

	sltu $c1, $t7, $c3
	sub $s7, $t7, $c3
	sltu $c3, $s7, $s3
	sub $s7, $s7, $s3
	add $c3, $c3, $c1

	slt $c0, $c0, $c3
	negw $c1, $c0
	addw $c0, $c0, -1

	and $t4, $t4, $c1
	and $t5, $t5, $c1
	and $t6, $t6, $c1
	and $t7, $t7, $c1
	and $s4, $s4, $c0
	and $s5, $s5, $c0
	and $s6, $s6, $c0
	and $s7, $s7, $c0
	or $t4, $t4, $s4
	or $t5, $t5, $s5
	or $t6, $t6, $s6
	or $t7, $t7, $s7

	// Store results
	sd $t4, 0($i0)
	sd $t5, 8($i0)
	sd $t6, 16($i0)
	sd $t7, 24($i0)
___
$code.= load_regs();
$code.=<<___;
	ret
.size ecp_sm2p256_mul_by_3,.-ecp_sm2p256_mul_by_3


// void ecp_sm2p256_add(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_add
.type	ecp_sm2p256_add,%function
.p2align 5
ecp_sm2p256_add:
___
    &bn_mod_add(".Lpoly");
$code.=<<___;
	ret
.size ecp_sm2p256_add,.-ecp_sm2p256_add

// void ecp_sm2p256_sub(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_sub
.type	ecp_sm2p256_sub,%function
.p2align 5
ecp_sm2p256_sub:
___
    &bn_mod_sub(".Lpoly");
$code.=<<___;
	ret
.size ecp_sm2p256_sub,.-ecp_sm2p256_sub

// void ecp_sm2p256_sub_mod_ord(BN_ULONG *r,const BN_ULONG *a,const BN_ULONG *b);
.globl	ecp_sm2p256_sub_mod_ord
.type	ecp_sm2p256_sub_mod_ord,%function
.p2align 5
ecp_sm2p256_sub_mod_ord:
___
    &bn_mod_sub(".Lord");
$code.=<<___;
	ret
.size ecp_sm2p256_sub_mod_ord,.-ecp_sm2p256_sub_mod_ord

// void ecp_sm2p256_div_by_2(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_div_by_2
.type	ecp_sm2p256_div_by_2,%function
.p2align 5
ecp_sm2p256_div_by_2:
___
    &bn_mod_div_by_2(".Lpoly_div_2");
$code.=<<___;
	ret
.size ecp_sm2p256_div_by_2,.-ecp_sm2p256_div_by_2

// void ecp_sm2p256_div_by_2_mod_ord(BN_ULONG *r,const BN_ULONG *a);
.globl	ecp_sm2p256_div_by_2_mod_ord
.type	ecp_sm2p256_div_by_2_mod_ord,%function
.p2align 5
ecp_sm2p256_div_by_2_mod_ord:
___
    &bn_mod_div_by_2(".Lord_div_2");
$code.=<<___;
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
	add $s0, $t6, $t7
	sltu $c0, $s0, $t6
	add $s0, $s0, $t7
	sltu $c1, $s0, $t7
	add $s10, $c0, $c1  //t6+t7+t7 carry

	// s2=t4+t5+s0
	add $s2, $t4, $t5
	sltu $c2, $s2, $t4
	add $c0, $c2, $s10
	add $s2, $s2, $s0
	sltu $c3, $s2, $s0
	add $c0, $c0, $c3  //t4+t5+t6+t7+t7 carry

	// sum
	add $t0, $t0, $s2 // t0 += (t4 + t5 + t6 + 2 * t7)
	sltu $c1, $t0, $s2
	add $t1, $t1, $c0
	sltu $c0, $t1, $c0
	add $t1, $t1, $c1
	sltu $c1, $t1, $c1
	add $c0, $c0, $c1
	add $t2, $t2, $s0  //t2 += t6 + 2 * t7
	sltu $c2, $t2, $s0
	add $t2, $t2, $c0
	sltu $c0, $t2, $c0
	add $c0, $c0, $c2
	add $t3, $t3, $t7 // t3 += t7
	sltu $c1, $t3, $t7
	add $t3, $t3, $c0
	sltu $c0, $t3, $c0
	add $c0, $c0, $c1
	add $t3, $t3, $s10
	sltu $c1, $t3, $s10
	add $s8, $c0, $c1  // s8 = carry from t3

	// 2. 64-bit to 32-bit spread
	zext.w $s0, $t4
	zext.w $s2, $t5
	zext.w $s4, $t6
	zext.w $s6, $t7

	srli $s1, $t4, 32
	srli $s3, $t5, 32
	srli $s5, $t6, 32
	srli $s7, $t7, 32

	// 3. 32-bit addition
	add $c0, $s4, $s6
	add $c1, $s5, $s7
	add $c2, $s0, $s1
	add $t5, $s6, $s2  // t5 :s2 + s6
	add $t6, $s7, $s3  // t6: s3 + s7
	add $s4, $c1, $c0  // s4 + s5 + s6 + s7
	add $s2, $s2, $s4
	add $s2, $s2, $s4
	add $c3, $s3, $c2
	add $t7, $s2, $c3  //  t7: s0 + s1 + s2 + s3 + 2*(s4 + s5 + s6 + s7)
	add $s4, $s4, $s5
	add $s4, $s4, $s3
	add $s4, $s4, $s0  // s0 + s3 + s4 + s5*2 + s6 + s7
	add $c2, $c2, $s6
	add $s2, $c2, $s5  // s2: s0 + s1 + s5 + s6
	add $t4, $s1, $c1  // t4: s1 +s5 +s7
	add $s3, $s3, $t4
	add $s0, $s3, $c1  // s0: s1 + s3 + (s5 + s7)*2
	add $s1, $c0, $t5  // s1: s2 + s4 + s6*2
	mv $s3, $s4   // s3: s4 + s5 + s6 + s7

	// 4. 32-bit to 64-bit
	slli $s9, $s1, 32
	slli $c0, $s3, 32
	srli $c1, $s1, 32
	or $s1, $c1, $c0
	slli $c2, $t5, 32
	srli $c3, $s3, 32
	or $s3, $c3, $c2
	slli $c0, $t7, 32
	srli $c1, $t5, 32
	or $t5, $c0, $c1
	srli $t7, $t7, 32

	// 5. 64-bit addition
	add $s0, $s0, $s9
	sltu $c0, $s0, $s9
	add $s1, $s1, $c0
	sltu $c1, $s1, $c0
	add $s3, $t4, $s3
	sltu $c2, $s3, $t4
	add $s3, $s3, $c1
	sltu $c3, $s3, $c1
	add $c0, $c2, $c3
	add $s4, $t6, $t5
	sltu $c1, $s4, $t6
	add $s4, $s4, $c0
	sltu $c2, $s4, $c0
	add $c0, $c1, $c2
	add $c0, $c0, $s8
	add $c0, $c0, $t7

	// V[0]	s0
	// V[1]	s1
	// V[2]	s3
	// V[3]	s4
	// carry	c0
	// sub	s2

	// add with V0-V3
	add $t0, $t0, $s0
	sltu $c1, $t0, $s0
	add $t1, $t1, $s1
	sltu $c2, $t1, $s1
	add $t1, $t1, $c1
	sltu $c1, $t1, $c1
	add $c1, $c1, $c2
	add $t2, $t2, $s3
	sltu $c2, $t2, $s3
	add $t2, $t2, $c1
	sltu $c1, $t2, $c1
	add $c1, $c1, $c2
	add $t3, $t3, $s4
	sltu $c2, $t3, $s4
	add $t3, $t3, $c1
	sltu $c1, $t3, $c1
	add $c1, $c1, $c2
	add $c0, $c0, $c1
	// sub with s2
	sltu $c2, $t1, $s2
	sub $t1, $t1, $s2
	sltu $c3, $t2, $c2
	sub $t2, $t2, $c2
	sltu $c1, $t3, $c3
	sub $t3, $t3, $c3
	sub $c0, $c0, $c1

	// 6. MOD
	// First Mod
	slli $s5, $c0, 32
	sub $s6, $s5, $c0

	add $t0, $t0, $c0
	sltu $c1, $t0, $c0
	add $t1, $t1, $s6
	sltu $c2, $t1, $s6
	add $t1, $t1, $c1
	sltu $c1, $t1, $c1
	add $c1, $c1, $c2
	add $t2, $t2, $c1
	sltu $c1, $t2, $c1
	add $t3, $t3, $s5
	sltu $c2, $t3, $s5
	add $t3, $t3, $c1
	sltu $c1, $t3, $c1
	add $c0, $c2, $c1

	// Last Mod
	// return y - p if y > p else y
	la $i2, .Lpoly
	ld $s0, 0($i2)
	ld $s1, 8($i2)
	ld $s2, 16($i2)
	ld $s3, 24($i2)

	sltu $c1, $t0, $s0
	sub $t4, $t0, $s0
	sltu $c2, $t1, $s1
	sub $t5, $t1, $s1
	sltu $c3, $t5, $c1
	sub $t5, $t5, $c1
	add $c1, $c2, $c3
	sltu $c2, $t2, $s2
	sub $t6, $t2, $s2
	sltu $c3, $t6, $c1
	sub $t6, $t6, $c1
	add $c1, $c2, $c3
	sltu $c2, $t3, $s3
	sub $t7, $t3, $s3
	sltu $c3, $t7, $c1
	sub $t7, $t7, $c1
	add $c1, $c2, $c3

	slt $c0, $c0, $c1
	negw $c1, $c0
	addw $c0, $c0, -1

	and $t0, $t0, $c1
	and $t1, $t1, $c1
	and $t2, $t2, $c1
	and $t3, $t3, $c1
	and $t4, $t4, $c0
	and $t5, $t5, $c0
	and $t6, $t6, $c0
	and $t7, $t7, $c0
	or $t0, $t0, $t4
	or $t1, $t1, $t5
	or $t2, $t2, $t6
	or $t3, $t3, $t7

	sd $t0, 0($i0)
	sd $t1, 8($i0)
	sd $t2, 16($i0)
	sd $t3, 24($i0)
.endm


// void ecp_sm2p256_mul(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b);
.globl	ecp_sm2p256_mul
.type	ecp_sm2p256_mul,%function
.p2align 5
ecp_sm2p256_mul:
___
$code.= save_regs();
$code.=<<___;
	// Load inputs
	ld $t0, 0($i1)
	ld $t1, 8($i1)
	ld $t2, 16($i1)
	ld $t3, 24($i1)
	ld $t4, 0($i2)
	ld $t5, 8($i2)
	ld $t6, 16($i2)
	ld $t7, 24($i2)
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
	mul $s0, $t0, $t4
	mulhu $s1, $t0, $t4

//	### t0*t5+t1*t4 ###
	mul $s2, $t0, $t5
	mulhu $s3, $t0, $t5
	mul $s4, $t1, $t4
	mulhu $s5, $t1, $t4

	add $s1, $s1, $s2
	sltu $c0, $s1, $s2
	add $s3, $s3, $s5
	sltu $c1, $s3, $s5

	add $s1, $s1, $s4
	sltu $c2, $s1, $s4
	add $c0, $c0, $c2
	add $s3, $s3, $c0
	sltu $c0, $s3, $c0
	add $c0, $c0, $c1

//	### t0*t6+t1*t5+t2*t4 ###
	mul $s2, $t0, $t6
	mulhu $s4, $t0, $t6
	mul $s5, $t1, $t5
	mulhu $s6, $t1, $t5
	mul $s7, $t2, $t4
	mulhu $s8, $t2, $t4

	add $s2, $s2, $s3
	sltu $c1, $s2, $s3
	add $s5, $s5, $s7
	sltu $c3, $s5, $s7
	add $s2, $s2, $s5
	sltu $c2, $s2, $s5
	add $c1, $c1, $c3
	add $c0, $c0, $c2
	add $c0, $c0, $c1

	add $s4, $s4, $s6
	sltu $c2, $s4, $s6
	add $s8, $s8, $c0
	sltu $c0, $s8, $c0
	add $s4, $s4, $s8
	sltu $c3, $s4, $s8
	add $c0, $c2, $c0
	add $c0, $c0, $c3

//	### t0*t7+t1*t6+t2*t5+t3*t4 ###
	mul $s3, $t0, $t7
	mulhu $s5, $t0, $t7
	mv $t0, $s0
	mul $s9, $t1, $t6
	mulhu $s10, $t1, $t6
	mul $s0, $t2, $t5
	mulhu $s6, $t2, $t5
	mul $s7, $t3, $t4
	mulhu $s8, $t3, $t4

	add $s3, $s3, $s9
	sltu $c1, $s3, $s9
	add $s0, $s0, $s4
	sltu $c2, $s0, $s4
	add $c1, $c1, $c2
	add $s3, $s3, $s7
	sltu $c3, $s3, $s7
	add $s3, $s3, $s0
	sltu $c2, $s3, $s0
	add $c3, $c3, $c2
	add $c0, $c0, $c1
	add $c0, $c0, $c3

	add $s5, $s5, $s10
	sltu $c1, $s5, $s10
	add $s6, $s6, $s8
	sltu $c2, $s6, $s8
	add $c1, $c1, $c2
	add $s5, $s5, $c0
	sltu $c0, $s5, $c0
	add $s5, $s5, $s6
	sltu $c3, $s5, $s6
	add $c0, $c0, $c1
	add $c0, $c0, $c3

//	### t1*t7+t2*t6+t3*t5 ###
	mul $s4, $t1, $t7
	mulhu $s6, $t1, $t7
	mv $t1, $s1
	mul $s0, $t2, $t6
	mulhu $s1, $t2, $t6
	mul $s7, $t3, $t5
	mulhu $s8, $t3, $t5

	add $s4, $s4, $s5
	sltu $c1, $s4, $s5
	add $s0, $s0, $s7
	sltu $c2, $s0, $s7
	add $c1, $c1, $c2
	add $t4, $s4, $s0
	sltu $c3, $t4, $s0
	add $c1, $c1, $c3
	add $c0, $c0, $c1

	add $s6, $s6, $s1
	sltu $c2, $s6, $s1
	add $s8, $s8, $c0
	sltu $c0, $s8, $c0
	add $c0, $c0, $c2
	add $s6, $s6, $s8
	sltu $c3, $s6, $s8
	add $c0, $c0, $c3

//	### t2*t7+t3*t6 ###
	mul $s0, $t2, $t7
	mulhu $s1, $t2, $t7
	mv $t2, $s2
	mul $s5, $t3, $t6
	mulhu $t6, $t3, $t6

	add $s0, $s0, $s5
	sltu $c1, $s0, $s5
	add $t5, $s0, $s6
	sltu $c2, $t5, $s6
	add $c0, $c0, $c1
	add $c0, $c0, $c2

	add $t6, $t6, $s1
	sltu $c1, $t6, $s1
	add $t6, $t6, $c0
	sltu $c0, $t6, $c0
	add $c0, $c0, $c1

//	### t3*t7 ###
	mul $s2, $t3, $t7
	mulhu $t7, $t3, $t7
	mv $t3, $s3

	add $t6, $t6, $s2
	sltu $c1, $t6, $s2
	add $t7, $t7, $c0
	add $t7, $t7, $c1

	// ### Reduction ###
	RDC
___
$code.= load_regs();
$code.=<<___;
	ret
.size ecp_sm2p256_mul,.-ecp_sm2p256_mul

// void ecp_sm2p256_sqr(BN_ULONG *r, const BN_ULONG *a);
.globl	ecp_sm2p256_sqr
.type	ecp_sm2p256_sqr,%function
.p2align 5
ecp_sm2p256_sqr:
___
$code.= save_regs();
$code.=<<___;
	// Load inputs
	ld $s0, 0($i1)
	ld $s1, 8($i1)
	ld $s2, 16($i1)
	ld $s3, 24($i1)

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
	mul $t0, $s0, $s0
	mulhu $t1, $s0, $s0

// ### s0*s1*2 ###
	mul $s4, $s0, $s1
	mulhu $s5, $s0, $s1

	slli $s6, $s4, 1
	sltu $c0, $s6, $s4
	add $t1, $t1, $s6
	sltu $c1, $t1, $s6
	add $c0, $c0, $c1

	slli $t2, $s5, 1
	sltu $c2, $t2, $s5
	add $t2, $t2, $c0
	sltu $c3, $t2, $c0
	add $c0, $c2, $c3

// ### s0*s2*2+s1*s1 ###
	mul $s7, $s0, $s2
	mulhu $s8, $s0, $s2
	mul $s9, $s1, $s1
	mulhu $s10, $s1, $s1

	slli $t4, $s7, 1
	sltu $c1, $t4, $s7
	add $t2, $t2, $s9
	sltu $c2, $t2, $s9
	add $c0, $c0, $c1
	add $t2, $t2, $t4
	sltu $c3, $t2, $t4
	add $c0, $c0, $c2
	add $c0, $c0, $c3

	slli $t3, $s8, 1
	sltu $c1, $t3, $s8
	add $s10, $s10, $c0
	sltu $c0, $s10, $c0
	add $t3, $t3, $s10
	sltu $c2, $t3, $s10
	add $c0, $c0, $c1
	add $c0, $c0, $c2

// ### (s0*s3+s1*s2)*2 ###
	mul $s4, $s0, $s3
	mulhu $s5, $s0, $s3
	mul $s6, $s1, $s2
	mulhu $s7, $s1, $s2

	add $s4, $s4, $s6
	sltu $c1, $s4, $s6
	slli $c1, $c1, 1
	slli $s6, $s4, 1
	sltu $c2, $s6, $s4
	add $c1, $c1, $c2
	add $t3, $t3, $s6
	sltu $c3, $t3, $s6
	add $c0, $c0, $c1
	add $c0, $c0, $c3

	add $s5, $s5, $s7
	sltu $c1, $s5, $s7
	slli $c1, $c1, 1
	slli $s8, $s5, 1
	sltu $c2, $s8, $s5
	add $c1, $c1, $c2
	add $t4, $c0, $s8
	sltu $c3, $t4, $s8
	add $c0, $c1, $c3

// ### s1*s3*2+s2*s2 ###
	mul $s4, $s1, $s3
	mulhu $s5, $s1, $s3
	mul $s6, $s2, $s2
	mulhu $s7, $s2, $s2

	slli $s8, $s4, 1
	sltu $c1, $s8, $s4
	add $c0, $c0, $c1
	add $t4, $t4, $s6
	sltu $c2, $t4, $s6
	add $c0, $c0, $c2
	add $t4, $t4, $s8
	sltu $c3, $t4, $s8
	add $c0, $c0, $c3

	slli $s9, $s5, 1
	sltu $c1, $s9, $s5
	add $t5, $c0, $s7
	sltu $c2, $t5, $s7
	add $c0, $c1, $c2
	add $t5, $t5, $s9
	sltu $c3, $t5, $s9
	add $c0, $c0, $c3

// ### s2*s3*2 ###
	mul $s4, $s2, $s3
	mulhu $s5, $s2, $s3

	slli $s6, $s4, 1
	sltu $c1, $s6, $s4
	add $t5, $t5, $s6
	sltu $c2, $t5, $s6
	add $c0, $c0, $c1
	add $c0, $c0, $c2

	slli $t6, $s5, 1
	sltu $c3, $t6, $s5
	add $t6, $t6, $c0
	sltu $c0, $t6, $c0
	add $c0, $c0, $c3

// ### s3*s3 ###
	mul $s8, $s3, $s3
	mulhu $s9, $s3, $s3

	add $t6, $t6, $s8
	sltu $c1, $t6, $s8

	add $t7, $s9, $c0
	add $t7, $t7, $c1

	// ### Reduction ###
	RDC
___
$code.= load_regs();
$code.=<<___;
	ret
.size ecp_sm2p256_sqr,.-ecp_sm2p256_sqr
___
}

foreach (split("\n",$code)) {
    s/\`([^\`]*)\`/eval $1/ge;

    print $_,"\n";
}
close STDOUT or die "error closing STDOUT: $!";		# enforce flush
