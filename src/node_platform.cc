#include "node_platform.h"
#include "node_internals.h"

#include "env-inl.h"
#include "debug_utils-inl.h"
#include <algorithm>  // find_if(), find(), move()
#include <cmath>  // llround()
#include <memory>  // unique_ptr(), shared_ptr(), make_shared()

namespace node {

using v8::Isolate;
using v8::Object;
using v8::Platform;
using v8::Task;
using v8::TaskPriority;

namespace {

struct PlatformWorkerData {
  TaskQueue<TaskQueueEntry>* task_queue;
  Mutex* platform_workers_mutex;
  ConditionVariable* platform_workers_ready;
  int* pending_platform_workers;
  int id;
  PlatformDebugLogLevel debug_log_level;
};

const char* GetTaskPriorityName(TaskPriority priority) {
  switch (priority) {
    case TaskPriority::kUserBlocking:
      return "UserBlocking";
    case TaskPriority::kUserVisible:
      return "UserVisible";
    case TaskPriority::kBestEffort:
      return "BestEffort";
    default:
      return "Unknown";
  }
}

static void PrintSourceLocation(const v8::SourceLocation& location) {
  auto loc = location.ToString();
  if (!loc.empty()) {
    fprintf(stderr, " %s\n", loc.c_str());
  } else {
    fprintf(stderr, " <no location>\n");
  }
}

static void PlatformWorkerThread(void* data) {
  uv_thread_setname("V8Worker");
  std::unique_ptr<PlatformWorkerData>
      worker_data(static_cast<PlatformWorkerData*>(data));

  TaskQueue<TaskQueueEntry>* pending_worker_tasks = worker_data->task_queue;
  TRACE_EVENT_METADATA1("__metadata", "thread_name", "name",
                        "PlatformWorkerThread");

  // Notify the main thread that the platform worker is ready.
  {
    Mutex::ScopedLock lock(*worker_data->platform_workers_mutex);
    (*worker_data->pending_platform_workers)--;
    worker_data->platform_workers_ready->Signal(lock);
  }

  bool debug_log_enabled =
      worker_data->debug_log_level != PlatformDebugLogLevel::kNone;
  int id = worker_data->id;
  while (std::unique_ptr<TaskQueueEntry> entry =
             pending_worker_tasks->Lock().BlockingPop()) {
    if (debug_log_enabled) {
      fprintf(stderr,
              "\nPlatformWorkerThread %d running task %p %s\n",
              id,
              entry->task.get(),
              GetTaskPriorityName(entry->priority));
      fflush(stderr);
    }
    entry->task->Run();
    pending_worker_tasks->Lock().NotifyOfCompletion();
  }
}

static int GetActualThreadPoolSize(int thread_pool_size) {
  if (thread_pool_size < 1) {
    thread_pool_size = uv_available_parallelism() - 1;
  }
  return std::max(thread_pool_size, 1);
}

}  // namespace

class WorkerThreadsTaskRunner::DelayedTaskScheduler {
 public:
  explicit DelayedTaskScheduler(TaskQueue<TaskQueueEntry>* tasks)
      : pending_worker_tasks_(tasks) {}

  std::unique_ptr<uv_thread_t> Start() {
    auto start_thread = [](void* data) {
      static_cast<DelayedTaskScheduler*>(data)->Run();
    };
    std::unique_ptr<uv_thread_t> t { new uv_thread_t() };
    uv_sem_init(&ready_, 0);
    CHECK_EQ(0, uv_thread_create(t.get(), start_thread, this));
    uv_sem_wait(&ready_);
    uv_sem_destroy(&ready_);
    return t;
  }

