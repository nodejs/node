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

#include "v8.h"

#include "oprofile-agent.h"

namespace v8 {
namespace internal {


bool OProfileAgent::Initialize() {
#ifdef ENABLE_OPROFILE_AGENT
  if (FLAG_oprofile) {
    if (handle_ != NULL) return false;

    // Disable code moving by GC.
    FLAG_always_compact = false;
    FLAG_never_compact = true;

    handle_ = op_open_agent();
    return (handle_ != NULL);
  } else {
    return true;
  }
#else
  if (FLAG_oprofile) {
    OS::Print("Warning: --oprofile specified but binary compiled without "
              "oprofile support.\n");
  }
  return true;
#endif
}


void OProfileAgent::TearDown() {
#ifdef ENABLE_OPROFILE_AGENT
  if (handle_ != NULL) {
    op_close_agent(handle_);
  }
#endif
}


#ifdef ENABLE_OPROFILE_AGENT
op_agent_t OProfileAgent::handle_ = NULL;


void OProfileAgent::CreateNativeCodeRegion(const char* name,
    const void* ptr, unsigned int size) {
  op_write_native_code(handle_, name, (uint64_t)ptr, ptr, size);
}


void OProfileAgent::CreateNativeCodeRegion(String* name,
    const void* ptr, unsigned int size) {
  const char* func_name;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  func_name = name->length() > 0 ? *str : "<anonymous>";
  CreateNativeCodeRegion(func_name, ptr, size);
}


void OProfileAgent::CreateNativeCodeRegion(String* name, String* source,
    int line_num, const void* ptr, unsigned int size) {
  Vector<char> buf = Vector<char>::New(OProfileAgent::kFormattingBufSize);
  const char* func_name;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  func_name = name->length() > 0 ? *str : "<anonymous>";
  SmartPointer<char> source_str =
      source->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  if (v8::internal::OS::SNPrintF(buf, "%s %s:%d",
                                 func_name, *source_str, line_num) != -1) {
    CreateNativeCodeRegion(buf.start(), ptr, size);
  } else {
    CreateNativeCodeRegion("<script/func name too long>", ptr, size);
  }
}

#endif  // ENABLE_OPROFILE_AGENT

} }  // namespace v8::internal
