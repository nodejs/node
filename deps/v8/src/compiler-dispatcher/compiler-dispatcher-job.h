// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
#define V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_

#include "src/contexts.h"
#include "src/handles.h"

namespace v8 {
namespace internal {

class SharedFunctionInfo;

class UnoptimizedCompileJob;

class V8_EXPORT_PRIVATE CompilerDispatcherJob {
 public:
  enum Type { kUnoptimizedCompile };

  virtual ~CompilerDispatcherJob() {}

  virtual Type type() const = 0;

  // Returns true if this CompilerDispatcherJob has finished (either with a
  // success or a failure).
  virtual bool IsFinished() = 0;

  // Returns true if this CompilerDispatcherJob has failed.
  virtual bool IsFailed() = 0;

  // Return true if the next step can be run on any thread, that is when both
  // StepNextOnMainThread and StepNextOnBackgroundThread could be used for the
  // next step.
  virtual bool CanStepNextOnAnyThread() = 0;

  // Step the job forward by one state on the main thread.
  virtual void StepNextOnMainThread(Isolate* isolate) = 0;

  // Step the job forward by one state on a background thread.
  virtual void StepNextOnBackgroundThread() = 0;

  // Transition from any state to kInitial and free all resources.
  virtual void ResetOnMainThread(Isolate* isolate) = 0;

  // Estimate how long the next step will take using the tracer.
  virtual double EstimateRuntimeOfNextStepInMs() const = 0;

  // Even though the name does not imply this, ShortPrint() must only be invoked
  // on the main thread.
  virtual void ShortPrintOnMainThread() = 0;

  // Casts to implementations.
  const UnoptimizedCompileJob* AsUnoptimizedCompileJob() const;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
