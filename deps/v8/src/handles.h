// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_H_
#define V8_HANDLES_H_

#include <type_traits>

#include "include/v8.h"
#include "src/base/functional.h"
#include "src/base/macros.h"
#include "src/checks.h"
#include "src/globals.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class DeferredHandles;
class HandleScopeImplementer;
class Isolate;
class Object;

// ----------------------------------------------------------------------------
// Base class for Handle instantiations.  Don't use directly.
class HandleBase {
 public:
  V8_INLINE explicit HandleBase(Object** location) : location_(location) {}
  V8_INLINE explicit HandleBase(Object* object, Isolate* isolate);

  // Check if this handle refers to the exact same object as the other handle.
  V8_INLINE bool is_identical_to(const HandleBase that) const {
    // Dereferencing deferred handles to check object equality is safe.
    SLOW_DCHECK((this->location_ == nullptr ||
                 this->IsDereferenceAllowed(NO_DEFERRED_CHECK)) &&
                (that.location_ == nullptr ||
                 that.IsDereferenceAllowed(NO_DEFERRED_CHECK)));
    if (this->location_ == that.location_) return true;
    if (this->location_ == nullptr || that.location_ == nullptr) return false;
    return *this->location_ == *that.location_;
  }

  V8_INLINE bool is_null() const { return location_ == nullptr; }

  // Returns the raw address where this handle is stored. This should only be
  // used for hashing handles; do not ever try to dereference it.
  V8_INLINE Address address() const { return bit_cast<Address>(location_); }

 protected:
  // Provides the C++ dereference operator.
  V8_INLINE Object* operator*() const {
    SLOW_DCHECK(IsDereferenceAllowed(INCLUDE_DEFERRED_CHECK));
    return *location_;
  }

  // Returns the address to where the raw pointer is stored.
  V8_INLINE Object** location() const {
    SLOW_DCHECK(location_ == nullptr ||
                IsDereferenceAllowed(INCLUDE_DEFERRED_CHECK));
    return location_;
  }

  enum DereferenceCheckMode { INCLUDE_DEFERRED_CHECK, NO_DEFERRED_CHECK };
#ifdef DEBUG
  bool V8_EXPORT_PRIVATE IsDereferenceAllowed(DereferenceCheckMode mode) const;
#else
  V8_INLINE
  bool V8_EXPORT_PRIVATE IsDereferenceAllowed(DereferenceCheckMode mode) const {
    return true;
  }
#endif  // DEBUG

  Object** location_;
};


// ----------------------------------------------------------------------------
// A Handle provides a reference to an object that survives relocation by
// the garbage collector.
//
// Handles are only valid within a HandleScope. When a handle is created
// for an object a cell is allocated in the current HandleScope.
//
// Also note that Handles do not provide default equality comparison or hashing
// operators on purpose. Such operators would be misleading, because intended
// semantics is ambiguous between Handle location and object identity. Instead
// use either {is_identical_to} or {location} explicitly.
template <typename T>
class Handle final : public HandleBase {
 public:
  V8_INLINE explicit Handle(T** location = nullptr)
      : HandleBase(reinterpret_cast<Object**>(location)) {
    // Type check:
    static_assert(std::is_convertible<T*, Object*>::value,
                  "static type violation");
  }

  V8_INLINE Handle(T* object, Isolate* isolate);

  // Allocate a new handle for the object, do not canonicalize.
  V8_INLINE static Handle<T> New(T* object, Isolate* isolate);

  // Constructor for handling automatic up casting.
  // Ex. Handle<JSFunction> can be passed when Handle<Object> is expected.
  template <typename S, typename = typename std::enable_if<
                            std::is_convertible<S*, T*>::value>::type>
  V8_INLINE Handle(Handle<S> handle) : HandleBase(handle) {}

  V8_INLINE T* operator->() const { return operator*(); }

  // Provides the C++ dereference operator.
  V8_INLINE T* operator*() const {
    return reinterpret_cast<T*>(HandleBase::operator*());
  }

  // Returns the address to where the raw pointer is stored.
  V8_INLINE T** location() const {
    return reinterpret_cast<T**>(HandleBase::location());
  }

  template <typename S>
  static const Handle<T> cast(Handle<S> that) {
    T::cast(*reinterpret_cast<T**>(that.location()));
    return Handle<T>(reinterpret_cast<T**>(that.location_));
  }

  // TODO(yangguo): Values that contain empty handles should be declared as
  // MaybeHandle to force validation before being used as handles.
  static const Handle<T> null() { return Handle<T>(); }

  // Location equality.
  bool equals(Handle<T> other) const { return address() == other.address(); }

