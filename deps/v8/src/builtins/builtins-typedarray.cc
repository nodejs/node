// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// ES6 section 22.2 TypedArray Objects

// ES6 section 22.2.3.1 get %TypedArray%.prototype.buffer
BUILTIN(TypedArrayPrototypeBuffer) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSTypedArray, typed_array, "get TypedArray.prototype.buffer");
  return *typed_array->GetBuffer();
}

namespace {

void Generate_TypedArrayProtoypeGetter(CodeStubAssembler* assembler,
                                       const char* method_name,
                                       int object_offset) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  // Check if the {receiver} is actually a JSTypedArray.
  Label if_receiverisincompatible(assembler, Label::kDeferred);
  assembler->GotoIf(assembler->TaggedIsSmi(receiver),
                    &if_receiverisincompatible);
  Node* receiver_instance_type = assembler->LoadInstanceType(receiver);
  assembler->GotoUnless(
      assembler->Word32Equal(receiver_instance_type,
                             assembler->Int32Constant(JS_TYPED_ARRAY_TYPE)),
      &if_receiverisincompatible);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      assembler->LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Label if_receiverisneutered(assembler, Label::kDeferred);
  assembler->GotoIf(assembler->IsDetachedBuffer(receiver_buffer),
                    &if_receiverisneutered);
  assembler->Return(assembler->LoadObjectField(receiver, object_offset));

  assembler->Bind(&if_receiverisneutered);
  {
    // The {receiver}s buffer was neutered, default to zero.
    assembler->Return(assembler->SmiConstant(0));
  }

  assembler->Bind(&if_receiverisincompatible);
  {
    // The {receiver} is not a valid JSGeneratorObject.
    Node* result = assembler->CallRuntime(
        Runtime::kThrowIncompatibleMethodReceiver, context,
        assembler->HeapConstant(assembler->factory()->NewStringFromAsciiChecked(
            method_name, TENURED)),
        receiver);
    assembler->Return(result);  // Never reached.
  }
}

}  // namespace

// ES6 section 22.2.3.2 get %TypedArray%.prototype.byteLength
void Builtins::Generate_TypedArrayPrototypeByteLength(
    CodeStubAssembler* assembler) {
  Generate_TypedArrayProtoypeGetter(assembler,
                                    "get TypedArray.prototype.byteLength",
                                    JSTypedArray::kByteLengthOffset);
}

// ES6 section 22.2.3.3 get %TypedArray%.prototype.byteOffset
void Builtins::Generate_TypedArrayPrototypeByteOffset(
    CodeStubAssembler* assembler) {
  Generate_TypedArrayProtoypeGetter(assembler,
                                    "get TypedArray.prototype.byteOffset",
                                    JSTypedArray::kByteOffsetOffset);
}

// ES6 section 22.2.3.18 get %TypedArray%.prototype.length
void Builtins::Generate_TypedArrayPrototypeLength(
    CodeStubAssembler* assembler) {
  Generate_TypedArrayProtoypeGetter(assembler,
                                    "get TypedArray.prototype.length",
                                    JSTypedArray::kLengthOffset);
}

namespace {

template <IterationKind kIterationKind>
void Generate_TypedArrayPrototypeIterationMethod(CodeStubAssembler* assembler,
                                                 const char* method_name) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Node* receiver = assembler->Parameter(0);
  Node* context = assembler->Parameter(3);

  Label throw_bad_receiver(assembler, Label::kDeferred);
  Label throw_typeerror(assembler, Label::kDeferred);

  assembler->GotoIf(assembler->TaggedIsSmi(receiver), &throw_bad_receiver);

  Node* map = assembler->LoadMap(receiver);
  Node* instance_type = assembler->LoadMapInstanceType(map);
  assembler->GotoIf(
      assembler->Word32NotEqual(instance_type,
                                assembler->Int32Constant(JS_TYPED_ARRAY_TYPE)),
      &throw_bad_receiver);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      assembler->LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Label if_receiverisneutered(assembler, Label::kDeferred);
  assembler->GotoIf(assembler->IsDetachedBuffer(receiver_buffer),
                    &if_receiverisneutered);

  assembler->Return(assembler->CreateArrayIterator(receiver, map, instance_type,
                                                   context, kIterationKind));

  Variable var_message(assembler, MachineRepresentation::kTagged);
  assembler->Bind(&throw_bad_receiver);
  var_message.Bind(
      assembler->SmiConstant(Smi::FromInt(MessageTemplate::kNotTypedArray)));
  assembler->Goto(&throw_typeerror);

  assembler->Bind(&if_receiverisneutered);
  var_message.Bind(assembler->SmiConstant(
      Smi::FromInt(MessageTemplate::kDetachedOperation)));
  assembler->Goto(&throw_typeerror);

  assembler->Bind(&throw_typeerror);
  {
    Node* arg1 = assembler->HeapConstant(
        assembler->isolate()->factory()->NewStringFromAsciiChecked(method_name,
                                                                   TENURED));
    Node* result = assembler->CallRuntime(Runtime::kThrowTypeError, context,
                                          var_message.value(), arg1);
    assembler->Return(result);
  }
}
}  // namespace

void Builtins::Generate_TypedArrayPrototypeValues(
    CodeStubAssembler* assembler) {
  Generate_TypedArrayPrototypeIterationMethod<IterationKind::kValues>(
      assembler, "%TypedArray%.prototype.values()");
}

void Builtins::Generate_TypedArrayPrototypeEntries(
    CodeStubAssembler* assembler) {
  Generate_TypedArrayPrototypeIterationMethod<IterationKind::kEntries>(
      assembler, "%TypedArray%.prototype.entries()");
}

void Builtins::Generate_TypedArrayPrototypeKeys(CodeStubAssembler* assembler) {
  Generate_TypedArrayPrototypeIterationMethod<IterationKind::kKeys>(
      assembler, "%TypedArray%.prototype.keys()");
}

}  // namespace internal
}  // namespace v8
