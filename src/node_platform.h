#ifndef SRC_NODE_PLATFORM_H_
#define SRC_NODE_PLATFORM_H_

#include <queue>
#include <unordered_map>
#include <vector>
#include <functional>

#include "libplatform/libplatform.h"
#include "node.h"
#include "node_mutex.h"
#include "uv.h"

namespace node {

class NodePlatform;
class IsolateData;
class PerIsolatePlatformData;

template <class T>
class TaskQueue {
 public:
  TaskQueue();
  ~TaskQueue() {}

  void Push(std::unique_ptr<T> task);
  std::unique_ptr<T> Pop();
  std::unique_ptr<T> BlockingPop();
  void NotifyOfCompletion();
  void BlockingDrain();
  void Stop();

 private:
  Mutex lock_;
  ConditionVariable tasks_available_;
  ConditionVariable tasks_drained_;
  int outstanding_tasks_;
  bool stopped_;
  std::queue<std::unique_ptr<T>> task_queue_;
};

struct DelayedTask {
  std::unique_ptr<v8::Task> task;
  uv_timer_t timer;
  double timeout;
  PerIsolatePlatformData* platform_data;
};

class PerIsolatePlatformData {
 public:
  PerIsolatePlatformData(v8::Isolate* isolate, uv_loop_t* loop);
  ~PerIsolatePlatformData();

  void CallOnForegroundThread(std::unique_ptr<v8::Task> task);
  void CallDelayedOnForegroundThread(std::unique_ptr<v8::Task> task,
    double delay_in_seconds);

  void Shutdown();

  void ref();
  int unref();

  // Returns true iff work was dispatched or executed.
  bool FlushForegroundTasksInternal();
  void CancelPendingDelayedTasks();

 private:
  void DeleteFromScheduledTasks(DelayedTask* task);

  static void FlushTasks(uv_async_t* handle);
  static void RunForegroundTask(std::unique_ptr<v8::Task> task);
  static void RunForegroundTask(uv_timer_t* timer);

  int ref_count_ = 1;
  v8::Isolate* isolate_;
  uv_loop_t* const loop_;
  uv_async_t* flush_tasks_ = nullptr;
  TaskQueue<v8::Task> foreground_tasks_;
  TaskQueue<DelayedTask> foreground_delayed_tasks_;

  // Use a custom deleter because libuv needs to close the handle first.
  typedef std::unique_ptr<DelayedTask, std::function<void(DelayedTask*)>>
      DelayedTaskPointer;
  std::vector<DelayedTaskPointer> scheduled_delayed_tasks_;
};

class NodePlatform : public MultiIsolatePlatform {
 public:
  NodePlatform(int thread_pool_size, v8::TracingController* tracing_controller);
  virtual ~NodePlatform() {}

  void DrainBackgroundTasks(v8::Isolate* isolate) override;
  void CancelPendingDelayedTasks(v8::Isolate* isolate) override;
  void Shutdown();

  // v8::Platform implementation.
  size_t NumberOfAvailableBackgroundThreads() override;
  void CallOnBackgroundThread(v8::Task* task,
                              ExpectedRuntime expected_runtime) override;
  void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) override;
  void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task,
                                     double delay_in_seconds) override;
  bool IdleTasksEnabled(v8::Isolate* isolate) override;
  double MonotonicallyIncreasingTime() override;
  v8::TracingController* GetTracingController() override;

  void FlushForegroundTasks(v8::Isolate* isolate);

  void RegisterIsolate(IsolateData* isolate_data, uv_loop_t* loop) override;
  void UnregisterIsolate(IsolateData* isolate_data) override;

 private:
  PerIsolatePlatformData* ForIsolate(v8::Isolate* isolate);

  Mutex per_isolate_mutex_;
  std::unordered_map<v8::Isolate*, PerIsolatePlatformData*> per_isolate_;
  TaskQueue<v8::Task> background_tasks_;
  std::vector<std::unique_ptr<uv_thread_t>> threads_;

  std::unique_ptr<v8::TracingController> tracing_controller_;
};

}  // namespace node

#endif  // SRC_NODE_PLATFORM_H_
