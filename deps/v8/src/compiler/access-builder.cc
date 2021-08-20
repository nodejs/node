// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"

#include "src/compiler/type-cache.h"
#include "src/execution/frames.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap.h"
#include "src/objects/arguments.h"
#include "src/objects/cell.h"
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMap(WriteBarrierKind write_barrier) {
  FieldAccess access = {kTaggedBase,           HeapObject::kMapOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::MapInHeader(),
                        write_barrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForHeapNumberValue() {
  FieldAccess access = {
      kTaggedBase,        HeapNumber::kValueOffset,   MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kFloat64, MachineType::Float64(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForBigIntBitfield() {
  FieldAccess access = {
      kTaggedBase,        BigInt::kBitfieldOffset,  MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kInt32, MachineType::Uint32(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForBigIntOptionalPadding() {
  DCHECK_EQ(FIELD_SIZE(BigInt::kOptionalPaddingOffset), 4);
  FieldAccess access = {
      kTaggedBase,        BigInt::kOptionalPaddingOffset, MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kInt32,       MachineType::Uint32(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForBigIntLeastSignificantDigit64() {
  DCHECK_EQ(BigInt::SizeFor(1) - BigInt::SizeFor(0), 8);
  FieldAccess access = {
      kTaggedBase,        BigInt::kDigitsOffset,        MaybeHandle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kBigUint64, MachineType::Uint64(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectPropertiesOrHash() {
  FieldAccess access = {kTaggedBase,         JSObject::kPropertiesOrHashOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Any(),         MachineType::AnyTagged(),
                        kFullWriteBarrier,   LoadSensitivity::kCritical};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectPropertiesOrHashKnownPointer() {
  FieldAccess access = {kTaggedBase,          JSObject::kPropertiesOrHashOffset,
                        MaybeHandle<Name>(),  MaybeHandle<Map>(),
                        Type::Any(),          MachineType::TaggedPointer(),
                        kPointerWriteBarrier, LoadSensitivity::kCritical};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectElements() {
  FieldAccess access = {kTaggedBase,          JSObject::kElementsOffset,
                        MaybeHandle<Name>(),  MaybeHandle<Map>(),
                        Type::Internal(),     MachineType::TaggedPointer(),
                        kPointerWriteBarrier, LoadSensitivity::kCritical};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectInObjectProperty(
    const MapRef& map, int index, MachineType machine_type) {
  int const offset = map.GetInObjectPropertyOffset(index);
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), machine_type,
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSObjectOffset(
    int offset, WriteBarrierKind write_barrier_kind) {
  FieldAccess access = {kTaggedBase,         offset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        write_barrier_kind};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionTable() {
  FieldAccess access = {kTaggedBase,           JSCollection::kTableOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSCollectionIteratorTable() {
  FieldAccess access = {
      kTaggedBase,           JSCollectionIterator::kTableOffset,
      MaybeHandle<Name>(),   MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionPrototypeOrInitialMap() {
  FieldAccess access = {
      kTaggedBase,         JSFunction::kPrototypeOrInitialMapOffset,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Any(),         MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionContext() {
  FieldAccess access = {kTaggedBase,         JSFunction::kContextOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionSharedFunctionInfo() {
  FieldAccess access = {
      kTaggedBase,           JSFunction::kSharedFunctionInfoOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionFeedbackCell() {
  FieldAccess access = {kTaggedBase,         JSFunction::kFeedbackCellOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSFunctionCode() {
  FieldAccess access = {kTaggedBase,           JSFunction::kCodeOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundTargetFunction() {
  FieldAccess access = {
      kTaggedBase,         JSBoundFunction::kBoundTargetFunctionOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Callable(),    MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundThis() {
  FieldAccess access = {kTaggedBase,         JSBoundFunction::kBoundThisOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSBoundFunctionBoundArguments() {
  FieldAccess access = {
      kTaggedBase,         JSBoundFunction::kBoundArgumentsOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Internal(),    MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContext() {
  FieldAccess access = {kTaggedBase,         JSGeneratorObject::kContextOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectFunction() {
  FieldAccess access = {kTaggedBase,         JSGeneratorObject::kFunctionOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Function(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectReceiver() {
  FieldAccess access = {kTaggedBase,         JSGeneratorObject::kReceiverOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Internal(),    MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectContinuation() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kContinuationOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectInputOrDebugPos() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kInputOrDebugPosOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectParametersAndRegisters() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kParametersAndRegistersOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Internal(),    MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSGeneratorObjectResumeMode() {
  FieldAccess access = {
      kTaggedBase,         JSGeneratorObject::kResumeModeOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncFunctionObjectPromise() {
  FieldAccess access = {
      kTaggedBase,         JSAsyncFunctionObject::kPromiseOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::OtherObject(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncGeneratorObjectQueue() {
  FieldAccess access = {
      kTaggedBase,         JSAsyncGeneratorObject::kQueueOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSAsyncGeneratorObjectIsAwaiting() {
  FieldAccess access = {
      kTaggedBase,         JSAsyncGeneratorObject::kIsAwaitingOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier};
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
                        kFullWriteBarrier};
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
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayBufferViewBuffer() {
  FieldAccess access = {kTaggedBase,           JSArrayBufferView::kBufferOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
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
                        kNoWriteBarrier};
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
                        kNoWriteBarrier};
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayBasePointer() {
  FieldAccess access = {kTaggedBase,           JSTypedArray::kBasePointerOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier,     LoadSensitivity::kCritical};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSTypedArrayExternalPointer() {
  FieldAccess access = {kTaggedBase,
                        JSTypedArray::kExternalPointerOffset,
                        MaybeHandle<Name>(),
                        MaybeHandle<Map>(),
                        V8_HEAP_SANDBOX_BOOL ? Type::SandboxedExternalPointer()
                                             : Type::ExternalPointer(),
                        MachineType::Pointer(),
                        kNoWriteBarrier,
                        LoadSensitivity::kCritical,
                        ConstFieldInfo::None(),
                        false,
#ifdef V8_HEAP_SANDBOX
                        kTypedArrayExternalPointerTag
#endif
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
      V8_HEAP_SANDBOX_BOOL ? Type::SandboxedExternalPointer()
                           : Type::ExternalPointer(),
      MachineType::Pointer(),
      kNoWriteBarrier,
      LoadSensitivity::kUnsafe,
      ConstFieldInfo::None(),
      false,
#ifdef V8_HEAP_SANDBOX
      kDataViewDataPointerTag,
#endif
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
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSDateField(JSDate::FieldIndex index) {
  FieldAccess access = {
      kTaggedBase,         JSDate::kValueOffset + index * kTaggedSize,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Number(),      MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSIteratorResultDone() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kDoneOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSIteratorResultValue() {
  FieldAccess access = {kTaggedBase,         JSIteratorResult::kValueOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpData() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kDataOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpFlags() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kFlagsOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpLastIndex() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kLastIndexOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSRegExpSource() {
  FieldAccess access = {kTaggedBase,         JSRegExp::kSourceOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
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
                        kNoWriteBarrier};
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForSloppyArgumentsElementsContext() {
  FieldAccess access = {
      kTaggedBase,         SloppyArgumentsElements::kContextOffset,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Any(),         MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForSloppyArgumentsElementsArguments() {
  FieldAccess access = {
      kTaggedBase,         SloppyArgumentsElements::kArgumentsOffset,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Any(),         MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForPropertyArrayLengthAndHash() {
  FieldAccess access = {
      kTaggedBase,         PropertyArray::kLengthAndHashOffset,
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::SignedSmall(), MachineType::TaggedSigned(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForDescriptorArrayEnumCache() {
  FieldAccess access = {
      kTaggedBase,           DescriptorArray::kEnumCacheOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitFieldOffset,     Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint8, MachineType::Uint8(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField2() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitField2Offset,    Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint8, MachineType::Uint8(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapBitField3() {
  FieldAccess access = {
      kTaggedBase,        Map::kBitField3Offset,    Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kInt32, MachineType::Int32(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapDescriptors() {
  FieldAccess access = {kTaggedBase,           Map::kInstanceDescriptorsOffset,
                        Handle<Name>(),        MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapInstanceType() {
  FieldAccess access = {
      kTaggedBase,        Map::kInstanceTypeOffset,  Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint16, MachineType::Uint16(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapPrototype() {
  FieldAccess access = {kTaggedBase,         Map::kPrototypeOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Any(),         MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForMapNativeContext() {
  FieldAccess access = {
      kTaggedBase,         Map::kConstructorOrBackPointerOrNativeContextOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Any(),         MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularExports() {
  FieldAccess access = {
      kTaggedBase,           SourceTextModule::kRegularExportsOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForModuleRegularImports() {
  FieldAccess access = {
      kTaggedBase,           SourceTextModule::kRegularImportsOffset,
      Handle<Name>(),        MaybeHandle<Map>(),
      Type::OtherInternal(), MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForNameRawHashField() {
  FieldAccess access = {kTaggedBase,        Name::kRawHashFieldOffset,
                        Handle<Name>(),     MaybeHandle<Map>(),
                        Type::Unsigned32(), MachineType::Uint32(),
                        kNoWriteBarrier};
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringFirst() {
  FieldAccess access = {kTaggedBase,         ConsString::kFirstOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForConsStringSecond() {
  FieldAccess access = {kTaggedBase,         ConsString::kSecondOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForThinStringActual() {
  FieldAccess access = {kTaggedBase,         ThinString::kActualOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringOffset() {
  FieldAccess access = {kTaggedBase,         SlicedString::kOffsetOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::SignedSmall(), MachineType::TaggedSigned(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForSlicedStringParent() {
  FieldAccess access = {kTaggedBase,         SlicedString::kParentOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForExternalStringResourceData() {
  FieldAccess access = {
      kTaggedBase,
      ExternalString::kResourceDataOffset,
      Handle<Name>(),
      MaybeHandle<Map>(),
      V8_HEAP_SANDBOX_BOOL ? Type::SandboxedExternalPointer()
                           : Type::ExternalPointer(),
      MachineType::Pointer(),
      kNoWriteBarrier,
      LoadSensitivity::kUnsafe,
      ConstFieldInfo::None(),
      false,
#ifdef V8_HEAP_SANDBOX
      kExternalStringResourceDataTag,
#endif
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
      kTaggedBase,         JSGlobalProxy::kNativeContextOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Internal(),    MachineType::TaggedPointer(),
      kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSArrayIteratorIteratedObject() {
  FieldAccess access = {
      kTaggedBase,         JSArrayIterator::kIteratedObjectOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::Receiver(),    MachineType::TaggedPointer(),
      kPointerWriteBarrier};
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
                        kFullWriteBarrier};
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForJSStringIteratorString() {
  FieldAccess access = {kTaggedBase,         JSStringIterator::kStringOffset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::String(),      MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForArgumentsLength() {
  constexpr int offset = JSStrictArgumentsObject::kLengthOffset;
  STATIC_ASSERT(offset == JSSloppyArgumentsObject::kLengthOffset);
  FieldAccess access = {kTaggedBase,         offset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::NonInternal(), MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForArgumentsCallee() {
  FieldAccess access = {
      kTaggedBase,         JSSloppyArgumentsObject::kCalleeOffset,
      Handle<Name>(),      MaybeHandle<Map>(),
      Type::NonInternal(), MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForFixedArraySlot(
    size_t index, WriteBarrierKind write_barrier_kind) {
  int offset = FixedArray::OffsetOfElementAt(static_cast<int>(index));
  FieldAccess access = {kTaggedBase,       offset,
                        Handle<Name>(),    MaybeHandle<Map>(),
                        Type::Any(),       MachineType::AnyTagged(),
                        write_barrier_kind};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorSlot(int index) {
  int offset = FeedbackVector::OffsetOfElementAt(index);
  FieldAccess access = {kTaggedBase,      offset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Any(),      MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForWeakFixedArraySlot(int index) {
  int offset = WeakFixedArray::OffsetOfElementAt(index);
  FieldAccess access = {kTaggedBase,      offset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Any(),      MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}
// static
FieldAccess AccessBuilder::ForCellValue() {
  FieldAccess access = {kTaggedBase,       Cell::kValueOffset,
                        Handle<Name>(),    MaybeHandle<Map>(),
                        Type::Any(),       MachineType::AnyTagged(),
                        kFullWriteBarrier, LoadSensitivity::kCritical};
  return access;
}

// static
FieldAccess AccessBuilder::ForScopeInfoFlags() {
  FieldAccess access = {kTaggedBase,         ScopeInfo::kFlagsOffset,
                        MaybeHandle<Name>(), MaybeHandle<Map>(),
                        Type::SignedSmall(), MachineType::TaggedSigned(),
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextSlot(size_t index) {
  int offset = Context::OffsetOfElementAt(static_cast<int>(index));
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase,      offset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Any(),      MachineType::AnyTagged(),
                        kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForContextSlotKnownPointer(size_t index) {
  int offset = Context::OffsetOfElementAt(static_cast<int>(index));
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase,         offset,
                        Handle<Name>(),      MaybeHandle<Map>(),
                        Type::Any(),         MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
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
ElementAccess AccessBuilder::ForFixedArrayElement(
    ElementsKind kind, LoadSensitivity load_sensitivity) {
  ElementAccess access = {kTaggedBase,       FixedArray::kHeaderSize,
                          Type::Any(),       MachineType::AnyTagged(),
                          kFullWriteBarrier, load_sensitivity};
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
                        kPointerWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForEnumCacheIndices() {
  FieldAccess access = {kTaggedBase,           EnumCache::kIndicesOffset,
                        MaybeHandle<Name>(),   MaybeHandle<Map>(),
                        Type::OtherInternal(), MachineType::TaggedPointer(),
                        kPointerWriteBarrier};
  return access;
}

// static
ElementAccess AccessBuilder::ForTypedArrayElement(
    ExternalArrayType type, bool is_external,
    LoadSensitivity load_sensitivity) {
  BaseTaggedness taggedness = is_external ? kUntaggedBase : kTaggedBase;
  int header_size = is_external ? 0 : ByteArray::kHeaderSize;
  switch (type) {
    case kExternalInt8Array: {
      ElementAccess access = {taggedness,       header_size,
                              Type::Signed32(), MachineType::Int8(),
                              kNoWriteBarrier,  load_sensitivity};
      return access;
    }
    case kExternalUint8Array:
    case kExternalUint8ClampedArray: {
      ElementAccess access = {taggedness,         header_size,
                              Type::Unsigned32(), MachineType::Uint8(),
                              kNoWriteBarrier,    load_sensitivity};
      return access;
    }
    case kExternalInt16Array: {
      ElementAccess access = {taggedness,       header_size,
                              Type::Signed32(), MachineType::Int16(),
                              kNoWriteBarrier,  load_sensitivity};
      return access;
    }
    case kExternalUint16Array: {
      ElementAccess access = {taggedness,         header_size,
                              Type::Unsigned32(), MachineType::Uint16(),
                              kNoWriteBarrier,    load_sensitivity};
      return access;
    }
    case kExternalInt32Array: {
      ElementAccess access = {taggedness,       header_size,
                              Type::Signed32(), MachineType::Int32(),
                              kNoWriteBarrier,  load_sensitivity};
      return access;
    }
    case kExternalUint32Array: {
      ElementAccess access = {taggedness,         header_size,
                              Type::Unsigned32(), MachineType::Uint32(),
                              kNoWriteBarrier,    load_sensitivity};
      return access;
    }
    case kExternalFloat32Array: {
      ElementAccess access = {taggedness,      header_size,
                              Type::Number(),  MachineType::Float32(),
                              kNoWriteBarrier, load_sensitivity};
      return access;
    }
    case kExternalFloat64Array: {
      ElementAccess access = {taggedness,      header_size,
                              Type::Number(),  MachineType::Float64(),
                              kNoWriteBarrier, load_sensitivity};
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
FieldAccess AccessBuilder::ForHashTableBaseNumberOfElements() {
  FieldAccess access = {
      kTaggedBase,
      FixedArray::OffsetOfElementAt(HashTableBase::kNumberOfElementsIndex),
      MaybeHandle<Name>(),
      MaybeHandle<Map>(),
      Type::SignedSmall(),
      MachineType::TaggedSigned(),
      kNoWriteBarrier};
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
                        kNoWriteBarrier};
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
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNextTable() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  STATIC_ASSERT(OrderedHashMap::NextTableOffset() ==
                OrderedHashSet::NextTableOffset());
  FieldAccess const access = {
      kTaggedBase,         OrderedHashMap::NextTableOffset(),
      MaybeHandle<Name>(), MaybeHandle<Map>(),
      Type::Any(),         MachineType::AnyTagged(),
      kFullWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNumberOfBuckets() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  STATIC_ASSERT(OrderedHashMap::NumberOfBucketsOffset() ==
                OrderedHashSet::NumberOfBucketsOffset());
  FieldAccess const access = {kTaggedBase,
                              OrderedHashMap::NumberOfBucketsOffset(),
                              MaybeHandle<Name>(),
                              MaybeHandle<Map>(),
                              TypeCache::Get()->kFixedArrayLengthType,
                              MachineType::TaggedSigned(),
                              kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNumberOfDeletedElements() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  STATIC_ASSERT(OrderedHashMap::NumberOfDeletedElementsOffset() ==
                OrderedHashSet::NumberOfDeletedElementsOffset());
  FieldAccess const access = {kTaggedBase,
                              OrderedHashMap::NumberOfDeletedElementsOffset(),
                              MaybeHandle<Name>(),
                              MaybeHandle<Map>(),
                              TypeCache::Get()->kFixedArrayLengthType,
                              MachineType::TaggedSigned(),
                              kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForOrderedHashMapOrSetNumberOfElements() {
  // TODO(turbofan): This will be redundant with the HashTableBase
  // methods above once the hash table unification is done.
  STATIC_ASSERT(OrderedHashMap::NumberOfElementsOffset() ==
                OrderedHashSet::NumberOfElementsOffset());
  FieldAccess const access = {kTaggedBase,
                              OrderedHashMap::NumberOfElementsOffset(),
                              MaybeHandle<Name>(),
                              MaybeHandle<Map>(),
                              TypeCache::Get()->kFixedArrayLengthType,
                              MachineType::TaggedSigned(),
                              kNoWriteBarrier};
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
      kNoWriteBarrier};
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
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackCellValue() {
  FieldAccess access = {kTaggedBase,      FeedbackCell::kValueOffset,
                        Handle<Name>(),   MaybeHandle<Map>(),
                        Type::Any(),      MachineType::TaggedPointer(),
                        kFullWriteBarrier};
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
                        kNoWriteBarrier};
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
                        kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorFlags() {
  FieldAccess access = {
      kTaggedBase,        FeedbackVector::kFlagsOffset, Handle<Name>(),
      MaybeHandle<Map>(), TypeCache::Get()->kUint32,    MachineType::Uint32(),
      kNoWriteBarrier};
  return access;
}

// static
FieldAccess AccessBuilder::ForFeedbackVectorClosureFeedbackCellArray() {
  FieldAccess access = {
      kTaggedBase,      FeedbackVector::kClosureFeedbackCellArrayOffset,
      Handle<Name>(),   MaybeHandle<Map>(),
      Type::Any(),      MachineType::TaggedPointer(),
      kFullWriteBarrier};
  return access;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
