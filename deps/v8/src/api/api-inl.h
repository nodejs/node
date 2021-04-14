// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_INL_H_
#define V8_API_API_INL_H_

#include "src/api/api.h"
#include "src/execution/interrupts-scope.h"
#include "src/execution/microtask-queue.h"
#include "src/handles/handles-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/objects-inl.h"
#include "src/objects/stack-frame-info.h"

namespace v8 {

template <typename T>
inline T ToCData(v8::internal::Object obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  if (obj == v8::internal::Smi::zero()) return nullptr;
  return reinterpret_cast<T>(
      v8::internal::Foreign::cast(obj).foreign_address());
}

template <>
inline v8::internal::Address ToCData(v8::internal::Object obj) {
  if (obj == v8::internal::Smi::zero()) return v8::internal::kNullAddress;
  return v8::internal::Foreign::cast(obj).foreign_address();
}

template <typename T>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, T obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  if (obj == nullptr) return handle(v8::internal::Smi::zero(), isolate);
  return isolate->factory()->NewForeign(
      reinterpret_cast<v8::internal::Address>(obj));
}

template <>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, v8::internal::Address obj) {
  if (obj == v8::internal::kNullAddress) {
    return handle(v8::internal::Smi::zero(), isolate);
  }
  return isolate->factory()->NewForeign(obj);
}

template <class From, class To>
inline Local<To> Utils::Convert(v8::internal::Handle<From> obj) {
  DCHECK(obj.is_null() || (obj->IsSmi() || !obj->IsTheHole()));
  return Local<To>(reinterpret_cast<To*>(obj.location()));
}

// Implementations of ToLocal

#define MAKE_TO_LOCAL(Name, From, To)                                       \
  Local<v8::To> Utils::Name(v8::internal::Handle<v8::internal::From> obj) { \
    return Convert<v8::internal::From, v8::To>(obj);                        \
  }

