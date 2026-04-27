// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_GENERATOR_INL_H_
#define V8_OBJECTS_JS_GENERATOR_INL_H_

#include "src/objects/js-generator.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-promise-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-generator-tq-inl.inc"

Tagged<UnionOf<AsyncGeneratorRequest, Undefined>> AsyncGeneratorRequest::next()
    const {
  return next_.load();
}
void AsyncGeneratorRequest::set_next(
    Tagged<UnionOf<AsyncGeneratorRequest, Undefined>> value,
    WriteBarrierMode mode) {
  next_.store(this, value, mode);
}

int AsyncGeneratorRequest::resume_mode() const {
  return resume_mode_.load().value();
}
void AsyncGeneratorRequest::set_resume_mode(int value) {
  resume_mode_.store(this, Smi::FromInt(value));
}

Tagged<Object> AsyncGeneratorRequest::value() const { return value_.load(); }
void AsyncGeneratorRequest::set_value(Tagged<Object> value,
                                      WriteBarrierMode mode) {
  value_.store(this, value, mode);
}

Tagged<JSPromise> AsyncGeneratorRequest::promise() const {
  return promise_.load();
}
void AsyncGeneratorRequest::set_promise(Tagged<JSPromise> value,
                                        WriteBarrierMode mode) {
  promise_.store(this, value, mode);
}

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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_GENERATOR_INL_H_
