// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/cancelable-task.h"

#include "src/base/platform/platform.h"
#include "src/v8.h"

namespace v8 {
namespace internal {


Cancelable::Cancelable(Isolate* isolate)
    : isolate_(isolate), is_cancelled_(false) {
  isolate->RegisterCancelableTask(this);
}


Cancelable::~Cancelable() {
  if (!is_cancelled_) {
    isolate_->RemoveCancelableTask(this);
  }
}


}  // namespace internal
}  // namespace v8
