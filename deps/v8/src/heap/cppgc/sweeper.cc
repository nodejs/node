// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/sweeper.h"

#include <atomic>
#include <memory>
#include <vector>

#include "include/cppgc/platform.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/heap/cppgc/free-list.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/object-start-bitmap.h"
#include "src/heap/cppgc/raw-heap.h"
#include "src/heap/cppgc/sanitizers.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/heap/cppgc/task-handle.h"

namespace cppgc {
namespace internal {

namespace {

using v8::base::Optional;

class ObjectStartBitmapVerifier
    : private HeapVisitor<ObjectStartBitmapVerifier> {
  friend class HeapVisitor<ObjectStartBitmapVerifier>;

 public:
  void Verify(RawHeap* heap) { Traverse(heap); }

 private:
  bool VisitNormalPage(NormalPage* page) {
    // Remember bitmap and reset previous pointer.
    bitmap_ = &page->object_start_bitmap();
    prev_ = nullptr;
    return false;
  }

  bool VisitHeapObjectHeader(HeapObjectHeader* header) {
    if (header->IsLargeObject()) return true;

    auto* raw_header = reinterpret_cast<ConstAddress>(header);
    CHECK(bitmap_->CheckBit(raw_header));
    if (prev_) {
      CHECK_EQ(prev_, bitmap_->FindHeader(raw_header - 1));
    }
    prev_ = header;
    return true;
  }

  PlatformAwareObjectStartBitmap* bitmap_ = nullptr;
  HeapObjectHeader* prev_ = nullptr;
};

template <typename T>
class ThreadSafeStack {
 public:
  ThreadSafeStack() = default;

  void Push(T t) {
    v8::base::LockGuard<v8::base::Mutex> lock(&mutex_);
    vector_.push_back(std::move(t));
  }

  Optional<T> Pop() {
    v8::base::LockGuard<v8::base::Mutex> lock(&mutex_);
    if (vector_.empty()) return v8::base::nullopt;
    T top = std::move(vector_.back());
    vector_.pop_back();
    // std::move is redundant but is needed to avoid the bug in gcc-7.
    return std::move(top);
  }

  template <typename It>
  void Insert(It begin, It end) {
    v8::base::LockGuard<v8::base::Mutex> lock(&mutex_);
    vector_.insert(vector_.end(), begin, end);
  }

  bool IsEmpty() const {
    v8::base::LockGuard<v8::base::Mutex> lock(&mutex_);
    return vector_.empty();
  }

 private:
  std::vector<T> vector_;
  mutable v8::base::Mutex mutex_;
};

struct SpaceState {
  struct SweptPageState {
    BasePage* page = nullptr;
    std::vector<HeapObjectHeader*> unfinalized_objects;
    FreeList cached_free_list;
    std::vector<FreeList::Block> unfinalized_free_list;
    bool is_empty = false;
    size_t largest_new_free_list_entry = 0;
  };

  ThreadSafeStack<BasePage*> unswept_pages;
  ThreadSafeStack<SweptPageState> swept_unfinalized_pages;
};

using SpaceStates = std::vector<SpaceState>;

void StickyUnmark(HeapObjectHeader* header) {
  // Young generation in Oilpan uses sticky mark bits.
#if !defined(CPPGC_YOUNG_GENERATION)
  header->Unmark<AccessMode::kAtomic>();
#endif
}

// Builder that finalizes objects and adds freelist entries right away.
class InlinedFinalizationBuilder final {
 public:
  struct ResultType {
    bool is_empty = false;
    size_t largest_new_free_list_entry = 0;
  };

  explicit InlinedFinalizationBuilder(BasePage* page) : page_(page) {}

  void AddFinalizer(HeapObjectHeader* header, size_t size) {
    header->Finalize();
    SET_MEMORY_INACCESSIBLE(header, size);
  }

  void AddFreeListEntry(Address start, size_t size) {
    auto* space = NormalPageSpace::From(page_->space());
    space->free_list().Add({start, size});
  }

  ResultType GetResult(bool is_empty, size_t largest_new_free_list_entry) {
    return {is_empty, largest_new_free_list_entry};
  }

 private:
  BasePage* page_;
};

// Builder that produces results for deferred processing.
class DeferredFinalizationBuilder final {
 public:
  using ResultType = SpaceState::SweptPageState;

  explicit DeferredFinalizationBuilder(BasePage* page) { result_.page = page; }

