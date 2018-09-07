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

    queue->NotifyOfCompletion();
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
  : queue_(), outstanding_tasks_(0), stopped_(false)
  , lock_()
  , task_available_(), tasks_drained_() {
}

bool TaskQueue::Push(std::unique_ptr<Task> task) {
  Mutex::ScopedLock scoped_lock(lock_);

  if (stopped_) {
    return false;
  }

  task->UpdateState(Task::QUEUED);
  queue_.push(std::move(task));
  outstanding_tasks_++;
  task_available_.Signal(scoped_lock);

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
    task_available_.Wait(scoped_lock);
  }

  if (queue_.empty()) {
    return std::unique_ptr<Task>(nullptr);
  }

  std::unique_ptr<Task> result = std::move(queue_.front());
  queue_.pop();
  return result;
}

void TaskQueue::NotifyOfCompletion(void) {
  Mutex::ScopedLock scoped_lock(lock_);
  outstanding_tasks_--;
  CHECK_GE(outstanding_tasks_, 0);
  if (!outstanding_tasks_) {
    tasks_drained_.Broadcast(scoped_lock);
  }
}

void TaskQueue::BlockingDrain(void) {
  Mutex::ScopedLock scoped_lock(lock_);
  while (outstanding_tasks_) {
    tasks_drained_.Wait(scoped_lock);
  }
  LOG("TaskQueue::BlockingDrain: Fully drained\n");
}

void TaskQueue::Stop(void) {
  Mutex::ScopedLock scoped_lock(lock_);
  stopped_ = true;
  task_available_.Broadcast(scoped_lock);
}

int TaskQueue::Length(void) const {
  Mutex::ScopedLock scoped_lock(lock_);
  return queue_.size();
}

/**************
 * Threadpool
 ***************/

Threadpool::Threadpool(int threadpool_size)
  : threadpool_size_(threadpool_size), queue_(), workers_() {
  LOG("Threadpool::Threadpool: threadpool_size_ %d\n", threadpool_size_);
  if (threadpool_size_ <= 0) {
    // Check UV_THREADPOOL_SIZE
    char buf[32];
    size_t buf_size = sizeof(buf);
    if (uv_os_getenv("UV_THREADPOOL_SIZE", buf, &buf_size) == 0) {
      threadpool_size_ = atoi(buf);
    }
  }

  if (threadpool_size_ <= 0) {
    // No/bad UV_THREADPOOL_SIZE, so take a guess.
    threadpool_size_ = GoodThreadpoolSize();
  }
  LOG("Threadpool::Threadpool: threadpool_size_ %d\n", threadpool_size_);
  CHECK_GT(threadpool_size_, 0);

  Initialize();
}

int Threadpool::GoodThreadpoolSize(void) {
  // Ask libuv how many cores we have.
  uv_cpu_info_t* cpu_infos;
  int count;

  if (uv_cpu_info(&cpu_infos, &count)) {
    return 4;  // Old libuv TP default.
  }

  uv_free_cpu_info(cpu_infos, count);
  return count;
}

void Threadpool::Initialize() {
  for (int i = 0; i < threadpool_size_; i++) {
    std::unique_ptr<Worker> worker(new Worker());
    worker->Start(&queue_);
    workers_.push_back(std::move(worker));
  }
}

Threadpool::~Threadpool(void) {
  // Block future Push's.
  queue_.Stop();

  // Workers will drain the queue and then return.
  for (auto& worker : workers_) {
    worker->Join();
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

void Threadpool::BlockingDrain(void) {
  queue_.BlockingDrain();
}

int Threadpool::NWorkers(void) const {
  return workers_.size();
}

}  // namespace threadpool
}  // namespace node
