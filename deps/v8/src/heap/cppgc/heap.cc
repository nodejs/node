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
#include "src/heap/cppgc/marking-verifier.h"
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
      {internal::GarbageCollector::Config::CollectionType::kMajor, stack_state,
       internal::GarbageCollector::Config::MarkingType::kAtomic,
       internal::GarbageCollector::Config::SweepingType::kAtomic});
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
    : HeapBase(platform, options.custom_spaces, options.stack_support),
      gc_invoker_(this, platform_.get(), options.stack_support),
      growing_(&gc_invoker_, stats_collector_.get(),
               options.resource_constraints) {}

Heap::~Heap() {
  NoGCScope no_gc(*this);
  // Finish already running GC if any, but don't finalize live objects.
  sweeper_.FinishIfRunning();
}

void Heap::CollectGarbage(Config config) {
  DCHECK_EQ(Config::MarkingType::kAtomic, config.marking_type);
  CheckConfig(config);

  if (in_no_gc_scope()) return;

  config_ = config;

  if (!gc_in_progress_) StartGarbageCollection(config);

  DCHECK(marker_);

  FinalizeGarbageCollection(config.stack_state);
}

void Heap::StartIncrementalGarbageCollection(Config config) {
  DCHECK_NE(Config::MarkingType::kAtomic, config.marking_type);
  CheckConfig(config);

  if (gc_in_progress_ || in_no_gc_scope()) return;

  config_ = config;

  StartGarbageCollection(config);
}

void Heap::FinalizeIncrementalGarbageCollectionIfRunning(Config config) {
  if (!gc_in_progress_) return;

  DCHECK(!in_no_gc_scope());

  DCHECK_NE(Config::MarkingType::kAtomic, config_.marking_type);
  config_ = config;
  FinalizeGarbageCollection(config.stack_state);
}

void Heap::StartGarbageCollection(Config config) {
  DCHECK(!gc_in_progress_);

  DCHECK(!in_no_gc_scope());

  // Finish sweeping in case it is still running.
  sweeper_.FinishIfRunning();

  gc_in_progress_ = true;
  epoch_++;

#if defined(CPPGC_YOUNG_GENERATION)
  if (config.collection_type == Config::CollectionType::kMajor)
    Unmarker unmarker(&raw_heap());
#endif

  const Marker::MarkingConfig marking_config{
      config.collection_type, config.stack_state, config.marking_type};
  marker_ = MarkerFactory::CreateAndStartMarking<Marker>(
      AsBase(), platform_.get(), marking_config);
}

void Heap::FinalizeGarbageCollection(Config::StackState stack_state) {
  DCHECK(gc_in_progress_);
  DCHECK(!in_no_gc_scope());
  config_.stack_state = stack_state;
  DCHECK(marker_);
  {
    // Pre finalizers are forbidden from allocating objects. Note that this also
    // guard atomic pause marking below, meaning that no internal method or
    // external callbacks are allowed to allocate new objects.
    ObjectAllocator::NoAllocationScope no_allocation_scope_(object_allocator_);
    marker_->FinishMarking(stack_state);
    prefinalizer_handler_->InvokePreFinalizers();
  }
  marker_.reset();
  // TODO(chromium:1056170): replace build flag with dedicated flag.
#if DEBUG
  MarkingVerifier verifier(*this);
  verifier.Run(stack_state);
#endif
  {
    NoGCScope no_gc(*this);
    const Sweeper::SweepingConfig sweeping_config{
        config_.sweeping_type,
        Sweeper::SweepingConfig::CompactableSpaceHandling::kSweep};
    sweeper_.Start(sweeping_config);
  }
  gc_in_progress_ = false;
}

void Heap::DisableHeapGrowingForTesting() { growing_.DisableForTesting(); }

}  // namespace internal
}  // namespace cppgc
