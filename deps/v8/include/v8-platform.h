// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8_PLATFORM_H_
#define V8_V8_PLATFORM_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>  // For abort.
#include <memory>
#include <string>

#include "v8config.h"  // NOLINT(build/include)

namespace v8 {

class Isolate;

/**
 * A Task represents a unit of work.
 */
class Task {
 public:
  virtual ~Task() = default;

  virtual void Run() = 0;
};

/**
 * An IdleTask represents a unit of work to be performed in idle time.
 * The Run method is invoked with an argument that specifies the deadline in
 * seconds returned by MonotonicallyIncreasingTime().
 * The idle task is expected to complete by this deadline.
 */
class IdleTask {
 public:
  virtual ~IdleTask() = default;
  virtual void Run(double deadline_in_seconds) = 0;
};

/**
 * A TaskRunner allows scheduling of tasks. The TaskRunner may still be used to
 * post tasks after the isolate gets destructed, but these tasks may not get
 * executed anymore. All tasks posted to a given TaskRunner will be invoked in
 * sequence. Tasks can be posted from any thread.
 */
class TaskRunner {
 public:
  /**
   * Schedules a task to be invoked by this TaskRunner. The TaskRunner
   * implementation takes ownership of |task|.
   */
  virtual void PostTask(std::unique_ptr<Task> task) = 0;

  /**
   * Schedules a task to be invoked by this TaskRunner. The TaskRunner
   * implementation takes ownership of |task|. The |task| cannot be nested
   * within other task executions.
   *
   * Requires that |TaskRunner::NonNestableTasksEnabled()| is true.
   */
  virtual void PostNonNestableTask(std::unique_ptr<Task> task) {}

  /**
   * Schedules a task to be invoked by this TaskRunner. The task is scheduled
   * after the given number of seconds |delay_in_seconds|. The TaskRunner
   * implementation takes ownership of |task|.
   */
  virtual void PostDelayedTask(std::unique_ptr<Task> task,
                               double delay_in_seconds) = 0;

  /**
   * Schedules a task to be invoked by this TaskRunner. The task is scheduled
   * after the given number of seconds |delay_in_seconds|. The TaskRunner
   * implementation takes ownership of |task|. The |task| cannot be nested
   * within other task executions.
   *
   * Requires that |TaskRunner::NonNestableDelayedTasksEnabled()| is true.
   */
  virtual void PostNonNestableDelayedTask(std::unique_ptr<Task> task,
                                          double delay_in_seconds) {}

  /**
   * Schedules an idle task to be invoked by this TaskRunner. The task is
   * scheduled when the embedder is idle. Requires that
   * |TaskRunner::IdleTasksEnabled()| is true. Idle tasks may be reordered
   * relative to other task types and may be starved for an arbitrarily long
   * time if no idle time is available. The TaskRunner implementation takes
   * ownership of |task|.
   */
  virtual void PostIdleTask(std::unique_ptr<IdleTask> task) = 0;

  /**
   * Returns true if idle tasks are enabled for this TaskRunner.
   */
  virtual bool IdleTasksEnabled() = 0;

  /**
   * Returns true if non-nestable tasks are enabled for this TaskRunner.
   */
  virtual bool NonNestableTasksEnabled() const { return false; }

  /**
   * Returns true if non-nestable delayed tasks are enabled for this TaskRunner.
   */
  virtual bool NonNestableDelayedTasksEnabled() const { return false; }

  TaskRunner() = default;
  virtual ~TaskRunner() = default;

 private:
  TaskRunner(const TaskRunner&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;
};

/**
 * The interface represents complex arguments to trace events.
 */
class ConvertableToTraceFormat {
 public:
  virtual ~ConvertableToTraceFormat() = default;

  /**
   * Append the class info to the provided |out| string. The appended
   * data must be a valid JSON object. Strings must be properly quoted, and
   * escaped. There is no processing applied to the content after it is
   * appended.
   */
  virtual void AppendAsTraceFormat(std::string* out) const = 0;
};

/**
 * V8 Tracing controller.
 *
 * Can be implemented by an embedder to record trace events from V8.
 */
class TracingController {
 public:
  virtual ~TracingController() = default;

  /**
   * Called by TRACE_EVENT* macros, don't call this directly.
   * The name parameter is a category group for example:
   * TRACE_EVENT0("v8,parse", "V8.Parse")
   * The pointer returned points to a value with zero or more of the bits
   * defined in CategoryGroupEnabledFlags.
   **/
  virtual const uint8_t* GetCategoryGroupEnabled(const char* name) {
    static uint8_t no = 0;
    return &no;
  }

