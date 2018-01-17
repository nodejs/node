// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8_PLATFORM_H_
#define V8_V8_PLATFORM_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>

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
   * Schedules a task to be invoked by this TaskRunner. The task is scheduled
   * after the given number of seconds |delay_in_seconds|. The TaskRunner
   * implementation takes ownership of |task|.
   */
  virtual void PostDelayedTask(std::unique_ptr<Task> task,
                               double delay_in_seconds) = 0;

  /**
   * Schedules an idle task to be invoked by this TaskRunner. The task is
   * scheduled when the embedder is idle. Requires that
   * TaskRunner::SupportsIdleTasks(isolate) is true. Idle tasks may be reordered
   * relative to other task types and may be starved for an arbitrarily long
   * time if no idle time is available. The TaskRunner implementation takes
   * ownership of |task|.
   */
  virtual void PostIdleTask(std::unique_ptr<IdleTask> task) = 0;

  /**
   * Returns true if idle tasks are enabled for this TaskRunner.
   */
  virtual bool IdleTasksEnabled() = 0;

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
 * V8 Platform abstraction layer.
 *
 * The embedder has to provide an implementation of this interface before
 * initializing the rest of V8.
 */
class Platform {
 public:
  /**
   * This enum is used to indicate whether a task is potentially long running,
   * or causes a long wait. The embedder might want to use this hint to decide
   * whether to execute the task on a dedicated thread.
   */
  enum ExpectedRuntime {
    kShortRunningTask,
    kLongRunningTask
  };

  virtual ~Platform() = default;

  /**
   * Enables the embedder to respond in cases where V8 can't allocate large
   * blocks of memory. V8 retries the failed allocation once after calling this
   * method. On success, execution continues; otherwise V8 exits with a fatal
   * error.
   * Embedder overrides of this function must NOT call back into V8.
   */
  virtual void OnCriticalMemoryPressure() {}

  /**
   * Gets the number of threads that are used to execute background tasks. Is
   * used to estimate the number of tasks a work package should be split into.
   * A return value of 0 means that there are no background threads available.
   * Note that a value of 0 won't prohibit V8 from posting tasks using
   * |CallOnBackgroundThread|.
   */
  virtual size_t NumberOfAvailableBackgroundThreads() { return 0; }

  /**
   * Returns a TaskRunner which can be used to post a task on the foreground.
   * This function should only be called from a foreground thread.
   */
  virtual std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      Isolate* isolate) {
    // TODO(ahaas): Make this function abstract after it got implemented on all
    // platforms.
    return {};
  }

  /**
   * Returns a TaskRunner which can be used to post a task on a background.
   * This function should only be called from a foreground thread.
   */
  virtual std::shared_ptr<v8::TaskRunner> GetBackgroundTaskRunner(
      Isolate* isolate) {
    // TODO(ahaas): Make this function abstract after it got implemented on all
    // platforms.
    return {};
  }

  /**
   * Schedules a task to be invoked on a background thread. |expected_runtime|
   * indicates that the task will run a long time. The Platform implementation
   * takes ownership of |task|. There is no guarantee about order of execution
   * of tasks wrt order of scheduling, nor is there a guarantee about the
   * thread the task will be run on.
   */
  virtual void CallOnBackgroundThread(Task* task,
                                      ExpectedRuntime expected_runtime) = 0;

  /**
   * Schedules a task to be invoked on a foreground thread wrt a specific
   * |isolate|. Tasks posted for the same isolate should be execute in order of
   * scheduling. The definition of "foreground" is opaque to V8.
   */
  virtual void CallOnForegroundThread(Isolate* isolate, Task* task) = 0;

  /**
   * Schedules a task to be invoked on a foreground thread wrt a specific
   * |isolate| after the given number of seconds |delay_in_seconds|.
   * Tasks posted for the same isolate should be execute in order of
   * scheduling. The definition of "foreground" is opaque to V8.
   */
  virtual void CallDelayedOnForegroundThread(Isolate* isolate, Task* task,
                                             double delay_in_seconds) = 0;

  /**
   * Schedules a task to be invoked on a foreground thread wrt a specific
   * |isolate| when the embedder is idle.
   * Requires that SupportsIdleTasks(isolate) is true.
   * Idle tasks may be reordered relative to other task types and may be
   * starved for an arbitrarily long time if no idle time is available.
   * The definition of "foreground" is opaque to V8.
   */
  virtual void CallIdleOnForegroundThread(Isolate* isolate, IdleTask* task) {
    // TODO(ulan): Make this function abstract after V8 roll in Chromium.
  }

  /**
   * Returns true if idle tasks are enabled for the given |isolate|.
   */
  virtual bool IdleTasksEnabled(Isolate* isolate) {
    // TODO(ulan): Make this function abstract after V8 roll in Chromium.
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

 protected:
  /**
   * Default implementation of current wall-clock time in milliseconds
   * since epoch. Useful for implementing |CurrentClockTimeMillis| if
   * nothing special needed.
   */
  static double SystemClockTimeMillis();
};

}  // namespace v8

#endif  // V8_V8_PLATFORM_H_
