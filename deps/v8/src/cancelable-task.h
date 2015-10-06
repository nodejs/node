// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CANCELABLE_TASK_H_
#define V8_CANCELABLE_TASK_H_

#include "include/v8-platform.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

class Isolate;


class Cancelable {
 public:
  explicit Cancelable(Isolate* isolate);
  virtual ~Cancelable();

  virtual void Cancel() { is_cancelled_ = true; }

 protected:
  Isolate* isolate_;
  bool is_cancelled_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Cancelable);
};


// Multiple inheritance can be used because Task is a pure interface.
class CancelableTask : public Cancelable, public Task {
 public:
  explicit CancelableTask(Isolate* isolate) : Cancelable(isolate) {}

  // Task overrides.
  void Run() final {
    if (!is_cancelled_) {
      RunInternal();
    }
  }

  virtual void RunInternal() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelableTask);
};


// Multiple inheritance can be used because IdleTask is a pure interface.
class CancelableIdleTask : public Cancelable, public IdleTask {
 public:
  explicit CancelableIdleTask(Isolate* isolate) : Cancelable(isolate) {}

  // IdleTask overrides.
  void Run(double deadline_in_seconds) final {
    if (!is_cancelled_) {
      RunInternal(deadline_in_seconds);
    }
  }

  virtual void RunInternal(double deadline_in_seconds) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelableIdleTask);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CANCELABLE_TASK_H_
