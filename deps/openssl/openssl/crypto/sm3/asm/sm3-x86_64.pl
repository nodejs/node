#! /usr/bin/env perl
# Copyright 2024-2025 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2024, Intel Corporation. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
#
# This module implements support for Intel(R) SM3 instructions
# from Intel(R) Multi-Buffer Crypto for IPsec Library
# (https://github.com/intel/intel-ipsec-mb).
# Original author is Tomasz Kantecki <tomasz.kantecki@intel.com>

# $output is the last argument if it looks like a file (it has an extension)
# $flavour is the first argument if it doesn't look like a file
$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$win64=0;
$win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$0 =~ m/(.*[\/\\])[^\/\\]+$/;
$dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

# Check Intel(R) SM3 instructions support
if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
		=~ /GNU assembler version ([2-9]\.[0-9]+)/) {
	$avx2_sm3_ni = ($1>=2.22); # minimal avx2 supported version, binary translation for SM3 instructions (sub sm3op) is used
    $avx2_sm3_ni_native = ($1>=2.42); # support added at GNU asm 2.42
}

if (!$avx2_sm3_ni && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
	   `nasm -v 2>&1` =~ /NASM version ([2-9])\.([0-9]+)\.([0-9]+)/) {
    my ($major, $minor, $patch) = ($1, $2, $3);
    $avx2_sm3_ni = ($major > 2) || ($major == 2 && $minor > 10); # minimal avx2 supported version, binary translation for SM3 instructions (sub sm3op) is used
	$avx2_sm3_ni_native = ($major > 2) || ($major == 2 && $minor > 16) || ($major == 2 && $minor == 16 && $patch >= 2); # support added at NASM 2.16.02
}

if (!$avx2_sm3_ni && `$ENV{CC} -v 2>&1` =~ /((?:clang|LLVM) version|.*based on LLVM) ([0-9]+\.[0-9]+)/) {
	$avx2_sm3_ni = ($2>=7.0); # minimal tested version, binary translation for SM3 instructions (sub sm3op) is used
    $avx2_sm3_ni_native = ($2>=17.0); # support added at LLVM 17.0.1
}

open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

