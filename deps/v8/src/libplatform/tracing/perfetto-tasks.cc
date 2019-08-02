// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/tracing/perfetto-tasks.h"

#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace platform {
namespace tracing {

PerfettoTaskRunner::PerfettoTaskRunner() : runner_(1, DefaultTimeFunction) {}

PerfettoTaskRunner::~PerfettoTaskRunner() { runner_.Terminate(); }

// static
double PerfettoTaskRunner::DefaultTimeFunction() {
  return (base::TimeTicks::HighResolutionNow() - base::TimeTicks())
      .InSecondsF();
}

void PerfettoTaskRunner::PostTask(std::function<void()> f) {
  runner_.PostTask(base::make_unique<TracingTask>(std::move(f)));
}

void PerfettoTaskRunner::PostDelayedTask(std::function<void()> f,
                                         uint32_t delay_ms) {
  double delay_in_seconds =
      delay_ms / static_cast<double>(base::Time::kMillisecondsPerSecond);
  runner_.PostDelayedTask(base::make_unique<TracingTask>(std::move(f)),
                          delay_in_seconds);
}

bool PerfettoTaskRunner::RunsTasksOnCurrentThread() const {
  return runner_.RunsTasksOnCurrentThread();
}

void PerfettoTaskRunner::FinishImmediateTasks() {
  DCHECK(!RunsTasksOnCurrentThread());
  base::Semaphore semaphore(0);
  // PostTask has guaranteed ordering so this will be the last task executed.
  runner_.PostTask(
      base::make_unique<TracingTask>([&semaphore] { semaphore.Signal(); }));

  semaphore.Wait();
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
