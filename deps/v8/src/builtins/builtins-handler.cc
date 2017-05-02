// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/keyed-store-generic.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

TF_BUILTIN(KeyedLoadIC_IndexedString, CodeStubAssembler) {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* index = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  Node* index_intptr = TryToIntptr(index, &miss);
  Node* length = SmiUntag(LoadStringLength(receiver));
  GotoIf(UintPtrGreaterThanOrEqual(index_intptr, length), &miss);

  Node* code = StringCharCodeAt(receiver, index_intptr, INTPTR_PARAMETERS);
  Node* result = StringFromCharCode(code);
  Return(result);

  Bind(&miss);
  TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, index, slot,
                  vector);
}

TF_BUILTIN(KeyedLoadIC_Miss, CodeStubAssembler) {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, name, slot,
                  vector);
}

TF_BUILTIN(KeyedLoadIC_Slow, CodeStubAssembler) {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kKeyedGetProperty, context, receiver, name);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state, SLOPPY);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic_Strict(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state, STRICT);
}

TF_BUILTIN(KeyedStoreIC_Miss, CodeStubAssembler) {
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value, slot, vector,
                  receiver, name);
}

TF_BUILTIN(KeyedStoreIC_Slow, CodeStubAssembler) {
  typedef StoreWithVectorDescriptor Descriptor;

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

TF_BUILTIN(LoadGlobalIC_Miss, CodeStubAssembler) {
  typedef LoadGlobalWithVectorDescriptor Descriptor;

  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadGlobalIC_Miss, context, name, slot, vector);
}

TF_BUILTIN(LoadGlobalIC_Slow, CodeStubAssembler) {
  typedef LoadGlobalWithVectorDescriptor Descriptor;

  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadGlobalIC_Slow, context, name, slot, vector);
}

void Builtins::Generate_LoadIC_Getter_ForDeopt(MacroAssembler* masm) {
  NamedLoadHandlerCompiler::GenerateLoadViaGetterForDeopt(masm);
}

TF_BUILTIN(LoadIC_FunctionPrototype, CodeStubAssembler) {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  Node* proto_or_map =
      LoadObjectField(receiver, JSFunction::kPrototypeOrInitialMapOffset);
  GotoIf(IsTheHole(proto_or_map), &miss);

  Variable var_result(this, MachineRepresentation::kTagged, proto_or_map);
  Label done(this, &var_result);
  GotoIfNot(IsMap(proto_or_map), &done);

  var_result.Bind(LoadMapPrototype(proto_or_map));
  Goto(&done);

  Bind(&done);
  Return(var_result.value());

  Bind(&miss);
  TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name, slot, vector);
}

TF_BUILTIN(LoadIC_Miss, CodeStubAssembler) {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name, slot, vector);
}

TF_BUILTIN(LoadIC_Normal, CodeStubAssembler) {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  Label slow(this);
  {
    Node* properties = LoadProperties(receiver);
    Variable var_name_index(this, MachineType::PointerRepresentation());
    Label found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, name, &found,
                                         &var_name_index, &slow);
    Bind(&found);
    {
      Variable var_details(this, MachineRepresentation::kWord32);
      Variable var_value(this, MachineRepresentation::kTagged);
      LoadPropertyFromNameDictionary(properties, var_name_index.value(),
                                     &var_details, &var_value);
      Node* value = CallGetterIfAccessor(var_value.value(), var_details.value(),
                                         context, receiver, &slow);
      Return(value);
    }
  }

  Bind(&slow);
  TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

TF_BUILTIN(LoadIC_Slow, CodeStubAssembler) {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

TF_BUILTIN(StoreIC_Miss, CodeStubAssembler) {
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot, vector,
                  receiver, name);
}

TF_BUILTIN(StoreIC_Normal, CodeStubAssembler) {
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label slow(this);
  {
    Node* properties = LoadProperties(receiver);
    Variable var_name_index(this, MachineType::PointerRepresentation());
    Label found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, name, &found,
                                         &var_name_index, &slow);
    Bind(&found);
    {
      Node* details = LoadDetailsByKeyIndex<NameDictionary>(
          properties, var_name_index.value());
      // Check that the property is a writable data property (no accessor).
      const int kTypeAndReadOnlyMask = PropertyDetails::KindField::kMask |
                                       PropertyDetails::kAttributesReadOnlyMask;
      STATIC_ASSERT(kData == 0);
      GotoIf(IsSetWord32(details, kTypeAndReadOnlyMask), &slow);
      StoreValueByKeyIndex<NameDictionary>(properties, var_name_index.value(),
                                           value);
      Return(value);
    }
  }

  Bind(&slow);
  TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot, vector,
                  receiver, name);
}

void Builtins::Generate_StoreIC_Setter_ForDeopt(MacroAssembler* masm) {
  NamedStoreHandlerCompiler::GenerateStoreViaSetterForDeopt(masm);
}

}  // namespace internal
}  // namespace v8
