// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-typed-array-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/builtins/growable-fixed-array-gen.h"
#include "src/execution/protectors.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory-inl.h"

namespace v8 {
namespace internal {

using compiler::Node;

// -----------------------------------------------------------------------------
// ES6 section 22.2 TypedArray Objects

// Sets the embedder fields to 0 for a TypedArray which is under construction.
void TypedArrayBuiltinsAssembler::SetupTypedArrayEmbedderFields(
    TNode<JSTypedArray> holder) {
  for (int offset = JSTypedArray::kHeaderSize;
       offset < JSTypedArray::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectField(holder, offset, SmiConstant(0));
  }
}

// Allocate a new ArrayBuffer and initialize it with empty properties and
// elements.
// TODO(bmeurer,v8:4153): Rename this and maybe fix up the implementation a bit.
TNode<JSArrayBuffer> TypedArrayBuiltinsAssembler::AllocateEmptyOnHeapBuffer(
    TNode<Context> context, TNode<UintPtrT> byte_length) {
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Map> map =
      CAST(LoadContextElement(native_context, Context::ARRAY_BUFFER_MAP_INDEX));
  TNode<FixedArray> empty_fixed_array = EmptyFixedArrayConstant();

  TNode<JSArrayBuffer> buffer = UncheckedCast<JSArrayBuffer>(
      Allocate(JSArrayBuffer::kSizeWithEmbedderFields));
  StoreMapNoWriteBarrier(buffer, map);
  StoreObjectFieldNoWriteBarrier(buffer, JSArray::kPropertiesOrHashOffset,
                                 empty_fixed_array);
  StoreObjectFieldNoWriteBarrier(buffer, JSArray::kElementsOffset,
                                 empty_fixed_array);
  // Setup the ArrayBuffer.
  //  - Set BitField to 0.
  //  - Set IsExternal and IsDetachable bits of BitFieldSlot.
  //  - Set the byte_length field to byte_length.
  //  - Set backing_store to null/Smi(0).
  //  - Set extension to null.
  //  - Set all embedder fields to Smi(0).
  if (FIELD_SIZE(JSArrayBuffer::kOptionalPaddingOffset) != 0) {
    DCHECK_EQ(4, FIELD_SIZE(JSArrayBuffer::kOptionalPaddingOffset));
    StoreObjectFieldNoWriteBarrier(
        buffer, JSArrayBuffer::kOptionalPaddingOffset, Int32Constant(0));
  }
  int32_t bitfield_value = (1 << JSArrayBuffer::IsExternalBit::kShift) |
                           (1 << JSArrayBuffer::IsDetachableBit::kShift);
  StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBitFieldOffset,
                                 Int32Constant(bitfield_value));

  StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kByteLengthOffset,
                                 byte_length);
  StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBackingStoreOffset,
                                 IntPtrConstant(0));
  if (V8_ARRAY_BUFFER_EXTENSION_BOOL) {
    StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kExtensionOffset,
                                   IntPtrConstant(0));
  }
  for (int offset = JSArrayBuffer::kHeaderSize;
       offset < JSArrayBuffer::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(buffer, offset, SmiConstant(0));
  }
  return buffer;
}

TF_BUILTIN(TypedArrayBaseConstructor, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  ThrowTypeError(context, MessageTemplate::kConstructAbstractClass,
                 "TypedArray");
}

