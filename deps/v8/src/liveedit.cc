// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "liveedit.h"
#include "compiler.h"
#include "oprofile-agent.h"
#include "scopes.h"
#include "global-handles.h"
#include "debug.h"

namespace v8 {
namespace internal {


class FunctionInfoListener {
 public:
  void FunctionStarted(FunctionLiteral* fun) {
    // Implementation follows.
  }

  void FunctionDone() {
    // Implementation follows.
  }

  void FunctionScope(Scope* scope) {
    // Implementation follows.
  }

  void FunctionCode(Handle<Code> function_code) {
    // Implementation follows.
  }
};

static FunctionInfoListener* active_function_info_listener = NULL;

LiveEditFunctionTracker::LiveEditFunctionTracker(FunctionLiteral* fun) {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionStarted(fun);
  }
}
LiveEditFunctionTracker::~LiveEditFunctionTracker() {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionDone();
  }
}
void LiveEditFunctionTracker::RecordFunctionCode(Handle<Code> code) {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionCode(code);
  }
}
void LiveEditFunctionTracker::RecordFunctionScope(Scope* scope) {
  if (active_function_info_listener != NULL) {
    active_function_info_listener->FunctionScope(scope);
  }
}
bool LiveEditFunctionTracker::IsActive() {
  return active_function_info_listener != NULL;
}

} }  // namespace v8::internal
