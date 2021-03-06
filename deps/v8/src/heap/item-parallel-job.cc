// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/item-parallel-job.h"

#include "src/base/platform/semaphore.h"
#include "src/init/v8.h"
#include "src/logging/counters.h"

namespace v8 {
namespace internal {

ItemParallelJob::Task::Task(Isolate* isolate) : CancelableTask(isolate) {}

void ItemParallelJob::Task::SetupInternal(base::Semaphore* on_finish,
                                          std::vector<Item*>* items,
                                          size_t start_index) {
  on_finish_ = on_finish;
  items_ = items;

  if (start_index < items->size()) {
    cur_index_ = start_index;
  } else {
    items_considered_ = items_->size();
  }
}

void ItemParallelJob::Task::WillRunOnForeground() {
  runner_ = Runner::kForeground;
}

void ItemParallelJob::Task::RunInternal() {
  RunInParallel(runner_);
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

void ItemParallelJob::Run() {
  DCHECK_GT(tasks_.size(), 0);
  const size_t num_items = items_.size();
  const size_t num_tasks = tasks_.size();

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
                       "ItemParallelJob::Run", TRACE_EVENT_SCOPE_THREAD,
                       "num_tasks", static_cast<int>(num_tasks), "num_items",
                       static_cast<int>(num_items));

  // Some jobs have more tasks than items (when the items are mere coarse
  // grain tasks that generate work dynamically for a second phase which all
  // tasks participate in). Some jobs even have 0 items to preprocess but
  // still have multiple tasks.
  // TODO(gab): Figure out a cleaner scheme for this.
  const size_t num_tasks_processing_items = std::min(num_items, tasks_.size());

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

    task->SetupInternal(pending_tasks_, &items_, start_index);
    task_ids[i] = task->id();
    if (i > 0) {
      V8::GetCurrentPlatform()->CallBlockingTaskOnWorkerThread(std::move(task));
    } else {
      main_task = std::move(task);
    }
  }

  // Contribute on main thread.
  DCHECK(main_task);
  main_task->WillRunOnForeground();
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