// ES #sec-typedarray-constructors
TF_BUILTIN(TypedArrayConstructor, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kJSTarget));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));
  TNode<IntPtrT> argc = ChangeInt32ToIntPtr(
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount)));
  CodeStubArguments args(this, argc);
  TNode<Object> arg1 = args.GetOptionalArgumentValue(0);
  TNode<Object> arg2 = args.GetOptionalArgumentValue(1);
  TNode<Object> arg3 = args.GetOptionalArgumentValue(2);

  // If NewTarget is undefined, throw a TypeError exception.
  // All the TypedArray constructors have this as the first step:
  // https://tc39.github.io/ecma262/#sec-typedarray-constructors
  Label throwtypeerror(this, Label::kDeferred);
  GotoIf(IsUndefined(new_target), &throwtypeerror);

  TNode<Object> result = CallBuiltin(Builtins::kCreateTypedArray, context,
                                     target, new_target, arg1, arg2, arg3);
  args.PopAndReturn(result);

  BIND(&throwtypeerror);
  {
    TNode<String> name =
        CAST(CallRuntime(Runtime::kGetFunctionName, context, target));
    ThrowTypeError(context, MessageTemplate::kConstructorNotFunction, name);
  }
}

// ES6 #sec-get-%typedarray%.prototype.bytelength
TF_BUILTIN(TypedArrayPrototypeByteLength, TypedArrayBuiltinsAssembler) {
  const char* const kMethodName = "get TypedArray.prototype.byteLength";
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  // Check if the {receiver} is actually a JSTypedArray.
  ThrowIfNotInstanceType(context, receiver, JS_TYPED_ARRAY_TYPE, kMethodName);

  // Default to zero if the {receiver}s buffer was detached.
  TNode<JSArrayBuffer> receiver_buffer =
      LoadJSArrayBufferViewBuffer(CAST(receiver));
  TNode<UintPtrT> byte_length = Select<UintPtrT>(
      IsDetachedBuffer(receiver_buffer), [=] { return UintPtrConstant(0); },
      [=] { return LoadJSArrayBufferViewByteLength(CAST(receiver)); });
  Return(ChangeUintPtrToTagged(byte_length));
}

// ES6 #sec-get-%typedarray%.prototype.byteoffset
TF_BUILTIN(TypedArrayPrototypeByteOffset, TypedArrayBuiltinsAssembler) {
  const char* const kMethodName = "get TypedArray.prototype.byteOffset";
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  // Check if the {receiver} is actually a JSTypedArray.
  ThrowIfNotInstanceType(context, receiver, JS_TYPED_ARRAY_TYPE, kMethodName);

  // Default to zero if the {receiver}s buffer was detached.
  TNode<JSArrayBuffer> receiver_buffer =
      LoadJSArrayBufferViewBuffer(CAST(receiver));
  TNode<UintPtrT> byte_offset = Select<UintPtrT>(
      IsDetachedBuffer(receiver_buffer), [=] { return UintPtrConstant(0); },
      [=] { return LoadJSArrayBufferViewByteOffset(CAST(receiver)); });
  Return(ChangeUintPtrToTagged(byte_offset));
}

// ES6 #sec-get-%typedarray%.prototype.length
TF_BUILTIN(TypedArrayPrototypeLength, TypedArrayBuiltinsAssembler) {
  const char* const kMethodName = "get TypedArray.prototype.length";
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));

  // Check if the {receiver} is actually a JSTypedArray.
  ThrowIfNotInstanceType(context, receiver, JS_TYPED_ARRAY_TYPE, kMethodName);

  // Default to zero if the {receiver}s buffer was detached.
  TNode<JSArrayBuffer> receiver_buffer =
      LoadJSArrayBufferViewBuffer(CAST(receiver));
  TNode<UintPtrT> length = Select<UintPtrT>(
      IsDetachedBuffer(receiver_buffer), [=] { return UintPtrConstant(0); },
      [=] { return LoadJSTypedArrayLength(CAST(receiver)); });
  Return(ChangeUintPtrToTagged(length));
}

TNode<BoolT> TypedArrayBuiltinsAssembler::IsUint8ElementsKind(
    TNode<Int32T> kind) {
  return Word32Or(Word32Equal(kind, Int32Constant(UINT8_ELEMENTS)),
                  Word32Equal(kind, Int32Constant(UINT8_CLAMPED_ELEMENTS)));
}

