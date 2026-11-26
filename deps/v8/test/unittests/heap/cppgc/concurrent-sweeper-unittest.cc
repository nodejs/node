// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>
#include <vector>

#include "include/cppgc/allocation.h"
#include "include/cppgc/platform.h"
#include "include/v8-platform.h"
#include "src/base/page-allocator.h"
#include "src/heap/cppgc-internal/globals.h"
#include "src/heap/cppgc-internal/heap-object-header.h"
#include "src/heap/cppgc-internal/heap-page.h"
#include "src/heap/cppgc-internal/heap-space.h"
#include "src/heap/cppgc-internal/heap-visitor.h"
#include "src/heap/cppgc-internal/page-memory.h"
#include "src/heap/cppgc-internal/raw-heap.h"
#include "src/heap/cppgc-internal/stats-collector.h"
#include "src/heap/cppgc-internal/sweeper.h"
#include "test/unittests/heap/cppgc/test-platform.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

size_t g_destructor_callcount;

template <size_t Size>
class Finalizable : public GarbageCollected<Finalizable<Size>> {
 public:
  Finalizable() : creation_thread_{v8::base::OS::GetCurrentThreadId()} {}

  virtual ~Finalizable() {
    ++g_destructor_callcount;
    EXPECT_EQ(creation_thread_, v8::base::OS::GetCurrentThreadId());
  }

  virtual void Trace(cppgc::Visitor*) const {}

 private:
  char array_[Size];
  int creation_thread_;
};

using NormalFinalizable = Finalizable<32>;
using LargeFinalizable = Finalizable<kLargeObjectSizeThreshold * 2>;

template <size_t Size>
class NonFinalizable : public GarbageCollected<NonFinalizable<Size>> {
 public:
  virtual void Trace(cppgc::Visitor*) const {}

 private:
  char array_[Size];
  int padding_to_make_size_the_same_as_finalizible_;
};

using NormalNonFinalizable = NonFinalizable<32>;
using LargeNonFinalizable = NonFinalizable<kLargeObjectSizeThreshold * 2>;

}  // namespace

class ConcurrentSweeperTest : public testing::TestWithHeap {
 public:
  ConcurrentSweeperTest() { g_destructor_callcount = 0; }

  void StartSweeping() { StartSweeping(Heap::From(GetHeap())); }

  static void StartSweeping(Heap* heap) {
    heap->object_allocator().ResetLinearAllocationBuffers();
    // Pretend to finish marking as StatsCollector verifies that Notify*
    // methods are called in the right order.
    heap->stats_collector()->NotifyMarkingStarted(
        CollectionType::kMajor, GCConfig::MarkingType::kAtomic,
        GCConfig::IsForcedGC::kNotForced);
    heap->stats_collector()->NotifyMarkingCompleted(0);
    const SweepingConfig sweeping_config{
        SweepingConfig::SweepingType::kIncrementalAndConcurrent,
        SweepingConfig::CompactableSpaceHandling::kSweep};
    heap->sweeper().Start(sweeping_config);
  }

  void WaitForConcurrentSweeping() {
    WaitForConcurrentSweeping(Heap::From(GetHeap()));
  }

  static void WaitForConcurrentSweeping(Heap* heap) {
    heap->sweeper().WaitForConcurrentSweepingForTesting();
  }

  void FinishSweeping() {
    Heap* heap = Heap::From(GetHeap());
    Sweeper& sweeper = heap->sweeper();
    sweeper.FinishIfRunning();
  }

  const RawHeap& GetRawHeap() const {
    const Heap* heap = Heap::From(GetHeap());
    return heap->raw_heap();
  }

  void CheckFreeListEntries(const std::vector<void*>& objects) {
    const Heap* heap = Heap::From(GetHeap());
    const PageBackend* backend = heap->page_backend();

    for (auto* object : objects) {
      // The corresponding page could be removed.
      if (!backend->Lookup(static_cast<ConstAddress>(object))) continue;

      const auto* header =
          BasePage::FromPayload(object)->TryObjectHeaderFromInnerAddress(
              object);
      // TryObjectHeaderFromInnerAddress returns nullptr for freelist entries.
      EXPECT_EQ(nullptr, header);
    }
  }

