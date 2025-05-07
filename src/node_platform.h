#ifndef SRC_NODE_PLATFORM_H_
#define SRC_NODE_PLATFORM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <functional>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "libplatform/libplatform.h"
#include "node.h"
#include "node_mutex.h"
#include "uv.h"

namespace node {

class NodePlatform;
class IsolateData;
class PerIsolatePlatformData;

template <typename, typename = void>
struct has_priority : std::false_type {};

template <typename T>
struct has_priority<T, std::void_t<decltype(std::declval<T>().priority)>>
    : std::true_type {};

template <class T>
class TaskQueue {
 public:
  // If the entry type has a priority memeber, order the priority queue by
  // that - higher priority first. Otherwise, maintain insertion order.
  struct EntryCompare {
    bool operator()(const std::unique_ptr<T>& a,
                    const std::unique_ptr<T>& b) const {
      if constexpr (has_priority<T>::value) {
        return a->priority < b->priority;
      } else {
        return false;
      }
    }
  };

  using PriorityQueue = std::priority_queue<std::unique_ptr<T>,
                                            std::vector<std::unique_ptr<T>>,
                                            EntryCompare>;
  class Locked {
   public:
    void Push(std::unique_ptr<T> task, bool outstanding = false);
    std::unique_ptr<T> Pop();
    std::unique_ptr<T> BlockingPop();
    void NotifyOfOutstandingCompletion();
    void BlockingDrain();
    void Stop();
    PriorityQueue PopAll();

   private:
    friend class TaskQueue;
    explicit Locked(TaskQueue* queue);

    TaskQueue* queue_;
    Mutex::ScopedLock lock_;
  };

  TaskQueue();
  ~TaskQueue() = default;

  Locked Lock() { return Locked(this); }

 private:
  Mutex lock_;
  ConditionVariable tasks_available_;
  ConditionVariable outstanding_tasks_drained_;
  int outstanding_tasks_;
  bool stopped_;
  PriorityQueue task_queue_;
};

struct TaskQueueEntry {
  std::unique_ptr<v8::Task> task;
  v8::TaskPriority priority;
  TaskQueueEntry(std::unique_ptr<v8::Task> t, v8::TaskPriority p)
      : task(std::move(t)), priority(p) {}
  inline bool is_outstanding() const {
    return priority == v8::TaskPriority::kUserBlocking;
  }
};

struct DelayedTask {
  std::unique_ptr<v8::Task> task;
  v8::TaskPriority priority;
  uv_timer_t timer;
  double timeout;
  std::shared_ptr<PerIsolatePlatformData> platform_data;
};

enum class PlatformDebugLogLevel {
  kNone = 0,
  kMinimal = 1,
  kVerbose = 2,
};

// This acts as the foreground task runner for a given Isolate.
class PerIsolatePlatformData
    : public IsolatePlatformDelegate,
      public v8::TaskRunner,
      public std::enable_shared_from_this<PerIsolatePlatformData> {
 public:
  PerIsolatePlatformData(
      v8::Isolate* isolate,
      uv_loop_t* loop,
      PlatformDebugLogLevel debug_log_level = PlatformDebugLogLevel::kNone);
  ~PerIsolatePlatformData() override;

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner() override;
  bool IdleTasksEnabled() override { return false; }

  // Non-nestable tasks are treated like regular tasks.
  bool NonNestableTasksEnabled() const override { return true; }
  bool NonNestableDelayedTasksEnabled() const override { return true; }

  void AddShutdownCallback(void (*callback)(void*), void* data);
  void Shutdown();

  // Returns true if work was dispatched or executed. New tasks that are
  // posted during flushing of the queue are postponed until the next
  // flushing.
  bool FlushForegroundTasksInternal();

  const uv_loop_t* event_loop() const { return loop_; }

 private:
  // v8::TaskRunner implementation.
  void PostTaskImpl(std::unique_ptr<v8::Task> task,
                    const v8::SourceLocation& location) override;
  void PostDelayedTaskImpl(std::unique_ptr<v8::Task> task,
                           double delay_in_seconds,
                           const v8::SourceLocation& location) override;
  void PostIdleTaskImpl(std::unique_ptr<v8::IdleTask> task,
                        const v8::SourceLocation& location) override;
  void PostNonNestableTaskImpl(std::unique_ptr<v8::Task> task,
                               const v8::SourceLocation& location) override;
  void PostNonNestableDelayedTaskImpl(
      std::unique_ptr<v8::Task> task,
      double delay_in_seconds,
      const v8::SourceLocation& location) override;

  void DeleteFromScheduledTasks(DelayedTask* task);
  void DecreaseHandleCount();

  static void FlushTasks(uv_async_t* handle);
  void RunForegroundTask(std::unique_ptr<v8::Task> task);
  static void RunForegroundTask(uv_timer_t* timer);

  uv_async_t* flush_tasks_ = nullptr;

  struct ShutdownCallback {
    void (*cb)(void*);
    void* data;
  };
  typedef std::vector<ShutdownCallback> ShutdownCbList;
  ShutdownCbList shutdown_callbacks_;
  // shared_ptr to self to keep this object alive during shutdown.
  std::shared_ptr<PerIsolatePlatformData> self_reference_;
  uint32_t uv_handle_count_ = 1;  // 1 = flush_tasks_

  v8::Isolate* const isolate_;
  uv_loop_t* const loop_;

  // When acquiring locks for both task queues, lock foreground_tasks_
  // first then foreground_delayed_tasks_ to avoid deadlocks.
  TaskQueue<TaskQueueEntry> foreground_tasks_;
  TaskQueue<DelayedTask> foreground_delayed_tasks_;

  // Use a custom deleter because libuv needs to close the handle first.
  typedef std::unique_ptr<DelayedTask, void (*)(DelayedTask*)>
      DelayedTaskPointer;
  std::vector<DelayedTaskPointer> scheduled_delayed_tasks_;
  PlatformDebugLogLevel debug_log_level_ = PlatformDebugLogLevel::kNone;
};

// This acts as the single worker thread task runner for all Isolates.
class WorkerThreadsTaskRunner {
 public:
  explicit WorkerThreadsTaskRunner(int thread_pool_size,
                                   PlatformDebugLogLevel debug_log_level);