  void PostDelayedTask(v8::TaskPriority priority,
                       std::unique_ptr<Task> task,
                       double delay_in_seconds) {
    auto locked = tasks_.Lock();

    auto entry = std::make_unique<TaskQueueEntry>(std::move(task), priority);
    auto delayed = std::make_unique<ScheduleTask>(
        this, std::move(entry), delay_in_seconds);

    // The delayed task scheuler is on is own thread with its own loop that
    // runs the timers for the scheduled tasks to pop the original task back
    // into the the worker task queue. This first pushes the tasks that
    // schedules the timers into the local task queue that will be flushed
    // by the local event loop.
    locked.Push(std::move(delayed));
    uv_async_send(&flush_tasks_);
  }

  void Stop() {
    auto locked = tasks_.Lock();
    locked.Push(std::make_unique<StopTask>(this));
    uv_async_send(&flush_tasks_);
  }

 private:
  void Run() {
    TRACE_EVENT_METADATA1("__metadata", "thread_name", "name",
                          "WorkerThreadsTaskRunner::DelayedTaskScheduler");
    loop_.data = this;
    CHECK_EQ(0, uv_loop_init(&loop_));
    flush_tasks_.data = this;
    CHECK_EQ(0, uv_async_init(&loop_, &flush_tasks_, FlushTasks));
    uv_sem_post(&ready_);

    uv_run(&loop_, UV_RUN_DEFAULT);
    CheckedUvLoopClose(&loop_);
  }

  static void FlushTasks(uv_async_t* flush_tasks) {
    DelayedTaskScheduler* scheduler =
        ContainerOf(&DelayedTaskScheduler::loop_, flush_tasks->loop);

    auto tasks_to_run = scheduler->tasks_.Lock().PopAll();
    while (!tasks_to_run.empty()) {
      // We have to use const_cast because std::priority_queue::top() does not
      // return a movable item.
      std::unique_ptr<Task> task =
          std::move(const_cast<std::unique_ptr<Task>&>(tasks_to_run.top()));
      tasks_to_run.pop();
      // This runs either the ScheduleTasks that scheduels the timers to
      // pop the tasks back into the worker task runner queue, or the
      // or the StopTasks to stop the timers and drop all the pending tasks.
      task->Run();
    }
  }

  class StopTask : public Task {
   public:
    explicit StopTask(DelayedTaskScheduler* scheduler): scheduler_(scheduler) {}

    void Run() override {
      std::vector<uv_timer_t*> timers;
      for (uv_timer_t* timer : scheduler_->timers_)
        timers.push_back(timer);
      for (uv_timer_t* timer : timers)
        scheduler_->TakeTimerTask(timer);
      uv_close(reinterpret_cast<uv_handle_t*>(&scheduler_->flush_tasks_),
               [](uv_handle_t* handle) {});
    }

   private:
     DelayedTaskScheduler* scheduler_;
  };

  class ScheduleTask : public Task {
   public:
    ScheduleTask(DelayedTaskScheduler* scheduler,
                 std::unique_ptr<TaskQueueEntry> task,
                 double delay_in_seconds)
        : scheduler_(scheduler),
          task_(std::move(task)),
          delay_in_seconds_(delay_in_seconds) {}

    void Run() override {
      uint64_t delay_millis = llround(delay_in_seconds_ * 1000);
      std::unique_ptr<uv_timer_t> timer(new uv_timer_t());
      CHECK_EQ(0, uv_timer_init(&scheduler_->loop_, timer.get()));
      timer->data = task_.release();
      CHECK_EQ(0, uv_timer_start(timer.get(), RunTask, delay_millis, 0));
      scheduler_->timers_.insert(timer.release());
    }

   private:
    DelayedTaskScheduler* scheduler_;
    std::unique_ptr<TaskQueueEntry> task_;
    double delay_in_seconds_;
  };

  static void RunTask(uv_timer_t* timer) {
    DelayedTaskScheduler* scheduler =
        ContainerOf(&DelayedTaskScheduler::loop_, timer->loop);
    scheduler->pending_worker_tasks_->Lock().Push(
        scheduler->TakeTimerTask(timer));
  }