  // Provide function object for location equality comparison.
  struct equal_to : public std::binary_function<Handle<T>, Handle<T>, bool> {
    V8_INLINE bool operator()(Handle<T> lhs, Handle<T> rhs) const {
      return lhs.equals(rhs);
    }
  };

  // Provide function object for location hashing.
  struct hash : public std::unary_function<Handle<T>, size_t> {
    V8_INLINE size_t operator()(Handle<T> const& handle) const {
      return base::hash<Address>()(handle.address());
    }
  };

 private:
  // Handles of different classes are allowed to access each other's location_.
  template <typename>
  friend class Handle;
  // MaybeHandle is allowed to access location_.
  template <typename>
  friend class MaybeHandle;
};

template <typename T>
inline std::ostream& operator<<(std::ostream& os, Handle<T> handle);

template <typename T>
V8_INLINE Handle<T> handle(T* object, Isolate* isolate) {
  return Handle<T>(object, isolate);
}

// ----------------------------------------------------------------------------
// A Handle can be converted into a MaybeHandle. Converting a MaybeHandle
// into a Handle requires checking that it does not point to nullptr.  This
// ensures nullptr checks before use.
//
// Also note that Handles do not provide default equality comparison or hashing
// operators on purpose. Such operators would be misleading, because intended
// semantics is ambiguous between Handle location and object identity.
template <typename T>
class MaybeHandle final {
 public:
  V8_INLINE MaybeHandle() {}

  // Constructor for handling automatic up casting from Handle.
  // Ex. Handle<JSArray> can be passed when MaybeHandle<Object> is expected.
  template <typename S, typename = typename std::enable_if<
                            std::is_convertible<S*, T*>::value>::type>
  V8_INLINE MaybeHandle(Handle<S> handle)
      : location_(reinterpret_cast<T**>(handle.location_)) {}

  // Constructor for handling automatic up casting.
  // Ex. MaybeHandle<JSArray> can be passed when Handle<Object> is expected.
  template <typename S, typename = typename std::enable_if<
                            std::is_convertible<S*, T*>::value>::type>
  V8_INLINE MaybeHandle(MaybeHandle<S> maybe_handle)
      : location_(reinterpret_cast<T**>(maybe_handle.location_)) {}

  V8_INLINE MaybeHandle(T* object, Isolate* isolate)
      : MaybeHandle(handle(object, isolate)) {}

  V8_INLINE void Assert() const { DCHECK_NOT_NULL(location_); }
  V8_INLINE void Check() const { CHECK_NOT_NULL(location_); }

  V8_INLINE Handle<T> ToHandleChecked() const {
    Check();
    return Handle<T>(location_);
  }

  // Convert to a Handle with a type that can be upcasted to.
  template <typename S>
  V8_INLINE bool ToHandle(Handle<S>* out) const {
    if (location_ == nullptr) {
      *out = Handle<T>::null();
      return false;
    } else {
      *out = Handle<T>(location_);
      return true;
    }
  }

  // Returns the raw address where this handle is stored. This should only be
  // used for hashing handles; do not ever try to dereference it.
  V8_INLINE Address address() const { return bit_cast<Address>(location_); }

  bool is_null() const { return location_ == nullptr; }

 protected:
  T** location_ = nullptr;

  // MaybeHandles of different classes are allowed to access each
  // other's location_.
  template <typename>
  friend class MaybeHandle;
};

// A handle which contains a potentially weak pointer. Keeps it alive (strongly)
// while the MaybeObjectHandle is alive.
class MaybeObjectHandle {
 public:
  inline MaybeObjectHandle();
  inline MaybeObjectHandle(MaybeObject* object, Isolate* isolate);
  inline MaybeObjectHandle(Object* object, Isolate* isolate);
  inline explicit MaybeObjectHandle(Handle<Object> object);

  static inline MaybeObjectHandle Weak(Object* object, Isolate* isolate);
  static inline MaybeObjectHandle Weak(Handle<Object> object);

  inline MaybeObject* operator*() const;
  inline MaybeObject* operator->() const;
  inline Handle<Object> object() const;

  bool is_identical_to(const MaybeObjectHandle& other) const {
    Handle<Object> this_handle;
    Handle<Object> other_handle;
    return reference_type_ == other.reference_type_ &&
           handle_.ToHandle(&this_handle) ==
               other.handle_.ToHandle(&other_handle) &&
           this_handle.is_identical_to(other_handle);
  }

  bool is_null() const { return handle_.is_null(); }

 private:
  inline MaybeObjectHandle(Object* object,
                           HeapObjectReferenceType reference_type,
                           Isolate* isolate);
  inline MaybeObjectHandle(Handle<Object> object,
                           HeapObjectReferenceType reference_type);

