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
class TaskSummary;

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

// TODO(davisjam): Who should keep track of time, and for what?
// At what level does the user want to monitor TP performance?
// Presumably they want this info via APIs in NodeThreadpool.
// Tracking it on a per-Task basis might be overkill. But on the other hand
// this would permit users to dynamically identify slower and faster tasks for us.
// Which would be cool.
// If we track this in TaskState, then Task knows about it, and can tell TaskQueue about it,
// which can propagate to Threadpool, which can propagate to NodeThreadpool.

// TODO(davisjam): I don't like all of the 'friend class XXX' I introduced to make the time APIs compile.

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
  int priority;  // Larger numbers signal higher priority.
                 // Does nothing in this class.
  bool cancelable;  // If true, by some yet-to-be-determined mechanism we can
                    // cancel this Task *while* it is scheduled.
};

// Each TaskState is shared by a Task and its Post()'er.
// A TaskState is a two-way communication channel:
//  - The threadpool updates its State and TimeInX.
//  - The Post'er can:
//      - try to Cancel it
//      - monitor how long it spends in the QUEUED and ASSIGNED states.
class TaskState {
 // My friends may TryUpdateState, update my time, etc.
 friend class Task;
 friend class TaskQueue;
 friend class Worker;
 friend class TaskSummary;

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

  // Time in nanoseconds.
  uint64_t TimeInQueue() const;
  uint64_t TimeInRun() const;
  uint64_t TimeInThreadpool() const;

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

  void MarkEnteredQueue();
  void MarkExitedQueue();
  void MarkEnteredRun();
  void MarkExitedRun();

 private:
  uint64_t time_in_queue_;
  uint64_t time_in_run_;

  uint64_t time_entered_queue_;
  uint64_t time_exited_queue_;
  uint64_t time_entered_run_;
  uint64_t time_exited_run_;
};

// Abstract notion of a Task.
// Clients of node::Threadpool should sub-class this for their type of request.
//  - V8::Platform Tasks
//  - libuv async work
//  - User work from the N-API
class Task {
  // For access to task_state_'s time tracking.
 friend class TaskQueue;   
 friend class Worker;
 friend class TaskSummary;

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

class TaskSummary {
 public:
  TaskSummary(Task* completed_task);

  TaskDetails details_;
  uint64_t time_in_queue_;
  uint64_t time_in_run_;
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

class QueueLengthSample {
 public:
  QueueLengthSample(int length, uint64_t time)
   : length_(length), time_(time) {}
 
  int length_;
  uint64_t time_;
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
  TaskQueue(int id =-1);

  // Return true if Push succeeds, else false.
  // Thread-safe.
  bool Push(std::unique_ptr<Task> task);

  // Non-blocking Pop. Returns nullptr if queue is empty.
  std::unique_ptr<Task> Pop();
  // Blocking Pop. Returns nullptr if queue is empty or Stop'd.
  std::unique_ptr<Task> BlockingPop();

  // Workers should call this after completing a Pop'd Task.
  void NotifyOfCompletion(std::unique_ptr<Task> completed_task);

  // Block until there are no Tasks pending or scheduled.
  void BlockingDrain();

  // Subsequent Push() will fail.
  // Pop calls will return nullptr once queue is drained.
  void Stop();

  int Length() const;

  std::vector<std::unique_ptr<TaskSummary>> const& GetTaskSummaries() const;
  std::vector<std::unique_ptr<QueueLengthSample>> const& GetQueueLengths() const;

 private:
  // Caller must hold lock_.
  void UpdateLength(bool grew);

  int id_;

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

  // For statistics tracking.
  int length_;
  int n_changes_since_last_length_sample_;
  int length_report_freq_;
  std::vector<std::unique_ptr<TaskSummary>> task_summaries_;
  std::vector<std::unique_ptr<QueueLengthSample>> queue_lengths_;
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
  explicit Threadpool(int threadpool_size, int id = -1);
  // Waits for queue to drain.
  ~Threadpool();

  int Id() const { return id_; }

  // Thread-safe.
  // Returns a TaskState by which caller can track the progress of the Task.
  // Caller can also use the TaskState to cancel the Task.
  // Returns nullptr on failure.
  // TODO(davisjam): It should not return nullptr on failure.
  // Then the task would be destroyed!
  // Since the underlying queues should not be Stop'd until the Threadpool d'tor,
  // I think it's reasonable that Post will *never* fail.
  std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task);
  // Block until there are no tasks pending or scheduled in the TP.
  void BlockingDrain();

  // Status monitoring
  int QueueLength() const;

  // Attributes
  int NWorkers() const;

  std::vector<std::unique_ptr<TaskSummary>> const& GetTaskSummaries() const;
  std::vector<std::unique_ptr<QueueLengthSample>> const& GetQueueLengths() const;

 protected:
  void Initialize();

 private:
  int id_;

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
  // For default implementation.
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

  virtual std::shared_ptr<TaskState> Post(std::unique_ptr<Task> task);
  // Sub-class can use our Post, but needs to tell us which TP to use.
  virtual int ChooseThreadpool(Task* task) const =0;
  virtual void BlockingDrain() override;

  virtual int QueueLength() const override;

  virtual int NWorkers() const override;

 protected:
  // Sub-classes should call this after computing tp_sizes in their c'tors.
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

  int ChooseThreadpool(Task* task) const;

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

  int ChooseThreadpool(Task* task) const;

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

  int ChooseThreadpool(Task* task) const;

 private:
  int V8_TP_IX;
  int LIBUV_CPU_TP_IX;
  int LIBUV_IO_TP_IX;
};

}  // namespace threadpool
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_THREADPOOL_H_
