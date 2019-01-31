// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <typename T>
using TNode = compiler::TNode<T>;

class SharedArrayBufferBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit SharedArrayBufferBuiltinsAssembler(
      compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  typedef Node* (CodeAssembler::*AssemblerFunction)(MachineType type,
                                                    Node* base, Node* offset,
                                                    Node* value,
                                                    Node* value_high);
  void ValidateSharedTypedArray(Node* tagged, Node* context,
                                Node** out_instance_type,
                                Node** out_backing_store);
  Node* ConvertTaggedAtomicIndexToWord32(Node* tagged, Node* context,
                                         Node** number_index);
  void ValidateAtomicIndex(Node* array, Node* index_word, Node* context);
#if DEBUG
  void DebugSanityCheckAtomicIndex(Node* array, Node* index_word,
                                   Node* context);
#endif
  void AtomicBinopBuiltinCommon(Node* array, Node* index, Node* value,
                                Node* context, AssemblerFunction function,
                                Runtime::FunctionId runtime_function);

  // Create a BigInt from the result of a 64-bit atomic operation, using
  // projections on 32-bit platforms.
  TNode<BigInt> BigIntFromSigned64(Node* signed64);
  TNode<BigInt> BigIntFromUnsigned64(Node* unsigned64);
};

void SharedArrayBufferBuiltinsAssembler::ValidateSharedTypedArray(
    Node* tagged, Node* context, Node** out_instance_type,
    Node** out_backing_store) {
  Label not_float_or_clamped(this), invalid(this);

  // Fail if it is not a heap object.
  GotoIf(TaggedIsSmi(tagged), &invalid);

  // Fail if the array's instance type is not JSTypedArray.
  GotoIfNot(InstanceTypeEqual(LoadInstanceType(tagged), JS_TYPED_ARRAY_TYPE),
            &invalid);

  // Fail if the array's JSArrayBuffer is not shared.
  TNode<JSArrayBuffer> array_buffer = LoadJSArrayBufferViewBuffer(CAST(tagged));
  TNode<Uint32T> bitfield = LoadJSArrayBufferBitField(array_buffer);
  GotoIfNot(IsSetWord32<JSArrayBuffer::IsSharedBit>(bitfield), &invalid);

  // Fail if the array's element type is float32, float64 or clamped.
  Node* elements_instance_type = LoadInstanceType(LoadElements(tagged));
  STATIC_ASSERT(FIXED_INT8_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_INT16_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_INT32_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_UINT8_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_UINT16_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_UINT32_ARRAY_TYPE < FIXED_FLOAT32_ARRAY_TYPE);
  GotoIf(Int32LessThan(elements_instance_type,
                       Int32Constant(FIXED_FLOAT32_ARRAY_TYPE)),
         &not_float_or_clamped);
  STATIC_ASSERT(FIXED_BIGINT64_ARRAY_TYPE > FIXED_UINT8_CLAMPED_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_BIGUINT64_ARRAY_TYPE > FIXED_UINT8_CLAMPED_ARRAY_TYPE);
  Branch(Int32GreaterThan(elements_instance_type,
                          Int32Constant(FIXED_UINT8_CLAMPED_ARRAY_TYPE)),
         &not_float_or_clamped, &invalid);

  BIND(&invalid);
  {
    ThrowTypeError(context, MessageTemplate::kNotIntegerSharedTypedArray,
                   tagged);
  }

  BIND(&not_float_or_clamped);
  *out_instance_type = elements_instance_type;

  TNode<RawPtrT> backing_store = LoadJSArrayBufferBackingStore(array_buffer);
  TNode<UintPtrT> byte_offset = LoadJSArrayBufferViewByteOffset(CAST(tagged));
  *out_backing_store = IntPtrAdd(backing_store, byte_offset);
}

