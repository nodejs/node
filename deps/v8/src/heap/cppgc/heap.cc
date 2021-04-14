// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap.h"

#include "include/cppgc/heap-consistency.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc/garbage-collector.h"
#include "src/heap/cppgc/gc-invoker.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-verifier.h"
#include "src/heap/cppgc/prefinalizer-handler.h"
#include "src/heap/cppgc/stats-collector.h"

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
       MarkingType::kAtomic, SweepingType::kAtomic,
       internal::GarbageCollector::Config::IsForcedGC::kForced});
}

AllocationHandle& Heap::GetAllocationHandle() {
  return internal::Heap::From(this)->object_allocator();
}

HeapHandle& Heap::GetHeapHandle() { return *internal::Heap::From(this); }

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

void CheckConfig(Heap::Config config, Heap::MarkingType marking_support,
                 Heap::SweepingType sweeping_support) {
  CHECK_WITH_MSG(
      (config.collection_type != Heap::Config::CollectionType::kMinor) ||
          (config.stack_state == Heap::Config::StackState::kNoHeapPointers),
      "Minor GCs with stack is currently not supported");
  CHECK_LE(static_cast<int>(config.marking_type),
           static_cast<int>(marking_support));
  CHECK_LE(static_cast<int>(config.sweeping_type),
           static_cast<int>(sweeping_support));
}

}  // namespace

Heap::Heap(std::shared_ptr<cppgc::Platform> platform,
           cppgc::Heap::HeapOptions options)
    : HeapBase(platform, options.custom_spaces, options.stack_support,
               nullptr /* metric_recorder */),
      gc_invoker_(this, platform_.get(), options.stack_support),
      growing_(&gc_invoker_, stats_collector_.get(),
               options.resource_constraints, options.marking_support,
               options.sweeping_support),
      marking_support_(options.marking_support),
      sweeping_support_(options.sweeping_support) {
  CHECK_IMPLIES(options.marking_support != MarkingType::kAtomic,
                platform_->GetForegroundTaskRunner());
  CHECK_IMPLIES(options.sweeping_support != SweepingType::kAtomic,
                platform_->GetForegroundTaskRunner());
}

Heap::~Heap() {
  subtle::NoGarbageCollectionScope no_gc(*this);
  // Finish already running GC if any, but don't finalize live objects.
  sweeper_.FinishIfRunning();
}

void Heap::CollectGarbage(Config config) {
  DCHECK_EQ(Config::MarkingType::kAtomic, config.marking_type);
  CheckConfig(config, marking_support_, sweeping_support_);

  if (in_no_gc_scope()) return;

  config_ = config;

  if (!IsMarking()) {
    StartGarbageCollection(config);
  }
  DCHECK(IsMarking());
  FinalizeGarbageCollection(config.stack_state);
}

void Heap::StartIncrementalGarbageCollection(Config config) {
  DCHECK_NE(Config::MarkingType::kAtomic, config.marking_type);
  DCHECK_NE(marking_support_, MarkingType::kAtomic);
  CheckConfig(config, marking_support_, sweeping_support_);

  if (IsMarking() || in_no_gc_scope()) return;

  config_ = config;

  StartGarbageCollection(config);
}

void Heap::FinalizeIncrementalGarbageCollectionIfRunning(Config config) {
  DCHECK_NE(marking_support_, MarkingType::kAtomic);
  CheckConfig(config, marking_support_, sweeping_support_);

  if (!IsMarking()) return;

  DCHECK(!in_no_gc_scope());

  DCHECK_NE(Config::MarkingType::kAtomic, config_.marking_type);
  config_ = config;
  FinalizeGarbageCollection(config.stack_state);
}

void Heap::StartGarbageCollection(Config config) {
  DCHECK(!IsMarking());
  DCHECK(!in_no_gc_scope());

  // Finish sweeping in case it is still running.
  sweeper_.FinishIfRunning();

  epoch_++;

#if defined(CPPGC_YOUNG_GENERATION)
  if (config.collection_type == Config::CollectionType::kMajor)
    Unmarker unmarker(&raw_heap());
#endif

  const Marker::MarkingConfig marking_config{
      config.collection_type, config.stack_state, config.marking_type,
      config.is_forced_gc};
  marker_ = MarkerFactory::CreateAndStartMarking<Marker>(
      AsBase(), platform_.get(), marking_config);
}

void Heap::FinalizeGarbageCollection(Config::StackState stack_state) {
  DCHECK(IsMarking());
  DCHECK(!in_no_gc_scope());
  CHECK(!in_disallow_gc_scope());
  config_.stack_state = stack_state;
  if (override_stack_state_) {
    config_.stack_state = *override_stack_state_;
  }
  in_atomic_pause_ = true;
  {
    // This guards atomic pause marking, meaning that no internal method or
    // external callbacks are allowed to allocate new objects.
    cppgc::subtle::DisallowGarbageCollectionScope no_gc_scope(*this);
    marker_->FinishMarking(config_.stack_state);
  }
  marker_.reset();
  ExecutePreFinalizers();
  // TODO(chromium:1056170): replace build flag with dedicated flag.
#if DEBUG
  MarkingVerifier verifier(*this);
  verifier.Run(config_.stack_state);
#endif

  subtle::NoGarbageCollectionScope no_gc(*this);
  const Sweeper::SweepingConfig sweeping_config{
      config_.sweeping_type,
      Sweeper::SweepingConfig::CompactableSpaceHandling::kSweep};
  sweeper_.Start(sweeping_config);
  in_atomic_pause_ = false;
  sweeper_.NotifyDoneIfNeeded();
}

void Heap::DisableHeapGrowingForTesting() { growing_.DisableForTesting(); }

void Heap::FinalizeIncrementalGarbageCollectionIfNeeded(
    Config::StackState stack_state) {
  StatsCollector::EnabledScope stats_scope(
      stats_collector(), StatsCollector::kMarkIncrementalFinalize);
  FinalizeGarbageCollection(stack_state);
}

void Heap::StartIncrementalGarbageCollectionForTesting() {
  DCHECK(!IsMarking());
  DCHECK(!in_no_gc_scope());
  StartGarbageCollection({Config::CollectionType::kMajor,
                          Config::StackState::kNoHeapPointers,
                          Config::MarkingType::kIncrementalAndConcurrent,
                          Config::SweepingType::kIncrementalAndConcurrent});
}

void Heap::FinalizeIncrementalGarbageCollectionForTesting(
    EmbedderStackState stack_state) {
  DCHECK(!in_no_gc_scope());
  DCHECK(IsMarking());
  FinalizeGarbageCollection(stack_state);
  sweeper_.FinishIfRunning();
}

}  // namespace internal
}  // namespace cppgc
