// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/thread-local-top.h"
#include "src/execution/isolate.h"
#include "src/execution/simulator.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {

void ThreadLocalTop::Clear() {
  try_catch_handler_ = nullptr;
  isolate_ = nullptr;
  context_ = Context();
  thread_id_ = ThreadId();
  pending_handler_entrypoint_ = kNullAddress;
  pending_handler_constant_pool_ = kNullAddress;
  pending_handler_fp_ = kNullAddress;
  pending_handler_sp_ = kNullAddress;
  last_api_entry_ = kNullAddress;
  pending_message_ = Object();
  rethrowing_message_ = false;
  external_caught_exception_ = false;
  c_entry_fp_ = kNullAddress;
  handler_ = kNullAddress;
  c_function_ = kNullAddress;
  promise_on_stack_ = nullptr;
  simulator_ = nullptr;
  js_entry_sp_ = kNullAddress;
  external_callback_scope_ = nullptr;
  current_vm_state_ = EXTERNAL;
  failed_access_check_callback_ = nullptr;
  thread_in_wasm_flag_address_ = kNullAddress;
#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  stack_ = ::heap::base::Stack(nullptr);
#endif
}

void ThreadLocalTop::Initialize(Isolate* isolate) {
  Clear();
  isolate_ = isolate;
  thread_id_ = ThreadId::Current();
#if V8_ENABLE_WEBASSEMBLY
  thread_in_wasm_flag_address_ = reinterpret_cast<Address>(
      trap_handler::GetThreadInWasmThreadLocalAddress());
#endif  // V8_ENABLE_WEBASSEMBLY
#ifdef USE_SIMULATOR
  simulator_ = Simulator::current(isolate);
#endif

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
  stack_ = ::heap::base::Stack(base::Stack::GetStackStart());
#endif
}

void ThreadLocalTop::Free() {
  // Match unmatched PopPromise calls.
  while (promise_on_stack_) isolate_->PopPromise();
}

#if defined(USE_SIMULATOR)
void ThreadLocalTop::StoreCurrentStackPosition() {
  last_api_entry_ = simulator_->get_sp();
}
#elif defined(V8_USE_ADDRESS_SANITIZER)
void ThreadLocalTop::StoreCurrentStackPosition() {
  last_api_entry_ = reinterpret_cast<Address>(GetCurrentStackPosition());
}
#endif

}  // namespace internal
}  // namespace v8
