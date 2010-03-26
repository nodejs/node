// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#ifndef V8_OPROFILE_AGENT_H_
#define V8_OPROFILE_AGENT_H_

#include <stdlib.h>

#include "globals.h"

#ifdef ENABLE_OPROFILE_AGENT
// opagent.h uses uint64_t type, which can be missing in
// system headers (they have __uint64_t), but is defined
// in V8's headers.
#include <opagent.h>  // NOLINT

#define OPROFILE(Call)                             \
  do {                                             \
    if (v8::internal::OProfileAgent::is_enabled()) \
      v8::internal::OProfileAgent::Call;           \
  } while (false)
#else
#define OPROFILE(Call) ((void) 0)
#endif

namespace v8 {
namespace internal {

class OProfileAgent {
 public:
  static bool Initialize();
  static void TearDown();
#ifdef ENABLE_OPROFILE_AGENT
  static void CreateNativeCodeRegion(const char* name,
                                     const void* ptr, unsigned int size);
  static void CreateNativeCodeRegion(String* name,
                                     const void* ptr, unsigned int size);
  static void CreateNativeCodeRegion(String* name, String* source, int line_num,
                                     const void* ptr, unsigned int size);
  static bool is_enabled() { return handle_ != NULL; }

 private:
  static op_agent_t handle_;

  // Size of the buffer that is used for composing code areas names.
  static const int kFormattingBufSize = 256;
#else
  static bool is_enabled() { return false; }
#endif
};
} }

#endif  // V8_OPROFILE_AGENT_H_
