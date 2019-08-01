// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tasks/task-utils.h"

#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

namespace {

class CancelableFuncTask final : public CancelableTask {
 public:
  CancelableFuncTask(Isolate* isolate, std::function<void()> func)
      : CancelableTask(isolate), func_(std::move(func)) {}
  CancelableFuncTask(CancelableTaskManager* manager, std::function<void()> func)
      : CancelableTask(manager), func_(std::move(func)) {}
  void RunInternal() final { func_(); }

 private:
  const std::function<void()> func_;
};

class CancelableIdleFuncTask final : public CancelableIdleTask {
 public:
  CancelableIdleFuncTask(Isolate* isolate, std::function<void(double)> func)
      : CancelableIdleTask(isolate), func_(std::move(func)) {}
  CancelableIdleFuncTask(CancelableTaskManager* manager,
                         std::function<void(double)> func)
      : CancelableIdleTask(manager), func_(std::move(func)) {}
  void RunInternal(double deadline_in_seconds) final {
    func_(deadline_in_seconds);
  }

 private:
  const std::function<void(double)> func_;
};

}  // namespace

std::unique_ptr<CancelableTask> MakeCancelableTask(Isolate* isolate,
                                                   std::function<void()> func) {
  return base::make_unique<CancelableFuncTask>(isolate, std::move(func));
}

std::unique_ptr<CancelableTask> MakeCancelableTask(
    CancelableTaskManager* manager, std::function<void()> func) {
  return base::make_unique<CancelableFuncTask>(manager, std::move(func));
}

std::unique_ptr<CancelableIdleTask> MakeCancelableIdleTask(
    Isolate* isolate, std::function<void(double)> func) {
  return base::make_unique<CancelableIdleFuncTask>(isolate, std::move(func));
}

std::unique_ptr<CancelableIdleTask> MakeCancelableIdleTask(
    CancelableTaskManager* manager, std::function<void(double)> func) {
  return base::make_unique<CancelableIdleFuncTask>(manager, std::move(func));
}

}  // namespace internal
}  // namespace v8
