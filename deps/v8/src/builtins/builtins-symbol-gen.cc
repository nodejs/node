// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES #sec-symbol-objects
// ES #sec-symbol.prototype.description
TF_BUILTIN(SymbolPrototypeDescriptionGetter, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  TNode<Symbol> value =
      CAST(ToThisValue(context, receiver, PrimitiveType::kSymbol,
                       "Symbol.prototype.description"));
  TNode<Object> result = LoadObjectField(value, Symbol::kNameOffset);
  Return(result);
}

// ES6 #sec-symbol.prototype-@@toprimitive
TF_BUILTIN(SymbolPrototypeToPrimitive, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  TNode<Object> result = ToThisValue(context, receiver, PrimitiveType::kSymbol,
                                     "Symbol.prototype [ @@toPrimitive ]");
  Return(result);
}

// ES6 #sec-symbol.prototype.tostring
TF_BUILTIN(SymbolPrototypeToString, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  TNode<Object> value = ToThisValue(context, receiver, PrimitiveType::kSymbol,
                                    "Symbol.prototype.toString");
  TNode<Object> result =
      CallRuntime(Runtime::kSymbolDescriptiveString, context, value);
  Return(result);
}

// ES6 #sec-symbol.prototype.valueof
TF_BUILTIN(SymbolPrototypeValueOf, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  TNode<Object> result = ToThisValue(context, receiver, PrimitiveType::kSymbol,
                                     "Symbol.prototype.valueOf");
  Return(result);
}

}  // namespace internal
}  // namespace v8
