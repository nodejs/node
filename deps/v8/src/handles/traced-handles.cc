// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/traced-handles.h"

#include <limits>

#include "include/v8-embedder-heap.h"
#include "include/v8-internal.h"
#include "include/v8-traced-handle.h"
#include "src/base/logging.h"
#include "src/base/platform/memory.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/handles/traced-handles-inl.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"

namespace v8::internal {

class TracedNodeBlock;

TracedNode::TracedNode(IndexType index, IndexType next_free_index)
    : next_free_index_(next_free_index), index_(index) {
  // TracedNode size should stay within 2 words.
  static_assert(sizeof(TracedNode) <= (2 * kSystemPointerSize));
  DCHECK(!is_in_use());
  DCHECK(!is_in_young_list());
  DCHECK(!is_weak());
  DCHECK(!markbit());
  DCHECK(!has_old_host());
  DCHECK(!is_droppable());
}

void TracedNode::Release(Address zap_value) {
  DCHECK(is_in_use());
  // Clear all flags.
  flags_ = 0;
  clear_markbit();
  set_raw_object(zap_value);
  DCHECK(IsMetadataCleared());
}

// static
TracedNodeBlock* TracedNodeBlock::Create(TracedHandles& traced_handles) {
  static_assert(alignof(TracedNodeBlock) >= alignof(TracedNode));
  static_assert(sizeof(TracedNodeBlock) % alignof(TracedNode) == 0,
                "TracedNodeBlock size is used to auto-align node FAM storage.");
  const size_t min_wanted_size =
      sizeof(TracedNodeBlock) +
      sizeof(TracedNode) * TracedNodeBlock::kMinCapacity;
  const auto raw_result = v8::base::AllocateAtLeast<char>(min_wanted_size);
  const size_t capacity = std::min(
      (raw_result.count - sizeof(TracedNodeBlock)) / sizeof(TracedNode),
      kMaxCapacity);
  CHECK_LT(capacity, std::numeric_limits<TracedNode::IndexType>::max());
  const auto result = std::make_pair(raw_result.ptr, capacity);
  return new (result.first) TracedNodeBlock(
      traced_handles, static_cast<TracedNode::IndexType>(result.second));
}

// static
void TracedNodeBlock::Delete(TracedNodeBlock* block) { free(block); }

TracedNodeBlock::TracedNodeBlock(TracedHandles& traced_handles,
                                 TracedNode::IndexType capacity)
    : traced_handles_(traced_handles), capacity_(capacity) {
  for (TracedNode::IndexType i = 0; i < (capacity_ - 1); i++) {
    new (at(i)) TracedNode(i, i + 1);
  }
  new (at(capacity_ - 1)) TracedNode(capacity_ - 1, kInvalidFreeListNodeIndex);
}

// static
TracedNodeBlock& TracedNodeBlock::From(TracedNode& node) {
  TracedNode* first_node = &node - node.index();
  return *reinterpret_cast<TracedNodeBlock*>(
      reinterpret_cast<uintptr_t>(first_node) - sizeof(TracedNodeBlock));
}

// static
const TracedNodeBlock& TracedNodeBlock::From(const TracedNode& node) {
  return From(const_cast<TracedNode&>(node));
}

void TracedNodeBlock::FreeNode(TracedNode* node, Address zap_value) {
  DCHECK(node->is_in_use());
  node->Release(zap_value);
  DCHECK(!node->is_in_use());
  node->set_next_free(first_free_node_);
  first_free_node_ = node->index();
  used_--;
}

void SetSlotThreadSafe(Address** slot, Address* val) {
  reinterpret_cast<std::atomic<Address*>*>(slot)->store(
      val, std::memory_order_relaxed);
}

void TracedHandles::RefillUsableNodeBlocks() {
  TracedNodeBlock* block;
  if (empty_blocks_.empty()) {
    block = TracedNodeBlock::Create(*this);
    block_size_bytes_ += block->size_bytes();
  } else {
    block = empty_blocks_.back();
    empty_blocks_.pop_back();
  }
  usable_blocks_.PushFront(block);
  blocks_.PushFront(block);
  num_blocks_++;
  DCHECK(!block->InYoungList());
  DCHECK(block->IsEmpty());
  DCHECK_EQ(usable_blocks_.Front(), block);
  DCHECK(!usable_blocks_.empty());
}

void TracedHandles::FreeNode(TracedNode* node, Address zap_value) {
  auto& block = TracedNodeBlock::From(*node);
  if (disable_block_handling_on_free_) {
    // The list of blocks and used nodes will be updated separately.
    block.FreeNode(node, zap_value);
    return;
  }
  if (V8_UNLIKELY(block.IsFull())) {
    DCHECK(!usable_blocks_.ContainsSlow(&block));
    usable_blocks_.PushFront(&block);
  }
  block.FreeNode(node, zap_value);
  if (block.IsEmpty()) {
    usable_blocks_.Remove(&block);
    blocks_.Remove(&block);
    if (block.InYoungList()) {
      young_blocks_.Remove(&block);
      DCHECK(!block.InYoungList());
      num_young_blocks_--;
    }
    num_blocks_--;
    empty_blocks_.push_back(&block);
  }
  used_nodes_--;
}

TracedHandles::TracedHandles(Isolate* isolate) : isolate_(isolate) {}

TracedHandles::~TracedHandles() {
  size_t block_size_bytes = 0;
  while (!blocks_.empty()) {
    auto* block = blocks_.Front();
    blocks_.PopFront();
    block_size_bytes += block->size_bytes();
    TracedNodeBlock::Delete(block);
  }
  for (auto* block : empty_blocks_) {
    block_size_bytes += block->size_bytes();
    TracedNodeBlock::Delete(block);
  }
  USE(block_size_bytes);
  DCHECK_EQ(block_size_bytes, block_size_bytes_);
}

void TracedHandles::Destroy(TracedNodeBlock& node_block, TracedNode& node) {
  DCHECK_IMPLIES(is_marking_, !is_sweeping_on_mutator_thread_);
  DCHECK_IMPLIES(is_sweeping_on_mutator_thread_, !is_marking_);

  // If sweeping on the mutator thread is running then the handle destruction
  // may be a result of a Reset() call from a destructor. The node will be
  // reclaimed on the next cycle.
  //
  // This allows v8::TracedReference::Reset() calls from destructors on
  // objects that may be used from stack and heap.
  if (is_sweeping_on_mutator_thread_) {
    return;
  }

  if (is_marking_) {
    // Incremental/concurrent marking is running.
    //
    // On-heap traced nodes are released in the atomic pause in
    // `ResetDeadNodes()` when they are discovered as not marked. Eagerly clear
    // out the object here to avoid needlessly marking it from this point on.
    // The node will be reclaimed on the next cycle.
    node.set_raw_object<AccessMode::ATOMIC>(kNullAddress);
    return;
  }

  // In case marking and sweeping are off, the handle may be freed immediately.
  // Note that this includes also the case when invoking the first pass
  // callbacks during the atomic pause which requires releasing a node fully.
  FreeNode(&node, kTracedHandleEagerResetZapValue);
}

void TracedHandles::Copy(const TracedNode& from_node, Address** to) {
  DCHECK_NE(kGlobalHandleZapValue, from_node.raw_object());
  FullObjectSlot o =
      Create(from_node.raw_object(), reinterpret_cast<Address*>(to),
             TracedReferenceStoreMode::kAssigningStore,
             TracedReferenceHandling::kDefault);
  SetSlotThreadSafe(to, o.location());
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) {
    Object::ObjectVerify(Tagged<Object>(**to), isolate_);
  }
#endif  // VERIFY_HEAP
}

