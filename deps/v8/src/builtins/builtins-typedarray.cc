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
  assembler->GotoIf(assembler->WordIsSmi(receiver), &if_receiverisincompatible);
  Node* receiver_instance_type = assembler->LoadInstanceType(receiver);
  assembler->GotoUnless(
      assembler->Word32Equal(receiver_instance_type,
                             assembler->Int32Constant(JS_TYPED_ARRAY_TYPE)),
      &if_receiverisincompatible);

  // Check if the {receiver}'s JSArrayBuffer was neutered.
  Node* receiver_buffer =
      assembler->LoadObjectField(receiver, JSTypedArray::kBufferOffset);
  Node* receiver_buffer_bit_field = assembler->LoadObjectField(
      receiver_buffer, JSArrayBuffer::kBitFieldOffset, MachineType::Uint32());
  Label if_receiverisneutered(assembler, Label::kDeferred);
  assembler->GotoUnless(
      assembler->Word32Equal(
          assembler->Word32And(
              receiver_buffer_bit_field,
              assembler->Int32Constant(JSArrayBuffer::WasNeutered::kMask)),
          assembler->Int32Constant(0)),
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

}  // namespace internal
}  // namespace v8