  bool PageInBackend(const BasePage* page) {
    const Heap* heap = Heap::From(GetHeap());
    const PageBackend* backend = heap->page_backend();
    return backend->Lookup(reinterpret_cast<ConstAddress>(page));
  }

  bool FreeListContains(const BaseSpace& space,
                        const std::vector<void*>& objects) {
    const Heap* heap = Heap::From(GetHeap());
    const PageBackend* backend = heap->page_backend();
    const auto& freelist = NormalPageSpace::From(space).free_list();

    for (void* object : objects) {
      // The corresponding page could be removed.
      if (!backend->Lookup(static_cast<ConstAddress>(object))) continue;

      if (!freelist.ContainsForTesting({object, 0})) return false;
    }

    return true;
  }

  void MarkObject(void* payload) {
    HeapObjectHeader& header = HeapObjectHeader::FromObject(payload);
    header.TryMarkAtomic();
    BasePage* page = BasePage::FromPayload(&header);
    page->IncrementMarkedBytes(page->is_large()
                                   ? LargePage::From(page)->PayloadSize()
                                   : header.AllocatedSize());
  }
};

TEST_F(ConcurrentSweeperTest, BackgroundSweepOfNormalPage) {
  // Non finalizable objects are swept right away.
  using GCedType = NormalNonFinalizable;

  auto* unmarked_object = MakeGarbageCollected<GCedType>(GetAllocationHandle());
  auto* marked_object = MakeGarbageCollected<GCedType>(GetAllocationHandle());
  MarkObject(marked_object);

  auto* page = BasePage::FromPayload(unmarked_object);
  auto& space = page->space();

  // The test requires objects to be allocated on the same page;
  ASSERT_EQ(page, BasePage::FromPayload(marked_object));

  StartSweeping();

  // Wait for concurrent sweeping to finish.
  WaitForConcurrentSweeping();

  const auto& hoh = HeapObjectHeader::FromObject(marked_object);
  if (Heap::From(GetHeap())->generational_gc_supported()) {
    // Check that the marked object is still marked.
    EXPECT_TRUE(hoh.IsMarked());
  } else {
    // Check that the marked object was unmarked.
    EXPECT_FALSE(hoh.IsMarked());
  }

  // Check that free list entries are created right away for non-finalizable
  // objects, but not immediately returned to the space's freelist.
  CheckFreeListEntries({unmarked_object});
  EXPECT_FALSE(FreeListContains(space, {unmarked_object}));

  FinishSweeping();

  // Check that finalizable objects are swept and put into the freelist of the
  // corresponding space.
  EXPECT_TRUE(FreeListContains(space, {unmarked_object}));
}

TEST_F(ConcurrentSweeperTest, BackgroundSweepOfLargePage) {
  // Non finalizable objects are swept right away but the page is only returned
  // from the main thread.
  using GCedType = LargeNonFinalizable;

  auto* unmarked_object = MakeGarbageCollected<GCedType>(GetAllocationHandle());
  auto* marked_object = MakeGarbageCollected<GCedType>(GetAllocationHandle());
  MarkObject(marked_object);

  auto* unmarked_page = BasePage::FromPayload(unmarked_object);
  auto* marked_page = BasePage::FromPayload(marked_object);
  auto& space = unmarked_page->space();

  ASSERT_EQ(&space, &marked_page->space());

  StartSweeping();

  // Wait for concurrent sweeping to finish.
  WaitForConcurrentSweeping();

  const auto& hoh = HeapObjectHeader::FromObject(marked_object);
  if (Heap::From(GetHeap())->generational_gc_supported()) {
    // Check that the marked object is still marked.
    EXPECT_TRUE(hoh.IsMarked());
  } else {
    // Check that the marked object was unmarked.
    EXPECT_FALSE(hoh.IsMarked());
  }

  // The page should not have been removed on the background threads.
  EXPECT_TRUE(PageInBackend(unmarked_page));

  FinishSweeping();

  // Check that free list entries are created right away for non-finalizable
  // objects, but not immediately returned to the space's freelist.
  EXPECT_FALSE(PageInBackend(unmarked_page));

  // Check that marked pages are returned to space right away.
  EXPECT_NE(space.end(), std::find(space.begin(), space.end(), marked_page));
}

