// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ARGUMENTS_INL_H_
#define V8_OBJECTS_ARGUMENTS_INL_H_

#include "src/execution/isolate-inl.h"
#include "src/objects/arguments.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(JSArgumentsObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(AliasedArgumentsEntry)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ARGUMENTS_INL_H_
