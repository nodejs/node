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
#ifdef ENABLE_VMSTATE_TRACKING
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

VMState::VMState(StateTag tag) : previous_tag_(Top::current_vm_state()) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log_state_changes) {
    LOG(UncheckedStringEvent("Entering", StateToString(tag)));
    LOG(UncheckedStringEvent("From", StateToString(previous_tag_)));
  }
#endif

  Top::SetCurrentVMState(tag);

#ifdef ENABLE_HEAP_PROTECTION
  if (FLAG_protect_heap) {
    if (tag == EXTERNAL) {
      // We are leaving V8.
      ASSERT(previous_tag_ != EXTERNAL);
      Heap::Protect();
    } else if (previous_tag_ = EXTERNAL) {
      // We are entering V8.
      Heap::Unprotect();
    }
  }
#endif
}


VMState::~VMState() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log_state_changes) {
    LOG(UncheckedStringEvent("Leaving",
                             StateToString(Top::current_vm_state())));
    LOG(UncheckedStringEvent("To", StateToString(previous_tag_)));
  }
#endif  // ENABLE_LOGGING_AND_PROFILING

#ifdef ENABLE_HEAP_PROTECTION
  StateTag tag = Top::current_vm_state();
#endif

  Top::SetCurrentVMState(previous_tag_);

#ifdef ENABLE_HEAP_PROTECTION
  if (FLAG_protect_heap) {
    if (tag == EXTERNAL) {
      // We are reentering V8.
      ASSERT(previous_tag_ != EXTERNAL);
      Heap::Unprotect();
    } else if (previous_tag_ == EXTERNAL) {
      // We are leaving V8.
      Heap::Protect();
    }
  }
#endif  // ENABLE_HEAP_PROTECTION
}

#endif  // ENABLE_VMSTATE_TRACKING


#ifdef ENABLE_LOGGING_AND_PROFILING

ExternalCallbackScope::ExternalCallbackScope(Address callback)
    : previous_callback_(Top::external_callback()) {
  Top::set_external_callback(callback);
}

ExternalCallbackScope::~ExternalCallbackScope() {
  Top::set_external_callback(previous_callback_);
}

#endif  // ENABLE_LOGGING_AND_PROFILING


} }  // namespace v8::internal

#endif  // V8_VM_STATE_INL_H_
