// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PAGE_PARALLEL_JOB_
#define V8_HEAP_PAGE_PARALLEL_JOB_

#include "src/allocation.h"
#include "src/cancelable-task.h"
#include "src/utils.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;

// This class manages background tasks that process set of pages in parallel.
// The JobTraits class needs to define:
// - PerPageData type - state associated with each page.
// - PerTaskData type - state associated with each task.
// - static void ProcessPageInParallel(Heap* heap,
//                                     PerTaskData task_data,
//                                     MemoryChunk* page,
//                                     PerPageData page_data)
template <typename JobTraits>
class PageParallelJob {
 public:
  // PageParallelJob cannot dynamically create a semaphore because of a bug in
  // glibc. See http://crbug.com/609249 and
  // https://sourceware.org/bugzilla/show_bug.cgi?id=12674.
  // The caller must provide a semaphore with value 0 and ensure that
  // the lifetime of the semaphore is the same as the lifetime of the Isolate.
  // It is guaranteed that the semaphore value will be 0 after Run() call.
  PageParallelJob(Heap* heap, CancelableTaskManager* cancelable_task_manager,
                  base::Semaphore* semaphore)
      : heap_(heap),
        cancelable_task_manager_(cancelable_task_manager),
        items_(nullptr),
        num_items_(0),
        num_tasks_(0),
        pending_tasks_(semaphore) {}

  ~PageParallelJob() {
    Item* item = items_;
    while (item != nullptr) {
      Item* next = item->next;
      delete item;
      item = next;
    }
  }

  void AddPage(MemoryChunk* chunk, typename JobTraits::PerPageData data) {
    Item* item = new Item(chunk, data, items_);
    items_ = item;
    ++num_items_;
  }

  int NumberOfPages() const { return num_items_; }

  // Returns the number of tasks that were spawned when running the job.
  int NumberOfTasks() const { return num_tasks_; }

  // Runs the given number of tasks in parallel and processes the previously
  // added pages. This function blocks until all tasks finish.
  // The callback takes the index of a task and returns data for that task.
  template <typename Callback>
  void Run(int num_tasks, Callback per_task_data_callback) {
    if (num_items_ == 0) return;
    DCHECK_GE(num_tasks, 1);
    CancelableTaskManager::Id task_ids[kMaxNumberOfTasks];
    const int max_num_tasks = Min(
        kMaxNumberOfTasks,
        static_cast<int>(
            V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads()));
    num_tasks_ = Max(1, Min(num_tasks, max_num_tasks));
    int items_per_task = (num_items_ + num_tasks_ - 1) / num_tasks_;
    int start_index = 0;
    Task* main_task = nullptr;
    for (int i = 0; i < num_tasks_; i++, start_index += items_per_task) {
      if (start_index >= num_items_) {
        start_index -= num_items_;
      }
      Task* task = new Task(heap_, items_, num_items_, start_index,
                            pending_tasks_, per_task_data_callback(i));
      task_ids[i] = task->id();
      if (i > 0) {
        V8::GetCurrentPlatform()->CallOnBackgroundThread(
            task, v8::Platform::kShortRunningTask);
      } else {
        main_task = task;
      }
    }
    // Contribute on main thread.
    main_task->Run();
    delete main_task;
    // Wait for background tasks.
    for (int i = 0; i < num_tasks_; i++) {
      if (cancelable_task_manager_->TryAbort(task_ids[i]) !=
          CancelableTaskManager::kTaskAborted) {
        pending_tasks_->Wait();
      }
    }
  }

 private:
  static const int kMaxNumberOfTasks = 32;

  enum ProcessingState { kAvailable, kProcessing, kFinished };

  struct Item : public Malloced {
    Item(MemoryChunk* chunk, typename JobTraits::PerPageData data, Item* next)
        : chunk(chunk), state(kAvailable), data(data), next(next) {}
    MemoryChunk* chunk;
    base::AtomicValue<ProcessingState> state;
    typename JobTraits::PerPageData data;
    Item* next;
  };

  class Task : public CancelableTask {
   public:
    Task(Heap* heap, Item* items, int num_items, int start_index,
         base::Semaphore* on_finish, typename JobTraits::PerTaskData data)
        : CancelableTask(heap->isolate()),
          heap_(heap),
          items_(items),
          num_items_(num_items),
          start_index_(start_index),
          on_finish_(on_finish),
          data_(data) {}

    virtual ~Task() {}

   private:
    // v8::internal::CancelableTask overrides.
    void RunInternal() override {
      // Each task starts at a different index to improve parallelization.
      Item* current = items_;
      int skip = start_index_;
      while (skip-- > 0) {
        current = current->next;
      }
      for (int i = 0; i < num_items_; i++) {
        if (current->state.TrySetValue(kAvailable, kProcessing)) {
          JobTraits::ProcessPageInParallel(heap_, data_, current->chunk,
                                           current->data);
          current->state.SetValue(kFinished);
        }
        current = current->next;
        // Wrap around if needed.
        if (current == nullptr) {
          current = items_;
        }
      }
      on_finish_->Signal();
    }

    Heap* heap_;
    Item* items_;
    int num_items_;
    int start_index_;
    base::Semaphore* on_finish_;
    typename JobTraits::PerTaskData data_;
    DISALLOW_COPY_AND_ASSIGN(Task);
  };

  Heap* heap_;
  CancelableTaskManager* cancelable_task_manager_;
  Item* items_;
  int num_items_;
  int num_tasks_;
  base::Semaphore* pending_tasks_;
  DISALLOW_COPY_AND_ASSIGN(PageParallelJob);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PAGE_PARALLEL_JOB_
