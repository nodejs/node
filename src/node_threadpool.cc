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
#define LOG(...) () 0
#endif

namespace node {
namespace threadpool {

/**************
 * NodeThreadpool
 ***************/

NodeThreadpool::NodeThreadpool() : tp_(nullptr) {
}

NodeThreadpool::NodeThreadpool(int threadpool_size) {
  if (threadpool_size <= 0) {
    // Check UV_THREADPOOL_SIZE
    char buf[32];
    size_t buf_size = sizeof(buf);
    if (uv_os_getenv("UV_THREADPOOL_SIZE", buf, &buf_size) == 0) {
      threadpool_size = atoi(buf);
    }
  }

  if (threadpool_size <= 0) {
    // No/bad UV_THREADPOOL_SIZE, so take a guess.
    threadpool_size = GoodCPUThreadpoolSize();
  }
  LOG("NodeThreadpool::NodeThreadpool: threadpool_size %d\n", threadpool_size);
  CHECK_GT(threadpool_size, 0);

  tp_ = std::make_shared<Threadpool>(threadpool_size);
}

int NodeThreadpool::GoodCPUThreadpoolSize(void) {
  // Ask libuv how many cores we have.
  uv_cpu_info_t* cpu_infos;
  int cpu_count;

  if (uv_cpu_info(&cpu_infos, &cpu_count)) {
    LOG("NodeThreadpool::GoodCPUThreadpoolSize: Huh, uv_cpu_info failed?\n");
    return 4;  // Old libuv TP default.
  }

  uv_free_cpu_info(cpu_infos, cpu_count);
    LOG("NodeThreadpool::GoodCPUThreadpoolSize: cpu_count %d\n", cpu_count);
  return cpu_count - 1;  // Leave one core for main loop
}

NodeThreadpool::~NodeThreadpool() {
}

std::shared_ptr<TaskState> NodeThreadpool::Post(std::unique_ptr<Task> task) {
  return tp_->Post(std::move(task));
}

void NodeThreadpool::BlockingDrain() {
  return tp_->BlockingDrain();
}

int NodeThreadpool::QueueLength() const {
  return tp_->QueueLength();
}

int NodeThreadpool::NWorkers() const {
  return tp_->NWorkers();
}

/**************
 * PartitionedNodeThreadpool
 ***************/

PartitionedNodeThreadpool::PartitionedNodeThreadpool() {
  LOG("PartitionedNodeThreadpool::PartitionedNodeThreadpool: default constructor\n");
}

PartitionedNodeThreadpool::PartitionedNodeThreadpool(std::vector<int> tp_sizes) {
  LOG("PartitionedNodeThreadpool::PartitionedNodeThreadpool: vector constructor\n");
  Initialize(tp_sizes);
}

void PartitionedNodeThreadpool::Initialize(const std::vector<int>& tp_sizes) {
  int i = 0;
  for (auto size : tp_sizes) {
    LOG("PartitionedNodeThreadpool::Initialize: tp %d: %d threads\n", i, size);
    std::shared_ptr<Threadpool> tp = std::make_shared<Threadpool>(size);
    tps_.push_back(tp);
    i++;
  }
}

PartitionedNodeThreadpool::~PartitionedNodeThreadpool() {
}

void PartitionedNodeThreadpool::BlockingDrain() {
  for (auto &tp : tps_) {
    tp->BlockingDrain();
  }
}

int PartitionedNodeThreadpool::QueueLength() const {
  int sum = 0;
  for (auto &tp : tps_) {
    sum += tp->QueueLength();
  }
  return sum;
}

int PartitionedNodeThreadpool::NWorkers() const {
  int sum = 0;
  for (auto &tp : tps_) {
    sum += tp->NWorkers();
  }
  return sum;
}

/**************
 * ByTaskTypePartitionedNodeThreadpool
 ***************/

ByTaskTypePartitionedNodeThreadpool::ByTaskTypePartitionedNodeThreadpool(
  std::vector<int> tp_sizes) : CPU_TP_IX(0), IO_TP_IX(1) {
  CHECK_EQ(tp_sizes.size(), 2);

  // CPU TP size
  if (tp_sizes[CPU_TP_IX] <= 0) {
    char buf[32];
    size_t buf_size = sizeof(buf);
    if (uv_os_getenv("NODE_THREADPOOL_CPU_TP_SIZE", buf, &buf_size) == 0) {
      tp_sizes[CPU_TP_IX] = atoi(buf);
    }
  }
  if (tp_sizes[CPU_TP_IX] <= 0) {
    // No/bad env var, so take a guess.
    tp_sizes[CPU_TP_IX] = GoodCPUThreadpoolSize();
  }
  LOG("ByTaskTypePartitionedNodeThreadpool::ByTaskTypePartitionedNodeThreadpool: cpu_pool_size %d\n", tp_sizes[CPU_TP_IX]);
  CHECK_GT(tp_sizes[CPU_TP_IX], 0);

  // IO TP size
  if (tp_sizes[IO_TP_IX] < 0) {
    tp_sizes[IO_TP_IX] = 1 * tp_sizes[CPU_TP_IX];
  }
  LOG("ByTaskTypePartitionedNodeThreadpool::ByTaskTypePartitionedNodeThreadpool: io_pool_size %d\n", tp_sizes[IO_TP_IX]);
  CHECK_GT(tp_sizes[IO_TP_IX], 0);

  Initialize(tp_sizes);
}

ByTaskTypePartitionedNodeThreadpool::~ByTaskTypePartitionedNodeThreadpool() {
}

std::shared_ptr<TaskState> ByTaskTypePartitionedNodeThreadpool::Post(std::unique_ptr<Task> task) {
  switch (task->details_.type) {
    case TaskDetails::MEMORY:
    case TaskDetails::CPU:
    case TaskDetails::CPU_SLOW:
    case TaskDetails::CPU_FAST:
    case TaskDetails::V8:
      LOG("ByTaskTypePartitionedNodeThreadpool::Post: CPU\n");
      return tps_[CPU_TP_IX]->Post(std::move(task));
    default:
      LOG("ByTaskTypePartitionedNodeThreadpool::Post: IO\n");
      return tps_[IO_TP_IX]->Post(std::move(task));
  }
}

/**************
 * WorkerGroup
 ***************/

WorkerGroup::WorkerGroup(int n_workers, std::shared_ptr<TaskQueue> tq)
 : workers_() {
  for (int i = 0; i < n_workers; i++) {
    std::unique_ptr<Worker> worker(new Worker(tq));
    worker->Start();
    workers_.push_back(std::move(worker));
  }
}

WorkerGroup::~WorkerGroup() {
  for (auto& worker : workers_) {
    worker->Join();
  }
}

int WorkerGroup::Size() const {
  return workers_.size();
}

/**************
 * Worker
 ***************/

Worker::Worker(std::shared_ptr<TaskQueue> tq) : tq_(tq) {
}

void Worker::Start() {
  CHECK_EQ(0, uv_thread_create(&self_, _Run, reinterpret_cast<void *>(this)));
}

void Worker::Join() {
  CHECK_EQ(0, uv_thread_join(&self_));
}

void Worker::_Run(void* data) {
  Worker* worker = static_cast<Worker*>(data);

  TaskState::State task_state;
  while (std::unique_ptr<Task> task = worker->tq_->BlockingPop()) {
    // May have been cancelled while queued.
    task_state = task->TryUpdateState(TaskState::ASSIGNED);
    if (task_state == TaskState::ASSIGNED) {
      task->Run();
    } else {
      CHECK_EQ(task_state, TaskState::CANCELLED);
    }

    CHECK_EQ(task->TryUpdateState(TaskState::COMPLETED),
                                  TaskState::COMPLETED);
    worker->tq_->NotifyOfCompletion();
  }
}

/**************
 * Task
 ***************/

Task::Task() : task_state_() {
}

void Task::SetTaskState(std::shared_ptr<TaskState> task_state) {
  task_state_ = task_state;
}

TaskState::State Task::TryUpdateState(TaskState::State new_state) {
  return task_state_->TryUpdateState(new_state);
}


/**************
 * TaskState
 ***************/

TaskState::TaskState() : state_(INITIAL) {
}

TaskState::State TaskState::GetState() const {
  Mutex::ScopedLock scoped_lock(lock_);
  return state_;
}

bool TaskState::Cancel() {
  if (TryUpdateState(CANCELLED) == CANCELLED) {
    return true;
  }
  return false;
}

TaskState::State TaskState::TryUpdateState(TaskState::State new_state) {
  Mutex::ScopedLock scoped_lock(lock_);
  if (ValidStateTransition(state_, new_state)) {
    state_ = new_state;
  }
  return state_;
}

bool TaskState::ValidStateTransition(TaskState::State old_state, TaskState::State new_state) {
  // Normal flow: INITIAL -> QUEUED -> ASSIGNED -> COMPLETED.
  // Also: non-terminal state -> CANCELLED -> COMPLETED.
  switch (old_state) {
    case INITIAL:
      return new_state == QUEUED || new_state == CANCELLED;
    case QUEUED:
      return new_state == ASSIGNED || new_state == CANCELLED;
    case ASSIGNED:
      return new_state == COMPLETED || new_state == CANCELLED;
    case CANCELLED:
      return new_state == COMPLETED;
    // No transitions out of terminal state.
    case COMPLETED:
      return false;
    default:
      CHECK(0);
  }
  return false;
}


/**************
 * LibuvExecutor
 ***************/

class LibuvTaskData;
class LibuvTask;

// Internal LibuvExecutor mechanism to enable uv_cancel.
// Preserves task_state so smart pointers knows not to delete it.
class LibuvTaskData {
 friend class LibuvExecutor;