void TracedHandles::Move(TracedNode& from_node, Address** from, Address** to) {
  DCHECK(from_node.is_in_use());

  // Deal with old "to".
  auto* to_node = TracedNode::FromLocation(*to);
  DCHECK_IMPLIES(*to, to_node->is_in_use());
  DCHECK_IMPLIES(*to, kGlobalHandleZapValue != to_node->raw_object());
  DCHECK_NE(kGlobalHandleZapValue, from_node.raw_object());
  if (*to) {
    auto& to_node_block = TracedNodeBlock::From(*to_node);
    Destroy(to_node_block, *to_node);
  }

  // Set "to" to "from".
  SetSlotThreadSafe(to, *from);
  to_node = &from_node;

  // Deal with new "to"
  DCHECK_NOT_NULL(*to);
  DCHECK_EQ(*from, *to);
  if (is_marking_) {
    // Write barrier needs to cover node as well as object.
    to_node->set_markbit();
    WriteBarrier::MarkingFromTracedHandle(to_node->object());
  } else if (auto* cpp_heap = GetCppHeapIfUnifiedYoungGC(isolate_)) {
    const bool object_is_young_and_not_yet_recorded =
        !from_node.has_old_host() &&
        HeapLayout::InYoungGeneration(from_node.object());
    if (object_is_young_and_not_yet_recorded &&
        IsCppGCHostOld(*cpp_heap, reinterpret_cast<Address>(to))) {
      DCHECK(from_node.is_in_young_list());
      from_node.set_has_old_host(true);
    }
  }
  SetSlotThreadSafe(from, nullptr);
}

