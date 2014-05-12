// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROPERTY_DETAILS_INL_H_
#define V8_PROPERTY_DETAILS_INL_H_

#include "conversions.h"
#include "objects.h"
#include "property-details.h"

namespace v8 {
namespace internal {

inline bool Representation::CanContainDouble(double value) {
  if (IsDouble() || is_more_general_than(Representation::Double())) {
    return true;
  }
  if (IsInt32Double(value)) {
    if (IsInteger32()) return true;
    if (IsSmi()) return Smi::IsValid(static_cast<int32_t>(value));
  }
  return false;
}

} }  // namespace v8::internal

#endif  // V8_PROPERTY_DETAILS_INL_H_