// https://tc39.github.io/ecma262/#sec-validateatomicaccess
Node* SharedArrayBufferBuiltinsAssembler::ConvertTaggedAtomicIndexToWord32(
    Node* tagged, Node* context, Node** number_index) {
  VARIABLE(var_result, MachineRepresentation::kWord32);
  Label done(this), range_error(this);

  // Returns word32 since index cannot be longer than a TypedArray length,
  // which has a uint32 maximum.
  // The |number_index| output parameter is used only for architectures that
  // don't currently have a TF implementation and forward to runtime functions
  // instead; they expect the value has already been coerced to an integer.
  *number_index = ToSmiIndex(CAST(context), CAST(tagged), &range_error);
  var_result.Bind(SmiToInt32(*number_index));
  Goto(&done);

  BIND(&range_error);
  { ThrowRangeError(context, MessageTemplate::kInvalidAtomicAccessIndex); }

  BIND(&done);
  return var_result.value();
}

void SharedArrayBufferBuiltinsAssembler::ValidateAtomicIndex(Node* array,
                                                             Node* index_word,
                                                             Node* context) {
  // Check if the index is in bounds. If not, throw RangeError.
  Label check_passed(this);
  Node* array_length_word32 =
      TruncateTaggedToWord32(context, LoadJSTypedArrayLength(CAST(array)));
  GotoIf(Uint32LessThan(index_word, array_length_word32), &check_passed);

  ThrowRangeError(context, MessageTemplate::kInvalidAtomicAccessIndex);

  BIND(&check_passed);
}

#if DEBUG
void SharedArrayBufferBuiltinsAssembler::DebugSanityCheckAtomicIndex(
    Node* array, Node* index_word, Node* context) {
  // In Debug mode, we re-validate the index as a sanity check because
  // ToInteger above calls out to JavaScript. A SharedArrayBuffer can't be
  // detached and the TypedArray length can't change either, so skipping this
  // check in Release mode is safe.
  CSA_ASSERT(this,
             Uint32LessThan(index_word,
                            TruncateTaggedToWord32(
                                context, LoadJSTypedArrayLength(CAST(array)))));
}
#endif

TNode<BigInt> SharedArrayBufferBuiltinsAssembler::BigIntFromSigned64(
    Node* signed64) {
  if (Is64()) {
    return BigIntFromInt64(UncheckedCast<IntPtrT>(signed64));
  } else {
    TNode<IntPtrT> low = UncheckedCast<IntPtrT>(Projection(0, signed64));
    TNode<IntPtrT> high = UncheckedCast<IntPtrT>(Projection(1, signed64));
    return BigIntFromInt32Pair(low, high);
  }
}

TNode<BigInt> SharedArrayBufferBuiltinsAssembler::BigIntFromUnsigned64(
    Node* unsigned64) {
  if (Is64()) {
    return BigIntFromUint64(UncheckedCast<UintPtrT>(unsigned64));
  } else {
    TNode<UintPtrT> low = UncheckedCast<UintPtrT>(Projection(0, unsigned64));
    TNode<UintPtrT> high = UncheckedCast<UintPtrT>(Projection(1, unsigned64));
    return BigIntFromUint32Pair(low, high);
  }
}

