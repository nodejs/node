// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/gc-invoker.h"

#include <memory>

#include "include/cppgc/platform.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/task-handle.h"

namespace cppgc {
namespace internal {

class GCInvoker::GCInvokerImpl final : public GarbageCollector {
 public:
  GCInvokerImpl(GarbageCollector*, cppgc::Platform*, cppgc::Heap::StackSupport);
  ~GCInvokerImpl();

  GCInvokerImpl(const GCInvokerImpl&) = delete;
  GCInvokerImpl& operator=(const GCInvokerImpl&) = delete;

  void CollectGarbage(GarbageCollector::Config) final;
  void StartIncrementalGarbageCollection(GarbageCollector::Config) final;
  size_t epoch() const final { return collector_->epoch(); }

 private:
  class GCTask final : public cppgc::Task {
   public:
    using Handle = SingleThreadedHandle;

    static Handle Post(GarbageCollector* collector, cppgc::TaskRunner* runner) {
      auto task = std::make_unique<GCInvoker::GCInvokerImpl::GCTask>(collector);
      auto handle = task->GetHandle();
      runner->PostNonNestableTask(std::move(task));
      return handle;
    }

    explicit GCTask(GarbageCollector* collector)
        : collector_(collector),
          handle_(Handle::NonEmptyTag{}),
          saved_epoch_(collector->epoch()) {}

   private:
    void Run() final {
      if (handle_.IsCanceled() || (collector_->epoch() != saved_epoch_)) return;

      collector_->CollectGarbage(
          GarbageCollector::Config::PreciseAtomicConfig());
      handle_.Cancel();
    }

    Handle GetHandle() { return handle_; }

    GarbageCollector* collector_;
    Handle handle_;
    size_t saved_epoch_;
  };

  GarbageCollector* collector_;
  cppgc::Platform* platform_;
  cppgc::Heap::StackSupport stack_support_;
  GCTask::Handle gc_task_handle_;
};

GCInvoker::GCInvokerImpl::GCInvokerImpl(GarbageCollector* collector,
                                        cppgc::Platform* platform,
                                        cppgc::Heap::StackSupport stack_support)
    : collector_(collector),
      platform_(platform),
      stack_support_(stack_support) {}

GCInvoker::GCInvokerImpl::~GCInvokerImpl() {
  if (gc_task_handle_) {
    gc_task_handle_.Cancel();
  }
}

void GCInvoker::GCInvokerImpl::CollectGarbage(GarbageCollector::Config config) {
  if ((config.stack_state ==
       GarbageCollector::Config::StackState::kNoHeapPointers) ||
      (stack_support_ ==
       cppgc::Heap::StackSupport::kSupportsConservativeStackScan)) {
    collector_->CollectGarbage(config);
  } else if (platform_->GetForegroundTaskRunner()->NonNestableTasksEnabled()) {
    if (!gc_task_handle_) {
      gc_task_handle_ =
          GCTask::Post(collector_, platform_->GetForegroundTaskRunner().get());
    }
  }
}

void GCInvoker::GCInvokerImpl::StartIncrementalGarbageCollection(
    GarbageCollector::Config config) {
  if ((stack_support_ !=
       cppgc::Heap::StackSupport::kSupportsConservativeStackScan) &&
      (!platform_->GetForegroundTaskRunner() ||
       !platform_->GetForegroundTaskRunner()->NonNestableTasksEnabled())) {
    // In this configuration the GC finalization can only be triggered through
    // ForceGarbageCollectionSlow. If incremental GC is started, there is no
    // way to know how long it will remain enabled (and the write barrier with
    // it). For that reason, we do not support running incremental GCs in this
    // configuration.
    return;
  }
  // No need to postpone starting incremental GC since the stack is not scanned
  // until GC finalization.
  collector_->StartIncrementalGarbageCollection(config);
}

GCInvoker::GCInvoker(GarbageCollector* collector, cppgc::Platform* platform,
                     cppgc::Heap::StackSupport stack_support)
    : impl_(std::make_unique<GCInvoker::GCInvokerImpl>(collector, platform,
                                                       stack_support)) {}

GCInvoker::~GCInvoker() = default;

void GCInvoker::CollectGarbage(GarbageCollector::Config config) {
  impl_->CollectGarbage(config);
}

void GCInvoker::StartIncrementalGarbageCollection(
    GarbageCollector::Config config) {
  impl_->StartIncrementalGarbageCollection(config);
}

size_t GCInvoker::epoch() const { return impl_->epoch(); }

}  // namespace internal
}  // namespace cppgc
