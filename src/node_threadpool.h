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

class NodeThreadpool;

// Consumer of NodeThreadpool.
class LibuvExecutor;

// NodeThreadpool components.
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
  void Join();

 protected:
  // Override e.g. to implement cancellation.
  static void _Run(void* data);

  uv_thread_t self_;
  std::shared_ptr<TaskQueue> tq_;
};

// This is basically a struct
class TaskDetails {
 public:
  enum TaskOrigin {
       V8
     , LIBUV
     , USER   // N-API
     , TASK_ORIGIN_UNKNOWN
  };

  enum TaskType {
      FS
    , DNS
    , IO
    , MEMORY
    , CPU
    , TASK_TYPE_UNKNOWN
  };

  enum TaskSize {
      SMALL
    , LARGE
    , TASK_SIZE_UNKNOWN
  };

  TaskOrigin origin;
  TaskType type;
  TaskSize size;
  int priority;  // Larger numbers signal higher priority.i
                 // Does nothing in this class.
  bool cancelable;  // If true, by some yet-to-be-determined mechanism we can
                    // cancel this Task *while* it is scheduled.
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
  explicit LibuvExecutor(std::shared_ptr<NodeThreadpool> tp);

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

  std::shared_ptr<NodeThreadpool> tp_;
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
  std::unique_ptr<Task> Pop();
  // Blocking Pop. Returns nullptr if queue is empty or Stop'd.
  std::unique_ptr<Task> BlockingPop();

  // Workers should call this after completing a Pop'd Task.
  void NotifyOfCompletion();

  // Block until there are no Tasks pending or scheduled.
  void BlockingDrain();

  // Subsequent Push() will fail.
  // Pop calls will return nullptr once queue is drained.
  void Stop();

  int Length() const;

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
  explicit Threadpool(int threadpool_size);
  // Waits for queue to drain.
  ~Threadpool();

  // Returns a TaskState by which caller can track the progress of the Task.
  // Caller can also use the TaskState to cancel the Task.
  // Returns nullptr on failure.
  std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task);
  // Block until there are no tasks pending or scheduled in the TP.
  void BlockingDrain();

  // Status monitoring
  int QueueLength() const;

  // Attributes
  int NWorkers() const;

 protected:
  void Initialize();

 private:
  std::shared_ptr<TaskQueue> task_queue_;
  std::unique_ptr<WorkerGroup> worker_group_;
};

// Public face of the threadpool.
// Subclass for customized threadpool(s).
class NodeThreadpool {
 public:
  // TODO(davisjam): Is this OK? It permits sub-classing.
  // But maybe we should take an interface approach and have all of these virtual
  // methods be pure virtual?
  NodeThreadpool();
  // If threadpool_size <= 0:
  //   - checks UV_THREADPOOL_SIZE to determine threadpool_size
  //   - if this is not set, takes a guess
  // TODO(davisjam): Ponder --v8-pool-size and UV_THREADPOOL_SIZE.
  explicit NodeThreadpool(int threadpool_size);
  // Waits for queue to drain.
  ~NodeThreadpool();

  // Returns a TaskState by which caller can track the progress of the Task.
  // Caller can also use the TaskState to cancel the Task.
  // Returns nullptr on failure.
  virtual std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task);
  // Block until there are no tasks pending or scheduled in the TP.
  virtual void BlockingDrain();

  // Status monitoring
  virtual int QueueLength() const;

  // Attributes
  virtual int NWorkers() const;

 protected:
  int GoodCPUThreadpoolSize();

 private:
  std::shared_ptr<Threadpool> tp_;
};

// Maintains multiple Threadpools
class PartitionedNodeThreadpool : public NodeThreadpool {
 public:
  // So sub-classes can define their own constructors.
  PartitionedNodeThreadpool();
  // Create tp_sizes.size() TPs with these sizes.
  explicit PartitionedNodeThreadpool(std::vector<int> tp_sizes);
  // Waits for queue to drain.
  ~PartitionedNodeThreadpool();

  virtual std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task) =0;
  virtual void BlockingDrain() override;

  virtual int QueueLength() const override;

  virtual int NWorkers() const override;

 protected:
  // Permits sub-classes to compute tp_sizes as needed.
  void Initialize(const std::vector<int>& tp_sizes);
  std::vector<std::shared_ptr<Threadpool>> tps_;
};

// Splits based on task origin: V8 or libuv
class ByTaskOriginPartitionedNodeThreadpool : public PartitionedNodeThreadpool {
 public:
  // tp_sizes[0] is V8, tp_sizes[1] is libuv
  // tp_sizes[0] -1: reads NODE_THREADPOOL_V8_TP_SIZE, or guesses based on # cores
  // tp_sizes[1] -1: reads UV_THREADPOOL_SIZE, defaults to 4
  explicit ByTaskOriginPartitionedNodeThreadpool(std::vector<int> tp_sizes);
  // Waits for queue to drain.
  ~ByTaskOriginPartitionedNodeThreadpool();

  virtual std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task) override;

 private:
  int V8_TP_IX;
  int LIBUV_TP_IX;
};

// Splits based on task type: CPU or I/O
class ByTaskTypePartitionedNodeThreadpool : public PartitionedNodeThreadpool {
 public:
  // tp_sizes[0] is CPU, tp_sizes[1] is I/O
  // tp_sizes[0] -1: reads NODE_THREADPOOL_CPU_TP_SIZE, or guesses based on # cores
  // tp_sizes[1] -1: reads NODE_THREADPOOL_IO_TP_SIZE, or guesses based on # cores
  explicit ByTaskTypePartitionedNodeThreadpool(std::vector<int> tp_sizes);
  // Waits for queue to drain.
  ~ByTaskTypePartitionedNodeThreadpool();

  virtual std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task) override;

 private:
  int CPU_TP_IX;
  int IO_TP_IX;
};

// Splits based on task origin and type: V8 or libuv-{CPU or I/O}
class ByTaskOriginAndTypePartitionedNodeThreadpool : public PartitionedNodeThreadpool {
 public:
  // tp_sizes[0] is V8, tp_sizes[1] is libuv-CPU, tp_sizes[2] is libuv-I/O
  // tp_sizes[1] -1: reads NODE_THREADPOOL_UVTP_CPU_TP_SIZE, or guesses based on # cores
  // tp_sizes[2] -1: reads NODE_THREADPOOL_UVTP_IO_TP_SIZE, or guesses based on # cores
  explicit ByTaskOriginAndTypePartitionedNodeThreadpool(std::vector<int> tp_sizes);
  // Waits for queue to drain.
  ~ByTaskOriginAndTypePartitionedNodeThreadpool();

  virtual std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task) override;

 private:
  int V8_TP_IX;
  int LIBUV_CPU_TP_IX;
  int LIBUV_IO_TP_IX;
};

}  // namespace threadpool
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_THREADPOOL_H_
