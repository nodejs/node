// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECT_CONVERSIONS_INL_H_
#define V8_OBJECTS_OBJECT_CONVERSIONS_INL_H_

#include "src/common/globals.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/casting.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/objects.h"
#include "src/objects/smi.h"

namespace v8::internal {

// static
double Object::NumberValue(Tagged<Number> obj) {
  if (IsSmi(obj)) {
    return static_cast<double>(UncheckedCast<Smi>(obj).value());
  }
  DCHECK(IsHeapNumber(obj));
  return UncheckedCast<HeapNumber>(obj)->value();
}
// TODO(leszeks): Remove in favour of Tagged<Number>
// static
double Object::NumberValue(Tagged<Object> obj) {
  return NumberValue(UncheckedCast<Number>(obj));
}
double Object::NumberValue(Tagged<HeapNumber> obj) {
  return NumberValue(UncheckedCast<Number>(obj));
}
double Object::NumberValue(Tagged<Smi> obj) {
  return NumberValue(UncheckedCast<Number>(obj));
}

// static
bool Object::ToUint32(Tagged<Object> obj, uint32_t* value) {
  if (IsSmi(obj)) {
    int num = Smi::ToInt(obj);
    if (num < 0) return false;
    *value = static_cast<uint32_t>(num);
    return true;
  }
  if (IsHeapNumber(obj)) {
    double num = Cast<HeapNumber>(obj)->value();
    return DoubleToUint32IfEqualToSelf(num, value);
  }
  return false;
}

// static
bool Object::ToArrayLength(Tagged<Object> obj, uint32_t* index) {
  return Object::ToUint32(obj, index);
}

// static
bool Object::ToArrayIndex(Tagged<Object> obj, uint32_t* index) {
  return Object::ToUint32(obj, index) && *index != kMaxUInt32;
}

// static
bool Object::ToIntegerIndex(Tagged<Object> obj, size_t* index) {
  if (IsSmi(obj)) {
    int num = Smi::ToInt(obj);
    if (num < 0) return false;
    *index = static_cast<size_t>(num);
    return true;
  }
  if (IsHeapNumber(obj)) {
    double num = Cast<HeapNumber>(obj)->value();
    if (!(num >= 0)) return false;  // Negation to catch NaNs.
    constexpr double max =
        std::min(kMaxSafeInteger,
                 // The maximum size_t is reserved as "invalid" sentinel.
                 static_cast<double>(std::numeric_limits<size_t>::max() - 1));
    if (num > max) return false;
    size_t result = static_cast<size_t>(num);
    if (num != result) return false;  // Conversion lost fractional precision.
    *index = result;
    return true;
  }
  return false;
}

}  // namespace v8::internal

#endif  // V8_OBJECTS_OBJECT_CONVERSIONS_INL_H_
