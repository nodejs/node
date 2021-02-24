// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_INL_H_
#define V8_EXECUTION_ISOLATE_INL_H_

#include "src/execution/isolate.h"
#include "src/objects/cell-inl.h"
#include "src/objects/js-function.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell.h"
#include "src/objects/regexp-match-info.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

void Isolate::set_context(Context context) {
  DCHECK(context.is_null() || context.IsContext());
  thread_local_top()->context_ = context;
}

Handle<NativeContext> Isolate::native_context() {
  DCHECK(!context().is_null());
  return handle(context().native_context(), this);
}

NativeContext Isolate::raw_native_context() {
  DCHECK(!context().is_null());
  return context().native_context();
}

Object Isolate::pending_exception() {
  DCHECK(has_pending_exception());
  DCHECK(!thread_local_top()->pending_exception_.IsException(this));
  return thread_local_top()->pending_exception_;
}

void Isolate::set_pending_exception(Object exception_obj) {
  DCHECK(!exception_obj.IsException(this));
  thread_local_top()->pending_exception_ = exception_obj;
}

void Isolate::clear_pending_exception() {
  DCHECK(!thread_local_top()->pending_exception_.IsException(this));
  thread_local_top()->pending_exception_ = ReadOnlyRoots(this).the_hole_value();
}

bool Isolate::has_pending_exception() {
  DCHECK(!thread_local_top()->pending_exception_.IsException(this));
  return !thread_local_top()->pending_exception_.IsTheHole(this);
}

void Isolate::clear_pending_message() {
  thread_local_top()->pending_message_obj_ =
      ReadOnlyRoots(this).the_hole_value();
}

Object Isolate::scheduled_exception() {
  DCHECK(has_scheduled_exception());
  DCHECK(!thread_local_top()->scheduled_exception_.IsException(this));
  return thread_local_top()->scheduled_exception_;
}

bool Isolate::has_scheduled_exception() {
  DCHECK(!thread_local_top()->scheduled_exception_.IsException(this));
  return thread_local_top()->scheduled_exception_ !=
         ReadOnlyRoots(this).the_hole_value();
}

void Isolate::clear_scheduled_exception() {
  DCHECK(!thread_local_top()->scheduled_exception_.IsException(this));
  thread_local_top()->scheduled_exception_ =
      ReadOnlyRoots(this).the_hole_value();
}

bool Isolate::is_catchable_by_javascript(Object exception) {
  return exception != ReadOnlyRoots(heap()).termination_exception();
}

bool Isolate::is_catchable_by_wasm(Object exception) {
  if (!is_catchable_by_javascript(exception)) return false;
  if (!exception.IsJSObject()) return true;
  // We don't allocate, but the LookupIterator interface expects a handle.
  DisallowGarbageCollection no_gc;
  HandleScope handle_scope(this);
  LookupIterator it(this, handle(JSReceiver::cast(exception), this),
                    factory()->wasm_uncatchable_symbol(),
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  return !JSReceiver::HasProperty(&it).FromJust();
}

void Isolate::FireBeforeCallEnteredCallback() {
  for (auto& callback : before_call_entered_callbacks_) {
    callback(reinterpret_cast<v8::Isolate*>(this));
  }
}

Handle<JSGlobalObject> Isolate::global_object() {
  return handle(context().global_object(), this);
}

Handle<JSGlobalProxy> Isolate::global_proxy() {
  return handle(context().global_proxy(), this);
}

Isolate::ExceptionScope::ExceptionScope(Isolate* isolate)
    : isolate_(isolate),
      pending_exception_(isolate_->pending_exception(), isolate_) {}

Isolate::ExceptionScope::~ExceptionScope() {
  isolate_->set_pending_exception(*pending_exception_);
}

#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name)    \
  Handle<type> Isolate::name() {                            \
    return Handle<type>(raw_native_context().name(), this); \
  }                                                         \
  bool Isolate::is_##name(type value) {                     \
    return raw_native_context().is_##name(value);           \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
#undef NATIVE_CONTEXT_FIELD_ACCESSOR

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_INL_H_
