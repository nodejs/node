// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"
#include "src/interface-descriptors.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

BUILTIN(Illegal) {
  UNREACHABLE();
  return isolate->heap()->undefined_value();  // Make compiler happy.
}

BUILTIN(EmptyFunction) { return isolate->heap()->undefined_value(); }

BUILTIN(UnsupportedThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                 NewError(MessageTemplate::kUnsupported));
}

// -----------------------------------------------------------------------------
// Throwers for restricted function properties and strict arguments object
// properties

BUILTIN(RestrictedFunctionPropertiesThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kRestrictedFunctionProperties));
}

BUILTIN(RestrictedStrictArgumentsPropertiesThrower) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kStrictPoisonPill));
}

// -----------------------------------------------------------------------------
// Interrupt and stack checks.

void Builtins::Generate_InterruptCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kInterrupt);
}

void Builtins::Generate_StackCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kStackGuard);
}

// -----------------------------------------------------------------------------
// TurboFan support builtins.

void Builtins::Generate_CopyFastSmiOrObjectElements(
    CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CopyFastSmiOrObjectElementsDescriptor Descriptor;

  Node* object = assembler->Parameter(Descriptor::kObject);

  // Load the {object}s elements.
  Node* source = assembler->LoadObjectField(object, JSObject::kElementsOffset);

  CodeStubAssembler::ParameterMode mode = assembler->OptimalParameterMode();
  Node* length = assembler->UntagParameter(
      assembler->LoadFixedArrayBaseLength(source), mode);

  // Check if we can allocate in new space.
  ElementsKind kind = FAST_ELEMENTS;
  int max_elements = FixedArrayBase::GetMaxLengthForNewSpaceAllocation(kind);
  Label if_newspace(assembler), if_oldspace(assembler);
  assembler->Branch(
      assembler->UintPtrLessThan(
          length, assembler->IntPtrOrSmiConstant(max_elements, mode)),
      &if_newspace, &if_oldspace);

  assembler->Bind(&if_newspace);
  {
    Node* target = assembler->AllocateFixedArray(kind, length, mode);
    assembler->CopyFixedArrayElements(kind, source, target, length,
                                      SKIP_WRITE_BARRIER, mode);
    assembler->StoreObjectField(object, JSObject::kElementsOffset, target);
    assembler->Return(target);
  }

  assembler->Bind(&if_oldspace);
  {
    Node* target = assembler->AllocateFixedArray(
        kind, length, mode, CodeStubAssembler::kPretenured);
    assembler->CopyFixedArrayElements(kind, source, target, length,
                                      UPDATE_WRITE_BARRIER, mode);
    assembler->StoreObjectField(object, JSObject::kElementsOffset, target);
    assembler->Return(target);
  }
}

void Builtins::Generate_GrowFastDoubleElements(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef GrowArrayElementsDescriptor Descriptor;

  Node* object = assembler->Parameter(Descriptor::kObject);
  Node* key = assembler->Parameter(Descriptor::kKey);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label runtime(assembler, CodeStubAssembler::Label::kDeferred);
  Node* elements = assembler->LoadElements(object);
  elements = assembler->TryGrowElementsCapacity(
      object, elements, FAST_DOUBLE_ELEMENTS, key, &runtime);
  assembler->Return(elements);

  assembler->Bind(&runtime);
  assembler->TailCallRuntime(Runtime::kGrowArrayElements, context, object, key);
}

void Builtins::Generate_GrowFastSmiOrObjectElements(
    CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef GrowArrayElementsDescriptor Descriptor;

  Node* object = assembler->Parameter(Descriptor::kObject);
  Node* key = assembler->Parameter(Descriptor::kKey);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label runtime(assembler, CodeStubAssembler::Label::kDeferred);
  Node* elements = assembler->LoadElements(object);
  elements = assembler->TryGrowElementsCapacity(object, elements, FAST_ELEMENTS,
                                                key, &runtime);
  assembler->Return(elements);

  assembler->Bind(&runtime);
  assembler->TailCallRuntime(Runtime::kGrowArrayElements, context, object, key);
}

}  // namespace internal
}  // namespace v8
