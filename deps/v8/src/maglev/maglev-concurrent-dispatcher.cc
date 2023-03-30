// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-concurrent-dispatcher.h"

#include "src/codegen/compiler.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-heap-broker.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/handles/persistent-handles.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compiler.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/objects/js-function-inl.h"
#include "src/utils/identity-map.h"
#include "src/utils/locked-queue-inl.h"

namespace v8 {
namespace internal {

namespace compiler {

void JSHeapBroker::AttachLocalIsolateForMaglev(
    maglev::MaglevCompilationInfo* info, LocalIsolate* local_isolate) {
  set_canonical_handles(info->DetachCanonicalHandles());
  DCHECK_NULL(local_isolate_);
  local_isolate_ = local_isolate;
  DCHECK_NOT_NULL(local_isolate_);
  local_isolate_->heap()->AttachPersistentHandles(
      info->DetachPersistentHandles());
}

void JSHeapBroker::DetachLocalIsolateForMaglev(
    maglev::MaglevCompilationInfo* info) {
  DCHECK_NULL(ph_);
  DCHECK_NOT_NULL(local_isolate_);
  std::unique_ptr<PersistentHandles> ph =
      local_isolate_->heap()->DetachPersistentHandles();
  local_isolate_ = nullptr;
  info->set_canonical_handles(DetachCanonicalHandles());
  info->set_persistent_handles(std::move(ph));
}

}  // namespace compiler

namespace maglev {

namespace {

constexpr char kMaglevCompilerName[] = "Maglev";

// LocalIsolateScope encapsulates the phase where persistent handles are
// attached to the LocalHeap inside {local_isolate}.
class V8_NODISCARD LocalIsolateScope final {
 public:
  explicit LocalIsolateScope(MaglevCompilationInfo* info,
                             LocalIsolate* local_isolate)
      : info_(info) {
    info_->broker()->AttachLocalIsolateForMaglev(info_, local_isolate);
  }

  ~LocalIsolateScope() { info_->broker()->DetachLocalIsolateForMaglev(info_); }

 private:
  MaglevCompilationInfo* const info_;
};

}  // namespace

Zone* ExportedMaglevCompilationInfo::zone() const { return info_->zone(); }

void ExportedMaglevCompilationInfo::set_canonical_handles(
    std::unique_ptr<CanonicalHandlesMap>&& canonical_handles) {
  info_->set_canonical_handles(std::move(canonical_handles));
}

// static
std::unique_ptr<MaglevCompilationJob> MaglevCompilationJob::New(
    Isolate* isolate, Handle<JSFunction> function) {
  auto info = maglev::MaglevCompilationInfo::New(isolate, function);
  return std::unique_ptr<MaglevCompilationJob>(
      new MaglevCompilationJob(std::move(info)));
}

MaglevCompilationJob::MaglevCompilationJob(
    std::unique_ptr<MaglevCompilationInfo>&& info)
    : OptimizedCompilationJob(kMaglevCompilerName, State::kReadyToPrepare),
      info_(std::move(info)) {
  DCHECK(v8_flags.maglev);
}

MaglevCompilationJob::~MaglevCompilationJob() = default;

CompilationJob::Status MaglevCompilationJob::PrepareJobImpl(Isolate* isolate) {
  if (info()->collect_source_positions()) {
    SharedFunctionInfo::EnsureSourcePositionsAvailable(
        isolate,
        info()->toplevel_compilation_unit()->shared_function_info().object());
  }
  // TODO(v8:7700): Actual return codes.
  return CompilationJob::SUCCEEDED;
}

CompilationJob::Status MaglevCompilationJob::ExecuteJobImpl(
    RuntimeCallStats* stats, LocalIsolate* local_isolate) {
  LocalIsolateScope scope{info(), local_isolate};
  if (!maglev::MaglevCompiler::Compile(local_isolate, info())) {
    return CompilationJob::FAILED;
  }
  // TODO(v8:7700): Actual return codes.
  return CompilationJob::SUCCEEDED;
}

CompilationJob::Status MaglevCompilationJob::FinalizeJobImpl(Isolate* isolate) {
  Handle<Code> code;
  if (!maglev::MaglevCompiler::GenerateCode(isolate, info()).ToHandle(&code)) {
    return CompilationJob::FAILED;
  }
  info()->toplevel_compilation_unit()->function().object()->set_code(*code);
  return CompilationJob::SUCCEEDED;
}

Handle<JSFunction> MaglevCompilationJob::function() const {
  return info_->toplevel_compilation_unit()->function().object();
}

bool MaglevCompilationJob::specialize_to_function_context() const {
  return info_->specialize_to_function_context();
}

void MaglevCompilationJob::RecordCompilationStats(Isolate* isolate) const {
  // Don't record samples from machines without high-resolution timers,
  // as that can cause serious reporting issues. See the thread at
  // http://g/chrome-metrics-team/NwwJEyL8odU/discussion for more details.
  if (base::TimeTicks::IsHighResolution()) {
    Counters* const counters = isolate->counters();
    counters->maglev_optimize_prepare()->AddSample(
        static_cast<int>(time_taken_to_prepare_.InMicroseconds()));
    counters->maglev_optimize_execute()->AddSample(
        static_cast<int>(time_taken_to_execute_.InMicroseconds()));
    counters->maglev_optimize_finalize()->AddSample(
        static_cast<int>(time_taken_to_finalize_.InMicroseconds()));
    counters->maglev_optimize_total_time()->AddSample(
        static_cast<int>(ElapsedTime().InMicroseconds()));
  }
}

// The JobTask is posted to V8::GetCurrentPlatform(). It's responsible for
// processing the incoming queue on a worker thread.
class MaglevConcurrentDispatcher::JobTask final : public v8::JobTask {
 public:
  explicit JobTask(MaglevConcurrentDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}