TNode<BoolT> TypedArrayBuiltinsAssembler::IsBigInt64ElementsKind(
    TNode<Int32T> kind) {
  STATIC_ASSERT(BIGUINT64_ELEMENTS + 1 == BIGINT64_ELEMENTS);
  return IsElementsKindInRange(kind, BIGUINT64_ELEMENTS, BIGINT64_ELEMENTS);
}

TNode<IntPtrT> TypedArrayBuiltinsAssembler::GetTypedArrayElementSize(
    TNode<Int32T> elements_kind) {
  TVARIABLE(IntPtrT, element_size);

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind el_kind, int size, int typed_array_fun_index) {
        element_size = IntPtrConstant(size);
      });

  return element_size.value();
}

TorqueStructTypedArrayElementsInfo
TypedArrayBuiltinsAssembler::GetTypedArrayElementsInfo(
    TNode<JSTypedArray> typed_array) {
  return GetTypedArrayElementsInfo(LoadMap(typed_array));
}

TorqueStructTypedArrayElementsInfo
TypedArrayBuiltinsAssembler::GetTypedArrayElementsInfo(TNode<Map> map) {
  TNode<Int32T> elements_kind = LoadMapElementsKind(map);
  TVARIABLE(UintPtrT, var_size_log2);
  TVARIABLE(Map, var_map);
  ReadOnlyRoots roots(isolate());

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind kind, int size, int typed_array_fun_index) {
        DCHECK_GT(size, 0);
        var_size_log2 = UintPtrConstant(ElementsKindToShiftSize(kind));
      });

  return TorqueStructTypedArrayElementsInfo{var_size_log2.value(),
                                            elements_kind};
}

TNode<JSFunction> TypedArrayBuiltinsAssembler::GetDefaultConstructor(
    TNode<Context> context, TNode<JSTypedArray> exemplar) {
  TVARIABLE(IntPtrT, context_slot);
  TNode<Int32T> elements_kind = LoadElementsKind(exemplar);

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind el_kind, int size, int typed_array_function_index) {
        context_slot = IntPtrConstant(typed_array_function_index);
      });

  return CAST(
      LoadContextElement(LoadNativeContext(context), context_slot.value()));
}

TNode<JSArrayBuffer> TypedArrayBuiltinsAssembler::GetBuffer(
    TNode<Context> context, TNode<JSTypedArray> array) {
  Label call_runtime(this), done(this);
  TVARIABLE(Object, var_result);

  TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(array);
  GotoIf(IsDetachedBuffer(buffer), &call_runtime);
  TNode<RawPtrT> backing_store = LoadJSArrayBufferBackingStore(buffer);
  GotoIf(WordEqual(backing_store, IntPtrConstant(0)), &call_runtime);
  var_result = buffer;
  Goto(&done);

  BIND(&call_runtime);
  {
    var_result = CallRuntime(Runtime::kTypedArrayGetBuffer, context, array);
    Goto(&done);
  }

  BIND(&done);
  return CAST(var_result.value());
}

TNode<JSTypedArray> TypedArrayBuiltinsAssembler::ValidateTypedArray(
    TNode<Context> context, TNode<Object> obj, const char* method_name) {
  // If it is not a typed array, throw
  ThrowIfNotInstanceType(context, obj, JS_TYPED_ARRAY_TYPE, method_name);

  // If the typed array's buffer is detached, throw
  ThrowIfArrayBufferViewBufferIsDetached(context, CAST(obj), method_name);

  return CAST(obj);
}

void TypedArrayBuiltinsAssembler::CallCMemmove(TNode<RawPtrT> dest_ptr,
                                               TNode<RawPtrT> src_ptr,
                                               TNode<UintPtrT> byte_length) {
  TNode<ExternalReference> memmove =
      ExternalConstant(ExternalReference::libc_memmove_function());
  CallCFunction(memmove, MachineType::AnyTagged(),
                std::make_pair(MachineType::Pointer(), dest_ptr),
                std::make_pair(MachineType::Pointer(), src_ptr),
                std::make_pair(MachineType::UintPtr(), byte_length));
}

