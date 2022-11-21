// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <src/heap/base/stack.h>

// Save all callee-saved registers in the specified buffer.
// extern "C" void SaveCalleeSavedRegisters(intptr_t* buffer);

// See asm/x64/save_registers_asm.cc for why the function is not generated
// using clang.
//
// Do not depend on V8_TARGET_OS_* defines as some embedders may override the
// GN toolchain (e.g. ChromeOS) and not provide them.
//
// We maintain 8-byte alignment at calls by pushing an additional
// non-callee-saved register (r3).
//
// Calling convention source:
// https://en.wikipedia.org/wiki/Calling_convention#ARM_(A32)
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka4127.html

// 8 32-bit registers = 8 intprt_t
static_assert(heap::base::Stack::NumberOfCalleeSavedRegisters() == 8,
              "Mismatch in the number of callee-saved registers");
static_assert(sizeof(intptr_t) == 4, "Mismatch in word size");

asm(".globl SaveCalleeSavedRegisters             \n"
    ".type SaveCalleeSavedRegisters, %function   \n"
    ".hidden SaveCalleeSavedRegisters            \n"
    "SaveCalleeSavedRegisters:                   \n"
    // r0: [ intptr_t* buffer ]
    // Save the callee-saved registers: {r4-r11}.
    "  stm r0, {r4-r11}                          \n"
    // Return.
    "  bx lr                                     \n");
