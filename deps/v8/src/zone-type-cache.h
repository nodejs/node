// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_TYPE_CACHE_H_
#define V8_ZONE_TYPE_CACHE_H_


#include "src/types.h"

namespace v8 {
namespace internal {

class ZoneTypeCache final {
 private:
  // This has to be first for the initialization magic to work.
  Zone zone_;

 public:
  ZoneTypeCache() = default;

  Type* const kInt8 =
      CreateNative(CreateRange<int8_t>(), Type::UntaggedSigned8());
  Type* const kUint8 =
      CreateNative(CreateRange<uint8_t>(), Type::UntaggedUnsigned8());
  Type* const kUint8Clamped = kUint8;
  Type* const kInt16 =
      CreateNative(CreateRange<int16_t>(), Type::UntaggedSigned16());
  Type* const kUint16 =
      CreateNative(CreateRange<uint16_t>(), Type::UntaggedUnsigned16());
  Type* const kInt32 = CreateNative(Type::Signed32(), Type::UntaggedSigned32());
  Type* const kUint32 =
      CreateNative(Type::Unsigned32(), Type::UntaggedUnsigned32());
  Type* const kFloat32 = CreateNative(Type::Number(), Type::UntaggedFloat32());
  Type* const kFloat64 = CreateNative(Type::Number(), Type::UntaggedFloat64());

  Type* const kSingletonZero = CreateRange(0.0, 0.0);
  Type* const kSingletonOne = CreateRange(1.0, 1.0);
  Type* const kZeroOrOne = CreateRange(0.0, 1.0);
  Type* const kZeroish =
      Type::Union(kSingletonZero, Type::MinusZeroOrNaN(), zone());
  Type* const kInteger = CreateRange(-V8_INFINITY, V8_INFINITY);
  Type* const kWeakint = Type::Union(kInteger, Type::MinusZeroOrNaN(), zone());
  Type* const kWeakintFunc1 = Type::Function(kWeakint, Type::Number(), zone());

  Type* const kRandomFunc0 = Type::Function(Type::OrderedNumber(), zone());
  Type* const kAnyFunc0 = Type::Function(Type::Any(), zone());
  Type* const kAnyFunc1 = Type::Function(Type::Any(), Type::Any(), zone());
  Type* const kAnyFunc2 =
      Type::Function(Type::Any(), Type::Any(), Type::Any(), zone());
  Type* const kAnyFunc3 = Type::Function(Type::Any(), Type::Any(), Type::Any(),
                                         Type::Any(), zone());
  Type* const kNumberFunc0 = Type::Function(Type::Number(), zone());
  Type* const kNumberFunc1 =
      Type::Function(Type::Number(), Type::Number(), zone());
  Type* const kNumberFunc2 =
      Type::Function(Type::Number(), Type::Number(), Type::Number(), zone());
  Type* const kImulFunc = Type::Function(Type::Signed32(), Type::Integral32(),
                                         Type::Integral32(), zone());
  Type* const kClz32Func =
      Type::Function(CreateRange(0, 32), Type::Number(), zone());

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

#endif  // V8_ZONE_TYPE_CACHE_H_
