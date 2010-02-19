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


#ifndef V8_CODEGEN_INL_H_
#define V8_CODEGEN_INL_H_

#include "codegen.h"
#include "compiler.h"
#include "register-allocator-inl.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/codegen-ia32-inl.h"
#elif V8_TARGET_ARCH_X64
#include "x64/codegen-x64-inl.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/codegen-arm-inl.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/codegen-mips-inl.h"
#else
#error Unsupported target architecture.
#endif


namespace v8 {
namespace internal {

Handle<Script> CodeGenerator::script() { return info_->script(); }
bool CodeGenerator::is_eval() { return info_->is_eval(); }

} }  // namespace v8::internal

#endif  // V8_CODEGEN_INL_H_