  std::unique_ptr<TaskQueueEntry> TakeTimerTask(uv_timer_t* timer) {
    std::unique_ptr<TaskQueueEntry> task_entry(
        static_cast<TaskQueueEntry*>(timer->data));
    uv_timer_stop(timer);
    uv_close(reinterpret_cast<uv_handle_t*>(timer), [](uv_handle_t* handle) {
      delete reinterpret_cast<uv_timer_t*>(handle);
    });
    timers_.erase(timer);
    return task_entry;
  }

  uv_sem_t ready_;
  // Task queue in the worker thread task runner, we push the delayed task back
  // to it when the timer expires.
  TaskQueue<TaskQueueEntry>* pending_worker_tasks_;

  // Locally scheduled tasks to be poped into the worker task runner queue.
  // It is flushed whenever the next closest timer expires.
  TaskQueue<Task> tasks_;
  uv_loop_t loop_;
  uv_async_t flush_tasks_;
  std::unordered_set<uv_timer_t*> timers_;
};

WorkerThreadsTaskRunner::WorkerThreadsTaskRunner(
    int thread_pool_size, PlatformDebugLogLevel debug_log_level)
    : debug_log_level_(debug_log_level) {
  Mutex platform_workers_mutex;
  ConditionVariable platform_workers_ready;

  Mutex::ScopedLock lock(platform_workers_mutex);
  int pending_platform_workers = thread_pool_size;

  delayed_task_scheduler_ = std::make_unique<DelayedTaskScheduler>(
      &pending_worker_tasks_);
  threads_.push_back(delayed_task_scheduler_->Start());

  for (int i = 0; i < thread_pool_size; i++) {
    PlatformWorkerData* worker_data =
        new PlatformWorkerData{&pending_worker_tasks_,
                               &platform_workers_mutex,
                               &platform_workers_ready,
                               &pending_platform_workers,
                               i,
                               debug_log_level_};
    std::unique_ptr<uv_thread_t> t { new uv_thread_t() };
    if (uv_thread_create(t.get(), PlatformWorkerThread,
                         worker_data) != 0) {
      break;
    }
    threads_.push_back(std::move(t));
  }

  // Wait for platform workers to initialize before continuing with the
  // bootstrap.
  while (pending_platform_workers > 0) {
    platform_workers_ready.Wait(lock);
  }
}

void WorkerThreadsTaskRunner::PostTask(v8::TaskPriority priority,
                                       std::unique_ptr<v8::Task> task,
                                       const v8::SourceLocation& location) {
  auto entry = std::make_unique<TaskQueueEntry>(std::move(task), priority);
  pending_worker_tasks_.Lock().Push(std::move(entry));
}

void WorkerThreadsTaskRunner::PostDelayedTask(
    v8::TaskPriority priority,
    std::unique_ptr<v8::Task> task,
    const v8::SourceLocation& location,
    double delay_in_seconds) {
  delayed_task_scheduler_->PostDelayedTask(
      priority, std::move(task), delay_in_seconds);
}

void WorkerThreadsTaskRunner::BlockingDrain() {
  pending_worker_tasks_.Lock().BlockingDrain();
}

void WorkerThreadsTaskRunner::Shutdown() {
  pending_worker_tasks_.Lock().Stop();
  delayed_task_scheduler_->Stop();
  for (size_t i = 0; i < threads_.size(); i++) {
    CHECK_EQ(0, uv_thread_join(threads_[i].get()));
  }
}

int WorkerThreadsTaskRunner::NumberOfWorkerThreads() const {
  return threads_.size();
}

PerIsolatePlatformData::PerIsolatePlatformData(
    Isolate* isolate, uv_loop_t* loop, PlatformDebugLogLevel debug_log_level)
    : isolate_(isolate), loop_(loop), debug_log_level_(debug_log_level) {
  flush_tasks_ = new uv_async_t();
  CHECK_EQ(0, uv_async_init(loop, flush_tasks_, FlushTasks));
  flush_tasks_->data = static_cast<void*>(this);
  uv_unref(reinterpret_cast<uv_handle_t*>(flush_tasks_));
}

std::shared_ptr<v8::TaskRunner>
PerIsolatePlatformData::GetForegroundTaskRunner() {
  return shared_from_this();
}

void PerIsolatePlatformData::FlushTasks(uv_async_t* handle) {
  auto platform_data = static_cast<PerIsolatePlatformData*>(handle->data);
  platform_data->FlushForegroundTasksInternal();
}

void PerIsolatePlatformData::PostIdleTaskImpl(
    std::unique_ptr<v8::IdleTask> task, const v8::SourceLocation& location) {
  UNREACHABLE();
}

void PerIsolatePlatformData::PostTaskImpl(std::unique_ptr<Task> task,
                                          const v8::SourceLocation& location) {
  // The task can be posted from any V8 background worker thread, even when
  // the foreground task runner is being cleaned up by Shutdown(). In that
  // case, make sure we wait until the shutdown is completed (which leads
  // to flush_tasks_ == nullptr, and the task will be discarded).
  if (debug_log_level_ != PlatformDebugLogLevel::kNone) {
    fprintf(stderr, "\nPerIsolatePlatformData::PostTaskImpl %p", task.get());
    PrintSourceLocation(location);
    if (debug_log_level_ == PlatformDebugLogLevel::kVerbose) {
      DumpNativeBacktrace(stderr);
    }
    fflush(stderr);
  }

  auto locked = foreground_tasks_.Lock();
  if (flush_tasks_ == nullptr) return;
  // All foreground tasks are treated as user blocking tasks.
  locked.Push(std::make_unique<TaskQueueEntry>(
      std::move(task), v8::TaskPriority::kUserBlocking));
  uv_async_send(flush_tasks_);
}

void PerIsolatePlatformData::PostDelayedTaskImpl(
    std::unique_ptr<Task> task,
    double delay_in_seconds,
    const v8::SourceLocation& location) {
  if (debug_log_level_ != PlatformDebugLogLevel::kNone) {
    fprintf(stderr,
            "\nPerIsolatePlatformData::PostDelayedTaskImpl %p %f",
            task.get(),
            delay_in_seconds);
    PrintSourceLocation(location);
    if (debug_log_level_ == PlatformDebugLogLevel::kVerbose) {
      DumpNativeBacktrace(stderr);
    }
    fflush(stderr);
  }

  auto locked = foreground_delayed_tasks_.Lock();
  if (flush_tasks_ == nullptr) return;
  std::unique_ptr<DelayedTask> delayed(new DelayedTask());
  delayed->task = std::move(task);
  delayed->platform_data = shared_from_this();
  delayed->timeout = delay_in_seconds;
  // All foreground tasks are treated as user blocking tasks.
  delayed->priority = v8::TaskPriority::kUserBlocking;
  locked.Push(std::move(delayed));
  uv_async_send(flush_tasks_);
}

void PerIsolatePlatformData::PostNonNestableTaskImpl(
    std::unique_ptr<Task> task, const v8::SourceLocation& location) {
  PostTaskImpl(std::move(task), location);
}

void PerIsolatePlatformData::PostNonNestableDelayedTaskImpl(
    std::unique_ptr<Task> task,
    double delay_in_seconds,
    const v8::SourceLocation& location) {
  PostDelayedTaskImpl(std::move(task), delay_in_seconds, location);
}

PerIsolatePlatformData::~PerIsolatePlatformData() {
  CHECK(!flush_tasks_);
}

void PerIsolatePlatformData::AddShutdownCallback(void (*callback)(void*),
                                                 void* data) {
  shutdown_callbacks_.emplace_back(ShutdownCallback { callback, data });
}

void PerIsolatePlatformData::Shutdown() {
  auto foreground_tasks_locked = foreground_tasks_.Lock();
  auto foreground_delayed_tasks_locked = foreground_delayed_tasks_.Lock();

  foreground_delayed_tasks_locked.PopAll();
  foreground_tasks_locked.PopAll();
  scheduled_delayed_tasks_.clear();

  if (flush_tasks_ != nullptr) {
    // Both destroying the scheduled_delayed_tasks_ lists and closing
    // flush_tasks_ handle add tasks to the event loop. We keep a count of all
    // non-closed handles, and when that reaches zero, we inform any shutdown
    // callbacks that the platform is done as far as this Isolate is concerned.
    self_reference_ = shared_from_this();
    uv_close(reinterpret_cast<uv_handle_t*>(flush_tasks_),
             [](uv_handle_t* handle) {
               std::unique_ptr<uv_async_t> flush_tasks{
                   reinterpret_cast<uv_async_t*>(handle)};
               PerIsolatePlatformData* platform_data =
                   static_cast<PerIsolatePlatformData*>(flush_tasks->data);
               platform_data->DecreaseHandleCount();
               platform_data->self_reference_.reset();
             });
    flush_tasks_ = nullptr;
  }
}

void PerIsolatePlatformData::DecreaseHandleCount() {
  CHECK_GE(uv_handle_count_, 1);
  if (--uv_handle_count_ == 0) {
    for (const auto& callback : shutdown_callbacks_)
      callback.cb(callback.data);
  }
}

NodePlatform::NodePlatform(int thread_pool_size,
                           v8::TracingController* tracing_controller,
                           v8::PageAllocator* page_allocator) {
  if (per_process::enabled_debug_list.enabled(
          DebugCategory::PLATFORM_VERBOSE)) {
    debug_log_level_ = PlatformDebugLogLevel::kVerbose;
  } else if (per_process::enabled_debug_list.enabled(
                 DebugCategory::PLATFORM_MINIMAL)) {
    debug_log_level_ = PlatformDebugLogLevel::kMinimal;
  } else {
    debug_log_level_ = PlatformDebugLogLevel::kNone;
  }

  if (tracing_controller != nullptr) {
    tracing_controller_ = tracing_controller;
  } else {
    tracing_controller_ = new v8::TracingController();
  }

  // V8 will default to its built in allocator if none is provided.
  page_allocator_ = page_allocator;

  // TODO(addaleax): It's a bit icky that we use global state here, but we can't
  // really do anything about it unless V8 starts exposing a way to access the
  // current v8::Platform instance.
  SetTracingController(tracing_controller_);
  DCHECK_EQ(GetTracingController(), tracing_controller_);

  thread_pool_size = GetActualThreadPoolSize(thread_pool_size);
  worker_thread_task_runner_ = std::make_shared<WorkerThreadsTaskRunner>(
      thread_pool_size, debug_log_level_);
}

NodePlatform::~NodePlatform() {
  Shutdown();
}

void NodePlatform::RegisterIsolate(Isolate* isolate, uv_loop_t* loop) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  auto delegate =
      std::make_shared<PerIsolatePlatformData>(isolate, loop, debug_log_level_);
  IsolatePlatformDelegate* ptr = delegate.get();
  auto insertion = per_isolate_.emplace(
    isolate,
    std::make_pair(ptr, std::move(delegate)));
  CHECK(insertion.second);
}

