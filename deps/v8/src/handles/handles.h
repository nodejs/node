// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_HANDLES_H_
#define V8_HANDLES_HANDLES_H_

#include <type_traits>

#include "src/base/functional.h"
#include "src/base/macros.h"
#include "src/common/checks.h"
#include "src/common/globals.h"
#include "src/zone/zone.h"

namespace v8 {

class HandleScope;

namespace internal {

// Forward declarations.
class HandleScopeImplementer;
class Isolate;
class LocalHeap;
class LocalIsolate;
template <typename T>
class MaybeHandle;
class Object;
class OrderedHashMap;
class OrderedHashSet;
class OrderedNameDictionary;
class RootVisitor;
class SmallOrderedHashMap;
class SmallOrderedHashSet;
class SmallOrderedNameDictionary;
class SwissNameDictionary;
class WasmExportedFunctionData;

// ----------------------------------------------------------------------------
// Base class for Handle instantiations.  Don't use directly.
class HandleBase {
 public:
  V8_INLINE explicit HandleBase(Address* location) : location_(location) {}
  V8_INLINE explicit HandleBase(Address object, Isolate* isolate);
  V8_INLINE explicit HandleBase(Address object, LocalIsolate* isolate);
  V8_INLINE explicit HandleBase(Address object, LocalHeap* local_heap);

  // Check if this handle refers to the exact same object as the other handle.
  V8_INLINE bool is_identical_to(const HandleBase that) const;
  V8_INLINE bool is_null() const { return location_ == nullptr; }

  // Returns the raw address where this handle is stored. This should only be
  // used for hashing handles; do not ever try to dereference it.
  V8_INLINE Address address() const { return bit_cast<Address>(location_); }

  // Returns the address to where the raw pointer is stored.
  // TODO(leszeks): This should probably be a const Address*, to encourage using
  // PatchValue for modifying the handle's value.
  V8_INLINE Address* location() const {
    SLOW_DCHECK(location_ == nullptr || IsDereferenceAllowed());
    return location_;
  }

 protected:
#ifdef DEBUG
  bool V8_EXPORT_PRIVATE IsDereferenceAllowed() const;
#else
  V8_INLINE
  bool V8_EXPORT_PRIVATE IsDereferenceAllowed() const { return true; }
#endif  // DEBUG

  // This uses type Address* as opposed to a pointer type to a typed
  // wrapper class, because it doesn't point to instances of such a
  // wrapper class. Design overview: https://goo.gl/Ph4CGz
  Address* location_;
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
  // {ObjectRef} is returned by {Handle::operator->}. It should never be stored
  // anywhere or used in any other code; no one should ever have to spell out
  // {ObjectRef} in code. Its only purpose is to be dereferenced immediately by
  // "operator-> chaining". Returning the address of the field is valid because
  // this objects lifetime only ends at the end of the full statement.
  class ObjectRef {
   public:
    T* operator->() { return &object_; }

   private:
    friend class Handle<T>;
    explicit ObjectRef(T object) : object_(object) {}

    T object_;
  };

  V8_INLINE explicit Handle() : HandleBase(nullptr) {
    // Skip static type check in order to allow Handle<XXX>::null() as default
    // parameter values in non-inl header files without requiring full
    // definition of type XXX.
  }

  V8_INLINE explicit Handle(Address* location) : HandleBase(location) {
    // This static type check also fails for forward class declarations.
    static_assert(std::is_convertible<T*, Object*>::value,
                  "static type violation");
    // TODO(jkummerow): Runtime type check here as a SLOW_DCHECK?
  }

  V8_INLINE Handle(T object, Isolate* isolate);
  V8_INLINE Handle(T object, LocalIsolate* isolate);
  V8_INLINE Handle(T object, LocalHeap* local_heap);

  // Allocate a new handle for the object, do not canonicalize.
  V8_INLINE static Handle<T> New(T object, Isolate* isolate);

  // Constructor for handling automatic up casting.
  // Ex. Handle<JSFunction> can be passed when Handle<Object> is expected.
  template <typename S, typename = typename std::enable_if<
                            std::is_convertible<S*, T*>::value>::type>
  V8_INLINE Handle(Handle<S> handle) : HandleBase(handle) {}

  V8_INLINE ObjectRef operator->() const { return ObjectRef{**this}; }

  V8_INLINE T operator*() const {
    // unchecked_cast because we rather trust Handle<T> to contain a T than
    // include all the respective -inl.h headers for SLOW_DCHECKs.
    SLOW_DCHECK(IsDereferenceAllowed());
    return T::unchecked_cast(Object(*location()));
  }

  template <typename S>
  inline static const Handle<T> cast(Handle<S> that);

  // Consider declaring values that contain empty handles as
  // MaybeHandle to force validation before being used as handles.
  static const Handle<T> null() { return Handle<T>(); }

  // Location equality.
  bool equals(Handle<T> other) const { return address() == other.address(); }

  // Patches this Handle's value, in-place, with a new value. All handles with
  // the same location will see this update.
  void PatchValue(T new_value) {
    SLOW_DCHECK(location_ != nullptr && IsDereferenceAllowed());
    *location_ = new_value.ptr();
  }

  // Provide function object for location equality comparison.
  struct equal_to {
    V8_INLINE bool operator()(Handle<T> lhs, Handle<T> rhs) const {
      return lhs.equals(rhs);
    }
  };

