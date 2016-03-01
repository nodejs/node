// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_CACHE_H_
#define V8_TYPE_CACHE_H_

#include "src/types.h"

namespace v8 {
namespace internal {

class TypeCache final {
 private:
  // This has to be first for the initialization magic to work.
  Zone zone_;

 public:
  static TypeCache const& Get();

  TypeCache() = default;

  Type* const kInt8 =
      CreateNative(CreateRange<int8_t>(), Type::UntaggedIntegral8());
  Type* const kUint8 =
      CreateNative(CreateRange<uint8_t>(), Type::UntaggedIntegral8());
  Type* const kUint8Clamped = kUint8;
  Type* const kInt16 =
      CreateNative(CreateRange<int16_t>(), Type::UntaggedIntegral16());
  Type* const kUint16 =
      CreateNative(CreateRange<uint16_t>(), Type::UntaggedIntegral16());
  Type* const kInt32 =
      CreateNative(Type::Signed32(), Type::UntaggedIntegral32());
  Type* const kUint32 =
      CreateNative(Type::Unsigned32(), Type::UntaggedIntegral32());
  Type* const kFloat32 = CreateNative(Type::Number(), Type::UntaggedFloat32());
  Type* const kFloat64 = CreateNative(Type::Number(), Type::UntaggedFloat64());

  Type* const kSmi = CreateNative(Type::SignedSmall(), Type::TaggedSigned());
  Type* const kHeapNumber = CreateNative(Type::Number(), Type::TaggedPointer());

  Type* const kSingletonZero = CreateRange(0.0, 0.0);
  Type* const kSingletonOne = CreateRange(1.0, 1.0);
  Type* const kZeroOrOne = CreateRange(0.0, 1.0);
  Type* const kZeroToThirtyOne = CreateRange(0.0, 31.0);
  Type* const kZeroToThirtyTwo = CreateRange(0.0, 32.0);
  Type* const kZeroish =
      Type::Union(kSingletonZero, Type::MinusZeroOrNaN(), zone());
  Type* const kInteger = CreateRange(-V8_INFINITY, V8_INFINITY);
  Type* const kPositiveInteger = CreateRange(0.0, V8_INFINITY);
  Type* const kIntegerOrMinusZero =
      Type::Union(kInteger, Type::MinusZero(), zone());
  Type* const kIntegerOrMinusZeroOrNaN =
      Type::Union(kIntegerOrMinusZero, Type::NaN(), zone());

  Type* const kAdditiveSafeInteger =
      CreateRange(-4503599627370496.0, 4503599627370496.0);
  Type* const kSafeInteger = CreateRange(-kMaxSafeInteger, kMaxSafeInteger);
  Type* const kPositiveSafeInteger = CreateRange(0.0, kMaxSafeInteger);

  Type* const kUntaggedUndefined =
      Type::Intersect(Type::Undefined(), Type::Untagged(), zone());

  // Asm.js related types.
  Type* const kAsmSigned = kInt32;
  Type* const kAsmUnsigned = kUint32;
  Type* const kAsmInt = Type::Union(kAsmSigned, kAsmUnsigned, zone());
  Type* const kAsmFixnum = Type::Intersect(kAsmSigned, kAsmUnsigned, zone());
  Type* const kAsmFloat = kFloat32;
  Type* const kAsmDouble = kFloat64;
  Type* const kAsmFloatQ = Type::Union(kAsmFloat, kUntaggedUndefined, zone());
  Type* const kAsmDoubleQ = Type::Union(kAsmDouble, kUntaggedUndefined, zone());
  // Not part of the Asm.js type hierarchy, but represents a part of what
  // intish encompasses.
  Type* const kAsmIntQ = Type::Union(kAsmInt, kUntaggedUndefined, zone());
  Type* const kAsmFloatDoubleQ = Type::Union(kAsmFloatQ, kAsmDoubleQ, zone());
  // Asm.js size unions.
  Type* const kAsmSize8 = Type::Union(kInt8, kUint8, zone());
  Type* const kAsmSize16 = Type::Union(kInt16, kUint16, zone());
  Type* const kAsmSize32 =
      Type::Union(Type::Union(kInt32, kUint32, zone()), kAsmFloat, zone());
  Type* const kAsmSize64 = kFloat64;
  // Asm.js other types.
  Type* const kAsmComparable = Type::Union(
      kAsmSigned,
      Type::Union(kAsmUnsigned, Type::Union(kAsmDouble, kAsmFloat, zone()),
                  zone()),
      zone());
  Type* const kAsmIntArrayElement =
      Type::Union(Type::Union(kInt8, kUint8, zone()),
                  Type::Union(Type::Union(kInt16, kUint16, zone()),
                              Type::Union(kInt32, kUint32, zone()), zone()),
                  zone());

  // The FixedArray::length property always containts a smi in the range
  // [0, FixedArray::kMaxLength].
  Type* const kFixedArrayLengthType = CreateNative(
      CreateRange(0.0, FixedArray::kMaxLength), Type::TaggedSigned());

  // The FixedDoubleArray::length property always containts a smi in the range
  // [0, FixedDoubleArray::kMaxLength].
  Type* const kFixedDoubleArrayLengthType = CreateNative(
      CreateRange(0.0, FixedDoubleArray::kMaxLength), Type::TaggedSigned());

  // The JSArray::length property always contains a tagged number in the range
  // [0, kMaxUInt32].
  Type* const kJSArrayLengthType =
      CreateNative(Type::Unsigned32(), Type::Tagged());

  // The String::length property always contains a smi in the range
  // [0, String::kMaxLength].
  Type* const kStringLengthType =
      CreateNative(CreateRange(0.0, String::kMaxLength), Type::TaggedSigned());

  // When initializing arrays, we'll unfold the loop if the number of
  // elements is known to be of this type.
  Type* const kElementLoopUnrollType = CreateRange(0.0, 16.0);

#define TYPED_ARRAY(TypeName, type_name, TYPE_NAME, ctype, size) \
  Type* const k##TypeName##Array = CreateArray(k##TypeName);
  TYPED_ARRAYS(TYPED_ARRAY)
#undef TYPED_ARRAY

 private:
  Type* CreateArray(Type* element) { return Type::Array(element, zone()); }

  Type* CreateArrayFunction(Type* array) {
    Type* arg1 = Type::Union(Type::Unsigned32(), Type::Object(), zone());
    Type* arg2 = Type::Union(Type::Unsigned32(), Type::Undefined(), zone());
    Type* arg3 = arg2;
    return Type::Function(array, arg1, arg2, arg3, zone());
  }

  Type* CreateNative(Type* semantic, Type* representation) {
    return Type::Intersect(semantic, representation, zone());
  }

  template <typename T>
  Type* CreateRange() {
    return CreateRange(std::numeric_limits<T>::min(),
                       std::numeric_limits<T>::max());
  }

  Type* CreateRange(double min, double max) {
    return Type::Range(min, max, zone());
  }

  Zone* zone() { return &zone_; }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TYPE_CACHE_H_
