// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_LOCAL_HANDLE_H_
#define INCLUDE_V8_LOCAL_HANDLE_H_

#include <stddef.h>

#include <type_traits>

#include "v8-handle-base.h"  // NOLINT(build/include_directory)

namespace v8 {

template <class T>
class LocalBase;
template <class T>
class Local;
template <class F>
class MaybeLocal;

template <class T>
class Eternal;
template <class T>
class Global;

template <class T>
class NonCopyablePersistentTraits;
template <class T>
class PersistentBase;
template <class T, class M = NonCopyablePersistentTraits<T>>
class Persistent;

class TracedReferenceBase;
template <class T>
class BasicTracedReference;
template <class F>
class TracedReference;

class Boolean;
class Context;
class EscapableHandleScope;
template <class F>
class FunctionCallbackInfo;
class Isolate;
class Object;
template <class F1, class F2, class F3>
class PersistentValueMapBase;
template <class F1, class F2>
class PersistentValueVector;
class Primitive;
class Private;
template <class F>
class PropertyCallbackInfo;
template <class F>
class ReturnValue;
class String;
template <class F>
class Traced;
class Utils;

namespace debug {
class ConsoleCallArguments;
}

namespace internal {
template <typename T>
class CustomArguments;
class SamplingHeapProfiler;
}  // namespace internal

namespace api_internal {
// Called when ToLocalChecked is called on an empty Local.
V8_EXPORT void ToLocalEmpty();
}  // namespace api_internal

/**
 * A stack-allocated class that governs a number of local handles.
 * After a handle scope has been created, all local handles will be
 * allocated within that handle scope until either the handle scope is
 * deleted or another handle scope is created.  If there is already a
 * handle scope and a new one is created, all allocations will take
 * place in the new handle scope until it is deleted.  After that,
 * new handles will again be allocated in the original handle scope.
 *
 * After the handle scope of a local handle has been deleted the
 * garbage collector will no longer track the object stored in the
 * handle and may deallocate it.  The behavior of accessing a handle
 * for which the handle scope has been deleted is undefined.
 */
class V8_EXPORT V8_NODISCARD HandleScope {
 public:
  explicit HandleScope(Isolate* isolate);

  ~HandleScope();

  /**
   * Counts the number of allocated handles.
   */
  static int NumberOfHandles(Isolate* isolate);

  V8_INLINE Isolate* GetIsolate() const {
    return reinterpret_cast<Isolate*>(i_isolate_);
  }

  HandleScope(const HandleScope&) = delete;
  void operator=(const HandleScope&) = delete;

  static internal::Address* CreateHandleForCurrentIsolate(
      internal::Address value);

 protected:
  V8_INLINE HandleScope() = default;

  void Initialize(Isolate* isolate);

  static internal::Address* CreateHandle(internal::Isolate* i_isolate,
                                         internal::Address value);

 private:
  // Declaring operator new and delete as deleted is not spec compliant.
  // Therefore declare them private instead to disable dynamic alloc
  void* operator new(size_t size);
  void* operator new[](size_t size);
  void operator delete(void*, size_t);
  void operator delete[](void*, size_t);

  internal::Isolate* i_isolate_;
  internal::Address* prev_next_;
  internal::Address* prev_limit_;

  // LocalBase<T>::New uses CreateHandle with an Isolate* parameter.
  template <typename T>
  friend class LocalBase;

  // Object::GetInternalField and Context::GetEmbedderData use CreateHandle with
  // a HeapObject in their shortcuts.
  friend class Object;
  friend class Context;
};

/**
 * A base class for local handles.
 * Its implementation depends on whether direct local support is enabled.
 * When it is, a local handle contains a direct pointer to the referenced
 * object, otherwise it contains an indirect pointer.
 */
#ifdef V8_ENABLE_DIRECT_LOCAL

template <typename T>
class LocalBase : public DirectHandleBase {
 protected:
  template <class F>
  friend class Local;

  V8_INLINE LocalBase() = default;

  V8_INLINE explicit LocalBase(internal::Address ptr) : DirectHandleBase(ptr) {}

