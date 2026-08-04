// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_
#define V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_

#include "src/objects/template-objects.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/template-objects-tq-inl.inc"

Tagged<FixedArray> TemplateObjectDescription::raw_strings() const {
  return raw_strings_.load();
}
void TemplateObjectDescription::set_raw_strings(Tagged<FixedArray> value,
                                                WriteBarrierMode mode) {
  raw_strings_.store(this, value, mode);
}

Tagged<FixedArray> TemplateObjectDescription::cooked_strings() const {
  return cooked_strings_.load();
}
void TemplateObjectDescription::set_cooked_strings(Tagged<FixedArray> value,
                                                   WriteBarrierMode mode) {
  cooked_strings_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_