  // Provide function object for location hashing.
  struct hash {
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
std::ostream& operator<<(std::ostream& os, Handle<T> handle);

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
class V8_NODISCARD HandleScope {
 public:
  explicit V8_INLINE HandleScope(Isolate* isolate);
  inline HandleScope(HandleScope&& other) V8_NOEXCEPT;
  HandleScope(const HandleScope&) = delete;
  HandleScope& operator=(const HandleScope&) = delete;

  // Allow placement new.
  void* operator new(size_t size, void* storage) {
    return ::operator new(size, storage);
  }

  // Prevent heap allocation or illegal handle scopes.
  void* operator new(size_t size) = delete;
  void operator delete(void* size_t) = delete;

  V8_INLINE ~HandleScope();

  inline HandleScope& operator=(HandleScope&& other) V8_NOEXCEPT;

  // Counts the number of allocated handles.
  V8_EXPORT_PRIVATE static int NumberOfHandles(Isolate* isolate);

  // Create a new handle or lookup a canonical handle.
  V8_INLINE static Address* GetHandle(Isolate* isolate, Address value);

  // Creates a new handle with the given value.
  V8_INLINE static Address* CreateHandle(Isolate* isolate, Address value);

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
  Isolate* isolate_;
  Address* prev_next_;
  Address* prev_limit_;

  // Close the handle scope resetting limits to a previous state.
  static V8_INLINE void CloseScope(Isolate* isolate, Address* prev_next,
                                   Address* prev_limit);

  // Extend the handle scope making room for more handles.
  V8_EXPORT_PRIVATE static Address* Extend(Isolate* isolate);

#ifdef ENABLE_HANDLE_ZAPPING
  // Zaps the handles in the half-open interval [start, end).
  V8_EXPORT_PRIVATE static void ZapRange(Address* start, Address* end);
#endif

  friend class v8::HandleScope;
  friend class HandleScopeImplementer;
  friend class Isolate;
  friend class LocalHandles;
  friend class LocalHandleScope;
  friend class PersistentHandles;
};

// Forward declarations for CanonicalHandleScope.
template <typename V, class AllocationPolicy>
class IdentityMap;
class RootIndexMap;
class OptimizedCompilationInfo;

namespace maglev {
class ExportedMaglevCompilationInfo;
}  // namespace maglev

using CanonicalHandlesMap = IdentityMap<Address*, ZoneAllocationPolicy>;

// A CanonicalHandleScope does not open a new HandleScope. It changes the
// existing HandleScope so that Handles created within are canonicalized.
// This does not apply to nested inner HandleScopes unless a nested
// CanonicalHandleScope is introduced. Handles are only canonicalized within
// the same CanonicalHandleScope, but not across nested ones.
class V8_EXPORT_PRIVATE V8_NODISCARD CanonicalHandleScope {
 public:
  // If no Zone is passed to this constructor, we create (and own) a new zone.
  // To properly dispose of said zone, we need to first free the identity_map_
  // which is done manually even though identity_map_ is a unique_ptr.
  explicit CanonicalHandleScope(Isolate* isolate, Zone* zone = nullptr);
  ~CanonicalHandleScope();

 protected:
  std::unique_ptr<CanonicalHandlesMap> DetachCanonicalHandles();

  Zone* zone_;  // *Not* const, may be mutated by subclasses.

 private:
  Address* Lookup(Address object);

  Isolate* const isolate_;
  RootIndexMap* root_index_map_;
  std::unique_ptr<CanonicalHandlesMap> identity_map_;
  // Ordinary nested handle scopes within the current one are not canonical.
  int canonical_level_;
  // We may have nested canonical scopes. Handles are canonical within each one.
  CanonicalHandleScope* prev_canonical_scope_;

  friend class HandleScope;
};

template <class CompilationInfoT>
class V8_EXPORT_PRIVATE V8_NODISCARD CanonicalHandleScopeForOptimization final
    : public CanonicalHandleScope {
 public:
  // We created the
  // CanonicalHandlesMap on the compilation info's zone(). In the
  // CanonicalHandleScope destructor we hand off the canonical handle map to the
  // compilation info. The compilation info is responsible for the disposal.
  explicit CanonicalHandleScopeForOptimization(Isolate* isolate,
                                               CompilationInfoT* info);
  ~CanonicalHandleScopeForOptimization();

 private:
  CompilationInfoT* const info_;
};

using CanonicalHandleScopeForTurbofan =
    CanonicalHandleScopeForOptimization<OptimizedCompilationInfo>;
using CanonicalHandleScopeForMaglev =
    CanonicalHandleScopeForOptimization<maglev::ExportedMaglevCompilationInfo>;

// Seal off the current HandleScope so that new handles can only be created
// if a new HandleScope is entered.
class V8_NODISCARD SealHandleScope final {
 public:
#ifndef DEBUG
  explicit SealHandleScope(Isolate* isolate) {}
  ~SealHandleScope() = default;
#else
  explicit inline SealHandleScope(Isolate* isolate);
  inline ~SealHandleScope();

 private:
  Isolate* isolate_;
  Address* prev_limit_;
  int prev_sealed_level_;
#endif
};

struct HandleScopeData final {
  Address* next;
  Address* limit;
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

#endif  // V8_HANDLES_HANDLES_H_
