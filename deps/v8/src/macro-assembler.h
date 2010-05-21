// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_MACRO_ASSEMBLER_H_
#define V8_MACRO_ASSEMBLER_H_


// Helper types to make boolean flag easier to read at call-site.
enum InvokeFlag {
  CALL_FUNCTION,
  JUMP_FUNCTION
};


enum CodeLocation {
  IN_JAVASCRIPT,
  IN_JS_ENTRY,
  IN_C_ENTRY
};


enum HandlerType {
  TRY_CATCH_HANDLER,
  TRY_FINALLY_HANDLER,
  JS_ENTRY_HANDLER
};


// Invalid depth in prototype chain.
const int kInvalidProtoDepth = -1;

#if V8_TARGET_ARCH_IA32
#include "assembler.h"
#include "ia32/assembler-ia32.h"
#include "ia32/assembler-ia32-inl.h"
#include "code.h"  // must be after assembler_*.h
#include "ia32/macro-assembler-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "assembler.h"
#include "x64/assembler-x64.h"
#include "x64/assembler-x64-inl.h"
#include "code.h"  // must be after assembler_*.h
#include "x64/macro-assembler-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/constants-arm.h"
#include "assembler.h"
#include "arm/assembler-arm.h"
#include "arm/assembler-arm-inl.h"
#include "code.h"  // must be after assembler_*.h
#include "arm/macro-assembler-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/constants-mips.h"
#include "assembler.h"
#include "mips/assembler-mips.h"
#include "mips/assembler-mips-inl.h"
#include "code.h"  // must be after assembler_*.h
#include "mips/macro-assembler-mips.h"
#else
#error Unsupported target architecture.
#endif

#endif  // V8_MACRO_ASSEMBLER_H_
