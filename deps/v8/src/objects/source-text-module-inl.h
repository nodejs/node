// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SOURCE_TEXT_MODULE_INL_H_
#define V8_OBJECTS_SOURCE_TEXT_MODULE_INL_H_

#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/source-text-module.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/source-text-module-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(ModuleRequest)
TQ_OBJECT_CONSTRUCTORS_IMPL(SourceTextModule)
TQ_OBJECT_CONSTRUCTORS_IMPL(SourceTextModuleInfoEntry)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SOURCE_TEXT_MODULE_INL_H_
