#ifndef SRC_NODE_THREADPOOL_H_
#define SRC_NODE_THREADPOOL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <queue>
#include <vector>
#include <functional>

#include "node.h"
#include "node_mutex.h"
#include "uv.h"

namespace node {
namespace threadpool {

// Consumer of Threadpool.
class LibuvExecutor;

// Threadpool components.
class Threadpool;

class TaskQueue;
class WorkerGroup;

class Task;
class TaskDetails;
class TaskState;

class Worker;

// Represents a set of Workers
class WorkerGroup {
 public:
  WorkerGroup(int n_workers, std::shared_ptr<TaskQueue> tq);
  // Assumes tq has been Stop'd by its owner.
  ~WorkerGroup();

  int Size() const;

 private:
  std::vector<std::unique_ptr<Worker>> workers_;
};

// Inhabited by a uv_thread_t.
// Subclass to experiment, e.g.:
//   - cancellation (a la Davis et al. 2018's Manager-Worker-Hangman approach)
class Worker {
 public:
  Worker(std::shared_ptr<TaskQueue> tq);

  // Starts a thread and returns control to the caller.
  void Start();
  // Join the internal uv_thread_t.
  void Join(void);

 protected:
  // Override e.g. to implement cancellation.
  static void _Run(void* data);

  uv_thread_t self_;
  std::shared_ptr<TaskQueue> tq_;
};

// This is basically a struct
class TaskDetails {
 public:
  enum TaskType {
      FS
    , FS_LIKELY_CACHED  // Likely to be bound by memory or CPU
    , OTHER_DISK_IO
    , DNS
    , OTHER_NETWORK_IO
    , IO
    , MEMORY
    , CPU
    , CPU_SLOW
    , CPU_FAST
    , V8
    , UNKNOWN
  };

  TaskType type;
  int priority;  // Larger numbers signal higher priority.i
                 // Does nothing in this class.
  bool cancelable;  // If true, by some yet-to-be-determined mechanism we can
                    // cancel this Task while it is scheduled.
};

// Each TaskState is shared by a Task and its Post()'er.
// A TaskState is a two-way communication channel:
//  - The threadpool updates its State
//  - The Post'er can try to Cancel it
//
// TODO(davisjam): Could add tracking of how long
//  it spent in QUEUED, ASSIGNED, COMPLETED states,
//  and what its total lifetime was.
class TaskState {
 // My friends can call TryUpdateState.
 friend class Task;

 public:
  enum State {
      INITIAL
    , QUEUED
    , ASSIGNED
    , COMPLETED // Terminal state
    , CANCELLED
  };

  TaskState();

  // For the benefit of an impatient Post'er.
  State GetState() const;

  // Attempt to cancel the associated Task.
  bool Cancel();

 protected:
  // Synchronization.
  Mutex lock_;

  // Different Threadpool components should update this as the
  // Task travels around.
  // Returns the state after the attempted update.
  // Thread safe.
  State TryUpdateState(State new_state);

  // Caller must hold lock_.
  bool ValidStateTransition(State old_state, State new_state);

  State state_;
};

// Abstract notion of a Task.
// Clients of node::Threadpool should sub-class this for their type of request.
//  - V8::Platform Tasks
//  - libuv async work
//  - User work from the N-API
class Task {
 public:
  // Subclasses should set details_ in their constructor.
  Task();
  // Invoked after Run().
  virtual ~Task() {}

  void SetTaskState(std::shared_ptr<TaskState> task_state);

  // Invoked on some thread in the Threadpool.
  virtual void Run() = 0;

  TaskState::State TryUpdateState(TaskState::State new_state);

  TaskDetails details_;

 protected:
  std::shared_ptr<TaskState> task_state_;
};

// Shim that we plug into the libuv "pluggable TP" interface.
//
// Like WorkerThreadsTaskRunner, this routes libuv requests to the
// internal Node.js Threadpool.
class LibuvExecutor {
 public:
  explicit LibuvExecutor(std::shared_ptr<Threadpool> tp);

  uv_executor_t* GetExecutor();

  // Returns true on success.
  bool Cancel(std::shared_ptr<TaskState> task_state);

 private:
  // These redirect into appropriate public methods.
  static void uv_executor_init(uv_executor_t* executor);
  static void uv_executor_submit(uv_executor_t* executor,
                                 uv_work_t* req,
                                 const uv_work_options_t* opts);
  static int uv_executor_cancel(uv_executor_t* executor,
                                uv_work_t* req);

  std::shared_ptr<Threadpool> tp_;
  uv_executor_t executor_;  // executor_.data points to an
                            // instance of LibuvExecutor.
};

// Abstract notion of a queue of Tasks.
// The default implementation is a FIFO queue.
// Subclass to experiment, e.g.:
//   - prioritization
//   - multi-queue e.g. for CPU-bound and I/O-bound Tasks or Fast and Slow ones.
//
// All Tasks Push'd to TaskQueue should have been assigned a TaskState.
// The TaskQueue contains both QUEUED and CANCELLED Tasks.
// Users should check the state of Tasks they Pop.
class TaskQueue {
 public:
  TaskQueue();

  // Return true if Push succeeds, else false.
  bool Push(std::unique_ptr<Task> task);

  // Non-blocking Pop. Returns nullptr if queue is empty.
  std::unique_ptr<Task> Pop(void);
  // Blocking Pop. Returns nullptr if queue is empty or Stop'd.
  std::unique_ptr<Task> BlockingPop(void);

  // Workers should call this after completing a Pop'd Task.
  void NotifyOfCompletion(void);

  // Block until there are no Tasks pending or scheduled.
  void BlockingDrain(void);

  // Subsequent Push() will fail.
  // Pop calls will return nullptr once queue is drained.
  void Stop();

  int Length(void) const;

 private:
  // Synchronization.
  Mutex lock_;
  // Signal'd when there is at least one task in the queue.
  ConditionVariable task_available_;
  // Signal'd when all Push'd Tasks are in COMPLETED state.
  ConditionVariable tasks_drained_;

  // Structures.
  std::queue<std::unique_ptr<Task>> queue_;
  int outstanding_tasks_;  // Number of Tasks in non-COMPLETED states.
  bool stopped_;
};

// A threadpool works on asynchronous Tasks.
// It consists of:
//   - a TaskQueue of pending Tasks
//   - a set of Workers that handle Tasks from the TaskQueue
// Subclass to experiment, e.g.:
//   - Use a different type of TaskQueue
//   - Elastic workers (scale up and down)
class Threadpool {
 public:
  // If threadpool_size <= 0:
  //   - checks UV_THREADPOOL_SIZE to determine threadpool_size
  //   - if this is not set, takes a guess
  // TODO(davisjam): Ponder --v8-pool-size and UV_THREADPOOL_SIZE.
  explicit Threadpool(int threadpool_size);
  // Waits for queue to drain.
  ~Threadpool(void);

  // Returns a TaskState by which caller can track the progress of the Task.
  // Caller can also use the TaskState to cancel the Task.
  // Returns nullptr on failure.
  std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task);
  int QueueLength(void) const;
  // Block until there are no tasks pending or scheduled in the TP.
  void BlockingDrain(void);

  int NWorkers(void) const;

 private:
  int GoodThreadpoolSize(void);
  void Initialize(void);

  int threadpool_size_;
  std::shared_ptr<TaskQueue> task_queue_;
  std::unique_ptr<WorkerGroup> worker_group_;
};

}  // namespace threadpool
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_THREADPOOL_H_
