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
#if defined(BUILDING_V8_SHARED) && defined(USING_V8_SHARED)
#error both BUILDING_V8_SHARED and USING_V8_SHARED are set - please check the\
  build configuration to ensure that at most one of these is set
#endif

#ifdef BUILDING_V8_SHARED
# define V8_EXPORT __declspec(dllexport)
#elif USING_V8_SHARED
# define V8_EXPORT __declspec(dllimport)
#else
# define V8_EXPORT
#endif  // BUILDING_V8_SHARED

#else  // V8_OS_WIN

// Setup for Linux shared library export.
#if V8_HAS_ATTRIBUTE_VISIBILITY && defined(V8_SHARED)
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
class Private;
class Uint32;
class Utils;
class Value;
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
class Data;
template<typename T> class FunctionCallbackInfo;
template<typename T> class PropertyCallbackInfo;
class StackTrace;
class StackFrame;
class Isolate;
class CallHandlerHelper;
class EscapableHandleScope;
template<typename T> class ReturnValue;

namespace experimental {
class FastAccessorBuilder;
}  // namespace experimental

namespace internal {
class Arguments;
class Heap;
class HeapObject;
class Isolate;
class Object;
struct StreamedSource;
template<typename T> class CustomArguments;
class PropertyCallbackArguments;
class FunctionCallbackArguments;
class GlobalHandles;
}  // namespace internal


/**
 * General purpose unique identifier.
 */
class UniqueId {
 public:
  explicit UniqueId(intptr_t data)
      : data_(data) {}

  bool operator==(const UniqueId& other) const {
    return data_ == other.data_;
  }

  bool operator!=(const UniqueId& other) const {
    return data_ != other.data_;
  }

  bool operator<(const UniqueId& other) const {
    return data_ < other.data_;
  }

 private:
  intptr_t data_;
};

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
 * Local handles are light-weight and transient and typically used in
 * local operations.  They are managed by HandleScopes.  Persistent
 * handles can be used when storing objects across several independent
 * operations and have to be explicitly deallocated when they're no
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

  template <class S> V8_INLINE static Local<T> Cast(Local<S> that) {
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (that.IsEmpty()) return Local<T>();
#endif
    return Local<T>(T::Cast(*that));
  }


  template <class S> V8_INLINE Local<S> As() {
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

  template <class S>
  V8_INLINE Local(S* that)
      : val_(that) {}
  V8_INLINE static Local<T> New(Isolate* isolate, T* that);
  T* val_;
};


#if !defined(V8_IMMINENT_DEPRECATION_WARNINGS)
// Local is an alias for Local for historical reasons.
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

  template <class S>
  V8_WARN_UNUSED_RESULT V8_INLINE bool ToLocal(Local<S>* out) const {
    out->val_ = IsEmpty() ? nullptr : this->val_;
    return !IsEmpty();
  }

  // Will crash if the MaybeLocal<> is empty.
  V8_INLINE Local<T> ToLocalChecked();

  template <class S>
  V8_INLINE Local<S> FromMaybe(Local<S> default_value) const {
    return IsEmpty() ? default_value : Local<S>(val_);
  }

 private:
  T* val_;
};


// Eternal handles are set-once handles that live for the life of the isolate.
template <class T> class Eternal {
 public:
  V8_INLINE Eternal() : index_(kInitialValue) { }
  template<class S>
  V8_INLINE Eternal(Isolate* isolate, Local<S> handle) : index_(kInitialValue) {
    Set(isolate, handle);
  }
  // Can only be safely called if already set.
  V8_INLINE Local<T> Get(Isolate* isolate);
  V8_INLINE bool IsEmpty() { return index_ == kInitialValue; }
  template<class S> V8_INLINE void Set(Isolate* isolate, Local<S> handle);

 private:
  static const int kInitialValue = -1;
  int index_;
};


static const int kInternalFieldsInWeakCallback = 2;


template <typename T>
class WeakCallbackInfo {
 public:
  typedef void (*Callback)(const WeakCallbackInfo<T>& data);

  WeakCallbackInfo(Isolate* isolate, T* parameter,
                   void* internal_fields[kInternalFieldsInWeakCallback],
                   Callback* callback)
      : isolate_(isolate), parameter_(parameter), callback_(callback) {
    for (int i = 0; i < kInternalFieldsInWeakCallback; ++i) {
      internal_fields_[i] = internal_fields[i];
    }
  }

  V8_INLINE Isolate* GetIsolate() const { return isolate_; }
  V8_INLINE T* GetParameter() const { return parameter_; }
  V8_INLINE void* GetInternalField(int index) const;

  V8_INLINE V8_DEPRECATED("use indexed version",
                          void* GetInternalField1() const) {
    return internal_fields_[0];
  }
  V8_INLINE V8_DEPRECATED("use indexed version",
                          void* GetInternalField2() const) {
    return internal_fields_[1];
  }

  bool IsFirstPass() const { return callback_ != nullptr; }

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
  void* internal_fields_[kInternalFieldsInWeakCallback];
};


template <class T, class P>
class WeakCallbackData {
 public:
  typedef void (*Callback)(const WeakCallbackData<T, P>& data);

  WeakCallbackData(Isolate* isolate, P* parameter, Local<T> handle)
      : isolate_(isolate), parameter_(parameter), handle_(handle) {}

  V8_INLINE Isolate* GetIsolate() const { return isolate_; }
  V8_INLINE P* GetParameter() const { return parameter_; }
  V8_INLINE Local<T> GetValue() const { return handle_; }

 private:
  Isolate* isolate_;
  P* parameter_;
  Local<T> handle_;
};


// TODO(dcarney): delete this with WeakCallbackData
template <class T>
using PhantomCallbackData = WeakCallbackInfo<T>;


enum class WeakCallbackType { kParameter, kInternalFields };


/**
 * An object reference that is independent of any handle scope.  Where
 * a Local handle only lives as long as the HandleScope in which it was
 * allocated, a PersistentBase handle remains valid until it is explicitly
 * disposed.
 *
 * A persistent handle contains a reference to a storage cell within
 * the v8 engine which holds an object value and which is updated by
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
  V8_INLINE V8_DEPRECATED(
      "use WeakCallbackInfo version",
      void SetWeak(P* parameter,
                   typename WeakCallbackData<T, P>::Callback callback));

  template <typename S, typename P>
  V8_INLINE V8_DEPRECATED(
      "use WeakCallbackInfo version",
      void SetWeak(P* parameter,
                   typename WeakCallbackData<S, P>::Callback callback));

  // Phantom persistents work like weak persistents, except that the pointer to
  // the object being collected is not available in the finalization callback.
  // This enables the garbage collector to collect the object and any objects
  // it references transitively in one GC cycle. At the moment you can either
  // specify a parameter for the callback or the location of two internal
  // fields in the dying object.
  template <typename P>
  V8_INLINE V8_DEPRECATED(
      "use SetWeak",
      void SetPhantom(P* parameter,
                      typename WeakCallbackInfo<P>::Callback callback,
                      int internal_field_index1 = -1,
                      int internal_field_index2 = -1));

  template <typename P>
  V8_INLINE void SetWeak(P* parameter,
                         typename WeakCallbackInfo<P>::Callback callback,
                         WeakCallbackType type);

  template<typename P>
  V8_INLINE P* ClearWeak();

  // TODO(dcarney): remove this.
  V8_INLINE void ClearWeak() { ClearWeak<void>(); }

  /**
   * Marks the reference to this object independent. Garbage collector is free
   * to ignore any object groups containing this object. Weak callback for an
   * independent handle should not assume that it will be preceded by a global
   * GC prologue callback or followed by a global GC epilogue callback.
   */
  V8_INLINE void MarkIndependent();

  /**
   * Marks the reference to this object partially dependent. Partially dependent
   * handles only depend on other partially dependent handles and these
   * dependencies are provided through object groups. It provides a way to build
   * smaller object groups for young objects that represent only a subset of all
   * external dependencies. This mark is automatically cleared after each
   * garbage collection.
   */
  V8_INLINE void MarkPartiallyDependent();

  /**
   * Marks the reference to this object as active. The scavenge garbage
   * collection should not reclaim the objects marked as active.
   * This bit is cleared after the each garbage collection pass.
   */
  V8_INLINE void MarkActive();

  V8_INLINE bool IsIndependent() const;

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
  PersistentBase(const PersistentBase& other) = delete;  // NOLINT
  void operator=(const PersistentBase&) = delete;
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
 * Copy, assignment and destructor bevavior is controlled by the traits
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
  V8_INLINE static Persistent<T>& Cast(Persistent<S>& that) { // NOLINT
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (!that.IsEmpty()) T::Cast(*that);
#endif
    return reinterpret_cast<Persistent<T>&>(that);
  }

  // TODO(dcarney): this is pretty useless, fix or remove
  template <class S> V8_INLINE Persistent<S>& As() { // NOLINT
    return Persistent<S>::Cast(*this);
  }

 private:
  friend class Isolate;
  friend class Utils;
  template<class F> friend class Local;
  template<class F1, class F2> friend class Persistent;
  template<class F> friend class ReturnValue;

  template <class S> V8_INLINE Persistent(S* that) : PersistentBase<T>(that) { }
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

 private:
  template <class F>
  friend class ReturnValue;
  Global(const Global&) = delete;
  void operator=(const Global&) = delete;
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
  HandleScope(Isolate* isolate);

  ~HandleScope();

  /**
   * Counts the number of allocated handles.
   */
  static int NumberOfHandles(Isolate* isolate);

  V8_INLINE Isolate* GetIsolate() const {
    return reinterpret_cast<Isolate*>(isolate_);
  }

 protected:
  V8_INLINE HandleScope() {}

  void Initialize(Isolate* isolate);

  static internal::Object** CreateHandle(internal::Isolate* isolate,
                                         internal::Object* value);

 private:
  // Uses heap_object to obtain the current Isolate.
  static internal::Object** CreateHandle(internal::HeapObject* heap_object,
                                         internal::Object* value);

  // Make it hard to create heap-allocated or illegal handle scopes by
  // disallowing certain operations.
  HandleScope(const HandleScope&);
  void operator=(const HandleScope&);
  void* operator new(size_t size);
  void operator delete(void*, size_t);

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
  EscapableHandleScope(Isolate* isolate);
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

 private:
  internal::Object** Escape(internal::Object** escape_value);

  // Make it hard to create heap-allocated or illegal handle scopes by
  // disallowing certain operations.
  EscapableHandleScope(const EscapableHandleScope&);
  void operator=(const EscapableHandleScope&);
  void* operator new(size_t size);
  void operator delete(void*, size_t);

  internal::Object** escape_slot_;
};

class V8_EXPORT SealHandleScope {
 public:
  SealHandleScope(Isolate* isolate);
  ~SealHandleScope();

 private:
  // Make it hard to create heap-allocated or illegal handle scopes by
  // disallowing certain operations.
  SealHandleScope(const SealHandleScope&);
  void operator=(const SealHandleScope&);
  void* operator new(size_t size);
  void operator delete(void*, size_t);

