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

#include "v8.h"

#include "ast.h"
#include "func-name-inferrer.h"
#include "list-inl.h"

namespace v8 {
namespace internal {

FuncNameInferrer::FuncNameInferrer(Isolate* isolate, Zone* zone)
    : isolate_(isolate),
      entries_stack_(10, zone),
      names_stack_(5, zone),
      funcs_to_infer_(4, zone),
      zone_(zone) {
}


void FuncNameInferrer::PushEnclosingName(Handle<String> name) {
  // Enclosing name is a name of a constructor function. To check
  // that it is really a constructor, we check that it is not empty
  // and starts with a capital letter.
  if (name->length() > 0 && Runtime::IsUpperCaseChar(
          isolate()->runtime_state(), name->Get(0))) {
    names_stack_.Add(Name(name, kEnclosingConstructorName), zone());
  }
}


void FuncNameInferrer::PushLiteralName(Handle<String> name) {
  if (IsOpen() && !isolate()->heap()->prototype_string()->Equals(*name)) {
    names_stack_.Add(Name(name, kLiteralName), zone());
  }
}


void FuncNameInferrer::PushVariableName(Handle<String> name) {
  if (IsOpen() && !isolate()->heap()->dot_result_string()->Equals(*name)) {
    names_stack_.Add(Name(name, kVariableName), zone());
  }
}


Handle<String> FuncNameInferrer::MakeNameFromStack() {
  return MakeNameFromStackHelper(0, isolate()->factory()->empty_string());
}


Handle<String> FuncNameInferrer::MakeNameFromStackHelper(int pos,
                                                         Handle<String> prev) {
  if (pos >= names_stack_.length()) return prev;
  if (pos < names_stack_.length() - 1 &&
      names_stack_.at(pos).type == kVariableName &&
      names_stack_.at(pos + 1).type == kVariableName) {
    // Skip consecutive variable declarations.
    return MakeNameFromStackHelper(pos + 1, prev);
  } else {
    if (prev->length() > 0) {
      Factory* factory = isolate()->factory();
      Handle<String> curr = factory->NewConsString(
          factory->dot_string(), names_stack_.at(pos).name);
      return MakeNameFromStackHelper(pos + 1,
                                     factory->NewConsString(prev, curr));
    } else {
      return MakeNameFromStackHelper(pos + 1, names_stack_.at(pos).name);
    }
  }
}


void FuncNameInferrer::InferFunctionsNames() {
  Handle<String> func_name = MakeNameFromStack();
  for (int i = 0; i < funcs_to_infer_.length(); ++i) {
    funcs_to_infer_[i]->set_inferred_name(func_name);
  }
  funcs_to_infer_.Rewind(0);
}


} }  // namespace v8::internal