  template <typename S>
  V8_INLINE LocalBase(const LocalBase<S>& other) : DirectHandleBase(other) {}

  V8_INLINE static LocalBase<T> New(Isolate* isolate, internal::Address value) {
    return LocalBase<T>(value);
  }

  V8_INLINE static LocalBase<T> New(Isolate* isolate, T* that) {
    return LocalBase<T>::New(isolate,
                             internal::ValueHelper::ValueAsAddress(that));
  }

  V8_INLINE static LocalBase<T> FromSlot(internal::Address* slot) {
    return LocalBase<T>(*slot);
  }
};

#else  // !V8_ENABLE_DIRECT_LOCAL

template <typename T>
class LocalBase : public IndirectHandleBase {
 protected:
  template <class F>
  friend class Local;

  V8_INLINE LocalBase() = default;

  V8_INLINE explicit LocalBase(internal::Address* location)
      : IndirectHandleBase(location) {}

  template <typename S>
  V8_INLINE LocalBase(const LocalBase<S>& other) : IndirectHandleBase(other) {}

  V8_INLINE static LocalBase<T> New(Isolate* isolate, internal::Address value) {
    return LocalBase(HandleScope::CreateHandle(
        reinterpret_cast<internal::Isolate*>(isolate), value));
  }

  V8_INLINE static LocalBase<T> New(Isolate* isolate, T* that) {
    if (internal::ValueHelper::IsEmpty(that)) return LocalBase<T>();
    return LocalBase<T>::New(isolate,
                             internal::ValueHelper::ValueAsAddress(that));
  }

  V8_INLINE static LocalBase<T> FromSlot(internal::Address* slot) {
    return LocalBase<T>(slot);
  }
};

#endif  // V8_ENABLE_DIRECT_LOCAL

/**
 * An object reference managed by the v8 garbage collector.
 *
 * All objects returned from v8 have to be tracked by the garbage collector so
 * that it knows that the objects are still alive.  Also, because the garbage
 * collector may move objects, it is unsafe to point directly to an object.
 * Instead, all objects are stored in handles which are known by the garbage
 * collector and updated whenever an object moves.  Handles should always be
 * passed by value (except in cases like out-parameters) and they should never
 * be allocated on the heap.
 *
 * There are two types of handles: local and persistent handles.
 *
 * Local handles are light-weight and transient and typically used in local
 * operations.  They are managed by HandleScopes. That means that a HandleScope
 * must exist on the stack when they are created and that they are only valid
 * inside of the HandleScope active during their creation. For passing a local
 * handle to an outer HandleScope, an EscapableHandleScope and its Escape()
 * method must be used.
 *
 * Persistent handles can be used when storing objects across several
 * independent operations and have to be explicitly deallocated when they're no
 * longer used.
 *
 * It is safe to extract the object stored in the handle by dereferencing the
 * handle (for instance, to extract the Object* from a Local<Object>); the value
 * will still be governed by a handle behind the scenes and the same rules apply
 * to these values as to their handles.
 */
template <class T>
class Local : public LocalBase<T> {
 public:
  V8_INLINE Local() = default;