void TracedHandles::SetIsMarking(bool value) {
  DCHECK_EQ(is_marking_, !value);
  is_marking_ = value;
}

void TracedHandles::SetIsSweepingOnMutatorThread(bool value) {
  DCHECK_EQ(is_sweeping_on_mutator_thread_, !value);
  is_sweeping_on_mutator_thread_ = value;
}

const TracedHandles::NodeBounds TracedHandles::GetNodeBounds() const {
  TracedHandles::NodeBounds block_bounds;
  block_bounds.reserve(num_blocks_);
  for (const auto* block : blocks_) {
    block_bounds.push_back(
        {block->nodes_begin_address(), block->nodes_end_address()});
  }
  std::sort(block_bounds.begin(), block_bounds.end(),
            [](const auto& pair1, const auto& pair2) {
              return pair1.first < pair2.first;
            });
  return block_bounds;
}

void TracedHandles::UpdateListOfYoungNodes() {
  const bool needs_to_mark_as_old =
      static_cast<bool>(GetCppHeapIfUnifiedYoungGC(isolate_));

  for (auto it = young_blocks_.begin(); it != young_blocks_.end();) {
    bool contains_young_node = false;
    TracedNodeBlock* const block = *it;
    DCHECK(block->InYoungList());

    for (auto* node : *block) {
      if (!node->is_in_young_list()) continue;
      DCHECK(node->is_in_use());
      if (HeapLayout::InYoungGeneration(node->object())) {
        contains_young_node = true;
        // The node was discovered through a cppgc object, which will be
        // immediately promoted. Remember the object.
        if (needs_to_mark_as_old) node->set_has_old_host(true);
      } else {
        node->set_is_in_young_list(false);
        node->set_has_old_host(false);
      }
    }
    if (contains_young_node) {
      ++it;
    } else {
      it = young_blocks_.RemoveAt(it);
      DCHECK(!block->InYoungList());
      num_young_blocks_--;
    }
  }
}

void TracedHandles::DeleteEmptyBlocks() {
  // Keep one node block around for fast allocation/deallocation patterns.
  if (empty_blocks_.size() <= 1) return;

  for (size_t i = 1; i < empty_blocks_.size(); i++) {
    auto* block = empty_blocks_[i];
    DCHECK(block->IsEmpty());
    DCHECK_GE(block_size_bytes_, block->size_bytes());
    block_size_bytes_ -= block->size_bytes();
    TracedNodeBlock::Delete(block);
  }
  empty_blocks_.resize(1);
  empty_blocks_.shrink_to_fit();
}

