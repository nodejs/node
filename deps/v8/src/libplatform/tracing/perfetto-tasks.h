// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_TRACING_PERFETTO_TASKS_H_
#define V8_LIBPLATFORM_TRACING_PERFETTO_TASKS_H_

#include <functional>

#include "include/v8-platform.h"
#include "perfetto/base/task_runner.h"
#include "src/libplatform/default-worker-threads-task-runner.h"

namespace v8 {
namespace platform {
namespace tracing {

class TracingTask : public Task {
 public:
  explicit TracingTask(std::function<void()> f) : f_(std::move(f)) {}

  void Run() override { f_(); }

 private:
  std::function<void()> f_;
};

class PerfettoTaskRunner : public ::perfetto::base::TaskRunner {
 public:
  PerfettoTaskRunner();
  ~PerfettoTaskRunner() override;

  // ::perfetto::base::TaskRunner implementation
  void PostTask(std::function<void()> f) override;
  void PostDelayedTask(std::function<void()> f, uint32_t delay_ms) override;
  void AddFileDescriptorWatch(int fd, std::function<void()>) override {
    UNREACHABLE();
  }
  void RemoveFileDescriptorWatch(int fd) override { UNREACHABLE(); }
  bool RunsTasksOnCurrentThread() const override;

  // PerfettoTaskRunner implementation
  void FinishImmediateTasks();

 private:
  static double DefaultTimeFunction();

  DefaultWorkerThreadsTaskRunner runner_;
};

}  // namespace tracing
}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_TRACING_PERFETTO_TASKS_H_
