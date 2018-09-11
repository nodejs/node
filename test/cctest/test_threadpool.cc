#include "node_internals.h"
#include "node_threadpool.h"

#include "node.h"
#include "node_platform.h"
#include "node_internals.h"
#include "env.h"
#include "v8.h"
#include "node_test_fixture.h"

#include "gtest/gtest.h"

#include <string>
#include <atomic>

using node::threadpool::TaskDetails;
using node::threadpool::TaskState;
using node::threadpool::Task;
using node::threadpool::TaskQueue;
using node::threadpool::Worker;
using node::threadpool::WorkerGroup;
using node::threadpool::Threadpool;
using node::threadpool::NodeThreadpool;

// Thread-safe counters
static std::atomic<int> testTaskRunCount(0);
static std::atomic<int> testTaskDestroyedCount(0);

// Tests of internals: Worker, Task, TaskQueue, Threadpool.
//
// NB The node instance defined by NodeTestFixture does not use our Threadpool.
// So we can't easily test LibuvExecutor etc.
// Rely on higher-level tests for that.
class ThreadpoolTest : public NodeTestFixture { };

// Helpers so we have different Task types.
class FastTestTask : public node::threadpool::Task {
 public:
  FastTestTask() {
    details_.origin = TaskDetails::USER;
    details_.type = TaskDetails::CPU;
    details_.size = TaskDetails::SMALL;
    details_.priority = -1;
    details_.cancelable = false;
  }
  ~FastTestTask() {
    testTaskDestroyedCount++;
  }

  void Run() {
    testTaskRunCount++;
  }
};

class SlowTestTask : public node::threadpool::Task {
 public:
  SlowTestTask() {
    details_.origin = TaskDetails::USER;
    details_.type = TaskDetails::CPU;
    details_.size = TaskDetails::LARGE;
    details_.priority = -1;
    details_.cancelable = false;
  }
  ~SlowTestTask() {
    testTaskDestroyedCount++;
  }

  void Run() {
    testTaskRunCount++;
    for (int i = 0; i < 10000000; i++);
  }
};

TEST_F(ThreadpoolTest, TaskQueueEndToEnd) {
  int nTasks = 100;
  TaskQueue tq;

  // Reset globals
  testTaskRunCount = 0;
  testTaskDestroyedCount = 0;

  // Push
  fprintf(stderr, "TaskQueueEndToEnd: Push\n");
  EXPECT_EQ(tq.Length(), 0);
  for (int i = 0; i < nTasks; i++) {
    auto task_state = std::make_shared<TaskState>();
    auto task = std::unique_ptr<FastTestTask>(new FastTestTask());
    task->SetTaskState(task_state);
    EXPECT_EQ(tq.Push(std::move(task)), true);
  }
  EXPECT_EQ(tq.Length(), nTasks);

  // Successful Pop, BlockingPop
  fprintf(stderr, "TaskQueueEndToEnd: Pop\n");
  for (int i = 0; i < nTasks; i++) {
    std::unique_ptr<Task> task;
    if (i % 2)
      task = tq.Pop();
    else
      task = tq.BlockingPop();
    EXPECT_NE(task.get(), nullptr);
    EXPECT_EQ(tq.Length(), nTasks - (i + 1));
  }

  // Pop fails when queue is empty
  std::unique_ptr<Task> task = tq.Pop();  // Non-blocking
  EXPECT_EQ(task.get(), nullptr);
  EXPECT_EQ(tq.Length(), 0);

  // Stop works
  fprintf(stderr, "TaskQueueEndToEnd: Push after Stop\n");
  tq.Stop();
  EXPECT_EQ(tq.Push(std::unique_ptr<FastTestTask>(new FastTestTask())), false);
}

