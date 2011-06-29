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

#ifndef V8_PLATFORM_TLS_WIN32_H_
#define V8_PLATFORM_TLS_WIN32_H_

#include "checks.h"
#include "globals.h"
#include "win32-headers.h"

namespace v8 {
namespace internal {

#if defined(_WIN32) && !defined(_WIN64)

#define V8_FAST_TLS_SUPPORTED 1

inline intptr_t InternalGetExistingThreadLocal(intptr_t index) {
  const intptr_t kTibInlineTlsOffset = 0xE10;
  const intptr_t kTibExtraTlsOffset = 0xF94;
  const intptr_t kMaxInlineSlots = 64;
  const intptr_t kMaxSlots = kMaxInlineSlots + 1024;
  ASSERT(0 <= index && index < kMaxSlots);
  if (index < kMaxInlineSlots) {
    return static_cast<intptr_t>(__readfsdword(kTibInlineTlsOffset +
                                               kPointerSize * index));
  }
  intptr_t extra = static_cast<intptr_t>(__readfsdword(kTibExtraTlsOffset));
  ASSERT(extra != 0);
  return *reinterpret_cast<intptr_t*>(extra +
                                      kPointerSize * (index - kMaxInlineSlots));
}

#endif

} }  // namespace v8::internal

#endif  // V8_PLATFORM_TLS_WIN32_H_
