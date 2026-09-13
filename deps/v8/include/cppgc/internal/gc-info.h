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

namespace cppgc::internal {

using GCInfoIndex = uint16_t;
static constexpr GCInfoIndex kMaxGCInfoIndex = 1 << 14;
static constexpr GCInfoIndex kMinGCInfoIndex = 1;

struct GCInfo final {
  constexpr GCInfo(FinalizationCallback finalize, TraceCallback trace,
                   NameCallback name)
      : finalize(finalize), trace(trace), name(name) {}

  FinalizationCallback finalize;
  TraceCallback trace;
  NameCallback name;
  size_t padding = 0;
};

inline HeapObjectName GetHiddenName(
    const void*, HeapObjectNameForUnnamedObject name_retrieval_mode) {
  return {
      NameProvider::kHiddenName,
      name_retrieval_mode == HeapObjectNameForUnnamedObject::kUseHiddenName};
}

#if defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)

#if defined(__APPLE__)
// Use __attribute__((visibility("hidden"))) explicitly here, since otherwise
// the compiler will generate GOT-indirected loads. Although the linker emits
// these symbols with `protected` visibility (same as `default`, i.e. exported,
// but without GOT/PLT indirection), the compiler is not aware of that and
// assumes any extern "C" has the `default` visibility.
extern "C" __attribute__((visibility("hidden"))) const GCInfo
    __start_gc_info_section[] __asm("section$start$__DATA$gc_info_section");
extern "C" __attribute__((visibility("hidden"))) const GCInfo
    __stop_gc_info_section[] __asm("section$end$__DATA$gc_info_section");
#define CPPGC_GC_INFO_SECTION \
  __attribute__((section("__DATA,gc_info_section"), used))
#else  // defined(__APPLE__)
extern "C" __attribute__((visibility("hidden")))
const GCInfo __start_gc_info_section[];
extern "C" __attribute__((visibility("hidden")))
const GCInfo __stop_gc_info_section[];
#define CPPGC_GC_INFO_SECTION __attribute__((section("gc_info_section"), used))
#endif  // defined(__APPLE__)

class V8_EXPORT GCInfoTableSection final {
 public:
  GCInfoTableSection() = delete;

  V8_INLINE static GCInfoIndex Index(const GCInfo& info) {
    return &info - __start_gc_info_section + 1;
  }

  V8_INLINE static const GCInfo& GCInfoFromIndex(GCInfoIndex index) {
    CPPGC_DCHECK(index >= 1);
    CPPGC_DCHECK(
        static_cast<size_t>(index - 1) <
        static_cast<size_t>(__stop_gc_info_section - __start_gc_info_section));
    return __start_gc_info_section[index - 1];
  }
};

#else  // defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)

struct V8_EXPORT EnsureGCInfoIndexTrait final {
  // Acquires a new GC info object and updates `registered_index` with the index
  // that identifies that new info accordingly.
  template <typename T>
  V8_INLINE static GCInfoIndex EnsureIndex(
      std::atomic<GCInfoIndex>& registered_index) {
    return EnsureGCInfoIndexTraitDispatch<T>{}(registered_index);
  }

 private:
  template <typename T, bool = FinalizerTrait<T>::HasFinalizer(),
            bool = NameTrait<T>::HasNonHiddenName()>
  struct EnsureGCInfoIndexTraitDispatch;

