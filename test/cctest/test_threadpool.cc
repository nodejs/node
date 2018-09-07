#include "node_internals.h"
#include "node_threadpool.h"
#include "libplatform/libplatform.h"

#include <string>
#include "gtest/gtest.h"
#include "node_test_fixture.h"

#include <atomic>
#include <stdlib.h>

using node::threadpool::Task;
using node::threadpool::TaskDetails;
using node::threadpool::TaskQueue;
using node::threadpool::Worker;
using node::threadpool::Threadpool;

// Thread-safe counters
static std::atomic<int> testTaskRunCount(0);
static std::atomic<int> testTaskDestroyedCount(0);

// TODO(davisjam): Do I need this?
class ThreadpoolTest : public NodeTestFixture {
 private:
  virtual void TearDown() {
    NodeTestFixture::TearDown();
  }
};

// Helper so we have a type of Task
class TestTask : public node::threadpool::Task {
 public:
  TestTask() {
    details_.type = TaskDetails::CPU_FAST;
    details_.priority = -1;
    details_.cancelable = false;
  }
  ~TestTask() {
    testTaskDestroyedCount++;
  }

  void Run() {
    testTaskRunCount++;
  }
};

TEST_F(ThreadpoolTest, TaskQueueEndToEnd) {
  int nTasks = 100;
  TaskQueue tq;

  // Reset globals
  testTaskRunCount = 0;
  testTaskDestroyedCount = 0;

  // Push
  EXPECT_EQ(tq.Length(), 0);
  for (int i = 0; i < nTasks; i++) {
    EXPECT_EQ(tq.Push(std::unique_ptr<TestTask>(new TestTask())),
              true);
  }
  EXPECT_EQ(tq.Length(), nTasks);

  // Successful Pop, BlockingPop
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
  tq.Stop();
  EXPECT_EQ(tq.Push(std::unique_ptr<TestTask>(new TestTask())), false);
}

TEST_F(ThreadpoolTest, WorkersWorkWithTaskQueue) {
  int nTasks = 100;
  TaskQueue tq;
  Worker w;

  // Reset globals
  testTaskRunCount = 0;
  testTaskDestroyedCount = 0;

  // Push
  EXPECT_EQ(tq.Length(), 0);
  for (int i = 0; i < nTasks; i++) {
    EXPECT_EQ(tq.Push(std::unique_ptr<TestTask>(new TestTask())),
              true);
  }
  // Worker hasn't started yet, so tq should be "full".
  EXPECT_EQ(tq.Length(), nTasks);

  // Once we start the worker, it should empty tq.
  w.Start(&tq);

  tq.Stop();  // Signal Worker that we're done
  w.Join();   // Wait for Worker to finish
  EXPECT_EQ(tq.Length(), 0);

  // And it should have run and destroyed every Task.
  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, ThreadpoolEndToEnd) {
  int nTasks = 100;

  {
    std::unique_ptr<Threadpool> tp(new Threadpool(-1));

    // Reset globals
    testTaskRunCount = 0;
    testTaskDestroyedCount = 0;

    EXPECT_GT(tp->NWorkers(), 0);

    // Push
    EXPECT_EQ(tp->QueueLength(), 0);
    for (int i = 0; i < nTasks; i++) {
      tp->Post(std::unique_ptr<TestTask>(new TestTask()));
    }
  }
  // tp leaves scope. In destructor it drains the queue.

  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, ThreadpoolBlockingDrain) {
  // Enough that we will probably have to wait for them to finish.
  int nTasks = 10000;

  std::unique_ptr<Threadpool> tp(new Threadpool(-1));

  // Reset globals
  testTaskRunCount = 0;
  testTaskDestroyedCount = 0;

  // Push
  EXPECT_EQ(tp->QueueLength(), 0);
  for (int i = 0; i < nTasks; i++) {
    tp->Post(std::unique_ptr<TestTask>(new TestTask()));
  }

  tp->BlockingDrain();
  EXPECT_EQ(testTaskRunCount, nTasks);
  EXPECT_EQ(testTaskDestroyedCount, nTasks);
}

TEST_F(ThreadpoolTest, ThreadpoolSize) {
  char* old = getenv("UV_THREADPOOL_SIZE");

  int tp_size = 17;
  char tp_size_str[4];
  snprintf(tp_size_str, sizeof(tp_size_str), "%d", tp_size);

  setenv("UV_THREADPOOL_SIZE", tp_size_str, 1);
  std::unique_ptr<Threadpool> tp(new Threadpool(-1));
  EXPECT_EQ(tp->NWorkers(), tp_size);

  // Restore previous value of UV_THREADPOOL_SIZE.
  if (old) {
    setenv("UV_THREADPOOL_SIZE", old, 1);
  } else {
    unsetenv("UV_THREADPOOL_SIZE");
  }
}