void NodePlatform::RegisterIsolate(Isolate* isolate,
                                   IsolatePlatformDelegate* delegate) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  auto insertion = per_isolate_.emplace(
    isolate,
    std::make_pair(delegate, std::shared_ptr<PerIsolatePlatformData>{}));
  CHECK(insertion.second);
}

void NodePlatform::UnregisterIsolate(Isolate* isolate) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  auto existing_it = per_isolate_.find(isolate);
  CHECK_NE(existing_it, per_isolate_.end());
  auto& existing = existing_it->second;
  if (existing.second) {
    existing.second->Shutdown();
  }
  per_isolate_.erase(existing_it);
}

void NodePlatform::AddIsolateFinishedCallback(Isolate* isolate,
                                              void (*cb)(void*), void* data) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  auto it = per_isolate_.find(isolate);
  if (it == per_isolate_.end()) {
    cb(data);
    return;
  }
  CHECK(it->second.second);
  it->second.second->AddShutdownCallback(cb, data);
}

void NodePlatform::Shutdown() {
  if (has_shut_down_) return;
  has_shut_down_ = true;
  worker_thread_task_runner_->Shutdown();

  {
    Mutex::ScopedLock lock(per_isolate_mutex_);
    per_isolate_.clear();
  }
}

int NodePlatform::NumberOfWorkerThreads() {
  return worker_thread_task_runner_->NumberOfWorkerThreads();
}