  void AddFinalizer(HeapObjectHeader* header, size_t size) {
    if (header->IsFinalizable()) {
      result_.unfinalized_objects.push_back({header});
      found_finalizer_ = true;
    } else {
      SET_MEMORY_INACCESSIBLE(header, size);
    }
  }

  void AddFreeListEntry(Address start, size_t size) {
    if (found_finalizer_) {
      result_.unfinalized_free_list.push_back({start, size});
    } else {
      result_.cached_free_list.Add({start, size});
    }
    found_finalizer_ = false;
  }

  ResultType&& GetResult(bool is_empty, size_t largest_new_free_list_entry) {
    result_.is_empty = is_empty;
    result_.largest_new_free_list_entry = largest_new_free_list_entry;
    return std::move(result_);
  }

 private:
  ResultType result_;
  bool found_finalizer_ = false;
};

template <typename FinalizationBuilder>
typename FinalizationBuilder::ResultType SweepNormalPage(NormalPage* page) {
  constexpr auto kAtomicAccess = AccessMode::kAtomic;
  FinalizationBuilder builder(page);

  PlatformAwareObjectStartBitmap& bitmap = page->object_start_bitmap();
  bitmap.Clear();

  size_t largest_new_free_list_entry = 0;

  Address start_of_gap = page->PayloadStart();
  for (Address begin = page->PayloadStart(), end = page->PayloadEnd();
       begin != end;) {
    HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(begin);
    const size_t size = header->GetSize();
    // Check if this is a free list entry.
    if (header->IsFree<kAtomicAccess>()) {
      SET_MEMORY_INACCESSIBLE(header, std::min(kFreeListEntrySize, size));
      begin += size;
      continue;
    }
    // Check if object is not marked (not reachable).
    if (!header->IsMarked<kAtomicAccess>()) {
      builder.AddFinalizer(header, size);
      begin += size;
      continue;
    }
    // The object is alive.
    const Address header_address = reinterpret_cast<Address>(header);
    if (start_of_gap != header_address) {
      size_t new_free_list_entry_size =
          static_cast<size_t>(header_address - start_of_gap);
      builder.AddFreeListEntry(start_of_gap, new_free_list_entry_size);
      largest_new_free_list_entry =
          std::max(largest_new_free_list_entry, new_free_list_entry_size);
      bitmap.SetBit(start_of_gap);
    }
    StickyUnmark(header);
    bitmap.SetBit(begin);
    begin += size;
    start_of_gap = begin;
  }

  if (start_of_gap != page->PayloadStart() &&
      start_of_gap != page->PayloadEnd()) {
    builder.AddFreeListEntry(
        start_of_gap, static_cast<size_t>(page->PayloadEnd() - start_of_gap));
    bitmap.SetBit(start_of_gap);
  }

  const bool is_empty = (start_of_gap == page->PayloadStart());
  return builder.GetResult(is_empty, largest_new_free_list_entry);
}

// SweepFinalizer is responsible for heap/space/page finalization. Finalization
// is defined as a step following concurrent sweeping which:
// - calls finalizers;
// - returns (unmaps) empty pages;
// - merges freelists to the space's freelist.
class SweepFinalizer final {
 public:
  explicit SweepFinalizer(cppgc::Platform* platform) : platform_(platform) {}

  void FinalizeHeap(SpaceStates* space_states) {
    for (SpaceState& space_state : *space_states) {
      FinalizeSpace(&space_state);
    }
  }

  void FinalizeSpace(SpaceState* space_state) {
    while (auto page_state = space_state->swept_unfinalized_pages.Pop()) {
      FinalizePage(&*page_state);
    }
  }

  bool FinalizeSpaceWithDeadline(SpaceState* space_state,
                                 double deadline_in_seconds) {
    DCHECK(platform_);
    static constexpr size_t kDeadlineCheckInterval = 8;
    size_t page_count = 1;

    while (auto page_state = space_state->swept_unfinalized_pages.Pop()) {
      FinalizePage(&*page_state);

      if (page_count % kDeadlineCheckInterval == 0 &&
          deadline_in_seconds <= platform_->MonotonicallyIncreasingTime()) {
        return false;
      }

      page_count++;
    }

    return true;
  }

