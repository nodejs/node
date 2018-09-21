// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_MACROS_H_
#define V8_SNAPSHOT_MACROS_H_

#include "include/v8config.h"

// .byte portability macros.

#if defined(V8_OS_MACOSX)  // MACOSX
#define V8_ASM_MANGLE_LABEL "_"
#define V8_ASM_RODATA_SECTION ".const_data\n"
#define V8_ASM_TEXT_SECTION ".text\n"
#define V8_ASM_DECLARE(NAME) ".private_extern " V8_ASM_MANGLE_LABEL NAME "\n"
#elif defined(V8_OS_AIX)  // AIX
#define V8_ASM_RODATA_SECTION ".csect[RO]\n"
#define V8_ASM_TEXT_SECTION ".csect .text[PR]\n"
#define V8_ASM_MANGLE_LABEL ""
#define V8_ASM_DECLARE(NAME) ".globl " V8_ASM_MANGLE_LABEL NAME "\n"
#elif defined(V8_OS_WIN)  // WIN
#if defined(V8_TARGET_ARCH_X64)
#define V8_ASM_MANGLE_LABEL ""
#else
#define V8_ASM_MANGLE_LABEL "_"
#endif
#define V8_ASM_RODATA_SECTION ".section .rodata\n"
#define V8_ASM_TEXT_SECTION ".section .text\n"
#define V8_ASM_DECLARE(NAME)
#else  // !MACOSX && !WIN && !AIX
#define V8_ASM_MANGLE_LABEL ""
#define V8_ASM_RODATA_SECTION ".section .rodata\n"
#define V8_ASM_TEXT_SECTION ".section .text\n"
#if defined(V8_TARGET_ARCH_MIPS) || defined(V8_TARGET_ARCH_MIPS64)
#define V8_ASM_DECLARE(NAME) ".global " V8_ASM_MANGLE_LABEL NAME "\n"
#else
#define V8_ASM_DECLARE(NAME) ".local " V8_ASM_MANGLE_LABEL NAME "\n"
#endif
#endif

// Align to kCodeAlignment.
#define V8_ASM_BALIGN32 ".balign 32\n"
#define V8_ASM_LABEL(NAME) V8_ASM_MANGLE_LABEL NAME ":\n"

// clang-format off
#if defined(V8_OS_AIX)

#define V8_EMBEDDED_TEXT_HEADER(LABEL)         \
  __asm__(V8_ASM_DECLARE(#LABEL)               \
          ".csect " #LABEL "[DS]\n"            \
          #LABEL ":\n"                         \
          ".llong ." #LABEL ", TOC[tc0], 0\n"  \
          V8_ASM_TEXT_SECTION                  \
          "." #LABEL ":\n");

#define V8_EMBEDDED_RODATA_HEADER(LABEL)    \
  __asm__(V8_ASM_RODATA_SECTION             \
          V8_ASM_DECLARE(#LABEL)            \
          ".align 5\n"                      \
          V8_ASM_LABEL(#LABEL));

#else

#define V8_EMBEDDED_TEXT_HEADER(LABEL) \
  __asm__(V8_ASM_TEXT_SECTION          \
          V8_ASM_DECLARE(#LABEL)       \
          V8_ASM_BALIGN32              \
          V8_ASM_LABEL(#LABEL));

#define V8_EMBEDDED_RODATA_HEADER(LABEL) \
  __asm__(V8_ASM_RODATA_SECTION          \
          V8_ASM_DECLARE(#LABEL)         \
          V8_ASM_BALIGN32                \
          V8_ASM_LABEL(#LABEL));

#endif  // #if defined(V8_OS_AIX)
#endif  // V8_SNAPSHOT_MACROS_H_
