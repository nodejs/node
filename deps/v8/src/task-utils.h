// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TASK_UTILS_H_
#define V8_TASK_UTILS_H_

#include <functional>
#include <memory>

namespace v8 {

namespace internal {

class CancelableIdleTask;
class CancelableTask;
class CancelableTaskManager;
class Isolate;

std::unique_ptr<CancelableTask> MakeCancelableTask(Isolate*,
                                                   std::function<void()>);
std::unique_ptr<CancelableTask> MakeCancelableTask(CancelableTaskManager*,
                                                   std::function<void()>);

std::unique_ptr<CancelableIdleTask> MakeCancelableIdleTask(
    Isolate*, std::function<void(double)>);
std::unique_ptr<CancelableIdleTask> MakeCancelableIdleTask(
    CancelableTaskManager* manager, std::function<void(double)>);

}  // namespace internal
}  // namespace v8

#endif  // V8_TASK_UTILS_H_