TEST_F(ConcurrentSweeperTest, DeferredFinalizationOfNormalPage) {
  static constexpr size_t kNumberOfObjects = 10;
  // Finalizable types are left intact by concurrent sweeper.
  using GCedType = NormalFinalizable;

  std::set<BasePage*> pages;
  std::vector<void*> objects;

  BaseSpace* space = nullptr;
  for (size_t i = 0; i < kNumberOfObjects; ++i) {
    auto* object = MakeGarbageCollected<GCedType>(GetAllocationHandle());
    objects.push_back(object);
    auto* page = BasePage::FromPayload(object);
    pages.insert(page);
    if (!space) space = &page->space();
  }

  StartSweeping();

  // Wait for concurrent sweeping to finish.
  WaitForConcurrentSweeping();

  // Check that pages are not returned right away.
  for (auto* page : pages) {
    EXPECT_EQ(space->end(), std::find(space->begin(), space->end(), page));
  }
  // Check that finalizable objects are left intact in pages.
  EXPECT_FALSE(FreeListContains(*space, objects));
  // No finalizers have been executed.
  EXPECT_EQ(0u, g_destructor_callcount);

  FinishSweeping();

  // Check that finalizable objects are swept and turned into freelist entries.
  CheckFreeListEntries(objects);
  // Check that space's freelist contains these entries.
  EXPECT_TRUE(FreeListContains(*space, objects));
  // Check that finalizers have been executed.
  EXPECT_EQ(kNumberOfObjects, g_destructor_callcount);
}

TEST_F(ConcurrentSweeperTest, DeferredFinalizationOfLargePage) {
  using GCedType = LargeFinalizable;

  auto* object = MakeGarbageCollected<GCedType>(GetAllocationHandle());

  auto* page = BasePage::FromPayload(object);
  auto& space = page->space();

  StartSweeping();

  // Wait for concurrent sweeping to finish.
  WaitForConcurrentSweeping();

  // Check that the page is not returned to the space.
  EXPECT_EQ(space.end(), std::find(space.begin(), space.end(), page));
  // Check that no destructors have been executed yet.
  EXPECT_EQ(0u, g_destructor_callcount);

  FinishSweeping();

  // Check that the destructor was executed.
  EXPECT_EQ(1u, g_destructor_callcount);
  // Check that page was unmapped.
  EXPECT_FALSE(PageInBackend(page));
}

TEST_F(ConcurrentSweeperTest, DestroyLargePageOnMainThread) {
  // This test fails with TSAN when large pages are destroyed concurrently
  // without proper support by the backend.
  using GCedType = LargeNonFinalizable;

  auto* object = MakeGarbageCollected<GCedType>(GetAllocationHandle());
  auto* page = BasePage::FromPayload(object);

  StartSweeping();

  // Allocating another large object should not race here.
  MakeGarbageCollected<GCedType>(GetAllocationHandle());

  // Wait for concurrent sweeping to finish.
  WaitForConcurrentSweeping();

  FinishSweeping();

  // Check that page was unmapped.
  EXPECT_FALSE(PageInBackend(page));
}

