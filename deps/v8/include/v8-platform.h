// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8_PLATFORM_H_
#define V8_V8_PLATFORM_H_

#include <stddef.h>
#include <stdint.h>

namespace v8 {

class Isolate;

/**
 * A Task represents a unit of work.
 */
class Task {
 public:
  virtual ~Task() {}

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
  virtual ~IdleTask() {}
  virtual void Run(double deadline_in_seconds) = 0;
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

  virtual ~Platform() {}

  /**
   * Gets the number of threads that are used to execute background tasks. Is
   * used to estimate the number of tasks a work package should be split into.
   * A return value of 0 means that there are no background threads available.
   * Note that a value of 0 won't prohibit V8 from posting tasks using
   * |CallOnBackgroundThread|.
   */
  virtual size_t NumberOfAvailableBackgroundThreads() { return 0; }

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
   * Gets the category group name of the given category_enabled_flag pointer.
   * Usually used while serliazing TRACE_EVENTs.
   **/
  virtual const char* GetCategoryGroupName(
      const uint8_t* category_enabled_flag) {
    static const char dummy[] = "dummy";
    return dummy;
  }

  /**
   * Adds a trace event to the platform tracing system. This function call is
   * usually the result of a TRACE_* macro from trace_event_common.h when
   * tracing and the category of the particular trace are enabled. It is not
   * advisable to call this function on its own; it is really only meant to be
   * used by the trace macros. The returned handle can be used by
   * UpdateTraceEventDuration to update the duration of COMPLETE events.
   */
  virtual uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      uint64_t id, uint64_t bind_id, int32_t num_args, const char** arg_names,
      const uint8_t* arg_types, const uint64_t* arg_values,
      unsigned int flags) {
    return 0;
  }

  /**
   * Sets the duration field of a COMPLETE trace event. It must be called with
   * the handle returned from AddTraceEvent().
   **/
  virtual void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                        const char* name, uint64_t handle) {}
};

}  // namespace v8

#endif  // V8_V8_PLATFORM_H_
