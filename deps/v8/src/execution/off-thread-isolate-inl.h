// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_OFF_THREAD_ISOLATE_INL_H_
#define V8_EXECUTION_OFF_THREAD_ISOLATE_INL_H_

#include "src/execution/isolate.h"
#include "src/execution/off-thread-isolate.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {

Address OffThreadIsolate::isolate_root() const {
  return isolate_->isolate_root();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_OFF_THREAD_ISOLATE_INL_H_
