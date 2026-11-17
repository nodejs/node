#! /usr/bin/env perl
# Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
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

################################################################################
# Register assignment
################################################################################

# Function arguments
#      RISC-V    ABI
# $rp   x10     a0  # BN_ULONG *rp
# $ap   x11     a1  # const BN_ULONG *ap
# $bp   x12     a2  # const BN_ULONG *bp
# $np   x13     a3  # const BN_ULONG *np
# $n0   x14     a4  # const BN_ULONG *n0
# $num  x15     a5  # int num
my ($rp,$ap,$bp,$np,$n0,$num) = use_regs(10,11,12,13,14,15);

# Return address and Frame pointer
#      RISC-V    ABI
# $ra   x1       ra
# $fp   x8       s0
my ($ra,$fp) = use_regs(1,8);

# Temporary variable allocation
#      RISC-V    ABI
# $lo0  x5     t0    the sum of partial products of a and b
# $hi0  x6     t1    the high word of partial product of a and b + Carry
# $aj   x7     t2    ap[j]
# $m0   x28    t3    bp[i]
# $alo  x29    t4    the low word of partial product
# $ahi  x30    t5    the high word of partial product
# $lo1  x31    t6    partial product + reduction term
# $hi1  x18    s2    the high word of reduction term + Carry
# $nj   x19    s3    np[j],modulus
# $m1   x20    s4    montgomery reduction coefficient
# $nlo  x21    s5    the low word of reduction term
# $nhi  x22    s6    the high word of reduction term
# $ovf  x23    s7    highest carry bit,overflow flag
# $i    x24    s8    outer loop index
# $j    x25    s9    inner loop index
# $tp   x26    s10   temporary result storage
# $tj   x27    s11   tp[j],temporary result value
# $temp x9     s1
my ($lo0,$hi0,$aj,$m0,$alo,$ahi,$lo1,$hi1,$nj,$m1,$nlo,$nhi,$ovf,$i,$j,$tp,$tj,$temp) = use_regs(5..7,28..31,18..27,9);

# Carry variable
# $carry1 x16      a6
# $carry2 x17      a7
my ($carry1,$carry2,$numtst) = use_regs(16,17,17);

my $code .= <<___;
.text
.balign 32
.globl bn_mul_mont
.type bn_mul_mont,\@function
bn_mul_mont:
    andi  $numtst, $num, 7
    beqz  $numtst, bn_sqr8x_mont
.Lmul_mont:
___

$code .= save_regs();

$code .= <<___;
    mv $fp, sp
___

$code .= <<___;
    ld $m0, 0($bp)    # bp[0]
    addi $bp, $bp,8
    ld $hi0, 0($ap)    # ap[0]
    slli $num, $num, 3
    sub $tp, sp, $num
    ld $aj, 8($ap)    # ap[1]
    addi $ap, $ap, 16
    ld $n0, 0($n0)    # n0,precomputed modular inverse
    andi $tp, $tp, -16    # address alignment
    ld $hi1, 0($np)    # np[0]
    mv sp, $tp    # alloca
    ld $nj, 8($np)    # np[1]
    addi $np, $np, 16


    mul $lo0, $hi0, $m0    # ap[0]*bp[0]
    addi $j, $num, -16    # $j=(num-2)*8
    mulhu $hi0, $hi0, $m0
    mul $alo, $aj, $m0    # ap[1]*bp[0]
    mulhu $ahi, $aj, $m0

    mul $m1, $lo0, $n0    # montgomery reduction coefficient tp[0]*n0
    # montgomery optimization: np[0]*m1 ensures (np[0]*m1+lo0) has zero lower bits
    # only carry status needed, not full lo1 result
    # eliminates mul/adds instructions → Saves cycles & power
    # mul $lo1, $hi1, $m1   // np[0]*m1
    # adds $lo1, $lo1, $lo0   // discarded
    mulhu $hi1, $hi1, $m1
    snez $carry1, $lo0
    mul $nlo, $nj, $m1    # np[1]*m1
    add $hi1, $hi1, $carry1
    mulhu $nhi, $nj, $m1
    beqz $j, .L1st_last_entry

.L1st:
    ld $aj, 0($ap)
    addi $ap, $ap, 8

    # compute the sum of partial products of a and b
    add $lo0, $alo, $hi0    # {ap[j-1]*bp[0],low}+{ap[j-2]*bp[0],high}, j ranges from 2 to num-1
    sltu $carry1, $lo0, $alo
    add $hi0, $ahi, $carry1    # {ap[j-1]*bp[0],high}+C_lo0, j ranges from 2 to num-1

    addi $j, $j, -8    # $j--, $j ranges from (num-2)*8 to 0
    ld $nj, 0($np)
    addi $np, $np, 8

    # compute the sum of reduction term
    add $lo1, $nlo, $hi1    # {np[j-1]*m1,low}+{np[j-2]*m1,high}, j ranges from 2 to num-1
    sltu $carry1, $lo1, $nlo
    mul $alo, $aj, $m0    # ap[j]*bp[0], j ranges from 2 to num-1
    add $hi1, $nhi, $carry1    # {np[j-1]*m1,high}+C_lo1, j ranges from 2 to num-1
    mulhu $ahi, $aj, $m0
    # partial product + reduction term
    add $lo1, $lo1, $lo0
    sltu $carry1, $lo1, $lo0
    mul $nlo, $nj, $m1    # np[j]*m1, j ranges from 2 to num-1
    add $hi1, $hi1, $carry1
    mulhu $nhi, $nj, $m1
    sd $lo1, 0($tp)    # tp[j-2], j ranges from 2 to num-1
    addi $tp, $tp, 8

    bnez $j, .L1st

.L1st_last_entry:
    # last partial product
    add $lo0, $alo, $hi0    # {ap[j]*bp[0],low}+{ap[j-1]*bp[0],high}, j is num-1
    sltu $carry1, $lo0, $alo
    sub $ap, $ap, $num    # rewind $ap
    add $hi0, $ahi, $carry1    # {ap[j]*bp[0],high}+C_lo0, j is num-1

    # last reduction term
    add $lo1, $nlo, $hi1    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu $carry1, $lo1, $nlo
    sub $np, $np, $num    # rewind $np
    add $hi1, $nhi, $carry1    # {np[j]*m1,high}+C_lo1, j is num-1

    # last partial product + last reduction term
    add $lo1, $lo1, $lo0
    sltu $carry1, $lo1, $lo0

    add $hi1, $hi1, $hi0
    sltu $carry2, $hi1, $hi0
    add $hi1, $hi1, $carry1
    sltu $ovf, $hi1, $carry1
    or $carry1, $carry2, $ovf    # carry2 and ovf are mutually exclusive, both cannot be 1 simultaneously

    mv $ovf, $carry1    # upmost overflow bit

    addi $i, $num, -8    # $i=(num-1)*8

    sd $lo1, 0($tp) # tp[j-1], j is num-1
    sd $hi1, 8($tp) # tp[j], j is num-1

.Louter:
    ld $m0, 0($bp)    # bp[i], i ranges from 1 to num-1
    addi $bp, $bp, 8
    ld $hi0, 0($ap)
    ld $aj, 8($ap)
    addi $ap, $ap, 16
    ld $tj, 0(sp)    # tp[0]
    addi $tp, sp, 8    # tp[1]

    mul $lo0, $hi0, $m0    # ap[0]*bp[i], i ranges from 1 to num-1
    addi $j, $num,-16    # $j=(num-2)*8
    mulhu $hi0, $hi0, $m0
    ld $hi1, 0($np)
    ld $nj, 8($np)
    addi $np, $np, 16

    mul $alo, $aj, $m0    # ap[1]*bp[i], i ranges from 1 to num-1
    add $lo0, $lo0, $tj    # ap[0]*bp[i] + last_tp[0] , i ranges from 1 to num-1
    sltu $carry1, $lo0, $tj
    mulhu $ahi, $aj, $m0
    add $hi0, $hi0, $carry1    # $hi0 will not overflow

    # compute the modular reduction coefficient
    mul $m1, $lo0, $n0

    addi $i, $i, -8    # $i--, $i ranges from (num-1)*8 to 0

    mulhu $hi1, $hi1, $m1
    snez $carry1, $lo0
    mul $nlo, $nj, $m1    # np[1]*m1
    mulhu $nhi, $nj, $m1

    beqz $j, .Linner_last_entry

.Linner:
    ld $aj, 0($ap)
    addi $ap, $ap, 8
    add $hi1, $hi1, $carry1

    ld $tj, 0($tp)    # tp[j-1], j is 2 to num-1
    addi $tp, $tp, 8

    # compute the sum of partial products of a and b
    add $lo0, $alo, $hi0    # {ap[j-1]*bp[i],low}+{ap[j-2]*bp[i],high}, j ranges from 2 to num-1, i ranges from 1 to num-1
    sltu $carry1, $lo0, $alo
    add $hi0, $ahi, $carry1    # {ap[j-1]*bp[i],high}+C_lo0, j ranges from 2 to num-1, i ranges from 1 to num-1

    addi $j, $j, -8    # $j--, $j ranges from (num-2)*8 to 0

    # compute the sum of reduction term
    add $lo1, $nlo, $hi1    # {np[j-1]*m1,low}+{np[j-2]*m1,high}, j ranges from 2 to num-1
    sltu $carry1, $lo1, $nlo
    add $hi1, $nhi, $carry1    # {np[j-1]*m1}+C_lo1, j ranges from 2 to num-1

    ld $nj, 0($np)
    addi $np, $np, 8

    mul $alo, $aj, $m0    # ap[j]*bp[i], j ranges from 2 to num-1, i ranges from 1 to num-1
    # partial product + reduction term
    add $lo0, $lo0, $tj
    sltu $carry1, $lo0, $tj
    mulhu $ahi, $aj, $m0
    add $hi0, $hi0, $carry1

    mul $nlo, $nj, $m1    # np[j]*m1, j ranges from 2 to num-1
    add $lo1, $lo1, $lo0
    sltu $carry1, $lo1, $lo0
    mulhu $nhi, $nj, $m1
    sd $lo1, -16($tp)    # tp[j-2], j ranges from 2 to num-1

    bnez $j, .Linner

