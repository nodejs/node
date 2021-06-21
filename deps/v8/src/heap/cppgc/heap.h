// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_H_
#define V8_HEAP_CPPGC_HEAP_H_

#include "include/cppgc/heap.h"
#include "include/cppgc/liveness-broker.h"
#include "include/cppgc/macros.h"
#include "src/heap/cppgc/garbage-collector.h"
#include "src/heap/cppgc/gc-invoker.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-growing.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE Heap final : public HeapBase,
                                     public cppgc::Heap,
                                     public GarbageCollector {
 public:
  static Heap* From(cppgc::Heap* heap) { return static_cast<Heap*>(heap); }
  static const Heap* From(const cppgc::Heap* heap) {
    return static_cast<const Heap*>(heap);
  }

  Heap(std::shared_ptr<cppgc::Platform> platform,
       cppgc::Heap::HeapOptions options);
  ~Heap() final;

  HeapBase& AsBase() { return *this; }
  const HeapBase& AsBase() const { return *this; }

  void CollectGarbage(Config) final;
  void StartIncrementalGarbageCollection(Config) final;
  void FinalizeIncrementalGarbageCollectionIfRunning(Config);

  size_t epoch() const final { return epoch_; }

  void DisableHeapGrowingForTesting();

 private:
  void StartGarbageCollection(Config);
  void FinalizeGarbageCollection(Config::StackState);

  void FinalizeIncrementalGarbageCollectionIfNeeded(Config::StackState) final;

  void StartIncrementalGarbageCollectionForTesting() final;
  void FinalizeIncrementalGarbageCollectionForTesting(EmbedderStackState) final;

  Config config_;
  GCInvoker gc_invoker_;
  HeapGrowing growing_;

  const MarkingType marking_support_;
  const SweepingType sweeping_support_;

  size_t epoch_ = 0;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_H_
