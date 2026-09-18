// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_DETAILS_INL_H_
#define V8_OBJECTS_PROPERTY_DETAILS_INL_H_

#include "src/objects/property-details.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/logging.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

PropertyDetails::PropertyDetails(Tagged<Smi> smi) { value_ = smi.value(); }

Tagged<Smi> PropertyDetails::AsSmi() const {
  // Ensure the upper 2 bits have the same value by sign extending it. This is
  // necessary to be able to use the 31st bit of the property details.
  int value = value_ << 1;
  return Smi::FromInt(value >> 1);
}

int PropertyDetails::field_width_in_words() const {
  DCHECK_EQ(location(), PropertyLocation::kField);
  return 1;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_PROPERTY_DETAILS_INL_H_