void TypedArrayBuiltinsAssembler::CallCMemcpy(TNode<RawPtrT> dest_ptr,
                                              TNode<RawPtrT> src_ptr,
                                              TNode<UintPtrT> byte_length) {
  TNode<ExternalReference> memcpy =
      ExternalConstant(ExternalReference::libc_memcpy_function());
  CallCFunction(memcpy, MachineType::AnyTagged(),
                std::make_pair(MachineType::Pointer(), dest_ptr),
                std::make_pair(MachineType::Pointer(), src_ptr),
                std::make_pair(MachineType::UintPtr(), byte_length));
}

void TypedArrayBuiltinsAssembler::CallCMemset(TNode<RawPtrT> dest_ptr,
                                              TNode<IntPtrT> value,
                                              TNode<UintPtrT> length) {
  TNode<ExternalReference> memset =
      ExternalConstant(ExternalReference::libc_memset_function());
  CallCFunction(memset, MachineType::AnyTagged(),
                std::make_pair(MachineType::Pointer(), dest_ptr),
                std::make_pair(MachineType::IntPtr(), value),
                std::make_pair(MachineType::UintPtr(), length));
}

void TypedArrayBuiltinsAssembler::
    CallCCopyFastNumberJSArrayElementsToTypedArray(
        TNode<Context> context, TNode<JSArray> source, TNode<JSTypedArray> dest,
        TNode<UintPtrT> source_length, TNode<UintPtrT> offset) {
  CSA_ASSERT(this,
             Word32BinaryNot(IsBigInt64ElementsKind(LoadElementsKind(dest))));
  TNode<ExternalReference> f = ExternalConstant(
      ExternalReference::copy_fast_number_jsarray_elements_to_typed_array());
  CallCFunction(f, MachineType::AnyTagged(),
                std::make_pair(MachineType::AnyTagged(), context),
                std::make_pair(MachineType::AnyTagged(), source),
                std::make_pair(MachineType::AnyTagged(), dest),
                std::make_pair(MachineType::UintPtr(), source_length),
                std::make_pair(MachineType::UintPtr(), offset));
}

void TypedArrayBuiltinsAssembler::CallCCopyTypedArrayElementsToTypedArray(
    TNode<JSTypedArray> source, TNode<JSTypedArray> dest,
    TNode<UintPtrT> source_length, TNode<UintPtrT> offset) {
  TNode<ExternalReference> f = ExternalConstant(
      ExternalReference::copy_typed_array_elements_to_typed_array());
  CallCFunction(f, MachineType::AnyTagged(),
                std::make_pair(MachineType::AnyTagged(), source),
                std::make_pair(MachineType::AnyTagged(), dest),
                std::make_pair(MachineType::UintPtr(), source_length),
                std::make_pair(MachineType::UintPtr(), offset));
}

void TypedArrayBuiltinsAssembler::CallCCopyTypedArrayElementsSlice(
    TNode<JSTypedArray> source, TNode<JSTypedArray> dest, TNode<UintPtrT> start,
    TNode<UintPtrT> end) {
  TNode<ExternalReference> f =
      ExternalConstant(ExternalReference::copy_typed_array_elements_slice());
  CallCFunction(f, MachineType::AnyTagged(),
                std::make_pair(MachineType::AnyTagged(), source),
                std::make_pair(MachineType::AnyTagged(), dest),
                std::make_pair(MachineType::UintPtr(), start),
                std::make_pair(MachineType::UintPtr(), end));
}

