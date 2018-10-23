// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/ic/ic.h"
#include "src/ic/keyed-store-generic.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

TF_BUILTIN(LoadIC_StringLength, CodeStubAssembler) {
  Node* string = Parameter(Descriptor::kReceiver);
  Return(LoadStringLengthAsSmi(string));
}

TF_BUILTIN(LoadIC_StringWrapperLength, CodeStubAssembler) {
  Node* value = Parameter(Descriptor::kReceiver);
  Node* string = LoadJSValueValue(value);
  Return(LoadStringLengthAsSmi(string));
}

TF_BUILTIN(KeyedLoadIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state);
}

void Builtins::Generate_StoreIC_Uninitialized(
    compiler::CodeAssemblerState* state) {
  StoreICUninitializedGenerator::Generate(state);
}

TF_BUILTIN(KeyedStoreIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  TailCallRuntime(Runtime::kKeyedStoreIC_Slow, context, value, slot, vector,
                  receiver, name);
}

TF_BUILTIN(StoreInArrayLiteralIC_Slow, CodeStubAssembler) {
  Node* array = Parameter(Descriptor::kReceiver);
  Node* index = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);
  TailCallRuntime(Runtime::kStoreInArrayLiteralIC_Slow, context, value, array,
                  index);
}

TF_BUILTIN(LoadGlobalIC_Slow, CodeStubAssembler) {
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadGlobalIC_Slow, context, name, slot, vector);
}

TF_BUILTIN(LoadIC_FunctionPrototype, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this, Label::kDeferred);
  Return(LoadJSFunctionPrototype(receiver, &miss));

  BIND(&miss);
  TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name, slot, vector);
}

TF_BUILTIN(LoadIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

TF_BUILTIN(StoreGlobalIC_Slow, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  TailCallRuntime(Runtime::kStoreGlobalIC_Slow, context, value, slot, vector,
                  receiver, name);
}

}  // namespace internal
}  // namespace v8
