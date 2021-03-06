// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROPERTY_CELL_INL_H_
#define V8_OBJECTS_PROPERTY_CELL_INL_H_

#include "src/objects/property-cell.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/code-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(PropertyCell, HeapObject)

CAST_ACCESSOR(PropertyCell)

ACCESSORS(PropertyCell, dependent_code, DependentCode, kDependentCodeOffset)
ACCESSORS(PropertyCell, name, Name, kNameOffset)
ACCESSORS(PropertyCell, value, Object, kValueOffset)
ACCESSORS(PropertyCell, property_details_raw, Smi, kPropertyDetailsRawOffset)

PropertyDetails PropertyCell::property_details() const {
  return PropertyDetails(Smi::cast(property_details_raw()));
}

void PropertyCell::set_property_details(PropertyDetails details) {
  set_property_details_raw(details.AsSmi());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROPERTY_CELL_INL_H_