TF_BUILTIN(AtomicsLoad, SharedArrayBufferBuiltinsAssembler) {
  Node* array = Parameter(Descriptor::kArray);
  Node* index = Parameter(Descriptor::kIndex);
  Node* context = Parameter(Descriptor::kContext);

  Node* instance_type;
  Node* backing_store;
  ValidateSharedTypedArray(array, context, &instance_type, &backing_store);

  Node* index_integer;
  Node* index_word32 =
      ConvertTaggedAtomicIndexToWord32(index, context, &index_integer);
  ValidateAtomicIndex(array, index_word32, context);
  Node* index_word = ChangeUint32ToWord(index_word32);

  Label i8(this), u8(this), i16(this), u16(this), i32(this), u32(this),
      i64(this), u64(this), other(this);
  int32_t case_values[] = {
      FIXED_INT8_ARRAY_TYPE,     FIXED_UINT8_ARRAY_TYPE,
      FIXED_INT16_ARRAY_TYPE,    FIXED_UINT16_ARRAY_TYPE,
      FIXED_INT32_ARRAY_TYPE,    FIXED_UINT32_ARRAY_TYPE,
      FIXED_BIGINT64_ARRAY_TYPE, FIXED_BIGUINT64_ARRAY_TYPE,
  };
  Label* case_labels[] = {&i8, &u8, &i16, &u16, &i32, &u32, &i64, &u64};
  Switch(instance_type, &other, case_values, case_labels,
         arraysize(case_labels));

  BIND(&i8);
  Return(
      SmiFromInt32(AtomicLoad(MachineType::Int8(), backing_store, index_word)));

  BIND(&u8);
  Return(SmiFromInt32(
      AtomicLoad(MachineType::Uint8(), backing_store, index_word)));

  BIND(&i16);
  Return(SmiFromInt32(
      AtomicLoad(MachineType::Int16(), backing_store, WordShl(index_word, 1))));

  BIND(&u16);
  Return(SmiFromInt32(AtomicLoad(MachineType::Uint16(), backing_store,
                                 WordShl(index_word, 1))));

  BIND(&i32);
  Return(ChangeInt32ToTagged(
      AtomicLoad(MachineType::Int32(), backing_store, WordShl(index_word, 2))));

  BIND(&u32);
  Return(ChangeUint32ToTagged(AtomicLoad(MachineType::Uint32(), backing_store,
                                         WordShl(index_word, 2))));
#if V8_TARGET_ARCH_MIPS && !_MIPS_ARCH_MIPS32R6
  BIND(&i64);
  Return(CallRuntime(Runtime::kAtomicsLoad64, context, array, index_integer));

  BIND(&u64);
  Return(CallRuntime(Runtime::kAtomicsLoad64, context, array, index_integer));
#else
  BIND(&i64);
  // This uses Uint64() intentionally: AtomicLoad is not implemented for
  // Int64(), which is fine because the machine instruction only cares
  // about words.
  Return(BigIntFromSigned64(AtomicLoad(MachineType::Uint64(), backing_store,
                                       WordShl(index_word, 3))));

  BIND(&u64);
  Return(BigIntFromUnsigned64(AtomicLoad(MachineType::Uint64(), backing_store,
                                         WordShl(index_word, 3))));
#endif
  // This shouldn't happen, we've already validated the type.
  BIND(&other);
  Unreachable();
}

TF_BUILTIN(AtomicsStore, SharedArrayBufferBuiltinsAssembler) {
  Node* array = Parameter(Descriptor::kArray);
  Node* index = Parameter(Descriptor::kIndex);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);

  Node* instance_type;
  Node* backing_store;
  ValidateSharedTypedArray(array, context, &instance_type, &backing_store);

  Node* index_integer;
  Node* index_word32 =
      ConvertTaggedAtomicIndexToWord32(index, context, &index_integer);
  ValidateAtomicIndex(array, index_word32, context);
  Node* index_word = ChangeUint32ToWord(index_word32);

  Label u8(this), u16(this), u32(this), u64(this), other(this);
  STATIC_ASSERT(FIXED_BIGINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_BIGUINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  GotoIf(
      Int32GreaterThan(instance_type, Int32Constant(FIXED_UINT32_ARRAY_TYPE)),
      &u64);

  Node* value_integer = ToInteger_Inline(CAST(context), CAST(value));
  Node* value_word32 = TruncateTaggedToWord32(context, value_integer);

#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif

  int32_t case_values[] = {
      FIXED_INT8_ARRAY_TYPE,   FIXED_UINT8_ARRAY_TYPE, FIXED_INT16_ARRAY_TYPE,
      FIXED_UINT16_ARRAY_TYPE, FIXED_INT32_ARRAY_TYPE, FIXED_UINT32_ARRAY_TYPE,
  };
  Label* case_labels[] = {&u8, &u8, &u16, &u16, &u32, &u32};
  Switch(instance_type, &other, case_values, case_labels,
         arraysize(case_labels));

  BIND(&u8);
  AtomicStore(MachineRepresentation::kWord8, backing_store, index_word,
              value_word32);
  Return(value_integer);

  BIND(&u16);
  AtomicStore(MachineRepresentation::kWord16, backing_store,
              WordShl(index_word, 1), value_word32);
  Return(value_integer);

  BIND(&u32);
  AtomicStore(MachineRepresentation::kWord32, backing_store,
              WordShl(index_word, 2), value_word32);
  Return(value_integer);

  BIND(&u64);
#if V8_TARGET_ARCH_MIPS && !_MIPS_ARCH_MIPS32R6
  Return(CallRuntime(Runtime::kAtomicsStore64, context, array, index_integer,
                     value));
#else
  TNode<BigInt> value_bigint = ToBigInt(CAST(context), CAST(value));
#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif
  TVARIABLE(UintPtrT, var_low);
  TVARIABLE(UintPtrT, var_high);
  BigIntToRawBytes(value_bigint, &var_low, &var_high);
  Node* high = Is64() ? nullptr : static_cast<Node*>(var_high.value());
  AtomicStore(MachineRepresentation::kWord64, backing_store,
              WordShl(index_word, 3), var_low.value(), high);
  Return(value_bigint);
#endif

  // This shouldn't happen, we've already validated the type.
  BIND(&other);
  Unreachable();
}

