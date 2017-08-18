#include "node_platform.h"

#include "util.h"

namespace node {

using v8::Isolate;
using v8::Platform;
using v8::Task;
using v8::TracingController;

static void FlushTasks(uv_async_t* handle) {
  NodePlatform* platform = static_cast<NodePlatform*>(handle->data);
  platform->FlushForegroundTasksInternal();
}

static void BackgroundRunner(void* data) {
  TaskQueue<Task>* background_tasks = static_cast<TaskQueue<Task>*>(data);
  while (Task* task = background_tasks->BlockingPop()) {
    task->Run();
    delete task;
    background_tasks->NotifyOfCompletion();
  }
}

NodePlatform::NodePlatform(int thread_pool_size, uv_loop_t* loop,
                           TracingController* tracing_controller)
    : loop_(loop) {
  CHECK_EQ(0, uv_async_init(loop, &flush_tasks_, FlushTasks));
  flush_tasks_.data = static_cast<void*>(this);
  uv_unref(reinterpret_cast<uv_handle_t*>(&flush_tasks_));
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

void NodePlatform::Shutdown() {
  background_tasks_.Stop();
  for (size_t i = 0; i < threads_.size(); i++) {
    CHECK_EQ(0, uv_thread_join(threads_[i].get()));
  }
  // uv_run cannot be called from the time before the beforeExit callback
  // runs until the program exits unless the event loop has any referenced
  // handles after beforeExit terminates. This prevents unrefed timers
  // that happen to terminate during shutdown from being run unsafely.
  // Since uv_run cannot be called, this handle will never be fully cleaned
  // up.
  uv_close(reinterpret_cast<uv_handle_t*>(&flush_tasks_), nullptr);
}

size_t NodePlatform::NumberOfAvailableBackgroundThreads() {
  return threads_.size();
}

static void RunForegroundTask(uv_timer_t* handle) {
  Task* task = static_cast<Task*>(handle->data);
  task->Run();
  delete task;
  uv_close(reinterpret_cast<uv_handle_t*>(handle), [](uv_handle_t* handle) {
    delete reinterpret_cast<uv_timer_t*>(handle);
  });
}

void NodePlatform::DrainBackgroundTasks() {
  FlushForegroundTasksInternal();
  background_tasks_.BlockingDrain();
}

void NodePlatform::FlushForegroundTasksInternal() {
  while (auto delayed = foreground_delayed_tasks_.Pop()) {
    uint64_t delay_millis =
        static_cast<uint64_t>(delayed->second + 0.5) * 1000;
    uv_timer_t* handle = new uv_timer_t();
    handle->data = static_cast<void*>(delayed->first);
    uv_timer_init(loop_, handle);
    // Timers may not guarantee queue ordering of events with the same delay if
    // the delay is non-zero. This should not be a problem in practice.
    uv_timer_start(handle, RunForegroundTask, delay_millis, 0);
    uv_unref(reinterpret_cast<uv_handle_t*>(handle));
    delete delayed;
  }
  while (Task* task = foreground_tasks_.Pop()) {
    task->Run();
    delete task;
  }
}

void NodePlatform::CallOnBackgroundThread(Task* task,
                                          ExpectedRuntime expected_runtime) {
  background_tasks_.Push(task);
}

void NodePlatform::CallOnForegroundThread(Isolate* isolate, Task* task) {
  foreground_tasks_.Push(task);
  uv_async_send(&flush_tasks_);
}

void NodePlatform::CallDelayedOnForegroundThread(Isolate* isolate,
                                                 Task* task,
                                                 double delay_in_seconds) {
  auto pair = new std::pair<Task*, double>(task, delay_in_seconds);
  foreground_delayed_tasks_.Push(pair);
  uv_async_send(&flush_tasks_);
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