TEST_F(ConcurrentSweeperTest, IncrementalSweeping) {
  testing::TestPlatform::DisableBackgroundTasksScope disable_concurrent_sweeper(
      &GetPlatform());

  auto task_runner =
      GetPlatform().GetForegroundTaskRunner(TaskPriority::kUserBlocking);

  // Create two unmarked objects.
  MakeGarbageCollected<NormalFinalizable>(GetAllocationHandle());
  MakeGarbageCollected<LargeFinalizable>(GetAllocationHandle());

  // Create two marked objects.
  auto* marked_normal_object =
      MakeGarbageCollected<NormalFinalizable>(GetAllocationHandle());
  auto* marked_large_object =
      MakeGarbageCollected<LargeFinalizable>(GetAllocationHandle());

  auto& marked_normal_header =
      HeapObjectHeader::FromObject(marked_normal_object);
  auto& marked_large_header = HeapObjectHeader::FromObject(marked_large_object);

  MarkObject(marked_normal_object);
  MarkObject(marked_large_object);

  StartSweeping();

  EXPECT_EQ(0u, g_destructor_callcount);
  EXPECT_TRUE(marked_normal_header.IsMarked());
  // Live large objects are eagerly swept.
  if (Heap::From(GetHeap())->generational_gc_supported()) {
    EXPECT_TRUE(marked_large_header.IsMarked());
  } else {
    EXPECT_FALSE(marked_large_header.IsMarked());
  }

  // Wait for incremental sweeper to finish.
  GetPlatform().RunAllForegroundTasks();

  EXPECT_EQ(2u, g_destructor_callcount);

  if (Heap::From(GetHeap())->generational_gc_supported()) {
    EXPECT_TRUE(marked_normal_header.IsMarked());
    EXPECT_TRUE(marked_large_header.IsMarked());
  } else {
    EXPECT_FALSE(marked_normal_header.IsMarked());
    EXPECT_FALSE(marked_large_header.IsMarked());
  }

  FinishSweeping();
}

TEST_F(ConcurrentSweeperTest, SweepOnAllocationReturnEmptyPage) {
  PreciseGC();

  // First, allocate the full page of finalizable objects.
  const size_t objects_to_allocated =
      NormalPage::PayloadSize() /
      (sizeof(HeapObjectHeader) + sizeof(NormalFinalizable));
  auto* first_obj =
      MakeGarbageCollected<NormalFinalizable>(GetAllocationHandle());
  auto* finalizable_page =
      NormalPage::FromInnerAddress(&HeapBase::From(GetHeapHandle()), first_obj);
  for (size_t i = 1; i < objects_to_allocated; ++i) {
    MakeGarbageCollected<NormalFinalizable>(GetAllocationHandle());
  }

  // Then, allocate a new unfinalizable object on a fresh page. We do that so
  // that the sweeper on allocation doesn't allocate a new page.
  auto* non_finalizable =
      MakeGarbageCollected<NormalNonFinalizable>(GetAllocationHandle());
  auto* non_finalizable_page = NormalPage::FromInnerAddress(
      &HeapBase::From(GetHeapHandle()), non_finalizable);
  ASSERT_NE(finalizable_page, non_finalizable_page);

  // Start the GC without sweeping.
  static constexpr GCConfig config = {
      CollectionType::kMajor, StackState::kNoHeapPointers,
      GCConfig::MarkingType::kAtomic,
      GCConfig::SweepingType::kIncrementalAndConcurrent};
  Heap::From(GetHeap())->CollectGarbage(config);

  WaitForConcurrentSweeping();

  // Allocate and sweep.
  auto* allocated_after_sweeping =
      MakeGarbageCollected<NormalFinalizable>(GetAllocationHandle());
  // Check that the empty page of finalizable objects was returned.
  EXPECT_EQ(finalizable_page,
            NormalPage::FromInnerAddress(&HeapBase::From(GetHeapHandle()),
                                         allocated_after_sweeping));
}

namespace {

// Platform that defers foreground tasks and background jobs so tests can
// inspect and run them explicitly.
class ControlledExecutionPlatform final : public cppgc::Platform {
 public:
  // Captures the posted immediate and delayed non-nestable tasks.
  struct RecordingTaskRunner final : public cppgc::TaskRunner {
    bool IdleTasksEnabled() override { return false; }
    bool NonNestableTasksEnabled() const override { return true; }
    bool NonNestableDelayedTasksEnabled() const override { return true; }

