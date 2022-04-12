// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8config.h"
#include "src/trap-handler/trap-handler-simulator.h"

#if V8_OS_DARWIN
#define SYMBOL(name) "_" #name
#else  // !V8_OS_DARWIN
#define SYMBOL(name) #name
#endif  // !V8_OS_DARWIN

// Define the ProbeMemory function declared in trap-handler-simulators.h.
asm(
    ".globl " SYMBOL(ProbeMemory) "                 \n"
    SYMBOL(ProbeMemory) ":                          \n"
// First parameter (address) passed in %rdi on Linux/Mac, and %rcx on Windows.
// The second parameter (pc) is unused here. It is read by the trap handler
// instead.
#if V8_OS_WIN
    "  movb (%rcx), %al                             \n"
#else
    "  movb (%rdi), %al                             \n"
#endif  // V8_OS_WIN
    // Return 0 on success.
    "  xorl %eax, %eax                              \n"
    // Place an additional "ret" here instead of falling through to the one
    // below, because (some) toolchain(s) on Mac set ".subsections_via_symbols",
    // which can cause the "ret" below to be placed elsewhere. An alternative
    // prevention would be to add ".alt_entry" (see
    // https://reviews.llvm.org/D79926), but just adding a "ret" is simpler.
    "  ret                                          \n"
    ".globl " SYMBOL(v8_probe_memory_continuation) "\n"
    SYMBOL(v8_probe_memory_continuation) ":         \n"
    // If the trap handler continues here, it wrote the landing pad in %rax.
    "  ret                                          \n");