  void FinalizePage(SpaceState::SweptPageState* page_state) {
    DCHECK(page_state);
    DCHECK(page_state->page);
    BasePage* page = page_state->page;

    // Call finalizers.
    for (HeapObjectHeader* object : page_state->unfinalized_objects) {
      const size_t size = object->GetSize();
      object->Finalize();
      SET_MEMORY_INACCESSIBLE(object, size);
    }

    // Unmap page if empty.
    if (page_state->is_empty) {
      BasePage::Destroy(page);
      return;
    }

    DCHECK(!page->is_large());

    // Merge freelists without finalizers.
    FreeList& space_freelist =
        NormalPageSpace::From(page->space())->free_list();
    space_freelist.Append(std::move(page_state->cached_free_list));

    // Merge freelist with finalizers.
    for (auto entry : page_state->unfinalized_free_list) {
      space_freelist.Add(std::move(entry));
    }

    largest_new_free_list_entry_ = std::max(
        page_state->largest_new_free_list_entry, largest_new_free_list_entry_);

    // Add the page to the space.
    page->space()->AddPage(page);
  }

  size_t largest_new_free_list_entry() const {
    return largest_new_free_list_entry_;
  }

 private:
  cppgc::Platform* platform_;
  size_t largest_new_free_list_entry_ = 0;
};

class MutatorThreadSweeper final : private HeapVisitor<MutatorThreadSweeper> {
  friend class HeapVisitor<MutatorThreadSweeper>;

 public:
  explicit MutatorThreadSweeper(SpaceStates* states, cppgc::Platform* platform)
      : states_(states), platform_(platform) {}

  void Sweep() {
    for (SpaceState& state : *states_) {
      while (auto page = state.unswept_pages.Pop()) {
        SweepPage(*page);
      }
    }
  }

  void SweepPage(BasePage* page) { Traverse(page); }

  bool SweepWithDeadline(double deadline_in_seconds) {
    DCHECK(platform_);
    static constexpr double kSlackInSeconds = 0.001;
    for (SpaceState& state : *states_) {
      // FinalizeSpaceWithDeadline() and SweepSpaceWithDeadline() won't check
      // the deadline until it sweeps 10 pages. So we give a small slack for
      // safety.
      const double remaining_budget = deadline_in_seconds - kSlackInSeconds -
                                      platform_->MonotonicallyIncreasingTime();
      if (remaining_budget <= 0.) return false;

      // First, prioritize finalization of pages that were swept concurrently.
      SweepFinalizer finalizer(platform_);
      if (!finalizer.FinalizeSpaceWithDeadline(&state, deadline_in_seconds)) {
        return false;
      }

      // Help out the concurrent sweeper.
      if (!SweepSpaceWithDeadline(&state, deadline_in_seconds)) {
        return false;
      }
    }
    return true;
  }

  size_t largest_new_free_list_entry() const {
    return largest_new_free_list_entry_;
  }

 private:
  bool SweepSpaceWithDeadline(SpaceState* state, double deadline_in_seconds) {
    static constexpr size_t kDeadlineCheckInterval = 8;
    size_t page_count = 1;
    while (auto page = state->unswept_pages.Pop()) {
      Traverse(*page);
      if (page_count % kDeadlineCheckInterval == 0 &&
          deadline_in_seconds <= platform_->MonotonicallyIncreasingTime()) {
        return false;
      }
      page_count++;
    }

    return true;
  }

  bool VisitNormalPage(NormalPage* page) {
    const InlinedFinalizationBuilder::ResultType result =
        SweepNormalPage<InlinedFinalizationBuilder>(page);
    if (result.is_empty) {
      NormalPage::Destroy(page);
    } else {
      page->space()->AddPage(page);
      largest_new_free_list_entry_ = std::max(
          result.largest_new_free_list_entry, largest_new_free_list_entry_);
    }
    return true;
  }

  bool VisitLargePage(LargePage* page) {
    HeapObjectHeader* header = page->ObjectHeader();
    if (header->IsMarked()) {
      StickyUnmark(header);
      page->space()->AddPage(page);
    } else {
      header->Finalize();
      LargePage::Destroy(page);
    }
    return true;
  }