.Linner_last_entry:
    ld $tj, 0($tp)    # tp[j], j is num-1
    addi $tp, $tp, 8
    add $hi1, $hi1, $carry1

    # last partial product
    add $lo0, $alo, $hi0    # {ap[j]*bp[i],low}+{ap[j-1]*bp[i],high}, j is num-1, i ranges from 1 to num-1
    sltu $carry1, $lo0, $alo
    sub $ap, $ap, $num    # rewind $ap
    add $hi0, $ahi, $carry1    # {ap[j]*bp[i],high}+C_lo0, j is num-1, i ranges from 1 to num-1

    # last reduction term
    add $lo1, $nlo, $hi1    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu $carry1, $lo1, $nlo

    sub $np, $np, $num    # rewind $np

    add $hi1, $nhi, $ovf
    sltu $carry2, $hi1, $ovf
    add $hi1, $hi1, $carry1    # {np[j]*m1,high}+C_lo1, j is num-1
    sltu $ovf, $hi1, $carry1
    or $carry1, $carry2, $ovf

    mv $ovf, $carry1    # update the upmost overflow bit

    # last partial product + last reduction term
    add $lo0, $lo0, $tj
    sltu $carry1, $lo0, $tj
    add $hi0, $hi0, $carry1

    add $lo1, $lo1, $lo0
    sltu $carry1, $lo1, $lo0

    add $hi1, $hi1, $hi0
    sltu $carry2, $hi1, $hi0
    add $hi1, $hi1, $carry1
    sltu $carry1, $hi1, $carry1
    or $carry1, $carry2, $carry1

    add $ovf, $ovf, $carry1    # upmost overflow bit

    sd $lo1, -16($tp)    # tp[j-1], j is num-1
    sd $hi1, -8($tp)    # tp[j], j is num-1
    bnez $i, .Louter

    ld $tj, 0(sp)    # tp[0]
    addi $tp, sp, 8
    ld $nj, 0($np)    # np[0]
    addi $np, $np, 8
    addi $j, $num, -8    # $j=(num-1)*8 and clear borrow

    li $carry1,0   # Custom, no borrow, C=0 (normal case, with borrow C=1)
    mv $ap, $rp
.Lsub:
    # tp[j]-np[j], j ranges from 0 to num-2, set carry flag
    sub $temp, $tj, $nj
    sltu $carry2, $tj, $temp
    sub $aj, $temp, $carry1
    sltu $carry1, $temp, $aj
    or $carry1, $carry2, $carry1

    ld $tj, 0($tp)    # tp[j], j ranges from 1 to num-1
    addi $tp, $tp, 8
    addi $j, $j, -8    # $j--, $j ranges from (num-1)*8 to 0
    ld $nj, 0($np)
    addi $np, $np, 8

    sd $aj, 0($ap)    # rp[j]=tp[j]-np[j], j ranges from 0 to num-2
    addi $ap, $ap, 8
    bnez $j, .Lsub

    # process the last word, tp[j]-np[j], j is num-1
    sub $temp, $tj, $nj
    sltu $carry2, $tj, $temp
    sub $aj, $temp, $carry1
    sltu $carry1, $temp, $aj
    or $carry1, $carry2, $carry1

    # whether there is a borrow
    sub $temp, $ovf, $carry1
    sltu $carry1, $ovf, $temp
    xori $carry1, $carry1, 1

    sd $aj, 0($ap)    # rp[j], j is num-1
    addi $ap, $ap, 8

    # conditional result copying and cleanup
    ld $tj, 0(sp)    # tp[0]
    addi $tp, sp, 8
    ld $aj, 0($rp)    # rp[0]
    addi $rp, $rp, 8
    addi $num, $num, -8    # num--
    nop
.Lcond_copy:
    addi $num,$num, -8    # num--
    # conditionally selects value based on borrow flag:
    # when borrow occurs (borrow flag set): nj = tj (original t value)
    snez $carry1, $carry1
    sub $carry1, zero, $carry1
    xor $nj, $tj, $aj
    and $nj, $nj, $carry1
    xor $nj, $tj, $nj

    ld $tj, 0($tp)
    addi $tp, $tp, 8
    ld $aj, 0($rp)
    addi $rp, $rp, 8
    sd zero, -16($tp)    # wipe tp
    sd $nj, -16($rp)    # result
    bnez $num, .Lcond_copy

    # process the last word
    snez $carry1, $carry1
    sub $carry1, zero, $carry1
    xor $nj, $tj, $aj
    and $nj, $nj, $carry1
    xor $nj, $tj, $nj

    sd zero, -8($tp)    # wipe tp
    sd $nj, -8($rp)
___

$code .= <<___;
    mv sp, $fp
    li $rp, 1
___

$code .= load_regs();

$code .= <<___;
    ret
.size bn_mul_mont,.-bn_mul_mont
___

