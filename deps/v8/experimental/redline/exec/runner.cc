// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/exec/runner.h"

#include <unistd.h>

#include "experimental/redline/base/log.h"

namespace redline {

namespace {
constexpr const char* kLog = "runner";
}

// static
std::unique_ptr<ScriptRunner> ScriptRunner::Create(const TargetOptions& options,
                                                   int id, std::string* error) {
  std::unique_ptr<ScriptRunner> runner(new ScriptRunner(options, id));
  runner->coverage_ = CoverageMap::Create(id, error);
  if (!runner->coverage_) {
    return nullptr;
  }
  runner->child_ = ReprlChild::Create(
      options.ToReprlOptions(runner->coverage_->shm_name()), error);
  if (!runner->child_) {
    return nullptr;
  }
  return runner;
}

ScriptRunner::ScriptRunner(TargetOptions options, int id)
    : options_(std::move(options)), id_(id) {}

ScriptRunner::~ScriptRunner() = default;

bool ScriptRunner::Initialize(std::string* error) {
  if (initialized_) {
    return true;
  }
  // An empty script lets the target start up and publish its edge count.
  Execution execution;
  if (!child_->Execute("", options_.timeout_us, &execution, error)) {
    return false;
  }
  if (!execution.succeeded()) {
    *error = StringPrintf(
        "The target cannot even run an empty script (%s, exit code %d, "
        "signal %d). stderr: %s",
        OutcomeName(execution.outcome), execution.exit_code, execution.signal,
        execution.stderr_output.c_str());
    return false;
  }
  if (!coverage_->FinishInitialization(error)) {
    return false;
  }
  initialized_ = true;
  REDLINE_VLOG(kLog, "Runner %d ready: %u edges instrumented, target pid %d",
               id_, coverage_->num_edges(), static_cast<int>(child_->pid()));
  return true;
}

void ScriptRunner::Account(const Execution& execution) {
  stats_.executions++;
  stats_.total_exec_time_us += execution.exec_time_us;
  switch (execution.outcome) {
    case Outcome::kSucceeded:
      stats_.succeeded++;
      break;
    case Outcome::kFailed:
      stats_.failed++;
      break;
    case Outcome::kCrashed:
      stats_.crashed++;
      break;
    case Outcome::kTimedOut:
      stats_.timed_out++;
      break;
  }
}

bool ScriptRunner::ExecuteWithRetry(std::string_view script,
                                    uint64_t timeout_us, Execution* execution,
                                    std::string* error) {
  if (child_->Execute(script, timeout_us, execution, error)) {
    return true;
  }
  // Like Fuzzilli: retry once with a fresh instance after a short pause;
  // transient failures (e.g. a child that died between executions) are
  // expected now and then.
  stats_.infrastructure_failures++;
  REDLINE_WARN(kLog,
               "Script execution failed: %s. Retrying with a fresh target.",
               error->c_str());
  usleep(100 * 1000);
  child_->Kill();
  coverage_->ClearBitmap();
  if (child_->Execute(script, timeout_us, execution, error)) {
    // A respawned child publishes its edge count again; make sure it is
    // the same binary.
    return coverage_->FinishInitialization(error);
  }
  stats_.infrastructure_failures++;
  return false;
}

bool ScriptRunner::Run(std::string_view script, Result* result,
                       std::string* error) {
  return Run(script, options_.timeout_us, result, error);
}

bool ScriptRunner::Run(std::string_view script, uint64_t timeout_us,
                       Result* result, std::string* error) {
  *result = Result();
  coverage_->ClearBitmap();
  if (!ExecuteWithRetry(script, timeout_us, &result->execution, error)) {
    return false;
  }
  Account(result->execution);
  result->edges_hit = coverage_->CountCurrentEdges();
  switch (result->execution.outcome) {
    case Outcome::kSucceeded:
      result->new_edges = coverage_->EvaluateNewEdges();
      break;
    case Outcome::kCrashed:
      result->new_crash_edges = coverage_->EvaluateCrash();
      break;
    case Outcome::kFailed:
    case Outcome::kTimedOut:
      break;
  }
  return true;
}

bool ScriptRunner::RunWithoutEvaluation(std::string_view script,
                                        uint64_t timeout_us,
                                        Execution* execution,
                                        std::string* error) {
  coverage_->ClearBitmap();
  if (!ExecuteWithRetry(script, timeout_us, execution, error)) {
    return false;
  }
  Account(*execution);
  return true;
}

}  // namespace redline