void PerIsolatePlatformData::RunForegroundTask(std::unique_ptr<Task> task) {
  if (isolate_->IsExecutionTerminating()) return;
  DebugSealHandleScope scope(isolate_);
  Environment* env = Environment::GetCurrent(isolate_);
  if (env != nullptr) {
    v8::HandleScope scope(isolate_);
    InternalCallbackScope cb_scope(env, Object::New(isolate_), { 0, 0 },
                                   InternalCallbackScope::kNoFlags);
    task->Run();
  } else {
    // When the Environment was freed, the tasks of the Isolate should also be
    // canceled by `NodePlatform::UnregisterIsolate`. However, if the embedder
    // request to run the foreground task after the Environment was freed, run
    // the task without InternalCallbackScope.

    // The task is moved out of InternalCallbackScope if env is not available.
    // This is a required else block, and should not be removed.
    // See comment: https://github.com/nodejs/node/pull/34688#pullrequestreview-463867489
    task->Run();
  }
}

void PerIsolatePlatformData::DeleteFromScheduledTasks(DelayedTask* task) {
  auto it = std::find_if(scheduled_delayed_tasks_.begin(),
                         scheduled_delayed_tasks_.end(),
                         [task](const DelayedTaskPointer& delayed) -> bool {
          return delayed.get() == task;
      });
  CHECK_NE(it, scheduled_delayed_tasks_.end());
  scheduled_delayed_tasks_.erase(it);
}

