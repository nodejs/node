// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Push all callee-saved registers to get them on the stack for conservative
// stack scanning.
//
// See asm/x64/push_registers_clang.cc for why the function is not generated
// using clang.

// Do not depend on V8_TARGET_OS_* defines as some embedders may override the
// GN toolchain (e.g. ChromeOS) and not provide them.

// PPC ABI source:
// http://refspecs.linuxfoundation.org/ELF/ppc64/PPC-elf64abi.html

// AIX Runtime process stack:
// https://www.ibm.com/support/knowledgecenter/ssw_aix_71/assembler/idalangref_runtime_process.html
asm(
#if defined(_AIX)
    ".globl .PushAllRegistersAndIterateStack, hidden    \n"
    ".csect .text[PR]                                   \n"
    ".PushAllRegistersAndIterateStack:                  \n"
#else
    ".globl PushAllRegistersAndIterateStack             \n"
    ".type PushAllRegistersAndIterateStack, %function   \n"
    ".hidden PushAllRegistersAndIterateStack            \n"
    "PushAllRegistersAndIterateStack:                   \n"
#endif
    // Push all callee-saved registers.
    // lr, TOC pointer, r16 to r31. 160 bytes.
    // The parameter save area shall be allocated by the caller. 112 bytes.
    // At anytime, SP (r1) needs to be multiple of 16 (i.e. 16-aligned).
    "  mflr 0                                          \n"
    "  std 0, 16(1)                                    \n"
#if defined(_AIX)
    "  std 2, 40(1)                                    \n"
#else
    "  std 2, 24(1)                                    \n"
#endif
    "  stdu 1, -256(1)                                 \n"
    "  std 14, 112(1)                                  \n"
    "  std 15, 120(1)                                  \n"
    "  std 16, 128(1)                                  \n"
    "  std 17, 136(1)                                  \n"
    "  std 18, 144(1)                                  \n"
    "  std 19, 152(1)                                  \n"
    "  std 20, 160(1)                                  \n"
    "  std 21, 168(1)                                  \n"
    "  std 22, 176(1)                                  \n"
    "  std 23, 184(1)                                  \n"
    "  std 24, 192(1)                                  \n"
    "  std 25, 200(1)                                  \n"
    "  std 26, 208(1)                                  \n"
    "  std 27, 216(1)                                  \n"
    "  std 28, 224(1)                                  \n"
    "  std 29, 232(1)                                  \n"
    "  std 30, 240(1)                                  \n"
    "  std 31, 248(1)                                  \n"
    // Pass 1st parameter (r3) unchanged (Stack*).
    // Pass 2nd parameter (r4) unchanged (StackVisitor*).
    // Save 3rd parameter (r5; IterateStackCallback).
    "  mr 6, 5                                         \n"
#if defined(_AIX)
    // Set up TOC for callee.
    "  ld 2,8(5)                                       \n"
    // AIX uses function descriptors, which means that
    // pointers to functions do not point to code, but
    // instead point to metadata about them, hence
    // need to deterrence.
    "  ld 6,0(6)                                       \n"
#endif
    // Pass 3rd parameter as sp (stack pointer).
    "  mr 5, 1                                         \n"
#if !defined(_AIX)
    // Set up r12 to be equal to the callee address (in order for TOC
    // relocation). Only needed on LE Linux.
    "  mr 12, 6                                        \n"
#endif
    // Call the callback.
    "  mtctr 6                                         \n"
    "  bctrl                                           \n"
    // Discard all the registers.
    "  addi 1, 1, 256                                  \n"
    // Restore lr.
    "  ld 0, 16(1)                                     \n"
    "  mtlr  0                                         \n"
#if defined(_AIX)
    // Restore TOC pointer.
    "  ld 2, 40(1)                                     \n"
#else
    "  ld 2, 24(1)                                     \n"
#endif
    "  blr                                             \n");
