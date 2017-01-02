// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 19.3 Boolean Objects

// ES6 section 19.3.1.1 Boolean ( value ) for the [[Call]] case.
BUILTIN(BooleanConstructor) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  return isolate->heap()->ToBoolean(value->BooleanValue());
}

// ES6 section 19.3.1.1 Boolean ( value ) for the [[Construct]] case.
BUILTIN(BooleanConstructor_ConstructStub) {
  HandleScope scope(isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  Handle<JSFunction> target = args.target<JSFunction>();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  DCHECK(*target == target->native_context()->boolean_function());
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::New(target, new_target));
  Handle<JSValue>::cast(result)->set_value(
      isolate->heap()->ToBoolean(value->BooleanValue()));
  return *result;
}

// ES6 section 19.3.3.2 Boolean.prototype.toString ( )
void Builtins::Generate_BooleanPrototypeToString(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Node* value = assembler->ToThisValue(
      context, receiver, PrimitiveType::kBoolean, "Boolean.prototype.toString");
  Node* result = assembler->LoadObjectField(value, Oddball::kToStringOffset);
  assembler->Return(result);
}

// ES6 section 19.3.3.3 Boolean.prototype.valueOf ( )
void Builtins::Generate_BooleanPrototypeValueOf(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Node* result = assembler->ToThisValue(
      context, receiver, PrimitiveType::kBoolean, "Boolean.prototype.valueOf");
  assembler->Return(result);
}

}  // namespace internal
}  // namespace v8