void TracedHandles::ResetDeadNodes(
    WeakSlotCallbackWithHeap should_reset_handle) {
  // Manual iteration as the block may be deleted in `FreeNode()`.
  for (auto it = blocks_.begin(); it != blocks_.end();) {
    auto* block = *(it++);
    for (auto* node : *block) {
      if (!node->is_in_use()) continue;

      // Detect unreachable nodes first.
      if (!node->markbit()) {
        FreeNode(node, kTracedHandleFullGCResetZapValue);
        continue;
      }

      // Node was reachable. Clear the markbit for the next GC.
      node->clear_markbit();
      // TODO(v8:13141): Turn into a DCHECK after some time.
      CHECK(!should_reset_handle(isolate_->heap(), node->location()));
    }

    if (block->InYoungList()) {
      young_blocks_.Remove(block);
      DCHECK(!block->InYoungList());
      num_young_blocks_--;
    }
  }

  CHECK(young_blocks_.empty());
}

void TracedHandles::ResetYoungDeadNodes(
    WeakSlotCallbackWithHeap should_reset_handle) {
  for (auto* block : young_blocks_) {
    for (auto* node : *block) {
      if (!node->is_in_young_list()) continue;
      DCHECK(node->is_in_use());
      DCHECK_IMPLIES(node->has_old_host(), node->markbit());

      if (!node->markbit()) {
        FreeNode(node, kTracedHandleMinorGCResetZapValue);
        continue;
      }

      // Node was reachable. Clear the markbit for the next GC.
      node->clear_markbit();
      // TODO(v8:13141): Turn into a DCHECK after some time.
      CHECK(!should_reset_handle(isolate_->heap(), node->location()));
    }
  }
}

bool TracedHandles::SupportsClearingWeakNonLiveWrappers() {
  DCHECK(!is_marking_);
  if (!v8_flags.reclaim_unmodified_wrappers) {
    return false;
  }
  if (!isolate_->heap()->GetEmbedderRootsHandler()) {
    return false;
  }
  return true;
}

namespace {

template <typename Derived>
class ParallelWeakHandlesProcessor {
 public:
  class Job : public v8::JobTask {
   public:
    explicit Job(Derived& derived) : derived_(derived) {}

    void Run(JobDelegate* delegate) override {
      if (delegate->IsJoiningThread()) {
        TRACE_GC_WITH_FLOW(derived_.heap()->tracer(), Derived::kMainThreadScope,
                           derived_.trace_id_, TRACE_EVENT_FLAG_FLOW_IN);
        RunImpl</*IsMainThread=*/true>(delegate);
      } else {
        TRACE_GC_EPOCH_WITH_FLOW(derived_.heap()->tracer(),
                                 Derived::kBackgroundThreadScope,
                                 ThreadKind::kBackground, derived_.trace_id_,
                                 TRACE_EVENT_FLAG_FLOW_IN);
        RunImpl</*IsMainThread=*/false>(delegate);
      }
    }

    size_t GetMaxConcurrency(size_t worker_count) const override {
      const auto processed_young_blocks =
          derived_.processed_young_blocks_.load(std::memory_order_relaxed);
      if (derived_.num_young_blocks_ < processed_young_blocks) {
        return 0;
      }
      if (!v8_flags.parallel_reclaim_unmodified_wrappers) {
        return 1;
      }
      const auto blocks_left =
          derived_.num_young_blocks_ - processed_young_blocks;
      constexpr size_t kMaxParallelTasks = 3;
      constexpr size_t kBlocksPerTask = 8;
      const auto wanted_tasks =
          (blocks_left + (kBlocksPerTask - 1)) / kBlocksPerTask;
      return std::min(kMaxParallelTasks, wanted_tasks);
    }

