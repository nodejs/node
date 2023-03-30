// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_GC_INFO_H_
#define INCLUDE_CPPGC_INTERNAL_GC_INFO_H_

#include <atomic>
#include <cstdint>
#include <type_traits>

#include "cppgc/internal/finalizer-trait.h"
#include "cppgc/internal/logging.h"
#include "cppgc/internal/name-trait.h"
#include "cppgc/trace-trait.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

using GCInfoIndex = uint16_t;

struct V8_EXPORT EnsureGCInfoIndexTrait final {
  // Acquires a new GC info object and updates `registered_index` with the index
  // that identifies that new info accordingly.
  template <typename T>
  V8_INLINE static void EnsureIndex(
      std::atomic<GCInfoIndex>& registered_index) {
    EnsureGCInfoIndexTraitDispatch<T>{}(registered_index);
  }

 private:
  template <typename T, bool = std::is_polymorphic<T>::value,
            bool = FinalizerTrait<T>::HasFinalizer(),
            bool = NameTrait<T>::HasNonHiddenName()>
  struct EnsureGCInfoIndexTraitDispatch;

  static void V8_PRESERVE_MOST
  EnsureGCInfoIndexPolymorphic(std::atomic<GCInfoIndex>&, TraceCallback,
                               FinalizationCallback, NameCallback);
  static void V8_PRESERVE_MOST EnsureGCInfoIndexPolymorphic(
      std::atomic<GCInfoIndex>&, TraceCallback, FinalizationCallback);
  static void V8_PRESERVE_MOST EnsureGCInfoIndexPolymorphic(
      std::atomic<GCInfoIndex>&, TraceCallback, NameCallback);
  static void V8_PRESERVE_MOST
  EnsureGCInfoIndexPolymorphic(std::atomic<GCInfoIndex>&, TraceCallback);
  static void V8_PRESERVE_MOST
  EnsureGCInfoIndexNonPolymorphic(std::atomic<GCInfoIndex>&, TraceCallback,
                                  FinalizationCallback, NameCallback);
  static void V8_PRESERVE_MOST EnsureGCInfoIndexNonPolymorphic(
      std::atomic<GCInfoIndex>&, TraceCallback, FinalizationCallback);
  static void V8_PRESERVE_MOST EnsureGCInfoIndexNonPolymorphic(
      std::atomic<GCInfoIndex>&, TraceCallback, NameCallback);
  static void V8_PRESERVE_MOST
  EnsureGCInfoIndexNonPolymorphic(std::atomic<GCInfoIndex>&, TraceCallback);
};

#define DISPATCH(is_polymorphic, has_finalizer, has_non_hidden_name, function) \
  template <typename T>                                                        \
  struct EnsureGCInfoIndexTrait::EnsureGCInfoIndexTraitDispatch<               \
      T, is_polymorphic, has_finalizer, has_non_hidden_name> {                 \
    V8_INLINE void operator()(std::atomic<GCInfoIndex>& registered_index) {    \
      function;                                                                \
    }                                                                          \
  };

// --------------------------------------------------------------------- //
// DISPATCH(is_polymorphic, has_finalizer, has_non_hidden_name, function)
// --------------------------------------------------------------------- //
DISPATCH(true, true, true,                                               //
         EnsureGCInfoIndexPolymorphic(registered_index,                  //
                                      TraceTrait<T>::Trace,              //
                                      FinalizerTrait<T>::kCallback,      //
                                      NameTrait<T>::GetName))            //
DISPATCH(true, true, false,                                              //
         EnsureGCInfoIndexPolymorphic(registered_index,                  //
                                      TraceTrait<T>::Trace,              //
                                      FinalizerTrait<T>::kCallback))     //
DISPATCH(true, false, true,                                              //
         EnsureGCInfoIndexPolymorphic(registered_index,                  //
                                      TraceTrait<T>::Trace,              //
                                      NameTrait<T>::GetName))            //
DISPATCH(true, false, false,                                             //
         EnsureGCInfoIndexPolymorphic(registered_index,                  //
                                      TraceTrait<T>::Trace))             //
DISPATCH(false, true, true,                                              //
         EnsureGCInfoIndexNonPolymorphic(registered_index,               //
                                         TraceTrait<T>::Trace,           //
                                         FinalizerTrait<T>::kCallback,   //
                                         NameTrait<T>::GetName))         //
DISPATCH(false, true, false,                                             //
         EnsureGCInfoIndexNonPolymorphic(registered_index,               //
                                         TraceTrait<T>::Trace,           //
                                         FinalizerTrait<T>::kCallback))  //
DISPATCH(false, false, true,                                             //
         EnsureGCInfoIndexNonPolymorphic(registered_index,               //
                                         TraceTrait<T>::Trace,           //
                                         NameTrait<T>::GetName))         //
DISPATCH(false, false, false,                                            //
         EnsureGCInfoIndexNonPolymorphic(registered_index,               //
                                         TraceTrait<T>::Trace))          //

#undef DISPATCH

// Fold types based on finalizer behavior. Note that finalizer characteristics
// align with trace behavior, i.e., destructors are virtual when trace methods
// are and vice versa.
template <typename T, typename ParentMostGarbageCollectedType>
struct GCInfoFolding {
  static constexpr bool kHasVirtualDestructorAtBase =
      std::has_virtual_destructor<ParentMostGarbageCollectedType>::value;
  static constexpr bool kBothTypesAreTriviallyDestructible =
      std::is_trivially_destructible<ParentMostGarbageCollectedType>::value &&
      std::is_trivially_destructible<T>::value;
  static constexpr bool kHasCustomFinalizerDispatchAtBase =
      internal::HasFinalizeGarbageCollectedObject<
          ParentMostGarbageCollectedType>::value;
#ifdef CPPGC_SUPPORTS_OBJECT_NAMES
  static constexpr bool kWantsDetailedObjectNames = true;
#else   // !CPPGC_SUPPORTS_OBJECT_NAMES
  static constexpr bool kWantsDetailedObjectNames = false;
#endif  // !CPPGC_SUPPORTS_OBJECT_NAMES

  // Folding would regresses name resolution when deriving names from C++
  // class names as it would just folds a name to the base class name.
  using ResultType = std::conditional_t<(kHasVirtualDestructorAtBase ||
                                         kBothTypesAreTriviallyDestructible ||
                                         kHasCustomFinalizerDispatchAtBase) &&
                                            !kWantsDetailedObjectNames,
                                        ParentMostGarbageCollectedType, T>;
};

// Trait determines how the garbage collector treats objects wrt. to traversing,
// finalization, and naming.
template <typename T>
struct GCInfoTrait final {
  V8_INLINE static GCInfoIndex Index() {
    static_assert(sizeof(T), "T must be fully defined");
    static std::atomic<GCInfoIndex>
        registered_index;  // Uses zero initialization.
    GCInfoIndex index = registered_index.load(std::memory_order_acquire);
    if (V8_UNLIKELY(!index)) {
      EnsureGCInfoIndexTrait::EnsureIndex<T>(registered_index);
      // Slow path call uses V8_PRESERVE_MOST which does not support return
      // values (also preserves RAX). Avoid out parameter by just reloading the
      // value here which at this point is guaranteed to be set.
      index = registered_index.load(std::memory_order_acquire);
      CPPGC_DCHECK(index != 0);
    }
    return index;
  }
};

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_GC_INFO_H_