  HeapObjectReferenceType reference_type_;
  MaybeHandle<Object> handle_;
};

// ----------------------------------------------------------------------------
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
  V8_EXPORT_PRIVATE static int NumberOfHandles(Isolate* isolate);

  // Create a new handle or lookup a canonical handle.
  V8_INLINE static Object** GetHandle(Isolate* isolate, Object* value);

  // Creates a new handle with the given value.
  V8_INLINE static Object** CreateHandle(Isolate* isolate, Object* value);

  // Deallocates any extensions used by the current scope.
  V8_EXPORT_PRIVATE static void DeleteExtensions(Isolate* isolate);

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

  // Limit for number of handles with --check-handle-count. This is
  // large enough to compile natives and pass unit tests with some
  // slack for future changes to natives.
  static const int kCheckHandleThreshold = 30 * 1024;

 private:
  // Prevent heap allocation or illegal handle scopes.
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
  V8_EXPORT_PRIVATE static Object** Extend(Isolate* isolate);

#ifdef ENABLE_HANDLE_ZAPPING
  // Zaps the handles in the half-open interval [start, end).
  V8_EXPORT_PRIVATE static void ZapRange(Object** start, Object** end);
#endif

  friend class v8::HandleScope;
  friend class DeferredHandles;
  friend class DeferredHandleScope;
  friend class HandleScopeImplementer;
  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(HandleScope);
};


// Forward declarations for CanonicalHandleScope.
template <typename V, class AllocationPolicy>
class IdentityMap;
class RootIndexMap;


// A CanonicalHandleScope does not open a new HandleScope. It changes the
// existing HandleScope so that Handles created within are canonicalized.
// This does not apply to nested inner HandleScopes unless a nested
// CanonicalHandleScope is introduced. Handles are only canonicalized within
// the same CanonicalHandleScope, but not across nested ones.
class V8_EXPORT_PRIVATE CanonicalHandleScope final {
 public:
  explicit CanonicalHandleScope(Isolate* isolate);
  ~CanonicalHandleScope();

 private:
  Object** Lookup(Object* object);

  Isolate* isolate_;
  Zone zone_;
  RootIndexMap* root_index_map_;
  IdentityMap<Object**, ZoneAllocationPolicy>* identity_map_;
  // Ordinary nested handle scopes within the current one are not canonical.
  int canonical_level_;
  // We may have nested canonical scopes. Handles are canonical within each one.
  CanonicalHandleScope* prev_canonical_scope_;

  friend class HandleScope;
};

// A DeferredHandleScope is a HandleScope in which handles are not destroyed
// when the DeferredHandleScope is left. Instead the DeferredHandleScope has to
// be detached with {Detach}, and the result of {Detach} has to be destroyed
// explicitly. A DeferredHandleScope should only be used with the following
// design pattern:
// 1) Open a HandleScope (not a DeferredHandleScope).
//    HandleScope scope(isolate_);
// 2) Create handles.
//    Handle<Object> h1 = handle(object1, isolate);
//    Handle<Object> h2 = handle(object2, isolate);
// 3) Open a DeferredHandleScope.
//    DeferredHandleScope deferred_scope(isolate);
// 4) Reopen handles which should be in the DeferredHandleScope, e.g only h1.
//    h1 = handle(*h1, isolate);
// 5) Detach the DeferredHandleScope.
//    DeferredHandles* deferred_handles = deferred_scope.Detach();
// 6) Destroy the deferred handles.
//    delete deferred_handles;
//
// Note: A DeferredHandleScope must not be opened within a DeferredHandleScope.
class V8_EXPORT_PRIVATE DeferredHandleScope final {
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
  bool handles_detached_ = false;
  int prev_level_;
#endif

  friend class HandleScopeImplementer;
};


// Seal off the current HandleScope so that new handles can only be created
// if a new HandleScope is entered.
class SealHandleScope final {
 public:
#ifndef DEBUG
  explicit SealHandleScope(Isolate* isolate) {}
  ~SealHandleScope() {}
#else
  explicit inline SealHandleScope(Isolate* isolate);
  inline ~SealHandleScope();
 private:
  Isolate* isolate_;
  Object** prev_limit_;
  int prev_sealed_level_;
#endif
};


struct HandleScopeData final {
  Object** next;
  Object** limit;
  int level;
  int sealed_level;
  CanonicalHandleScope* canonical_scope;

  void Initialize() {
    next = limit = nullptr;
    sealed_level = level = 0;
    canonical_scope = nullptr;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_H_
