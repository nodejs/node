// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_H_
#define V8_HANDLES_H_

#include "src/objects.h"

namespace v8 {
namespace internal {

// A Handle can be converted into a MaybeHandle. Converting a MaybeHandle
// into a Handle requires checking that it does not point to NULL.  This
// ensures NULL checks before use.
// Do not use MaybeHandle as argument type.

template<typename T>
class MaybeHandle {
 public:
  INLINE(MaybeHandle()) : location_(NULL) { }

  // Constructor for handling automatic up casting from Handle.
  // Ex. Handle<JSArray> can be passed when MaybeHandle<Object> is expected.
  template <class S> MaybeHandle(Handle<S> handle) {
#ifdef DEBUG
    T* a = NULL;
    S* b = NULL;
    a = b;  // Fake assignment to enforce type checks.
    USE(a);
#endif
    this->location_ = reinterpret_cast<T**>(handle.location());
  }

  // Constructor for handling automatic up casting.
  // Ex. MaybeHandle<JSArray> can be passed when Handle<Object> is expected.
  template <class S> MaybeHandle(MaybeHandle<S> maybe_handle) {
#ifdef DEBUG
    T* a = NULL;
    S* b = NULL;
    a = b;  // Fake assignment to enforce type checks.
    USE(a);
#endif
    location_ = reinterpret_cast<T**>(maybe_handle.location_);
  }

  INLINE(void Assert() const) { DCHECK(location_ != NULL); }
  INLINE(void Check() const) { CHECK(location_ != NULL); }

  INLINE(Handle<T> ToHandleChecked()) const {
    Check();
    return Handle<T>(location_);
  }

  // Convert to a Handle with a type that can be upcasted to.
  template <class S> INLINE(bool ToHandle(Handle<S>* out)) {
    if (location_ == NULL) {
      *out = Handle<T>::null();
      return false;
    } else {
      *out = Handle<T>(location_);
      return true;
    }
  }

  bool is_null() const { return location_ == NULL; }

 protected:
  T** location_;

  // MaybeHandles of different classes are allowed to access each
  // other's location_.
  template<class S> friend class MaybeHandle;
};

// ----------------------------------------------------------------------------
// A Handle provides a reference to an object that survives relocation by
// the garbage collector.
// Handles are only valid within a HandleScope.
// When a handle is created for an object a cell is allocated in the heap.

template<typename T>
class Handle {
 public:
  INLINE(explicit Handle(T** location)) { location_ = location; }
  INLINE(explicit Handle(T* obj));
  INLINE(Handle(T* obj, Isolate* isolate));

  // TODO(yangguo): Values that contain empty handles should be declared as
  // MaybeHandle to force validation before being used as handles.
  INLINE(Handle()) : location_(NULL) { }

  // Constructor for handling automatic up casting.
  // Ex. Handle<JSFunction> can be passed when Handle<Object> is expected.
  template <class S> Handle(Handle<S> handle) {
#ifdef DEBUG
    T* a = NULL;
    S* b = NULL;
    a = b;  // Fake assignment to enforce type checks.
    USE(a);
#endif
    location_ = reinterpret_cast<T**>(handle.location_);
  }

  INLINE(T* operator->() const) { return operator*(); }

  // Check if this handle refers to the exact same object as the other handle.
  INLINE(bool is_identical_to(const Handle<T> other) const);

  // Provides the C++ dereference operator.
  INLINE(T* operator*() const);

  // Returns the address to where the raw pointer is stored.
  INLINE(T** location() const);

  template <class S> static Handle<T> cast(Handle<S> that) {
    T::cast(*reinterpret_cast<T**>(that.location_));
    return Handle<T>(reinterpret_cast<T**>(that.location_));
  }

  // TODO(yangguo): Values that contain empty handles should be declared as
  // MaybeHandle to force validation before being used as handles.
  static Handle<T> null() { return Handle<T>(); }
  bool is_null() const { return location_ == NULL; }

  // Closes the given scope, but lets this handle escape. See
  // implementation in api.h.
  inline Handle<T> EscapeFrom(v8::EscapableHandleScope* scope);

#ifdef DEBUG
  enum DereferenceCheckMode { INCLUDE_DEFERRED_CHECK, NO_DEFERRED_CHECK };

  bool IsDereferenceAllowed(DereferenceCheckMode mode) const;
#endif  // DEBUG

