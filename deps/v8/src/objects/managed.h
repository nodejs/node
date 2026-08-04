// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MANAGED_H_
#define V8_OBJECTS_MANAGED_H_

#include <memory>
#include <utility>

#include "include/v8-callbacks.h"
#include "include/v8-external-memory-accounter.h"
#include "include/v8-object.h"
#include "include/v8config.h"
#include "src/base/compiler-specific.h"
#include "src/handles/handles.h"
#include "src/objects/foreign.h"
#include "src/sandbox/external-pointer-table.h"

namespace v8::internal {

class Isolate;

// Mechanism for associating an ExternalPointerTag with a C++ type that is
// referenced via a Managed. Every such C++ type must have a unique
// ExternalPointerTag to ensure type-safe access to the external object.
//
// This mechanism supports two ways of associating tags with types:
//
// 1. By adding a 'static constexpr ExternalPointerTag kManagedTag` field to
//    the C++ class (preferred for C++ types defined in V8 code):
//
//      class MyCppClass {
//       public:
//        static constexpr ExternalPointerTag kManagedTag = kMyCppClassTag;
//        ...;
//
// 2. Through the ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED macro, which uses
//    template specialization (necessary for C++ types defined outside of V8):
//
//      ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(MyCppClass, kMyCppClassTag)
//
//    Note that the struct created by this macro must be visible when the
//    Managed<CppType> is used. In particular, there may be issues if the
//    CppType is only forward declared and the respective header isn't included.
//    Note also that this macro must be used inside the v8::internal namespace.
//
template <typename CppType>
struct TagForManaged {
  static constexpr ExternalPointerTag value = CppType::kManagedTag;
};

#define ASSIGN_EXTERNAL_POINTER_TAG_FOR_MANAGED(CppType, Tag) \
  template <>                                                 \
  struct TagForManaged<CppType> {                             \
    static constexpr ExternalPointerTag value = Tag;          \
  };

// Implements a doubly-linked lists of destructors for the isolate.
struct ManagedPtrDestructor
#ifdef V8_ENABLE_SANDBOX
    : public ExternalPointerTable::ManagedResource {
#else
    : public Malloced {
#endif  // V8_ENABLE_SANDBOX

  // Estimated size of external memory associated with the managed object.
  // This is used to adjust the garbage collector's heuristics upon
  // allocation and deallocation of a managed object.
  size_t estimated_size_ = 0;
  ManagedPtrDestructor* prev_ = nullptr;
  ManagedPtrDestructor* next_ = nullptr;
  void* shared_ptr_ptr_ = nullptr;
  void (*destructor_)(void* shared_ptr) = nullptr;
  SharedFlag shared_ = SharedFlag::kNo;
  Address* global_handle_location_ = nullptr;
  V8_NO_UNIQUE_ADDRESS ExternalMemoryAccounter external_memory_accounter_;

  ManagedPtrDestructor(size_t estimated_size, void* shared_ptr_ptr,
                       void (*destructor)(void*), SharedFlag shared)
      : estimated_size_(estimated_size),
        shared_ptr_ptr_(shared_ptr_ptr),
        destructor_(destructor),
        shared_(shared) {}

  V8_EXPORT_PRIVATE void UpdateEstimatedSize(size_t new_estimated_size,
                                             Isolate* isolate);
};

// The GC finalizer of a managed object, which does not depend on
// the template parameter.
V8_EXPORT_PRIVATE void ManagedObjectFinalizer(
    const v8::WeakCallbackInfo<void>& data);

// {Managed<T>} is essentially a {std::shared_ptr<T>} allocated on the heap
// that can be used to manage the lifetime of C++ objects that are shared
// across multiple isolates.
// When a {Managed<T>} object is garbage collected (or an isolate which
// contains {Managed<T>} is torn down), the {Managed<T>} deletes its underlying
// {std::shared_ptr<T>}, thereby decrementing its internal reference count,
// which will delete the C++ object when the reference count drops to 0.
template <class CppType>
class Managed : public Foreign {
 public:
  // Exposes the underlying C++ object and keeps the ref counter incremented.
  //
  // Usage examples:
  //
  //   managed1.ptr()->DoStuff();  // `Ptr` lives till end of full-expression
  //
  //   ReadFrom(*managed2.ptr());  // ditto
  //
  //   Managed<T>::Ptr ptr = managed3.ptr();  // kept for multiple statements
  //   ReadFrom(*ptr);
  //   WriteTo(ptr.raw());
  //
  // Note: it's generally unsafe to dereference the raw pointer after the `Ptr`
  // went out of scope and GC happened.
  class Ptr final {
   public:
    V8_INLINE Ptr() = default;

    V8_INLINE Ptr(Ptr&& other) V8_NOEXCEPT = default;
    Ptr(const Ptr&) = delete;
    V8_INLINE Ptr& operator=(Ptr&& other) V8_NOEXCEPT = default;
    Ptr& operator=(const Ptr&) = delete;

    V8_INLINE CppType* operator->() V8_LIFETIME_BOUND { return ptr_.get(); }
    V8_INLINE const CppType* operator->() const V8_LIFETIME_BOUND {
      return ptr_.get();
    }

    V8_INLINE CppType& operator*() V8_LIFETIME_BOUND { return *ptr_; }
    V8_INLINE const CppType& operator*() const V8_LIFETIME_BOUND {
      return *ptr_;
    }

    V8_INLINE CppType* raw() V8_LIFETIME_BOUND { return ptr_.get(); }
    V8_INLINE const CppType* raw() const V8_LIFETIME_BOUND {
      return ptr_.get();
    }

    // Only use these when necessary, since unlike `std::shared_ptr` the `Ptr`
    // class uses "lifetimebound" annotations for static analysis.
    V8_INLINE std::shared_ptr<CppType> as_shared_ptr() & { return ptr_; }
    V8_INLINE std::shared_ptr<CppType> as_shared_ptr() && {
      return std::move(ptr_);
    }

    V8_INLINE void Reset() { ptr_.reset(); }

    V8_INLINE explicit operator bool() const { return ptr_ != nullptr; }
    V8_INLINE bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
    V8_INLINE bool operator==(const Ptr&) const = default;
    V8_INLINE bool operator==(const std::shared_ptr<CppType>& other) const {
      return ptr_ == other;
    }

   private:
    friend class Managed;

    V8_INLINE explicit Ptr(std::shared_ptr<CppType> ptr)
        : ptr_(std::move(ptr)) {}

    std::shared_ptr<CppType> ptr_;
  };

  // Get a raw pointer to the C++ object. The returned pointer is only valid as
  // long as no GC happens; prefer `ptr()` unless on performance-critical code
  // paths.
  V8_INLINE CppType* raw(
      const DisallowGarbageCollection& no_gc V8_LIFETIME_BOUND) {
    return GetSharedPtrPtr(GetDestructor())->get();
  }

  // Get the wrapper that exposes access to the C++ object.
  //
  // The wrapper keeps the ref counter incremented, guaranteeing that the C++
  // object stays alive even if our Foreign gets corrupted by an in-sandbox
  // corruption and collected by GC.
  V8_INLINE Ptr ptr() { return Ptr(*GetSharedPtrPtr(GetDestructor())); }

  // Read back the memory estimate that was provided when creating this Managed.
  size_t estimated_size() const { return GetDestructor()->estimated_size_; }

  // Set a new managed object, dropping the old reference.
  void SetManagedObject(std::shared_ptr<CppType> new_managed, Isolate* isolate,
                        size_t new_estimated_size) {
    ManagedPtrDestructor* destructor = GetDestructor();
    *GetSharedPtrPtr(destructor) = std::move(new_managed);
    destructor->UpdateEstimatedSize(new_estimated_size, isolate);
  }

  void UpdateEstimatedSize(Isolate* isolate, size_t new_estimated_size) {
    GetDestructor()->UpdateEstimatedSize(new_estimated_size, isolate);
  }

  // Create a {Managed>} from an existing {std::shared_ptr} or {std::unique_ptr}
  // (which will automatically convert to a {std::shared_ptr}).
  static DirectHandle<Managed<CppType>> From(
      Isolate* isolate, size_t estimated_size,
      std::shared_ptr<CppType> shared_ptr,
      AllocationType allocation_type = AllocationType::kYoung);

 private:
  friend class Tagged<Managed>;

  // Internally this {Foreign} object stores a pointer to a
  // ManagedPtrDestructor, which again stores the std::shared_ptr.
  ManagedPtrDestructor* GetDestructor() const {
    static constexpr ExternalPointerTag kTag = TagForManaged<CppType>::value;
    return reinterpret_cast<ManagedPtrDestructor*>(foreign_address<kTag>());
  }

  static std::shared_ptr<CppType>* GetSharedPtrPtr(
      ManagedPtrDestructor* destructor) {
    return reinterpret_cast<std::shared_ptr<CppType>*>(
        destructor->shared_ptr_ptr_);
  }
};

// {TrustedManaged<T>} is semantically equivalent to {Managed<T>}, but lives in
// the trusted space. It is thus based on {TrustedForeign} instead of {Foreign}
// and does not need any tagging.
template <class CppType>
class TrustedManaged : public TrustedForeign {
 public:
  // For every object, add a `->` operator which returns a pointer to this
  // object. This will allow smoother transition between T and Tagged<T>.
  TrustedManaged* operator->() { return this; }
  const TrustedManaged* operator->() const { return this; }

  // Get a raw pointer to the C++ object.
  V8_INLINE CppType* raw() { return GetSharedPtrPtr()->get(); }

  // Get a reference to the shared pointer to the C++ object.
  V8_INLINE const std::shared_ptr<CppType>& get() { return *GetSharedPtrPtr(); }

  // Create a {Managed<CppType>} from an existing {std::shared_ptr} or
  // {std::unique_ptr} (which will implicitly convert to {std::shared_ptr}).
  static DirectHandle<TrustedManaged<CppType>> From(
      Isolate* isolate, size_t estimated_size,
      std::shared_ptr<CppType> shared_ptr, SharedFlag shared);

 private:
  friend class Tagged<TrustedManaged>;

  // Internally the {TrustedForeign} stores a pointer to the
  // {std::shared_ptr<CppType>}.
  std::shared_ptr<CppType>* GetSharedPtrPtr() const {
    auto destructor =
        reinterpret_cast<ManagedPtrDestructor*>(foreign_address());
    return reinterpret_cast<std::shared_ptr<CppType>*>(
        destructor->shared_ptr_ptr_);
  }
};

}  // namespace v8::internal

#endif  // V8_OBJECTS_MANAGED_H_
