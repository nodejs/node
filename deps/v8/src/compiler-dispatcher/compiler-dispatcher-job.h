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
  enum class Type { kUnoptimizedCompile };

  enum class Status {
    kInitial,
    kPrepared,
    kCompiled,
    kHasErrorsToReport,
    kDone,
    kFailed,
  };

  CompilerDispatcherJob(Type type) : type_(type), status_(Status::kInitial) {}

  virtual ~CompilerDispatcherJob() {}

  Type type() const { return type_; }

  // Returns the current status of the compile
  Status status() const { return status_; }

  // Returns true if this CompilerDispatcherJob has finished (either with a
  // success or a failure).
  bool IsFinished() const {
    return status() == Status::kDone || status() == Status::kFailed;
  }

  // Returns true if this CompilerDispatcherJob has failed.
  bool IsFailed() const { return status() == Status::kFailed; }

  // Return true if the next step can be run on any thread.
  bool NextStepCanRunOnAnyThread() const {
    return status() == Status::kPrepared;
  }

  // Casts to implementations.
  const UnoptimizedCompileJob* AsUnoptimizedCompileJob() const;

  // Transition from kInitial to kPrepared. Must only be invoked on the
  // main thread.
  virtual void PrepareOnMainThread(Isolate* isolate) = 0;

  // Transition from kPrepared to kCompiled (or kReportErrors).
  virtual void Compile(bool on_background_thread) = 0;

  // Transition from kCompiled to kDone (or kFailed). Must only be invoked on
  // the main thread.
  virtual void FinalizeOnMainThread(Isolate* isolate) = 0;

  // Transition from kReportErrors to kFailed. Must only be invoked on the main
  // thread.
  virtual void ReportErrorsOnMainThread(Isolate* isolate) = 0;

  // Free all resources. Must only be invoked on the main thread.
  virtual void ResetOnMainThread(Isolate* isolate) = 0;

  // Estimate how long the next step will take using the tracer.
  virtual double EstimateRuntimeOfNextStepInMs() const = 0;

  // Print short description of job. Must only be invoked on the main thread.
  virtual void ShortPrintOnMainThread() = 0;

 protected:
  void set_status(Status status) { status_ = status; }

 private:
  Type type_;
  Status status_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