   private:
    template <bool IsMainThread>
    void RunImpl(JobDelegate* delegate) {
      // The following logic parallelizes the handling of the doubly-linked
      // list. We basically race through the list from begin() with acquiring
      // exclusive access by incrementing a single counter.
      auto it = derived_.young_blocks_.begin();
      size_t current = 0;
      for (size_t index = derived_.processed_young_blocks_.fetch_add(
               1, std::memory_order_relaxed);
           index < derived_.num_young_blocks_;
           index = derived_.processed_young_blocks_.fetch_add(
               +1, std::memory_order_relaxed)) {
        while (current < index) {
          it++;
          current++;
        }
        TracedNodeBlock* block = *it;
        DCHECK(block->InYoungList());
        derived_.template ProcessBlock<IsMainThread>(block);
        // TracedNodeBlock is the minimum granularity of processing.
        if (delegate->ShouldYield()) {
          return;
        }
      }
    }

    Derived& derived_;
  };

  ParallelWeakHandlesProcessor(Heap* heap,
                               TracedNodeBlock::YoungList& young_blocks,
                               size_t num_young_blocks)
      : heap_(heap),
        young_blocks_(young_blocks),
        num_young_blocks_(num_young_blocks),
        trace_id_(reinterpret_cast<uint64_t>(this) ^
                  heap_->tracer()->CurrentEpoch(
                      GCTracer::Scope::SCAVENGER_SCAVENGE)) {}

  void Run() {
    TRACE_GC_NOTE_WITH_FLOW(Derived::kStartNote, trace_id(),
                            TRACE_EVENT_FLAG_FLOW_OUT);
    V8::GetCurrentPlatform()
        ->CreateJob(v8::TaskPriority::kUserBlocking,
                    std::make_unique<Job>(static_cast<Derived&>(*this)))
        ->Join();
  }

  Heap* heap() const { return heap_; }
  uint64_t trace_id() const { return trace_id_; }

 private:
  Heap* heap_;
  TracedNodeBlock::YoungList& young_blocks_;
  const size_t num_young_blocks_;
  const uint64_t trace_id_;
  std::atomic<size_t> processed_young_blocks_{0};
};

class ComputeWeaknessProcessor final
    : public ParallelWeakHandlesProcessor<ComputeWeaknessProcessor> {
 public:
  static constexpr auto kMainThreadScope =
      GCTracer::Scope::SCAVENGER_TRACED_HANDLES_COMPUTE_WEAKNESS_PARALLEL;
  static constexpr auto kBackgroundThreadScope = GCTracer::Scope::
      SCAVENGER_BACKGROUND_TRACED_HANDLES_COMPUTE_WEAKNESS_PARALLEL;
  static constexpr char kStartNote[] = "ComputeWeaknessProcessor start";

  ComputeWeaknessProcessor(Heap* heap, TracedNodeBlock::YoungList& young_blocks,
                           size_t num_young_blocks)
      : ParallelWeakHandlesProcessor(heap, young_blocks, num_young_blocks) {}

  template <bool IsMainThread>
  void ProcessBlock(TracedNodeBlock* block) {
    for (TracedNode* node : *block) {
      if (!node->is_in_young_list()) {
        continue;
      }
      DCHECK(node->is_in_use());
      DCHECK(!node->is_weak());
      if (node->is_droppable() &&
          JSObject::IsUnmodifiedApiObject(node->location())) {
        node->set_weak(true);
      }
    }
  }
};

}  // namespace

void TracedHandles::ComputeWeaknessForYoungObjects() {
  if (!SupportsClearingWeakNonLiveWrappers()) {
    return;
  }
  ComputeWeaknessProcessor job(isolate_->heap(), young_blocks_,
                               num_young_blocks_);
  job.Run();
}

