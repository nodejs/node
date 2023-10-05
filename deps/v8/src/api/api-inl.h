// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_INL_H_
#define V8_API_API_INL_H_

#include "include/v8-fast-api-calls.h"
#include "src/api/api.h"
#include "src/common/assert-scope.h"
#include "src/execution/interrupts-scope.h"
#include "src/execution/microtask-queue.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {

template <typename T>
inline T ToCData(v8::internal::Tagged<v8::internal::Object> obj) {
  static_assert(sizeof(T) == sizeof(v8::internal::Address));
  if (obj == v8::internal::Smi::zero()) return nullptr;
  return reinterpret_cast<T>(
      v8::internal::Foreign::cast(obj)->foreign_address());
}

template <>
inline v8::internal::Address ToCData(
    v8::internal::Tagged<v8::internal::Object> obj) {
  if (obj == v8::internal::Smi::zero()) return v8::internal::kNullAddress;
  return v8::internal::Foreign::cast(obj)->foreign_address();
}

template <typename T>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, T obj) {
  static_assert(sizeof(T) == sizeof(v8::internal::Address));
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
  DCHECK(obj.is_null() || (IsSmi(*obj) || !IsTheHole(*obj)));
#ifdef V8_ENABLE_DIRECT_LOCAL
  if (obj.is_null()) return Local<To>();
#endif
  return Local<To>::FromSlot(obj.location());
}

template <class From, class To>
inline Local<To> Utils::Convert(v8::internal::DirectHandle<From> obj,
                                v8::internal::Isolate* isolate) {
#if defined(V8_ENABLE_DIRECT_LOCAL)
  DCHECK(obj.is_null() || (IsSmi(*obj) || !IsTheHole(*obj)));
  return Local<To>::FromAddress(obj.address());
#elif defined(V8_ENABLE_DIRECT_HANDLE)
  return Convert<From, To>(v8::internal::Handle<From>(*obj, isolate));
#else
  return Convert<From, To>(obj);
#endif
}

// Implementations of ToLocal

#define MAKE_TO_LOCAL(Name, From, To)                                       \
  Local<v8::To> Utils::Name(v8::internal::Handle<v8::internal::From> obj) { \
    return Convert<v8::internal::From, v8::To>(obj);                        \
  }                                                                         \
                                                                            \
  Local<v8::To> Utils::Name(                                                \
      v8::internal::DirectHandle<v8::internal::From> obj,                   \
      i::Isolate* isolate) {                                                \
    return Convert<v8::internal::From, v8::To>(obj, isolate);               \
  }

TO_LOCAL_LIST(MAKE_TO_LOCAL)

#define MAKE_TO_LOCAL_TYPED_ARRAY(Type, typeName, TYPE, ctype)                 \
  Local<v8::Type##Array> Utils::ToLocal##Type##Array(                          \
      v8::internal::Handle<v8::internal::JSTypedArray> obj) {                  \
    DCHECK(obj->type() == v8::internal::kExternal##Type##Array);               \
    return Convert<v8::internal::JSTypedArray, v8::Type##Array>(obj);          \
  }                                                                            \
                                                                               \
  Local<v8::Type##Array> Utils::ToLocal##Type##Array(                          \
      v8::internal::DirectHandle<v8::internal::JSTypedArray> obj,              \
      v8::internal::Isolate* isolate) {                                        \
    DCHECK(obj->type() == v8::internal::kExternal##Type##Array);               \
    return Convert<v8::internal::JSTypedArray, v8::Type##Array>(obj, isolate); \
  }

TYPED_ARRAYS(MAKE_TO_LOCAL_TYPED_ARRAY)

#undef MAKE_TO_LOCAL_TYPED_ARRAY
#undef MAKE_TO_LOCAL
#undef TO_LOCAL_LIST

// Implementations of OpenHandle

#ifdef V8_ENABLE_DIRECT_LOCAL

#define MAKE_OPEN_HANDLE(From, To)                                           \
  v8::internal::Handle<v8::internal::To> Utils::OpenHandle(                  \
      const v8::From* that, bool allow_empty_handle) {                       \
    DCHECK(allow_empty_handle || !v8::internal::ValueHelper::IsEmpty(that)); \
    DCHECK(v8::internal::ValueHelper::IsEmpty(that) ||                       \
           Is##To(v8::internal::Object(                                      \
               v8::internal::ValueHelper::ValueAsAddress(that))));           \
    if (v8::internal::ValueHelper::IsEmpty(that)) {                          \
      return v8::internal::Handle<v8::internal::To>::null();                 \
    }                                                                        \
    return v8::internal::Handle<v8::internal::To>(                           \
        v8::HandleScope::CreateHandleForCurrentIsolate(                      \
            v8::internal::ValueHelper::ValueAsAddress(that)));               \
  }                                                                          \
                                                                             \
  v8::internal::DirectHandle<v8::internal::To> Utils::OpenDirectHandle(      \
      const v8::From* that, bool allow_empty_handle) {                       \
    DCHECK(allow_empty_handle || !v8::internal::ValueHelper::IsEmpty(that)); \
    DCHECK(v8::internal::ValueHelper::IsEmpty(that) ||                       \
           Is##To(v8::internal::Object(                                      \
               v8::internal::ValueHelper::ValueAsAddress(that))));           \
    return v8::internal::DirectHandle<v8::internal::To>(                     \
        v8::internal::ValueHelper::ValueAsAddress(that));                    \
  }                                                                          \
                                                                             \
  v8::internal::IndirectHandle<v8::internal::To> Utils::OpenIndirectHandle(  \
      const v8::From* that, bool allow_empty_handle) {                       \
    return Utils::OpenHandle(that, allow_empty_handle);                      \
  }

