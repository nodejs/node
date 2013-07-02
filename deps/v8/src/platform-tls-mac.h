// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_PLATFORM_TLS_MAC_H_
#define V8_PLATFORM_TLS_MAC_H_

#include "globals.h"

namespace v8 {
namespace internal {

#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64

#define V8_FAST_TLS_SUPPORTED 1

extern intptr_t kMacTlsBaseOffset;

INLINE(intptr_t InternalGetExistingThreadLocal(intptr_t index));

inline intptr_t InternalGetExistingThreadLocal(intptr_t index) {
  intptr_t result;
#if V8_HOST_ARCH_IA32
  asm("movl %%gs:(%1,%2,4), %0;"
      :"=r"(result)  // Output must be a writable register.
      :"r"(kMacTlsBaseOffset), "r"(index));
#else
  asm("movq %%gs:(%1,%2,8), %0;"
      :"=r"(result)
      :"r"(kMacTlsBaseOffset), "r"(index));
#endif
  return result;
}

#endif

} }  // namespace v8::internal

#endif  // V8_PLATFORM_TLS_MAC_H_
