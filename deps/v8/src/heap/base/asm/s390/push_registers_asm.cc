// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Push all callee-saved registers to get them on the stack for conservative
// stack scanning.

// See asm/x64/push_registers_clang.cc for why the function is not generated
// using clang.

// Do not depend on V8_TARGET_OS_* defines as some embedders may override the
// GN toolchain (e.g. ChromeOS) and not provide them.

// S390 ABI source:
// http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_zSeries.html
asm(".globl PushAllRegistersAndIterateStack             \n"
    ".type PushAllRegistersAndIterateStack, %function   \n"
    ".hidden PushAllRegistersAndIterateStack            \n"
    "PushAllRegistersAndIterateStack:                   \n"
    // Push all callee-saved registers.
    // r6-r13, r14 and sp(r15)
    "  stmg %r6, %sp, 48(%sp)                           \n"
    // Allocate frame.
    "  lay %sp, -160(%sp)                               \n"
    // Pass 1st parameter (r2) unchanged (Stack*).
    // Pass 2nd parameter (r3) unchanged (StackVisitor*).
    // Save 3rd parameter (r4; IterateStackCallback).
    "  lgr %r5, %r4                                     \n"
    // Pass sp as 3rd parameter. 160+48 to point
    // to callee saved region stored above.
    "  lay %r4, 208(%sp)                                \n"
    // Call the callback.
    "  basr %r14, %r5                                   \n"
    "  lmg %r14,%sp, 272(%sp)                           \n"
    "  br %r14                                          \n");
