// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_PROCESS_HEAP_H_
#define V8_HEAP_CPPGC_PROCESS_HEAP_H_

#include <vector>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"

namespace cppgc {
namespace internal {

class HeapBase;

extern v8::base::LazyMutex g_process_mutex;

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

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_PROCESS_HEAP_H_
