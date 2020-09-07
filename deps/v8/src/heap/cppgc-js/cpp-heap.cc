// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/cpp-heap.h"

#include "include/cppgc/platform.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/time.h"
#include "src/flags/flags.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/marker.h"
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

class UnifiedHeapMarker : public cppgc::internal::Marker {
 public:
  explicit UnifiedHeapMarker(cppgc::internal::HeapBase& heap);

  void AddObject(void*);

  // TODO(chromium:1056170): Implement unified heap specific
  // CreateMutatorThreadMarkingVisitor and AdvanceMarkingWithDeadline.
};

UnifiedHeapMarker::UnifiedHeapMarker(cppgc::internal::HeapBase& heap)
    : cppgc::internal::Marker(heap) {}

void UnifiedHeapMarker::AddObject(void* object) {
  auto& header = cppgc::internal::HeapObjectHeader::FromPayload(object);
  marking_visitor_->MarkObject(header);
}

}  // namespace

CppHeap::CppHeap(v8::Isolate* isolate, size_t custom_spaces)
    : cppgc::internal::HeapBase(std::make_shared<CppgcPlatformAdapter>(isolate),
                                custom_spaces) {
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
  marker_ = std::make_unique<UnifiedHeapMarker>(AsBase());
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
  {
    NoGCScope no_gc(*this);
    sweeper().Start(cppgc::internal::Sweeper::Config::kAtomic);
  }
}

}  // namespace internal
}  // namespace v8
