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
 * For other documentation see http://code.google.com/apis/v8/
 */

#ifndef INCLUDE_V8_H_
#define INCLUDE_V8_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <utility>
#include <vector>

#include "v8-version.h"  // NOLINT(build/include)
#include "v8config.h"    // NOLINT(build/include)

// We reserve the V8_* prefix for macros defined in V8 public API and
// assume there are no name conflicts with the embedder's code.

#ifdef V8_OS_WIN

// Setup for Windows DLL export/import. When building the V8 DLL the
// BUILDING_V8_SHARED needs to be defined. When building a program which uses
// the V8 DLL USING_V8_SHARED needs to be defined. When either building the V8
// static library or building a program which uses the V8 static library neither
// BUILDING_V8_SHARED nor USING_V8_SHARED should be defined.
#ifdef BUILDING_V8_SHARED
# define V8_EXPORT __declspec(dllexport)
#elif USING_V8_SHARED
# define V8_EXPORT __declspec(dllimport)
#else
# define V8_EXPORT
#endif  // BUILDING_V8_SHARED

#else  // V8_OS_WIN

// Setup for Linux shared library export.
#if V8_HAS_ATTRIBUTE_VISIBILITY
# ifdef BUILDING_V8_SHARED
#  define V8_EXPORT __attribute__ ((visibility("default")))
# else
#  define V8_EXPORT
# endif
#else
# define V8_EXPORT
#endif

#endif  // V8_OS_WIN

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
class CpuProfiler;
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
class WasmCompiledModule;
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
class Arguments;
class DeferredHandles;
class Heap;
class HeapObject;
class Isolate;
class Object;
struct ScriptStreamingData;
template<typename T> class CustomArguments;
class PropertyCallbackArguments;
class FunctionCallbackArguments;
class GlobalHandles;

namespace wasm {
class StreamingDecoder;
}  // namespace wasm
}  // namespace internal

namespace debug {
class ConsoleCallArguments;
}  // namespace debug

// --- Handles ---

#define TYPE_CHECK(T, S)                                       \
  while (false) {                                              \
    *(static_cast<T* volatile*>(0)) = static_cast<S*>(0);      \
  }

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
  V8_INLINE Local() : val_(0) {}
  template <class S>
  V8_INLINE Local(Local<S> that)
      : val_(reinterpret_cast<T*>(*that)) {
    /**
     * This check fails when trying to convert between incompatible
     * handles. For example, converting from a Local<String> to a
     * Local<Number>.
     */
    TYPE_CHECK(T, S);
  }

  /**
   * Returns true if the handle is empty.
   */
  V8_INLINE bool IsEmpty() const { return val_ == 0; }

  /**
   * Sets the handle to be empty. IsEmpty() will then return true.
   */
  V8_INLINE void Clear() { val_ = 0; }

  V8_INLINE T* operator->() const { return val_; }

  V8_INLINE T* operator*() const { return val_; }

  /**
   * Checks whether two handles are the same.
   * Returns true if both are empty, or if the objects
   * to which they refer are identical.
   * The handles' references are not checked.
   */
  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const {
    internal::Object** a = reinterpret_cast<internal::Object**>(this->val_);
    internal::Object** b = reinterpret_cast<internal::Object**>(that.val_);
    if (a == 0) return b == 0;
    if (b == 0) return false;
    return *a == *b;
  }

  template <class S> V8_INLINE bool operator==(
      const PersistentBase<S>& that) const {
    internal::Object** a = reinterpret_cast<internal::Object**>(this->val_);
    internal::Object** b = reinterpret_cast<internal::Object**>(that.val_);
    if (a == 0) return b == 0;
    if (b == 0) return false;
    return *a == *b;
  }

  /**
   * Checks whether two handles are different.
   * Returns true if only one of the handles is empty, or if
   * the objects to which they refer are different.
   * The handles' references are not checked.
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
    TYPE_CHECK(T, S);
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

  V8_INLINE bool IsEmpty() const { return val_ == NULL; }
  V8_INLINE void Empty() { val_ = 0; }

  V8_INLINE Local<T> Get(Isolate* isolate) const {
    return Local<T>::New(isolate, *this);
  }

  template <class S>
  V8_INLINE bool operator==(const PersistentBase<S>& that) const {
    internal::Object** a = reinterpret_cast<internal::Object**>(this->val_);
    internal::Object** b = reinterpret_cast<internal::Object**>(that.val_);
    if (a == NULL) return b == NULL;
    if (b == NULL) return false;
    return *a == *b;
  }

  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const {
    internal::Object** a = reinterpret_cast<internal::Object**>(this->val_);
    internal::Object** b = reinterpret_cast<internal::Object**>(that.val_);
    if (a == NULL) return b == NULL;
    if (b == NULL) return false;
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
   *  Install a finalization callback on this object.
   *  NOTE: There is no guarantee as to *when* or even *if* the callback is
   *  invoked. The invocation is performed solely on a best effort basis.
   *  As always, GC-based finalization should *not* be relied upon for any
   *  critical form of resource management!
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

  /**
   * Allows the embedder to tell the v8 garbage collector that a certain object
   * is alive. Only allowed when the embedder is asked to trace its heap by
   * EmbedderHeapTracer.
   */
  V8_INLINE void RegisterExternalReference(Isolate* isolate) const;

  /**
   * Marks the reference to this object independent. Garbage collector is free
   * to ignore any object groups containing this object. Weak callback for an
   * independent handle should not assume that it will be preceded by a global
   * GC prologue callback or followed by a global GC epilogue callback.
   */
  V8_DEPRECATE_SOON(
      "Objects are always considered independent. "
      "Use MarkActive to avoid collecting otherwise dead weak handles.",
      V8_INLINE void MarkIndependent());

  /**
   * Marks the reference to this object as active. The scavenge garbage
   * collection should not reclaim the objects marked as active, even if the
   * object held by the handle is otherwise unreachable.
   *
   * This bit is cleared after the each garbage collection pass.
   */
  V8_INLINE void MarkActive();

  V8_DEPRECATE_SOON("See MarkIndependent.",
                    V8_INLINE bool IsIndependent() const);

  /** Checks if the handle holds the only reference to an object. */
  V8_INLINE bool IsNearDeath() const;

  /** Returns true if the handle's reference is weak.  */
  V8_INLINE bool IsWeak() const;

  /**
   * Assigns a wrapper class ID to the handle. See RetainedObjectInfo interface
   * description in v8-profiler.h for details.
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
    Uncompilable<Object>();
  }
  // TODO(dcarney): come up with a good compile error here.
  template<class O> V8_INLINE static void Uncompilable() {
    TYPE_CHECK(O, Primitive);
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
  V8_INLINE Persistent() : PersistentBase<T>(0) { }
  /**
   * Construct a Persistent from a Local.
   * When the Local is non-empty, a new storage cell is created
   * pointing to the same object, and no flags are set.
   */
  template <class S>
  V8_INLINE Persistent(Isolate* isolate, Local<S> that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }
  /**
   * Construct a Persistent from a Persistent.
   * When the Persistent is non-empty, a new storage cell is created
   * pointing to the same object, and no flags are set.
   */
  template <class S, class M2>
  V8_INLINE Persistent(Isolate* isolate, const Persistent<S, M2>& that)
    : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }
  /**
   * The copy constructors and assignment operator create a Persistent
   * exactly as the Persistent constructor, but the Copy function from the
   * traits class is called, allowing the setting of flags based on the
   * copied Persistent.
   */
  V8_INLINE Persistent(const Persistent& that) : PersistentBase<T>(0) {
    Copy(that);
  }
  template <class S, class M2>
  V8_INLINE Persistent(const Persistent<S, M2>& that) : PersistentBase<T>(0) {
    Copy(that);
  }
  V8_INLINE Persistent& operator=(const Persistent& that) { // NOLINT
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
    TYPE_CHECK(T, S);
  }
  /**
   * Construct a Global from a PersistentBase.
   * When the Persistent is non-empty, a new storage cell is created
   * pointing to the same object, and no flags are set.
   */
  template <class S>
  V8_INLINE Global(Isolate* isolate, const PersistentBase<S>& that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, that.val_)) {
    TYPE_CHECK(T, S);
  }
  /**
   * Move constructor.
   */
  V8_INLINE Global(Global&& other) : PersistentBase<T>(other.val_) {  // NOLINT
    other.val_ = nullptr;
  }
  V8_INLINE ~Global() { this->Reset(); }
  /**
   * Move via assignment.
   */
  template <class S>
  V8_INLINE Global& operator=(Global<S>&& rhs) {  // NOLINT
    TYPE_CHECK(T, S);
    if (this != &rhs) {
      this->Reset();
      this->val_ = rhs.val_;
      rhs.val_ = nullptr;
    }
    return *this;
  }
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
  V8_INLINE HandleScope() {}

  void Initialize(Isolate* isolate);

  static internal::Object** CreateHandle(internal::Isolate* isolate,
                                         internal::Object* value);

 private:
  // Declaring operator new and delete as deleted is not spec compliant.
  // Therefore declare them private instead to disable dynamic alloc
  void* operator new(size_t size);
  void* operator new[](size_t size);
  void operator delete(void*, size_t);
  void operator delete[](void*, size_t);

  // Uses heap_object to obtain the current Isolate.
  static internal::Object** CreateHandle(internal::HeapObject* heap_object,
                                         internal::Object* value);

  internal::Isolate* isolate_;
  internal::Object** prev_next_;
  internal::Object** prev_limit_;

  // Local::New uses CreateHandle with an Isolate* parameter.
  template<class F> friend class Local;

  // Object::GetInternalField and Context::GetEmbedderData use CreateHandle with
  // a HeapObject* in their shortcuts.
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
  V8_INLINE ~EscapableHandleScope() {}

  /**
   * Pushes the value into the previous scope and returns a handle to it.
   * Cannot be called twice.
   */
  template <class T>
  V8_INLINE Local<T> Escape(Local<T> value) {
    internal::Object** slot =
        Escape(reinterpret_cast<internal::Object**>(*value));
    return Local<T>(reinterpret_cast<T*>(slot));
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

  internal::Object** Escape(internal::Object** escape_value);
  internal::Object** escape_slot_;
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
  internal::Object** prev_limit_;
  int prev_sealed_level_;
};


// --- Special objects ---