  /**
   * Adds a trace event to the platform tracing system. These function calls are
   * usually the result of a TRACE_* macro from trace_event_common.h when
   * tracing and the category of the particular trace are enabled. It is not
   * advisable to call these functions on their own; they are really only meant
   * to be used by the trace macros. The returned handle can be used by
   * UpdateTraceEventDuration to update the duration of COMPLETE events.
   */
  virtual uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) {
    return 0;
  }
  virtual uint64_t AddTraceEventWithTimestamp(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags, int64_t timestamp) {
    return 0;
  }

  /**
   * Sets the duration field of a COMPLETE trace event. It must be called with
   * the handle returned from AddTraceEvent().
   **/
  virtual void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                        const char* name, uint64_t handle) {}

  class TraceStateObserver {
   public:
    virtual ~TraceStateObserver() = default;
    virtual void OnTraceEnabled() = 0;
    virtual void OnTraceDisabled() = 0;
  };

  /** Adds tracing state change observer. */
  virtual void AddTraceStateObserver(TraceStateObserver*) {}

  /** Removes tracing state change observer. */
  virtual void RemoveTraceStateObserver(TraceStateObserver*) {}
};

/**
 * A V8 memory page allocator.
 *
 * Can be implemented by an embedder to manage large host OS allocations.
 */
class PageAllocator {
 public:
  virtual ~PageAllocator() = default;

  /**
   * Gets the page granularity for AllocatePages and FreePages. Addresses and
   * lengths for those calls should be multiples of AllocatePageSize().
   */
  virtual size_t AllocatePageSize() = 0;

  /**
   * Gets the page granularity for SetPermissions and ReleasePages. Addresses
   * and lengths for those calls should be multiples of CommitPageSize().
   */
  virtual size_t CommitPageSize() = 0;

  /**
   * Sets the random seed so that GetRandomMmapAddr() will generate repeatable
   * sequences of random mmap addresses.
   */
  virtual void SetRandomMmapSeed(int64_t seed) = 0;

  /**
   * Returns a randomized address, suitable for memory allocation under ASLR.
   * The address will be aligned to AllocatePageSize.
   */
  virtual void* GetRandomMmapAddr() = 0;

  /**
   * Memory permissions.
   */
  enum Permission {
    kNoAccess,
    kRead,
    kReadWrite,
    // TODO(hpayer): Remove this flag. Memory should never be rwx.
    kReadWriteExecute,
    kReadExecute
  };

  /**
   * Allocates memory in range with the given alignment and permission.
   */
  virtual void* AllocatePages(void* address, size_t length, size_t alignment,
                              Permission permissions) = 0;

  /**
   * Frees memory in a range that was allocated by a call to AllocatePages.
   */
  virtual bool FreePages(void* address, size_t length) = 0;

  /**
   * Releases memory in a range that was allocated by a call to AllocatePages.
   */
  virtual bool ReleasePages(void* address, size_t length,
                            size_t new_length) = 0;

  /**
   * Sets permissions on pages in an allocated range.
   */
  virtual bool SetPermissions(void* address, size_t length,
                              Permission permissions) = 0;

  /**
   * Frees memory in the given [address, address + size) range. address and size
   * should be operating system page-aligned. The next write to this
   * memory area brings the memory transparently back.
   */
  virtual bool DiscardSystemPages(void* address, size_t size) { return true; }
};

/**
 * V8 Platform abstraction layer.
 *
 * The embedder has to provide an implementation of this interface before
 * initializing the rest of V8.
 */
class Platform {
 public:
  virtual ~Platform() = default;

  /**
   * Allows the embedder to manage memory page allocations.
   */
  virtual PageAllocator* GetPageAllocator() {
    // TODO(bbudge) Make this abstract after all embedders implement this.
    return nullptr;
  }

  /**
   * Enables the embedder to respond in cases where V8 can't allocate large
   * blocks of memory. V8 retries the failed allocation once after calling this
   * method. On success, execution continues; otherwise V8 exits with a fatal
   * error.
   * Embedder overrides of this function must NOT call back into V8.
   */
  virtual void OnCriticalMemoryPressure() {
    // TODO(bbudge) Remove this when embedders override the following method.
    // See crbug.com/634547.
  }

  /**
   * Enables the embedder to respond in cases where V8 can't allocate large
   * memory regions. The |length| parameter is the amount of memory needed.
   * Returns true if memory is now available. Returns false if no memory could
   * be made available. V8 will retry allocations until this method returns
   * false.
   *
   * Embedder overrides of this function must NOT call back into V8.
   */
  virtual bool OnCriticalMemoryPressure(size_t length) { return false; }