  SpaceStates* states_;
  cppgc::Platform* platform_;
  size_t largest_new_free_list_entry_ = 0;
};

class ConcurrentSweepTask final : public cppgc::JobTask,
                                  private HeapVisitor<ConcurrentSweepTask> {
  friend class HeapVisitor<ConcurrentSweepTask>;

 public:
  explicit ConcurrentSweepTask(HeapBase& heap, SpaceStates* states)
      : heap_(heap), states_(states) {}

  void Run(cppgc::JobDelegate* delegate) final {
    StatsCollector::EnabledConcurrentScope stats_scope(
        heap_.stats_collector(), StatsCollector::kConcurrentSweep);

    for (SpaceState& state : *states_) {
      while (auto page = state.unswept_pages.Pop()) {
        Traverse(*page);
        if (delegate->ShouldYield()) return;
      }
    }
    is_completed_.store(true, std::memory_order_relaxed);
  }

  size_t GetMaxConcurrency(size_t /* active_worker_count */) const final {
    return is_completed_.load(std::memory_order_relaxed) ? 0 : 1;
  }

 private:
  bool VisitNormalPage(NormalPage* page) {
    SpaceState::SweptPageState sweep_result =
        SweepNormalPage<DeferredFinalizationBuilder>(page);
    const size_t space_index = page->space()->index();
    DCHECK_GT(states_->size(), space_index);
    SpaceState& space_state = (*states_)[space_index];
    space_state.swept_unfinalized_pages.Push(std::move(sweep_result));
    return true;
  }

  bool VisitLargePage(LargePage* page) {
    HeapObjectHeader* header = page->ObjectHeader();
    if (header->IsMarked()) {
      StickyUnmark(header);
      page->space()->AddPage(page);
      return true;
    }
    if (!header->IsFinalizable()) {
      LargePage::Destroy(page);
      return true;
    }
    const size_t space_index = page->space()->index();
    DCHECK_GT(states_->size(), space_index);
    SpaceState& state = (*states_)[space_index];
    state.swept_unfinalized_pages.Push(
        {page, {page->ObjectHeader()}, {}, {}, true});
    return true;
  }

  HeapBase& heap_;
  SpaceStates* states_;
  std::atomic_bool is_completed_{false};
};

// This visitor:
// - resets linear allocation buffers and clears free lists for all spaces;
// - moves all Heap pages to local Sweeper's state (SpaceStates).
class PrepareForSweepVisitor final
    : public HeapVisitor<PrepareForSweepVisitor> {
  using CompactableSpaceHandling =
      Sweeper::SweepingConfig::CompactableSpaceHandling;

 public:
  PrepareForSweepVisitor(SpaceStates* states,
                         CompactableSpaceHandling compactable_space_handling)
      : states_(states),
        compactable_space_handling_(compactable_space_handling) {}

  bool VisitNormalPageSpace(NormalPageSpace* space) {
    if ((compactable_space_handling_ == CompactableSpaceHandling::kIgnore) &&
        space->is_compactable())
      return true;
    DCHECK(!space->linear_allocation_buffer().size());
    space->free_list().Clear();
    ExtractPages(space);
    return true;
  }

  bool VisitLargePageSpace(LargePageSpace* space) {
    ExtractPages(space);
    return true;
  }

 private:
  void ExtractPages(BaseSpace* space) {
    BaseSpace::Pages space_pages = space->RemoveAllPages();
    (*states_)[space->index()].unswept_pages.Insert(space_pages.begin(),
                                                    space_pages.end());
  }

  SpaceStates* states_;
  CompactableSpaceHandling compactable_space_handling_;
};

}  // namespace

class Sweeper::SweeperImpl final {
 public:
  SweeperImpl(RawHeap* heap, cppgc::Platform* platform,
              StatsCollector* stats_collector)
      : heap_(heap),
        stats_collector_(stats_collector),
        space_states_(heap->size()),
        platform_(platform) {}

  ~SweeperImpl() { CancelSweepers(); }

  void Start(SweepingConfig config) {
    StatsCollector::EnabledScope stats_scope(heap_->heap()->stats_collector(),
                                             StatsCollector::kAtomicSweep);
    is_in_progress_ = true;
#if DEBUG
    // Verify bitmap for all spaces regardless of |compactable_space_handling|.
    ObjectStartBitmapVerifier().Verify(heap_);
#endif
    PrepareForSweepVisitor(&space_states_, config.compactable_space_handling)
        .Traverse(heap_);

    if (config.sweeping_type == SweepingConfig::SweepingType::kAtomic) {
      Finish();
    } else {
      DCHECK_EQ(SweepingConfig::SweepingType::kIncrementalAndConcurrent,
                config.sweeping_type);
      ScheduleIncrementalSweeping();
      ScheduleConcurrentSweeping();
    }
  }

