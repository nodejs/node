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
    "  addiu $sp, $sp, -48                               \n"
    "  sw $ra, 44($sp)                                   \n"
    "  sw $s8, 40($sp)                                   \n"
    "  sw $sp, 36($sp)                                   \n"
    "  sw $gp, 32($sp)                                   \n"
    "  sw $s7, 28($sp)                                   \n"
    "  sw $s6, 24($sp)                                   \n"
    "  sw $s5, 20($sp)                                   \n"
    "  sw $s4, 16($sp)                                   \n"
    "  sw $s3, 12($sp)                                   \n"
    "  sw $s2,  8($sp)                                   \n"
    "  sw $s1,  4($sp)                                   \n"
    "  sw $s0,  0($sp)                                   \n"
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
    "  lw $ra, 44($sp)                                   \n"
    // Restore frame pointer.
    "  lw $s8, 40($sp)                                   \n"
    "  jr $ra                                            \n"
    // Delay slot: Discard all callee-saved registers.
    "  addiu $sp, $sp, 48                                \n");