void TypedArrayBuiltinsAssembler::DispatchTypedArrayByElementsKind(
    TNode<Word32T> elements_kind, const TypedArraySwitchCase& case_function) {
  Label next(this), if_unknown_type(this, Label::kDeferred);

  int32_t elements_kinds[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) TYPE##_ELEMENTS,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) Label if_##type##array(this);
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  Label* elements_kind_labels[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) &if_##type##array,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };
  STATIC_ASSERT(arraysize(elements_kinds) == arraysize(elements_kind_labels));

  Switch(elements_kind, &if_unknown_type, elements_kinds, elements_kind_labels,
         arraysize(elements_kinds));

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype)   \
  BIND(&if_##type##array);                          \
  {                                                 \
    case_function(TYPE##_ELEMENTS, sizeof(ctype),   \
                  Context::TYPE##_ARRAY_FUN_INDEX); \
    Goto(&next);                                    \
  }
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  BIND(&if_unknown_type);
  Unreachable();

  BIND(&next);
}

void TypedArrayBuiltinsAssembler::SetJSTypedArrayOnHeapDataPtr(
    TNode<JSTypedArray> holder, TNode<ByteArray> base, TNode<UintPtrT> offset) {
  offset = UintPtrAdd(UintPtrConstant(ByteArray::kHeaderSize - kHeapObjectTag),
                      offset);
  if (COMPRESS_POINTERS_BOOL) {
    TNode<IntPtrT> full_base = Signed(BitcastTaggedToWord(base));
    TNode<Int32T> compressed_base = TruncateIntPtrToInt32(full_base);
    // TODO(v8:9706): Add a way to directly use kRootRegister value.
    TNode<IntPtrT> isolate_root =
        IntPtrSub(full_base, Signed(ChangeUint32ToWord(compressed_base)));
    // Add JSTypedArray::ExternalPointerCompensationForOnHeapArray() to offset.
    DCHECK_EQ(
        isolate()->isolate_root(),
        JSTypedArray::ExternalPointerCompensationForOnHeapArray(isolate()));
    // See JSTypedArray::SetOnHeapDataPtr() for details.
    offset = Unsigned(IntPtrAdd(offset, isolate_root));
  }

  StoreObjectField(holder, JSTypedArray::kBasePointerOffset, base);
  StoreObjectFieldNoWriteBarrier<UintPtrT>(
      holder, JSTypedArray::kExternalPointerOffset, offset);
}

void TypedArrayBuiltinsAssembler::SetJSTypedArrayOffHeapDataPtr(
    TNode<JSTypedArray> holder, TNode<RawPtrT> base, TNode<UintPtrT> offset) {
  StoreObjectFieldNoWriteBarrier(holder, JSTypedArray::kBasePointerOffset,
                                 SmiConstant(0));

  base = RawPtrAdd(base, Signed(offset));
  StoreObjectFieldNoWriteBarrier<RawPtrT>(
      holder, JSTypedArray::kExternalPointerOffset, base);
}

void TypedArrayBuiltinsAssembler::StoreJSTypedArrayElementFromNumeric(
    TNode<Context> context, TNode<JSTypedArray> typed_array,
    TNode<UintPtrT> index, TNode<Numeric> value, ElementsKind elements_kind) {
  TNode<RawPtrT> data_ptr = LoadJSTypedArrayDataPtr(typed_array);
  switch (elements_kind) {
    case UINT8_ELEMENTS:
    case UINT8_CLAMPED_ELEMENTS:
    case INT8_ELEMENTS:
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
      StoreElement(data_ptr, elements_kind, index, SmiToInt32(CAST(value)));
      break;
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
      StoreElement(data_ptr, elements_kind, index,
                   TruncateTaggedToWord32(context, value));
      break;
    case FLOAT32_ELEMENTS:
      StoreElement(data_ptr, elements_kind, index,
                   TruncateFloat64ToFloat32(LoadHeapNumberValue(CAST(value))));
      break;
    case FLOAT64_ELEMENTS:
      StoreElement(data_ptr, elements_kind, index,
                   LoadHeapNumberValue(CAST(value)));
      break;
    case BIGUINT64_ELEMENTS:
    case BIGINT64_ELEMENTS:
      StoreElement(data_ptr, elements_kind, index,
                   UncheckedCast<BigInt>(value));
      break;
    default:
      UNREACHABLE();
  }
}

void TypedArrayBuiltinsAssembler::StoreJSTypedArrayElementFromTagged(
    TNode<Context> context, TNode<JSTypedArray> typed_array,
    TNode<UintPtrT> index, TNode<Object> value, ElementsKind elements_kind,
    Label* if_detached) {
  // |prepared_value| is Word32T or Float64T or Float32T or BigInt.
  Node* prepared_value =
      PrepareValueForWriteToTypedArray(value, elements_kind, context);

  // ToNumber/ToBigInt may execute JavaScript code, which could detach
  // the array's buffer.
  TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(typed_array);
  GotoIf(IsDetachedBuffer(buffer), if_detached);

  TNode<RawPtrT> data_ptr = LoadJSTypedArrayDataPtr(typed_array);
  StoreElement(data_ptr, elements_kind, index, prepared_value);
}

// ES #sec-get-%typedarray%.prototype-@@tostringtag
TF_BUILTIN(TypedArrayPrototypeToStringTag, TypedArrayBuiltinsAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Label if_receiverisheapobject(this), return_undefined(this);
  Branch(TaggedIsSmi(receiver), &return_undefined, &if_receiverisheapobject);

  // Dispatch on the elements kind, offset by
  // FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND.
  size_t const kTypedElementsKindCount = LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND -
                                         FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND +
                                         1;
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  Label return_##type##array(this);               \
  BIND(&return_##type##array);                    \
  Return(StringConstant(#Type "Array"));
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  Label* elements_kind_labels[kTypedElementsKindCount] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) &return_##type##array,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };
  int32_t elements_kinds[kTypedElementsKindCount] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  TYPE##_ELEMENTS - FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };

  // We offset the dispatch by FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND, so that
  // this can be turned into a non-sparse table switch for ideal performance.
  BIND(&if_receiverisheapobject);
  TNode<HeapObject> receiver_heap_object = CAST(receiver);
  TNode<Int32T> elements_kind =
      Int32Sub(LoadElementsKind(receiver_heap_object),
               Int32Constant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND));
  Switch(elements_kind, &return_undefined, elements_kinds, elements_kind_labels,
         kTypedElementsKindCount);

  BIND(&return_undefined);
  Return(UndefinedConstant());
}

void TypedArrayBuiltinsAssembler::GenerateTypedArrayPrototypeIterationMethod(
    TNode<Context> context, TNode<Object> receiver, const char* method_name,
    IterationKind kind) {
  Label throw_bad_receiver(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &throw_bad_receiver);
  GotoIfNot(IsJSTypedArray(CAST(receiver)), &throw_bad_receiver);

  // Check if the {receiver}'s JSArrayBuffer was detached.
  ThrowIfArrayBufferViewBufferIsDetached(context, CAST(receiver), method_name);

  Return(CreateArrayIterator(context, receiver, kind));

  BIND(&throw_bad_receiver);
  ThrowTypeError(context, MessageTemplate::kNotTypedArray, method_name);
}

// ES #sec-%typedarray%.prototype.values
TF_BUILTIN(TypedArrayPrototypeValues, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.values()",
                                             IterationKind::kValues);
}

// ES #sec-%typedarray%.prototype.entries
TF_BUILTIN(TypedArrayPrototypeEntries, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.entries()",
                                             IterationKind::kEntries);
}

// ES #sec-%typedarray%.prototype.keys
TF_BUILTIN(TypedArrayPrototypeKeys, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  GenerateTypedArrayPrototypeIterationMethod(
      context, receiver, "%TypedArray%.prototype.keys()", IterationKind::kKeys);
}

}  // namespace internal
}  // namespace v8