void PerIsolatePlatformData::RunForegroundTask(uv_timer_t* handle) {
  DelayedTask* delayed = ContainerOf(&DelayedTask::timer, handle);
  delayed->platform_data->RunForegroundTask(std::move(delayed->task));
  delayed->platform_data->DeleteFromScheduledTasks(delayed);
}

void NodePlatform::DrainTasks(Isolate* isolate) {
  std::shared_ptr<PerIsolatePlatformData> per_isolate = ForNodeIsolate(isolate);
  if (!per_isolate) return;

  do {
    // Worker tasks aren't associated with an Isolate.
    worker_thread_task_runner_->BlockingDrain();
  } while (per_isolate->FlushForegroundTasksInternal());
}

bool PerIsolatePlatformData::FlushForegroundTasksInternal() {
  bool did_work = false;

  auto delayed_tasks_to_schedule = foreground_delayed_tasks_.Lock().PopAll();
  while (!delayed_tasks_to_schedule.empty()) {
    // We have to use const_cast because std::priority_queue::top() does not
    // return a movable item.
    std::unique_ptr<DelayedTask> delayed =
        std::move(const_cast<std::unique_ptr<DelayedTask>&>(
            delayed_tasks_to_schedule.top()));
    delayed_tasks_to_schedule.pop();

    did_work = true;
    uint64_t delay_millis = llround(delayed->timeout * 1000);

    delayed->timer.data = static_cast<void*>(delayed.get());
    uv_timer_init(loop_, &delayed->timer);
    // Timers may not guarantee queue ordering of events with the same delay
    // if the delay is non-zero. This should not be a problem in practice.
    uv_timer_start(&delayed->timer, RunForegroundTask, delay_millis, 0);
    uv_unref(reinterpret_cast<uv_handle_t*>(&delayed->timer));
    uv_handle_count_++;

    scheduled_delayed_tasks_.emplace_back(
        delayed.release(), [](DelayedTask* delayed) {
          uv_close(reinterpret_cast<uv_handle_t*>(&delayed->timer),
                   [](uv_handle_t* handle) {
                     std::unique_ptr<DelayedTask> task{
                         static_cast<DelayedTask*>(handle->data)};
                     task->platform_data->DecreaseHandleCount();
                   });
        });
  }

  TaskQueue<TaskQueueEntry>::PriorityQueue tasks;
  {
    auto locked = foreground_tasks_.Lock();
    tasks = locked.PopAll();
  }

  while (!tasks.empty()) {
    // We have to use const_cast because std::priority_queue::top() does not
    // return a movable item.
    std::unique_ptr<TaskQueueEntry> entry =
        std::move(const_cast<std::unique_ptr<TaskQueueEntry>&>(tasks.top()));
    tasks.pop();
    did_work = true;
    RunForegroundTask(std::move(entry->task));
  }

  return did_work;
}

