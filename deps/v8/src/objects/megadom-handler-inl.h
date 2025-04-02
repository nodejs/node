// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MEGADOM_HANDLER_INL_H_
#define V8_OBJECTS_MEGADOM_HANDLER_INL_H_

#include "src/objects/megadom-handler.h"
#include "src/objects/objects-inl.h"  // Needed for write barriers

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/megadom-handler-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(MegaDomHandler)

RELEASE_ACQUIRE_ACCESSORS(MegaDomHandler, accessor, Tagged<MaybeObject>,
                          kAccessorOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MEGADOM_HANDLER_INL_H_
