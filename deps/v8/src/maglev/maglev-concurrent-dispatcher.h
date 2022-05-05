// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_CONCURRENT_DISPATCHER_H_
#define V8_MAGLEV_MAGLEV_CONCURRENT_DISPATCHER_H_

#ifdef V8_ENABLE_MAGLEV

#include <memory>

#include "src/codegen/compiler.h"  // For OptimizedCompilationJob.
#include "src/utils/locked-queue.h"

namespace v8 {
namespace internal {

class Isolate;

namespace maglev {

class MaglevCompilationInfo;

// TODO(v8:7700): While basic infrastructure now exists, there are many TODOs
// that should still be addressed soon:
// - Full tracing support through --trace-opt.
// - Concurrent codegen.
// - Concurrent Code object creation (optional?).
// - Test support for concurrency (see %FinalizeOptimization).

// Exports needed functionality without exposing implementation details.
class ExportedMaglevCompilationInfo final {
 public:
  explicit ExportedMaglevCompilationInfo(MaglevCompilationInfo* info)
      : info_(info) {}

  Zone* zone() const;
  void set_canonical_handles(
      std::unique_ptr<CanonicalHandlesMap>&& canonical_handles);

 private:
  MaglevCompilationInfo* const info_;
};

// The job is a single actual compilation task.
class MaglevCompilationJob final : public OptimizedCompilationJob {
 public:
  static std::unique_ptr<MaglevCompilationJob> New(Isolate* isolate,
                                                   Handle<JSFunction> function);
  virtual ~MaglevCompilationJob();

  Status PrepareJobImpl(Isolate* isolate) override;
  Status ExecuteJobImpl(RuntimeCallStats* stats,
                        LocalIsolate* local_isolate) override;
  Status FinalizeJobImpl(Isolate* isolate) override;

  Handle<JSFunction> function() const;

 private:
  explicit MaglevCompilationJob(std::unique_ptr<MaglevCompilationInfo>&& info);

  MaglevCompilationInfo* info() const { return info_.get(); }

  const std::unique_ptr<MaglevCompilationInfo> info_;
};

// The public API for Maglev concurrent compilation.
// Keep this as minimal as possible.
class MaglevConcurrentDispatcher final {
  class JobTask;

  // TODO(jgruber): There's no reason to use locking queues here, we only use
  // them for simplicity - consider replacing with lock-free data structures.
  using QueueT = LockedQueue<std::unique_ptr<MaglevCompilationJob>>;

 public:
  explicit MaglevConcurrentDispatcher(Isolate* isolate);
  ~MaglevConcurrentDispatcher();

  // Called from the main thread.
  void EnqueueJob(std::unique_ptr<MaglevCompilationJob>&& job);

  // Called from the main thread.
  void FinalizeFinishedJobs();

  bool is_enabled() const { return static_cast<bool>(job_handle_); }

 private:
  Isolate* const isolate_;
  std::unique_ptr<JobHandle> job_handle_;
  QueueT incoming_queue_;
  QueueT outgoing_queue_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV

#endif  // V8_MAGLEV_MAGLEV_CONCURRENT_DISPATCHER_H_
