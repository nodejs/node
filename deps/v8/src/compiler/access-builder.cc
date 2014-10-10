// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/types-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
FieldAccess AccessBuilder::ForMap() {
  return {kTaggedBase, HeapObject::kMapOffset, Handle<Name>(), Type::Any(),
          kMachAnyTagged};
}


// static
FieldAccess AccessBuilder::ForJSObjectProperties() {
  return {kTaggedBase, JSObject::kPropertiesOffset, Handle<Name>(), Type::Any(),
          kMachAnyTagged};
}


// static
FieldAccess AccessBuilder::ForJSObjectElements() {
  return {kTaggedBase, JSObject::kElementsOffset, Handle<Name>(),
          Type::Internal(), kMachAnyTagged};
}


// static
FieldAccess AccessBuilder::ForJSFunctionContext() {
  return {kTaggedBase, JSFunction::kContextOffset, Handle<Name>(),
          Type::Internal(), kMachAnyTagged};
}


// static
FieldAccess AccessBuilder::ForJSArrayBufferBackingStore() {
  return {kTaggedBase, JSArrayBuffer::kBackingStoreOffset, Handle<Name>(),
          Type::UntaggedPtr(), kMachPtr};
}


// static
FieldAccess AccessBuilder::ForExternalArrayPointer() {
  return {kTaggedBase, ExternalArray::kExternalPointerOffset, Handle<Name>(),
          Type::UntaggedPtr(), kMachPtr};
}


// static
ElementAccess AccessBuilder::ForFixedArrayElement() {
  return {kTaggedBase, FixedArray::kHeaderSize, Type::Any(), kMachAnyTagged};
}


// static
ElementAccess AccessBuilder::ForTypedArrayElement(ExternalArrayType type,
                                                  bool is_external) {
  BaseTaggedness taggedness = is_external ? kUntaggedBase : kTaggedBase;
  int header_size = is_external ? 0 : FixedTypedArrayBase::kDataOffset;
  switch (type) {
    case kExternalInt8Array:
      return {taggedness, header_size, Type::Signed32(), kMachInt8};
    case kExternalUint8Array:
    case kExternalUint8ClampedArray:
      return {taggedness, header_size, Type::Unsigned32(), kMachUint8};
    case kExternalInt16Array:
      return {taggedness, header_size, Type::Signed32(), kMachInt16};
    case kExternalUint16Array:
      return {taggedness, header_size, Type::Unsigned32(), kMachUint16};
    case kExternalInt32Array:
      return {taggedness, header_size, Type::Signed32(), kMachInt32};
    case kExternalUint32Array:
      return {taggedness, header_size, Type::Unsigned32(), kMachUint32};
    case kExternalFloat32Array:
      return {taggedness, header_size, Type::Number(), kRepFloat32};
    case kExternalFloat64Array:
      return {taggedness, header_size, Type::Number(), kRepFloat64};
  }
  UNREACHABLE();
  return {kUntaggedBase, 0, Type::None(), kMachNone};
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
