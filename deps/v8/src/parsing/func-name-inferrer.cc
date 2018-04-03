// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/func-name-inferrer.h"

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

FuncNameInferrer::FuncNameInferrer(AstValueFactory* ast_value_factory,
                                   Zone* zone)
    : ast_value_factory_(ast_value_factory),
      entries_stack_(10, zone),
      names_stack_(5, zone),
      funcs_to_infer_(4, zone),
      zone_(zone) {
}


void FuncNameInferrer::PushEnclosingName(const AstRawString* name) {
  // Enclosing name is a name of a constructor function. To check
  // that it is really a constructor, we check that it is not empty
  // and starts with a capital letter.
  if (!name->IsEmpty() && unibrow::Uppercase::Is(name->FirstCharacter())) {
    names_stack_.Add(Name(name, kEnclosingConstructorName), zone());
  }
}


void FuncNameInferrer::PushLiteralName(const AstRawString* name) {
  if (IsOpen() && name != ast_value_factory_->prototype_string()) {
    names_stack_.Add(Name(name, kLiteralName), zone());
  }
}


void FuncNameInferrer::PushVariableName(const AstRawString* name) {
  if (IsOpen() && name != ast_value_factory_->dot_result_string()) {
    names_stack_.Add(Name(name, kVariableName), zone());
  }
}

void FuncNameInferrer::RemoveAsyncKeywordFromEnd() {
  if (IsOpen()) {
    CHECK_GT(names_stack_.length(), 0);
    CHECK(names_stack_.last().name->IsOneByteEqualTo("async"));
    names_stack_.RemoveLast();
  }
}

const AstConsString* FuncNameInferrer::MakeNameFromStack() {
  AstConsString* result = ast_value_factory_->NewConsString();
  for (int pos = 0; pos < names_stack_.length(); pos++) {
    // Skip consecutive variable declarations.
    if (pos + 1 < names_stack_.length() &&
        names_stack_.at(pos).type == kVariableName &&
        names_stack_.at(pos + 1).type == kVariableName) {
      continue;
    }
    // Add name. Separate names with ".".
    if (!result->IsEmpty()) {
      result->AddString(zone(), ast_value_factory_->dot_string());
    }
    result->AddString(zone(), names_stack_.at(pos).name);
  }
  return result;
}

void FuncNameInferrer::InferFunctionsNames() {
  const AstConsString* func_name = MakeNameFromStack();
  for (int i = 0; i < funcs_to_infer_.length(); ++i) {
    funcs_to_infer_[i]->set_raw_inferred_name(func_name);
  }
  funcs_to_infer_.Rewind(0);
}


}  // namespace internal
}  // namespace v8