 public:
  LibuvTaskData(std::shared_ptr<TaskState> state) : state_(state) {
  }

 private:
  std::shared_ptr<TaskState> state_;
};

// The LibuvExecutor wraps libuv uv_work_t's into LibuvTasks
// and routes them to the internal Threadpool.
class LibuvTask : public Task {
 public:
  LibuvTask(LibuvExecutor* libuv_executor,
            uv_work_t* req,
            const uv_work_options_t* opts)
    : Task(), libuv_executor_(libuv_executor), req_(req) {
    CHECK(req_);
    req_->reserved[0] = nullptr;

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

  ~LibuvTask() {
    LOG("LibuvTask::Run: Task %p done\n", req_);
    // Clean up our storage.
    LibuvTaskData* data = reinterpret_cast<LibuvTaskData*>(req_->reserved[0]);
    delete data;
    req_->reserved[0] = nullptr;

    // Inform libuv.
    libuv_executor_->GetExecutor()->done(req_);
  }

  void Run() {
    LOG("LibuvTask::Run: Running Task %p\n", req_);
    req_->work_cb(req_);
  }

 protected:
 private:
  LibuvExecutor* libuv_executor_;
  uv_work_t* req_;
};

LibuvExecutor::LibuvExecutor(std::shared_ptr<NodeThreadpool> tp)
  : tp_(tp) {
  executor_.init = uv_executor_init;
  executor_.destroy = nullptr;
  executor_.submit = uv_executor_submit;
  executor_.cancel = uv_executor_cancel;
  executor_.data = this;
}

uv_executor_t* LibuvExecutor::GetExecutor() {
  return &executor_;
}

bool LibuvExecutor::Cancel(std::shared_ptr<TaskState> task_state) {
  return task_state->Cancel();
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
  LOG("LibuvExecutor::uv_executor_submit: Got work %p\n", req);

  auto task_state = libuv_executor->tp_->Post(std::unique_ptr<Task>(
    new LibuvTask(libuv_executor, req, opts)));
  CHECK(task_state);  // Must not fail. We have no mechanism to tell libuv.

  auto data = new LibuvTaskData(task_state);
  req->reserved[0] = data;
}

// Remember, libuv user won't free uv_work_t until after its done_cb is called.
// That won't happen until after the wrapping LibuvTask is destroyed.
int LibuvExecutor::uv_executor_cancel(uv_executor_t* executor,
                                      uv_work_t* req) {
  if (!req || !req->reserved[0]) {
    return UV_EINVAL;
  }

  LibuvExecutor* libuv_executor =
    reinterpret_cast<LibuvExecutor *>(executor->data);
  LibuvTaskData* task_data =
    reinterpret_cast<LibuvTaskData *>(req->reserved[0]);

  if (libuv_executor->Cancel(task_data->state_)) {
    return 0;
  } else {
    return UV_EBUSY;
  }
}

/**************
 * TaskQueue
 ***************/

TaskQueue::TaskQueue()
  : lock_()
  , task_available_(), tasks_drained_()
  , queue_(), outstanding_tasks_(0), stopped_(false) {
}

bool TaskQueue::Push(std::unique_ptr<Task> task) {
  Mutex::ScopedLock scoped_lock(lock_);

  if (stopped_) {
    return false;
  }

  // The queue contains QUEUED or CANCELLED Tasks.
  // There's little harm in queueing CANCELLED tasks.
  TaskState::State task_state = task->TryUpdateState(TaskState::QUEUED);
  CHECK(task_state == TaskState::QUEUED || task_state == TaskState::CANCELLED);

  queue_.push(std::move(task));
  outstanding_tasks_++;
  task_available_.Signal(scoped_lock);

  return true;
}

std::unique_ptr<Task> TaskQueue::Pop() {
  Mutex::ScopedLock scoped_lock(lock_);

  if (queue_.empty()) {
    return std::unique_ptr<Task>(nullptr);
  }

  std::unique_ptr<Task> task = std::move(queue_.front());
  queue_.pop();
  return task;
}

std::unique_ptr<Task> TaskQueue::BlockingPop() {
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

void TaskQueue::NotifyOfCompletion() {
  Mutex::ScopedLock scoped_lock(lock_);
  outstanding_tasks_--;
  CHECK_GE(outstanding_tasks_, 0);
  if (!outstanding_tasks_) {
    tasks_drained_.Broadcast(scoped_lock);
  }
}

void TaskQueue::BlockingDrain() {
  Mutex::ScopedLock scoped_lock(lock_);
  while (outstanding_tasks_) {
    tasks_drained_.Wait(scoped_lock);
  }
  LOG("TaskQueue::BlockingDrain: Fully drained\n");
}

void TaskQueue::Stop() {
  Mutex::ScopedLock scoped_lock(lock_);
  stopped_ = true;
  task_available_.Broadcast(scoped_lock);
}

int TaskQueue::Length() const {
  Mutex::ScopedLock scoped_lock(lock_);
  return queue_.size();
}

/**************
 * Threadpool
 ***************/

Threadpool::Threadpool(int threadpool_size) {
  CHECK_GT(threadpool_size, 0);
  task_queue_ = std::make_shared<TaskQueue>();
  worker_group_ = std::unique_ptr<WorkerGroup>(
    new WorkerGroup(threadpool_size, task_queue_));
}

Threadpool::~Threadpool() {
  // Block future Push's.
  task_queue_->Stop();
  // As worker_group_ leaves scope, it drains tq and Join's its threads.
}

std::shared_ptr<TaskState> Threadpool::Post(std::unique_ptr<Task> task) {
  LOG("Threadpool::Post: Got task of type %d\n",
    task->details_.type);

  std::shared_ptr<TaskState> task_state = std::make_shared<TaskState>();
  task->SetTaskState(task_state);

  task_queue_->Push(std::move(task));

  return task_state;
}

int Threadpool::QueueLength() const {
  return task_queue_->Length();
}

void Threadpool::BlockingDrain() {
  task_queue_->BlockingDrain();
}

int Threadpool::NWorkers() const {
  return worker_group_->Size();
}

}  // namespace threadpool
}  // namespace node