    std::unique_ptr<cppgc::Task> immediate_task;
    std::unique_ptr<cppgc::Task> delayed_task;

   protected:
    void PostTaskImpl(std::unique_ptr<cppgc::Task>,
                      const SourceLocation&) override {
      UNREACHABLE();
    }
    void PostDelayedTaskImpl(std::unique_ptr<cppgc::Task>, double,
                             const SourceLocation&) override {
      UNREACHABLE();
    }
    void PostNonNestableTaskImpl(std::unique_ptr<cppgc::Task> task,
                                 const SourceLocation&) override {
      EXPECT_EQ(nullptr, immediate_task);
      immediate_task = std::move(task);
    }
    void PostNonNestableDelayedTaskImpl(std::unique_ptr<cppgc::Task> task,
                                        double delay_in_seconds,
                                        const SourceLocation&) override {
      EXPECT_EQ(nullptr, delayed_task);
      EXPECT_GT(delay_in_seconds, 0.0);
      delayed_task = std::move(task);
    }
  };

  PageAllocator* GetPageAllocator() override { return &page_allocator_; }
  double MonotonicallyIncreasingTime() override { return 0.0; }
  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(TaskPriority) override {
    return task_runner_;
  }
  std::unique_ptr<JobHandle> PostJob(TaskPriority,
                                     std::unique_ptr<JobTask>) override {
    return std::make_unique<DeferredJobHandle>();
  }

  RecordingTaskRunner& task_runner() { return *task_runner_; }

 private:
  // Stays active until joined or canceled, without running the background job.
  struct DeferredJobHandle final : public cppgc::JobHandle {
    void NotifyConcurrencyIncrease() override { UNREACHABLE(); }
    void Join() override { active = false; }
    void Cancel() override { active = false; }
    void CancelAndDetach() override { UNREACHABLE(); }
    bool IsActive() override { return active; }
    bool IsValid() override { return active; }

    bool active = true;
  };

  v8::base::PageAllocator page_allocator_;
  std::shared_ptr<RecordingTaskRunner> task_runner_ =
      std::make_shared<RecordingTaskRunner>();
};

}  // namespace

TEST_F(ConcurrentSweeperTest,
       LowPriorityIncrementalSweepingDelaysWhileConcurrentActive) {
  auto platform = std::make_shared<ControlledExecutionPlatform>();
  auto heap = cppgc::Heap::Create(platform);
  Heap* internal_heap = Heap::From(heap.get());

  MakeGarbageCollected<NormalFinalizable>(heap->GetAllocationHandle());
  StartSweeping(internal_heap);

  auto& runner = platform->task_runner();
  ASSERT_NE(nullptr, runner.immediate_task);
  ASSERT_NE(nullptr, runner.delayed_task);

  // Clear the regular-priority fallback task posted by StartSweeping(), then
  // run the low-priority task while concurrent sweeping is still active and no
  // swept-unfinalized pages are ready yet. It must back off with a delayed task
  // rather than reposting a zero-delay task.
  runner.delayed_task.reset();
  auto task = std::move(runner.immediate_task);
  task->Run();

  EXPECT_TRUE(internal_heap->sweeper().IsSweepingInProgress());
  EXPECT_EQ(0u, g_destructor_callcount);
  EXPECT_EQ(nullptr, runner.immediate_task);
  ASSERT_NE(nullptr, runner.delayed_task);

  // Once concurrent sweeping completes, the rescheduled task finishes sweeping.
  WaitForConcurrentSweeping(internal_heap);
  task = std::move(runner.delayed_task);
  task->Run();

  EXPECT_FALSE(internal_heap->sweeper().IsSweepingInProgress());
  EXPECT_EQ(1u, g_destructor_callcount);
  EXPECT_EQ(nullptr, runner.immediate_task);
  EXPECT_EQ(nullptr, runner.delayed_task);
}

}  // namespace internal
}  // namespace cppgc
