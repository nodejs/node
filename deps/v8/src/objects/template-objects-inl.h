// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_
#define V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_

#include "src/objects/template-objects.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/heap-object-set-map-inl.h"
#include "src/objects/js-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TemplateObjectDescription::TemplateObjectDescription(
    const AllocationWitness& witness, Tagged<FixedArray> raw_strings,
    Tagged<FixedArray> cooked_strings)
    : Struct(witness.roots().template_object_description_map()),
      raw_strings_(witness, raw_strings),
      cooked_strings_(witness, cooked_strings) {}

Tagged<FixedArray> TemplateObjectDescription::raw_strings() const {
  return raw_strings_.load();
}

Tagged<FixedArray> TemplateObjectDescription::cooked_strings() const {
  return cooked_strings_.load();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_