namespace {

class ClearWeaknessProcessor final
    : public ParallelWeakHandlesProcessor<ClearWeaknessProcessor> {
 public:
  static constexpr auto kMainThreadScope =
      GCTracer::Scope::SCAVENGER_TRACED_HANDLES_RESET_PARALLEL;
  static constexpr auto kBackgroundThreadScope =
      GCTracer::Scope::SCAVENGER_BACKGROUND_TRACED_HANDLES_RESET_PARALLEL;
  static constexpr char kStartNote[] = "ClearWeaknessProcessor start";

  ClearWeaknessProcessor(TracedNodeBlock::YoungList& young_blocks,
                         size_t num_young_blocks, Heap* heap,
                         RootVisitor* visitor,
                         WeakSlotCallbackWithHeap should_reset_handle)
      : ParallelWeakHandlesProcessor(heap, young_blocks, num_young_blocks),
        visitor_(visitor),
        handler_(heap->GetEmbedderRootsHandler()),
        should_reset_handle_(should_reset_handle) {}

  template <bool IsMainThread>
  void ProcessBlock(TracedNodeBlock* block) {
    const auto saved_used_nodes_in_block = block->used();
    for (TracedNode* node : *block) {
      if (!node->is_weak()) {
        continue;
      }
      DCHECK(node->is_in_use());
      DCHECK(node->is_in_young_list());

      const bool should_reset = should_reset_handle_(heap(), node->location());
      if (should_reset) {
        FullObjectSlot slot = node->location();
        bool node_cleared = true;
        if constexpr (IsMainThread) {
          handler_->ResetRoot(
              *reinterpret_cast<v8::TracedReference<v8::Value>*>(&slot));
        } else {
          node_cleared = handler_->TryResetRoot(
              *reinterpret_cast<v8::TracedReference<v8::Value>*>(&slot));
        }
        if (node_cleared) {
          // Mark as cleared due to weak semantics.
          node->set_raw_object(kTracedHandleMinorGCWeakResetZapValue);
          DCHECK(!node->is_in_use());
          DCHECK(!node->is_weak());
        } else {
          block->SetReprocessing(true);
        }
      } else {
        node->set_weak(false);
        if (visitor_) {
          visitor_->VisitRootPointer(Root::kTracedHandles, nullptr,
                                     node->location());
        }
      }
    }
    DCHECK_GE(saved_used_nodes_in_block, block->used());
    block->SetLocallyFreed(saved_used_nodes_in_block - block->used());
  }

 private:
  RootVisitor* visitor_;
  EmbedderRootsHandler* handler_;
  WeakSlotCallbackWithHeap should_reset_handle_;
};

}  // namespace

