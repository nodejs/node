// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// ES7 sharedmem 6.3.4.1 get SharedArrayBuffer.prototype.byteLength
BUILTIN(SharedArrayBufferPrototypeGetByteLength) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSArrayBuffer, array_buffer,
                 "get SharedArrayBuffer.prototype.byteLength");
  if (!array_buffer->is_shared()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "get SharedArrayBuffer.prototype.byteLength"),
                              args.receiver()));
  }
  return array_buffer->byte_length();
}

namespace {

void ValidateSharedTypedArray(CodeStubAssembler* a, compiler::Node* tagged,
                              compiler::Node* context,
                              compiler::Node** out_instance_type,
                              compiler::Node** out_backing_store) {
  using compiler::Node;
  CodeStubAssembler::Label is_smi(a), not_smi(a), is_typed_array(a),
      not_typed_array(a), is_shared(a), not_shared(a), is_float_or_clamped(a),
      not_float_or_clamped(a), invalid(a);

  // Fail if it is not a heap object.
  a->Branch(a->TaggedIsSmi(tagged), &is_smi, &not_smi);
  a->Bind(&is_smi);
  a->Goto(&invalid);

  // Fail if the array's instance type is not JSTypedArray.
  a->Bind(&not_smi);
  a->Branch(a->Word32Equal(a->LoadInstanceType(tagged),
                           a->Int32Constant(JS_TYPED_ARRAY_TYPE)),
            &is_typed_array, &not_typed_array);
  a->Bind(&not_typed_array);
  a->Goto(&invalid);

  // Fail if the array's JSArrayBuffer is not shared.
  a->Bind(&is_typed_array);
  Node* array_buffer = a->LoadObjectField(tagged, JSTypedArray::kBufferOffset);
  Node* is_buffer_shared =
      a->IsSetWord32<JSArrayBuffer::IsShared>(a->LoadObjectField(
          array_buffer, JSArrayBuffer::kBitFieldOffset, MachineType::Uint32()));
  a->Branch(is_buffer_shared, &is_shared, &not_shared);
  a->Bind(&not_shared);
  a->Goto(&invalid);

  // Fail if the array's element type is float32, float64 or clamped.
  a->Bind(&is_shared);
  Node* elements_instance_type = a->LoadInstanceType(
      a->LoadObjectField(tagged, JSObject::kElementsOffset));
  STATIC_ASSERT(FIXED_INT8_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_INT16_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_INT32_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_UINT8_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_UINT16_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_UINT32_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  a->Branch(a->Int32LessThan(elements_instance_type,
                             a->Int32Constant(FIXED_FLOAT32_ARRAY_TYPE)),
            &not_float_or_clamped, &is_float_or_clamped);
  a->Bind(&is_float_or_clamped);
  a->Goto(&invalid);

  a->Bind(&invalid);
  a->CallRuntime(Runtime::kThrowNotIntegerSharedTypedArrayError, context,
                 tagged);
  a->Return(a->UndefinedConstant());

  a->Bind(&not_float_or_clamped);
  *out_instance_type = elements_instance_type;

  Node* backing_store =
      a->LoadObjectField(array_buffer, JSArrayBuffer::kBackingStoreOffset);
  Node* byte_offset = a->ChangeUint32ToWord(a->TruncateTaggedToWord32(
      context,
      a->LoadObjectField(tagged, JSArrayBufferView::kByteOffsetOffset)));
  *out_backing_store =
      a->IntPtrAdd(a->BitcastTaggedToWord(backing_store), byte_offset);
}

// https://tc39.github.io/ecmascript_sharedmem/shmem.html#Atomics.ValidateAtomicAccess
compiler::Node* ConvertTaggedAtomicIndexToWord32(CodeStubAssembler* a,
                                                 compiler::Node* tagged,
                                                 compiler::Node* context) {
  using compiler::Node;
  CodeStubAssembler::Variable var_result(a, MachineRepresentation::kWord32);

  Callable to_number = CodeFactory::ToNumber(a->isolate());
  Node* number_index = a->CallStub(to_number, context, tagged);
  CodeStubAssembler::Label done(a, &var_result);

  CodeStubAssembler::Label if_numberissmi(a), if_numberisnotsmi(a);
  a->Branch(a->TaggedIsSmi(number_index), &if_numberissmi, &if_numberisnotsmi);

  a->Bind(&if_numberissmi);
  {
    var_result.Bind(a->SmiToWord32(number_index));
    a->Goto(&done);
  }

  a->Bind(&if_numberisnotsmi);
  {
    Node* number_index_value = a->LoadHeapNumberValue(number_index);
    Node* access_index = a->TruncateFloat64ToWord32(number_index_value);
    Node* test_index = a->ChangeInt32ToFloat64(access_index);

    CodeStubAssembler::Label if_indexesareequal(a), if_indexesarenotequal(a);
    a->Branch(a->Float64Equal(number_index_value, test_index),
              &if_indexesareequal, &if_indexesarenotequal);

    a->Bind(&if_indexesareequal);
    {
      var_result.Bind(access_index);
      a->Goto(&done);
    }

    a->Bind(&if_indexesarenotequal);
    a->Return(
        a->CallRuntime(Runtime::kThrowInvalidAtomicAccessIndexError, context));
  }

  a->Bind(&done);
  return var_result.value();
}

void ValidateAtomicIndex(CodeStubAssembler* a, compiler::Node* index_word,
                         compiler::Node* array_length_word,
                         compiler::Node* context) {
  using compiler::Node;
  // Check if the index is in bounds. If not, throw RangeError.
  CodeStubAssembler::Label if_inbounds(a), if_notinbounds(a);
  // TODO(jkummerow): Use unsigned comparison instead of "i<0 || i>length".
  a->Branch(
      a->Word32Or(a->Int32LessThan(index_word, a->Int32Constant(0)),
                  a->Int32GreaterThanOrEqual(index_word, array_length_word)),
      &if_notinbounds, &if_inbounds);
  a->Bind(&if_notinbounds);
  a->Return(
      a->CallRuntime(Runtime::kThrowInvalidAtomicAccessIndexError, context));
  a->Bind(&if_inbounds);
}

}  // anonymous namespace

void Builtins::Generate_AtomicsLoad(compiler::CodeAssemblerState* state) {
  using compiler::Node;
  CodeStubAssembler a(state);
  Node* array = a.Parameter(1);
  Node* index = a.Parameter(2);
  Node* context = a.Parameter(3 + 2);

  Node* instance_type;
  Node* backing_store;
  ValidateSharedTypedArray(&a, array, context, &instance_type, &backing_store);

  Node* index_word32 = ConvertTaggedAtomicIndexToWord32(&a, index, context);
  Node* array_length_word32 = a.TruncateTaggedToWord32(
      context, a.LoadObjectField(array, JSTypedArray::kLengthOffset));
  ValidateAtomicIndex(&a, index_word32, array_length_word32, context);
  Node* index_word = a.ChangeUint32ToWord(index_word32);

  CodeStubAssembler::Label i8(&a), u8(&a), i16(&a), u16(&a), i32(&a), u32(&a),
      other(&a);
  int32_t case_values[] = {
      FIXED_INT8_ARRAY_TYPE,   FIXED_UINT8_ARRAY_TYPE, FIXED_INT16_ARRAY_TYPE,
      FIXED_UINT16_ARRAY_TYPE, FIXED_INT32_ARRAY_TYPE, FIXED_UINT32_ARRAY_TYPE,
  };
  CodeStubAssembler::Label* case_labels[] = {
      &i8, &u8, &i16, &u16, &i32, &u32,
  };
  a.Switch(instance_type, &other, case_values, case_labels,
           arraysize(case_labels));

  a.Bind(&i8);
  a.Return(a.SmiFromWord32(
      a.AtomicLoad(MachineType::Int8(), backing_store, index_word)));

  a.Bind(&u8);
  a.Return(a.SmiFromWord32(
      a.AtomicLoad(MachineType::Uint8(), backing_store, index_word)));

  a.Bind(&i16);
  a.Return(a.SmiFromWord32(a.AtomicLoad(MachineType::Int16(), backing_store,
                                        a.WordShl(index_word, 1))));

  a.Bind(&u16);
  a.Return(a.SmiFromWord32(a.AtomicLoad(MachineType::Uint16(), backing_store,
                                        a.WordShl(index_word, 1))));

  a.Bind(&i32);
  a.Return(a.ChangeInt32ToTagged(a.AtomicLoad(
      MachineType::Int32(), backing_store, a.WordShl(index_word, 2))));

  a.Bind(&u32);
  a.Return(a.ChangeUint32ToTagged(a.AtomicLoad(
      MachineType::Uint32(), backing_store, a.WordShl(index_word, 2))));

  // This shouldn't happen, we've already validated the type.
  a.Bind(&other);
  a.Return(a.SmiConstant(0));
}

void Builtins::Generate_AtomicsStore(compiler::CodeAssemblerState* state) {
  using compiler::Node;
  CodeStubAssembler a(state);
  Node* array = a.Parameter(1);
  Node* index = a.Parameter(2);
  Node* value = a.Parameter(3);
  Node* context = a.Parameter(4 + 2);

  Node* instance_type;
  Node* backing_store;
  ValidateSharedTypedArray(&a, array, context, &instance_type, &backing_store);

  Node* index_word32 = ConvertTaggedAtomicIndexToWord32(&a, index, context);
  Node* array_length_word32 = a.TruncateTaggedToWord32(
      context, a.LoadObjectField(array, JSTypedArray::kLengthOffset));
  ValidateAtomicIndex(&a, index_word32, array_length_word32, context);
  Node* index_word = a.ChangeUint32ToWord(index_word32);

  Node* value_integer = a.ToInteger(context, value);
  Node* value_word32 = a.TruncateTaggedToWord32(context, value_integer);

  CodeStubAssembler::Label u8(&a), u16(&a), u32(&a), other(&a);
  int32_t case_values[] = {
      FIXED_INT8_ARRAY_TYPE,   FIXED_UINT8_ARRAY_TYPE, FIXED_INT16_ARRAY_TYPE,
      FIXED_UINT16_ARRAY_TYPE, FIXED_INT32_ARRAY_TYPE, FIXED_UINT32_ARRAY_TYPE,
  };
  CodeStubAssembler::Label* case_labels[] = {
      &u8, &u8, &u16, &u16, &u32, &u32,
  };
  a.Switch(instance_type, &other, case_values, case_labels,
           arraysize(case_labels));

  a.Bind(&u8);
  a.AtomicStore(MachineRepresentation::kWord8, backing_store, index_word,
                value_word32);
  a.Return(value_integer);

  a.Bind(&u16);
  a.AtomicStore(MachineRepresentation::kWord16, backing_store,
                a.WordShl(index_word, 1), value_word32);
  a.Return(value_integer);

  a.Bind(&u32);
  a.AtomicStore(MachineRepresentation::kWord32, backing_store,
                a.WordShl(index_word, 2), value_word32);
  a.Return(value_integer);

  // This shouldn't happen, we've already validated the type.
  a.Bind(&other);
  a.Return(a.SmiConstant(0));
}

}  // namespace internal
}  // namespace v8