  void PostTask(v8::TaskPriority priority,
                std::unique_ptr<v8::Task> task,
                const v8::SourceLocation& location);
  void PostDelayedTask(v8::TaskPriority priority,
                       std::unique_ptr<v8::Task> task,
                       const v8::SourceLocation& location,
                       double delay_in_seconds);

  void BlockingDrain();
  void Shutdown();

  int NumberOfWorkerThreads() const;

 private:
  // A queue shared by all threads. The consumers are the worker threads which
  // take tasks from it to run in PlatformWorkerThread(). The producers can be
  // any thread. Both the foreground thread and the worker threads can push
  // tasks into the queue via v8::Platform::PostTaskOnWorkerThread() which
  // eventually calls PostTask() on this class. When any thread calls
  // v8::Platform::PostDelayedTaskOnWorkerThread(), the DelayedTaskScheduler
  // thread will schedule a timer that pushes the delayed tasks back into this
  // queue when the timer expires.
  TaskQueue<TaskQueueEntry> pending_worker_tasks_;

  class DelayedTaskScheduler;
  std::unique_ptr<DelayedTaskScheduler> delayed_task_scheduler_;

  std::vector<std::unique_ptr<uv_thread_t>> threads_;
  PlatformDebugLogLevel debug_log_level_ = PlatformDebugLogLevel::kNone;
};

class NodePlatform : public MultiIsolatePlatform {
 public:
  NodePlatform(int thread_pool_size,
               v8::TracingController* tracing_controller,
               v8::PageAllocator* page_allocator = nullptr);
  ~NodePlatform() override;

  void DrainTasks(v8::Isolate* isolate) override;
  void Shutdown();

  // v8::Platform implementation.
  int NumberOfWorkerThreads() override;
  void PostTaskOnWorkerThreadImpl(v8::TaskPriority priority,
                                  std::unique_ptr<v8::Task> task,
                                  const v8::SourceLocation& location) override;
  void PostDelayedTaskOnWorkerThreadImpl(
      v8::TaskPriority priority,
      std::unique_ptr<v8::Task> task,
      double delay_in_seconds,
      const v8::SourceLocation& location) override;
  bool IdleTasksEnabled(v8::Isolate* isolate) override;
  double MonotonicallyIncreasingTime() override;
  double CurrentClockTimeMillis() override;
  v8::TracingController* GetTracingController() override;
  bool FlushForegroundTasks(v8::Isolate* isolate) override;
  std::unique_ptr<v8::JobHandle> CreateJobImpl(
      v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task,
      const v8::SourceLocation& location) override;

  void RegisterIsolate(v8::Isolate* isolate, uv_loop_t* loop) override;
  void RegisterIsolate(v8::Isolate* isolate,
                       IsolatePlatformDelegate* delegate) override;

  void UnregisterIsolate(v8::Isolate* isolate) override;
  void AddIsolateFinishedCallback(v8::Isolate* isolate,
                                  void (*callback)(void*),
                                  void* data) override;

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate, v8::TaskPriority priority) override;

  Platform::StackTracePrinter GetStackTracePrinter() override;
  v8::PageAllocator* GetPageAllocator() override;

 private:
  IsolatePlatformDelegate* ForIsolate(v8::Isolate* isolate);
  std::shared_ptr<PerIsolatePlatformData> ForNodeIsolate(v8::Isolate* isolate);

  Mutex per_isolate_mutex_;
  using DelegatePair = std::pair<IsolatePlatformDelegate*,
                                 std::shared_ptr<PerIsolatePlatformData>>;
  std::unordered_map<v8::Isolate*, DelegatePair> per_isolate_;

  v8::TracingController* tracing_controller_;
  v8::PageAllocator* page_allocator_;
  std::shared_ptr<WorkerThreadsTaskRunner> worker_thread_task_runner_;
  bool has_shut_down_ = false;
  PlatformDebugLogLevel debug_log_level_ = PlatformDebugLogLevel::kNone;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PLATFORM_H_
