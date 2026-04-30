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

Tagged<JSFunction> JSGeneratorObject::function() const {
  return function_.load();
}
void JSGeneratorObject::set_function(Tagged<JSFunction> value,
                                     WriteBarrierMode mode) {
  function_.store(this, value, mode);
}

Tagged<Context> JSGeneratorObject::context() const { return context_.load(); }
void JSGeneratorObject::set_context(Tagged<Context> value,
                                    WriteBarrierMode mode) {
  context_.store(this, value, mode);
}

Tagged<JSAny> JSGeneratorObject::receiver() const { return receiver_.load(); }
void JSGeneratorObject::set_receiver(Tagged<JSAny> value,
                                     WriteBarrierMode mode) {
  receiver_.store(this, value, mode);
}

Tagged<Object> JSGeneratorObject::input_or_debug_pos() const {
  return input_or_debug_pos_.load();
}
void JSGeneratorObject::set_input_or_debug_pos(Tagged<Object> value,
                                               WriteBarrierMode mode) {
  input_or_debug_pos_.store(this, value, mode);
}

int JSGeneratorObject::resume_mode() const {
  return resume_mode_.load().value();
}
void JSGeneratorObject::set_resume_mode(int value) {
  resume_mode_.store(this, Smi::FromInt(value));
}

int JSGeneratorObject::continuation() const {
  return continuation_.load().value();
}
void JSGeneratorObject::set_continuation(int value) {
  continuation_.store(this, Smi::FromInt(value));
}

Tagged<FixedArray> JSGeneratorObject::parameters_and_registers() const {
  return parameters_and_registers_.load();
}
void JSGeneratorObject::set_parameters_and_registers(Tagged<FixedArray> value,
                                                     WriteBarrierMode mode) {
  parameters_and_registers_.store(this, value, mode);
}

Tagged<JSPromise> JSAsyncFunctionObject::promise() const {
  return promise_.load();
}
void JSAsyncFunctionObject::set_promise(Tagged<JSPromise> value,
                                        WriteBarrierMode mode) {
  promise_.store(this, value, mode);
}

Tagged<UnionOf<JSFunction, Undefined>>
JSAsyncFunctionObject::await_resolve_closure() const {
  return await_resolve_closure_.load();
}
void JSAsyncFunctionObject::set_await_resolve_closure(
    Tagged<UnionOf<JSFunction, Undefined>> value, WriteBarrierMode mode) {
  await_resolve_closure_.store(this, value, mode);
}

Tagged<UnionOf<JSFunction, Undefined>>
JSAsyncFunctionObject::await_reject_closure() const {
  return await_reject_closure_.load();
}
void JSAsyncFunctionObject::set_await_reject_closure(
    Tagged<UnionOf<JSFunction, Undefined>> value, WriteBarrierMode mode) {
  await_reject_closure_.store(this, value, mode);
}

Tagged<UnionOf<AsyncGeneratorRequest, Undefined>>
JSAsyncGeneratorObject::queue() const {
  return queue_.load();
}
void JSAsyncGeneratorObject::set_queue(
    Tagged<UnionOf<AsyncGeneratorRequest, Undefined>> value,
    WriteBarrierMode mode) {
  queue_.store(this, value, mode);
}

int JSAsyncGeneratorObject::is_awaiting() const {
  return is_awaiting_.load().value();
}
void JSAsyncGeneratorObject::set_is_awaiting(int value) {
  is_awaiting_.store(this, Smi::FromInt(value));
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
