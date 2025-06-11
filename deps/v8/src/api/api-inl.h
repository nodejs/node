// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_INL_H_
#define V8_API_API_INL_H_

#include "src/api/api.h"
// Include the non-inl header before the rest of the headers.

#include "include/v8-fast-api-calls.h"
#include "src/common/assert-scope.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/vm-state-inl.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "src/logging/runtime-call-stats.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {

template <typename T, internal::ExternalPointerTag tag>
inline T ToCData(i::Isolate* isolate, i::Tagged<i::Object> obj) {
  static_assert(sizeof(T) == sizeof(i::Address));
  if (obj == i::Smi::zero()) return nullptr;
  return reinterpret_cast<T>(
      i::Cast<i::Foreign>(obj)->foreign_address<tag>(isolate));
}

template <internal::ExternalPointerTag tag>
inline i::Address ToCData(i::Isolate* isolate, i::Tagged<i::Object> obj) {
  if (obj == i::Smi::zero()) return i::kNullAddress;
  return i::Cast<i::Foreign>(obj)->foreign_address<tag>(isolate);
}

template <internal::ExternalPointerTag tag, typename T>
inline i::DirectHandle<i::UnionOf<i::Smi, i::Foreign>> FromCData(
    i::Isolate* isolate, T obj) {
  static_assert(sizeof(T) == sizeof(i::Address));
  if (obj == nullptr) return direct_handle(i::Smi::zero(), isolate);
  return isolate->factory()->NewForeign<tag>(reinterpret_cast<i::Address>(obj));
}

template <internal::ExternalPointerTag tag>
inline i::DirectHandle<i::UnionOf<i::Smi, i::Foreign>> FromCData(
    i::Isolate* isolate, i::Address obj) {
  if (obj == i::kNullAddress) {
    return direct_handle(i::Smi::zero(), isolate);
  }
  return isolate->factory()->NewForeign<tag>(obj);
}

template <class From, class To>
inline Local<To> Utils::Convert(i::DirectHandle<From> obj) {
  DCHECK(obj.is_null() || IsSmi(*obj) || !IsTheHole(*obj));
#ifdef V8_ENABLE_DIRECT_HANDLE
  if (obj.is_null()) return Local<To>();
  return Local<To>::FromAddress(obj.address());
#else
  // This simply uses the location of the indirect handle wrapped inside a
  // "fake" direct handle.
  return Local<To>::FromSlot(indirect_handle(obj).location());
#endif
}

// Implementations of ToLocal

#define MAKE_TO_LOCAL(Name, From, To)                              \
  inline Local<v8::To> Utils::Name(i::DirectHandle<i::From> obj) { \
    return Convert<i::From, v8::To>(obj);                          \
  }

TO_LOCAL_LIST(MAKE_TO_LOCAL)
#undef MAKE_TO_LOCAL
#undef TO_LOCAL_LIST

