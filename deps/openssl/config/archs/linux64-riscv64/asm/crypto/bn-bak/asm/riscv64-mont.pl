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

################################################################################
# Register assignment
################################################################################

# Function arguments
#      RISC-V    ABI
# $rp	x10	     a0  # BN_ULONG *rp
# $ap	x11	     a1  # const BN_ULONG *ap
# $bp	x12	     a2  # const BN_ULONG *bp
# $np	x13	     a3  # const BN_ULONG *np
# $n0	x14      a4  # const BN_ULONG *n0
# $num	x15      a5  # int num
my ($rp,$ap,$bp,$np,$n0,$num) = use_regs(10,11,12,13,14,15);

# Return address and Frame pointer
#      RISC-V    ABI
# $ra   x1       ra
# $fp   x8       s0
my ($ra,$fp) = use_regs(1,8);

# Temporary variable allocation
#      RISC-V    ABI
# $lo0	x5	     t0    the sum of partial products of a and b
# $hi0	x6	     t1    the high word of partial product of a and b + Carry
# $aj	x7	     t2    ap[j]
# $m0	x28	     t3    bp[i]
# $alo	x29	     t4    the low word of partial product
# $ahi	x30      t5    the high word of partial product
# $lo1	x31	     t6    partial product + reduction term
# $hi1	x18	     s2    the high word of reduction term + Carry
# $nj	x19	     s3    np[j],modulus
# $m1	x20	     s4    montgomery reduction coefficient
# $nlo	x21	     s5    the low word of reduction term
# $nhi	x22	     s6    the high word of reduction term
# $ovf	x23	     s7    highest carry bit,overflow flag
# $i	x24	     s8    outer loop index
# $j	x25	     s9    inner loop index
# $tp	x26	     s10   temporary result storage
# $tj	x27	     s11   tp[j],temporary result value
# $temp x9       s1
my ($lo0,$hi0,$aj,$m0,$alo,$ahi,$lo1,$hi1,$nj,$m1,$nlo,$nhi,$ovf,$i,$j,$tp,$tj,$temp) = use_regs(5..7,28..31,18..27,9);

# Carry variable
# $carry1 x16      a6
# $carry2 x17      a7
my ($carry1,$carry2) = use_regs(16,17);

my $code .= <<___;
.text
.balign 32
.globl bn_mul_mont
.type   bn_mul_mont,\@function
bn_mul_mont:
___

$code .= save_regs();

$code .= <<___;
    mv $fp, sp
___

$code .= <<___;	
    ld $m0, 0($bp)    # bp[0]
    addi $bp, $bp,8
    ld $hi0, 0($ap)    # ap[0]
    ld $aj, 8($ap)    # ap[1]
    addi $ap, $ap, 16
    ld $n0, 0($n0)    # n0,precomputed modular inverse
    ld $hi1, 0($np)    # np[0]
    ld $nj, 8($np)    # np[1]
    addi $np, $np, 16

    slli $num, $num, 3
    sub $tp, sp, $num
    andi $tp, $tp, -16    # address alignment
    mv sp, $tp    # alloca

    addi $j, $num, -16    # $j=(num-2)*8

    mul $lo0, $hi0, $m0    # ap[0]*bp[0]
    mulhu $hi0, $hi0, $m0
    mul $alo, $aj, $m0    # ap[1]*bp[0]
    mulhu $ahi, $aj, $m0

    mul $m1, $lo0, $n0    # montgomery reduction coefficient tp[0]*n0
    # montgomery optimization: np[0]*m1 ensures (np[0]*m1+lo0) has zero lower bits
    # only carry status needed, not full lo1 result
    # eliminates mul/adds instructions → Saves cycles & power
    # mul $lo1, $hi1, $m1		// np[0]*m1
    # adds $lo1, $lo1, $lo0   // discarded
    mulhu $hi1, $hi1, $m1
    snez $carry1, $lo0
    add $hi1, $hi1, $carry1
    mul $nlo, $nj, $m1    # np[1]*m1
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
    add $hi1, $nhi, $carry1    # {np[j-1]*m1,high}+C_lo1, j ranges from 2 to num-1

    # partial product + reduction term
    add $temp, $lo1, $lo0
    sltu $carry1, $temp, $lo1
    mv $lo1, $temp
    add $hi1, $hi1, $carry1

    sd $lo1, 0($tp)    # tp[j-2], j ranges from 2 to num-1
    addi $tp, $tp, 8

    mul $alo, $aj, $m0    # ap[j]*bp[0], j ranges from 2 to num-1
    mulhu $ahi, $aj, $m0
    mul $nlo, $nj, $m1    # np[j]*m1, j ranges from 2 to num-1
    mulhu $nhi, $nj, $m1
    bnez $j, .L1st

