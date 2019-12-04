// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_GENERATOR_INL_H_
#define V8_OBJECTS_JS_GENERATOR_INL_H_

#include "src/objects/js-generator.h"
#include "src/objects/js-promise-inl.h"

#include "src/objects/objects-inl.h"  // Needed for write barriers

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(JSGeneratorObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSAsyncFunctionObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSAsyncGeneratorObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(AsyncGeneratorRequest)

TQ_SMI_ACCESSORS(JSGeneratorObject, resume_mode)
TQ_SMI_ACCESSORS(JSGeneratorObject, continuation)

TQ_SMI_ACCESSORS(AsyncGeneratorRequest, resume_mode)

bool JSGeneratorObject::is_suspended() const {
  DCHECK_LT(kGeneratorExecuting, 0);
  DCHECK_LT(kGeneratorClosed, 0);
  return continuation() >= 0;
}

bool JSGeneratorObject::is_closed() const {
  return continuation() == kGeneratorClosed;
}

bool JSGeneratorObject::is_executing() const {
  return continuation() == kGeneratorExecuting;
}

TQ_SMI_ACCESSORS(JSAsyncGeneratorObject, is_awaiting)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_GENERATOR_INL_H_
