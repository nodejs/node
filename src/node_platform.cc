#include "node_platform.h"
#include "node_internals.h"

#include "env.h"
#include "env-inl.h"
#include "util.h"
#include <algorithm>

namespace node {

using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Platform;
using v8::Task;
using v8::TracingController;

static void BackgroundRunner(void* data) {
  TaskQueue<Task>* background_tasks = static_cast<TaskQueue<Task>*>(data);
  while (std::unique_ptr<Task> task = background_tasks->BlockingPop()) {
    task->Run();
    background_tasks->NotifyOfCompletion();
  }
}

BackgroundTaskRunner::BackgroundTaskRunner(int thread_pool_size) {
  for (int i = 0; i < thread_pool_size; i++) {
    std::unique_ptr<uv_thread_t> t { new uv_thread_t() };
    if (uv_thread_create(t.get(), BackgroundRunner, &background_tasks_) != 0)
      break;
    threads_.push_back(std::move(t));
  }
}

void BackgroundTaskRunner::PostTask(std::unique_ptr<Task> task) {
  background_tasks_.Push(std::move(task));
}

void BackgroundTaskRunner::PostIdleTask(std::unique_ptr<v8::IdleTask> task) {
  UNREACHABLE();
}

void BackgroundTaskRunner::PostDelayedTask(std::unique_ptr<v8::Task> task,
                                           double delay_in_seconds) {
  UNREACHABLE();
}

void BackgroundTaskRunner::BlockingDrain() {
  background_tasks_.BlockingDrain();
}

void BackgroundTaskRunner::Shutdown() {
  background_tasks_.Stop();
  for (size_t i = 0; i < threads_.size(); i++) {
    CHECK_EQ(0, uv_thread_join(threads_[i].get()));
  }
}

size_t BackgroundTaskRunner::NumberOfAvailableBackgroundThreads() const {
  return threads_.size();
}

PerIsolatePlatformData::PerIsolatePlatformData(
    v8::Isolate* isolate, uv_loop_t* loop)
  : isolate_(isolate), loop_(loop) {
  flush_tasks_ = new uv_async_t();
  CHECK_EQ(0, uv_async_init(loop, flush_tasks_, FlushTasks));
  flush_tasks_->data = static_cast<void*>(this);
  uv_unref(reinterpret_cast<uv_handle_t*>(flush_tasks_));
}

void PerIsolatePlatformData::FlushTasks(uv_async_t* handle) {
  auto platform_data = static_cast<PerIsolatePlatformData*>(handle->data);
  platform_data->FlushForegroundTasksInternal();
}

void PerIsolatePlatformData::PostIdleTask(std::unique_ptr<v8::IdleTask> task) {
  UNREACHABLE();
}

void PerIsolatePlatformData::PostTask(std::unique_ptr<Task> task) {
  foreground_tasks_.Push(std::move(task));
  uv_async_send(flush_tasks_);
}

void PerIsolatePlatformData::PostDelayedTask(
    std::unique_ptr<Task> task, double delay_in_seconds) {
  std::unique_ptr<DelayedTask> delayed(new DelayedTask());
  delayed->task = std::move(task);
  delayed->platform_data = shared_from_this();
  delayed->timeout = delay_in_seconds;
  foreground_delayed_tasks_.Push(std::move(delayed));
  uv_async_send(flush_tasks_);
}

PerIsolatePlatformData::~PerIsolatePlatformData() {
  FlushForegroundTasksInternal();
  CancelPendingDelayedTasks();

  uv_close(reinterpret_cast<uv_handle_t*>(flush_tasks_),
           [](uv_handle_t* handle) {
    delete reinterpret_cast<uv_async_t*>(handle);
  });
}

void PerIsolatePlatformData::ref() {
  ref_count_++;
}

int PerIsolatePlatformData::unref() {
  return --ref_count_;
}

NodePlatform::NodePlatform(int thread_pool_size,
                           TracingController* tracing_controller) {
  if (tracing_controller) {
    tracing_controller_.reset(tracing_controller);
  } else {
    TracingController* controller = new TracingController();
    tracing_controller_.reset(controller);
  }
  background_task_runner_ =
      std::make_shared<BackgroundTaskRunner>(thread_pool_size);
}

void NodePlatform::RegisterIsolate(IsolateData* isolate_data, uv_loop_t* loop) {
  Isolate* isolate = isolate_data->isolate();
  Mutex::ScopedLock lock(per_isolate_mutex_);
  std::shared_ptr<PerIsolatePlatformData> existing = per_isolate_[isolate];
  if (existing) {
    existing->ref();
  } else {
    per_isolate_[isolate] =
        std::make_shared<PerIsolatePlatformData>(isolate, loop);
  }
}

void NodePlatform::UnregisterIsolate(IsolateData* isolate_data) {
  Isolate* isolate = isolate_data->isolate();
  Mutex::ScopedLock lock(per_isolate_mutex_);
  std::shared_ptr<PerIsolatePlatformData> existing = per_isolate_[isolate];
  CHECK(existing);
  if (existing->unref() == 0) {
    per_isolate_.erase(isolate);
  }
}

void NodePlatform::Shutdown() {
  background_task_runner_->Shutdown();

  {
    Mutex::ScopedLock lock(per_isolate_mutex_);
    per_isolate_.clear();
  }
}

size_t NodePlatform::NumberOfAvailableBackgroundThreads() {
  return background_task_runner_->NumberOfAvailableBackgroundThreads();
}

void PerIsolatePlatformData::RunForegroundTask(std::unique_ptr<Task> task) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  InternalCallbackScope cb_scope(env, Local<Object>(), { 0, 0 },
                                 InternalCallbackScope::kAllowEmptyResource);
  task->Run();
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
  DelayedTask* delayed = static_cast<DelayedTask*>(handle->data);
  RunForegroundTask(std::move(delayed->task));
  delayed->platform_data->DeleteFromScheduledTasks(delayed);
}