  internal::Isolate* isolate_;
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
 * The optional attributes of ScriptOrigin.
 */
class ScriptOriginOptions {
 public:
  V8_INLINE ScriptOriginOptions(bool is_embedder_debug_script = false,
                                bool is_shared_cross_origin = false,
                                bool is_opaque = false)
      : flags_((is_embedder_debug_script ? kIsEmbedderDebugScript : 0) |
               (is_shared_cross_origin ? kIsSharedCrossOrigin : 0) |
               (is_opaque ? kIsOpaque : 0)) {}
  V8_INLINE ScriptOriginOptions(int flags)
      : flags_(flags &
               (kIsEmbedderDebugScript | kIsSharedCrossOrigin | kIsOpaque)) {}
  bool IsEmbedderDebugScript() const {
    return (flags_ & kIsEmbedderDebugScript) != 0;
  }
  bool IsSharedCrossOrigin() const {
    return (flags_ & kIsSharedCrossOrigin) != 0;
  }
  bool IsOpaque() const { return (flags_ & kIsOpaque) != 0; }
  int Flags() const { return flags_; }

 private:
  enum {
    kIsEmbedderDebugScript = 1,
    kIsSharedCrossOrigin = 1 << 1,
    kIsOpaque = 1 << 2
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
      Local<Boolean> resource_is_embedder_debug_script = Local<Boolean>(),
      Local<Value> source_map_url = Local<Value>(),
      Local<Boolean> resource_is_opaque = Local<Boolean>());
  V8_INLINE Local<Value> ResourceName() const;
  V8_INLINE Local<Integer> ResourceLineOffset() const;
  V8_INLINE Local<Integer> ResourceColumnOffset() const;
  /**
    * Returns true for embedder's debugger scripts
    */
  V8_INLINE Local<Integer> ScriptID() const;
  V8_INLINE Local<Value> SourceMapUrl() const;
  V8_INLINE ScriptOriginOptions Options() const { return options_; }

 private:
  Local<Value> resource_name_;
  Local<Integer> resource_line_offset_;
  Local<Integer> resource_column_offset_;
  ScriptOriginOptions options_;
  Local<Integer> script_id_;
  Local<Value> source_map_url_;
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
 * A compiled JavaScript script, tied to a Context which was active when the
 * script was compiled.
 */
class V8_EXPORT Script {
 public:
  /**
   * A shorthand for ScriptCompiler::Compile().
   */
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<Script> Compile(Local<String> source,
                            ScriptOrigin* origin = nullptr));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Local<String> source,
      ScriptOrigin* origin = nullptr);

  static Local<Script> V8_DEPRECATE_SOON("Use maybe version",
                                         Compile(Local<String> source,
                                                 Local<String> file_name));

  /**
   * Runs the script returning the resulting value. It will be run in the
   * context in which it was created (ScriptCompiler::CompileBound or
   * UnboundScript::BindToCurrentContext()).
   */
  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Run());
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

   private:
    // Prevent copying. Not implemented.
    CachedData(const CachedData&);
    CachedData& operator=(const CachedData&);
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

   private:
    friend class ScriptCompiler;
    // Prevent copying. Not implemented.
    Source(const Source&);
    Source& operator=(const Source&);

    Local<String> source_string;

    // Origin information
    Local<Value> resource_name;
    Local<Integer> resource_line_offset;
    Local<Integer> resource_column_offset;
    ScriptOriginOptions resource_options;
    Local<Value> source_map_url;

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

    internal::StreamedSource* impl() const { return impl_; }

   private:
    // Prevent copying. Not implemented.
    StreamedSource(const StreamedSource&);
    StreamedSource& operator=(const StreamedSource&);

    internal::StreamedSource* impl_;
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
    kConsumeCodeCache
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
  static V8_DEPRECATED("Use maybe version",
                       Local<UnboundScript> CompileUnbound(
                           Isolate* isolate, Source* source,
                           CompileOptions options = kNoCompileOptions));
  static V8_WARN_UNUSED_RESULT MaybeLocal<UnboundScript> CompileUnboundScript(
      Isolate* isolate, Source* source,
      CompileOptions options = kNoCompileOptions);

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
  static V8_DEPRECATED(
      "Use maybe version",
      Local<Script> Compile(Isolate* isolate, Source* source,
                            CompileOptions options = kNoCompileOptions));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Source* source,
      CompileOptions options = kNoCompileOptions);

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
  static V8_DEPRECATED("Use maybe version",
                       Local<Script> Compile(Isolate* isolate,
                                             StreamedSource* source,
                                             Local<String> full_source_string,
                                             const ScriptOrigin& origin));
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
   *   V8 version number or currently compiler flags. This means when
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
   * Compile an ES6 module.
   *
   * This is an unfinished experimental feature, and is only exposed
   * here for internal testing purposes.
   * Only parsing works at the moment. Do not use.
   *
   * TODO(adamk): Script is likely the wrong return value for this;
   * should return some new Module type.
   */
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> CompileModule(
      Local<Context> context, Source* source,
      CompileOptions options = kNoCompileOptions);

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
  static V8_DEPRECATE_SOON("Use maybe version",
                           Local<Function> CompileFunctionInContext(
                               Isolate* isolate, Source* source,
                               Local<Context> context, size_t arguments_count,
                               Local<String> arguments[],
                               size_t context_extension_count,
                               Local<Object> context_extensions[]));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Function> CompileFunctionInContext(
      Local<Context> context, Source* source, size_t arguments_count,
      Local<String> arguments[], size_t context_extension_count,
      Local<Object> context_extensions[]);

 private:
  static V8_WARN_UNUSED_RESULT MaybeLocal<UnboundScript> CompileUnboundInternal(
      Isolate* isolate, Source* source, CompileOptions options, bool is_module);
};


/**
 * An error message.
 */
class V8_EXPORT Message {
 public:
  Local<String> Get() const;

  V8_DEPRECATE_SOON("Use maybe version", Local<String> GetSourceLine() const);
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
  V8_DEPRECATE_SOON("Use maybe version", int GetLineNumber() const);
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
   * Returns the index within the line of the first character where
   * the error occurred.
   */
  V8_DEPRECATE_SOON("Use maybe version", int GetStartColumn() const);
  V8_WARN_UNUSED_RESULT Maybe<int> GetStartColumn(Local<Context> context) const;

  /**
   * Returns the index within the line of the last character where
   * the error occurred.
   */
  V8_DEPRECATED("Use maybe version", int GetEndColumn() const);
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
   * Returns StackTrace as a v8::Array that contains StackFrame objects.
   */
  Local<Array> AsArray();

  /**
   * Grab a snapshot of the current JavaScript execution stack.
   *
   * \param frame_limit The maximum number of stack frames we want to capture.
   * \param options Enumerates the set of things we will capture for each
   *   StackFrame.
   */
  static Local<StackTrace> CurrentStackTrace(
      Isolate* isolate,
      int frame_limit,
      StackTraceOptions options = kOverview);
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
};


// A StateTag represents a possible state of the VM.
enum StateTag { JS, GC, COMPILER, OTHER, EXTERNAL, IDLE };


// A RegisterState represents the current state of registers used
// by the sampling profiler API.
struct RegisterState {
  RegisterState() : pc(NULL), sp(NULL), fp(NULL) {}
  void* pc;  // Instruction pointer.
  void* sp;  // Stack pointer.
  void* fp;  // Frame pointer.
};


// The output structure filled up by GetStackSample API function.
struct SampleInfo {
  size_t frames_count;
  StateTag vm_state;
};


/**
 * A JSON Parser.
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
  static V8_DEPRECATED("Use maybe version",
                       Local<Value> Parse(Local<String> json_string));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Value> Parse(
      Isolate* isolate, Local<String> json_string);
};


/**
 * A map whose keys are referenced weakly. It is similar to JavaScript WeakMap
 * but can be created without entering a v8::Context and hence shouldn't
 * escape to JavaScript.
 */
class V8_EXPORT NativeWeakMap : public Data {
 public:
  static Local<NativeWeakMap> New(Isolate* isolate);
  void Set(Local<Value> key, Local<Value> value);
  Local<Value> Get(Local<Value> key);
  bool Has(Local<Value> key);
  bool Delete(Local<Value> key);
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
   * Returns true if this value is true.
   */
  bool IsTrue() const;

  /**
   * Returns true if this value is false.
   */
  bool IsFalse() const;

  /**
   * Returns true if this value is a symbol or a string.
   * This is an experimental feature.
   */
  bool IsName() const;

  /**
   * Returns true if this value is an instance of the String type.
   * See ECMA-262 8.4.
   */
  V8_INLINE bool IsString() const;

  /**
   * Returns true if this value is a symbol.
   * This is an experimental feature.
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
   * This is an experimental feature.
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
   * Returns true if this value is a Generator function.
   * This is an experimental feature.
   */
  bool IsGeneratorFunction() const;

  /**
   * Returns true if this value is a Generator object (iterator).
   * This is an experimental feature.
   */
  bool IsGeneratorObject() const;

  /**
   * Returns true if this value is a Promise.
   * This is an experimental feature.
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
   * This is an experimental feature.
   */
  bool IsArrayBuffer() const;

  /**
   * Returns true if this value is an ArrayBufferView.
   * This is an experimental feature.
   */
  bool IsArrayBufferView() const;

  /**
   * Returns true if this value is one of TypedArrays.
   * This is an experimental feature.
   */
  bool IsTypedArray() const;

  /**
   * Returns true if this value is an Uint8Array.
   * This is an experimental feature.
   */
  bool IsUint8Array() const;

  /**
   * Returns true if this value is an Uint8ClampedArray.
   * This is an experimental feature.
   */
  bool IsUint8ClampedArray() const;

  /**
   * Returns true if this value is an Int8Array.
   * This is an experimental feature.
   */
  bool IsInt8Array() const;

  /**
   * Returns true if this value is an Uint16Array.
   * This is an experimental feature.
   */
  bool IsUint16Array() const;

  /**
   * Returns true if this value is an Int16Array.
   * This is an experimental feature.
   */
  bool IsInt16Array() const;

  /**
   * Returns true if this value is an Uint32Array.
   * This is an experimental feature.
   */
  bool IsUint32Array() const;

  /**
   * Returns true if this value is an Int32Array.
   * This is an experimental feature.
   */
  bool IsInt32Array() const;

  /**
   * Returns true if this value is a Float32Array.
   * This is an experimental feature.
   */
  bool IsFloat32Array() const;

  /**
   * Returns true if this value is a Float64Array.
   * This is an experimental feature.
   */
  bool IsFloat64Array() const;

  /**
   * Returns true if this value is a SIMD Float32x4.
   * This is an experimental feature.
   */
  bool IsFloat32x4() const;

  /**
   * Returns true if this value is a DataView.
   * This is an experimental feature.
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
  V8_DEPRECATED("Use maybe version",
                Local<String> ToDetailString(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Object> ToObject(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Integer> ToInteger(Isolate* isolate) const);
  V8_DEPRECATED("Use maybe version",
                Local<Uint32> ToUint32(Isolate* isolate) const);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Int32> ToInt32(Isolate* isolate) const);

  inline V8_DEPRECATE_SOON("Use maybe version",
                           Local<Boolean> ToBoolean() const);
  inline V8_DEPRECATED("Use maybe version", Local<Number> ToNumber() const);
  inline V8_DEPRECATE_SOON("Use maybe version", Local<String> ToString() const);
  inline V8_DEPRECATED("Use maybe version",
                       Local<String> ToDetailString() const);
  inline V8_DEPRECATE_SOON("Use maybe version", Local<Object> ToObject() const);
  inline V8_DEPRECATE_SOON("Use maybe version",
                           Local<Integer> ToInteger() const);
  inline V8_DEPRECATED("Use maybe version", Local<Uint32> ToUint32() const);
  inline V8_DEPRECATED("Use maybe version", Local<Int32> ToInt32() const);

  /**
   * Attempts to convert a string to an array index.
   * Returns an empty handle if the conversion fails.
   */
  V8_DEPRECATED("Use maybe version", Local<Uint32> ToArrayIndex() const);
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

