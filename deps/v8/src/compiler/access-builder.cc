// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"

#include "src/compiler/type-cache.h"
#include "src/handles/handles-inl.h"
#include "src/objects/arguments.h"
#include "src/objects/contexts.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-collection.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/source-text-module.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
FieldAccess AccessBuilder::ForExternalIntPtr() {
  FieldAccess access = {kUntaggedBase,      0,           MaybeHandle<Name>(),
                        MaybeHandle<Map>(), Type::Any(), MachineType::IntPtr(),
                        kNoWriteBarrier,    "ExternalIntPtr"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMap(WriteBarrierKind write_barrier) {
  FieldAccess access = {kTaggedBase,           HeapObject::kMapOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::MapInHeader(),
                        write_barrier,         "Map"};
  return access;
}

// static
FieldAccess AccessBuilder::ForHeapNumberValue() {
  FieldAccess access = {
      kTaggedBase,        HeapNumber::kValueOffset,   MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kFloat64, MachineType::Float64(),
      kNoWriteBarrier,    "HeapNumberValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForBigIntBitfield() {
  FieldAccess access = {
      kTaggedBase,        BigInt::kBitfieldOffset,  MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kInt32, MachineType::Uint32(),
      kNoWriteBarrier,    "BigIntBitfield"};
  return access;
}

// static
FieldAccess AccessBuilder::ForBigIntOptionalPadding() {
  DCHECK_EQ(FIELD_SIZE(BigInt::kOptionalPaddingOffset), 4);
  FieldAccess access = {
      kTaggedBase,        BigInt::kOptionalPaddingOffset, MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kInt32,       MachineType::Uint32(),
      kNoWriteBarrier,    "BigIntOptionalPadding"};
  return access;
}

// static
FieldAccess AccessBuilder::ForBigIntLeastSignificantDigit64() {
  DCHECK_EQ(BigInt::SizeFor(1) - BigInt::SizeFor(0), 8);
  FieldAccess access = {
      kTaggedBase,        BigInt::kDigitsOffset,        MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kBigUint64, MachineType::Uint64(),
      kNoWriteBarrier,    "BigIntLeastSignificantDigit64"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectPropertiesOrHash() {
  FieldAccess access = {kTaggedBase,         JSObject::kPropertiesOrHashOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Any(),         MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSObjectPropertiesOrHash"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectPropertiesOrHashKnownPointer() {
  FieldAccess access = {kTaggedBase,         JSObject::kPropertiesOrHashOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Any(),         MachineType::TaggedPointer(),
                        kPointerWriteBarrier,
                        "JSObjectPropertiesOrHashKnownPointer"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectElements() {
  FieldAccess access = {kTaggedBase,          JSObject::kElementsOffset,
                        MaybeHandle<Name>(),  MaybeHandle<Map>(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSObjectElements"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectInObjectProperty(
    const MapRef& map, int index, MachineType machine_type) {
  int const offset = map.GetInObjectPropertyOffset(index);
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), machine_type,
                        kFullWriteBarrier,   "JSObjectInObjectProperty"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectOffset(
    int offset, WriteBarrierKind write_barrier_kind) {
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        write_barrier_kind,  "JSObjectOffset"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionTable() {
  FieldAccess access = {kTaggedBase,           JSCollection::kTableOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "JSCollectionTable"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionIteratorTable() {
  FieldAccess access = {
      kTaggedBase,           JSCollectionIterator::kTableOffset,
      MaybeHandle<Name>(),   MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "JSCollectionIteratorTable"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionIteratorIndex() {
  FieldAccess access = {kTaggedBase,
                        JSCollectionIterator::kIndexOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kFixedArrayLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "JSCollectionIteratorIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionPrototypeOrInitialMap() {
  FieldAccess access = {
      kTaggedBase,          JSFunction::kPrototypeOrInitialMapOffset,
      MaybeHandle<Name>(),  MaybeHandle<Map>(),
      Type::Any(),          MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSFunctionPrototypeOrInitialMap"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionContext() {
  FieldAccess access = {kTaggedBase,          JSFunction::kContextOffset,
                        MaybeHandle<Name>(),  MaybeHandle<Map>(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSFunctionContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionSharedFunctionInfo() {
  FieldAccess access = {
      kTaggedBase,           JSFunction::kSharedFunctionInfoOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "JSFunctionSharedFunctionInfo"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionFeedbackCell() {
  FieldAccess access = {kTaggedBase,          JSFunction::kFeedbackCellOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSFunctionFeedbackCell"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionCode() {
  FieldAccess access = {kTaggedBase,           JSFunction::kCodeOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "JSFunctionCode"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundTargetFunction() {
  FieldAccess access = {
      kTaggedBase,          JSBoundFunction::kBoundTargetFunctionOffset,
      Handle<Name>(),       MaybeHandle<Map>(),
      Type::Callable(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSBoundFunctionBoundTargetFunction"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundThis() {
  FieldAccess access = {kTaggedBase,         JSBoundFunction::kBoundThisOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSBoundFunctionBoundThis"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundArguments() {
  FieldAccess access = {
      kTaggedBase,          JSBoundFunction::kBoundArgumentsOffset,
      Handle<Name>(),       MaybeHandle<Map>(),
      Type::Internal(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSBoundFunctionBoundArguments"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContext() {
  FieldAccess access = {kTaggedBase,          JSGeneratorObject::kContextOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSGeneratorObjectContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectFunction() {
  FieldAccess access = {kTaggedBase,
                        JSGeneratorObject::kFunctionOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        Type::CallableFunction(),
                        MachineType::TaggedPointer(),
                        kPointerWriteBarrier,
                        "JSGeneratorObjectFunction"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectReceiver() {
  FieldAccess access = {kTaggedBase,          JSGeneratorObject::kReceiverOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSGeneratorObjectReceiver"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContinuation() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kContinuationOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "JSGeneratorObjectContinuation"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectInputOrDebugPos() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kInputOrDebugPosOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSGeneratorObjectInputOrDebugPos"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectParametersAndRegisters() {
  FieldAccess access = {
      kTaggedBase,          JSGeneratorObject::kParametersAndRegistersOffset,
      Handle<Name>(),       MaybeHandle<Map>(),
      Type::Internal(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSGeneratorObjectParametersAndRegisters"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectResumeMode() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kResumeModeOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "JSGeneratorObjectResumeMode"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncFunctionObjectPromise() {
  FieldAccess access = {
      kTaggedBase,          JSAsyncFunctionObject::kPromiseOffset,
      Handle<Name>(),       MaybeHandle<Map>(),
      Type::OtherObject(),  MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSAsyncFunctionObjectPromise"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncGeneratorObjectQueue() {
  FieldAccess access = {
      kTaggedBase,         JSAsyncGeneratorObject::kQueueOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSAsyncGeneratorObjectQueue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncGeneratorObjectIsAwaiting() {
  FieldAccess access = {
      kTaggedBase,         JSAsyncGeneratorObject::kIsAwaitingOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "JSAsyncGeneratorObjectIsAwaiting"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayLength(ElementsKind elements_kind) {
  TypeCache const* type_cache = TypeCache::Get();
  FieldAccess access = {kTaggedBase,
                        JSArray::kLengthOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        type_cache->kJSArrayLengthType,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier,
                        "JSArrayLength"};
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
FieldAccess AccessBuilder::ForJSArrayBufferBitField() {
  FieldAccess access = {
      kTaggedBase,        JSArrayBuffer::kBitFieldOffset, MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint8,       MachineType::Uint32(),
      kNoWriteBarrier,    "JSArrayBufferBitField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewBuffer() {
  FieldAccess access = {kTaggedBase,           JSArrayBufferView::kBufferOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "JSArrayBufferViewBuffer"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewByteLength() {
  FieldAccess access = {kTaggedBase,
                        JSArrayBufferView::kByteLengthOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kJSArrayBufferViewByteLengthType,
                        MachineType::UintPtr(),
                        kNoWriteBarrier,
                        "JSArrayBufferViewByteLength"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewByteOffset() {
  FieldAccess access = {kTaggedBase,
                        JSArrayBufferView::kByteOffsetOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kJSArrayBufferViewByteOffsetType,
                        MachineType::UintPtr(),
                        kNoWriteBarrier,
                        "JSArrayBufferViewByteOffset"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayLength() {
  FieldAccess access = {kTaggedBase,
                        JSTypedArray::kLengthOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kJSTypedArrayLengthType,
                        MachineType::UintPtr(),
                        kNoWriteBarrier,
                        "JSTypedArrayLength"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayBasePointer() {
  FieldAccess access = {kTaggedBase,           JSTypedArray::kBasePointerOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,     "JSTypedArrayBasePointer"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayExternalPointer() {
  FieldAccess access = {
      kTaggedBase,
      JSTypedArray::kExternalPointerOffset,
      MaybeHandle<Name>(),
      MaybeHandle<Map>(),
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
      JSDataView::kDataPointerOffset,
      MaybeHandle<Name>(),
      MaybeHandle<Map>(),
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
                        JSDate::kValueOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kJSDateValueType,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier,
                        "JSDateValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSDateField(JSDate::FieldIndex index) {
  FieldAccess access = {
      kTaggedBase,         JSDate::kValueOffset + index * kTaggedSize,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Number(),      MachineType::AnyTagged(),
      kFullWriteBarrier,   "JSDateField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSIteratorResultDone() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kDoneOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSIteratorResultDone"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSIteratorResultValue() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kValueOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSIteratorResultValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpData() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kDataOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSRegExpData"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpFlags() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kFlagsOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSRegExpFlags"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpLastIndex() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kLastIndexOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSRegExpLastIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpSource() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kSourceOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "JSRegExpSource"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedArrayLength() {
  FieldAccess access = {kTaggedBase,
                        FixedArray::kLengthOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kFixedArrayLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "FixedArrayLength"};
  return access;
}

// static
FieldAccess AccessBuilder::ForWeakFixedArrayLength() {
  FieldAccess access = {kTaggedBase,
                        WeakFixedArray::kLengthOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kWeakFixedArrayLengthType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "WeakFixedArrayLength"};
  return access;
}

// static
FieldAccess AccessBuilder::ForSloppyArgumentsElementsContext() {
  FieldAccess access = {
      kTaggedBase,          SloppyArgumentsElements::kContextOffset,
      MaybeHandle<Name>(),  MaybeHandle<Map>(),
      Type::Any(),          MachineType::TaggedPointer(),
      kPointerWriteBarrier, "SloppyArgumentsElementsContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForSloppyArgumentsElementsArguments() {
  FieldAccess access = {
      kTaggedBase,          SloppyArgumentsElements::kArgumentsOffset,
      MaybeHandle<Name>(),  MaybeHandle<Map>(),
      Type::Any(),          MachineType::TaggedPointer(),
      kPointerWriteBarrier, "SloppyArgumentsElementsArguments"};
  return access;
}

// static
FieldAccess AccessBuilder::ForPropertyArrayLengthAndHash() {
  FieldAccess access = {
      kTaggedBase,         PropertyArray::kLengthAndHashOffset,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier,     "PropertyArrayLengthAndHash"};
  return access;
}

// static
FieldAccess AccessBuilder::ForDescriptorArrayEnumCache() {
  FieldAccess access = {
      kTaggedBase,           DescriptorArray::kEnumCacheOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "DescriptorArrayEnumCache"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitFieldOffset,     Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint8, MachineType::Uint8(),
      kNoWriteBarrier,    "MapBitField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField2() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitField2Offset,    Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint8, MachineType::Uint8(),
      kNoWriteBarrier,    "MapBitField2"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField3() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitField3Offset,    Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kInt32, MachineType::Int32(),
      kNoWriteBarrier,    "MapBitField3"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapDescriptors() {
  FieldAccess access = {kTaggedBase,           Map::kInstanceDescriptorsOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "MapDescriptors"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapInstanceType() {
  FieldAccess access = {
      kTaggedBase,        Map::kInstanceTypeOffset,  Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint16, MachineType::Uint16(),
      kNoWriteBarrier,    "MapInstanceType"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapPrototype() {
  FieldAccess access = {kTaggedBase,          Map::kPrototypeOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::Any(),          MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "MapPrototype"};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapNativeContext() {
  FieldAccess access = {
      kTaggedBase,          Map::kConstructorOrBackPointerOrNativeContextOffset,
      Handle<Name>(),       MaybeHandle<Map>(),
      Type::Any(),          MachineType::TaggedPointer(),
      kPointerWriteBarrier, "MapNativeContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularExports() {
  FieldAccess access = {
      kTaggedBase,           SourceTextModule::kRegularExportsOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "ModuleRegularExports"};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularImports() {
  FieldAccess access = {
      kTaggedBase,           SourceTextModule::kRegularImportsOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier,  "ModuleRegularImports"};
  return access;
}

// static
FieldAccess AccessBuilder::ForNameRawHashField() {
  FieldAccess access = {kTaggedBase,        Name::kRawHashFieldOffset,
                        Handle<Name>(),     MaybeHandle<Map>(),
                        Type::Unsigned32(), MachineType::Uint32(),
                        kNoWriteBarrier ,   "NameRawHashField"};
  return access;
}

// static
FieldAccess AccessBuilder::ForStringLength() {
  FieldAccess access = {kTaggedBase,
                        String::kLengthOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kStringLengthType,
                        MachineType::Uint32(),
                        kNoWriteBarrier,
                        "StringLength"};
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringFirst() {
  FieldAccess access = {kTaggedBase,         ConsString::kFirstOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier,
                        "ConsStringFirst"};
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringSecond() {
  FieldAccess access = {kTaggedBase,          ConsString::kSecondOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "ConsStringSecond"};
  return access;
}

// static
FieldAccess AccessBuilder::ForThinStringActual() {
  FieldAccess access = {kTaggedBase,          ThinString::kActualOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "ThinStringActual"};
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringOffset() {
  FieldAccess access = {kTaggedBase,         SlicedString::kOffsetOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::SignedSmall(), MachineType::TaggedSigned(),
                        kNoWriteBarrier,     "SlicedStringOffset"};
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringParent() {
  FieldAccess access = {kTaggedBase,          SlicedString::kParentOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "SlicedStringParent"};
  return access;
}

// static
FieldAccess AccessBuilder::ForExternalStringResourceData() {
  FieldAccess access = {
      kTaggedBase,
      ExternalString::kResourceDataOffset,
      Handle<Name>(),
      MaybeHandle<Map>(),
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
  ElementAccess access = {kTaggedBase, SeqOneByteString::kHeaderSize,
                          TypeCache::Get()->kUint8, MachineType::Uint8(),
                          kNoWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForSeqTwoByteStringCharacter() {
  ElementAccess access = {kTaggedBase, SeqTwoByteString::kHeaderSize,
                          TypeCache::Get()->kUint16, MachineType::Uint16(),
                          kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGlobalProxyNativeContext() {
  FieldAccess access = {
      kTaggedBase,          JSGlobalProxy::kNativeContextOffset,
      Handle<Name>(),       MaybeHandle<Map>(),
      Type::Internal(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSGlobalProxyNativeContext"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorIteratedObject() {
  FieldAccess access = {
      kTaggedBase,          JSArrayIterator::kIteratedObjectOffset,
      Handle<Name>(),       MaybeHandle<Map>(),
      Type::Receiver(),     MachineType::TaggedPointer(),
      kPointerWriteBarrier, "JSArrayIteratorIteratedObject"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorNextIndex() {
  // In generic case, cap to 2^53-1 (per ToLength() in spec) via
  // kPositiveSafeInteger
  FieldAccess access = {kTaggedBase,
                        JSArrayIterator::kNextIndexOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kPositiveSafeInteger,
                        MachineType::AnyTagged(),
                        kFullWriteBarrier,
                        "JSArrayIteratorNextIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorKind() {
  FieldAccess access = {kTaggedBase,
                        JSArrayIterator::kKindOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kJSArrayIteratorKindType,
                        MachineType::TaggedSigned(),
                        kNoWriteBarrier,
                        "JSArrayIteratorKind"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSStringIteratorString() {
  FieldAccess access = {kTaggedBase,          JSStringIterator::kStringOffset,
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::String(),       MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "JSStringIteratorString"};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSStringIteratorIndex() {
  FieldAccess access = {kTaggedBase,
                        JSStringIterator::kIndexOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
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
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,   "ArgumentsLength"};
  return access;
}

// static
FieldAccess AccessBuilder::ForArgumentsCallee() {
  FieldAccess access = {
      kTaggedBase,         JSSloppyArgumentsObject::kCalleeOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier,   "ArgumentsCallee"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedArraySlot(
    size_t index, WriteBarrierKind write_barrier_kind) {
  int offset = FixedArray::OffsetOfElementAt(static_cast<int>(index));
  FieldAccess access = {kTaggedBase,        offset,
                        Handle<Name>(),     MaybeHandle<Map>(),
                        Type::Any(),        MachineType::AnyTagged(),
                        write_barrier_kind, "FixedArraySlot"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorSlot(int index) {
  int offset = FeedbackVector::OffsetOfElementAt(index);
  FieldAccess access = {kTaggedBase,       offset,
                        Handle<Name>(),    MaybeHandle<Map>(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, "FeedbackVectorSlot"};
  return access;
}

// static
FieldAccess AccessBuilder::ForWeakFixedArraySlot(int index) {
  int offset = WeakFixedArray::OffsetOfElementAt(index);
  FieldAccess access = {kTaggedBase,       offset,
                        Handle<Name>(),    MaybeHandle<Map>(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, "WeakFixedArraySlot"};
  return access;
}
// static
FieldAccess AccessBuilder::ForCellValue() {
  FieldAccess access = {kTaggedBase,       Cell::kValueOffset,
                        Handle<Name>(),    MaybeHandle<Map>(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, "CellValue"};
  return access;
}

// static
FieldAccess AccessBuilder::ForScopeInfoFlags() {
  FieldAccess access = {kTaggedBase,         ScopeInfo::kFlagsOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::SignedSmall(), MachineType::TaggedSigned(),
                        kNoWriteBarrier,     "ScopeInfoFlags"};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextSlot(size_t index) {
  int offset = Context::OffsetOfElementAt(static_cast<int>(index));
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase,       offset,
                        Handle<Name>(),    MaybeHandle<Map>(),
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
                        Handle<Name>(),       MaybeHandle<Map>(),
                        Type::Any(),          MachineType::TaggedPointer(),
                        kPointerWriteBarrier, "ContextSlotKnownPointer"};
  return access;
}

// static
ElementAccess AccessBuilder::ForFixedArrayElement() {
  ElementAccess access = {kTaggedBase, FixedArray::kHeaderSize, Type::Any(),
                          MachineType::AnyTagged(), kFullWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForWeakFixedArrayElement() {
  ElementAccess const access = {kTaggedBase, WeakFixedArray::kHeaderSize,
                                Type::Any(), MachineType::AnyTagged(),
                                kFullWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForSloppyArgumentsElementsMappedEntry() {
  ElementAccess access = {
      kTaggedBase, SloppyArgumentsElements::kMappedEntriesOffset, Type::Any(),
      MachineType::AnyTagged(), kFullWriteBarrier};
  return access;
}

// statics
ElementAccess AccessBuilder::ForFixedArrayElement(ElementsKind kind) {
  ElementAccess access = {kTaggedBase, FixedArray::kHeaderSize, Type::Any(),
                          MachineType::AnyTagged(), kFullWriteBarrier};
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
      access.type = Type::NumberOrHole();
      access.write_barrier_kind = kNoWriteBarrier;
      access.machine_type = MachineType::Float64();
      break;
    default:
      UNREACHABLE();
  }
  return access;
}

// static
ElementAccess AccessBuilder::ForStackArgument() {
  ElementAccess access = {
      kUntaggedBase,
      CommonFrameConstants::kFixedFrameSizeAboveFp - kSystemPointerSize,
      Type::NonInternal(), MachineType::Pointer(),
      WriteBarrierKind::kNoWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForFixedDoubleArrayElement() {
  ElementAccess access = {kTaggedBase, FixedDoubleArray::kHeaderSize,
                          TypeCache::Get()->kFloat64, MachineType::Float64(),
                          kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForEnumCacheKeys() {
  FieldAccess access = {kTaggedBase,           EnumCache::kKeysOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "EnumCacheKeys"};
  return access;
}

// static
FieldAccess AccessBuilder::ForEnumCacheIndices() {
  FieldAccess access = {kTaggedBase,           EnumCache::kIndicesOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier,  "EnumCacheIndices"};
  return access;
}

// static
ElementAccess AccessBuilder::ForTypedArrayElement(ExternalArrayType type,
                                                  bool is_external) {
  BaseTaggedness taggedness = is_external ? kUntaggedBase : kTaggedBase;
  int header_size = is_external ? 0 : ByteArray::kHeaderSize;
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
    case kExternalBigInt64Array:
    case kExternalBigUint64Array:
      // TODO(neis/jkummerow): Define appropriate types.
      UNIMPLEMENTED();
  }
  UNREACHABLE();
}

// static
ElementAccess AccessBuilder::ForJSForInCacheArrayElement(ForInMode mode) {
  ElementAccess access = {
      kTaggedBase, FixedArray::kHeaderSize,
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
      MaybeHandle<Map>(),
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
                        MaybeHandle<Map>(),
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
      MaybeHandle<Map>(),
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
      MaybeHandle<Name>(), MaybeHandle<Map>(),
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
                              MaybeHandle<Map>(),
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
                              MaybeHandle<Map>(),
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
                              MaybeHandle<Map>(),
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
      MaybeHandle<Map>(),
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
      MaybeHandle<Map>(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier,
      "DictionaryObjectHashIndex"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackCellInterruptBudget() {
  FieldAccess access = {kTaggedBase,
                        FeedbackCell::kInterruptBudgetOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "FeedbackCellInterruptBudget"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorInvocationCount() {
  FieldAccess access = {kTaggedBase,
                        FeedbackVector::kInvocationCountOffset,
                        Handle<Name>(),
                        MaybeHandle<Map>(),
                        TypeCache::Get()->kInt32,
                        MachineType::Int32(),
                        kNoWriteBarrier,
                        "FeedbackVectorInvocationCount"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorFlags() {
  FieldAccess access = {
      kTaggedBase,        FeedbackVector::kFlagsOffset, Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint16,    MachineType::Uint16(),
      kNoWriteBarrier,    "FeedbackVectorFlags"};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorClosureFeedbackCellArray() {
  FieldAccess access = {
      kTaggedBase,       FeedbackVector::kClosureFeedbackCellArrayOffset,
      Handle<Name>(),    MaybeHandle<Map>(),
      Type::Any(),       MachineType::TaggedPointer(),
      kFullWriteBarrier, "FeedbackVectorClosureFeedbackCellArray"};
  return access;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