void PerIsolatePlatformData::CancelPendingDelayedTasks() {
  scheduled_delayed_tasks_.clear();
}

void NodePlatform::DrainBackgroundTasks(Isolate* isolate) {
  std::shared_ptr<PerIsolatePlatformData> per_isolate = ForIsolate(isolate);

  do {
    // Right now, there is no way to drain only background tasks associated
    // with a specific isolate, so this sometimes does more work than
    // necessary. In the long run, that functionality is probably going to
    // be available anyway, though.
    background_task_runner_->BlockingDrain();
  } while (per_isolate->FlushForegroundTasksInternal());
}

bool PerIsolatePlatformData::FlushForegroundTasksInternal() {
  bool did_work = false;

  while (std::unique_ptr<DelayedTask> delayed =
      foreground_delayed_tasks_.Pop()) {
    did_work = true;
    uint64_t delay_millis =
        static_cast<uint64_t>(delayed->timeout + 0.5) * 1000;
    delayed->timer.data = static_cast<void*>(delayed.get());
    uv_timer_init(loop_, &delayed->timer);
    // Timers may not guarantee queue ordering of events with the same delay if
    // the delay is non-zero. This should not be a problem in practice.
    uv_timer_start(&delayed->timer, RunForegroundTask, delay_millis, 0);
    uv_unref(reinterpret_cast<uv_handle_t*>(&delayed->timer));

    scheduled_delayed_tasks_.emplace_back(delayed.release(),
                                          [](DelayedTask* delayed) {
      uv_close(reinterpret_cast<uv_handle_t*>(&delayed->timer),
               [](uv_handle_t* handle) {
        delete static_cast<DelayedTask*>(handle->data);
      });
    });
  }
  while (std::unique_ptr<Task> task = foreground_tasks_.Pop()) {
    did_work = true;
    RunForegroundTask(std::move(task));
  }
  return did_work;
}

