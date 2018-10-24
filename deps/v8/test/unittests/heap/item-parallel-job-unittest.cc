// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/item-parallel-job.h"

#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

class ItemParallelJobTest : public TestWithIsolate {
 public:
  ItemParallelJobTest() : parallel_job_semaphore_(0) {}

  base::Semaphore* parallel_job_semaphore() { return &parallel_job_semaphore_; }

 private:
  base::Semaphore parallel_job_semaphore_;
  DISALLOW_COPY_AND_ASSIGN(ItemParallelJobTest);
};

namespace {

class SimpleTask : public ItemParallelJob::Task {
 public:
  SimpleTask(Isolate* isolate, bool* did_run)
      : ItemParallelJob::Task(isolate), did_run_(did_run) {}

  void RunInParallel() override {
    ItemParallelJob::Item* item = nullptr;
    while ((item = GetItem<ItemParallelJob::Item>()) != nullptr) {
      item->MarkFinished();
    }
    *did_run_ = true;
  }

 private:
  bool* did_run_;
};

// A simple work item which sets |was_processed| to true, if non-null, when it
// is processed.
class SimpleItem : public ItemParallelJob::Item {
 public:
  explicit SimpleItem(bool* was_processed = nullptr)
      : ItemParallelJob::Item(), was_processed_(was_processed) {}
  void Process() {
    if (was_processed_) *was_processed_ = true;
  }

 private:
  bool* was_processed_;
};

class EagerTask : public ItemParallelJob::Task {
 public:
  explicit EagerTask(Isolate* isolate) : ItemParallelJob::Task(isolate) {}

  void RunInParallel() override {
    SimpleItem* item = nullptr;
    while ((item = GetItem<SimpleItem>()) != nullptr) {
      item->Process();
      item->MarkFinished();
    }
  }
};

// A OneShotBarrier is meant to be passed to |counter| users. Users should
// either Signal() or Wait() when done (based on whether they want to return
// immediately or wait until others are also done).
class OneShotBarrier {
 public:
  explicit OneShotBarrier(size_t counter) : counter_(counter) {
    DCHECK_GE(counter_, 0);
  }

  void Wait() {
    DCHECK_NE(counter_, 0);
    mutex_.Lock();
    counter_--;
    if (counter_ == 0) {
      condition_.NotifyAll();
    } else {
      while (counter_ > 0) {
        condition_.Wait(&mutex_);
      }
    }
    mutex_.Unlock();
  }

  void Signal() {
    mutex_.Lock();
    counter_--;
    if (counter_ == 0) {
      condition_.NotifyAll();
    }
    mutex_.Unlock();
  }

 private:
  base::Mutex mutex_;
  base::ConditionVariable condition_;
  size_t counter_;
};

// A task that only processes a single item. Signals |barrier| when done; if
// |wait_when_done|, will blocks until all other tasks have signaled |barrier|.
// If |did_process_an_item| is non-null, will set it to true if it does process
// an item. Otherwise, it will expect to get an item to process (and will report
// a failure if it doesn't).
class TaskProcessingOneItem : public ItemParallelJob::Task {
 public:
  TaskProcessingOneItem(Isolate* isolate, OneShotBarrier* barrier,
                        bool wait_when_done,
                        bool* did_process_an_item = nullptr)
      : ItemParallelJob::Task(isolate),
        barrier_(barrier),
        wait_when_done_(wait_when_done),
        did_process_an_item_(did_process_an_item) {}

  void RunInParallel() override {
    SimpleItem* item = GetItem<SimpleItem>();

    if (did_process_an_item_) {
      *did_process_an_item_ = item != nullptr;
    } else {
      EXPECT_NE(nullptr, item);
    }

    if (item) {
      item->Process();
      item->MarkFinished();
    }

    if (wait_when_done_) {
      barrier_->Wait();
    } else {
      barrier_->Signal();
    }
  }

 private:
  OneShotBarrier* barrier_;
  bool wait_when_done_;
  bool* did_process_an_item_;
};

class TaskForDifferentItems;

class BaseItem : public ItemParallelJob::Item {
 public:
  ~BaseItem() override = default;
  virtual void ProcessItem(TaskForDifferentItems* task) = 0;
};

class TaskForDifferentItems : public ItemParallelJob::Task {
 public:
  explicit TaskForDifferentItems(Isolate* isolate, bool* processed_a,
                                 bool* processed_b)
      : ItemParallelJob::Task(isolate),
        processed_a_(processed_a),
        processed_b_(processed_b) {}
  ~TaskForDifferentItems() override = default;

