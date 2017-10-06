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

class EmptyTask : public ItemParallelJob::Task {
 public:
  explicit EmptyTask(Isolate* isolate, bool* did_run)
      : ItemParallelJob::Task(isolate), did_run_(did_run) {}

  void RunInParallel() override { *did_run_ = true; }

 private:
  bool* did_run_;
};

class SimpleItem : public ItemParallelJob::Item {
 public:
  explicit SimpleItem(bool* was_processed)
      : ItemParallelJob::Item(), was_processed_(was_processed) {}
  void Process() { *was_processed_ = true; }

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

 private:
  base::Mutex mutex_;
  base::ConditionVariable condition_;
  size_t counter_;
};

class TaskProcessingOneItem : public ItemParallelJob::Task {
 public:
  explicit TaskProcessingOneItem(Isolate* isolate, OneShotBarrier* barrier)
      : ItemParallelJob::Task(isolate), barrier_(barrier) {}

  void RunInParallel() override {
    SimpleItem* item = GetItem<SimpleItem>();
    EXPECT_NE(nullptr, item);
    item->Process();
    item->MarkFinished();
    // Avoid canceling the remaining tasks with a simple barrier.
    barrier_->Wait();
  }

 private:
  OneShotBarrier* barrier_;
};

class TaskForDifferentItems;

class BaseItem : public ItemParallelJob::Item {
 public:
  virtual ~BaseItem() {}
  virtual void ProcessItem(TaskForDifferentItems* task) = 0;
};

class TaskForDifferentItems : public ItemParallelJob::Task {
 public:
  explicit TaskForDifferentItems(Isolate* isolate, bool* processed_a,
                                 bool* processed_b)
      : ItemParallelJob::Task(isolate),
        processed_a_(processed_a),
        processed_b_(processed_b) {}
  virtual ~TaskForDifferentItems() {}

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
  virtual ~ItemA() {}
  void ProcessItem(TaskForDifferentItems* task) override { task->ProcessA(); }
};

class ItemB : public BaseItem {
 public:
  virtual ~ItemB() {}
  void ProcessItem(TaskForDifferentItems* task) override { task->ProcessB(); }
};

}  // namespace

TEST_F(ItemParallelJobTest, EmptyTaskRuns) {
  bool did_run = false;
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  job.AddTask(new EmptyTask(i_isolate(), &did_run));
  job.Run();
  EXPECT_TRUE(did_run);
}

TEST_F(ItemParallelJobTest, FinishAllItems) {
  const int kItems = 111;
  bool was_processed[kItems];
  for (int i = 0; i < kItems; i++) {
    was_processed[i] = false;
  }
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  job.AddTask(new EagerTask(i_isolate()));
  for (int i = 0; i < kItems; i++) {
    job.AddItem(new SimpleItem(&was_processed[i]));
  }
  job.Run();
  for (int i = 0; i < kItems; i++) {
    EXPECT_TRUE(was_processed[i]);
  }
}

TEST_F(ItemParallelJobTest, DistributeItemsMultipleTasks) {
  const int kItemsAndTasks = 2;  // Main thread + additional task.
  bool was_processed[kItemsAndTasks];
  OneShotBarrier barrier(kItemsAndTasks);
  for (int i = 0; i < kItemsAndTasks; i++) {
    was_processed[i] = false;
  }
  ItemParallelJob job(i_isolate()->cancelable_task_manager(),
                      parallel_job_semaphore());
  for (int i = 0; i < kItemsAndTasks; i++) {
    job.AddItem(new SimpleItem(&was_processed[i]));
    job.AddTask(new TaskProcessingOneItem(i_isolate(), &barrier));
  }
  job.Run();
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
  job.Run();
  EXPECT_TRUE(item_a);
  EXPECT_TRUE(item_b);
}

}  // namespace internal
}  // namespace v8
