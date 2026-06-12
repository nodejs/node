#! /usr/bin/env perl
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
# Copyright (c) 2025, Intel Corporation. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html
#
# Implements AES-CFB128 encryption and decryption with Intel(R) VAES

$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$avx512vaes=0; # will be non-zero if tooling supports Intel AVX-512 and VAES

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
        =~ /GNU assembler version ([2-9]\.[0-9]+)/) {
    $avx512vaes = ($1>=2.30);
}

if (!$avx512vaes && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
       `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)(?:\.([0-9]+))?/) {
    $avx512vaes = ($1==2.13 && $2>=3) + ($1>=2.14);
}

if (!$avx512vaes && $win64 && ($flavour =~ /masm/ || $ENV{ASM} =~ /ml64/) &&
       `ml64 2>&1` =~ /Version ([0-9]+\.[0-9]+)\./) {
    $avx512vaes = ($1>=14.16);
}

if (!$avx512vaes && `$ENV{CC} -v 2>&1`
    =~ /(Apple)?\s*((?:clang|LLVM) version|.*based on LLVM) ([0-9]+)\.([0-9]+)\.([0-9]+)?/) {
    my $ver = $3 + $4/100.0 + $5/10000.0; # 3.1.0->3.01, 3.10.1->3.1001
    if ($1) {
        # Apple conditions, they use a different version series, see
        # https://en.wikipedia.org/wiki/Xcode#Xcode_7.0_-_10.x_(since_Free_On-Device_Development)_2
        # clang 7.0.0 is Apple clang 10.0.1
        $avx512vaes = ($ver>=10.0001)
    } else {
        $avx512vaes = ($ver>=7.0);
    }
}

open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT=*OUT;

##################################################################

$code=".text\n";

if ($avx512vaes) {

$code.=<<___;
.extern  OPENSSL_ia32cap_P

#################################################################
# Signature:
#
# int ossl_aes_cfb128_vaes_eligible(void);
#
# Detects if the underlying hardware supports all the features
# required to run the Intel AVX-512 implementations of AES-CFB128 algorithms.
#
# Returns: non zero if all the required features are detected, 0 otherwise
#################################################################

.globl   ossl_aes_cfb128_vaes_eligible
.type    ossl_aes_cfb128_vaes_eligible,\@abi-omnipotent
.balign  64

ossl_aes_cfb128_vaes_eligible:
.cfi_startproc
    endbranch

    mov OPENSSL_ia32cap_P+8(%rip),%ecx
    xor %eax,%eax

    # Check 3rd 32-bit word of OPENSSL_ia32cap_P for the feature bit(s):
    # AVX512BW (bit 30) + AVX512DQ (bit 17) + AVX512F (bit 16)

    and \$0x40030000,%ecx                 # mask is 1<<30|1<<17|1<<16
    cmp \$0x40030000,%ecx
    jne .Laes_cfb128_vaes_eligible_done

    mov OPENSSL_ia32cap_P+12(%rip),%ecx

    # Check 4th 32-bit word of OPENSSL_ia32cap_P for the feature bit(s):
    # AVX512VAES (bit 9)

    and \$0x200,%ecx                      # mask is 1<<9
    cmp \$0x200,%ecx
    cmove %ecx,%eax

.Laes_cfb128_vaes_eligible_done:
    ret
.cfi_endproc
.size   ossl_aes_cfb128_vaes_eligible, .-ossl_aes_cfb128_vaes_eligible
___

#################################################################
#
# AES subroutines for:
# - preloading the AES key schedule into AVX registers
# - single-block AES encryption used by CFB encryption and decryption
# - multiple-block AES encryption used by CFB decryption
#
# The CFB mode only uses block cipher encryption.
#
# The AES encryption step is described in Section 5.1 Cipher() of
# FIPS 197 https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf
# and implemented with Intel(R) AES-NI and VAES instructions:
#
# - AESKEYGENASSIST for key expansion, elsewhere in aesni_set_encrypt_key()
# - VPXORD for AES pre-whitening
# - VAESENC for performing one AES encryption round
# - VAESENCLAST for performing the last AES encryption round
#
# For more information please consult:
# - the Intel(R) 64 and IA-32 Architectures Optimization Reference Manual,
# Chapter 21: Cryptography & Finite Field Arithmetic Instructions
# https://www.intel.com/content/www/us/en/developer/articles/technical/intel64-and-ia32-architectures-optimization.html
# - the Intel(R) Advanced Encryption Standard (AES) New Instructions Set Whitepaper
# https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf
#
#################################################################

# expects the key schedule address in $key_original
sub load_aes_key_schedule_1x() {
$code.=<<___;
    vmovdqu8   0($key_original),%xmm17  # schedule  0 whitening
    vmovdqu8  16($key_original),%xmm18  #           1
    vmovdqu8  32($key_original),%xmm19  #           2
    vmovdqu8  48($key_original),%xmm20  #           3
    vmovdqu8  64($key_original),%xmm21  #           4
    vmovdqu8  80($key_original),%xmm22  #           5
    vmovdqu8  96($key_original),%xmm23  #           6
    vmovdqu8 112($key_original),%xmm24  #           7
    vmovdqu8 128($key_original),%xmm25  #           8
    vmovdqu8 144($key_original),%xmm26  #           9
    vmovdqu8 160($key_original),%xmm27  #          10 last for AES-128
    vmovdqu8 176($key_original),%xmm28  #          11
    vmovdqu8 192($key_original),%xmm29  #          12 last for AES-192
    vmovdqu8 208($key_original),%xmm30  #          13
    vmovdqu8 224($key_original),%xmm31  #          14 last for AES-256

    mov 240($key_original),$rounds      # load AES rounds
                                        # 240 is the byte-offset of the rounds field in AES_KEY
___
}


# expects the key schedule address in $key_original
sub load_aes_key_schedule_4x() {
$code.=<<___;
    vbroadcasti32x4   0($key_original),%zmm17  # schedule  0 whitening
    vbroadcasti32x4  16($key_original),%zmm18  #           1
    vbroadcasti32x4  32($key_original),%zmm19  #           2
    vbroadcasti32x4  48($key_original),%zmm20  #           3
    vbroadcasti32x4  64($key_original),%zmm21  #           4
    vbroadcasti32x4  80($key_original),%zmm22  #           5
    vbroadcasti32x4  96($key_original),%zmm23  #           6
    vbroadcasti32x4 112($key_original),%zmm24  #           7
    vbroadcasti32x4 128($key_original),%zmm25  #           8
    vbroadcasti32x4 144($key_original),%zmm26  #           9
    vbroadcasti32x4 160($key_original),%zmm27  #          10 last for AES-128
    vbroadcasti32x4 176($key_original),%zmm28  #          11
    vbroadcasti32x4 192($key_original),%zmm29  #          12 last for AES-192
    vbroadcasti32x4 208($key_original),%zmm30  #          13
    vbroadcasti32x4 224($key_original),%zmm31  #          14 last for AES-256

    mov 240($key_original),$rounds             # load AES rounds
                                               # 240 is the byte-offset of the rounds field in AES_KEY
___
}

# Performs AES encryption of 1 128-bit block
# Expects iv in $temp, non-final AES rounds in $rounds and key schedule in xmm17..31
sub vaes_encrypt_block_1x() {
    my ($label_prefix)=@_;
$code.=<<___;
    vpxord  %xmm17,$temp,$temp     # AES pre-whitening
    vaesenc %xmm18,$temp,$temp
    vaesenc %xmm19,$temp,$temp
    vaesenc %xmm20,$temp,$temp
    vaesenc %xmm21,$temp,$temp
    vaesenc %xmm22,$temp,$temp
    vaesenc %xmm23,$temp,$temp
    vaesenc %xmm24,$temp,$temp
    vaesenc %xmm25,$temp,$temp
    vaesenc %xmm26,$temp,$temp

    cmp \$0x09,$rounds
    ja ${label_prefix}_192_256

    vaesenclast %xmm27,$temp,$temp # last AES-128 encryption round
    jmp ${label_prefix}_end

.balign 32
${label_prefix}_192_256:

    vaesenc %xmm27,$temp,$temp
    vaesenc %xmm28,$temp,$temp

    cmp \$0x0B,$rounds
    ja ${label_prefix}_256

    vaesenclast %xmm29,$temp,$temp # last AES-192 encryption round
    jmp ${label_prefix}_end

.balign 32
${label_prefix}_256:

    vaesenc %xmm29,$temp,$temp
    vaesenc %xmm30,$temp,$temp
    vaesenclast %xmm31,$temp,$temp # last AES-256 encryption round

.balign 32
${label_prefix}_end:
___
}


# Performs parallel AES encryption of 4 128-bit blocks
# Expects iv in $temp_4x, non-final AES rounds in $rounds and key schedule in zmm17..31
sub vaes_encrypt_block_4x() {
    my ($label_prefix)=@_;
$code.=<<___;
    vpxord  %zmm17,$temp_4x,$temp_4x     # AES pre-whitening
    vaesenc %zmm18,$temp_4x,$temp_4x
    vaesenc %zmm19,$temp_4x,$temp_4x
    vaesenc %zmm20,$temp_4x,$temp_4x
    vaesenc %zmm21,$temp_4x,$temp_4x
    vaesenc %zmm22,$temp_4x,$temp_4x
    vaesenc %zmm23,$temp_4x,$temp_4x
    vaesenc %zmm24,$temp_4x,$temp_4x
    vaesenc %zmm25,$temp_4x,$temp_4x
    vaesenc %zmm26,$temp_4x,$temp_4x

    cmp \$0x09,$rounds
    ja ${label_prefix}_192_256

    vaesenclast %zmm27,$temp_4x,$temp_4x # last AES-128 encryption round
    jmp ${label_prefix}_end

.balign 32
${label_prefix}_192_256:

    vaesenc %zmm27,$temp_4x,$temp_4x
    vaesenc %zmm28,$temp_4x,$temp_4x

    cmp \$0x0B,$rounds
    ja ${label_prefix}_256

    vaesenclast %zmm29,$temp_4x,$temp_4x # last AES-192 encryption round
    jmp ${label_prefix}_end

.balign 32
${label_prefix}_256:

    vaesenc %zmm29,$temp_4x,$temp_4x
    vaesenc %zmm30,$temp_4x,$temp_4x
    vaesenclast %zmm31,$temp_4x,$temp_4x # last AES-256 encryption round

.balign 32
${label_prefix}_end:
___
}


# Performs parallel AES encryption of 16 128-bit blocks
# Expects input in $temp_*x, non-final AES rounds in $rounds and key schedule in zmm17..31
sub vaes_encrypt_block_16x() {
    my ($label_prefix)=@_;
$code.=<<___;
    vpxord %zmm17,$temp_4x, $temp_4x       # parallel AES pre-whitening
    vpxord %zmm17,$temp_8x, $temp_8x
    vpxord %zmm17,$temp_12x,$temp_12x
    vpxord %zmm17,$temp_16x,$temp_16x

    vaesenc %zmm18,$temp_4x, $temp_4x
    vaesenc %zmm18,$temp_8x, $temp_8x
    vaesenc %zmm18,$temp_12x,$temp_12x
    vaesenc %zmm18,$temp_16x,$temp_16x

    vaesenc %zmm19,$temp_4x, $temp_4x
    vaesenc %zmm19,$temp_8x, $temp_8x
    vaesenc %zmm19,$temp_12x,$temp_12x
    vaesenc %zmm19,$temp_16x,$temp_16x

    vaesenc %zmm20,$temp_4x, $temp_4x
    vaesenc %zmm20,$temp_8x, $temp_8x
    vaesenc %zmm20,$temp_12x,$temp_12x
    vaesenc %zmm20,$temp_16x,$temp_16x

    vaesenc %zmm21,$temp_4x, $temp_4x
    vaesenc %zmm21,$temp_8x, $temp_8x
    vaesenc %zmm21,$temp_12x,$temp_12x
    vaesenc %zmm21,$temp_16x,$temp_16x

    vaesenc %zmm22,$temp_4x, $temp_4x
    vaesenc %zmm22,$temp_8x, $temp_8x
    vaesenc %zmm22,$temp_12x,$temp_12x
    vaesenc %zmm22,$temp_16x,$temp_16x

    vaesenc %zmm23,$temp_4x, $temp_4x
    vaesenc %zmm23,$temp_8x, $temp_8x
    vaesenc %zmm23,$temp_12x,$temp_12x
    vaesenc %zmm23,$temp_16x,$temp_16x

    vaesenc %zmm24,$temp_4x, $temp_4x
    vaesenc %zmm24,$temp_8x, $temp_8x
    vaesenc %zmm24,$temp_12x,$temp_12x
    vaesenc %zmm24,$temp_16x,$temp_16x

    vaesenc %zmm25,$temp_4x, $temp_4x
    vaesenc %zmm25,$temp_8x, $temp_8x
    vaesenc %zmm25,$temp_12x,$temp_12x
    vaesenc %zmm25,$temp_16x,$temp_16x

    vaesenc %zmm26,$temp_4x, $temp_4x
    vaesenc %zmm26,$temp_8x, $temp_8x
    vaesenc %zmm26,$temp_12x,$temp_12x
    vaesenc %zmm26,$temp_16x,$temp_16x

    cmp \$0x09,$rounds
    ja ${label_prefix}_192_256

    vaesenclast %zmm27,$temp_4x, $temp_4x  # last AES-128 encryption round
    vaesenclast %zmm27,$temp_8x, $temp_8x
    vaesenclast %zmm27,$temp_12x,$temp_12x
    vaesenclast %zmm27,$temp_16x,$temp_16x
    jmp ${label_prefix}_end

.balign 32
${label_prefix}_192_256:

    vaesenc %zmm27,$temp_4x, $temp_4x
    vaesenc %zmm27,$temp_8x, $temp_8x
    vaesenc %zmm27,$temp_12x,$temp_12x
    vaesenc %zmm27,$temp_16x,$temp_16x

    vaesenc %zmm28,$temp_4x, $temp_4x
    vaesenc %zmm28,$temp_8x, $temp_8x
    vaesenc %zmm28,$temp_12x,$temp_12x
    vaesenc %zmm28,$temp_16x,$temp_16x

    cmp \$0x0B,$rounds
    ja ${label_prefix}_256

    vaesenclast %zmm29,$temp_4x, $temp_4x  # last AES-192 encryption round
    vaesenclast %zmm29,$temp_8x, $temp_8x
    vaesenclast %zmm29,$temp_12x,$temp_12x
    vaesenclast %zmm29,$temp_16x,$temp_16x
    jmp ${label_prefix}_end

.balign 32
${label_prefix}_256:

    vaesenc %zmm29,$temp_4x, $temp_4x
    vaesenc %zmm29,$temp_8x, $temp_8x
    vaesenc %zmm29,$temp_12x,$temp_12x
    vaesenc %zmm29,$temp_16x,$temp_16x

    vaesenc %zmm30,$temp_4x, $temp_4x
    vaesenc %zmm30,$temp_8x, $temp_8x
    vaesenc %zmm30,$temp_12x,$temp_12x
    vaesenc %zmm30,$temp_16x,$temp_16x

    vaesenclast %zmm31,$temp_4x, $temp_4x  # last AES-256 encryption round
    vaesenclast %zmm31,$temp_8x, $temp_8x
    vaesenclast %zmm31,$temp_12x,$temp_12x
    vaesenclast %zmm31,$temp_16x,$temp_16x

.balign 32
${label_prefix}_end:
___
}

#################################################################
# Signature:
#
# void ossl_aes_cfb128_vaes_enc(
#     const unsigned char *in,
#     unsigned char *out,
#     size_t len,
#     const AES_KEY *ks,
#     const unsigned char ivec[16],
#     /*in-out*/ ossl_ssize_t *num);
#
# Preconditions:
# - all pointers are valid (not NULL...)
# - AES key schedule and rounds in `ks` are precomputed
#
# Invariants:
# - `*num` is between 0 and 15 (inclusive)
#
#################################################################
#
# The implementation follows closely the encryption half of CRYPTO_cfb128_encrypt:
# - "pre" step: processes the last bytes of a partial block
# - "mid" step: processes complete blocks
# - "post" step: processes the first bytes of a partial block
#
# To obtain the next ciphertext block `cipher <N>` from
# the plaintext block `plain <N>`, the previous ciphertext
# block `cipher <N-1>` is required as input.
#
# The dependency on previous encryption outputs (ciphertexts)
# makes CFB encryption inherently serial.
#
#                  +----+                       +----------+
#                  | iv |       +---------------> cipher 0 |
#                  +--+-+       |               +----------+
#                     |         |                     |
#                     |         |                     |
#              +------v------+  |              +------v------+
#              | AES encrypt |  |              | AES encrypt |
#              |  with  key  |  |              |  with  key  |
#              +------+------+  |              +------+------+
#                     |         |                     |
#                     |         |                     |
#   +---------+    +--v--+      |   +---------+    +--v--+
#   | plain 0 +----> XOR |      |   | plain 1 +----> XOR |
#   +---------+    +--+--+      |   +---------+    +--+--+
#                     |         |                     |
#                     |         |                     |
#               +-----v----+    |               +-----v----+
#               | cipher 0 +----+               | cipher 1 |
#               +----------+                    +----------+
#
#################################################################

$code.=<<___;
.globl   ossl_aes_cfb128_vaes_enc
.type    ossl_aes_cfb128_vaes_enc,\@function,6
.balign  64
ossl_aes_cfb128_vaes_enc:
.cfi_startproc
    endbranch
___

$inp="%rdi";          # arg0
$out="%rsi";          # arg1
$len="%rdx";          # arg2

$key_original="%rcx"; # arg3
$key_backup="%r10";

$ivp="%r8";           # arg4
$nump="%r9";          # arg5

$num="%r11";
$left="%rcx";
$mask="%rax";

$rounds="%r11d";

$temp="%xmm2";
$plain="%xmm3";

$code.=<<___;

    mov ($nump),$num                 # $num is the current byte index in the first partial block
                                     # $num belongs to 0..15; non-zero means a partial first block

    test $len,$len                   # return early if $len==0, unlikely to occur
    jz .Laes_cfb128_vaes_enc_done

    test $num,$num                   # check if the first block is partial
    jz .Laes_cfb128_enc_mid          # if not, jump to processing full blocks

###########################################################
# first partial block pre-processing
###########################################################

    mov $key_original,$key_backup    # make room for variable shl with cl

    mov \$0x10,$left                 # first block is partial
    sub $num,$left                   # calculate how many bytes $left to process in the block
    cmp $len,$left                   #
    cmova $len,$left                 # $left = min(16-$num,$len)

    mov \$1,$mask                    # build a mask with the least significant $left bits set
    shlq %cl,$mask                   # $left is left shift counter
    dec $mask                        # $mask is 2^$left-1
    kmovq $mask,%k1

    mov $num,%rax                    # keep in-out $num in %al
    add $left,%rax                   # advance $num
    and \$0x0F,%al                   # wrap-around $num in a 16-byte block

    leaq ($num,$ivp),%r11            # process $left iv bytes
    vmovdqu8 (%r11),%xmm0
    vmovdqu8 ($inp),%xmm1            # process $left input bytes
    vpxor %xmm0,%xmm1,%xmm2          # CipherFeedBack XOR
    vmovdqu8 %xmm2,($out){%k1}       # write $left output bytes
    vmovdqu8 %xmm2,(%r11){%k1}       # blend $left output bytes into iv

    add $left,$inp                   # advance pointers
    add $left,$out
    sub $left,$len
    jz .Laes_cfb128_enc_zero_pre     # return early if no AES encryption required

    mov $key_backup,$key_original    # restore "key_original" as arg3

.Laes_cfb128_enc_mid:
___

    &load_aes_key_schedule_1x();

$code.=<<___;
###########################################################
# inner full blocks processing
###########################################################

    vmovdqu ($ivp),$temp             # load iv

    cmp \$0x10,$len                  # is there a full plaintext block left (128 bits) ?
    jb .Laes_cfb128_enc_post

.balign 32
.Loop_aes_cfb128_enc_main:
    sub \$0x10,$len

    vmovdqu ($inp),$plain            # load plaintext block
    lea 16($inp),$inp                # $inp points to next plaintext
___

    &vaes_encrypt_block_1x(".Laes_cfb128_enc_mid");

$code.=<<___;

    vpxor $plain,$temp,$temp         # CipherFeedBack XOR
    cmp \$0x10,$len
    vmovdqu $temp,($out)             # write ciphertext
    lea 16($out),$out                # $out points to the next output block
    jae .Loop_aes_cfb128_enc_main

    xor %eax,%eax                    # reset num when processing full blocks

    vmovdqu $temp,($ivp)             # latest ciphertext block is next encryption input

.Laes_cfb128_enc_post:

###########################################################
# last partial block post-processing
###########################################################

    test $len,$len                   # check if the last block is partial
    jz .Laes_cfb128_enc_zero_all
___

    &vaes_encrypt_block_1x(".Laes_cfb128_enc_post");

$code.=<<___;

    mov $len,%rax                    # num=$len

    mov \$1,%r11                     # build a mask with the least significant $len bits set
    mov %dl,%cl                      # $len is left shift counter less than 16
    shlq %cl,%r11
    dec %r11                         # mask is 2^$len-1
    kmovq %r11,%k1

    vmovdqu8 ($inp),%xmm1{%k1}{z}    # read $len input bytes, zero the rest to not impact XOR
    vpxor $temp,%xmm1,%xmm0          # CipherFeedBack XOR
    vmovdqu8 %xmm0,($out){%k1}       # write $len output bytes
    vmovdqu8 %xmm0,($ivp)            # write chained/streaming iv

    # clear registers

.Laes_cfb128_enc_zero_all:
    vpxord %xmm17,%xmm17,%xmm17      # clear the AES key schedule
    vpxord %xmm18,%xmm18,%xmm18
    vpxord %xmm19,%xmm19,%xmm19
    vpxord %xmm20,%xmm20,%xmm20
    vpxord %xmm21,%xmm21,%xmm21
    vpxord %xmm22,%xmm22,%xmm22
    vpxord %xmm23,%xmm23,%xmm23
    vpxord %xmm24,%xmm24,%xmm24
    vpxord %xmm25,%xmm25,%xmm25
    vpxord %xmm26,%xmm26,%xmm26
    vpxord %xmm27,%xmm27,%xmm27
    vpxord %xmm28,%xmm28,%xmm28
    vpxord %xmm29,%xmm29,%xmm29
    vpxord %xmm30,%xmm30,%xmm30
    vpxord %xmm31,%xmm31,%xmm31

    vpxor %xmm3,%xmm3,%xmm3          # clear registers used during AES encryption

.Laes_cfb128_enc_zero_pre:
    vpxor %xmm0,%xmm0,%xmm0          # clear the rest of the registers
    vpxor %xmm1,%xmm1,%xmm1
    vpxor %xmm2,%xmm2,%xmm2

    mov %rax,($nump)                 # num is in/out, update for future/chained calls

    vzeroupper

.Laes_cfb128_vaes_enc_done:
    ret
.cfi_endproc
.size ossl_aes_cfb128_vaes_enc,.-ossl_aes_cfb128_vaes_enc
___


#################################################################
# Signature:
#
# void ossl_aes_cfb128_vaes_dec(
#     const unsigned char *in,
#     unsigned char *out,
#     size_t len,
#     const AES_KEY *ks,
#     const unsigned char ivec[16],
#     /*in-out*/ ossl_ssize_t *num);
#
# Preconditions:
# - all pointers are valid (not NULL...)
# - AES key schedule and rounds in `ks` are precomputed
#
# Invariants:
# - `*num` is between 0 and 15 (inclusive)
#
#################################################################
#
# The implementation follows closely the decryption half of CRYPTO_cfb128_encrypt:
#
# - "pre" step: processes the last bytes of a partial block
# - "mid" step: processes complete blocks using an unrolled approach:
#   - processes 16 blocks in parallel until fewer than 16 blocks remain
#   - processes  4 blocks in parallel until fewer than  4 blocks remain
#   - processes  1 block  in series   until none are left
# - "post" step: processes the first bytes of a partial block
#
# To obtain the next plaintext block `plain <N>` from
# its ciphertext block `cipher <N>`, the previous ciphertext
# block `cipher <N-1>` is required as input.
#
# Since CFB decryption for the current block only depends on
# iv and ciphertext blocks (already available as inputs)
# and not on plaintext blocks, it can be efficiently parallelized.
#
#     +----+                    +----------+                +----------+                +----------+
#     | iv |                    | cipher 0 |                | cipher 1 |                | cipher 2 |
#     +--+-+                    +----+-----+                +----+-----+                +----+-----+
#        |                           |                           |                           |
#        |                           |                           |                           |
# +------v------+             +------v------+             +------v------+             +------v------+
# | AES encrypt |             | AES encrypt |             | AES encrypt |             | AES encrypt |
# |  with  key  |             |  with  key  |             |  with  key  |             |  with  key  |
# +------+------+             +------+------+             +------+------+             +------+------+
#        |                           |                           |                           |
#        |                           |                           |                           |
#     +--v--+     +----------+    +--v--+     +----------+    +--v--+     +----------+    +--v--+     +----------+
#     | XOR <-----+ cipher 0 |    | XOR <-----+ cipher 1 |    | XOR <-----+ cipher 2 |    | XOR <-----+ cipher 3 |
#     +--+--+     +----------+    +--+--+     +----------+    +--+--+     +----------+    +--+--+     +----------+
#        |                           |                           |                           |
#        |                           |                           |                           |
#   +----v----+                 +----v----+                 +----v----+                 +----v----+
#   | plain 0 |                 | plain 1 |                 | plain 2 |                 | plain 3 |
#   +---------+                 +---------+                 +---------+                 +---------+
#
# To produce N (4 in the diagram above) output/plaintext blocks we require as inputs:
# - iv
# - N ciphertext blocks
# The N-th ciphertext block is not encrypted and becomes the next iv input.
#
#################################################################

$code.=<<___;
.globl   ossl_aes_cfb128_vaes_dec
.type    ossl_aes_cfb128_vaes_dec,\@function,6
.balign  64
ossl_aes_cfb128_vaes_dec:
.cfi_startproc
    endbranch
___

$inp="%rdi";          # arg0
$out="%rsi";          # arg1
$len="%rdx";          # arg2

$key_original="%rcx"; # arg3
$key_backup="%r10";

$ivp="%r8";           # arg4
$nump="%r9";          # arg5

$num="%r11";
$left="%rcx";
$mask="%rax";

$rounds="%r11d";

$temp="%xmm2";
$temp_4x="%zmm2";
$temp_8x="%zmm4";
$temp_12x="%zmm0";
$temp_16x="%zmm6";

$cipher="%xmm3";
$cipher_4x="%zmm3";
$cipher_8x="%zmm5";
$cipher_12x="%zmm1";
$cipher_16x="%zmm16";

$code.=<<___;

    mov ($nump),$num                  # $num is the current byte index in the first partial block
                                      # $num belongs to 0..15; non-zero means a partial first block

    test $len,$len                    # return early if $len==0, unlikely to occur
    jz .Laes_cfb128_vaes_dec_done
___

$code.=<<___ if($win64);
    sub \$0x10,%rsp
.cfi_adjust_cfa_offset 16
    vmovdqu %xmm6,(%rsp)              # xmm6 needs to be maintained for Windows
___

$code.=<<___;
    test $num,$num                    # check if the first block is partial
    jz .Laes_cfb128_dec_mid           # if not, jump to processing full blocks

###########################################################
# first partial block pre-processing
###########################################################

    mov $key_original,$key_backup     # make room for variable shl with cl

    mov \$0x10,$left                  # first block is partial
    sub $num,$left                    # calculate how many bytes $left to process in the block
    cmp $len,$left                    #
    cmova $len,$left                  # $left = min(16-$num,$len)

    mov \$1,$mask                     # build a mask with the least significant $left bits set
    shlq %cl,$mask                    # $left is left shift counter
    dec $mask                         # $mask is 2^$left-1
    kmovq $mask,%k1

    lea ($num,$left),%rax             # keep in-out num in %al, advance by $left
    and \$0x0F,%al                    # wrap-around in a 16-byte block

    leaq ($num,$ivp),%r11             # process $left iv bytes
    vmovdqu8 (%r11),%xmm0
    vmovdqu8 ($inp),%xmm1             # process $left input bytes
    vpxor %xmm0,%xmm1,%xmm2           # CipherFeedBack XOR
    vmovdqu8 %xmm2,($out){%k1}        # write $left output bytes
    vmovdqu8 %xmm1,(%r11){%k1}        # blend $left input bytes into iv

    add $left,$inp                    # advance pointers
    add $left,$out
    sub $left,$len
    jz .Laes_cfb128_dec_zero_pre      # return early if no AES encryption required

    mov $key_backup,$key_original     # restore "key_original" as arg3

.Laes_cfb128_dec_mid:
___

    &load_aes_key_schedule_4x();

$code.=<<___;
###########################################################
# inner full blocks processing
###########################################################

    # $temp_4x is "iv | iv | iv | iv"
    vbroadcasti32x4 ($ivp),$temp_4x             # load iv

    cmp \$0x100,$len                            # are there 16 ciphertext blocks left (2048 bits) ?
    jb .Laes_cfb128_dec_check_4x

###########################################################
# decrypt groups of 16 128-bit blocks in parallel
# behaves as 16x loop unroll
###########################################################

.balign 32
.Loop_aes_cfb128_dec_mid_16x:
    sub \$0x100,$len

    # load 16 ciphertext blocks

    # $cipher_4x  is "ciphertext  0 | ciphertext  1 | ciphertext  2 | ciphertext  3"
    vmovdqu32 ($inp),$cipher_4x
    # $cipher_8x  is "ciphertext  4 | ciphertext  5 | ciphertext  6 | ciphertext  7"
    vmovdqu32 64($inp),$cipher_8x
    # $cipher_12x is "ciphertext  8 | ciphertext  9 | ciphertext 10 | ciphertext 11"
    vmovdqu32 128($inp),$cipher_12x
    # $cipher_16x is "ciphertext 12 | ciphertext 13 | ciphertext 14 | ciphertext 15"
    vmovdqu32 192($inp),$cipher_16x

    # $temp_4x  is   "iv            | ciphertext  0 | ciphertext  1 | ciphertext  2"
    valignq \$6,$temp_4x,$cipher_4x,$temp_4x
    # $temp_8x  is   "ciphertext  3 | ciphertext  4 | ciphertext  5 | ciphertext  6"
    valignq \$6,$cipher_4x,$cipher_8x,$temp_8x
    # $temp_12x is   "ciphertext  7 | ciphertext  8 | ciphertext  9 | ciphertext 10"
    valignq \$6,$cipher_8x,$cipher_12x,$temp_12x
    # $temp_16x is   "ciphertext 11 | ciphertext 12 | ciphertext 13 | ciphertext 14"
    valignq \$6,$cipher_12x,$cipher_16x,$temp_16x

    lea 256($inp),$inp                          # $inp points to next ciphertext
___

    &vaes_encrypt_block_16x(".Laes_cfb128_dec_mid_16x");

$code.=<<___;

    vpxord $cipher_4x,$temp_4x,$temp_4x         # CipherFeedBack XOR of 16 blocks
    vpxord $cipher_8x,$temp_8x,$temp_8x
    vpxord $cipher_12x,$temp_12x,$temp_12x
    vpxord $cipher_16x,$temp_16x,$temp_16x

    cmp \$0x100,$len

    vmovdqu32 $temp_4x,($out)                   # write 16 plaintext blocks
    vmovdqu32 $temp_8x,64($out)
    vmovdqu32 $temp_12x,128($out)
    vmovdqu32 $temp_16x,192($out)

    vmovdqu8 $cipher_16x,$temp_4x

    lea 256($out),$out                          # $out points to the next output block

    jae .Loop_aes_cfb128_dec_mid_16x

    vextracti64x2 \$3,$cipher_16x,$temp         # latest ciphertext block is next decryption iv
    vinserti32x4 \$3,$temp,$temp_4x,$temp_4x    # move ciphertext3 to positions 0 and 3 in preparation for next shuffle

    xor %eax,%eax                               # reset $num when processing full blocks

    vmovdqu $temp,($ivp)                        # latest plaintext block is next decryption iv

.Laes_cfb128_dec_check_4x:
    cmp \$0x40,$len                             # are there 4 ciphertext blocks left (512 bits) ?
    jb .Laes_cfb128_dec_check_1x

###########################################################
# decrypt groups of 4 128-bit blocks in parallel
# behaves as 4x loop unroll
###########################################################

# expects $temp_4x to contain "iv" in the 3rd (most significant) lane

.balign 32
.Loop_aes_cfb128_dec_mid_4x:
    sub \$0x40,$len

    # $cipher_4x is "ciphertext0 | ciphertext1 | ciphertext 2 | ciphertext 3"
    vmovdqu32 ($inp),$cipher_4x                 # load 4 ciphertext blocks

    # $temp_4x is   "iv          | ciphertext0 | ciphertext 1 | ciphertext 2"
    valignq \$6,$temp_4x,$cipher_4x,$temp_4x

    lea 64($inp),$inp                           # $inp points to next ciphertext
___

    &vaes_encrypt_block_4x(".Laes_cfb128_dec_mid_4x");

$code.=<<___;
    vpxord $cipher_4x,$temp_4x,$temp_4x         # CipherFeedBack XOR of 4 blocks
    cmp \$0x40,$len
    vmovdqu32 $temp_4x,($out)                   # write 4 plaintext blocks
    vmovdqu8 $cipher_4x,$temp_4x
    lea 64($out),$out                           # $out points to the next output block

    jae .Loop_aes_cfb128_dec_mid_4x

    vextracti64x2 \$3,$temp_4x,$temp            # latest ciphertext block is next decryption iv
                                                # move ciphertext3 to position 0 in preparation for next step

    xor %eax,%eax                               # reset $num when processing full blocks

    vmovdqu $temp,($ivp)                        # latest plaintext block is next decryption iv

.Laes_cfb128_dec_check_1x:
    cmp \$0x10,$len                             # are there full ciphertext blocks left (128 bits) ?
    jb .Laes_cfb128_dec_post

###########################################################
# decrypt the rest of full 128-bit blocks in series
###########################################################

# expects $temp to contain iv

.balign 32
.Loop_aes_cfb128_dec_mid_1x:
    sub \$0x10,$len

    vmovdqu ($inp),$cipher            # load ciphertext block
    lea 16($inp),$inp                 # $inp points to next ciphertext
___

    &vaes_encrypt_block_1x(".Loop_aes_cfb128_dec_mid_1x_inner");

$code.=<<___;
    vpxor $cipher,$temp,$temp         # CipherFeedBack XOR
    cmp \$0x10,$len
    vmovdqu $temp,($out)              # write plaintext
    vmovdqu8 $cipher,$temp
    lea 16($out),$out                 # $out points to the next output block
    jae .Loop_aes_cfb128_dec_mid_1x

    xor %eax,%eax                     # reset $num when processing full blocks

    vmovdqu $temp,($ivp)              # latest plaintext block is next decryption input

.Laes_cfb128_dec_post:

###########################################################
# last partial block post-processing
###########################################################

    test $len,$len                     # check if the last block is partial
    jz .Laes_cfb128_dec_zero_all
___

    &vaes_encrypt_block_1x(".Loop_aes_cfb128_dec_post");

$code.=<<___;

    mov $len,%rax                      # num=$len
    mov \$1,%r11                       # build a mask with the least significant $len bits set
    mov %dl,%cl                        # $len is left shift counter less than 16
    shlq %cl,%r11
    dec %r11                           # mask is 2^$len-1
    kmovq %r11,%k1

    vmovdqu8 ($inp),%xmm1{%k1}{z}      # read $len input bytes, zero the rest to not impact XOR
    vpxor $temp,%xmm1,%xmm0            # CipherFeedBack XOR
    vmovdqu8 %xmm0,($out){%k1}         # write $len output bytes
    vpblendmb %xmm1,$temp,${temp}{%k1} # blend $len input bytes into iv

    vmovdqu8 $temp,($ivp)              # write chained/streaming iv

    # clear registers

.Laes_cfb128_dec_zero_all:
    vpxord %xmm17,%xmm17,%xmm17        # clear the AES key schedule
    vpxord %xmm18,%xmm18,%xmm18        # zero the upper lanes of zmm registers
    vpxord %xmm19,%xmm19,%xmm19
    vpxord %xmm20,%xmm20,%xmm20
    vpxord %xmm21,%xmm21,%xmm21
    vpxord %xmm22,%xmm22,%xmm22
    vpxord %xmm23,%xmm23,%xmm23
    vpxord %xmm24,%xmm24,%xmm24
    vpxord %xmm25,%xmm25,%xmm25
    vpxord %xmm26,%xmm26,%xmm26
    vpxord %xmm27,%xmm27,%xmm27
    vpxord %xmm28,%xmm28,%xmm28
    vpxord %xmm29,%xmm29,%xmm29
    vpxord %xmm30,%xmm30,%xmm30
    vpxord %xmm31,%xmm31,%xmm31

    vpxord %xmm3,%xmm3,%xmm3           # clear registers used during AES encryption
    vpxord %xmm4,%xmm4,%xmm4
    vpxord %xmm5,%xmm5,%xmm5
    vpxord %xmm6,%xmm6,%xmm6
    vpxord %xmm16,%xmm16,%xmm16

.Laes_cfb128_dec_zero_pre:

    vpxord %xmm0,%xmm0,%xmm0           # clear the rest of the registers
    vpxord %xmm1,%xmm1,%xmm1
    vpxord %xmm2,%xmm2,%xmm2

    vzeroupper
___

$code.=<<___ if($win64);
    vmovdqu (%rsp),%xmm6               # xmm6 needs to be maintained for Windows
    add \$16,%rsp
.cfi_adjust_cfa_offset -16
___

$code.=<<___;
    mov %rax,($nump)                   # num is in/out, update for future/chained calls

.Laes_cfb128_vaes_dec_done:
    ret
.cfi_endproc
.size ossl_aes_cfb128_vaes_dec,.-ossl_aes_cfb128_vaes_dec
___

} else {

$code .= <<___;
.globl     ossl_aes_cfb128_vaes_enc
.globl     ossl_aes_cfb128_vaes_dec

# Mock implementations of AES-CFB128 encryption/decryption
# that always fail. Should not be executed under normal circumstances.

ossl_aes_cfb128_vaes_enc:
ossl_aes_cfb128_vaes_dec:
    .byte 0x0f,0x0b                # Undefined Instruction in the Intel architecture
                                   # Raises the Invalid Opcode exception
    ret

#################################################################
# Signature:
#
# int ossl_aes_cfb128_vaes_eligible(void);
#
# Always returns 0 (not eligible), meaning that tooling does not support
# the Intel AVX-512 extensions. Signals higher level code to fallback
# to an alternative implementation.
#################################################################

.globl     ossl_aes_cfb128_vaes_eligible
.type      ossl_aes_cfb128_vaes_eligible,\@abi-omnipotent
ossl_aes_cfb128_vaes_eligible:
    xor %eax,%eax
    ret
.size ossl_aes_cfb128_vaes_eligible, .-ossl_aes_cfb128_vaes_eligible
___
}

print $code;

close STDOUT or die "error closing STDOUT: $!";
