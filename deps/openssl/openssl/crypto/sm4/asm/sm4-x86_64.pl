#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2025, Intel Corporation. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
#
# This module implements support for Intel(R) SM4 instructions
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

# Check Intel(R) SM4 instructions support
if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
        =~ /GNU assembler version ([2-9]\.[0-9]+)/) {
    $avx2_sm4_ni = ($1>=2.22); # minimal avx2 supported version, binary translation for SM4 instructions (sub sm4op) is used
    $avx2_sm4_ni_native = ($1>=2.42); # support added at GNU asm 2.42
}

if (!$avx2_sm4_ni && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
       `nasm -v 2>&1` =~ /NASM version ([2-9])\.([0-9]+)\.([0-9]+)/) {
    my ($major, $minor, $patch) = ($1, $2, $3);
    $avx2_sm4_ni = ($major > 2) || ($major == 2 && $minor > 10); # minimal avx2 supported version, binary translation for SM4 instructions (sub sm4op) is used
    $avx2_sm4_ni_native = ($major > 2) || ($major == 2 && $minor > 16) || ($major == 2 && $minor == 16 && $patch >= 2); # support added at NASM 2.16.02
}

if (!$avx2_sm4_ni && `$ENV{CC} -v 2>&1` =~ /((?:clang|LLVM) version|.*based on LLVM) ([0-9]+\.[0-9]+)/) {
    $avx2_sm4_ni = ($2>=7.0); # minimal tested version, binary translation for SM4 instructions (sub sm4op) is used
    $avx2_sm4_ni_native = ($2>=17.0); # support added at LLVM 17.0.1
}

open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

$prefix="hw_x86_64_sm4";

