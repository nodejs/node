// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap.h"

#include "src/heap/base/stack.h"
#include "src/heap/cppgc/garbage-collector.h"
#include "src/heap/cppgc/gc-invoker.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/prefinalizer-handler.h"

namespace cppgc {

namespace {

void VerifyCustomSpaces(
    const std::vector<std::unique_ptr<CustomSpaceBase>>& custom_spaces) {
  // Ensures that user-provided custom spaces have indices that form a sequence
  // starting at 0.
#ifdef DEBUG
  for (size_t i = 0; i < custom_spaces.size(); ++i) {
    DCHECK_EQ(i, custom_spaces[i]->GetCustomSpaceIndex().value);
  }
#endif  // DEBUG
}

}  // namespace

std::unique_ptr<Heap> Heap::Create(std::shared_ptr<cppgc::Platform> platform,
                                   cppgc::Heap::HeapOptions options) {
  DCHECK(platform.get());
  VerifyCustomSpaces(options.custom_spaces);
  return std::make_unique<internal::Heap>(std::move(platform),
                                          std::move(options));
}

void Heap::ForceGarbageCollectionSlow(const char* source, const char* reason,
                                      Heap::StackState stack_state) {
  internal::Heap::From(this)->CollectGarbage(
      {internal::GarbageCollector::Config::CollectionType::kMajor,
       stack_state});
}

AllocationHandle& Heap::GetAllocationHandle() {
  return internal::Heap::From(this)->object_allocator();
}

namespace internal {

namespace {

class Unmarker final : private HeapVisitor<Unmarker> {
  friend class HeapVisitor<Unmarker>;

 public:
  explicit Unmarker(RawHeap* heap) { Traverse(heap); }

 private:
  bool VisitHeapObjectHeader(HeapObjectHeader* header) {
    if (header->IsMarked()) header->Unmark();
    return true;
  }
};

void CheckConfig(Heap::Config config) {
  CHECK_WITH_MSG(
      (config.collection_type != Heap::Config::CollectionType::kMinor) ||
          (config.stack_state == Heap::Config::StackState::kNoHeapPointers),
      "Minor GCs with stack is currently not supported");
}

}  // namespace

Heap::Heap(std::shared_ptr<cppgc::Platform> platform,
           cppgc::Heap::HeapOptions options)
    : HeapBase(platform, options.custom_spaces.size()),
      gc_invoker_(this, platform_.get(), options.stack_support),
      growing_(&gc_invoker_, stats_collector_.get(),
               options.resource_constraints) {}

Heap::~Heap() {
  NoGCScope no_gc(*this);
  // Finish already running GC if any, but don't finalize live objects.
  sweeper_.Finish();
}

void Heap::CollectGarbage(Config config) {
  CheckConfig(config);

  if (in_no_gc_scope()) return;

  epoch_++;

#if defined(CPPGC_YOUNG_GENERATION)
  if (config.collection_type == Config::CollectionType::kMajor)
    Unmarker unmarker(&raw_heap());
#endif

  // "Marking".
  marker_ = std::make_unique<Marker>(AsBase());
  const Marker::MarkingConfig marking_config{
      config.collection_type, config.stack_state, config.marking_type};
  marker_->StartMarking(marking_config);
  marker_->FinishMarking(marking_config);
  // "Sweeping and finalization".
  {
    // Pre finalizers are forbidden from allocating objects.
    ObjectAllocator::NoAllocationScope no_allocation_scope_(object_allocator_);
    marker_->ProcessWeakness();
    prefinalizer_handler_->InvokePreFinalizers();
  }
  marker_.reset();
  // TODO(chromium:1056170): replace build flag with dedicated flag.
#if DEBUG
  VerifyMarking(config.stack_state);
#endif
  {
    NoGCScope no_gc(*this);
    sweeper_.Start(config.sweeping_type);
  }
}

}  // namespace internal
}  // namespace cppgc