/**
 * The superclass of values and API object templates.
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
  void Set(int index, Local<Primitive> item);
  Local<Primitive> Get(int index);
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
class V8_EXPORT Module {
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
  static V8_DEPRECATED("Use maybe version",
                       Local<Script> Compile(Local<String> source,
                                             ScriptOrigin* origin = nullptr));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Local<String> source,
      ScriptOrigin* origin = nullptr);

  static Local<Script> V8_DEPRECATED("Use maybe version",
                                     Compile(Local<String> source,
                                             Local<String> file_name));

  /**
   * Runs the script returning the resulting value. It will be run in the
   * context in which it was created (ScriptCompiler::CompileBound or
   * UnboundScript::BindToCurrentContext()).
   */
  V8_DEPRECATED("Use maybe version", Local<Value> Run());
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
        : data(NULL),
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
           CachedData* cached_data = NULL);
    V8_INLINE Source(Local<String> source_string,
                     CachedData* cached_data = NULL);
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
    virtual ~ExternalSourceStream() {}

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
   * while streaming. It can be compiled after the streaming is complete.
   * StreamedSource must be kept alive while the streaming task is ran (see
   * ScriptStreamingTask below).
   */
  class V8_EXPORT StreamedSource {
   public:
    enum Encoding { ONE_BYTE, TWO_BYTE, UTF8 };

    StreamedSource(ExternalSourceStream* source_stream, Encoding encoding);
    ~StreamedSource();

    // Ownership of the CachedData or its buffers is *not* transferred to the
    // caller. The CachedData object is alive as long as the StreamedSource
    // object is alive.
    const CachedData* GetCachedData() const;

    internal::ScriptStreamingData* impl() const { return impl_; }

    // Prevent copying.
    StreamedSource(const StreamedSource&) = delete;
    StreamedSource& operator=(const StreamedSource&) = delete;

   private:
    internal::ScriptStreamingData* impl_;
  };

  /**
   * A streaming task which the embedder must run on a background thread to
   * stream scripts into V8. Returned by ScriptCompiler::StartStreamingScript.
   */
  class ScriptStreamingTask {
   public:
    virtual ~ScriptStreamingTask() {}
    virtual void Run() = 0;
  };

  enum CompileOptions {
    kNoCompileOptions = 0,
    kProduceParserCache,
    kConsumeParserCache,
    kProduceCodeCache,
    kProduceFullCodeCache,
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
      Isolate* isolate, Source* source);

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
  static V8_DEPRECATED("Use maybe version",
                       Local<Function> CompileFunctionInContext(
                           Isolate* isolate, Source* source,
                           Local<Context> context, size_t arguments_count,
                           Local<String> arguments[],
                           size_t context_extension_count,
                           Local<Object> context_extensions[]));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Function> CompileFunctionInContext(
      Local<Context> context, Source* source, size_t arguments_count,
      Local<String> arguments[], size_t context_extension_count,
      Local<Object> context_extensions[],
      CompileOptions options = kNoCompileOptions,
      NoCacheReason no_cache_reason = kNoCacheNoReason);

  /**
   * Creates and returns code cache for the specified unbound_script.
   * This will return nullptr if the script cannot be serialized. The
   * CachedData returned by this function should be owned by the caller.
   */
  static CachedData* CreateCodeCache(Local<UnboundScript> unbound_script);

  // Deprecated.
  static CachedData* CreateCodeCache(Local<UnboundScript> unbound_script,
                                     Local<String> source);

  /**
   * Creates and returns code cache for the specified function that was
   * previously produced by CompileFunctionInContext.
   * This will return nullptr if the script cannot be serialized. The
   * CachedData returned by this function should be owned by the caller.
   */
  static CachedData* CreateCodeCacheForFunction(Local<Function> function);

  // Deprecated.
  static CachedData* CreateCodeCacheForFunction(Local<Function> function,
                                                Local<String> source);

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

  V8_DEPRECATED("Use maybe version", Local<String> GetSourceLine() const);
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
  V8_DEPRECATED("Use maybe version", int GetLineNumber() const);
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
  Local<StackFrame> GetFrame(uint32_t index) const;

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
  IDLE
};

// A RegisterState represents the current state of registers used
// by the sampling profiler API.
struct RegisterState {
  RegisterState() : pc(nullptr), sp(nullptr), fp(nullptr) {}
  void* pc;  // Instruction pointer.
  void* sp;  // Stack pointer.
  void* fp;  // Frame pointer.
};

// The output structure filled up by GetStackSample API function.
struct SampleInfo {
  size_t frames_count;            // Number of frames collected.
  StateTag vm_state;              // Current VM state.
  void* external_callback_entry;  // External callback address if VM is
                                  // executing an external callback.
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
   * \param json_string The string to parse.
   * \return The corresponding value if successfully parsed.
   */
  static V8_DEPRECATE_SOON("Use the maybe version taking context",
                           MaybeLocal<Value> Parse(Isolate* isolate,
                                                   Local<String> json_string));
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
 *
 * WARNING: This API is under development, and changes (including incompatible
 * changes to the API or wire format) may occur without notice until this
 * warning is removed.
 */
class V8_EXPORT ValueSerializer {
 public:
  class V8_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

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
        Isolate* isolate, Local<WasmCompiledModule> module);
    /**
     * Allocates memory for the buffer of at least the size provided. The actual
     * size (which may be greater or equal) is written to |actual_size|. If no
     * buffer has been allocated yet, nullptr will be provided.
     *
     * If the memory cannot be allocated, nullptr should be returned.
     * |actual_size| will be ignored. It is assumed that |old_buffer| is still
     * valid in this case and has not been modified.
     */
    virtual void* ReallocateBufferMemory(void* old_buffer, size_t size,
                                         size_t* actual_size);

    /**
     * Frees a buffer allocated with |ReallocateBufferMemory|.
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
   * Returns the stored data. This serializer should not be used once the buffer
   * is released. The contents are undefined if a previous write has failed.
   */
  V8_DEPRECATE_SOON("Use Release()", std::vector<uint8_t> ReleaseBuffer());

  /**
   * Returns the stored data (allocated using the delegate's
   * AllocateBufferMemory) and its size. This serializer should not be used once
   * the buffer is released. The contents are undefined if a previous write has
   * failed.
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
   * Similar to TransferArrayBuffer, but for SharedArrayBuffer.
   */
  V8_DEPRECATE_SOON("Use Delegate::GetSharedArrayBufferId",
                    void TransferSharedArrayBuffer(
                        uint32_t transfer_id,
                        Local<SharedArrayBuffer> shared_array_buffer));

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

 private:
  ValueSerializer(const ValueSerializer&) = delete;
  void operator=(const ValueSerializer&) = delete;

  struct PrivateData;
  PrivateData* private_;
};

/**
 * Deserializes values from data written with ValueSerializer, or a compatible
 * implementation.
 *
 * WARNING: This API is under development, and changes (including incompatible
 * changes to the API or wire format) may occur without notice until this
 * warning is removed.
 */
class V8_EXPORT ValueDeserializer {
 public:
  class V8_EXPORT Delegate {
   public:
    virtual ~Delegate() {}

    /**
     * The embedder overrides this method to read some kind of host object, if
     * possible. If not, a suitable exception should be thrown and
     * MaybeLocal<Object>() returned.
     */
    virtual MaybeLocal<Object> ReadHostObject(Isolate* isolate);

