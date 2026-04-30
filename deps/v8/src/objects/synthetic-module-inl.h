// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SYNTHETIC_MODULE_INL_H_
#define V8_OBJECTS_SYNTHETIC_MODULE_INL_H_

#include "src/objects/synthetic-module.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/synthetic-module-tq-inl.inc"

Tagged<String> SyntheticModule::name() const { return name_.load(); }
void SyntheticModule::set_name(Tagged<String> value, WriteBarrierMode mode) {
  name_.store(this, value, mode);
}

Tagged<FixedArray> SyntheticModule::export_names() const {
  return export_names_.load();
}
void SyntheticModule::set_export_names(Tagged<FixedArray> value,
                                       WriteBarrierMode mode) {
  export_names_.store(this, value, mode);
}

Tagged<Foreign> SyntheticModule::evaluation_steps() const {
  return evaluation_steps_.load();
}
void SyntheticModule::set_evaluation_steps(Tagged<Foreign> value,
                                           WriteBarrierMode mode) {
  evaluation_steps_.store(this, value, mode);
}
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SYNTHETIC_MODULE_INL_H_
