// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.4 Symbol Objects

// ES6 section 19.4.1.1 Symbol ( [ description ] ) for the [[Call]] case.
BUILTIN(SymbolConstructor) {
  HandleScope scope(isolate);
  Handle<Symbol> result = isolate->factory()->NewSymbol();
  Handle<Object> description = args.atOrUndefined(isolate, 1);
  if (!description->IsUndefined(isolate)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, description,
                                       Object::ToString(isolate, description));
    result->set_name(*description);
  }
  return *result;
}

// ES6 section 19.4.1.1 Symbol ( [ description ] ) for the [[Construct]] case.
BUILTIN(SymbolConstructor_ConstructStub) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotConstructor,
                            isolate->factory()->Symbol_string()));
}

// ES6 section 19.4.3.4 Symbol.prototype [ @@toPrimitive ] ( hint )
void Builtins::Generate_SymbolPrototypeToPrimitive(
    CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(4);

  Node* result =
      assembler->ToThisValue(context, receiver, PrimitiveType::kSymbol,
                             "Symbol.prototype [ @@toPrimitive ]");
  assembler->Return(result);
}

// ES6 section 19.4.3.2 Symbol.prototype.toString ( )
void Builtins::Generate_SymbolPrototypeToString(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Node* value = assembler->ToThisValue(
      context, receiver, PrimitiveType::kSymbol, "Symbol.prototype.toString");
  Node* result =
      assembler->CallRuntime(Runtime::kSymbolDescriptiveString, context, value);
  assembler->Return(result);
}

// ES6 section 19.4.3.3 Symbol.prototype.valueOf ( )
void Builtins::Generate_SymbolPrototypeValueOf(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Node* result = assembler->ToThisValue(
      context, receiver, PrimitiveType::kSymbol, "Symbol.prototype.valueOf");
  assembler->Return(result);
}

}  // namespace internal
}  // namespace v8
