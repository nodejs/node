// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8_PLATFORM_H_
#define V8_V8_PLATFORM_H_

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
   * Monotonically increasing time in seconds from an arbitrary fixed point in
   * the past. This function is expected to return at least
   * millisecond-precision values. For this reason,
   * it is recommended that the fixed point be no further in the past than
   * the epoch.
   **/
  virtual double MonotonicallyIncreasingTime() = 0;
};

}  // namespace v8

#endif  // V8_V8_PLATFORM_H_