  template <class S>
  V8_INLINE Local(Local<S> that) : LocalBase<T>(that) {
    /**
     * This check fails when trying to convert between incompatible
     * handles. For example, converting from a Local<String> to a
     * Local<Number>.
     */
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  V8_INLINE T* operator->() const { return this->template value<T>(); }

  V8_INLINE T* operator*() const { return this->operator->(); }

  /**
   * Checks whether two handles are equal or different.
   * They are equal iff they are both empty or they are both non-empty and the
   * objects to which they refer are physically equal.
   *
   * If both handles refer to JS objects, this is the same as strict
   * non-equality. For primitives, such as numbers or strings, a `true` return
   * value does not indicate that the values aren't equal in the JavaScript
   * sense. Use `Value::StrictEquals()` to check primitives for equality.
   */

  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const {
    return internal::HandleHelper::EqualHandles(*this, that);
  }

  template <class S>
  V8_INLINE bool operator==(const PersistentBase<S>& that) const {
    return internal::HandleHelper::EqualHandles(*this, that);
  }

  template <class S>
  V8_INLINE bool operator!=(const Local<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE bool operator!=(const Persistent<S>& that) const {
    return !operator==(that);
  }

  /**
   * Cast a handle to a subclass, e.g. Local<Value> to Local<Object>.
   * This is only valid if the handle actually refers to a value of the
   * target type.
   */
  template <class S>
  V8_INLINE static Local<T> Cast(Local<S> that) {
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (that.IsEmpty()) return Local<T>();
    T::Cast(that.template value<S>());
#endif
    return Local<T>(LocalBase<T>(that));
  }

  /**
   * Calling this is equivalent to Local<S>::Cast().
   * In particular, this is only valid if the handle actually refers to a value
   * of the target type.
   */
  template <class S>
  V8_INLINE Local<S> As() const {
    return Local<S>::Cast(*this);
  }

  /**
   * Create a local handle for the content of another handle.
   * The referee is kept alive by the local handle even when
   * the original handle is destroyed/disposed.
   */
  V8_INLINE static Local<T> New(Isolate* isolate, Local<T> that) {
    return New(isolate, that.template value<T>());
  }

  V8_INLINE static Local<T> New(Isolate* isolate,
                                const PersistentBase<T>& that) {
    return New(isolate, that.template value<T>());
  }

  V8_INLINE static Local<T> New(Isolate* isolate,
                                const BasicTracedReference<T>& that) {
    return New(isolate, that.template value<T>());
  }

 private:
  friend class TracedReferenceBase;
  friend class Utils;
  template <class F>
  friend class Eternal;
  template <class F>
  friend class Global;
  template <class F>
  friend class Local;
  template <class F>
  friend class MaybeLocal;
  template <class F, class M>
  friend class Persistent;
  template <class F>
  friend class FunctionCallbackInfo;
  template <class F>
  friend class PropertyCallbackInfo;
  friend class String;
  friend class Object;
  friend class Context;
  friend class Isolate;
  friend class Private;
  template <class F>
  friend class internal::CustomArguments;
  friend Local<Primitive> Undefined(Isolate* isolate);
  friend Local<Primitive> Null(Isolate* isolate);
  friend Local<Boolean> True(Isolate* isolate);
  friend Local<Boolean> False(Isolate* isolate);
  friend class HandleScope;
  friend class EscapableHandleScope;
  template <class F1, class F2, class F3>
  friend class PersistentValueMapBase;
  template <class F1, class F2>
  friend class PersistentValueVector;
  template <class F>
  friend class ReturnValue;
  template <class F>
  friend class Traced;
  friend class internal::SamplingHeapProfiler;
  friend class internal::HandleHelper;
  friend class debug::ConsoleCallArguments;

  V8_INLINE explicit Local<T>(const LocalBase<T>& other)
      : LocalBase<T>(other) {}

  V8_INLINE static Local<T> FromSlot(internal::Address* slot) {
    return Local<T>(LocalBase<T>::FromSlot(slot));
  }

#ifdef V8_ENABLE_DIRECT_LOCAL
  V8_INLINE static Local<T> FromAddress(internal::Address ptr) {
    return Local<T>(LocalBase<T>(ptr));
  }
#endif  // V8_ENABLE_DIRECT_LOCAL

  V8_INLINE static Local<T> New(Isolate* isolate, internal::Address value) {
    return Local<T>(LocalBase<T>::New(isolate, value));
  }

  V8_INLINE static Local<T> New(Isolate* isolate, T* that) {
    return Local<T>(LocalBase<T>::New(isolate, that));
  }

  // Unsafe cast, should be avoided.
  template <class S>
  V8_INLINE Local<S> UnsafeAs() const {
    return Local<S>(LocalBase<S>(*this));
  }
};

#if !defined(V8_IMMINENT_DEPRECATION_WARNINGS)
// Handle is an alias for Local for historical reasons.
template <class T>
using Handle = Local<T>;
#endif

/**
 * A MaybeLocal<> is a wrapper around Local<> that enforces a check whether
 * the Local<> is empty before it can be used.
 *
 * If an API method returns a MaybeLocal<>, the API method can potentially fail
 * either because an exception is thrown, or because an exception is pending,
 * e.g. because a previous API call threw an exception that hasn't been caught
 * yet, or because a TerminateExecution exception was thrown. In that case, an
 * empty MaybeLocal is returned.
 */
template <class T>
class MaybeLocal {
 public:
  V8_INLINE MaybeLocal() : local_() {}
  template <class S>
  V8_INLINE MaybeLocal(Local<S> that) : local_(that) {}

  V8_INLINE bool IsEmpty() const { return local_.IsEmpty(); }

  /**
   * Converts this MaybeLocal<> to a Local<>. If this MaybeLocal<> is empty,
   * |false| is returned and |out| is assigned with nullptr.
   */
  template <class S>
  V8_WARN_UNUSED_RESULT V8_INLINE bool ToLocal(Local<S>* out) const {
    *out = local_;
    return !IsEmpty();
  }

  /**
   * Converts this MaybeLocal<> to a Local<>. If this MaybeLocal<> is empty,
   * V8 will crash the process.
   */
  V8_INLINE Local<T> ToLocalChecked() {
    if (V8_UNLIKELY(IsEmpty())) api_internal::ToLocalEmpty();
    return local_;
  }

  /**
   * Converts this MaybeLocal<> to a Local<>, using a default value if this
   * MaybeLocal<> is empty.
   */
  template <class S>
  V8_INLINE Local<S> FromMaybe(Local<S> default_value) const {
    return IsEmpty() ? default_value : Local<S>(local_);
  }

 private:
  Local<T> local_;
};

/**
 * A HandleScope which first allocates a handle in the current scope
 * which will be later filled with the escape value.
 */
class V8_EXPORT V8_NODISCARD EscapableHandleScope : public HandleScope {
 public:
  explicit EscapableHandleScope(Isolate* isolate);
  V8_INLINE ~EscapableHandleScope() = default;

  /**
   * Pushes the value into the previous scope and returns a handle to it.
   * Cannot be called twice.
   */
  template <class T>
  V8_INLINE Local<T> Escape(Local<T> value) {
#ifdef V8_ENABLE_DIRECT_LOCAL
    return value;
#else
    return Local<T>::FromSlot(Escape(value.slot()));
#endif
  }

  template <class T>
  V8_INLINE MaybeLocal<T> EscapeMaybe(MaybeLocal<T> value) {
    return Escape(value.FromMaybe(Local<T>()));
  }

  EscapableHandleScope(const EscapableHandleScope&) = delete;
  void operator=(const EscapableHandleScope&) = delete;

 private:
  // Declaring operator new and delete as deleted is not spec compliant.
  // Therefore declare them private instead to disable dynamic alloc
  void* operator new(size_t size);
  void* operator new[](size_t size);
  void operator delete(void*, size_t);
  void operator delete[](void*, size_t);

  internal::Address* Escape(internal::Address* escape_value);
  internal::Address* escape_slot_;
};

/**
 * A SealHandleScope acts like a handle scope in which no handle allocations
 * are allowed. It can be useful for debugging handle leaks.
 * Handles can be allocated within inner normal HandleScopes.
 */
class V8_EXPORT V8_NODISCARD SealHandleScope {
 public:
  explicit SealHandleScope(Isolate* isolate);
  ~SealHandleScope();

  SealHandleScope(const SealHandleScope&) = delete;
  void operator=(const SealHandleScope&) = delete;

 private:
  // Declaring operator new and delete as deleted is not spec compliant.
  // Therefore declare them private instead to disable dynamic alloc
  void* operator new(size_t size);
  void* operator new[](size_t size);
  void operator delete(void*, size_t);
  void operator delete[](void*, size_t);

  internal::Isolate* const i_isolate_;
  internal::Address* prev_limit_;
  int prev_sealed_level_;
};

}  // namespace v8

#endif  // INCLUDE_V8_LOCAL_HANDLE_H_