if ($avx2_sm3_ni>0) {
# Create 4 x 32-bit new words of message schedule W[] using SM3-NI ISA
sub sm3msg {
my ($W03_00, $W07_04, $W11_08, $W15_12, $W19_16, $T1,$T2) = @_;
my $T3 = $W19_16;
$code.=<<___;
    vpalignr        \$12, $W07_04, $W11_08, $T3
    vpsrldq         \$4, $W15_12, $T1
    vsm3msg1        $W03_00, $T1, $T3
    vpalignr        \$12, $W03_00, $W07_04, $T1
    vpalignr        \$8, $W11_08, $W15_12, $T2
    vsm3msg2        $T2, $T1, $T3
___
}

# Performs 4 rounds of SM3 algorithm
#   - consumes 4 words of message schedule W[]
#   - updates SM3 state registers: ABEF and CDGH
sub sm3rounds4 {
my ($ABEF, $CDGH, $W03_00, $W07_04, $T1,$R)=@_;
my $R2 = $R + 2;
$code.=<<___;
    vpunpcklqdq     $W07_04, $W03_00, $T1
    vsm3rnds2       \$$R, $T1, $ABEF, $CDGH
    vpunpckhqdq     $W07_04, $W03_00, $T1
    vsm3rnds2       \$$R2, $T1, $CDGH, $ABEF
___
}

$code.= ".data\n";
{
# input arguments aliases
my ($ctx,$p,$num) = ("%rdi","%rsi","%rdx");

$code.=<<___;
.align 16
SHUFF_MASK:
    .byte 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12

.text

# void ossl_hwsm3_block_data_order(SM3_CTX *c, const void *p, size_t num)
#
# input: $ctx SM3 context
#        $p  pointer to the data
#        $num number of blocks
#

.globl	ossl_hwsm3_block_data_order
.type	ossl_hwsm3_block_data_order,\@function,3
.align	32
ossl_hwsm3_block_data_order:
.cfi_startproc
    endbranch
# Prolog
    push    %rbp
.cfi_push   %rbp
.cfi_def_cfa_register %rbp
.Lossl_hwsm3_block_data_order_seh_setfp:
___

$code.=<<___ if($win64);
    # xmm6:xmm12 need to be maintained for Windows
    sub \$`7*16`,%rsp
.cfi_adjust_cfa_offset \$`7*16`
    vmovdqu %xmm6, 16*0(%rsp)
    vmovdqu %xmm7, 16*1(%rsp)
    vmovdqu %xmm8, 16*2(%rsp)
    vmovdqu %xmm9, 16*3(%rsp)
    vmovdqu %xmm10,16*4(%rsp)
    vmovdqu %xmm11,16*5(%rsp)
    vmovdqu %xmm12,16*6(%rsp)
___

  $code .= <<___;
# Prolog ends here.
.Lossl_hwsm3_block_data_order_seh_prolog_end:
    or $num, $num
    je .done_hash

    # xmm = D C B A
    # D - most significant word in an `xmm`
    # A - least significant word in an `xmm`
    vmovdqu         ($ctx), %xmm6 # xmm6 = D C B A
    vmovdqu         16($ctx), %xmm7 # xmm7 = H G F E

    vpshufd         \$0x1B, %xmm6, %xmm0
    vpshufd         \$0x1B, %xmm7, %xmm1
    vpunpckhqdq     %xmm0, %xmm1, %xmm6
    vpunpcklqdq     %xmm0, %xmm1, %xmm7
    vpsrld          \$9, %xmm7, %xmm2
    vpslld          \$23, %xmm7, %xmm3
    vpxor           %xmm3, %xmm2, %xmm1
    vpsrld          \$19, %xmm7, %xmm4
    vpslld          \$13, %xmm7, %xmm5
    vpxor           %xmm5, %xmm4, %xmm0
    # xmm7 = ROL32(C, 23) ROL32(D, 23) ROL32(G, 13) ROL32(H, 13)
    vpblendd        \$0x3, %xmm0, %xmm1, %xmm7

    vmovdqa         SHUFF_MASK(%rip), %xmm12

.align 32
.block_loop:
    vmovdqa         %xmm6, %xmm10
    vmovdqa         %xmm7, %xmm11

    # prepare W[0..15] - read and shuffle the data
    vmovdqu         ($p), %xmm2
    vmovdqu         16($p), %xmm3
    vmovdqu         32($p), %xmm4
    vmovdqu         48($p), %xmm5
    vpshufb         %xmm12, %xmm2, %xmm2                            # xmm2 = W03 W02 W01 W00
    vpshufb         %xmm12, %xmm3, %xmm3                            # xmm3 = W07 W06 W05 W04
    vpshufb         %xmm12, %xmm4, %xmm4                            # xmm4 = W11 W10 W09 W08
    vpshufb         %xmm12, %xmm5, %xmm5                            # xmm5 = W15 W14 W13 W12

___
    sm3msg("%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm2", "%xmm3", "%xmm1", 0);

    $code.="vmovdqa         %xmm8, %xmm2\n";
    sm3msg("%xmm3", "%xmm4", "%xmm5", "%xmm2", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm3", "%xmm4", "%xmm1", 4);

    $code.="vmovdqa         %xmm8, %xmm3\n";
    sm3msg("%xmm4", "%xmm5", "%xmm2", "%xmm3", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm4", "%xmm5", "%xmm1", 8);

    $code.="vmovdqa         %xmm8, %xmm4\n";
    sm3msg("%xmm5", "%xmm2", "%xmm3", "%xmm4", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm5", "%xmm2", "%xmm1", 12);

    $code.="vmovdqa         %xmm8, %xmm5\n";
    sm3msg("%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm2", "%xmm3", "%xmm1", 16);

    $code.="vmovdqa         %xmm8, %xmm2\n";
    sm3msg("%xmm3", "%xmm4", "%xmm5", "%xmm2", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm3", "%xmm4", "%xmm1", 20);

    $code.="vmovdqa         %xmm8, %xmm3\n";
    sm3msg("%xmm4", "%xmm5", "%xmm2", "%xmm3", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm4", "%xmm5", "%xmm1", 24);

    $code.="vmovdqa         %xmm8, %xmm4\n";
    sm3msg("%xmm5", "%xmm2", "%xmm3", "%xmm4", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm5", "%xmm2", "%xmm1", 28);

    $code.="vmovdqa         %xmm8, %xmm5\n";
    sm3msg("%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm2", "%xmm3", "%xmm1", 32);

    $code.="vmovdqa         %xmm8, %xmm2\n";
    sm3msg("%xmm3", "%xmm4", "%xmm5", "%xmm2", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm3", "%xmm4", "%xmm1", 36);

    $code.="vmovdqa         %xmm8, %xmm3\n";
    sm3msg("%xmm4", "%xmm5", "%xmm2", "%xmm3", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm4", "%xmm5", "%xmm1", 40);

    $code.="vmovdqa         %xmm8, %xmm4\n";
    sm3msg("%xmm5", "%xmm2", "%xmm3", "%xmm4", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm5", "%xmm2", "%xmm1", 44);

    $code.="vmovdqa         %xmm8, %xmm5\n";
    sm3msg("%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm8", "%xmm9", "%xmm1");
    sm3rounds4("%xmm6", "%xmm7", "%xmm2", "%xmm3", "%xmm1", 48);

    $code.="vmovdqa         %xmm8, %xmm2\n";
    sm3rounds4("%xmm6", "%xmm7", "%xmm3", "%xmm4", "%xmm1", 52);
    sm3rounds4("%xmm6", "%xmm7", "%xmm4", "%xmm5", "%xmm1", 56);
    sm3rounds4("%xmm6", "%xmm7", "%xmm5", "%xmm2", "%xmm1", 60);

$code.=<<___;
    # update hash value
    vpxor           %xmm10, %xmm6, %xmm6
    vpxor           %xmm11, %xmm7, %xmm7
    addq             \$64, $p
    dec             $num
    jnz             .block_loop

    # store the hash value back in memory
    vpslld          \$9, %xmm7, %xmm2
    vpsrld          \$23, %xmm7, %xmm3
    vpxor           %xmm3, %xmm2, %xmm1
    vpslld          \$19, %xmm7, %xmm4
    vpsrld          \$13, %xmm7, %xmm5
    vpxor           %xmm5, %xmm4, %xmm0
    vpblendd        \$0x3, %xmm0, %xmm1, %xmm7
    vpshufd         \$0x1B, %xmm6, %xmm0
    vpshufd         \$0x1B, %xmm7, %xmm1

    vpunpcklqdq     %xmm1, %xmm0, %xmm6
    vpunpckhqdq     %xmm1, %xmm0, %xmm7

    vmovdqu         %xmm6, ($ctx)
    vmovdqu         %xmm7, 16($ctx)
.done_hash:
    # Epilog
___
$code.=<<___ if($win64);
    # xmm6:xmm12 need to be maintained for Windows
    vmovdqu 16*0(%rsp),%xmm6
    vmovdqu 16*1(%rsp),%xmm7
    vmovdqu 16*2(%rsp),%xmm8
    vmovdqu 16*3(%rsp),%xmm9
    vmovdqu 16*4(%rsp),%xmm10
    vmovdqu 16*5(%rsp),%xmm11
    vmovdqu 16*6(%rsp),%xmm12
    add \$`7*16`,%rsp
.cfi_adjust_cfa_offset \$`-7*16`
___
  $code .= <<___;
     pop     %rbp
.cfi_pop     %rbp
    ret
.cfi_endproc
___
}
} else { # fallback
$code .= <<___;
.text

.globl	ossl_hwsm3_block_data_order
.type ossl_hwsm3_block_data_order,\@abi-omnipotent
ossl_hwsm3_block_data_order:
    .byte   0x0f,0x0b    # ud2
    ret
.size   ossl_hwsm3_block_data_order, .-ossl_hwsm3_block_data_order
___
} # avx2_sm3_ni

if ($avx2_sm3_ni_native > 0) { # SM3 instructions are supported in asm
  $code =~ s/\`([^\`]*)\`/eval $1/gem;
  print $code;
} else { # binary translation for SM3 instructions
sub sm3op {
  my $instr = shift;
  my $args = shift;
  if ($args =~ /^(.+)\s*#/) {
    $args = $1; # drop comment and its leading whitespace
  }
  if (($instr eq "vsm3msg1") && ($args =~ /%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})/)) {
    my $b1 = sprintf("0x%02x", 0x42 | ((1-int($1/8))<<5) | ((1-int($3/8))<<7) );
    my $b2 = sprintf("0x%02x", 0x00 | (15 - $2 & 15)<<3                       );
    my $b3 = sprintf("0x%02x", 0xc0 | ($1 & 7) | (($3 & 7)<<3)                );
    return ".byte 0xc4,".$b1.",".$b2.",0xda,".$b3;
  }
  elsif (($instr eq "vsm3msg2") && ($args =~ /%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})/)) {
    my $b1 = sprintf("0x%02x", 0x42 | ((1-int($1/8))<<5) | ((1-int($3/8))<<7) );
    my $b2 = sprintf("0x%02x", 0x01 | (15 - $2 & 15)<<3                       );
    my $b3 = sprintf("0x%02x", 0xc0 | ($1 & 7) | (($3 & 7)<<3)                );
    return ".byte 0xc4,".$b1.",".$b2.",0xda,".$b3;
  }
  elsif (($instr eq "vsm3rnds2") && ($args =~ /\$(0x[0-9a-fA-F]+|\d{1,2})\s*,\s*%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})/)) {
    my $b1 = sprintf("0x%02x", $1                                             );
    my $b2 = sprintf("0x%02x", 0x43 | ((1-int($2/8))<<5) | ((1-int($4/8))<<7) );
    my $b3 = sprintf("0x%02x", 0x01 | (15 - $3 & 15)<<3                       );
    my $b4 = sprintf("0x%02x", 0xc0 | ($2 & 7) | (($4 & 7)<<3)                );
    return ".byte 0xc4,".$b2.",".$b3.",0xde,".$b4.",".$b1;
  }

  return $instr."\t".$args;
}

foreach (split("\n",$code)) {
  s/\`([^\`]*)\`/eval $1/geo;
  s/\b(vsm3[^\s]*)\s+(.*)/sm3op($1,$2)/geo;
  print $_,"\n";
}

}

close STDOUT or die "error closing STDOUT: $!";
