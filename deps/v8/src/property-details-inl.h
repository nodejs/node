// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROPERTY_DETAILS_INL_H_
#define V8_PROPERTY_DETAILS_INL_H_

#include "src/conversions.h"
#include "src/objects.h"
#include "src/property-details.h"
#include "src/types.h"

namespace v8 {
namespace internal {

Representation Representation::FromType(Type* type) {
  DisallowHeapAllocation no_allocation;
  if (type->Is(Type::None())) return Representation::None();
  if (type->Is(Type::SignedSmall())) return Representation::Smi();
  if (type->Is(Type::Signed32())) return Representation::Integer32();
  if (type->Is(Type::Number())) return Representation::Double();
  return Representation::Tagged();
}

} }  // namespace v8::internal

#endif  // V8_PROPERTY_DETAILS_INL_H_