#else  // !V8_ENABLE_DIRECT_LOCAL

#define MAKE_OPEN_HANDLE(From, To)                                           \
  v8::internal::Handle<v8::internal::To> Utils::OpenHandle(                  \
      const v8::From* that, bool allow_empty_handle) {                       \
    DCHECK(allow_empty_handle || !v8::internal::ValueHelper::IsEmpty(that)); \
    DCHECK(v8::internal::ValueHelper::IsEmpty(that) ||                       \
           Is##To(v8::internal::Object(                                      \
               v8::internal::ValueHelper::ValueAsAddress(that))));           \
    return v8::internal::Handle<v8::internal::To>(                           \
        reinterpret_cast<v8::internal::Address*>(                            \
            const_cast<v8::From*>(that)));                                   \
  }                                                                          \
                                                                             \
  v8::internal::DirectHandle<v8::internal::To> Utils::OpenDirectHandle(      \
      const v8::From* that, bool allow_empty_handle) {                       \
    return Utils::OpenHandle(that, allow_empty_handle);                      \
  }                                                                          \
                                                                             \
  v8::internal::IndirectHandle<v8::internal::To> Utils::OpenIndirectHandle(  \
      const v8::From* that, bool allow_empty_handle) {                       \
    return Utils::OpenHandle(that, allow_empty_handle);                      \
  }

#endif  // V8_ENABLE_DIRECT_LOCAL

OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

#undef MAKE_OPEN_HANDLE
#undef OPEN_HANDLE_LIST

