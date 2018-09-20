// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 #sec-symbol-objects
// ES ##sec-symbol.prototype.description
TF_BUILTIN(SymbolPrototypeDescriptionGetter, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* value = ToThisValue(context, receiver, PrimitiveType::kSymbol,
                            "Symbol.prototype.description");
  Node* result = LoadObjectField(value, Symbol::kNameOffset);
  Return(result);
}

// ES6 #sec-symbol.prototype-@@toprimitive
TF_BUILTIN(SymbolPrototypeToPrimitive, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* result = ToThisValue(context, receiver, PrimitiveType::kSymbol,
                             "Symbol.prototype [ @@toPrimitive ]");
  Return(result);
}

// ES6 #sec-symbol.prototype.tostring
TF_BUILTIN(SymbolPrototypeToString, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* value = ToThisValue(context, receiver, PrimitiveType::kSymbol,
                            "Symbol.prototype.toString");
  Node* result = CallRuntime(Runtime::kSymbolDescriptiveString, context, value);
  Return(result);
}

// ES6 #sec-symbol.prototype.valueof
TF_BUILTIN(SymbolPrototypeValueOf, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  Node* result = ToThisValue(context, receiver, PrimitiveType::kSymbol,
                             "Symbol.prototype.valueOf");
  Return(result);
}

}  // namespace internal
}  // namespace v8
