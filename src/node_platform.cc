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
  while (Task* task = background_tasks->BlockingPop()) {
    task->Run();
    delete task;
    background_tasks->NotifyOfCompletion();
  }
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

void PerIsolatePlatformData::CallOnForegroundThread(Task* task) {
  foreground_tasks_.Push(task);
  uv_async_send(flush_tasks_);
}

void PerIsolatePlatformData::CallDelayedOnForegroundThread(
    Task* task, double delay_in_seconds) {
  auto delayed = new DelayedTask();
  delayed->task = task;
  delayed->platform_data = this;
  delayed->timeout = delay_in_seconds;
  foreground_delayed_tasks_.Push(delayed);
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
  for (int i = 0; i < thread_pool_size; i++) {
    uv_thread_t* t = new uv_thread_t();
    if (uv_thread_create(t, BackgroundRunner, &background_tasks_) != 0) {
      delete t;
      break;
    }
    threads_.push_back(std::unique_ptr<uv_thread_t>(t));
  }
}

void NodePlatform::RegisterIsolate(IsolateData* isolate_data, uv_loop_t* loop) {
  Isolate* isolate = isolate_data->isolate();
  Mutex::ScopedLock lock(per_isolate_mutex_);
  PerIsolatePlatformData* existing = per_isolate_[isolate];
  if (existing != nullptr)
    existing->ref();
  else
    per_isolate_[isolate] = new PerIsolatePlatformData(isolate, loop);
}

void NodePlatform::UnregisterIsolate(IsolateData* isolate_data) {
  Isolate* isolate = isolate_data->isolate();
  Mutex::ScopedLock lock(per_isolate_mutex_);
  PerIsolatePlatformData* existing = per_isolate_[isolate];
  CHECK_NE(existing, nullptr);
  if (existing->unref() == 0) {
    delete existing;
    per_isolate_.erase(isolate);
  }
}

void NodePlatform::Shutdown() {
  background_tasks_.Stop();
  for (size_t i = 0; i < threads_.size(); i++) {
    CHECK_EQ(0, uv_thread_join(threads_[i].get()));
  }
  Mutex::ScopedLock lock(per_isolate_mutex_);
  for (const auto& pair : per_isolate_)
    delete pair.second;
}

size_t NodePlatform::NumberOfAvailableBackgroundThreads() {
  return threads_.size();
}

void PerIsolatePlatformData::RunForegroundTask(Task* task) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  InternalCallbackScope cb_scope(env, Local<Object>(), { 0, 0 },
                                 InternalCallbackScope::kAllowEmptyResource);
  task->Run();
  delete task;
}

void PerIsolatePlatformData::RunForegroundTask(uv_timer_t* handle) {
  DelayedTask* delayed = static_cast<DelayedTask*>(handle->data);
  auto& tasklist = delayed->platform_data->scheduled_delayed_tasks_;
  auto it = std::find(tasklist.begin(), tasklist.end(), delayed);
  CHECK_NE(it, tasklist.end());
  tasklist.erase(it);
  RunForegroundTask(delayed->task);
  uv_close(reinterpret_cast<uv_handle_t*>(&delayed->timer),
           [](uv_handle_t* handle) {
    delete static_cast<DelayedTask*>(handle->data);
  });
}

void PerIsolatePlatformData::CancelPendingDelayedTasks() {
  for (auto delayed : scheduled_delayed_tasks_) {
    uv_close(reinterpret_cast<uv_handle_t*>(&delayed->timer),
             [](uv_handle_t* handle) {
      delete static_cast<DelayedTask*>(handle->data);
    });
  }
  scheduled_delayed_tasks_.clear();
}

void NodePlatform::DrainBackgroundTasks(Isolate* isolate) {
  PerIsolatePlatformData* per_isolate = ForIsolate(isolate);

  do {
    // Right now, there is no way to drain only background tasks associated with
    // a specific isolate, so this sometimes does more work than necessary.
    // In the long run, that functionality is probably going to be available
    // anyway, though.
    background_tasks_.BlockingDrain();
  } while (per_isolate->FlushForegroundTasksInternal());
}

bool PerIsolatePlatformData::FlushForegroundTasksInternal() {
  bool did_work = false;

  while (auto delayed = foreground_delayed_tasks_.Pop()) {
    did_work = true;
    uint64_t delay_millis =
        static_cast<uint64_t>(delayed->timeout + 0.5) * 1000;
    delayed->timer.data = static_cast<void*>(delayed);
    uv_timer_init(loop_, &delayed->timer);
    // Timers may not guarantee queue ordering of events with the same delay if
    // the delay is non-zero. This should not be a problem in practice.
    uv_timer_start(&delayed->timer, RunForegroundTask, delay_millis, 0);
    uv_unref(reinterpret_cast<uv_handle_t*>(&delayed->timer));
    scheduled_delayed_tasks_.push_back(delayed);
  }
  while (Task* task = foreground_tasks_.Pop()) {
    did_work = true;
    RunForegroundTask(task);
  }
  return did_work;
}

void NodePlatform::CallOnBackgroundThread(Task* task,
                                          ExpectedRuntime expected_runtime) {
  background_tasks_.Push(task);
}

PerIsolatePlatformData* NodePlatform::ForIsolate(Isolate* isolate) {
  Mutex::ScopedLock lock(per_isolate_mutex_);
  PerIsolatePlatformData* data = per_isolate_[isolate];
  CHECK_NE(data, nullptr);
  return data;
}

void NodePlatform::CallOnForegroundThread(Isolate* isolate, Task* task) {
  ForIsolate(isolate)->CallOnForegroundThread(task);
}

void NodePlatform::CallDelayedOnForegroundThread(Isolate* isolate,
                                                 Task* task,
                                                 double delay_in_seconds) {
  ForIsolate(isolate)->CallDelayedOnForegroundThread(task,
                                                     delay_in_seconds);
}

void NodePlatform::FlushForegroundTasks(v8::Isolate* isolate) {
  ForIsolate(isolate)->FlushForegroundTasksInternal();
}

void NodePlatform::CancelPendingDelayedTasks(v8::Isolate* isolate) {
  ForIsolate(isolate)->CancelPendingDelayedTasks();
}

bool NodePlatform::IdleTasksEnabled(Isolate* isolate) { return false; }

double NodePlatform::MonotonicallyIncreasingTime() {
  // Convert nanos to seconds.
  return uv_hrtime() / 1e9;
}

TracingController* NodePlatform::GetTracingController() {
  return tracing_controller_.get();
}

template <class T>
TaskQueue<T>::TaskQueue()
    : lock_(), tasks_available_(), tasks_drained_(),
      outstanding_tasks_(0), stopped_(false), task_queue_() { }

template <class T>
void TaskQueue<T>::Push(T* task) {
  Mutex::ScopedLock scoped_lock(lock_);
  outstanding_tasks_++;
  task_queue_.push(task);
  tasks_available_.Signal(scoped_lock);
}

template <class T>
T* TaskQueue<T>::Pop() {
  Mutex::ScopedLock scoped_lock(lock_);
  T* result = nullptr;
  if (!task_queue_.empty()) {
    result = task_queue_.front();
    task_queue_.pop();
  }
  return result;
}

template <class T>
T* TaskQueue<T>::BlockingPop() {
  Mutex::ScopedLock scoped_lock(lock_);
  while (task_queue_.empty() && !stopped_) {
    tasks_available_.Wait(scoped_lock);
  }
  if (stopped_) {
    return nullptr;
  }
  T* result = task_queue_.front();
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
