// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"

#include "src/contexts.h"
#include "src/frames.h"
#include "src/heap/heap.h"
#include "src/type-cache.h"
#include "src/types-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
FieldAccess AccessBuilder::ForMap() {
  FieldAccess access = {kTaggedBase, HeapObject::kMapOffset,
                        MaybeHandle<Name>(), Type::Any(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForHeapNumberValue() {
  FieldAccess access = {kTaggedBase, HeapNumber::kValueOffset,
                        MaybeHandle<Name>(), TypeCache().Get().kFloat64,
                        kMachFloat64};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSObjectProperties() {
  FieldAccess access = {kTaggedBase, JSObject::kPropertiesOffset,
                        MaybeHandle<Name>(), Type::Internal(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSObjectElements() {
  FieldAccess access = {kTaggedBase, JSObject::kElementsOffset,
                        MaybeHandle<Name>(), Type::Internal(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSFunctionContext() {
  FieldAccess access = {kTaggedBase, JSFunction::kContextOffset,
                        MaybeHandle<Name>(), Type::Internal(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSFunctionSharedFunctionInfo() {
  FieldAccess access = {kTaggedBase, JSFunction::kSharedFunctionInfoOffset,
                        Handle<Name>(), Type::Any(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSArrayBufferBackingStore() {
  FieldAccess access = {kTaggedBase, JSArrayBuffer::kBackingStoreOffset,
                        MaybeHandle<Name>(), Type::UntaggedPointer(), kMachPtr};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSDateField(JSDate::FieldIndex index) {
  FieldAccess access = {kTaggedBase,
                        JSDate::kValueOffset + index * kPointerSize,
                        MaybeHandle<Name>(), Type::Number(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForFixedArrayLength() {
  FieldAccess access = {kTaggedBase, FixedArray::kLengthOffset,
                        MaybeHandle<Name>(),
                        TypeCache::Get().kFixedArrayLengthType, kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForDescriptorArrayEnumCache() {
  FieldAccess access = {kTaggedBase, DescriptorArray::kEnumCacheOffset,
                        Handle<Name>(), Type::TaggedPointer(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForDescriptorArrayEnumCacheBridgeCache() {
  FieldAccess access = {kTaggedBase,
                        DescriptorArray::kEnumCacheBridgeCacheOffset,
                        Handle<Name>(), Type::TaggedPointer(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapBitField3() {
  FieldAccess access = {kTaggedBase, Map::kBitField3Offset, Handle<Name>(),
                        TypeCache::Get().kInt32, kMachInt32};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapDescriptors() {
  FieldAccess access = {kTaggedBase, Map::kDescriptorsOffset, Handle<Name>(),
                        Type::TaggedPointer(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapInstanceType() {
  FieldAccess access = {kTaggedBase, Map::kInstanceTypeOffset, Handle<Name>(),
                        TypeCache::Get().kUint8, kMachUint8};
  return access;
}


// static
FieldAccess AccessBuilder::ForMapPrototype() {
  FieldAccess access = {kTaggedBase, Map::kPrototypeOffset, Handle<Name>(),
                        Type::TaggedPointer(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForStringLength() {
  FieldAccess access = {kTaggedBase, String::kLengthOffset, Handle<Name>(),
                        TypeCache::Get().kStringLengthType, kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSGlobalObjectGlobalProxy() {
  FieldAccess access = {kTaggedBase, JSGlobalObject::kGlobalProxyOffset,
                        Handle<Name>(), Type::Receiver(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForJSGlobalObjectNativeContext() {
  FieldAccess access = {kTaggedBase, JSGlobalObject::kNativeContextOffset,
                        Handle<Name>(), Type::Internal(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForValue() {
  FieldAccess access = {kTaggedBase, JSValue::kValueOffset, Handle<Name>(),
                        Type::Any(), kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForArgumentsLength() {
  int offset =
      JSObject::kHeaderSize + Heap::kArgumentsLengthIndex * kPointerSize;
  FieldAccess access = {kTaggedBase, offset, Handle<Name>(), Type::Any(),
                        kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForArgumentsCallee() {
  int offset =
      JSObject::kHeaderSize + Heap::kArgumentsCalleeIndex * kPointerSize;
  FieldAccess access = {kTaggedBase, offset, Handle<Name>(), Type::Any(),
                        kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForFixedArraySlot(size_t index) {
  int offset = FixedArray::OffsetOfElementAt(static_cast<int>(index));
  FieldAccess access = {kTaggedBase, offset, Handle<Name>(), Type::Any(),
                        kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForContextSlot(size_t index) {
  int offset = Context::kHeaderSize + static_cast<int>(index) * kPointerSize;
  DCHECK_EQ(offset,
            Context::SlotOffset(static_cast<int>(index)) + kHeapObjectTag);
  FieldAccess access = {kTaggedBase, offset, Handle<Name>(), Type::Any(),
                        kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForPropertyCellValue() {
  return ForPropertyCellValue(Type::Tagged());
}


// static
FieldAccess AccessBuilder::ForPropertyCellValue(Type* type) {
  FieldAccess access = {kTaggedBase, PropertyCell::kValueOffset, Handle<Name>(),
                        type, kMachAnyTagged};
  return access;
}


// static
FieldAccess AccessBuilder::ForSharedFunctionInfoTypeFeedbackVector() {
  FieldAccess access = {kTaggedBase, SharedFunctionInfo::kFeedbackVectorOffset,
                        Handle<Name>(), Type::Any(), kMachAnyTagged};
  return access;
}


// static
ElementAccess AccessBuilder::ForFixedArrayElement() {
  ElementAccess access = {kTaggedBase, FixedArray::kHeaderSize, Type::Any(),
                          kMachAnyTagged};
  return access;
}


// static
ElementAccess AccessBuilder::ForTypedArrayElement(ExternalArrayType type,
                                                  bool is_external) {
  BaseTaggedness taggedness = is_external ? kUntaggedBase : kTaggedBase;
  int header_size = is_external ? 0 : FixedTypedArrayBase::kDataOffset;
  switch (type) {
    case kExternalInt8Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              kMachInt8};
      return access;
    }
    case kExternalUint8Array:
    case kExternalUint8ClampedArray: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              kMachUint8};
      return access;
    }
    case kExternalInt16Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              kMachInt16};
      return access;
    }
    case kExternalUint16Array: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              kMachUint16};
      return access;
    }
    case kExternalInt32Array: {
      ElementAccess access = {taggedness, header_size, Type::Signed32(),
                              kMachInt32};
      return access;
    }
    case kExternalUint32Array: {
      ElementAccess access = {taggedness, header_size, Type::Unsigned32(),
                              kMachUint32};
      return access;
    }
    case kExternalFloat32Array: {
      ElementAccess access = {taggedness, header_size, Type::Number(),
                              kMachFloat32};
      return access;
    }
    case kExternalFloat64Array: {
      ElementAccess access = {taggedness, header_size, Type::Number(),
                              kMachFloat64};
      return access;
    }
  }
  UNREACHABLE();
  ElementAccess access = {kUntaggedBase, 0, Type::None(), kMachNone};
  return access;
}


// static
ElementAccess AccessBuilder::ForSeqStringChar(String::Encoding encoding) {
  switch (encoding) {
    case String::ONE_BYTE_ENCODING: {
      ElementAccess access = {kTaggedBase, SeqString::kHeaderSize,
                              Type::Unsigned32(), kMachUint8};
      return access;
    }
    case String::TWO_BYTE_ENCODING: {
      ElementAccess access = {kTaggedBase, SeqString::kHeaderSize,
                              Type::Unsigned32(), kMachUint16};
      return access;
    }
  }
  UNREACHABLE();
  ElementAccess access = {kUntaggedBase, 0, Type::None(), kMachNone};
  return access;
}


// static
FieldAccess AccessBuilder::ForStatsCounter() {
  FieldAccess access = {kUntaggedBase, 0, MaybeHandle<Name>(),
                        TypeCache::Get().kInt32, kMachInt32};
  return access;
}


// static
FieldAccess AccessBuilder::ForFrameCallerFramePtr() {
  FieldAccess access = {kUntaggedBase, StandardFrameConstants::kCallerFPOffset,
                        MaybeHandle<Name>(), Type::Internal(), kMachPtr};
  return access;
}


// static
FieldAccess AccessBuilder::ForFrameMarker() {
  FieldAccess access = {kUntaggedBase, StandardFrameConstants::kMarkerOffset,
                        MaybeHandle<Name>(), Type::Tagged(), kMachAnyTagged};
  return access;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