template <bool do_callback>
class V8_NODISCARD CallDepthScope {
 public:
  CallDepthScope(i::Isolate* isolate, Local<Context> context)
      : isolate_(isolate),
        context_(context),
        did_enter_context_(false),
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
      i::DisallowGarbageCollection no_gc;
      i::Tagged<i::Context> env = *Utils::OpenHandle(*context);
      i::HandleScopeImplementer* impl = isolate->handle_scope_implementer();
      if (isolate->context().is_null() ||
          isolate->context()->native_context() != env->native_context()) {
        impl->SaveContext(isolate->context());
        isolate->set_context(env);
        did_enter_context_ = true;
      }
    }
    if (do_callback) isolate_->FireBeforeCallEnteredCallback();
  }
  ~CallDepthScope() {
    i::MicrotaskQueue* microtask_queue = isolate_->default_microtask_queue();
    if (!context_.IsEmpty()) {
      if (did_enter_context_) {
        i::HandleScopeImplementer* impl = isolate_->handle_scope_implementer();
        isolate_->set_context(impl->RestoreContext());
      }

      i::Handle<i::Context> env = Utils::OpenHandle(*context_);
      microtask_queue = env->native_context()->microtask_queue();
    }
    if (!escaped_) isolate_->thread_local_top()->DecrementCallDepth(this);
    if (do_callback) isolate_->FireCallCompletedCallback(microtask_queue);
#ifdef DEBUG
    if (do_callback) {
      if (microtask_queue && microtask_queue->microtasks_policy() ==
                                 v8::MicrotasksPolicy::kScoped) {
        DCHECK(microtask_queue->GetMicrotasksScopeDepth() ||
               !microtask_queue->DebugMicrotasksScopeDepthIsZero());
      }
    }
    DCHECK(CheckKeptObjectsClearedAfterMicrotaskCheckpoint(microtask_queue));
#endif
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
#ifdef DEBUG
  bool CheckKeptObjectsClearedAfterMicrotaskCheckpoint(
      i::MicrotaskQueue* microtask_queue) {
    bool did_perform_microtask_checkpoint =
        isolate_->thread_local_top()->CallDepthIsZero() && do_callback &&
        microtask_queue &&
        microtask_queue->microtasks_policy() == MicrotasksPolicy::kAuto &&
        !isolate_->is_execution_terminating();
    return !did_perform_microtask_checkpoint ||
           IsUndefined(isolate_->heap()->weak_refs_keep_during_job(), isolate_);
  }
#endif

  i::Isolate* const isolate_;
  Local<Context> context_;
  bool did_enter_context_ : 1;
  bool escaped_ : 1;
  bool safe_for_termination_ : 1;
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

template <typename T>
void CopySmiElementsToTypedBuffer(T* dst, uint32_t length,
                                  i::Tagged<i::FixedArray> elements) {
  for (uint32_t i = 0; i < length; ++i) {
    double value = i::Object::Number(elements->get(static_cast<int>(i)));
    // TODO(mslekova): Avoid converting back-and-forth when possible, e.g
    // avoid int->double->int conversions to boost performance.
    dst[i] = i::ConvertDouble<T>(value);
  }
}

template <typename T>
void CopyDoubleElementsToTypedBuffer(T* dst, uint32_t length,
                                     i::Tagged<i::FixedDoubleArray> elements) {
  for (uint32_t i = 0; i < length; ++i) {
    double value = elements->get_scalar(static_cast<int>(i));
    // TODO(mslekova): There are certain cases, e.g. double->double, in which
    // we could do a memcpy directly.
    dst[i] = i::ConvertDouble<T>(value);
  }
}

template <CTypeInfo::Identifier type_info_id, typename T>
bool CopyAndConvertArrayToCppBuffer(Local<Array> src, T* dst,
                                    uint32_t max_length) {
  static_assert(
      std::is_same<T, typename i::CTypeInfoTraits<
                          CTypeInfo(type_info_id).GetType()>::ctype>::value,
      "Type mismatch between the expected CTypeInfo::Type and the destination "
      "array");

  uint32_t length = src->Length();
  if (length > max_length) {
    return false;
  }

  i::DisallowGarbageCollection no_gc;
  i::Tagged<i::JSArray> obj = *reinterpret_cast<i::JSArray*>(*src);
  if (i::Object::IterationHasObservableEffects(obj)) {
    // The array has a custom iterator.
    return false;
  }

  i::Tagged<i::FixedArrayBase> elements = obj->elements();
  switch (obj->GetElementsKind()) {
    case i::PACKED_SMI_ELEMENTS:
      CopySmiElementsToTypedBuffer(dst, length, i::FixedArray::cast(elements));
      return true;
    case i::PACKED_DOUBLE_ELEMENTS:
      CopyDoubleElementsToTypedBuffer(dst, length,
                                      i::FixedDoubleArray::cast(elements));
      return true;
    default:
      return false;
  }
}

// Deprecated; to be removed.
template <const CTypeInfo* type_info, typename T>
inline bool V8_EXPORT TryCopyAndConvertArrayToCppBuffer(Local<Array> src,
                                                        T* dst,
                                                        uint32_t max_length) {
  return CopyAndConvertArrayToCppBuffer<type_info->GetId(), T>(src, dst,
                                                               max_length);
}

template <CTypeInfo::Identifier type_info_id, typename T>
inline bool V8_EXPORT TryToCopyAndConvertArrayToCppBuffer(Local<Array> src,
                                                          T* dst,
                                                          uint32_t max_length) {
  return CopyAndConvertArrayToCppBuffer<type_info_id, T>(src, dst, max_length);
}

namespace internal {

void HandleScopeImplementer::EnterContext(Tagged<NativeContext> context) {
  DCHECK_EQ(entered_contexts_.capacity(), is_microtask_context_.capacity());
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());
  entered_contexts_.push_back(context);
  is_microtask_context_.push_back(0);
}

void HandleScopeImplementer::EnterMicrotaskContext(
    Tagged<NativeContext> context) {
  DCHECK_EQ(entered_contexts_.capacity(), is_microtask_context_.capacity());
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());
  entered_contexts_.push_back(context);
  is_microtask_context_.push_back(1);
}

Handle<NativeContext> HandleScopeImplementer::LastEnteredContext() {
  DCHECK_EQ(entered_contexts_.capacity(), is_microtask_context_.capacity());
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());

  for (size_t i = 0; i < entered_contexts_.size(); ++i) {
    size_t j = entered_contexts_.size() - i - 1;
    if (!is_microtask_context_.at(j)) {
      return handle(entered_contexts_.at(j), isolate_);
    }
  }

  return {};
}

Handle<NativeContext> HandleScopeImplementer::LastEnteredOrMicrotaskContext() {
  if (entered_contexts_.empty()) return {};
  return handle(entered_contexts_.back(), isolate_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_INL_H_