 private:
  V8_INLINE bool QuickIsUndefined() const;
  V8_INLINE bool QuickIsNull() const;
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

  V8_INLINE static Name* Cast(v8::Value* obj);
 private:
  static void CheckCast(v8::Value* obj);
};


enum class NewStringType { kNormal, kInternalized };


/**
 * A JavaScript string value (ECMA-262, 4.3.17).
 */
class V8_EXPORT String : public Name {
 public:
  static const int kMaxLength = (1 << 28) - 16;

  enum Encoding {
    UNKNOWN_ENCODING = 0x1,
    TWO_BYTE_ENCODING = 0x0,
    ONE_BYTE_ENCODING = 0x4
  };
  /**
   * Returns the number of characters in this string.
   */
  int Length() const;

  /**
   * Returns the number of bytes in the UTF-8 encoded
   * representation of this string.
   */
  int Utf8Length() const;

  /**
   * Returns whether this string is known to contain only one byte data.
   * Does not read the string.
   * False negatives are possible.
   */
  bool IsOneByte() const;

  /**
   * Returns whether this string contain only one byte data.
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
  V8_INLINE static v8::Local<v8::String> Empty(Isolate* isolate);

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

   private:
    // Disallow copying and assigning.
    ExternalStringResourceBase(const ExternalStringResourceBase&);
    void operator=(const ExternalStringResourceBase&);

    friend class v8::internal::Heap;
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

  /** Allocates a new string from Latin-1 data.*/
  static V8_DEPRECATED(
      "Use maybe version",
      Local<String> NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                   NewStringType type = kNormalString,
                                   int length = -1));

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
  static V8_DEPRECATED("Use maybe version",
                       Local<String> NewExternal(
                           Isolate* isolate, ExternalStringResource* resource));
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
    explicit Utf8Value(Local<v8::Value> obj);
    ~Utf8Value();
    char* operator*() { return str_; }
    const char* operator*() const { return str_; }
    int length() const { return length_; }
   private:
    char* str_;
    int length_;

    // Disallow copying and assigning.
    Utf8Value(const Utf8Value&);
    void operator=(const Utf8Value&);
  };

  /**
   * Converts an object to a two-byte string.
   * If conversion to a string fails (eg. due to an exception in the toString()
   * method of the object) then the length() method returns 0 and the * operator
   * returns NULL.
   */
  class V8_EXPORT Value {
   public:
    explicit Value(Local<v8::Value> obj);
    ~Value();
    uint16_t* operator*() { return str_; }
    const uint16_t* operator*() const { return str_; }
    int length() const { return length_; }
   private:
    uint16_t* str_;
    int length_;

    // Disallow copying and assigning.
    Value(const Value&);
    void operator=(const Value&);
  };

 private:
  void VerifyExternalStringResourceBase(ExternalStringResourceBase* v,
                                        Encoding encoding) const;
  void VerifyExternalStringResource(ExternalStringResource* val) const;
  static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript symbol (ECMA-262 edition 6)
 *
 * This is an experimental feature. Use at your own risk.
 */
class V8_EXPORT Symbol : public Name {
 public:
  // Returns the print name string of the symbol, or undefined if none.
  Local<Value> Name() const;

  // Create a symbol. If name is not empty, it will be used as the description.
  static Local<Symbol> New(Isolate* isolate,
                           Local<String> name = Local<String>());

  // Access global symbol registry.
  // Note that symbols created this way are never collected, so
  // they should only be used for statically fixed properties.
  // Also, there is only one global name space for the names used as keys.
  // To minimize the potential for clashes, use qualified names as keys.
  static Local<Symbol> For(Isolate *isolate, Local<String> name);

  // Retrieve a global symbol. Similar to |For|, but using a separate
  // registry that is not accessible by (and cannot clash with) JavaScript code.
  static Local<Symbol> ForApi(Isolate *isolate, Local<String> name);

  // Well-known symbols
  static Local<Symbol> GetIterator(Isolate* isolate);
  static Local<Symbol> GetUnscopables(Isolate* isolate);
  static Local<Symbol> GetToStringTag(Isolate* isolate);
  static Local<Symbol> GetIsConcatSpreadable(Isolate* isolate);

  V8_INLINE static Symbol* Cast(v8::Value* obj);

 private:
  Symbol();
  static void CheckCast(v8::Value* obj);
};


/**
 * A private symbol
 *
 * This is an experimental feature. Use at your own risk.
 */
class V8_EXPORT Private : public Data {
 public:
  // Returns the print name string of the private symbol, or undefined if none.
  Local<Value> Name() const;

  // Create a private symbol. If name is not empty, it will be the description.
  static Local<Private> New(Isolate* isolate,
                            Local<String> name = Local<String>());

  // Retrieve a global private symbol. If a symbol with this name has not
  // been retrieved in the same isolate before, it is created.
  // Note that private symbols created this way are never collected, so
  // they should only be used for statically fixed properties.
  // Also, there is only one global name space for the names used as keys.
  // To minimize the potential for clashes, use qualified names as keys,
  // e.g., "Class#property".
  static Local<Private> ForApi(Isolate* isolate, Local<String> name);

 private:
  Private();
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


enum PropertyAttribute {
  None       = 0,
  ReadOnly   = 1 << 0,
  DontEnum   = 1 << 1,
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