void TracedHandles::ProcessWeakYoungObjects(
    RootVisitor* visitor, WeakSlotCallbackWithHeap should_reset_handle) {
  if (!SupportsClearingWeakNonLiveWrappers()) {
    return;
  }

  auto* heap = isolate_->heap();
  // ResetRoot() below should not trigger allocations in CppGC.
  if (auto* cpp_heap = CppHeap::From(heap->cpp_heap())) {
    cpp_heap->EnterDisallowGCScope();
    cpp_heap->EnterNoGCScope();
  }

#ifdef DEBUG
  size_t num_young_blocks = 0;
  for (auto it = young_blocks_.begin(); it != young_blocks_.end(); it++) {
    TracedNodeBlock* block = *it;
    DCHECK(block->InYoungList());
    DCHECK(!block->NeedsReprocessing());
    num_young_blocks++;
  }
  DCHECK_EQ(num_young_blocks_, num_young_blocks);
#endif

  disable_block_handling_on_free_ = true;
  ClearWeaknessProcessor job(young_blocks_, num_young_blocks_, heap, visitor,
                             should_reset_handle);
  job.Run();
  disable_block_handling_on_free_ = false;

  // Post processing on block level.
  for (auto it = young_blocks_.begin(); it != young_blocks_.end();) {
    TracedNodeBlock* block = *it;
    // Avoid iterator invalidation by incrementing iterator here before a block
    // is possible removed below.
    it++;
    DCHECK(block->InYoungList());

    // Freeing a node will not make the block fuller, so IsFull() should mean
    // that the block was already not usable before freeing.
    CHECK_IMPLIES(block->IsFull(), !usable_blocks_.Contains(block));
    if (!block->IsFull() && !block->IsEmpty()) {
      // A block is usable but may have been full before. Check if we need to
      // add it to the usable blocks.
      if (!usable_blocks_.Contains(block)) {
        DCHECK(!block->InUsableList());
        usable_blocks_.PushFront(block);
        DCHECK(block->InUsableList());
      }
    } else if (block->IsEmpty()) {
      // A non-empty block got empty during freeing. The block must not require
      // reprocessing which would mean that at least one node was not yet freed.
      DCHECK(!block->NeedsReprocessing());
      if (usable_blocks_.Contains(block)) {
        DCHECK(block->InUsableList());
        usable_blocks_.Remove(block);
        DCHECK(!block->InUsableList());
      }
      blocks_.Remove(block);
      DCHECK(block->InYoungList());
      young_blocks_.Remove(block);
      DCHECK(!block->InYoungList());
      num_young_blocks_--;
      empty_blocks_.push_back(block);
      num_blocks_--;
    }

    used_nodes_ -= block->ConsumeLocallyFreed();

    // Handle reprocessing of blocks because `TryReset()` was not able to reset
    // a node concurrently.
    if (!block->NeedsReprocessing()) {
      continue;
    }
    block->SetReprocessing(false);
    job.template ProcessBlock</*IsMainThread=*/true>(block);
    DCHECK(!block->NeedsReprocessing());
    // The nodes are fully freed and accounted but still reported as locally
    // freed as we reuse the processor.
    const auto locally_freed = block->ConsumeLocallyFreed();
    (void)locally_freed;
    DCHECK_GT(locally_freed, 0);
  }

  if (auto* cpp_heap = CppHeap::From(isolate_->heap()->cpp_heap())) {
    cpp_heap->LeaveNoGCScope();
    cpp_heap->LeaveDisallowGCScope();
  }
}

void TracedHandles::Iterate(RootVisitor* visitor) {
  for (auto* block : blocks_) {
    for (auto* node : *block) {
      if (!node->is_in_use()) continue;

      visitor->VisitRootPointer(Root::kTracedHandles, nullptr,
                                node->location());
    }
  }
}

void TracedHandles::IterateYoung(RootVisitor* visitor) {
  for (auto* block : young_blocks_) {
    for (auto* node : *block) {
      if (!node->is_in_young_list()) continue;
      DCHECK(node->is_in_use());
      visitor->VisitRootPointer(Root::kTracedHandles, nullptr,
                                node->location());
    }
  }
}

void TracedHandles::IterateYoungRoots(RootVisitor* visitor) {
  DCHECK(!is_marking_);
  for (auto* block : young_blocks_) {
    DCHECK(block->InYoungList());

    for (auto* node : *block) {
      if (!node->is_in_young_list()) continue;
      DCHECK(node->is_in_use());

      if (node->is_weak()) continue;

      visitor->VisitRootPointer(Root::kTracedHandles, nullptr,
                                node->location());
    }
  }
}

void TracedHandles::IterateAndMarkYoungRootsWithOldHosts(RootVisitor* visitor) {
  DCHECK(!is_marking_);
  for (auto* block : young_blocks_) {
    for (auto* node : *block) {
      if (!node->is_in_young_list()) continue;
      DCHECK(node->is_in_use());
      if (!node->has_old_host()) continue;

      if (node->is_weak()) continue;

      node->set_markbit();
      CHECK(HeapLayout::InYoungGeneration(node->object()));
      visitor->VisitRootPointer(Root::kTracedHandles, nullptr,
                                node->location());
    }
  }
}

