// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/ic/accessor-assembler.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/keyed-store-generic.h"

namespace v8 {
namespace internal {

void Builtins::Generate_KeyedLoadIC_Megamorphic_TF(
    compiler::CodeAssemblerState* state) {
  AccessorAssembler::GenerateKeyedLoadICMegamorphic(state);
}

void Builtins::Generate_KeyedLoadIC_Miss(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef LoadWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* receiver = assembler.Parameter(Descriptor::kReceiver);
  Node* name = assembler.Parameter(Descriptor::kName);
  Node* slot = assembler.Parameter(Descriptor::kSlot);
  Node* vector = assembler.Parameter(Descriptor::kVector);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, name,
                            slot, vector);
}

void Builtins::Generate_KeyedLoadIC_Slow(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef LoadWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* receiver = assembler.Parameter(Descriptor::kReceiver);
  Node* name = assembler.Parameter(Descriptor::kName);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.TailCallRuntime(Runtime::kKeyedGetProperty, context, receiver,
                            name);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic_TF(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state, SLOPPY);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic_Strict_TF(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericGenerator::Generate(state, STRICT);
}

void Builtins::Generate_KeyedStoreIC_Miss(MacroAssembler* masm) {
  KeyedStoreIC::GenerateMiss(masm);
}

void Builtins::Generate_KeyedStoreIC_Slow(MacroAssembler* masm) {
  KeyedStoreIC::GenerateSlow(masm);
}

void Builtins::Generate_LoadGlobalIC_Miss(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef LoadGlobalWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* name = assembler.Parameter(Descriptor::kName);
  Node* slot = assembler.Parameter(Descriptor::kSlot);
  Node* vector = assembler.Parameter(Descriptor::kVector);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.TailCallRuntime(Runtime::kLoadGlobalIC_Miss, context, name, slot,
                            vector);
}

void Builtins::Generate_LoadGlobalIC_Slow(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef LoadGlobalWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* name = assembler.Parameter(Descriptor::kName);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.TailCallRuntime(Runtime::kLoadGlobalIC_Slow, context, name);
}

void Builtins::Generate_LoadIC_Getter_ForDeopt(MacroAssembler* masm) {
  NamedLoadHandlerCompiler::GenerateLoadViaGetterForDeopt(masm);
}

void Builtins::Generate_LoadIC_Miss(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef LoadWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* receiver = assembler.Parameter(Descriptor::kReceiver);
  Node* name = assembler.Parameter(Descriptor::kName);
  Node* slot = assembler.Parameter(Descriptor::kSlot);
  Node* vector = assembler.Parameter(Descriptor::kVector);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name,
                            slot, vector);
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

void Builtins::Generate_LoadIC_Slow(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef LoadWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* receiver = assembler.Parameter(Descriptor::kReceiver);
  Node* name = assembler.Parameter(Descriptor::kName);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

void Builtins::Generate_StoreIC_Miss(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef StoreWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* receiver = assembler.Parameter(Descriptor::kReceiver);
  Node* name = assembler.Parameter(Descriptor::kName);
  Node* value = assembler.Parameter(Descriptor::kValue);
  Node* slot = assembler.Parameter(Descriptor::kSlot);
  Node* vector = assembler.Parameter(Descriptor::kVector);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot,
                            vector, receiver, name);
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
      const int kNameToDetailsOffset = (NameDictionary::kEntryDetailsIndex -
                                        NameDictionary::kEntryKeyIndex) *
                                       kPointerSize;
      Node* details = LoadFixedArrayElement(properties, var_name_index.value(),
                                            kNameToDetailsOffset);
      // Check that the property is a writable data property (no accessor).
      const int kTypeAndReadOnlyMask = PropertyDetails::KindField::kMask |
                                       PropertyDetails::kAttributesReadOnlyMask;
      STATIC_ASSERT(kData == 0);
      GotoIf(IsSetSmi(details, kTypeAndReadOnlyMask), &slow);
      const int kNameToValueOffset =
          (NameDictionary::kEntryValueIndex - NameDictionary::kEntryKeyIndex) *
          kPointerSize;
      StoreFixedArrayElement(properties, var_name_index.value(), value,
                             UPDATE_WRITE_BARRIER, kNameToValueOffset);
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

namespace {
void Generate_StoreIC_Slow(compiler::CodeAssemblerState* state,
                           LanguageMode language_mode) {
  typedef compiler::Node Node;
  typedef StoreWithVectorDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* receiver = assembler.Parameter(Descriptor::kReceiver);
  Node* name = assembler.Parameter(Descriptor::kName);
  Node* value = assembler.Parameter(Descriptor::kValue);
  Node* context = assembler.Parameter(Descriptor::kContext);
  Node* lang_mode = assembler.SmiConstant(Smi::FromInt(language_mode));

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  assembler.TailCallRuntime(Runtime::kSetProperty, context, receiver, name,
                            value, lang_mode);
}
}  // anonymous namespace

void Builtins::Generate_StoreIC_SlowSloppy(
    compiler::CodeAssemblerState* state) {
  Generate_StoreIC_Slow(state, SLOPPY);
}

void Builtins::Generate_StoreIC_SlowStrict(
    compiler::CodeAssemblerState* state) {
  Generate_StoreIC_Slow(state, STRICT);
}

}  // namespace internal
}  // namespace v8
