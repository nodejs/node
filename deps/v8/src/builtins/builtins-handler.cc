// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"

namespace v8 {
namespace internal {

void Builtins::Generate_KeyedLoadIC_Megamorphic(MacroAssembler* masm) {
  KeyedLoadIC::GenerateMegamorphic(masm);
}

void Builtins::Generate_KeyedLoadIC_Megamorphic_TF(
    CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  CodeStubAssembler::LoadICParameters p(context, receiver, name, slot, vector);
  assembler->KeyedLoadICGeneric(&p);
}

void Builtins::Generate_KeyedLoadIC_Miss(MacroAssembler* masm) {
  KeyedLoadIC::GenerateMiss(masm);
}
void Builtins::Generate_KeyedLoadIC_Slow(MacroAssembler* masm) {
  KeyedLoadIC::GenerateRuntimeGetProperty(masm);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic(MacroAssembler* masm) {
  KeyedStoreIC::GenerateMegamorphic(masm, SLOPPY);
}

void Builtins::Generate_KeyedStoreIC_Megamorphic_Strict(MacroAssembler* masm) {
  KeyedStoreIC::GenerateMegamorphic(masm, STRICT);
}

void Builtins::Generate_KeyedStoreIC_Miss(MacroAssembler* masm) {
  KeyedStoreIC::GenerateMiss(masm);
}

void Builtins::Generate_KeyedStoreIC_Slow(MacroAssembler* masm) {
  KeyedStoreIC::GenerateSlow(masm);
}

void Builtins::Generate_LoadGlobalIC_Miss(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef LoadGlobalWithVectorDescriptor Descriptor;

  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->TailCallRuntime(Runtime::kLoadGlobalIC_Miss, context, slot,
                             vector);
}

void Builtins::Generate_LoadGlobalIC_Slow(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef LoadGlobalWithVectorDescriptor Descriptor;

  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->TailCallRuntime(Runtime::kLoadGlobalIC_Slow, context, slot,
                             vector);
}

void Builtins::Generate_LoadIC_Getter_ForDeopt(MacroAssembler* masm) {
  NamedLoadHandlerCompiler::GenerateLoadViaGetterForDeopt(masm);
}

void Builtins::Generate_LoadIC_Miss(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->TailCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name,
                             slot, vector);
}

void Builtins::Generate_LoadIC_Normal(MacroAssembler* masm) {
  LoadIC::GenerateNormal(masm);
}

void Builtins::Generate_LoadIC_Slow(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->TailCallRuntime(Runtime::kGetProperty, context, receiver, name);
}

void Builtins::Generate_StoreIC_Miss(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot,
                             vector, receiver, name);
}

void Builtins::Generate_StoreIC_Normal(MacroAssembler* masm) {
  StoreIC::GenerateNormal(masm);
}

void Builtins::Generate_StoreIC_Setter_ForDeopt(MacroAssembler* masm) {
  NamedStoreHandlerCompiler::GenerateStoreViaSetterForDeopt(masm);
}

namespace {
void Generate_StoreIC_Slow(CodeStubAssembler* assembler,
                           LanguageMode language_mode) {
  typedef compiler::Node Node;
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* context = assembler->Parameter(Descriptor::kContext);
  Node* lang_mode = assembler->SmiConstant(Smi::FromInt(language_mode));

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  assembler->TailCallRuntime(Runtime::kSetProperty, context, receiver, name,
                             value, lang_mode);
}
}  // anonymous namespace

void Builtins::Generate_StoreIC_SlowSloppy(CodeStubAssembler* assembler) {
  Generate_StoreIC_Slow(assembler, SLOPPY);
}

void Builtins::Generate_StoreIC_SlowStrict(CodeStubAssembler* assembler) {
  Generate_StoreIC_Slow(assembler, STRICT);
}

}  // namespace internal
}  // namespace v8
