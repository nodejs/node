// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-api.h"

#include "src/isolate-inl.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {
namespace wasm {

ScheduledErrorThrower::~ScheduledErrorThrower() {
  // There should never be both a pending and a scheduled exception.
  DCHECK(!isolate()->has_scheduled_exception() ||
         !isolate()->has_pending_exception());
  // Don't throw another error if there is already a scheduled error.
  if (isolate()->has_scheduled_exception()) {
    Reset();
  } else if (isolate()->has_pending_exception()) {
    Reset();
    isolate()->OptionalRescheduleException(false);
  } else if (error()) {
    isolate()->ScheduleThrow(*Reify());
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