 private:
  T** location_;

  // Handles of different classes are allowed to access each other's location_.
  template<class S> friend class Handle;
};


// Convenience wrapper.
template<class T>
inline Handle<T> handle(T* t, Isolate* isolate) {
  return Handle<T>(t, isolate);
}


// Convenience wrapper.
template<class T>
inline Handle<T> handle(T* t) {
  return Handle<T>(t, t->GetIsolate());
}


// Key comparison function for Map handles.
inline bool operator<(const Handle<Map>& lhs, const Handle<Map>& rhs) {
  // This is safe because maps don't move.
  return *lhs < *rhs;
}


class DeferredHandles;
class HandleScopeImplementer;


// A stack-allocated class that governs a number of local handles.
// After a handle scope has been created, all local handles will be
// allocated within that handle scope until either the handle scope is
// deleted or another handle scope is created.  If there is already a
// handle scope and a new one is created, all allocations will take
// place in the new handle scope until it is deleted.  After that,
// new handles will again be allocated in the original handle scope.
//
// After the handle scope of a local handle has been deleted the
// garbage collector will no longer track the object stored in the
// handle and may deallocate it.  The behavior of accessing a handle
// for which the handle scope has been deleted is undefined.
class HandleScope {
 public:
  explicit inline HandleScope(Isolate* isolate);

  inline ~HandleScope();

  // Counts the number of allocated handles.
  static int NumberOfHandles(Isolate* isolate);

  // Creates a new handle with the given value.
  template <typename T>
  static inline T** CreateHandle(Isolate* isolate, T* value);

  // Deallocates any extensions used by the current scope.
  static void DeleteExtensions(Isolate* isolate);

  static Address current_next_address(Isolate* isolate);
  static Address current_limit_address(Isolate* isolate);
  static Address current_level_address(Isolate* isolate);

  // Closes the HandleScope (invalidating all handles
  // created in the scope of the HandleScope) and returns
  // a Handle backed by the parent scope holding the
  // value of the argument handle.
  template <typename T>
  Handle<T> CloseAndEscape(Handle<T> handle_value);

  Isolate* isolate() { return isolate_; }

 private:
  // Prevent heap allocation or illegal handle scopes.
  HandleScope(const HandleScope&);
  void operator=(const HandleScope&);
  void* operator new(size_t size);
  void operator delete(void* size_t);

  Isolate* isolate_;
  Object** prev_next_;
  Object** prev_limit_;

  // Close the handle scope resetting limits to a previous state.
  static inline void CloseScope(Isolate* isolate,
                                Object** prev_next,
                                Object** prev_limit);

  // Extend the handle scope making room for more handles.
  static internal::Object** Extend(Isolate* isolate);

#ifdef ENABLE_HANDLE_ZAPPING
  // Zaps the handles in the half-open interval [start, end).
  static void ZapRange(Object** start, Object** end);
#endif

  friend class v8::HandleScope;
  friend class v8::internal::DeferredHandles;
  friend class v8::internal::HandleScopeImplementer;
  friend class v8::internal::Isolate;
};


class DeferredHandles;


class DeferredHandleScope {
 public:
  explicit DeferredHandleScope(Isolate* isolate);
  // The DeferredHandles object returned stores the Handles created
  // since the creation of this DeferredHandleScope.  The Handles are
  // alive as long as the DeferredHandles object is alive.
  DeferredHandles* Detach();
  ~DeferredHandleScope();

 private:
  Object** prev_limit_;
  Object** prev_next_;
  HandleScopeImplementer* impl_;

#ifdef DEBUG
  bool handles_detached_;
  int prev_level_;
#endif

  friend class HandleScopeImplementer;
};


// Seal off the current HandleScope so that new handles can only be created
// if a new HandleScope is entered.
class SealHandleScope BASE_EMBEDDED {
 public:
#ifndef DEBUG
  explicit SealHandleScope(Isolate* isolate) {}
  ~SealHandleScope() {}
#else
  explicit inline SealHandleScope(Isolate* isolate);
  inline ~SealHandleScope();
 private:
  Isolate* isolate_;
  Object** limit_;
  int level_;
#endif
};

struct HandleScopeData {
  internal::Object** next;
  internal::Object** limit;
  int level;

  void Initialize() {
    next = limit = NULL;
    level = 0;
  }
};

} }  // namespace v8::internal

#endif  // V8_HANDLES_H_