.L1st_last_entry:
    # last partial product
    add $lo0, $alo, $hi0    # {ap[j]*bp[0],low}+{ap[j-1]*bp[0],high}, j is num-1
    sltu $carry1, $lo0, $alo
    add $hi0, $ahi, $carry1    # {ap[j]*bp[0],high}+C_lo0, j is num-1

    sub $ap, $ap, $num    # rewind $ap
    sub $np, $np, $num    # rewind $np

    # last reduction term
    add $lo1, $nlo, $hi1    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu $carry1, $lo1, $nlo
    add $hi1, $nhi, $carry1    # {np[j]*m1,high}+C_lo1, j is num-1

    # last partial product + last reduction term
    add $lo1, $lo1, $lo0
    sltu $carry1, $lo1, $lo0

    add $temp, $hi1, $hi0
    sltu $carry2, $temp, $hi1
    add $hi1, $temp, $carry1
    sltu $ovf, $hi1, $temp
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
    mulhu $hi0, $hi0, $m0

    addi $j, $num,-16    # $j=(num-2)*8
    ld $hi1, 0($np)
    ld $nj, 8($np)
    addi $np, $np, 16

    mul $alo, $aj, $m0    # ap[1]*bp[i], i ranges from 1 to num-1
    mulhu $ahi, $aj, $m0

    add $lo0, $lo0, $tj    # ap[0]*bp[i] + last_tp[0] , i ranges from 1 to num-1
    sltu $carry1, $lo0, $tj
    add $hi0, $hi0, $carry1    # $hi0 will not overflow

    # compute the modular reduction coefficient
    mul $m1, $lo0, $n0

    addi $i, $i, -8    # $i--, $i ranges from (num-1)*8 to 0

    # mul $lo1, $hi1, $m1	 # discarded
    # adds	$lo1, $lo1, $lo0   # discarded
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

    # partial product + reduction term
    add $lo0, $lo0, $tj
    sltu $carry1, $lo0, $tj
    add $hi0, $hi0, $carry1

    add $lo1, $lo1, $lo0
    sltu $carry1, $lo1, $lo0

    sd $lo1, -16($tp)    # tp[j-2], j ranges from 2 to num-1

    mul $alo, $aj, $m0    # ap[j]*bp[i], j ranges from 2 to num-1, i ranges from 1 to num-1
    mulhu $ahi, $aj, $m0
    mul $nlo, $nj, $m1    # np[j]*m1, j ranges from 2 to num-1
    mulhu $nhi, $nj, $m1

    bnez $j, .Linner

.Linner_last_entry:
    ld $tj, 0($tp)    # tp[j], j is num-1
    addi $tp, $tp, 8
    add $hi1, $hi1, $carry1

    # last partial product
    add $lo0, $alo, $hi0    # {ap[j]*bp[i],low}+{ap[j-1]*bp[i],high}, j is num-1, i ranges from 1 to num-1
    sltu $carry1, $lo0, $alo
    add $hi0, $ahi, $carry1    # {ap[j]*bp[i],high}+C_lo0, j is num-1, i ranges from 1 to num-1

    sub $ap, $ap, $num    # rewind $ap
    sub	$np, $np, $num    # rewind $np

    # last reduction term
    add $lo1, $nlo, $hi1    # {np[j]*m1,low}+{np[j-1]*m1,high}, j is num-1
    sltu $carry1, $lo1, $nlo
    add $temp, $nhi, $ovf
    sltu $carry2, $temp, $nhi
    add $hi1, $temp, $carry1    # {np[j]*m1,high}+C_lo1, j is num-1
    sltu $ovf, $hi1, $temp
    or $carry1, $carry2, $ovf
    mv $ovf, $carry1    # update the upmost overflow bit

    # last partial product + last reduction term
    add $lo0, $lo0, $tj
    sltu $carry1, $lo0, $tj
    add $hi0, $hi0, $carry1

    add $lo1, $lo1, $lo0
    sltu $carry1, $lo1, $lo0
    add $temp, $hi1, $hi0
    sltu $carry2, $temp, $hi1
    add $hi1, $temp, $carry1
    sltu $carry1, $hi1, $temp
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
    sltu $carry1, $num, 8
    xori $carry1, $carry1, 1
    mv $ap, $rp
.Lsub:
    # tp[j]-np[j], j ranges from 0 to num-2, set carry flag
    xori $carry1, $carry1,1
    sub $temp, $tj, $nj
    sltu $carry2, $tj, $temp
    sub $aj, $temp, $carry1
    sltu $carry1, $temp, $aj
    or $carry1, $carry2, $carry1
    xori $carry1, $carry1, 1

    ld $tj, 0($tp)    # tp[j], j ranges from 1 to num-1
    addi $tp, $tp, 8
    addi $j, $j, -8    # $j--, $j ranges from (num-1)*8 to 0
    ld $nj, 0($np)
    addi $np, $np, 8

    sd $aj, 0($ap)    # rp[j]=tp[j]-np[j], j ranges from 0 to num-2
    addi $ap, $ap, 8
    bnez $j, .Lsub

    # process the last word, tp[j]-np[j], j is num-1
    xori $carry1, $carry1,1
    sub $temp, $tj, $nj
    sltu $carry2, $tj, $temp
    sub $aj, $temp, $carry1
    sltu $carry1, $temp, $aj
    or $carry1, $carry2, $carry1
    xori $carry1, $carry1, 1

    # whether there is a borrow
    xori $carry1, $carry1, 1
    sub $temp, $ovf, zero
    sltu $carry2, $ovf, $temp
    sub $ovf, $temp, $carry1
    sltu $carry1, $temp, $ovf
    or $carry1, $carry2, $carry1
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
.size	bn_mul_mont,.-bn_mul_mont
___

print $code;
close STDOUT or die "error closing STDOUT: $!";
