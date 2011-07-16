// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_VM_STATE_INL_H_
#define V8_VM_STATE_INL_H_

#include "vm-state.h"
#include "runtime-profiler.h"

namespace v8 {
namespace internal {

//
// VMState class implementation.  A simple stack of VM states held by the
// logger and partially threaded through the call stack.  States are pushed by
// VMState construction and popped by destruction.
//
inline const char* StateToString(StateTag state) {
  switch (state) {
    case JS:
      return "JS";
    case GC:
      return "GC";
    case COMPILER:
      return "COMPILER";
    case OTHER:
      return "OTHER";
    case EXTERNAL:
      return "EXTERNAL";
    default:
      UNREACHABLE();
      return NULL;
  }
}


VMState::VMState(Isolate* isolate, StateTag tag)
    : isolate_(isolate), previous_tag_(isolate->current_vm_state()) {
  if (FLAG_log_state_changes) {
    LOG(isolate, UncheckedStringEvent("Entering", StateToString(tag)));
    LOG(isolate, UncheckedStringEvent("From", StateToString(previous_tag_)));
  }

  isolate_->SetCurrentVMState(tag);
}


VMState::~VMState() {
  if (FLAG_log_state_changes) {
    LOG(isolate_,
        UncheckedStringEvent("Leaving",
                              StateToString(isolate_->current_vm_state())));
    LOG(isolate_,
        UncheckedStringEvent("To", StateToString(previous_tag_)));
  }

  isolate_->SetCurrentVMState(previous_tag_);
}


ExternalCallbackScope::ExternalCallbackScope(Isolate* isolate, Address callback)
    : isolate_(isolate), previous_callback_(isolate->external_callback()) {
  isolate_->set_external_callback(callback);
}

ExternalCallbackScope::~ExternalCallbackScope() {
  isolate_->set_external_callback(previous_callback_);
}


} }  // namespace v8::internal

#endif  // V8_VM_STATE_INL_H_
