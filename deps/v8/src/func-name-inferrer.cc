// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  if (IsOpen() &&
      !String::Equals(isolate()->factory()->prototype_string(), name)) {
    names_stack_.Add(Name(name, kLiteralName), zone());
  }
}


void FuncNameInferrer::PushVariableName(Handle<String> name) {
  if (IsOpen() &&
      !String::Equals(isolate()->factory()->dot_result_string(), name)) {
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
      Handle<String> name = names_stack_.at(pos).name;
      if (prev->length() + name->length() + 1 > String::kMaxLength) return prev;
      Factory* factory = isolate()->factory();
      Handle<String> curr =
          factory->NewConsString(factory->dot_string(), name).ToHandleChecked();
      curr = factory->NewConsString(prev, curr).ToHandleChecked();
      return MakeNameFromStackHelper(pos + 1, curr);
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
