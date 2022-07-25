// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/process-heap.h"

#include <algorithm>
#include <vector>

#include "src/base/lazy-instance.h"
#include "src/base/logging.h"
#include "src/base/platform/mutex.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/page-memory.h"

namespace cppgc {
namespace internal {

v8::base::LazyMutex g_process_mutex = LAZY_MUTEX_INITIALIZER;

namespace {

v8::base::LazyMutex g_heap_registry_mutex = LAZY_MUTEX_INITIALIZER;

HeapRegistry::Storage& GetHeapRegistryStorage() {
  static v8::base::LazyInstance<HeapRegistry::Storage>::type heap_registry =
      LAZY_INSTANCE_INITIALIZER;
  return *heap_registry.Pointer();
}

}  // namespace

// static
void HeapRegistry::RegisterHeap(HeapBase& heap) {
  v8::base::MutexGuard guard(g_heap_registry_mutex.Pointer());

  auto& storage = GetHeapRegistryStorage();
  DCHECK_EQ(storage.end(), std::find(storage.begin(), storage.end(), &heap));
  storage.push_back(&heap);
}

// static
void HeapRegistry::UnregisterHeap(HeapBase& heap) {
  v8::base::MutexGuard guard(g_heap_registry_mutex.Pointer());

  // HeapRegistry requires access to PageBackend which means it must still
  // be present by the time a heap is removed from the registry.
  DCHECK_NOT_NULL(heap.page_backend());

  auto& storage = GetHeapRegistryStorage();
  const auto pos = std::find(storage.begin(), storage.end(), &heap);
  DCHECK_NE(storage.end(), pos);
  storage.erase(pos);
}

// static
HeapBase* HeapRegistry::TryFromManagedPointer(const void* needle) {
  v8::base::MutexGuard guard(g_heap_registry_mutex.Pointer());

  for (auto* heap : GetHeapRegistryStorage()) {
    const auto address =
        heap->page_backend()->Lookup(reinterpret_cast<ConstAddress>(needle));
    if (address) return heap;
  }
  return nullptr;
}

// static
const HeapRegistry::Storage& HeapRegistry::GetRegisteredHeapsForTesting() {
  return GetHeapRegistryStorage();
}

}  // namespace internal
}  // namespace cppgc