void TracedHandles::IterateYoungRootsWithOldHostsForTesting(
    RootVisitor* visitor) {
  DCHECK(!is_marking_);
  for (auto* block : young_blocks_) {
    for (auto* node : *block) {
      if (!node->is_in_young_list()) continue;
      DCHECK(node->is_in_use());
      if (!node->has_old_host()) continue;

      if (node->is_weak()) continue;

      visitor->VisitRootPointer(Root::kTracedHandles, nullptr,
                                node->location());
    }
  }
}

// static
void TracedHandles::Destroy(Address* location) {
  if (!location) return;

  auto* node = TracedNode::FromLocation(location);
  auto& node_block = TracedNodeBlock::From(*node);
  auto& traced_handles = node_block.traced_handles();
  traced_handles.Destroy(node_block, *node);
}

// static
void TracedHandles::Copy(const Address* const* from, Address** to) {
  DCHECK_NOT_NULL(*from);
  DCHECK_NULL(*to);

  const TracedNode* from_node = TracedNode::FromLocation(*from);
  const auto& node_block = TracedNodeBlock::From(*from_node);
  auto& traced_handles = node_block.traced_handles();
  traced_handles.Copy(*from_node, to);
}

// static
void TracedHandles::Move(Address** from, Address** to) {
  // Fast path for moving from an empty reference.
  if (!*from) {
    Destroy(*to);
    SetSlotThreadSafe(to, nullptr);
    return;
  }

  TracedNode* from_node = TracedNode::FromLocation(*from);
  auto& node_block = TracedNodeBlock::From(*from_node);
  auto& traced_handles = node_block.traced_handles();
  traced_handles.Move(*from_node, from, to);
}

namespace {
Tagged<Object> MarkObject(Tagged<Object> obj, TracedNode& node,
                          TracedHandles::MarkMode mark_mode) {
  if (mark_mode == TracedHandles::MarkMode::kOnlyYoung &&
      !node.is_in_young_list())
    return Smi::zero();
  node.set_markbit();
  // Being in the young list, the node may still point to an old object, in
  // which case we want to keep the node marked, but not follow the reference.
  if (mark_mode == TracedHandles::MarkMode::kOnlyYoung &&
      !HeapLayout::InYoungGeneration(obj))
    return Smi::zero();
  return obj;
}
}  // namespace

// static
Tagged<Object> TracedHandles::Mark(Address* location, MarkMode mark_mode) {
  // The load synchronizes internal bitfields that are also read atomically
  // from the concurrent marker. The counterpart is `TracedNode::Publish()`.
  Tagged<Object> object =
      Tagged<Object>(reinterpret_cast<std::atomic<Address>*>(location)->load(
          std::memory_order_acquire));
  auto* node = TracedNode::FromLocation(location);
  DCHECK(node->is_in_use());
  return MarkObject(object, *node, mark_mode);
}

// static
Tagged<Object> TracedHandles::MarkConservatively(
    Address* inner_location, Address* traced_node_block_base,
    MarkMode mark_mode) {
  // Compute the `TracedNode` address based on its inner pointer.
  const ptrdiff_t delta = reinterpret_cast<uintptr_t>(inner_location) -
                          reinterpret_cast<uintptr_t>(traced_node_block_base);
  const auto index = delta / sizeof(TracedNode);
  TracedNode& node =
      reinterpret_cast<TracedNode*>(traced_node_block_base)[index];
  if (!node.is_in_use()) return Smi::zero();
  return MarkObject(node.object(), node, mark_mode);
}

bool TracedHandles::IsValidInUseNode(const Address* location) {
  const TracedNode* node = TracedNode::FromLocation(location);
  // This method is called after mark bits have been cleared.
  DCHECK(!node->markbit());
  CHECK_IMPLIES(node->is_in_use(), node->raw_object() != kGlobalHandleZapValue);
  CHECK_IMPLIES(!node->is_in_use(),
                node->raw_object() == kGlobalHandleZapValue);
  return node->is_in_use();
}

bool TracedHandles::HasYoung() const { return !young_blocks_.empty(); }

}  // namespace v8::internal
