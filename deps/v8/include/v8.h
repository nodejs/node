// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** \mainpage V8 API Reference Guide
 *
 * V8 is Google's open source JavaScript engine.
 *
 * This set of documents provides reference material generated from the
 * V8 header file, include/v8.h.
 *
 * For other documentation see https://v8.dev/.
 */

#ifndef INCLUDE_V8_H_
#define INCLUDE_V8_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "v8-internal.h"  // NOLINT(build/include)
#include "v8-version.h"   // NOLINT(build/include)
#include "v8config.h"     // NOLINT(build/include)

// We reserve the V8_* prefix for macros defined in V8 public API and
// assume there are no name conflicts with the embedder's code.

/**
 * The v8 JavaScript engine.
 */
namespace v8 {

class AccessorSignature;
class Array;
class ArrayBuffer;
class BigInt;
class BigIntObject;
class Boolean;
class BooleanObject;
class Context;
class Data;
class Date;
class External;
class Function;
class FunctionTemplate;
class HeapProfiler;
class ImplementationUtilities;
class Int32;
class Integer;
class Isolate;
template <class T>
class Maybe;
class MicrotaskQueue;
class Name;
class Number;
class NumberObject;
class Object;
class ObjectOperationDescriptor;
class ObjectTemplate;
class Platform;
class Primitive;
class Promise;
class PropertyDescriptor;
class Proxy;
class RawOperationDescriptor;
class Script;
class SharedArrayBuffer;
class Signature;
class StartupData;
class StackFrame;
class StackTrace;
class String;
class StringObject;
class Symbol;
class SymbolObject;
class PrimitiveArray;
class Private;
class Uint32;
class Utils;
class Value;
class WasmModuleObject;
template <class T> class Local;
template <class T>
class MaybeLocal;
template <class T> class Eternal;
template<class T> class NonCopyablePersistentTraits;
template<class T> class PersistentBase;
template <class T, class M = NonCopyablePersistentTraits<T> >
class Persistent;
template <class T>
class Global;
template <class T>
class TracedGlobal;
template <class T>
class TracedReference;
template <class T>
class TracedReferenceBase;
template<class K, class V, class T> class PersistentValueMap;
template <class K, class V, class T>
class PersistentValueMapBase;
template <class K, class V, class T>
class GlobalValueMap;
template<class V, class T> class PersistentValueVector;
template<class T, class P> class WeakCallbackObject;
class FunctionTemplate;
class ObjectTemplate;
template<typename T> class FunctionCallbackInfo;
template<typename T> class PropertyCallbackInfo;
class StackTrace;
class StackFrame;
class Isolate;
class CallHandlerHelper;
class EscapableHandleScope;
template<typename T> class ReturnValue;

namespace internal {
enum class ArgumentsType;
template <ArgumentsType>
class Arguments;
class DeferredHandles;
class Heap;
class HeapObject;
class ExternalString;
class Isolate;
class LocalEmbedderHeapTracer;
class MicrotaskQueue;
struct ScriptStreamingData;
template<typename T> class CustomArguments;
class PropertyCallbackArguments;
class FunctionCallbackArguments;
class GlobalHandles;
class ScopedExternalStringLock;
class ThreadLocalTop;

namespace wasm {
class NativeModule;
class StreamingDecoder;
}  // namespace wasm

}  // namespace internal

namespace debug {
class ConsoleCallArguments;
}  // namespace debug

// --- Handles ---

/**
 * An object reference managed by the v8 garbage collector.
 *
 * All objects returned from v8 have to be tracked by the garbage
 * collector so that it knows that the objects are still alive.  Also,
 * because the garbage collector may move objects, it is unsafe to
 * point directly to an object.  Instead, all objects are stored in
 * handles which are known by the garbage collector and updated
 * whenever an object moves.  Handles should always be passed by value
 * (except in cases like out-parameters) and they should never be
 * allocated on the heap.
 *
 * There are two types of handles: local and persistent handles.
 *
 * Local handles are light-weight and transient and typically used in
 * local operations.  They are managed by HandleScopes. That means that a
 * HandleScope must exist on the stack when they are created and that they are
 * only valid inside of the HandleScope active during their creation.
 * For passing a local handle to an outer HandleScope, an EscapableHandleScope
 * and its Escape() method must be used.
 *
 * Persistent handles can be used when storing objects across several
 * independent operations and have to be explicitly deallocated when they're no
 * longer used.
 *
 * It is safe to extract the object stored in the handle by
 * dereferencing the handle (for instance, to extract the Object* from
 * a Local<Object>); the value will still be governed by a handle
 * behind the scenes and the same rules apply to these values as to
 * their handles.
 */
template <class T>
class Local {
 public:
  V8_INLINE Local() : val_(nullptr) {}
  template <class S>
  V8_INLINE Local(Local<S> that)
      : val_(reinterpret_cast<T*>(*that)) {
    /**
     * This check fails when trying to convert between incompatible
     * handles. For example, converting from a Local<String> to a
     * Local<Number>.
     */
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  /**
   * Returns true if the handle is empty.
   */
  V8_INLINE bool IsEmpty() const { return val_ == nullptr; }

  /**
   * Sets the handle to be empty. IsEmpty() will then return true.
   */
  V8_INLINE void Clear() { val_ = nullptr; }

  V8_INLINE T* operator->() const { return val_; }

  V8_INLINE T* operator*() const { return val_; }

  /**
   * Checks whether two handles are the same.
   * Returns true if both are empty, or if the objects to which they refer
   * are identical.
   *
   * If both handles refer to JS objects, this is the same as strict equality.
   * For primitives, such as numbers or strings, a `false` return value does not
   * indicate that the values aren't equal in the JavaScript sense.
   * Use `Value::StrictEquals()` to check primitives for equality.
   */
  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const {
    internal::Address* a = reinterpret_cast<internal::Address*>(this->val_);
    internal::Address* b = reinterpret_cast<internal::Address*>(that.val_);
    if (a == nullptr) return b == nullptr;
    if (b == nullptr) return false;
    return *a == *b;
  }

  template <class S> V8_INLINE bool operator==(
      const PersistentBase<S>& that) const {
    internal::Address* a = reinterpret_cast<internal::Address*>(this->val_);
    internal::Address* b = reinterpret_cast<internal::Address*>(that.val_);
    if (a == nullptr) return b == nullptr;
    if (b == nullptr) return false;
    return *a == *b;
  }

  /**
   * Checks whether two handles are different.
   * Returns true if only one of the handles is empty, or if
   * the objects to which they refer are different.
   *
   * If both handles refer to JS objects, this is the same as strict
   * non-equality. For primitives, such as numbers or strings, a `true` return
   * value does not indicate that the values aren't equal in the JavaScript
   * sense. Use `Value::StrictEquals()` to check primitives for equality.
   */
  template <class S>
  V8_INLINE bool operator!=(const Local<S>& that) const {
    return !operator==(that);
  }

  template <class S> V8_INLINE bool operator!=(
      const Persistent<S>& that) const {
    return !operator==(that);
  }

  /**
   * Cast a handle to a subclass, e.g. Local<Value> to Local<Object>.
   * This is only valid if the handle actually refers to a value of the
   * target type.
   */
  template <class S> V8_INLINE static Local<T> Cast(Local<S> that) {
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (that.IsEmpty()) return Local<T>();
#endif
    return Local<T>(T::Cast(*that));
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
  V8_INLINE static Local<T> New(Isolate* isolate, Local<T> that);
  V8_INLINE static Local<T> New(Isolate* isolate,
                                const PersistentBase<T>& that);
  V8_INLINE static Local<T> New(Isolate* isolate,
                                const TracedReferenceBase<T>& that);

 private:
  friend class Utils;
  template<class F> friend class Eternal;
  template<class F> friend class PersistentBase;
  template<class F, class M> friend class Persistent;
  template<class F> friend class Local;
  template <class F>
  friend class MaybeLocal;
  template<class F> friend class FunctionCallbackInfo;
  template<class F> friend class PropertyCallbackInfo;
  friend class String;
  friend class Object;
  friend class Context;
  friend class Isolate;
  friend class Private;
  template<class F> friend class internal::CustomArguments;
  friend Local<Primitive> Undefined(Isolate* isolate);
  friend Local<Primitive> Null(Isolate* isolate);
  friend Local<Boolean> True(Isolate* isolate);
  friend Local<Boolean> False(Isolate* isolate);
  friend class HandleScope;
  friend class EscapableHandleScope;
  template <class F1, class F2, class F3>
  friend class PersistentValueMapBase;
  template<class F1, class F2> friend class PersistentValueVector;
  template <class F>
  friend class ReturnValue;
  template <class F>
  friend class Traced;
  template <class F>
  friend class TracedGlobal;
  template <class F>
  friend class TracedReferenceBase;
  template <class F>
  friend class TracedReference;

  explicit V8_INLINE Local(T* that) : val_(that) {}
  V8_INLINE static Local<T> New(Isolate* isolate, T* that);
  T* val_;
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
  V8_INLINE MaybeLocal() : val_(nullptr) {}
  template <class S>
  V8_INLINE MaybeLocal(Local<S> that)
      : val_(reinterpret_cast<T*>(*that)) {
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  V8_INLINE bool IsEmpty() const { return val_ == nullptr; }

  /**
   * Converts this MaybeLocal<> to a Local<>. If this MaybeLocal<> is empty,
   * |false| is returned and |out| is left untouched.
   */
  template <class S>
  V8_WARN_UNUSED_RESULT V8_INLINE bool ToLocal(Local<S>* out) const {
    out->val_ = IsEmpty() ? nullptr : this->val_;
    return !IsEmpty();
  }

  /**
   * Converts this MaybeLocal<> to a Local<>. If this MaybeLocal<> is empty,
   * V8 will crash the process.
   */
  V8_INLINE Local<T> ToLocalChecked();

  /**
   * Converts this MaybeLocal<> to a Local<>, using a default value if this
   * MaybeLocal<> is empty.
   */
  template <class S>
  V8_INLINE Local<S> FromMaybe(Local<S> default_value) const {
    return IsEmpty() ? default_value : Local<S>(val_);
  }

 private:
  T* val_;
};

/**
 * Eternal handles are set-once handles that live for the lifetime of the
 * isolate.
 */
template <class T> class Eternal {
 public:
  V8_INLINE Eternal() : val_(nullptr) {}
  template <class S>
  V8_INLINE Eternal(Isolate* isolate, Local<S> handle) : val_(nullptr) {
    Set(isolate, handle);
  }
  // Can only be safely called if already set.
  V8_INLINE Local<T> Get(Isolate* isolate) const;
  V8_INLINE bool IsEmpty() const { return val_ == nullptr; }
  template<class S> V8_INLINE void Set(Isolate* isolate, Local<S> handle);

 private:
  T* val_;
};


static const int kInternalFieldsInWeakCallback = 2;
static const int kEmbedderFieldsInWeakCallback = 2;

template <typename T>
class WeakCallbackInfo {
 public:
  typedef void (*Callback)(const WeakCallbackInfo<T>& data);

  WeakCallbackInfo(Isolate* isolate, T* parameter,
                   void* embedder_fields[kEmbedderFieldsInWeakCallback],
                   Callback* callback)
      : isolate_(isolate), parameter_(parameter), callback_(callback) {
    for (int i = 0; i < kEmbedderFieldsInWeakCallback; ++i) {
      embedder_fields_[i] = embedder_fields[i];
    }
  }

  V8_INLINE Isolate* GetIsolate() const { return isolate_; }
  V8_INLINE T* GetParameter() const { return parameter_; }
  V8_INLINE void* GetInternalField(int index) const;

  // When first called, the embedder MUST Reset() the Global which triggered the
  // callback. The Global itself is unusable for anything else. No v8 other api
  // calls may be called in the first callback. Should additional work be
  // required, the embedder must set a second pass callback, which will be
  // called after all the initial callbacks are processed.
  // Calling SetSecondPassCallback on the second pass will immediately crash.
  void SetSecondPassCallback(Callback callback) const { *callback_ = callback; }

 private:
  Isolate* isolate_;
  T* parameter_;
  Callback* callback_;
  void* embedder_fields_[kEmbedderFieldsInWeakCallback];
};


// kParameter will pass a void* parameter back to the callback, kInternalFields
// will pass the first two internal fields back to the callback, kFinalizer
// will pass a void* parameter back, but is invoked before the object is
// actually collected, so it can be resurrected. In the last case, it is not
// possible to request a second pass callback.
enum class WeakCallbackType { kParameter, kInternalFields, kFinalizer };

/**
 * An object reference that is independent of any handle scope.  Where
 * a Local handle only lives as long as the HandleScope in which it was
 * allocated, a PersistentBase handle remains valid until it is explicitly
 * disposed using Reset().
 *
 * A persistent handle contains a reference to a storage cell within
 * the V8 engine which holds an object value and which is updated by
 * the garbage collector whenever the object is moved.  A new storage
 * cell can be created using the constructor or PersistentBase::Reset and
 * existing handles can be disposed using PersistentBase::Reset.
 *
 */
template <class T> class PersistentBase {
 public:
  /**
   * If non-empty, destroy the underlying storage cell
   * IsEmpty() will return true after this call.
   */
  V8_INLINE void Reset();
  /**
   * If non-empty, destroy the underlying storage cell
   * and create a new one with the contents of other if other is non empty
   */
  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const Local<S>& other);

  /**
   * If non-empty, destroy the underlying storage cell
   * and create a new one with the contents of other if other is non empty
   */
  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const PersistentBase<S>& other);

  V8_INLINE bool IsEmpty() const { return val_ == nullptr; }
  V8_INLINE void Empty() { val_ = 0; }

  V8_INLINE Local<T> Get(Isolate* isolate) const {
    return Local<T>::New(isolate, *this);
  }

  template <class S>
  V8_INLINE bool operator==(const PersistentBase<S>& that) const {
    internal::Address* a = reinterpret_cast<internal::Address*>(this->val_);
    internal::Address* b = reinterpret_cast<internal::Address*>(that.val_);
    if (a == nullptr) return b == nullptr;
    if (b == nullptr) return false;
    return *a == *b;
  }

  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const {
    internal::Address* a = reinterpret_cast<internal::Address*>(this->val_);
    internal::Address* b = reinterpret_cast<internal::Address*>(that.val_);
    if (a == nullptr) return b == nullptr;
    if (b == nullptr) return false;
    return *a == *b;
  }

  template <class S>
  V8_INLINE bool operator!=(const PersistentBase<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE bool operator!=(const Local<S>& that) const {
    return !operator==(that);
  }

  /**
   * Install a finalization callback on this object.
   * NOTE: There is no guarantee as to *when* or even *if* the callback is
   * invoked. The invocation is performed solely on a best effort basis.
   * As always, GC-based finalization should *not* be relied upon for any
   * critical form of resource management!
   *
   * The callback is supposed to reset the handle. No further V8 API may be
   * called in this callback. In case additional work involving V8 needs to be
   * done, a second callback can be scheduled using
   * WeakCallbackInfo<void>::SetSecondPassCallback.
   */
  template <typename P>
  V8_INLINE void SetWeak(P* parameter,
                         typename WeakCallbackInfo<P>::Callback callback,
                         WeakCallbackType type);

  /**
   * Turns this handle into a weak phantom handle without finalization callback.
   * The handle will be reset automatically when the garbage collector detects
   * that the object is no longer reachable.
   * A related function Isolate::NumberOfPhantomHandleResetsSinceLastCall
   * returns how many phantom handles were reset by the garbage collector.
   */
  V8_INLINE void SetWeak();

  template<typename P>
  V8_INLINE P* ClearWeak();

  // TODO(dcarney): remove this.
  V8_INLINE void ClearWeak() { ClearWeak<void>(); }

  /**
   * Annotates the strong handle with the given label, which is then used by the
   * heap snapshot generator as a name of the edge from the root to the handle.
   * The function does not take ownership of the label and assumes that the
   * label is valid as long as the handle is valid.
   */
  V8_INLINE void AnnotateStrongRetainer(const char* label);

  /** Returns true if the handle's reference is weak.  */
  V8_INLINE bool IsWeak() const;

  /**
   * Assigns a wrapper class ID to the handle.
   */
  V8_INLINE void SetWrapperClassId(uint16_t class_id);

  /**
   * Returns the class ID previously assigned to this handle or 0 if no class ID
   * was previously assigned.
   */
  V8_INLINE uint16_t WrapperClassId() const;

  PersistentBase(const PersistentBase& other) = delete;  // NOLINT
  void operator=(const PersistentBase&) = delete;

 private:
  friend class Isolate;
  friend class Utils;
  template<class F> friend class Local;
  template<class F1, class F2> friend class Persistent;
  template <class F>
  friend class Global;
  template<class F> friend class PersistentBase;
  template<class F> friend class ReturnValue;
  template <class F1, class F2, class F3>
  friend class PersistentValueMapBase;
  template<class F1, class F2> friend class PersistentValueVector;
  friend class Object;

  explicit V8_INLINE PersistentBase(T* val) : val_(val) {}
  V8_INLINE static T* New(Isolate* isolate, T* that);

  T* val_;
};


/**
 * Default traits for Persistent. This class does not allow
 * use of the copy constructor or assignment operator.
 * At present kResetInDestructor is not set, but that will change in a future
 * version.
 */
template<class T>
class NonCopyablePersistentTraits {
 public:
  typedef Persistent<T, NonCopyablePersistentTraits<T> > NonCopyablePersistent;
  static const bool kResetInDestructor = false;
  template<class S, class M>
  V8_INLINE static void Copy(const Persistent<S, M>& source,
                             NonCopyablePersistent* dest) {
    static_assert(sizeof(S) < 0,
                  "NonCopyablePersistentTraits::Copy is not instantiable");
  }
};


/**
 * Helper class traits to allow copying and assignment of Persistent.
 * This will clone the contents of storage cell, but not any of the flags, etc.
 */
template<class T>
struct CopyablePersistentTraits {
  typedef Persistent<T, CopyablePersistentTraits<T> > CopyablePersistent;
  static const bool kResetInDestructor = true;
  template<class S, class M>
  static V8_INLINE void Copy(const Persistent<S, M>& source,
                             CopyablePersistent* dest) {
    // do nothing, just allow copy
  }
};


/**
 * A PersistentBase which allows copy and assignment.
 *
 * Copy, assignment and destructor behavior is controlled by the traits
 * class M.
 *
 * Note: Persistent class hierarchy is subject to future changes.
 */
template <class T, class M> class Persistent : public PersistentBase<T> {
 public:
  /**
   * A Persistent with no storage cell.
   */
  V8_INLINE Persistent() : PersistentBase<T>(nullptr) {}
  /**
   * Construct a Persistent from a Local.
   * When the Local is non-empty, a new storage cell is created
   * pointing to the same object, and no flags are set.
   */
  template <class S>
  V8_INLINE Persistent(Isolate* isolate, Local<S> that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    static_assert(std::is_base_of<T, S>::value, "type check");
  }
  /**
   * Construct a Persistent from a Persistent.
   * When the Persistent is non-empty, a new storage cell is created
   * pointing to the same object, and no flags are set.
   */
  template <class S, class M2>
  V8_INLINE Persistent(Isolate* isolate, const Persistent<S, M2>& that)
    : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    static_assert(std::is_base_of<T, S>::value, "type check");
  }
  /**
   * The copy constructors and assignment operator create a Persistent
   * exactly as the Persistent constructor, but the Copy function from the
   * traits class is called, allowing the setting of flags based on the
   * copied Persistent.
   */
  V8_INLINE Persistent(const Persistent& that) : PersistentBase<T>(nullptr) {
    Copy(that);
  }
  template <class S, class M2>
  V8_INLINE Persistent(const Persistent<S, M2>& that) : PersistentBase<T>(0) {
    Copy(that);
  }
  V8_INLINE Persistent& operator=(const Persistent& that) {
    Copy(that);
    return *this;
  }
  template <class S, class M2>
  V8_INLINE Persistent& operator=(const Persistent<S, M2>& that) { // NOLINT
    Copy(that);
    return *this;
  }
  /**
   * The destructor will dispose the Persistent based on the
   * kResetInDestructor flags in the traits class.  Since not calling dispose
   * can result in a memory leak, it is recommended to always set this flag.
   */
  V8_INLINE ~Persistent() {
    if (M::kResetInDestructor) this->Reset();
  }

  // TODO(dcarney): this is pretty useless, fix or remove
  template <class S>
  V8_INLINE static Persistent<T>& Cast(const Persistent<S>& that) {  // NOLINT
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (!that.IsEmpty()) T::Cast(*that);
#endif
    return reinterpret_cast<Persistent<T>&>(const_cast<Persistent<S>&>(that));
  }

  // TODO(dcarney): this is pretty useless, fix or remove
  template <class S>
  V8_INLINE Persistent<S>& As() const {  // NOLINT
    return Persistent<S>::Cast(*this);
  }

 private:
  friend class Isolate;
  friend class Utils;
  template<class F> friend class Local;
  template<class F1, class F2> friend class Persistent;
  template<class F> friend class ReturnValue;

  explicit V8_INLINE Persistent(T* that) : PersistentBase<T>(that) {}
  V8_INLINE T* operator*() const { return this->val_; }
  template<class S, class M2>
  V8_INLINE void Copy(const Persistent<S, M2>& that);
};


/**
 * A PersistentBase which has move semantics.
 *
 * Note: Persistent class hierarchy is subject to future changes.
 */
template <class T>
class Global : public PersistentBase<T> {
 public:
  /**
   * A Global with no storage cell.
   */
  V8_INLINE Global() : PersistentBase<T>(nullptr) {}

  /**
   * Construct a Global from a Local.
   * When the Local is non-empty, a new storage cell is created
   * pointing to the same object, and no flags are set.
   */
  template <class S>
  V8_INLINE Global(Isolate* isolate, Local<S> that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  /**
   * Construct a Global from a PersistentBase.
   * When the Persistent is non-empty, a new storage cell is created
   * pointing to the same object, and no flags are set.
   */
  template <class S>
  V8_INLINE Global(Isolate* isolate, const PersistentBase<S>& that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, that.val_)) {
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  /**
   * Move constructor.
   */
  V8_INLINE Global(Global&& other);

  V8_INLINE ~Global() { this->Reset(); }

  /**
   * Move via assignment.
   */
  template <class S>
  V8_INLINE Global& operator=(Global<S>&& rhs);

  /**
   * Pass allows returning uniques from functions, etc.
   */
  Global Pass() { return static_cast<Global&&>(*this); }  // NOLINT

  /*
   * For compatibility with Chromium's base::Bind (base::Passed).
   */
  typedef void MoveOnlyTypeForCPP03;

  Global(const Global&) = delete;
  void operator=(const Global&) = delete;

 private:
  template <class F>
  friend class ReturnValue;
  V8_INLINE T* operator*() const { return this->val_; }
};


// UniquePersistent is an alias for Global for historical reason.
template <class T>
using UniquePersistent = Global<T>;

/**
 * Deprecated. Use |TracedReference<T>| instead.
 */
template <typename T>
struct TracedGlobalTrait {};

/**
 * A traced handle with copy and move semantics. The handle is to be used
 * together with |v8::EmbedderHeapTracer| and specifies edges from the embedder
 * into V8's heap.
 *
 * The exact semantics are:
 * - Tracing garbage collections use |v8::EmbedderHeapTracer|.
 * - Non-tracing garbage collections refer to
 *   |v8::EmbedderHeapTracer::IsRootForNonTracingGC()| whether the handle should
 *   be treated as root or not.
 *
 * Note that the base class cannot be instantiated itself. Choose from
 * - TracedGlobal
 * - TracedReference
 */
template <typename T>
class TracedReferenceBase {
 public:
  /**
   * Returns true if this TracedReferenceBase is empty, i.e., has not been
   * assigned an object.
   */
  bool IsEmpty() const { return val_ == nullptr; }

  /**
   * If non-empty, destroy the underlying storage cell. |IsEmpty| will return
   * true after this call.
   */
  V8_INLINE void Reset();

  /**
   * Construct a Local<T> from this handle.
   */
  Local<T> Get(Isolate* isolate) const { return Local<T>::New(isolate, *this); }

  template <class S>
  V8_INLINE bool operator==(const TracedReferenceBase<S>& that) const {
    internal::Address* a = reinterpret_cast<internal::Address*>(val_);
    internal::Address* b = reinterpret_cast<internal::Address*>(that.val_);
    if (a == nullptr) return b == nullptr;
    if (b == nullptr) return false;
    return *a == *b;
  }

  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const {
    internal::Address* a = reinterpret_cast<internal::Address*>(val_);
    internal::Address* b = reinterpret_cast<internal::Address*>(that.val_);
    if (a == nullptr) return b == nullptr;
    if (b == nullptr) return false;
    return *a == *b;
  }

  template <class S>
  V8_INLINE bool operator!=(const TracedReferenceBase<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE bool operator!=(const Local<S>& that) const {
    return !operator==(that);
  }

  /**
   * Assigns a wrapper class ID to the handle.
   */
  V8_INLINE void SetWrapperClassId(uint16_t class_id);

  /**
   * Returns the class ID previously assigned to this handle or 0 if no class ID
   * was previously assigned.
   */
  V8_INLINE uint16_t WrapperClassId() const;

  template <class S>
  V8_INLINE TracedReferenceBase<S>& As() const {
    return reinterpret_cast<TracedReferenceBase<S>&>(
        const_cast<TracedReferenceBase<T>&>(*this));
  }

 private:
  enum DestructionMode { kWithDestructor, kWithoutDestructor };

  /**
   * An empty TracedReferenceBase without storage cell.
   */
  TracedReferenceBase() = default;

  V8_INLINE static T* New(Isolate* isolate, T* that, void* slot,
                          DestructionMode destruction_mode);

  T* val_ = nullptr;

  friend class EmbedderHeapTracer;
  template <typename F>
  friend class Local;
  friend class Object;
  template <typename F>
  friend class TracedGlobal;
  template <typename F>
  friend class TracedReference;
  template <typename F>
  friend class ReturnValue;
};

/**
 * A traced handle with destructor that clears the handle. For more details see
 * TracedReferenceBase.
 */
template <typename T>
class TracedGlobal : public TracedReferenceBase<T> {
 public:
  using TracedReferenceBase<T>::Reset;

  /**
   * Destructor resetting the handle.
   */
  ~TracedGlobal() { this->Reset(); }

  /**
   * An empty TracedGlobal without storage cell.
   */
  TracedGlobal() : TracedReferenceBase<T>() {}

  /**
   * Construct a TracedGlobal from a Local.
   *
   * When the Local is non-empty, a new storage cell is created
   * pointing to the same object.
   */
  template <class S>
  TracedGlobal(Isolate* isolate, Local<S> that) : TracedReferenceBase<T>() {
    this->val_ = this->New(isolate, that.val_, &this->val_,
                           TracedReferenceBase<T>::kWithDestructor);
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  /**
   * Move constructor initializing TracedGlobal from an existing one.
   */
  V8_INLINE TracedGlobal(TracedGlobal&& other) {
    // Forward to operator=.
    *this = std::move(other);
  }

  /**
   * Move constructor initializing TracedGlobal from an existing one.
   */
  template <typename S>
  V8_INLINE TracedGlobal(TracedGlobal<S>&& other) {
    // Forward to operator=.
    *this = std::move(other);
  }

  /**
   * Copy constructor initializing TracedGlobal from an existing one.
   */
  V8_INLINE TracedGlobal(const TracedGlobal& other) {
    // Forward to operator=;
    *this = other;
  }

  /**
   * Copy constructor initializing TracedGlobal from an existing one.
   */
  template <typename S>
  V8_INLINE TracedGlobal(const TracedGlobal<S>& other) {
    // Forward to operator=;
    *this = other;
  }

  /**
   * Move assignment operator initializing TracedGlobal from an existing one.
   */
  V8_INLINE TracedGlobal& operator=(TracedGlobal&& rhs);

  /**
   * Move assignment operator initializing TracedGlobal from an existing one.
   */
  template <class S>
  V8_INLINE TracedGlobal& operator=(TracedGlobal<S>&& rhs);

  /**
   * Copy assignment operator initializing TracedGlobal from an existing one.
   *
   * Note: Prohibited when |other| has a finalization callback set through
   * |SetFinalizationCallback|.
   */
  V8_INLINE TracedGlobal& operator=(const TracedGlobal& rhs);

  /**
   * Copy assignment operator initializing TracedGlobal from an existing one.
   *
   * Note: Prohibited when |other| has a finalization callback set through
   * |SetFinalizationCallback|.
   */
  template <class S>
  V8_INLINE TracedGlobal& operator=(const TracedGlobal<S>& rhs);

  /**
   * If non-empty, destroy the underlying storage cell and create a new one with
   * the contents of other if other is non empty
   */
  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const Local<S>& other);

  template <class S>
  V8_INLINE TracedGlobal<S>& As() const {
    return reinterpret_cast<TracedGlobal<S>&>(
        const_cast<TracedGlobal<T>&>(*this));
  }

  /**
   * Adds a finalization callback to the handle. The type of this callback is
   * similar to WeakCallbackType::kInternalFields, i.e., it will pass the
   * parameter and the first two internal fields of the object.
   *
   * The callback is then supposed to reset the handle in the callback. No
   * further V8 API may be called in this callback. In case additional work
   * involving V8 needs to be done, a second callback can be scheduled using
   * WeakCallbackInfo<void>::SetSecondPassCallback.
   */
  V8_INLINE void SetFinalizationCallback(
      void* parameter, WeakCallbackInfo<void>::Callback callback);
};

/**
 * A traced handle without destructor that clears the handle. The embedder needs
 * to ensure that the handle is not accessed once the V8 object has been
 * reclaimed. This can happen when the handle is not passed through the
 * EmbedderHeapTracer. For more details see TracedReferenceBase.
 *
 * The reference assumes the embedder has precise knowledge about references at
 * all times. In case V8 needs to separately handle on-stack references, the
 * embedder is required to set the stack start through
 * |EmbedderHeapTracer::SetStackStart|.
 */
template <typename T>
class TracedReference : public TracedReferenceBase<T> {
 public:
  using TracedReferenceBase<T>::Reset;

  /**
   * An empty TracedReference without storage cell.
   */
  TracedReference() : TracedReferenceBase<T>() {}

  /**
   * Construct a TracedReference from a Local.
   *
   * When the Local is non-empty, a new storage cell is created
   * pointing to the same object.
   */
  template <class S>
  TracedReference(Isolate* isolate, Local<S> that) : TracedReferenceBase<T>() {
    this->val_ = this->New(isolate, that.val_, &this->val_,
                           TracedReferenceBase<T>::kWithoutDestructor);
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  /**
   * Move constructor initializing TracedReference from an
   * existing one.
   */
  V8_INLINE TracedReference(TracedReference&& other) {
    // Forward to operator=.
    *this = std::move(other);
  }

  /**
   * Move constructor initializing TracedReference from an
   * existing one.
   */
  template <typename S>
  V8_INLINE TracedReference(TracedReference<S>&& other) {
    // Forward to operator=.
    *this = std::move(other);
  }

  /**
   * Copy constructor initializing TracedReference from an
   * existing one.
   */
  V8_INLINE TracedReference(const TracedReference& other) {
    // Forward to operator=;
    *this = other;
  }

  /**
   * Copy constructor initializing TracedReference from an
   * existing one.
   */
  template <typename S>
  V8_INLINE TracedReference(const TracedReference<S>& other) {
    // Forward to operator=;
    *this = other;
  }

  /**
   * Move assignment operator initializing TracedGlobal from an existing one.
   */
  V8_INLINE TracedReference& operator=(TracedReference&& rhs);

  /**
   * Move assignment operator initializing TracedGlobal from an existing one.
   */
  template <class S>
  V8_INLINE TracedReference& operator=(TracedReference<S>&& rhs);

  /**
   * Copy assignment operator initializing TracedGlobal from an existing one.
   */
  V8_INLINE TracedReference& operator=(const TracedReference& rhs);

  /**
   * Copy assignment operator initializing TracedGlobal from an existing one.
   */
  template <class S>
  V8_INLINE TracedReference& operator=(const TracedReference<S>& rhs);

  /**
   * If non-empty, destroy the underlying storage cell and create a new one with
   * the contents of other if other is non empty
   */
  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const Local<S>& other);

  template <class S>
  V8_INLINE TracedReference<S>& As() const {
    return reinterpret_cast<TracedReference<S>&>(
        const_cast<TracedReference<T>&>(*this));
  }
};

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
class V8_EXPORT HandleScope {
 public:
  explicit HandleScope(Isolate* isolate);

  ~HandleScope();

  /**
   * Counts the number of allocated handles.
   */
  static int NumberOfHandles(Isolate* isolate);

  V8_INLINE Isolate* GetIsolate() const {
    return reinterpret_cast<Isolate*>(isolate_);
  }

  HandleScope(const HandleScope&) = delete;
  void operator=(const HandleScope&) = delete;

 protected:
  V8_INLINE HandleScope() = default;

  void Initialize(Isolate* isolate);

  static internal::Address* CreateHandle(internal::Isolate* isolate,
                                         internal::Address value);

 private:
  // Declaring operator new and delete as deleted is not spec compliant.
  // Therefore declare them private instead to disable dynamic alloc
  void* operator new(size_t size);
  void* operator new[](size_t size);
  void operator delete(void*, size_t);
  void operator delete[](void*, size_t);

  internal::Isolate* isolate_;
  internal::Address* prev_next_;
  internal::Address* prev_limit_;

  // Local::New uses CreateHandle with an Isolate* parameter.
  template<class F> friend class Local;

  // Object::GetInternalField and Context::GetEmbedderData use CreateHandle with
  // a HeapObject in their shortcuts.
  friend class Object;
  friend class Context;
};


/**
 * A HandleScope which first allocates a handle in the current scope
 * which will be later filled with the escape value.
 */
class V8_EXPORT EscapableHandleScope : public HandleScope {
 public:
  explicit EscapableHandleScope(Isolate* isolate);
  V8_INLINE ~EscapableHandleScope() = default;

  /**
   * Pushes the value into the previous scope and returns a handle to it.
   * Cannot be called twice.
   */
  template <class T>
  V8_INLINE Local<T> Escape(Local<T> value) {
    internal::Address* slot =
        Escape(reinterpret_cast<internal::Address*>(*value));
    return Local<T>(reinterpret_cast<T*>(slot));
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
class V8_EXPORT SealHandleScope {
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

  internal::Isolate* const isolate_;
  internal::Address* prev_limit_;
  int prev_sealed_level_;
};


// --- Special objects ---

/**
 * The superclass of objects that can reside on V8's heap.
 */
class V8_EXPORT Data {
 private:
  Data();
};

/**
 * A container type that holds relevant metadata for module loading.
 *
 * This is passed back to the embedder as part of
 * HostImportModuleDynamicallyCallback for module loading.
 */
class V8_EXPORT ScriptOrModule {
 public:
  /**
   * The name that was passed by the embedder as ResourceName to the
   * ScriptOrigin. This can be either a v8::String or v8::Undefined.
   */
  Local<Value> GetResourceName();

  /**
   * The options that were passed by the embedder as HostDefinedOptions to
   * the ScriptOrigin.
   */
  Local<PrimitiveArray> GetHostDefinedOptions();
};

/**
 * An array to hold Primitive values. This is used by the embedder to
 * pass host defined options to the ScriptOptions during compilation.
 *
 * This is passed back to the embedder as part of
 * HostImportModuleDynamicallyCallback for module loading.
 *
 */
class V8_EXPORT PrimitiveArray {
 public:
  static Local<PrimitiveArray> New(Isolate* isolate, int length);
  int Length() const;
  void Set(Isolate* isolate, int index, Local<Primitive> item);
  Local<Primitive> Get(Isolate* isolate, int index);
};

/**
 * The optional attributes of ScriptOrigin.
 */
class ScriptOriginOptions {
 public:
  V8_INLINE ScriptOriginOptions(bool is_shared_cross_origin = false,
                                bool is_opaque = false, bool is_wasm = false,
                                bool is_module = false)
      : flags_((is_shared_cross_origin ? kIsSharedCrossOrigin : 0) |
               (is_wasm ? kIsWasm : 0) | (is_opaque ? kIsOpaque : 0) |
               (is_module ? kIsModule : 0)) {}
  V8_INLINE ScriptOriginOptions(int flags)
      : flags_(flags &
               (kIsSharedCrossOrigin | kIsOpaque | kIsWasm | kIsModule)) {}

  bool IsSharedCrossOrigin() const {
    return (flags_ & kIsSharedCrossOrigin) != 0;
  }
  bool IsOpaque() const { return (flags_ & kIsOpaque) != 0; }
  bool IsWasm() const { return (flags_ & kIsWasm) != 0; }
  bool IsModule() const { return (flags_ & kIsModule) != 0; }

  int Flags() const { return flags_; }

 private:
  enum {
    kIsSharedCrossOrigin = 1,
    kIsOpaque = 1 << 1,
    kIsWasm = 1 << 2,
    kIsModule = 1 << 3
  };
  const int flags_;
};

/**
 * The origin, within a file, of a script.
 */
class ScriptOrigin {
 public:
  V8_INLINE ScriptOrigin(
      Local<Value> resource_name,
      Local<Integer> resource_line_offset = Local<Integer>(),
      Local<Integer> resource_column_offset = Local<Integer>(),
      Local<Boolean> resource_is_shared_cross_origin = Local<Boolean>(),
      Local<Integer> script_id = Local<Integer>(),
      Local<Value> source_map_url = Local<Value>(),
      Local<Boolean> resource_is_opaque = Local<Boolean>(),
      Local<Boolean> is_wasm = Local<Boolean>(),
      Local<Boolean> is_module = Local<Boolean>(),
      Local<PrimitiveArray> host_defined_options = Local<PrimitiveArray>());

  V8_INLINE Local<Value> ResourceName() const;
  V8_INLINE Local<Integer> ResourceLineOffset() const;
  V8_INLINE Local<Integer> ResourceColumnOffset() const;
  V8_INLINE Local<Integer> ScriptID() const;
  V8_INLINE Local<Value> SourceMapUrl() const;
  V8_INLINE Local<PrimitiveArray> HostDefinedOptions() const;
  V8_INLINE ScriptOriginOptions Options() const { return options_; }

 private:
  Local<Value> resource_name_;
  Local<Integer> resource_line_offset_;
  Local<Integer> resource_column_offset_;
  ScriptOriginOptions options_;
  Local<Integer> script_id_;
  Local<Value> source_map_url_;
  Local<PrimitiveArray> host_defined_options_;
};

/**
 * A compiled JavaScript script, not yet tied to a Context.
 */
class V8_EXPORT UnboundScript {
 public:
  /**
   * Binds the script to the currently entered context.
   */
  Local<Script> BindToCurrentContext();

  int GetId();
  Local<Value> GetScriptName();

  /**
   * Data read from magic sourceURL comments.
   */
  Local<Value> GetSourceURL();
  /**
   * Data read from magic sourceMappingURL comments.
   */
  Local<Value> GetSourceMappingURL();

  /**
   * Returns zero based line number of the code_pos location in the script.
   * -1 will be returned if no information available.
   */
  int GetLineNumber(int code_pos);

  static const int kNoScriptId = 0;
};

/**
 * A compiled JavaScript module, not yet tied to a Context.
 */
class V8_EXPORT UnboundModuleScript : public Data {
  // Only used as a container for code caching.
};

/**
 * A location in JavaScript source.
 */
class V8_EXPORT Location {
 public:
  int GetLineNumber() { return line_number_; }
  int GetColumnNumber() { return column_number_; }

  Location(int line_number, int column_number)
      : line_number_(line_number), column_number_(column_number) {}

 private:
  int line_number_;
  int column_number_;
};

/**
 * A compiled JavaScript module.
 */
class V8_EXPORT Module : public Data {
 public:
  /**
   * The different states a module can be in.
   *
   * This corresponds to the states used in ECMAScript except that "evaluated"
   * is split into kEvaluated and kErrored, indicating success and failure,
   * respectively.
   */
  enum Status {
    kUninstantiated,
    kInstantiating,
    kInstantiated,
    kEvaluating,
    kEvaluated,
    kErrored
  };

  /**
   * Returns the module's current status.
   */
  Status GetStatus() const;

  /**
   * For a module in kErrored status, this returns the corresponding exception.
   */
  Local<Value> GetException() const;

  /**
   * Returns the number of modules requested by this module.
   */
  int GetModuleRequestsLength() const;

  /**
   * Returns the ith module specifier in this module.
   * i must be < GetModuleRequestsLength() and >= 0.
   */
  Local<String> GetModuleRequest(int i) const;

  /**
   * Returns the source location (line number and column number) of the ith
   * module specifier's first occurrence in this module.
   */
  Location GetModuleRequestLocation(int i) const;

  /**
   * Returns the identity hash for this object.
   */
  int GetIdentityHash() const;

  typedef MaybeLocal<Module> (*ResolveCallback)(Local<Context> context,
                                                Local<String> specifier,
                                                Local<Module> referrer);

  /**
   * Instantiates the module and its dependencies.
   *
   * Returns an empty Maybe<bool> if an exception occurred during
   * instantiation. (In the case where the callback throws an exception, that
   * exception is propagated.)
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> InstantiateModule(Local<Context> context,
                                                      ResolveCallback callback);

  /**
   * Evaluates the module and its dependencies.
   *
   * If status is kInstantiated, run the module's code. On success, set status
   * to kEvaluated and return the completion value; on failure, set status to
   * kErrored and propagate the thrown exception (which is then also available
   * via |GetException|).
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Evaluate(Local<Context> context);

  /**
   * Returns the namespace object of this module.
   *
   * The module's status must be at least kInstantiated.
   */
  Local<Value> GetModuleNamespace();

  /**
   * Returns the corresponding context-unbound module script.
   *
   * The module must be unevaluated, i.e. its status must not be kEvaluating,
   * kEvaluated or kErrored.
   */
  Local<UnboundModuleScript> GetUnboundModuleScript();

  /*
   * Callback defined in the embedder.  This is responsible for setting
   * the module's exported values with calls to SetSyntheticModuleExport().
   * The callback must return a Value to indicate success (where no
   * exception was thrown) and return an empy MaybeLocal to indicate falure
   * (where an exception was thrown).
   */
  typedef MaybeLocal<Value> (*SyntheticModuleEvaluationSteps)(
      Local<Context> context, Local<Module> module);

  /**
   * Creates a new SyntheticModule with the specified export names, where
   * evaluation_steps will be executed upon module evaluation.
   * export_names must not contain duplicates.
   * module_name is used solely for logging/debugging and doesn't affect module
   * behavior.
   */
  static Local<Module> CreateSyntheticModule(
      Isolate* isolate, Local<String> module_name,
      const std::vector<Local<String>>& export_names,
      SyntheticModuleEvaluationSteps evaluation_steps);

  /**
   * Set this module's exported value for the name export_name to the specified
   * export_value. This method must be called only on Modules created via
   * CreateSyntheticModule.  An error will be thrown if export_name is not one
   * of the export_names that were passed in that CreateSyntheticModule call.
   * Returns Just(true) on success, Nothing<bool>() if an error was thrown.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetSyntheticModuleExport(
      Isolate* isolate, Local<String> export_name, Local<Value> export_value);
  V8_DEPRECATE_SOON(
      "Use the preceding SetSyntheticModuleExport with an Isolate parameter, "
      "instead of the one that follows.  The former will throw a runtime "
      "error if called for an export that doesn't exist (as per spec); "
      "the latter will crash with a failed CHECK().")
  void SetSyntheticModuleExport(Local<String> export_name,
                                Local<Value> export_value);
};

/**
 * A compiled JavaScript script, tied to a Context which was active when the
 * script was compiled.
 */
class V8_EXPORT Script {
 public:
  /**
   * A shorthand for ScriptCompiler::Compile().
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Local<String> source,
      ScriptOrigin* origin = nullptr);

  /**
   * Runs the script returning the resulting value. It will be run in the
   * context in which it was created (ScriptCompiler::CompileBound or
   * UnboundScript::BindToCurrentContext()).
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Run(Local<Context> context);

  /**
   * Returns the corresponding context-unbound script.
   */
  Local<UnboundScript> GetUnboundScript();
};


/**
 * For compiling scripts.
 */
class V8_EXPORT ScriptCompiler {
 public:
  /**
   * Compilation data that the embedder can cache and pass back to speed up
   * future compilations. The data is produced if the CompilerOptions passed to
   * the compilation functions in ScriptCompiler contains produce_data_to_cache
   * = true. The data to cache can then can be retrieved from
   * UnboundScript.
   */
  struct V8_EXPORT CachedData {
    enum BufferPolicy {
      BufferNotOwned,
      BufferOwned
    };

    CachedData()
        : data(nullptr),
          length(0),
          rejected(false),
          buffer_policy(BufferNotOwned) {}

    // If buffer_policy is BufferNotOwned, the caller keeps the ownership of
    // data and guarantees that it stays alive until the CachedData object is
    // destroyed. If the policy is BufferOwned, the given data will be deleted
    // (with delete[]) when the CachedData object is destroyed.
    CachedData(const uint8_t* data, int length,
               BufferPolicy buffer_policy = BufferNotOwned);
    ~CachedData();
    // TODO(marja): Async compilation; add constructors which take a callback
    // which will be called when V8 no longer needs the data.
    const uint8_t* data;
    int length;
    bool rejected;
    BufferPolicy buffer_policy;

    // Prevent copying.
    CachedData(const CachedData&) = delete;
    CachedData& operator=(const CachedData&) = delete;
  };

  /**
   * Source code which can be then compiled to a UnboundScript or Script.
   */
  class Source {
   public:
    // Source takes ownership of CachedData.
    V8_INLINE Source(Local<String> source_string, const ScriptOrigin& origin,
                     CachedData* cached_data = nullptr);
    V8_INLINE Source(Local<String> source_string,
                     CachedData* cached_data = nullptr);
    V8_INLINE ~Source();

    // Ownership of the CachedData or its buffers is *not* transferred to the
    // caller. The CachedData object is alive as long as the Source object is
    // alive.
    V8_INLINE const CachedData* GetCachedData() const;

    V8_INLINE const ScriptOriginOptions& GetResourceOptions() const;

    // Prevent copying.
    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;

   private:
    friend class ScriptCompiler;

    Local<String> source_string;

    // Origin information
    Local<Value> resource_name;
    Local<Integer> resource_line_offset;
    Local<Integer> resource_column_offset;
    ScriptOriginOptions resource_options;
    Local<Value> source_map_url;
    Local<PrimitiveArray> host_defined_options;

    // Cached data from previous compilation (if a kConsume*Cache flag is
    // set), or hold newly generated cache data (kProduce*Cache flags) are
    // set when calling a compile method.
    CachedData* cached_data;
  };

  /**
   * For streaming incomplete script data to V8. The embedder should implement a
   * subclass of this class.
   */
  class V8_EXPORT ExternalSourceStream {
   public:
    virtual ~ExternalSourceStream() = default;

    /**
     * V8 calls this to request the next chunk of data from the embedder. This
     * function will be called on a background thread, so it's OK to block and
     * wait for the data, if the embedder doesn't have data yet. Returns the
     * length of the data returned. When the data ends, GetMoreData should
     * return 0. Caller takes ownership of the data.
     *
     * When streaming UTF-8 data, V8 handles multi-byte characters split between
     * two data chunks, but doesn't handle multi-byte characters split between
     * more than two data chunks. The embedder can avoid this problem by always
     * returning at least 2 bytes of data.
     *
     * When streaming UTF-16 data, V8 does not handle characters split between
     * two data chunks. The embedder has to make sure that chunks have an even
     * length.
     *
     * If the embedder wants to cancel the streaming, they should make the next
     * GetMoreData call return 0. V8 will interpret it as end of data (and most
     * probably, parsing will fail). The streaming task will return as soon as
     * V8 has parsed the data it received so far.
     */
    virtual size_t GetMoreData(const uint8_t** src) = 0;

    /**
     * V8 calls this method to set a 'bookmark' at the current position in
     * the source stream, for the purpose of (maybe) later calling
     * ResetToBookmark. If ResetToBookmark is called later, then subsequent
     * calls to GetMoreData should return the same data as they did when
     * SetBookmark was called earlier.
     *
     * The embedder may return 'false' to indicate it cannot provide this
     * functionality.
     */
    virtual bool SetBookmark();

    /**
     * V8 calls this to return to a previously set bookmark.
     */
    virtual void ResetToBookmark();
  };

  /**
   * Source code which can be streamed into V8 in pieces. It will be parsed
   * while streaming and compiled after parsing has completed. StreamedSource
   * must be kept alive while the streaming task is run (see ScriptStreamingTask
   * below).
   */
  class V8_EXPORT StreamedSource {
   public:
    enum Encoding { ONE_BYTE, TWO_BYTE, UTF8 };

#if defined(_MSC_VER) && _MSC_VER >= 1910 /* Disable on VS2015 */
    V8_DEPRECATE_SOON(
        "This class takes ownership of source_stream, so use the constructor "
        "taking a unique_ptr to make these semantics clearer")
#endif
    StreamedSource(ExternalSourceStream* source_stream, Encoding encoding);
    StreamedSource(std::unique_ptr<ExternalSourceStream> source_stream,
                   Encoding encoding);
    ~StreamedSource();

    internal::ScriptStreamingData* impl() const { return impl_.get(); }

    // Prevent copying.
    StreamedSource(const StreamedSource&) = delete;
    StreamedSource& operator=(const StreamedSource&) = delete;

   private:
    std::unique_ptr<internal::ScriptStreamingData> impl_;
  };

  /**
   * A streaming task which the embedder must run on a background thread to
   * stream scripts into V8. Returned by ScriptCompiler::StartStreamingScript.
   */
  class V8_EXPORT ScriptStreamingTask final {
   public:
    void Run();

   private:
    friend class ScriptCompiler;

    explicit ScriptStreamingTask(internal::ScriptStreamingData* data)
        : data_(data) {}

    internal::ScriptStreamingData* data_;
  };

  enum CompileOptions {
    kNoCompileOptions = 0,
    kConsumeCodeCache,
    kEagerCompile
  };

  /**
   * The reason for which we are not requesting or providing a code cache.
   */
  enum NoCacheReason {
    kNoCacheNoReason = 0,
    kNoCacheBecauseCachingDisabled,
    kNoCacheBecauseNoResource,
    kNoCacheBecauseInlineScript,
    kNoCacheBecauseModule,
    kNoCacheBecauseStreamingSource,
    kNoCacheBecauseInspector,
    kNoCacheBecauseScriptTooSmall,
    kNoCacheBecauseCacheTooCold,
    kNoCacheBecauseV8Extension,
    kNoCacheBecauseExtensionModule,
    kNoCacheBecausePacScript,
    kNoCacheBecauseInDocumentWrite,
    kNoCacheBecauseResourceWithNoCacheHandler,
    kNoCacheBecauseDeferredProduceCodeCache
  };

  /**
   * Compiles the specified script (context-independent).
   * Cached data as part of the source object can be optionally produced to be
   * consumed later to speed up compilation of identical source scripts.
   *
   * Note that when producing cached data, the source must point to NULL for
   * cached data. When consuming cached data, the cached data must have been
   * produced by the same version of V8.
   *
   * \param source Script source code.
   * \return Compiled script object (context independent; for running it must be
   *   bound to a context).
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<UnboundScript> CompileUnboundScript(
      Isolate* isolate, Source* source,
      CompileOptions options = kNoCompileOptions,
      NoCacheReason no_cache_reason = kNoCacheNoReason);

  /**
   * Compiles the specified script (bound to current context).
   *
   * \param source Script source code.
   * \param pre_data Pre-parsing data, as obtained by ScriptData::PreCompile()
   *   using pre_data speeds compilation if it's done multiple times.
   *   Owned by caller, no references are kept when this function returns.
   * \return Compiled script object, bound to the context that was active
   *   when this function was called. When run it will always use this
   *   context.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Source* source,
      CompileOptions options = kNoCompileOptions,
      NoCacheReason no_cache_reason = kNoCacheNoReason);

  /**
   * Returns a task which streams script data into V8, or NULL if the script
   * cannot be streamed. The user is responsible for running the task on a
   * background thread and deleting it. When ran, the task starts parsing the
   * script, and it will request data from the StreamedSource as needed. When
   * ScriptStreamingTask::Run exits, all data has been streamed and the script
   * can be compiled (see Compile below).
   *
   * This API allows to start the streaming with as little data as possible, and
   * the remaining data (for example, the ScriptOrigin) is passed to Compile.
   */
  static ScriptStreamingTask* StartStreamingScript(
      Isolate* isolate, StreamedSource* source,
      CompileOptions options = kNoCompileOptions);

  /**
   * Compiles a streamed script (bound to current context).
   *
   * This can only be called after the streaming has finished
   * (ScriptStreamingTask has been run). V8 doesn't construct the source string
   * during streaming, so the embedder needs to pass the full source here.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, StreamedSource* source,
      Local<String> full_source_string, const ScriptOrigin& origin);

  /**
   * Return a version tag for CachedData for the current V8 version & flags.
   *
   * This value is meant only for determining whether a previously generated
   * CachedData instance is still valid; the tag has no other meaing.
   *
   * Background: The data carried by CachedData may depend on the exact
   *   V8 version number or current compiler flags. This means that when
   *   persisting CachedData, the embedder must take care to not pass in
   *   data from another V8 version, or the same version with different
   *   features enabled.
   *
   *   The easiest way to do so is to clear the embedder's cache on any
   *   such change.
   *
   *   Alternatively, this tag can be stored alongside the cached data and
   *   compared when it is being used.
   */
  static uint32_t CachedDataVersionTag();

  /**
   * Compile an ES module, returning a Module that encapsulates
   * the compiled code.
   *
   * Corresponds to the ParseModule abstract operation in the
   * ECMAScript specification.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<Module> CompileModule(
      Isolate* isolate, Source* source,
      CompileOptions options = kNoCompileOptions,
      NoCacheReason no_cache_reason = kNoCacheNoReason);

  /**
   * Compile a function for a given context. This is equivalent to running
   *
   * with (obj) {
   *   return function(args) { ... }
   * }
   *
   * It is possible to specify multiple context extensions (obj in the above
   * example).
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<Function> CompileFunctionInContext(
      Local<Context> context, Source* source, size_t arguments_count,
      Local<String> arguments[], size_t context_extension_count,
      Local<Object> context_extensions[],
      CompileOptions options = kNoCompileOptions,
      NoCacheReason no_cache_reason = kNoCacheNoReason,
      Local<ScriptOrModule>* script_or_module_out = nullptr);

  /**
   * Creates and returns code cache for the specified unbound_script.
   * This will return nullptr if the script cannot be serialized. The
   * CachedData returned by this function should be owned by the caller.
   */
  static CachedData* CreateCodeCache(Local<UnboundScript> unbound_script);

  /**
   * Creates and returns code cache for the specified unbound_module_script.
   * This will return nullptr if the script cannot be serialized. The
   * CachedData returned by this function should be owned by the caller.
   */
  static CachedData* CreateCodeCache(
      Local<UnboundModuleScript> unbound_module_script);

  /**
   * Creates and returns code cache for the specified function that was
   * previously produced by CompileFunctionInContext.
   * This will return nullptr if the script cannot be serialized. The
   * CachedData returned by this function should be owned by the caller.
   */
  static CachedData* CreateCodeCacheForFunction(Local<Function> function);

 private:
  static V8_WARN_UNUSED_RESULT MaybeLocal<UnboundScript> CompileUnboundInternal(
      Isolate* isolate, Source* source, CompileOptions options,
      NoCacheReason no_cache_reason);
};


/**
 * An error message.
 */
class V8_EXPORT Message {
 public:
  Local<String> Get() const;

  /**
   * Return the isolate to which the Message belongs.
   */
  Isolate* GetIsolate() const;

  V8_WARN_UNUSED_RESULT MaybeLocal<String> GetSourceLine(
      Local<Context> context) const;

  /**
   * Returns the origin for the script from where the function causing the
   * error originates.
   */
  ScriptOrigin GetScriptOrigin() const;

  /**
   * Returns the resource name for the script from where the function causing
   * the error originates.
   */
  Local<Value> GetScriptResourceName() const;

  /**
   * Exception stack trace. By default stack traces are not captured for
   * uncaught exceptions. SetCaptureStackTraceForUncaughtExceptions allows
   * to change this option.
   */
  Local<StackTrace> GetStackTrace() const;

  /**
   * Returns the number, 1-based, of the line where the error occurred.
   */
  V8_WARN_UNUSED_RESULT Maybe<int> GetLineNumber(Local<Context> context) const;

  /**
   * Returns the index within the script of the first character where
   * the error occurred.
   */
  int GetStartPosition() const;

  /**
   * Returns the index within the script of the last character where
   * the error occurred.
   */
  int GetEndPosition() const;

  /**
   * Returns the Wasm function index where the error occurred. Returns -1 if
   * message is not from a Wasm script.
   */
  int GetWasmFunctionIndex() const;

  /**
   * Returns the error level of the message.
   */
  int ErrorLevel() const;

  /**
   * Returns the index within the line of the first character where
   * the error occurred.
   */
  int GetStartColumn() const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetStartColumn(Local<Context> context) const;

  /**
   * Returns the index within the line of the last character where
   * the error occurred.
   */
  int GetEndColumn() const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetEndColumn(Local<Context> context) const;

  /**
   * Passes on the value set by the embedder when it fed the script from which
   * this Message was generated to V8.
   */
  bool IsSharedCrossOrigin() const;
  bool IsOpaque() const;

  // TODO(1245381): Print to a string instead of on a FILE.
  static void PrintCurrentStackTrace(Isolate* isolate, FILE* out);

  static const int kNoLineNumberInfo = 0;
  static const int kNoColumnInfo = 0;
  static const int kNoScriptIdInfo = 0;
  static const int kNoWasmFunctionIndexInfo = -1;
};


/**
 * Representation of a JavaScript stack trace. The information collected is a
 * snapshot of the execution stack and the information remains valid after
 * execution continues.
 */
class V8_EXPORT StackTrace {
 public:
  /**
   * Flags that determine what information is placed captured for each
   * StackFrame when grabbing the current stack trace.
   * Note: these options are deprecated and we always collect all available
   * information (kDetailed).
   */
  enum StackTraceOptions {
    kLineNumber = 1,
    kColumnOffset = 1 << 1 | kLineNumber,
    kScriptName = 1 << 2,
    kFunctionName = 1 << 3,
    kIsEval = 1 << 4,
    kIsConstructor = 1 << 5,
    kScriptNameOrSourceURL = 1 << 6,
    kScriptId = 1 << 7,
    kExposeFramesAcrossSecurityOrigins = 1 << 8,
    kOverview = kLineNumber | kColumnOffset | kScriptName | kFunctionName,
    kDetailed = kOverview | kIsEval | kIsConstructor | kScriptNameOrSourceURL
  };

  /**
   * Returns a StackFrame at a particular index.
   */
  Local<StackFrame> GetFrame(Isolate* isolate, uint32_t index) const;

  /**
   * Returns the number of StackFrames.
   */
  int GetFrameCount() const;

  /**
   * Grab a snapshot of the current JavaScript execution stack.
   *
   * \param frame_limit The maximum number of stack frames we want to capture.
   * \param options Enumerates the set of things we will capture for each
   *   StackFrame.
   */
  static Local<StackTrace> CurrentStackTrace(
      Isolate* isolate, int frame_limit, StackTraceOptions options = kDetailed);
};


/**
 * A single JavaScript stack frame.
 */
class V8_EXPORT StackFrame {
 public:
  /**
   * Returns the number, 1-based, of the line for the associate function call.
   * This method will return Message::kNoLineNumberInfo if it is unable to
   * retrieve the line number, or if kLineNumber was not passed as an option
   * when capturing the StackTrace.
   */
  int GetLineNumber() const;

  /**
   * Returns the 1-based column offset on the line for the associated function
   * call.
   * This method will return Message::kNoColumnInfo if it is unable to retrieve
   * the column number, or if kColumnOffset was not passed as an option when
   * capturing the StackTrace.
   */
  int GetColumn() const;

  /**
   * Returns the id of the script for the function for this StackFrame.
   * This method will return Message::kNoScriptIdInfo if it is unable to
   * retrieve the script id, or if kScriptId was not passed as an option when
   * capturing the StackTrace.
   */
  int GetScriptId() const;

  /**
   * Returns the name of the resource that contains the script for the
   * function for this StackFrame.
   */
  Local<String> GetScriptName() const;

  /**
   * Returns the name of the resource that contains the script for the
   * function for this StackFrame or sourceURL value if the script name
   * is undefined and its source ends with //# sourceURL=... string or
   * deprecated //@ sourceURL=... string.
   */
  Local<String> GetScriptNameOrSourceURL() const;

  /**
   * Returns the name of the function associated with this stack frame.
   */
  Local<String> GetFunctionName() const;

  /**
   * Returns whether or not the associated function is compiled via a call to
   * eval().
   */
  bool IsEval() const;

  /**
   * Returns whether or not the associated function is called as a
   * constructor via "new".
   */
  bool IsConstructor() const;

  /**
   * Returns whether or not the associated functions is defined in wasm.
   */
  bool IsWasm() const;

  /**
   * Returns whether or not the associated function is defined by the user.
   */
  bool IsUserJavaScript() const;
};


// A StateTag represents a possible state of the VM.
enum StateTag {
  JS,
  GC,
  PARSER,
  BYTECODE_COMPILER,
  COMPILER,
  OTHER,
  EXTERNAL,
  ATOMICS_WAIT,
  IDLE
};

// A RegisterState represents the current state of registers used
// by the sampling profiler API.
struct RegisterState {
  RegisterState() : pc(nullptr), sp(nullptr), fp(nullptr), lr(nullptr) {}
  void* pc;  // Instruction pointer.
  void* sp;  // Stack pointer.
  void* fp;  // Frame pointer.
  void* lr;  // Link register (or nullptr on platforms without a link register).
};

// The output structure filled up by GetStackSample API function.
struct SampleInfo {
  size_t frames_count;            // Number of frames collected.
  StateTag vm_state;              // Current VM state.
  void* external_callback_entry;  // External callback address if VM is
                                  // executing an external callback.
  void* top_context;              // Incumbent native context address.
};

struct MemoryRange {
  const void* start = nullptr;
  size_t length_in_bytes = 0;
};

struct JSEntryStub {
  MemoryRange code;
};

struct UnwindState {
  MemoryRange code_range;
  MemoryRange embedded_code_range;
  JSEntryStub js_entry_stub;
  JSEntryStub js_construct_entry_stub;
  JSEntryStub js_run_microtasks_entry_stub;
};

struct JSEntryStubs {
  JSEntryStub js_entry_stub;
  JSEntryStub js_construct_entry_stub;
  JSEntryStub js_run_microtasks_entry_stub;
};

/**
 * A JSON Parser and Stringifier.
 */
class V8_EXPORT JSON {
 public:
  /**
   * Tries to parse the string |json_string| and returns it as value if
   * successful.
   *
   * \param the context in which to parse and create the value.
   * \param json_string The string to parse.
   * \return The corresponding value if successfully parsed.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<Value> Parse(
      Local<Context> context, Local<String> json_string);

  /**
   * Tries to stringify the JSON-serializable object |json_object| and returns
   * it as string if successful.
   *
   * \param json_object The JSON-serializable object to stringify.
   * \return The corresponding string if successfully stringified.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> Stringify(
      Local<Context> context, Local<Value> json_object,
      Local<String> gap = Local<String>());
};

/**
 * Value serialization compatible with the HTML structured clone algorithm.
 * The format is backward-compatible (i.e. safe to store to disk).
 */
class V8_EXPORT ValueSerializer {
 public:
  class V8_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    /**
     * Handles the case where a DataCloneError would be thrown in the structured
     * clone spec. Other V8 embedders may throw some other appropriate exception
     * type.
     */
    virtual void ThrowDataCloneError(Local<String> message) = 0;

    /**
     * The embedder overrides this method to write some kind of host object, if
     * possible. If not, a suitable exception should be thrown and
     * Nothing<bool>() returned.
     */
    virtual Maybe<bool> WriteHostObject(Isolate* isolate, Local<Object> object);

    /**
     * Called when the ValueSerializer is going to serialize a
     * SharedArrayBuffer object. The embedder must return an ID for the
     * object, using the same ID if this SharedArrayBuffer has already been
     * serialized in this buffer. When deserializing, this ID will be passed to
     * ValueDeserializer::GetSharedArrayBufferFromId as |clone_id|.
     *
     * If the object cannot be serialized, an
     * exception should be thrown and Nothing<uint32_t>() returned.
     */
    virtual Maybe<uint32_t> GetSharedArrayBufferId(
        Isolate* isolate, Local<SharedArrayBuffer> shared_array_buffer);

    virtual Maybe<uint32_t> GetWasmModuleTransferId(
        Isolate* isolate, Local<WasmModuleObject> module);
    /**
     * Allocates memory for the buffer of at least the size provided. The actual
     * size (which may be greater or equal) is written to |actual_size|. If no
     * buffer has been allocated yet, nullptr will be provided.
     *
     * If the memory cannot be allocated, nullptr should be returned.
     * |actual_size| will be ignored. It is assumed that |old_buffer| is still
     * valid in this case and has not been modified.
     *
     * The default implementation uses the stdlib's `realloc()` function.
     */
    virtual void* ReallocateBufferMemory(void* old_buffer, size_t size,
                                         size_t* actual_size);

    /**
     * Frees a buffer allocated with |ReallocateBufferMemory|.
     *
     * The default implementation uses the stdlib's `free()` function.
     */
    virtual void FreeBufferMemory(void* buffer);
  };

  explicit ValueSerializer(Isolate* isolate);
  ValueSerializer(Isolate* isolate, Delegate* delegate);
  ~ValueSerializer();

  /**
   * Writes out a header, which includes the format version.
   */
  void WriteHeader();

  /**
   * Serializes a JavaScript value into the buffer.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> WriteValue(Local<Context> context,
                                               Local<Value> value);

  /**
   * Returns the stored data (allocated using the delegate's
   * ReallocateBufferMemory) and its size. This serializer should not be used
   * once the buffer is released. The contents are undefined if a previous write
   * has failed. Ownership of the buffer is transferred to the caller.
   */
  V8_WARN_UNUSED_RESULT std::pair<uint8_t*, size_t> Release();

  /**
   * Marks an ArrayBuffer as havings its contents transferred out of band.
   * Pass the corresponding ArrayBuffer in the deserializing context to
   * ValueDeserializer::TransferArrayBuffer.
   */
  void TransferArrayBuffer(uint32_t transfer_id,
                           Local<ArrayBuffer> array_buffer);


  /**
   * Indicate whether to treat ArrayBufferView objects as host objects,
   * i.e. pass them to Delegate::WriteHostObject. This should not be
   * called when no Delegate was passed.
   *
   * The default is not to treat ArrayBufferViews as host objects.
   */
  void SetTreatArrayBufferViewsAsHostObjects(bool mode);

  /**
   * Write raw data in various common formats to the buffer.
   * Note that integer types are written in base-128 varint format, not with a
   * binary copy. For use during an override of Delegate::WriteHostObject.
   */
  void WriteUint32(uint32_t value);
  void WriteUint64(uint64_t value);
  void WriteDouble(double value);
  void WriteRawBytes(const void* source, size_t length);

  ValueSerializer(const ValueSerializer&) = delete;
  void operator=(const ValueSerializer&) = delete;

 private:
  struct PrivateData;
  PrivateData* private_;
};

/**
 * Deserializes values from data written with ValueSerializer, or a compatible
 * implementation.
 */
class V8_EXPORT ValueDeserializer {
 public:
  class V8_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    /**
     * The embedder overrides this method to read some kind of host object, if
     * possible. If not, a suitable exception should be thrown and
     * MaybeLocal<Object>() returned.
     */
    virtual MaybeLocal<Object> ReadHostObject(Isolate* isolate);

    /**
     * Get a WasmModuleObject given a transfer_id previously provided
     * by ValueSerializer::GetWasmModuleTransferId
     */
    virtual MaybeLocal<WasmModuleObject> GetWasmModuleFromId(
        Isolate* isolate, uint32_t transfer_id);

    /**
     * Get a SharedArrayBuffer given a clone_id previously provided
     * by ValueSerializer::GetSharedArrayBufferId
     */
    virtual MaybeLocal<SharedArrayBuffer> GetSharedArrayBufferFromId(
        Isolate* isolate, uint32_t clone_id);
  };

  ValueDeserializer(Isolate* isolate, const uint8_t* data, size_t size);
  ValueDeserializer(Isolate* isolate, const uint8_t* data, size_t size,
                    Delegate* delegate);
  ~ValueDeserializer();

  /**
   * Reads and validates a header (including the format version).
   * May, for example, reject an invalid or unsupported wire format.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> ReadHeader(Local<Context> context);

  /**
   * Deserializes a JavaScript value from the buffer.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> ReadValue(Local<Context> context);

  /**
   * Accepts the array buffer corresponding to the one passed previously to
   * ValueSerializer::TransferArrayBuffer.
   */
  void TransferArrayBuffer(uint32_t transfer_id,
                           Local<ArrayBuffer> array_buffer);

  /**
   * Similar to TransferArrayBuffer, but for SharedArrayBuffer.
   * The id is not necessarily in the same namespace as unshared ArrayBuffer
   * objects.
   */
  void TransferSharedArrayBuffer(uint32_t id,
                                 Local<SharedArrayBuffer> shared_array_buffer);

  /**
   * Must be called before ReadHeader to enable support for reading the legacy
   * wire format (i.e., which predates this being shipped).
   *
   * Don't use this unless you need to read data written by previous versions of
   * blink::ScriptValueSerializer.
   */
  void SetSupportsLegacyWireFormat(bool supports_legacy_wire_format);

  /**
   * Reads the underlying wire format version. Likely mostly to be useful to
   * legacy code reading old wire format versions. Must be called after
   * ReadHeader.
   */
  uint32_t GetWireFormatVersion() const;

  /**
   * Reads raw data in various common formats to the buffer.
   * Note that integer types are read in base-128 varint format, not with a
   * binary copy. For use during an override of Delegate::ReadHostObject.
   */
  V8_WARN_UNUSED_RESULT bool ReadUint32(uint32_t* value);
  V8_WARN_UNUSED_RESULT bool ReadUint64(uint64_t* value);
  V8_WARN_UNUSED_RESULT bool ReadDouble(double* value);
  V8_WARN_UNUSED_RESULT bool ReadRawBytes(size_t length, const void** data);

  ValueDeserializer(const ValueDeserializer&) = delete;
  void operator=(const ValueDeserializer&) = delete;

 private:
  struct PrivateData;
  PrivateData* private_;
};


// --- Value ---


/**
 * The superclass of all JavaScript values and objects.
 */
class V8_EXPORT Value : public Data {
 public:
  /**
   * Returns true if this value is the undefined value.  See ECMA-262
   * 4.3.10.
   *
   * This is equivalent to `value === undefined` in JS.
   */
  V8_INLINE bool IsUndefined() const;

  /**
   * Returns true if this value is the null value.  See ECMA-262
   * 4.3.11.
   *
   * This is equivalent to `value === null` in JS.
   */
  V8_INLINE bool IsNull() const;

  /**
   * Returns true if this value is either the null or the undefined value.
   * See ECMA-262
   * 4.3.11. and 4.3.12
   *
   * This is equivalent to `value == null` in JS.
   */
  V8_INLINE bool IsNullOrUndefined() const;

  /**
   * Returns true if this value is true.
   *
   * This is not the same as `BooleanValue()`. The latter performs a
   * conversion to boolean, i.e. the result of `Boolean(value)` in JS, whereas
   * this checks `value === true`.
   */
  bool IsTrue() const;

  /**
   * Returns true if this value is false.
   *
   * This is not the same as `!BooleanValue()`. The latter performs a
   * conversion to boolean, i.e. the result of `!Boolean(value)` in JS, whereas
   * this checks `value === false`.
   */
  bool IsFalse() const;

  /**
   * Returns true if this value is a symbol or a string.
   *
   * This is equivalent to
   * `typeof value === 'string' || typeof value === 'symbol'` in JS.
   */
  bool IsName() const;

  /**
   * Returns true if this value is an instance of the String type.
   * See ECMA-262 8.4.
   *
   * This is equivalent to `typeof value === 'string'` in JS.
   */
  V8_INLINE bool IsString() const;

  /**
   * Returns true if this value is a symbol.
   *
   * This is equivalent to `typeof value === 'symbol'` in JS.
   */
  bool IsSymbol() const;

  /**
   * Returns true if this value is a function.
   *
   * This is equivalent to `typeof value === 'function'` in JS.
   */
  bool IsFunction() const;

  /**
   * Returns true if this value is an array. Note that it will return false for
   * an Proxy for an array.
   */
  bool IsArray() const;

  /**
   * Returns true if this value is an object.
   */
  bool IsObject() const;

  /**
   * Returns true if this value is a bigint.
   *
   * This is equivalent to `typeof value === 'bigint'` in JS.
   */
  bool IsBigInt() const;

  /**
   * Returns true if this value is boolean.
   *
   * This is equivalent to `typeof value === 'boolean'` in JS.
   */
  bool IsBoolean() const;

  /**
   * Returns true if this value is a number.
   *
   * This is equivalent to `typeof value === 'number'` in JS.
   */
  bool IsNumber() const;

  /**
   * Returns true if this value is an `External` object.
   */
  bool IsExternal() const;

  /**
   * Returns true if this value is a 32-bit signed integer.
   */
  bool IsInt32() const;

  /**
   * Returns true if this value is a 32-bit unsigned integer.
   */
  bool IsUint32() const;

  /**
   * Returns true if this value is a Date.
   */
  bool IsDate() const;

  /**
   * Returns true if this value is an Arguments object.
   */
  bool IsArgumentsObject() const;

  /**
   * Returns true if this value is a BigInt object.
   */
  bool IsBigIntObject() const;

  /**
   * Returns true if this value is a Boolean object.
   */
  bool IsBooleanObject() const;

  /**
   * Returns true if this value is a Number object.
   */
  bool IsNumberObject() const;

  /**
   * Returns true if this value is a String object.
   */
  bool IsStringObject() const;

  /**
   * Returns true if this value is a Symbol object.
   */
  bool IsSymbolObject() const;

  /**
   * Returns true if this value is a NativeError.
   */
  bool IsNativeError() const;

  /**
   * Returns true if this value is a RegExp.
   */
  bool IsRegExp() const;

  /**
   * Returns true if this value is an async function.
   */
  bool IsAsyncFunction() const;

  /**
   * Returns true if this value is a Generator function.
   */
  bool IsGeneratorFunction() const;

  /**
   * Returns true if this value is a Generator object (iterator).
   */
  bool IsGeneratorObject() const;

  /**
   * Returns true if this value is a Promise.
   */
  bool IsPromise() const;

  /**
   * Returns true if this value is a Map.
   */
  bool IsMap() const;

  /**
   * Returns true if this value is a Set.
   */
  bool IsSet() const;

  /**
   * Returns true if this value is a Map Iterator.
   */
  bool IsMapIterator() const;

  /**
   * Returns true if this value is a Set Iterator.
   */
  bool IsSetIterator() const;

  /**
   * Returns true if this value is a WeakMap.
   */
  bool IsWeakMap() const;

  /**
   * Returns true if this value is a WeakSet.
   */
  bool IsWeakSet() const;

  /**
   * Returns true if this value is an ArrayBuffer.
   */
  bool IsArrayBuffer() const;

  /**
   * Returns true if this value is an ArrayBufferView.
   */
  bool IsArrayBufferView() const;

  /**
   * Returns true if this value is one of TypedArrays.
   */
  bool IsTypedArray() const;

  /**
   * Returns true if this value is an Uint8Array.
   */
  bool IsUint8Array() const;

  /**
   * Returns true if this value is an Uint8ClampedArray.
   */
  bool IsUint8ClampedArray() const;

  /**
   * Returns true if this value is an Int8Array.
   */
  bool IsInt8Array() const;

  /**
   * Returns true if this value is an Uint16Array.
   */
  bool IsUint16Array() const;

  /**
   * Returns true if this value is an Int16Array.
   */
  bool IsInt16Array() const;

  /**
   * Returns true if this value is an Uint32Array.
   */
  bool IsUint32Array() const;

  /**
   * Returns true if this value is an Int32Array.
   */
  bool IsInt32Array() const;

  /**
   * Returns true if this value is a Float32Array.
   */
  bool IsFloat32Array() const;

  /**
   * Returns true if this value is a Float64Array.
   */
  bool IsFloat64Array() const;

  /**
   * Returns true if this value is a BigInt64Array.
   */
  bool IsBigInt64Array() const;

  /**
   * Returns true if this value is a BigUint64Array.
   */
  bool IsBigUint64Array() const;

  /**
   * Returns true if this value is a DataView.
   */
  bool IsDataView() const;

  /**
   * Returns true if this value is a SharedArrayBuffer.
   */
  bool IsSharedArrayBuffer() const;

  /**
   * Returns true if this value is a JavaScript Proxy.
   */
  bool IsProxy() const;

  /**
   * Returns true if this value is a WasmModuleObject.
   */
  bool IsWasmModuleObject() const;

  /**
   * Returns true if the value is a Module Namespace Object.
   */
  bool IsModuleNamespaceObject() const;

  /**
   * Perform the equivalent of `BigInt(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<BigInt> ToBigInt(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Number> ToNumber(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `String(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToString(
      Local<Context> context) const;
  /**
   * Provide a string representation of this value usable for debugging.
   * This operation has no observable side effects and will succeed
   * unless e.g. execution is being terminated.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToDetailString(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Object(value)` in JS.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> ToObject(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS and convert the result
   * to an integer. Negative values are rounded up, positive values are rounded
   * down. NaN is converted to 0. Infinite values yield undefined results.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Integer> ToInteger(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS and convert the result
   * to an unsigned 32-bit integer by performing the steps in
   * https://tc39.es/ecma262/#sec-touint32.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToUint32(
      Local<Context> context) const;
  /**
   * Perform the equivalent of `Number(value)` in JS and convert the result
   * to a signed 32-bit integer by performing the steps in
   * https://tc39.es/ecma262/#sec-toint32.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Int32> ToInt32(Local<Context> context) const;

  /**
   * Perform the equivalent of `Boolean(value)` in JS. This can never fail.
   */
  Local<Boolean> ToBoolean(Isolate* isolate) const;

  /**
   * Attempts to convert a string to an array index.
   * Returns an empty handle if the conversion fails.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToArrayIndex(
      Local<Context> context) const;

  /** Returns the equivalent of `ToBoolean()->Value()`. */
  bool BooleanValue(Isolate* isolate) const;

  /** Returns the equivalent of `ToNumber()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<double> NumberValue(Local<Context> context) const;
  /** Returns the equivalent of `ToInteger()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<int64_t> IntegerValue(
      Local<Context> context) const;
  /** Returns the equivalent of `ToUint32()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<uint32_t> Uint32Value(
      Local<Context> context) const;
  /** Returns the equivalent of `ToInt32()->Value()`. */
  V8_WARN_UNUSED_RESULT Maybe<int32_t> Int32Value(Local<Context> context) const;

  /** JS == */
  V8_WARN_UNUSED_RESULT Maybe<bool> Equals(Local<Context> context,
                                           Local<Value> that) const;
  bool StrictEquals(Local<Value> that) const;
  bool SameValue(Local<Value> that) const;

  template <class T> V8_INLINE static Value* Cast(T* value);

  Local<String> TypeOf(Isolate*);

  Maybe<bool> InstanceOf(Local<Context> context, Local<Object> object);

 private:
  V8_INLINE bool QuickIsUndefined() const;
  V8_INLINE bool QuickIsNull() const;
  V8_INLINE bool QuickIsNullOrUndefined() const;
  V8_INLINE bool QuickIsString() const;
  bool FullIsUndefined() const;
  bool FullIsNull() const;
  bool FullIsString() const;
};


/**
 * The superclass of primitive values.  See ECMA-262 4.3.2.
 */
class V8_EXPORT Primitive : public Value { };


/**
 * A primitive boolean value (ECMA-262, 4.3.14).  Either the true
 * or false value.
 */
class V8_EXPORT Boolean : public Primitive {
 public:
  bool Value() const;
  V8_INLINE static Boolean* Cast(v8::Value* obj);
  V8_INLINE static Local<Boolean> New(Isolate* isolate, bool value);

 private:
  static void CheckCast(v8::Value* obj);
};


/**
 * A superclass for symbols and strings.
 */
class V8_EXPORT Name : public Primitive {
 public:
  /**
   * Returns the identity hash for this object. The current implementation
   * uses an inline property on the object to store the identity hash.
   *
   * The return value will never be 0. Also, it is not guaranteed to be
   * unique.
   */
  int GetIdentityHash();

  V8_INLINE static Name* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};

/**
 * A flag describing different modes of string creation.
 *
 * Aside from performance implications there are no differences between the two
 * creation modes.
 */
enum class NewStringType {
  /**
   * Create a new string, always allocating new storage memory.
   */
  kNormal,

  /**
   * Acts as a hint that the string should be created in the
   * old generation heap space and be deduplicated if an identical string
   * already exists.
   */
  kInternalized
};

/**
 * A JavaScript string value (ECMA-262, 4.3.17).
 */
class V8_EXPORT String : public Name {
 public:
  static constexpr int kMaxLength =
      internal::kApiSystemPointerSize == 4 ? (1 << 28) - 16 : (1 << 29) - 24;

  enum Encoding {
    UNKNOWN_ENCODING = 0x1,
    TWO_BYTE_ENCODING = 0x0,
    ONE_BYTE_ENCODING = 0x8
  };
  /**
   * Returns the number of characters (UTF-16 code units) in this string.
   */
  int Length() const;

  /**
   * Returns the number of bytes in the UTF-8 encoded
   * representation of this string.
   */
  int Utf8Length(Isolate* isolate) const;

  /**
   * Returns whether this string is known to contain only one byte data,
   * i.e. ISO-8859-1 code points.
   * Does not read the string.
   * False negatives are possible.
   */
  bool IsOneByte() const;

  /**
   * Returns whether this string contain only one byte data,
   * i.e. ISO-8859-1 code points.
   * Will read the entire string in some cases.
   */
  bool ContainsOnlyOneByte() const;

  /**
   * Write the contents of the string to an external buffer.
   * If no arguments are given, expects the buffer to be large
   * enough to hold the entire string and NULL terminator. Copies
   * the contents of the string and the NULL terminator into the
   * buffer.
   *
   * WriteUtf8 will not write partial UTF-8 sequences, preferring to stop
   * before the end of the buffer.
   *
   * Copies up to length characters into the output buffer.
   * Only null-terminates if there is enough space in the buffer.
   *
   * \param buffer The buffer into which the string will be copied.
   * \param start The starting position within the string at which
   * copying begins.
   * \param length The number of characters to copy from the string.  For
   *    WriteUtf8 the number of bytes in the buffer.
   * \param nchars_ref The number of characters written, can be NULL.
   * \param options Various options that might affect performance of this or
   *    subsequent operations.
   * \return The number of characters copied to the buffer excluding the null
   *    terminator.  For WriteUtf8: The number of bytes copied to the buffer
   *    including the null terminator (if written).
   */
  enum WriteOptions {
    NO_OPTIONS = 0,
    HINT_MANY_WRITES_EXPECTED = 1,
    NO_NULL_TERMINATION = 2,
    PRESERVE_ONE_BYTE_NULL = 4,
    // Used by WriteUtf8 to replace orphan surrogate code units with the
    // unicode replacement character. Needs to be set to guarantee valid UTF-8
    // output.
    REPLACE_INVALID_UTF8 = 8
  };

  // 16-bit character codes.
  int Write(Isolate* isolate, uint16_t* buffer, int start = 0, int length = -1,
            int options = NO_OPTIONS) const;
  // One byte characters.
  int WriteOneByte(Isolate* isolate, uint8_t* buffer, int start = 0,
                   int length = -1, int options = NO_OPTIONS) const;
  // UTF-8 encoded characters.
  int WriteUtf8(Isolate* isolate, char* buffer, int length = -1,
                int* nchars_ref = nullptr, int options = NO_OPTIONS) const;

  /**
   * A zero length string.
   */
  V8_INLINE static Local<String> Empty(Isolate* isolate);

  /**
   * Returns true if the string is external
   */
  bool IsExternal() const;

  /**
   * Returns true if the string is both external and one-byte.
   */
  bool IsExternalOneByte() const;

  class V8_EXPORT ExternalStringResourceBase {  // NOLINT
   public:
    virtual ~ExternalStringResourceBase() = default;

    /**
     * If a string is cacheable, the value returned by
     * ExternalStringResource::data() may be cached, otherwise it is not
     * expected to be stable beyond the current top-level task.
     */
    virtual bool IsCacheable() const { return true; }

    // Disallow copying and assigning.
    ExternalStringResourceBase(const ExternalStringResourceBase&) = delete;
    void operator=(const ExternalStringResourceBase&) = delete;

   protected:
    ExternalStringResourceBase() = default;

    /**
     * Internally V8 will call this Dispose method when the external string
     * resource is no longer needed. The default implementation will use the
     * delete operator. This method can be overridden in subclasses to
     * control how allocated external string resources are disposed.
     */
    virtual void Dispose() { delete this; }

    /**
     * For a non-cacheable string, the value returned by
     * |ExternalStringResource::data()| has to be stable between |Lock()| and
     * |Unlock()|, that is the string must behave as is |IsCacheable()| returned
     * true.
     *
     * These two functions must be thread-safe, and can be called from anywhere.
     * They also must handle lock depth, in the sense that each can be called
     * several times, from different threads, and unlocking should only happen
     * when the balance of Lock() and Unlock() calls is 0.
     */
    virtual void Lock() const {}

    /**
     * Unlocks the string.
     */
    virtual void Unlock() const {}

   private:
    friend class internal::ExternalString;
    friend class v8::String;
    friend class internal::ScopedExternalStringLock;
  };

  /**
   * An ExternalStringResource is a wrapper around a two-byte string
   * buffer that resides outside V8's heap. Implement an
   * ExternalStringResource to manage the life cycle of the underlying
   * buffer.  Note that the string data must be immutable.
   */
  class V8_EXPORT ExternalStringResource
      : public ExternalStringResourceBase {
   public:
    /**
     * Override the destructor to manage the life cycle of the underlying
     * buffer.
     */
    ~ExternalStringResource() override = default;

    /**
     * The string data from the underlying buffer.
     */
    virtual const uint16_t* data() const = 0;

    /**
     * The length of the string. That is, the number of two-byte characters.
     */
    virtual size_t length() const = 0;

   protected:
    ExternalStringResource() = default;
  };

  /**
   * An ExternalOneByteStringResource is a wrapper around an one-byte
   * string buffer that resides outside V8's heap. Implement an
   * ExternalOneByteStringResource to manage the life cycle of the
   * underlying buffer.  Note that the string data must be immutable
   * and that the data must be Latin-1 and not UTF-8, which would require
   * special treatment internally in the engine and do not allow efficient
   * indexing.  Use String::New or convert to 16 bit data for non-Latin1.
   */

  class V8_EXPORT ExternalOneByteStringResource
      : public ExternalStringResourceBase {
   public:
    /**
     * Override the destructor to manage the life cycle of the underlying
     * buffer.
     */
    ~ExternalOneByteStringResource() override = default;
    /** The string data from the underlying buffer.*/
    virtual const char* data() const = 0;
    /** The number of Latin-1 characters in the string.*/
    virtual size_t length() const = 0;
   protected:
    ExternalOneByteStringResource() = default;
  };

  /**
   * If the string is an external string, return the ExternalStringResourceBase
   * regardless of the encoding, otherwise return NULL.  The encoding of the
   * string is returned in encoding_out.
   */
  V8_INLINE ExternalStringResourceBase* GetExternalStringResourceBase(
      Encoding* encoding_out) const;

  /**
   * Get the ExternalStringResource for an external string.  Returns
   * NULL if IsExternal() doesn't return true.
   */
  V8_INLINE ExternalStringResource* GetExternalStringResource() const;

  /**
   * Get the ExternalOneByteStringResource for an external one-byte string.
   * Returns NULL if IsExternalOneByte() doesn't return true.
   */
  const ExternalOneByteStringResource* GetExternalOneByteStringResource() const;

  V8_INLINE static String* Cast(v8::Value* obj);

  /**
   * Allocates a new string from a UTF-8 literal. This is equivalent to calling
   * String::NewFromUtf(isolate, "...").ToLocalChecked(), but without the check
   * overhead.
   *
   * When called on a string literal containing '\0', the inferred length is the
   * length of the input array minus 1 (for the final '\0') and not the value
   * returned by strlen.
   **/
  template <int N>
  static V8_WARN_UNUSED_RESULT Local<String> NewFromUtf8Literal(
      Isolate* isolate, const char (&literal)[N],
      NewStringType type = NewStringType::kNormal) {
    static_assert(N <= kMaxLength, "String is too long");
    return NewFromUtf8Literal(isolate, literal, type, N - 1);
  }

  /** Allocates a new string from UTF-8 data. Only returns an empty value when
   * length > kMaxLength. **/
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromUtf8(
      Isolate* isolate, const char* data,
      NewStringType type = NewStringType::kNormal, int length = -1);

  /** Allocates a new string from Latin-1 data.  Only returns an empty value
   * when length > kMaxLength. **/
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromOneByte(
      Isolate* isolate, const uint8_t* data,
      NewStringType type = NewStringType::kNormal, int length = -1);

  /** Allocates a new string from UTF-16 data. Only returns an empty value when
   * length > kMaxLength. **/
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromTwoByte(
      Isolate* isolate, const uint16_t* data,
      NewStringType type = NewStringType::kNormal, int length = -1);

  /**
   * Creates a new string by concatenating the left and the right strings
   * passed in as parameters.
   */
  static Local<String> Concat(Isolate* isolate, Local<String> left,
                              Local<String> right);

  /**
   * Creates a new external string using the data defined in the given
   * resource. When the external string is no longer live on V8's heap the
   * resource will be disposed by calling its Dispose method. The caller of
   * this function should not otherwise delete or modify the resource. Neither
   * should the underlying buffer be deallocated or modified except through the
   * destructor of the external string resource.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewExternalTwoByte(
      Isolate* isolate, ExternalStringResource* resource);

  /**
   * Associate an external string resource with this string by transforming it
   * in place so that existing references to this string in the JavaScript heap
   * will use the external string resource. The external string resource's
   * character contents need to be equivalent to this string.
   * Returns true if the string has been changed to be an external string.
   * The string is not modified if the operation fails. See NewExternal for
   * information on the lifetime of the resource.
   */
  bool MakeExternal(ExternalStringResource* resource);

  /**
   * Creates a new external string using the one-byte data defined in the given
   * resource. When the external string is no longer live on V8's heap the
   * resource will be disposed by calling its Dispose method. The caller of
   * this function should not otherwise delete or modify the resource. Neither
   * should the underlying buffer be deallocated or modified except through the
   * destructor of the external string resource.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewExternalOneByte(
      Isolate* isolate, ExternalOneByteStringResource* resource);

  /**
   * Associate an external string resource with this string by transforming it
   * in place so that existing references to this string in the JavaScript heap
   * will use the external string resource. The external string resource's
   * character contents need to be equivalent to this string.
   * Returns true if the string has been changed to be an external string.
   * The string is not modified if the operation fails. See NewExternal for
   * information on the lifetime of the resource.
   */
  bool MakeExternal(ExternalOneByteStringResource* resource);

  /**
   * Returns true if this string can be made external.
   */
  bool CanMakeExternal();

  /**
   * Returns true if the strings values are equal. Same as JS ==/===.
   */
  bool StringEquals(Local<String> str);

  /**
   * Converts an object to a UTF-8-encoded character array.  Useful if
   * you want to print the object.  If conversion to a string fails
   * (e.g. due to an exception in the toString() method of the object)
   * then the length() method returns 0 and the * operator returns
   * NULL.
   */
  class V8_EXPORT Utf8Value {
   public:
    Utf8Value(Isolate* isolate, Local<v8::Value> obj);
    ~Utf8Value();
    char* operator*() { return str_; }
    const char* operator*() const { return str_; }
    int length() const { return length_; }

    // Disallow copying and assigning.
    Utf8Value(const Utf8Value&) = delete;
    void operator=(const Utf8Value&) = delete;

   private:
    char* str_;
    int length_;
  };

  /**
   * Converts an object to a two-byte (UTF-16-encoded) string.
   * If conversion to a string fails (eg. due to an exception in the toString()
   * method of the object) then the length() method returns 0 and the * operator
   * returns NULL.
   */
  class V8_EXPORT Value {
   public:
    Value(Isolate* isolate, Local<v8::Value> obj);
    ~Value();
    uint16_t* operator*() { return str_; }
    const uint16_t* operator*() const { return str_; }
    int length() const { return length_; }

    // Disallow copying and assigning.
    Value(const Value&) = delete;
    void operator=(const Value&) = delete;

   private:
    uint16_t* str_;
    int length_;
  };

 private:
  void VerifyExternalStringResourceBase(ExternalStringResourceBase* v,
                                        Encoding encoding) const;
  void VerifyExternalStringResource(ExternalStringResource* val) const;
  ExternalStringResource* GetExternalStringResourceSlow() const;
  ExternalStringResourceBase* GetExternalStringResourceBaseSlow(
      String::Encoding* encoding_out) const;

  static Local<v8::String> NewFromUtf8Literal(Isolate* isolate,
                                              const char* literal,
                                              NewStringType type, int length);

  static void CheckCast(v8::Value* obj);
};

// Zero-length string specialization (templated string size includes
// terminator).
template <>
inline V8_WARN_UNUSED_RESULT Local<String> String::NewFromUtf8Literal(
    Isolate* isolate, const char (&literal)[1], NewStringType type) {
  return String::Empty(isolate);
}

/**
 * A JavaScript symbol (ECMA-262 edition 6)
 */
class V8_EXPORT Symbol : public Name {
 public:
  /**
   * Returns the description string of the symbol, or undefined if none.
   */
  Local<Value> Description() const;

  V8_DEPRECATE_SOON("Use Symbol::Description()")
  Local<Value> Name() const { return Description(); }

  /**
   * Create a symbol. If description is not empty, it will be used as the
   * description.
   */
  static Local<Symbol> New(Isolate* isolate,
                           Local<String> description = Local<String>());

  /**
   * Access global symbol registry.
   * Note that symbols created this way are never collected, so
   * they should only be used for statically fixed properties.
   * Also, there is only one global name space for the descriptions used as
   * keys.
   * To minimize the potential for clashes, use qualified names as keys.
   */
  static Local<Symbol> For(Isolate* isolate, Local<String> description);

  /**
   * Retrieve a global symbol. Similar to |For|, but using a separate
   * registry that is not accessible by (and cannot clash with) JavaScript code.
   */
  static Local<Symbol> ForApi(Isolate* isolate, Local<String> description);

  // Well-known symbols
  static Local<Symbol> GetAsyncIterator(Isolate* isolate);
  static Local<Symbol> GetHasInstance(Isolate* isolate);
  static Local<Symbol> GetIsConcatSpreadable(Isolate* isolate);
  static Local<Symbol> GetIterator(Isolate* isolate);
  static Local<Symbol> GetMatch(Isolate* isolate);
  static Local<Symbol> GetReplace(Isolate* isolate);
  static Local<Symbol> GetSearch(Isolate* isolate);
  static Local<Symbol> GetSplit(Isolate* isolate);
  static Local<Symbol> GetToPrimitive(Isolate* isolate);
  static Local<Symbol> GetToStringTag(Isolate* isolate);
  static Local<Symbol> GetUnscopables(Isolate* isolate);

  V8_INLINE static Symbol* Cast(Value* obj);

 private:
  Symbol();
  static void CheckCast(Value* obj);
};


/**
 * A private symbol
 *
 * This is an experimental feature. Use at your own risk.
 */
class V8_EXPORT Private : public Data {
 public:
  /**
   * Returns the print name string of the private symbol, or undefined if none.
   */
  Local<Value> Name() const;

  /**
   * Create a private symbol. If name is not empty, it will be the description.
   */
  static Local<Private> New(Isolate* isolate,
                            Local<String> name = Local<String>());

  /**
   * Retrieve a global private symbol. If a symbol with this name has not
   * been retrieved in the same isolate before, it is created.
   * Note that private symbols created this way are never collected, so
   * they should only be used for statically fixed properties.
   * Also, there is only one global name space for the names used as keys.
   * To minimize the potential for clashes, use qualified names as keys,
   * e.g., "Class#property".
   */
  static Local<Private> ForApi(Isolate* isolate, Local<String> name);

  V8_INLINE static Private* Cast(Data* data);

 private:
  Private();

  static void CheckCast(Data* that);
};


/**
 * A JavaScript number value (ECMA-262, 4.3.20)
 */
class V8_EXPORT Number : public Primitive {
 public:
  double Value() const;
  static Local<Number> New(Isolate* isolate, double value);
  V8_INLINE static Number* Cast(v8::Value* obj);
 private:
  Number();
  static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript value representing a signed integer.
 */
class V8_EXPORT Integer : public Number {
 public:
  static Local<Integer> New(Isolate* isolate, int32_t value);
  static Local<Integer> NewFromUnsigned(Isolate* isolate, uint32_t value);
  int64_t Value() const;
  V8_INLINE static Integer* Cast(v8::Value* obj);
 private:
  Integer();
  static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript value representing a 32-bit signed integer.
 */
class V8_EXPORT Int32 : public Integer {
 public:
  int32_t Value() const;
  V8_INLINE static Int32* Cast(v8::Value* obj);

 private:
  Int32();
  static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript value representing a 32-bit unsigned integer.
 */
class V8_EXPORT Uint32 : public Integer {
 public:
  uint32_t Value() const;
  V8_INLINE static Uint32* Cast(v8::Value* obj);

 private:
  Uint32();
  static void CheckCast(v8::Value* obj);
};

/**
 * A JavaScript BigInt value (https://tc39.github.io/proposal-bigint)
 */
class V8_EXPORT BigInt : public Primitive {
 public:
  static Local<BigInt> New(Isolate* isolate, int64_t value);
  static Local<BigInt> NewFromUnsigned(Isolate* isolate, uint64_t value);
  /**
   * Creates a new BigInt object using a specified sign bit and a
   * specified list of digits/words.
   * The resulting number is calculated as:
   *
   * (-1)^sign_bit * (words[0] * (2^64)^0 + words[1] * (2^64)^1 + ...)
   */
  static MaybeLocal<BigInt> NewFromWords(Local<Context> context, int sign_bit,
                                         int word_count, const uint64_t* words);

  /**
   * Returns the value of this BigInt as an unsigned 64-bit integer.
   * If `lossless` is provided, it will reflect whether the return value was
   * truncated or wrapped around. In particular, it is set to `false` if this
   * BigInt is negative.
   */
  uint64_t Uint64Value(bool* lossless = nullptr) const;

  /**
   * Returns the value of this BigInt as a signed 64-bit integer.
   * If `lossless` is provided, it will reflect whether this BigInt was
   * truncated or not.
   */
  int64_t Int64Value(bool* lossless = nullptr) const;

  /**
   * Returns the number of 64-bit words needed to store the result of
   * ToWordsArray().
   */
  int WordCount() const;

  /**
   * Writes the contents of this BigInt to a specified memory location.
   * `sign_bit` must be provided and will be set to 1 if this BigInt is
   * negative.
   * `*word_count` has to be initialized to the length of the `words` array.
   * Upon return, it will be set to the actual number of words that would
   * be needed to store this BigInt (i.e. the return value of `WordCount()`).
   */
  void ToWordsArray(int* sign_bit, int* word_count, uint64_t* words) const;

  V8_INLINE static BigInt* Cast(v8::Value* obj);

 private:
  BigInt();
  static void CheckCast(v8::Value* obj);
};

/**
 * PropertyAttribute.
 */
enum PropertyAttribute {
  /** None. **/
  None = 0,
  /** ReadOnly, i.e., not writable. **/
  ReadOnly = 1 << 0,
  /** DontEnum, i.e., not enumerable. **/
  DontEnum = 1 << 1,
  /** DontDelete, i.e., not configurable. **/
  DontDelete = 1 << 2
};

/**
 * Accessor[Getter|Setter] are used as callback functions when
 * setting|getting a particular property. See Object and ObjectTemplate's
 * method SetAccessor.
 */
typedef void (*AccessorGetterCallback)(
    Local<String> property,
    const PropertyCallbackInfo<Value>& info);
typedef void (*AccessorNameGetterCallback)(
    Local<Name> property,
    const PropertyCallbackInfo<Value>& info);


typedef void (*AccessorSetterCallback)(
    Local<String> property,
    Local<Value> value,
    const PropertyCallbackInfo<void>& info);
typedef void (*AccessorNameSetterCallback)(
    Local<Name> property,
    Local<Value> value,
    const PropertyCallbackInfo<void>& info);


/**
 * Access control specifications.
 *
 * Some accessors should be accessible across contexts.  These
 * accessors have an explicit access control parameter which specifies
 * the kind of cross-context access that should be allowed.
 *
 * TODO(dcarney): Remove PROHIBITS_OVERWRITING as it is now unused.
 */
enum AccessControl {
  DEFAULT               = 0,
  ALL_CAN_READ          = 1,
  ALL_CAN_WRITE         = 1 << 1,
  PROHIBITS_OVERWRITING = 1 << 2
};

/**
 * Property filter bits. They can be or'ed to build a composite filter.
 */
enum PropertyFilter {
  ALL_PROPERTIES = 0,
  ONLY_WRITABLE = 1,
  ONLY_ENUMERABLE = 2,
  ONLY_CONFIGURABLE = 4,
  SKIP_STRINGS = 8,
  SKIP_SYMBOLS = 16
};

/**
 * Options for marking whether callbacks may trigger JS-observable side effects.
 * Side-effect-free callbacks are whitelisted during debug evaluation with
 * throwOnSideEffect. It applies when calling a Function, FunctionTemplate,
 * or an Accessor callback. For Interceptors, please see
 * PropertyHandlerFlags's kHasNoSideEffect.
 * Callbacks that only cause side effects to the receiver are whitelisted if
 * invoked on receiver objects that are created within the same debug-evaluate
 * call, as these objects are temporary and the side effect does not escape.
 */
enum class SideEffectType {
  kHasSideEffect,
  kHasNoSideEffect,
  kHasSideEffectToReceiver
};

/**
 * Keys/Properties filter enums:
 *
 * KeyCollectionMode limits the range of collected properties. kOwnOnly limits
 * the collected properties to the given Object only. kIncludesPrototypes will
 * include all keys of the objects's prototype chain as well.
 */
enum class KeyCollectionMode { kOwnOnly, kIncludePrototypes };

/**
 * kIncludesIndices allows for integer indices to be collected, while
 * kSkipIndices will exclude integer indices from being collected.
 */
enum class IndexFilter { kIncludeIndices, kSkipIndices };

/**
 * kConvertToString will convert integer indices to strings.
 * kKeepNumbers will return numbers for integer indices.
 */
enum class KeyConversionMode { kConvertToString, kKeepNumbers, kNoNumbers };

/**
 * Integrity level for objects.
 */
enum class IntegrityLevel { kFrozen, kSealed };

/**
 * A JavaScript object (ECMA-262, 4.3.3)
 */
class V8_EXPORT Object : public Value {
 public:
  /**
   * Set only return Just(true) or Empty(), so if it should never fail, use
   * result.Check().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context,
                                        Local<Value> key, Local<Value> value);

  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context, uint32_t index,
                                        Local<Value> value);

  // Implements CreateDataProperty (ECMA-262, 7.3.4).
  //
  // Defines a configurable, writable, enumerable property with the given value
  // on the object unless the property already exists and is not configurable
  // or the object is not extensible.
  //
  // Returns true on success.
  V8_WARN_UNUSED_RESULT Maybe<bool> CreateDataProperty(Local<Context> context,
                                                       Local<Name> key,
                                                       Local<Value> value);
  V8_WARN_UNUSED_RESULT Maybe<bool> CreateDataProperty(Local<Context> context,
                                                       uint32_t index,
                                                       Local<Value> value);

  // Implements DefineOwnProperty.
  //
  // In general, CreateDataProperty will be faster, however, does not allow
  // for specifying attributes.
  //
  // Returns true on success.
  V8_WARN_UNUSED_RESULT Maybe<bool> DefineOwnProperty(
      Local<Context> context, Local<Name> key, Local<Value> value,
      PropertyAttribute attributes = None);

  // Implements Object.DefineProperty(O, P, Attributes), see Ecma-262 19.1.2.4.
  //
  // The defineProperty function is used to add an own property or
  // update the attributes of an existing own property of an object.
  //
  // Both data and accessor descriptors can be used.
  //
  // In general, CreateDataProperty is faster, however, does not allow
  // for specifying attributes or an accessor descriptor.
  //
  // The PropertyDescriptor can change when redefining a property.
  //
  // Returns true on success.
  V8_WARN_UNUSED_RESULT Maybe<bool> DefineProperty(
      Local<Context> context, Local<Name> key,
      PropertyDescriptor& descriptor);  // NOLINT(runtime/references)

  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key);

  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              uint32_t index);

  /**
   * Gets the property attributes of a property which can be None or
   * any combination of ReadOnly, DontEnum and DontDelete. Returns
   * None when the property doesn't exist.
   */
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetPropertyAttributes(
      Local<Context> context, Local<Value> key);

  /**
   * Returns Object.getOwnPropertyDescriptor as per ES2016 section 19.1.2.6.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetOwnPropertyDescriptor(
      Local<Context> context, Local<Name> key);

  /**
   * Object::Has() calls the abstract operation HasProperty(O, P) described
   * in ECMA-262, 7.3.10. Has() returns
   * true, if the object has the property, either own or on the prototype chain.
   * Interceptors, i.e., PropertyQueryCallbacks, are called if present.
   *
   * Has() has the same side effects as JavaScript's `variable in object`.
   * For example, calling Has() on a revoked proxy will throw an exception.
   *
   * \note Has() converts the key to a name, which possibly calls back into
   * JavaScript.
   *
   * See also v8::Object::HasOwnProperty() and
   * v8::Object::HasRealNamedProperty().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context,
                                        Local<Value> key);

  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           Local<Value> key);

  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context, uint32_t index);

  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           uint32_t index);

  /**
   * Note: SideEffectType affects the getter only, not the setter.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetAccessor(
      Local<Context> context, Local<Name> name,
      AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      MaybeLocal<Value> data = MaybeLocal<Value>(),
      AccessControl settings = DEFAULT, PropertyAttribute attribute = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  void SetAccessorProperty(Local<Name> name, Local<Function> getter,
                           Local<Function> setter = Local<Function>(),
                           PropertyAttribute attribute = None,
                           AccessControl settings = DEFAULT);

  /**
   * Sets a native data property like Template::SetNativeDataProperty, but
   * this method sets on this object directly.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetNativeDataProperty(
      Local<Context> context, Local<Name> name,
      AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), PropertyAttribute attributes = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Attempts to create a property with the given name which behaves like a data
   * property, except that the provided getter is invoked (and provided with the
   * data value) to supply its value the first time it is read. After the
   * property is accessed once, it is replaced with an ordinary data property.
   *
   * Analogous to Template::SetLazyDataProperty.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetLazyDataProperty(
      Local<Context> context, Local<Name> name,
      AccessorNameGetterCallback getter, Local<Value> data = Local<Value>(),
      PropertyAttribute attributes = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Functionality for private properties.
   * This is an experimental feature, use at your own risk.
   * Note: Private properties are not inherited. Do not rely on this, since it
   * may change.
   */
  Maybe<bool> HasPrivate(Local<Context> context, Local<Private> key);
  Maybe<bool> SetPrivate(Local<Context> context, Local<Private> key,
                         Local<Value> value);
  Maybe<bool> DeletePrivate(Local<Context> context, Local<Private> key);
  MaybeLocal<Value> GetPrivate(Local<Context> context, Local<Private> key);

  /**
   * Returns an array containing the names of the enumerable properties
   * of this object, including properties from prototype objects.  The
   * array returned by this method contains the same values as would
   * be enumerated by a for-in statement over this object.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetPropertyNames(
      Local<Context> context);
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetPropertyNames(
      Local<Context> context, KeyCollectionMode mode,
      PropertyFilter property_filter, IndexFilter index_filter,
      KeyConversionMode key_conversion = KeyConversionMode::kKeepNumbers);

  /**
   * This function has the same functionality as GetPropertyNames but
   * the returned array doesn't contain the names of properties from
   * prototype objects.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
      Local<Context> context);

  /**
   * Returns an array containing the names of the filtered properties
   * of this object, including properties from prototype objects.  The
   * array returned by this method contains the same values as would
   * be enumerated by a for-in statement over this object.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
      Local<Context> context, PropertyFilter filter,
      KeyConversionMode key_conversion = KeyConversionMode::kKeepNumbers);

  /**
   * Get the prototype object.  This does not skip objects marked to
   * be skipped by __proto__ and it does not consult the security
   * handler.
   */
  Local<Value> GetPrototype();

  /**
   * Set the prototype object.  This does not skip objects marked to
   * be skipped by __proto__ and it does not consult the security
   * handler.
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> SetPrototype(Local<Context> context,
                                                 Local<Value> prototype);

  /**
   * Finds an instance of the given function template in the prototype
   * chain.
   */
  Local<Object> FindInstanceInPrototypeChain(Local<FunctionTemplate> tmpl);

  /**
   * Call builtin Object.prototype.toString on this object.
   * This is different from Value::ToString() that may call
   * user-defined toString function. This one does not.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ObjectProtoToString(
      Local<Context> context);

  /**
   * Returns the name of the function invoked as a constructor for this object.
   */
  Local<String> GetConstructorName();

  /**
   * Sets the integrity level of the object.
   */
  Maybe<bool> SetIntegrityLevel(Local<Context> context, IntegrityLevel level);

  /** Gets the number of internal fields for this Object. */
  int InternalFieldCount();

  /** Same as above, but works for PersistentBase. */
  V8_INLINE static int InternalFieldCount(
      const PersistentBase<Object>& object) {
    return object.val_->InternalFieldCount();
  }

  /** Same as above, but works for TracedReferenceBase. */
  V8_INLINE static int InternalFieldCount(
      const TracedReferenceBase<Object>& object) {
    return object.val_->InternalFieldCount();
  }

  /** Gets the value from an internal field. */
  V8_INLINE Local<Value> GetInternalField(int index);

  /** Sets the value in an internal field. */
  void SetInternalField(int index, Local<Value> value);

  /**
   * Gets a 2-byte-aligned native pointer from an internal field. This field
   * must have been set by SetAlignedPointerInInternalField, everything else
   * leads to undefined behavior.
   */
  V8_INLINE void* GetAlignedPointerFromInternalField(int index);

  /** Same as above, but works for PersistentBase. */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const PersistentBase<Object>& object, int index) {
    return object.val_->GetAlignedPointerFromInternalField(index);
  }

  /** Same as above, but works for TracedGlobal. */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const TracedReferenceBase<Object>& object, int index) {
    return object.val_->GetAlignedPointerFromInternalField(index);
  }

  /**
   * Sets a 2-byte-aligned native pointer in an internal field. To retrieve such
   * a field, GetAlignedPointerFromInternalField must be used, everything else
   * leads to undefined behavior.
   */
  void SetAlignedPointerInInternalField(int index, void* value);
  void SetAlignedPointerInInternalFields(int argc, int indices[],
                                         void* values[]);

  /**
   * HasOwnProperty() is like JavaScript's Object.prototype.hasOwnProperty().
   *
   * See also v8::Object::Has() and v8::Object::HasRealNamedProperty().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   Local<Name> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   uint32_t index);
  /**
   * Use HasRealNamedProperty() if you want to check if an object has an own
   * property without causing side effects, i.e., without calling interceptors.
   *
   * This function is similar to v8::Object::HasOwnProperty(), but it does not
   * call interceptors.
   *
   * \note Consider using non-masking interceptors, i.e., the interceptors are
   * not called if the receiver has the real named property. See
   * `v8::PropertyHandlerFlags::kNonMasking`.
   *
   * See also v8::Object::Has().
   */
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedProperty(Local<Context> context,
                                                         Local<Name> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealIndexedProperty(
      Local<Context> context, uint32_t index);
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedCallbackProperty(
      Local<Context> context, Local<Name> key);

  /**
   * If result.IsEmpty() no real property was located in the prototype chain.
   * This means interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedPropertyInPrototypeChain(
      Local<Context> context, Local<Name> key);

  /**
   * Gets the property attributes of a real property in the prototype chain,
   * which can be None or any combination of ReadOnly, DontEnum and DontDelete.
   * Interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute>
  GetRealNamedPropertyAttributesInPrototypeChain(Local<Context> context,
                                                 Local<Name> key);

  /**
   * If result.IsEmpty() no real property was located on the object or
   * in the prototype chain.
   * This means interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedProperty(
      Local<Context> context, Local<Name> key);

  /**
   * Gets the property attributes of a real property which can be
   * None or any combination of ReadOnly, DontEnum and DontDelete.
   * Interceptors in the prototype chain are not called.
   */
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetRealNamedPropertyAttributes(
      Local<Context> context, Local<Name> key);

  /** Tests for a named lookup interceptor.*/
  bool HasNamedLookupInterceptor();

  /** Tests for an index lookup interceptor.*/
  bool HasIndexedLookupInterceptor();

  /**
   * Returns the identity hash for this object. The current implementation
   * uses a hidden property on the object to store the identity hash.
   *
   * The return value will never be 0. Also, it is not guaranteed to be
   * unique.
   */
  int GetIdentityHash();

  /**
   * Clone this object with a fast but shallow copy.  Values will point
   * to the same values as the original object.
   */
  // TODO(dcarney): take an isolate and optionally bail out?
  Local<Object> Clone();

  /**
   * Returns the context in which the object was created.
   */
  Local<Context> CreationContext();

  /** Same as above, but works for Persistents */
  V8_INLINE static Local<Context> CreationContext(
      const PersistentBase<Object>& object) {
    return object.val_->CreationContext();
  }

  /**
   * Checks whether a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   * When an Object is callable this method returns true.
   */
  bool IsCallable();

  /**
   * True if this object is a constructor.
   */
  bool IsConstructor();

  /**
   * True if this object can carry information relevant to the embedder in its
   * embedder fields, false otherwise. This is generally true for objects
   * constructed through function templates but also holds for other types where
   * V8 automatically adds internal fields at compile time, such as e.g.
   * v8::ArrayBuffer.
   */
  bool IsApiWrapper();

  /**
   * True if this object was created from an object template which was marked
   * as undetectable. See v8::ObjectTemplate::MarkAsUndetectable for more
   * information.
   */
  bool IsUndetectable();

  /**
   * Call an Object as a function if a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsFunction(Local<Context> context,
                                                         Local<Value> recv,
                                                         int argc,
                                                         Local<Value> argv[]);

  /**
   * Call an Object as a constructor if a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   * Note: This method behaves like the Function::NewInstance method.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsConstructor(
      Local<Context> context, int argc, Local<Value> argv[]);

  /**
   * Return the isolate to which the Object belongs to.
   */
  Isolate* GetIsolate();

  /**
   * If this object is a Set, Map, WeakSet or WeakMap, this returns a
   * representation of the elements of this object as an array.
   * If this object is a SetIterator or MapIterator, this returns all
   * elements of the underlying collection, starting at the iterator's current
   * position.
   * For other types, this will return an empty MaybeLocal<Array> (without
   * scheduling an exception).
   */
  MaybeLocal<Array> PreviewEntries(bool* is_key_value);

  static Local<Object> New(Isolate* isolate);

  /**
   * Creates a JavaScript object with the given properties, and
   * a the given prototype_or_null (which can be any JavaScript
   * value, and if it's null, the newly created object won't have
   * a prototype at all). This is similar to Object.create().
   * All properties will be created as enumerable, configurable
   * and writable properties.
   */
  static Local<Object> New(Isolate* isolate, Local<Value> prototype_or_null,
                           Local<Name>* names, Local<Value>* values,
                           size_t length);

  V8_INLINE static Object* Cast(Value* obj);

 private:
  Object();
  static void CheckCast(Value* obj);
  Local<Value> SlowGetInternalField(int index);
  void* SlowGetAlignedPointerFromInternalField(int index);
};


/**
 * An instance of the built-in array constructor (ECMA-262, 15.4.2).
 */
class V8_EXPORT Array : public Object {
 public:
  uint32_t Length() const;

  /**
   * Creates a JavaScript array with the given length. If the length
   * is negative the returned array will have length 0.
   */
  static Local<Array> New(Isolate* isolate, int length = 0);

  /**
   * Creates a JavaScript array out of a Local<Value> array in C++
   * with a known length.
   */
  static Local<Array> New(Isolate* isolate, Local<Value>* elements,
                          size_t length);
  V8_INLINE static Array* Cast(Value* obj);
 private:
  Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of the built-in Map constructor (ECMA-262, 6th Edition, 23.1.1).
 */
class V8_EXPORT Map : public Object {
 public:
  size_t Size() const;
  void Clear();
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key);
  V8_WARN_UNUSED_RESULT MaybeLocal<Map> Set(Local<Context> context,
                                            Local<Value> key,
                                            Local<Value> value);
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context,
                                        Local<Value> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           Local<Value> key);

  /**
   * Returns an array of length Size() * 2, where index N is the Nth key and
   * index N + 1 is the Nth value.
   */
  Local<Array> AsArray() const;

  /**
   * Creates a new empty Map.
   */
  static Local<Map> New(Isolate* isolate);

  V8_INLINE static Map* Cast(Value* obj);

 private:
  Map();
  static void CheckCast(Value* obj);
};


/**
 * An instance of the built-in Set constructor (ECMA-262, 6th Edition, 23.2.1).
 */
class V8_EXPORT Set : public Object {
 public:
  size_t Size() const;
  void Clear();
  V8_WARN_UNUSED_RESULT MaybeLocal<Set> Add(Local<Context> context,
                                            Local<Value> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context,
                                        Local<Value> key);
  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           Local<Value> key);

  /**
   * Returns an array of the keys in this Set.
   */
  Local<Array> AsArray() const;

  /**
   * Creates a new empty Set.
   */
  static Local<Set> New(Isolate* isolate);

  V8_INLINE static Set* Cast(Value* obj);

 private:
  Set();
  static void CheckCast(Value* obj);
};


template<typename T>
class ReturnValue {
 public:
  template <class S> V8_INLINE ReturnValue(const ReturnValue<S>& that)
      : value_(that.value_) {
    static_assert(std::is_base_of<T, S>::value, "type check");
  }
  // Local setters
  template <typename S>
  V8_INLINE void Set(const Global<S>& handle);
  template <typename S>
  V8_INLINE void Set(const TracedReferenceBase<S>& handle);
  template <typename S>
  V8_INLINE void Set(const Local<S> handle);
  // Fast primitive setters
  V8_INLINE void Set(bool value);
  V8_INLINE void Set(double i);
  V8_INLINE void Set(int32_t i);
  V8_INLINE void Set(uint32_t i);
  // Fast JS primitive setters
  V8_INLINE void SetNull();
  V8_INLINE void SetUndefined();
  V8_INLINE void SetEmptyString();
  // Convenience getter for Isolate
  V8_INLINE Isolate* GetIsolate() const;

  // Pointer setter: Uncompilable to prevent inadvertent misuse.
  template <typename S>
  V8_INLINE void Set(S* whatever);

  // Getter. Creates a new Local<> so it comes with a certain performance
  // hit. If the ReturnValue was not yet set, this will return the undefined
  // value.
  V8_INLINE Local<Value> Get() const;

 private:
  template<class F> friend class ReturnValue;
  template<class F> friend class FunctionCallbackInfo;
  template<class F> friend class PropertyCallbackInfo;
  template <class F, class G, class H>
  friend class PersistentValueMapBase;
  V8_INLINE void SetInternal(internal::Address value) { *value_ = value; }
  V8_INLINE internal::Address GetDefaultValue();
  V8_INLINE explicit ReturnValue(internal::Address* slot);
  internal::Address* value_;
};


/**
 * The argument information given to function call callbacks.  This
 * class provides access to information about the context of the call,
 * including the receiver, the number and values of arguments, and
 * the holder of the function.
 */
template<typename T>
class FunctionCallbackInfo {
 public:
  /** The number of available arguments. */
  V8_INLINE int Length() const;
  /**
   * Accessor for the available arguments. Returns `undefined` if the index
   * is out of bounds.
   */
  V8_INLINE Local<Value> operator[](int i) const;
  /** Returns the receiver. This corresponds to the "this" value. */
  V8_INLINE Local<Object> This() const;
  /**
   * If the callback was created without a Signature, this is the same
   * value as This(). If there is a signature, and the signature didn't match
   * This() but one of its hidden prototypes, this will be the respective
   * hidden prototype.
   *
   * Note that this is not the prototype of This() on which the accessor
   * referencing this callback was found (which in V8 internally is often
   * referred to as holder [sic]).
   */
  V8_INLINE Local<Object> Holder() const;
  /** For construct calls, this returns the "new.target" value. */
  V8_INLINE Local<Value> NewTarget() const;
  /** Indicates whether this is a regular call or a construct call. */
  V8_INLINE bool IsConstructCall() const;
  /** The data argument specified when creating the callback. */
  V8_INLINE Local<Value> Data() const;
  /** The current Isolate. */
  V8_INLINE Isolate* GetIsolate() const;
  /** The ReturnValue for the call. */
  V8_INLINE ReturnValue<T> GetReturnValue() const;
  // This shouldn't be public, but the arm compiler needs it.
  static const int kArgsLength = 6;

 protected:
  friend class internal::FunctionCallbackArguments;
  friend class internal::CustomArguments<FunctionCallbackInfo>;
  friend class debug::ConsoleCallArguments;
  static const int kHolderIndex = 0;
  static const int kIsolateIndex = 1;
  static const int kReturnValueDefaultValueIndex = 2;
  static const int kReturnValueIndex = 3;
  static const int kDataIndex = 4;
  static const int kNewTargetIndex = 5;

  V8_INLINE FunctionCallbackInfo(internal::Address* implicit_args,
                                 internal::Address* values, int length);
  internal::Address* implicit_args_;
  internal::Address* values_;
  int length_;
};


/**
 * The information passed to a property callback about the context
 * of the property access.
 */
template<typename T>
class PropertyCallbackInfo {
 public:
  /**
   * \return The isolate of the property access.
   */
  V8_INLINE Isolate* GetIsolate() const;

  /**
   * \return The data set in the configuration, i.e., in
   * `NamedPropertyHandlerConfiguration` or
   * `IndexedPropertyHandlerConfiguration.`
   */
  V8_INLINE Local<Value> Data() const;

  /**
   * \return The receiver. In many cases, this is the object on which the
   * property access was intercepted. When using
   * `Reflect.get`, `Function.prototype.call`, or similar functions, it is the
   * object passed in as receiver or thisArg.
   *
   * \code
   *  void GetterCallback(Local<Name> name,
   *                      const v8::PropertyCallbackInfo<v8::Value>& info) {
   *     auto context = info.GetIsolate()->GetCurrentContext();
   *
   *     v8::Local<v8::Value> a_this =
   *         info.This()
   *             ->GetRealNamedProperty(context, v8_str("a"))
   *             .ToLocalChecked();
   *     v8::Local<v8::Value> a_holder =
   *         info.Holder()
   *             ->GetRealNamedProperty(context, v8_str("a"))
   *             .ToLocalChecked();
   *
   *    CHECK(v8_str("r")->Equals(context, a_this).FromJust());
   *    CHECK(v8_str("obj")->Equals(context, a_holder).FromJust());
   *
   *    info.GetReturnValue().Set(name);
   *  }
   *
   *  v8::Local<v8::FunctionTemplate> templ =
   *  v8::FunctionTemplate::New(isolate);
   *  templ->InstanceTemplate()->SetHandler(
   *      v8::NamedPropertyHandlerConfiguration(GetterCallback));
   *  LocalContext env;
   *  env->Global()
   *      ->Set(env.local(), v8_str("obj"), templ->GetFunction(env.local())
   *                                           .ToLocalChecked()
   *                                           ->NewInstance(env.local())
   *                                           .ToLocalChecked())
   *      .FromJust();
   *
   *  CompileRun("obj.a = 'obj'; var r = {a: 'r'}; Reflect.get(obj, 'x', r)");
   * \endcode
   */
  V8_INLINE Local<Object> This() const;

  /**
   * \return The object in the prototype chain of the receiver that has the
   * interceptor. Suppose you have `x` and its prototype is `y`, and `y`
   * has an interceptor. Then `info.This()` is `x` and `info.Holder()` is `y`.
   * The Holder() could be a hidden object (the global object, rather
   * than the global proxy).
   *
   * \note For security reasons, do not pass the object back into the runtime.
   */
  V8_INLINE Local<Object> Holder() const;

  /**
   * \return The return value of the callback.
   * Can be changed by calling Set().
   * \code
   * info.GetReturnValue().Set(...)
   * \endcode
   *
   */
  V8_INLINE ReturnValue<T> GetReturnValue() const;

  /**
   * \return True if the intercepted function should throw if an error occurs.
   * Usually, `true` corresponds to `'use strict'`.
   *
   * \note Always `false` when intercepting `Reflect.set()`
   * independent of the language mode.
   */
  V8_INLINE bool ShouldThrowOnError() const;

  // This shouldn't be public, but the arm compiler needs it.
  static const int kArgsLength = 7;

 protected:
  friend class MacroAssembler;
  friend class internal::PropertyCallbackArguments;
  friend class internal::CustomArguments<PropertyCallbackInfo>;
  static const int kShouldThrowOnErrorIndex = 0;
  static const int kHolderIndex = 1;
  static const int kIsolateIndex = 2;
  static const int kReturnValueDefaultValueIndex = 3;
  static const int kReturnValueIndex = 4;
  static const int kDataIndex = 5;
  static const int kThisIndex = 6;

  V8_INLINE PropertyCallbackInfo(internal::Address* args) : args_(args) {}
  internal::Address* args_;
};


typedef void (*FunctionCallback)(const FunctionCallbackInfo<Value>& info);

enum class ConstructorBehavior { kThrow, kAllow };

/**
 * A JavaScript function object (ECMA-262, 15.3).
 */
class V8_EXPORT Function : public Object {
 public:
  /**
   * Create a function in the current execution context
   * for a given FunctionCallback.
   */
  static MaybeLocal<Function> New(
      Local<Context> context, FunctionCallback callback,
      Local<Value> data = Local<Value>(), int length = 0,
      ConstructorBehavior behavior = ConstructorBehavior::kAllow,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect);

  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
      Local<Context> context, int argc, Local<Value> argv[]) const;

  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
      Local<Context> context) const {
    return NewInstance(context, 0, nullptr);
  }

  /**
   * When side effect checks are enabled, passing kHasNoSideEffect allows the
   * constructor to be invoked without throwing. Calls made within the
   * constructor are still checked.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstanceWithSideEffectType(
      Local<Context> context, int argc, Local<Value> argv[],
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect) const;

  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Call(Local<Context> context,
                                               Local<Value> recv, int argc,
                                               Local<Value> argv[]);

  void SetName(Local<String> name);
  Local<Value> GetName() const;

  /**
   * Name inferred from variable or property assignment of this function.
   * Used to facilitate debugging and profiling of JavaScript code written
   * in an OO style, where many functions are anonymous but are assigned
   * to object properties.
   */
  Local<Value> GetInferredName() const;

  /**
   * displayName if it is set, otherwise name if it is configured, otherwise
   * function name, otherwise inferred name.
   */
  Local<Value> GetDebugName() const;

  /**
   * User-defined name assigned to the "displayName" property of this function.
   * Used to facilitate debugging and profiling of JavaScript code.
   */
  Local<Value> GetDisplayName() const;

  /**
   * Returns zero based line number of function body and
   * kLineOffsetNotFound if no information available.
   */
  int GetScriptLineNumber() const;
  /**
   * Returns zero based column number of function body and
   * kLineOffsetNotFound if no information available.
   */
  int GetScriptColumnNumber() const;

  /**
   * Returns scriptId.
   */
  int ScriptId() const;

  /**
   * Returns the original function if this function is bound, else returns
   * v8::Undefined.
   */
  Local<Value> GetBoundFunction() const;

  ScriptOrigin GetScriptOrigin() const;
  V8_INLINE static Function* Cast(Value* obj);
  static const int kLineOffsetNotFound;

 private:
  Function();
  static void CheckCast(Value* obj);
};

#ifndef V8_PROMISE_INTERNAL_FIELD_COUNT
// The number of required internal fields can be defined by embedder.
#define V8_PROMISE_INTERNAL_FIELD_COUNT 0
#endif

/**
 * An instance of the built-in Promise constructor (ES6 draft).
 */
class V8_EXPORT Promise : public Object {
 public:
  /**
   * State of the promise. Each value corresponds to one of the possible values
   * of the [[PromiseState]] field.
   */
  enum PromiseState { kPending, kFulfilled, kRejected };

  class V8_EXPORT Resolver : public Object {
   public:
    /**
     * Create a new resolver, along with an associated promise in pending state.
     */
    static V8_WARN_UNUSED_RESULT MaybeLocal<Resolver> New(
        Local<Context> context);

    /**
     * Extract the associated promise.
     */
    Local<Promise> GetPromise();

    /**
     * Resolve/reject the associated promise with a given value.
     * Ignored if the promise is no longer pending.
     */
    V8_WARN_UNUSED_RESULT Maybe<bool> Resolve(Local<Context> context,
                                              Local<Value> value);

    V8_WARN_UNUSED_RESULT Maybe<bool> Reject(Local<Context> context,
                                             Local<Value> value);

    V8_INLINE static Resolver* Cast(Value* obj);

   private:
    Resolver();
    static void CheckCast(Value* obj);
  };

  /**
   * Register a resolution/rejection handler with a promise.
   * The handler is given the respective resolution/rejection value as
   * an argument. If the promise is already resolved/rejected, the handler is
   * invoked at the end of turn.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Promise> Catch(Local<Context> context,
                                                  Local<Function> handler);

  V8_WARN_UNUSED_RESULT MaybeLocal<Promise> Then(Local<Context> context,
                                                 Local<Function> handler);

  V8_WARN_UNUSED_RESULT MaybeLocal<Promise> Then(Local<Context> context,
                                                 Local<Function> on_fulfilled,
                                                 Local<Function> on_rejected);

  /**
   * Returns true if the promise has at least one derived promise, and
   * therefore resolve/reject handlers (including default handler).
   */
  bool HasHandler();

  /**
   * Returns the content of the [[PromiseResult]] field. The Promise must not
   * be pending.
   */
  Local<Value> Result();

  /**
   * Returns the value of the [[PromiseState]] field.
   */
  PromiseState State();

  /**
   * Marks this promise as handled to avoid reporting unhandled rejections.
   */
  void MarkAsHandled();

  V8_INLINE static Promise* Cast(Value* obj);

  static const int kEmbedderFieldCount = V8_PROMISE_INTERNAL_FIELD_COUNT;

 private:
  Promise();
  static void CheckCast(Value* obj);
};

/**
 * An instance of a Property Descriptor, see Ecma-262 6.2.4.
 *
 * Properties in a descriptor are present or absent. If you do not set
 * `enumerable`, `configurable`, and `writable`, they are absent. If `value`,
 * `get`, or `set` are absent, but you must specify them in the constructor, use
 * empty handles.
 *
 * Accessors `get` and `set` must be callable or undefined if they are present.
 *
 * \note Only query properties if they are present, i.e., call `x()` only if
 * `has_x()` returns true.
 *
 * \code
 * // var desc = {writable: false}
 * v8::PropertyDescriptor d(Local<Value>()), false);
 * d.value(); // error, value not set
 * if (d.has_writable()) {
 *   d.writable(); // false
 * }
 *
 * // var desc = {value: undefined}
 * v8::PropertyDescriptor d(v8::Undefined(isolate));
 *
 * // var desc = {get: undefined}
 * v8::PropertyDescriptor d(v8::Undefined(isolate), Local<Value>()));
 * \endcode
 */
class V8_EXPORT PropertyDescriptor {
 public:
  // GenericDescriptor
  PropertyDescriptor();

  // DataDescriptor
  explicit PropertyDescriptor(Local<Value> value);

  // DataDescriptor with writable property
  PropertyDescriptor(Local<Value> value, bool writable);

  // AccessorDescriptor
  PropertyDescriptor(Local<Value> get, Local<Value> set);

  ~PropertyDescriptor();

  Local<Value> value() const;
  bool has_value() const;

  Local<Value> get() const;
  bool has_get() const;
  Local<Value> set() const;
  bool has_set() const;

  void set_enumerable(bool enumerable);
  bool enumerable() const;
  bool has_enumerable() const;

  void set_configurable(bool configurable);
  bool configurable() const;
  bool has_configurable() const;

  bool writable() const;
  bool has_writable() const;

  struct PrivateData;
  PrivateData* get_private() const { return private_; }

  PropertyDescriptor(const PropertyDescriptor&) = delete;
  void operator=(const PropertyDescriptor&) = delete;

 private:
  PrivateData* private_;
};

/**
 * An instance of the built-in Proxy constructor (ECMA-262, 6th Edition,
 * 26.2.1).
 */
class V8_EXPORT Proxy : public Object {
 public:
  Local<Value> GetTarget();
  Local<Value> GetHandler();
  bool IsRevoked();
  void Revoke();

  /**
   * Creates a new Proxy for the target object.
   */
  static MaybeLocal<Proxy> New(Local<Context> context,
                               Local<Object> local_target,
                               Local<Object> local_handler);

  V8_INLINE static Proxy* Cast(Value* obj);

 private:
  Proxy();
  static void CheckCast(Value* obj);
};

/**
 * Points to an unowned continous buffer holding a known number of elements.
 *
 * This is similar to std::span (under consideration for C++20), but does not
 * require advanced C++ support. In the (far) future, this may be replaced with
 * or aliased to std::span.
 *
 * To facilitate future migration, this class exposes a subset of the interface
 * implemented by std::span.
 */
template <typename T>
class V8_EXPORT MemorySpan {
 public:
  /** The default constructor creates an empty span. */
  constexpr MemorySpan() = default;

  constexpr MemorySpan(T* data, size_t size) : data_(data), size_(size) {}

  /** Returns a pointer to the beginning of the buffer. */
  constexpr T* data() const { return data_; }
  /** Returns the number of elements that the buffer holds. */
  constexpr size_t size() const { return size_; }

 private:
  T* data_ = nullptr;
  size_t size_ = 0;
};

/**
 * An owned byte buffer with associated size.
 */
struct OwnedBuffer {
  std::unique_ptr<const uint8_t[]> buffer;
  size_t size = 0;
  OwnedBuffer(std::unique_ptr<const uint8_t[]> buffer, size_t size)
      : buffer(std::move(buffer)), size(size) {}
  OwnedBuffer() = default;
};

// Wrapper around a compiled WebAssembly module, which is potentially shared by
// different WasmModuleObjects.
class V8_EXPORT CompiledWasmModule {
 public:
  /**
   * Serialize the compiled module. The serialized data does not include the
   * wire bytes.
   */
  OwnedBuffer Serialize();

  /**
   * Get the (wasm-encoded) wire bytes that were used to compile this module.
   */
  MemorySpan<const uint8_t> GetWireBytesRef();

 private:
  explicit CompiledWasmModule(std::shared_ptr<internal::wasm::NativeModule>);
  friend class Utils;

  const std::shared_ptr<internal::wasm::NativeModule> native_module_;
};

// An instance of WebAssembly.Module.
class V8_EXPORT WasmModuleObject : public Object {
 public:
  WasmModuleObject() = delete;

  /**
   * Efficiently re-create a WasmModuleObject, without recompiling, from
   * a CompiledWasmModule.
   */
  static MaybeLocal<WasmModuleObject> FromCompiledModule(
      Isolate* isolate, const CompiledWasmModule&);

  /**
   * Get the compiled module for this module object. The compiled module can be
   * shared by several module objects.
   */
  CompiledWasmModule GetCompiledModule();

  V8_INLINE static WasmModuleObject* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};

/**
 * The V8 interface for WebAssembly streaming compilation. When streaming
 * compilation is initiated, V8 passes a {WasmStreaming} object to the embedder
 * such that the embedder can pass the input bytes for streaming compilation to
 * V8.
 */
class V8_EXPORT WasmStreaming final {
 public:
  class WasmStreamingImpl;

  /**
   * Client to receive streaming event notifications.
   */
  class Client {
   public:
    virtual ~Client() = default;
    /**
     * Passes the fully compiled module to the client. This can be used to
     * implement code caching.
     */
    virtual void OnModuleCompiled(CompiledWasmModule compiled_module) = 0;
  };

  explicit WasmStreaming(std::unique_ptr<WasmStreamingImpl> impl);

  ~WasmStreaming();

  /**
   * Pass a new chunk of bytes to WebAssembly streaming compilation.
   * The buffer passed into {OnBytesReceived} is owned by the caller.
   */
  void OnBytesReceived(const uint8_t* bytes, size_t size);

  /**
   * {Finish} should be called after all received bytes where passed to
   * {OnBytesReceived} to tell V8 that there will be no more bytes. {Finish}
   * does not have to be called after {Abort} has been called already.
   */
  void Finish();

  /**
   * Abort streaming compilation. If {exception} has a value, then the promise
   * associated with streaming compilation is rejected with that value. If
   * {exception} does not have value, the promise does not get rejected.
   */
  void Abort(MaybeLocal<Value> exception);

  /**
   * Passes previously compiled module bytes. This must be called before
   * {OnBytesReceived}, {Finish}, or {Abort}. Returns true if the module bytes
   * can be used, false otherwise. The buffer passed via {bytes} and {size}
   * is owned by the caller. If {SetCompiledModuleBytes} returns true, the
   * buffer must remain valid until either {Finish} or {Abort} completes.
   */
  bool SetCompiledModuleBytes(const uint8_t* bytes, size_t size);

  /**
   * Sets the client object that will receive streaming event notifications.
   * This must be called before {OnBytesReceived}, {Finish}, or {Abort}.
   */
  void SetClient(std::shared_ptr<Client> client);

  /*
   * Sets the UTF-8 encoded source URL for the {Script} object. This must be
   * called before {Finish}.
   */
  void SetUrl(const char* url, size_t length);

  /**
   * Unpacks a {WasmStreaming} object wrapped in a  {Managed} for the embedder.
   * Since the embedder is on the other side of the API, it cannot unpack the
   * {Managed} itself.
   */
  static std::shared_ptr<WasmStreaming> Unpack(Isolate* isolate,
                                               Local<Value> value);

 private:
  std::unique_ptr<WasmStreamingImpl> impl_;
};

// TODO(mtrofin): when streaming compilation is done, we can rename this
// to simply WasmModuleObjectBuilder
class V8_EXPORT WasmModuleObjectBuilderStreaming final {
 public:
  explicit WasmModuleObjectBuilderStreaming(Isolate* isolate);
  /**
   * The buffer passed into OnBytesReceived is owned by the caller.
   */
  void OnBytesReceived(const uint8_t*, size_t size);
  void Finish();
  /**
   * Abort streaming compilation. If {exception} has a value, then the promise
   * associated with streaming compilation is rejected with that value. If
   * {exception} does not have value, the promise does not get rejected.
   */
  void Abort(MaybeLocal<Value> exception);
  Local<Promise> GetPromise();

  ~WasmModuleObjectBuilderStreaming() = default;

 private:
  WasmModuleObjectBuilderStreaming(const WasmModuleObjectBuilderStreaming&) =
      delete;
  WasmModuleObjectBuilderStreaming(WasmModuleObjectBuilderStreaming&&) =
      default;
  WasmModuleObjectBuilderStreaming& operator=(
      const WasmModuleObjectBuilderStreaming&) = delete;
  WasmModuleObjectBuilderStreaming& operator=(
      WasmModuleObjectBuilderStreaming&&) = default;
  Isolate* isolate_ = nullptr;

#if V8_CC_MSVC
  /**
   * We don't need the static Copy API, so the default
   * NonCopyablePersistentTraits would be sufficient, however,
   * MSVC eagerly instantiates the Copy.
   * We ensure we don't use Copy, however, by compiling with the
   * defaults everywhere else.
   */
  Persistent<Promise, CopyablePersistentTraits<Promise>> promise_;
#else
  Persistent<Promise> promise_;
#endif
  std::shared_ptr<internal::wasm::StreamingDecoder> streaming_decoder_;
};

#ifndef V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT
// The number of required internal fields can be defined by embedder.
#define V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT 2
#endif


enum class ArrayBufferCreationMode { kInternalized, kExternalized };

/**
 * A wrapper around the backing store (i.e. the raw memory) of an array buffer.
 * See a document linked in http://crbug.com/v8/9908 for more information.
 *
 * The allocation and destruction of backing stores is generally managed by
 * V8. Clients should always use standard C++ memory ownership types (i.e.
 * std::unique_ptr and std::shared_ptr) to manage lifetimes of backing stores
 * properly, since V8 internal objects may alias backing stores.
 *
 * This object does not keep the underlying |ArrayBuffer::Allocator| alive by
 * default. Use Isolate::CreateParams::array_buffer_allocator_shared when
 * creating the Isolate to make it hold a reference to the allocator itself.
 */
class V8_EXPORT BackingStore : public v8::internal::BackingStoreBase {
 public:
  ~BackingStore();

  /**
   * Return a pointer to the beginning of the memory block for this backing
   * store. The pointer is only valid as long as this backing store object
   * lives.
   */
  void* Data() const;

  /**
   * The length (in bytes) of this backing store.
   */
  size_t ByteLength() const;

  /**
   * Indicates whether the backing store was created for an ArrayBuffer or
   * a SharedArrayBuffer.
   */
  bool IsShared() const;

  /**
   * Wrapper around ArrayBuffer::Allocator::Reallocate that preserves IsShared.
   * Assumes that the backing_store was allocated by the ArrayBuffer allocator
   * of the given isolate.
   */
  static std::unique_ptr<BackingStore> Reallocate(
      v8::Isolate* isolate, std::unique_ptr<BackingStore> backing_store,
      size_t byte_length);

  /**
   * This callback is used only if the memory block for a BackingStore cannot be
   * allocated with an ArrayBuffer::Allocator. In such cases the destructor of
   * the BackingStore invokes the callback to free the memory block.
   */
  using DeleterCallback = void (*)(void* data, size_t length,
                                   void* deleter_data);

  /**
   * If the memory block of a BackingStore is static or is managed manually,
   * then this empty deleter along with nullptr deleter_data can be passed to
   * ArrayBuffer::NewBackingStore to indicate that.
   *
   * The manually managed case should be used with caution and only when it
   * is guaranteed that the memory block freeing happens after detaching its
   * ArrayBuffer.
   */
  static void EmptyDeleter(void* data, size_t length, void* deleter_data);

 private:
  /**
   * See [Shared]ArrayBuffer::GetBackingStore and
   * [Shared]ArrayBuffer::NewBackingStore.
   */
  BackingStore();
};

#if !defined(V8_IMMINENT_DEPRECATION_WARNINGS)
// Use v8::BackingStore::DeleterCallback instead.
using BackingStoreDeleterCallback = void (*)(void* data, size_t length,
                                             void* deleter_data);

#endif

/**
 * An instance of the built-in ArrayBuffer constructor (ES6 draft 15.13.5).
 */
class V8_EXPORT ArrayBuffer : public Object {
 public:
  /**
   * A thread-safe allocator that V8 uses to allocate |ArrayBuffer|'s memory.
   * The allocator is a global V8 setting. It has to be set via
   * Isolate::CreateParams.
   *
   * Memory allocated through this allocator by V8 is accounted for as external
   * memory by V8. Note that V8 keeps track of the memory for all internalized
   * |ArrayBuffer|s. Responsibility for tracking external memory (using
   * Isolate::AdjustAmountOfExternalAllocatedMemory) is handed over to the
   * embedder upon externalization and taken over upon internalization (creating
   * an internalized buffer from an existing buffer).
   *
   * Note that it is unsafe to call back into V8 from any of the allocator
   * functions.
   */
  class V8_EXPORT Allocator { // NOLINT
   public:
    virtual ~Allocator() = default;

    /**
     * Allocate |length| bytes. Return nullptr if allocation is not successful.
     * Memory should be initialized to zeroes.
     */
    virtual void* Allocate(size_t length) = 0;

    /**
     * Allocate |length| bytes. Return nullptr if allocation is not successful.
     * Memory does not have to be initialized.
     */
    virtual void* AllocateUninitialized(size_t length) = 0;

    /**
     * Free the memory block of size |length|, pointed to by |data|.
     * That memory is guaranteed to be previously allocated by |Allocate|.
     */
    virtual void Free(void* data, size_t length) = 0;

    /**
     * Reallocate the memory block of size |old_length| to a memory block of
     * size |new_length| by expanding, contracting, or copying the existing
     * memory block. If |new_length| > |old_length|, then the new part of
     * the memory must be initialized to zeros. Return nullptr if reallocation
     * is not successful.
     *
     * The caller guarantees that the memory block was previously allocated
     * using Allocate or AllocateUninitialized.
     *
     * The default implementation allocates a new block and copies data.
     */
    virtual void* Reallocate(void* data, size_t old_length, size_t new_length);

    /**
     * ArrayBuffer allocation mode. kNormal is a malloc/free style allocation,
     * while kReservation is for larger allocations with the ability to set
     * access permissions.
     */
    enum class AllocationMode { kNormal, kReservation };

    /**
     * malloc/free based convenience allocator.
     *
     * Caller takes ownership, i.e. the returned object needs to be freed using
     * |delete allocator| once it is no longer in use.
     */
    static Allocator* NewDefaultAllocator();
  };

  /**
   * The contents of an |ArrayBuffer|. Externalization of |ArrayBuffer|
   * returns an instance of this class, populated, with a pointer to data
   * and byte length.
   *
   * The Data pointer of ArrayBuffer::Contents must be freed using the provided
   * deleter, which will call ArrayBuffer::Allocator::Free if the buffer
   * was allocated with ArraryBuffer::Allocator::Allocate.
   */
  class V8_EXPORT Contents { // NOLINT
   public:
    using DeleterCallback = void (*)(void* buffer, size_t length, void* info);

    Contents()
        : data_(nullptr),
          byte_length_(0),
          allocation_base_(nullptr),
          allocation_length_(0),
          allocation_mode_(Allocator::AllocationMode::kNormal),
          deleter_(nullptr),
          deleter_data_(nullptr) {}

    void* AllocationBase() const { return allocation_base_; }
    size_t AllocationLength() const { return allocation_length_; }
    Allocator::AllocationMode AllocationMode() const {
      return allocation_mode_;
    }

    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }
    DeleterCallback Deleter() const { return deleter_; }
    void* DeleterData() const { return deleter_data_; }

   private:
    Contents(void* data, size_t byte_length, void* allocation_base,
             size_t allocation_length,
             Allocator::AllocationMode allocation_mode, DeleterCallback deleter,
             void* deleter_data);

    void* data_;
    size_t byte_length_;
    void* allocation_base_;
    size_t allocation_length_;
    Allocator::AllocationMode allocation_mode_;
    DeleterCallback deleter_;
    void* deleter_data_;

    friend class ArrayBuffer;
  };


  /**
   * Data length in bytes.
   */
  size_t ByteLength() const;

  /**
   * Create a new ArrayBuffer. Allocate |byte_length| bytes.
   * Allocated memory will be owned by a created ArrayBuffer and
   * will be deallocated when it is garbage-collected,
   * unless the object is externalized.
   */
  static Local<ArrayBuffer> New(Isolate* isolate, size_t byte_length);

  /**
   * Create a new ArrayBuffer over an existing memory block.
   * The created array buffer is by default immediately in externalized state.
   * In externalized state, the memory block will not be reclaimed when a
   * created ArrayBuffer is garbage-collected.
   * In internalized state, the memory block will be released using
   * |Allocator::Free| once all ArrayBuffers referencing it are collected by
   * the garbage collector.
   */
  V8_DEPRECATE_SOON(
      "Use the version that takes a BackingStore. "
      "See http://crbug.com/v8/9908.")
  static Local<ArrayBuffer> New(
      Isolate* isolate, void* data, size_t byte_length,
      ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  /**
   * Create a new ArrayBuffer with an existing backing store.
   * The created array keeps a reference to the backing store until the array
   * is garbage collected. Note that the IsExternal bit does not affect this
   * reference from the array to the backing store.
   *
   * In future IsExternal bit will be removed. Until then the bit is set as
   * follows. If the backing store does not own the underlying buffer, then
   * the array is created in externalized state. Otherwise, the array is created
   * in internalized state. In the latter case the array can be transitioned
   * to the externalized state using Externalize(backing_store).
   */
  static Local<ArrayBuffer> New(Isolate* isolate,
                                std::shared_ptr<BackingStore> backing_store);

  /**
   * Returns a new standalone BackingStore that is allocated using the array
   * buffer allocator of the isolate. The result can be later passed to
   * ArrayBuffer::New.
   *
   * If the allocator returns nullptr, then the function may cause GCs in the
   * given isolate and re-try the allocation. If GCs do not help, then the
   * function will crash with an out-of-memory error.
   */
  static std::unique_ptr<BackingStore> NewBackingStore(Isolate* isolate,
                                                       size_t byte_length);
  /**
   * Returns a new standalone BackingStore that takes over the ownership of
   * the given buffer. The destructor of the BackingStore invokes the given
   * deleter callback.
   *
   * The result can be later passed to ArrayBuffer::New. The raw pointer
   * to the buffer must not be passed again to any V8 API function.
   */
  static std::unique_ptr<BackingStore> NewBackingStore(
      void* data, size_t byte_length, v8::BackingStore::DeleterCallback deleter,
      void* deleter_data);

  /**
   * Returns true if ArrayBuffer is externalized, that is, does not
   * own its memory block.
   */
  V8_DEPRECATE_SOON(
      "With v8::BackingStore externalized ArrayBuffers are "
      "the same as ordinary ArrayBuffers. See http://crbug.com/v8/9908.")
  bool IsExternal() const;

  /**
   * Returns true if this ArrayBuffer may be detached.
   */
  bool IsDetachable() const;

  /**
   * Detaches this ArrayBuffer and all its views (typed arrays).
   * Detaching sets the byte length of the buffer and all typed arrays to zero,
   * preventing JavaScript from ever accessing underlying backing store.
   * ArrayBuffer should have been externalized and must be detachable.
   */
  void Detach();

  /**
   * Make this ArrayBuffer external. The pointer to underlying memory block
   * and byte length are returned as |Contents| structure. After ArrayBuffer
   * had been externalized, it does no longer own the memory block. The caller
   * should take steps to free memory when it is no longer needed.
   *
   * The Data pointer of ArrayBuffer::Contents must be freed using the provided
   * deleter, which will call ArrayBuffer::Allocator::Free if the buffer
   * was allocated with ArrayBuffer::Allocator::Allocate.
   */
  V8_DEPRECATE_SOON(
      "Use GetBackingStore or Detach. See http://crbug.com/v8/9908.")
  Contents Externalize();

  /**
   * Marks this ArrayBuffer external given a witness that the embedder
   * has fetched the backing store using the new GetBackingStore() function.
   *
   * With the new lifetime management of backing stores there is no need for
   * externalizing, so this function exists only to make the transition easier.
   */
  V8_DEPRECATE_SOON("This will be removed together with IsExternal.")
  void Externalize(const std::shared_ptr<BackingStore>& backing_store);

  /**
   * Get a pointer to the ArrayBuffer's underlying memory block without
   * externalizing it. If the ArrayBuffer is not externalized, this pointer
   * will become invalid as soon as the ArrayBuffer gets garbage collected.
   *
   * The embedder should make sure to hold a strong reference to the
   * ArrayBuffer while accessing this pointer.
   */
  V8_DEPRECATE_SOON("Use GetBackingStore. See http://crbug.com/v8/9908.")
  Contents GetContents();

  /**
   * Get a shared pointer to the backing store of this array buffer. This
   * pointer coordinates the lifetime management of the internal storage
   * with any live ArrayBuffers on the heap, even across isolates. The embedder
   * should not attempt to manage lifetime of the storage through other means.
   *
   * This function replaces both Externalize() and GetContents().
   */
  std::shared_ptr<BackingStore> GetBackingStore();

  V8_INLINE static ArrayBuffer* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;
  static const int kEmbedderFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

 private:
  ArrayBuffer();
  static void CheckCast(Value* obj);
  Contents GetContents(bool externalize);
};


#ifndef V8_ARRAY_BUFFER_VIEW_INTERNAL_FIELD_COUNT
// The number of required internal fields can be defined by embedder.
#define V8_ARRAY_BUFFER_VIEW_INTERNAL_FIELD_COUNT 2
#endif


/**
 * A base class for an instance of one of "views" over ArrayBuffer,
 * including TypedArrays and DataView (ES6 draft 15.13).
 */
class V8_EXPORT ArrayBufferView : public Object {
 public:
  /**
   * Returns underlying ArrayBuffer.
   */
  Local<ArrayBuffer> Buffer();
  /**
   * Byte offset in |Buffer|.
   */
  size_t ByteOffset();
  /**
   * Size of a view in bytes.
   */
  size_t ByteLength();

  /**
   * Copy the contents of the ArrayBufferView's buffer to an embedder defined
   * memory without additional overhead that calling ArrayBufferView::Buffer
   * might incur.
   *
   * Will write at most min(|byte_length|, ByteLength) bytes starting at
   * ByteOffset of the underlying buffer to the memory starting at |dest|.
   * Returns the number of bytes actually written.
   */
  size_t CopyContents(void* dest, size_t byte_length);

  /**
   * Returns true if ArrayBufferView's backing ArrayBuffer has already been
   * allocated.
   */
  bool HasBuffer() const;

  V8_INLINE static ArrayBufferView* Cast(Value* obj);

  static const int kInternalFieldCount =
      V8_ARRAY_BUFFER_VIEW_INTERNAL_FIELD_COUNT;
  static const int kEmbedderFieldCount =
      V8_ARRAY_BUFFER_VIEW_INTERNAL_FIELD_COUNT;

 private:
  ArrayBufferView();
  static void CheckCast(Value* obj);
};


/**
 * A base class for an instance of TypedArray series of constructors
 * (ES6 draft 15.13.6).
 */
class V8_EXPORT TypedArray : public ArrayBufferView {
 public:
  /*
   * The largest typed array size that can be constructed using New.
   */
  static constexpr size_t kMaxLength = internal::kApiSystemPointerSize == 4
                                           ? internal::kSmiMaxValue
                                           : 0xFFFFFFFF;

  /**
   * Number of elements in this typed array
   * (e.g. for Int16Array, |ByteLength|/2).
   */
  size_t Length();

  V8_INLINE static TypedArray* Cast(Value* obj);

 private:
  TypedArray();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Uint8Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint8Array : public TypedArray {
 public:
  static Local<Uint8Array> New(Local<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Local<Uint8Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                               size_t byte_offset, size_t length);
  V8_INLINE static Uint8Array* Cast(Value* obj);

 private:
  Uint8Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Uint8ClampedArray constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint8ClampedArray : public TypedArray {
 public:
  static Local<Uint8ClampedArray> New(Local<ArrayBuffer> array_buffer,
                                      size_t byte_offset, size_t length);
  static Local<Uint8ClampedArray> New(
      Local<SharedArrayBuffer> shared_array_buffer, size_t byte_offset,
      size_t length);
  V8_INLINE static Uint8ClampedArray* Cast(Value* obj);

 private:
  Uint8ClampedArray();
  static void CheckCast(Value* obj);
};

/**
 * An instance of Int8Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Int8Array : public TypedArray {
 public:
  static Local<Int8Array> New(Local<ArrayBuffer> array_buffer,
                              size_t byte_offset, size_t length);
  static Local<Int8Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                              size_t byte_offset, size_t length);
  V8_INLINE static Int8Array* Cast(Value* obj);

 private:
  Int8Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Uint16Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint16Array : public TypedArray {
 public:
  static Local<Uint16Array> New(Local<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Local<Uint16Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                size_t byte_offset, size_t length);
  V8_INLINE static Uint16Array* Cast(Value* obj);

 private:
  Uint16Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Int16Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Int16Array : public TypedArray {
 public:
  static Local<Int16Array> New(Local<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Local<Int16Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                               size_t byte_offset, size_t length);
  V8_INLINE static Int16Array* Cast(Value* obj);

 private:
  Int16Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Uint32Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Uint32Array : public TypedArray {
 public:
  static Local<Uint32Array> New(Local<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Local<Uint32Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                size_t byte_offset, size_t length);
  V8_INLINE static Uint32Array* Cast(Value* obj);

 private:
  Uint32Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Int32Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Int32Array : public TypedArray {
 public:
  static Local<Int32Array> New(Local<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Local<Int32Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                               size_t byte_offset, size_t length);
  V8_INLINE static Int32Array* Cast(Value* obj);

 private:
  Int32Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Float32Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Float32Array : public TypedArray {
 public:
  static Local<Float32Array> New(Local<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Local<Float32Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                 size_t byte_offset, size_t length);
  V8_INLINE static Float32Array* Cast(Value* obj);

 private:
  Float32Array();
  static void CheckCast(Value* obj);
};


/**
 * An instance of Float64Array constructor (ES6 draft 15.13.6).
 */
class V8_EXPORT Float64Array : public TypedArray {
 public:
  static Local<Float64Array> New(Local<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Local<Float64Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                 size_t byte_offset, size_t length);
  V8_INLINE static Float64Array* Cast(Value* obj);

 private:
  Float64Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of BigInt64Array constructor.
 */
class V8_EXPORT BigInt64Array : public TypedArray {
 public:
  static Local<BigInt64Array> New(Local<ArrayBuffer> array_buffer,
                                  size_t byte_offset, size_t length);
  static Local<BigInt64Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                  size_t byte_offset, size_t length);
  V8_INLINE static BigInt64Array* Cast(Value* obj);

 private:
  BigInt64Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of BigUint64Array constructor.
 */
class V8_EXPORT BigUint64Array : public TypedArray {
 public:
  static Local<BigUint64Array> New(Local<ArrayBuffer> array_buffer,
                                   size_t byte_offset, size_t length);
  static Local<BigUint64Array> New(Local<SharedArrayBuffer> shared_array_buffer,
                                   size_t byte_offset, size_t length);
  V8_INLINE static BigUint64Array* Cast(Value* obj);

 private:
  BigUint64Array();
  static void CheckCast(Value* obj);
};

/**
 * An instance of DataView constructor (ES6 draft 15.13.7).
 */
class V8_EXPORT DataView : public ArrayBufferView {
 public:
  static Local<DataView> New(Local<ArrayBuffer> array_buffer,
                             size_t byte_offset, size_t length);
  static Local<DataView> New(Local<SharedArrayBuffer> shared_array_buffer,
                             size_t byte_offset, size_t length);
  V8_INLINE static DataView* Cast(Value* obj);

 private:
  DataView();
  static void CheckCast(Value* obj);
};


/**
 * An instance of the built-in SharedArrayBuffer constructor.
 */
class V8_EXPORT SharedArrayBuffer : public Object {
 public:
  /**
   * The contents of an |SharedArrayBuffer|. Externalization of
   * |SharedArrayBuffer| returns an instance of this class, populated, with a
   * pointer to data and byte length.
   *
   * The Data pointer of ArrayBuffer::Contents must be freed using the provided
   * deleter, which will call ArrayBuffer::Allocator::Free if the buffer
   * was allocated with ArraryBuffer::Allocator::Allocate.
   */
  class V8_EXPORT Contents {  // NOLINT
   public:
    using Allocator = v8::ArrayBuffer::Allocator;
    using DeleterCallback = void (*)(void* buffer, size_t length, void* info);

    Contents()
        : data_(nullptr),
          byte_length_(0),
          allocation_base_(nullptr),
          allocation_length_(0),
          allocation_mode_(Allocator::AllocationMode::kNormal),
          deleter_(nullptr),
          deleter_data_(nullptr) {}

    void* AllocationBase() const { return allocation_base_; }
    size_t AllocationLength() const { return allocation_length_; }
    Allocator::AllocationMode AllocationMode() const {
      return allocation_mode_;
    }

    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }
    DeleterCallback Deleter() const { return deleter_; }
    void* DeleterData() const { return deleter_data_; }

   private:
    Contents(void* data, size_t byte_length, void* allocation_base,
             size_t allocation_length,
             Allocator::AllocationMode allocation_mode, DeleterCallback deleter,
             void* deleter_data);

    void* data_;
    size_t byte_length_;
    void* allocation_base_;
    size_t allocation_length_;
    Allocator::AllocationMode allocation_mode_;
    DeleterCallback deleter_;
    void* deleter_data_;

    friend class SharedArrayBuffer;
  };

  /**
   * Data length in bytes.
   */
  size_t ByteLength() const;

  /**
   * Create a new SharedArrayBuffer. Allocate |byte_length| bytes.
   * Allocated memory will be owned by a created SharedArrayBuffer and
   * will be deallocated when it is garbage-collected,
   * unless the object is externalized.
   */
  static Local<SharedArrayBuffer> New(Isolate* isolate, size_t byte_length);

  /**
   * Create a new SharedArrayBuffer over an existing memory block.  The created
   * array buffer is immediately in externalized state unless otherwise
   * specified. The memory block will not be reclaimed when a created
   * SharedArrayBuffer is garbage-collected.
   */
  V8_DEPRECATE_SOON(
      "Use the version that takes a BackingStore. "
      "See http://crbug.com/v8/9908.")
  static Local<SharedArrayBuffer> New(
      Isolate* isolate, void* data, size_t byte_length,
      ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  /**
   * Create a new SharedArrayBuffer with an existing backing store.
   * The created array keeps a reference to the backing store until the array
   * is garbage collected. Note that the IsExternal bit does not affect this
   * reference from the array to the backing store.
   *
   * In future IsExternal bit will be removed. Until then the bit is set as
   * follows. If the backing store does not own the underlying buffer, then
   * the array is created in externalized state. Otherwise, the array is created
   * in internalized state. In the latter case the array can be transitioned
   * to the externalized state using Externalize(backing_store).
   */
  static Local<SharedArrayBuffer> New(
      Isolate* isolate, std::shared_ptr<BackingStore> backing_store);

  /**
   * Returns a new standalone BackingStore that is allocated using the array
   * buffer allocator of the isolate. The result can be later passed to
   * SharedArrayBuffer::New.
   *
   * If the allocator returns nullptr, then the function may cause GCs in the
   * given isolate and re-try the allocation. If GCs do not help, then the
   * function will crash with an out-of-memory error.
   */
  static std::unique_ptr<BackingStore> NewBackingStore(Isolate* isolate,
                                                       size_t byte_length);
  /**
   * Returns a new standalone BackingStore that takes over the ownership of
   * the given buffer. The destructor of the BackingStore invokes the given
   * deleter callback.
   *
   * The result can be later passed to SharedArrayBuffer::New. The raw pointer
   * to the buffer must not be passed again to any V8 functions.
   */
  static std::unique_ptr<BackingStore> NewBackingStore(
      void* data, size_t byte_length, v8::BackingStore::DeleterCallback deleter,
      void* deleter_data);

  /**
   * Create a new SharedArrayBuffer over an existing memory block. Propagate
   * flags to indicate whether the underlying buffer can be grown.
   */
  V8_DEPRECATED(
      "Use the version that takes a BackingStore. "
      "See http://crbug.com/v8/9908.")
  static Local<SharedArrayBuffer> New(
      Isolate* isolate, const SharedArrayBuffer::Contents&,
      ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  /**
   * Returns true if SharedArrayBuffer is externalized, that is, does not
   * own its memory block.
   */
  V8_DEPRECATE_SOON(
      "With v8::BackingStore externalized SharedArrayBuffers are the same "
      "as ordinary SharedArrayBuffers. See http://crbug.com/v8/9908.")
  bool IsExternal() const;

  /**
   * Make this SharedArrayBuffer external. The pointer to underlying memory
   * block and byte length are returned as |Contents| structure. After
   * SharedArrayBuffer had been externalized, it does no longer own the memory
   * block. The caller should take steps to free memory when it is no longer
   * needed.
   *
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|
   * by the allocator specified in
   * v8::Isolate::CreateParams::array_buffer_allocator.
   *
   */
  V8_DEPRECATE_SOON(
      "Use GetBackingStore or Detach. See http://crbug.com/v8/9908.")
  Contents Externalize();

  /**
   * Marks this SharedArrayBuffer external given a witness that the embedder
   * has fetched the backing store using the new GetBackingStore() function.
   *
   * With the new lifetime management of backing stores there is no need for
   * externalizing, so this function exists only to make the transition easier.
   */
  V8_DEPRECATE_SOON("This will be removed together with IsExternal.")
  void Externalize(const std::shared_ptr<BackingStore>& backing_store);

  /**
   * Get a pointer to the ArrayBuffer's underlying memory block without
   * externalizing it. If the ArrayBuffer is not externalized, this pointer
   * will become invalid as soon as the ArrayBuffer became garbage collected.
   *
   * The embedder should make sure to hold a strong reference to the
   * ArrayBuffer while accessing this pointer.
   *
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|
   * by the allocator specified in
   * v8::Isolate::CreateParams::array_buffer_allocator.
   */
  V8_DEPRECATE_SOON("Use GetBackingStore. See http://crbug.com/v8/9908.")
  Contents GetContents();

  /**
   * Get a shared pointer to the backing store of this array buffer. This
   * pointer coordinates the lifetime management of the internal storage
   * with any live ArrayBuffers on the heap, even across isolates. The embedder
   * should not attempt to manage lifetime of the storage through other means.
   *
   * This function replaces both Externalize() and GetContents().
   */
  std::shared_ptr<BackingStore> GetBackingStore();

  V8_INLINE static SharedArrayBuffer* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

 private:
  SharedArrayBuffer();
  static void CheckCast(Value* obj);
  Contents GetContents(bool externalize);
};


/**
 * An instance of the built-in Date constructor (ECMA-262, 15.9).
 */
class V8_EXPORT Date : public Object {
 public:
  static V8_WARN_UNUSED_RESULT MaybeLocal<Value> New(Local<Context> context,
                                                     double time);

  /**
   * A specialization of Value::NumberValue that is more efficient
   * because we know the structure of this object.
   */
  double ValueOf() const;

  V8_INLINE static Date* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};


/**
 * A Number object (ECMA-262, 4.3.21).
 */
class V8_EXPORT NumberObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, double value);

  double ValueOf() const;

  V8_INLINE static NumberObject* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};

/**
 * A BigInt object (https://tc39.github.io/proposal-bigint)
 */
class V8_EXPORT BigIntObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, int64_t value);

  Local<BigInt> ValueOf() const;

  V8_INLINE static BigIntObject* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};

/**
 * A Boolean object (ECMA-262, 4.3.15).
 */
class V8_EXPORT BooleanObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, bool value);

  bool ValueOf() const;

  V8_INLINE static BooleanObject* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};


/**
 * A String object (ECMA-262, 4.3.18).
 */
class V8_EXPORT StringObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, Local<String> value);

  Local<String> ValueOf() const;

  V8_INLINE static StringObject* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};


/**
 * A Symbol object (ECMA-262 edition 6).
 */
class V8_EXPORT SymbolObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, Local<Symbol> value);

  Local<Symbol> ValueOf() const;

  V8_INLINE static SymbolObject* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};


/**
 * An instance of the built-in RegExp constructor (ECMA-262, 15.10).
 */
class V8_EXPORT RegExp : public Object {
 public:
  /**
   * Regular expression flag bits. They can be or'ed to enable a set
   * of flags.
   */
  enum Flags {
    kNone = 0,
    kGlobal = 1 << 0,
    kIgnoreCase = 1 << 1,
    kMultiline = 1 << 2,
    kSticky = 1 << 3,
    kUnicode = 1 << 4,
    kDotAll = 1 << 5,
  };

  static constexpr int kFlagCount = 6;

  /**
   * Creates a regular expression from the given pattern string and
   * the flags bit field. May throw a JavaScript exception as
   * described in ECMA-262, 15.10.4.1.
   *
   * For example,
   *   RegExp::New(v8::String::New("foo"),
   *               static_cast<RegExp::Flags>(kGlobal | kMultiline))
   * is equivalent to evaluating "/foo/gm".
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<RegExp> New(Local<Context> context,
                                                      Local<String> pattern,
                                                      Flags flags);

  /**
   * Like New, but additionally specifies a backtrack limit. If the number of
   * backtracks done in one Exec call hits the limit, a match failure is
   * immediately returned.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<RegExp> NewWithBacktrackLimit(
      Local<Context> context, Local<String> pattern, Flags flags,
      uint32_t backtrack_limit);

  /**
   * Executes the current RegExp instance on the given subject string.
   * Equivalent to RegExp.prototype.exec as described in
   *
   *   https://tc39.es/ecma262/#sec-regexp.prototype.exec
   *
   * On success, an Array containing the matched strings is returned. On
   * failure, returns Null.
   *
   * Note: modifies global context state, accessible e.g. through RegExp.input.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> Exec(Local<Context> context,
                                                Local<String> subject);

  /**
   * Returns the value of the source property: a string representing
   * the regular expression.
   */
  Local<String> GetSource() const;

  /**
   * Returns the flags bit field.
   */
  Flags GetFlags() const;

  V8_INLINE static RegExp* Cast(Value* obj);

 private:
  static void CheckCast(Value* obj);
};

/**
 * An instance of the built-in FinalizationRegistry constructor.
 *
 * The C++ name is FinalizationGroup for backwards compatibility. This API is
 * experimental and deprecated.
 */
class V8_EXPORT FinalizationGroup : public Object {
 public:
  /**
   * Runs the cleanup callback of the given FinalizationRegistry.
   *
   * V8 will inform the embedder that there are finalizer callbacks be
   * called through HostCleanupFinalizationGroupCallback.
   *
   * HostCleanupFinalizationGroupCallback should schedule a task to
   * call FinalizationGroup::Cleanup() at some point in the
   * future. It's the embedders responsiblity to make this call at a
   * time which does not interrupt synchronous ECMAScript code
   * execution.
   *
   * If the result is Nothing<bool> then an exception has
   * occurred. Otherwise the result is |true| if the cleanup callback
   * was called successfully. The result is never |false|.
   */
  V8_DEPRECATED(
      "FinalizationGroup cleanup is automatic if "
      "HostCleanupFinalizationGroupCallback is not set")
  static V8_WARN_UNUSED_RESULT Maybe<bool> Cleanup(
      Local<FinalizationGroup> finalization_group);
};

/**
 * A JavaScript value that wraps a C++ void*. This type of value is mainly used
 * to associate C++ data structures with JavaScript objects.
 */
class V8_EXPORT External : public Value {
 public:
  static Local<External> New(Isolate* isolate, void* value);
  V8_INLINE static External* Cast(Value* obj);
  void* Value() const;
 private:
  static void CheckCast(v8::Value* obj);
};

#define V8_INTRINSICS_LIST(F)                      \
  F(ArrayProto_entries, array_entries_iterator)    \
  F(ArrayProto_forEach, array_for_each_iterator)   \
  F(ArrayProto_keys, array_keys_iterator)          \
  F(ArrayProto_values, array_values_iterator)      \
  F(ErrorPrototype, initial_error_prototype)       \
  F(IteratorPrototype, initial_iterator_prototype) \
  F(ObjProto_valueOf, object_value_of_function)

enum Intrinsic {
#define V8_DECL_INTRINSIC(name, iname) k##name,
  V8_INTRINSICS_LIST(V8_DECL_INTRINSIC)
#undef V8_DECL_INTRINSIC
};


// --- Templates ---


/**
 * The superclass of object and function templates.
 */
class V8_EXPORT Template : public Data {
 public:
  /**
   * Adds a property to each instance created by this template.
   *
   * The property must be defined either as a primitive value, or a template.
   */
  void Set(Local<Name> name, Local<Data> value,
           PropertyAttribute attributes = None);
  void SetPrivate(Local<Private> name, Local<Data> value,
                  PropertyAttribute attributes = None);
  V8_INLINE void Set(Isolate* isolate, const char* name, Local<Data> value);

  void SetAccessorProperty(
     Local<Name> name,
     Local<FunctionTemplate> getter = Local<FunctionTemplate>(),
     Local<FunctionTemplate> setter = Local<FunctionTemplate>(),
     PropertyAttribute attribute = None,
     AccessControl settings = DEFAULT);

  /**
   * Whenever the property with the given name is accessed on objects
   * created from this Template the getter and setter callbacks
   * are called instead of getting and setting the property directly
   * on the JavaScript object.
   *
   * \param name The name of the property for which an accessor is added.
   * \param getter The callback to invoke when getting the property.
   * \param setter The callback to invoke when setting the property.
   * \param data A piece of data that will be passed to the getter and setter
   *   callbacks whenever they are invoked.
   * \param settings Access control settings for the accessor. This is a bit
   *   field consisting of one of more of
   *   DEFAULT = 0, ALL_CAN_READ = 1, or ALL_CAN_WRITE = 2.
   *   The default is to not allow cross-context access.
   *   ALL_CAN_READ means that all cross-context reads are allowed.
   *   ALL_CAN_WRITE means that all cross-context writes are allowed.
   *   The combination ALL_CAN_READ | ALL_CAN_WRITE can be used to allow all
   *   cross-context access.
   * \param attribute The attributes of the property for which an accessor
   *   is added.
   * \param signature The signature describes valid receivers for the accessor
   *   and is used to perform implicit instance checks against them. If the
   *   receiver is incompatible (i.e. is not an instance of the constructor as
   *   defined by FunctionTemplate::HasInstance()), an implicit TypeError is
   *   thrown and no callback is invoked.
   */
  void SetNativeDataProperty(
      Local<String> name, AccessorGetterCallback getter,
      AccessorSetterCallback setter = nullptr,
      // TODO(dcarney): gcc can't handle Local below
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>(),
      AccessControl settings = DEFAULT,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);
  void SetNativeDataProperty(
      Local<Name> name, AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      // TODO(dcarney): gcc can't handle Local below
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>(),
      AccessControl settings = DEFAULT,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Like SetNativeDataProperty, but V8 will replace the native data property
   * with a real data property on first access.
   */
  void SetLazyDataProperty(
      Local<Name> name, AccessorNameGetterCallback getter,
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * During template instantiation, sets the value with the intrinsic property
   * from the correct context.
   */
  void SetIntrinsicDataProperty(Local<Name> name, Intrinsic intrinsic,
                                PropertyAttribute attribute = None);

 private:
  Template();

  friend class ObjectTemplate;
  friend class FunctionTemplate;
};

// TODO(dcarney): Replace GenericNamedPropertyFooCallback with just
// NamedPropertyFooCallback.

/**
 * Interceptor for get requests on an object.
 *
 * Use `info.GetReturnValue().Set()` to set the return value of the
 * intercepted get request.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict`' mode.
 * See `PropertyCallbackInfo`.
 *
 * \code
 *  void GetterCallback(
 *    Local<Name> name,
 *    const v8::PropertyCallbackInfo<v8::Value>& info) {
 *      info.GetReturnValue().Set(v8_num(42));
 *  }
 *
 *  v8::Local<v8::FunctionTemplate> templ =
 *      v8::FunctionTemplate::New(isolate);
 *  templ->InstanceTemplate()->SetHandler(
 *      v8::NamedPropertyHandlerConfiguration(GetterCallback));
 *  LocalContext env;
 *  env->Global()
 *      ->Set(env.local(), v8_str("obj"), templ->GetFunction(env.local())
 *                                             .ToLocalChecked()
 *                                             ->NewInstance(env.local())
 *                                             .ToLocalChecked())
 *      .FromJust();
 *  v8::Local<v8::Value> result = CompileRun("obj.a = 17; obj.a");
 *  CHECK(v8_num(42)->Equals(env.local(), result).FromJust());
 * \endcode
 *
 * See also `ObjectTemplate::SetHandler`.
 */
typedef void (*GenericNamedPropertyGetterCallback)(
    Local<Name> property, const PropertyCallbackInfo<Value>& info);

/**
 * Interceptor for set requests on an object.
 *
 * Use `info.GetReturnValue()` to indicate whether the request was intercepted
 * or not. If the setter successfully intercepts the request, i.e., if the
 * request should not be further executed, call
 * `info.GetReturnValue().Set(value)`. If the setter
 * did not intercept the request, i.e., if the request should be handled as
 * if no interceptor is present, do not not call `Set()`.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param value The value which the property will have if the request
 * is not intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * See also
 * `ObjectTemplate::SetHandler.`
 */
typedef void (*GenericNamedPropertySetterCallback)(
    Local<Name> property, Local<Value> value,
    const PropertyCallbackInfo<Value>& info);

/**
 * Intercepts all requests that query the attributes of the
 * property, e.g., getOwnPropertyDescriptor(), propertyIsEnumerable(), and
 * defineProperty().
 *
 * Use `info.GetReturnValue().Set(value)` to set the property attributes. The
 * value is an integer encoding a `v8::PropertyAttribute`.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * \note Some functions query the property attributes internally, even though
 * they do not return the attributes. For example, `hasOwnProperty()` can
 * trigger this interceptor depending on the state of the object.
 *
 * See also
 * `ObjectTemplate::SetHandler.`
 */
typedef void (*GenericNamedPropertyQueryCallback)(
    Local<Name> property, const PropertyCallbackInfo<Integer>& info);

/**
 * Interceptor for delete requests on an object.
 *
 * Use `info.GetReturnValue()` to indicate whether the request was intercepted
 * or not. If the deleter successfully intercepts the request, i.e., if the
 * request should not be further executed, call
 * `info.GetReturnValue().Set(value)` with a boolean `value`. The `value` is
 * used as the return value of `delete`.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * \note If you need to mimic the behavior of `delete`, i.e., throw in strict
 * mode instead of returning false, use `info.ShouldThrowOnError()` to determine
 * if you are in strict mode.
 *
 * See also `ObjectTemplate::SetHandler.`
 */
typedef void (*GenericNamedPropertyDeleterCallback)(
    Local<Name> property, const PropertyCallbackInfo<Boolean>& info);

/**
 * Returns an array containing the names of the properties the named
 * property getter intercepts.
 *
 * Note: The values in the array must be of type v8::Name.
 */
typedef void (*GenericNamedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);

/**
 * Interceptor for defineProperty requests on an object.
 *
 * Use `info.GetReturnValue()` to indicate whether the request was intercepted
 * or not. If the definer successfully intercepts the request, i.e., if the
 * request should not be further executed, call
 * `info.GetReturnValue().Set(value)`. If the definer
 * did not intercept the request, i.e., if the request should be handled as
 * if no interceptor is present, do not not call `Set()`.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \param desc The property descriptor which is used to define the
 * property if the request is not intercepted.
 * \param info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * See also `ObjectTemplate::SetHandler`.
 */
typedef void (*GenericNamedPropertyDefinerCallback)(
    Local<Name> property, const PropertyDescriptor& desc,
    const PropertyCallbackInfo<Value>& info);

/**
 * Interceptor for getOwnPropertyDescriptor requests on an object.
 *
 * Use `info.GetReturnValue().Set()` to set the return value of the
 * intercepted request. The return value must be an object that
 * can be converted to a PropertyDescriptor, e.g., a `v8::value` returned from
 * `v8::Object::getOwnPropertyDescriptor`.
 *
 * \param property The name of the property for which the request was
 * intercepted.
 * \info Information about the intercepted request, such as
 * isolate, receiver, return value, or whether running in `'use strict'` mode.
 * See `PropertyCallbackInfo`.
 *
 * \note If GetOwnPropertyDescriptor is intercepted, it will
 * always return true, i.e., indicate that the property was found.
 *
 * See also `ObjectTemplate::SetHandler`.
 */
typedef void (*GenericNamedPropertyDescriptorCallback)(
    Local<Name> property, const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertyGetterCallback`.
 */
typedef void (*IndexedPropertyGetterCallback)(
    uint32_t index,
    const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertySetterCallback`.
 */
typedef void (*IndexedPropertySetterCallback)(
    uint32_t index,
    Local<Value> value,
    const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertyQueryCallback`.
 */
typedef void (*IndexedPropertyQueryCallback)(
    uint32_t index,
    const PropertyCallbackInfo<Integer>& info);

/**
 * See `v8::GenericNamedPropertyDeleterCallback`.
 */
typedef void (*IndexedPropertyDeleterCallback)(
    uint32_t index,
    const PropertyCallbackInfo<Boolean>& info);

/**
 * Returns an array containing the indices of the properties the indexed
 * property getter intercepts.
 *
 * Note: The values in the array must be uint32_t.
 */
typedef void (*IndexedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);

/**
 * See `v8::GenericNamedPropertyDefinerCallback`.
 */
typedef void (*IndexedPropertyDefinerCallback)(
    uint32_t index, const PropertyDescriptor& desc,
    const PropertyCallbackInfo<Value>& info);

/**
 * See `v8::GenericNamedPropertyDescriptorCallback`.
 */
typedef void (*IndexedPropertyDescriptorCallback)(
    uint32_t index, const PropertyCallbackInfo<Value>& info);

/**
 * Access type specification.
 */
enum AccessType {
  ACCESS_GET,
  ACCESS_SET,
  ACCESS_HAS,
  ACCESS_DELETE,
  ACCESS_KEYS
};


/**
 * Returns true if the given context should be allowed to access the given
 * object.
 */
typedef bool (*AccessCheckCallback)(Local<Context> accessing_context,
                                    Local<Object> accessed_object,
                                    Local<Value> data);

class CFunction;
/**
 * A FunctionTemplate is used to create functions at runtime. There
 * can only be one function created from a FunctionTemplate in a
 * context.  The lifetime of the created function is equal to the
 * lifetime of the context.  So in case the embedder needs to create
 * temporary functions that can be collected using Scripts is
 * preferred.
 *
 * Any modification of a FunctionTemplate after first instantiation will trigger
 * a crash.
 *
 * A FunctionTemplate can have properties, these properties are added to the
 * function object when it is created.
 *
 * A FunctionTemplate has a corresponding instance template which is
 * used to create object instances when the function is used as a
 * constructor. Properties added to the instance template are added to
 * each object instance.
 *
 * A FunctionTemplate can have a prototype template. The prototype template
 * is used to create the prototype object of the function.
 *
 * The following example shows how to use a FunctionTemplate:
 *
 * \code
 *    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(isolate);
 *    t->Set(isolate, "func_property", v8::Number::New(isolate, 1));
 *
 *    v8::Local<v8::Template> proto_t = t->PrototypeTemplate();
 *    proto_t->Set(isolate,
 *                 "proto_method",
 *                 v8::FunctionTemplate::New(isolate, InvokeCallback));
 *    proto_t->Set(isolate, "proto_const", v8::Number::New(isolate, 2));
 *
 *    v8::Local<v8::ObjectTemplate> instance_t = t->InstanceTemplate();
 *    instance_t->SetAccessor(
          String::NewFromUtf8Literal(isolate, "instance_accessor"),
 *        InstanceAccessorCallback);
 *    instance_t->SetHandler(
 *        NamedPropertyHandlerConfiguration(PropertyHandlerCallback));
 *    instance_t->Set(String::NewFromUtf8Literal(isolate, "instance_property"),
 *                    Number::New(isolate, 3));
 *
 *    v8::Local<v8::Function> function = t->GetFunction();
 *    v8::Local<v8::Object> instance = function->NewInstance();
 * \endcode
 *
 * Let's use "function" as the JS variable name of the function object
 * and "instance" for the instance object created above.  The function
 * and the instance will have the following properties:
 *
 * \code
 *   func_property in function == true;
 *   function.func_property == 1;
 *
 *   function.prototype.proto_method() invokes 'InvokeCallback'
 *   function.prototype.proto_const == 2;
 *
 *   instance instanceof function == true;
 *   instance.instance_accessor calls 'InstanceAccessorCallback'
 *   instance.instance_property == 3;
 * \endcode
 *
 * A FunctionTemplate can inherit from another one by calling the
 * FunctionTemplate::Inherit method.  The following graph illustrates
 * the semantics of inheritance:
 *
 * \code
 *   FunctionTemplate Parent  -> Parent() . prototype -> { }
 *     ^                                                  ^
 *     | Inherit(Parent)                                  | .__proto__
 *     |                                                  |
 *   FunctionTemplate Child   -> Child()  . prototype -> { }
 * \endcode
 *
 * A FunctionTemplate 'Child' inherits from 'Parent', the prototype
 * object of the Child() function has __proto__ pointing to the
 * Parent() function's prototype object. An instance of the Child
 * function has all properties on Parent's instance templates.
 *
 * Let Parent be the FunctionTemplate initialized in the previous
 * section and create a Child FunctionTemplate by:
 *
 * \code
 *   Local<FunctionTemplate> parent = t;
 *   Local<FunctionTemplate> child = FunctionTemplate::New();
 *   child->Inherit(parent);
 *
 *   Local<Function> child_function = child->GetFunction();
 *   Local<Object> child_instance = child_function->NewInstance();
 * \endcode
 *
 * The Child function and Child instance will have the following
 * properties:
 *
 * \code
 *   child_func.prototype.__proto__ == function.prototype;
 *   child_instance.instance_accessor calls 'InstanceAccessorCallback'
 *   child_instance.instance_property == 3;
 * \endcode
 *
 * The additional 'c_function' parameter refers to a fast API call, which
 * must not trigger GC or JavaScript execution, or call into V8 in other
 * ways. For more information how to define them, see
 * include/v8-fast-api-calls.h. Please note that this feature is still
 * experimental.
 */
class V8_EXPORT FunctionTemplate : public Template {
 public:
  /** Creates a function template.*/
  static Local<FunctionTemplate> New(
      Isolate* isolate, FunctionCallback callback = nullptr,
      Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0,
      ConstructorBehavior behavior = ConstructorBehavior::kAllow,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect,
      const CFunction* c_function = nullptr);

  /**
   * Creates a function template backed/cached by a private property.
   */
  static Local<FunctionTemplate> NewWithCache(
      Isolate* isolate, FunctionCallback callback,
      Local<Private> cache_property, Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect);

  /** Returns the unique function instance in the current execution context.*/
  V8_WARN_UNUSED_RESULT MaybeLocal<Function> GetFunction(
      Local<Context> context);

  /**
   * Similar to Context::NewRemoteContext, this creates an instance that
   * isn't backed by an actual object.
   *
   * The InstanceTemplate of this FunctionTemplate must have access checks with
   * handlers installed.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewRemoteInstance();

  /**
   * Set the call-handler callback for a FunctionTemplate.  This
   * callback is called whenever the function created from this
   * FunctionTemplate is called. The 'c_function' represents a fast
   * API call, see the comment above the class declaration.
   */
  void SetCallHandler(
      FunctionCallback callback, Local<Value> data = Local<Value>(),
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect,
      const CFunction* c_function = nullptr);

  /** Set the predefined length property for the FunctionTemplate. */
  void SetLength(int length);

  /** Get the InstanceTemplate. */
  Local<ObjectTemplate> InstanceTemplate();

  /**
   * Causes the function template to inherit from a parent function template.
   * This means the function's prototype.__proto__ is set to the parent
   * function's prototype.
   **/
  void Inherit(Local<FunctionTemplate> parent);

  /**
   * A PrototypeTemplate is the template used to create the prototype object
   * of the function created by this template.
   */
  Local<ObjectTemplate> PrototypeTemplate();

  /**
   * A PrototypeProviderTemplate is another function template whose prototype
   * property is used for this template. This is mutually exclusive with setting
   * a prototype template indirectly by calling PrototypeTemplate() or using
   * Inherit().
   **/
  void SetPrototypeProviderTemplate(Local<FunctionTemplate> prototype_provider);

  /**
   * Set the class name of the FunctionTemplate.  This is used for
   * printing objects created with the function created from the
   * FunctionTemplate as its constructor.
   */
  void SetClassName(Local<String> name);


  /**
   * When set to true, no access check will be performed on the receiver of a
   * function call.  Currently defaults to true, but this is subject to change.
   */
  void SetAcceptAnyReceiver(bool value);

  /**
   * Sets the ReadOnly flag in the attributes of the 'prototype' property
   * of functions created from this FunctionTemplate to true.
   */
  void ReadOnlyPrototype();

  /**
   * Removes the prototype property from functions created from this
   * FunctionTemplate.
   */
  void RemovePrototype();

  /**
   * Returns true if the given object is an instance of this function
   * template.
   */
  bool HasInstance(Local<Value> object);

  V8_INLINE static FunctionTemplate* Cast(Data* data);

 private:
  FunctionTemplate();

  static void CheckCast(Data* that);
  friend class Context;
  friend class ObjectTemplate;
};

/**
 * Configuration flags for v8::NamedPropertyHandlerConfiguration or
 * v8::IndexedPropertyHandlerConfiguration.
 */
enum class PropertyHandlerFlags {
  /**
   * None.
   */
  kNone = 0,

  /**
   * See ALL_CAN_READ above.
   */
  kAllCanRead = 1,

  /** Will not call into interceptor for properties on the receiver or prototype
   * chain, i.e., only call into interceptor for properties that do not exist.
   * Currently only valid for named interceptors.
   */
  kNonMasking = 1 << 1,

  /**
   * Will not call into interceptor for symbol lookup.  Only meaningful for
   * named interceptors.
   */
  kOnlyInterceptStrings = 1 << 2,

  /**
   * The getter, query, enumerator callbacks do not produce side effects.
   */
  kHasNoSideEffect = 1 << 3,
};

struct NamedPropertyHandlerConfiguration {
  NamedPropertyHandlerConfiguration(
      GenericNamedPropertyGetterCallback getter,
      GenericNamedPropertySetterCallback setter,
      GenericNamedPropertyQueryCallback query,
      GenericNamedPropertyDeleterCallback deleter,
      GenericNamedPropertyEnumeratorCallback enumerator,
      GenericNamedPropertyDefinerCallback definer,
      GenericNamedPropertyDescriptorCallback descriptor,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  NamedPropertyHandlerConfiguration(
      /** Note: getter is required */
      GenericNamedPropertyGetterCallback getter = nullptr,
      GenericNamedPropertySetterCallback setter = nullptr,
      GenericNamedPropertyQueryCallback query = nullptr,
      GenericNamedPropertyDeleterCallback deleter = nullptr,
      GenericNamedPropertyEnumeratorCallback enumerator = nullptr,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(nullptr),
        descriptor(nullptr),
        data(data),
        flags(flags) {}

  NamedPropertyHandlerConfiguration(
      GenericNamedPropertyGetterCallback getter,
      GenericNamedPropertySetterCallback setter,
      GenericNamedPropertyDescriptorCallback descriptor,
      GenericNamedPropertyDeleterCallback deleter,
      GenericNamedPropertyEnumeratorCallback enumerator,
      GenericNamedPropertyDefinerCallback definer,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(nullptr),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  GenericNamedPropertyGetterCallback getter;
  GenericNamedPropertySetterCallback setter;
  GenericNamedPropertyQueryCallback query;
  GenericNamedPropertyDeleterCallback deleter;
  GenericNamedPropertyEnumeratorCallback enumerator;
  GenericNamedPropertyDefinerCallback definer;
  GenericNamedPropertyDescriptorCallback descriptor;
  Local<Value> data;
  PropertyHandlerFlags flags;
};


struct IndexedPropertyHandlerConfiguration {
  IndexedPropertyHandlerConfiguration(
      IndexedPropertyGetterCallback getter,
      IndexedPropertySetterCallback setter, IndexedPropertyQueryCallback query,
      IndexedPropertyDeleterCallback deleter,
      IndexedPropertyEnumeratorCallback enumerator,
      IndexedPropertyDefinerCallback definer,
      IndexedPropertyDescriptorCallback descriptor,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  IndexedPropertyHandlerConfiguration(
      /** Note: getter is required */
      IndexedPropertyGetterCallback getter = nullptr,
      IndexedPropertySetterCallback setter = nullptr,
      IndexedPropertyQueryCallback query = nullptr,
      IndexedPropertyDeleterCallback deleter = nullptr,
      IndexedPropertyEnumeratorCallback enumerator = nullptr,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(nullptr),
        descriptor(nullptr),
        data(data),
        flags(flags) {}

  IndexedPropertyHandlerConfiguration(
      IndexedPropertyGetterCallback getter,
      IndexedPropertySetterCallback setter,
      IndexedPropertyDescriptorCallback descriptor,
      IndexedPropertyDeleterCallback deleter,
      IndexedPropertyEnumeratorCallback enumerator,
      IndexedPropertyDefinerCallback definer,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(nullptr),
        deleter(deleter),
        enumerator(enumerator),
        definer(definer),
        descriptor(descriptor),
        data(data),
        flags(flags) {}

  IndexedPropertyGetterCallback getter;
  IndexedPropertySetterCallback setter;
  IndexedPropertyQueryCallback query;
  IndexedPropertyDeleterCallback deleter;
  IndexedPropertyEnumeratorCallback enumerator;
  IndexedPropertyDefinerCallback definer;
  IndexedPropertyDescriptorCallback descriptor;
  Local<Value> data;
  PropertyHandlerFlags flags;
};


/**
 * An ObjectTemplate is used to create objects at runtime.
 *
 * Properties added to an ObjectTemplate are added to each object
 * created from the ObjectTemplate.
 */
class V8_EXPORT ObjectTemplate : public Template {
 public:
  /** Creates an ObjectTemplate. */
  static Local<ObjectTemplate> New(
      Isolate* isolate,
      Local<FunctionTemplate> constructor = Local<FunctionTemplate>());

  /** Creates a new instance of this template.*/
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(Local<Context> context);

  /**
   * Sets an accessor on the object template.
   *
   * Whenever the property with the given name is accessed on objects
   * created from this ObjectTemplate the getter and setter callbacks
   * are called instead of getting and setting the property directly
   * on the JavaScript object.
   *
   * \param name The name of the property for which an accessor is added.
   * \param getter The callback to invoke when getting the property.
   * \param setter The callback to invoke when setting the property.
   * \param data A piece of data that will be passed to the getter and setter
   *   callbacks whenever they are invoked.
   * \param settings Access control settings for the accessor. This is a bit
   *   field consisting of one of more of
   *   DEFAULT = 0, ALL_CAN_READ = 1, or ALL_CAN_WRITE = 2.
   *   The default is to not allow cross-context access.
   *   ALL_CAN_READ means that all cross-context reads are allowed.
   *   ALL_CAN_WRITE means that all cross-context writes are allowed.
   *   The combination ALL_CAN_READ | ALL_CAN_WRITE can be used to allow all
   *   cross-context access.
   * \param attribute The attributes of the property for which an accessor
   *   is added.
   * \param signature The signature describes valid receivers for the accessor
   *   and is used to perform implicit instance checks against them. If the
   *   receiver is incompatible (i.e. is not an instance of the constructor as
   *   defined by FunctionTemplate::HasInstance()), an implicit TypeError is
   *   thrown and no callback is invoked.
   */
  void SetAccessor(
      Local<String> name, AccessorGetterCallback getter,
      AccessorSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), AccessControl settings = DEFAULT,
      PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>(),
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);
  void SetAccessor(
      Local<Name> name, AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = nullptr,
      Local<Value> data = Local<Value>(), AccessControl settings = DEFAULT,
      PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>(),
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect,
      SideEffectType setter_side_effect_type = SideEffectType::kHasSideEffect);

  /**
   * Sets a named property handler on the object template.
   *
   * Whenever a property whose name is a string or a symbol is accessed on
   * objects created from this object template, the provided callback is
   * invoked instead of accessing the property directly on the JavaScript
   * object.
   *
   * @param configuration The NamedPropertyHandlerConfiguration that defines the
   * callbacks to invoke when accessing a property.
   */
  void SetHandler(const NamedPropertyHandlerConfiguration& configuration);

  /**
   * Sets an indexed property handler on the object template.
   *
   * Whenever an indexed property is accessed on objects created from
   * this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * \param getter The callback to invoke when getting a property.
   * \param setter The callback to invoke when setting a property.
   * \param query The callback to invoke to check if an object has a property.
   * \param deleter The callback to invoke when deleting a property.
   * \param enumerator The callback to invoke to enumerate all the indexed
   *   properties of an object.
   * \param data A piece of data that will be passed to the callbacks
   *   whenever they are invoked.
   */
  // TODO(dcarney): deprecate
  void SetIndexedPropertyHandler(
      IndexedPropertyGetterCallback getter,
      IndexedPropertySetterCallback setter = nullptr,
      IndexedPropertyQueryCallback query = nullptr,
      IndexedPropertyDeleterCallback deleter = nullptr,
      IndexedPropertyEnumeratorCallback enumerator = nullptr,
      Local<Value> data = Local<Value>()) {
    SetHandler(IndexedPropertyHandlerConfiguration(getter, setter, query,
                                                   deleter, enumerator, data));
  }

  /**
   * Sets an indexed property handler on the object template.
   *
   * Whenever an indexed property is accessed on objects created from
   * this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * @param configuration The IndexedPropertyHandlerConfiguration that defines
   * the callbacks to invoke when accessing a property.
   */
  void SetHandler(const IndexedPropertyHandlerConfiguration& configuration);

  /**
   * Sets the callback to be used when calling instances created from
   * this template as a function.  If no callback is set, instances
   * behave like normal JavaScript objects that cannot be called as a
   * function.
   */
  void SetCallAsFunctionHandler(FunctionCallback callback,
                                Local<Value> data = Local<Value>());

  /**
   * Mark object instances of the template as undetectable.
   *
   * In many ways, undetectable objects behave as though they are not
   * there.  They behave like 'undefined' in conditionals and when
   * printed.  However, properties can be accessed and called as on
   * normal objects.
   */
  void MarkAsUndetectable();

  /**
   * Sets access check callback on the object template and enables access
   * checks.
   *
   * When accessing properties on instances of this object template,
   * the access check callback will be called to determine whether or
   * not to allow cross-context access to the properties.
   */
  void SetAccessCheckCallback(AccessCheckCallback callback,
                              Local<Value> data = Local<Value>());

  /**
   * Like SetAccessCheckCallback but invokes an interceptor on failed access
   * checks instead of looking up all-can-read properties. You can only use
   * either this method or SetAccessCheckCallback, but not both at the same
   * time.
   */
  void SetAccessCheckCallbackAndHandler(
      AccessCheckCallback callback,
      const NamedPropertyHandlerConfiguration& named_handler,
      const IndexedPropertyHandlerConfiguration& indexed_handler,
      Local<Value> data = Local<Value>());

  /**
   * Gets the number of internal fields for objects generated from
   * this template.
   */
  int InternalFieldCount();

  /**
   * Sets the number of internal fields for objects generated from
   * this template.
   */
  void SetInternalFieldCount(int value);

  /**
   * Returns true if the object will be an immutable prototype exotic object.
   */
  bool IsImmutableProto();

  /**
   * Makes the ObjectTemplate for an immutable prototype exotic object, with an
   * immutable __proto__.
   */
  void SetImmutableProto();

  V8_INLINE static ObjectTemplate* Cast(Data* data);

 private:
  ObjectTemplate();
  static Local<ObjectTemplate> New(internal::Isolate* isolate,
                                   Local<FunctionTemplate> constructor);
  static void CheckCast(Data* that);
  friend class FunctionTemplate;
};

/**
 * A Signature specifies which receiver is valid for a function.
 *
 * A receiver matches a given signature if the receiver (or any of its
 * hidden prototypes) was created from the signature's FunctionTemplate, or
 * from a FunctionTemplate that inherits directly or indirectly from the
 * signature's FunctionTemplate.
 */
class V8_EXPORT Signature : public Data {
 public:
  static Local<Signature> New(
      Isolate* isolate,
      Local<FunctionTemplate> receiver = Local<FunctionTemplate>());

  V8_INLINE static Signature* Cast(Data* data);

 private:
  Signature();

  static void CheckCast(Data* that);
};


/**
 * An AccessorSignature specifies which receivers are valid parameters
 * to an accessor callback.
 */
class V8_EXPORT AccessorSignature : public Data {
 public:
  static Local<AccessorSignature> New(
      Isolate* isolate,
      Local<FunctionTemplate> receiver = Local<FunctionTemplate>());

  V8_INLINE static AccessorSignature* Cast(Data* data);

 private:
  AccessorSignature();

  static void CheckCast(Data* that);
};


// --- Extensions ---

/**
 * Ignore
 */
class V8_EXPORT Extension {  // NOLINT
 public:
  // Note that the strings passed into this constructor must live as long
  // as the Extension itself.
  Extension(const char* name, const char* source = nullptr, int dep_count = 0,
            const char** deps = nullptr, int source_length = -1);
  virtual ~Extension() { delete source_; }
  virtual Local<FunctionTemplate> GetNativeFunctionTemplate(
      Isolate* isolate, Local<String> name) {
    return Local<FunctionTemplate>();
  }

  const char* name() const { return name_; }
  size_t source_length() const { return source_length_; }
  const String::ExternalOneByteStringResource* source() const {
    return source_;
  }
  int dependency_count() const { return dep_count_; }
  const char** dependencies() const { return deps_; }
  void set_auto_enable(bool value) { auto_enable_ = value; }
  bool auto_enable() { return auto_enable_; }

  // Disallow copying and assigning.
  Extension(const Extension&) = delete;
  void operator=(const Extension&) = delete;

 private:
  const char* name_;
  size_t source_length_;  // expected to initialize before source_
  String::ExternalOneByteStringResource* source_;
  int dep_count_;
  const char** deps_;
  bool auto_enable_;
};

void V8_EXPORT RegisterExtension(std::unique_ptr<Extension>);

// --- Statics ---

V8_INLINE Local<Primitive> Undefined(Isolate* isolate);
V8_INLINE Local<Primitive> Null(Isolate* isolate);
V8_INLINE Local<Boolean> True(Isolate* isolate);
V8_INLINE Local<Boolean> False(Isolate* isolate);

/**
 * A set of constraints that specifies the limits of the runtime's memory use.
 * You must set the heap size before initializing the VM - the size cannot be
 * adjusted after the VM is initialized.
 *
 * If you are using threads then you should hold the V8::Locker lock while
 * setting the stack limit and you must set a non-default stack limit separately
 * for each thread.
 *
 * The arguments for set_max_semi_space_size, set_max_old_space_size,
 * set_max_executable_size, set_code_range_size specify limits in MB.
 *
 * The argument for set_max_semi_space_size_in_kb is in KB.
 */
class V8_EXPORT ResourceConstraints {
 public:
  /**
   * Configures the constraints with reasonable default values based on the
   * provided heap size limit. The heap size includes both the young and
   * the old generation.
   *
   * \param initial_heap_size_in_bytes The initial heap size or zero.
   *    By default V8 starts with a small heap and dynamically grows it to
   *    match the set of live objects. This may lead to ineffective
   *    garbage collections at startup if the live set is large.
   *    Setting the initial heap size avoids such garbage collections.
   *    Note that this does not affect young generation garbage collections.
   *
   * \param maximum_heap_size_in_bytes The hard limit for the heap size.
   *    When the heap size approaches this limit, V8 will perform series of
   *    garbage collections and invoke the NearHeapLimitCallback. If the garbage
   *    collections do not help and the callback does not increase the limit,
   *    then V8 will crash with V8::FatalProcessOutOfMemory.
   */
  void ConfigureDefaultsFromHeapSize(size_t initial_heap_size_in_bytes,
                                     size_t maximum_heap_size_in_bytes);

  /**
   * Configures the constraints with reasonable default values based on the
   * capabilities of the current device the VM is running on.
   *
   * \param physical_memory The total amount of physical memory on the current
   *   device, in bytes.
   * \param virtual_memory_limit The amount of virtual memory on the current
   *   device, in bytes, or zero, if there is no limit.
   */
  void ConfigureDefaults(uint64_t physical_memory,
                         uint64_t virtual_memory_limit);

  /**
   * The address beyond which the VM's stack may not grow.
   */
  uint32_t* stack_limit() const { return stack_limit_; }
  void set_stack_limit(uint32_t* value) { stack_limit_ = value; }

  /**
   * The amount of virtual memory reserved for generated code. This is relevant
   * for 64-bit architectures that rely on code range for calls in code.
   */
  size_t code_range_size_in_bytes() const { return code_range_size_; }
  void set_code_range_size_in_bytes(size_t limit) { code_range_size_ = limit; }

  /**
   * The maximum size of the old generation.
   * When the old generation approaches this limit, V8 will perform series of
   * garbage collections and invoke the NearHeapLimitCallback.
   * If the garbage collections do not help and the callback does not
   * increase the limit, then V8 will crash with V8::FatalProcessOutOfMemory.
   */
  size_t max_old_generation_size_in_bytes() const {
    return max_old_generation_size_;
  }
  void set_max_old_generation_size_in_bytes(size_t limit) {
    max_old_generation_size_ = limit;
  }

  /**
   * The maximum size of the young generation, which consists of two semi-spaces
   * and a large object space. This affects frequency of Scavenge garbage
   * collections and should be typically much smaller that the old generation.
   */
  size_t max_young_generation_size_in_bytes() const {
    return max_young_generation_size_;
  }
  void set_max_young_generation_size_in_bytes(size_t limit) {
    max_young_generation_size_ = limit;
  }

  size_t initial_old_generation_size_in_bytes() const {
    return initial_old_generation_size_;
  }
  void set_initial_old_generation_size_in_bytes(size_t initial_size) {
    initial_old_generation_size_ = initial_size;
  }

  size_t initial_young_generation_size_in_bytes() const {
    return initial_young_generation_size_;
  }
  void set_initial_young_generation_size_in_bytes(size_t initial_size) {
    initial_young_generation_size_ = initial_size;
  }

  /**
   * Deprecated functions. Do not use in new code.
   */
  V8_DEPRECATE_SOON("Use code_range_size_in_bytes.")
  size_t code_range_size() const { return code_range_size_ / kMB; }
  V8_DEPRECATE_SOON("Use set_code_range_size_in_bytes.")
  void set_code_range_size(size_t limit_in_mb) {
    code_range_size_ = limit_in_mb * kMB;
  }
  V8_DEPRECATE_SOON("Use max_young_generation_size_in_bytes.")
  size_t max_semi_space_size_in_kb() const;
  V8_DEPRECATE_SOON("Use set_max_young_generation_size_in_bytes.")
  void set_max_semi_space_size_in_kb(size_t limit_in_kb);
  V8_DEPRECATE_SOON("Use max_old_generation_size_in_bytes.")
  size_t max_old_space_size() const { return max_old_generation_size_ / kMB; }
  V8_DEPRECATE_SOON("Use set_max_old_generation_size_in_bytes.")
  void set_max_old_space_size(size_t limit_in_mb) {
    max_old_generation_size_ = limit_in_mb * kMB;
  }
  V8_DEPRECATE_SOON("Zone does not pool memory any more.")
  size_t max_zone_pool_size() const { return max_zone_pool_size_; }
  V8_DEPRECATE_SOON("Zone does not pool memory any more.")
  void set_max_zone_pool_size(size_t bytes) { max_zone_pool_size_ = bytes; }

 private:
  static constexpr size_t kMB = 1048576u;
  size_t code_range_size_ = 0;
  size_t max_old_generation_size_ = 0;
  size_t max_young_generation_size_ = 0;
  size_t max_zone_pool_size_ = 0;
  size_t initial_old_generation_size_ = 0;
  size_t initial_young_generation_size_ = 0;
  uint32_t* stack_limit_ = nullptr;
};


// --- Exceptions ---


typedef void (*FatalErrorCallback)(const char* location, const char* message);

typedef void (*OOMErrorCallback)(const char* location, bool is_heap_oom);

typedef void (*DcheckErrorCallback)(const char* file, int line,
                                    const char* message);

typedef void (*MessageCallback)(Local<Message> message, Local<Value> data);

// --- Tracing ---

typedef void (*LogEventCallback)(const char* name, int event);

/**
 * Create new error objects by calling the corresponding error object
 * constructor with the message.
 */
class V8_EXPORT Exception {
 public:
  static Local<Value> RangeError(Local<String> message);
  static Local<Value> ReferenceError(Local<String> message);
  static Local<Value> SyntaxError(Local<String> message);
  static Local<Value> TypeError(Local<String> message);
  static Local<Value> Error(Local<String> message);

  /**
   * Creates an error message for the given exception.
   * Will try to reconstruct the original stack trace from the exception value,
   * or capture the current stack trace if not available.
   */
  static Local<Message> CreateMessage(Isolate* isolate, Local<Value> exception);

  /**
   * Returns the original stack trace that was captured at the creation time
   * of a given exception, or an empty handle if not available.
   */
  static Local<StackTrace> GetStackTrace(Local<Value> exception);
};


// --- Counters Callbacks ---

typedef int* (*CounterLookupCallback)(const char* name);

typedef void* (*CreateHistogramCallback)(const char* name,
                                         int min,
                                         int max,
                                         size_t buckets);

typedef void (*AddHistogramSampleCallback)(void* histogram, int sample);

// --- Crashkeys Callback ---
enum class CrashKeyId {
  kIsolateAddress,
  kReadonlySpaceFirstPageAddress,
  kMapSpaceFirstPageAddress,
  kCodeSpaceFirstPageAddress,
  kDumpType,
};

typedef void (*AddCrashKeyCallback)(CrashKeyId id, const std::string& value);

// --- Enter/Leave Script Callback ---
typedef void (*BeforeCallEnteredCallback)(Isolate*);
typedef void (*CallCompletedCallback)(Isolate*);

/**
 * HostCleanupFinalizationGroupCallback is called when we require the
 * embedder to enqueue a task that would call
 * FinalizationGroup::Cleanup().
 *
 * The FinalizationGroup is the one for which the embedder needs to
 * call FinalizationGroup::Cleanup() on.
 *
 * The context provided is the one in which the FinalizationGroup was
 * created in.
 */
typedef void (*HostCleanupFinalizationGroupCallback)(
    Local<Context> context, Local<FinalizationGroup> fg);

/**
 * HostImportModuleDynamicallyCallback is called when we require the
 * embedder to load a module. This is used as part of the dynamic
 * import syntax.
 *
 * The referrer contains metadata about the script/module that calls
 * import.
 *
 * The specifier is the name of the module that should be imported.
 *
 * The embedder must compile, instantiate, evaluate the Module, and
 * obtain it's namespace object.
 *
 * The Promise returned from this function is forwarded to userland
 * JavaScript. The embedder must resolve this promise with the module
 * namespace object. In case of an exception, the embedder must reject
 * this promise with the exception. If the promise creation itself
 * fails (e.g. due to stack overflow), the embedder must propagate
 * that exception by returning an empty MaybeLocal.
 */
typedef MaybeLocal<Promise> (*HostImportModuleDynamicallyCallback)(
    Local<Context> context, Local<ScriptOrModule> referrer,
    Local<String> specifier);

/**
 * HostInitializeImportMetaObjectCallback is called the first time import.meta
 * is accessed for a module. Subsequent access will reuse the same value.
 *
 * The method combines two implementation-defined abstract operations into one:
 * HostGetImportMetaProperties and HostFinalizeImportMeta.
 *
 * The embedder should use v8::Object::CreateDataProperty to add properties on
 * the meta object.
 */
typedef void (*HostInitializeImportMetaObjectCallback)(Local<Context> context,
                                                       Local<Module> module,
                                                       Local<Object> meta);

/**
 * PrepareStackTraceCallback is called when the stack property of an error is
 * first accessed. The return value will be used as the stack value. If this
 * callback is registed, the |Error.prepareStackTrace| API will be disabled.
 * |sites| is an array of call sites, specified in
 * https://v8.dev/docs/stack-trace-api
 */
typedef MaybeLocal<Value> (*PrepareStackTraceCallback)(Local<Context> context,
                                                       Local<Value> error,
                                                       Local<Array> sites);

/**
 * PromiseHook with type kInit is called when a new promise is
 * created. When a new promise is created as part of the chain in the
 * case of Promise.then or in the intermediate promises created by
 * Promise.{race, all}/AsyncFunctionAwait, we pass the parent promise
 * otherwise we pass undefined.
 *
 * PromiseHook with type kResolve is called at the beginning of
 * resolve or reject function defined by CreateResolvingFunctions.
 *
 * PromiseHook with type kBefore is called at the beginning of the
 * PromiseReactionJob.
 *
 * PromiseHook with type kAfter is called right at the end of the
 * PromiseReactionJob.
 */
enum class PromiseHookType { kInit, kResolve, kBefore, kAfter };

typedef void (*PromiseHook)(PromiseHookType type, Local<Promise> promise,
                            Local<Value> parent);

// --- Promise Reject Callback ---
enum PromiseRejectEvent {
  kPromiseRejectWithNoHandler = 0,
  kPromiseHandlerAddedAfterReject = 1,
  kPromiseRejectAfterResolved = 2,
  kPromiseResolveAfterResolved = 3,
};

class PromiseRejectMessage {
 public:
  PromiseRejectMessage(Local<Promise> promise, PromiseRejectEvent event,
                       Local<Value> value)
      : promise_(promise), event_(event), value_(value) {}

  V8_INLINE Local<Promise> GetPromise() const { return promise_; }
  V8_INLINE PromiseRejectEvent GetEvent() const { return event_; }
  V8_INLINE Local<Value> GetValue() const { return value_; }

 private:
  Local<Promise> promise_;
  PromiseRejectEvent event_;
  Local<Value> value_;
};

typedef void (*PromiseRejectCallback)(PromiseRejectMessage message);

// --- Microtasks Callbacks ---
typedef void (*MicrotasksCompletedCallback)(Isolate*);
typedef void (*MicrotasksCompletedCallbackWithData)(Isolate*, void*);
typedef void (*MicrotaskCallback)(void* data);

/**
 * Policy for running microtasks:
 *   - explicit: microtasks are invoked with the
 *               Isolate::PerformMicrotaskCheckpoint() method;
 *   - scoped: microtasks invocation is controlled by MicrotasksScope objects;
 *   - auto: microtasks are invoked when the script call depth decrements
 *           to zero.
 */
enum class MicrotasksPolicy { kExplicit, kScoped, kAuto };

/**
 * Represents the microtask queue, where microtasks are stored and processed.
 * https://html.spec.whatwg.org/multipage/webappapis.html#microtask-queue
 * https://html.spec.whatwg.org/multipage/webappapis.html#enqueuejob(queuename,-job,-arguments)
 * https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint
 *
 * A MicrotaskQueue instance may be associated to multiple Contexts by passing
 * it to Context::New(), and they can be detached by Context::DetachGlobal().
 * The embedder must keep the MicrotaskQueue instance alive until all associated
 * Contexts are gone or detached.
 *
 * Use the same instance of MicrotaskQueue for all Contexts that may access each
 * other synchronously. E.g. for Web embedding, use the same instance for all
 * origins that share the same URL scheme and eTLD+1.
 */
class V8_EXPORT MicrotaskQueue {
 public:
  /**
   * Creates an empty MicrotaskQueue instance.
   */
  static std::unique_ptr<MicrotaskQueue> New(
      Isolate* isolate, MicrotasksPolicy policy = MicrotasksPolicy::kAuto);

  virtual ~MicrotaskQueue() = default;

  /**
   * Enqueues the callback to the queue.
   */
  virtual void EnqueueMicrotask(Isolate* isolate,
                                Local<Function> microtask) = 0;

  /**
   * Enqueues the callback to the queue.
   */
  virtual void EnqueueMicrotask(v8::Isolate* isolate,
                                MicrotaskCallback callback,
                                void* data = nullptr) = 0;

  /**
   * Adds a callback to notify the embedder after microtasks were run. The
   * callback is triggered by explicit RunMicrotasks call or automatic
   * microtasks execution (see Isolate::SetMicrotasksPolicy).
   *
   * Callback will trigger even if microtasks were attempted to run,
   * but the microtasks queue was empty and no single microtask was actually
   * executed.
   *
   * Executing scripts inside the callback will not re-trigger microtasks and
   * the callback.
   */
  virtual void AddMicrotasksCompletedCallback(
      MicrotasksCompletedCallbackWithData callback, void* data = nullptr) = 0;

  /**
   * Removes callback that was installed by AddMicrotasksCompletedCallback.
   */
  virtual void RemoveMicrotasksCompletedCallback(
      MicrotasksCompletedCallbackWithData callback, void* data = nullptr) = 0;

  /**
   * Runs microtasks if no microtask is running on this MicrotaskQueue instance.
   */
  virtual void PerformCheckpoint(Isolate* isolate) = 0;

  /**
   * Returns true if a microtask is running on this MicrotaskQueue instance.
   */
  virtual bool IsRunningMicrotasks() const = 0;

  /**
   * Returns the current depth of nested MicrotasksScope that has
   * kRunMicrotasks.
   */
  virtual int GetMicrotasksScopeDepth() const = 0;

  MicrotaskQueue(const MicrotaskQueue&) = delete;
  MicrotaskQueue& operator=(const MicrotaskQueue&) = delete;

 private:
  friend class internal::MicrotaskQueue;
  MicrotaskQueue() = default;
};

/**
 * This scope is used to control microtasks when MicrotasksPolicy::kScoped
 * is used on Isolate. In this mode every non-primitive call to V8 should be
 * done inside some MicrotasksScope.
 * Microtasks are executed when topmost MicrotasksScope marked as kRunMicrotasks
 * exits.
 * kDoNotRunMicrotasks should be used to annotate calls not intended to trigger
 * microtasks.
 */
class V8_EXPORT MicrotasksScope {
 public:
  enum Type { kRunMicrotasks, kDoNotRunMicrotasks };

  MicrotasksScope(Isolate* isolate, Type type);
  MicrotasksScope(Isolate* isolate, MicrotaskQueue* microtask_queue, Type type);
  ~MicrotasksScope();

  /**
   * Runs microtasks if no kRunMicrotasks scope is currently active.
   */
  static void PerformCheckpoint(Isolate* isolate);

  /**
   * Returns current depth of nested kRunMicrotasks scopes.
   */
  static int GetCurrentDepth(Isolate* isolate);

  /**
   * Returns true while microtasks are being executed.
   */
  static bool IsRunningMicrotasks(Isolate* isolate);

  // Prevent copying.
  MicrotasksScope(const MicrotasksScope&) = delete;
  MicrotasksScope& operator=(const MicrotasksScope&) = delete;

 private:
  internal::Isolate* const isolate_;
  internal::MicrotaskQueue* const microtask_queue_;
  bool run_;
};


// --- Failed Access Check Callback ---
typedef void (*FailedAccessCheckCallback)(Local<Object> target,
                                          AccessType type,
                                          Local<Value> data);

// --- AllowCodeGenerationFromStrings callbacks ---

/**
 * Callback to check if code generation from strings is allowed. See
 * Context::AllowCodeGenerationFromStrings.
 */
typedef bool (*AllowCodeGenerationFromStringsCallback)(Local<Context> context,
                                                       Local<String> source);

struct ModifyCodeGenerationFromStringsResult {
  // If true, proceed with the codegen algorithm. Otherwise, block it.
  bool codegen_allowed = false;
  // Overwrite the original source with this string, if present.
  // Use the original source if empty.
  // This field is considered only if codegen_allowed is true.
  MaybeLocal<String> modified_source;
};

/**
 * Callback to check if codegen is allowed from a source object, and convert
 * the source to string if necessary.See  ModifyCodeGenerationFromStrings.
 */
typedef ModifyCodeGenerationFromStringsResult (
    *ModifyCodeGenerationFromStringsCallback)(Local<Context> context,
                                              Local<Value> source);

// --- WebAssembly compilation callbacks ---
typedef bool (*ExtensionCallback)(const FunctionCallbackInfo<Value>&);

typedef bool (*AllowWasmCodeGenerationCallback)(Local<Context> context,
                                                Local<String> source);

// --- Callback for APIs defined on v8-supported objects, but implemented
// by the embedder. Example: WebAssembly.{compile|instantiate}Streaming ---
typedef void (*ApiImplementationCallback)(const FunctionCallbackInfo<Value>&);

// --- Callback for WebAssembly.compileStreaming ---
typedef void (*WasmStreamingCallback)(const FunctionCallbackInfo<Value>&);

// --- Callback for checking if WebAssembly threads are enabled ---
typedef bool (*WasmThreadsEnabledCallback)(Local<Context> context);

// --- Callback for loading source map file for Wasm profiling support
typedef Local<String> (*WasmLoadSourceMapCallback)(Isolate* isolate,
                                                   const char* name);

// --- Garbage Collection Callbacks ---

/**
 * Applications can register callback functions which will be called before and
 * after certain garbage collection operations.  Allocations are not allowed in
 * the callback functions, you therefore cannot manipulate objects (set or
 * delete properties for example) since it is possible such operations will
 * result in the allocation of objects.
 */
enum GCType {
  kGCTypeScavenge = 1 << 0,
  kGCTypeMarkSweepCompact = 1 << 1,
  kGCTypeIncrementalMarking = 1 << 2,
  kGCTypeProcessWeakCallbacks = 1 << 3,
  kGCTypeAll = kGCTypeScavenge | kGCTypeMarkSweepCompact |
               kGCTypeIncrementalMarking | kGCTypeProcessWeakCallbacks
};

/**
 * GCCallbackFlags is used to notify additional information about the GC
 * callback.
 *   - kGCCallbackFlagConstructRetainedObjectInfos: The GC callback is for
 *     constructing retained object infos.
 *   - kGCCallbackFlagForced: The GC callback is for a forced GC for testing.
 *   - kGCCallbackFlagSynchronousPhantomCallbackProcessing: The GC callback
 *     is called synchronously without getting posted to an idle task.
 *   - kGCCallbackFlagCollectAllAvailableGarbage: The GC callback is called
 *     in a phase where V8 is trying to collect all available garbage
 *     (e.g., handling a low memory notification).
 *   - kGCCallbackScheduleIdleGarbageCollection: The GC callback is called to
 *     trigger an idle garbage collection.
 */
enum GCCallbackFlags {
  kNoGCCallbackFlags = 0,
  kGCCallbackFlagConstructRetainedObjectInfos = 1 << 1,
  kGCCallbackFlagForced = 1 << 2,
  kGCCallbackFlagSynchronousPhantomCallbackProcessing = 1 << 3,
  kGCCallbackFlagCollectAllAvailableGarbage = 1 << 4,
  kGCCallbackFlagCollectAllExternalMemory = 1 << 5,
  kGCCallbackScheduleIdleGarbageCollection = 1 << 6,
};

typedef void (*GCCallback)(GCType type, GCCallbackFlags flags);

typedef void (*InterruptCallback)(Isolate* isolate, void* data);

/**
 * This callback is invoked when the heap size is close to the heap limit and
 * V8 is likely to abort with out-of-memory error.
 * The callback can extend the heap limit by returning a value that is greater
 * than the current_heap_limit. The initial heap limit is the limit that was
 * set after heap setup.
 */
typedef size_t (*NearHeapLimitCallback)(void* data, size_t current_heap_limit,
                                        size_t initial_heap_limit);

/**
 * Collection of shared per-process V8 memory information.
 *
 * Instances of this class can be passed to
 * v8::V8::GetSharedMemoryStatistics to get shared memory statistics from V8.
 */
class V8_EXPORT SharedMemoryStatistics {
 public:
  SharedMemoryStatistics();
  size_t read_only_space_size() { return read_only_space_size_; }
  size_t read_only_space_used_size() { return read_only_space_used_size_; }
  size_t read_only_space_physical_size() {
    return read_only_space_physical_size_;
  }

 private:
  size_t read_only_space_size_;
  size_t read_only_space_used_size_;
  size_t read_only_space_physical_size_;

  friend class V8;
};

/**
 * Collection of V8 heap information.
 *
 * Instances of this class can be passed to v8::Isolate::GetHeapStatistics to
 * get heap statistics from V8.
 */
class V8_EXPORT HeapStatistics {
 public:
  HeapStatistics();
  size_t total_heap_size() { return total_heap_size_; }
  size_t total_heap_size_executable() { return total_heap_size_executable_; }
  size_t total_physical_size() { return total_physical_size_; }
  size_t total_available_size() { return total_available_size_; }
  size_t total_global_handles_size() { return total_global_handles_size_; }
  size_t used_global_handles_size() { return used_global_handles_size_; }
  size_t used_heap_size() { return used_heap_size_; }
  size_t heap_size_limit() { return heap_size_limit_; }
  size_t malloced_memory() { return malloced_memory_; }
  size_t external_memory() { return external_memory_; }
  size_t peak_malloced_memory() { return peak_malloced_memory_; }
  size_t number_of_native_contexts() { return number_of_native_contexts_; }
  size_t number_of_detached_contexts() { return number_of_detached_contexts_; }

  /**
   * Returns a 0/1 boolean, which signifies whether the V8 overwrite heap
   * garbage with a bit pattern.
   */
  size_t does_zap_garbage() { return does_zap_garbage_; }

 private:
  size_t total_heap_size_;
  size_t total_heap_size_executable_;
  size_t total_physical_size_;
  size_t total_available_size_;
  size_t used_heap_size_;
  size_t heap_size_limit_;
  size_t malloced_memory_;
  size_t external_memory_;
  size_t peak_malloced_memory_;
  bool does_zap_garbage_;
  size_t number_of_native_contexts_;
  size_t number_of_detached_contexts_;
  size_t total_global_handles_size_;
  size_t used_global_handles_size_;

  friend class V8;
  friend class Isolate;
};


class V8_EXPORT HeapSpaceStatistics {
 public:
  HeapSpaceStatistics();
  const char* space_name() { return space_name_; }
  size_t space_size() { return space_size_; }
  size_t space_used_size() { return space_used_size_; }
  size_t space_available_size() { return space_available_size_; }
  size_t physical_space_size() { return physical_space_size_; }

 private:
  const char* space_name_;
  size_t space_size_;
  size_t space_used_size_;
  size_t space_available_size_;
  size_t physical_space_size_;

  friend class Isolate;
};


class V8_EXPORT HeapObjectStatistics {
 public:
  HeapObjectStatistics();
  const char* object_type() { return object_type_; }
  const char* object_sub_type() { return object_sub_type_; }
  size_t object_count() { return object_count_; }
  size_t object_size() { return object_size_; }

 private:
  const char* object_type_;
  const char* object_sub_type_;
  size_t object_count_;
  size_t object_size_;

  friend class Isolate;
};

class V8_EXPORT HeapCodeStatistics {
 public:
  HeapCodeStatistics();
  size_t code_and_metadata_size() { return code_and_metadata_size_; }
  size_t bytecode_and_metadata_size() { return bytecode_and_metadata_size_; }
  size_t external_script_source_size() { return external_script_source_size_; }

 private:
  size_t code_and_metadata_size_;
  size_t bytecode_and_metadata_size_;
  size_t external_script_source_size_;

  friend class Isolate;
};

/**
 * A JIT code event is issued each time code is added, moved or removed.
 *
 * \note removal events are not currently issued.
 */
struct JitCodeEvent {
  enum EventType {
    CODE_ADDED,
    CODE_MOVED,
    CODE_REMOVED,
    CODE_ADD_LINE_POS_INFO,
    CODE_START_LINE_INFO_RECORDING,
    CODE_END_LINE_INFO_RECORDING
  };
  // Definition of the code position type. The "POSITION" type means the place
  // in the source code which are of interest when making stack traces to
  // pin-point the source location of a stack frame as close as possible.
  // The "STATEMENT_POSITION" means the place at the beginning of each
  // statement, and is used to indicate possible break locations.
  enum PositionType { POSITION, STATEMENT_POSITION };

  // There are two different kinds of JitCodeEvents, one for JIT code generated
  // by the optimizing compiler, and one for byte code generated for the
  // interpreter.  For JIT_CODE events, the |code_start| member of the event
  // points to the beginning of jitted assembly code, while for BYTE_CODE
  // events, |code_start| points to the first bytecode of the interpreted
  // function.
  enum CodeType { BYTE_CODE, JIT_CODE };

  // Type of event.
  EventType type;
  CodeType code_type;
  // Start of the instructions.
  void* code_start;
  // Size of the instructions.
  size_t code_len;
  // Script info for CODE_ADDED event.
  Local<UnboundScript> script;
  // User-defined data for *_LINE_INFO_* event. It's used to hold the source
  // code line information which is returned from the
  // CODE_START_LINE_INFO_RECORDING event. And it's passed to subsequent
  // CODE_ADD_LINE_POS_INFO and CODE_END_LINE_INFO_RECORDING events.
  void* user_data;

  struct name_t {
    // Name of the object associated with the code, note that the string is not
    // zero-terminated.
    const char* str;
    // Number of chars in str.
    size_t len;
  };

  struct line_info_t {
    // PC offset
    size_t offset;
    // Code position
    size_t pos;
    // The position type.
    PositionType position_type;
  };

  struct wasm_source_info_t {
    // Source file name.
    const char* filename;
    // Length of filename.
    size_t filename_size;
    // Line number table, which maps offsets of JITted code to line numbers of
    // source file.
    const line_info_t* line_number_table;
    // Number of entries in the line number table.
    size_t line_number_table_size;
  };

  wasm_source_info_t* wasm_source_info;

  union {
    // Only valid for CODE_ADDED.
    struct name_t name;

    // Only valid for CODE_ADD_LINE_POS_INFO
    struct line_info_t line_info;

    // New location of instructions. Only valid for CODE_MOVED.
    void* new_code_start;
  };

  Isolate* isolate;
};

/**
 * Option flags passed to the SetRAILMode function.
 * See documentation https://developers.google.com/web/tools/chrome-devtools/
 * profile/evaluate-performance/rail
 */
enum RAILMode : unsigned {
  // Response performance mode: In this mode very low virtual machine latency
  // is provided. V8 will try to avoid JavaScript execution interruptions.
  // Throughput may be throttled.
  PERFORMANCE_RESPONSE,
  // Animation performance mode: In this mode low virtual machine latency is
  // provided. V8 will try to avoid as many JavaScript execution interruptions
  // as possible. Throughput may be throttled. This is the default mode.
  PERFORMANCE_ANIMATION,
  // Idle performance mode: The embedder is idle. V8 can complete deferred work
  // in this mode.
  PERFORMANCE_IDLE,
  // Load performance mode: In this mode high throughput is provided. V8 may
  // turn off latency optimizations.
  PERFORMANCE_LOAD
};

/**
 * Option flags passed to the SetJitCodeEventHandler function.
 */
enum JitCodeEventOptions {
  kJitCodeEventDefault = 0,
  // Generate callbacks for already existent code.
  kJitCodeEventEnumExisting = 1
};


/**
 * Callback function passed to SetJitCodeEventHandler.
 *
 * \param event code add, move or removal event.
 */
typedef void (*JitCodeEventHandler)(const JitCodeEvent* event);

/**
 * Callback function passed to SetUnhandledExceptionCallback.
 */
#if defined(V8_OS_WIN)
typedef int (*UnhandledExceptionCallback)(
    _EXCEPTION_POINTERS* exception_pointers);
#endif

/**
 * Interface for iterating through all external resources in the heap.
 */
class V8_EXPORT ExternalResourceVisitor {  // NOLINT
 public:
  virtual ~ExternalResourceVisitor() = default;
  virtual void VisitExternalString(Local<String> string) {}
};


/**
 * Interface for iterating through all the persistent handles in the heap.
 */
class V8_EXPORT PersistentHandleVisitor {  // NOLINT
 public:
  virtual ~PersistentHandleVisitor() = default;
  virtual void VisitPersistentHandle(Persistent<Value>* value,
                                     uint16_t class_id) {}
};

/**
 * Memory pressure level for the MemoryPressureNotification.
 * kNone hints V8 that there is no memory pressure.
 * kModerate hints V8 to speed up incremental garbage collection at the cost of
 * of higher latency due to garbage collection pauses.
 * kCritical hints V8 to free memory as soon as possible. Garbage collection
 * pauses at this level will be large.
 */
enum class MemoryPressureLevel { kNone, kModerate, kCritical };

/**
 * Interface for tracing through the embedder heap. During a V8 garbage
 * collection, V8 collects hidden fields of all potential wrappers, and at the
 * end of its marking phase iterates the collection and asks the embedder to
 * trace through its heap and use reporter to report each JavaScript object
 * reachable from any of the given wrappers.
 */
class V8_EXPORT EmbedderHeapTracer {
 public:
  enum TraceFlags : uint64_t {
    kNoFlags = 0,
    kReduceMemory = 1 << 0,
  };

  // Indicator for the stack state of the embedder.
  enum EmbedderStackState {
    kUnknown,
    kNonEmpty,
    kEmpty,
  };

  /**
   * Interface for iterating through TracedGlobal handles.
   */
  class V8_EXPORT TracedGlobalHandleVisitor {
   public:
    virtual ~TracedGlobalHandleVisitor() = default;
    virtual void VisitTracedGlobalHandle(const TracedGlobal<Value>& handle) {}
    virtual void VisitTracedReference(const TracedReference<Value>& handle) {}
  };

  /**
   * Summary of a garbage collection cycle. See |TraceEpilogue| on how the
   * summary is reported.
   */
  struct TraceSummary {
    /**
     * Time spent managing the retained memory in milliseconds. This can e.g.
     * include the time tracing through objects in the embedder.
     */
    double time = 0.0;

    /**
     * Memory retained by the embedder through the |EmbedderHeapTracer|
     * mechanism in bytes.
     */
    size_t allocated_size = 0;
  };

  virtual ~EmbedderHeapTracer() = default;

  /**
   * Iterates all TracedGlobal handles created for the v8::Isolate the tracer is
   * attached to.
   */
  void IterateTracedGlobalHandles(TracedGlobalHandleVisitor* visitor);

  /**
   * Called by the embedder to set the start of the stack which is e.g. used by
   * V8 to determine whether handles are used from stack or heap.
   */
  void SetStackStart(void* stack_start);

  /**
   * Called by the embedder to notify V8 of an empty execution stack.
   */
  void NotifyEmptyEmbedderStack();

  /**
   * Called by v8 to register internal fields of found wrappers.
   *
   * The embedder is expected to store them somewhere and trace reachable
   * wrappers from them when called through |AdvanceTracing|.
   */
  virtual void RegisterV8References(
      const std::vector<std::pair<void*, void*> >& embedder_fields) = 0;

  void RegisterEmbedderReference(const TracedReferenceBase<v8::Data>& ref);

  /**
   * Called at the beginning of a GC cycle.
   */
  virtual void TracePrologue(TraceFlags flags) {}

  /**
   * Called to advance tracing in the embedder.
   *
   * The embedder is expected to trace its heap starting from wrappers reported
   * by RegisterV8References method, and report back all reachable wrappers.
   * Furthermore, the embedder is expected to stop tracing by the given
   * deadline. A deadline of infinity means that tracing should be finished.
   *
   * Returns |true| if tracing is done, and false otherwise.
   */
  virtual bool AdvanceTracing(double deadline_in_ms) = 0;

  /*
   * Returns true if there no more tracing work to be done (see AdvanceTracing)
   * and false otherwise.
   */
  virtual bool IsTracingDone() = 0;

  /**
   * Called at the end of a GC cycle.
   *
   * Note that allocation is *not* allowed within |TraceEpilogue|. Can be
   * overriden to fill a |TraceSummary| that is used by V8 to schedule future
   * garbage collections.
   */
  virtual void TraceEpilogue(TraceSummary* trace_summary) {}

  /**
   * Called upon entering the final marking pause. No more incremental marking
   * steps will follow this call.
   */
  virtual void EnterFinalPause(EmbedderStackState stack_state) = 0;

  /*
   * Called by the embedder to request immediate finalization of the currently
   * running tracing phase that has been started with TracePrologue and not
   * yet finished with TraceEpilogue.
   *
   * Will be a noop when currently not in tracing.
   *
   * This is an experimental feature.
   */
  void FinalizeTracing();

  /**
   * Returns true if the TracedGlobal handle should be considered as root for
   * the currently running non-tracing garbage collection and false otherwise.
   * The default implementation will keep all TracedGlobal references as roots.
   *
   * If this returns false, then V8 may decide that the object referred to by
   * such a handle is reclaimed. In that case:
   * - No action is required if handles are used with destructors, i.e., by just
   * using |TracedGlobal|.
   * - When run without destructors, i.e., by using
   * |TracedReference|, V8 calls |ResetHandleInNonTracingGC|.
   *
   * Note that the |handle| is different from the handle that the embedder holds
   * for retaining the object. The embedder may use |WrapperClassId()| to
   * distinguish cases where it wants handles to be treated as roots from not
   * being treated as roots.
   */
  virtual bool IsRootForNonTracingGC(
      const v8::TracedReference<v8::Value>& handle);
  virtual bool IsRootForNonTracingGC(const v8::TracedGlobal<v8::Value>& handle);

  /**
   * Used in combination with |IsRootForNonTracingGC|. Called by V8 when an
   * object that is backed by a handle is reclaimed by a non-tracing garbage
   * collection. It is up to the embedder to reset the original handle.
   *
   * Note that the |handle| is different from the handle that the embedder holds
   * for retaining the object. It is up to the embedder to find the original
   * handle via the object or class id.
   */
  virtual void ResetHandleInNonTracingGC(
      const v8::TracedReference<v8::Value>& handle);

  /*
   * Called by the embedder to immediately perform a full garbage collection.
   *
   * Should only be used in testing code.
   */
  void GarbageCollectionForTesting(EmbedderStackState stack_state);

  /*
   * Called by the embedder to signal newly allocated or freed memory. Not bound
   * to tracing phases. Embedders should trade off when increments are reported
   * as V8 may consult global heuristics on whether to trigger garbage
   * collection on this change.
   */
  void IncreaseAllocatedSize(size_t bytes);
  void DecreaseAllocatedSize(size_t bytes);

  /*
   * Returns the v8::Isolate this tracer is attached too and |nullptr| if it
   * is not attached to any v8::Isolate.
   */
  v8::Isolate* isolate() const { return isolate_; }

 protected:
  v8::Isolate* isolate_ = nullptr;

  friend class internal::LocalEmbedderHeapTracer;
};

/**
 * Callback and supporting data used in SnapshotCreator to implement embedder
 * logic to serialize internal fields.
 * Internal fields that directly reference V8 objects are serialized without
 * calling this callback. Internal fields that contain aligned pointers are
 * serialized by this callback if it returns non-zero result. Otherwise it is
 * serialized verbatim.
 */
struct SerializeInternalFieldsCallback {
  typedef StartupData (*CallbackFunction)(Local<Object> holder, int index,
                                          void* data);
  SerializeInternalFieldsCallback(CallbackFunction function = nullptr,
                                  void* data_arg = nullptr)
      : callback(function), data(data_arg) {}
  CallbackFunction callback;
  void* data;
};
// Note that these fields are called "internal fields" in the API and called
// "embedder fields" within V8.
typedef SerializeInternalFieldsCallback SerializeEmbedderFieldsCallback;

/**
 * Callback and supporting data used to implement embedder logic to deserialize
 * internal fields.
 */
struct DeserializeInternalFieldsCallback {
  typedef void (*CallbackFunction)(Local<Object> holder, int index,
                                   StartupData payload, void* data);
  DeserializeInternalFieldsCallback(CallbackFunction function = nullptr,
                                    void* data_arg = nullptr)
      : callback(function), data(data_arg) {}
  void (*callback)(Local<Object> holder, int index, StartupData payload,
                   void* data);
  void* data;
};
typedef DeserializeInternalFieldsCallback DeserializeEmbedderFieldsCallback;

/**
 * Controls how the default MeasureMemoryDelegate reports the result of
 * the memory measurement to JS. With kSummary only the total size is reported.
 * With kDetailed the result includes the size of each native context.
 */
enum class MeasureMemoryMode { kSummary, kDetailed };

/**
 * Controls how promptly a memory measurement request is executed.
 * By default the measurement is folded with the next scheduled GC which may
 * happen after a while. The kEager starts increment GC right away and
 * is useful for testing.
 */
enum class MeasureMemoryExecution { kDefault, kEager };

/**
 * The delegate is used in Isolate::MeasureMemory API.
 *
 * It specifies the contexts that need to be measured and gets called when
 * the measurement is completed to report the results.
 */
class V8_EXPORT MeasureMemoryDelegate {
 public:
  virtual ~MeasureMemoryDelegate() = default;

  /**
   * Returns true if the size of the given context needs to be measured.
   */
  virtual bool ShouldMeasure(Local<Context> context) = 0;

  /**
   * This function is called when memory measurement finishes.
   *
   * \param context_sizes_in_bytes a vector of (context, size) pairs that
   *   includes each context for which ShouldMeasure returned true and that
   *   was not garbage collected while the memory measurement was in progress.
   *
   * \param unattributed_size_in_bytes total size of objects that were not
   *   attributed to any context (i.e. are likely shared objects).
   */
  virtual void MeasurementComplete(
      const std::vector<std::pair<Local<Context>, size_t>>&
          context_sizes_in_bytes,
      size_t unattributed_size_in_bytes) = 0;

  /**
   * Returns a default delegate that resolves the given promise when
   * the memory measurement completes.
   *
   * \param isolate the current isolate
   * \param context the current context
   * \param promise_resolver the promise resolver that is given the
   *   result of the memory measurement.
   * \param mode the detail level of the result.
   */
  static std::unique_ptr<MeasureMemoryDelegate> Default(
      Isolate* isolate, Local<Context> context,
      Local<Promise::Resolver> promise_resolver, MeasureMemoryMode mode);
};

/**
 * Isolate represents an isolated instance of the V8 engine.  V8 isolates have
 * completely separate states.  Objects from one isolate must not be used in
 * other isolates.  The embedder can create multiple isolates and use them in
 * parallel in multiple threads.  An isolate can be entered by at most one
 * thread at any given time.  The Locker/Unlocker API must be used to
 * synchronize.
 */
class V8_EXPORT Isolate {
 public:
  /**
   * Initial configuration parameters for a new Isolate.
   */
  struct CreateParams {
    CreateParams()
        : code_event_handler(nullptr),
          snapshot_blob(nullptr),
          counter_lookup_callback(nullptr),
          create_histogram_callback(nullptr),
          add_histogram_sample_callback(nullptr),
          array_buffer_allocator(nullptr),
          array_buffer_allocator_shared(),
          external_references(nullptr),
          allow_atomics_wait(true),
          only_terminate_in_safe_scope(false),
          embedder_wrapper_type_index(-1),
          embedder_wrapper_object_index(-1) {}

    /**
     * Allows the host application to provide the address of a function that is
     * notified each time code is added, moved or removed.
     */
    JitCodeEventHandler code_event_handler;

    /**
     * ResourceConstraints to use for the new Isolate.
     */
    ResourceConstraints constraints;

    /**
     * Explicitly specify a startup snapshot blob. The embedder owns the blob.
     */
    StartupData* snapshot_blob;


    /**
     * Enables the host application to provide a mechanism for recording
     * statistics counters.
     */
    CounterLookupCallback counter_lookup_callback;

    /**
     * Enables the host application to provide a mechanism for recording
     * histograms. The CreateHistogram function returns a
     * histogram which will later be passed to the AddHistogramSample
     * function.
     */
    CreateHistogramCallback create_histogram_callback;
    AddHistogramSampleCallback add_histogram_sample_callback;

    /**
     * The ArrayBuffer::Allocator to use for allocating and freeing the backing
     * store of ArrayBuffers.
     *
     * If the shared_ptr version is used, the Isolate instance and every
     * |BackingStore| allocated using this allocator hold a std::shared_ptr
     * to the allocator, in order to facilitate lifetime
     * management for the allocator instance.
     */
    ArrayBuffer::Allocator* array_buffer_allocator;
    std::shared_ptr<ArrayBuffer::Allocator> array_buffer_allocator_shared;

    /**
     * Specifies an optional nullptr-terminated array of raw addresses in the
     * embedder that V8 can match against during serialization and use for
     * deserialization. This array and its content must stay valid for the
     * entire lifetime of the isolate.
     */
    const intptr_t* external_references;

    /**
     * Whether calling Atomics.wait (a function that may block) is allowed in
     * this isolate. This can also be configured via SetAllowAtomicsWait.
     */
    bool allow_atomics_wait;

    /**
     * Termination is postponed when there is no active SafeForTerminationScope.
     */
    bool only_terminate_in_safe_scope;

    /**
     * The following parameters describe the offsets for addressing type info
     * for wrapped API objects and are used by the fast C API
     * (for details see v8-fast-api-calls.h).
     */
    int embedder_wrapper_type_index;
    int embedder_wrapper_object_index;
  };


  /**
   * Stack-allocated class which sets the isolate for all operations
   * executed within a local scope.
   */
  class V8_EXPORT Scope {
   public:
    explicit Scope(Isolate* isolate) : isolate_(isolate) {
      isolate->Enter();
    }

    ~Scope() { isolate_->Exit(); }

    // Prevent copying of Scope objects.
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

   private:
    Isolate* const isolate_;
  };


  /**
   * Assert that no Javascript code is invoked.
   */
  class V8_EXPORT DisallowJavascriptExecutionScope {
   public:
    enum OnFailure { CRASH_ON_FAILURE, THROW_ON_FAILURE, DUMP_ON_FAILURE };

    DisallowJavascriptExecutionScope(Isolate* isolate, OnFailure on_failure);
    ~DisallowJavascriptExecutionScope();

    // Prevent copying of Scope objects.
    DisallowJavascriptExecutionScope(const DisallowJavascriptExecutionScope&) =
        delete;
    DisallowJavascriptExecutionScope& operator=(
        const DisallowJavascriptExecutionScope&) = delete;

   private:
    OnFailure on_failure_;
    void* internal_;
  };


  /**
   * Introduce exception to DisallowJavascriptExecutionScope.
   */
  class V8_EXPORT AllowJavascriptExecutionScope {
   public:
    explicit AllowJavascriptExecutionScope(Isolate* isolate);
    ~AllowJavascriptExecutionScope();

    // Prevent copying of Scope objects.
    AllowJavascriptExecutionScope(const AllowJavascriptExecutionScope&) =
        delete;
    AllowJavascriptExecutionScope& operator=(
        const AllowJavascriptExecutionScope&) = delete;

   private:
    void* internal_throws_;
    void* internal_assert_;
    void* internal_dump_;
  };

  /**
   * Do not run microtasks while this scope is active, even if microtasks are
   * automatically executed otherwise.
   */
  class V8_EXPORT SuppressMicrotaskExecutionScope {
   public:
    explicit SuppressMicrotaskExecutionScope(
        Isolate* isolate, MicrotaskQueue* microtask_queue = nullptr);
    ~SuppressMicrotaskExecutionScope();

    // Prevent copying of Scope objects.
    SuppressMicrotaskExecutionScope(const SuppressMicrotaskExecutionScope&) =
        delete;
    SuppressMicrotaskExecutionScope& operator=(
        const SuppressMicrotaskExecutionScope&) = delete;

   private:
    internal::Isolate* const isolate_;
    internal::MicrotaskQueue* const microtask_queue_;
    internal::Address previous_stack_height_;

    friend class internal::ThreadLocalTop;
  };

  /**
   * This scope allows terminations inside direct V8 API calls and forbid them
   * inside any recursice API calls without explicit SafeForTerminationScope.
   */
  class V8_EXPORT SafeForTerminationScope {
   public:
    explicit SafeForTerminationScope(v8::Isolate* isolate);
    ~SafeForTerminationScope();

    // Prevent copying of Scope objects.
    SafeForTerminationScope(const SafeForTerminationScope&) = delete;
    SafeForTerminationScope& operator=(const SafeForTerminationScope&) = delete;

   private:
    internal::Isolate* isolate_;
    bool prev_value_;
  };

  /**
   * Types of garbage collections that can be requested via
   * RequestGarbageCollectionForTesting.
   */
  enum GarbageCollectionType {
    kFullGarbageCollection,
    kMinorGarbageCollection
  };

  /**
   * Features reported via the SetUseCounterCallback callback. Do not change
   * assigned numbers of existing items; add new features to the end of this
   * list.
   */
  enum UseCounterFeature {
    kUseAsm = 0,
    kBreakIterator = 1,
    kLegacyConst = 2,
    kMarkDequeOverflow = 3,
    kStoreBufferOverflow = 4,
    kSlotsBufferOverflow = 5,
    kObjectObserve = 6,
    kForcedGC = 7,
    kSloppyMode = 8,
    kStrictMode = 9,
    kStrongMode = 10,
    kRegExpPrototypeStickyGetter = 11,
    kRegExpPrototypeToString = 12,
    kRegExpPrototypeUnicodeGetter = 13,
    kIntlV8Parse = 14,
    kIntlPattern = 15,
    kIntlResolved = 16,
    kPromiseChain = 17,
    kPromiseAccept = 18,
    kPromiseDefer = 19,
    kHtmlCommentInExternalScript = 20,
    kHtmlComment = 21,
    kSloppyModeBlockScopedFunctionRedefinition = 22,
    kForInInitializer = 23,
    kArrayProtectorDirtied = 24,
    kArraySpeciesModified = 25,
    kArrayPrototypeConstructorModified = 26,
    kArrayInstanceProtoModified = 27,
    kArrayInstanceConstructorModified = 28,
    kLegacyFunctionDeclaration = 29,
    kRegExpPrototypeSourceGetter = 30,
    kRegExpPrototypeOldFlagGetter = 31,
    kDecimalWithLeadingZeroInStrictMode = 32,
    kLegacyDateParser = 33,
    kDefineGetterOrSetterWouldThrow = 34,
    kFunctionConstructorReturnedUndefined = 35,
    kAssigmentExpressionLHSIsCallInSloppy = 36,
    kAssigmentExpressionLHSIsCallInStrict = 37,
    kPromiseConstructorReturnedUndefined = 38,
    kConstructorNonUndefinedPrimitiveReturn = 39,
    kLabeledExpressionStatement = 40,
    kLineOrParagraphSeparatorAsLineTerminator = 41,
    kIndexAccessor = 42,
    kErrorCaptureStackTrace = 43,
    kErrorPrepareStackTrace = 44,
    kErrorStackTraceLimit = 45,
    kWebAssemblyInstantiation = 46,
    kDeoptimizerDisableSpeculation = 47,
    kArrayPrototypeSortJSArrayModifiedPrototype = 48,
    kFunctionTokenOffsetTooLongForToString = 49,
    kWasmSharedMemory = 50,
    kWasmThreadOpcodes = 51,
    kAtomicsNotify = 52,
    kAtomicsWake = 53,
    kCollator = 54,
    kNumberFormat = 55,
    kDateTimeFormat = 56,
    kPluralRules = 57,
    kRelativeTimeFormat = 58,
    kLocale = 59,
    kListFormat = 60,
    kSegmenter = 61,
    kStringLocaleCompare = 62,
    kStringToLocaleUpperCase = 63,
    kStringToLocaleLowerCase = 64,
    kNumberToLocaleString = 65,
    kDateToLocaleString = 66,
    kDateToLocaleDateString = 67,
    kDateToLocaleTimeString = 68,
    kAttemptOverrideReadOnlyOnPrototypeSloppy = 69,
    kAttemptOverrideReadOnlyOnPrototypeStrict = 70,
    kOptimizedFunctionWithOneShotBytecode = 71,
    kRegExpMatchIsTrueishOnNonJSRegExp = 72,
    kRegExpMatchIsFalseishOnJSRegExp = 73,
    kDateGetTimezoneOffset = 74,
    kStringNormalize = 75,
    kCallSiteAPIGetFunctionSloppyCall = 76,
    kCallSiteAPIGetThisSloppyCall = 77,
    kRegExpMatchAllWithNonGlobalRegExp = 78,
    kRegExpExecCalledOnSlowRegExp = 79,
    kRegExpReplaceCalledOnSlowRegExp = 80,
    kDisplayNames = 81,
    kSharedArrayBufferConstructed = 82,
    kArrayPrototypeHasElements = 83,
    kObjectPrototypeHasElements = 84,
    kNumberFormatStyleUnit = 85,
    kDateTimeFormatRange = 86,
    kDateTimeFormatDateTimeStyle = 87,
    kBreakIteratorTypeWord = 88,
    kBreakIteratorTypeLine = 89,

    // If you add new values here, you'll also need to update Chromium's:
    // web_feature.mojom, use_counter_callback.cc, and enums.xml. V8 changes to
    // this list need to be landed first, then changes on the Chromium side.
    kUseCounterFeatureCount  // This enum value must be last.
  };

  enum MessageErrorLevel {
    kMessageLog = (1 << 0),
    kMessageDebug = (1 << 1),
    kMessageInfo = (1 << 2),
    kMessageError = (1 << 3),
    kMessageWarning = (1 << 4),
    kMessageAll = kMessageLog | kMessageDebug | kMessageInfo | kMessageError |
                  kMessageWarning,
  };

  typedef void (*UseCounterCallback)(Isolate* isolate,
                                     UseCounterFeature feature);

  /**
   * Allocates a new isolate but does not initialize it. Does not change the
   * currently entered isolate.
   *
   * Only Isolate::GetData() and Isolate::SetData(), which access the
   * embedder-controlled parts of the isolate, are allowed to be called on the
   * uninitialized isolate. To initialize the isolate, call
   * Isolate::Initialize().
   *
   * When an isolate is no longer used its resources should be freed
   * by calling Dispose().  Using the delete operator is not allowed.
   *
   * V8::Initialize() must have run prior to this.
   */
  static Isolate* Allocate();

  /**
   * Initialize an Isolate previously allocated by Isolate::Allocate().
   */
  static void Initialize(Isolate* isolate, const CreateParams& params);

  /**
   * Creates a new isolate.  Does not change the currently entered
   * isolate.
   *
   * When an isolate is no longer used its resources should be freed
   * by calling Dispose().  Using the delete operator is not allowed.
   *
   * V8::Initialize() must have run prior to this.
   */
  static Isolate* New(const CreateParams& params);

  /**
   * Returns the entered isolate for the current thread or NULL in
   * case there is no current isolate.
   *
   * This method must not be invoked before V8::Initialize() was invoked.
   */
  static Isolate* GetCurrent();

  /**
   * Clears the set of objects held strongly by the heap. This set of
   * objects are originally built when a WeakRef is created or
   * successfully dereferenced.
   *
   * This is invoked automatically after microtasks are run. See
   * MicrotasksPolicy for when microtasks are run.
   *
   * This needs to be manually invoked only if the embedder is manually running
   * microtasks via a custom MicrotaskQueue class's PerformCheckpoint. In that
   * case, it is the embedder's responsibility to make this call at a time which
   * does not interrupt synchronous ECMAScript code execution.
   */
  void ClearKeptObjects();

  /**
   * Custom callback used by embedders to help V8 determine if it should abort
   * when it throws and no internal handler is predicted to catch the
   * exception. If --abort-on-uncaught-exception is used on the command line,
   * then V8 will abort if either:
   * - no custom callback is set.
   * - the custom callback set returns true.
   * Otherwise, the custom callback will not be called and V8 will not abort.
   */
  typedef bool (*AbortOnUncaughtExceptionCallback)(Isolate*);
  void SetAbortOnUncaughtExceptionCallback(
      AbortOnUncaughtExceptionCallback callback);

  /**
   * This specifies the callback to be called when FinalizationRegistries
   * are ready to be cleaned up and require FinalizationGroup::Cleanup()
   * to be called in a future task.
   */
  V8_DEPRECATED(
      "FinalizationRegistry cleanup is automatic if "
      "HostCleanupFinalizationGroupCallback is not set")
  void SetHostCleanupFinalizationGroupCallback(
      HostCleanupFinalizationGroupCallback callback);

  /**
   * This specifies the callback called by the upcoming dynamic
   * import() language feature to load modules.
   */
  void SetHostImportModuleDynamicallyCallback(
      HostImportModuleDynamicallyCallback callback);

  /**
   * This specifies the callback called by the upcoming importa.meta
   * language feature to retrieve host-defined meta data for a module.
   */
  void SetHostInitializeImportMetaObjectCallback(
      HostInitializeImportMetaObjectCallback callback);

  /**
   * This specifies the callback called when the stack property of Error
   * is accessed.
   */
  void SetPrepareStackTraceCallback(PrepareStackTraceCallback callback);

  /**
   * Optional notification that the system is running low on memory.
   * V8 uses these notifications to guide heuristics.
   * It is allowed to call this function from another thread while
   * the isolate is executing long running JavaScript code.
   */
  void MemoryPressureNotification(MemoryPressureLevel level);

  /**
   * Methods below this point require holding a lock (using Locker) in
   * a multi-threaded environment.
   */

  /**
   * Sets this isolate as the entered one for the current thread.
   * Saves the previously entered one (if any), so that it can be
   * restored when exiting.  Re-entering an isolate is allowed.
   */
  void Enter();

  /**
   * Exits this isolate by restoring the previously entered one in the
   * current thread.  The isolate may still stay the same, if it was
   * entered more than once.
   *
   * Requires: this == Isolate::GetCurrent().
   */
  void Exit();

  /**
   * Disposes the isolate.  The isolate must not be entered by any
   * thread to be disposable.
   */
  void Dispose();

  /**
   * Dumps activated low-level V8 internal stats. This can be used instead
   * of performing a full isolate disposal.
   */
  void DumpAndResetStats();

  /**
   * Discards all V8 thread-specific data for the Isolate. Should be used
   * if a thread is terminating and it has used an Isolate that will outlive
   * the thread -- all thread-specific data for an Isolate is discarded when
   * an Isolate is disposed so this call is pointless if an Isolate is about
   * to be Disposed.
   */
  void DiscardThreadSpecificMetadata();

  /**
   * Associate embedder-specific data with the isolate. |slot| has to be
   * between 0 and GetNumberOfDataSlots() - 1.
   */
  V8_INLINE void SetData(uint32_t slot, void* data);

  /**
   * Retrieve embedder-specific data from the isolate.
   * Returns NULL if SetData has never been called for the given |slot|.
   */
  V8_INLINE void* GetData(uint32_t slot);

  /**
   * Returns the maximum number of available embedder data slots. Valid slots
   * are in the range of 0 - GetNumberOfDataSlots() - 1.
   */
  V8_INLINE static uint32_t GetNumberOfDataSlots();

  /**
   * Return data that was previously attached to the isolate snapshot via
   * SnapshotCreator, and removes the reference to it.
   * Repeated call with the same index returns an empty MaybeLocal.
   */
  template <class T>
  V8_INLINE MaybeLocal<T> GetDataFromSnapshotOnce(size_t index);

  /**
   * Get statistics about the heap memory usage.
   */
  void GetHeapStatistics(HeapStatistics* heap_statistics);

  /**
   * Returns the number of spaces in the heap.
   */
  size_t NumberOfHeapSpaces();

  /**
   * Get the memory usage of a space in the heap.
   *
   * \param space_statistics The HeapSpaceStatistics object to fill in
   *   statistics.
   * \param index The index of the space to get statistics from, which ranges
   *   from 0 to NumberOfHeapSpaces() - 1.
   * \returns true on success.
   */
  bool GetHeapSpaceStatistics(HeapSpaceStatistics* space_statistics,
                              size_t index);

  /**
   * Returns the number of types of objects tracked in the heap at GC.
   */
  size_t NumberOfTrackedHeapObjectTypes();

  /**
   * Get statistics about objects in the heap.
   *
   * \param object_statistics The HeapObjectStatistics object to fill in
   *   statistics of objects of given type, which were live in the previous GC.
   * \param type_index The index of the type of object to fill details about,
   *   which ranges from 0 to NumberOfTrackedHeapObjectTypes() - 1.
   * \returns true on success.
   */
  bool GetHeapObjectStatisticsAtLastGC(HeapObjectStatistics* object_statistics,
                                       size_t type_index);

  /**
   * Get statistics about code and its metadata in the heap.
   *
   * \param object_statistics The HeapCodeStatistics object to fill in
   *   statistics of code, bytecode and their metadata.
   * \returns true on success.
   */
  bool GetHeapCodeAndMetadataStatistics(HeapCodeStatistics* object_statistics);

  /**
   * This API is experimental and may change significantly.
   *
   * Enqueues a memory measurement request and invokes the delegate with the
   * results.
   *
   * \param delegate the delegate that defines which contexts to measure and
   *   reports the results.
   *
   * \param execution promptness executing the memory measurement.
   *   The kEager value is expected to be used only in tests.
   */
  bool MeasureMemory(
      std::unique_ptr<MeasureMemoryDelegate> delegate,
      MeasureMemoryExecution execution = MeasureMemoryExecution::kDefault);

  V8_DEPRECATE_SOON("Use the version with a delegate")
  MaybeLocal<Promise> MeasureMemory(Local<Context> context,
                                    MeasureMemoryMode mode);

  /**
   * Get a call stack sample from the isolate.
   * \param state Execution state.
   * \param frames Caller allocated buffer to store stack frames.
   * \param frames_limit Maximum number of frames to capture. The buffer must
   *                     be large enough to hold the number of frames.
   * \param sample_info The sample info is filled up by the function
   *                    provides number of actual captured stack frames and
   *                    the current VM state.
   * \note GetStackSample should only be called when the JS thread is paused or
   *       interrupted. Otherwise the behavior is undefined.
   */
  void GetStackSample(const RegisterState& state, void** frames,
                      size_t frames_limit, SampleInfo* sample_info);

  /**
   * Adjusts the amount of registered external memory. Used to give V8 an
   * indication of the amount of externally allocated memory that is kept alive
   * by JavaScript objects. V8 uses this to decide when to perform global
   * garbage collections. Registering externally allocated memory will trigger
   * global garbage collections more often than it would otherwise in an attempt
   * to garbage collect the JavaScript objects that keep the externally
   * allocated memory alive.
   *
   * \param change_in_bytes the change in externally allocated memory that is
   *   kept alive by JavaScript objects.
   * \returns the adjusted value.
   */
  V8_INLINE int64_t
      AdjustAmountOfExternalAllocatedMemory(int64_t change_in_bytes);

  /**
   * Returns the number of phantom handles without callbacks that were reset
   * by the garbage collector since the last call to this function.
   */
  size_t NumberOfPhantomHandleResetsSinceLastCall();

  /**
   * Returns heap profiler for this isolate. Will return NULL until the isolate
   * is initialized.
   */
  HeapProfiler* GetHeapProfiler();

  /**
   * Tells the VM whether the embedder is idle or not.
   */
  void SetIdle(bool is_idle);

  /** Returns the ArrayBuffer::Allocator used in this isolate. */
  ArrayBuffer::Allocator* GetArrayBufferAllocator();

  /** Returns true if this isolate has a current context. */
  bool InContext();

  /**
   * Returns the context of the currently running JavaScript, or the context
   * on the top of the stack if no JavaScript is running.
   */
  Local<Context> GetCurrentContext();

  /** Returns the last context entered through V8's C++ API. */
  V8_DEPRECATED("Use GetEnteredOrMicrotaskContext().")
  Local<Context> GetEnteredContext();

  /**
   * Returns either the last context entered through V8's C++ API, or the
   * context of the currently running microtask while processing microtasks.
   * If a context is entered while executing a microtask, that context is
   * returned.
   */
  Local<Context> GetEnteredOrMicrotaskContext();

  /**
   * Returns the Context that corresponds to the Incumbent realm in HTML spec.
   * https://html.spec.whatwg.org/multipage/webappapis.html#incumbent
   */
  Local<Context> GetIncumbentContext();

  /**
   * Schedules an exception to be thrown when returning to JavaScript.  When an
   * exception has been scheduled it is illegal to invoke any JavaScript
   * operation; the caller must return immediately and only after the exception
   * has been handled does it become legal to invoke JavaScript operations.
   */
  Local<Value> ThrowException(Local<Value> exception);

  typedef void (*GCCallback)(Isolate* isolate, GCType type,
                             GCCallbackFlags flags);
  typedef void (*GCCallbackWithData)(Isolate* isolate, GCType type,
                                     GCCallbackFlags flags, void* data);

  /**
   * Enables the host application to receive a notification before a
   * garbage collection. Allocations are allowed in the callback function,
   * but the callback is not re-entrant: if the allocation inside it will
   * trigger the garbage collection, the callback won't be called again.
   * It is possible to specify the GCType filter for your callback. But it is
   * not possible to register the same callback function two times with
   * different GCType filters.
   */
  void AddGCPrologueCallback(GCCallbackWithData callback, void* data = nullptr,
                             GCType gc_type_filter = kGCTypeAll);
  void AddGCPrologueCallback(GCCallback callback,
                             GCType gc_type_filter = kGCTypeAll);

  /**
   * This function removes callback which was installed by
   * AddGCPrologueCallback function.
   */
  void RemoveGCPrologueCallback(GCCallbackWithData, void* data = nullptr);
  void RemoveGCPrologueCallback(GCCallback callback);

  /**
   * Sets the embedder heap tracer for the isolate.
   */
  void SetEmbedderHeapTracer(EmbedderHeapTracer* tracer);

  /*
   * Gets the currently active heap tracer for the isolate.
   */
  EmbedderHeapTracer* GetEmbedderHeapTracer();

  /**
   * Use for |AtomicsWaitCallback| to indicate the type of event it receives.
   */
  enum class AtomicsWaitEvent {
    /** Indicates that this call is happening before waiting. */
    kStartWait,
    /** `Atomics.wait()` finished because of an `Atomics.wake()` call. */
    kWokenUp,
    /** `Atomics.wait()` finished because it timed out. */
    kTimedOut,
    /** `Atomics.wait()` was interrupted through |TerminateExecution()|. */
    kTerminatedExecution,
    /** `Atomics.wait()` was stopped through |AtomicsWaitWakeHandle|. */
    kAPIStopped,
    /** `Atomics.wait()` did not wait, as the initial condition was not met. */
    kNotEqual
  };

  /**
   * Passed to |AtomicsWaitCallback| as a means of stopping an ongoing
   * `Atomics.wait` call.
   */
  class V8_EXPORT AtomicsWaitWakeHandle {
   public:
    /**
     * Stop this `Atomics.wait()` call and call the |AtomicsWaitCallback|
     * with |kAPIStopped|.
     *
     * This function may be called from another thread. The caller has to ensure
     * through proper synchronization that it is not called after
     * the finishing |AtomicsWaitCallback|.
     *
     * Note that the ECMAScript specification does not plan for the possibility
     * of wakeups that are neither coming from a timeout or an `Atomics.wake()`
     * call, so this may invalidate assumptions made by existing code.
     * The embedder may accordingly wish to schedule an exception in the
     * finishing |AtomicsWaitCallback|.
     */
    void Wake();
  };

  /**
   * Embedder callback for `Atomics.wait()` that can be added through
   * |SetAtomicsWaitCallback|.
   *
   * This will be called just before starting to wait with the |event| value
   * |kStartWait| and after finishing waiting with one of the other
   * values of |AtomicsWaitEvent| inside of an `Atomics.wait()` call.
   *
   * |array_buffer| will refer to the underlying SharedArrayBuffer,
   * |offset_in_bytes| to the location of the waited-on memory address inside
   * the SharedArrayBuffer.
   *
   * |value| and |timeout_in_ms| will be the values passed to
   * the `Atomics.wait()` call. If no timeout was used, |timeout_in_ms|
   * will be `INFINITY`.
   *
   * In the |kStartWait| callback, |stop_handle| will be an object that
   * is only valid until the corresponding finishing callback and that
   * can be used to stop the wait process while it is happening.
   *
   * This callback may schedule exceptions, *unless* |event| is equal to
   * |kTerminatedExecution|.
   */
  typedef void (*AtomicsWaitCallback)(AtomicsWaitEvent event,
                                      Local<SharedArrayBuffer> array_buffer,
                                      size_t offset_in_bytes, int64_t value,
                                      double timeout_in_ms,
                                      AtomicsWaitWakeHandle* stop_handle,
                                      void* data);

  /**
   * Set a new |AtomicsWaitCallback|. This overrides an earlier
   * |AtomicsWaitCallback|, if there was any. If |callback| is nullptr,
   * this unsets the callback. |data| will be passed to the callback
   * as its last parameter.
   */
  void SetAtomicsWaitCallback(AtomicsWaitCallback callback, void* data);

  /**
   * Enables the host application to receive a notification after a
   * garbage collection. Allocations are allowed in the callback function,
   * but the callback is not re-entrant: if the allocation inside it will
   * trigger the garbage collection, the callback won't be called again.
   * It is possible to specify the GCType filter for your callback. But it is
   * not possible to register the same callback function two times with
   * different GCType filters.
   */
  void AddGCEpilogueCallback(GCCallbackWithData callback, void* data = nullptr,
                             GCType gc_type_filter = kGCTypeAll);
  void AddGCEpilogueCallback(GCCallback callback,
                             GCType gc_type_filter = kGCTypeAll);

  /**
   * This function removes callback which was installed by
   * AddGCEpilogueCallback function.
   */
  void RemoveGCEpilogueCallback(GCCallbackWithData callback,
                                void* data = nullptr);
  void RemoveGCEpilogueCallback(GCCallback callback);

  typedef size_t (*GetExternallyAllocatedMemoryInBytesCallback)();

  /**
   * Set the callback that tells V8 how much memory is currently allocated
   * externally of the V8 heap. Ideally this memory is somehow connected to V8
   * objects and may get freed-up when the corresponding V8 objects get
   * collected by a V8 garbage collection.
   */
  void SetGetExternallyAllocatedMemoryInBytesCallback(
      GetExternallyAllocatedMemoryInBytesCallback callback);

  /**
   * Forcefully terminate the current thread of JavaScript execution
   * in the given isolate.
   *
   * This method can be used by any thread even if that thread has not
   * acquired the V8 lock with a Locker object.
   */
  void TerminateExecution();

  /**
   * Is V8 terminating JavaScript execution.
   *
   * Returns true if JavaScript execution is currently terminating
   * because of a call to TerminateExecution.  In that case there are
   * still JavaScript frames on the stack and the termination
   * exception is still active.
   */
  bool IsExecutionTerminating();

  /**
   * Resume execution capability in the given isolate, whose execution
   * was previously forcefully terminated using TerminateExecution().
   *
   * When execution is forcefully terminated using TerminateExecution(),
   * the isolate can not resume execution until all JavaScript frames
   * have propagated the uncatchable exception which is generated.  This
   * method allows the program embedding the engine to handle the
   * termination event and resume execution capability, even if
   * JavaScript frames remain on the stack.
   *
   * This method can be used by any thread even if that thread has not
   * acquired the V8 lock with a Locker object.
   */
  void CancelTerminateExecution();

  /**
   * Request V8 to interrupt long running JavaScript code and invoke
   * the given |callback| passing the given |data| to it. After |callback|
   * returns control will be returned to the JavaScript code.
   * There may be a number of interrupt requests in flight.
   * Can be called from another thread without acquiring a |Locker|.
   * Registered |callback| must not reenter interrupted Isolate.
   */
  void RequestInterrupt(InterruptCallback callback, void* data);

  /**
   * Request garbage collection in this Isolate. It is only valid to call this
   * function if --expose_gc was specified.
   *
   * This should only be used for testing purposes and not to enforce a garbage
   * collection schedule. It has strong negative impact on the garbage
   * collection performance. Use IdleNotificationDeadline() or
   * LowMemoryNotification() instead to influence the garbage collection
   * schedule.
   */
  void RequestGarbageCollectionForTesting(GarbageCollectionType type);

  /**
   * Set the callback to invoke for logging event.
   */
  void SetEventLogger(LogEventCallback that);

  /**
   * Adds a callback to notify the host application right before a script
   * is about to run. If a script re-enters the runtime during executing, the
   * BeforeCallEnteredCallback is invoked for each re-entrance.
   * Executing scripts inside the callback will re-trigger the callback.
   */
  void AddBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);

  /**
   * Removes callback that was installed by AddBeforeCallEnteredCallback.
   */
  void RemoveBeforeCallEnteredCallback(BeforeCallEnteredCallback callback);

  /**
   * Adds a callback to notify the host application when a script finished
   * running.  If a script re-enters the runtime during executing, the
   * CallCompletedCallback is only invoked when the outer-most script
   * execution ends.  Executing scripts inside the callback do not trigger
   * further callbacks.
   */
  void AddCallCompletedCallback(CallCompletedCallback callback);

  /**
   * Removes callback that was installed by AddCallCompletedCallback.
   */
  void RemoveCallCompletedCallback(CallCompletedCallback callback);

  /**
   * Set the PromiseHook callback for various promise lifecycle
   * events.
   */
  void SetPromiseHook(PromiseHook hook);

  /**
   * Set callback to notify about promise reject with no handler, or
   * revocation of such a previous notification once the handler is added.
   */
  void SetPromiseRejectCallback(PromiseRejectCallback callback);

  /**
   * An alias for PerformMicrotaskCheckpoint.
   */
  V8_DEPRECATE_SOON("Use PerformMicrotaskCheckpoint.")
  void RunMicrotasks() { PerformMicrotaskCheckpoint(); }

  /**
   * Runs the default MicrotaskQueue until it gets empty and perform other
   * microtask checkpoint steps, such as calling ClearKeptObjects. Asserts that
   * the MicrotasksPolicy is not kScoped. Any exceptions thrown by microtask
   * callbacks are swallowed.
   */
  void PerformMicrotaskCheckpoint();

  /**
   * Enqueues the callback to the default MicrotaskQueue
   */
  void EnqueueMicrotask(Local<Function> microtask);

  /**
   * Enqueues the callback to the default MicrotaskQueue
   */
  void EnqueueMicrotask(MicrotaskCallback callback, void* data = nullptr);

  /**
   * Controls how Microtasks are invoked. See MicrotasksPolicy for details.
   */
  void SetMicrotasksPolicy(MicrotasksPolicy policy);

  /**
   * Returns the policy controlling how Microtasks are invoked.
   */
  MicrotasksPolicy GetMicrotasksPolicy() const;

  /**
   * Adds a callback to notify the host application after
   * microtasks were run on the default MicrotaskQueue. The callback is
   * triggered by explicit RunMicrotasks call or automatic microtasks execution
   * (see SetMicrotaskPolicy).
   *
   * Callback will trigger even if microtasks were attempted to run,
   * but the microtasks queue was empty and no single microtask was actually
   * executed.
   *
   * Executing scripts inside the callback will not re-trigger microtasks and
   * the callback.
   */
  V8_DEPRECATE_SOON("Use *WithData version.")
  void AddMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  void AddMicrotasksCompletedCallback(
      MicrotasksCompletedCallbackWithData callback, void* data = nullptr);

  /**
   * Removes callback that was installed by AddMicrotasksCompletedCallback.
   */
  V8_DEPRECATE_SOON("Use *WithData version.")
  void RemoveMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  void RemoveMicrotasksCompletedCallback(
      MicrotasksCompletedCallbackWithData callback, void* data = nullptr);

  /**
   * Sets a callback for counting the number of times a feature of V8 is used.
   */
  void SetUseCounterCallback(UseCounterCallback callback);

  /**
   * Enables the host application to provide a mechanism for recording
   * statistics counters.
   */
  void SetCounterFunction(CounterLookupCallback);

  /**
   * Enables the host application to provide a mechanism for recording
   * histograms. The CreateHistogram function returns a
   * histogram which will later be passed to the AddHistogramSample
   * function.
   */
  void SetCreateHistogramFunction(CreateHistogramCallback);
  void SetAddHistogramSampleFunction(AddHistogramSampleCallback);

  /**
   * Enables the host application to provide a mechanism for recording a
   * predefined set of data as crash keys to be used in postmortem debugging in
   * case of a crash.
   */
  void SetAddCrashKeyCallback(AddCrashKeyCallback);

  /**
   * Optional notification that the embedder is idle.
   * V8 uses the notification to perform garbage collection.
   * This call can be used repeatedly if the embedder remains idle.
   * Returns true if the embedder should stop calling IdleNotificationDeadline
   * until real work has been done.  This indicates that V8 has done
   * as much cleanup as it will be able to do.
   *
   * The deadline_in_seconds argument specifies the deadline V8 has to finish
   * garbage collection work. deadline_in_seconds is compared with
   * MonotonicallyIncreasingTime() and should be based on the same timebase as
   * that function. There is no guarantee that the actual work will be done
   * within the time limit.
   */
  bool IdleNotificationDeadline(double deadline_in_seconds);

  /**
   * Optional notification that the system is running low on memory.
   * V8 uses these notifications to attempt to free memory.
   */
  void LowMemoryNotification();

  /**
   * Optional notification that a context has been disposed. V8 uses these
   * notifications to guide the GC heuristic and cancel FinalizationRegistry
   * cleanup tasks. Returns the number of context disposals - including this one
   * - since the last time V8 had a chance to clean up.
   *
   * The optional parameter |dependant_context| specifies whether the disposed
   * context was depending on state from other contexts or not.
   */
  int ContextDisposedNotification(bool dependant_context = true);

  /**
   * Optional notification that the isolate switched to the foreground.
   * V8 uses these notifications to guide heuristics.
   */
  void IsolateInForegroundNotification();

  /**
   * Optional notification that the isolate switched to the background.
   * V8 uses these notifications to guide heuristics.
   */
  void IsolateInBackgroundNotification();

  /**
   * Optional notification which will enable the memory savings mode.
   * V8 uses this notification to guide heuristics which may result in a
   * smaller memory footprint at the cost of reduced runtime performance.
   */
  void EnableMemorySavingsMode();

  /**
   * Optional notification which will disable the memory savings mode.
   */
  void DisableMemorySavingsMode();

  /**
   * Optional notification to tell V8 the current performance requirements
   * of the embedder based on RAIL.
   * V8 uses these notifications to guide heuristics.
   * This is an unfinished experimental feature. Semantics and implementation
   * may change frequently.
   */
  void SetRAILMode(RAILMode rail_mode);

  /**
   * Optional notification to tell V8 the current isolate is used for debugging
   * and requires higher heap limit.
   */
  void IncreaseHeapLimitForDebugging();

  /**
   * Restores the original heap limit after IncreaseHeapLimitForDebugging().
   */
  void RestoreOriginalHeapLimit();

  /**
   * Returns true if the heap limit was increased for debugging and the
   * original heap limit was not restored yet.
   */
  bool IsHeapLimitIncreasedForDebugging();

  /**
   * Allows the host application to provide the address of a function that is
   * notified each time code is added, moved or removed.
   *
   * \param options options for the JIT code event handler.
   * \param event_handler the JIT code event handler, which will be invoked
   *     each time code is added, moved or removed.
   * \note \p event_handler won't get notified of existent code.
   * \note since code removal notifications are not currently issued, the
   *     \p event_handler may get notifications of code that overlaps earlier
   *     code notifications. This happens when code areas are reused, and the
   *     earlier overlapping code areas should therefore be discarded.
   * \note the events passed to \p event_handler and the strings they point to
   *     are not guaranteed to live past each call. The \p event_handler must
   *     copy strings and other parameters it needs to keep around.
   * \note the set of events declared in JitCodeEvent::EventType is expected to
   *     grow over time, and the JitCodeEvent structure is expected to accrue
   *     new members. The \p event_handler function must ignore event codes
   *     it does not recognize to maintain future compatibility.
   * \note Use Isolate::CreateParams to get events for code executed during
   *     Isolate setup.
   */
  void SetJitCodeEventHandler(JitCodeEventOptions options,
                              JitCodeEventHandler event_handler);

  /**
   * Modifies the stack limit for this Isolate.
   *
   * \param stack_limit An address beyond which the Vm's stack may not grow.
   *
   * \note  If you are using threads then you should hold the V8::Locker lock
   *     while setting the stack limit and you must set a non-default stack
   *     limit separately for each thread.
   */
  void SetStackLimit(uintptr_t stack_limit);

  /**
   * Returns a memory range that can potentially contain jitted code. Code for
   * V8's 'builtins' will not be in this range if embedded builtins is enabled.
   *
   * On Win64, embedders are advised to install function table callbacks for
   * these ranges, as default SEH won't be able to unwind through jitted code.
   * The first page of the code range is reserved for the embedder and is
   * committed, writable, and executable, to be used to store unwind data, as
   * documented in
   * https://docs.microsoft.com/en-us/cpp/build/exception-handling-x64.
   *
   * Might be empty on other platforms.
   *
   * https://code.google.com/p/v8/issues/detail?id=3598
   */
  void GetCodeRange(void** start, size_t* length_in_bytes);

  /**
   * Returns the UnwindState necessary for use with the Unwinder API.
   */
  // TODO(petermarshall): Remove this API.
  V8_DEPRECATED("Use entry_stubs + code_pages version.")
  UnwindState GetUnwindState();

  /**
   * Returns the JSEntryStubs necessary for use with the Unwinder API.
   */
  JSEntryStubs GetJSEntryStubs();

  static constexpr size_t kMinCodePagesBufferSize = 32;

  /**
   * Copies the code heap pages currently in use by V8 into |code_pages_out|.
   * |code_pages_out| must have at least kMinCodePagesBufferSize capacity and
   * must be empty.
   *
   * Signal-safe, does not allocate, does not access the V8 heap.
   * No code on the stack can rely on pages that might be missing.
   *
   * Returns the number of pages available to be copied, which might be greater
   * than |capacity|. In this case, only |capacity| pages will be copied into
   * |code_pages_out|. The caller should provide a bigger buffer on the next
   * call in order to get all available code pages, but this is not required.
   */
  size_t CopyCodePages(size_t capacity, MemoryRange* code_pages_out);

  /** Set the callback to invoke in case of fatal errors. */
  void SetFatalErrorHandler(FatalErrorCallback that);

  /** Set the callback to invoke in case of OOM errors. */
  void SetOOMErrorHandler(OOMErrorCallback that);

  /**
   * Add a callback to invoke in case the heap size is close to the heap limit.
   * If multiple callbacks are added, only the most recently added callback is
   * invoked.
   */
  void AddNearHeapLimitCallback(NearHeapLimitCallback callback, void* data);

  /**
   * Remove the given callback and restore the heap limit to the
   * given limit. If the given limit is zero, then it is ignored.
   * If the current heap size is greater than the given limit,
   * then the heap limit is restored to the minimal limit that
   * is possible for the current heap size.
   */
  void RemoveNearHeapLimitCallback(NearHeapLimitCallback callback,
                                   size_t heap_limit);

  /**
   * If the heap limit was changed by the NearHeapLimitCallback, then the
   * initial heap limit will be restored once the heap size falls below the
   * given threshold percentage of the initial heap limit.
   * The threshold percentage is a number in (0.0, 1.0) range.
   */
  void AutomaticallyRestoreInitialHeapLimit(double threshold_percent = 0.5);

  /**
   * Set the callback to invoke to check if code generation from
   * strings should be allowed.
   */
  V8_DEPRECATED(
      "Use Isolate::SetModifyCodeGenerationFromStringsCallback instead. "
      "See http://crbug.com/v8/10096.")
  void SetAllowCodeGenerationFromStringsCallback(
      AllowCodeGenerationFromStringsCallback callback);
  void SetModifyCodeGenerationFromStringsCallback(
      ModifyCodeGenerationFromStringsCallback callback);

  /**
   * Set the callback to invoke to check if wasm code generation should
   * be allowed.
   */
  void SetAllowWasmCodeGenerationCallback(
      AllowWasmCodeGenerationCallback callback);

  /**
   * Embedder over{ride|load} injection points for wasm APIs. The expectation
   * is that the embedder sets them at most once.
   */
  void SetWasmModuleCallback(ExtensionCallback callback);
  void SetWasmInstanceCallback(ExtensionCallback callback);

  void SetWasmStreamingCallback(WasmStreamingCallback callback);

  void SetWasmThreadsEnabledCallback(WasmThreadsEnabledCallback callback);

  void SetWasmLoadSourceMapCallback(WasmLoadSourceMapCallback callback);

  /**
  * Check if V8 is dead and therefore unusable.  This is the case after
  * fatal errors such as out-of-memory situations.
  */
  bool IsDead();

  /**
   * Adds a message listener (errors only).
   *
   * The same message listener can be added more than once and in that
   * case it will be called more than once for each message.
   *
   * If data is specified, it will be passed to the callback when it is called.
   * Otherwise, the exception object will be passed to the callback instead.
   */
  bool AddMessageListener(MessageCallback that,
                          Local<Value> data = Local<Value>());

  /**
   * Adds a message listener.
   *
   * The same message listener can be added more than once and in that
   * case it will be called more than once for each message.
   *
   * If data is specified, it will be passed to the callback when it is called.
   * Otherwise, the exception object will be passed to the callback instead.
   *
   * A listener can listen for particular error levels by providing a mask.
   */
  bool AddMessageListenerWithErrorLevel(MessageCallback that,
                                        int message_levels,
                                        Local<Value> data = Local<Value>());

  /**
   * Remove all message listeners from the specified callback function.
   */
  void RemoveMessageListeners(MessageCallback that);

  /** Callback function for reporting failed access checks.*/
  void SetFailedAccessCheckCallbackFunction(FailedAccessCheckCallback);

  /**
   * Tells V8 to capture current stack trace when uncaught exception occurs
   * and report it to the message listeners. The option is off by default.
   */
  void SetCaptureStackTraceForUncaughtExceptions(
      bool capture, int frame_limit = 10,
      StackTrace::StackTraceOptions options = StackTrace::kOverview);

  /**
   * Iterates through all external resources referenced from current isolate
   * heap.  GC is not invoked prior to iterating, therefore there is no
   * guarantee that visited objects are still alive.
   */
  void VisitExternalResources(ExternalResourceVisitor* visitor);

  /**
   * Iterates through all the persistent handles in the current isolate's heap
   * that have class_ids.
   */
  void VisitHandlesWithClassIds(PersistentHandleVisitor* visitor);

  /**
   * Iterates through all the persistent handles in the current isolate's heap
   * that have class_ids and are weak to be marked as inactive if there is no
   * pending activity for the handle.
   */
  void VisitWeakHandles(PersistentHandleVisitor* visitor);

  /**
   * Check if this isolate is in use.
   * True if at least one thread Enter'ed this isolate.
   */
  bool IsInUse();

  /**
   * Set whether calling Atomics.wait (a function that may block) is allowed in
   * this isolate. This can also be configured via
   * CreateParams::allow_atomics_wait.
   */
  void SetAllowAtomicsWait(bool allow);

  /**
   * Time zone redetection indicator for
   * DateTimeConfigurationChangeNotification.
   *
   * kSkip indicates V8 that the notification should not trigger redetecting
   * host time zone. kRedetect indicates V8 that host time zone should be
   * redetected, and used to set the default time zone.
   *
   * The host time zone detection may require file system access or similar
   * operations unlikely to be available inside a sandbox. If v8 is run inside a
   * sandbox, the host time zone has to be detected outside the sandbox before
   * calling DateTimeConfigurationChangeNotification function.
   */
  enum class TimeZoneDetection { kSkip, kRedetect };

  /**
   * Notification that the embedder has changed the time zone, daylight savings
   * time or other date / time configuration parameters. V8 keeps a cache of
   * various values used for date / time computation. This notification will
   * reset those cached values for the current context so that date / time
   * configuration changes would be reflected.
   *
   * This API should not be called more than needed as it will negatively impact
   * the performance of date operations.
   */
  void DateTimeConfigurationChangeNotification(
      TimeZoneDetection time_zone_detection = TimeZoneDetection::kSkip);

  /**
   * Notification that the embedder has changed the locale. V8 keeps a cache of
   * various values used for locale computation. This notification will reset
   * those cached values for the current context so that locale configuration
   * changes would be reflected.
   *
   * This API should not be called more than needed as it will negatively impact
   * the performance of locale operations.
   */
  void LocaleConfigurationChangeNotification();

  Isolate() = delete;
  ~Isolate() = delete;
  Isolate(const Isolate&) = delete;
  Isolate& operator=(const Isolate&) = delete;
  // Deleting operator new and delete here is allowed as ctor and dtor is also
  // deleted.
  void* operator new(size_t size) = delete;
  void* operator new[](size_t size) = delete;
  void operator delete(void*, size_t) = delete;
  void operator delete[](void*, size_t) = delete;

 private:
  template <class K, class V, class Traits>
  friend class PersistentValueMapBase;

  internal::Address* GetDataFromSnapshotOnce(size_t index);
  void ReportExternalAllocationLimitReached();
  void CheckMemoryPressure();
};

class V8_EXPORT StartupData {
 public:
  /**
   * Whether the data created can be rehashed and and the hash seed can be
   * recomputed when deserialized.
   * Only valid for StartupData returned by SnapshotCreator::CreateBlob().
   */
  bool CanBeRehashed() const;

  const char* data;
  int raw_size;
};


/**
 * EntropySource is used as a callback function when v8 needs a source
 * of entropy.
 */
typedef bool (*EntropySource)(unsigned char* buffer, size_t length);

/**
 * ReturnAddressLocationResolver is used as a callback function when v8 is
 * resolving the location of a return address on the stack. Profilers that
 * change the return address on the stack can use this to resolve the stack
 * location to wherever the profiler stashed the original return address.
 *
 * \param return_addr_location A location on stack where a machine
 *    return address resides.
 * \returns Either return_addr_location, or else a pointer to the profiler's
 *    copy of the original return address.
 *
 * \note The resolver function must not cause garbage collection.
 */
typedef uintptr_t (*ReturnAddressLocationResolver)(
    uintptr_t return_addr_location);


/**
 * Container class for static utility functions.
 */
class V8_EXPORT V8 {
 public:
  /**
   * Hand startup data to V8, in case the embedder has chosen to build
   * V8 with external startup data.
   *
   * Note:
   * - By default the startup data is linked into the V8 library, in which
   *   case this function is not meaningful.
   * - If this needs to be called, it needs to be called before V8
   *   tries to make use of its built-ins.
   * - To avoid unnecessary copies of data, V8 will point directly into the
   *   given data blob, so pretty please keep it around until V8 exit.
   * - Compression of the startup blob might be useful, but needs to
   *   handled entirely on the embedders' side.
   * - The call will abort if the data is invalid.
   */
  static void SetSnapshotDataBlob(StartupData* startup_blob);

  /** Set the callback to invoke in case of Dcheck failures. */
  static void SetDcheckErrorHandler(DcheckErrorCallback that);


  /**
   * Sets V8 flags from a string.
   */
  static void SetFlagsFromString(const char* str);
  static void SetFlagsFromString(const char* str, size_t length);

  /**
   * Sets V8 flags from the command line.
   */
  static void SetFlagsFromCommandLine(int* argc,
                                      char** argv,
                                      bool remove_flags);

  /** Get the version string. */
  static const char* GetVersion();

  /**
   * Initializes V8. This function needs to be called before the first Isolate
   * is created. It always returns true.
   */
  V8_INLINE static bool Initialize() {
    const int kBuildConfiguration =
        (internal::PointerCompressionIsEnabled() ? kPointerCompression : 0) |
        (internal::SmiValuesAre31Bits() ? k31BitSmis : 0);
    return Initialize(kBuildConfiguration);
  }

  /**
   * Allows the host application to provide a callback which can be used
   * as a source of entropy for random number generators.
   */
  static void SetEntropySource(EntropySource source);

  /**
   * Allows the host application to provide a callback that allows v8 to
   * cooperate with a profiler that rewrites return addresses on stack.
   */
  static void SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver return_address_resolver);

  /**
   * Releases any resources used by v8 and stops any utility threads
   * that may be running.  Note that disposing v8 is permanent, it
   * cannot be reinitialized.
   *
   * It should generally not be necessary to dispose v8 before exiting
   * a process, this should happen automatically.  It is only necessary
   * to use if the process needs the resources taken up by v8.
   */
  static bool Dispose();

  /**
   * Initialize the ICU library bundled with V8. The embedder should only
   * invoke this method when using the bundled ICU. Returns true on success.
   *
   * If V8 was compiled with the ICU data in an external file, the location
   * of the data file has to be provided.
   */
  static bool InitializeICU(const char* icu_data_file = nullptr);

  /**
   * Initialize the ICU library bundled with V8. The embedder should only
   * invoke this method when using the bundled ICU. If V8 was compiled with
   * the ICU data in an external file and when the default location of that
   * file should be used, a path to the executable must be provided.
   * Returns true on success.
   *
   * The default is a file called icudtl.dat side-by-side with the executable.
   *
   * Optionally, the location of the data file can be provided to override the
   * default.
   */
  static bool InitializeICUDefaultLocation(const char* exec_path,
                                           const char* icu_data_file = nullptr);

  /**
   * Initialize the external startup data. The embedder only needs to
   * invoke this method when external startup data was enabled in a build.
   *
   * If V8 was compiled with the startup data in an external file, then
   * V8 needs to be given those external files during startup. There are
   * three ways to do this:
   * - InitializeExternalStartupData(const char*)
   *   This will look in the given directory for the file "snapshot_blob.bin".
   * - InitializeExternalStartupDataFromFile(const char*)
   *   As above, but will directly use the given file name.
   * - Call SetSnapshotDataBlob.
   *   This will read the blobs from the given data structure and will
   *   not perform any file IO.
   */
  static void InitializeExternalStartupData(const char* directory_path);
  static void InitializeExternalStartupDataFromFile(const char* snapshot_blob);

  /**
   * Sets the v8::Platform to use. This should be invoked before V8 is
   * initialized.
   */
  static void InitializePlatform(Platform* platform);

  /**
   * Clears all references to the v8::Platform. This should be invoked after
   * V8 was disposed.
   */
  static void ShutdownPlatform();

#if V8_OS_POSIX
  /**
   * Give the V8 signal handler a chance to handle a fault.
   *
   * This function determines whether a memory access violation can be recovered
   * by V8. If so, it will return true and modify context to return to a code
   * fragment that can recover from the fault. Otherwise, TryHandleSignal will
   * return false.
   *
   * The parameters to this function correspond to those passed to a Linux
   * signal handler.
   *
   * \param signal_number The signal number.
   *
   * \param info A pointer to the siginfo_t structure provided to the signal
   * handler.
   *
   * \param context The third argument passed to the Linux signal handler, which
   * points to a ucontext_t structure.
   */
  V8_DEPRECATE_SOON("Use TryHandleWebAssemblyTrapPosix")
  static bool TryHandleSignal(int signal_number, void* info, void* context);
#endif  // V8_OS_POSIX

  /**
   * Activate trap-based bounds checking for WebAssembly.
   *
   * \param use_v8_signal_handler Whether V8 should install its own signal
   * handler or rely on the embedder's.
   */
  static bool EnableWebAssemblyTrapHandler(bool use_v8_signal_handler);

#if defined(V8_OS_WIN)
  /**
   * On Win64, by default V8 does not emit unwinding data for jitted code,
   * which means the OS cannot walk the stack frames and the system Structured
   * Exception Handling (SEH) cannot unwind through V8-generated code:
   * https://code.google.com/p/v8/issues/detail?id=3598.
   *
   * This function allows embedders to register a custom exception handler for
   * exceptions in V8-generated code.
   */
  static void SetUnhandledExceptionCallback(
      UnhandledExceptionCallback unhandled_exception_callback);
#endif

  /**
   * Get statistics about the shared memory usage.
   */
  static void GetSharedMemoryStatistics(SharedMemoryStatistics* statistics);

 private:
  V8();

  enum BuildConfigurationFeatures {
    kPointerCompression = 1 << 0,
    k31BitSmis = 1 << 1,
  };

  /**
   * Checks that the embedder build configuration is compatible with
   * the V8 binary and if so initializes V8.
   */
  static bool Initialize(int build_config);

  static internal::Address* GlobalizeReference(internal::Isolate* isolate,
                                               internal::Address* handle);
  static internal::Address* GlobalizeTracedReference(internal::Isolate* isolate,
                                                     internal::Address* handle,
                                                     internal::Address* slot,
                                                     bool has_destructor);
  static void MoveGlobalReference(internal::Address** from,
                                  internal::Address** to);
  static void MoveTracedGlobalReference(internal::Address** from,
                                        internal::Address** to);
  static void CopyTracedGlobalReference(const internal::Address* const* from,
                                        internal::Address** to);
  static internal::Address* CopyGlobalReference(internal::Address* from);
  static void DisposeGlobal(internal::Address* global_handle);
  static void DisposeTracedGlobal(internal::Address* global_handle);
  static void MakeWeak(internal::Address* location, void* data,
                       WeakCallbackInfo<void>::Callback weak_callback,
                       WeakCallbackType type);
  static void MakeWeak(internal::Address** location_addr);
  static void* ClearWeak(internal::Address* location);
  static void SetFinalizationCallbackTraced(
      internal::Address* location, void* parameter,
      WeakCallbackInfo<void>::Callback callback);
  static void AnnotateStrongRetainer(internal::Address* location,
                                     const char* label);
  static Value* Eternalize(Isolate* isolate, Value* handle);

  template <class K, class V, class T>
  friend class PersistentValueMapBase;

  static void FromJustIsNothing();
  static void ToLocalEmpty();
  static void InternalFieldOutOfBounds(int index);
  template <class T>
  friend class Global;
  template <class T> friend class Local;
  template <class T>
  friend class MaybeLocal;
  template <class T>
  friend class Maybe;
  template <class T>
  friend class TracedReferenceBase;
  template <class T>
  friend class TracedGlobal;
  template <class T>
  friend class TracedReference;
  template <class T>
  friend class WeakCallbackInfo;
  template <class T> friend class Eternal;
  template <class T> friend class PersistentBase;
  template <class T, class M> friend class Persistent;
  friend class Context;
};

/**
 * Helper class to create a snapshot data blob.
 *
 * The Isolate used by a SnapshotCreator is owned by it, and will be entered
 * and exited by the constructor and destructor, respectively; The destructor
 * will also destroy the Isolate. Experimental language features, including
 * those available by default, are not available while creating a snapshot.
 */
class V8_EXPORT SnapshotCreator {
 public:
  enum class FunctionCodeHandling { kClear, kKeep };

  /**
   * Initialize and enter an isolate, and set it up for serialization.
   * The isolate is either created from scratch or from an existing snapshot.
   * The caller keeps ownership of the argument snapshot.
   * \param existing_blob existing snapshot from which to create this one.
   * \param external_references a null-terminated array of external references
   *        that must be equivalent to CreateParams::external_references.
   */
  SnapshotCreator(Isolate* isolate,
                  const intptr_t* external_references = nullptr,
                  StartupData* existing_blob = nullptr);

  /**
   * Create and enter an isolate, and set it up for serialization.
   * The isolate is either created from scratch or from an existing snapshot.
   * The caller keeps ownership of the argument snapshot.
   * \param existing_blob existing snapshot from which to create this one.
   * \param external_references a null-terminated array of external references
   *        that must be equivalent to CreateParams::external_references.
   */
  SnapshotCreator(const intptr_t* external_references = nullptr,
                  StartupData* existing_blob = nullptr);

  /**
   * Destroy the snapshot creator, and exit and dispose of the Isolate
   * associated with it.
   */
  ~SnapshotCreator();

  /**
   * \returns the isolate prepared by the snapshot creator.
   */
  Isolate* GetIsolate();

  /**
   * Set the default context to be included in the snapshot blob.
   * The snapshot will not contain the global proxy, and we expect one or a
   * global object template to create one, to be provided upon deserialization.
   *
   * \param callback optional callback to serialize internal fields.
   */
  void SetDefaultContext(Local<Context> context,
                         SerializeInternalFieldsCallback callback =
                             SerializeInternalFieldsCallback());

  /**
   * Add additional context to be included in the snapshot blob.
   * The snapshot will include the global proxy.
   *
   * \param callback optional callback to serialize internal fields.
   *
   * \returns the index of the context in the snapshot blob.
   */
  size_t AddContext(Local<Context> context,
                    SerializeInternalFieldsCallback callback =
                        SerializeInternalFieldsCallback());

  /**
   * Attach arbitrary V8::Data to the context snapshot, which can be retrieved
   * via Context::GetDataFromSnapshot after deserialization. This data does not
   * survive when a new snapshot is created from an existing snapshot.
   * \returns the index for retrieval.
   */
  template <class T>
  V8_INLINE size_t AddData(Local<Context> context, Local<T> object);

  /**
   * Attach arbitrary V8::Data to the isolate snapshot, which can be retrieved
   * via Isolate::GetDataFromSnapshot after deserialization. This data does not
   * survive when a new snapshot is created from an existing snapshot.
   * \returns the index for retrieval.
   */
  template <class T>
  V8_INLINE size_t AddData(Local<T> object);

  /**
   * Created a snapshot data blob.
   * This must not be called from within a handle scope.
   * \param function_code_handling whether to include compiled function code
   *        in the snapshot.
   * \returns { nullptr, 0 } on failure, and a startup snapshot on success. The
   *        caller acquires ownership of the data array in the return value.
   */
  StartupData CreateBlob(FunctionCodeHandling function_code_handling);

  // Disallow copying and assigning.
  SnapshotCreator(const SnapshotCreator&) = delete;
  void operator=(const SnapshotCreator&) = delete;

 private:
  size_t AddData(Local<Context> context, internal::Address object);
  size_t AddData(internal::Address object);

  void* data_;
};

/**
 * A simple Maybe type, representing an object which may or may not have a
 * value, see https://hackage.haskell.org/package/base/docs/Data-Maybe.html.
 *
 * If an API method returns a Maybe<>, the API method can potentially fail
 * either because an exception is thrown, or because an exception is pending,
 * e.g. because a previous API call threw an exception that hasn't been caught
 * yet, or because a TerminateExecution exception was thrown. In that case, a
 * "Nothing" value is returned.
 */
template <class T>
class Maybe {
 public:
  V8_INLINE bool IsNothing() const { return !has_value_; }
  V8_INLINE bool IsJust() const { return has_value_; }

  /**
   * An alias for |FromJust|. Will crash if the Maybe<> is nothing.
   */
  V8_INLINE T ToChecked() const { return FromJust(); }

  /**
   * Short-hand for ToChecked(), which doesn't return a value. To be used, where
   * the actual value of the Maybe is not needed like Object::Set.
   */
  V8_INLINE void Check() const {
    if (V8_UNLIKELY(!IsJust())) V8::FromJustIsNothing();
  }

  /**
   * Converts this Maybe<> to a value of type T. If this Maybe<> is
   * nothing (empty), |false| is returned and |out| is left untouched.
   */
  V8_WARN_UNUSED_RESULT V8_INLINE bool To(T* out) const {
    if (V8_LIKELY(IsJust())) *out = value_;
    return IsJust();
  }

  /**
   * Converts this Maybe<> to a value of type T. If this Maybe<> is
   * nothing (empty), V8 will crash the process.
   */
  V8_INLINE T FromJust() const {
    if (V8_UNLIKELY(!IsJust())) V8::FromJustIsNothing();
    return value_;
  }

  /**
   * Converts this Maybe<> to a value of type T, using a default value if this
   * Maybe<> is nothing (empty).
   */
  V8_INLINE T FromMaybe(const T& default_value) const {
    return has_value_ ? value_ : default_value;
  }

  V8_INLINE bool operator==(const Maybe& other) const {
    return (IsJust() == other.IsJust()) &&
           (!IsJust() || FromJust() == other.FromJust());
  }

  V8_INLINE bool operator!=(const Maybe& other) const {
    return !operator==(other);
  }

 private:
  Maybe() : has_value_(false) {}
  explicit Maybe(const T& t) : has_value_(true), value_(t) {}

  bool has_value_;
  T value_;

  template <class U>
  friend Maybe<U> Nothing();
  template <class U>
  friend Maybe<U> Just(const U& u);
};

template <class T>
inline Maybe<T> Nothing() {
  return Maybe<T>();
}

template <class T>
inline Maybe<T> Just(const T& t) {
  return Maybe<T>(t);
}

// A template specialization of Maybe<T> for the case of T = void.
template <>
class Maybe<void> {
 public:
  V8_INLINE bool IsNothing() const { return !is_valid_; }
  V8_INLINE bool IsJust() const { return is_valid_; }

  V8_INLINE bool operator==(const Maybe& other) const {
    return IsJust() == other.IsJust();
  }

  V8_INLINE bool operator!=(const Maybe& other) const {
    return !operator==(other);
  }

 private:
  struct JustTag {};

  Maybe() : is_valid_(false) {}
  explicit Maybe(JustTag) : is_valid_(true) {}

  bool is_valid_;

  template <class U>
  friend Maybe<U> Nothing();
  friend Maybe<void> JustVoid();
};

inline Maybe<void> JustVoid() { return Maybe<void>(Maybe<void>::JustTag()); }

/**
 * An external exception handler.
 */
class V8_EXPORT TryCatch {
 public:
  /**
   * Creates a new try/catch block and registers it with v8.  Note that
   * all TryCatch blocks should be stack allocated because the memory
   * location itself is compared against JavaScript try/catch blocks.
   */
  explicit TryCatch(Isolate* isolate);

  /**
   * Unregisters and deletes this try/catch block.
   */
  ~TryCatch();

  /**
   * Returns true if an exception has been caught by this try/catch block.
   */
  bool HasCaught() const;

  /**
   * For certain types of exceptions, it makes no sense to continue execution.
   *
   * If CanContinue returns false, the correct action is to perform any C++
   * cleanup needed and then return.  If CanContinue returns false and
   * HasTerminated returns true, it is possible to call
   * CancelTerminateExecution in order to continue calling into the engine.
   */
  bool CanContinue() const;

  /**
   * Returns true if an exception has been caught due to script execution
   * being terminated.
   *
   * There is no JavaScript representation of an execution termination
   * exception.  Such exceptions are thrown when the TerminateExecution
   * methods are called to terminate a long-running script.
   *
   * If such an exception has been thrown, HasTerminated will return true,
   * indicating that it is possible to call CancelTerminateExecution in order
   * to continue calling into the engine.
   */
  bool HasTerminated() const;

  /**
   * Throws the exception caught by this TryCatch in a way that avoids
   * it being caught again by this same TryCatch.  As with ThrowException
   * it is illegal to execute any JavaScript operations after calling
   * ReThrow; the caller must return immediately to where the exception
   * is caught.
   */
  Local<Value> ReThrow();

  /**
   * Returns the exception caught by this try/catch block.  If no exception has
   * been caught an empty handle is returned.
   *
   * The returned handle is valid until this TryCatch block has been destroyed.
   */
  Local<Value> Exception() const;

  /**
   * Returns the .stack property of an object.  If no .stack
   * property is present an empty handle is returned.
   */
  V8_WARN_UNUSED_RESULT static MaybeLocal<Value> StackTrace(
      Local<Context> context, Local<Value> exception);

  /**
   * Returns the .stack property of the thrown object.  If no .stack property is
   * present or if this try/catch block has not caught an exception, an empty
   * handle is returned.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> StackTrace(
      Local<Context> context) const;

  /**
   * Returns the message associated with this exception.  If there is
   * no message associated an empty handle is returned.
   *
   * The returned handle is valid until this TryCatch block has been
   * destroyed.
   */
  Local<v8::Message> Message() const;

  /**
   * Clears any exceptions that may have been caught by this try/catch block.
   * After this method has been called, HasCaught() will return false. Cancels
   * the scheduled exception if it is caught and ReThrow() is not called before.
   *
   * It is not necessary to clear a try/catch block before using it again; if
   * another exception is thrown the previously caught exception will just be
   * overwritten.  However, it is often a good idea since it makes it easier
   * to determine which operation threw a given exception.
   */
  void Reset();

  /**
   * Set verbosity of the external exception handler.
   *
   * By default, exceptions that are caught by an external exception
   * handler are not reported.  Call SetVerbose with true on an
   * external exception handler to have exceptions caught by the
   * handler reported as if they were not caught.
   */
  void SetVerbose(bool value);

  /**
   * Returns true if verbosity is enabled.
   */
  bool IsVerbose() const;

  /**
   * Set whether or not this TryCatch should capture a Message object
   * which holds source information about where the exception
   * occurred.  True by default.
   */
  void SetCaptureMessage(bool value);

  /**
   * There are cases when the raw address of C++ TryCatch object cannot be
   * used for comparisons with addresses into the JS stack. The cases are:
   * 1) ARM, ARM64 and MIPS simulators which have separate JS stack.
   * 2) Address sanitizer allocates local C++ object in the heap when
   *    UseAfterReturn mode is enabled.
   * This method returns address that can be used for comparisons with
   * addresses into the JS stack. When neither simulator nor ASAN's
   * UseAfterReturn is enabled, then the address returned will be the address
   * of the C++ try catch handler itself.
   */
  static void* JSStackComparableAddress(TryCatch* handler) {
    if (handler == nullptr) return nullptr;
    return handler->js_stack_comparable_address_;
  }

  TryCatch(const TryCatch&) = delete;
  void operator=(const TryCatch&) = delete;

 private:
  // Declaring operator new and delete as deleted is not spec compliant.
  // Therefore declare them private instead to disable dynamic alloc
  void* operator new(size_t size);
  void* operator new[](size_t size);
  void operator delete(void*, size_t);
  void operator delete[](void*, size_t);

  void ResetInternal();

  internal::Isolate* isolate_;
  TryCatch* next_;
  void* exception_;
  void* message_obj_;
  void* js_stack_comparable_address_;
  bool is_verbose_ : 1;
  bool can_continue_ : 1;
  bool capture_message_ : 1;
  bool rethrow_ : 1;
  bool has_terminated_ : 1;

  friend class internal::Isolate;
};


// --- Context ---


/**
 * A container for extension names.
 */
class V8_EXPORT ExtensionConfiguration {
 public:
  ExtensionConfiguration() : name_count_(0), names_(nullptr) {}
  ExtensionConfiguration(int name_count, const char* names[])
      : name_count_(name_count), names_(names) { }

  const char** begin() const { return &names_[0]; }
  const char** end()  const { return &names_[name_count_]; }

 private:
  const int name_count_;
  const char** names_;
};

/**
 * A sandboxed execution context with its own set of built-in objects
 * and functions.
 */
class V8_EXPORT Context {
 public:
  /**
   * Returns the global proxy object.
   *
   * Global proxy object is a thin wrapper whose prototype points to actual
   * context's global object with the properties like Object, etc. This is done
   * that way for security reasons (for more details see
   * https://wiki.mozilla.org/Gecko:SplitWindow).
   *
   * Please note that changes to global proxy object prototype most probably
   * would break VM---v8 expects only global object as a prototype of global
   * proxy object.
   */
  Local<Object> Global();

  /**
   * Detaches the global object from its context before
   * the global object can be reused to create a new context.
   */
  void DetachGlobal();

  /**
   * Creates a new context and returns a handle to the newly allocated
   * context.
   *
   * \param isolate The isolate in which to create the context.
   *
   * \param extensions An optional extension configuration containing
   * the extensions to be installed in the newly created context.
   *
   * \param global_template An optional object template from which the
   * global object for the newly created context will be created.
   *
   * \param global_object An optional global object to be reused for
   * the newly created context. This global object must have been
   * created by a previous call to Context::New with the same global
   * template. The state of the global object will be completely reset
   * and only object identify will remain.
   */
  static Local<Context> New(
      Isolate* isolate, ExtensionConfiguration* extensions = nullptr,
      MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
      MaybeLocal<Value> global_object = MaybeLocal<Value>(),
      DeserializeInternalFieldsCallback internal_fields_deserializer =
          DeserializeInternalFieldsCallback(),
      MicrotaskQueue* microtask_queue = nullptr);

  /**
   * Create a new context from a (non-default) context snapshot. There
   * is no way to provide a global object template since we do not create
   * a new global object from template, but we can reuse a global object.
   *
   * \param isolate See v8::Context::New.
   *
   * \param context_snapshot_index The index of the context snapshot to
   * deserialize from. Use v8::Context::New for the default snapshot.
   *
   * \param embedder_fields_deserializer Optional callback to deserialize
   * internal fields. It should match the SerializeInternalFieldCallback used
   * to serialize.
   *
   * \param extensions See v8::Context::New.
   *
   * \param global_object See v8::Context::New.
   */
  static MaybeLocal<Context> FromSnapshot(
      Isolate* isolate, size_t context_snapshot_index,
      DeserializeInternalFieldsCallback embedder_fields_deserializer =
          DeserializeInternalFieldsCallback(),
      ExtensionConfiguration* extensions = nullptr,
      MaybeLocal<Value> global_object = MaybeLocal<Value>(),
      MicrotaskQueue* microtask_queue = nullptr);

  /**
   * Returns an global object that isn't backed by an actual context.
   *
   * The global template needs to have access checks with handlers installed.
   * If an existing global object is passed in, the global object is detached
   * from its context.
   *
   * Note that this is different from a detached context where all accesses to
   * the global proxy will fail. Instead, the access check handlers are invoked.
   *
   * It is also not possible to detach an object returned by this method.
   * Instead, the access check handlers need to return nothing to achieve the
   * same effect.
   *
   * It is possible, however, to create a new context from the global object
   * returned by this method.
   */
  static MaybeLocal<Object> NewRemoteContext(
      Isolate* isolate, Local<ObjectTemplate> global_template,
      MaybeLocal<Value> global_object = MaybeLocal<Value>());

  /**
   * Sets the security token for the context.  To access an object in
   * another context, the security tokens must match.
   */
  void SetSecurityToken(Local<Value> token);

  /** Restores the security token to the default value. */
  void UseDefaultSecurityToken();

  /** Returns the security token of this context.*/
  Local<Value> GetSecurityToken();

  /**
   * Enter this context.  After entering a context, all code compiled
   * and run is compiled and run in this context.  If another context
   * is already entered, this old context is saved so it can be
   * restored when the new context is exited.
   */
  void Enter();

  /**
   * Exit this context.  Exiting the current context restores the
   * context that was in place when entering the current context.
   */
  void Exit();

  /** Returns an isolate associated with a current context. */
  Isolate* GetIsolate();

  /**
   * The field at kDebugIdIndex used to be reserved for the inspector.
   * It now serves no purpose.
   */
  enum EmbedderDataFields { kDebugIdIndex = 0 };

  /**
   * Return the number of fields allocated for embedder data.
   */
  uint32_t GetNumberOfEmbedderDataFields();

  /**
   * Gets the embedder data with the given index, which must have been set by a
   * previous call to SetEmbedderData with the same index.
   */
  V8_INLINE Local<Value> GetEmbedderData(int index);

  /**
   * Gets the binding object used by V8 extras. Extra natives get a reference
   * to this object and can use it to "export" functionality by adding
   * properties. Extra natives can also "import" functionality by accessing
   * properties added by the embedder using the V8 API.
   */
  Local<Object> GetExtrasBindingObject();

  /**
   * Sets the embedder data with the given index, growing the data as
   * needed. Note that index 0 currently has a special meaning for Chrome's
   * debugger.
   */
  void SetEmbedderData(int index, Local<Value> value);

  /**
   * Gets a 2-byte-aligned native pointer from the embedder data with the given
   * index, which must have been set by a previous call to
   * SetAlignedPointerInEmbedderData with the same index. Note that index 0
   * currently has a special meaning for Chrome's debugger.
   */
  V8_INLINE void* GetAlignedPointerFromEmbedderData(int index);

  /**
   * Sets a 2-byte-aligned native pointer in the embedder data with the given
   * index, growing the data as needed. Note that index 0 currently has a
   * special meaning for Chrome's debugger.
   */
  void SetAlignedPointerInEmbedderData(int index, void* value);

  /**
   * Control whether code generation from strings is allowed. Calling
   * this method with false will disable 'eval' and the 'Function'
   * constructor for code running in this context. If 'eval' or the
   * 'Function' constructor are used an exception will be thrown.
   *
   * If code generation from strings is not allowed the
   * V8::AllowCodeGenerationFromStrings callback will be invoked if
   * set before blocking the call to 'eval' or the 'Function'
   * constructor. If that callback returns true, the call will be
   * allowed, otherwise an exception will be thrown. If no callback is
   * set an exception will be thrown.
   */
  void AllowCodeGenerationFromStrings(bool allow);

  /**
   * Returns true if code generation from strings is allowed for the context.
   * For more details see AllowCodeGenerationFromStrings(bool) documentation.
   */
  bool IsCodeGenerationFromStringsAllowed();

  /**
   * Sets the error description for the exception that is thrown when
   * code generation from strings is not allowed and 'eval' or the 'Function'
   * constructor are called.
   */
  void SetErrorMessageForCodeGenerationFromStrings(Local<String> message);

  /**
   * Return data that was previously attached to the context snapshot via
   * SnapshotCreator, and removes the reference to it.
   * Repeated call with the same index returns an empty MaybeLocal.
   */
  template <class T>
  V8_INLINE MaybeLocal<T> GetDataFromSnapshotOnce(size_t index);

  /**
   * If callback is set, abort any attempt to execute JavaScript in this
   * context, call the specified callback, and throw an exception.
   * To unset abort, pass nullptr as callback.
   */
  typedef void (*AbortScriptExecutionCallback)(Isolate* isolate,
                                               Local<Context> context);
  void SetAbortScriptExecution(AbortScriptExecutionCallback callback);

  /**
   * Returns the value that was set or restored by
   * SetContinuationPreservedEmbedderData(), if any.
   */
  Local<Value> GetContinuationPreservedEmbedderData() const;

  /**
   * Sets a value that will be stored on continuations and reset while the
   * continuation runs.
   */
  void SetContinuationPreservedEmbedderData(Local<Value> context);

  /**
   * Stack-allocated class which sets the execution context for all
   * operations executed within a local scope.
   */
  class Scope {
   public:
    explicit V8_INLINE Scope(Local<Context> context) : context_(context) {
      context_->Enter();
    }
    V8_INLINE ~Scope() { context_->Exit(); }

   private:
    Local<Context> context_;
  };

  /**
   * Stack-allocated class to support the backup incumbent settings object
   * stack.
   * https://html.spec.whatwg.org/multipage/webappapis.html#backup-incumbent-settings-object-stack
   */
  class V8_EXPORT BackupIncumbentScope final {
   public:
    /**
     * |backup_incumbent_context| is pushed onto the backup incumbent settings
     * object stack.
     */
    explicit BackupIncumbentScope(Local<Context> backup_incumbent_context);
    ~BackupIncumbentScope();

    /**
     * Returns address that is comparable with JS stack address.  Note that JS
     * stack may be allocated separately from the native stack.  See also
     * |TryCatch::JSStackComparableAddress| for details.
     */
    uintptr_t JSStackComparableAddress() const {
      return js_stack_comparable_address_;
    }

   private:
    friend class internal::Isolate;

    Local<Context> backup_incumbent_context_;
    uintptr_t js_stack_comparable_address_ = 0;
    const BackupIncumbentScope* prev_ = nullptr;
  };

 private:
  friend class Value;
  friend class Script;
  friend class Object;
  friend class Function;

  internal::Address* GetDataFromSnapshotOnce(size_t index);
  Local<Value> SlowGetEmbedderData(int index);
  void* SlowGetAlignedPointerFromEmbedderData(int index);
};


/**
 * Multiple threads in V8 are allowed, but only one thread at a time is allowed
 * to use any given V8 isolate, see the comments in the Isolate class. The
 * definition of 'using a V8 isolate' includes accessing handles or holding onto
 * object pointers obtained from V8 handles while in the particular V8 isolate.
 * It is up to the user of V8 to ensure, perhaps with locking, that this
 * constraint is not violated. In addition to any other synchronization
 * mechanism that may be used, the v8::Locker and v8::Unlocker classes must be
 * used to signal thread switches to V8.
 *
 * v8::Locker is a scoped lock object. While it's active, i.e. between its
 * construction and destruction, the current thread is allowed to use the locked
 * isolate. V8 guarantees that an isolate can be locked by at most one thread at
 * any time. In other words, the scope of a v8::Locker is a critical section.
 *
 * Sample usage:
* \code
 * ...
 * {
 *   v8::Locker locker(isolate);
 *   v8::Isolate::Scope isolate_scope(isolate);
 *   ...
 *   // Code using V8 and isolate goes here.
 *   ...
 * } // Destructor called here
 * \endcode
 *
 * If you wish to stop using V8 in a thread A you can do this either by
 * destroying the v8::Locker object as above or by constructing a v8::Unlocker
 * object:
 *
 * \code
 * {
 *   isolate->Exit();
 *   v8::Unlocker unlocker(isolate);
 *   ...
 *   // Code not using V8 goes here while V8 can run in another thread.
 *   ...
 * } // Destructor called here.
 * isolate->Enter();
 * \endcode
 *
 * The Unlocker object is intended for use in a long-running callback from V8,
 * where you want to release the V8 lock for other threads to use.
 *
 * The v8::Locker is a recursive lock, i.e. you can lock more than once in a
 * given thread. This can be useful if you have code that can be called either
 * from code that holds the lock or from code that does not. The Unlocker is
 * not recursive so you can not have several Unlockers on the stack at once, and
 * you can not use an Unlocker in a thread that is not inside a Locker's scope.
 *
 * An unlocker will unlock several lockers if it has to and reinstate the
 * correct depth of locking on its destruction, e.g.:
 *
 * \code
 * // V8 not locked.
 * {
 *   v8::Locker locker(isolate);
 *   Isolate::Scope isolate_scope(isolate);
 *   // V8 locked.
 *   {
 *     v8::Locker another_locker(isolate);
 *     // V8 still locked (2 levels).
 *     {
 *       isolate->Exit();
 *       v8::Unlocker unlocker(isolate);
 *       // V8 not locked.
 *     }
 *     isolate->Enter();
 *     // V8 locked again (2 levels).
 *   }
 *   // V8 still locked (1 level).
 * }
 * // V8 Now no longer locked.
 * \endcode
 */
class V8_EXPORT Unlocker {
 public:
  /**
   * Initialize Unlocker for a given Isolate.
   */
  V8_INLINE explicit Unlocker(Isolate* isolate) { Initialize(isolate); }

  ~Unlocker();
 private:
  void Initialize(Isolate* isolate);

  internal::Isolate* isolate_;
};


class V8_EXPORT Locker {
 public:
  /**
   * Initialize Locker for a given Isolate.
   */
  V8_INLINE explicit Locker(Isolate* isolate) { Initialize(isolate); }

  ~Locker();

  /**
   * Returns whether or not the locker for a given isolate, is locked by the
   * current thread.
   */
  static bool IsLocked(Isolate* isolate);

  /**
   * Returns whether v8::Locker is being used by this V8 instance.
   */
  static bool IsActive();

  // Disallow copying and assigning.
  Locker(const Locker&) = delete;
  void operator=(const Locker&) = delete;

 private:
  void Initialize(Isolate* isolate);

  bool has_lock_;
  bool top_level_;
  internal::Isolate* isolate_;
};

/**
 * Various helpers for skipping over V8 frames in a given stack.
 *
 * The unwinder API is only supported on the x64, ARM64 and ARM32 architectures.
 */
class V8_EXPORT Unwinder {
 public:
  /**
   * Attempt to unwind the stack to the most recent C++ frame. This function is
   * signal-safe and does not access any V8 state and thus doesn't require an
   * Isolate.
   *
   * The unwinder needs to know the location of the JS Entry Stub (a piece of
   * code that is run when C++ code calls into generated JS code). This is used
   * for edge cases where the current frame is being constructed or torn down
   * when the stack sample occurs.
   *
   * The unwinder also needs the virtual memory range of all possible V8 code
   * objects. There are two ranges required - the heap code range and the range
   * for code embedded in the binary. The V8 API provides all required inputs
   * via an UnwindState object through the Isolate::GetUnwindState() API. These
   * values will not change after Isolate initialization, so the same
   * |unwind_state| can be used for multiple calls.
   *
   * \param unwind_state Input state for the Isolate that the stack comes from.
   * \param register_state The current registers. This is an in-out param that
   * will be overwritten with the register values after unwinding, on success.
   * \param stack_base The resulting stack pointer and frame pointer values are
   * bounds-checked against the stack_base and the original stack pointer value
   * to ensure that they are valid locations in the given stack. If these values
   * or any intermediate frame pointer values used during unwinding are ever out
   * of these bounds, unwinding will fail.
   *
   * \return True on success.
   */
  // TODO(petermarshall): Remove this API
  V8_DEPRECATED("Use entry_stubs + code_pages version.")
  static bool TryUnwindV8Frames(const UnwindState& unwind_state,
                                RegisterState* register_state,
                                const void* stack_base);

  /**
   * The same as above, but is available on x64, ARM64 and ARM32.
   *
   * \param code_pages A list of all of the ranges in which V8 has allocated
   * executable code. The caller should obtain this list by calling
   * Isolate::CopyCodePages() during the same interrupt/thread suspension that
   * captures the stack.
   */
  static bool TryUnwindV8Frames(const JSEntryStubs& entry_stubs,
                                size_t code_pages_length,
                                const MemoryRange* code_pages,
                                RegisterState* register_state,
                                const void* stack_base);

  /**
   * Whether the PC is within the V8 code range represented by code_range or
   * embedded_code_range in |unwind_state|.
   *
   * If this returns false, then calling UnwindV8Frames() with the same PC
   * and unwind_state will always fail. If it returns true, then unwinding may
   * (but not necessarily) be successful.
   */
  // TODO(petermarshall): Remove this API
  V8_DEPRECATED("Use code_pages version.")
  static bool PCIsInV8(const UnwindState& unwind_state, void* pc);

  /**
   * The same as above, but is available on x64, ARM64 and ARM32. See the
   * comment on TryUnwindV8Frames.
   */
  static bool PCIsInV8(size_t code_pages_length, const MemoryRange* code_pages,
                       void* pc);
};

// --- Implementation ---

template <class T>
Local<T> Local<T>::New(Isolate* isolate, Local<T> that) {
  return New(isolate, that.val_);
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, const PersistentBase<T>& that) {
  return New(isolate, that.val_);
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, const TracedReferenceBase<T>& that) {
  return New(isolate, that.val_);
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, T* that) {
  if (that == nullptr) return Local<T>();
  T* that_ptr = that;
  internal::Address* p = reinterpret_cast<internal::Address*>(that_ptr);
  return Local<T>(reinterpret_cast<T*>(HandleScope::CreateHandle(
      reinterpret_cast<internal::Isolate*>(isolate), *p)));
}


template<class T>
template<class S>
void Eternal<T>::Set(Isolate* isolate, Local<S> handle) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  val_ = reinterpret_cast<T*>(
      V8::Eternalize(isolate, reinterpret_cast<Value*>(*handle)));
}

template <class T>
Local<T> Eternal<T>::Get(Isolate* isolate) const {
  // The eternal handle will never go away, so as with the roots, we don't even
  // need to open a handle.
  return Local<T>(val_);
}


template <class T>
Local<T> MaybeLocal<T>::ToLocalChecked() {
  if (V8_UNLIKELY(val_ == nullptr)) V8::ToLocalEmpty();
  return Local<T>(val_);
}


template <class T>
void* WeakCallbackInfo<T>::GetInternalField(int index) const {
#ifdef V8_ENABLE_CHECKS
  if (index < 0 || index >= kEmbedderFieldsInWeakCallback) {
    V8::InternalFieldOutOfBounds(index);
  }
#endif
  return embedder_fields_[index];
}


template <class T>
T* PersistentBase<T>::New(Isolate* isolate, T* that) {
  if (that == nullptr) return nullptr;
  internal::Address* p = reinterpret_cast<internal::Address*>(that);
  return reinterpret_cast<T*>(
      V8::GlobalizeReference(reinterpret_cast<internal::Isolate*>(isolate),
                             p));
}


template <class T, class M>
template <class S, class M2>
void Persistent<T, M>::Copy(const Persistent<S, M2>& that) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  this->Reset();
  if (that.IsEmpty()) return;
  internal::Address* p = reinterpret_cast<internal::Address*>(that.val_);
  this->val_ = reinterpret_cast<T*>(V8::CopyGlobalReference(p));
  M::Copy(that, this);
}

template <class T>
bool PersistentBase<T>::IsWeak() const {
  typedef internal::Internals I;
  if (this->IsEmpty()) return false;
  return I::GetNodeState(reinterpret_cast<internal::Address*>(this->val_)) ==
         I::kNodeStateIsWeakValue;
}


template <class T>
void PersistentBase<T>::Reset() {
  if (this->IsEmpty()) return;
  V8::DisposeGlobal(reinterpret_cast<internal::Address*>(this->val_));
  val_ = nullptr;
}


template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate, const Local<S>& other) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  Reset();
  if (other.IsEmpty()) return;
  this->val_ = New(isolate, other.val_);
}


template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate,
                              const PersistentBase<S>& other) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  Reset();
  if (other.IsEmpty()) return;
  this->val_ = New(isolate, other.val_);
}


template <class T>
template <typename P>
V8_INLINE void PersistentBase<T>::SetWeak(
    P* parameter, typename WeakCallbackInfo<P>::Callback callback,
    WeakCallbackType type) {
  typedef typename WeakCallbackInfo<void>::Callback Callback;
  V8::MakeWeak(reinterpret_cast<internal::Address*>(this->val_), parameter,
               reinterpret_cast<Callback>(callback), type);
}

template <class T>
void PersistentBase<T>::SetWeak() {
  V8::MakeWeak(reinterpret_cast<internal::Address**>(&this->val_));
}

template <class T>
template <typename P>
P* PersistentBase<T>::ClearWeak() {
  return reinterpret_cast<P*>(
      V8::ClearWeak(reinterpret_cast<internal::Address*>(this->val_)));
}

template <class T>
void PersistentBase<T>::AnnotateStrongRetainer(const char* label) {
  V8::AnnotateStrongRetainer(reinterpret_cast<internal::Address*>(this->val_),
                             label);
}

template <class T>
void PersistentBase<T>::SetWrapperClassId(uint16_t class_id) {
  typedef internal::Internals I;
  if (this->IsEmpty()) return;
  internal::Address* obj = reinterpret_cast<internal::Address*>(this->val_);
  uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + I::kNodeClassIdOffset;
  *reinterpret_cast<uint16_t*>(addr) = class_id;
}


template <class T>
uint16_t PersistentBase<T>::WrapperClassId() const {
  typedef internal::Internals I;
  if (this->IsEmpty()) return 0;
  internal::Address* obj = reinterpret_cast<internal::Address*>(this->val_);
  uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + I::kNodeClassIdOffset;
  return *reinterpret_cast<uint16_t*>(addr);
}

template <class T>
Global<T>::Global(Global&& other) : PersistentBase<T>(other.val_) {
  if (other.val_ != nullptr) {
    V8::MoveGlobalReference(reinterpret_cast<internal::Address**>(&other.val_),
                            reinterpret_cast<internal::Address**>(&this->val_));
    other.val_ = nullptr;
  }
}

template <class T>
template <class S>
Global<T>& Global<T>::operator=(Global<S>&& rhs) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  if (this != &rhs) {
    this->Reset();
    if (rhs.val_ != nullptr) {
      this->val_ = rhs.val_;
      V8::MoveGlobalReference(
          reinterpret_cast<internal::Address**>(&rhs.val_),
          reinterpret_cast<internal::Address**>(&this->val_));
      rhs.val_ = nullptr;
    }
  }
  return *this;
}

template <class T>
T* TracedReferenceBase<T>::New(Isolate* isolate, T* that, void* slot,
                               DestructionMode destruction_mode) {
  if (that == nullptr) return nullptr;
  internal::Address* p = reinterpret_cast<internal::Address*>(that);
  return reinterpret_cast<T*>(V8::GlobalizeTracedReference(
      reinterpret_cast<internal::Isolate*>(isolate), p,
      reinterpret_cast<internal::Address*>(slot),
      destruction_mode == kWithDestructor));
}

template <class T>
void TracedReferenceBase<T>::Reset() {
  if (IsEmpty()) return;
  V8::DisposeTracedGlobal(reinterpret_cast<internal::Address*>(val_));
  val_ = nullptr;
}

template <class T>
template <class S>
void TracedGlobal<T>::Reset(Isolate* isolate, const Local<S>& other) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  Reset();
  if (other.IsEmpty()) return;
  this->val_ = this->New(isolate, other.val_, &this->val_,
                         TracedReferenceBase<T>::kWithDestructor);
}

template <class T>
template <class S>
TracedGlobal<T>& TracedGlobal<T>::operator=(TracedGlobal<S>&& rhs) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  *this = std::move(rhs.template As<T>());
  return *this;
}

template <class T>
template <class S>
TracedGlobal<T>& TracedGlobal<T>::operator=(const TracedGlobal<S>& rhs) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  *this = rhs.template As<T>();
  return *this;
}

template <class T>
TracedGlobal<T>& TracedGlobal<T>::operator=(TracedGlobal&& rhs) {
  if (this != &rhs) {
      V8::MoveTracedGlobalReference(
          reinterpret_cast<internal::Address**>(&rhs.val_),
          reinterpret_cast<internal::Address**>(&this->val_));
  }
  return *this;
}

template <class T>
TracedGlobal<T>& TracedGlobal<T>::operator=(const TracedGlobal& rhs) {
  if (this != &rhs) {
    this->Reset();
    if (rhs.val_ != nullptr) {
      V8::CopyTracedGlobalReference(
          reinterpret_cast<const internal::Address* const*>(&rhs.val_),
          reinterpret_cast<internal::Address**>(&this->val_));
    }
  }
  return *this;
}

template <class T>
template <class S>
void TracedReference<T>::Reset(Isolate* isolate, const Local<S>& other) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  Reset();
  if (other.IsEmpty()) return;
  this->val_ = this->New(isolate, other.val_, &this->val_,
                         TracedReferenceBase<T>::kWithoutDestructor);
}

template <class T>
template <class S>
TracedReference<T>& TracedReference<T>::operator=(TracedReference<S>&& rhs) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  *this = std::move(rhs.template As<T>());
  return *this;
}

template <class T>
template <class S>
TracedReference<T>& TracedReference<T>::operator=(
    const TracedReference<S>& rhs) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  *this = rhs.template As<T>();
  return *this;
}

template <class T>
TracedReference<T>& TracedReference<T>::operator=(TracedReference&& rhs) {
  if (this != &rhs) {
    V8::MoveTracedGlobalReference(
        reinterpret_cast<internal::Address**>(&rhs.val_),
        reinterpret_cast<internal::Address**>(&this->val_));
  }
  return *this;
}

template <class T>
TracedReference<T>& TracedReference<T>::operator=(const TracedReference& rhs) {
  if (this != &rhs) {
    this->Reset();
    if (rhs.val_ != nullptr) {
      V8::CopyTracedGlobalReference(
          reinterpret_cast<const internal::Address* const*>(&rhs.val_),
          reinterpret_cast<internal::Address**>(&this->val_));
    }
  }
  return *this;
}

template <class T>
void TracedReferenceBase<T>::SetWrapperClassId(uint16_t class_id) {
  typedef internal::Internals I;
  if (IsEmpty()) return;
  internal::Address* obj = reinterpret_cast<internal::Address*>(val_);
  uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + I::kNodeClassIdOffset;
  *reinterpret_cast<uint16_t*>(addr) = class_id;
}

template <class T>
uint16_t TracedReferenceBase<T>::WrapperClassId() const {
  typedef internal::Internals I;
  if (IsEmpty()) return 0;
  internal::Address* obj = reinterpret_cast<internal::Address*>(val_);
  uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + I::kNodeClassIdOffset;
  return *reinterpret_cast<uint16_t*>(addr);
}

template <class T>
void TracedGlobal<T>::SetFinalizationCallback(
    void* parameter, typename WeakCallbackInfo<void>::Callback callback) {
  V8::SetFinalizationCallbackTraced(
      reinterpret_cast<internal::Address*>(this->val_), parameter, callback);
}

template <typename T>
ReturnValue<T>::ReturnValue(internal::Address* slot) : value_(slot) {}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Global<S>& handle) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Address*>(*handle);
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const TracedReferenceBase<S>& handle) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Address*>(handle.val_);
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Local<S> handle) {
  static_assert(std::is_void<T>::value || std::is_base_of<T, S>::value,
                "type check");
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Address*>(*handle);
  }
}

template<typename T>
void ReturnValue<T>::Set(double i) {
  static_assert(std::is_base_of<T, Number>::value, "type check");
  Set(Number::New(GetIsolate(), i));
}

template<typename T>
void ReturnValue<T>::Set(int32_t i) {
  static_assert(std::is_base_of<T, Integer>::value, "type check");
  typedef internal::Internals I;
  if (V8_LIKELY(I::IsValidSmi(i))) {
    *value_ = I::IntToSmi(i);
    return;
  }
  Set(Integer::New(GetIsolate(), i));
}

template<typename T>
void ReturnValue<T>::Set(uint32_t i) {
  static_assert(std::is_base_of<T, Integer>::value, "type check");
  // Can't simply use INT32_MAX here for whatever reason.
  bool fits_into_int32_t = (i & (1U << 31)) == 0;
  if (V8_LIKELY(fits_into_int32_t)) {
    Set(static_cast<int32_t>(i));
    return;
  }
  Set(Integer::NewFromUnsigned(GetIsolate(), i));
}

template<typename T>
void ReturnValue<T>::Set(bool value) {
  static_assert(std::is_base_of<T, Boolean>::value, "type check");
  typedef internal::Internals I;
  int root_index;
  if (value) {
    root_index = I::kTrueValueRootIndex;
  } else {
    root_index = I::kFalseValueRootIndex;
  }
  *value_ = *I::GetRoot(GetIsolate(), root_index);
}

template<typename T>
void ReturnValue<T>::SetNull() {
  static_assert(std::is_base_of<T, Primitive>::value, "type check");
  typedef internal::Internals I;
  *value_ = *I::GetRoot(GetIsolate(), I::kNullValueRootIndex);
}

template<typename T>
void ReturnValue<T>::SetUndefined() {
  static_assert(std::is_base_of<T, Primitive>::value, "type check");
  typedef internal::Internals I;
  *value_ = *I::GetRoot(GetIsolate(), I::kUndefinedValueRootIndex);
}

template<typename T>
void ReturnValue<T>::SetEmptyString() {
  static_assert(std::is_base_of<T, String>::value, "type check");
  typedef internal::Internals I;
  *value_ = *I::GetRoot(GetIsolate(), I::kEmptyStringRootIndex);
}

template <typename T>
Isolate* ReturnValue<T>::GetIsolate() const {
  // Isolate is always the pointer below the default value on the stack.
  return *reinterpret_cast<Isolate**>(&value_[-2]);
}

template <typename T>
Local<Value> ReturnValue<T>::Get() const {
  typedef internal::Internals I;
  if (*value_ == *I::GetRoot(GetIsolate(), I::kTheHoleValueRootIndex))
    return Local<Value>(*Undefined(GetIsolate()));
  return Local<Value>::New(GetIsolate(), reinterpret_cast<Value*>(value_));
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(S* whatever) {
  static_assert(sizeof(S) < 0, "incompilable to prevent inadvertent misuse");
}

template <typename T>
internal::Address ReturnValue<T>::GetDefaultValue() {
  // Default value is always the pointer below value_ on the stack.
  return value_[-1];
}

template <typename T>
FunctionCallbackInfo<T>::FunctionCallbackInfo(internal::Address* implicit_args,
                                              internal::Address* values,
                                              int length)
    : implicit_args_(implicit_args), values_(values), length_(length) {}

template<typename T>
Local<Value> FunctionCallbackInfo<T>::operator[](int i) const {
  // values_ points to the first argument (not the receiver).
  if (i < 0 || length_ <= i) return Local<Value>(*Undefined(GetIsolate()));
#ifdef V8_REVERSE_JSARGS
  return Local<Value>(reinterpret_cast<Value*>(values_ + i));
#else
  return Local<Value>(reinterpret_cast<Value*>(values_ - i));
#endif
}


template<typename T>
Local<Object> FunctionCallbackInfo<T>::This() const {
  // values_ points to the first argument (not the receiver).
#ifdef V8_REVERSE_JSARGS
  return Local<Object>(reinterpret_cast<Object*>(values_ - 1));
#else
  return Local<Object>(reinterpret_cast<Object*>(values_ + 1));
#endif
}


template<typename T>
Local<Object> FunctionCallbackInfo<T>::Holder() const {
  return Local<Object>(reinterpret_cast<Object*>(
      &implicit_args_[kHolderIndex]));
}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::NewTarget() const {
  return Local<Value>(
      reinterpret_cast<Value*>(&implicit_args_[kNewTargetIndex]));
}

template <typename T>
Local<Value> FunctionCallbackInfo<T>::Data() const {
  return Local<Value>(reinterpret_cast<Value*>(&implicit_args_[kDataIndex]));
}


template<typename T>
Isolate* FunctionCallbackInfo<T>::GetIsolate() const {
  return *reinterpret_cast<Isolate**>(&implicit_args_[kIsolateIndex]);
}


template<typename T>
ReturnValue<T> FunctionCallbackInfo<T>::GetReturnValue() const {
  return ReturnValue<T>(&implicit_args_[kReturnValueIndex]);
}


template<typename T>
bool FunctionCallbackInfo<T>::IsConstructCall() const {
  return !NewTarget()->IsUndefined();
}


template<typename T>
int FunctionCallbackInfo<T>::Length() const {
  return length_;
}

ScriptOrigin::ScriptOrigin(Local<Value> resource_name,
                           Local<Integer> resource_line_offset,
                           Local<Integer> resource_column_offset,
                           Local<Boolean> resource_is_shared_cross_origin,
                           Local<Integer> script_id,
                           Local<Value> source_map_url,
                           Local<Boolean> resource_is_opaque,
                           Local<Boolean> is_wasm, Local<Boolean> is_module,
                           Local<PrimitiveArray> host_defined_options)
    : resource_name_(resource_name),
      resource_line_offset_(resource_line_offset),
      resource_column_offset_(resource_column_offset),
      options_(!resource_is_shared_cross_origin.IsEmpty() &&
                   resource_is_shared_cross_origin->IsTrue(),
               !resource_is_opaque.IsEmpty() && resource_is_opaque->IsTrue(),
               !is_wasm.IsEmpty() && is_wasm->IsTrue(),
               !is_module.IsEmpty() && is_module->IsTrue()),
      script_id_(script_id),
      source_map_url_(source_map_url),
      host_defined_options_(host_defined_options) {}

Local<Value> ScriptOrigin::ResourceName() const { return resource_name_; }

Local<PrimitiveArray> ScriptOrigin::HostDefinedOptions() const {
  return host_defined_options_;
}

Local<Integer> ScriptOrigin::ResourceLineOffset() const {
  return resource_line_offset_;
}


Local<Integer> ScriptOrigin::ResourceColumnOffset() const {
  return resource_column_offset_;
}


Local<Integer> ScriptOrigin::ScriptID() const { return script_id_; }


Local<Value> ScriptOrigin::SourceMapUrl() const { return source_map_url_; }

ScriptCompiler::Source::Source(Local<String> string, const ScriptOrigin& origin,
                               CachedData* data)
    : source_string(string),
      resource_name(origin.ResourceName()),
      resource_line_offset(origin.ResourceLineOffset()),
      resource_column_offset(origin.ResourceColumnOffset()),
      resource_options(origin.Options()),
      source_map_url(origin.SourceMapUrl()),
      host_defined_options(origin.HostDefinedOptions()),
      cached_data(data) {}

ScriptCompiler::Source::Source(Local<String> string,
                               CachedData* data)
    : source_string(string), cached_data(data) {}


ScriptCompiler::Source::~Source() {
  delete cached_data;
}


const ScriptCompiler::CachedData* ScriptCompiler::Source::GetCachedData()
    const {
  return cached_data;
}

const ScriptOriginOptions& ScriptCompiler::Source::GetResourceOptions() const {
  return resource_options;
}

Local<Boolean> Boolean::New(Isolate* isolate, bool value) {
  return value ? True(isolate) : False(isolate);
}

void Template::Set(Isolate* isolate, const char* name, Local<Data> value) {
  Set(String::NewFromUtf8(isolate, name, NewStringType::kInternalized)
          .ToLocalChecked(),
      value);
}

FunctionTemplate* FunctionTemplate::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<FunctionTemplate*>(data);
}

ObjectTemplate* ObjectTemplate::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<ObjectTemplate*>(data);
}

Signature* Signature::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<Signature*>(data);
}

AccessorSignature* AccessorSignature::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<AccessorSignature*>(data);
}

Local<Value> Object::GetInternalField(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<A*>(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  auto instance_type = I::GetInstanceType(obj);
  if (instance_type == I::kJSObjectType ||
      instance_type == I::kJSApiObjectType ||
      instance_type == I::kJSSpecialApiObjectType) {
    int offset = I::kJSObjectHeaderSize + (I::kEmbedderDataSlotSize * index);
    A value = I::ReadRawField<A>(obj, offset);
#ifdef V8_COMPRESS_POINTERS
    // We read the full pointer value and then decompress it in order to avoid
    // dealing with potential endiannes issues.
    value = I::DecompressTaggedAnyField(obj, static_cast<uint32_t>(value));
#endif
    internal::Isolate* isolate =
        internal::IsolateFromNeverReadOnlySpaceObject(obj);
    A* result = HandleScope::CreateHandle(isolate, value);
    return Local<Value>(reinterpret_cast<Value*>(result));
  }
#endif
  return SlowGetInternalField(index);
}


void* Object::GetAlignedPointerFromInternalField(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<A*>(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  auto instance_type = I::GetInstanceType(obj);
  if (V8_LIKELY(instance_type == I::kJSObjectType ||
                instance_type == I::kJSApiObjectType ||
                instance_type == I::kJSSpecialApiObjectType)) {
    int offset = I::kJSObjectHeaderSize + (I::kEmbedderDataSlotSize * index);
    return I::ReadRawField<void*>(obj, offset);
  }
#endif
  return SlowGetAlignedPointerFromInternalField(index);
}

String* String::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<String*>(value);
}


Local<String> String::Empty(Isolate* isolate) {
  typedef internal::Address S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kEmptyStringRootIndex);
  return Local<String>(reinterpret_cast<String*>(slot));
}


String::ExternalStringResource* String::GetExternalStringResource() const {
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<const A*>(this);

  ExternalStringResource* result;
  if (I::IsExternalTwoByteString(I::GetInstanceType(obj))) {
    void* value = I::ReadRawField<void*>(obj, I::kStringResourceOffset);
    result = reinterpret_cast<String::ExternalStringResource*>(value);
  } else {
    result = GetExternalStringResourceSlow();
  }
#ifdef V8_ENABLE_CHECKS
  VerifyExternalStringResource(result);
#endif
  return result;
}


String::ExternalStringResourceBase* String::GetExternalStringResourceBase(
    String::Encoding* encoding_out) const {
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<const A*>(this);
  int type = I::GetInstanceType(obj) & I::kFullStringRepresentationMask;
  *encoding_out = static_cast<Encoding>(type & I::kStringEncodingMask);
  ExternalStringResourceBase* resource;
  if (type == I::kExternalOneByteRepresentationTag ||
      type == I::kExternalTwoByteRepresentationTag) {
    void* value = I::ReadRawField<void*>(obj, I::kStringResourceOffset);
    resource = static_cast<ExternalStringResourceBase*>(value);
  } else {
    resource = GetExternalStringResourceBaseSlow(encoding_out);
  }
#ifdef V8_ENABLE_CHECKS
  VerifyExternalStringResourceBase(resource, *encoding_out);
#endif
  return resource;
}


bool Value::IsUndefined() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsUndefined();
#else
  return QuickIsUndefined();
#endif
}

bool Value::QuickIsUndefined() const {
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  if (I::GetInstanceType(obj) != I::kOddballType) return false;
  return (I::GetOddballKind(obj) == I::kUndefinedOddballKind);
}


bool Value::IsNull() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsNull();
#else
  return QuickIsNull();
#endif
}

bool Value::QuickIsNull() const {
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  if (I::GetInstanceType(obj) != I::kOddballType) return false;
  return (I::GetOddballKind(obj) == I::kNullOddballKind);
}

bool Value::IsNullOrUndefined() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsNull() || FullIsUndefined();
#else
  return QuickIsNullOrUndefined();
#endif
}

bool Value::QuickIsNullOrUndefined() const {
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  if (I::GetInstanceType(obj) != I::kOddballType) return false;
  int kind = I::GetOddballKind(obj);
  return kind == I::kNullOddballKind || kind == I::kUndefinedOddballKind;
}

bool Value::IsString() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsString();
#else
  return QuickIsString();
#endif
}

bool Value::QuickIsString() const {
  typedef internal::Address A;
  typedef internal::Internals I;
  A obj = *reinterpret_cast<const A*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  return (I::GetInstanceType(obj) < I::kFirstNonstringType);
}


template <class T> Value* Value::Cast(T* value) {
  return static_cast<Value*>(value);
}


Boolean* Boolean::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Boolean*>(value);
}


Name* Name::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Name*>(value);
}


Symbol* Symbol::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Symbol*>(value);
}


Private* Private::Cast(Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return reinterpret_cast<Private*>(data);
}


Number* Number::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Number*>(value);
}


Integer* Integer::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Integer*>(value);
}


Int32* Int32::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Int32*>(value);
}


Uint32* Uint32::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Uint32*>(value);
}

BigInt* BigInt::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<BigInt*>(value);
}

Date* Date::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Date*>(value);
}


StringObject* StringObject::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<StringObject*>(value);
}


SymbolObject* SymbolObject::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<SymbolObject*>(value);
}


NumberObject* NumberObject::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<NumberObject*>(value);
}

BigIntObject* BigIntObject::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<BigIntObject*>(value);
}

BooleanObject* BooleanObject::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<BooleanObject*>(value);
}


RegExp* RegExp::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<RegExp*>(value);
}


Object* Object::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Object*>(value);
}


Array* Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Array*>(value);
}


Map* Map::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Map*>(value);
}


Set* Set::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Set*>(value);
}


Promise* Promise::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Promise*>(value);
}


Proxy* Proxy::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Proxy*>(value);
}

WasmModuleObject* WasmModuleObject::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<WasmModuleObject*>(value);
}

Promise::Resolver* Promise::Resolver::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Promise::Resolver*>(value);
}


ArrayBuffer* ArrayBuffer::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<ArrayBuffer*>(value);
}


ArrayBufferView* ArrayBufferView::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<ArrayBufferView*>(value);
}


TypedArray* TypedArray::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<TypedArray*>(value);
}


Uint8Array* Uint8Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Uint8Array*>(value);
}


Int8Array* Int8Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Int8Array*>(value);
}


Uint16Array* Uint16Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Uint16Array*>(value);
}


Int16Array* Int16Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Int16Array*>(value);
}


Uint32Array* Uint32Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Uint32Array*>(value);
}


Int32Array* Int32Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Int32Array*>(value);
}


Float32Array* Float32Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Float32Array*>(value);
}


Float64Array* Float64Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Float64Array*>(value);
}

BigInt64Array* BigInt64Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<BigInt64Array*>(value);
}

BigUint64Array* BigUint64Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<BigUint64Array*>(value);
}

Uint8ClampedArray* Uint8ClampedArray::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Uint8ClampedArray*>(value);
}


DataView* DataView::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<DataView*>(value);
}


SharedArrayBuffer* SharedArrayBuffer::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<SharedArrayBuffer*>(value);
}


Function* Function::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Function*>(value);
}


External* External::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<External*>(value);
}


template<typename T>
Isolate* PropertyCallbackInfo<T>::GetIsolate() const {
  return *reinterpret_cast<Isolate**>(&args_[kIsolateIndex]);
}


template<typename T>
Local<Value> PropertyCallbackInfo<T>::Data() const {
  return Local<Value>(reinterpret_cast<Value*>(&args_[kDataIndex]));
}


template<typename T>
Local<Object> PropertyCallbackInfo<T>::This() const {
  return Local<Object>(reinterpret_cast<Object*>(&args_[kThisIndex]));
}


template<typename T>
Local<Object> PropertyCallbackInfo<T>::Holder() const {
  return Local<Object>(reinterpret_cast<Object*>(&args_[kHolderIndex]));
}


template<typename T>
ReturnValue<T> PropertyCallbackInfo<T>::GetReturnValue() const {
  return ReturnValue<T>(&args_[kReturnValueIndex]);
}

template <typename T>
bool PropertyCallbackInfo<T>::ShouldThrowOnError() const {
  typedef internal::Internals I;
  if (args_[kShouldThrowOnErrorIndex] !=
      I::IntToSmi(I::kInferShouldThrowMode)) {
    return args_[kShouldThrowOnErrorIndex] != I::IntToSmi(I::kDontThrow);
  }
  return v8::internal::ShouldThrowOnError(
      reinterpret_cast<v8::internal::Isolate*>(GetIsolate()));
}

Local<Primitive> Undefined(Isolate* isolate) {
  typedef internal::Address S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kUndefinedValueRootIndex);
  return Local<Primitive>(reinterpret_cast<Primitive*>(slot));
}


Local<Primitive> Null(Isolate* isolate) {
  typedef internal::Address S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kNullValueRootIndex);
  return Local<Primitive>(reinterpret_cast<Primitive*>(slot));
}


Local<Boolean> True(Isolate* isolate) {
  typedef internal::Address S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kTrueValueRootIndex);
  return Local<Boolean>(reinterpret_cast<Boolean*>(slot));
}


Local<Boolean> False(Isolate* isolate) {
  typedef internal::Address S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kFalseValueRootIndex);
  return Local<Boolean>(reinterpret_cast<Boolean*>(slot));
}


void Isolate::SetData(uint32_t slot, void* data) {
  typedef internal::Internals I;
  I::SetEmbedderData(this, slot, data);
}


void* Isolate::GetData(uint32_t slot) {
  typedef internal::Internals I;
  return I::GetEmbedderData(this, slot);
}


uint32_t Isolate::GetNumberOfDataSlots() {
  typedef internal::Internals I;
  return I::kNumIsolateDataSlots;
}

template <class T>
MaybeLocal<T> Isolate::GetDataFromSnapshotOnce(size_t index) {
  T* data = reinterpret_cast<T*>(GetDataFromSnapshotOnce(index));
  if (data) internal::PerformCastCheck(data);
  return Local<T>(data);
}

int64_t Isolate::AdjustAmountOfExternalAllocatedMemory(
    int64_t change_in_bytes) {
  typedef internal::Internals I;
  constexpr int64_t kMemoryReducerActivationLimit = 32 * 1024 * 1024;
  int64_t* external_memory = reinterpret_cast<int64_t*>(
      reinterpret_cast<uint8_t*>(this) + I::kExternalMemoryOffset);
  int64_t* external_memory_limit = reinterpret_cast<int64_t*>(
      reinterpret_cast<uint8_t*>(this) + I::kExternalMemoryLimitOffset);
  int64_t* external_memory_low_since_mc =
      reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(this) +
                                 I::kExternalMemoryLowSinceMarkCompactOffset);

  // Embedders are weird: we see both over- and underflows here. Perform the
  // addition with unsigned types to avoid undefined behavior.
  const int64_t amount =
      static_cast<int64_t>(static_cast<uint64_t>(change_in_bytes) +
                           static_cast<uint64_t>(*external_memory));
  *external_memory = amount;

  if (amount < *external_memory_low_since_mc) {
    *external_memory_low_since_mc = amount;
    *external_memory_limit = amount + I::kExternalAllocationSoftLimit;
  }

  if (change_in_bytes <= 0) return *external_memory;

  int64_t allocation_diff_since_last_mc = static_cast<int64_t>(
      static_cast<uint64_t>(*external_memory) -
      static_cast<uint64_t>(*external_memory_low_since_mc));
  // Only check memory pressure and potentially trigger GC if the amount of
  // external memory increased.
  if (allocation_diff_since_last_mc > kMemoryReducerActivationLimit) {
    CheckMemoryPressure();
  }
  if (amount > *external_memory_limit) {
    ReportExternalAllocationLimitReached();
  }
  return *external_memory;
}

Local<Value> Context::GetEmbedderData(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Address A;
  typedef internal::Internals I;
  A ctx = *reinterpret_cast<const A*>(this);
  A embedder_data =
      I::ReadTaggedPointerField(ctx, I::kNativeContextEmbedderDataOffset);
  int value_offset =
      I::kEmbedderDataArrayHeaderSize + (I::kEmbedderDataSlotSize * index);
  A value = I::ReadRawField<A>(embedder_data, value_offset);
#ifdef V8_COMPRESS_POINTERS
  // We read the full pointer value and then decompress it in order to avoid
  // dealing with potential endiannes issues.
  value =
      I::DecompressTaggedAnyField(embedder_data, static_cast<uint32_t>(value));
#endif
  internal::Isolate* isolate = internal::IsolateFromNeverReadOnlySpaceObject(
      *reinterpret_cast<A*>(this));
  A* result = HandleScope::CreateHandle(isolate, value);
  return Local<Value>(reinterpret_cast<Value*>(result));
#else
  return SlowGetEmbedderData(index);
#endif
}


void* Context::GetAlignedPointerFromEmbedderData(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Address A;
  typedef internal::Internals I;
  A ctx = *reinterpret_cast<const A*>(this);
  A embedder_data =
      I::ReadTaggedPointerField(ctx, I::kNativeContextEmbedderDataOffset);
  int value_offset =
      I::kEmbedderDataArrayHeaderSize + (I::kEmbedderDataSlotSize * index);
  return I::ReadRawField<void*>(embedder_data, value_offset);
#else
  return SlowGetAlignedPointerFromEmbedderData(index);
#endif
}

template <class T>
MaybeLocal<T> Context::GetDataFromSnapshotOnce(size_t index) {
  T* data = reinterpret_cast<T*>(GetDataFromSnapshotOnce(index));
  if (data) internal::PerformCastCheck(data);
  return Local<T>(data);
}

template <class T>
size_t SnapshotCreator::AddData(Local<Context> context, Local<T> object) {
  T* object_ptr = *object;
  internal::Address* p = reinterpret_cast<internal::Address*>(object_ptr);
  return AddData(context, *p);
}

template <class T>
size_t SnapshotCreator::AddData(Local<T> object) {
  T* object_ptr = *object;
  internal::Address* p = reinterpret_cast<internal::Address*>(object_ptr);
  return AddData(*p);
}

/**
 * \example shell.cc
 * A simple shell that takes a list of expressions on the
 * command-line and executes them.
 */


/**
 * \example process.cc
 */


}  // namespace v8

#endif  // INCLUDE_V8_H_
