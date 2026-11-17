#! /usr/bin/env perl
# Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# Erase the temporary broadcast key schedule used by the AES-CBC VAES
# intrinsic implementation and clear its caller-clobbered register state.

$output = $#ARGV >= 0 && $ARGV[$#ARGV] =~ m|\.\w+$| ? pop : undef;
$flavour = $#ARGV >= 0 && $ARGV[0] !~ m|\.| ? shift : undef;

$win64 = 0;
$win64 = 1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);

$avx512vaes = 0;

$0 =~ m/(.*[\/\\])[^\/\\]+$/;
$dir = $1;
($xlate = "${dir}x86_64-xlate.pl" and -f $xlate)
    or ($xlate = "${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate)
    or die "can't locate x86_64-xlate.pl";

if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
        =~ /GNU assembler version ([2-9]\.[0-9]+)/) {
    $avx512vaes = ($1 >= 2.30);
}

if (!$avx512vaes && $win64
        && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/)
        && `nasm -v 2>&1`
            =~ /NASM version ([2-9]\.[0-9]+)(?:\.([0-9]+))?/) {
    $avx512vaes = ($1 == 2.13 && $2 >= 3) + ($1 >= 2.14);
}

if (!$avx512vaes && $win64
        && ($flavour =~ /masm/ || $ENV{ASM} =~ /ml64/)
        && `ml64 2>&1` =~ /Version ([0-9]+\.[0-9]+)\./) {
    $avx512vaes = ($1 >= 14.16);
}

if (!$avx512vaes && `$ENV{CC} -v 2>&1`
        =~ /(Apple)?\s*((?:clang|LLVM) version|.*based on LLVM) ([0-9]+)\.([0-9]+)\.([0-9]+)?/) {
    $ver = $3 + $4 / 100.0 + $5 / 10000.0;
    if ($1) {
        $avx512vaes = ($ver >= 10.0001);
    } else {
        $avx512vaes = ($ver >= 7.0);
    }
}

if (!$avx512vaes && `$ENV{CC} -x c /dev/null -dM -E 2>/dev/null`
        =~ /#define __clang_major__\s+([0-9]+)/) {
    $avx512vaes = ($1 >= 11); # icx started with clang 11
}

open OUT, "| \"$^X\" \"$xlate\" $flavour \"$output\""
    or die "can't call $xlate: $!";
*STDOUT = *OUT;

$key_schedule = $win64 ? "%rcx" : "%rdi";
$num_keys = $win64 ? "%rdx" : "%rsi";

$code = <<___;
.text

.globl  ossl_aes_cbc_vaes_cleanup_eligible
.hidden ossl_aes_cbc_vaes_cleanup_eligible
.type   ossl_aes_cbc_vaes_cleanup_eligible,\@abi-omnipotent
.align  16
ossl_aes_cbc_vaes_cleanup_eligible:
.cfi_startproc
    endbranch
___

if ($avx512vaes) {
    $code .= <<___;
    mov \$1,%eax
    ret
.cfi_endproc
.size ossl_aes_cbc_vaes_cleanup_eligible,.-ossl_aes_cbc_vaes_cleanup_eligible

# void ossl_aes_cbc_vaes_cleanup(void *key_schedule, size_t num_keys);
#
# num_keys counts 64-byte broadcast round keys. The caller invokes this only
# after VAES use, so AVX-512 instructions are already safe to execute.
# On Win64, the caller's epilogue restores the ABI-preserved low 128 bits of
# XMM6-XMM15 if it used them. Their volatile upper lanes are cleared here.
.globl  ossl_aes_cbc_vaes_cleanup
.hidden ossl_aes_cbc_vaes_cleanup
.type   ossl_aes_cbc_vaes_cleanup,\@abi-omnipotent
.align  32
ossl_aes_cbc_vaes_cleanup:
.cfi_startproc
    endbranch
    vpxord %zmm0,%zmm0,%zmm0
    test $num_keys,$num_keys
    jz .Lvaes_cbc_clear_registers

.Lvaes_cbc_clear_keys:
    vmovdqu64 %zmm0,($key_schedule)
    add \$64,$key_schedule
    dec $num_keys
    jnz .Lvaes_cbc_clear_keys

.Lvaes_cbc_clear_registers:
___

    if ($win64) {
        $code .= <<___;
    # The low 128 bits of XMM6-XMM15 are nonvolatile on Win64. Preserve
    # them, while clearing all of ZMM0-ZMM5 here.
___
        for ($i = 0; $i <= 5; $i++) {
            $code .= "    vpxor %xmm$i,%xmm$i,%xmm$i\n";
        }
    }

    for ($i = 16; $i <= 31; $i++) {
        $code .= "    vpxord %zmm$i,%zmm$i,%zmm$i\n";
    }

    if ($win64) {
        $code .= <<___;
    # Clear the volatile upper lanes of ZMM6-ZMM15 last.
    vzeroupper

    # Clear the Win64 volatile general-purpose registers used by the caller.
    xor %eax,%eax
    xor %ecx,%ecx
    xor %edx,%edx
    xor %r8d,%r8d
    xor %r9d,%r9d
    xor %r10d,%r10d
    xor %r11d,%r11d
___
    } else {
        $code .= <<___;
    # VZEROALL clears ZMM0-ZMM15, but does not affect ZMM16-ZMM31.
    vzeroall

    # Clear the SysV volatile general-purpose registers used by the caller.
    xor %eax,%eax
    xor %ecx,%ecx
    xor %edx,%edx
    xor %esi,%esi
    xor %edi,%edi
    xor %r8d,%r8d
    xor %r9d,%r9d
    xor %r10d,%r10d
    xor %r11d,%r11d
___
    }

    $code .= <<___;
    ret
.cfi_endproc
.size ossl_aes_cbc_vaes_cleanup,.-ossl_aes_cbc_vaes_cleanup
___
} else {
    $code .= <<___;
    xor %eax,%eax
    ret
.cfi_endproc
.size ossl_aes_cbc_vaes_cleanup_eligible,.-ossl_aes_cbc_vaes_cleanup_eligible

.globl  ossl_aes_cbc_vaes_cleanup
.hidden ossl_aes_cbc_vaes_cleanup
.type   ossl_aes_cbc_vaes_cleanup,\@abi-omnipotent
ossl_aes_cbc_vaes_cleanup:
.cfi_startproc
    endbranch
    .byte 0x0f,0x0b                # ud2
    ret
.cfi_endproc
.size ossl_aes_cbc_vaes_cleanup,.-ossl_aes_cbc_vaes_cleanup
___
}

print $code;

close STDOUT or die "error closing STDOUT: $!";
