// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/oddball.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.3 Boolean Objects

// ES6 #sec-boolean.prototype.tostring
TF_BUILTIN(BooleanPrototypeToString, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* value = ToThisValue(context, receiver, PrimitiveType::kBoolean,
                            "Boolean.prototype.toString");
  Node* result = LoadObjectField(value, Oddball::kToStringOffset);
  Return(result);
}

// ES6 #sec-boolean.prototype.valueof
TF_BUILTIN(BooleanPrototypeValueOf, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* result = ToThisValue(context, receiver, PrimitiveType::kBoolean,
                             "Boolean.prototype.valueOf");
  Return(result);
}

}  // namespace internal
}  // namespace v8
