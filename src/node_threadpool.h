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

class LibuvExecutor;
class Threadpool;
class TaskQueue;
class Task;
class TaskDetails;
class Worker;

// Inhabited by a uv_thread_t.
// Subclass to experiment, e.g.:
//   - cancellation (a la Davis et al. 2018's Manager-Worker-Hangman approach)
class Worker {
 public:
  Worker();

  // Starts a thread and returns control to the caller.
  void Start(TaskQueue* queue);
  void Join(void);

 protected:
  // Override e.g. to implement cancellation.
  static void _Run(void* data);

  uv_thread_t self_;

 private:
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

// Abstract notion of a Task.
// Clients of node::Threadpool should sub-class this for their type of request.
//  - V8::Platform Tasks
//  - libuv async work
//  - User work from the N-API
class Task {
 public:
  enum State {
      QUEUED
    , ASSIGNED
    , COMPLETED
  };

  Task() {}
  // Invoked after Run().
  virtual ~Task() {}

  // Invoked on some thread in the Threadpool.
  virtual void Run() = 0;

  // Different Threadpool components should update this as the
  // Task travels around.
  void UpdateState(enum State state);

  // Run() can access details.
  // Should be set in subclass constructor.
  TaskDetails details_;

 protected:
  enum State state_;
};

// Shim that we plug into the libuv "pluggable TP" interface.
//
// Like WorkerThreadsTaskRunner, this routes libuv requests to the
// internal Node.js Threadpool.
class LibuvExecutor {
 public:
  explicit LibuvExecutor(std::shared_ptr<Threadpool> tp);

  uv_executor_t* GetExecutor();

 private:
  static void uv_executor_init(uv_executor_t* executor);
  static void uv_executor_submit(uv_executor_t* executor,
                                 uv_work_t* req,
                                 const uv_work_options_t* opts);

  std::shared_ptr<Threadpool> tp_;
  uv_executor_t executor_;  // executor_.data points to
                            // instance of LibuvExecutor.
};

// The LibuvExecutor wraps libuv uv_work_t's into LibuvTasks
// and routes them to the internal Threadpool.
class LibuvTask : public Task {
 public:
  LibuvTask(LibuvExecutor* libuv_executor,
            uv_work_t* req,
            const uv_work_options_t* opts);
  ~LibuvTask();

  void Run();

 protected:
 private:
  LibuvExecutor* libuv_executor_;
  uv_work_t* req_;
};

// Abstract notion of a queue of Tasks.
// The default implementation is a FIFO queue.
// Subclass to experiment, e.g.:
//   - prioritization
//   - multi-queue e.g. for CPU-bound and I/O-bound Tasks or Fast and Slow ones.
class TaskQueue {
 public:
  TaskQueue();

  // Return true if Push succeeds, else false.
  bool Push(std::unique_ptr<Task> task);

  // Non-blocking Pop. Returns nullptr if queue is empty.
  std::unique_ptr<Task> Pop(void);
  // Blocking Pop. Returns nullptr if queue is empty or Stop'd.
  std::unique_ptr<Task> BlockingPop(void);

  // Workers should call this after completing a Task.
  void NotifyOfCompletion(void);

  // Block until there are no Tasks pending or scheduled.
  void BlockingDrain(void);

  // Subsequent Push() will fail.
  // Pop calls will return nullptr once queue is drained.
  void Stop();

  int Length(void) const;

 private:
  // Structures.
  std::queue<std::unique_ptr<Task>> queue_;
  int outstanding_tasks_;  // Number of Tasks in non-COMPLETED states.
  bool stopped_;

  // Synchronization.
  Mutex lock_;
  // Signal'd when there is at least one task in the queue.
  ConditionVariable task_available_;
  // Signal'd when all Push'd Tasks are in COMPLETED state.
  ConditionVariable tasks_drained_;
};

// A threadpool works on asynchronous Tasks.
// It consists of:
//   - a TaskQueue of pending Tasks
//   - a set of Workers that handle Tasks from the TaskQueue
// Subclass to experiment, e.g.:
//   - Use a different type of TaskQueue
//   - Elastic workers (scale up and down)
//
// TODO(davisjam): Thread pool size recommendation.
class Threadpool {
 public:
  // TODO(davisjam): RAII.
  Threadpool(void);
  // Waits for queue to drain.
  ~Threadpool(void);

  // Call once, before you Post.
  // TODO(davisjam): Remove, replace with RAII.
  void Initialize(void);

  void Post(std::unique_ptr<Task> task);
  int QueueLength(void) const;
  // Block until there are no tasks pending or scheduled in the TP.
  void BlockingDrain(void);

  int NWorkers(void) const;

 private:
  TaskQueue queue_;
  std::vector<std::unique_ptr<Worker>> workers_;
};

}  // namespace threadpool
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_THREADPOOL_H_