  static GCInfoIndex V8_PRESERVE_MOST
  EnsureGCInfoIndex(std::atomic<GCInfoIndex>&, TraceCallback,
                    FinalizationCallback, NameCallback);
  static GCInfoIndex V8_PRESERVE_MOST EnsureGCInfoIndex(
      std::atomic<GCInfoIndex>&, TraceCallback, FinalizationCallback);
  static GCInfoIndex V8_PRESERVE_MOST
  EnsureGCInfoIndex(std::atomic<GCInfoIndex>&, TraceCallback, NameCallback);
  static GCInfoIndex V8_PRESERVE_MOST
  EnsureGCInfoIndex(std::atomic<GCInfoIndex>&, TraceCallback);
};

#define DISPATCH(has_finalizer, has_non_hidden_name, function)   \
  template <typename T>                                          \
  struct EnsureGCInfoIndexTrait::EnsureGCInfoIndexTraitDispatch< \
      T, has_finalizer, has_non_hidden_name> {                   \
    V8_INLINE GCInfoIndex                                        \
    operator()(std::atomic<GCInfoIndex>& registered_index) {     \
      return function;                                           \
    }                                                            \
  };

// ------------------------------------------------------- //
// DISPATCH(has_finalizer, has_non_hidden_name, function)  //
// ------------------------------------------------------- //
DISPATCH(true, true,                                       //
         EnsureGCInfoIndex(registered_index,               //
                           TraceTrait<T>::Trace,           //
                           FinalizerTrait<T>::kCallback,   //
                           NameTrait<T>::GetName))         //
DISPATCH(true, false,                                      //
         EnsureGCInfoIndex(registered_index,               //
                           TraceTrait<T>::Trace,           //
                           FinalizerTrait<T>::kCallback))  //
DISPATCH(false, true,                                      //
         EnsureGCInfoIndex(registered_index,               //
                           TraceTrait<T>::Trace,           //
                           NameTrait<T>::GetName))         //
DISPATCH(false, false,                                     //
         EnsureGCInfoIndex(registered_index,               //
                           TraceTrait<T>::Trace))          //

#undef DISPATCH

#endif  // defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)

// Trait determines how the garbage collector treats objects wrt. to traversing,
// finalization, and naming.
template <typename T>
struct GCInfoTrait final {
#if defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)
  static const GCInfo gc_info;
  V8_INLINE static GCInfoIndex Index() {
    static_assert(sizeof(T), "T must be fully defined");
    return GCInfoTableSection::Index(gc_info);
  }
#else   // defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)
  V8_INLINE static GCInfoIndex Index() {
    static_assert(sizeof(T), "T must be fully defined");
    static std::atomic<GCInfoIndex>
        registered_index;  // Uses zero initialization.
    GCInfoIndex index = registered_index.load(std::memory_order_acquire);
    if (V8_UNLIKELY(!index)) {
      index = EnsureGCInfoIndexTrait::EnsureIndex<T>(registered_index);
      CPPGC_DCHECK(index != 0);
      CPPGC_DCHECK(index == registered_index.load(std::memory_order_acquire));
    }
    return index;
  }
#endif  // defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)

  static constexpr void CheckCallbacksAreDefined() {
    // No USE() macro available.
    (void)static_cast<TraceCallback>(TraceTrait<T>::Trace);
    (void)static_cast<FinalizationCallback>(FinalizerTrait<T>::kCallback);
    (void)static_cast<NameCallback>(NameTrait<T>::GetName);
  }
};

// Fold types based on finalizer behavior. Note that finalizer characteristics
// align with trace behavior, i.e., destructors are virtual when trace methods
// are and vice versa.
template <typename T, typename ParentMostGarbageCollectedType>
struct GCInfoFolding final {
  static constexpr bool kHasVirtualDestructorAtBase =
      std::has_virtual_destructor_v<ParentMostGarbageCollectedType>;
  static constexpr bool kBothTypesAreTriviallyDestructible =
      std::is_trivially_destructible_v<ParentMostGarbageCollectedType> &&
      std::is_trivially_destructible_v<T>;
  static constexpr bool kHasCustomFinalizerDispatchAtBase =
      internal::HasFinalizeGarbageCollectedObject<
          ParentMostGarbageCollectedType>::value;
#ifdef CPPGC_SUPPORTS_OBJECT_NAMES
  static constexpr bool kWantsDetailedObjectNames = true;
#else   // !CPPGC_SUPPORTS_OBJECT_NAMES
  static constexpr bool kWantsDetailedObjectNames = false;
#endif  // !CPPGC_SUPPORTS_OBJECT_NAMES

  // Always true. Forces the compiler to resolve callbacks which ensures that
  // both modes don't break without requiring compiling a separate
  // configuration. Only a single GCInfo (for `ResultType` below) will actually
  // be instantiated but existence (and well-formedness) of all callbacks is
  // checked.
  static constexpr bool WantToFold() {
    if constexpr ((kHasVirtualDestructorAtBase ||
                   kBothTypesAreTriviallyDestructible ||
                   kHasCustomFinalizerDispatchAtBase) &&
                  !kWantsDetailedObjectNames) {
      GCInfoTrait<T>::CheckCallbacksAreDefined();
      GCInfoTrait<ParentMostGarbageCollectedType>::CheckCallbacksAreDefined();
      return true;
    }
    return false;
  }

  // Folding would regress name resolution when deriving names from C++
  // class names as it would just folds a name to the base class name.
  using ResultType =
      std::conditional_t<WantToFold(), ParentMostGarbageCollectedType, T>;
};

#if defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)
template <typename T>
const GCInfo GCInfoTrait<T>::gc_info CPPGC_GC_INFO_SECTION = GCInfo(
    FinalizerTrait<T>::kCallback, TraceTrait<T>::Trace,
    NameTrait<T>::HasNonHiddenName() ? NameTrait<T>::GetName : GetHiddenName);
#endif  // defined(CPPGC_ENABLE_OBJECT_SECTION_GCINFO)

}  // namespace cppgc::internal

#endif  // INCLUDE_CPPGC_INTERNAL_GC_INFO_H_
