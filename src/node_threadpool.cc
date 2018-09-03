#include "node_threadpool.h"
#include "node_internals.h"

#include "env-inl.h"
#include "debug_utils.h"
#include "util.h"
#include <algorithm>

// TODO(davisjam): DO NOT MERGE. Only for debugging.
// TODO(davisjam): There must be a better way to do this.
#define DEBUG_LOG 1
#undef DEBUG_LOG

#ifdef DEBUG_LOG
#include <stdio.h>
#define LOG_0(fmt) fprintf(stderr, fmt)
#define LOG_1(fmt, a1) fprintf(stderr, fmt, a1)
#define LOG_2(fmt, a1, a2) fprintf(stderr, fmt, a1, a2)
#define LOG_3(fmt, a1, a2, a3) fprintf(stderr, fmt, a1, a2, a3)
#define LOG_4(fmt, a1, a2, a3, a4) fprintf(stderr, fmt, a1, a2, a3, a4)
#define LOG_5(fmt, a1, a2, a3, a4, a5) fprintf(stderr, fmt, a1, a2, a3, a4, a5)
#else
#define LOG_0(fmt) (void) 0
#define LOG_1(fmt, a1) (void) 0
#define LOG_2(fmt, a1, a2) (void) 0
#define LOG_3(fmt, a1, a2, a3) (void) 0
#define LOG_4(fmt, a1, a2, a3, a4) (void) 0
#define LOG_5(fmt, a1, a2, a3, a4, a5) (void) 0
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
    task->Run();
  }
}

/**************
 * LibuvTask
 ***************/

LibuvTask::LibuvTask(Threadpool* tp,
                     uv_work_t* req,
                     const uv_work_options_t* opts)
  : Task(), tp_(tp), req_(req) {
  req_ = req;

  // Copy opts.
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

  LOG_1("LibuvTask::LibuvTask: type %d\n", details_.type);
}

LibuvTask::~LibuvTask(void) {
  LOG_1("LibuvTask::Run: Task %p done\n", req_);
  tp_->GetExecutor()->done(req_);
}

void LibuvTask::Run() {
  LOG_1("LibuvTask::Run: Running Task %p\n", req_);
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
  executor_.init = uv_executor_init;
  executor_.destroy = nullptr;
  executor_.submit = uv_executor_submit;
  executor_.cancel = nullptr;
  executor_.data = this;
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
  LOG_1("Threadpool::Post: Got task of type %d\n",
    task->details_.type);
  queue_.Push(std::move(task));
}

int Threadpool::QueueLength(void) const {
  return queue_.Length();
}

void Threadpool::uv_executor_init(uv_executor_t* executor) {
}

void Threadpool::uv_executor_submit(uv_executor_t* executor,
                                    uv_work_t* req,
                                    const uv_work_options_t* opts) {
  Threadpool* threadpool = reinterpret_cast<Threadpool *>(executor->data);
  LOG_0("Threadpool::uv_executor_submit: Got some work!\n");
  threadpool->Post(std::unique_ptr<Task>(new LibuvTask(threadpool, req, opts)));
}

uv_executor_t* Threadpool::GetExecutor() {
  return &executor_;
}

}  // namespace threadpool
}  // namespace node