if ($avx2_sm4_ni>0) {

$code.= ".text\n";
{
# input arguments aliases for set_key
my ($userKey,$key) = ("%rdi","%rsi");

# input arguments aliases for encrypt/decrypt
my ($in,$out,$ks) = ("%rdi","%rsi","%rdx");

$code.=<<___;
.section .rodata align=64
.align 16
SM4_FK:
.long 0xa3b1bac6, 0x56aa3350, 0x677d9197, 0xb27022dc

.align 16
SM4_CK:
.long 0x00070E15, 0x1C232A31, 0x383F464D, 0x545B6269
.long 0x70777E85, 0x8C939AA1, 0xA8AFB6BD, 0xC4CBD2D9
.long 0xE0E7EEF5, 0xFC030A11, 0x181F262D, 0x343B4249
.long 0x50575E65, 0x6C737A81, 0x888F969D, 0xA4ABB2B9
.long 0xC0C7CED5, 0xDCE3EAF1, 0xF8FF060D, 0x141B2229
.long 0x30373E45, 0x4C535A61, 0x686F767D, 0x848B9299
.long 0xA0A7AEB5, 0xBCC3CAD1, 0xD8DFE6ED, 0xF4FB0209
.long 0x10171E25, 0x2C333A41, 0x484F565D, 0x646B7279

IN_SHUFB:
.byte 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04
.byte 0x0b, 0x0a, 0x09, 0x08, 0x0f, 0x0e, 0x0d, 0x0c
.byte 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04
.byte 0x0b, 0x0a, 0x09, 0x08, 0x0f, 0x0e, 0x0d, 0x0c

OUT_SHUFB:
.byte 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08
.byte 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00
.byte 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08
.byte 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00

.text

# int ${prefix}_set_key(const unsigned char *userKey, SM4_KEY *key)
#
# input: $userKey secret key
#        $key  round keys
#

.globl    ${prefix}_set_key
.type     ${prefix}_set_key,\@function,2
.align    32
${prefix}_set_key:
.cfi_startproc
    endbranch
# Prolog
    push    %rbp
.cfi_push   %rbp
# Prolog ends here.
.Lossl_${prefix}_set_key_seh_prolog_end:

    vmovdqu         ($userKey), %xmm0
    vpshufb         IN_SHUFB(%rip), %xmm0, %xmm0
    vpxor           SM4_FK(%rip), %xmm0, %xmm0

    vmovdqu         SM4_CK(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, ($key)
    vmovdqu         SM4_CK + 16(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, 16($key)
    vmovdqu         SM4_CK + 32(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, 32($key)
    vmovdqu         SM4_CK + 48(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, 48($key)
    vmovdqu         SM4_CK + 64(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, 64($key)
    vmovdqu         SM4_CK + 80(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, 80($key)
    vmovdqu         SM4_CK + 96(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, 96($key)
    vmovdqu         SM4_CK + 112(%rip), %xmm1
    vsm4key4        %xmm1, %xmm0, %xmm0
    vmovdqu         %xmm0, 112($key)

    vpxor           %xmm0, %xmm0, %xmm0 # clear register
    mov     \$1, %eax
    pop     %rbp
.cfi_pop     %rbp
    ret
.cfi_endproc

# void ${prefix}_encrypt(const uint8_t *in, uint8_t *out, const SM4_KEY *ks)

.globl    ${prefix}_encrypt
.type     ${prefix}_encrypt,\@function,3
.align    32
${prefix}_encrypt:
.cfi_startproc
    endbranch
# Prolog
    push    %rbp
.cfi_push   %rbp
# Prolog ends here.
.Lossl_${prefix}_encrypt_seh_prolog_end:

    vmovdqu         ($in), %xmm0
    vpshufb         IN_SHUFB(%rip), %xmm0, %xmm0

    # note: to simplify binary instructions translation
    mov             $ks, %r10

    vsm4rnds4       (%r10), %xmm0, %xmm0
    vsm4rnds4       16(%r10), %xmm0, %xmm0
    vsm4rnds4       32(%r10), %xmm0, %xmm0
    vsm4rnds4       48(%r10), %xmm0, %xmm0
    vsm4rnds4       64(%r10), %xmm0, %xmm0
    vsm4rnds4       80(%r10), %xmm0, %xmm0
    vsm4rnds4       96(%r10), %xmm0, %xmm0
    vsm4rnds4       112(%r10), %xmm0, %xmm0

    vpshufb         OUT_SHUFB(%rip), %xmm0, %xmm0
    vmovdqu         %xmm0, ($out)
    vpxor           %xmm0, %xmm0, %xmm0 # clear register
    pop             %rbp
.cfi_pop            %rbp
    ret
.cfi_endproc

# void ${prefix}_decrypt(const uint8_t *in, uint8_t *out, const SM4_KEY *ks)

.globl    ${prefix}_decrypt
.type     ${prefix}_decrypt,\@function,3
.align    32
${prefix}_decrypt:
.cfi_startproc
    endbranch
# Prolog
    push    %rbp
.cfi_push   %rbp
# Prolog ends here.
.Lossl_${prefix}_decrypt_seh_prolog_end:

    vmovdqu         ($in), %xmm0
    vpshufb         IN_SHUFB(%rip), %xmm0, %xmm0

    vmovdqu         112($ks), %xmm1
    vpshufd         \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0
    vmovdqu         96($ks), %xmm1
    vpshufd         \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0
    vmovdqu         80($ks), %xmm1
    vpshufd          \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0
    vmovdqu         64($ks), %xmm1
    vpshufd         \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0
    vmovdqu         48($ks), %xmm1
    vpshufd         \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0
    vmovdqu         32($ks), %xmm1
    vpshufd         \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0
    vmovdqu         16($ks), %xmm1
    vpshufd         \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0
    vmovdqu         ($ks), %xmm1
    vpshufd         \$27, %xmm1, %xmm1
    vsm4rnds4       %xmm1, %xmm0, %xmm0

    vpshufb         OUT_SHUFB(%rip), %xmm0, %xmm0
    vmovdqu         %xmm0, ($out)
    vpxor           %xmm0, %xmm0, %xmm0 # clear registers
    vpxor           %xmm1, %xmm1, %xmm1
    pop             %rbp
.cfi_pop            %rbp
    ret
.cfi_endproc
___
}

} else { # fallback for the unsupported configurations with undefined instruction

$code .= <<___;
.text

.globl    ${prefix}_set_key
.type     ${prefix}_set_key,\@abi-omnipotent
${prefix}_set_key:
    .byte   0x0f,0x0b    # ud2
    ret
.size     ${prefix}_set_key, .-${prefix}_set_key

.globl    ${prefix}_encrypt
.type ${prefix}_encrypt,\@abi-omnipotent
${prefix}_encrypt:
    .byte   0x0f,0x0b    # ud2
    ret
.size   ${prefix}_encrypt, .-${prefix}_encrypt

.globl    ${prefix}_decrypt
.type     ${prefix}_decrypt,\@abi-omnipotent
${prefix}_decrypt:
    .byte   0x0f,0x0b    # ud2
    ret
.size     ${prefix}_decrypt, .-${prefix}_decrypt
___
} # avx2_sm4_ni

if ($avx2_sm4_ni_native > 0) { # SM4 instructions are supported in asm
  $code =~ s/\`([^\`]*)\`/eval $1/gem;
  print $code;
} else { # binary translation for SM4 instructions
  sub sm4op {
    my $instr = shift;
    my $args = shift;
    if ($args =~ /^(.+)\s*#/) {
      $args = $1; # drop comment and its leading whitespace
    }
    if (($instr eq "vsm4key4") && ($args =~ /%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})/)) {
      my $b1 = sprintf("0x%02x", 0x62 | ((1-int($1/8))<<5) | ((1-int($3/8))<<7) );
      my $b2 = sprintf("0x%02x", 0x02 | (15 - $2 & 15)<<3                       );
      my $b3 = sprintf("0x%02x", 0xc0 | ($1 & 7) | (($3 & 7)<<3)                );
      return ".byte 0xc4,".$b1.",".$b2.",0xda,".$b3;
    }
    elsif (($instr eq "vsm4rnds4") && ($args =~ /(\d*)\(([^)]+)\)\s*,\s*%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})/)) {
      my $shift = $1;
      my $b3_offset = 0x00;
      if ($shift) {
        $shift = ",0x".sprintf("%02x", $shift);
        $b3_offset = 0x40;
      }
      my $b1 = sprintf("0x%02x", 0x42 | ((1-int($4/8))<<7) );
      my $b2 = sprintf("0x%02x", 0x03 | (15 - $3 & 15)<<3                       );
      my $b3 = sprintf("0x%02x", 0x02 | ($4 & 7)<<3 | $b3_offset                );
      return ".byte 0xc4,".$b1.",".$b2.",0xda,".$b3.$shift;
    }
    elsif (($instr eq "vsm4rnds4") && ($args =~ /%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})\s*,\s*%xmm(\d{1,2})/)) {
      my $b1 = sprintf("0x%02x", 0x62 | ((1-int($1/8))<<5) | ((1-int($3/8))<<7) );
      my $b2 = sprintf("0x%02x", 0x03 | (15 - $2 & 15)<<3                       );
      my $b3 = sprintf("0x%02x", 0xc0 | ($1 & 7) | (($3 & 7)<<3)                );
      return ".byte 0xc4,".$b1.",".$b2.",0xda,".$b3;
    }
    return $instr."\t".$args;
  }

  foreach (split("\n",$code)) {
    s/\`([^\`]*)\`/eval $1/geo;
    s/\b(vsm4[^\s]*)\s+(.*)/sm4op($1,$2)/geo;
    print $_,"\n";
  }
} # avx2_sm4_ni_native > 0

close STDOUT or die "error closing STDOUT: $!";