  bool SweepForAllocationIfRunning(NormalPageSpace* space, size_t size) {
    if (!is_in_progress_) return false;

    // Bail out for recursive sweeping calls. This can happen when finalizers
    // allocate new memory.
    if (is_sweeping_on_mutator_thread_) return false;

    StatsCollector::EnabledScope stats_scope(heap_->heap()->stats_collector(),
                                             StatsCollector::kIncrementalSweep);
    StatsCollector::EnabledScope inner_scope(
        heap_->heap()->stats_collector(), StatsCollector::kSweepOnAllocation);
    MutatorThreadSweepingScope sweeping_in_progresss(*this);

    SpaceState& space_state = space_states_[space->index()];

    {
      // First, process unfinalized pages as finalizing a page is faster than
      // sweeping.
      SweepFinalizer finalizer(platform_);
      while (auto page = space_state.swept_unfinalized_pages.Pop()) {
        finalizer.FinalizePage(&*page);
        if (size <= finalizer.largest_new_free_list_entry()) return true;
      }
    }
    {
      // Then, if no matching slot is found in the unfinalized pages, search the
      // unswept page. This also helps out the concurrent sweeper.
      MutatorThreadSweeper sweeper(&space_states_, platform_);
      while (auto page = space_state.unswept_pages.Pop()) {
        sweeper.SweepPage(*page);
        if (size <= sweeper.largest_new_free_list_entry()) return true;
      }
    }

    return false;
  }

  void FinishIfRunning() {
    if (!is_in_progress_) return;

    // Bail out for recursive sweeping calls. This can happen when finalizers
    // allocate new memory.
    if (is_sweeping_on_mutator_thread_) return;

    {
      StatsCollector::EnabledScope stats_scope(
          heap_->heap()->stats_collector(), StatsCollector::kIncrementalSweep);
      StatsCollector::EnabledScope inner_scope(heap_->heap()->stats_collector(),
                                               StatsCollector::kSweepFinalize);
      if (concurrent_sweeper_handle_ && concurrent_sweeper_handle_->IsValid() &&
          concurrent_sweeper_handle_->UpdatePriorityEnabled()) {
        concurrent_sweeper_handle_->UpdatePriority(
            cppgc::TaskPriority::kUserBlocking);
      }
      Finish();
    }
    NotifyDone();
  }

  void Finish() {
    DCHECK(is_in_progress_);

    MutatorThreadSweepingScope sweeping_in_progresss(*this);

    // First, call finalizers on the mutator thread.
    SweepFinalizer finalizer(platform_);
    finalizer.FinalizeHeap(&space_states_);

    // Then, help out the concurrent thread.
    MutatorThreadSweeper sweeper(&space_states_, platform_);
    sweeper.Sweep();

    FinalizeSweep();
  }

  void FinalizeSweep() {
    // Synchronize with the concurrent sweeper and call remaining finalizers.
    SynchronizeAndFinalizeConcurrentSweeping();
    is_in_progress_ = false;
    notify_done_pending_ = true;
  }

  void NotifyDone() {
    DCHECK(!is_in_progress_);
    DCHECK(notify_done_pending_);
    notify_done_pending_ = false;
    stats_collector_->NotifySweepingCompleted();
  }

  void NotifyDoneIfNeeded() {
    if (!notify_done_pending_) return;
    NotifyDone();
  }

  void WaitForConcurrentSweepingForTesting() {
    if (concurrent_sweeper_handle_) concurrent_sweeper_handle_->Join();
  }

  bool IsSweepingOnMutatorThread() const {
    return is_sweeping_on_mutator_thread_;
  }

  bool IsSweepingInProgress() const { return is_in_progress_; }

 private:
  class MutatorThreadSweepingScope final {
   public:
    explicit MutatorThreadSweepingScope(SweeperImpl& sweeper)
        : sweeper_(sweeper) {
      DCHECK(!sweeper_.is_sweeping_on_mutator_thread_);
      sweeper_.is_sweeping_on_mutator_thread_ = true;
    }
    ~MutatorThreadSweepingScope() {
      sweeper_.is_sweeping_on_mutator_thread_ = false;
    }

    MutatorThreadSweepingScope(const MutatorThreadSweepingScope&) = delete;
    MutatorThreadSweepingScope& operator=(const MutatorThreadSweepingScope&) =
        delete;

   private:
    SweeperImpl& sweeper_;
  };

  class IncrementalSweepTask : public cppgc::IdleTask {
   public:
    using Handle = SingleThreadedHandle;

    explicit IncrementalSweepTask(SweeperImpl* sweeper)
        : sweeper_(sweeper), handle_(Handle::NonEmptyTag{}) {}