  // Sets an own property on this object bypassing interceptors and
  // overriding accessors or read-only properties.
  //
  // Note that if the object has an interceptor the property will be set
  // locally, but since the interceptor takes precedence the local property
  // will only be returned if the interceptor doesn't return a value.
  //
  // Note also that this only works for named properties.
  V8_DEPRECATED("Use CreateDataProperty / DefineOwnProperty",
                bool ForceSet(Local<Value> key, Local<Value> value,
                              PropertyAttribute attribs = None));
  V8_DEPRECATE_SOON("Use CreateDataProperty / DefineOwnProperty",
                    Maybe<bool> ForceSet(Local<Context> context,
                                         Local<Value> key, Local<Value> value,
                                         PropertyAttribute attribs = None));

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
  V8_DEPRECATED("Use maybe version",
                PropertyAttribute GetPropertyAttributes(Local<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetPropertyAttributes(
      Local<Context> context, Local<Value> key);

  /**
   * Returns Object.getOwnPropertyDescriptor as per ES5 section 15.2.3.3.
   */
  V8_DEPRECATED("Use maybe version",
                Local<Value> GetOwnPropertyDescriptor(Local<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetOwnPropertyDescriptor(
      Local<Context> context, Local<String> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Has(Local<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context,
                                        Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Delete(Local<Value> key));
  // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
  Maybe<bool> Delete(Local<Context> context, Local<Value> key);

  V8_DEPRECATED("Use maybe version", bool Has(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context, uint32_t index);

  V8_DEPRECATED("Use maybe version", bool Delete(uint32_t index));
  // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
  Maybe<bool> Delete(Local<Context> context, uint32_t index);

  V8_DEPRECATED("Use maybe version",
                bool SetAccessor(Local<String> name,
                                 AccessorGetterCallback getter,
                                 AccessorSetterCallback setter = 0,
                                 Local<Value> data = Local<Value>(),
                                 AccessControl settings = DEFAULT,
                                 PropertyAttribute attribute = None));
  V8_DEPRECATED("Use maybe version",
                bool SetAccessor(Local<Name> name,
                                 AccessorNameGetterCallback getter,
                                 AccessorNameSetterCallback setter = 0,
                                 Local<Value> data = Local<Value>(),
                                 AccessControl settings = DEFAULT,
                                 PropertyAttribute attribute = None));
  // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
  Maybe<bool> SetAccessor(Local<Context> context, Local<Name> name,
                          AccessorNameGetterCallback getter,
                          AccessorNameSetterCallback setter = 0,
                          MaybeLocal<Value> data = MaybeLocal<Value>(),
                          AccessControl settings = DEFAULT,
                          PropertyAttribute attribute = None);

  void SetAccessorProperty(Local<Name> name, Local<Function> getter,
                           Local<Function> setter = Local<Function>(),
                           PropertyAttribute attribute = None,
                           AccessControl settings = DEFAULT);

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

  /**
   * This function has the same functionality as GetPropertyNames but
   * the returned array doesn't contain the names of properties from
   * prototype objects.
   */
  V8_DEPRECATE_SOON("Use maybe version", Local<Array> GetOwnPropertyNames());
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
      Local<Context> context);

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
  V8_DEPRECATED("Use maybe version", bool SetPrototype(Local<Value> prototype));
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
  V8_DEPRECATED("Use maybe version", Local<String> ObjectProtoToString());
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ObjectProtoToString(
      Local<Context> context);

  /**
   * Returns the name of the function invoked as a constructor for this object.
   */
  Local<String> GetConstructorName();

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

  // Testers for local properties.
  V8_DEPRECATED("Use maybe version", bool HasOwnProperty(Local<String> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   Local<Name> key);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealNamedProperty(Local<String> key));
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
  V8_DEPRECATED(
      "Use maybe version",
      Local<Value> GetRealNamedPropertyInPrototypeChain(Local<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedPropertyInPrototypeChain(
      Local<Context> context, Local<Name> key);

  /**
   * Gets the property attributes of a real property in the prototype chain,
   * which can be None or any combination of ReadOnly, DontEnum and DontDelete.
   * Interceptors in the prototype chain are not called.
   */
  V8_DEPRECATED(
      "Use maybe version",
      Maybe<PropertyAttribute> GetRealNamedPropertyAttributesInPrototypeChain(
          Local<String> key));
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute>
  GetRealNamedPropertyAttributesInPrototypeChain(Local<Context> context,
                                                 Local<Name> key);

  /**
   * If result.IsEmpty() no real property was located on the object or
   * in the prototype chain.
   * This means interceptors in the prototype chain are not called.
   */
  V8_DEPRECATED("Use maybe version",
                Local<Value> GetRealNamedProperty(Local<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedProperty(
      Local<Context> context, Local<Name> key);

  /**
   * Gets the property attributes of a real property which can be
   * None or any combination of ReadOnly, DontEnum and DontDelete.
   * Interceptors in the prototype chain are not called.
   */
  V8_DEPRECATED("Use maybe version",
                Maybe<PropertyAttribute> GetRealNamedPropertyAttributes(
                    Local<String> key));
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

  V8_DEPRECATED("Use v8::Object::SetPrivate instead.",
                bool SetHiddenValue(Local<String> key, Local<Value> value));
  V8_DEPRECATED("Use v8::Object::GetPrivate instead.",
                Local<Value> GetHiddenValue(Local<String> key));
  V8_DEPRECATED("Use v8::Object::DeletePrivate instead.",
                bool DeleteHiddenValue(Local<String> key));

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

  /**
   * Checks whether a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   * When an Object is callable this method returns true.
   */
  bool IsCallable();

  /**
   * Call an Object as a function if a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   */
  V8_DEPRECATED("Use maybe version",
                Local<Value> CallAsFunction(Local<Value> recv, int argc,
                                            Local<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsFunction(Local<Context> context,
                                                         Local<Value> recv,
                                                         int argc,
                                                         Local<Value> argv[]);

  /**
   * Call an Object as a constructor if a callback is set by the
   * ObjectTemplate::SetCallAsFunctionHandler method.
   * Note: This method behaves like the Function::NewInstance method.
   */
  V8_DEPRECATED("Use maybe version",
                Local<Value> CallAsConstructor(int argc, Local<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsConstructor(
      Local<Context> context, int argc, Local<Value> argv[]);

  /**
   * Return the isolate to which the Object belongs to.
   */
  V8_DEPRECATE_SOON("Keep track of isolate correctly", Isolate* GetIsolate());

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
   * Clones an element at index |index|.  Returns an empty
   * handle if cloning fails (for any reason).
   */
  V8_DEPRECATED("Cloning is not supported.",
                Local<Object> CloneElementAt(uint32_t index));
  V8_DEPRECATED("Cloning is not supported.",
                MaybeLocal<Object> CloneElementAt(Local<Context> context,
                                                  uint32_t index));

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
  V8_INLINE Isolate* GetIsolate();

  // Pointer setter: Uncompilable to prevent inadvertent misuse.
  template <typename S>
  V8_INLINE void Set(S* whatever);

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
  V8_INLINE int Length() const;
  V8_INLINE Local<Value> operator[](int i) const;
  V8_INLINE Local<Function> Callee() const;
  V8_INLINE Local<Object> This() const;
  V8_INLINE Local<Object> Holder() const;
  V8_INLINE bool IsConstructCall() const;
  V8_INLINE Local<Value> Data() const;
  V8_INLINE Isolate* GetIsolate() const;
  V8_INLINE ReturnValue<T> GetReturnValue() const;
  // This shouldn't be public, but the arm compiler needs it.
  static const int kArgsLength = 7;

 protected:
  friend class internal::FunctionCallbackArguments;
  friend class internal::CustomArguments<FunctionCallbackInfo>;
  static const int kHolderIndex = 0;
  static const int kIsolateIndex = 1;
  static const int kReturnValueDefaultValueIndex = 2;
  static const int kReturnValueIndex = 3;
  static const int kDataIndex = 4;
  static const int kCalleeIndex = 5;
  static const int kContextSaveIndex = 6;

  V8_INLINE FunctionCallbackInfo(internal::Object** implicit_args,
                   internal::Object** values,
                   int length,
                   bool is_construct_call);
  internal::Object** implicit_args_;
  internal::Object** values_;
  int length_;
  int is_construct_call_;
};


/**
 * The information passed to a property callback about the context
 * of the property access.
 */
template<typename T>
class PropertyCallbackInfo {
 public:
  V8_INLINE Isolate* GetIsolate() const;
  V8_INLINE Local<Value> Data() const;
  V8_INLINE Local<Object> This() const;
  V8_INLINE Local<Object> Holder() const;
  V8_INLINE ReturnValue<T> GetReturnValue() const;
  // This shouldn't be public, but the arm compiler needs it.
  static const int kArgsLength = 6;

 protected:
  friend class MacroAssembler;
  friend class internal::PropertyCallbackArguments;
  friend class internal::CustomArguments<PropertyCallbackInfo>;
  static const int kHolderIndex = 0;
  static const int kIsolateIndex = 1;
  static const int kReturnValueDefaultValueIndex = 2;
  static const int kReturnValueIndex = 3;
  static const int kDataIndex = 4;
  static const int kThisIndex = 5;

  V8_INLINE PropertyCallbackInfo(internal::Object** args) : args_(args) {}
  internal::Object** args_;
};


typedef void (*FunctionCallback)(const FunctionCallbackInfo<Value>& info);


/**
 * A JavaScript function object (ECMA-262, 15.3).
 */
class V8_EXPORT Function : public Object {
 public:
  /**
   * Create a function in the current execution context
   * for a given FunctionCallback.
   */
  static MaybeLocal<Function> New(Local<Context> context,
                                  FunctionCallback callback,
                                  Local<Value> data = Local<Value>(),
                                  int length = 0);
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<Function> New(Isolate* isolate, FunctionCallback callback,
                          Local<Value> data = Local<Value>(), int length = 0));

  V8_DEPRECATED("Use maybe version",
                Local<Object> NewInstance(int argc, Local<Value> argv[]) const);
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
      Local<Context> context, int argc, Local<Value> argv[]) const;

  V8_DEPRECATED("Use maybe version", Local<Object> NewInstance() const);
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
   * Tells whether this function is builtin.
   */
  bool IsBuiltin() const;

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


/**
 * An instance of the built-in Promise constructor (ES6 draft).
 * This API is experimental. Only works with --harmony flag.
 */
class V8_EXPORT Promise : public Object {
 public:
  class V8_EXPORT Resolver : public Object {
   public:
    /**
     * Create a new resolver, along with an associated promise in pending state.
     */
    static V8_DEPRECATE_SOON("Use maybe version",
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
    V8_DEPRECATE_SOON("Use maybe version", void Resolve(Local<Value> value));
    // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
    Maybe<bool> Resolve(Local<Context> context, Local<Value> value);

    V8_DEPRECATE_SOON("Use maybe version", void Reject(Local<Value> value));
    // TODO(dcarney): mark V8_WARN_UNUSED_RESULT
    Maybe<bool> Reject(Local<Context> context, Local<Value> value);

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
  V8_DEPRECATED("Use maybe version of Then",
                Local<Promise> Chain(Local<Function> handler));
  V8_DEPRECATED("Use Then",
                V8_WARN_UNUSED_RESULT MaybeLocal<Promise> Chain(
                    Local<Context> context, Local<Function> handler));

  V8_DEPRECATED("Use maybe version",
                Local<Promise> Catch(Local<Function> handler));
  V8_WARN_UNUSED_RESULT MaybeLocal<Promise> Catch(Local<Context> context,
                                                  Local<Function> handler);

  V8_DEPRECATED("Use maybe version",
                Local<Promise> Then(Local<Function> handler));
  V8_WARN_UNUSED_RESULT MaybeLocal<Promise> Then(Local<Context> context,
                                                 Local<Function> handler);

  /**
   * Returns true if the promise has at least one derived promise, and
   * therefore resolve/reject handlers (including default handler).
   */
  bool HasHandler();

  V8_INLINE static Promise* Cast(Value* obj);

 private:
  Promise();
  static void CheckCast(Value* obj);
};


/**
 * An instance of the built-in Proxy constructor (ECMA-262, 6th Edition,
 * 26.2.1).
 */
class V8_EXPORT Proxy : public Object {
 public:
  Local<Object> GetTarget();
  Local<Value> GetHandler();
  bool IsRevoked();
  void Revoke();

  /**
   * Creates a new empty Map.
   */
  static MaybeLocal<Proxy> New(Local<Context> context,
                               Local<Object> local_target,
                               Local<Object> local_handler);

  V8_INLINE static Proxy* Cast(Value* obj);

 private:
  Proxy();
  static void CheckCast(Value* obj);
};


#ifndef V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT
// The number of required internal fields can be defined by embedder.
#define V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT 2
#endif


enum class ArrayBufferCreationMode { kInternalized, kExternalized };


/**
 * An instance of the built-in ArrayBuffer constructor (ES6 draft 15.13.5).
 * This API is experimental and may change significantly.
 */
class V8_EXPORT ArrayBuffer : public Object {
 public:
  /**
   * Allocator that V8 uses to allocate |ArrayBuffer|'s memory.
   * The allocator is a global V8 setting. It has to be set via
   * Isolate::CreateParams.
   *
   * This API is experimental and may change significantly.
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
  };

  /**
   * The contents of an |ArrayBuffer|. Externalization of |ArrayBuffer|
   * returns an instance of this class, populated, with a pointer to data
   * and byte length.
   *
   * The Data pointer of ArrayBuffer::Contents is always allocated with
   * Allocator::Allocate that is set via Isolate::CreateParams.
   *
   * This API is experimental and may change significantly.
   */
  class V8_EXPORT Contents { // NOLINT
   public:
    Contents() : data_(NULL), byte_length_(0) {}

    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }

   private:
    void* data_;
    size_t byte_length_;

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
   * The memory block will not be reclaimed when a created ArrayBuffer
   * is garbage-collected.
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
   * had been etxrenalized, it does no longer owns the memory block. The caller
   * should take steps to free memory when it is no longer needed.
   *
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|
   * that has been set via Isolate::CreateParams.
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
   * The memory block is guaranteed to be allocated with |Allocator::Allocate|.
   */
  Contents GetContents();

  V8_INLINE static ArrayBuffer* Cast(Value* obj);

  static const int kInternalFieldCount = V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT;

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
 *
 * This API is experimental and may change significantly.
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
   * ByteOffset of the underling buffer to the memory starting at |dest|.
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

 private:
  ArrayBufferView();
  static void CheckCast(Value* obj);
};


/**
 * A base class for an instance of TypedArray series of constructors
 * (ES6 draft 15.13.6).
 * This API is experimental and may change significantly.
 */
class V8_EXPORT TypedArray : public ArrayBufferView {
 public:
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * This API is experimental and may change significantly.
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
 * An instance of DataView constructor (ES6 draft 15.13.7).
 * This API is experimental and may change significantly.
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
    Contents() : data_(NULL), byte_length_(0) {}

    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }

   private:
    void* data_;
    size_t byte_length_;

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
   * SharedArrayBuffer had been etxrenalized, it does no longer owns the memory
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

  V8_INLINE static Date* Cast(v8::Value* obj);

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
  static void CheckCast(v8::Value* obj);
};


/**
 * A Number object (ECMA-262, 4.3.21).
 */
class V8_EXPORT NumberObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, double value);

  double ValueOf() const;

  V8_INLINE static NumberObject* Cast(v8::Value* obj);

 private:
  static void CheckCast(v8::Value* obj);
};


/**
 * A Boolean object (ECMA-262, 4.3.15).
 */
class V8_EXPORT BooleanObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, bool value);
  V8_DEPRECATED("Pass an isolate", static Local<Value> New(bool value));

  bool ValueOf() const;

  V8_INLINE static BooleanObject* Cast(v8::Value* obj);

 private:
  static void CheckCast(v8::Value* obj);
};


/**
 * A String object (ECMA-262, 4.3.18).
 */
class V8_EXPORT StringObject : public Object {
 public:
  static Local<Value> New(Local<String> value);

  Local<String> ValueOf() const;

  V8_INLINE static StringObject* Cast(v8::Value* obj);

 private:
  static void CheckCast(v8::Value* obj);
};


/**
 * A Symbol object (ECMA-262 edition 6).
 *
 * This is an experimental feature. Use at your own risk.
 */
class V8_EXPORT SymbolObject : public Object {
 public:
  static Local<Value> New(Isolate* isolate, Local<Symbol> value);

  Local<Symbol> ValueOf() const;

  V8_INLINE static SymbolObject* Cast(v8::Value* obj);

 private:
  static void CheckCast(v8::Value* obj);
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
    kGlobal = 1,
    kIgnoreCase = 2,
    kMultiline = 4,
    kSticky = 8,
    kUnicode = 16
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
  static V8_DEPRECATE_SOON("Use maybe version",
                           Local<RegExp> New(Local<String> pattern,
                                             Flags flags));
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

  V8_INLINE static RegExp* Cast(v8::Value* obj);

 private:
  static void CheckCast(v8::Value* obj);
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


#define V8_INTRINSICS_LIST(F) F(ArrayProto_values, array_values_iterator)

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
  /** Adds a property to each instance created by this template.*/
  void Set(Local<Name> name, Local<Data> value,
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
 */
typedef void (*NamedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);


// TODO(dcarney): Deprecate and remove previous typedefs, and replace
// GenericNamedPropertyFooCallback with just NamedPropertyFooCallback.
/**
 * GenericNamedProperty[Getter|Setter] are used as interceptors on object.
 * See ObjectTemplate::SetNamedPropertyHandler.
 */
typedef void (*GenericNamedPropertyGetterCallback)(
    Local<Name> property, const PropertyCallbackInfo<Value>& info);


/**
 * Returns the value if the setter intercepts the request.
 * Otherwise, returns an empty handle.
 */
typedef void (*GenericNamedPropertySetterCallback)(
    Local<Name> property, Local<Value> value,
    const PropertyCallbackInfo<Value>& info);


/**
 * Returns a non-empty handle if the interceptor intercepts the request.
 * The result is an integer encoding property attributes (like v8::None,
 * v8::DontEnum, etc.)
 */
typedef void (*GenericNamedPropertyQueryCallback)(
    Local<Name> property, const PropertyCallbackInfo<Integer>& info);


/**
 * Returns a non-empty handle if the deleter intercepts the request.
 * The return value is true if the property could be deleted and false
 * otherwise.
 */
typedef void (*GenericNamedPropertyDeleterCallback)(
    Local<Name> property, const PropertyCallbackInfo<Boolean>& info);


/**
 * Returns an array containing the names of the properties the named
 * property getter intercepts.
 */
typedef void (*GenericNamedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);


/**
 * Returns the value of the property if the getter intercepts the
 * request.  Otherwise, returns an empty handle.
 */
typedef void (*IndexedPropertyGetterCallback)(
    uint32_t index,
    const PropertyCallbackInfo<Value>& info);


/**
 * Returns the value if the setter intercepts the request.
 * Otherwise, returns an empty handle.
 */
typedef void (*IndexedPropertySetterCallback)(
    uint32_t index,
    Local<Value> value,
    const PropertyCallbackInfo<Value>& info);


/**
 * Returns a non-empty handle if the interceptor intercepts the request.
 * The result is an integer encoding property attributes.
 */
typedef void (*IndexedPropertyQueryCallback)(
    uint32_t index,
    const PropertyCallbackInfo<Integer>& info);


/**
 * Returns a non-empty handle if the deleter intercepts the request.
 * The return value is true if the property could be deleted and false
 * otherwise.
 */
typedef void (*IndexedPropertyDeleterCallback)(
    uint32_t index,
    const PropertyCallbackInfo<Boolean>& info);


/**
 * Returns an array containing the indices of the properties the
 * indexed property getter intercepts.
 */
typedef void (*IndexedPropertyEnumeratorCallback)(
    const PropertyCallbackInfo<Array>& info);


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
                                    Local<Object> accessed_object);


/**
 * Returns true if cross-context access should be allowed to the named
 * property with the given key on the host object.
 */
typedef bool (*NamedSecurityCallback)(Local<Object> host,
                                      Local<Value> key,
                                      AccessType type,
                                      Local<Value> data);


/**
 * Returns true if cross-context access should be allowed to the indexed
 * property with the given index on the host object.
 */
typedef bool (*IndexedSecurityCallback)(Local<Object> host,
                                        uint32_t index,
                                        AccessType type,
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
 *a crash.
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
 *    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New();
 *    t->Set("func_property", v8::Number::New(1));
 *
 *    v8::Local<v8::Template> proto_t = t->PrototypeTemplate();
 *    proto_t->Set("proto_method", v8::FunctionTemplate::New(InvokeCallback));
 *    proto_t->Set("proto_const", v8::Number::New(2));
 *
 *    v8::Local<v8::ObjectTemplate> instance_t = t->InstanceTemplate();
 *    instance_t->SetAccessor("instance_accessor", InstanceAccessorCallback);
 *    instance_t->SetNamedPropertyHandler(PropertyHandlerCallback, ...);
 *    instance_t->Set("instance_property", Number::New(3));
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
      Local<Signature> signature = Local<Signature>(), int length = 0);

  /**
   * Creates a function template with a fast handler. If a fast handler is set,
   * the callback cannot be null.
   */
  static Local<FunctionTemplate> NewWithFastHandler(
      Isolate* isolate, FunctionCallback callback,
      experimental::FastAccessorBuilder* fast_handler = nullptr,
      Local<Value> data = Local<Value>(),
      Local<Signature> signature = Local<Signature>(), int length = 0);

  /** Returns the unique function instance in the current execution context.*/
  V8_DEPRECATE_SOON("Use maybe version", Local<Function> GetFunction());
  V8_WARN_UNUSED_RESULT MaybeLocal<Function> GetFunction(
      Local<Context> context);

  /**
   * Set the call-handler callback for a FunctionTemplate.  This
   * callback is called whenever the function created from this
   * FunctionTemplate is called.
   */
  void SetCallHandler(
      FunctionCallback callback, Local<Value> data = Local<Value>(),
      experimental::FastAccessorBuilder* fast_handler = nullptr);

  /** Set the predefined length property for the FunctionTemplate. */
  void SetLength(int length);

  /** Get the InstanceTemplate. */
  Local<ObjectTemplate> InstanceTemplate();

  /** Causes the function template to inherit from a parent function template.*/
  void Inherit(Local<FunctionTemplate> parent);

  /**
   * A PrototypeTemplate is the template used to create the prototype object
   * of the function created by this template.
   */
  Local<ObjectTemplate> PrototypeTemplate();

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

 private:
  FunctionTemplate();
  friend class Context;
  friend class ObjectTemplate;
};


enum class PropertyHandlerFlags {
  kNone = 0,
  // See ALL_CAN_READ above.
  kAllCanRead = 1,
  // Will not call into interceptor for properties on the receiver or prototype
  // chain.  Currently only valid for named interceptors.
  kNonMasking = 1 << 1,
  // Will not call into interceptor for symbol lookup.  Only meaningful for
  // named interceptors.
  kOnlyInterceptStrings = 1 << 2,
};


struct NamedPropertyHandlerConfiguration {
  NamedPropertyHandlerConfiguration(
      /** Note: getter is required **/
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
        data(data),
        flags(flags) {}

  GenericNamedPropertyGetterCallback getter;
  GenericNamedPropertySetterCallback setter;
  GenericNamedPropertyQueryCallback query;
  GenericNamedPropertyDeleterCallback deleter;
  GenericNamedPropertyEnumeratorCallback enumerator;
  Local<Value> data;
  PropertyHandlerFlags flags;
};


struct IndexedPropertyHandlerConfiguration {
  IndexedPropertyHandlerConfiguration(
      /** Note: getter is required **/
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
        data(data),
        flags(flags) {}

  IndexedPropertyGetterCallback getter;
  IndexedPropertySetterCallback setter;
  IndexedPropertyQueryCallback query;
  IndexedPropertyDeleterCallback deleter;
  IndexedPropertyEnumeratorCallback enumerator;
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
  static V8_DEPRECATED("Use isolate version", Local<ObjectTemplate> New());

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
   * Note that new code should use the second version that can intercept
   * symbol-named properties as well as string-named properties.
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
  // TODO(dcarney): deprecate
  void SetNamedPropertyHandler(NamedPropertyGetterCallback getter,
                               NamedPropertySetterCallback setter = 0,
                               NamedPropertyQueryCallback query = 0,
                               NamedPropertyDeleterCallback deleter = 0,
                               NamedPropertyEnumeratorCallback enumerator = 0,
                               Local<Value> data = Local<Value>());
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
  void SetHandler(const IndexedPropertyHandlerConfiguration& configuration);
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

  V8_DEPRECATED(
      "Use SetAccessCheckCallback instead",
      void SetAccessCheckCallbacks(NamedSecurityCallback named_handler,
                                   IndexedSecurityCallback indexed_handler,
                                   Local<Value> data = Local<Value>()));

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

 private:
  ObjectTemplate();
  static Local<ObjectTemplate> New(internal::Isolate* isolate,
                                   Local<FunctionTemplate> constructor);
  friend class FunctionTemplate;
};


/**
 * A Signature specifies which receiver is valid for a function.
 */
class V8_EXPORT Signature : public Data {
 public:
  static Local<Signature> New(
      Isolate* isolate,
      Local<FunctionTemplate> receiver = Local<FunctionTemplate>());

 private:
  Signature();
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

 private:
  AccessorSignature();
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
  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name) {
    return v8::Local<v8::FunctionTemplate>();
  }

  const char* name() const { return name_; }
  size_t source_length() const { return source_length_; }
  const String::ExternalOneByteStringResource* source() const {
    return &source_; }
  int dependency_count() { return dep_count_; }
  const char** dependencies() { return deps_; }
  void set_auto_enable(bool value) { auto_enable_ = value; }
  bool auto_enable() { return auto_enable_; }

 private:
  const char* name_;
  size_t source_length_;  // expected to initialize before source_
  ExternalOneByteStringResourceImpl source_;
  int dep_count_;
  const char** deps_;
  bool auto_enable_;

  // Disallow copying and assigning.
  Extension(const Extension&);
  void operator=(const Extension&);
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

  int max_semi_space_size() const { return max_semi_space_size_; }
  void set_max_semi_space_size(int value) { max_semi_space_size_ = value; }
  int max_old_space_size() const { return max_old_space_size_; }
  void set_max_old_space_size(int value) { max_old_space_size_ = value; }
  int max_executable_size() const { return max_executable_size_; }
  void set_max_executable_size(int value) { max_executable_size_ = value; }
  uint32_t* stack_limit() const { return stack_limit_; }
  // Sets an address beyond which the VM's stack may not grow.
  void set_stack_limit(uint32_t* value) { stack_limit_ = value; }
  size_t code_range_size() const { return code_range_size_; }
  void set_code_range_size(size_t value) {
    code_range_size_ = value;
  }

 private:
  int max_semi_space_size_;
  int max_old_space_size_;
  int max_executable_size_;
  uint32_t* stack_limit_;
  size_t code_range_size_;
};


// --- Exceptions ---


typedef void (*FatalErrorCallback)(const char* location, const char* message);


typedef void (*MessageCallback)(Local<Message> message, Local<Value> error);

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
  V8_DEPRECATED("Use version with an Isolate*",
                static Local<Message> CreateMessage(Local<Value> exception));

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

// --- Memory Allocation Callback ---
enum ObjectSpace {
  kObjectSpaceNewSpace = 1 << 0,
  kObjectSpaceOldSpace = 1 << 1,
  kObjectSpaceCodeSpace = 1 << 2,
  kObjectSpaceMapSpace = 1 << 3,
  kObjectSpaceLoSpace = 1 << 4,
  kObjectSpaceAll = kObjectSpaceNewSpace | kObjectSpaceOldSpace |
                    kObjectSpaceCodeSpace | kObjectSpaceMapSpace |
                    kObjectSpaceLoSpace
};

  enum AllocationAction {
    kAllocationActionAllocate = 1 << 0,
    kAllocationActionFree = 1 << 1,
    kAllocationActionAll = kAllocationActionAllocate | kAllocationActionFree
  };

typedef void (*MemoryAllocationCallback)(ObjectSpace space,
                                         AllocationAction action,
                                         int size);

// --- Leave Script Callback ---
typedef void (*CallCompletedCallback)();

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

  V8_DEPRECATED("Use v8::Exception::CreateMessage(GetValue())->GetStackTrace()",
                V8_INLINE Local<StackTrace> GetStackTrace() const) {
    return stack_trace_;
  }

 private:
  Local<Promise> promise_;
  PromiseRejectEvent event_;
  Local<Value> value_;
  Local<StackTrace> stack_trace_;
};

typedef void (*PromiseRejectCallback)(PromiseRejectMessage message);

// --- Microtask Callback ---
typedef void (*MicrotaskCallback)(void* data);

// --- Failed Access Check Callback ---
typedef void (*FailedAccessCheckCallback)(Local<Object> target,
                                          AccessType type,
                                          Local<Value> data);

// --- AllowCodeGenerationFromStrings callbacks ---

/**
 * Callback to check if code generation from strings is allowed. See
 * Context::AllowCodeGenerationFromStrings.
 */
typedef bool (*AllowCodeGenerationFromStringsCallback)(Local<Context> context);

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

enum GCCallbackFlags {
  kNoGCCallbackFlags = 0,
  kGCCallbackFlagConstructRetainedObjectInfos = 1 << 1,
  kGCCallbackFlagForced = 1 << 2,
  kGCCallbackFlagSynchronousPhantomCallbackProcessing = 1 << 3
};

typedef void (*GCCallback)(GCType type, GCCallbackFlags flags);

typedef void (*InterruptCallback)(Isolate* isolate, void* data);


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
  size_t does_zap_garbage() { return does_zap_garbage_; }

 private:
  size_t total_heap_size_;
  size_t total_heap_size_executable_;
  size_t total_physical_size_;
  size_t total_available_size_;
  size_t used_heap_size_;
  size_t heap_size_limit_;
  bool does_zap_garbage_;

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

  // Type of event.
  EventType type;
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
    // Code postion
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
        : entry_hook(NULL),
          code_event_handler(NULL),
          snapshot_blob(NULL),
          counter_lookup_callback(NULL),
          create_histogram_callback(NULL),
          add_histogram_sample_callback(NULL),
          array_buffer_allocator(NULL) {}

    /**
     * The optional entry_hook allows the host application to provide the
     * address of a function that's invoked on entry to every V8-generated
     * function.  Note that entry_hook is invoked at the very start of each
     * generated function. Furthermore, if an  entry_hook is given, V8 will
     * always run without a context snapshot.
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

   private:
    Isolate* const isolate_;

    // Prevent copying of Scope objects.
    Scope(const Scope&);
    Scope& operator=(const Scope&);
  };


  /**
   * Assert that no Javascript code is invoked.
   */
  class V8_EXPORT DisallowJavascriptExecutionScope {
   public:
    enum OnFailure { CRASH_ON_FAILURE, THROW_ON_FAILURE };

    DisallowJavascriptExecutionScope(Isolate* isolate, OnFailure on_failure);
    ~DisallowJavascriptExecutionScope();

   private:
    bool on_failure_;
    void* internal_;

    // Prevent copying of Scope objects.
    DisallowJavascriptExecutionScope(const DisallowJavascriptExecutionScope&);
    DisallowJavascriptExecutionScope& operator=(
        const DisallowJavascriptExecutionScope&);
  };


  /**
   * Introduce exception to DisallowJavascriptExecutionScope.
   */
  class V8_EXPORT AllowJavascriptExecutionScope {
   public:
    explicit AllowJavascriptExecutionScope(Isolate* isolate);
    ~AllowJavascriptExecutionScope();

   private:
    void* internal_throws_;
    void* internal_assert_;

    // Prevent copying of Scope objects.
    AllowJavascriptExecutionScope(const AllowJavascriptExecutionScope&);
    AllowJavascriptExecutionScope& operator=(
        const AllowJavascriptExecutionScope&);
  };

  /**
   * Do not run microtasks while this scope is active, even if microtasks are
   * automatically executed otherwise.
   */
  class V8_EXPORT SuppressMicrotaskExecutionScope {
   public:
    explicit SuppressMicrotaskExecutionScope(Isolate* isolate);
    ~SuppressMicrotaskExecutionScope();

   private:
    internal::Isolate* isolate_;

    // Prevent copying of Scope objects.
    SuppressMicrotaskExecutionScope(const SuppressMicrotaskExecutionScope&);
    SuppressMicrotaskExecutionScope& operator=(
        const SuppressMicrotaskExecutionScope&);
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
    kUseCounterFeatureCount  // This enum value must be last.
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
   * Returns heap profiler for this isolate. Will return NULL until the isolate
   * is initialized.
   */
  HeapProfiler* GetHeapProfiler();

  /**
   * Returns CPU profiler for this isolate. Will return NULL unless the isolate
   * is initialized. It is the embedder's responsibility to stop all CPU
   * profiling activities if it has started any.
   */
  CpuProfiler* GetCpuProfiler();

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
  V8_DEPRECATE_SOON(
      "Calling context concept is not compatible with tail calls, and will be "
      "removed.",
      Local<Context> GetCallingContext());

  /** Returns the last context entered through V8's C++ API. */
  Local<Context> GetEnteredContext();

  /**
   * Schedules an exception to be thrown when returning to JavaScript.  When an
   * exception has been scheduled it is illegal to invoke any JavaScript
   * operation; the caller must return immediately and only after the exception
   * has been handled does it become legal to invoke JavaScript operations.
   */
  Local<Value> ThrowException(Local<Value> exception);

  /**
   * Allows the host application to group objects together. If one
   * object in the group is alive, all objects in the group are alive.
   * After each garbage collection, object groups are removed. It is
   * intended to be used in the before-garbage-collection callback
   * function, for instance to simulate DOM tree connections among JS
   * wrapper objects. Object groups for all dependent handles need to
   * be provided for kGCTypeMarkSweepCompact collections, for all other
   * garbage collection types it is sufficient to provide object groups
   * for partially dependent handles only.
   */
  template<typename T> void SetObjectGroupId(const Persistent<T>& object,
                                             UniqueId id);

  /**
   * Allows the host application to declare implicit references from an object
   * group to an object. If the objects of the object group are alive, the child
   * object is alive too. After each garbage collection, all implicit references
   * are removed. It is intended to be used in the before-garbage-collection
   * callback function.
   */
  template<typename T> void SetReferenceFromGroup(UniqueId id,
                                                  const Persistent<T>& child);

  /**
   * Allows the host application to declare implicit references from an object
   * to another object. If the parent object is alive, the child object is alive
   * too. After each garbage collection, all implicit references are removed. It
   * is intended to be used in the before-garbage-collection callback function.
   */
  template<typename T, typename S>
  void SetReference(const Persistent<T>& parent, const Persistent<S>& child);

  typedef void (*GCCallback)(Isolate* isolate, GCType type,
                             GCCallbackFlags flags);

  /**
   * Enables the host application to receive a notification before a
   * garbage collection. Allocations are allowed in the callback function,
   * but the callback is not re-entrant: if the allocation inside it will
   * trigger the garbage collection, the callback won't be called again.
   * It is possible to specify the GCType filter for your callback. But it is
   * not possible to register the same callback function two times with
   * different GCType filters.
   */
  void AddGCPrologueCallback(GCCallback callback,
                             GCType gc_type_filter = kGCTypeAll);

  /**
   * This function removes callback which was installed by
   * AddGCPrologueCallback function.
   */
  void RemoveGCPrologueCallback(GCCallback callback);

  /**
   * Enables the host application to receive a notification after a
   * garbage collection. Allocations are allowed in the callback function,
   * but the callback is not re-entrant: if the allocation inside it will
   * trigger the garbage collection, the callback won't be called again.
   * It is possible to specify the GCType filter for your callback. But it is
   * not possible to register the same callback function two times with
   * different GCType filters.
   */
  void AddGCEpilogueCallback(GCCallback callback,
                             GCType gc_type_filter = kGCTypeAll);

  /**
   * This function removes callback which was installed by
   * AddGCEpilogueCallback function.
   */
  void RemoveGCEpilogueCallback(GCCallback callback);

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
   * Set callback to notify about promise reject with no handler, or
   * revocation of such a previous notification once the handler is added.
   */
  void SetPromiseRejectCallback(PromiseRejectCallback callback);

  /**
   * Experimental: Runs the Microtask Work Queue until empty
   * Any exceptions thrown by microtask callbacks are swallowed.
   */
  void RunMicrotasks();

  /**
   * Experimental: Enqueues the callback to the Microtask Work Queue
   */
  void EnqueueMicrotask(Local<Function> microtask);

  /**
   * Experimental: Enqueues the callback to the Microtask Work Queue
   */
  void EnqueueMicrotask(MicrotaskCallback microtask, void* data = NULL);

   /**
   * Experimental: Controls whether the Microtask Work Queue is automatically
   * run when the script call depth decrements to zero.
   */
  void SetAutorunMicrotasks(bool autorun);

  /**
   * Experimental: Returns whether the Microtask Work Queue is automatically
   * run when the script call depth decrements to zero.
   */
  bool WillAutorunMicrotasks() const;

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

  V8_DEPRECATED("use IdleNotificationDeadline()",
                bool IdleNotification(int idle_time_in_ms));

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

  /**
   * Set the callback to invoke to check if code generation from
   * strings should be allowed.
   */
  void SetAllowCodeGenerationFromStringsCallback(
      AllowCodeGenerationFromStringsCallback callback);

  /**
  * Check if V8 is dead and therefore unusable.  This is the case after
  * fatal errors such as out-of-memory situations.
  */
  bool IsDead();

  /**
   * Adds a message listener.
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
   * Enables the host application to provide a mechanism to be notified
   * and perform custom logging when V8 Allocates Executable Memory.
   */
  void AddMemoryAllocationCallback(MemoryAllocationCallback callback,
                                   ObjectSpace space, AllocationAction action);

  /**
   * Removes callback that was installed by AddMemoryAllocationCallback.
   */
  void RemoveMemoryAllocationCallback(MemoryAllocationCallback callback);

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

 private:
  template <class K, class V, class Traits>
  friend class PersistentValueMapBase;

  Isolate();
  Isolate(const Isolate&);
  ~Isolate();
  Isolate& operator=(const Isolate&);
  void* operator new(size_t size);
  void operator delete(void*, size_t);

  void SetObjectGroupId(internal::Object** object, UniqueId id);
  void SetReferenceFromGroup(UniqueId id, internal::Object** object);
  void SetReference(internal::Object** parent, internal::Object** child);
  void ReportExternalAllocationLimitReached();
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
 * location to whereever the profiler stashed the original return address.
 *
 * \param return_addr_location points to a location on stack where a machine
 *    return address resides.
 * \returns either return_addr_location, or else a pointer to the profiler's
 *    copy of the original return address.
 *
 * \note the resolver function must not cause garbage collection.
 */
typedef uintptr_t (*ReturnAddressLocationResolver)(
    uintptr_t return_addr_location);


/**
 * Container class for static utility functions.
 */
class V8_EXPORT V8 {
 public:
  /** Set the callback to invoke in case of fatal errors. */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void SetFatalErrorHandler(FatalErrorCallback that));

  /**
   * Set the callback to invoke to check if code generation from
   * strings should be allowed.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version", void SetAllowCodeGenerationFromStringsCallback(
                                 AllowCodeGenerationFromStringsCallback that));

  /**
  * Check if V8 is dead and therefore unusable.  This is the case after
  * fatal errors such as out-of-memory situations.
  */
  V8_INLINE static V8_DEPRECATED("Use isolate version", bool IsDead());

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
   * Create a new isolate and context for the purpose of capturing a snapshot
   * Returns { NULL, 0 } on failure.
   * The caller owns the data array in the return value.
   */
  static StartupData CreateSnapshotDataBlob(const char* custom_source = NULL);

  /**
   * Adds a message listener.
   *
   * The same message listener can be added more than once and in that
   * case it will be called more than once for each message.
   *
   * If data is specified, it will be passed to the callback when it is called.
   * Otherwise, the exception object will be passed to the callback instead.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      bool AddMessageListener(MessageCallback that,
                              Local<Value> data = Local<Value>()));

  /**
   * Remove all message listeners from the specified callback function.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version", void RemoveMessageListeners(MessageCallback that));

  /**
   * Tells V8 to capture current stack trace when uncaught exception occurs
   * and report it to the message listeners. The option is off by default.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void SetCaptureStackTraceForUncaughtExceptions(
          bool capture, int frame_limit = 10,
          StackTrace::StackTraceOptions options = StackTrace::kOverview));

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

  /** Callback function for reporting failed access checks.*/
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void SetFailedAccessCheckCallbackFunction(FailedAccessCheckCallback));

  /**
   * Enables the host application to receive a notification before a
   * garbage collection.  Allocations are not allowed in the
   * callback function, you therefore cannot manipulate objects (set
   * or delete properties for example) since it is possible such
   * operations will result in the allocation of objects. It is possible
   * to specify the GCType filter for your callback. But it is not possible to
   * register the same callback function two times with different
   * GCType filters.
   */
  static V8_DEPRECATED(
      "Use isolate version",
      void AddGCPrologueCallback(GCCallback callback,
                                 GCType gc_type_filter = kGCTypeAll));

  /**
   * This function removes callback which was installed by
   * AddGCPrologueCallback function.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void RemoveGCPrologueCallback(GCCallback callback));

  /**
   * Enables the host application to receive a notification after a
   * garbage collection.  Allocations are not allowed in the
   * callback function, you therefore cannot manipulate objects (set
   * or delete properties for example) since it is possible such
   * operations will result in the allocation of objects. It is possible
   * to specify the GCType filter for your callback. But it is not possible to
   * register the same callback function two times with different
   * GCType filters.
   */
  static V8_DEPRECATED(
      "Use isolate version",
      void AddGCEpilogueCallback(GCCallback callback,
                                 GCType gc_type_filter = kGCTypeAll));

  /**
   * This function removes callback which was installed by
   * AddGCEpilogueCallback function.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void RemoveGCEpilogueCallback(GCCallback callback));

  /**
   * Enables the host application to provide a mechanism to be notified
   * and perform custom logging when V8 Allocates Executable Memory.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void AddMemoryAllocationCallback(MemoryAllocationCallback callback,
                                       ObjectSpace space,
                                       AllocationAction action));

  /**
   * Removes callback that was installed by AddMemoryAllocationCallback.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void RemoveMemoryAllocationCallback(MemoryAllocationCallback callback));

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
   * Forcefully terminate the current thread of JavaScript execution
   * in the given isolate.
   *
   * This method can be used by any thread even if that thread has not
   * acquired the V8 lock with a Locker object.
   *
   * \param isolate The isolate in which to terminate the current JS execution.
   */
  V8_INLINE static V8_DEPRECATED("Use isolate version",
                                 void TerminateExecution(Isolate* isolate));

  /**
   * Is V8 terminating JavaScript execution.
   *
   * Returns true if JavaScript execution is currently terminating
   * because of a call to TerminateExecution.  In that case there are
   * still JavaScript frames on the stack and the termination
   * exception is still active.
   *
   * \param isolate The isolate in which to check.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      bool IsExecutionTerminating(Isolate* isolate = NULL));

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
   *
   * \param isolate The isolate in which to resume execution capability.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version", void CancelTerminateExecution(Isolate* isolate));

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
   * Iterates through all external resources referenced from current isolate
   * heap.  GC is not invoked prior to iterating, therefore there is no
   * guarantee that visited objects are still alive.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void VisitExternalResources(ExternalResourceVisitor* visitor));

  /**
   * Iterates through all the persistent handles in the current isolate's heap
   * that have class_ids.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void VisitHandlesWithClassIds(PersistentHandleVisitor* visitor));

  /**
   * Iterates through all the persistent handles in isolate's heap that have
   * class_ids.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void VisitHandlesWithClassIds(Isolate* isolate,
                                    PersistentHandleVisitor* visitor));

  /**
   * Iterates through all the persistent handles in the current isolate's heap
   * that have class_ids and are candidates to be marked as partially dependent
   * handles. This will visit handles to young objects created since the last
   * garbage collection but is free to visit an arbitrary superset of these
   * objects.
   */
  V8_INLINE static V8_DEPRECATED(
      "Use isolate version",
      void VisitHandlesForPartialDependence(Isolate* isolate,
                                            PersistentHandleVisitor* visitor));

  /**
   * Initialize the ICU library bundled with V8. The embedder should only
   * invoke this method when using the bundled ICU. Returns true on success.
   *
   * If V8 was compiled with the ICU data in an external file, the location
   * of the data file has to be provided.
   */
  static bool InitializeICU(const char* icu_data_file = NULL);

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

 private:
  V8();

  static internal::Object** GlobalizeReference(internal::Isolate* isolate,
                                               internal::Object** handle);
  static internal::Object** CopyPersistent(internal::Object** handle);
  static void DisposeGlobal(internal::Object** global_handle);
  typedef WeakCallbackData<Value, void>::Callback WeakCallback;
  static void MakeWeak(internal::Object** global_handle, void* data,
                       WeakCallback weak_callback);
  static void MakeWeak(internal::Object** global_handle, void* data,
                       WeakCallbackInfo<void>::Callback weak_callback,
                       WeakCallbackType type);
  static void MakeWeak(internal::Object** global_handle, void* data,
                       // Must be 0 or -1.
                       int internal_field_index1,
                       // Must be 1 or -1.
                       int internal_field_index2,
                       WeakCallbackInfo<void>::Callback weak_callback);
  static void* ClearWeak(internal::Object** global_handle);
  static void Eternalize(Isolate* isolate,
                         Value* handle,
                         int* index);
  static Local<Value> GetEternal(Isolate* isolate, int index);

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
  V8_INLINE bool IsNothing() const { return !has_value; }
  V8_INLINE bool IsJust() const { return has_value; }

  // Will crash if the Maybe<> is nothing.
  V8_INLINE T FromJust() const {
    if (V8_UNLIKELY(!IsJust())) V8::FromJustIsNothing();
    return value;
  }

  V8_INLINE T FromMaybe(const T& default_value) const {
    return has_value ? value : default_value;
  }

  V8_INLINE bool operator==(const Maybe& other) const {
    return (IsJust() == other.IsJust()) &&
           (!IsJust() || FromJust() == other.FromJust());
  }

  V8_INLINE bool operator!=(const Maybe& other) const {
    return !operator==(other);
  }

 private:
  Maybe() : has_value(false) {}
  explicit Maybe(const T& t) : has_value(true), value(t) {}

  bool has_value;
  T value;

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
  V8_DEPRECATED("Use isolate version", TryCatch());

  /**
   * Creates a new try/catch block and registers it with v8.  Note that
   * all TryCatch blocks should be stack allocated because the memory
   * location itself is compared against JavaScript try/catch blocks.
   */
  TryCatch(Isolate* isolate);

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
  V8_DEPRECATE_SOON("Use maybe version.", Local<Value> StackTrace() const);
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
  static void* JSStackComparableAddress(v8::TryCatch* handler) {
    if (handler == NULL) return NULL;
    return handler->js_stack_comparable_address_;
  }

 private:
  void ResetInternal();

  // Make it hard to create heap-allocated TryCatch blocks.
  TryCatch(const TryCatch&);
  void operator=(const TryCatch&);
  void* operator new(size_t size);
  void operator delete(void*, size_t);

  v8::internal::Isolate* isolate_;
  v8::TryCatch* next_;
  void* exception_;
  void* message_obj_;
  void* js_stack_comparable_address_;
  bool is_verbose_ : 1;
  bool can_continue_ : 1;
  bool capture_message_ : 1;
  bool rethrow_ : 1;
  bool has_terminated_ : 1;

  friend class v8::internal::Isolate;
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
      Local<ObjectTemplate> global_template = Local<ObjectTemplate>(),
      Local<Value> global_object = Local<Value>());

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
  v8::Isolate* GetIsolate();

  /**
   * The field at kDebugIdIndex is reserved for V8 debugger implementation.
   * The value is propagated to the scripts compiled in given Context and
   * can be used for filtering scripts.
   */
  enum EmbedderDataFields { kDebugIdIndex = 0 };

  /**
   * Gets the embedder data with the given index, which must have been set by a
   * previous call to SetEmbedderData with the same index. Note that index 0
   * currently has a special meaning for Chrome's debugger.
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
   * index, which must have bees set by a previous call to
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
   * Estimate the memory in bytes retained by this context.
   */
  size_t EstimatedSize();

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

 private:
  friend class Value;
  friend class Script;
  friend class Object;
  friend class Function;

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
 * used to signal thead switches to V8.
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

 private:
  void Initialize(Isolate* isolate);

  bool has_lock_;
  bool top_level_;
  internal::Isolate* isolate_;

  // Disallow copying and assigning.
  Locker(const Locker&);
  void operator=(const Locker&);
};


// --- Implementation ---


namespace internal {

const int kApiPointerSize = sizeof(void*);  // NOLINT
const int kApiIntSize = sizeof(int);  // NOLINT
const int kApiInt64Size = sizeof(int64_t);  // NOLINT

// Tag information for HeapObject.
const int kHeapObjectTag = 1;
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
  static const int kMapInstanceTypeAndBitFieldOffset =
      1 * kApiPointerSize + kApiIntSize;
  static const int kStringResourceOffset = 3 * kApiPointerSize;

  static const int kOddballKindOffset = 4 * kApiPointerSize;
  static const int kForeignAddressOffset = kApiPointerSize;
  static const int kJSObjectHeaderSize = 3 * kApiPointerSize;
  static const int kFixedArrayHeaderSize = 2 * kApiPointerSize;
  static const int kContextHeaderSize = 2 * kApiPointerSize;
  static const int kContextEmbedderDataIndex = 5;
  static const int kFullStringRepresentationMask = 0x07;
  static const int kStringEncodingMask = 0x4;
  static const int kExternalTwoByteRepresentationTag = 0x02;
  static const int kExternalOneByteRepresentationTag = 0x06;

  static const int kIsolateEmbedderDataOffset = 0 * kApiPointerSize;
  static const int kAmountOfExternalAllocatedMemoryOffset =
      4 * kApiPointerSize;
  static const int kAmountOfExternalAllocatedMemoryAtLastGlobalGCOffset =
      kAmountOfExternalAllocatedMemoryOffset + kApiInt64Size;
  static const int kIsolateRootsOffset =
      kAmountOfExternalAllocatedMemoryAtLastGlobalGCOffset + kApiInt64Size +
      kApiPointerSize;
  static const int kUndefinedValueRootIndex = 5;
  static const int kNullValueRootIndex = 7;
  static const int kTrueValueRootIndex = 8;
  static const int kFalseValueRootIndex = 9;
  static const int kEmptyStringRootIndex = 10;

  // The external allocation limit should be below 256 MB on all architectures
  // to avoid that resource-constrained embedders run low on memory.
  static const int kExternalAllocationLimit = 192 * 1024 * 1024;

  static const int kNodeClassIdOffset = 1 * kApiPointerSize;
  static const int kNodeFlagsOffset = 1 * kApiPointerSize + 3;
  static const int kNodeStateMask = 0x7;
  static const int kNodeStateIsWeakValue = 2;
  static const int kNodeStateIsPendingValue = 3;
  static const int kNodeStateIsNearDeathValue = 4;
  static const int kNodeIsIndependentShift = 3;
  static const int kNodeIsPartiallyDependentShift = 4;
  static const int kNodeIsActiveShift = 4;

  static const int kJSObjectType = 0xb7;
  static const int kFirstNonstringType = 0x80;
  static const int kOddballType = 0x83;
  static const int kForeignType = 0x87;

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
    // Map::InstanceType is defined so that it will always be loaded into
    // the LS 8 bits of one 16-bit word, regardless of endianess.
    return ReadField<uint16_t>(map, kMapInstanceTypeAndBitFieldOffset) & 0xff;
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
  V8::Eternalize(isolate, reinterpret_cast<Value*>(*handle), &this->index_);
}


template<class T>
Local<T> Eternal<T>::Get(Isolate* isolate) {
  return Local<T>(reinterpret_cast<T*>(*V8::GetEternal(isolate, index_)));
}


template <class T>
Local<T> MaybeLocal<T>::ToLocalChecked() {
  if (V8_UNLIKELY(val_ == nullptr)) V8::ToLocalEmpty();
  return Local<T>(val_);
}


template <class T>
void* WeakCallbackInfo<T>::GetInternalField(int index) const {
#ifdef V8_ENABLE_CHECKS
  if (index < 0 || index >= kInternalFieldsInWeakCallback) {
    V8::InternalFieldOutOfBounds(index);
  }
#endif
  return internal_fields_[index];
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
template <typename S, typename P>
void PersistentBase<T>::SetWeak(
    P* parameter,
    typename WeakCallbackData<S, P>::Callback callback) {
  TYPE_CHECK(S, T);
  typedef typename WeakCallbackData<Value, void>::Callback Callback;
  V8::MakeWeak(reinterpret_cast<internal::Object**>(this->val_), parameter,
               reinterpret_cast<Callback>(callback));
}


template <class T>
template <typename P>
void PersistentBase<T>::SetWeak(
    P* parameter,
    typename WeakCallbackData<T, P>::Callback callback) {
  SetWeak<T, P>(parameter, callback);
}


template <class T>
template <typename P>
void PersistentBase<T>::SetPhantom(
    P* parameter, typename WeakCallbackInfo<P>::Callback callback,
    int internal_field_index1, int internal_field_index2) {
  typedef typename WeakCallbackInfo<void>::Callback Callback;
  V8::MakeWeak(reinterpret_cast<internal::Object**>(this->val_), parameter,
               internal_field_index1, internal_field_index2,
               reinterpret_cast<Callback>(callback));
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
template <typename P>
P* PersistentBase<T>::ClearWeak() {
  return reinterpret_cast<P*>(
    V8::ClearWeak(reinterpret_cast<internal::Object**>(this->val_)));
}


template <class T>
void PersistentBase<T>::MarkIndependent() {
  typedef internal::Internals I;
  if (this->IsEmpty()) return;
  I::UpdateNodeFlag(reinterpret_cast<internal::Object**>(this->val_),
                    true,
                    I::kNodeIsIndependentShift);
}


template <class T>
void PersistentBase<T>::MarkPartiallyDependent() {
  typedef internal::Internals I;
  if (this->IsEmpty()) return;
  I::UpdateNodeFlag(reinterpret_cast<internal::Object**>(this->val_),
                    true,
                    I::kNodeIsPartiallyDependentShift);
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

template<typename T>
Isolate* ReturnValue<T>::GetIsolate() {
  // Isolate is always the pointer below the default value on the stack.
  return *reinterpret_cast<Isolate**>(&value_[-2]);
}

template<typename T>
template<typename S>
void ReturnValue<T>::Set(S* whatever) {
  // Uncompilable to prevent inadvertent misuse.
  TYPE_CHECK(S*, Primitive);
}

template<typename T>
internal::Object* ReturnValue<T>::GetDefaultValue() {
  // Default value is always the pointer below value_ on the stack.
  return value_[-1];
}


template<typename T>
FunctionCallbackInfo<T>::FunctionCallbackInfo(internal::Object** implicit_args,
                                              internal::Object** values,
                                              int length,
                                              bool is_construct_call)
    : implicit_args_(implicit_args),
      values_(values),
      length_(length),
      is_construct_call_(is_construct_call) { }


template<typename T>
Local<Value> FunctionCallbackInfo<T>::operator[](int i) const {
  if (i < 0 || length_ <= i) return Local<Value>(*Undefined(GetIsolate()));
  return Local<Value>(reinterpret_cast<Value*>(values_ - i));
}


template<typename T>
Local<Function> FunctionCallbackInfo<T>::Callee() const {
  return Local<Function>(reinterpret_cast<Function*>(
      &implicit_args_[kCalleeIndex]));
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


template<typename T>
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
  return is_construct_call_ & 0x1;
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
                           Local<Boolean> resource_is_embedder_debug_script,
                           Local<Value> source_map_url,
                           Local<Boolean> resource_is_opaque)
    : resource_name_(resource_name),
      resource_line_offset_(resource_line_offset),
      resource_column_offset_(resource_column_offset),
      options_(!resource_is_embedder_debug_script.IsEmpty() &&
                   resource_is_embedder_debug_script->IsTrue(),
               !resource_is_shared_cross_origin.IsEmpty() &&
                   resource_is_shared_cross_origin->IsTrue(),
               !resource_is_opaque.IsEmpty() && resource_is_opaque->IsTrue()),
      script_id_(script_id),
      source_map_url_(source_map_url) {}

Local<Value> ScriptOrigin::ResourceName() const { return resource_name_; }


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


Local<Boolean> Boolean::New(Isolate* isolate, bool value) {
  return value ? True(isolate) : False(isolate);
}


void Template::Set(Isolate* isolate, const char* name, v8::Local<Data> value) {
  Set(v8::String::NewFromUtf8(isolate, name, NewStringType::kNormal)
          .ToLocalChecked(),
      value);
}


Local<Value> Object::GetInternalField(int index) {
#ifndef V8_ENABLE_CHECKS
  typedef internal::Object O;
  typedef internal::HeapObject HO;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O**>(this);
  // Fast path: If the object is a plain JSObject, which is the common case, we
  // know where to find the internal fields and can return the value directly.
  if (I::GetInstanceType(obj) == I::kJSObjectType) {
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
  if (V8_LIKELY(I::GetInstanceType(obj) == I::kJSObjectType)) {
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


Local<Number> Value::ToNumber() const {
  return ToNumber(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Number>());
}


Local<String> Value::ToString() const {
  return ToString(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<String>());
}


Local<String> Value::ToDetailString() const {
  return ToDetailString(Isolate::GetCurrent()->GetCurrentContext())
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


Local<Uint32> Value::ToUint32() const {
  return ToUint32(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Uint32>());
}


Local<Int32> Value::ToInt32() const {
  return ToInt32(Isolate::GetCurrent()->GetCurrentContext())
      .FromMaybe(Local<Int32>());
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


int64_t Isolate::AdjustAmountOfExternalAllocatedMemory(
    int64_t change_in_bytes) {
  typedef internal::Internals I;
  int64_t* amount_of_external_allocated_memory =
      reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(this) +
                                 I::kAmountOfExternalAllocatedMemoryOffset);
  int64_t* amount_of_external_allocated_memory_at_last_global_gc =
      reinterpret_cast<int64_t*>(
          reinterpret_cast<uint8_t*>(this) +
          I::kAmountOfExternalAllocatedMemoryAtLastGlobalGCOffset);
  int64_t amount = *amount_of_external_allocated_memory + change_in_bytes;
  if (change_in_bytes > 0 &&
      amount - *amount_of_external_allocated_memory_at_last_global_gc >
          I::kExternalAllocationLimit) {
    ReportExternalAllocationLimitReached();
  }
  *amount_of_external_allocated_memory = amount;
  return *amount_of_external_allocated_memory;
}


template<typename T>
void Isolate::SetObjectGroupId(const Persistent<T>& object,
                               UniqueId id) {
  TYPE_CHECK(Value, T);
  SetObjectGroupId(reinterpret_cast<v8::internal::Object**>(object.val_), id);
}


template<typename T>
void Isolate::SetReferenceFromGroup(UniqueId id,
                                    const Persistent<T>& object) {
  TYPE_CHECK(Value, T);
  SetReferenceFromGroup(id,
                        reinterpret_cast<v8::internal::Object**>(object.val_));
}


template<typename T, typename S>
void Isolate::SetReference(const Persistent<T>& parent,
                           const Persistent<S>& child) {
  TYPE_CHECK(Object, T);
  TYPE_CHECK(Value, S);
  SetReference(reinterpret_cast<v8::internal::Object**>(parent.val_),
               reinterpret_cast<v8::internal::Object**>(child.val_));
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


void V8::SetAllowCodeGenerationFromStringsCallback(
    AllowCodeGenerationFromStringsCallback callback) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->SetAllowCodeGenerationFromStringsCallback(callback);
}


bool V8::IsDead() {
  Isolate* isolate = Isolate::GetCurrent();
  return isolate->IsDead();
}


bool V8::AddMessageListener(MessageCallback that, Local<Value> data) {
  Isolate* isolate = Isolate::GetCurrent();
  return isolate->AddMessageListener(that, data);
}


void V8::RemoveMessageListeners(MessageCallback that) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->RemoveMessageListeners(that);
}


void V8::SetFailedAccessCheckCallbackFunction(
    FailedAccessCheckCallback callback) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->SetFailedAccessCheckCallbackFunction(callback);
}


void V8::SetCaptureStackTraceForUncaughtExceptions(
    bool capture, int frame_limit, StackTrace::StackTraceOptions options) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->SetCaptureStackTraceForUncaughtExceptions(capture, frame_limit,
                                                     options);
}


void V8::SetFatalErrorHandler(FatalErrorCallback callback) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->SetFatalErrorHandler(callback);
}


void V8::RemoveGCPrologueCallback(GCCallback callback) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->RemoveGCPrologueCallback(
      reinterpret_cast<v8::Isolate::GCCallback>(callback));
}


void V8::RemoveGCEpilogueCallback(GCCallback callback) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->RemoveGCEpilogueCallback(
      reinterpret_cast<v8::Isolate::GCCallback>(callback));
}


void V8::AddMemoryAllocationCallback(MemoryAllocationCallback callback,
                                     ObjectSpace space,
                                     AllocationAction action) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->AddMemoryAllocationCallback(callback, space, action);
}


void V8::RemoveMemoryAllocationCallback(MemoryAllocationCallback callback) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->RemoveMemoryAllocationCallback(callback);
}


void V8::TerminateExecution(Isolate* isolate) { isolate->TerminateExecution(); }


bool V8::IsExecutionTerminating(Isolate* isolate) {
  if (isolate == NULL) {
    isolate = Isolate::GetCurrent();
  }
  return isolate->IsExecutionTerminating();
}


void V8::CancelTerminateExecution(Isolate* isolate) {
  isolate->CancelTerminateExecution();
}


void V8::VisitExternalResources(ExternalResourceVisitor* visitor) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->VisitExternalResources(visitor);
}


void V8::VisitHandlesWithClassIds(PersistentHandleVisitor* visitor) {
  Isolate* isolate = Isolate::GetCurrent();
  isolate->VisitHandlesWithClassIds(visitor);
}


void V8::VisitHandlesWithClassIds(Isolate* isolate,
                                  PersistentHandleVisitor* visitor) {
  isolate->VisitHandlesWithClassIds(visitor);
}


void V8::VisitHandlesForPartialDependence(Isolate* isolate,
                                          PersistentHandleVisitor* visitor) {
  isolate->VisitHandlesForPartialDependence(visitor);
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
