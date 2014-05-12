// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VM_STATE_INL_H_
#define V8_VM_STATE_INL_H_

#include "vm-state.h"
#include "log.h"
#include "simulator.h"

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


template <StateTag Tag>
VMState<Tag>::VMState(Isolate* isolate)
    : isolate_(isolate), previous_tag_(isolate->current_vm_state()) {
  if (FLAG_log_timer_events && previous_tag_ != EXTERNAL && Tag == EXTERNAL) {
    LOG(isolate_,
        TimerEvent(Logger::START, Logger::TimerEventScope::v8_external));
  }
  isolate_->set_current_vm_state(Tag);
}


template <StateTag Tag>
VMState<Tag>::~VMState() {
  if (FLAG_log_timer_events && previous_tag_ != EXTERNAL && Tag == EXTERNAL) {
    LOG(isolate_,
        TimerEvent(Logger::END, Logger::TimerEventScope::v8_external));
  }
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
}

ExternalCallbackScope::~ExternalCallbackScope() {
  isolate_->set_external_callback_scope(previous_scope_);
}

Address ExternalCallbackScope::scope_address() {
#ifdef USE_SIMULATOR
  return scope_address_;
#else
  return reinterpret_cast<Address>(this);
#endif
}


} }  // namespace v8::internal

#endif  // V8_VM_STATE_INL_H_
