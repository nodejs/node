#include "node_threadpool.h"
#include "node_internals.h"

#include "env-inl.h"
#include "debug_utils.h"
#include "util.h"
#include <algorithm>

// TODO(davisjam): DO NOT MERGE. Only for debugging.
// TODO(davisjam): There must be a better way to do this.
#define DEBUG_LOG 1
// #undef DEBUG_LOG

#ifdef DEBUG_LOG
#include <stdio.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) (void) 0
#endif

namespace node {
namespace threadpool {

/**************
 * Worker
 ***************/

Worker::Worker() {
}

void Worker::Start(TaskQueue* queue) {
  CHECK_EQ(0, uv_thread_create(&self_, _Run, reinterpret_cast<void *>(queue)));
}

void Worker::Join(void) {
  CHECK_EQ(0, uv_thread_join(&self_));
}

void Worker::_Run(void* data) {
  TaskQueue* queue = static_cast<TaskQueue*>(data);
  while (std::unique_ptr<Task> task = queue->BlockingPop()) {
    task->UpdateState(Task::ASSIGNED);
    task->Run();
    task->UpdateState(Task::COMPLETED);
  }
}

/**************
 * Task
 ***************/

void Task::UpdateState(enum State state) {
  state_ = state;
}

/**************
 * LibuvExecutor
 ***************/

LibuvExecutor::LibuvExecutor(std::shared_ptr<Threadpool> tp)
  : tp_(tp) {
  executor_.init = uv_executor_init;
  executor_.destroy = nullptr;
  executor_.submit = uv_executor_submit;
  executor_.cancel = nullptr;
  executor_.data = this;
}

uv_executor_t* LibuvExecutor::GetExecutor() {
  return &executor_;
}

void LibuvExecutor::uv_executor_init(uv_executor_t* executor) {
  // Already initialized.
  // TODO(davisjam): I don't think we need this API in libuv. Nor destroy.
}

void LibuvExecutor::uv_executor_submit(uv_executor_t* executor,
                                       uv_work_t* req,
                                       const uv_work_options_t* opts) {
  LibuvExecutor* libuv_executor =
    reinterpret_cast<LibuvExecutor *>(executor->data);
  LOG("LibuvExecutor::uv_executor_submit: Got some work!\n");
  libuv_executor->tp_->Post(std::unique_ptr<Task>(
    new LibuvTask(libuv_executor, req, opts)));
}


/**************
 * LibuvTask
 ***************/

LibuvTask::LibuvTask(LibuvExecutor* libuv_executor,
                     uv_work_t* req,
                     const uv_work_options_t* opts)
  : Task(), libuv_executor_(libuv_executor), req_(req) {
  // Fill in TaskDetails based on opts.
  if (opts) {
    switch (opts->type) {
    case UV_WORK_FS:
      details_.type = TaskDetails::FS;
      break;
    case UV_WORK_DNS:
      details_.type = TaskDetails::DNS;
      break;
    case UV_WORK_USER_IO:
      details_.type = TaskDetails::IO;
      break;
    case UV_WORK_USER_CPU:
      details_.type = TaskDetails::CPU;
      break;
    default:
      details_.type = TaskDetails::UNKNOWN;
    }

    details_.priority = opts->priority;
    details_.cancelable = opts->cancelable;
  } else {
    details_.type = TaskDetails::UNKNOWN;
    details_.priority = -1;
    details_.cancelable = false;
  }

  LOG("LibuvTask::LibuvTask: type %d\n", details_.type);
}

LibuvTask::~LibuvTask(void) {
  LOG("LibuvTask::Run: Task %p done\n", req_);
  libuv_executor_->GetExecutor()->done(req_);
}

void LibuvTask::Run() {
  LOG("LibuvTask::Run: Running Task %p\n", req_);
  req_->work_cb(req_);
}

/**************
 * TaskQueue
 ***************/

TaskQueue::TaskQueue()
  : queue_(), stopped_(false), lock_(), tasks_available_() {
}

bool TaskQueue::Push(std::unique_ptr<Task> task) {
  Mutex::ScopedLock scoped_lock(lock_);

  if (stopped_) {
    return false;
  }

  task->UpdateState(Task::QUEUED);
  queue_.push(std::move(task));
  tasks_available_.Signal(scoped_lock);

  return true;
}

std::unique_ptr<Task> TaskQueue::Pop(void) {
  Mutex::ScopedLock scoped_lock(lock_);

  if (queue_.empty()) {
    return std::unique_ptr<Task>(nullptr);
  }

  std::unique_ptr<Task> task = std::move(queue_.front());
  queue_.pop();
  return task;
}

std::unique_ptr<Task> TaskQueue::BlockingPop(void) {
  Mutex::ScopedLock scoped_lock(lock_);

  while (queue_.empty() && !stopped_) {
    tasks_available_.Wait(scoped_lock);
  }

  if (queue_.empty()) {
    return std::unique_ptr<Task>(nullptr);
  }

  std::unique_ptr<Task> result = std::move(queue_.front());
  queue_.pop();
  return result;
}

void TaskQueue::Stop(void) {
  Mutex::ScopedLock scoped_lock(lock_);
  stopped_ = true;
  tasks_available_.Broadcast(scoped_lock);
}

int TaskQueue::Length(void) const {
  Mutex::ScopedLock scoped_lock(lock_);
  return queue_.size();
}

/**************
 * Threadpool
 ***************/

Threadpool::Threadpool(void)
  : queue_(), workers_() {
}

Threadpool::~Threadpool(void) {
  // Block future Push's.
  queue_.Stop();

  // Wait for Workers to drain the queue.
  for (auto& worker : workers_) {
    worker->Join();
  }
}

// TODO(davisjam): Return early on multiple initialization
void Threadpool::Initialize(void) {
  int n_workers = 4;  // TODO(davisjam):

  for (int i = 0; i < n_workers; i++) {
    std::unique_ptr<Worker> worker(new Worker());
    worker->Start(&queue_);
    workers_.push_back(std::move(worker));
  }
}

void Threadpool::Post(std::unique_ptr<Task> task) {
  LOG("Threadpool::Post: Got task of type %d\n",
    task->details_.type);
  queue_.Push(std::move(task));
}

int Threadpool::QueueLength(void) const {
  return queue_.Length();
}

}  // namespace threadpool
}  // namespace node
