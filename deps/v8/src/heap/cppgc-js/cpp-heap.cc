// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/cpp-heap.h"

#include "include/cppgc/platform.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/cppgc-js/unified-heap-marking-visitor.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/object-allocator.h"
#include "src/heap/cppgc/prefinalizer-handler.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/heap/cppgc/sweeper.h"
#include "src/heap/marking-worklist.h"
#include "src/heap/sweeper.h"
#include "src/init/v8.h"

namespace v8 {
namespace internal {

namespace {

class CppgcPlatformAdapter final : public cppgc::Platform {
 public:
  explicit CppgcPlatformAdapter(v8::Isolate* isolate)
      : platform_(V8::GetCurrentPlatform()), isolate_(isolate) {}

  CppgcPlatformAdapter(const CppgcPlatformAdapter&) = delete;
  CppgcPlatformAdapter& operator=(const CppgcPlatformAdapter&) = delete;

  PageAllocator* GetPageAllocator() final {
    return platform_->GetPageAllocator();
  }

  double MonotonicallyIncreasingTime() final {
    return platform_->MonotonicallyIncreasingTime();
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner() final {
    return platform_->GetForegroundTaskRunner(isolate_);
  }

  std::unique_ptr<JobHandle> PostJob(TaskPriority priority,
                                     std::unique_ptr<JobTask> job_task) final {
    return platform_->PostJob(priority, std::move(job_task));
  }

 private:
  v8::Platform* platform_;
  v8::Isolate* isolate_;
};

class UnifiedHeapMarker final : public cppgc::internal::MarkerBase {
 public:
  explicit UnifiedHeapMarker(Heap& v8_heap, cppgc::internal::HeapBase& heap);

  ~UnifiedHeapMarker() final = default;

  void AddObject(void*);

 protected:
  cppgc::Visitor& visitor() final { return marking_visitor_; }
  cppgc::internal::ConservativeTracingVisitor& conservative_visitor() final {
    return conservative_marking_visitor_;
  }
  ::heap::base::StackVisitor& stack_visitor() final {
    return conservative_marking_visitor_;
  }

 private:
  UnifiedHeapMarkingState unified_heap_mutator_marking_state_;
  UnifiedHeapMarkingVisitor marking_visitor_;
  cppgc::internal::ConservativeMarkingVisitor conservative_marking_visitor_;
};

UnifiedHeapMarker::UnifiedHeapMarker(Heap& v8_heap,
                                     cppgc::internal::HeapBase& heap)
    : cppgc::internal::MarkerBase(heap),
      unified_heap_mutator_marking_state_(v8_heap),
      marking_visitor_(heap, mutator_marking_state_,
                       unified_heap_mutator_marking_state_),
      conservative_marking_visitor_(heap, mutator_marking_state_,
                                    marking_visitor_) {}

void UnifiedHeapMarker::AddObject(void* object) {
  mutator_marking_state_.MarkAndPush(
      cppgc::internal::HeapObjectHeader::FromPayload(object));
}

}  // namespace

CppHeap::CppHeap(v8::Isolate* isolate, size_t custom_spaces)
    : cppgc::internal::HeapBase(std::make_shared<CppgcPlatformAdapter>(isolate),
                                custom_spaces),
      isolate_(*reinterpret_cast<Isolate*>(isolate)) {
  CHECK(!FLAG_incremental_marking_wrappers);
}

void CppHeap::RegisterV8References(
    const std::vector<std::pair<void*, void*> >& embedder_fields) {
  DCHECK(marker_);
  for (auto& tuple : embedder_fields) {
    // First field points to type.
    // Second field points to object.
    static_cast<UnifiedHeapMarker*>(marker_.get())->AddObject(tuple.second);
  }
  marking_done_ = false;
}

void CppHeap::TracePrologue(TraceFlags flags) {
  marker_.reset(new UnifiedHeapMarker(*isolate_.heap(), AsBase()));
  const UnifiedHeapMarker::MarkingConfig marking_config{
      UnifiedHeapMarker::MarkingConfig::CollectionType::kMajor,
      cppgc::Heap::StackState::kNoHeapPointers,
      UnifiedHeapMarker::MarkingConfig::MarkingType::kAtomic};
  marker_->StartMarking(marking_config);
  marking_done_ = false;
}

bool CppHeap::AdvanceTracing(double deadline_in_ms) {
  marking_done_ = marker_->AdvanceMarkingWithDeadline(
      v8::base::TimeDelta::FromMillisecondsD(deadline_in_ms));
  return marking_done_;
}

bool CppHeap::IsTracingDone() { return marking_done_; }

void CppHeap::EnterFinalPause(EmbedderStackState stack_state) {
  const UnifiedHeapMarker::MarkingConfig marking_config{
      UnifiedHeapMarker::MarkingConfig::CollectionType::kMajor,
      cppgc::Heap::StackState::kNoHeapPointers,
      UnifiedHeapMarker::MarkingConfig::MarkingType::kAtomic};
  marker_->EnterAtomicPause(marking_config);
}

void CppHeap::TraceEpilogue(TraceSummary* trace_summary) {
  CHECK(marking_done_);
  marker_->LeaveAtomicPause();
  {
    // Pre finalizers are forbidden from allocating objects
    cppgc::internal::ObjectAllocator::NoAllocationScope no_allocation_scope_(
        object_allocator_);
    marker()->ProcessWeakness();
    prefinalizer_handler()->InvokePreFinalizers();
  }
  marker_.reset();
  // TODO(chromium:1056170): replace build flag with dedicated flag.
#if DEBUG
  VerifyMarking(cppgc::Heap::StackState::kNoHeapPointers);
#endif
  {
    NoGCScope no_gc(*this);
    sweeper().Start(cppgc::internal::Sweeper::Config::kAtomic);
  }
}

}  // namespace internal
}  // namespace v8