    static Handle Post(SweeperImpl* sweeper, cppgc::TaskRunner* runner) {
      auto task = std::make_unique<IncrementalSweepTask>(sweeper);
      auto handle = task->GetHandle();
      runner->PostIdleTask(std::move(task));
      return handle;
    }

   private:
    void Run(double deadline_in_seconds) override {
      if (handle_.IsCanceled() || !sweeper_->is_in_progress_) return;

      MutatorThreadSweepingScope sweeping_in_progresss(*sweeper_);

      bool sweep_complete;
      {
        StatsCollector::EnabledScope stats_scope(
            sweeper_->heap_->heap()->stats_collector(),
            StatsCollector::kIncrementalSweep);

        MutatorThreadSweeper sweeper(&sweeper_->space_states_,
                                     sweeper_->platform_);
        {
          StatsCollector::EnabledScope stats_scope(
              sweeper_->heap_->heap()->stats_collector(),
              StatsCollector::kSweepIdleStep, "idleDeltaInSeconds",
              (deadline_in_seconds -
               sweeper_->platform_->MonotonicallyIncreasingTime()));

          sweep_complete = sweeper.SweepWithDeadline(deadline_in_seconds);
        }
        if (sweep_complete) {
          sweeper_->FinalizeSweep();
        } else {
          sweeper_->ScheduleIncrementalSweeping();
        }
      }
      if (sweep_complete) sweeper_->NotifyDone();
    }

    Handle GetHandle() const { return handle_; }

    SweeperImpl* sweeper_;
    // TODO(chromium:1056170): Change to CancelableTask.
    Handle handle_;
  };

  void ScheduleIncrementalSweeping() {
    DCHECK(platform_);
    auto runner = platform_->GetForegroundTaskRunner();
    if (!runner || !runner->IdleTasksEnabled()) return;

    incremental_sweeper_handle_ =
        IncrementalSweepTask::Post(this, runner.get());
  }

  void ScheduleConcurrentSweeping() {
    DCHECK(platform_);

    concurrent_sweeper_handle_ = platform_->PostJob(
        cppgc::TaskPriority::kUserVisible,
        std::make_unique<ConcurrentSweepTask>(*heap_->heap(), &space_states_));
  }

  void CancelSweepers() {
    if (incremental_sweeper_handle_) incremental_sweeper_handle_.Cancel();
    if (concurrent_sweeper_handle_ && concurrent_sweeper_handle_->IsValid())
      concurrent_sweeper_handle_->Cancel();
  }

  void SynchronizeAndFinalizeConcurrentSweeping() {
    CancelSweepers();

    SweepFinalizer finalizer(platform_);
    finalizer.FinalizeHeap(&space_states_);
  }

  RawHeap* heap_;
  StatsCollector* stats_collector_;
  SpaceStates space_states_;
  cppgc::Platform* platform_;
  IncrementalSweepTask::Handle incremental_sweeper_handle_;
  std::unique_ptr<cppgc::JobHandle> concurrent_sweeper_handle_;
  // Indicates whether the sweeping phase is in progress.
  bool is_in_progress_ = false;
  bool notify_done_pending_ = false;
  // Indicates whether whether the sweeper (or its finalization) is currently
  // running on the main thread.
  bool is_sweeping_on_mutator_thread_ = false;
};

Sweeper::Sweeper(RawHeap* heap, cppgc::Platform* platform,
                 StatsCollector* stats_collector)
    : impl_(std::make_unique<SweeperImpl>(heap, platform, stats_collector)) {}

Sweeper::~Sweeper() = default;

void Sweeper::Start(SweepingConfig config) { impl_->Start(config); }
void Sweeper::FinishIfRunning() { impl_->FinishIfRunning(); }
void Sweeper::WaitForConcurrentSweepingForTesting() {
  impl_->WaitForConcurrentSweepingForTesting();
}
void Sweeper::NotifyDoneIfNeeded() { impl_->NotifyDoneIfNeeded(); }
bool Sweeper::SweepForAllocationIfRunning(NormalPageSpace* space, size_t size) {
  return impl_->SweepForAllocationIfRunning(space, size);
}
bool Sweeper::IsSweepingOnMutatorThread() const {
  return impl_->IsSweepingOnMutatorThread();
}

bool Sweeper::IsSweepingInProgress() const {
  return impl_->IsSweepingInProgress();
}

}  // namespace internal
}  // namespace cppgc