TF_BUILTIN(AtomicsExchange, SharedArrayBufferBuiltinsAssembler) {
  Node* array = Parameter(Descriptor::kArray);
  Node* index = Parameter(Descriptor::kIndex);
  Node* value = Parameter(Descriptor::kValue);
  Node* context = Parameter(Descriptor::kContext);

  Node* instance_type;
  Node* backing_store;
  ValidateSharedTypedArray(array, context, &instance_type, &backing_store);

  Node* index_integer;
  Node* index_word32 =
      ConvertTaggedAtomicIndexToWord32(index, context, &index_integer);
  ValidateAtomicIndex(array, index_word32, context);

#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
  Return(CallRuntime(Runtime::kAtomicsExchange, context, array, index_integer,
                     value));
#else
  Node* index_word = ChangeUint32ToWord(index_word32);

  Label i8(this), u8(this), i16(this), u16(this), i32(this), u32(this),
      i64(this), u64(this), big(this), other(this);
  STATIC_ASSERT(FIXED_BIGINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_BIGUINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  GotoIf(
      Int32GreaterThan(instance_type, Int32Constant(FIXED_UINT32_ARRAY_TYPE)),
      &big);

  Node* value_integer = ToInteger_Inline(CAST(context), CAST(value));
#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif
  Node* value_word32 = TruncateTaggedToWord32(context, value_integer);

  int32_t case_values[] = {
      FIXED_INT8_ARRAY_TYPE,   FIXED_UINT8_ARRAY_TYPE, FIXED_INT16_ARRAY_TYPE,
      FIXED_UINT16_ARRAY_TYPE, FIXED_INT32_ARRAY_TYPE, FIXED_UINT32_ARRAY_TYPE,
  };
  Label* case_labels[] = {
      &i8, &u8, &i16, &u16, &i32, &u32,
  };
  Switch(instance_type, &other, case_values, case_labels,
         arraysize(case_labels));

  BIND(&i8);
  Return(SmiFromInt32(AtomicExchange(MachineType::Int8(), backing_store,
                                     index_word, value_word32)));

  BIND(&u8);
  Return(SmiFromInt32(AtomicExchange(MachineType::Uint8(), backing_store,
                                     index_word, value_word32)));

  BIND(&i16);
  Return(SmiFromInt32(AtomicExchange(MachineType::Int16(), backing_store,
                                     WordShl(index_word, 1), value_word32)));

  BIND(&u16);
  Return(SmiFromInt32(AtomicExchange(MachineType::Uint16(), backing_store,
                                     WordShl(index_word, 1), value_word32)));

  BIND(&i32);
  Return(ChangeInt32ToTagged(AtomicExchange(MachineType::Int32(), backing_store,
                                            WordShl(index_word, 2),
                                            value_word32)));

  BIND(&u32);
  Return(ChangeUint32ToTagged(
      AtomicExchange(MachineType::Uint32(), backing_store,
                     WordShl(index_word, 2), value_word32)));

  BIND(&big);
  TNode<BigInt> value_bigint = ToBigInt(CAST(context), CAST(value));
#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif
  TVARIABLE(UintPtrT, var_low);
  TVARIABLE(UintPtrT, var_high);
  BigIntToRawBytes(value_bigint, &var_low, &var_high);
  Node* high = Is64() ? nullptr : static_cast<Node*>(var_high.value());
  GotoIf(Word32Equal(instance_type, Int32Constant(FIXED_BIGINT64_ARRAY_TYPE)),
         &i64);
  GotoIf(Word32Equal(instance_type, Int32Constant(FIXED_BIGUINT64_ARRAY_TYPE)),
         &u64);
  Unreachable();

  BIND(&i64);
  // This uses Uint64() intentionally: AtomicExchange is not implemented for
  // Int64(), which is fine because the machine instruction only cares
  // about words.
  Return(BigIntFromSigned64(AtomicExchange(MachineType::Uint64(), backing_store,
                                           WordShl(index_word, 3),
                                           var_low.value(), high)));

  BIND(&u64);
  Return(BigIntFromUnsigned64(
      AtomicExchange(MachineType::Uint64(), backing_store,
                     WordShl(index_word, 3), var_low.value(), high)));

  // This shouldn't happen, we've already validated the type.
  BIND(&other);
  Unreachable();
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
}

TF_BUILTIN(AtomicsCompareExchange, SharedArrayBufferBuiltinsAssembler) {
  Node* array = Parameter(Descriptor::kArray);
  Node* index = Parameter(Descriptor::kIndex);
  Node* old_value = Parameter(Descriptor::kOldValue);
  Node* new_value = Parameter(Descriptor::kNewValue);
  Node* context = Parameter(Descriptor::kContext);

  Node* instance_type;
  Node* backing_store;
  ValidateSharedTypedArray(array, context, &instance_type, &backing_store);

  Node* index_integer;
  Node* index_word32 =
      ConvertTaggedAtomicIndexToWord32(index, context, &index_integer);
  ValidateAtomicIndex(array, index_word32, context);

#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64 || \
    V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_S390X
  Return(CallRuntime(Runtime::kAtomicsCompareExchange, context, array,
                     index_integer, old_value, new_value));
#else
  Node* index_word = ChangeUint32ToWord(index_word32);

  Label i8(this), u8(this), i16(this), u16(this), i32(this), u32(this),
      i64(this), u64(this), big(this), other(this);
  STATIC_ASSERT(FIXED_BIGINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_BIGUINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  GotoIf(
      Int32GreaterThan(instance_type, Int32Constant(FIXED_UINT32_ARRAY_TYPE)),
      &big);

  Node* old_value_integer = ToInteger_Inline(CAST(context), CAST(old_value));
  Node* new_value_integer = ToInteger_Inline(CAST(context), CAST(new_value));
#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif
  Node* old_value_word32 = TruncateTaggedToWord32(context, old_value_integer);
  Node* new_value_word32 = TruncateTaggedToWord32(context, new_value_integer);

  int32_t case_values[] = {
      FIXED_INT8_ARRAY_TYPE,   FIXED_UINT8_ARRAY_TYPE, FIXED_INT16_ARRAY_TYPE,
      FIXED_UINT16_ARRAY_TYPE, FIXED_INT32_ARRAY_TYPE, FIXED_UINT32_ARRAY_TYPE,
  };
  Label* case_labels[] = {
      &i8, &u8, &i16, &u16, &i32, &u32,
  };
  Switch(instance_type, &other, case_values, case_labels,
         arraysize(case_labels));

  BIND(&i8);
  Return(SmiFromInt32(AtomicCompareExchange(MachineType::Int8(), backing_store,
                                            index_word, old_value_word32,
                                            new_value_word32)));

  BIND(&u8);
  Return(SmiFromInt32(AtomicCompareExchange(MachineType::Uint8(), backing_store,
                                            index_word, old_value_word32,
                                            new_value_word32)));

  BIND(&i16);
  Return(SmiFromInt32(AtomicCompareExchange(
      MachineType::Int16(), backing_store, WordShl(index_word, 1),
      old_value_word32, new_value_word32)));

  BIND(&u16);
  Return(SmiFromInt32(AtomicCompareExchange(
      MachineType::Uint16(), backing_store, WordShl(index_word, 1),
      old_value_word32, new_value_word32)));

  BIND(&i32);
  Return(ChangeInt32ToTagged(AtomicCompareExchange(
      MachineType::Int32(), backing_store, WordShl(index_word, 2),
      old_value_word32, new_value_word32)));

  BIND(&u32);
  Return(ChangeUint32ToTagged(AtomicCompareExchange(
      MachineType::Uint32(), backing_store, WordShl(index_word, 2),
      old_value_word32, new_value_word32)));

  BIND(&big);
  TNode<BigInt> old_value_bigint = ToBigInt(CAST(context), CAST(old_value));
  TNode<BigInt> new_value_bigint = ToBigInt(CAST(context), CAST(new_value));
#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif
  TVARIABLE(UintPtrT, var_old_low);
  TVARIABLE(UintPtrT, var_old_high);
  TVARIABLE(UintPtrT, var_new_low);
  TVARIABLE(UintPtrT, var_new_high);
  BigIntToRawBytes(old_value_bigint, &var_old_low, &var_old_high);
  BigIntToRawBytes(new_value_bigint, &var_new_low, &var_new_high);
  Node* old_high = Is64() ? nullptr : static_cast<Node*>(var_old_high.value());
  Node* new_high = Is64() ? nullptr : static_cast<Node*>(var_new_high.value());
  GotoIf(Word32Equal(instance_type, Int32Constant(FIXED_BIGINT64_ARRAY_TYPE)),
         &i64);
  GotoIf(Word32Equal(instance_type, Int32Constant(FIXED_BIGUINT64_ARRAY_TYPE)),
         &u64);
  Unreachable();

  BIND(&i64);
  // This uses Uint64() intentionally: AtomicCompareExchange is not implemented
  // for Int64(), which is fine because the machine instruction only cares
  // about words.
  Return(BigIntFromSigned64(AtomicCompareExchange(
      MachineType::Uint64(), backing_store, WordShl(index_word, 3),
      var_old_low.value(), var_new_low.value(), old_high, new_high)));

  BIND(&u64);
  Return(BigIntFromUnsigned64(AtomicCompareExchange(
      MachineType::Uint64(), backing_store, WordShl(index_word, 3),
      var_old_low.value(), var_new_low.value(), old_high, new_high)));

  // This shouldn't happen, we've already validated the type.
  BIND(&other);
  Unreachable();
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64
        // || V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_S390X
}

#define BINOP_BUILTIN(op)                                       \
  TF_BUILTIN(Atomics##op, SharedArrayBufferBuiltinsAssembler) { \
    Node* array = Parameter(Descriptor::kArray);                \
    Node* index = Parameter(Descriptor::kIndex);                \
    Node* value = Parameter(Descriptor::kValue);                \
    Node* context = Parameter(Descriptor::kContext);            \
    AtomicBinopBuiltinCommon(array, index, value, context,      \
                             &CodeAssembler::Atomic##op,        \
                             Runtime::kAtomics##op);            \
  }
BINOP_BUILTIN(Add)
BINOP_BUILTIN(Sub)
BINOP_BUILTIN(And)
BINOP_BUILTIN(Or)
BINOP_BUILTIN(Xor)
#undef BINOP_BUILTIN

void SharedArrayBufferBuiltinsAssembler::AtomicBinopBuiltinCommon(
    Node* array, Node* index, Node* value, Node* context,
    AssemblerFunction function, Runtime::FunctionId runtime_function) {
  Node* instance_type;
  Node* backing_store;
  ValidateSharedTypedArray(array, context, &instance_type, &backing_store);

  Node* index_integer;
  Node* index_word32 =
      ConvertTaggedAtomicIndexToWord32(index, context, &index_integer);
  ValidateAtomicIndex(array, index_word32, context);

#if V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64 || \
    V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_S390X
  Return(CallRuntime(runtime_function, context, array, index_integer, value));
#else
  Node* index_word = ChangeUint32ToWord(index_word32);

  Label i8(this), u8(this), i16(this), u16(this), i32(this), u32(this),
      i64(this), u64(this), big(this), other(this);

  STATIC_ASSERT(FIXED_BIGINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  STATIC_ASSERT(FIXED_BIGUINT64_ARRAY_TYPE > FIXED_UINT32_ARRAY_TYPE);
  GotoIf(
      Int32GreaterThan(instance_type, Int32Constant(FIXED_UINT32_ARRAY_TYPE)),
      &big);

  Node* value_integer = ToInteger_Inline(CAST(context), CAST(value));
#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif
  Node* value_word32 = TruncateTaggedToWord32(context, value_integer);

  int32_t case_values[] = {
      FIXED_INT8_ARRAY_TYPE,   FIXED_UINT8_ARRAY_TYPE, FIXED_INT16_ARRAY_TYPE,
      FIXED_UINT16_ARRAY_TYPE, FIXED_INT32_ARRAY_TYPE, FIXED_UINT32_ARRAY_TYPE,
  };
  Label* case_labels[] = {
      &i8, &u8, &i16, &u16, &i32, &u32,
  };
  Switch(instance_type, &other, case_values, case_labels,
         arraysize(case_labels));

  BIND(&i8);
  Return(SmiFromInt32((this->*function)(MachineType::Int8(), backing_store,
                                        index_word, value_word32, nullptr)));

  BIND(&u8);
  Return(SmiFromInt32((this->*function)(MachineType::Uint8(), backing_store,
                                        index_word, value_word32, nullptr)));

  BIND(&i16);
  Return(SmiFromInt32((this->*function)(MachineType::Int16(), backing_store,
                                        WordShl(index_word, 1), value_word32,
                                        nullptr)));

  BIND(&u16);
  Return(SmiFromInt32((this->*function)(MachineType::Uint16(), backing_store,
                                        WordShl(index_word, 1), value_word32,
                                        nullptr)));

  BIND(&i32);
  Return(ChangeInt32ToTagged(
      (this->*function)(MachineType::Int32(), backing_store,
                        WordShl(index_word, 2), value_word32, nullptr)));

  BIND(&u32);
  Return(ChangeUint32ToTagged(
      (this->*function)(MachineType::Uint32(), backing_store,
                        WordShl(index_word, 2), value_word32, nullptr)));

  BIND(&big);
  TNode<BigInt> value_bigint = ToBigInt(CAST(context), CAST(value));
#if DEBUG
  DebugSanityCheckAtomicIndex(array, index_word32, context);
#endif
  TVARIABLE(UintPtrT, var_low);
  TVARIABLE(UintPtrT, var_high);
  BigIntToRawBytes(value_bigint, &var_low, &var_high);
  Node* high = Is64() ? nullptr : static_cast<Node*>(var_high.value());
  GotoIf(Word32Equal(instance_type, Int32Constant(FIXED_BIGINT64_ARRAY_TYPE)),
         &i64);
  GotoIf(Word32Equal(instance_type, Int32Constant(FIXED_BIGUINT64_ARRAY_TYPE)),
         &u64);
  Unreachable();

  BIND(&i64);
  // This uses Uint64() intentionally: Atomic* ops are not implemented for
  // Int64(), which is fine because the machine instructions only care
  // about words.
  Return(BigIntFromSigned64(
      (this->*function)(MachineType::Uint64(), backing_store,
                        WordShl(index_word, 3), var_low.value(), high)));

  BIND(&u64);
  Return(BigIntFromUnsigned64(
      (this->*function)(MachineType::Uint64(), backing_store,
                        WordShl(index_word, 3), var_low.value(), high)));

  // This shouldn't happen, we've already validated the type.
  BIND(&other);
  Unreachable();
#endif  // V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64
        // || V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_S390X
}

}  // namespace internal
}  // namespace v8
