// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INTL_OBJECTS_INL_H_
#define V8_OBJECTS_INTL_OBJECTS_INL_H_

#include "src/objects/intl-objects.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {

inline Intl::Type Intl::TypeFromInt(int type_int) {
  STATIC_ASSERT(Intl::Type::kNumberFormat == 0);
  DCHECK_LE(Intl::Type::kNumberFormat, type_int);
  DCHECK_GT(Intl::Type::kTypeCount, type_int);
  return static_cast<Intl::Type>(type_int);
}

inline Intl::Type Intl::TypeFromSmi(Smi* type) {
  return TypeFromInt(Smi::ToInt(type));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_INTL_OBJECTS_INL_H_