void NodePlatform::PostTaskOnWorkerThreadImpl(
    v8::TaskPriority priority,
    std::unique_ptr<v8::Task> task,
    const v8::SourceLocation& location) {
  if (debug_log_level_ != PlatformDebugLogLevel::kNone) {
    fprintf(stderr,
            "\nNodePlatform::PostTaskOnWorkerThreadImpl %s %p",
            GetTaskPriorityName(priority),
            task.get());
    PrintSourceLocation(location);
    if (debug_log_level_ == PlatformDebugLogLevel::kVerbose) {
      DumpNativeBacktrace(stderr);
    }
    fflush(stderr);
  }
  worker_thread_task_runner_->PostTask(priority, std::move(task), location);
}

void NodePlatform::PostDelayedTaskOnWorkerThreadImpl(
    v8::TaskPriority priority,
    std::unique_ptr<v8::Task> task,
    double delay_in_seconds,
    const v8::SourceLocation& location) {
  if (debug_log_level_ != PlatformDebugLogLevel::kNone) {
    fprintf(stderr,
            "\nNodePlatform::PostDelayedTaskOnWorkerThreadImpl %s %p %f",
            GetTaskPriorityName(priority),
            task.get(),
            delay_in_seconds);
    PrintSourceLocation(location);
    if (debug_log_level_ == PlatformDebugLogLevel::kVerbose) {
      DumpNativeBacktrace(stderr);
    }
    fflush(stderr);
  }
  worker_thread_task_runner_->PostDelayedTask(
      priority, std::move(task), location, delay_in_seconds);
}

IsolatePlatformDelegate* NodePlatform::ForIsolate(Isolate* isolate) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  auto data = per_isolate_[isolate];
  CHECK_NOT_NULL(data.first);
  return data.first;
}

std::shared_ptr<PerIsolatePlatformData>
NodePlatform::ForNodeIsolate(Isolate* isolate) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  auto data = per_isolate_[isolate];
  CHECK_NOT_NULL(data.first);
  return data.second;
}

bool NodePlatform::FlushForegroundTasks(Isolate* isolate) {
  std::shared_ptr<PerIsolatePlatformData> per_isolate = ForNodeIsolate(isolate);
  if (!per_isolate) return false;
  return per_isolate->FlushForegroundTasksInternal();
}

std::unique_ptr<v8::JobHandle> NodePlatform::CreateJobImpl(
    v8::TaskPriority priority,
    std::unique_ptr<v8::JobTask> job_task,
    const v8::SourceLocation& location) {
  if (debug_log_level_ != PlatformDebugLogLevel::kNone) {
    fprintf(stderr,
            "\nNodePlatform::CreateJobImpl %s %p",
            GetTaskPriorityName(priority),
            job_task.get());
    PrintSourceLocation(location);
    if (debug_log_level_ == PlatformDebugLogLevel::kVerbose) {
      DumpNativeBacktrace(stderr);
    }
    fflush(stderr);
  }
  return v8::platform::NewDefaultJobHandle(
      this, priority, std::move(job_task), NumberOfWorkerThreads());
}

bool NodePlatform::IdleTasksEnabled(Isolate* isolate) {
  return ForIsolate(isolate)->IdleTasksEnabled();
}