TEST_F(ThreadpoolTest, WorkersWorkWithTaskQueue) {
  int nTasks = 100;
  std::shared_ptr<TaskQueue> tq = std::make_shared<TaskQueue>();
  Worker w(tq);

  // Reset globals
  testTaskRunCount = 0;
  testTaskDestroyedCount = 0;

  // Push
  EXPECT_EQ(tq->Length(), 0);
  for (int i = 0; i < nTasks; i++) {
    auto task_state = std::make_shared<TaskState>();
    auto task = std::unique_ptr<FastTestTask>(new FastTestTask());
    task->SetTaskState(task_state);
    EXPECT_EQ(tq->Push(std::move(task)), true);
  }
  // Worker hasn't started yet, so tq should be at high water mark.
  EXPECT_EQ(tq->Length(), nTasks);

  // Once we start the worker, it should empty tq.
  w.Start();

  tq->Stop();  // Signal Worker that we're done
  w.Join();   // Wait for Worker to finish
  EXPECT_EQ(tq->Length(), 0);

  // And it should have run and destroyed every Task.
  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, WorkerGroupWorksWithTaskQueue) {
  int nTasks = 100;
  std::shared_ptr<TaskQueue> tq = std::make_shared<TaskQueue>();

  // Reset globals
  testTaskRunCount = 0;
  testTaskDestroyedCount = 0;

  // Push
  EXPECT_EQ(tq->Length(), 0);
  for (int i = 0; i < nTasks; i++) {
    auto task_state = std::make_shared<TaskState>();
    auto task = std::unique_ptr<FastTestTask>(new FastTestTask());
    task->SetTaskState(task_state);
    EXPECT_EQ(tq->Push(std::move(task)), true);
  }
  // Worker hasn't started yet, so tq should be at high water mark.
  EXPECT_EQ(tq->Length(), nTasks);

  {
    // Once we create the WorkerGroup, it should empty tq.
    WorkerGroup wg(4, tq);
    tq->Stop();
  } // wg leaves scope
  // wg destructor should drain tq
  EXPECT_EQ(tq->Length(), 0);

  // And it should have run and destroyed every Task.
  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, ThreadpoolEndToEnd) {
  int nTasks = 100;

  {
    std::unique_ptr<Threadpool> tp(new Threadpool(10));

    // Reset globals
    testTaskRunCount = 0;
    testTaskDestroyedCount = 0;

    EXPECT_GT(tp->NWorkers(), 0);

    // Push
    EXPECT_EQ(tp->QueueLength(), 0);
    for (int i = 0; i < nTasks; i++) {
      tp->Post(std::unique_ptr<FastTestTask>(new FastTestTask()));
    }
  }
  // tp leaves scope. In destructor it drains the queue.

  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, ThreadpoolBlockingDrain) {
  // Enough that we will probably have to wait for them to finish.
  int nTasks = 10000;

  std::unique_ptr<Threadpool> tp(new Threadpool(10));

  // Reset globals
  testTaskRunCount = 0;
  testTaskDestroyedCount = 0;

  // Push
  EXPECT_EQ(tp->QueueLength(), 0);
  for (int i = 0; i < nTasks; i++) {
    tp->Post(std::unique_ptr<FastTestTask>(new FastTestTask()));
  }

  tp->BlockingDrain();
  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, ThreadpoolCancel) {
  int nTasks = 10000;
  int nCancelled = 0;

  {
    std::shared_ptr<TaskState> states[nTasks];
    std::unique_ptr<Threadpool> tp(new Threadpool(1));

    // Reset globals
    testTaskRunCount = 0;
    testTaskDestroyedCount = 0;

    EXPECT_GT(tp->NWorkers(), 0);

    // Push
    EXPECT_EQ(tp->QueueLength(), 0);
    for (int i = 0; i < nTasks; i++) {
      states[i] = tp->Post(std::unique_ptr<SlowTestTask>(new SlowTestTask()));
    }

    // Cancel
    for (int i = nTasks - 1; i >= 0; i--) {
      if (states[i]->Cancel()) {
        nCancelled++;
      }
    }
    fprintf(stderr, "DEBUG: cancelled %d\n", nCancelled);
  }
  // tp leaves scope. In destructor it drains the queue.

  // All Tasks, cancelled or not, should be destroyed.
  EXPECT_EQ(testTaskDestroyedCount, nTasks);

  // 0 <= testTaskRunCount <= nTasks.
  // We may have successfully cancelled all of them.
  EXPECT_GE(testTaskRunCount, 0);
  EXPECT_LE(testTaskRunCount, nTasks);

  // We used SlowTestTasks so we should have managed to cancel at least 1.
  EXPECT_GT(nCancelled, 0);
}

TEST_F(ThreadpoolTest, NodeThreadpoolEndToEnd) {
  int nTasks = 100;

  {
    std::unique_ptr<NodeThreadpool> tp(new NodeThreadpool(10));

    // Reset globals
    testTaskRunCount = 0;
    testTaskDestroyedCount = 0;

    EXPECT_GT(tp->NWorkers(), 0);

    // Push
    EXPECT_EQ(tp->QueueLength(), 0);
    for (int i = 0; i < nTasks; i++) {
      tp->Post(std::unique_ptr<FastTestTask>(new FastTestTask()));
    }
  }
  // tp leaves scope. In destructor it drains the queue.

  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, NodeThreadpoolSize) {
  char* old = getenv("UV_THREADPOOL_SIZE");

  int tp_size = 17;
  char tp_size_str[4];
  snprintf(tp_size_str, sizeof(tp_size_str), "%d", tp_size);

  setenv("UV_THREADPOOL_SIZE", tp_size_str, 1);
  std::unique_ptr<NodeThreadpool> tp(new NodeThreadpool(-1));
  EXPECT_EQ(tp->NWorkers(), tp_size);

  // Restore previous value of UV_THREADPOOL_SIZE.
  if (old) {
    setenv("UV_THREADPOOL_SIZE", old, 1);
  } else {
    unsetenv("UV_THREADPOOL_SIZE");
  }
}