    /**
     * Get a WasmCompiledModule given a transfer_id previously provided
     * by ValueSerializer::GetWasmModuleTransferId
     */
    virtual MaybeLocal<WasmCompiledModule> GetWasmModuleFromId(
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
   * Expect inline wasm in the data stream (rather than in-memory transfer)
   */
  void SetExpectInlineWasm(bool allow_inline_wasm);

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

 private:
  ValueDeserializer(const ValueDeserializer&) = delete;
  void operator=(const ValueDeserializer&) = delete;

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
   */
  V8_INLINE bool IsUndefined() const;

  /**
   * Returns true if this value is the null value.  See ECMA-262
   * 4.3.11.
   */
  V8_INLINE bool IsNull() const;

  /**
   * Returns true if this value is either the null or the undefined value.
   * See ECMA-262
   * 4.3.11. and 4.3.12
   */
  V8_INLINE bool IsNullOrUndefined() const;

  /**
  * Returns true if this value is true.
  */
  bool IsTrue() const;

  /**
   * Returns true if this value is false.
   */
  bool IsFalse() const;

  /**
   * Returns true if this value is a symbol or a string.
   */
  bool IsName() const;

  /**
   * Returns true if this value is an instance of the String type.
   * See ECMA-262 8.4.
   */
  V8_INLINE bool IsString() const;

  /**
   * Returns true if this value is a symbol.
   */
  bool IsSymbol() const;

  /**
   * Returns true if this value is a function.
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
   */
  bool IsBigInt() const;

  /**
   * Returns true if this value is boolean.
   */
  bool IsBoolean() const;

  /**
   * Returns true if this value is a number.
   */
  bool IsNumber() const;

  /**
   * Returns true if this value is external.
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
   * This is an experimental feature.
   */
  bool IsSharedArrayBuffer() const;

  /**
   * Returns true if this value is a JavaScript Proxy.
   */
  bool IsProxy() const;

  bool IsWebAssemblyCompiledModule() const;

  /**
   * Returns true if the value is a Module Namespace Object.
   */
  bool IsModuleNamespaceObject() const;

  V8_WARN_UNUSED_RESULT MaybeLocal<BigInt> ToBigInt(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Boolean> ToBoolean(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Number> ToNumber(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToString(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToDetailString(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> ToObject(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Integer> ToInteger(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToUint32(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Int32> ToInt32(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Boolean> ToBoolean(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Number> ToNumber(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<String> ToString(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Object> ToObject(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Integer> ToInteger(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Int32> ToInt32(Isolate* isolate) const);

  inline V8_DEPRECATE_SOON("Use maybe version",
                           Local<Boolean> ToBoolean() const);
  inline V8_DEPRECATE_SOON("Use maybe version", Local<String> ToString() const);
  inline V8_DEPRECATE_SOON("Use maybe version", Local<Object> ToObject() const);
  inline V8_DEPRECATE_SOON("Use maybe version",
                           Local<Integer> ToInteger() const);

  /**
   * Attempts to convert a string to an array index.
   * Returns an empty handle if the conversion fails.
   */
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToArrayIndex(
      Local<Context> context) const;

  V8_WARN_UNUSED_RESULT Maybe<bool> BooleanValue(Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<double> NumberValue(Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<int64_t> IntegerValue(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<uint32_t> Uint32Value(
      Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<int32_t> Int32Value(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version", bool BooleanValue() const);
  V8_DEPRECATE_SOON("Use maybe version", double NumberValue() const);
  V8_DEPRECATE_SOON("Use maybe version", int64_t IntegerValue() const);
  V8_DEPRECATE_SOON("Use maybe version", uint32_t Uint32Value() const);
  V8_DEPRECATE_SOON("Use maybe version", int32_t Int32Value() const);

  /** JS == */
  V8_DEPRECATE_SOON("Use maybe version", bool Equals(Local<Value> that) const);
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
      sizeof(void*) == 4 ? (1 << 28) - 16 : (1 << 30) - 1 - 24;

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
  int Utf8Length() const;

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
  int Write(uint16_t* buffer,
            int start = 0,
            int length = -1,
            int options = NO_OPTIONS) const;
  // One byte characters.
  int WriteOneByte(uint8_t* buffer,
                   int start = 0,
                   int length = -1,
                   int options = NO_OPTIONS) const;
  // UTF-8 encoded characters.
  int WriteUtf8(char* buffer,
                int length = -1,
                int* nchars_ref = NULL,
                int options = NO_OPTIONS) const;

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
    virtual ~ExternalStringResourceBase() {}

    virtual bool IsCompressible() const { return false; }

   protected:
    ExternalStringResourceBase() {}

    /**
     * Internally V8 will call this Dispose method when the external string
     * resource is no longer needed. The default implementation will use the
     * delete operator. This method can be overridden in subclasses to
     * control how allocated external string resources are disposed.
     */
    virtual void Dispose() { delete this; }

    // Disallow copying and assigning.
    ExternalStringResourceBase(const ExternalStringResourceBase&) = delete;
    void operator=(const ExternalStringResourceBase&) = delete;

   private:
    friend class internal::Heap;
    friend class v8::String;
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
    virtual ~ExternalStringResource() {}

    /**
     * The string data from the underlying buffer.
     */
    virtual const uint16_t* data() const = 0;

    /**
     * The length of the string. That is, the number of two-byte characters.
     */
    virtual size_t length() const = 0;

   protected:
    ExternalStringResource() {}
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
    virtual ~ExternalOneByteStringResource() {}
    /** The string data from the underlying buffer.*/
    virtual const char* data() const = 0;
    /** The number of Latin-1 characters in the string.*/
    virtual size_t length() const = 0;
   protected:
    ExternalOneByteStringResource() {}
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

  // TODO(dcarney): remove with deprecation of New functions.
  enum NewStringType {
    kNormalString = static_cast<int>(v8::NewStringType::kNormal),
    kInternalizedString = static_cast<int>(v8::NewStringType::kInternalized)
  };

  /** Allocates a new string from UTF-8 data.*/
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewFromUtf8(Isolate* isolate, const char* data,
                                NewStringType type = kNormalString,
                                int length = -1));

  /** Allocates a new string from UTF-8 data. Only returns an empty value when
   * length > kMaxLength. **/
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromUtf8(
      Isolate* isolate, const char* data, v8::NewStringType type,
      int length = -1);

  /** Allocates a new string from Latin-1 data.  Only returns an empty value
   * when length > kMaxLength. **/
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromOneByte(
      Isolate* isolate, const uint8_t* data, v8::NewStringType type,
      int length = -1);

  /** Allocates a new string from UTF-16 data.*/
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                   NewStringType type = kNormalString,
                                   int length = -1));

  /** Allocates a new string from UTF-16 data. Only returns an empty value when
   * length > kMaxLength. **/
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromTwoByte(
      Isolate* isolate, const uint16_t* data, v8::NewStringType type,
      int length = -1);

  /**
   * Creates a new string by concatenating the left and the right strings
   * passed in as parameters.
   */
  static Local<String> Concat(Local<String> left, Local<String> right);

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
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<String> NewExternal(Isolate* isolate,
                                ExternalOneByteStringResource* resource));
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
   * Converts an object to a UTF-8-encoded character array.  Useful if
   * you want to print the object.  If conversion to a string fails
   * (e.g. due to an exception in the toString() method of the object)
   * then the length() method returns 0 and the * operator returns
   * NULL.
   */
  class V8_EXPORT Utf8Value {
   public:
    V8_DEPRECATED("Use Isolate version",
                  explicit Utf8Value(Local<v8::Value> obj));
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
    V8_DEPRECATED("Use Isolate version", explicit Value(Local<v8::Value> obj));
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
  static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript symbol (ECMA-262 edition 6)
 */
class V8_EXPORT Symbol : public Name {
 public:
  /**
   * Returns the print name string of the symbol, or undefined if none.
   */
  Local<Value> Name() const;

  /**
   * Create a symbol. If name is not empty, it will be used as the description.
   */
  static Local<Symbol> New(Isolate* isolate,
                           Local<String> name = Local<String>());

  /**
   * Access global symbol registry.
   * Note that symbols created this way are never collected, so
   * they should only be used for statically fixed properties.
   * Also, there is only one global name space for the names used as keys.
   * To minimize the potential for clashes, use qualified names as keys.
   */
  static Local<Symbol> For(Isolate *isolate, Local<String> name);

  /**
   * Retrieve a global symbol. Similar to |For|, but using a separate
   * registry that is not accessible by (and cannot clash with) JavaScript code.
   */
  static Local<Symbol> ForApi(Isolate *isolate, Local<String> name);

  // Well-known symbols
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
 * or an Accessor's getter callback. For Interceptors, please see
 * PropertyHandlerFlags's kHasNoSideEffect.
 */
enum class SideEffectType { kHasSideEffect, kHasNoSideEffect };

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
enum class KeyConversionMode { kConvertToString, kKeepNumbers };

/**
 * Integrity level for objects.
 */
enum class IntegrityLevel { kFrozen, kSealed };

/**
 * A JavaScript object (ECMA-262, 4.3.3)
 */
class V8_EXPORT Object : public Value {
 public:
  V8_DEPRECATE_SOON("Use maybe version",
                    bool Set(Local<Value> key, Local<Value> value));
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context,
                                        Local<Value> key, Local<Value> value);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool Set(uint32_t index, Local<Value> value));
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
      Local<Context> context, Local<Name> key, PropertyDescriptor& descriptor);

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Get(Local<Value> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Get(uint32_t index));
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

  V8_DEPRECATE_SOON("Use maybe version", bool Has(Local<Value> key));
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

  V8_DEPRECATE_SOON("Use maybe version", bool Delete(Local<Value> key));
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
      AccessorNameGetterCallback getter, AccessorNameSetterCallback setter = 0,
      MaybeLocal<Value> data = MaybeLocal<Value>(),
      AccessControl settings = DEFAULT, PropertyAttribute attribute = None,
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect);

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
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect);

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
      SideEffectType getter_side_effect_type = SideEffectType::kHasSideEffect);

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
  V8_DEPRECATE_SOON("Use maybe version", Local<Array> GetPropertyNames());
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
  V8_DEPRECATE_SOON("Use maybe version", Local<Array> GetOwnPropertyNames());
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

  /** Same as above, but works for Persistents */
  V8_INLINE static int InternalFieldCount(
      const PersistentBase<Object>& object) {
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

  /** Same as above, but works for Persistents */
  V8_INLINE static void* GetAlignedPointerFromInternalField(
      const PersistentBase<Object>& object, int index) {
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
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealNamedProperty(Local<String> key));
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
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealIndexedProperty(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealIndexedProperty(
      Local<Context> context, uint32_t index);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealNamedCallbackProperty(Local<String> key));
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
    TYPE_CHECK(T, S);
  }
  // Local setters
  template <typename S>
  V8_INLINE V8_DEPRECATE_SOON("Use Global<> instead",
                              void Set(const Persistent<S>& handle));
  template <typename S>
  V8_INLINE void Set(const Global<S>& handle);
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
  V8_INLINE void SetInternal(internal::Object* value) { *value_ = value; }
  V8_INLINE internal::Object* GetDefaultValue();
  V8_INLINE explicit ReturnValue(internal::Object** slot);
  internal::Object** value_;
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
  /** Accessor for the available arguments. */
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

  V8_INLINE FunctionCallbackInfo(internal::Object** implicit_args,
                                 internal::Object** values, int length);
  internal::Object** implicit_args_;
  internal::Object** values_;
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

  V8_INLINE PropertyCallbackInfo(internal::Object** args) : args_(args) {}
  internal::Object** args_;
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
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<Function> New(Isolate* isolate, FunctionCallback callback,
                          Local<Value> data = Local<Value>(), int length = 0));

  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
      Local<Context> context, int argc, Local<Value> argv[]) const;

  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
      Local<Context> context) const {
    return NewInstance(context, 0, nullptr);
  }

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> Call(Local<Value> recv, int argc,
                                      Local<Value> argv[]));
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
    static V8_DEPRECATED("Use maybe version",
                         Local<Resolver> New(Isolate* isolate));
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
    V8_DEPRECATED("Use maybe version", void Resolve(Local<Value> value));
    V8_WARN_UNUSED_RESULT Maybe<bool> Resolve(Local<Context> context,
                                              Local<Value> value);

    V8_DEPRECATED("Use maybe version", void Reject(Local<Value> value));
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
  PropertyDescriptor(Local<Value> value);

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

// TODO(mtrofin): rename WasmCompiledModule to WasmModuleObject, for
// consistency with internal APIs.
class V8_EXPORT WasmCompiledModule : public Object {
 public:
  typedef std::pair<std::unique_ptr<const uint8_t[]>, size_t> SerializedModule;
  /**
   * A buffer that is owned by the caller.
   */
  typedef std::pair<const uint8_t*, size_t> CallerOwnedBuffer;

  /**
   * An opaque, native heap object for transferring wasm modules. It
   * supports move semantics, and does not support copy semantics.
   */
  class TransferrableModule final {
   public:
    TransferrableModule(TransferrableModule&& src) = default;
    TransferrableModule(const TransferrableModule& src) = delete;

    TransferrableModule& operator=(TransferrableModule&& src) = default;
    TransferrableModule& operator=(const TransferrableModule& src) = delete;

   private:
    typedef std::pair<std::unique_ptr<const uint8_t[]>, size_t> OwnedBuffer;
    friend class WasmCompiledModule;
    TransferrableModule(OwnedBuffer&& code, OwnedBuffer&& bytes)
        : compiled_code(std::move(code)), wire_bytes(std::move(bytes)) {}

    OwnedBuffer compiled_code = {nullptr, 0};
    OwnedBuffer wire_bytes = {nullptr, 0};
  };

  /**
   * Get an in-memory, non-persistable, and context-independent (meaning,
   * suitable for transfer to another Isolate and Context) representation
   * of this wasm compiled module.
   */
  TransferrableModule GetTransferrableModule();

  /**
   * Efficiently re-create a WasmCompiledModule, without recompiling, from
   * a TransferrableModule.
   */
  static MaybeLocal<WasmCompiledModule> FromTransferrableModule(
      Isolate* isolate, const TransferrableModule&);

  /**
   * Get the wasm-encoded bytes that were used to compile this module.
   */
  Local<String> GetWasmWireBytes();

  /**
   * Serialize the compiled module. The serialized data does not include the
   * uncompiled bytes.
   */
  SerializedModule Serialize();

  /**
   * If possible, deserialize the module, otherwise compile it from the provided
   * uncompiled bytes.
   */
  static MaybeLocal<WasmCompiledModule> DeserializeOrCompile(
      Isolate* isolate, const CallerOwnedBuffer& serialized_module,
      const CallerOwnedBuffer& wire_bytes);
  V8_INLINE static WasmCompiledModule* Cast(Value* obj);

 private:
  static MaybeLocal<WasmCompiledModule> Deserialize(
      Isolate* isolate, const CallerOwnedBuffer& serialized_module,
      const CallerOwnedBuffer& wire_bytes);
  static MaybeLocal<WasmCompiledModule> Compile(Isolate* isolate,
                                                const uint8_t* start,
                                                size_t length);
  static CallerOwnedBuffer AsCallerOwned(
      const TransferrableModule::OwnedBuffer& buff) {
    return {buff.first.get(), buff.second};
  }

  WasmCompiledModule();
  static void CheckCast(Value* obj);
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

  ~WasmModuleObjectBuilderStreaming();

 private:
  typedef std::pair<std::unique_ptr<const uint8_t[]>, size_t> Buffer;

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
  std::vector<Buffer> received_buffers_;
  size_t total_size_ = 0;
  std::shared_ptr<internal::wasm::StreamingDecoder> streaming_decoder_;
};

#ifndef V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT
// The number of required internal fields can be defined by embedder.
#define V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT 2
#endif


enum class ArrayBufferCreationMode { kInternalized, kExternalized };


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
    virtual ~Allocator() {}

    /**
     * Allocate |length| bytes. Return NULL if allocation is not successful.
     * Memory should be initialized to zeroes.
     */
    virtual void* Allocate(size_t length) = 0;

    /**
     * Allocate |length| bytes. Return NULL if allocation is not successful.
     * Memory does not have to be initialized.
     */
    virtual void* AllocateUninitialized(size_t length) = 0;

    /**
     * Free the memory block of size |length|, pointed to by |data|.
     * That memory is guaranteed to be previously allocated by |Allocate|.
     */
    virtual void Free(void* data, size_t length) = 0;

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
   * The Data pointer of ArrayBuffer::Contents is always allocated with
   * Allocator::Allocate that is set via Isolate::CreateParams.
   */
  class V8_EXPORT Contents { // NOLINT
   public:
    Contents()
        : data_(nullptr),
          byte_length_(0),
          allocation_base_(nullptr),
          allocation_length_(0),
          allocation_mode_(Allocator::AllocationMode::kNormal) {}

    void* AllocationBase() const { return allocation_base_; }
    size_t AllocationLength() const { return allocation_length_; }
    Allocator::AllocationMode AllocationMode() const {
      return allocation_mode_;
    }

    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }

   private:
    void* data_;
    size_t byte_length_;
    void* allocation_base_;
    size_t allocation_length_;
    Allocator::AllocationMode allocation_mode_;

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
  static Local<ArrayBuffer> New(
      Isolate* isolate, void* data, size_t byte_length,
      ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  /**
   * Returns true if ArrayBuffer is externalized, that is, does not
   * own its memory block.
   */
  bool IsExternal() const;

  /**
   * Returns true if this ArrayBuffer may be neutered.
   */
  bool IsNeuterable() const;

  /**
   * Neuters this ArrayBuffer and all its views (typed arrays).
   * Neutering sets the byte length of the buffer and all typed arrays to zero,
   * preventing JavaScript from ever accessing underlying backing store.
   * ArrayBuffer should have been externalized and must be neuterable.
   */
  void Neuter();

  /**
   * Make this ArrayBuffer external. The pointer to underlying memory block
   * and byte length are returned as |Contents| structure. After ArrayBuffer
   * had been externalized, it does no longer own the memory block. The caller
   * should take steps to free memory when it is no longer needed.
   *
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|
   * that has been set via Isolate::CreateParams.
   */
  Contents Externalize();

  /**
   * Get a pointer to the ArrayBuffer's underlying memory block without
   * externalizing it. If the ArrayBuffer is not externalized, this pointer
   * will become invalid as soon as the ArrayBuffer gets garbage collected.
   *
   * The embedder should make sure to hold a strong reference to the
   * ArrayBuffer while accessing this pointer.
   *
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|.
   */
  Contents GetContents();

  V8_INLINE static ArrayBuffer* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;
  static const int kEmbedderFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

 private:
  ArrayBuffer();
  static void CheckCast(Value* obj);
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
  static constexpr size_t kMaxLength =
      sizeof(void*) == 4 ? (1u << 30) - 1 : (1u << 31) - 1;

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
 * This API is experimental and may change significantly.
 */
class V8_EXPORT SharedArrayBuffer : public Object {
 public:
  /**
   * The contents of an |SharedArrayBuffer|. Externalization of
   * |SharedArrayBuffer| returns an instance of this class, populated, with a
   * pointer to data and byte length.
   *
   * The Data pointer of SharedArrayBuffer::Contents is always allocated with
   * |ArrayBuffer::Allocator::Allocate| by the allocator specified in
   * v8::Isolate::CreateParams::array_buffer_allocator.
   *
   * This API is experimental and may change significantly.
   */
  class V8_EXPORT Contents {  // NOLINT
   public:
    Contents()
        : data_(nullptr),
          byte_length_(0),
          allocation_base_(nullptr),
          allocation_length_(0),
          allocation_mode_(ArrayBuffer::Allocator::AllocationMode::kNormal) {}

    void* AllocationBase() const { return allocation_base_; }
    size_t AllocationLength() const { return allocation_length_; }
    ArrayBuffer::Allocator::AllocationMode AllocationMode() const {
      return allocation_mode_;
    }

    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }

   private:
    void* data_;
    size_t byte_length_;
    void* allocation_base_;
    size_t allocation_length_;
    ArrayBuffer::Allocator::AllocationMode allocation_mode_;

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
  static Local<SharedArrayBuffer> New(
      Isolate* isolate, void* data, size_t byte_length,
      ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  /**
   * Returns true if SharedArrayBuffer is externalized, that is, does not
   * own its memory block.
   */
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
  Contents Externalize();

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
  Contents GetContents();

  V8_INLINE static SharedArrayBuffer* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

 private:
  SharedArrayBuffer();
  static void CheckCast(Value* obj);
};


/**
 * An instance of the built-in Date constructor (ECMA-262, 15.9).
 */
class V8_EXPORT Date : public Object {
 public:
  static V8_DEPRECATE_SOON("Use maybe version.",
                           Local<Value> New(Isolate* isolate, double time));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Value> New(Local<Context> context,
                                                     double time);

  /**
   * A specialization of Value::NumberValue that is more efficient
   * because we know the structure of this object.
   */
  double ValueOf() const;

  V8_INLINE static Date* Cast(Value* obj);

  /**
   * Notification that the embedder has changed the time zone,
   * daylight savings time, or other date / time configuration
   * parameters.  V8 keeps a cache of various values used for
   * date / time computation.  This notification will reset
   * those cached values for the current context so that date /
   * time configuration changes would be reflected in the Date
   * object.
   *
   * This API should not be called more than needed as it will
   * negatively impact the performance of date operations.
   */
  static void DateTimeConfigurationChangeNotification(Isolate* isolate);

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
  static Local<Value> New(Local<String> value);

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
  static V8_DEPRECATED("Use maybe version",
                       Local<RegExp> New(Local<String> pattern, Flags flags));
  static V8_WARN_UNUSED_RESULT MaybeLocal<RegExp> New(Local<Context> context,
                                                      Local<String> pattern,
                                                      Flags flags);

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

#define V8_INTRINSICS_LIST(F)                    \
  F(ArrayProto_entries, array_entries_iterator)  \
  F(ArrayProto_forEach, array_for_each_iterator) \
  F(ArrayProto_keys, array_keys_iterator)        \
  F(ArrayProto_values, array_values_iterator)    \
  F(ErrorPrototype, initial_error_prototype)     \
  F(IteratorPrototype, initial_iterator_prototype)

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
      AccessorSetterCallback setter = 0,
      // TODO(dcarney): gcc can't handle Local below
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>(),
      AccessControl settings = DEFAULT);
  void SetNativeDataProperty(
      Local<Name> name, AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = 0,
      // TODO(dcarney): gcc can't handle Local below
      Local<Value> data = Local<Value>(), PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>(),
      AccessControl settings = DEFAULT);

  /**
   * Like SetNativeDataProperty, but V8 will replace the native data property
   * with a real data property on first access.
   */
  void SetLazyDataProperty(Local<Name> name, AccessorNameGetterCallback getter,
                           Local<Value> data = Local<Value>(),
                           PropertyAttribute attribute = None);

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


/**
 * NamedProperty[Getter|Setter] are used as interceptors on object.
 * See ObjectTemplate::SetNamedPropertyHandler.
 */
typedef void (*NamedPropertyGetterCallback)(
    Local<String> property,
    const PropertyCallbackInfo<Value>& info);


/**
 * Returns the value if the setter intercepts the request.
 * Otherwise, returns an empty handle.
 */
typedef void (*NamedPropertySetterCallback)(
    Local<String> property,
    Local<Value> value,
    const PropertyCallbackInfo<Value>& info);


/**
 * Returns a non-empty handle if the interceptor intercepts the request.
 * The result is an integer encoding property attributes (like v8::None,
 * v8::DontEnum, etc.)
 */
typedef void (*NamedPropertyQueryCallback)(
    Local<String> property,
    const PropertyCallbackInfo<Integer>& info);


/**
 * Returns a non-empty handle if the deleter intercepts the request.
 * The return value is true if the property could be deleted and false
 * otherwise.
 */
typedef void (*NamedPropertyDeleterCallback)(
    Local<String> property,
    const PropertyCallbackInfo<Boolean>& info);

/**
 * Returns an array containing the names of the properties the named
 * property getter intercepts.
 *
 * Note: The values in the array must be of type v8::Name.
 */
typedef void (*NamedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);


// TODO(dcarney): Deprecate and remove previous typedefs, and replace
// GenericNamedPropertyFooCallback with just NamedPropertyFooCallback.

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
 *    instance_t->SetAccessor(String::NewFromUtf8(isolate, "instance_accessor"),
 *                            InstanceAccessorCallback);
 *    instance_t->SetHandler(
 *        NamedPropertyHandlerConfiguration(PropertyHandlerCallback));
 *    instance_t->Set(String::NewFromUtf8(isolate, "instance_property"),
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
 */
class V8_EXPORT FunctionTemplate : public Template {
 public:
  /** Creates a function template.*/
  static Local<FunctionTemplate> New(
      Isolate* isolate, FunctionCallback callback = 0,
      Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0,
      ConstructorBehavior behavior = ConstructorBehavior::kAllow,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect);

  /** Get a template included in the snapshot by index. */
  static MaybeLocal<FunctionTemplate> FromSnapshot(Isolate* isolate,
                                                   size_t index);

  /**
   * Creates a function template backed/cached by a private property.
   */
  static Local<FunctionTemplate> NewWithCache(
      Isolate* isolate, FunctionCallback callback,
      Local<Private> cache_property, Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0,
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect);

  /** Returns the unique function instance in the current execution context.*/
  V8_DEPRECATE_SOON("Use maybe version", Local<Function> GetFunction());
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
   * FunctionTemplate is called.
   */
  void SetCallHandler(
      FunctionCallback callback, Local<Value> data = Local<Value>(),
      SideEffectType side_effect_type = SideEffectType::kHasSideEffect);

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
   * Determines whether the __proto__ accessor ignores instances of
   * the function template.  If instances of the function template are
   * ignored, __proto__ skips all instances and instead returns the
   * next object in the prototype chain.
   *
   * Call with a value of true to make the __proto__ accessor ignore
   * instances of the function template.  Call with a value of false
   * to make the __proto__ accessor not ignore instances of the
   * function template.  By default, instances of a function template
   * are not ignored.
   */
  void SetHiddenPrototype(bool value);

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
      /** Note: getter is required */
      GenericNamedPropertyGetterCallback getter = 0,
      GenericNamedPropertySetterCallback setter = 0,
      GenericNamedPropertyQueryCallback query = 0,
      GenericNamedPropertyDeleterCallback deleter = 0,
      GenericNamedPropertyEnumeratorCallback enumerator = 0,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(0),
        descriptor(0),
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
        query(0),
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
      /** Note: getter is required */
      IndexedPropertyGetterCallback getter = 0,
      IndexedPropertySetterCallback setter = 0,
      IndexedPropertyQueryCallback query = 0,
      IndexedPropertyDeleterCallback deleter = 0,
      IndexedPropertyEnumeratorCallback enumerator = 0,
      Local<Value> data = Local<Value>(),
      PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
      : getter(getter),
        setter(setter),
        query(query),
        deleter(deleter),
        enumerator(enumerator),
        definer(0),
        descriptor(0),
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
        query(0),
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

  /** Get a template included in the snapshot by index. */
  static MaybeLocal<ObjectTemplate> FromSnapshot(Isolate* isolate,
                                                 size_t index);

  /** Creates a new instance of this template.*/
  V8_DEPRECATE_SOON("Use maybe version", Local<Object> NewInstance());
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
      AccessorSetterCallback setter = 0, Local<Value> data = Local<Value>(),
      AccessControl settings = DEFAULT, PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>());
  void SetAccessor(
      Local<Name> name, AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter = 0, Local<Value> data = Local<Value>(),
      AccessControl settings = DEFAULT, PropertyAttribute attribute = None,
      Local<AccessorSignature> signature = Local<AccessorSignature>());

  /**
   * Sets a named property handler on the object template.
   *
   * Whenever a property whose name is a string is accessed on objects created
   * from this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * SetNamedPropertyHandler() is different from SetHandler(), in
   * that the latter can intercept symbol-named properties as well as
   * string-named properties when called with a
   * NamedPropertyHandlerConfiguration. New code should use SetHandler().
   *
   * \param getter The callback to invoke when getting a property.
   * \param setter The callback to invoke when setting a property.
   * \param query The callback to invoke to check if a property is present,
   *   and if present, get its attributes.
   * \param deleter The callback to invoke when deleting a property.
   * \param enumerator The callback to invoke to enumerate all the named
   *   properties of an object.
   * \param data A piece of data that will be passed to the callbacks
   *   whenever they are invoked.
   */
  V8_DEPRECATED(
      "Use SetHandler(const NamedPropertyHandlerConfiguration) "
      "with the kOnlyInterceptStrings flag set.",
      void SetNamedPropertyHandler(
          NamedPropertyGetterCallback getter,
          NamedPropertySetterCallback setter = 0,
          NamedPropertyQueryCallback query = 0,
          NamedPropertyDeleterCallback deleter = 0,
          NamedPropertyEnumeratorCallback enumerator = 0,
          Local<Value> data = Local<Value>()));

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
      IndexedPropertySetterCallback setter = 0,
      IndexedPropertyQueryCallback query = 0,
      IndexedPropertyDeleterCallback deleter = 0,
      IndexedPropertyEnumeratorCallback enumerator = 0,
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

class V8_EXPORT ExternalOneByteStringResourceImpl
    : public String::ExternalOneByteStringResource {
 public:
  ExternalOneByteStringResourceImpl() : data_(0), length_(0) {}
  ExternalOneByteStringResourceImpl(const char* data, size_t length)
      : data_(data), length_(length) {}
  const char* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};

/**
 * Ignore
 */
class V8_EXPORT Extension {  // NOLINT
 public:
  // Note that the strings passed into this constructor must live as long
  // as the Extension itself.
  Extension(const char* name,
            const char* source = 0,
            int dep_count = 0,
            const char** deps = 0,
            int source_length = -1);
  virtual ~Extension() { }
  virtual Local<FunctionTemplate> GetNativeFunctionTemplate(
      Isolate* isolate, Local<String> name) {
    return Local<FunctionTemplate>();
  }

  const char* name() const { return name_; }
  size_t source_length() const { return source_length_; }
  const String::ExternalOneByteStringResource* source() const {
    return &source_; }
  int dependency_count() { return dep_count_; }
  const char** dependencies() { return deps_; }
  void set_auto_enable(bool value) { auto_enable_ = value; }
  bool auto_enable() { return auto_enable_; }

  // Disallow copying and assigning.
  Extension(const Extension&) = delete;
  void operator=(const Extension&) = delete;

 private:
  const char* name_;
  size_t source_length_;  // expected to initialize before source_
  ExternalOneByteStringResourceImpl source_;
  int dep_count_;
  const char** deps_;
  bool auto_enable_;
};


void V8_EXPORT RegisterExtension(Extension* extension);


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
  ResourceConstraints();

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

  // Returns the max semi-space size in MB.
  V8_DEPRECATE_SOON("Use max_semi_space_size_in_kb()",
                    size_t max_semi_space_size()) {
    return max_semi_space_size_in_kb_ / 1024;
  }

  // Sets the max semi-space size in MB.
  V8_DEPRECATE_SOON("Use set_max_semi_space_size_in_kb(size_t limit_in_kb)",
                    void set_max_semi_space_size(size_t limit_in_mb)) {
    max_semi_space_size_in_kb_ = limit_in_mb * 1024;
  }

  // Returns the max semi-space size in KB.
  size_t max_semi_space_size_in_kb() const {
    return max_semi_space_size_in_kb_;
  }

  // Sets the max semi-space size in KB.
  void set_max_semi_space_size_in_kb(size_t limit_in_kb) {
    max_semi_space_size_in_kb_ = limit_in_kb;
  }

  size_t max_old_space_size() const { return max_old_space_size_; }
  void set_max_old_space_size(size_t limit_in_mb) {
    max_old_space_size_ = limit_in_mb;
  }
  V8_DEPRECATE_SOON("max_executable_size_ is subsumed by max_old_space_size_",
                    size_t max_executable_size() const) {
    return max_executable_size_;
  }
  V8_DEPRECATE_SOON("max_executable_size_ is subsumed by max_old_space_size_",
                    void set_max_executable_size(size_t limit_in_mb)) {
    max_executable_size_ = limit_in_mb;
  }
  uint32_t* stack_limit() const { return stack_limit_; }
  // Sets an address beyond which the VM's stack may not grow.
  void set_stack_limit(uint32_t* value) { stack_limit_ = value; }
  size_t code_range_size() const { return code_range_size_; }
  void set_code_range_size(size_t limit_in_mb) {
    code_range_size_ = limit_in_mb;
  }
  size_t max_zone_pool_size() const { return max_zone_pool_size_; }
  void set_max_zone_pool_size(size_t bytes) { max_zone_pool_size_ = bytes; }

 private:
  // max_semi_space_size_ is in KB
  size_t max_semi_space_size_in_kb_;

  // The remaining limits are in MB
  size_t max_old_space_size_;
  size_t max_executable_size_;
  uint32_t* stack_limit_;
  size_t code_range_size_;
  size_t max_zone_pool_size_;
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

// --- Enter/Leave Script Callback ---
typedef void (*BeforeCallEnteredCallback)(Isolate*);
typedef void (*CallCompletedCallback)(Isolate*);
typedef void (*DeprecatedCallCompletedCallback)();

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
  kPromiseHandlerAddedAfterReject = 1
};

class PromiseRejectMessage {
 public:
  PromiseRejectMessage(Local<Promise> promise, PromiseRejectEvent event,
                       Local<Value> value, Local<StackTrace> stack_trace)
      : promise_(promise),
        event_(event),
        value_(value),
        stack_trace_(stack_trace) {}

  V8_INLINE Local<Promise> GetPromise() const { return promise_; }
  V8_INLINE PromiseRejectEvent GetEvent() const { return event_; }
  V8_INLINE Local<Value> GetValue() const { return value_; }

 private:
  Local<Promise> promise_;
  PromiseRejectEvent event_;
  Local<Value> value_;
  Local<StackTrace> stack_trace_;
};

typedef void (*PromiseRejectCallback)(PromiseRejectMessage message);

// --- Microtasks Callbacks ---
typedef void (*MicrotasksCompletedCallback)(Isolate*);
typedef void (*MicrotaskCallback)(void* data);


/**
 * Policy for running microtasks:
 *   - explicit: microtasks are invoked with Isolate::RunMicrotasks() method;
 *   - scoped: microtasks invocation is controlled by MicrotasksScope objects;
 *   - auto: microtasks are invoked when the script call depth decrements
 *           to zero.
 */
enum class MicrotasksPolicy { kExplicit, kScoped, kAuto };


/**
 * This scope is used to control microtasks when kScopeMicrotasksInvocation
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

// --- WebAssembly compilation callbacks ---
typedef bool (*ExtensionCallback)(const FunctionCallbackInfo<Value>&);

typedef bool (*AllowWasmCodeGenerationCallback)(Local<Context> context,
                                                Local<String> source);

// --- Callback for APIs defined on v8-supported objects, but implemented
// by the embedder. Example: WebAssembly.{compile|instantiate}Streaming ---
typedef void (*ApiImplementationCallback)(const FunctionCallbackInfo<Value>&);

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
 * Collection of V8 heap information.
 *
 * Instances of this class can be passed to v8::V8::HeapStatistics to
 * get heap statistics from V8.
 */
class V8_EXPORT HeapStatistics {
 public:
  HeapStatistics();
  size_t total_heap_size() { return total_heap_size_; }
  size_t total_heap_size_executable() { return total_heap_size_executable_; }
  size_t total_physical_size() { return total_physical_size_; }
  size_t total_available_size() { return total_available_size_; }
  size_t used_heap_size() { return used_heap_size_; }
  size_t heap_size_limit() { return heap_size_limit_; }
  size_t malloced_memory() { return malloced_memory_; }
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
  size_t peak_malloced_memory_;
  bool does_zap_garbage_;
  size_t number_of_native_contexts_;
  size_t number_of_detached_contexts_;

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

 private:
  size_t code_and_metadata_size_;
  size_t bytecode_and_metadata_size_;

  friend class Isolate;
};

class RetainedObjectInfo;


/**
 * FunctionEntryHook is the type of the profile entry hook called at entry to
 * any generated function when function-level profiling is enabled.
 *
 * \param function the address of the function that's being entered.
 * \param return_addr_location points to a location on stack where the machine
 *    return address resides. This can be used to identify the caller of
 *    \p function, and/or modified to divert execution when \p function exits.
 *
 * \note the entry hook must not cause garbage collection.
 */
typedef void (*FunctionEntryHook)(uintptr_t function,
                                  uintptr_t return_addr_location);

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

  union {
    // Only valid for CODE_ADDED.
    struct name_t name;

    // Only valid for CODE_ADD_LINE_POS_INFO
    struct line_info_t line_info;

    // New location of instructions. Only valid for CODE_MOVED.
    void* new_code_start;
  };
};

/**
 * Option flags passed to the SetRAILMode function.
 * See documentation https://developers.google.com/web/tools/chrome-devtools/
 * profile/evaluate-performance/rail
 */
enum RAILMode {
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
 * Interface for iterating through all external resources in the heap.
 */
class V8_EXPORT ExternalResourceVisitor {  // NOLINT
 public:
  virtual ~ExternalResourceVisitor() {}
  virtual void VisitExternalString(Local<String> string) {}
};


/**
 * Interface for iterating through all the persistent handles in the heap.
 */
class V8_EXPORT PersistentHandleVisitor {  // NOLINT
 public:
  virtual ~PersistentHandleVisitor() {}
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
 * Interface for tracing through the embedder heap. During a v8 garbage
 * collection, v8 collects hidden fields of all potential wrappers, and at the
 * end of its marking phase iterates the collection and asks the embedder to
 * trace through its heap and use reporter to report each JavaScript object
 * reachable from any of the given wrappers.
 *
 * Before the first call to the TraceWrappersFrom function TracePrologue will be
 * called. When the garbage collection cycle is finished, TraceEpilogue will be
 * called.
 */
class V8_EXPORT EmbedderHeapTracer {
 public:
  enum ForceCompletionAction { FORCE_COMPLETION, DO_NOT_FORCE_COMPLETION };

  struct AdvanceTracingActions {
    explicit AdvanceTracingActions(ForceCompletionAction force_completion_)
        : force_completion(force_completion_) {}

    ForceCompletionAction force_completion;
  };

  /**
   * Called by v8 to register internal fields of found wrappers.
   *
   * The embedder is expected to store them somewhere and trace reachable
   * wrappers from them when called through |AdvanceTracing|.
   */
  virtual void RegisterV8References(
      const std::vector<std::pair<void*, void*> >& embedder_fields) = 0;

  /**
   * Called at the beginning of a GC cycle.
   */
  virtual void TracePrologue() = 0;

  /**
   * Called to to make a tracing step in the embedder.
   *
   * The embedder is expected to trace its heap starting from wrappers reported
   * by RegisterV8References method, and report back all reachable wrappers.
   * Furthermore, the embedder is expected to stop tracing by the given
   * deadline.
   *
   * Returns true if there is still work to do.
   */
  virtual bool AdvanceTracing(double deadline_in_ms,
                              AdvanceTracingActions actions) = 0;

  /**
   * Called at the end of a GC cycle.
   *
   * Note that allocation is *not* allowed within |TraceEpilogue|.
   */
  virtual void TraceEpilogue() = 0;

  /**
   * Called upon entering the final marking pause. No more incremental marking
   * steps will follow this call.
   */
  virtual void EnterFinalPause() = 0;

  /**
   * Called when tracing is aborted.
   *
   * The embedder is expected to throw away all intermediate data and reset to
   * the initial state.
   */
  virtual void AbortTracing() = 0;

  /**
   * Returns the number of wrappers that are still to be traced by the embedder.
   */
  virtual size_t NumberOfWrappersToTrace() { return 0; }

 protected:
  virtual ~EmbedderHeapTracer() = default;
};

/**
 * Callback and supporting data used in SnapshotCreator to implement embedder
 * logic to serialize internal fields.
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
        : entry_hook(nullptr),
          code_event_handler(nullptr),
          snapshot_blob(nullptr),
          counter_lookup_callback(nullptr),
          create_histogram_callback(nullptr),
          add_histogram_sample_callback(nullptr),
          array_buffer_allocator(nullptr),
          external_references(nullptr),
          allow_atomics_wait(true) {}

    /**
     * The optional entry_hook allows the host application to provide the
     * address of a function that's invoked on entry to every V8-generated
     * function.  Note that entry_hook is invoked at the very start of each
     * generated function.
     * An entry_hook can only be provided in no-snapshot builds; in snapshot
     * builds it must be nullptr.
     */
    FunctionEntryHook entry_hook;

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
     */
    ArrayBuffer::Allocator* array_buffer_allocator;

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
    enum OnFailure { CRASH_ON_FAILURE, THROW_ON_FAILURE };

    DisallowJavascriptExecutionScope(Isolate* isolate, OnFailure on_failure);
    ~DisallowJavascriptExecutionScope();

    // Prevent copying of Scope objects.
    DisallowJavascriptExecutionScope(const DisallowJavascriptExecutionScope&) =
        delete;
    DisallowJavascriptExecutionScope& operator=(
        const DisallowJavascriptExecutionScope&) = delete;

   private:
    bool on_failure_;
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
  };

  /**
   * Do not run microtasks while this scope is active, even if microtasks are
   * automatically executed otherwise.
   */
  class V8_EXPORT SuppressMicrotaskExecutionScope {
   public:
    explicit SuppressMicrotaskExecutionScope(Isolate* isolate);
    ~SuppressMicrotaskExecutionScope();

    // Prevent copying of Scope objects.
    SuppressMicrotaskExecutionScope(const SuppressMicrotaskExecutionScope&) =
        delete;
    SuppressMicrotaskExecutionScope& operator=(
        const SuppressMicrotaskExecutionScope&) = delete;

   private:
    internal::Isolate* const isolate_;
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

    // If you add new values here, you'll also need to update Chromium's:
    // web_feature.mojom, UseCounterCallback.cpp, and enums.xml. V8 changes to
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
   * Returns CPU profiler for this isolate. Will return NULL unless the isolate
   * is initialized. It is the embedder's responsibility to stop all CPU
   * profiling activities if it has started any.
   */
  V8_DEPRECATED("CpuProfiler should be created with CpuProfiler::New call.",
                CpuProfiler* GetCpuProfiler());

  /**
   * Tells the CPU profiler whether the embedder is idle.
   */
  void SetIdle(bool is_idle);

  /** Returns true if this isolate has a current context. */
  bool InContext();

  /**
   * Returns the context of the currently running JavaScript, or the context
   * on the top of the stack if no JavaScript is running.
   */
  Local<Context> GetCurrentContext();

  /**
   * Returns the context of the calling JavaScript code.  That is the
   * context of the top-most JavaScript frame.  If there are no
   * JavaScript frames an empty handle is returned.
   */
  V8_DEPRECATED(
      "Calling context concept is not compatible with tail calls, and will be "
      "removed.",
      Local<Context> GetCallingContext());

  /** Returns the last context entered through V8's C++ API. */
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
  V8_DEPRECATED(
      "Use callback with parameter",
      void AddCallCompletedCallback(DeprecatedCallCompletedCallback callback));

  /**
   * Removes callback that was installed by AddCallCompletedCallback.
   */
  void RemoveCallCompletedCallback(CallCompletedCallback callback);
  V8_DEPRECATED("Use callback with parameter",
                void RemoveCallCompletedCallback(
                    DeprecatedCallCompletedCallback callback));

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
   * Runs the Microtask Work Queue until empty
   * Any exceptions thrown by microtask callbacks are swallowed.
   */
  void RunMicrotasks();

  /**
   * Enqueues the callback to the Microtask Work Queue
   */
  void EnqueueMicrotask(Local<Function> microtask);

  /**
   * Enqueues the callback to the Microtask Work Queue
   */
  void EnqueueMicrotask(MicrotaskCallback callback, void* data = nullptr);

  /**
   * Controls how Microtasks are invoked. See MicrotasksPolicy for details.
   */
  void SetMicrotasksPolicy(MicrotasksPolicy policy);
  V8_DEPRECATED("Use SetMicrotasksPolicy",
                void SetAutorunMicrotasks(bool autorun));

  /**
   * Returns the policy controlling how Microtasks are invoked.
   */
  MicrotasksPolicy GetMicrotasksPolicy() const;
  V8_DEPRECATED("Use GetMicrotasksPolicy", bool WillAutorunMicrotasks() const);

  /**
   * Adds a callback to notify the host application after
   * microtasks were run. The callback is triggered by explicit RunMicrotasks
   * call or automatic microtasks execution (see SetAutorunMicrotasks).
   *
   * Callback will trigger even if microtasks were attempted to run,
   * but the microtasks queue was empty and no single microtask was actually
   * executed.
   *
   * Executing scriptsinside the callback will not re-trigger microtasks and
   * the callback.
   */
  void AddMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);

  /**
   * Removes callback that was installed by AddMicrotasksCompletedCallback.
   */
  void RemoveMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);

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
   * Optional notification that a context has been disposed. V8 uses
   * these notifications to guide the GC heuristic. Returns the number
   * of context disposals - including this one - since the last time
   * V8 had a chance to clean up.
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
   * Returns a memory range that can potentially contain jitted code.
   *
   * On Win64, embedders are advised to install function table callbacks for
   * these ranges, as default SEH won't be able to unwind through jitted code.
   *
   * The first page of the code range is reserved for the embedder and is
   * committed, writable, and executable.
   *
   * Might be empty on other platforms.
   *
   * https://code.google.com/p/v8/issues/detail?id=3598
   */
  void GetCodeRange(void** start, size_t* length_in_bytes);

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
   * Set the callback to invoke to check if code generation from
   * strings should be allowed.
   */
  void SetAllowCodeGenerationFromStringsCallback(
      AllowCodeGenerationFromStringsCallback callback);

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

  void SetWasmCompileStreamingCallback(ApiImplementationCallback callback);

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
   * that have class_ids and are candidates to be marked as partially dependent
   * handles. This will visit handles to young objects created since the last
   * garbage collection but is free to visit an arbitrary superset of these
   * objects.
   */
  void VisitHandlesForPartialDependence(PersistentHandleVisitor* visitor);

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

  internal::Object** GetDataFromSnapshotOnce(size_t index);
  void ReportExternalAllocationLimitReached();
  void CheckMemoryPressure();
};

class V8_EXPORT StartupData {
 public:
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
  static void SetNativesDataBlob(StartupData* startup_blob);
  static void SetSnapshotDataBlob(StartupData* startup_blob);

  /**
   * Bootstrap an isolate and a context from scratch to create a startup
   * snapshot. Include the side-effects of running the optional script.
   * Returns { NULL, 0 } on failure.
   * The caller acquires ownership of the data array in the return value.
   */
  static StartupData CreateSnapshotDataBlob(const char* embedded_source = NULL);

  /**
   * Bootstrap an isolate and a context from the cold startup blob, run the
   * warm-up script to trigger code compilation. The side effects are then
   * discarded. The resulting startup snapshot will include compiled code.
   * Returns { NULL, 0 } on failure.
   * The caller acquires ownership of the data array in the return value.
   * The argument startup blob is untouched.
   */
  static StartupData WarmUpSnapshotDataBlob(StartupData cold_startup_blob,
                                            const char* warmup_source);

  /** Set the callback to invoke in case of Dcheck failures. */
  static void SetDcheckErrorHandler(DcheckErrorCallback that);


  /**
   * Sets V8 flags from a string.
   */
  static void SetFlagsFromString(const char* str, int length);

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
  static bool Initialize();

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
   *   This will look in the given directory for files "natives_blob.bin"
   *   and "snapshot_blob.bin" - which is what the default build calls them.
   * - InitializeExternalStartupData(const char*, const char*)
   *   As above, but will directly use the two given file names.
   * - Call SetNativesDataBlob, SetNativesDataBlob.
   *   This will read the blobs from the given data structures and will
   *   not perform any file IO.
   */
  static void InitializeExternalStartupData(const char* directory_path);
  static void InitializeExternalStartupData(const char* natives_blob,
                                            const char* snapshot_blob);
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
  static bool TryHandleSignal(int signal_number, void* info, void* context);
#endif  // V8_OS_POSIX

  /**
   * Enable the default signal handler rather than using one provided by the
   * embedder.
   */
  V8_DEPRECATE_SOON("Use EnableWebAssemblyTrapHandler",
                    static bool RegisterDefaultSignalHandler());

  /**
   * Activate trap-based bounds checking for WebAssembly.
   *
   * \param use_v8_signal_handler Whether V8 should install its own signal
   * handler or rely on the embedder's.
   */
  static bool EnableWebAssemblyTrapHandler(bool use_v8_signal_handler);

 private:
  V8();

  static internal::Object** GlobalizeReference(internal::Isolate* isolate,
                                               internal::Object** handle);
  static internal::Object** CopyPersistent(internal::Object** handle);
  static void DisposeGlobal(internal::Object** global_handle);
  static void MakeWeak(internal::Object** location, void* data,
                       WeakCallbackInfo<void>::Callback weak_callback,
                       WeakCallbackType type);
  static void MakeWeak(internal::Object** location, void* data,
                       // Must be 0 or -1.
                       int internal_field_index1,
                       // Must be 1 or -1.
                       int internal_field_index2,
                       WeakCallbackInfo<void>::Callback weak_callback);
  static void MakeWeak(internal::Object*** location_addr);
  static void* ClearWeak(internal::Object** location);
  static void AnnotateStrongRetainer(internal::Object** location,
                                     const char* label);
  static Value* Eternalize(Isolate* isolate, Value* handle);

  static void RegisterExternallyReferencedObject(internal::Object** object,
                                                 internal::Isolate* isolate);

  template <class K, class V, class T>
  friend class PersistentValueMapBase;

  static void FromJustIsNothing();
  static void ToLocalEmpty();
  static void InternalFieldOutOfBounds(int index);
  template <class T> friend class Local;
  template <class T>
  friend class MaybeLocal;
  template <class T>
  friend class Maybe;
  template <class T>
  friend class WeakCallbackInfo;
  template <class T> friend class Eternal;
  template <class T> friend class PersistentBase;
  template <class T, class M> friend class Persistent;
  friend class Context;
};

/**
 * Helper class to create a snapshot data blob.
 */
class V8_EXPORT SnapshotCreator {
 public:
  enum class FunctionCodeHandling { kClear, kKeep };

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
   * Add a template to be included in the snapshot blob.
   * \returns the index of the template in the snapshot blob.
   */
  size_t AddTemplate(Local<Template> template_obj);

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
  size_t AddData(Local<Context> context, internal::Object* object);
  size_t AddData(internal::Object* object);

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
   * Returns the .stack property of the thrown object.  If no .stack
   * property is present an empty handle is returned.
   */
  V8_DEPRECATED("Use maybe version.", Local<Value> StackTrace() const);
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
    if (handler == NULL) return NULL;
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
  ExtensionConfiguration() : name_count_(0), names_(NULL) { }
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
      Isolate* isolate, ExtensionConfiguration* extensions = NULL,
      MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
      MaybeLocal<Value> global_object = MaybeLocal<Value>(),
      DeserializeInternalFieldsCallback internal_fields_deserializer =
          DeserializeInternalFieldsCallback());

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
      MaybeLocal<Value> global_object = MaybeLocal<Value>());

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
  class V8_EXPORT BackupIncumbentScope {
   public:
    /**
     * |backup_incumbent_context| is pushed onto the backup incumbent settings
     * object stack.
     */
    explicit BackupIncumbentScope(Local<Context> backup_incumbent_context);
    ~BackupIncumbentScope();

   private:
    friend class internal::Isolate;

    Local<Context> backup_incumbent_context_;
    const BackupIncumbentScope* prev_ = nullptr;
  };

 private:
  friend class Value;
  friend class Script;
  friend class Object;
  friend class Function;

  internal::Object** GetDataFromSnapshotOnce(size_t index);
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


// --- Implementation ---


namespace internal {

const int kApiPointerSize = sizeof(void*);  // NOLINT
const int kApiIntSize = sizeof(int);  // NOLINT
const int kApiInt64Size = sizeof(int64_t);  // NOLINT

// Tag information for HeapObject.
const int kHeapObjectTag = 1;
const int kWeakHeapObjectTag = 3;
const int kHeapObjectTagSize = 2;
const intptr_t kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1;

// Tag information for Smi.
const int kSmiTag = 0;
const int kSmiTagSize = 1;
const intptr_t kSmiTagMask = (1 << kSmiTagSize) - 1;

template <size_t ptr_size> struct SmiTagging;

template<int kSmiShiftSize>
V8_INLINE internal::Object* IntToSmi(int value) {
  int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
  uintptr_t tagged_value =
      (static_cast<uintptr_t>(value) << smi_shift_bits) | kSmiTag;
  return reinterpret_cast<internal::Object*>(tagged_value);
}

// Smi constants for 32-bit systems.
template <> struct SmiTagging<4> {
  enum { kSmiShiftSize = 0, kSmiValueSize = 31 };
  static int SmiShiftSize() { return kSmiShiftSize; }
  static int SmiValueSize() { return kSmiValueSize; }
  V8_INLINE static int SmiToInt(const internal::Object* value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Throw away top 32 bits and shift down (requires >> to be sign extending).
    return static_cast<int>(reinterpret_cast<intptr_t>(value)) >> shift_bits;
  }
  V8_INLINE static internal::Object* IntToSmi(int value) {
    return internal::IntToSmi<kSmiShiftSize>(value);
  }
  V8_INLINE static bool IsValidSmi(intptr_t value) {
    // To be representable as an tagged small integer, the two
    // most-significant bits of 'value' must be either 00 or 11 due to
    // sign-extension. To check this we add 01 to the two
    // most-significant bits, and check if the most-significant bit is 0
    //
    // CAUTION: The original code below:
    // bool result = ((value + 0x40000000) & 0x80000000) == 0;
    // may lead to incorrect results according to the C language spec, and
    // in fact doesn't work correctly with gcc4.1.1 in some cases: The
    // compiler may produce undefined results in case of signed integer
    // overflow. The computation must be done w/ unsigned ints.
    return static_cast<uintptr_t>(value + 0x40000000U) < 0x80000000U;
  }
};

// Smi constants for 64-bit systems.
template <> struct SmiTagging<8> {
  enum { kSmiShiftSize = 31, kSmiValueSize = 32 };
  static int SmiShiftSize() { return kSmiShiftSize; }
  static int SmiValueSize() { return kSmiValueSize; }
  V8_INLINE static int SmiToInt(const internal::Object* value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Shift down and throw away top 32 bits.
    return static_cast<int>(reinterpret_cast<intptr_t>(value) >> shift_bits);
  }
  V8_INLINE static internal::Object* IntToSmi(int value) {
    return internal::IntToSmi<kSmiShiftSize>(value);
  }
  V8_INLINE static bool IsValidSmi(intptr_t value) {
    // To be representable as a long smi, the value must be a 32-bit integer.
    return (value == static_cast<int32_t>(value));
  }
};

typedef SmiTagging<kApiPointerSize> PlatformSmiTagging;
const int kSmiShiftSize = PlatformSmiTagging::kSmiShiftSize;
const int kSmiValueSize = PlatformSmiTagging::kSmiValueSize;
V8_INLINE static bool SmiValuesAre31Bits() { return kSmiValueSize == 31; }
V8_INLINE static bool SmiValuesAre32Bits() { return kSmiValueSize == 32; }

/**
 * This class exports constants and functionality from within v8 that
 * is necessary to implement inline functions in the v8 api.  Don't
 * depend on functions and constants defined here.
 */
class Internals {
 public:
  // These values match non-compiler-dependent values defined within
  // the implementation of v8.
  static const int kHeapObjectMapOffset = 0;
  static const int kMapInstanceTypeOffset = 1 * kApiPointerSize + kApiIntSize;
  static const int kStringResourceOffset = 3 * kApiPointerSize;

  static const int kOddballKindOffset = 4 * kApiPointerSize + sizeof(double);
  static const int kForeignAddressOffset = kApiPointerSize;
  static const int kJSObjectHeaderSize = 3 * kApiPointerSize;
  static const int kFixedArrayHeaderSize = 2 * kApiPointerSize;
  static const int kContextHeaderSize = 2 * kApiPointerSize;
  static const int kContextEmbedderDataIndex = 5;
  static const int kFullStringRepresentationMask = 0x0f;
  static const int kStringEncodingMask = 0x8;
  static const int kExternalTwoByteRepresentationTag = 0x02;
  static const int kExternalOneByteRepresentationTag = 0x0a;

  static const int kIsolateEmbedderDataOffset = 0 * kApiPointerSize;
  static const int kExternalMemoryOffset = 4 * kApiPointerSize;
  static const int kExternalMemoryLimitOffset =
      kExternalMemoryOffset + kApiInt64Size;
  static const int kExternalMemoryAtLastMarkCompactOffset =
      kExternalMemoryLimitOffset + kApiInt64Size;
  static const int kIsolateRootsOffset = kExternalMemoryLimitOffset +
                                         kApiInt64Size + kApiInt64Size +
                                         kApiPointerSize + kApiPointerSize;
  static const int kUndefinedValueRootIndex = 4;
  static const int kTheHoleValueRootIndex = 5;
  static const int kNullValueRootIndex = 6;
  static const int kTrueValueRootIndex = 7;
  static const int kFalseValueRootIndex = 8;
  static const int kEmptyStringRootIndex = 9;

  static const int kNodeClassIdOffset = 1 * kApiPointerSize;
  static const int kNodeFlagsOffset = 1 * kApiPointerSize + 3;
  static const int kNodeStateMask = 0x7;
  static const int kNodeStateIsWeakValue = 2;
  static const int kNodeStateIsPendingValue = 3;
  static const int kNodeStateIsNearDeathValue = 4;
  static const int kNodeIsIndependentShift = 3;
  static const int kNodeIsActiveShift = 4;

  static const int kFirstNonstringType = 0x80;
  static const int kOddballType = 0x83;
  static const int kForeignType = 0x87;
  static const int kJSSpecialApiObjectType = 0x410;
  static const int kJSApiObjectType = 0x420;
  static const int kJSObjectType = 0x421;

  static const int kUndefinedOddballKind = 5;
  static const int kNullOddballKind = 3;

  static const uint32_t kNumIsolateDataSlots = 4;

  V8_EXPORT static void CheckInitializedImpl(v8::Isolate* isolate);
  V8_INLINE static void CheckInitialized(v8::Isolate* isolate) {
#ifdef V8_ENABLE_CHECKS
    CheckInitializedImpl(isolate);
#endif
  }

  V8_INLINE static bool HasHeapObjectTag(const internal::Object* value) {
    return ((reinterpret_cast<intptr_t>(value) & kHeapObjectTagMask) ==
            kHeapObjectTag);
  }

  V8_INLINE static int SmiValue(const internal::Object* value) {
    return PlatformSmiTagging::SmiToInt(value);
  }

  V8_INLINE static internal::Object* IntToSmi(int value) {
    return PlatformSmiTagging::IntToSmi(value);
  }

  V8_INLINE static bool IsValidSmi(intptr_t value) {
    return PlatformSmiTagging::IsValidSmi(value);
  }

  V8_INLINE static int GetInstanceType(const internal::Object* obj) {
    typedef internal::Object O;
    O* map = ReadField<O*>(obj, kHeapObjectMapOffset);
    return ReadField<uint16_t>(map, kMapInstanceTypeOffset);
  }

  V8_INLINE static int GetOddballKind(const internal::Object* obj) {
    typedef internal::Object O;
    return SmiValue(ReadField<O*>(obj, kOddballKindOffset));
  }

  V8_INLINE static bool IsExternalTwoByteString(int instance_type) {
    int representation = (instance_type & kFullStringRepresentationMask);
    return representation == kExternalTwoByteRepresentationTag;
  }

  V8_INLINE static uint8_t GetNodeFlag(internal::Object** obj, int shift) {
      uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
      return *addr & static_cast<uint8_t>(1U << shift);
  }

  V8_INLINE static void UpdateNodeFlag(internal::Object** obj,
                                       bool value, int shift) {
      uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
      uint8_t mask = static_cast<uint8_t>(1U << shift);
      *addr = static_cast<uint8_t>((*addr & ~mask) | (value << shift));
  }

  V8_INLINE static uint8_t GetNodeState(internal::Object** obj) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    return *addr & kNodeStateMask;
  }

  V8_INLINE static void UpdateNodeState(internal::Object** obj,
                                        uint8_t value) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + kNodeFlagsOffset;
    *addr = static_cast<uint8_t>((*addr & ~kNodeStateMask) | value);
  }

  V8_INLINE static void SetEmbedderData(v8::Isolate* isolate,
                                        uint32_t slot,
                                        void* data) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(isolate) +
                    kIsolateEmbedderDataOffset + slot * kApiPointerSize;
    *reinterpret_cast<void**>(addr) = data;
  }

  V8_INLINE static void* GetEmbedderData(const v8::Isolate* isolate,
                                         uint32_t slot) {
    const uint8_t* addr = reinterpret_cast<const uint8_t*>(isolate) +
        kIsolateEmbedderDataOffset + slot * kApiPointerSize;
    return *reinterpret_cast<void* const*>(addr);
  }

  V8_INLINE static internal::Object** GetRoot(v8::Isolate* isolate,
                                              int index) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(isolate) + kIsolateRootsOffset;
    return reinterpret_cast<internal::Object**>(addr + index * kApiPointerSize);
  }

  template <typename T>
  V8_INLINE static T ReadField(const internal::Object* ptr, int offset) {
    const uint8_t* addr =
        reinterpret_cast<const uint8_t*>(ptr) + offset - kHeapObjectTag;
    return *reinterpret_cast<const T*>(addr);
  }

  template <typename T>
  V8_INLINE static T ReadEmbedderData(const v8::Context* context, int index) {
    typedef internal::Object O;
    typedef internal::Internals I;
    O* ctx = *reinterpret_cast<O* const*>(context);
    int embedder_data_offset = I::kContextHeaderSize +
        (internal::kApiPointerSize * I::kContextEmbedderDataIndex);
    O* embedder_data = I::ReadField<O*>(ctx, embedder_data_offset);
    int value_offset =
        I::kFixedArrayHeaderSize + (internal::kApiPointerSize * index);
    return I::ReadField<T>(embedder_data, value_offset);
  }
};

// Only perform cast check for types derived from v8::Data since
// other types do not implement the Cast method.
template <bool PerformCheck>
struct CastCheck {
  template <class T>
  static void Perform(T* data);
};

template <>
template <class T>
void CastCheck<true>::Perform(T* data) {
  T::Cast(data);
}

template <>
template <class T>
void CastCheck<false>::Perform(T* data) {}

template <class T>
V8_INLINE void PerformCastCheck(T* data) {
  CastCheck<std::is_base_of<Data, T>::value>::Perform(data);
}

}  // namespace internal


template <class T>
Local<T> Local<T>::New(Isolate* isolate, Local<T> that) {
  return New(isolate, that.val_);
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, const PersistentBase<T>& that) {
  return New(isolate, that.val_);
}


template <class T>
Local<T> Local<T>::New(Isolate* isolate, T* that) {
  if (that == NULL) return Local<T>();
  T* that_ptr = that;
  internal::Object** p = reinterpret_cast<internal::Object**>(that_ptr);
  return Local<T>(reinterpret_cast<T*>(HandleScope::CreateHandle(
      reinterpret_cast<internal::Isolate*>(isolate), *p)));
}


template<class T>
template<class S>
void Eternal<T>::Set(Isolate* isolate, Local<S> handle) {
  TYPE_CHECK(T, S);
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
  if (that == NULL) return NULL;
  internal::Object** p = reinterpret_cast<internal::Object**>(that);
  return reinterpret_cast<T*>(
      V8::GlobalizeReference(reinterpret_cast<internal::Isolate*>(isolate),
                             p));
}


template <class T, class M>
template <class S, class M2>
void Persistent<T, M>::Copy(const Persistent<S, M2>& that) {
  TYPE_CHECK(T, S);
  this->Reset();
  if (that.IsEmpty()) return;
  internal::Object** p = reinterpret_cast<internal::Object**>(that.val_);
  this->val_ = reinterpret_cast<T*>(V8::CopyPersistent(p));
  M::Copy(that, this);
}

template <class T>
bool PersistentBase<T>::IsIndependent() const {
  typedef internal::Internals I;
  if (this->IsEmpty()) return false;
  return I::GetNodeFlag(reinterpret_cast<internal::Object**>(this->val_),
                        I::kNodeIsIndependentShift);
}

template <class T>
bool PersistentBase<T>::IsNearDeath() const {
  typedef internal::Internals I;
  if (this->IsEmpty()) return false;
  uint8_t node_state =
      I::GetNodeState(reinterpret_cast<internal::Object**>(this->val_));
  return node_state == I::kNodeStateIsNearDeathValue ||
      node_state == I::kNodeStateIsPendingValue;
}


template <class T>
bool PersistentBase<T>::IsWeak() const {
  typedef internal::Internals I;
  if (this->IsEmpty()) return false;
  return I::GetNodeState(reinterpret_cast<internal::Object**>(this->val_)) ==
      I::kNodeStateIsWeakValue;
}


template <class T>
void PersistentBase<T>::Reset() {
  if (this->IsEmpty()) return;
  V8::DisposeGlobal(reinterpret_cast<internal::Object**>(this->val_));
  val_ = 0;
}


template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate, const Local<S>& other) {
  TYPE_CHECK(T, S);
  Reset();
  if (other.IsEmpty()) return;
  this->val_ = New(isolate, other.val_);
}


template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate,
                              const PersistentBase<S>& other) {
  TYPE_CHECK(T, S);
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
  V8::MakeWeak(reinterpret_cast<internal::Object**>(this->val_), parameter,
               reinterpret_cast<Callback>(callback), type);
}

template <class T>
void PersistentBase<T>::SetWeak() {
  V8::MakeWeak(reinterpret_cast<internal::Object***>(&this->val_));
}

template <class T>
template <typename P>
P* PersistentBase<T>::ClearWeak() {
  return reinterpret_cast<P*>(
    V8::ClearWeak(reinterpret_cast<internal::Object**>(this->val_)));
}

template <class T>
void PersistentBase<T>::AnnotateStrongRetainer(const char* label) {
  V8::AnnotateStrongRetainer(reinterpret_cast<internal::Object**>(this->val_),
                             label);
}

template <class T>
void PersistentBase<T>::RegisterExternalReference(Isolate* isolate) const {
  if (IsEmpty()) return;
  V8::RegisterExternallyReferencedObject(
      reinterpret_cast<internal::Object**>(this->val_),
      reinterpret_cast<internal::Isolate*>(isolate));
}

template <class T>
void PersistentBase<T>::MarkIndependent() {
  typedef internal::Internals I;
  if (this->IsEmpty()) return;
  I::UpdateNodeFlag(reinterpret_cast<internal::Object**>(this->val_), true,
                    I::kNodeIsIndependentShift);
}

template <class T>
void PersistentBase<T>::MarkActive() {
  typedef internal::Internals I;
  if (this->IsEmpty()) return;
  I::UpdateNodeFlag(reinterpret_cast<internal::Object**>(this->val_), true,
                    I::kNodeIsActiveShift);
}


template <class T>
void PersistentBase<T>::SetWrapperClassId(uint16_t class_id) {
  typedef internal::Internals I;
  if (this->IsEmpty()) return;
  internal::Object** obj = reinterpret_cast<internal::Object**>(this->val_);
  uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + I::kNodeClassIdOffset;
  *reinterpret_cast<uint16_t*>(addr) = class_id;
}


template <class T>
uint16_t PersistentBase<T>::WrapperClassId() const {
  typedef internal::Internals I;
  if (this->IsEmpty()) return 0;
  internal::Object** obj = reinterpret_cast<internal::Object**>(this->val_);
  uint8_t* addr = reinterpret_cast<uint8_t*>(obj) + I::kNodeClassIdOffset;
  return *reinterpret_cast<uint16_t*>(addr);
}


template<typename T>
ReturnValue<T>::ReturnValue(internal::Object** slot) : value_(slot) {}

template<typename T>
template<typename S>
void ReturnValue<T>::Set(const Persistent<S>& handle) {
  TYPE_CHECK(T, S);
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Object**>(*handle);
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Global<S>& handle) {
  TYPE_CHECK(T, S);
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Object**>(*handle);
  }
}

template <typename T>
template <typename S>
void ReturnValue<T>::Set(const Local<S> handle) {
  TYPE_CHECK(T, S);
  if (V8_UNLIKELY(handle.IsEmpty())) {
    *value_ = GetDefaultValue();
  } else {
    *value_ = *reinterpret_cast<internal::Object**>(*handle);
  }
}

template<typename T>
void ReturnValue<T>::Set(double i) {
  TYPE_CHECK(T, Number);
  Set(Number::New(GetIsolate(), i));
}

template<typename T>
void ReturnValue<T>::Set(int32_t i) {
  TYPE_CHECK(T, Integer);
  typedef internal::Internals I;
  if (V8_LIKELY(I::IsValidSmi(i))) {
    *value_ = I::IntToSmi(i);
    return;
  }
  Set(Integer::New(GetIsolate(), i));
}

template<typename T>
void ReturnValue<T>::Set(uint32_t i) {
  TYPE_CHECK(T, Integer);
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
  TYPE_CHECK(T, Boolean);
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
  TYPE_CHECK(T, Primitive);
  typedef internal::Internals I;
  *value_ = *I::GetRoot(GetIsolate(), I::kNullValueRootIndex);
}

template<typename T>
void ReturnValue<T>::SetUndefined() {
  TYPE_CHECK(T, Primitive);
  typedef internal::Internals I;
  *value_ = *I::GetRoot(GetIsolate(), I::kUndefinedValueRootIndex);
}

template<typename T>
void ReturnValue<T>::SetEmptyString() {
  TYPE_CHECK(T, String);
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
  // Uncompilable to prevent inadvertent misuse.
  TYPE_CHECK(S*, Primitive);
}

template<typename T>
internal::Object* ReturnValue<T>::GetDefaultValue() {
  // Default value is always the pointer below value_ on the stack.
  return value_[-1];
}

template <typename T>
FunctionCallbackInfo<T>::FunctionCallbackInfo(internal::Object** implicit_args,
                                              internal::Object** values,
                                              int length)
    : implicit_args_(implicit_args), values_(values), length_(length) {}

template<typename T>
Local<Value> FunctionCallbackInfo<T>::operator[](int i) const {
  if (i < 0 || length_ <= i) return Local<Value>(*Undefined(GetIsolate()));
  return Local<Value>(reinterpret_cast<Value*>(values_ - i));
}


template<typename T>
Local<Object> FunctionCallbackInfo<T>::This() const {
  return Local<Object>(reinterpret_cast<Object*>(values_ + 1));
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
  typedef internal::Object O;
  typedef internal::HeapObject HO;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O**>(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  auto instance_type = I::GetInstanceType(obj);
  if (instance_type == I::kJSObjectType ||
      instance_type == I::kJSApiObjectType ||
      instance_type == I::kJSSpecialApiObjectType) {
    int offset = I::kJSObjectHeaderSize + (internal::kApiPointerSize * index);
    O* value = I::ReadField<O*>(obj, offset);
    O** result = HandleScope::CreateHandle(reinterpret_cast<HO*>(obj), value);
    return Local<Value>(reinterpret_cast<Value*>(result));
  }
#endif
  return SlowGetInternalField(index);
}


void* Object::GetAlignedPointerFromInternalField(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O**>(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  auto instance_type = I::GetInstanceType(obj);
  if (V8_LIKELY(instance_type == I::kJSObjectType ||
                instance_type == I::kJSApiObjectType ||
                instance_type == I::kJSSpecialApiObjectType)) {
    int offset = I::kJSObjectHeaderSize + (internal::kApiPointerSize * index);
    return I::ReadField<void*>(obj, offset);
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
  typedef internal::Object* S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kEmptyStringRootIndex);
  return Local<String>(reinterpret_cast<String*>(slot));
}


String::ExternalStringResource* String::GetExternalStringResource() const {
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O* const*>(this);
  String::ExternalStringResource* result;
  if (I::IsExternalTwoByteString(I::GetInstanceType(obj))) {
    void* value = I::ReadField<void*>(obj, I::kStringResourceOffset);
    result = reinterpret_cast<String::ExternalStringResource*>(value);
  } else {
    result = NULL;
  }
#ifdef V8_ENABLE_CHECKS
  VerifyExternalStringResource(result);
#endif
  return result;
}


String::ExternalStringResourceBase* String::GetExternalStringResourceBase(
    String::Encoding* encoding_out) const {
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O* const*>(this);
  int type = I::GetInstanceType(obj) & I::kFullStringRepresentationMask;
  *encoding_out = static_cast<Encoding>(type & I::kStringEncodingMask);
  ExternalStringResourceBase* resource = NULL;
  if (type == I::kExternalOneByteRepresentationTag ||
      type == I::kExternalTwoByteRepresentationTag) {
    void* value = I::ReadField<void*>(obj, I::kStringResourceOffset);
    resource = static_cast<ExternalStringResourceBase*>(value);
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
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O* const*>(this);
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
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O* const*>(this);
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
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O* const*>(this);
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
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O* const*>(this);
  if (!I::HasHeapObjectTag(obj)) return false;
  return (I::GetInstanceType(obj) < I::kFirstNonstringType);
}


template <class T> Value* Value::Cast(T* value) {
  return static_cast<Value*>(value);
}


Local<Boolean> Value::ToBoolean() const {
  return ToBoolean(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Boolean>());
}


Local<String> Value::ToString() const {
  return ToString(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<String>());
}


Local<Object> Value::ToObject() const {
  return ToObject(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Object>());
}


Local<Integer> Value::ToInteger() const {
  return ToInteger(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Integer>());
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

WasmCompiledModule* WasmCompiledModule::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<WasmCompiledModule*>(value);
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
  return args_[kShouldThrowOnErrorIndex] != I::IntToSmi(0);
}


Local<Primitive> Undefined(Isolate* isolate) {
  typedef internal::Object* S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kUndefinedValueRootIndex);
  return Local<Primitive>(reinterpret_cast<Primitive*>(slot));
}


Local<Primitive> Null(Isolate* isolate) {
  typedef internal::Object* S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kNullValueRootIndex);
  return Local<Primitive>(reinterpret_cast<Primitive*>(slot));
}


Local<Boolean> True(Isolate* isolate) {
  typedef internal::Object* S;
  typedef internal::Internals I;
  I::CheckInitialized(isolate);
  S* slot = I::GetRoot(isolate, I::kTrueValueRootIndex);
  return Local<Boolean>(reinterpret_cast<Boolean*>(slot));
}


Local<Boolean> False(Isolate* isolate) {
  typedef internal::Object* S;
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
  const int64_t kMemoryReducerActivationLimit = 32 * 1024 * 1024;
  int64_t* external_memory = reinterpret_cast<int64_t*>(
      reinterpret_cast<uint8_t*>(this) + I::kExternalMemoryOffset);
  int64_t* external_memory_limit = reinterpret_cast<int64_t*>(
      reinterpret_cast<uint8_t*>(this) + I::kExternalMemoryLimitOffset);
  int64_t* external_memory_at_last_mc =
      reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(this) +
                                 I::kExternalMemoryAtLastMarkCompactOffset);
  const int64_t amount = *external_memory + change_in_bytes;

  *external_memory = amount;

  int64_t allocation_diff_since_last_mc =
      *external_memory_at_last_mc - *external_memory;
  allocation_diff_since_last_mc = allocation_diff_since_last_mc < 0
                                      ? -allocation_diff_since_last_mc
                                      : allocation_diff_since_last_mc;
  if (allocation_diff_since_last_mc > kMemoryReducerActivationLimit) {
    CheckMemoryPressure();
  }

  if (change_in_bytes < 0) {
    *external_memory_limit += change_in_bytes;
  }

  if (change_in_bytes > 0 && amount > *external_memory_limit) {
    ReportExternalAllocationLimitReached();
  }
  return *external_memory;
}

Local<Value> Context::GetEmbedderData(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Object O;
  typedef internal::HeapObject HO;
  typedef internal::Internals I;
  HO* context = *reinterpret_cast<HO**>(this);
  O** result =
      HandleScope::CreateHandle(context, I::ReadEmbedderData<O*>(this, index));
  return Local<Value>(reinterpret_cast<Value*>(result));
#else
  return SlowGetEmbedderData(index);
#endif
}


void* Context::GetAlignedPointerFromEmbedderData(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Internals I;
  return I::ReadEmbedderData<void*>(this, index);
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
  internal::Object** p = reinterpret_cast<internal::Object**>(object_ptr);
  return AddData(context, *p);
}

template <class T>
size_t SnapshotCreator::AddData(Local<T> object) {
  T* object_ptr = *object;
  internal::Object** p = reinterpret_cast<internal::Object**>(object_ptr);
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


#undef TYPE_CHECK


#endif  // INCLUDE_V8_H_
