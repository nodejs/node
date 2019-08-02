// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_VM_STATE_INL_H_
#define V8_EXECUTION_VM_STATE_INL_H_

#include "src/execution/isolate.h"
#include "src/execution/simulator.h"
#include "src/execution/vm-state.h"
#include "src/logging/log.h"
#include "src/tracing/trace-event.h"

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
    case PARSER:
      return "PARSER";
    case BYTECODE_COMPILER:
      return "BYTECODE_COMPILER";
    case COMPILER:
      return "COMPILER";
    case OTHER:
      return "OTHER";
    case EXTERNAL:
      return "EXTERNAL";
    case IDLE:
      return "IDLE";
  }
}

template <StateTag Tag>
VMState<Tag>::VMState(Isolate* isolate)
    : isolate_(isolate), previous_tag_(isolate->current_vm_state()) {
  isolate_->set_current_vm_state(Tag);
}

template <StateTag Tag>
VMState<Tag>::~VMState() {
  isolate_->set_current_vm_state(previous_tag_);
}

ExternalCallbackScope::ExternalCallbackScope(Isolate* isolate, Address callback)
    : isolate_(isolate),
      callback_(callback),
      previous_scope_(isolate->external_callback_scope()) {
#ifdef USE_SIMULATOR
  scope_address_ = Simulator::current(isolate)->get_sp();
#endif
  isolate_->set_external_callback_scope(this);
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),
                     "V8.ExternalCallback");
}

ExternalCallbackScope::~ExternalCallbackScope() {
  isolate_->set_external_callback_scope(previous_scope_);
  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),
                   "V8.ExternalCallback");
}

Address ExternalCallbackScope::scope_address() {
#ifdef USE_SIMULATOR
  return scope_address_;
#else
  return reinterpret_cast<Address>(this);
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_VM_STATE_INL_H_
