// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MEGADOM_HANDLER_INL_H_
#define V8_OBJECTS_MEGADOM_HANDLER_INL_H_

#include "src/objects/megadom-handler.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/megadom-handler-tq-inl.inc"

Tagged<MaybeObject> MegaDomHandler::accessor() const {
  return accessor_.load();
}

Tagged<MaybeObject> MegaDomHandler::accessor(AcquireLoadTag) const {
  return accessor_.Acquire_Load();
}

void MegaDomHandler::set_accessor(Tagged<MaybeObject> value,
                                  WriteBarrierMode mode) {
  accessor_.store(this, value, mode);
}

void MegaDomHandler::set_accessor(Tagged<MaybeObject> value, ReleaseStoreTag,
                                  WriteBarrierMode mode) {
  accessor_.Release_Store(this, value, mode);
}

Tagged<MaybeObject> MegaDomHandler::context() const { return context_.load(); }

void MegaDomHandler::set_context(Tagged<MaybeObject> value,
                                 WriteBarrierMode mode) {
  context_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MEGADOM_HANDLER_INL_H_
