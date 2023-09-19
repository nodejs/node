// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_INL_H_
#define V8_EXECUTION_ISOLATE_INL_H_

#include "src/execution/isolate.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/js-function.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell.h"
#include "src/objects/regexp-match-info.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/source-text-module-inl.h"

#ifdef DEBUG
#include "src/common/ptr-compr-inl.h"
#include "src/runtime/runtime-utils.h"
#endif

namespace v8 {
namespace internal {

// static
V8_INLINE Isolate::PerIsolateThreadData*
Isolate::CurrentPerIsolateThreadData() {
  return g_current_per_isolate_thread_data_;
}

// static
V8_INLINE Isolate* Isolate::TryGetCurrent() { return g_current_isolate_; }

// static
V8_INLINE Isolate* Isolate::Current() {
  Isolate* isolate = TryGetCurrent();
  DCHECK_NOT_NULL(isolate);
  return isolate;
}

bool Isolate::IsCurrent() const { return this == TryGetCurrent(); }

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

void Isolate::set_pending_message(Object message_obj) {
  DCHECK(message_obj.IsTheHole(this) || message_obj.IsJSMessageObject());
  thread_local_top()->pending_message_ = message_obj;
}

Object Isolate::pending_message() {
  return thread_local_top()->pending_message_;
}

void Isolate::clear_pending_message() {
  set_pending_message(ReadOnlyRoots(this).the_hole_value());
}

bool Isolate::has_pending_message() {
  return !pending_message().IsTheHole(this);
}

Object Isolate::pending_exception() {
  CHECK(has_pending_exception());
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
  set_scheduled_exception(ReadOnlyRoots(this).the_hole_value());
}

void Isolate::set_scheduled_exception(Object exception) {
  thread_local_top()->scheduled_exception_ = exception;
}

bool Isolate::is_execution_termination_pending() {
  return thread_local_top()->pending_exception_ ==
         i::ReadOnlyRoots(this).termination_exception();
}

bool Isolate::is_execution_terminating() {
  return thread_local_top()->scheduled_exception_ ==
         i::ReadOnlyRoots(this).termination_exception();
}

#ifdef DEBUG
Object Isolate::VerifyBuiltinsResult(Object result) {
  DCHECK_EQ(has_pending_exception(), result == ReadOnlyRoots(this).exception());
#ifdef V8_COMPRESS_POINTERS
  // Check that the returned pointer is actually part of the current isolate,
  // because that's the assumption in generated code (which might call this
  // builtin).
  if (!result.IsSmi()) {
    DCHECK_EQ(result.ptr(), V8HeapCompressionScheme::DecompressTagged(
                                this, static_cast<Tagged_t>(result.ptr())));
  }
#endif
  return result;
}

ObjectPair Isolate::VerifyBuiltinsResult(ObjectPair pair) {
#ifdef V8_HOST_ARCH_64_BIT
  DCHECK_EQ(has_pending_exception(),
            pair.x == ReadOnlyRoots(this).exception().ptr());
#ifdef V8_COMPRESS_POINTERS
  // Check that the returned pointer is actually part of the current isolate,
  // because that's the assumption in generated code (which might call this
  // builtin).
  if (!HAS_SMI_TAG(pair.x)) {
    DCHECK_EQ(pair.x, V8HeapCompressionScheme::DecompressTagged(
                          this, static_cast<Tagged_t>(pair.x)));
  }
  if (!HAS_SMI_TAG(pair.y)) {
    DCHECK_EQ(pair.y, V8HeapCompressionScheme::DecompressTagged(
                          this, static_cast<Tagged_t>(pair.y)));
  }
#endif  // V8_COMPRESS_POINTERS
#endif  // V8_HOST_ARCH_64_BIT
  return pair;
}
#endif  // DEBUG

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

bool Isolate::IsAnyInitialArrayPrototype(JSArray array) {
  DisallowGarbageCollection no_gc;
  return IsInAnyContext(array, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
}

void Isolate::DidFinishModuleAsyncEvaluation(unsigned ordinal) {
  // To address overflow, the ordinal is reset when the async module with the
  // largest vended ordinal finishes evaluating. Modules are evaluated in
  // ascending order of their async_evaluating_ordinal.
  //
  // While the specification imposes a global total ordering, the intention is
  // that for each async module, all its parents are totally ordered by when
  // they first had their [[AsyncEvaluating]] bit set.
  //
  // The module with largest vended ordinal finishes evaluating implies that the
  // async dependency as well as all other modules in that module's graph
  // depending on async dependencies are finished evaluating.
  //
  // If the async dependency participates in other module graphs (e.g. via
  // dynamic import, or other <script type=module> tags), those module graphs
  // must have been evaluated either before or after the async dependency is
  // settled, as the concrete Evaluate() method on cyclic module records is
  // neither reentrant nor performs microtask checkpoints during its
  // evaluation. If before, then all modules that depend on the async
  // dependencies were given an ordinal that ensure they are relatively ordered,
  // before the global ordinal was reset. If after, then the async evaluating
  // ordering does not apply, as the dependency is no longer asynchronous.
  //
  // https://tc39.es/ecma262/#sec-moduleevaluation
  if (ordinal + 1 == next_module_async_evaluating_ordinal_) {
    next_module_async_evaluating_ordinal_ =
        SourceTextModule::kFirstAsyncEvaluatingOrdinal;
  }
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
