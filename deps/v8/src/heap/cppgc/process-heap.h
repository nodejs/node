// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PROCESS_HEAP_H_
#define V8_HEAP_CPPGC_PROCESS_HEAP_H_

#include <vector>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/heap/cppgc/trace-event.h"

namespace cppgc::internal {

class HeapBase;

class ProcessGlobalLock final {
 public:
  enum class Reason {
    kForGC,
    kForCrossThreadHandleCreation,
  };

  template <Reason reason>
  V8_INLINE static void Lock() {
    process_mutex_.Pointer()->Lock();

#if defined(V8_USE_PERFETTO)
    switch (reason) {
      case Reason::kForGC:
        TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("cppgc"), "AcquiredForGC",
                          perfetto::NamedTrack("CppGC.ProcessGlobalLock"));
        break;
      case Reason::kForCrossThreadHandleCreation:
#ifdef DEBUG
        TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("cppgc"),
                          "AcquiredForCrossThreadHandleCreation",
                          perfetto::NamedTrack("CppGC.ProcessGlobalLock"));
#endif  // DEBUG
        break;
    }
#endif  // defined(V8_USE_PERFETTO)
  }

  template <Reason reason>
  V8_INLINE static void Unlock() {
#if defined(V8_USE_PERFETTO)
    switch (reason) {
      case Reason::kForGC:
        TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("cppgc"),
                        perfetto::NamedTrack("CppGC.ProcessGlobalLock"));
        break;
      case Reason::kForCrossThreadHandleCreation:
#ifdef DEBUG
        TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("cppgc"),
                        perfetto::NamedTrack("CppGC.ProcessGlobalLock"));
#endif  // DEBUG
        break;
    }
#endif  // defined(V8_USE_PERFETTO)

    process_mutex_.Pointer()->Unlock();
  }

  V8_INLINE static void AssertHeld() { process_mutex_.Pointer()->AssertHeld(); }

 private:
  static v8::base::LazyMutex process_mutex_;
};

class V8_EXPORT_PRIVATE HeapRegistry final {
 public:
  using Storage = std::vector<HeapBase*>;

  class Subscription final {
   public:
    inline explicit Subscription(HeapBase&);
    inline ~Subscription();

   private:
    HeapBase& heap_;
  };

  static HeapBase* TryFromManagedPointer(const void* needle);

  // Does not take the registry mutex and is thus only useful for testing.
  static const Storage& GetRegisteredHeapsForTesting();

 private:
  static void RegisterHeap(HeapBase&);
  static void UnregisterHeap(HeapBase&);
};

HeapRegistry::Subscription::Subscription(HeapBase& heap) : heap_(heap) {
  HeapRegistry::RegisterHeap(heap_);
}

HeapRegistry::Subscription::~Subscription() {
  HeapRegistry::UnregisterHeap(heap_);
}

}  // namespace cppgc::internal

#endif  // V8_HEAP_CPPGC_PROCESS_HEAP_H_
