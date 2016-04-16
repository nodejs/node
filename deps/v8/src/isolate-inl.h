// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ISOLATE_INL_H_
#define V8_ISOLATE_INL_H_

#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


void Isolate::set_context(Context* context) {
  DCHECK(context == NULL || context->IsContext());
  thread_local_top_.context_ = context;
}


Object* Isolate::pending_exception() {
  DCHECK(has_pending_exception());
  DCHECK(!thread_local_top_.pending_exception_->IsException());
  return thread_local_top_.pending_exception_;
}


void Isolate::set_pending_exception(Object* exception_obj) {
  DCHECK(!exception_obj->IsException());
  thread_local_top_.pending_exception_ = exception_obj;
}


void Isolate::clear_pending_exception() {
  DCHECK(!thread_local_top_.pending_exception_->IsException());
  thread_local_top_.pending_exception_ = heap_.the_hole_value();
}


bool Isolate::has_pending_exception() {
  DCHECK(!thread_local_top_.pending_exception_->IsException());
  return !thread_local_top_.pending_exception_->IsTheHole();
}


void Isolate::clear_pending_message() {
  thread_local_top_.pending_message_obj_ = heap_.the_hole_value();
}


Object* Isolate::scheduled_exception() {
  DCHECK(has_scheduled_exception());
  DCHECK(!thread_local_top_.scheduled_exception_->IsException());
  return thread_local_top_.scheduled_exception_;
}


bool Isolate::has_scheduled_exception() {
  DCHECK(!thread_local_top_.scheduled_exception_->IsException());
  return thread_local_top_.scheduled_exception_ != heap_.the_hole_value();
}


void Isolate::clear_scheduled_exception() {
  DCHECK(!thread_local_top_.scheduled_exception_->IsException());
  thread_local_top_.scheduled_exception_ = heap_.the_hole_value();
}


bool Isolate::is_catchable_by_javascript(Object* exception) {
  return exception != heap()->termination_exception();
}


Handle<JSGlobalObject> Isolate::global_object() {
  return Handle<JSGlobalObject>(context()->global_object(), this);
}


Isolate::ExceptionScope::ExceptionScope(Isolate* isolate)
    : isolate_(isolate),
      pending_exception_(isolate_->pending_exception(), isolate_) {}


Isolate::ExceptionScope::~ExceptionScope() {
  isolate_->set_pending_exception(*pending_exception_);
}


#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name) \
  Handle<type> Isolate::name() {                         \
    return Handle<type>(native_context()->name(), this); \
  }                                                      \
  bool Isolate::is_##name(type* value) {                 \
    return native_context()->is_##name(value);           \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
#undef NATIVE_CONTEXT_FIELD_ACCESSOR


}  // namespace internal
}  // namespace v8

#endif  // V8_ISOLATE_INL_H_