void NodePlatform::CallOnBackgroundThread(Task* task,
                                          ExpectedRuntime expected_runtime) {
  background_task_runner_->PostTask(std::unique_ptr<Task>(task));
}

std::shared_ptr<PerIsolatePlatformData>
NodePlatform::ForIsolate(Isolate* isolate) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  std::shared_ptr<PerIsolatePlatformData> data = per_isolate_[isolate];
  CHECK(data);
  return data;
}

void NodePlatform::CallOnForegroundThread(Isolate* isolate, Task* task) {
  ForIsolate(isolate)->PostTask(std::unique_ptr<Task>(task));
}

void NodePlatform::CallDelayedOnForegroundThread(Isolate* isolate,
                                                 Task* task,
                                                 double delay_in_seconds) {
  ForIsolate(isolate)->PostDelayedTask(
    std::unique_ptr<Task>(task), delay_in_seconds);
}

void NodePlatform::FlushForegroundTasks(v8::Isolate* isolate) {
  ForIsolate(isolate)->FlushForegroundTasksInternal();
}

void NodePlatform::CancelPendingDelayedTasks(v8::Isolate* isolate) {
  ForIsolate(isolate)->CancelPendingDelayedTasks();
}

bool NodePlatform::IdleTasksEnabled(Isolate* isolate) { return false; }

std::shared_ptr<v8::TaskRunner>
NodePlatform::GetBackgroundTaskRunner(Isolate* isolate) {
  return background_task_runner_;
}

std::shared_ptr<v8::TaskRunner>
NodePlatform::GetForegroundTaskRunner(Isolate* isolate) {
  return ForIsolate(isolate);
}

double NodePlatform::MonotonicallyIncreasingTime() {
  // Convert nanos to seconds.
  return uv_hrtime() / 1e9;
}

double NodePlatform::CurrentClockTimeMillis() {
  return SystemClockTimeMillis();
}

TracingController* NodePlatform::GetTracingController() {
  return tracing_controller_.get();
}

template <class T>
TaskQueue<T>::TaskQueue()
    : lock_(), tasks_available_(), tasks_drained_(),
      outstanding_tasks_(0), stopped_(false), task_queue_() { }

template <class T>
void TaskQueue<T>::Push(std::unique_ptr<T> task) {
  Mutex::ScopedLock scoped_lock(lock_);
  outstanding_tasks_++;
  task_queue_.push(std::move(task));
  tasks_available_.Signal(scoped_lock);
}

template <class T>
std::unique_ptr<T> TaskQueue<T>::Pop() {
  Mutex::ScopedLock scoped_lock(lock_);
  if (task_queue_.empty()) {
    return std::unique_ptr<T>(nullptr);
  }
  std::unique_ptr<T> result = std::move(task_queue_.front());
  task_queue_.pop();
  return result;
}

template <class T>
std::unique_ptr<T> TaskQueue<T>::BlockingPop() {
  Mutex::ScopedLock scoped_lock(lock_);
  while (task_queue_.empty() && !stopped_) {
    tasks_available_.Wait(scoped_lock);
  }
  if (stopped_) {
    return std::unique_ptr<T>(nullptr);
  }
  std::unique_ptr<T> result = std::move(task_queue_.front());
  task_queue_.pop();
  return result;
}

template <class T>
void TaskQueue<T>::NotifyOfCompletion() {
  Mutex::ScopedLock scoped_lock(lock_);
  if (--outstanding_tasks_ == 0) {
    tasks_drained_.Broadcast(scoped_lock);
  }
}

template <class T>
void TaskQueue<T>::BlockingDrain() {
  Mutex::ScopedLock scoped_lock(lock_);
  while (outstanding_tasks_ > 0) {
    tasks_drained_.Wait(scoped_lock);
  }
}

template <class T>
void TaskQueue<T>::Stop() {
  Mutex::ScopedLock scoped_lock(lock_);
  stopped_ = true;
  tasks_available_.Broadcast(scoped_lock);
}

}  // namespace node