  void RunInParallel() override {
    BaseItem* item = nullptr;
    while ((item = GetItem<BaseItem>()) != nullptr) {
      item->ProcessItem(this);
      item->MarkFinished();
    }
  }

  void ProcessA() { *processed_a_ = true; }
  void ProcessB() { *processed_b_ = true; }

 private:
  bool* processed_a_;
  bool* processed_b_;
};

class ItemA : public BaseItem {
 public:
  ~ItemA() override = default;
  void ProcessItem(TaskForDifferentItems* task) override { task->ProcessA(); }
};

class ItemB : public BaseItem {
 public:
  ~ItemB() override = default;
  void ProcessItem(TaskForDifferentItems* task) override { task->ProcessB(); }
};

}  // namespace

// ItemParallelJob runs tasks even without work items (as requested tasks may be
// responsible for post-processing).
TEST_F(ItemParallelJobTest, SimpleTaskWithNoItemsRuns) {
  bool did_run = false;
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  job.AddTask(new SimpleTask(i_isolate(), &did_run));

  job.Run(i_isolate()->async_counters());
  EXPECT_TRUE(did_run);
}

TEST_F(ItemParallelJobTest, SimpleTaskWithSimpleItemRuns) {
  bool did_run = false;
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  job.AddTask(new SimpleTask(i_isolate(), &did_run));

  job.AddItem(new ItemParallelJob::Item);

  job.Run(i_isolate()->async_counters());
  EXPECT_TRUE(did_run);
}

TEST_F(ItemParallelJobTest, MoreTasksThanItems) {
  const int kNumTasks = 128;
  const int kNumItems = kNumTasks - 4;

  TaskProcessingOneItem* tasks[kNumTasks] = {};
  bool did_process_an_item[kNumTasks] = {};

  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());

  // The barrier ensures that all tasks run. But only the first kNumItems tasks
  // should be assigned an item to execute.
  OneShotBarrier barrier(kNumTasks);
  for (int i = 0; i < kNumTasks; i++) {
    // Block the main thread when done to prevent it from returning control to
    // the job (which could cancel tasks that have yet to be scheduled).
    const bool wait_when_done = i == 0;
    tasks[i] = new TaskProcessingOneItem(i_isolate(), &barrier, wait_when_done,
                                         &did_process_an_item[i]);
    job.AddTask(tasks[i]);
  }

  for (int i = 0; i < kNumItems; i++) {
    job.AddItem(new SimpleItem);
  }

  job.Run(i_isolate()->async_counters());

  for (int i = 0; i < kNumTasks; i++) {
    // Only the first kNumItems tasks should have been assigned a work item.
    EXPECT_EQ(i < kNumItems, did_process_an_item[i]);
  }
}

TEST_F(ItemParallelJobTest, SingleThreadProcessing) {
  const int kItems = 111;
  bool was_processed[kItems] = {};
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  job.AddTask(new EagerTask(i_isolate()));
  for (int i = 0; i < kItems; i++) {
    job.AddItem(new SimpleItem(&was_processed[i]));
  }
  job.Run(i_isolate()->async_counters());
  for (int i = 0; i < kItems; i++) {
    EXPECT_TRUE(was_processed[i]);
  }
}

TEST_F(ItemParallelJobTest, DistributeItemsMultipleTasks) {
  const int kItemsAndTasks = 256;
  bool was_processed[kItemsAndTasks] = {};
  OneShotBarrier barrier(kItemsAndTasks);
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  for (int i = 0; i < kItemsAndTasks; i++) {
    job.AddItem(new SimpleItem(&was_processed[i]));

    // Block the main thread when done to prevent it from returning control to
    // the job (which could cancel tasks that have yet to be scheduled).
    const bool wait_when_done = i == 0;
    job.AddTask(
        new TaskProcessingOneItem(i_isolate(), &barrier, wait_when_done));
  }
  job.Run(i_isolate()->async_counters());
  for (int i = 0; i < kItemsAndTasks; i++) {
    EXPECT_TRUE(was_processed[i]);
  }
}

TEST_F(ItemParallelJobTest, DifferentItems) {
  bool item_a = false;
  bool item_b = false;
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  job.AddItem(new ItemA());
  job.AddItem(new ItemB());
  job.AddTask(new TaskForDifferentItems(i_isolate(), &item_a, &item_b));
  job.Run(i_isolate()->async_counters());
  EXPECT_TRUE(item_a);
  EXPECT_TRUE(item_b);
}

}  // namespace internal
}  // namespace v8
