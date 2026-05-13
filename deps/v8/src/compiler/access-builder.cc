// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"

#include "src/codegen/machine-type.h"
#include "src/compiler/property-access-builder.h"
#include "src/compiler/type-cache.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/handles/handles-inl.h"
#include "src/objects/arguments.h"
#include "src/objects/contexts.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-collection.h"
#include "src/objects/js-generator.h"
#include "src/objects/js-objects.h"
#include "src/objects/objects-inl.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/source-text-module.h"
#include "src/objects/tagged-field.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
FieldAccess AccessBuilder::ForExternalIntPtr() {
  FieldAccess access = {kUntaggedBase,       0,
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::Any(),         MachineType::IntPtr(),
                        kNoWriteBarrier,     "ExternalIntPtr"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMap(WriteBarrierKind write_barrier) {
  FieldAccess access = {kTaggedBase,           offsetof(HeapObject, map_),
                        MaybeHandle<Name>(),   OptionalMapRef(),
                        Type::OtherInternal(), MachineType::MapInHeader(),
                        write_barrier,         "Map"};
  return access;
}

// static
FieldAccess AccessBuilder::ForHeapNumberValue() {
  FieldAccess access = {kTaggedBase,
                        offsetof(HeapNumber, value_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kFloat64,
                        MachineType::Float64(),
                        kNoWriteBarrier,
                        "HeapNumberValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextCellState() {
  FieldAccess access = {
      kTaggedBase,      offsetof(ContextCell, state_), MaybeHandle<Name>(),
      OptionalMapRef(), TypeCache::Get()->kInt32,      MachineType::Int32(),
      kNoWriteBarrier,  "ForContextCellState"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextCellTaggedValue() {
  FieldAccess access = {
      kTaggedBase,         offsetof(ContextCell, tagged_value_),
      MaybeHandle<Name>(), OptionalMapRef(),
      Type::Any(),         MachineType::AnyTagged(),
      kFullWriteBarrier,   "ForContextCellTaggedValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextCellInt32Value() {
  FieldAccess access = {kTaggedBase,
                        offsetof(ContextCell, double_value_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "ForContextCellInt32Value"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextCellFloat64Value() {
  FieldAccess access = {kTaggedBase,
                        offsetof(ContextCell, double_value_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kFloat64,
                        MachineType::Float64(),
                        kNoWriteBarrier,
                        "ContextCellFloat64Value"};
  return access;
}

// static
FieldAccess AccessBuilder::ForHeapNumberOrOddballValue() {
  STATIC_ASSERT_FIELD_OFFSETS_EQUAL(offsetof(HeapNumber, value_),
                                    offsetof(Oddball, to_number_raw_));
  return ForHeapNumberValue();
}

// static
FieldAccess AccessBuilder::ForBigIntBitfield() {
  FieldAccess access = {kTaggedBase,
                        offsetof(BigInt, bitfield_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kInt32,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "BigIntBitfield"};
  return access;
}

#ifdef BIGINT_NEEDS_PADDING
// static
FieldAccess AccessBuilder::ForBigIntOptionalPadding() {
  static_assert(arraysize(BigInt::padding_) == sizeof(uint32_t));
  FieldAccess access = {
      kTaggedBase,      offsetof(BigInt, padding_), MaybeHandle<Name>(),
      OptionalMapRef(), TypeCache::Get()->kInt32,   MachineType::Uint32(),
      kNoWriteBarrier,  "BigIntOptionalPadding"};
  return access;
}
#endif

// static
FieldAccess AccessBuilder::ForBigIntLeastSignificantDigit64() {
  DCHECK_EQ(BigInt::SizeFor(1) - BigInt::SizeFor(0), 8);
  FieldAccess access = {
      kTaggedBase,      OFFSET_OF_DATA_START(BigInt),   MaybeHandle<Name>(),
      OptionalMapRef(), TypeCache::Get()->kBigUint64,   MachineType::Uint64(),
      kNoWriteBarrier,  "BigIntLeastSignificantDigit64"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectPropertiesOrHash() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSObject, properties_or_hash_),
      MaybeHandle<Name>(), OptionalMapRef(),
      Type::Any(),         MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSObjectPropertiesOrHash"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectPropertiesOrHashKnownPointer() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSObject, properties_or_hash_),
      MaybeHandle<Name>(), OptionalMapRef(),
      Type::Any(),         MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSObjectPropertiesOrHashKnownPointer"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectElements() {
  FieldAccess access = {kTaggedBase,          offsetof(JSObject, elements_),
                        MaybeHandle<Name>(),  OptionalMapRef(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSObjectElements"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectInObjectProperty(
    MapRef map, int index, MachineType machine_type) {
  int const offset = map.GetInObjectPropertyOffset(index);
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::NonInternal(), machine_type,
                        kFullWriteBarrier,   "JSObjectInObjectProperty"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectOffset(
    int offset, WriteBarrierKind write_barrier_kind) {
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        write_barrier_kind,  "JSObjectOffset"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionTable() {
  FieldAccess access = {kTaggedBase,           offsetof(JSCollection, table_),
                        MaybeHandle<Name>(),   OptionalMapRef(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "JSCollectionTable"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSWeakCollectionTable() {
  FieldAccess access = {
      kTaggedBase,           offsetof(JSWeakCollection, table_),
      MaybeHandle<Name>(),   OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "JSWeakCollectionTable"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionIteratorTable() {
  FieldAccess access = {
      kTaggedBase,           offsetof(JSCollectionIterator, table_),
      MaybeHandle<Name>(),   OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "JSCollectionIteratorTable"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionIteratorIndex() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSCollectionIterator, index_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kFixedArrayLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "JSCollectionIteratorIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSExternalObjectValue() {
  FieldAccess access = {
      kTaggedBase,
      offsetof(JSExternalObject, value_),
      MaybeHandle<Name>(),
      OptionalMapRef(),
      Type::ExternalPointer(),
      MachineType::Pointer(),
      kNoWriteBarrier,
      "JSExternalObjectValue",
      ConstFieldInfo::None(),
      false,
      kFastApiExternalTypeTag,
  };
  return access;
}

#ifdef V8_ENABLE_SANDBOX
// static
FieldAccess AccessBuilder::ForJSExternalObjectPointerHandle() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSExternalObject, value_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kUint32,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "JSExternalObjectPointerHandle"};
  return access;
}
#endif

// static
FieldAccess AccessBuilder::ForJSFunctionPrototypeOrInitialMap() {
  FieldAccess access = {
      kTaggedBase,
      offsetof(JSFunctionWithPrototype, prototype_or_initial_map_),
      MaybeHandle<Name>(),
      OptionalMapRef(),
      Type::Any(),
      MachineType::TaggedPointer(),
      kPointerWriteBarrier,
      "JSFunctionPrototypeOrInitialMap"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionContext() {
  FieldAccess access = {kTaggedBase,          offsetof(JSFunction, context_),
                        MaybeHandle<Name>(),  OptionalMapRef(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSFunctionContext"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionSharedFunctionInfo() {
  FieldAccess access = {
      kTaggedBase,           offsetof(JSFunction, shared_function_info_),
      Handle<Name>(),        OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "JSFunctionSharedFunctionInfo"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionFeedbackCell() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSFunction, feedback_cell_),
      Handle<Name>(),       OptionalMapRef(),
      Type::Internal(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSFunctionFeedbackCell"};
  // The feedback cell is only set when the JSFunction is allocated, or via
  // LiveEdit, but that doesn't concern optimized code, so treat it as const.
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionDispatchHandleNoWriteBarrier() {
  // We currently don't require write barriers when writing dispatch handles of
  // JSFunctions because they are loaded from the function's FeedbackCell and
  // so must already be reachable. If this ever changes, we'll need to
  // implement write barrier support for dispatch handles in generated code.
  FieldAccess access = {kTaggedBase,
                        offsetof(JSFunction, dispatch_handle_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "JSFunctionDispatchHandle"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundTargetFunction() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSBoundFunction, bound_target_function_),
      Handle<Name>(),       OptionalMapRef(),
      Type::Callable(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSBoundFunctionBoundTargetFunction"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundThis() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSBoundFunction, bound_this_),
      Handle<Name>(),      OptionalMapRef(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSBoundFunctionBoundThis"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundArguments() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSBoundFunction, bound_arguments_),
      Handle<Name>(),       OptionalMapRef(),
      Type::Internal(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSBoundFunctionBoundArguments"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContext() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSGeneratorObject, context_),
      Handle<Name>(),       OptionalMapRef(),
      Type::Internal(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSGeneratorObjectContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectFunction() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSGeneratorObject, function_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        Type::CallableFunction(),
                        MachineType::TaggedPointer(),
                        kPointerWriteBarrier,
                        "JSGeneratorObjectFunction"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectReceiver() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSGeneratorObject, receiver_),
      Handle<Name>(),       OptionalMapRef(),
      Type::Internal(),     MachineType::AnyTagged(),
      kPointerWriteBarrier, "JSGeneratorObjectReceiver"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContinuation() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSGeneratorObject, continuation_),
      Handle<Name>(),      OptionalMapRef(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "JSGeneratorObjectContinuation"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectInputOrDebugPos() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSGeneratorObject, input_or_debug_pos_),
      Handle<Name>(),      OptionalMapRef(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSGeneratorObjectInputOrDebugPos"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectParametersAndRegisters() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSGeneratorObject, parameters_and_registers_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        Type::Internal(),
                        MachineType::TaggedPointer(),
                        kPointerWriteBarrier,
                        "JSGeneratorObjectParametersAndRegisters"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectResumeMode() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSGeneratorObject, resume_mode_),
      Handle<Name>(),      OptionalMapRef(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "JSGeneratorObjectResumeMode"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncFunctionObjectPromise() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSAsyncFunctionObject, promise_),
      Handle<Name>(),       OptionalMapRef(),
      Type::OtherObject(),  MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSAsyncFunctionObjectPromise"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncFunctionObjectAwaitResolveClosure() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSAsyncFunctionObject, await_resolve_closure_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        Type::Any(),
                        MachineType::AnyTagged(),
                        kFullWriteBarrier,
                        "JSAsyncFunctionObjectAwaitResolveClosure"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncFunctionObjectAwaitRejectClosure() {
  FieldAccess access = {
      kTaggedBase,       offsetof(JSAsyncFunctionObject, await_reject_closure_),
      Handle<Name>(),    OptionalMapRef(),
      Type::Any(),       MachineType::AnyTagged(),
      kFullWriteBarrier, "JSAsyncFunctionObjectAwaitRejectClosure"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncGeneratorObjectQueue() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSAsyncGeneratorObject, queue_),
      Handle<Name>(),      OptionalMapRef(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSAsyncGeneratorObjectQueue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncGeneratorObjectIsAwaiting() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSAsyncGeneratorObject, is_awaiting_),
      Handle<Name>(),      OptionalMapRef(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "JSAsyncGeneratorObjectIsAwaiting"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayLength(ElementsKind elements_kind) {
  TypeCache const* type_cache = TypeCache::Get();
  FieldAccess access = AccessBuilder::ForJSArrayLength();
  if (IsDoubleElementsKind(elements_kind)) {
    access.type = type_cache->kFixedDoubleArrayLengthType;
    access.machine_type = MachineType::TaggedSigned();
    access.write_barrier_kind = kNoWriteBarrier;
  } else if (IsFastElementsKind(elements_kind)) {
    access.type = type_cache->kFixedArrayLengthType;
    access.machine_type = MachineType::TaggedSigned();
    access.write_barrier_kind = kNoWriteBarrier;
  }
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayLength() {
  TypeCache const* type_cache = TypeCache::Get();
  return {kTaggedBase,
          offsetof(JSArray, length_),
          Handle<Name>(),
          OptionalMapRef(),
          type_cache->kJSArrayLengthType,
          MachineType::AnyTagged(),
          kFullWriteBarrier,
          "JSArrayLength"};
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferBitField() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSArrayBuffer, bit_field_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kUint8,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "JSArrayBufferBitField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferByteLength() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSArrayBuffer, raw_byte_length_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kJSArrayBufferByteLengthType,
                        MachineType::UintPtr(),
                        kNoWriteBarrier,
                        "JSArrayBufferByteLength"};
#ifdef V8_ENABLE_SANDBOX
  access.is_bounded_size_access = true;
#endif
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewBuffer() {
  FieldAccess access = {
      kTaggedBase,           offsetof(JSArrayBufferView, buffer_),
      MaybeHandle<Name>(),   OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "JSArrayBufferViewBuffer"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewByteLength() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSArrayBufferView, raw_byte_length_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kJSArrayBufferViewByteLengthType,
                        MachineType::UintPtr(),
                        kNoWriteBarrier,
                        "JSArrayBufferViewByteLength"};
#ifdef V8_ENABLE_SANDBOX
  access.is_bounded_size_access = true;
#endif
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewByteOffset() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSArrayBufferView, raw_byte_offset_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kJSArrayBufferViewByteOffsetType,
                        MachineType::UintPtr(),
                        kNoWriteBarrier,
                        "JSArrayBufferViewByteOffset"};
#ifdef V8_ENABLE_SANDBOX
  access.is_bounded_size_access = true;
#endif
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewBitField() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSArrayBufferView, bit_field_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kUint32,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "JSArrayBufferViewBitField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayBasePointer() {
  FieldAccess access = {
      kTaggedBase,           offsetof(JSTypedArray, base_pointer_),
      MaybeHandle<Name>(),   OptionalMapRef(),
      Type::OtherInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,     "JSTypedArrayBasePointer"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayExternalPointer() {
  FieldAccess access = {
      kTaggedBase,
      offsetof(JSTypedArray, external_pointer_),
      MaybeHandle<Name>(),
      OptionalMapRef(),
#ifdef V8_ENABLE_SANDBOX
      Type::SandboxedPointer(),
      MachineType::SandboxedPointer(),
#else
      Type::ExternalPointer(),
      MachineType::Pointer(),
#endif
      kNoWriteBarrier,
      "JSTypedArrayExternalPointer",
      ConstFieldInfo::None(),
      false,
  };
  return access;
}

// static
FieldAccess AccessBuilder::ForJSDataViewDataPointer() {
  FieldAccess access = {
      kTaggedBase,
      offsetof(JSDataViewOrRabGsabDataView, data_pointer_),
      MaybeHandle<Name>(),
      OptionalMapRef(),
#ifdef V8_ENABLE_SANDBOX
      Type::SandboxedPointer(),
      MachineType::SandboxedPointer(),
#else
      Type::ExternalPointer(),
      MachineType::Pointer(),
#endif
      kNoWriteBarrier,
      "JSDataViewDataPointer",
      ConstFieldInfo::None(),
      false,
  };
  return access;
}

// static
FieldAccess AccessBuilder::ForJSDateValue() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSDate, value_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kJSDateValueType,
                        MachineType::Float64(),
                        kNoWriteBarrier,
                        "JSDateValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSDateField(JSDate::FieldIndex index) {
  DCHECK_LT(index, JSDate::kFirstUncachedField);
  FieldAccess access = {
      kTaggedBase,
      static_cast<int>(offsetof(JSDate, year_)) + index * kTaggedSize,
      MaybeHandle<Name>(),
      OptionalMapRef(),
      TypeCache::Get()->kJSDateFields[index],
      MachineType::AnyTagged(),
      kFullWriteBarrier,
      "JSDateField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSIteratorResultDone() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kDoneOffset,
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSIteratorResultDone"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSIteratorResultValue() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kValueOffset,
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSIteratorResultValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSPrimitiveWrapperValue() {
  FieldAccess access = {
      kTaggedBase,         offsetof(JSPrimitiveWrapper, value_),
      MaybeHandle<Name>(), OptionalMapRef(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSPrimitiveWrapperValue"};
  return access;
}

#ifdef V8_ENABLE_SANDBOX
// static
FieldAccess AccessBuilder::ForJSRegExpData() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSRegExp, data_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        Type::OtherInternal(),
                        MachineType::IndirectPointer(),
                        kIndirectPointerWriteBarrier,
                        "JSRegExpData",
                        ConstFieldInfo::None(),
                        false,
                        kExternalPointerNullTag,
                        kRegExpDataIndirectPointerTag,
                        false,
                        false};
  return access;
}
#else
// static
FieldAccess AccessBuilder::ForJSRegExpData() {
  FieldAccess access = {kTaggedBase,           offsetof(JSRegExp, data_),
                        MaybeHandle<Name>(),   OptionalMapRef(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "JSRegExpData"};
  return access;
}
#endif  // V8_ENABLE_SANDBOX

// static
FieldAccess AccessBuilder::ForJSRegExpFlags() {
  FieldAccess access = {kTaggedBase,         offsetof(JSRegExp, flags_),
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSRegExpFlags"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpLastIndex() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kLastIndexOffset,
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSRegExpLastIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedArrayLengthLegacy() {
  FieldAccess access = {
      kTaggedBase, offsetof(FixedArray, length_), MaybeHandle<Name>(),
      OptionalMapRef(), TypeCache::Get()->kFixedArrayLengthType,
#if TAGGED_SIZE_8_BYTES && !V8_TARGET_BIG_ENDIAN
      // TF escape analysis expects all slots to be aligned to kTaggedSize,
      // therefore we store length+padding as a single 64-bit value when pointer
      // compression is disabled on 64-bit little-endian architectures.
      // On big-endian, we use Uint32 to access the uint32_t
      // length_ field directly.
      MachineType::Uint64(),
#else
      MachineType::Uint32(),
#endif  // TAGGED_SIZE_8_BYTES && !V8_TARGET_BIG_ENDIAN
      kNoWriteBarrier, "FixedArrayLength"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedArrayLength() {
  FieldAccess access = {kTaggedBase,
                        offsetof(FixedArray, length_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kFixedArrayLengthType,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "FixedArrayLength"};
  access.is_immutable = true;
  return access;
}

#if TAGGED_SIZE_8_BYTES
// static
FieldAccess AccessBuilder::ForFixedArrayLengthPadding() {
  FieldAccess access = {kTaggedBase,
                        offsetof(FixedArray, length_) + sizeof(uint32_t),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kSingletonZero,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "FixedArrayLengthPadding"};
  access.is_immutable = true;
  return access;
}
#endif

// static
FieldAccess AccessBuilder::ForContextLength() {
  FieldAccess access = {kTaggedBase,
                        offsetof(Context, length_),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kContextLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "ContextLength"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForWeakFixedArrayLength() {
  FieldAccess access = {
      kTaggedBase, offsetof(WeakFixedArray, length_), MaybeHandle<Name>(),
      OptionalMapRef(), TypeCache::Get()->kWeakFixedArrayLengthType,
#if TAGGED_SIZE_8_BYTES && !V8_TARGET_BIG_ENDIAN
      // TF escape analysis expects all slots to be aligned to kTaggedSize,
      // therefore we store length+padding as a single 64-bit value when pointer
      // compression is disabled on 64-bit little-endian architectures.
      // On big-endian, we use Uint32 to access the uint32_t
      // length_ field directly.
      MachineType::Uint64(),
#else
      MachineType::Uint32(),
#endif  // TAGGED_SIZE_8_BYTES && !V8_TARGET_BIG_ENDIAN
      kNoWriteBarrier, "WeakFixedArrayLength"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForSloppyArgumentsElementsContext() {
  FieldAccess access = {
      kTaggedBase,          offsetof(SloppyArgumentsElements, context_),
      MaybeHandle<Name>(),  OptionalMapRef(),
      Type::Any(),          MachineType::TaggedPointer(),
      kPointerWriteBarrier, "SloppyArgumentsElementsContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForSloppyArgumentsElementsArguments() {
  FieldAccess access = {
      kTaggedBase,          offsetof(SloppyArgumentsElements, arguments_),
      MaybeHandle<Name>(),  OptionalMapRef(),
      Type::Any(),          MachineType::TaggedPointer(),
      kPointerWriteBarrier, "SloppyArgumentsElementsArguments"};
  return access;
}

// static
FieldAccess AccessBuilder::ForPropertyArrayLengthAndHash() {
  FieldAccess access = {
      kTaggedBase,         offsetof(PropertyArray, length_and_hash_),
      MaybeHandle<Name>(), OptionalMapRef(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "PropertyArrayLengthAndHash"};
  return access;
}

// static
FieldAccess AccessBuilder::ForDescriptorArrayEnumCache() {
  FieldAccess access = {
      kTaggedBase,           offsetof(DescriptorArray, enum_cache_),
      Handle<Name>(),        OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "DescriptorArrayEnumCache"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField() {
  FieldAccess access = {kTaggedBase,
                        offsetof(Map, bit_field_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kUint8,
                        MachineType::Uint8(),
                        kNoWriteBarrier,
                        "MapBitField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField2() {
  FieldAccess access = {kTaggedBase,
                        offsetof(Map, bit_field2_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kUint8,
                        MachineType::Uint8(),
                        kNoWriteBarrier,
                        "MapBitField2"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField3() {
  FieldAccess access = {kTaggedBase,
                        offsetof(Map, bit_field3_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "MapBitField3"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapDescriptors() {
  FieldAccess access = {
      kTaggedBase,           offsetof(Map, instance_descriptors_),
      Handle<Name>(),        OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "MapDescriptors"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapInstanceType() {
  FieldAccess access = {kTaggedBase,
                        offsetof(Map, instance_type_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kUint16,
                        MachineType::Uint16(),
                        kNoWriteBarrier,
                        "MapInstanceType"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForMapPrototype() {
  FieldAccess access = {kTaggedBase,          offsetof(Map, prototype_),
                        Handle<Name>(),       OptionalMapRef(),
                        Type::Any(),          MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "MapPrototype"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapNativeContext() {
  FieldAccess access = {
      kTaggedBase,
      offsetof(Map, constructor_or_back_pointer_or_native_context_),
      Handle<Name>(),
      OptionalMapRef(),
      Type::Any(),
      MachineType::TaggedPointer(),
      kPointerWriteBarrier,
      "MapNativeContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularExports() {
  FieldAccess access = {
      kTaggedBase,           offsetof(SourceTextModule, regular_exports_),
      Handle<Name>(),        OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "ModuleRegularExports"};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularImports() {
  FieldAccess access = {
      kTaggedBase,           offsetof(SourceTextModule, regular_imports_),
      Handle<Name>(),        OptionalMapRef(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "ModuleRegularImports"};
  return access;
}

// static
FieldAccess AccessBuilder::ForNameRawHashField() {
  FieldAccess access = {kTaggedBase,        offsetof(Name, raw_hash_field_),
                        Handle<Name>(),     OptionalMapRef(),
                        Type::Unsigned32(), MachineType::Uint32(),
                        kNoWriteBarrier,    "NameRawHashField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForStringLength() {
  FieldAccess access = {kTaggedBase,
                        offsetof(String, length_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kStringLengthType,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "StringLength"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringFirst() {
  FieldAccess access = {kTaggedBase,          offsetof(ConsString, first_),
                        Handle<Name>(),       OptionalMapRef(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "ConsStringFirst"};
  // Not immutable since flattening can mutate.
  access.is_immutable = false;
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringSecond() {
  FieldAccess access = {kTaggedBase,          offsetof(ConsString, second_),
                        Handle<Name>(),       OptionalMapRef(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "ConsStringSecond"};
  // Not immutable since flattening can mutate.
  access.is_immutable = false;
  return access;
}

// static
FieldAccess AccessBuilder::ForThinStringActual() {
  FieldAccess access = {kTaggedBase,          offsetof(ThinString, actual_),
                        Handle<Name>(),       OptionalMapRef(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "ThinStringActual"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringOffset() {
  FieldAccess access = {kTaggedBase,         offsetof(SlicedString, offset_),
                        Handle<Name>(),      OptionalMapRef(),
                        Type::SignedSmall(), MachineType::TaggedSigned(),
                        kNoWriteBarrier,     "SlicedStringOffset"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringParent() {
  FieldAccess access = {kTaggedBase,          offsetof(SlicedString, parent_),
                        Handle<Name>(),       OptionalMapRef(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "SlicedStringParent"};
  access.is_immutable = true;
  return access;
}

// static
FieldAccess AccessBuilder::ForExternalStringResourceData() {
  FieldAccess access = {
      kTaggedBase,
      offsetof(ExternalString, resource_data_),
      Handle<Name>(),
      OptionalMapRef(),
      Type::ExternalPointer(),
      MachineType::Pointer(),
      kNoWriteBarrier,
      "ExternalStringResourceData",
      ConstFieldInfo::None(),
      false,
      kExternalStringResourceDataTag,
  };
  return access;
}

// static
ElementAccess AccessBuilder::ForSeqOneByteStringCharacter() {
  ElementAccess access = {kTaggedBase, OFFSET_OF_DATA_START(SeqOneByteString),
                          TypeCache::Get()->kUint8, MachineType::Uint8(),
                          kNoWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForSeqTwoByteStringCharacter() {
  ElementAccess access = {kTaggedBase, OFFSET_OF_DATA_START(SeqTwoByteString),
                          TypeCache::Get()->kUint16, MachineType::Uint16(),
                          kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorIteratedObject() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSArrayIterator, iterated_object_),
      Handle<Name>(),       OptionalMapRef(),
      Type::Receiver(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSArrayIteratorIteratedObject"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorNextIndex() {
  // In generic case, cap to 2^53-1 (per ToLength() in spec) via
  // kPositiveSafeInteger
  FieldAccess access = {kTaggedBase,
                        offsetof(JSArrayIterator, next_index_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kPositiveSafeInteger,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier,
                        "JSArrayIteratorNextIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorKind() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSArrayIterator, kind_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kJSArrayIteratorKindType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "JSArrayIteratorKind"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSStringIteratorString() {
  FieldAccess access = {
      kTaggedBase,          offsetof(JSStringIterator, string_),
      Handle<Name>(),       OptionalMapRef(),
      Type::String(),       MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSStringIteratorString"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSStringIteratorIndex() {
  FieldAccess access = {kTaggedBase,
                        offsetof(JSStringIterator, index_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kStringLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "JSStringIteratorIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForArgumentsLength() {
  constexpr int offset = JSStrictArgumentsObject::kLengthOffset;
  static_assert(offset == JSSloppyArgumentsObject::kLengthOffset);
  FieldAccess access = {kTaggedBase,         offset,
                        Handle<Name>(),      OptionalMapRef(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "ArgumentsLength"};
  return access;
}

// static
FieldAccess AccessBuilder::ForArgumentsCallee() {
  FieldAccess access = {
      kTaggedBase,         JSSloppyArgumentsObject::kCalleeOffset,
      Handle<Name>(),      OptionalMapRef(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "ArgumentsCallee"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedArraySlot(
    size_t index, WriteBarrierKind write_barrier_kind) {
  int offset = FixedArray::OffsetOfElementAt(static_cast<int>(index));
  FieldAccess access = {kTaggedBase,        offset,
                        Handle<Name>(),     OptionalMapRef(),
                        Type::Any(),        MachineType::AnyTagged(),
                        write_barrier_kind, "FixedArraySlot"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorSlot(int index) {
  int offset = FeedbackVector::OffsetOfElementAt(index);
  FieldAccess access = {kTaggedBase,       offset,
                        Handle<Name>(),    OptionalMapRef(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, "FeedbackVectorSlot"};
  return access;
}

// static
FieldAccess AccessBuilder::ForPropertyArraySlot(int index,
                                                Representation representation,
                                                bool can_optimize_smis) {
  int offset = PropertyArray::OffsetOfElementAt(index);
  MachineType machine_type;
  WriteBarrierKind write_barrier;
  switch (representation.kind()) {
    case Representation::kDouble:
    case Representation::kHeapObject:
      machine_type = MachineType::TaggedPointer();
      write_barrier = kPointerWriteBarrier;
      break;
    case Representation::kSmi:
      if (can_optimize_smis) {
        machine_type = MachineType::TaggedSigned();
        write_barrier = kNoWriteBarrier;
        break;
      }
      // If {can_optimize_smis} is false, we just return a regular AnyTagged
      // access with full write barrier. In general, it's of course better to
      // use the more precise MachineType/WriteBarrier, but this can lead to
      // inconsistencies in unreachable code, which Turbofan doesn't handle
      // well.
      [[fallthrough]];
    case Representation::kNone:
    case Representation::kTagged:
    case Representation::kWasmValue:
    case Representation::kNumRepresentations:
      machine_type = MachineType::AnyTagged();
      write_barrier = kFullWriteBarrier;
      break;
  }
  FieldAccess access = {
      kTaggedBase, offset,       Handle<Name>(), OptionalMapRef(),
      Type::Any(), machine_type, write_barrier,  "PropertyArraySlot"};
  return access;
}

// static
FieldAccess AccessBuilder::ForWeakFixedArraySlot(int index) {
  int offset = WeakFixedArray::OffsetOfElementAt(index);
  FieldAccess access = {kTaggedBase,       offset,
                        Handle<Name>(),    OptionalMapRef(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, "WeakFixedArraySlot"};
  return access;
}
// static
FieldAccess AccessBuilder::ForCellValue() {
  FieldAccess access = {kTaggedBase,       offsetof(Cell, maybe_value_),
                        Handle<Name>(),    OptionalMapRef(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, "CellValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForScopeInfoFlags() {
  FieldAccess access = {kTaggedBase,         offsetof(ScopeInfo, flags_),
                        MaybeHandle<Name>(), OptionalMapRef(),
                        Type::Unsigned32(),  MachineType::Uint32(),
                        kNoWriteBarrier,     "ScopeInfoFlags"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextSlot(size_t index) {
  int offset = Context::OffsetOfElementAt(static_cast<int>(index));
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase,       offset,
                        Handle<Name>(),    OptionalMapRef(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, "ContextSlot"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextSlotKnownPointer(size_t index) {
  int offset = Context::OffsetOfElementAt(static_cast<int>(index));
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase,          offset,
                        Handle<Name>(),       OptionalMapRef(),
                        Type::Any(),          MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "ContextSlotKnownPointer"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextSlotSmi(size_t index) {
  int offset = Context::OffsetOfElementAt(static_cast<int>(index));
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase,         offset,
                        Handle<Name>(),      OptionalMapRef(),
                        Type::SignedSmall(), MachineType::TaggedSigned(),
                        kNoWriteBarrier,     "Smi"};
  return access;
}

// static
ElementAccess AccessBuilder::ForFixedArrayElement() {
  ElementAccess access = {kTaggedBase, OFFSET_OF_DATA_START(FixedArray),
                          Type::Any(), MachineType::AnyTagged(),
                          kFullWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForWeakFixedArrayElement() {
  ElementAccess const access = {
      kTaggedBase, OFFSET_OF_DATA_START(WeakFixedArray), Type::Any(),
      MachineType::AnyTagged(), kFullWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForSloppyArgumentsElementsMappedEntry() {
  ElementAccess access = {
      kTaggedBase, OFFSET_OF_DATA_START(SloppyArgumentsElements), Type::Any(),
      MachineType::AnyTagged(), kFullWriteBarrier};
  return access;
}

// statics
ElementAccess AccessBuilder::ForFixedArrayElement(ElementsKind kind) {
  ElementAccess access = {kTaggedBase, OFFSET_OF_DATA_START(FixedArray),
                          Type::Any(), MachineType::AnyTagged(),
                          kFullWriteBarrier};
  switch (kind) {
    case PACKED_SMI_ELEMENTS:
      access.type = Type::SignedSmall();
      access.machine_type = MachineType::TaggedSigned();
      access.write_barrier_kind = kNoWriteBarrier;
      break;
    case HOLEY_SMI_ELEMENTS:
      access.type = TypeCache::Get()->kHoleySmi;
      break;
    case PACKED_ELEMENTS:
      access.type = Type::NonInternal();
      break;
    case HOLEY_ELEMENTS:
      break;
    case PACKED_DOUBLE_ELEMENTS:
      access.type = Type::Number();
      access.write_barrier_kind = kNoWriteBarrier;
      access.machine_type = MachineType::Float64();
      break;
    case HOLEY_DOUBLE_ELEMENTS:
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
      access.type = Type::NumberOrUndefinedOrHole();
#else
      access.type = Type::NumberOrHole();
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
      access.write_barrier_kind = kNoWriteBarrier;
      access.machine_type = MachineType::Float64();
      break;
    default:
      UNREACHABLE();
  }
  return access;
}

// static
ElementAccess AccessBuilder::ForFixedDoubleArrayElement() {
  ElementAccess access = {kTaggedBase, OFFSET_OF_DATA_START(FixedDoubleArray),
                          TypeCache::Get()->kFloat64, MachineType::Float64(),
                          kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForEnumCacheKeys() {
  FieldAccess access = {kTaggedBase,           offsetof(EnumCache, keys_),
                        MaybeHandle<Name>(),   OptionalMapRef(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "EnumCacheKeys"};
  return access;
}

// static
FieldAccess AccessBuilder::ForEnumCacheIndices() {
  FieldAccess access = {kTaggedBase,           offsetof(EnumCache, indices_),
                        MaybeHandle<Name>(),   OptionalMapRef(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "EnumCacheIndices"};
  return access;
}

// static
ElementAccess AccessBuilder::ForTypedArrayElement(ExternalArrayType type,
                                                  bool is_external) {
  BaseTaggedness taggedness = is_external ? kUntaggedBase : kTaggedBase;
  int header_size = is_external ? 0 : OFFSET_OF_DATA_START(ByteArray);
  switch (type) {
    case kExternalInt8Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              MachineType::Int8(), kNoWriteBarrier};
      return access;
    }
    case kExternalUint8Array:
    case kExternalUint8ClampedArray: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              MachineType::Uint8(), kNoWriteBarrier};
      return access;
    }
    case kExternalInt16Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              MachineType::Int16(), kNoWriteBarrier};
      return access;
    }
    case kExternalUint16Array: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              MachineType::Uint16(), kNoWriteBarrier};
      return access;
    }
    case kExternalInt32Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              MachineType::Int32(), kNoWriteBarrier};
      return access;
    }
    case kExternalUint32Array: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              MachineType::Uint32(), kNoWriteBarrier};
      return access;
    }
    case kExternalFloat16Array: {
      // Accesses to Float16Array use float16 raw bits because spotty native
      // fp16 support across architectures. See
      // MachineRepresentation::kFloat16RawBits, which is used during simplified
      // lowering to insert the correct conversions.
      ElementAccess access = {taggedness, header_size, Type::Number(),
                              MachineType::Uint16(), kNoWriteBarrier};
      return access;
    }
    case kExternalFloat32Array: {
      ElementAccess access = {taggedness, header_size, Type::Number(),
                              MachineType::Float32(), kNoWriteBarrier};
      return access;
    }
    case kExternalFloat64Array: {
      ElementAccess access = {taggedness, header_size, Type::Number(),
                              MachineType::Float64(), kNoWriteBarrier};
      return access;
    }
    case kExternalBigInt64Array: {
      ElementAccess access = {taggedness, header_size, Type::SignedBigInt64(),
                              MachineType::Int64(), kNoWriteBarrier};
      return access;
    }
    case kExternalBigUint64Array: {
      ElementAccess access = {taggedness, header_size, Type::UnsignedBigInt64(),
                              MachineType::Uint64(), kNoWriteBarrier};
      return access;
    }
  }
  UNREACHABLE();
}

// static
ElementAccess AccessBuilder::ForJSForInCacheArrayElement(ForInMode mode) {
  ElementAccess access = {
      kTaggedBase, OFFSET_OF_DATA_START(FixedArray),
      (mode == ForInMode::kGeneric ? Type::String()
                                   : Type::InternalizedString()),
      MachineType::AnyTagged(), kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForHashTableBaseNumberOfElements() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(HashTableBase::kNumberOfElementsIndex),
      MaybeHandle<Name>(),
      OptionalMapRef(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier,
      "HashTableBaseNumberOfElements"};
  return access;
}

// static
FieldAccess AccessBuilder::ForHashTableBaseNumberOfDeletedElement() {
  FieldAccess access = {kTaggedBase,
                        FixedArray::OffsetOfElementAt(
                            HashTableBase::kNumberOfDeletedElementsIndex),
                        MaybeHandle<Name>(),
                        OptionalMapRef(),
                        Type::SignedSmall(),
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "HashTableBaseNumberOfDeletedElement"};
  return access;
}

// static
FieldAccess AccessBuilder::ForHashTableBaseCapacity() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(HashTableBase::kCapacityIndex),
      MaybeHandle<Name>(),
      OptionalMapRef(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier,
      "HashTableBaseCapacity"};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNextTable() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  static_assert(OrderedHashMap::NextTableOffset() ==
                OrderedHashSet::NextTableOffset());
  FieldAccess const access = {
      kTaggedBase,         OrderedHashMap::NextTableOffset(),
      MaybeHandle<Name>(), OptionalMapRef(),
      Type::Any(),         MachineType::AnyTagged(),
      kFullWriteBarrier,   "OrderedHashMapOrSetNextTable"};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNumberOfBuckets() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  static_assert(OrderedHashMap::NumberOfBucketsOffset() ==
                OrderedHashSet::NumberOfBucketsOffset());
  FieldAccess const access = {kTaggedBase,
                              OrderedHashMap::NumberOfBucketsOffset(),
                              MaybeHandle<Name>(),
                              OptionalMapRef(),
                              TypeCache::Get()->kFixedArrayLengthType,
                              MachineType::TaggedSigned(),
                              kNoWriteBarrier,
                              "OrderedHashMapOrSetNumberOfBuckets"};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNumberOfDeletedElements() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  static_assert(OrderedHashMap::NumberOfDeletedElementsOffset() ==
                OrderedHashSet::NumberOfDeletedElementsOffset());
  FieldAccess const access = {kTaggedBase,
                              OrderedHashMap::NumberOfDeletedElementsOffset(),
                              MaybeHandle<Name>(),
                              OptionalMapRef(),
                              TypeCache::Get()->kFixedArrayLengthType,
                              MachineType::TaggedSigned(),
                              kNoWriteBarrier,
                              "OrderedHashMapOrSetNumberOfDeletedElements"};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNumberOfElements() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  static_assert(OrderedHashMap::NumberOfElementsOffset() ==
                OrderedHashSet::NumberOfElementsOffset());
  FieldAccess const access = {kTaggedBase,
                              OrderedHashMap::NumberOfElementsOffset(),
                              MaybeHandle<Name>(),
                              OptionalMapRef(),
                              TypeCache::Get()->kFixedArrayLengthType,
                              MachineType::TaggedSigned(),
                              kNoWriteBarrier,
                              "OrderedHashMapOrSetNumberOfElements"};
  return access;
}

// static
ElementAccess AccessBuilder::ForOrderedHashMapEntryValue() {
  ElementAccess const access = {kTaggedBase,
                                OrderedHashMap::HashTableStartOffset() +
                                    OrderedHashMap::kValueOffset * kTaggedSize,
                                Type::Any(), MachineType::AnyTagged(),
                                kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForDictionaryNextEnumerationIndex() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(NameDictionary::kNextEnumerationIndexIndex),
      MaybeHandle<Name>(),
      OptionalMapRef(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier,
      "DictionaryNextEnumerationIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForDictionaryObjectHashIndex() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(NameDictionary::kObjectHashIndex),
      MaybeHandle<Name>(),
      OptionalMapRef(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier,
      "DictionaryObjectHashIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForNameDictionaryFlagsIndex() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(NameDictionary::kFlagsIndex),
      MaybeHandle<Name>(),
      OptionalMapRef(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier,
      "NameDictionaryFlagsIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackCellInterruptBudget() {
  FieldAccess access = {kTaggedBase,
                        offsetof(FeedbackCell, interrupt_budget_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "FeedbackCellInterruptBudget"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackCellDispatchHandleNoWriteBarrier() {
  // Dispatch handles in FeedbackCells are effectively const-after-init and so
  // they are marked as kNoWriteBarrier here (because the fields will not be
  // written to).
  FieldAccess access = {kTaggedBase,
                        offsetof(FeedbackCell, dispatch_handle_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "FeedbackCellDispatchHandle"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorInvocationCount() {
  FieldAccess access = {kTaggedBase,
                        offsetof(FeedbackVector, invocation_count_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "FeedbackVectorInvocationCount"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorFlags() {
  FieldAccess access = {kTaggedBase,
                        offsetof(FeedbackVector, flags_),
                        Handle<Name>(),
                        OptionalMapRef(),
                        TypeCache::Get()->kUint16,
                        MachineType::Uint16(),
                        kNoWriteBarrier,
                        "FeedbackVectorFlags"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorClosureFeedbackCellArray() {
  FieldAccess access = {
      kTaggedBase,       offsetof(FeedbackVector, closure_feedback_cell_array_),
      Handle<Name>(),    OptionalMapRef(),
      Type::Any(),       MachineType::TaggedPointer(),
      kFullWriteBarrier, "FeedbackVectorClosureFeedbackCellArray"};
  return access;
}

#if V8_ENABLE_WEBASSEMBLY
// static
FieldAccess AccessBuilder::ForWasmArrayLength() {
  return {compiler::kTaggedBase,
          offsetof(WasmArray, length_),
          MaybeHandle<Name>(),
          compiler::OptionalMapRef(),
          compiler::Type::OtherInternal(),
          MachineType::Uint32(),
          compiler::kNoWriteBarrier,
          "WasmArrayLength"};
}

// static
FieldAccess AccessBuilder::ForWasmDispatchTableLength() {
  return {compiler::kTaggedBase,
          WasmDispatchTable::kLengthOffset,
          MaybeHandle<Name>{},
          compiler::OptionalMapRef{},
          compiler::Type::OtherInternal(),
          MachineType::Uint32(),
          compiler::kNoWriteBarrier,
          "WasmDispatchTableLength"};
}
#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace compiler
}  // namespace internal
}  // namespace v8
