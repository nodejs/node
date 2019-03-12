// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/item-parallel-job.h"

#include "src/base/platform/semaphore.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

ItemParallelJob::Task::Task(Isolate* isolate) : CancelableTask(isolate) {}

ItemParallelJob::Task::~Task() {
  // The histogram is reset in RunInternal(). If it's still around it means
  // this task was cancelled before being scheduled.
  if (gc_parallel_task_latency_histogram_)
    gc_parallel_task_latency_histogram_->RecordAbandon();
}

void ItemParallelJob::Task::SetupInternal(
    base::Semaphore* on_finish, std::vector<Item*>* items, size_t start_index,
    base::Optional<AsyncTimedHistogram> gc_parallel_task_latency_histogram) {
  on_finish_ = on_finish;
  items_ = items;

  if (start_index < items->size()) {
    cur_index_ = start_index;
  } else {
    items_considered_ = items_->size();
  }

  gc_parallel_task_latency_histogram_ =
      std::move(gc_parallel_task_latency_histogram);
}

void ItemParallelJob::Task::RunInternal() {
  if (gc_parallel_task_latency_histogram_) {
    gc_parallel_task_latency_histogram_->RecordDone();
    gc_parallel_task_latency_histogram_.reset();
  }

  RunInParallel();
  on_finish_->Signal();
}

ItemParallelJob::ItemParallelJob(CancelableTaskManager* cancelable_task_manager,
                                 base::Semaphore* pending_tasks)
    : cancelable_task_manager_(cancelable_task_manager),
      pending_tasks_(pending_tasks) {}

ItemParallelJob::~ItemParallelJob() {
  for (size_t i = 0; i < items_.size(); i++) {
    Item* item = items_[i];
    CHECK(item->IsFinished());
    delete item;
  }
}

void ItemParallelJob::Run(const std::shared_ptr<Counters>& async_counters) {
  DCHECK_GT(tasks_.size(), 0);
  const size_t num_items = items_.size();
  const size_t num_tasks = tasks_.size();

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                       "ItemParallelJob::Run", TRACE_EVENT_SCOPE_THREAD,
                       "num_tasks", static_cast<int>(num_tasks), "num_items",
                       static_cast<int>(num_items));

  AsyncTimedHistogram gc_parallel_task_latency_histogram(
      async_counters->gc_parallel_task_latency(), async_counters);

  // Some jobs have more tasks than items (when the items are mere coarse
  // grain tasks that generate work dynamically for a second phase which all
  // tasks participate in). Some jobs even have 0 items to preprocess but
  // still have multiple tasks.
  // TODO(gab): Figure out a cleaner scheme for this.
  const size_t num_tasks_processing_items = Min(num_items, tasks_.size());

  // In the event of an uneven workload, distribute an extra item to the first
  // |items_remainder| tasks.
  const size_t items_remainder = num_tasks_processing_items > 0
                                     ? num_items % num_tasks_processing_items
                                     : 0;
  // Base |items_per_task|, will be bumped by 1 for the first
  // |items_remainder| tasks.
  const size_t items_per_task = num_tasks_processing_items > 0
                                    ? num_items / num_tasks_processing_items
                                    : 0;
  CancelableTaskManager::Id* task_ids =
      new CancelableTaskManager::Id[num_tasks];
  std::unique_ptr<Task> main_task;
  for (size_t i = 0, start_index = 0; i < num_tasks;
       i++, start_index += items_per_task + (i < items_remainder ? 1 : 0)) {
    auto task = std::move(tasks_[i]);
    DCHECK(task);

    // By definition there are less |items_remainder| to distribute then
    // there are tasks processing items so this cannot overflow while we are
    // assigning work items.
    DCHECK_IMPLIES(start_index >= num_items, i >= num_tasks_processing_items);

    task->SetupInternal(pending_tasks_, &items_, start_index,
                        i > 0 ? gc_parallel_task_latency_histogram
                              : base::Optional<AsyncTimedHistogram>());
    task_ids[i] = task->id();
    if (i > 0) {
      V8::GetCurrentPlatform()->CallBlockingTaskOnWorkerThread(std::move(task));
    } else {
      main_task = std::move(task);
    }
  }

  // Contribute on main thread.
  DCHECK(main_task);
  main_task->Run();

  // Wait for background tasks.
  for (size_t i = 0; i < num_tasks; i++) {
    if (cancelable_task_manager_->TryAbort(task_ids[i]) !=
        TryAbortResult::kTaskAborted) {
      pending_tasks_->Wait();
    }
  }
  delete[] task_ids;
}

}  // namespace internal
}  // namespace v8