  void Run(JobDelegate* delegate) override {
    LocalIsolate local_isolate(isolate(), ThreadKind::kBackground);
    DCHECK(local_isolate.heap()->IsParked());

    while (!incoming_queue()->IsEmpty() && !delegate->ShouldYield()) {
      std::unique_ptr<MaglevCompilationJob> job;
      if (!incoming_queue()->Dequeue(&job)) break;
      DCHECK_NOT_NULL(job);
      TRACE_EVENT_WITH_FLOW0(
          TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.MaglevBackground",
          job.get(), TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
      RCS_SCOPE(&local_isolate,
                RuntimeCallCounterId::kOptimizeBackgroundMaglev);
      CompilationJob::Status status =
          job->ExecuteJob(local_isolate.runtime_call_stats(), &local_isolate);
      if (status == CompilationJob::SUCCEEDED) {
        outgoing_queue()->Enqueue(std::move(job));
      }
    }
    isolate()->stack_guard()->RequestInstallMaglevCode();
  }

  size_t GetMaxConcurrency(size_t) const override {
    return incoming_queue()->size();
  }

 private:
  Isolate* isolate() const { return dispatcher_->isolate_; }
  QueueT* incoming_queue() const { return &dispatcher_->incoming_queue_; }
  QueueT* outgoing_queue() const { return &dispatcher_->outgoing_queue_; }

  MaglevConcurrentDispatcher* const dispatcher_;
  const Handle<JSFunction> function_;
};

MaglevConcurrentDispatcher::MaglevConcurrentDispatcher(Isolate* isolate)
    : isolate_(isolate) {
  if (v8_flags.concurrent_recompilation && v8_flags.maglev) {
    job_handle_ = V8::GetCurrentPlatform()->PostJob(
        TaskPriority::kUserVisible, std::make_unique<JobTask>(this));
    DCHECK(is_enabled());
  } else {
    DCHECK(!is_enabled());
  }
}

MaglevConcurrentDispatcher::~MaglevConcurrentDispatcher() {
  if (is_enabled() && job_handle_->IsValid()) {
    // Wait for the job handle to complete, so that we know the queue
    // pointers are safe.
    job_handle_->Cancel();
  }
}

void MaglevConcurrentDispatcher::EnqueueJob(
    std::unique_ptr<MaglevCompilationJob>&& job) {
  DCHECK(is_enabled());
  incoming_queue_.Enqueue(std::move(job));
  job_handle_->NotifyConcurrencyIncrease();
}

void MaglevConcurrentDispatcher::FinalizeFinishedJobs() {
  HandleScope handle_scope(isolate_);
  while (!outgoing_queue_.IsEmpty()) {
    std::unique_ptr<MaglevCompilationJob> job;
    outgoing_queue_.Dequeue(&job);
    TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                           "V8.MaglevConcurrentFinalize", job.get(),
                           TRACE_EVENT_FLAG_FLOW_IN);
    RCS_SCOPE(isolate_,
              RuntimeCallCounterId::kOptimizeConcurrentFinalizeMaglev);
    Compiler::FinalizeMaglevCompilationJob(job.get(), isolate_);
  }
}

void MaglevConcurrentDispatcher::AwaitCompileJobs() {
  // Use Join to wait until there are no more queued or running jobs.
  job_handle_->Join();
  // Join kills the job handle, so drop it and post a new one.
  job_handle_ = V8::GetCurrentPlatform()->PostJob(
      TaskPriority::kUserVisible, std::make_unique<JobTask>(this));
  DCHECK(incoming_queue_.IsEmpty());
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