#define MAKE_TO_LOCAL_TYPED_ARRAY(Type, typeName, TYPE, ctype) \
  Local<v8::Type##Array> Utils::ToLocal##Type##Array(          \
      i::DirectHandle<i::JSTypedArray> obj) {                  \
    DCHECK(obj->type() == i::kExternal##Type##Array);          \
    return Convert<i::JSTypedArray, v8::Type##Array>(obj);     \
  }

TYPED_ARRAYS(MAKE_TO_LOCAL_TYPED_ARRAY)
#undef MAKE_TO_LOCAL_TYPED_ARRAY

// Implementations of OpenHandle

#ifdef V8_ENABLE_DIRECT_HANDLE

#define MAKE_OPEN_HANDLE(From, To)                                           \
  i::Handle<i::To> Utils::OpenHandle(const v8::From* that,                   \
                                     bool allow_empty_handle) {              \
    DCHECK(allow_empty_handle || !i::ValueHelper::IsEmpty(that));            \
    DCHECK(                                                                  \
        i::ValueHelper::IsEmpty(that) ||                                     \
        Is##To(i::Tagged<i::Object>(i::ValueHelper::ValueAsAddress(that)))); \
    if (i::ValueHelper::IsEmpty(that)) {                                     \
      return i::Handle<i::To>::null();                                       \
    }                                                                        \
    return i::Handle<i::To>(v8::HandleScope::CreateHandleForCurrentIsolate(  \
        i::ValueHelper::ValueAsAddress(that)));                              \
  }                                                                          \
                                                                             \
  i::DirectHandle<i::To> Utils::OpenDirectHandle(const v8::From* that,       \
                                                 bool allow_empty_handle) {  \
    DCHECK(allow_empty_handle || !i::ValueHelper::IsEmpty(that));            \
    DCHECK(                                                                  \
        i::ValueHelper::IsEmpty(that) ||                                     \
        Is##To(i::Tagged<i::Object>(i::ValueHelper::ValueAsAddress(that)))); \
    return i::DirectHandle<i::To>::FromAddress(                              \
        i::ValueHelper::ValueAsAddress(that));                               \
  }                                                                          \
                                                                             \
  i::IndirectHandle<i::To> Utils::OpenIndirectHandle(                        \
      const v8::From* that, bool allow_empty_handle) {                       \
    return Utils::OpenHandle(that, allow_empty_handle);                      \
  }

#else  // !V8_ENABLE_DIRECT_HANDLE

#define MAKE_OPEN_HANDLE(From, To)                                           \
  i::Handle<i::To> Utils::OpenHandle(const v8::From* that,                   \
                                     bool allow_empty_handle) {              \
    DCHECK(allow_empty_handle || !i::ValueHelper::IsEmpty(that));            \
    DCHECK(                                                                  \
        i::ValueHelper::IsEmpty(that) ||                                     \
        Is##To(i::Tagged<i::Object>(i::ValueHelper::ValueAsAddress(that)))); \
    return i::Handle<i::To>(                                                 \
        reinterpret_cast<i::Address*>(const_cast<v8::From*>(that)));         \
  }                                                                          \
                                                                             \
  i::DirectHandle<i::To> Utils::OpenDirectHandle(const v8::From* that,       \
                                                 bool allow_empty_handle) {  \
    return Utils::OpenHandle(that, allow_empty_handle);                      \
  }                                                                          \
                                                                             \
  i::IndirectHandle<i::To> Utils::OpenIndirectHandle(                        \
      const v8::From* that, bool allow_empty_handle) {                       \
    return Utils::OpenHandle(that, allow_empty_handle);                      \
  }

#endif  // V8_ENABLE_DIRECT_HANDLE

OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

#undef MAKE_OPEN_HANDLE
#undef OPEN_HANDLE_LIST

template <bool do_callback>
class V8_NODISCARD CallDepthScope {
 public:
  CallDepthScope(i::Isolate* isolate, Local<Context> context)
      : isolate_(isolate), saved_context_(isolate->context(), isolate_) {
    isolate_->thread_local_top()->IncrementCallDepth<do_callback>(this);
    i::Tagged<i::NativeContext> env = *Utils::OpenDirectHandle(*context);
    isolate->set_context(env);

    if (do_callback) isolate_->FireBeforeCallEnteredCallback();
  }
  explicit CallDepthScope(i::Isolate* isolate) : isolate_(isolate) {
    isolate_->thread_local_top()->IncrementCallDepth<do_callback>(this);

    if (do_callback) isolate_->FireBeforeCallEnteredCallback();
  }
  ~CallDepthScope() {
    i::MicrotaskQueue* microtask_queue =
        i::Cast<i::NativeContext>(isolate_->context())
            ->microtask_queue(isolate_);

    isolate_->thread_local_top()->DecrementCallDepth(this);
    // Clear the exception when exiting V8 to avoid memory leaks.
    // Also clear termination exceptions iff there's no TryCatch handler.
    // TODO(verwaest): Drop this once we propagate exceptions to external
    // TryCatch on Throw. This should be debug-only.
    if (isolate_->thread_local_top()->CallDepthIsZero() &&
        (isolate_->thread_local_top()->try_catch_handler_ == nullptr ||
         !isolate_->is_execution_terminating())) {
      isolate_->clear_internal_exception();
    }
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

    if (!saved_context_.is_null()) isolate_->set_context(*saved_context_);
  }

  CallDepthScope(const CallDepthScope&) = delete;
  CallDepthScope& operator=(const CallDepthScope&) = delete;

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
  i::Handle<i::Context> saved_context_;

  i::Address previous_stack_height_;

  friend class i::ThreadLocalTop;

  DISALLOW_NEW_AND_DELETE()
};

class V8_NODISCARD InternalEscapableScope : public EscapableHandleScopeBase {
 public:
  explicit inline InternalEscapableScope(i::Isolate* isolate)
      : EscapableHandleScopeBase(reinterpret_cast<v8::Isolate*>(isolate)) {}

  /**
   * Pushes the value into the previous scope and returns a handle to it.
   * Cannot be called twice.
   */
  template <class T>
  V8_INLINE Local<T> Escape(Local<T> value) {
#ifdef V8_ENABLE_DIRECT_HANDLE
    return value;
#else
    DCHECK(!value.IsEmpty());
    return Local<T>::FromSlot(EscapeSlot(value.slot()));
#endif
  }

  template <class T>
  V8_INLINE MaybeLocal<T> EscapeMaybe(MaybeLocal<T> maybe_value) {
    Local<T> value;
    if (!maybe_value.ToLocal(&value)) return maybe_value;
    return Escape(value);
  }
};

// Most API methods should use one of the following three classes:
// EnterV8Scope, EnterV8ScopeNoScriptScope, EnterV8NoScriptNoExceptionScope
//
// The latter two assume that no script is executed, and no exceptions are
// scheduled in addition (respectively). Creating an exception and
// removing it before returning is ok.
template <typename HandleScopeClass, bool do_callback>
class V8_NODISCARD EnterV8InternalScope {
 public:
  EnterV8InternalScope(i::Isolate* i_isolate, Local<Context> context,
                       i::RuntimeCallCounterId rcc_id)
      : handle_scope_{i_isolate},
        call_depth_scope_{i_isolate, context},
#ifdef V8_RUNTIME_CALL_STATS
        rcs_scope_{i_isolate, rcc_id},
#endif  // V8_RUNTIME_CALL_STATS
        vm_state_{i_isolate} {
    DCHECK(!i_isolate->is_execution_terminating());
    DCHECK_EQ(i_isolate, i::Isolate::TryGetCurrent());
  }

  EnterV8InternalScope(i::Isolate* i_isolate, i::RuntimeCallCounterId rcc_id)
      : handle_scope_{i_isolate},
        call_depth_scope_{i_isolate},
#ifdef V8_RUNTIME_CALL_STATS
        rcs_scope_{i_isolate, rcc_id},
#endif  // V8_RUNTIME_CALL_STATS
        vm_state_{i_isolate} {
    DCHECK(!i_isolate->is_execution_terminating());
    DCHECK_EQ(i_isolate, i::Isolate::TryGetCurrent());
  }

  template <typename T>
  V8_INLINE Local<T> Escape(Local<T> value) {
    return handle_scope_.Escape(value);
  }

  template <typename T>
  V8_INLINE MaybeLocal<T> EscapeMaybe(MaybeLocal<T> value) {
    return handle_scope_.EscapeMaybe(value);
  }

  // Same as above, but also converts `MaybeHandle` to `MaybeLocal` with type
  // inference via `Utils::ToMaybeLocal`.
  template <typename From>
    requires requires(From v) { Utils::ToMaybeLocal(v); }
  V8_INLINE auto EscapeMaybe(From value) {
    return handle_scope_.EscapeMaybe(Utils::ToMaybeLocal(value));
  }

 private:
  HandleScopeClass handle_scope_;
  CallDepthScope<do_callback> call_depth_scope_;
#ifdef V8_RUNTIME_CALL_STATS
  i::RuntimeCallTimerScope rcs_scope_;
#endif  // V8_RUNTIME_CALL_STATS
  i::VMState<v8::OTHER> vm_state_;
};

template <typename HandleScopeClass = i::HandleScope>
class V8_NODISCARD EnterV8Scope
    : public EnterV8InternalScope<HandleScopeClass, true> {
 public:
  template <typename... Args>
  explicit EnterV8Scope(Args... args)
      : EnterV8InternalScope<HandleScopeClass, true>{args...} {}
};

template <typename HandleScopeClass = i::HandleScope>
class V8_NODISCARD EnterV8NoScriptScope
    : public EnterV8InternalScope<HandleScopeClass, false> {
 public:
  template <typename... Args>
  EnterV8NoScriptScope(i::Isolate* i_isolate, Args... args)
      : EnterV8InternalScope<HandleScopeClass, false>{i_isolate, args...},
        no_script_scope_{i_isolate} {}

 private:
  V8_NO_UNIQUE_ADDRESS i::DisallowJavascriptExecutionDebugOnly no_script_scope_;
};

class V8_NODISCARD EnterV8NoScriptNoExceptionScope {
 public:
  explicit EnterV8NoScriptNoExceptionScope(i::Isolate* i_isolate)
      : vm_state_{i_isolate},
        no_script_scope_{i_isolate},
        no_exceptions_scope_{i_isolate} {}

 private:
  i::VMState<v8::OTHER> vm_state_;
  V8_NO_UNIQUE_ADDRESS i::DisallowJavascriptExecutionDebugOnly no_script_scope_;
  V8_NO_UNIQUE_ADDRESS i::DisallowExceptionsDebugOnly no_exceptions_scope_;
};

// TODO(clemensb): Make this the basis of all the other EnterV8* scopes?
class V8_NODISCARD EnterV8BasicScope {
 public:
  explicit EnterV8BasicScope(i::Isolate* i_isolate) : vm_state_{i_isolate} {
    // Embedders should never enter V8 after terminating it.
    DCHECK_IMPLIES(i::v8_flags.strict_termination_checks,
                   !i_isolate->is_execution_terminating());
  }

 private:
  i::VMState<v8::OTHER> vm_state_;
};

class V8_NODISCARD PrepareForExecutionScope
    : public EnterV8InternalScope<InternalEscapableScope, false> {
 public:
  PrepareForExecutionScope(Local<Context> context,
                           i::RuntimeCallCounterId rcc_id)
      : PrepareForExecutionScope{i::Isolate::Current(), context, rcc_id} {}

  PrepareForExecutionScope(i::Isolate* i_isolate, Local<Context> context,
                           i::RuntimeCallCounterId rcc_id)
      : EnterV8InternalScope<InternalEscapableScope,
                             false>{ClearInternalException(i_isolate), context,
                                    rcc_id},
        i_isolate_{i_isolate} {}

  i::Isolate* i_isolate() const { return i_isolate_; }

 private:
  static i::Isolate* ClearInternalException(i::Isolate* i_isolate) {
    i_isolate->clear_internal_exception();
    return i_isolate;
  }

  i::Isolate* const i_isolate_;
};

class V8_NODISCARD PrepareForDebugInterfaceExecutionScope {
 public:
  PrepareForDebugInterfaceExecutionScope(i::Isolate* i_isolate,
                                         Local<Context> context)
      : handle_scope_{i_isolate},
        call_depth_scope_{i_isolate, context},
        vm_state_{i_isolate} {
    DCHECK(!i_isolate->is_execution_terminating());
    DCHECK_EQ(i_isolate, i::Isolate::TryGetCurrent());
  }

  template <typename T>
  V8_INLINE Local<T> Escape(Local<T> value) {
    return handle_scope_.Escape(value);
  }

  template <typename T>
  V8_INLINE MaybeLocal<T> EscapeMaybe(MaybeLocal<T> value) {
    return handle_scope_.EscapeMaybe(value);
  }

 private:
  InternalEscapableScope handle_scope_;
  CallDepthScope<false> call_depth_scope_;
  i::VMState<v8::OTHER> vm_state_;
};

class V8_NODISCARD ApiRuntimeCallStatsScope {
 public:
#ifdef V8_RUNTIME_CALL_STATS
  ApiRuntimeCallStatsScope(i::Isolate* i_isolate,
                           i::RuntimeCallCounterId rcc_id)
      : rct_scope_{i_isolate, rcc_id} {}

 private:
  i::RuntimeCallTimerScope rct_scope_;
#else   // V8_RUNTIME_CALL_STATS
  constexpr ApiRuntimeCallStatsScope(i::Isolate*, i::RuntimeCallCounterId) {}
#endif  // V8_RUNTIME_CALL_STATS
};

template <typename T>
void CopySmiElementsToTypedBuffer(T* dst, uint32_t length,
                                  i::Tagged<i::FixedArray> elements) {
  for (uint32_t i = 0; i < length; ++i) {
    double value = i::Object::NumberValue(
        i::Cast<i::Smi>(elements->get(static_cast<int>(i))));
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
      std::is_same_v<T, typename i::CTypeInfoTraits<
                            CTypeInfo(type_info_id).GetType()>::ctype>,
      "Type mismatch between the expected CTypeInfo::Type and the destination "
      "array");

  uint32_t length = src->Length();
  if (length == 0) {
    // Early return here to avoid a cast error below, as the EmptyFixedArray
    // cannot be cast to a FixedDoubleArray.
    return true;
  }
  if (length > max_length) {
    return false;
  }

  i::DisallowGarbageCollection no_gc;
  i::Tagged<i::JSArray> obj = *Utils::OpenDirectHandle(*src);
  if (i::Object::IterationHasObservableEffects(obj)) {
    // The array has a custom iterator.
    return false;
  }

  i::Tagged<i::FixedArrayBase> elements = obj->elements();
  switch (obj->GetElementsKind()) {
    case i::PACKED_SMI_ELEMENTS:
      CopySmiElementsToTypedBuffer(dst, length,
                                   i::Cast<i::FixedArray>(elements));
      return true;
    case i::PACKED_DOUBLE_ELEMENTS:
      CopyDoubleElementsToTypedBuffer(dst, length,
                                      i::Cast<i::FixedDoubleArray>(elements));
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
  entered_contexts_.push_back(context);
}

DirectHandle<NativeContext> HandleScopeImplementer::LastEnteredContext() {
  if (entered_contexts_.empty()) return {};
  return direct_handle(entered_contexts_.back(), isolate_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_INL_H_