  /**
   * Gets the number of worker threads used by
   * Call(BlockingTask)OnWorkerThread(). This can be used to estimate the number
   * of tasks a work package should be split into. A return value of 0 means
   * that there are no worker threads available. Note that a value of 0 won't
   * prohibit V8 from posting tasks using |CallOnWorkerThread|.
   */
  virtual int NumberOfWorkerThreads() = 0;

  /**
   * Returns a TaskRunner which can be used to post a task on the foreground.
   * This function should only be called from a foreground thread.
   */
  virtual std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      Isolate* isolate) = 0;

  /**
   * Schedules a task to be invoked on a worker thread.
   */
  virtual void CallOnWorkerThread(std::unique_ptr<Task> task) = 0;

  /**
   * Schedules a task that blocks the main thread to be invoked with
   * high-priority on a worker thread.
   */
  virtual void CallBlockingTaskOnWorkerThread(std::unique_ptr<Task> task) {
    // Embedders may optionally override this to process these tasks in a high
    // priority pool.
    CallOnWorkerThread(std::move(task));
  }

  /**
   * Schedules a task to be invoked with low-priority on a worker thread.
   */
  virtual void CallLowPriorityTaskOnWorkerThread(std::unique_ptr<Task> task) {
    // Embedders may optionally override this to process these tasks in a low
    // priority pool.
    CallOnWorkerThread(std::move(task));
  }

  /**
   * Schedules a task to be invoked on a worker thread after |delay_in_seconds|
   * expires.
   */
  virtual void CallDelayedOnWorkerThread(std::unique_ptr<Task> task,
                                         double delay_in_seconds) = 0;

  /**
   * Schedules a task to be invoked on a foreground thread wrt a specific
   * |isolate|. Tasks posted for the same isolate should be execute in order of
   * scheduling. The definition of "foreground" is opaque to V8.
   */
  V8_DEPRECATE_SOON(
      "Use a taskrunner acquired by GetForegroundTaskRunner instead.",
      virtual void CallOnForegroundThread(Isolate* isolate, Task* task)) = 0;

  /**
   * Schedules a task to be invoked on a foreground thread wrt a specific
   * |isolate| after the given number of seconds |delay_in_seconds|.
   * Tasks posted for the same isolate should be execute in order of
   * scheduling. The definition of "foreground" is opaque to V8.
   */
  V8_DEPRECATE_SOON(
      "Use a taskrunner acquired by GetForegroundTaskRunner instead.",
      virtual void CallDelayedOnForegroundThread(Isolate* isolate, Task* task,
                                                 double delay_in_seconds)) = 0;

  /**
   * Schedules a task to be invoked on a foreground thread wrt a specific
   * |isolate| when the embedder is idle.
   * Requires that SupportsIdleTasks(isolate) is true.
   * Idle tasks may be reordered relative to other task types and may be
   * starved for an arbitrarily long time if no idle time is available.
   * The definition of "foreground" is opaque to V8.
   */
  V8_DEPRECATE_SOON(
      "Use a taskrunner acquired by GetForegroundTaskRunner instead.",
      virtual void CallIdleOnForegroundThread(Isolate* isolate,
                                              IdleTask* task)) {
    // This must be overriden if |IdleTasksEnabled()|.
    abort();
  }

  /**
   * Returns true if idle tasks are enabled for the given |isolate|.
   */
  virtual bool IdleTasksEnabled(Isolate* isolate) {
    return false;
  }

  /**
   * Monotonically increasing time in seconds from an arbitrary fixed point in
   * the past. This function is expected to return at least
   * millisecond-precision values. For this reason,
   * it is recommended that the fixed point be no further in the past than
   * the epoch.
   **/
  virtual double MonotonicallyIncreasingTime() = 0;

  /**
   * Current wall-clock time in milliseconds since epoch.
   * This function is expected to return at least millisecond-precision values.
   */
  virtual double CurrentClockTimeMillis() = 0;

  typedef void (*StackTracePrinter)();

  /**
   * Returns a function pointer that print a stack trace of the current stack
   * on invocation. Disables printing of the stack trace if nullptr.
   */
  virtual StackTracePrinter GetStackTracePrinter() { return nullptr; }

  /**
   * Returns an instance of a v8::TracingController. This must be non-nullptr.
   */
  virtual TracingController* GetTracingController() = 0;

  /**
   * Tells the embedder to generate and upload a crashdump during an unexpected
   * but non-critical scenario.
   */
  virtual void DumpWithoutCrashing() {}

 protected:
  /**
   * Default implementation of current wall-clock time in milliseconds
   * since epoch. Useful for implementing |CurrentClockTimeMillis| if
   * nothing special needed.
   */
  V8_EXPORT static double SystemClockTimeMillis();
};

}  // namespace v8

#endif  // V8_V8_PLATFORM_H_