{
# Following is RISCV64 adaptation of __bn_sqr8x_mont from armv8-mont module.

# Return address and Frame pointer
#      RISC-V    ABI
# $ra   x1       ra
# $fp   x8       s0
my ($ra,$fp) = use_regs(1,8);

# Temporary variable allocation
#      RISC-V    ABI
# $a0   x5     t0
# $a1   x6     t1
# $a2   x7     t2
# $a3   x28    t3
# $a4   x9     s1
# $a5   x18    s2
# $a6   x19    s3
# $a7   x20    s4
my ($a0,$a1,$a2,$a3,$a4,$a5,$a6,$a7) = use_regs(5,6,7,28,9,18,19,20);

# $t0   x16     a6
# $t1   x17     a7
# $t2   x29     t4
# $t3   x30     t5
my ($t0,$t1,$t2,$t3) = use_regs(16,17,29,30);

# $acc0 x21    s5
# $acc1 x22    s6
# $acc2 x23    s7
# $acc3 x24    s8
# $acc4 x25    s9
# $acc5 x31    t6
# $acc6 x27    s11
# $acc7 x26    s10
my ($acc0,$acc1,$acc2,$acc3,$acc4,$acc5,$acc6,$acc7) = use_regs(21,22,23,24,25,31,27,26);

# $temp   x14    a4<n0>
# $carry1 x15    a5<num>
# $carry2 x10    a0<rp>
# $carry  x1     ra
my ($temp, $carry1, $carry2, $carry) = use_regs(14,15,10,1);

# $tp     x12    a2<bp>
# $na0    x1     ra
my ($tp,$na0) = use_regs(12,1);

# Stack variables
# rp       fp+96
# np       fp+104
# num      fp+112
# n0       fp+120
# ra       fp+128
# cnt      fp+136
# ap_end   fp+144
# np_end   fp+152
# topmost  fp+160

# My_function
sub adds {
    # Simulate ARM 'adds'：add with carry flag set
    # (1) Final sum: dst = src1 + b, no input carry
    # (2) Output carry, if src1 + b overflowed, carry = 1. src1 + b < b ? carry1=1 : carry1=0
    my ($dst, $src1, $b) = @_;
    my $code=<<___;
    add $dst, $src1, $b
    sltu $carry1, $dst, $b
___
    return $code;
}

sub adcs {
    # Simulate ARM 'adcs': add with input carry and set output carry
    # (1) Temp sum. dst = src1 + b, ignore input carry
    # (2) Temp1 carry. if src1 + b overflowed, carry2 = 1. src1 + b < b ? carry2=1 : carry2=0
    # (3) Final sum: dst += carry1 (input carry)
    # (4) Temp2 carry. if adding input carry overflowed, carry1 = 1. dst + carry1 < carry1 ? carry1=1 : carry1=0
    # (5) Final carry. carry1(carry_out) = carry2 | carry1
    my ($dst, $src1, $b) = @_;
    my $code=<<___;
    add $dst, $src1, $b
    sltu $carry2, $dst, $b
    add $dst, $dst, $carry1
    sltu $carry1, $dst, $carry1
    or $carry1, $carry2, $carry1
___
    return $code;
}

sub adc {
    # (1) Simulate ARM 'adc': add with input carry1, no flags
    # (2) Temp sum: dst = src1 + b, ignore input carry
    # (3) Final sum: dst += carry1 (input carry), ignore output carry
    my ($dst, $src1, $b) = @_;
    my $code=<<___;
    add $dst,$src1,$b
    add $dst,$dst,$carry1
___
    return $code;
}

sub extr {
    # Simulate ARM 'extr': extract high bit and shift
    # (1) carry2 = b >> 63 (extract highest bit of b)
    # (2) dst = src1 << 1 (shift src1 left by 1, dst = src1*2)
    # (3) dst |= carry2 (combine: insert b's high bit as dst's LSB, dst= carry2 + src1*2 )
    my ($dst, $src1, $b) = @_;
    my $code=<<___;
    srli $carry2, $b, 63
    slli $dst, $src1, 1
    or   $dst, $dst, $carry2
___
    return $code;
}

sub subs{
    # Simulate ARM 'subs': subtract with borrow flag set.
    # (1) dst = src1 - b
    # (2) if src1 < dst, set borrow flag，carry1 = 1
    my ($dst, $src1, $b) = @_;
    my $code=<<___;
    sub $dst, $src1, $b
    sltu $carry1, $src1, $dst
___
    return $code;
}

sub sbcs{
    # Simulate ARM 'sbcs': subtract with input borrow (carry1) and set output borrow.
    # (1) temp = src1 - b
    # (2) if src1 < temp, set borrow flag, carry2 = 1
    # (3) dst = temp - carry1 (borrow_in)
    # (4) if temp < dst, set borrow flag, carry1 = 1
    # (5) carry1 (borrow_out) = carry2 | carry1
    my ($dst, $src1, $b) = @_;
    my $code=<<___;
    sub $temp, $src1, $b
    sltu $carry2, $src1, $temp
    sub $dst, $temp, $carry1
    sltu $carry1, $temp, $dst
    or $carry1, $carry2, $carry1
___
    return $code;
}

sub csel{
    # Simulate ARM 'csel': conditional select based on carry
    # Assumes carry1 is input condition, 1 if true/select b, 0 if false/select src1.
    # dst = (carry1 == 0) ? src1 : b
    # Uses bitwise ops for branchless execution
    # (1) Normalize carry1: carry1 !=0 ? carry1 = 1 : carry1 = 0
    # (2) Create mask: carry1 = 0 - carry1; yields -1 (all 1s) if 1 (true), 0 if false carry1 = -carry1 (0 or -1 mask)
    # (3) Compute diff: dst = src1 ^ b (bitwise XOR highlights differing bits)
    # (4) Mask diff: dst = diff & mask; yields diff if -1 (true), 0 if 0 (false)
    # (5) Select: dst = src1 ^ dst; yields b if dst=diff (true), src1 if dst=0 (false)
    my ($dst, $src1, $b) = @_;
    my $code=<<___;
    snez $carry1, $carry1
    sub $carry1, zero, $carry1
    xor $dst, $src1, $b
    and $dst, $dst, $carry1
    xor $dst, $src1, $dst
___
    return $code;
}

## store
sub sd_rp{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,96($dst)
___
    return $code;
}

sub sd_np{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,104($dst)
___
    return $code;
}

sub sd_num{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,112($dst)
___
    return $code;
}

sub sd_n0{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,120($dst)
___
    return $code;
}

sub sd_ra{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,128($dst)
___
    return $code;
}

sub sd_cnt{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,136($dst)
___
    return $code;
}

sub sd_apend{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,144($dst)
___
    return $code;
}

sub sd_npend{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,152($dst)
___
    return $code;
}

sub sd_topmost{
    my ($src1,$dst) = @_;
    my $code=<<___;
    sd $src1,160($dst)
___
    return $code;
}

## load
sub ld_rp{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,96($src1)
___
    return $code;
}

sub ld_np{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,104($src1)
___
    return $code;
}

sub ld_num{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,112($src1)
___
    return $code;
}

sub ld_n0{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,120($src1)
___
    return $code;
}

sub ld_ra{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,128($src1)
___
    return $code;
}

sub ld_cnt{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,136($src1)
___
    return $code;
}

sub ld_apend{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,144($src1)
___
    return $code;
}

sub ld_npend{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,152($src1)
___
    return $code;
}

sub ld_topmost{
    my ($dst,$src1) = @_;
    my $code=<<___;
    ld $dst,160($src1)
___
    return $code;
}

$code.=<<___;
.type   bn_sqr8x_mont,%function
.balign 32
bn_sqr8x_mont:
    # If ap != bp, -> normal multiplication, jump to .Lmul_mont
    # If ap == bp, -> continue with optimized sqr8x path
    bne $ap,$bp, .Lmul_mont
___
$code .= <<___;
    addi    sp,sp,-168
    sd      s0,88(sp)
    sd      s1,80(sp)
    sd      s2,72(sp)
    sd      s3,64(sp)
    sd      s4,56(sp)
    sd      s5,48(sp)
    sd      s6,40(sp)
    sd      s7,32(sp)
    sd      s8,24(sp)
    sd      s9,16(sp)
    sd      s11,8(sp)
    sd      s10,0(sp)
    mv $fp, sp
___
$code .= <<___;
.Lsqr8x_mont:
    ld $a0, 0($ap)    # load a[0-7]
    ld $a1, 8($ap)
    ld $a2, 16($ap)
    ld $a3, 24($ap)
    ld $a4, 32($ap)
    ld $a5, 40($ap)
    ld $a6, 48($ap)
    ld $a7, 56($ap)

    slli $t0, $num, 4
    sub $tp, sp, $t0    # alloca sp-num*16
    slli $num, $num, 3
    ld $n0, 0($n0)    # n0, precomputed modular inverse
    mv sp, $tp    # alloca

    # Save temporary values to stack to release registers for subsequent operations
    addi $t1,$num,-64    # number of bytes remaining to be zeroed
    @{[sd_cnt $t1,$fp]}    # offload cnt
    @{[sd_rp $rp,$fp]}    # offload rp
    @{[sd_np $np,$fp]}    # offload np
    @{[sd_num $num,$fp]}    # offload num
    @{[sd_n0 $n0,$fp]}    # offload n0
    @{[sd_ra $ra,$fp]}    # offload ra

    j .Lsqr8x_zero_start


    # Clear tp buffer to store partial products
    # Zero 128 bytes per iteration until cache cleared
.Lsqr8x_zero:
    addi $carry2,$carry2,-64    # cnt=cnt-64
    @{[sd_cnt $carry2,$fp]}

    sd zero, 0($tp)
    sd zero, 8($tp)
    sd zero, 16($tp)
    sd zero, 24($tp)
    sd zero, 32($tp)
    sd zero, 40($tp)
    sd zero, 48($tp)
    sd zero, 56($tp)

.Lsqr8x_zero_start:
    sd zero, 64($tp)
    sd zero, 72($tp)
    sd zero, 80($tp)
    sd zero, 88($tp)
    sd zero, 96($tp)
    sd zero, 104($tp)
    sd zero, 112($tp)
    sd zero, 120($tp)
    addi $tp,$tp,128

    @{[ld_cnt $carry2,$fp]}
    bnez $carry2,.Lsqr8x_zero    # cnt != 0 -> Lsqr8x_zero

    @{[ld_num $temp,$fp]}
    add $temp,$ap,$temp    # ap_end = ap + num
    @{[sd_apend $temp,$fp]} # offload ap_end, the last element of ap
    addi $ap,$ap,64    # ap += 64, skip initial 8 64-bit words
    mv $acc0, zero
    mv $acc1, zero
    mv $acc2, zero
    mv $acc3, zero
    mv $acc4, zero
    mv $acc5, zero
    mv $acc6, zero
    mv $acc7, zero
    mv $tp,sp    # points to buffer start

    # Multiply everything but a[i]*a[i]
.balign 16
.Lsqr8x_outer_loop:
    # Compute cross products: a[j] * a[i] for all < j
    #                                                 a[1]a[0]  (i)
    #                                             a[2]a[0]
    #                                         a[3]a[0]
    #                                     a[4]a[0]
    #                                 a[5]a[0]
    #                             a[6]a[0]
    #                         a[7]a[0]
    #                                         a[2]a[1]  (ii)
    #                                     a[3]a[1]
    #                                 a[4]a[1]
    #                             a[5]a[1]
    #                         a[6]a[1]
    #                     a[7]a[1]
    #                                 a[3]a[2]  (iii)
    #                             a[4]a[2]
    #                         a[5]a[2]
    #                     a[6]a[2]
    #                 a[7]a[2]
    #                         a[4]a[3]  (iv)
    #                     a[5]a[3]
    #                 a[6]a[3]
    #             a[7]a[3]
    #                 a[5]a[4]  (v)
    #             a[6]a[4]
    #         a[7]a[4]
    #         a[6]a[5]  (vi)
    #     a[7]a[5]
    # a[7]a[6]  (vii)
    mul $t0,$a1,$a0    # lo(a[1..7]*a[0])  (i)
    mul $t1,$a2,$a0
    mul $t2,$a3,$a0
    mul $t3,$a4,$a0

    @{[adds $acc1, $acc1, $t0]}    # t[1]+lo(a[1]*a[0])
    mul $t0,$a5,$a0
    @{[adcs $acc2, $acc2, $t1]}    # t[2]+lo(a[2]*a[0])
    mul $t1,$a6,$a0
    @{[adcs $acc3, $acc3, $t2]}    # t[3]+lo(a[3]*a[0])
    mul $t2,$a7,$a0
    @{[adcs $acc4, $acc4, $t3]}    # t[4]+lo(a[4]*a[0])

    mulhu $t3,$a1,$a0    # hi(a[1..7]*a[0])
    @{[adcs $acc5, $acc5, $t0]}    # t[5]+lo(a[5]*a[0])
    mulhu $t0,$a2,$a0
    @{[adcs $acc6, $acc6, $t1]}    # t[6]+lo(a[6]*a[0])
    mulhu $t1,$a3,$a0
    @{[adcs $acc7, $acc7, $t2]}    # t[7]+lo(a[7]*a[0])
    mulhu $t2,$a4,$a0
    sd $acc0, 0($tp)    # offload acc0, store t[0]
    add $acc0,zero,$carry1    # t[8]=0+c_pre{t[7]+lo(a[7]*a[0])}
    sd $acc1, 8($tp)    # offload acc1, store t[1]
    addi $tp,$tp,16    # tp=tp+16,advance buffer pointer by 16 bytes

    @{[adds $acc2, $acc2, $t3]}    # t[2]+hi(a[1]*a[0])
    mulhu $t3,$a5,$a0
    @{[adcs $acc3, $acc3, $t0]}    # t[3]+hi(a[2]*a[0])
    mulhu $t0,$a6,$a0
    @{[adcs $acc4, $acc4, $t1]}    # t[4]+hi(a[3]*a[0])
    mulhu $t1,$a7,$a0
    @{[adcs $acc5, $acc5, $t2]}    # t[5]+hi(a[4]*a[0])

    mul $t2,$a2,$a1    # lo(a[2..7]*a[1])  (ii)
    @{[adcs $acc6, $acc6, $t3]}    # t[6]+hi(a[5]*a[0])
    mul $t3,$a3,$a1
    @{[adcs $acc7, $acc7, $t0]}    # t[7]+hi(a[6]*a[0])
    mul $t0,$a4,$a1
    @{[adc $acc0, $acc0, $t1]}    # t[8]+hi(a[7]*a[0])
    mul $t1,$a5,$a1

    @{[adds $acc3, $acc3, $t2]}    # t[3]+ lo(a[2]*a[1])
    mul $t2,$a6,$a1
    @{[adcs $acc4, $acc4, $t3]}    # t[4]+ lo(a[3]*a[1])
    mul $t3,$a7,$a1
    @{[adcs $acc5, $acc5, $t0]}    # t[5]+ lo(a[4]*a[1])
    mulhu $t0,$a2,$a1    # hi(a[2..7]*a[1])
    @{[adcs $acc6,$acc6, $t1]}    # t[6]+ lo(a[5]*a[1])
    mulhu $t1,$a3,$a1
    @{[adcs $acc7, $acc7, $t2]}    # t[7]+ lo(a[6]*a[1])
    mulhu $t2,$a4,$a1
    @{[adcs $acc0, $acc0, $t3]}    # t[8]+ lo(a[7]*a[1])
    mulhu $t3,$a5,$a1
    sd $acc2,0($tp)    # offload acc2, store t[2]
    add $acc1,zero,$carry1    # t[9]=0+c_pre{t[8]+ lo(a[7]*a[1])}
    sd $acc3,8($tp)    # offload acc3, store t[3]
    addi $tp,$tp,16    # tp=tp+16,advance buffer pointer by 16 bytes

    @{[adds $acc4, $acc4, $t0]}    # t[4]+ hi(a[2]*a[1])
    mulhu $t0,$a6,$a1
    @{[adcs $acc5, $acc5, $t1]}    # t[5]+ hi(a[3]*a[1])
    mulhu $t1,$a7,$a1
    @{[adcs $acc6, $acc6, $t2]}    # t[6]+ hi(a[4]*a[1])
    mul $t2,$a3,$a2    # lo(a[3..7]*a[2])  (iii)
    @{[adcs $acc7, $acc7, $t3]}    # t[7]+ hi(a[5]*a[1])
    mul $t3,$a4,$a2
    @{[adcs $acc0, $acc0, $t0]}    # t[8]+ hi(a[6]*a[1])
    mul $t0,$a5,$a2
    @{[adc  $acc1, $acc1, $t1]}    # t[9]+ hi(a[7]*a[1])
    mul $t1,$a6,$a2
    @{[adds $acc5, $acc5, $t2]}    # t[5]+ lo(a[3]*a[2])
    mul $t2,$a7,$a2
    @{[adcs $acc6, $acc6, $t3]}    # t[6]+ lo(a[4]*a[2])
    mulhu $t3,$a3,$a2    # hi(a[3..7]*a[2])
    @{[adcs $acc7, $acc7, $t0]}    # t[7]+ lo(a[5]*a[2])
    mulhu $t0,$a4,$a2
    @{[adcs $acc0, $acc0, $t1]}    # t[8]+ lo(a[6]*a[2])
    mulhu $t1,$a5,$a2
    @{[adcs $acc1, $acc1, $t2]}    # t[9]+ lo(a[7]*a[2])
    mulhu $t2,$a6,$a2
    sd $acc4,0($tp)    # offload acc4, store t[4]
    add $acc2,zero,$carry1    # t[10]=0+c_pre{t[9]+ lo(a[7]*a[2])}
    sd $acc5,8($tp)    # offload acc5, store t[5]
    addi $tp,$tp,16    # tp=tp+16,advance buffer pointer by 16 bytes

    @{[adds $acc6, $acc6, $t3]}    # t[6]+ hi(a[3]*a[2])
    mulhu $t3,$a7,$a2
    @{[adcs $acc7, $acc7, $t0]}    # t[7]+ hi(a4]*a[2])
    mul $t0,$a4,$a3  # lo(a[4..7]*a[3])  (iv)
    @{[adcs $acc0, $acc0, $t1]}    # t[8]+ hi(a5]*a[2])
    mul $t1,$a5,$a3
    @{[adcs $acc1, $acc1, $t2]}    # t[9]+ hi(a6]*a[2])
    mul $t2,$a6,$a3
    @{[adc  $acc2, $acc2, $t3]}    # t[10]+ hi(a7]*a[2])
    mul $t3,$a7,$a3

    @{[adds $acc7, $acc7, $t0]}    # t[7]+ lo(a[4]*a[3])
    mulhu $t0,$a4,$a3   # hi(a[4..7]*a[3])
    @{[adcs $acc0, $acc0, $t1]}    # t[8]+ lo(a[5]*a[3])
    mulhu $t1,$a5,$a3
    @{[adcs $acc1, $acc1, $t2]}    # t[9]+ lo(a[6]*a[3])
    mulhu $t2,$a6,$a3
    @{[adcs $acc2, $acc2, $t3]}    # t[10]+ lo(a[7]*a[3])
    mulhu $t3,$a7,$a3
    sd $acc6,0($tp)    # offload acc6, store t[6]
    add $acc3,zero,$carry1    # t[11]=0+c_pre{t[10]+ lo(a[7]*a[3])}
    sd $acc7,8($tp)    # offload acc7, store t[7]
    addi $tp,$tp,16    # tp=tp+16, advance buffer pointer by 16 bytes

    @{[adds $acc0, $acc0, $t0]}    # t[8]+ hi(a[4]*a[3])
    mul $t0,$a5,$a4    # lo(a[5..7]*a[4])  (v)
    @{[adcs $acc1, $acc1, $t1]}    # t[9]+ hi(a[5]*a[3])
    mul $t1,$a6,$a4
    @{[adcs $acc2, $acc2, $t2]}    # t[10]+ hi(a[6]*a[3])
    mul $t2,$a7,$a4
    @{[adc  $acc3, $acc3, $t3]}    # t[11]+ hi(a[7]*a[3])

    mulhu $t3,$a5,$a4    # hi(a[5..7]*a[4])
    @{[adds $acc1, $acc1, $t0]}    # t[9]+ lo(a[5]*a[4])
    mulhu $t0,$a6,$a4
    @{[adcs $acc2, $acc2, $t1]}    # t[10]+ lo(a[6]*a[4])
    mulhu $t1,$a7,$a4
    @{[adcs $acc3, $acc3, $t2]}    # t[11]+ lo(a[7]*a[4])
    mul $t2,$a6,$a5    # lo(a[6..7]*a[5])  (vi)
    add $acc4,zero,$carry1    # t[12]=0+c_pre{t[11]+ lo(a[7]*a[4])}

    @{[adds $acc2, $acc2, $t3]}    # t[10]+ hi(a[5]*a[4])
    mul $t3,$a7,$a5
    @{[adcs $acc3, $acc3, $t0]}    # t[11]+ hi(a[6]*a[4])
    mulhu $t0,$a6,$a5    # hi(a[6..7]*a[5])
    @{[adc $acc4, $acc4, $t1]}    # t[12]+ hi(a[7]*a[4])
    mulhu $t1,$a7,$a5

    @{[adds $acc3, $acc3, $t2]}    # t[11]+ lo(a[6]*a[5])
    mul $t2,$a7,$a6    # lo(a[7]*a[6])  (vii)
    @{[adcs $acc4,$acc4, $t3]}    # t[12]+ lo(a[7]*a[5])
    mulhu $t3,$a7,$a6    # hi(a[7]*a[6])
    add $acc5,zero,$carry1    # t[13] =0+c_pre{t[12]+lo(a[7]*a[5])}

    @{[adds $acc4, $acc4, $t0]}    # t[12]+ hi(a[6]*a[5])

    @{[ld_apend $temp,$fp]} # load ap_end
    sub $temp,$temp,$ap    # cnt = ap_end-ap, done yet?
    @{[sd_cnt $temp,$fp]}

    @{[adc $acc5, $acc5, $t1]}    # t[13]+ hi(a[7]*a[5])

    @{[adds $acc5, $acc5, $t2]}     # t[13]+ lo(a[7]*a[6])
    @{[ld_num $temp,$fp]}
    @{[ld_apend $carry2,$fp]}
    sub $t0, $carry2, $temp    # rewinded ap
    add $acc6,zero,$carry1    # t[14]=0+c_pre{t[13]+ lo(a[7]*a[6])}
    add $acc6,$acc6,$t3    # t[14] + hi(a[7]*a[6])

    # Check if we have processed all elements of a:
    # If no remaining elements, jump to next step Lsqr8x_outer_break
    # Otherwise, load previous partial product from temp buffer,
    # add new a product, and continue inner loop .Lsqr8x_mul.
    @{[ld_cnt $temp,$fp]}  # load cnt, cnt = ap_end-ap
    beqz $temp, .Lsqr8x_outer_break

    mv $temp,$a0    # a0->temp,reuse register'temp'
    # loads next batch of data from temporary buffer
    ld $a0,0($tp)
    ld $a1,8($tp)
    ld $a2,16($tp)
    ld $a3,24($tp)
    ld $a4,32($tp)
    ld $a5,40($tp)
    ld $a6,48($tp)
    ld $a7,56($tp)

    # accumulate new data
    @{[adds $acc0, $acc0, $a0]}    # t[8]~t[14]...
    ld $a0,0($ap)    # load new data, a[8..15]...
    @{[adcs $acc1, $acc1, $a1]}
    ld $a1,8($ap)
    @{[adcs $acc2, $acc2, $a2]}
    ld $a2,16($ap)
    @{[adcs $acc3, $acc3, $a3]}
    ld $a3,24($ap)
    @{[adcs $acc4, $acc4, $a4]}
    ld $a4,32($ap)
    @{[adcs $acc5, $acc5, $a5]}
    ld $a5,40($ap)
    @{[adcs $acc6, $acc6, $a6]}
    ld $a6,48($ap)

    mv $np,$ap    # ap->np,reuse register'np',a[8]
    @{[adcs $acc7, "zero", $a7]}    # t[7]...
    ld $a7,56($ap)
    add $ap,$ap,64    # move pointer forward by 64 bits (8 bytes),a[16]

    li $carry2,-64    # cnt=-64, set loop count
    @{[sd_cnt $carry2,$fp]}

    # Compute cross products: (current 8 elements) x (remaining a)
    # Accumulate results in RAX (accumulator)
    #
    #                                                         a[8]a[0]
    #                                                     a[9]a[0]
    #                                                 a[a]a[0]
    #                                             a[b]a[0]
    #                                         a[c]a[0]
    #                                     a[d]a[0]
    #                                 a[e]a[0]
    #                             a[f]a[0]
    #                                                     a[8]a[1]
    #                         a[f]a[1]........................
    #                                                 a[8]a[2]
    #                     a[f]a[2]........................
    #                                             a[8]a[3]
    #                 a[f]a[3]........................
    #                                         a[8]a[4]
    #             a[f]a[4]........................
    #                                     a[8]a[5]
    #         a[f]a[5]........................
    #                                 a[8]a[6]
    #     a[f]a[6]........................
    #                             a[8]a[7]
    # a[f]a[7]........................
 .Lsqr8x_mul:
    mul $t0,$a0,$temp    # lo(a[i+8..i+15]*a[i]),..,lo(a[i+8..i+15]*a[i+7])
    add $carry,zero,$carry1    # carry bit
    mul $t1,$a1,$temp

    @{[ld_cnt $carry2,$fp]}
    add $carry2,$carry2,8    # cnt=cnt+8
    @{[sd_cnt $carry2,$fp]}

    mul $t2,$a2,$temp
    mul $t3,$a3,$temp
    @{[adds $acc0,$acc0,$t0]}
    mul $t0,$a4,$temp
    @{[adcs $acc1,$acc1,$t1]}
    mul $t1,$a5,$temp
    @{[adcs $acc2,$acc2,$t2]}
    mul $t2,$a6,$temp
    @{[adcs $acc3, $acc3, $t3]}
    mul $t3,$a7,$temp
    @{[adcs $acc4, $acc4, $t0]}
    mulhu $t0,$a0,$temp    # hi(a[i+8..i+15]*a[i]),..,lo(a[i+8..i+15]*a[i+7])
    @{[adcs $acc5, $acc5, $t1]}
    mulhu $t1,$a1,$temp
    @{[adcs $acc6, $acc6, $t2]}
    mulhu $t2,$a2,$temp
    @{[adcs $acc7, $acc7, $t3]}
    mulhu $t3,$a3,$temp
    add $carry,$carry,$carry1
    sd $acc0,0($tp)
    addi $tp,$tp,8    # move pointer forward by 8 bits (1 bytes)

    @{[adds $acc0, $acc1, $t0]}
    mulhu $t0,$a4,$temp
    @{[adcs $acc1, $acc2, $t1]}
    mulhu $t1,$a5,$temp
    @{[adcs $acc2, $acc3, $t2]}
    mulhu $t2,$a6,$temp
    @{[adcs $acc3, $acc4, $t3]}
    mulhu $t3,$a7,$temp

    @{[ld_cnt $carry2,$fp]}
    add $carry2,$np,$carry2    # carry2 = np + cnt
    ld $temp,0($carry2)    # a[i+1],..,a[i+7],i=0
    @{[adcs $acc4, $acc5, $t0]}
    @{[adcs $acc5, $acc6, $t1]}
    @{[adcs $acc6, $acc7, $t2]}
    @{[adcs $acc7, $carry, $t3]}

    # Process current 8 elements:
    #  - If not finished: continue inner loop, .Lsqr8x_mul
    #  - If finished: check remaining a elements
    #     - If none: break out,jump to .Lsqr8x_break
    #     - Else: load new data and restart inner loop, .Lsqr8x_mul
    @{[ld_cnt $carry2,$fp]}
    bnez $carry2,.Lsqr8x_mul

    @{[ld_apend $carry2,$fp]}
    beq $ap, $carry2, .Lsqr8x_break    # done yet?

    # loads next batch of data from temporary buffer
    ld $a0,0($tp)
    ld $a1,8($tp)
    ld $a2,16($tp)
    ld $a3,24($tp)
    ld $a4,32($tp)
    ld $a5,40($tp)
    ld $a6,48($tp)
    ld $a7,56($tp)

    # accumulate new data
    # load new data, a[16..23]...
    @{[adds $acc0, $acc0, $a0]}
    ld $a0,0($ap)
    ld $temp, -64($np)    # return to start of current 8 elements, a[0]、a[8]...
    @{[adcs $acc1, $acc1, $a1]}
    ld $a1,8($ap)
    @{[adcs $acc2, $acc2, $a2]}
    ld $a2,16($ap)
    @{[adcs $acc3, $acc3, $a3]}
    ld $a3,24($ap)
    @{[adcs $acc4, $acc4, $a4]}
    ld $a4,32($ap)
    @{[adcs $acc5, $acc5, $a5]}
    ld $a5,40($ap)
    @{[adcs $acc6, $acc6, $a6]}
    ld $a6,48($ap)
    li $carry2,-64    # set loop count, cnt=-64, 8 times
    @{[sd_cnt $carry2,$fp]}
    @{[adcs $acc7, $acc7, $a7]}
    ld $a7,56($ap)
    add $ap,$ap,64    # move pointer forward by 64 bytes, a[24]...
    j .Lsqr8x_mul

    # Outer iteration completion:
    #    - Fetch next data chunk for upcoming iteration
    #    - Determine if this is the final iteration
    #    - Update pointer positions and buffer offsets
.balign 16
.Lsqr8x_break:
    ld $a0,0($np)
    ld $a1,8($np)
    add $ap,$np,64
    ld $a2,16($np)
    ld $a3,24($np)
    @{[ld_apend $carry2,$fp]}
    sub $t0,$carry2,$ap    # is it last iteration?
    ld $a4,32($np)
    ld $a5,40($np)
    sub $t1,$tp,$t0
    ld $a6,48($np)
    ld $a7,56($np)
    beqz $t0, .Lsqr8x_outer_loop

    sd $acc0,0($tp)
    sd $acc1,8($tp)
    ld $acc0,0($t1)
    ld $acc1,8($t1)
    sd $acc2,16($tp)
    sd $acc3,24($tp)
    ld $acc2,16($t1)
    ld $acc3,24($t1)
    sd $acc4,32($tp)
    sd $acc5,40($tp)
    ld $acc4,32($t1)
    ld $acc5,40($t1)
    sd $acc6,48($tp)
    sd $acc7,56($tp)
    mv $tp,$t1
    ld $acc6,48($t1)
    ld $acc7,56($t1)

    j .Lsqr8x_outer_loop
    # Squaring algorithm:
    # 1.Reload input
    # 2.Compute diagonal squares, a[n-1]*a[n-1]|...|a[0]*a[0]
    # 3.Cross terms x 2: first via lsl#1, rest via extr chain (128-bit << 1)
    # 4.Merge diagonal + cross terms x 2
.balign 16
.Lsqr8x_outer_break:
    ld $a1,0($t0)    # recall that $t0 is &a[0], load a[0 1 2 3]
    ld $a3,8($t0)
    ld $t1,8(sp)    # load previously computed cross product term from buffer
    ld $t2,16(sp)
    ld $a5,16($t0)
    ld $a7,24($t0)

    add $ap,$t0,32    # a[4]
    ld $t3,24(sp)
    sd $acc0,0($tp)
    ld $t0,32(sp)
    sd $acc1,8($tp)

    mul $acc0,$a1,$a1    # lo(a[0]*a[0])
    sd $acc2,16($tp)
    sd $acc3,24($tp)
    mulhu $a1,$a1,$a1    # hi(a[0]*a[0])
    sd $acc4,32($tp)
    sd $acc5,40($tp)
    mul $a2,$a3,$a3    # lo(a[1]*a[1])
    sd $acc6,48($tp)
    sd $acc7,56($tp)
    mv $tp,sp
    mulhu $a3,$a3,$a3    # hi(a[1]*a[1])

    slli $carry2,$t1,1    # cross terms x 2
    @{[adds $acc1, $a1, $carry2]}    # (t1 << 1) + hi(a[0]*a[0])
    @{[extr $t1, $t2, $t1]}    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2

    @{[ld_num $carry2,$fp]}
    addi $carry2,$carry2,-32    # set loop count, cnt=num-32
    @{[sd_cnt $carry2,$fp]}

    # Loop processing 4 diagonal elements per iteration:
    #  - Compute a[i]*a[i] for 4 elements
    #  - Accumulate left-shifted cross product terms
.Lsqr4x_shift_n_add:
    @{[adcs $acc2, $a2, $t1]}    # t1+ lo(a[1]*a[1])
    @{[extr $t2, $t3, $t2]}    # t2 = (t3 << 1) + high(t2), cross term multiplied by 2

    @{[ld_cnt $carry2,$fp]}
    addi $carry2,$carry2,-32    # cnt=cnt-32
    @{[sd_cnt $carry2,$fp]}

    @{[adcs $acc3, $a3, $t2]}    # t2+ hi(a[1]*a[1])
    ld $t1,40($tp)
    ld $t2,48($tp)
    mul $a4,$a5,$a5    # lo(a[2]*a[2])
    ld $a1,0($ap)    # a[4 5]
    ld $a3,8($ap)
    addi $ap,$ap,16
    mulhu $a5,$a5,$a5    # hi(a[2]*a[2])
    @{[extr $t3, $t0, $t3]}    # t3 = (t0 << 1) + high(t3), cross term multiplied by 2
    sd $acc0,0($tp)
    sd $acc1,8($tp)
    mul $a6,$a7,$a7    # lo(a[3]*a[3])
    mulhu $a7,$a7,$a7    # hi(a[3]*a[3])
    @{[adcs $acc4, $a4, $t3]}    # t3 + lo(a[2]*a[2])
    @{[extr $t0, $t1, $t0]}    # t0 = (t1 << 1) + high(t0), cross term multiplied by 2
    sd $acc2,16($tp)
    sd $acc3,24($tp)
    @{[adcs $acc5, $a5, $t0]}    # t0 + hi(a[2]*a[2])
    ld $t3,56($tp)
    ld $t0,64($tp)
    @{[extr $t1, $t2, $t1]}    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2
    @{[adcs $acc6, $a6, $t1]}    # t1 + lo(a[3]*a[3])
    @{[extr $t2, $t3, $t2]}    # t2 = (t3 << 1) + high(t2), cross term multiplied by 2
    @{[adcs $acc7, $a7, $t2]}    # t2 + hi(a[3]*a[3])
    ld $t1,72($tp)
    ld $t2,80($tp)
    mul $a0,$a1,$a1    # lo(a[4]*a[4])
    ld $a5,0($ap)    # a[6 7]
    ld $a7,8($ap)
    addi $ap,$ap,16    # move forward by two elements
    mulhu $a1,$a1,$a1    # hi(a[4]*a[4])
    sd $acc4,32($tp)
    sd $acc5,40($tp)
    mul $a2,$a3,$a3    # lo(a[5]*a[5])
    mulhu $a3,$a3,$a3    # hi(a[5]*a[5])
    @{[extr $t3, $t0, $t3]}    # t3 = (t0 << 1) + high(t3), cross term multiplied by 2
    sd $acc6,48($tp)
    sd $acc7,56($tp)
    addi $tp,$tp,64
    @{[adcs $acc0, $a0, $t3]}    # t3 + lo(a[4]*a[4])
    @{[extr $t0, $t1, $t0]}    # t0 = (t1 << 1) + high(t0), cross term multiplied by 2
    @{[adcs $acc1, $a1, $t0]}    # t0 + hi(a[4]*a[4])
    ld $t3,24($tp)
    ld $t0,32($tp)
    @{[extr $t1, $t2, $t1]}    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2

    @{[ld_cnt $carry2,$fp]}
    bnez $carry2,.Lsqr4x_shift_n_add    # if cnt!=0, jump to .Lsqr4x_shift_n_add
___
my ($np,$np_temp) = use_regs(11,13);
$code.=<<___;
    # Tail element handling for square computation
    @{[ld_np $np,$fp]}    # load np, N
    @{[ld_n0 $n0,$fp]} # load n0, n0`
    @{[adcs $acc2, $a2, $t1]}    # t1 + lo(a[n-3]*a[n-3])
    @{[extr $t2, $t3, $t2]}    # t2 = (t3 << 1) + high(t2), cross term multiplied by 2
    @{[adcs $acc3, $a3, $t2]}    # t2 + hi(a[n-3]*a[n-3])
    ld $t1,40($tp)
    ld $t2,48($tp)
    mul $a4,$a5,$a5    # lo(a[n-2]*a[n-2])
    mulhu $a5,$a5,$a5    # hi(a[n-2]*a[n-2])
    sd $acc0,0($tp)
    sd $acc1,8($tp)
    mul $a6,$a7,$a7    # lo(a[n-1]*a[n-1])
    mulhu $a7,$a7,$a7    # hi(a[n-1]*a[n-1])
    sd $acc2,16($tp)
    sd $acc3,24($tp)
    @{[extr $t3, $t0, $t3]}    # t3 = (t0 << 1) + high(t3), cross term multiplied by 2
    @{[adcs $acc4, $a4, $t3]}    # t3 + lo(a[n-2]*a[n-2])
    @{[extr $t0, $t1, $t0]}    # t0 = (t1 << 1) + high(t0), cross term multiplied by 2
    ld $acc0,0(sp)
    ld $acc1,8(sp)
    @{[adcs $acc5, $a5, $t0]}    # t0 + hi(a[n-2]*a[n-2])
    @{[extr $t1, $t2, $t1]}    # t1 = (t2 << 1) + high(t1), cross term multiplied by 2
    ld $a0,0($np)    # load N[0..7]
    ld $a1,8($np)
    @{[adcs $acc6, $a6, $t1]}    # t1 + lo(a[n-1]*a[n-1])
    @{[extr $t2, "zero", $t2]}    # t2 = high(t2)
    ld $a2,16($np)
    ld $a3,24($np)
    @{[adc $acc7, $a7, $t2]}     # t2 + hi(a[n-1]*a[n-1])
    ld $a4,32($np)
    ld $a5,40($np)

    # Reduce by 512 bits per iteration
    mul $na0,$n0,$acc0    # t[0]*n0, the modular reduction coefficient
    ld $a6,48($np)
    ld $a7,56($np)
    @{[ld_num $carry2,$fp]}
    add $carry2,$np,$carry2    # np_end=np+num
    @{[sd_npend $carry2,$fp]}
    ld $acc2,16(sp)
    ld $acc3,24(sp)
    sd $acc4,32($tp)
    sd $acc5,40($tp)
    ld $acc4,32(sp)
    ld $acc5,40(sp)
    sd $acc6,48($tp)
    sd $acc7,56($tp)
    ld $acc6,48(sp)
    ld $acc7,56(sp)
    add $np,$np,64    # move pointer forward by 64 bytes, 8 elements
    mv $tp,sp
    li $carry2,0
    @{[sd_topmost $carry2,$fp]} # initial topmost carry as 0
    li $carry2,8    # set loop count, cnt=8
    @{[sd_cnt $carry2,$fp]}

    # Montgomery reduction, t = t + m * N / 2^64
    # m = t[i] * n0 mod 2^64
    # single-round reduction process:Lsqr8x_reduction ——>Lsqr8x_tail->Lsqr8x_tail_break
    #                                                 |__Lsqr8x8_post_condition
    # after one round of reduction completes,
    # process the current m[i..i+7]*N+t until
    # all tp in the buffer have been reduced
 .Lsqr8x_reduction:
    # mul $t0,$a0,$na0    # discarded
    mul $t1,$a1,$na0    # lo(n[1-7])*lo(t[0]*n0)

    @{[ld_cnt $carry2,$fp]}
    addi $carry2,$carry2,-1    # cnt=cnt-1
    @{[sd_cnt $carry2,$fp]}

    mul $t2,$a2,$na0
    sd $na0,0($tp)    # put aside "na0=t[0]*n0" for tail processing
    addi $tp,$tp,8    # tp=tp+8
    mul $t3,$a3,$na0

    # low64 partial product + reduction term
    snez $carry1, $acc0        # only carry status needed, not full lo1 result
    mul $t0,$a4,$na0
    @{[adcs $acc0, $acc1, $t1]}
    mul $t1,$a5,$na0
    @{[adcs $acc1, $acc2, $t2]}
    mul $t2,$a6,$na0
    @{[adcs $acc2, $acc3, $t3]}
    mul $t3,$a7,$na0
    @{[adcs $acc3, $acc4, $t0]}
    mulhu $t0,$a0,$na0    # hi(n[0-7])*lo(t[0]*n0)
    @{[adcs $acc4, $acc5, $t1]}
    mulhu $t1,$a1,$na0
    @{[adcs $acc5, $acc6, $t2]}
    mulhu $t2,$a2,$na0
    @{[adcs $acc6, $acc7, $t3]}
    mulhu $t3,$a3,$na0
    add $acc7,zero,$carry1

    # high64 partial product + reduction term
    @{[adds $acc0, $acc0, $t0]}
    mulhu $t0,$a4,$na0
    @{[adcs $acc1, $acc1, $t1]}
    mulhu $t1,$a5,$na0
    @{[adcs $acc2, $acc2, $t2]}
    mulhu $t2,$a6,$na0
    @{[adcs $acc3, $acc3, $t3]}
    mulhu $t3,$a7,$na0
    mul $na0,$n0,$acc0    # next na0
    @{[adcs $acc4, $acc4, $t0]}
    @{[adcs $acc5, $acc5, $t1]}
    @{[adcs $acc6, $acc6, $t2]}
    @{[adc  $acc7, $acc7, $t3]}

    @{[ld_cnt $carry2,$fp]}
    bnez $carry2,.Lsqr8x_reduction    # 8 iteration done?

    ld $t0,0($tp)
    ld $t1,8($tp)
    ld $t2,16($tp)
    ld $t3,24($tp)
    mv $np_temp,$tp
    @{[ld_npend $carry2,$fp]}
    sub $carry2,$carry2,$np    # cnt=np_end-np, done yet?
    @{[sd_cnt $carry2,$fp]}

    @{[adds $acc0, $acc0, $t0]}
    @{[adcs $acc1, $acc1, $t1]}
    ld $t0,32($tp)
    ld $t1,40($tp)
    @{[adcs $acc2, $acc2, $t2]}
    @{[adcs $acc3, $acc3, $t3]}
    ld $t2,48($tp)
    ld $t3,56($tp)
    @{[adcs $acc4, $acc4, $t0]}
    @{[adcs $acc5, $acc5, $t1]}
    @{[adcs $acc6, $acc6, $t2]}
    @{[adcs $acc7, $acc7, $t3]}

    # check whether N has finished iteration.
    # If completed, proceed to the special scenario '8' for processing;
    # otherwise, continue with the reduction Lsqr8x_tail.
    @{[ld_cnt $carry2,$fp]}
    beqz $carry2,.Lsqr8x8_post_condition

    ld $n0,-64($tp)    # load the previous na0
    ld $a0,0($np)    # load next N[0..7]
    ld $a1,8($np)
    ld $a2,16($np)
    ld $a3,24($np)
    ld $a4,32($np)
    ld $a5,40($np)
    li $carry2,-64
    @{[sd_cnt $carry2,$fp]}  # set loop cnt,cnt=-64
    ld $a6,48($np)
    ld $a7,56($np)
    add $np,$np,64    # move pointer forward by next 8 elements

.Lsqr8x_tail:
    mul $t0,$a0,$n0
    add $carry,zero,$carry1    # carry bit, modulo-scheduled
    mul $t1,$a1,$n0

    @{[ld_cnt $carry2,$fp]}
    addi $carry2,$carry2,8    # cnt=cnt+8
    @{[sd_cnt $carry2,$fp]}

    mul $t2,$a2,$n0
    mul $t3,$a3,$n0
    # low64 partial product + reduction term
    @{[adds $acc0, $acc0, $t0]}
    mul $t0,$a4,$n0
    @{[adcs $acc1, $acc1, $t1]}
    mul $t1,$a5,$n0
    @{[adcs $acc2, $acc2, $t2]}
    mul $t2,$a6,$n0
    @{[adcs $acc3, $acc3, $t3]}
    mul $t3,$a7,$n0
    @{[adcs $acc4, $acc4, $t0]}
    mulhu $t0,$a0,$n0
    @{[adcs $acc5, $acc5, $t1]}
    mulhu $t1,$a1,$n0
    @{[adcs $acc6, $acc6, $t2]}
    mulhu $t2,$a2,$n0
    @{[adcs $acc7, $acc7, $t3]}
    mulhu $t3,$a3,$n0
    add $carry,$carry,$carry1
    sd $acc0,0($tp)
    addi $tp,$tp,8    # move pointer forward by next 8 elements

    # high64 partial product + reduction term
    @{[adds $acc0, $acc1, $t0]}
    mulhu $t0,$a4,$n0
    @{[adcs $acc1, $acc2, $t1]}
    mulhu $t1,$a5,$n0
    @{[adcs $acc2, $acc3, $t2]}
    mulhu $t2,$a6,$n0
    @{[adcs $acc3, $acc4, $t3]}
    mulhu $t3,$a7,$n0

    @{[ld_cnt $carry2,$fp]}
    add $carry2,$np_temp,$carry2
    ld $n0,0($carry2)    # next n0, ld 0(np_temp+cnt)

    @{[adcs $acc4, $acc5, $t0]}
    @{[adcs $acc5, $acc6, $t1]}
    @{[adcs $acc6, $acc7, $t2]}
    @{[adcs $acc7, $carry, $t3]}
    @{[ld_cnt $carry2,$fp]}
    bnez $carry2,.Lsqr8x_tail    # 8 iteration done?

    ld $a0,0($tp)
    ld $a1,8($tp)
    @{[ld_npend $carry2,$fp]}
    sub $carry2,$carry2,$np    # done yet? cnt=np_end-np
    @{[sd_cnt $carry2,$fp]}

    @{[ld_npend $t0,$fp]}
    @{[ld_num $t1,$fp]}
    sub $t2, $t0, $t1    # rewinded np, t2=np_end-num
    ld $a2,16($tp)
    ld $a3,24($tp)
    ld $a4,32($tp)
    ld $a5,40($tp)
    ld $a6,48($tp)
    ld $a7,56($tp)

    @{[ld_cnt $carry2,$fp]}
    beqz $carry2,.Lsqr8x_tail_break    # exit loop when np=np_end

    ld $n0,-64($np_temp)    # load the previous n0 value
    @{[adds $acc0, $acc0, $a0]}
    @{[adcs $acc1, $acc1, $a1]}
    ld $a0,0($np)    # next N[0..7]
    ld $a1,8($np)
    @{[adcs $acc2, $acc2, $a2]}
    @{[adcs $acc3, $acc3, $a3]}
    ld $a2,16($np)
    ld $a3,24($np)
    @{[adcs $acc4, $acc4, $a4]}
    @{[adcs $acc5, $acc5, $a5]}
    ld $a4,32($np)
    ld $a5,40($np)
    @{[adcs $acc6, $acc6, $a6]}

    li $carry2,-64    # set loop cnt,cnt=-64
    @{[sd_cnt $carry2,$fp]}

    @{[adcs $acc7, $acc7, $a7]}
    ld $a6,48($np)
    ld $a7,56($np)
    add $np,$np,64
    j .Lsqr8x_tail

.balign 16
.Lsqr8x_tail_break:
    @{[ld_n0 $n0,$fp]}    # load n0
    addi $carry2,$tp,64    # cnt=cnt+64
    @{[sd_cnt $carry2,$fp]}
    @{[ld_topmost $carry2,$fp]} # load topmost

    snez $carry1, $carry2    # "move" topmost carry to carry bit
    @{[adcs $t0, $acc0, $a0]}
    @{[adcs $t1, $acc1, $a1]}
    ld $acc0,0($np_temp)    # load the current t[0..7] from tp
    ld $acc1,8($np_temp)
    @{[adcs $acc2, $acc2, $a2]}
    ld $a0,0($t2)    # recall that $t2 is &n[0], initialize, load N[0..7]
    ld $a1,8($t2)
    @{[adcs $acc3, $acc3, $a3]}
    ld $a2,16($t2)
    ld $a3,24($t2)
    @{[adcs $acc4, $acc4, $a4]}
    @{[adcs $acc5, $acc5, $a5]}
    ld $a4,32($t2)
    ld $a5,40($t2)
    @{[adcs $acc6, $acc6, $a6]}
    @{[adcs $acc7, $acc7, $a7]}
    ld $a6,48($t2)
    ld $a7,56($t2)
    addi $np,$t2,64    # np=np+64, move pointer forward by next 8 elements

    @{[ld_topmost $carry2,$fp]}
    add $carry2,zero,$carry1    # topmost carry
    @{[sd_topmost $carry2,$fp]} # offload topmost

    mul $na0,$n0,$acc0    # na0=n0*t[0]
    sd $t0,0($tp)
    sd $t1,8($tp)
    sd $acc2,16($tp)
    sd $acc3,24($tp)
    ld $acc2,16($np_temp)
    ld $acc3,24($np_temp)
    sd $acc4,32($tp)
    sd $acc5,40($tp)
    ld $acc4,32($np_temp)
    ld $acc5,40($np_temp)
    @{[ld_cnt $t1,$fp]}    # load cnt, t1<-cnt
    sd $acc6,48($tp)
    sd $acc7,56($tp)
    mv $tp,$np_temp    # update tp<-np_temp
    ld $acc6,48($np_temp)
    ld $acc7,56($np_temp)
    li $carry2,8    #  set loop cnt, cnt=8
    @{[sd_cnt $carry2,$fp]}
    bne $t1,$fp, .Lsqr8x_reduction    # if t1!=fp,jump to the next round of reduction and
                                      # continue with the reduction of the next set of na0.

    @{[sd_n0 $n0,$fp]}    # offload n0
    @{[ld_rp $carry,$fp]}
    add $tp,$tp,64
    @{[subs  $t0, $acc0, $a0]}
    @{[sbcs  $t1, $acc1, $a1]}
    @{[ld_num $t2,$fp]}
    addi $t3,$t2,-64    # cnt=num-64
    @{[sd_cnt $t3,$fp]}
    mv $carry2,$carry    # ap_end=rp, rp copy
    @{[sd_apend $carry2,$fp]}

    # conditional subtraction, compute T-N
    # If T < N (borrow occurs), set t = T;
    # If T >= N (no borrow), set t = T-N
    # Process 8 elements per loop iteration.
.Lsqr8x_sub:
    @{[sbcs  $t2, $acc2, $a2]}    # t=t-N
    ld $a0,0($np)
    ld $a1,8($np)
    @{[sbcs  $t3, $acc3, $a3]}
    sd $t0,0($carry)    # sd result
    sd $t1,8($carry)
    @{[sbcs  $t0, $acc4, $a4]}
    ld $a2,16($np)
    ld $a3,24($np)
    @{[sbcs  $t1, $acc5, $a5]}
    sd $t2,16($carry)
    sd $t3,24($carry)
    @{[sbcs  $t2, $acc6, $a6]}
    ld $a4,32($np)
    ld $a5,40($np)
    @{[sbcs  $t3, $acc7, $a7]}
    ld $a6,48($np)
    ld $a7,56($np)
    add $np,$np,64
    ld $acc0,0($tp)    # ld t[0..7]
    ld $acc1,8($tp)

    @{[ld_cnt $carry2,$fp]}
    addi $carry2,$carry2,-64    # cnt=cnt-64
    @{[sd_cnt $carry2,$fp]}

    ld $acc2,16($tp)
    ld $acc3,24($tp)
    ld $acc4,32($tp)
    ld $acc5,40($tp)
    ld $acc6,48($tp)
    ld $acc7,56($tp)
    add $tp,$tp,64    # move pointer forward by next 8 elements
    sd $t0,32($carry)
    sd $t1,40($carry)
    @{[sbcs  $t0, $acc0, $a0]}
    sd $t2,48($carry)
    sd $t3,56($carry)
    add $carry,$carry,64
    @{[sbcs  $t1, $acc1, $a1]}

    @{[ld_cnt $carry2,$fp]}
    bnez $carry2,.Lsqr8x_sub    # # if cnt!=0, jump to Lsqr8x_sub

    @{[sbcs  $t2, $acc2, $a2]}
    mv $tp,sp
    @{[ld_num $carry2,$fp]}
    add $ap,sp,$carry2    # ap=sp-num

    @{[ld_apend $carry2,$fp]}
    ld $a0,0($carry2)    # ld 0(ap_end)
    ld $a1,8($carry2)

    @{[sbcs  $t3, $acc3, $a3]}
    sd $t0,0($carry)
    sd $t1,8($carry)
    @{[sbcs  $t0, $acc4, $a4]}
    @{[ld_apend $carry2,$fp]}
    ld $a2,16($carry2)
    ld $a3,24($carry2)
    @{[sbcs  $t1, $acc5, $a5]}
    sd $t2,16($carry)
    sd $t3,24($carry)
    @{[sbcs  $t2, $acc6, $a6]}
    ld $acc0,0($ap)
    ld $acc1,8($ap)
    @{[sbcs  $t3, $acc7, $a7]}
    ld $acc2,16($ap)
    ld $acc3,24($ap)

    @{[ld_topmost $temp,$fp]}
    sub $carry2, $temp, $carry1    # did it borrow?
    sltu $carry1, $temp, $carry2
    xori $carry1, $carry1, 1

    sd $t0,32($carry)
    sd $t1,40($carry)
    sd $t2,48($carry)
    sd $t3,56($carry)

    @{[ld_num $carry2,$fp]}
    addi $carry2,$carry2,-32    # cnt=num-32
    @{[sd_cnt $carry2,$fp]}
    @{[ld_ra $ra,$fp]}    # offload ra


    # The csel conditional instruction selects the result based on the borrow flag.
    # If there is a borrow (C=0), select the original value acc
    # If there is no borrow (C=1), select the subtracted value a
    # Process 4 elements per loop iteration.
.Lsqr4x_cond_copy:
    @{[ld_cnt $carry2,$fp]}
    addi $carry2,$carry2,-32    # cnt=cnt_pre-32=num-32-32
    @{[sd_cnt $carry2,$fp]}

    @{[csel  $t0, $acc0, $a0]}    # when pre_borrow occurs, t0 = acc0
    sd zero,0($tp)
    sd zero,8($tp)
    @{[csel  $t1, $acc1, $a1]}
    @{[ld_apend $carry2,$fp]}
    ld $a0,32($carry2)
    ld $a1,40($carry2)
    ld $acc0,32($ap)
    ld $acc1,40($ap)
    @{[csel  $t2, $acc2, $a2]}
    sd zero,16($tp)
    sd zero,24($tp)
    add $tp,$tp,32
    @{[csel  $t3, $acc3, $a3]}
    ld $a2,48($carry2)
    ld $a3,56($carry2)
    ld $acc2,48($ap)
    ld $acc3,56($ap)
    add $ap,$ap,32
    sd $t0,0($carry2)
    sd $t1,8($carry2)
    sd $t2,16($carry2)
    sd $t3,24($carry2)
    add $carry2,$carry2,32    # ap_end=ap_end+32
    @{[sd_apend $carry2,$fp]}
    sd zero,0($ap)
    sd zero,8($ap)
    sd zero,16($ap)
    sd zero,24($ap)

    @{[ld_cnt $carry2,$fp]}
    bnez $carry2,.Lsqr4x_cond_copy    # if cnt!=0, jump to Lsqr4x_cond_copy

    @{[csel  $t0, $acc0, $a0]}
    sd zero,0($tp)
    sd zero,8($tp)
    @{[csel  $t1, $acc1, $a1]}
    sd zero,16($tp)
    sd zero,24($tp)
    @{[csel  $t2, $acc2, $a2]}
    @{[csel  $t3, $acc3, $a3]}
    @{[ld_apend $carry2,$fp]}
    sd $t0,0($carry2)
    sd $t1,8($carry2)
    sd $t2,16($carry2)
    sd $t3,24($carry2)
    j .Lsqr8x_done

# process special case for 8-word boundary
.balign 16
.Lsqr8x8_post_condition:
    add $carry,zero,$carry1
    @{[subs  $a0, $acc0, $a0]}    # acc0-7,carry hold result, a0-7 hold modulus
    @{[ld_rp $ap,$fp]}    # pull rp
    @{[sbcs  $a1, $acc1, $a1]}
    sd zero,0(sp)
    sd zero,8(sp)
    @{[sbcs  $a2, $acc2, $a2]}
    sd zero,16(sp)
    sd zero,24(sp)
    @{[sbcs  $a3, $acc3, $a3]}
    sd zero,32(sp)
    sd zero,40(sp)
    @{[sbcs  $a4, $acc4, $a4]}
    sd zero,48(sp)
    sd zero,56(sp)
    @{[sbcs  $a5, $acc5, $a5]}
    sd zero,64(sp)
    sd zero,72(sp)
    @{[sbcs  $a6, $acc6, $a6]}
    sd zero,80(sp)
    sd zero,88(sp)
    @{[sbcs  $a7, $acc7, $a7]}
    sd zero,96(sp)
    sd zero,104(sp)
    @{[sbcs  $carry, $carry, "zero"]}    # did it borrow?
    xori $carry1, $carry1, 1
    sd zero,112(sp)
    sd zero,120(sp)

    # a0-7 hold result-modulus
    @{[csel  $a0, $acc0, $a0]}
    @{[csel  $a1, $acc1, $a1]}
    @{[csel  $a2, $acc2, $a2]}
    @{[csel  $a3, $acc3, $a3]}
    sd $a0,0($ap)
    sd $a1,8($ap)
    @{[csel  $a4, $acc4, $a4]}
    @{[csel  $a5, $acc5, $a5]}
    sd $a2,16($ap)
    sd $a3,24($ap)
    @{[csel  $a6, $acc6, $a6]}
    @{[csel  $a7, $acc7, $a7]}
    sd $a4,32($ap)
    sd $a5,40($ap)
    sd $a6,48($ap)
    sd $a7,56($ap)

    @{[ld_ra $ra,$fp]}    # pull ra return address
.Lsqr8x_done:
    mv sp, $fp
    li $rp, 1
___
$code .= <<___;
    ld      s0,88(sp)
    ld      s1,80(sp)
    ld      s2,72(sp)
    ld      s3,64(sp)
    ld      s4,56(sp)
    ld      s5,48(sp)
    ld      s6,40(sp)
    ld      s7,32(sp)
    ld      s8,24(sp)
    ld      s9,16(sp)
    ld      s11,8(sp)
    ld      s10,0(sp)
    addi    sp,sp,168
    ret
.size bn_sqr8x_mont,.-bn_sqr8x_mont
___
}
print $code;
close STDOUT or die "error closing STDOUT: $!";
