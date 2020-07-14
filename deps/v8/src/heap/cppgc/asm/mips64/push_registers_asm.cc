// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Push all callee-saved registers to get them on the stack for conservative
// stack scanning.
//
// See asm/x64/push_registers_clang.cc for why the function is not generated
// using clang.
//
// Do not depend on V8_TARGET_OS_* defines as some embedders may override the
// GN toolchain (e.g. ChromeOS) and not provide them.
asm(".set noreorder                                      \n"
    ".global PushAllRegistersAndIterateStack             \n"
    ".type PushAllRegistersAndIterateStack, %function    \n"
    ".hidden PushAllRegistersAndIterateStack             \n"
    "PushAllRegistersAndIterateStack:                    \n"
    // Push all callee-saved registers and save return address.
    "  daddiu $sp, $sp, -96                              \n"
    "  sd $ra, 88($sp)                                   \n"
    "  sd $s8, 80($sp)                                   \n"
    "  sd $sp, 72($sp)                                   \n"
    "  sd $gp, 64($sp)                                   \n"
    "  sd $s7, 56($sp)                                   \n"
    "  sd $s6, 48($sp)                                   \n"
    "  sd $s5, 40($sp)                                   \n"
    "  sd $s4, 32($sp)                                   \n"
    "  sd $s3, 24($sp)                                   \n"
    "  sd $s2, 16($sp)                                   \n"
    "  sd $s1,  8($sp)                                   \n"
    "  sd $s0,  0($sp)                                   \n"
    // Maintain frame pointer.
    "  move $s8, $sp                                     \n"
    // Pass 1st parameter (a0) unchanged (Stack*).
    // Pass 2nd parameter (a1) unchanged (StackVisitor*).
    // Save 3rd parameter (a2; IterateStackCallback).
    "  move $a3, $a2                                     \n"
    // Call the callback.
    "  jalr $a3                                          \n"
    // Delay slot: Pass 3rd parameter as sp (stack pointer).
    "  move $a2, $sp                                     \n"
    // Load return address.
    "  ld $ra, 88($sp)                                   \n"
    // Restore frame pointer.
    "  ld $s8, 80($sp)                                   \n"
    "  jr $ra                                            \n"
    // Delay slot: Discard all callee-saved registers.
    "  daddiu $sp, $sp, 96                               \n");