std::shared_ptr<v8::TaskRunner> NodePlatform::GetForegroundTaskRunner(
    Isolate* isolate, v8::TaskPriority priority) {
  return ForIsolate(isolate)->GetForegroundTaskRunner();
}

double NodePlatform::MonotonicallyIncreasingTime() {
  // Convert nanos to seconds.
  return uv_hrtime() / 1e9;
}

double NodePlatform::CurrentClockTimeMillis() {
  return SystemClockTimeMillis();
}

v8::TracingController* NodePlatform::GetTracingController() {
  CHECK_NOT_NULL(tracing_controller_);
  return tracing_controller_;
}

Platform::StackTracePrinter NodePlatform::GetStackTracePrinter() {
  return []() {
    fprintf(stderr, "\n");
    DumpNativeBacktrace(stderr);
    fflush(stderr);
  };
}

v8::PageAllocator* NodePlatform::GetPageAllocator() {
  return page_allocator_;
}

template <class T>
TaskQueue<T>::TaskQueue()
    : lock_(), tasks_available_(), tasks_drained_(),
      outstanding_tasks_(0), stopped_(false), task_queue_() { }

template <class T>
TaskQueue<T>::Locked::Locked(TaskQueue* queue)
    : queue_(queue), lock_(queue->lock_) {}

template <class T>
void TaskQueue<T>::Locked::Push(std::unique_ptr<T> task) {
  queue_->outstanding_tasks_++;
  queue_->task_queue_.push(std::move(task));
  queue_->tasks_available_.Signal(lock_);
}

template <class T>
std::unique_ptr<T> TaskQueue<T>::Locked::Pop() {
  if (queue_->task_queue_.empty()) {
    return std::unique_ptr<T>(nullptr);
  }
  std::unique_ptr<T> result = std::move(
      std::move(const_cast<std::unique_ptr<T>&>(queue_->task_queue_.top())));
  queue_->task_queue_.pop();
  return result;
}

template <class T>
std::unique_ptr<T> TaskQueue<T>::Locked::BlockingPop() {
  while (queue_->task_queue_.empty() && !queue_->stopped_) {
    queue_->tasks_available_.Wait(lock_);
  }
  if (queue_->stopped_) {
    return std::unique_ptr<T>(nullptr);
  }
  std::unique_ptr<T> result = std::move(
      std::move(const_cast<std::unique_ptr<T>&>(queue_->task_queue_.top())));
  queue_->task_queue_.pop();
  return result;
}

template <class T>
void TaskQueue<T>::Locked::NotifyOfCompletion() {
  if (--queue_->outstanding_tasks_ == 0) {
    queue_->tasks_drained_.Broadcast(lock_);
  }
}

template <class T>
void TaskQueue<T>::Locked::BlockingDrain() {
  while (queue_->outstanding_tasks_ > 0) {
    queue_->tasks_drained_.Wait(lock_);
  }
}

template <class T>
void TaskQueue<T>::Locked::Stop() {
  queue_->stopped_ = true;
  queue_->tasks_available_.Broadcast(lock_);
}

template <class T>
TaskQueue<T>::PriorityQueue TaskQueue<T>::Locked::PopAll() {
  TaskQueue<T>::PriorityQueue result;
  result.swap(queue_->task_queue_);
  return result;
}

void MultiIsolatePlatform::DisposeIsolate(Isolate* isolate) {
  // The order of these calls is important. When the Isolate is disposed,
  // it may still post tasks to the platform, so it must still be registered
  // for the task runner to be found from the map. After the isolate is torn
  // down, we need to remove it from the map before we can free the address,
  // so that when another Isolate::Allocate() is called, that would not be
  // allocated to the same address and be registered on an existing map
  // entry.
  // Refs: https://github.com/nodejs/node/issues/30846
  isolate->Deinitialize();
  this->UnregisterIsolate(isolate);
  Isolate::Free(isolate);
}

}  // namespace node