#define MAKE_TO_LOCAL_TYPED_ARRAY(Type, typeName, TYPE, ctype)        \
  Local<v8::Type##Array> Utils::ToLocal##Type##Array(                 \
      v8::internal::Handle<v8::internal::JSTypedArray> obj) {         \
    DCHECK(obj->type() == v8::internal::kExternal##Type##Array);      \
    return Convert<v8::internal::JSTypedArray, v8::Type##Array>(obj); \
  }

MAKE_TO_LOCAL(ToLocal, AccessorPair, debug::AccessorPair)
MAKE_TO_LOCAL(ToLocal, Context, Context)
MAKE_TO_LOCAL(ToLocal, Object, Value)
MAKE_TO_LOCAL(ToLocal, Module, Module)
MAKE_TO_LOCAL(ToLocal, Name, Name)
MAKE_TO_LOCAL(ToLocal, String, String)
MAKE_TO_LOCAL(ToLocal, Symbol, Symbol)
MAKE_TO_LOCAL(ToLocal, JSRegExp, RegExp)
MAKE_TO_LOCAL(ToLocal, JSReceiver, Object)
MAKE_TO_LOCAL(ToLocal, JSObject, Object)
MAKE_TO_LOCAL(ToLocal, JSFunction, Function)
MAKE_TO_LOCAL(ToLocal, JSArray, Array)
MAKE_TO_LOCAL(ToLocal, JSMap, Map)
MAKE_TO_LOCAL(ToLocal, JSSet, Set)
MAKE_TO_LOCAL(ToLocal, JSProxy, Proxy)
MAKE_TO_LOCAL(ToLocal, JSArrayBuffer, ArrayBuffer)
MAKE_TO_LOCAL(ToLocal, JSArrayBufferView, ArrayBufferView)
MAKE_TO_LOCAL(ToLocal, JSDataView, DataView)
MAKE_TO_LOCAL(ToLocal, JSTypedArray, TypedArray)
MAKE_TO_LOCAL(ToLocalShared, JSArrayBuffer, SharedArrayBuffer)

TYPED_ARRAYS(MAKE_TO_LOCAL_TYPED_ARRAY)

MAKE_TO_LOCAL(ToLocal, FunctionTemplateInfo, FunctionTemplate)
MAKE_TO_LOCAL(ToLocal, ObjectTemplateInfo, ObjectTemplate)
MAKE_TO_LOCAL(SignatureToLocal, FunctionTemplateInfo, Signature)
MAKE_TO_LOCAL(AccessorSignatureToLocal, FunctionTemplateInfo, AccessorSignature)
MAKE_TO_LOCAL(MessageToLocal, Object, Message)
MAKE_TO_LOCAL(PromiseToLocal, JSObject, Promise)
MAKE_TO_LOCAL(StackTraceToLocal, FixedArray, StackTrace)
MAKE_TO_LOCAL(StackFrameToLocal, StackFrameInfo, StackFrame)
MAKE_TO_LOCAL(NumberToLocal, Object, Number)
MAKE_TO_LOCAL(IntegerToLocal, Object, Integer)
MAKE_TO_LOCAL(Uint32ToLocal, Object, Uint32)
MAKE_TO_LOCAL(ToLocal, BigInt, BigInt)
MAKE_TO_LOCAL(ExternalToLocal, JSObject, External)
MAKE_TO_LOCAL(CallableToLocal, JSReceiver, Function)
MAKE_TO_LOCAL(ToLocalPrimitive, Object, Primitive)
MAKE_TO_LOCAL(FixedArrayToLocal, FixedArray, FixedArray)
MAKE_TO_LOCAL(PrimitiveArrayToLocal, FixedArray, PrimitiveArray)
MAKE_TO_LOCAL(ScriptOrModuleToLocal, Script, ScriptOrModule)

#undef MAKE_TO_LOCAL_TYPED_ARRAY
#undef MAKE_TO_LOCAL

// Implementations of OpenHandle

#define MAKE_OPEN_HANDLE(From, To)                                    \
  v8::internal::Handle<v8::internal::To> Utils::OpenHandle(           \
      const v8::From* that, bool allow_empty_handle) {                \
    DCHECK(allow_empty_handle || that != nullptr);                    \
    DCHECK(that == nullptr ||                                         \
           v8::internal::Object(                                      \
               *reinterpret_cast<const v8::internal::Address*>(that)) \
               .Is##To());                                            \
    return v8::internal::Handle<v8::internal::To>(                    \
        reinterpret_cast<v8::internal::Address*>(                     \
            const_cast<v8::From*>(that)));                            \
  }

OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

#undef MAKE_OPEN_HANDLE
#undef OPEN_HANDLE_LIST

template <bool do_callback>
class V8_NODISCARD CallDepthScope {
 public:
  CallDepthScope(i::Isolate* isolate, Local<Context> context)
      : isolate_(isolate),
        context_(context),
        escaped_(false),
        safe_for_termination_(isolate->next_v8_call_is_safe_for_termination()),
        interrupts_scope_(isolate_, i::StackGuard::TERMINATE_EXECUTION,
                          isolate_->only_terminate_in_safe_scope()
                              ? (safe_for_termination_
                                     ? i::InterruptsScope::kRunInterrupts
                                     : i::InterruptsScope::kPostponeInterrupts)
                              : i::InterruptsScope::kNoop) {
    isolate_->thread_local_top()->IncrementCallDepth(this);
    isolate_->set_next_v8_call_is_safe_for_termination(false);
    if (!context.IsEmpty()) {
      i::Handle<i::Context> env = Utils::OpenHandle(*context);
      i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
      if (!isolate->context().is_null() &&
          isolate->context().native_context() == env->native_context()) {
        context_ = Local<Context>();
      } else {
        impl->SaveContext(isolate->context());
        isolate->set_context(*env);
      }
    }
    if (do_callback) isolate_->FireBeforeCallEnteredCallback();
  }
  ~CallDepthScope() {
    i::MicrotaskQueue* microtask_queue = isolate_->default_microtask_queue();
    if (!context_.IsEmpty()) {
      i::HandleScopeImplementer* impl = isolate_->handle_scope_implementer();
      isolate_->set_context(impl->RestoreContext());

      i::Handle<i::Context> env = Utils::OpenHandle(*context_);
      microtask_queue = env->native_context().microtask_queue();
    }
    if (!escaped_) isolate_->thread_local_top()->DecrementCallDepth(this);
    if (do_callback) isolate_->FireCallCompletedCallback(microtask_queue);
// TODO(jochen): This should be #ifdef DEBUG
#ifdef V8_CHECK_MICROTASKS_SCOPES_CONSISTENCY
    if (do_callback) {
      if (microtask_queue && microtask_queue->microtasks_policy() ==
                                 v8::MicrotasksPolicy::kScoped) {
        DCHECK(microtask_queue->GetMicrotasksScopeDepth() ||
               !microtask_queue->DebugMicrotasksScopeDepthIsZero());
      }
    }
#endif
    DCHECK(CheckKeptObjectsClearedAfterMicrotaskCheckpoint(microtask_queue));
    isolate_->set_next_v8_call_is_safe_for_termination(safe_for_termination_);
  }

  CallDepthScope(const CallDepthScope&) = delete;
  CallDepthScope& operator=(const CallDepthScope&) = delete;

  void Escape() {
    DCHECK(!escaped_);
    escaped_ = true;
    auto thread_local_top = isolate_->thread_local_top();
    thread_local_top->DecrementCallDepth(this);
    bool clear_exception = thread_local_top->CallDepthIsZero() &&
                           thread_local_top->try_catch_handler_ == nullptr;
    isolate_->OptionalRescheduleException(clear_exception);
  }

 private:
  bool CheckKeptObjectsClearedAfterMicrotaskCheckpoint(
      i::MicrotaskQueue* microtask_queue) {
    bool did_perform_microtask_checkpoint =
        isolate_->thread_local_top()->CallDepthIsZero() && do_callback &&
        microtask_queue &&
        microtask_queue->microtasks_policy() == MicrotasksPolicy::kAuto;
    return !did_perform_microtask_checkpoint ||
           isolate_->heap()->weak_refs_keep_during_job().IsUndefined(isolate_);
  }

  i::Isolate* const isolate_;
  Local<Context> context_;
  bool escaped_;
  bool do_callback_;
  bool safe_for_termination_;
  i::InterruptsScope interrupts_scope_;
  i::Address previous_stack_height_;

  friend class i::ThreadLocalTop;

  DISALLOW_NEW_AND_DELETE()
};

class V8_NODISCARD InternalEscapableScope : public EscapableHandleScope {
 public:
  explicit inline InternalEscapableScope(i::Isolate* isolate)
      : EscapableHandleScope(reinterpret_cast<v8::Isolate*>(isolate)) {}
};

inline bool IsExecutionTerminatingCheck(i::Isolate* isolate) {
  if (isolate->has_scheduled_exception()) {
    return isolate->scheduled_exception() ==
           i::ReadOnlyRoots(isolate).termination_exception();
  }
  return false;
}

namespace internal {

Handle<Context> HandleScopeImplementer::LastEnteredContext() {
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());

  for (size_t i = 0; i < entered_contexts_.size(); ++i) {
    size_t j = entered_contexts_.size() - i - 1;
    if (!is_microtask_context_.at(j)) {
      return Handle<Context>(entered_contexts_.at(j), isolate_);
    }
  }

  return Handle<Context>::null();
}

Handle<Context> HandleScopeImplementer::LastEnteredOrMicrotaskContext() {
  if (entered_contexts_.empty()) return Handle<Context>::null();
  return Handle<Context>(entered_contexts_.back(), isolate_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_INL_H_
