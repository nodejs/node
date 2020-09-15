// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/thread-local-top.h"
#include "src/execution/isolate.h"
#include "src/execution/simulator.h"
#include "src/trap-handler/trap-handler.h"

namespace v8 {
namespace internal {

void ThreadLocalTop::Initialize(Isolate* isolate) {
  *this = ThreadLocalTop();
  isolate_ = isolate;
#ifdef USE_SIMULATOR
  simulator_ = Simulator::current(isolate);
#endif
  thread_id_ = ThreadId::Current();
  thread_in_wasm_flag_address_ = reinterpret_cast<Address>(
      trap_handler::GetThreadInWasmThreadLocalAddress());

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
