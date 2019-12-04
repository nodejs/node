// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/interrupts-scope.h"

#include "src/execution/isolate.h"

namespace v8 {
namespace internal {

InterruptsScope::InterruptsScope(Isolate* isolate, intptr_t intercept_mask,
                                 Mode mode)
    : stack_guard_(isolate->stack_guard()),
      intercept_mask_(intercept_mask),
      intercepted_flags_(0),
      mode_(mode) {
  if (mode_ != kNoop) stack_guard_->PushInterruptsScope(this);
}

bool InterruptsScope::Intercept(StackGuard::InterruptFlag flag) {
  InterruptsScope* last_postpone_scope = nullptr;
  for (InterruptsScope* current = this; current; current = current->prev_) {
    // We only consider scopes related to passed flag.
    if (!(current->intercept_mask_ & flag)) continue;
    if (current->mode_ == kRunInterrupts) {
      // If innermost scope is kRunInterrupts scope, prevent interrupt from
      // being intercepted.
      break;
    } else {
      DCHECK_EQ(current->mode_, kPostponeInterrupts);
      last_postpone_scope = current;
    }
  }
  // If there is no postpone scope for passed flag then we should not intercept.
  if (!last_postpone_scope) return false;
  last_postpone_scope->intercepted_flags_ |= flag;
  return true;
}

}  // namespace internal
}  // namespace v8
